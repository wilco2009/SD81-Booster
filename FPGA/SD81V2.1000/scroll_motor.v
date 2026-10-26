`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// scroll_motor -- motor de movimiento del scroll por hardware (fases 2 y 3)
//
//  Diseno: claude/planning/scroll_hardware.md. Es el scroll "grueso": mueve la
//  memoria de la pantalla (y sus atributos) UN BYTE (horizontal) o UNA FILA DE
//  TEXTO (vertical) en la BRAM de sombra, cuando el fino ya ha llegado al
//  extremo. Es una accion aparte del Z80 (orden por el puerto $3FEF, registro 9).
//
//  HORIZONTAL (dir 1 = izquierda, 2 = derecha)
//  Cadena de nodos de cada linea (o de cada fila de atributos):
//        L  m0 m1 ... m31  R         L = bufer de la izquierda, R = el de la derecha
//  Izquierda: cada nodo toma el valor del de su derecha (m0 <- m1 ... m31 <- R) y el
//  byte que sale por la izquierda se guarda en L (asi ir y volver no obliga a regenerar).
//  Derecha: lo contrario (m31 <- m30 ... m0 <- L) y lo que sale por la derecha va a R.
//  Lo que entra (R al ir a la izquierda, L al ir a la derecha) lo tiene que haber
//  escrito el Z80; despues del movimiento el Z80 vuelve a rellenar el bufer del
//  lado de entrada.
//
//  VERTICAL (dir 3 = arriba: el contenido sube, 4 = abajo: el contenido baja)
//  Cadena de filas de la REGION (reg_top..reg_bot; la mascara de filas del horizontal
//  no cuenta):   T  fila_top ... fila_bot  B      T = bufer de arriba, B = el de abajo
//  Arriba: cada fila toma la de debajo (T <- fila_top, fila_r <- fila_r+1, fila_bot <- B).
//  Abajo:  al reves (B <- fila_bot, fila_r <- fila_r-1, fila_top <- T).
//  El motor recorre los PARES (fila r-1, fila r) con r de reg_top a reg_bot+1 (arriba,
//  de menor a mayor) o de reg_bot+1 a reg_top (abajo): asi cada fila se lee antes de que
//  se pise. En cada par copia las 8 lineas de los 34 nodos L, m0..m31, R (en los
//  atributos de una linea por fila, una sola). Las columnas L y R viajan con su fila;
//  los extremos que no existen se saltan y quedan como estaban:
//      L: no tiene T ni B (se salta en los pares de los extremos)
//      R: T no existe; B es la ESQUINA (bitmap $1A00+k / $1E40+k, atributo $1E48)
//      modo 1 de HiRes: no hay L ni T; B es la fila 24 de la rejilla (con su columna 32)
//  Lo que entra (B al subir, T al bajar) lo tiene que haber escrito el Z80; el que sale
//  se guarda en el bufer opuesto.
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
//  El horizontal respeta la mascara de filas (row_en): una fila con el bit a 0 no se mueve.
//  Los bufers de entrada estan en spec_scroll_addr.v / lin_scroll_addr.v:
//    Spectrum  R bitmap $1B00+y, R atributo $1BC0+fila, L bitmap $1C00+y, L atributo $1CC0+fila,
//              B bitmap $1D00+k*32+c, B atributo $1E00+c, T atributo $1E20+c, T bitmap $1F00+k*32+c
//    HiRes     R $1800+y,  L $1C00+y,  B $1900+k*32+c,  T $1D00+k*32+c  (en el bitmap y en el
//              area de atributos de modo 0)
//////////////////////////////////////////////////////////////////////////////////
module scroll_motor(
	input wire        clk,				// system_clk
	input wire        rst,				// activo alto
	input wire        start,			// pulso: empezar un movimiento ahora (se ignora si ya esta en marcha)
	input wire [2:0]  dir,				// 1 = izquierda, 2 = derecha, 3 = arriba, 4 = abajo (otro valor: no hace nada)
	input wire        mode_sp,			// Spectrum
	input wire        mode_hr,			// HiRes nativo
	input wire        attr0_en,			// HiRes con color en modo 0: mover los atributos de $C000
	input wire        attr1_en,			// HiRes con color en modo 1: mover la rejilla de atributos
	input wire [2:0]  blk,				// bloque de 8 KB de la pantalla (HFILE[15:13])
	input wire [15:0] a1_base,			// direccion del atributo de la fila 0, columna 0 (modo 1)
	input wire [23:0] row_en,			// filas que se mueven en horizontal (bit r = fila r)
	input wire [4:0]  reg_top,			// region vertical: primera fila
	input wire [4:0]  reg_bot,			// ... y ultima (mas de 23 cuenta como 23)
	input wire        sh_busy,			// el puerto A lo usa otro (CPU, depurador, traza)
	input wire [7:0]  rdata,			// salida del puerto B de la BRAM
	output reg        run = 1'b0,		// en marcha (usa los puertos A y B)
	output wire [15:0] addr,			// direccion (lectura en S_RD, escritura en S_WR)
	output wire       we,				// escribir wdata en addr (puerto A)
	output wire [7:0] wdata,
	output reg        done = 1'b0		// pulso de un ciclo: ha terminado (tambien si no habia nada que hacer)
    );

	localparam [2:0] S_ROW  = 3'd0,
	                 S_RD   = 3'd1,
	                 S_LAT  = 3'd2,
	                 S_WR   = 3'd3,
	                 S_SEEK = 3'd4,		// vertical: coloca r (y rowbase) en el primer par
	                 S_NXT  = 3'd5;		// vertical: nodo que no existe, se pasa al siguiente sin copiar

	reg [2:0]  st = S_ROW;
	reg [4:0]  r = 5'd0;			// horizontal: fila de texto. Vertical: fila r del par (r-1, r)
	reg [4:0]  rp = 5'd31;			// vertical: fila r-1 del par
	reg [2:0]  ln = 3'd0;			// linea dentro de la fila
	reg [5:0]  i = 6'd0;			// paso dentro de la cadena: 63 = -1 (L), 0-31, 32 = R
	reg        pass = 1'b0;			// 0 = bitmap, 1 = atributos
	reg        left = 1'b0;
	reg        vert = 1'b0;			// movimiento vertical
	reg        up = 1'b0;			// vertical: el contenido sube
	reg        l_sp = 1'b0, l_a0 = 1'b0, l_a1 = 1'b0;
	reg [2:0]  l_blk = 3'd0;
	reg [4:0]  l_top = 5'd0;		// region vertical (latcheada al empezar)
	reg [4:0]  l_botp1 = 5'd0;		// ... y la fila de abajo de la region (reg_bot + 1)
	reg [15:0] l_a1b = 16'd0;		// direccion del atributo (fila 0, col 0), fija
	reg [15:0] rowbase = 16'd0;		// ... de la fila actual, sumando 33 por fila
	reg [7:0]  buf_r = 8'd0;

	wire has_attr = l_sp | l_a0 | l_a1;
	wire a1_pass  = pass & l_a1;
	wire one_line = pass & (l_sp | l_a1);			// una sola "linea" por fila: atributos 8x8
	wire [5:0] start_i = vert ? (a1_pass ? 6'd0 : 6'd63) :
	                     left ? (a1_pass ? 6'd0 : 6'd63) : 6'd31;
	wire [5:0] end_i   = left ? 6'd31 : (a1_pass ? 6'd0 : 6'd63);
	wire last_i = vert ? (i == 6'd32) : (i == end_i);

	wire rd = (st == S_RD);
	wire [5:0] i1 = i + 6'd1;
	// nodo de este ciclo: lectura = origen, escritura = destino
	//   izquierda: origen i+1, destino i      derecha: origen i, destino i+1
	//   vertical: el mismo nodo (columna) en las dos filas del par
	wire [5:0] m = vert ? i : ((rd == left) ? i1 : i);
	wire isL = (m == 6'd63);
	wire isR = (m == 6'd32);
	wire [4:0] c = m[4:0];

	// --- vertical: que fila es la de este ciclo ---
	// arriba: destino r-1, origen r. abajo: destino r, origen r-1. La lectura usa el origen.
	wire use_r = (up == rd);						// este ciclo toca la fila r (si no, la r-1)
	wire lowT = (r == l_top);						// la fila r-1 del par es el bufer de arriba
	wire upB  = (r == l_botp1);						// la fila r del par es el bufer de abajo
	wire kT = vert & ~use_r & lowT;
	wire kB = vert & use_r & upB;
	wire [4:0] vr = (vert & ~use_r) ? rp : r;
	wire [7:0] y = {vr, ln};
	// nodos que no existen en este par (L en los extremos, R arriba, todo en el modo 1 arriba)
	wire skip = (isL & (lowT | upB)) | (isR & lowT) | (a1_pass & lowT);

	wire [12:0] off_lin = kT ? {5'b11101, ln, c} :
	                      kB ? (isR ? {10'b1101000000, ln} : {5'b11001, ln, c}) :
	                      isL ? {5'b11100, y} : isR ? {5'b11000, y} : {y, c};
	wire [12:0] off_spb = kT ? {5'b11111, ln, c} :
	                      kB ? (isR ? {10'b1111001000, ln} : {5'b11101, ln, c}) :
	                      isL ? {5'b11100, y} : isR ? {5'b11011, y} : {y[7:6], y[2:0], y[5:3], c};
	wire [12:0] off_spa = kT ? {8'b11110001, c} :
	                      kB ? (isR ? {10'b1111001001, 3'b000} : {8'b11110000, c}) :
	                      isL ? {8'b11100110, vr} : isR ? {8'b11011110, vr} : {3'b110, vr, c};
	// modo 1: la fila de abajo (B) es siempre la 24 de la rejilla; la fila r-1 esta 33 antes
	wire [15:0] a1_row24 = l_a1b + 16'd792;
	wire [15:0] rb_sel = kB ? a1_row24 : ((vert & ~use_r) ? (rowbase - 16'd33) : rowbase);

	assign addr = ~pass ? {l_blk, l_sp ? off_spb : off_lin} :
	              l_sp  ? {l_blk, off_spa} :
	              l_a0  ? {3'b110, off_lin} :
	                      (rb_sel + {10'd0, m});			// modo 1: m va de 0 a 32
	assign we = run & (st == S_WR) & ~sh_busy;
	assign wdata = buf_r;

	// --- vertical: region y fin del recorrido ---
	wire [4:0] bot_c = (reg_bot > 5'd23) ? 5'd23 : reg_bot;
	wire v_last_step = up ? upB : lowT;				// ultimo par de la pasada
	wire [4:0] v_tgt = up ? l_top : l_botp1;		// primer valor de r
	wire [4:0] r_next  = up ? (r + 5'd1)  : (r - 5'd1);
	wire [4:0] rp_next = up ? (rp + 5'd1) : (rp - 5'd1);
	wire [15:0] rb_next = up ? (rowbase + 16'd33) : (rowbase - 16'd33);

	always @(posedge clk) begin
		done <= 1'b0;
		if (rst) begin
			run <= 1'b0;
			st <= S_ROW;
		end else if (~run) begin
			if (start) begin
				left  <= (dir == 3'd1);
				vert  <= (dir == 3'd3 || dir == 3'd4);
				up    <= (dir == 3'd3);
				l_sp  <= mode_sp;
				l_a0  <= mode_hr & attr0_en;
				l_a1  <= mode_hr & attr1_en;
				l_blk <= blk;
				l_a1b <= a1_base;
				l_top <= reg_top;
				l_botp1 <= bot_c + 5'd1;
				pass  <= 1'b0;
				r     <= 5'd0;
				rp    <= 5'd31;
				ln    <= 3'd0;
				st    <= S_ROW;
				if ((dir == 3'd1 || dir == 3'd2) && (mode_sp || mode_hr)) begin
					run <= 1'b1;
					rowbase <= a1_base;
				end else if ((dir == 3'd3 || dir == 3'd4) && (mode_sp || mode_hr) && (reg_top <= bot_c)) begin
					run <= 1'b1;
					rowbase <= a1_base;
					st <= S_SEEK;
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
				S_SEEK: begin							// r hasta el primer par (con rowbase = a1_base + 33*r)
					if (r != v_tgt) begin
						r <= r + 5'd1;
						rp <= rp + 5'd1;
						rowbase <= rowbase + 16'd33;
					end else begin
						ln <= 3'd0;
						i <= start_i;
						st <= S_RD;
					end
				end
				S_RD:  st <= (vert & skip) ? S_NXT : S_LAT;
				S_LAT: begin
					buf_r <= rdata;
					st <= S_WR;
				end
				S_WR, S_NXT: if (st == S_NXT || ~sh_busy) begin
					if (vert) begin
						if (~last_i) begin				// siguiente nodo del mismo par y linea
							i <= i1;
							st <= S_RD;
						end else if (~one_line && ln != 3'd7) begin	// siguiente linea del par
							ln <= ln + 3'd1;
							i <= start_i;
							st <= S_RD;
						end else if (~v_last_step) begin			// siguiente par
							r <= r_next;
							rp <= rp_next;
							rowbase <= rb_next;
							ln <= 3'd0;
							i <= start_i;
							st <= S_RD;
						end else if (~pass && has_attr) begin		// pasada de atributos
							pass <= 1'b1;
							r <= 5'd0;
							rp <= 5'd31;
							rowbase <= l_a1b;
							st <= S_SEEK;
						end else begin
							run <= 1'b0;
							done <= 1'b1;
							st <= S_ROW;
						end
					end else if (last_i) begin
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
