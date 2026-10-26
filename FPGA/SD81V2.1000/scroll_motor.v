`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// scroll_motor -- motor de movimiento HORIZONTAL del scroll por hardware (fase 2)
//
//  Diseno: claude/planning/scroll_hardware.md. Es el scroll "grueso": mueve la
//  memoria de la pantalla (y sus atributos) UN BYTE a la izquierda o a la
//  derecha en la BRAM de sombra, cuando el fino ya ha llegado a 7. Es una
//  accion aparte del Z80 (orden por el puerto $3FEF, registro 9).
//
//  Cadena de nodos de cada linea (o de cada fila de atributos):
//        L  m0 m1 ... m31  R         L = bufer de la izquierda, R = el de la derecha
//  Izquierda: cada nodo toma el valor del de su derecha (m0 <- m1 ... m31 <- R) y el
//  byte que sale por la izquierda se guarda en L (asi ir y volver no obliga a regenerar).
//  Derecha: lo contrario (m31 <- m30 ... m0 <- L) y lo que sale por la derecha va a R.
//  Lo que entra (R al ir a la izquierda, L al ir a la derecha) lo tiene que haber
//  escrito el Z80; despues del movimiento el Z80 vuelve a rellenar el bufer del
//  lado de entrada.
//
//  Cada copia son 3 ciclos (direccion, dato de la BRAM, escritura), como el blit
//  del doble buffer: la CPU manda (sh_busy) y el motor espera. Una copia por
//  nodo y linea: 33 copias; el bitmap son 192 lineas y los atributos de HiRes
//  modo 0 otras 192 (los de Spectrum y de HiRes modo 1, 24 filas).
//
//  Que se mueve (todo dentro del bloque que escribe el Z80, no el del doble buffer):
//    Spectrum        bitmap ({y[7:6],y[2:0],y[5:3],col}) y atributos ($1800 + fila*32 + col)
//    HiRes nativo    bitmap lineal (y*32 + col) y, si hay color:
//                      modo 0 (8x1)  atributos paralelos en $C000 (misma direccion que el bitmap)
//                      modo 1 (8x8)  rejilla de 33 bytes por fila desde a1_base (la columna 32 es R;
//                                    no hay L: al ir a la derecha m0 se queda como estaba)
//  Respeta la mascara de filas (row_en): una fila con el bit a 0 no se mueve.
//  Los bufers de entrada estan en spec_scroll_addr.v / lin_scroll_addr.v:
//    Spectrum  R bitmap $1B00+y, R atributo $1BC0+fila, L bitmap $1C00+y, L atributo $1CC0+fila
//    HiRes     R $1800+y,  L $1C00+y  (en el bitmap y en el area de atributos de modo 0)
//////////////////////////////////////////////////////////////////////////////////
module scroll_motor(
	input wire        clk,				// system_clk
	input wire        rst,				// activo alto
	input wire        start,			// pulso: empezar un movimiento ahora (se ignora si ya esta en marcha)
	input wire [1:0]  dir,				// 1 = izquierda, 2 = derecha (otro valor: no hace nada)
	input wire        mode_sp,			// Spectrum
	input wire        mode_hr,			// HiRes nativo
	input wire        attr0_en,			// HiRes con color en modo 0: mover los atributos de $C000
	input wire        attr1_en,			// HiRes con color en modo 1: mover la rejilla de atributos
	input wire [2:0]  blk,				// bloque de 8 KB de la pantalla (HFILE[15:13])
	input wire [15:0] a1_base,			// direccion del atributo de la fila 0, columna 0 (modo 1)
	input wire [23:0] row_en,			// filas que se mueven (bit r = fila r)
	input wire        sh_busy,			// el puerto A lo usa otro (CPU, depurador, traza)
	input wire [7:0]  rdata,			// salida del puerto B de la BRAM
	output reg        run = 1'b0,		// en marcha (usa los puertos A y B)
	output wire [15:0] addr,			// direccion (lectura en S_RD, escritura en S_WR)
	output wire       we,				// escribir wdata en addr (puerto A)
	output wire [7:0] wdata,
	output reg        done = 1'b0		// pulso de un ciclo: ha terminado (tambien si no habia nada que hacer)
    );

	localparam [1:0] S_ROW = 2'd0,
	                 S_RD  = 2'd1,
	                 S_LAT = 2'd2,
	                 S_WR  = 2'd3;

	reg [1:0]  st = S_ROW;
	reg [4:0]  r = 5'd0;			// fila de texto
	reg [2:0]  ln = 3'd0;			// linea dentro de la fila
	reg [5:0]  i = 6'd0;			// paso dentro de la cadena: 63 = -1 (L), 0-31, 32 = R
	reg        pass = 1'b0;			// 0 = bitmap, 1 = atributos
	reg        left = 1'b0;
	reg        l_sp = 1'b0, l_a0 = 1'b0, l_a1 = 1'b0;
	reg [2:0]  l_blk = 3'd0;
	reg [15:0] l_a1b = 16'd0;		// direccion del atributo (fila 0, col 0), fija
	reg [15:0] rowbase = 16'd0;		// ... de la fila actual, sumando 33 por fila
	reg [7:0]  buf_r = 8'd0;

	wire has_attr = l_sp | l_a0 | l_a1;
	wire a1_pass  = pass & l_a1;
	wire one_line = pass & (l_sp | l_a1);			// una sola "linea" por fila: atributos 8x8
	wire [5:0] start_i = left ? (a1_pass ? 6'd0 : 6'd63) : 6'd31;
	wire [5:0] end_i   = left ? 6'd31 : (a1_pass ? 6'd0 : 6'd63);
	wire last_i = (i == end_i);

	wire rd = (st == S_RD);
	wire [5:0] i1 = i + 6'd1;
	// nodo de este ciclo: lectura = origen, escritura = destino
	//   izquierda: origen i+1, destino i      derecha: origen i, destino i+1
	wire [5:0] m = (rd == left) ? i1 : i;
	wire isL = (m == 6'd63);
	wire isR = (m == 6'd32);
	wire [4:0] c = m[4:0];
	wire [7:0] y = {r, ln};

	wire [12:0] off_lin = isL ? {5'b11100, y} : isR ? {5'b11000, y} : {y, c};
	wire [12:0] off_spb = isL ? {5'b11100, y} : isR ? {5'b11011, y} : {y[7:6], y[2:0], y[5:3], c};
	wire [12:0] off_spa = isL ? {8'b11100110, r} : isR ? {8'b11011110, r} : {3'b110, r, c};

	assign addr = ~pass ? {l_blk, l_sp ? off_spb : off_lin} :
	              l_sp  ? {l_blk, off_spa} :
	              l_a0  ? {3'b110, off_lin} :
	                      (rowbase + {10'd0, m});			// modo 1: m va de 0 a 32
	assign we = run & (st == S_WR) & ~sh_busy;
	assign wdata = buf_r;

	always @(posedge clk) begin
		done <= 1'b0;
		if (rst) begin
			run <= 1'b0;
			st <= S_ROW;
		end else if (~run) begin
			if (start) begin
				left  <= (dir == 2'd1);
				l_sp  <= mode_sp;
				l_a0  <= mode_hr & attr0_en;
				l_a1  <= mode_hr & attr1_en;
				l_blk <= blk;
				l_a1b <= a1_base;
				pass  <= 1'b0;
				r     <= 5'd0;
				ln    <= 3'd0;
				st    <= S_ROW;
				if ((dir == 2'd1 || dir == 2'd2) && (mode_sp || mode_hr)) begin
					run <= 1'b1;
					rowbase <= a1_base;
				end else
					done <= 1'b1;
			end
		end else begin
			case (st)
				S_ROW: begin
					if (r == 5'd24) begin				// se acabaron las filas de esta pasada
						if (~pass && has_attr) begin
							pass <= 1'b1;
							r <= 5'd0;
							rowbase <= l_a1b;
						end else begin
							run <= 1'b0;
							done <= 1'b1;
						end
					end else if (~row_en[r]) begin		// fila fija
						r <= r + 5'd1;
						rowbase <= rowbase + 16'd33;
					end else begin
						ln <= 3'd0;
						i <= start_i;
						st <= S_RD;
					end
				end
				S_RD:  st <= S_LAT;
				S_LAT: begin
					buf_r <= rdata;
					st <= S_WR;
				end
				S_WR: if (~sh_busy) begin
					if (last_i) begin
						if (~one_line && ln != 3'd7) begin	// siguiente linea de la misma fila
							ln <= ln + 3'd1;
							i <= start_i;
							st <= S_RD;
						end else begin					// siguiente fila
							r <= r + 5'd1;
							rowbase <= rowbase + 16'd33;
							ln <= 3'd0;
							st <= S_ROW;
						end
					end else begin
						i <= left ? i1 : (i - 6'd1);
						st <= S_RD;
					end
				end
			endcase
		end
	end

endmodule
