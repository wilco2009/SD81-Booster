// ============================================================================
// sprite_engine.v -- Sprites 8x8 de 4 bits por pixel (formato B), por LINEA,
//                    leidos de la RAM de sombra
// ============================================================================
// La FPGA guarda la cabecera de los NSPR sprites en la RAM de sombra:
//
//     sprites  0..31:  $0C00 + sprite*32 + campo
//     sprites 32..63:  $1800 + (sprite-32)*32 + campo
//
// (campo 0 activo, 1 X bajo, 2 X alto, 3 Y, 4 pagina de la hoja), la misma
// copia que usan los snapshots. Los graficos NO estan en la tabla: viven en la
// misma RAM de sombra (el espejo de lo que escribe el Z80), en una HOJA de
// celdas de 32 bytes, 8 filas de 4 bytes (8 pixeles de 4 bits; el nibble alto
// de cada byte es el pixel de la izquierda, como en la imagen de 4 bpp). El
// indice 0 es transparente; los demas pasan por pal16.
//
//     direccion de la fila f (0..7) de la celda: {sheet_base, pagina, f, byte}
//     sheet_base (3 bits): bloque de 8 KB; pagina (8 bits): celda de 32 B del bloque
//
// Animar un sprite es cambiar su byte de pagina; animar todos a la vez es
// cambiar sheet_base (que SD81.v latchea en el VSYNC).
//
//  1. Cuando empieza el sincronismo horizontal (la linea nueva ya esta en
//     line_cnt) recorre los NSPR sprites de mayor a menor indice. Los que estan
//     activos (spr_en) y cortan la linea que se va a dibujar se cargan en uno de
//     K slots de linea con SU fila: X y 8 pixeles.
//  2. Durante la linea, cada slot compara su X con el pixel actual y saca su
//     nibble.
//
// Prioridad: gana el sprite de indice mas alto con un pixel distinto de 0 (los
// transparentes dejan ver a los de debajo). El slot 0 es el de mas prioridad.
// Si en una linea hay mas de K sprites, se dibujan los K de indice mas alto y
// los demas no aparecen en ESA linea.
//
// Reloj: el motor va a system_clk (26 MHz) y da un paso cada DOS ciclos: la BRAM
// tiene un ciclo de latencia. Una lectura por paso. El caso peor son
// 2 + NSPR + 7*K pasos (hay que mirar todos y luego cargar K); si no da tiempo
// (pixel_cnt >= limit) el motor se para y quedan sin cargar los sprites de
// menor prioridad.
// ============================================================================

module sprite_engine #(
	parameter K    = 12,		// sprites por linea
	parameter NSPR = 64,		// sprites
	parameter SB   = 6		// bits para numerar los sprites (NSPR <= 2**SB)
) (
	input  wire            clk,			// system_clk
	input  wire            reset,		// activo alto
	input  wire [NSPR-1:0] spr_en,		// sprite activo (POKE 2101, ver SD81.v)
	input  wire [2:0]      sheet_base,	// bloque de 8 KB de la hoja de graficos
	input  wire            hsync,
	input  wire [9:0]      pixel_cnt,
	input  wire [9:0]      limit,		// pixel_cnt a partir del cual no se leen mas
	input  wire            blit_busy,	// el puerto B esta en uso por el doble bufer
	input  wire [8:0]      pos_x,		// posicion del barrido en coordenadas de sprite
	input  wire [8:0]      pos_y,
	input  wire            row_ok,		// la linea que empieza es visible (0..191)
	input  wire [7:0]      v_dout,		// salida del puerto B de la BRAM de sombra
	output reg             ev_rd,		// el motor usa el puerto B
	output reg  [15:0]     ev_raddr,
	output wire            hit,			// algun sprite pone un pixel (distinto de 0) AHORA
	output wire [3:0]      pix			// su indice de color (0 si no hay)
);

	// Direccion en la sombra de un campo de un sprite. Los 32 primeros en
	// $0C00 (6'b000011 es $0C00 >> 10) y los otros 32 en $1800 (6'b000110).
	function [15:0] tbl;
		input [SB-1:0] spr;
		input [4:0]    fld;
		reg   [5:0]    base;
		begin
			base = spr[5] ? 6'b000110 : 6'b000011;
			tbl  = {base, spr[4:0], fld};
		end
	endfunction

	// ------------------------------------------------------------------
	// Slots de linea
	// ------------------------------------------------------------------
	reg [K-1:0] sl_valid = {K{1'b0}};
	reg [8:0]   sl_x   [0:K-1];
	reg [31:0]  sl_dat [0:K-1];		// 8 pixeles de 4 bits, el de la izquierda en [31:28]

	// ------------------------------------------------------------------
	// Maquina de carga (un paso cada dos ciclos)
	// ------------------------------------------------------------------
	localparam [3:0] S_IDLE = 4'd0,	// espera el sincronismo
	                 S_WAIT = 4'd1,	// deja que line_cnt se actualice
	                 S_SCAN = 4'd2,	// pide la Y de cada sprite y mira si corta
	                 S_LX1  = 4'd3,	// X bajo
	                 S_LX2  = 4'd4,	// X alto
	                 S_LP   = 4'd5,	// pagina de la hoja
	                 S_D0   = 4'd6,	// bytes 0..2 de la fila
	                 S_D1   = 4'd7,
	                 S_D2   = 4'd8,
	                 S_D3   = 4'd9;	// byte 3 de la fila y carga del slot

	reg [3:0]    st = S_IDLE;
	reg          tog = 1'b0;
	reg          hs_d = 1'b0;
	reg          hs_pend = 1'b0;
	reg [1:0]    wcnt;
	reg [8:0]    tgt_y;
	reg [SB:0]   nxt;			// siguiente sprite a pedir (bit SB: se acabaron)
	reg [SB-1:0] p;				// sprite cuya Y esta llegando
	reg          ypend;			// hay una lectura de Y en vuelo (de p)
	reg [3:0]    kn;			// slots cargados
	reg [2:0]    row;
	reg [7:0]    lx_l, d0, d1, d2;
	reg          lx_h;
	reg [7:0]    pg;

	wire [8:0] y_diff = tgt_y - {1'b0, v_dout};
	wire       y_hit  = ypend && (y_diff < 9'd8);

	wire       req_en = ~nxt[SB] && spr_en[nxt[SB-1:0]];

	wire stop = (pixel_cnt >= limit) || blit_busy;

	integer j;
	always @(posedge clk) begin
		tog  <= ~tog;
		hs_d <= hsync;

		if (reset) begin
			sl_valid <= {K{1'b0}};
			st       <= S_IDLE;
			ev_rd    <= 1'b0;
			hs_pend  <= 1'b0;
		end else if (tog) begin
			ev_rd <= 1'b0;		// salvo que un estado la pida

			if (st != S_IDLE && stop) begin
				st <= S_IDLE;		// no hay tiempo: los que faltan no se cargan
			end else case (st)
				S_IDLE: begin
					if (hs_pend) begin
						hs_pend  <= 1'b0;
						sl_valid <= {K{1'b0}};	// la linea anterior ya se dibujo
						kn       <= 4'd0;
						wcnt     <= 2'd0;
						st       <= S_WAIT;
					end
				end

				S_WAIT: begin
					wcnt <= wcnt + 2'd1;
					if (wcnt == 2'd1) begin
						tgt_y <= pos_y;
						if (row_ok && !blit_busy) begin
							nxt   <= NSPR - 1;
							ypend <= 1'b0;
							st    <= S_SCAN;
						end else st <= S_IDLE;
					end
				end

				S_SCAN: begin
					if (y_hit) begin
						// corta la linea: pide el X bajo de ese sprite
						row      <= y_diff[2:0];
						ev_rd    <= 1'b1;
						ev_raddr <= tbl(p, 5'd1);
						ypend    <= 1'b0;
						st       <= S_LX1;
					end else if (~nxt[SB]) begin
						// pide la Y del siguiente (si esta activo)
						ev_rd    <= req_en;
						ev_raddr <= tbl(nxt[SB-1:0], 5'd3);
						ypend    <= req_en;
						p        <= nxt[SB-1:0];
						nxt      <= nxt - 1'b1;
					end else begin
						ypend <= 1'b0;			// se acabaron los sprites y la ultima Y no corta
						st    <= S_IDLE;
					end
				end

				S_LX1: begin
					lx_l     <= v_dout;
					ev_rd    <= 1'b1;
					ev_raddr <= tbl(p, 5'd2);
					st       <= S_LX2;
				end

				S_LX2: begin
					lx_h     <= v_dout[0];
					ev_rd    <= 1'b1;
					ev_raddr <= tbl(p, 5'd4);			// pagina de la hoja
					st       <= S_LP;
				end

				S_LP: begin
					pg       <= v_dout;
					ev_rd    <= 1'b1;
					ev_raddr <= {sheet_base, v_dout, row, 2'd0};	// byte 0 de la fila
					st       <= S_D0;
				end

				S_D0: begin
					d0       <= v_dout;
					ev_rd    <= 1'b1;
					ev_raddr <= {sheet_base, pg, row, 2'd1};
					st       <= S_D1;
				end

				S_D1: begin
					d1       <= v_dout;
					ev_rd    <= 1'b1;
					ev_raddr <= {sheet_base, pg, row, 2'd2};
					st       <= S_D2;
				end

				S_D2: begin
					d2       <= v_dout;
					ev_rd    <= 1'b1;
					ev_raddr <= {sheet_base, pg, row, 2'd3};
					st       <= S_D3;
				end

				S_D3: begin
					for (j = 0; j < K; j = j + 1)
						if (kn == j) begin
							sl_x[j]   <= {lx_h, lx_l};
							sl_dat[j] <= {d0, d1, d2, v_dout};
							sl_valid[j] <= 1'b1;
						end
					kn <= kn + 4'd1;
					if (kn == K - 1) st <= S_IDLE;		// los K slots llenos
					else begin
						// sigue con el siguiente sprite sin perder un paso
						ev_rd    <= req_en;
						ev_raddr <= tbl(nxt[SB-1:0], 5'd3);
						ypend    <= req_en;
						p        <= nxt[SB-1:0];
						nxt      <= nxt - 1'b1;
						st       <= S_SCAN;
					end
				end

				default: st <= S_IDLE;
			endcase
		end

		// el flanco de subida del sincronismo se recuerda hasta que el motor
		// pueda atenderlo (puede caer en el ciclo en que no da paso)
		if (hsync & ~hs_d & ~reset) hs_pend <= 1'b1;
	end

	// ------------------------------------------------------------------
	// Salida: el nibble del pixel actual del primer slot (el de mas prioridad)
	// cuyo nibble no sea 0. El slot 0 es el de mayor indice de sprite.
	// ------------------------------------------------------------------
	reg [3:0]  o_pix;
	reg [8:0]  d;
	reg [2:0]  c;
	reg [3:0]  nb;
	integer    k;
	always @(*) begin
		o_pix = 4'd0;
		for (k = K - 1; k >= 0; k = k - 1) begin
			d  = pos_x - sl_x[k];
			c  = d[2:0];
			nb = sl_dat[k][(3'd7 - c) * 4 +: 4];
			if (sl_valid[k] && (d < 9'd8) && (nb != 4'd0))
				o_pix = nb;
		end
	end

	assign pix = o_pix;
	assign hit = (o_pix != 4'd0);

endmodule
