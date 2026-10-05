// ============================================================================
// sprite_engine.v -- Sprites 8x8 por LINEA, leidos de la RAM de sombra
// ============================================================================
// Sustituye a los 32 sprite_slot (que guardaban cada sprite en memoria
// distribuida y comparaban su posicion en paralelo en cada pixel). La FPGA ya
// guarda una copia completa de los 32 sprites en la RAM de sombra, en
//     $0C00 + sprite*32 + campo
// (campo 0 activo, 1 X bajo, 2 X alto, 3 Y, 4..11 color de cada fila, 12..19
// pixel de cada fila, 20..27 mascara de cada fila), la misma que usan los
// snapshots. El motor la lee en vez de duplicarla:
//
//  1. Cuando empieza el sincronismo horizontal (la linea nueva ya esta en
//     line_cnt) recorre los 32 sprites de mayor a menor indice. Los que estan
//     activos (spr_en, que se borra con el reset como los slots antiguos) y
//     cortan la linea que se va a dibujar se cargan en uno de K slots de
//     linea, con los bytes de SU fila: X, color, pixel y mascara.
//  2. Durante la linea, cada slot de linea hace lo que hacia un sprite_slot:
//     compara la X con el pixel actual y saca el bit de su fila.
//
// Prioridad: gana el sprite de indice mas alto (como antes). Los sprites se
// cargan en ese orden, asi que el slot 0 es el de mas prioridad. Si en una
// linea hay mas de K sprites, se dibujan los K de indice mas alto y los demas
// no aparecen en ESA linea (como el limite de 8 sprites por linea de la NES).
//
// Lecturas: el puerto B de la BRAM no lo usa el vídeo entre el sincronismo
// horizontal y el pixel 120 (el vídeo Superfast empieza a leer en el 126; los
// sprites empiezan a verse en el 142), asi que el motor lo toma ahi, a una
// lectura por reloj de pixel. El caso peor son 32 + 7*K = 88 relojes; si no
// da tiempo (pixel_cnt >= LIMIT) se para y quedan sin cargar los sprites de
// menor prioridad.
//
// Todo va en el dominio de pixel_clk, como los slots antiguos: la direccion
// sale de un registro en un flanco de pixel_clk, la BRAM (system_clk, 2 o 4
// veces mas rapido) la muestrea y el dato esta estable antes del siguiente.
// ============================================================================

module sprite_engine #(
	parameter K     = 8,		// sprites por linea
	parameter LIMIT = 10'd120	// pixel_cnt a partir del cual no se leen mas
) (
	input  wire        pixel_clk,
	input  wire        reset,		// activo alto
	input  wire [31:0] spr_en,		// sprite activo (POKE 2101, ver SD81.v)
	input  wire        hsync,
	input  wire [9:0]  pixel_cnt,
	input  wire        blit_busy,	// el puerto B esta en uso por el doble bufer
	input  wire [8:0]  pos_x,		// posicion del barrido en coordenadas de sprite
	input  wire [8:0]  pos_y,
	input  wire        row_ok,		// la linea que empieza es visible (0..191)
	input  wire [7:0]  v_dout,		// salida del puerto B de la BRAM de sombra
	output reg         ev_rd,		// el motor usa el puerto B
	output reg  [15:0] ev_raddr,
	output wire        hit,			// algun sprite pone un pixel AHORA
	output wire        pixel,		// valor del pixel del sprite que gana
	output wire [7:0]  color		// color de la fila del sprite que gana
);

	localparam [5:0] TBL_HI = 6'b000011;		// $0C00 >> 10

	function [15:0] tbl;
		input [4:0] spr;
		input [4:0] fld;
		tbl = {TBL_HI, spr, fld};
	endfunction

	// ------------------------------------------------------------------
	// Slots de linea
	// ------------------------------------------------------------------
	reg [K-1:0] sl_valid = {K{1'b0}};
	reg [8:0]   sl_x   [0:K-1];
	reg [7:0]   sl_col [0:K-1];
	reg [7:0]   sl_dat [0:K-1];
	reg [7:0]   sl_msk [0:K-1];

	// ------------------------------------------------------------------
	// Maquina de carga
	// ------------------------------------------------------------------
	localparam [3:0] S_IDLE = 4'd0,	// espera el sincronismo
	                 S_WAIT = 4'd1,	// deja que line_cnt se actualice
	                 S_SCAN = 4'd2,	// pide la Y de cada sprite y mira si corta
	                 S_LX1  = 4'd3,	// X bajo
	                 S_LX2  = 4'd4,	// X alto
	                 S_LC   = 4'd5,	// color de la fila
	                 S_LD   = 4'd6,	// pixel de la fila
	                 S_LM   = 4'd7;	// mascara de la fila y carga del slot

	reg [3:0] st = S_IDLE;
	reg [1:0] wcnt;
	reg       hs_d = 1'b0;
	reg [8:0] tgt_y;
	reg [5:0] nxt;			// siguiente sprite a pedir (bit 5: se acabaron)
	reg [4:0] p;			// sprite cuya Y esta llegando
	reg       ypend;		// hay una lectura de Y en vuelo (de p)
	reg [3:0] kn;			// slots cargados
	reg [2:0] row;
	reg [7:0] lx_l, lcol, ldat;
	reg       lx_h;

	wire [8:0] y_diff = tgt_y - {1'b0, v_dout};
	wire       y_hit  = ypend && (y_diff < 9'd8);

	wire       req_en = ~nxt[5] && spr_en[nxt[4:0]];

	wire hs_rise = hsync & ~hs_d;
	wire stop    = (pixel_cnt >= LIMIT) || blit_busy;

	integer j;
	always @(posedge pixel_clk) begin
		hs_d <= hsync;

		if (reset) begin
			sl_valid <= {K{1'b0}};
			st       <= S_IDLE;
			ev_rd    <= 1'b0;
		end else begin
			ev_rd <= 1'b0;		// salvo que un estado la pida

			if (st != S_IDLE && stop) begin
				st <= S_IDLE;		// no hay tiempo: los que faltan no se cargan
			end else case (st)
				S_IDLE: begin
					if (hs_rise) begin
						sl_valid <= {K{1'b0}};	// la linea anterior ya se dibujo
						kn       <= 4'd0;
						wcnt     <= 2'd0;
						st       <= S_WAIT;
					end
				end

				S_WAIT: begin
					wcnt <= wcnt + 2'd1;
					if (wcnt == 2'd2) begin
						tgt_y <= pos_y;
						if (row_ok && !blit_busy) begin
							nxt   <= 6'd31;
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
					end else if (~nxt[5]) begin
						// pide la Y del siguiente (si esta activo)
						ev_rd    <= req_en;
						ev_raddr <= tbl(nxt[4:0], 5'd3);
						ypend    <= req_en;
						p        <= nxt[4:0];
						nxt      <= nxt - 6'd1;
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
					ev_raddr <= tbl(p, 5'd4 + {2'b00, row});		// color de la fila (campo 4 + fila)
					st       <= S_LC;
				end

				S_LC: begin
					lcol     <= v_dout;
					ev_rd    <= 1'b1;
					ev_raddr <= tbl(p, 5'd12 + {2'b00, row});		// pixel de la fila (campo 12 + fila)
					st       <= S_LD;
				end

				S_LD: begin
					ldat     <= v_dout;
					ev_rd    <= 1'b1;
					ev_raddr <= tbl(p, 5'd20 + {2'b00, row});		// mascara de la fila (campo 20 + fila)
					st       <= S_LM;
				end

				S_LM: begin
					for (j = 0; j < K; j = j + 1)
						if (kn == j) begin
							sl_x[j]   <= {lx_h, lx_l};
							sl_col[j] <= lcol;
							sl_dat[j] <= ldat;
							sl_msk[j] <= v_dout;
							sl_valid[j] <= 1'b1;
						end
					kn <= kn + 4'd1;
					if (kn == K - 1) st <= S_IDLE;		// los K slots llenos
					else begin
						// sigue con el siguiente sprite sin perder un reloj
						ev_rd    <= req_en;
						ev_raddr <= tbl(nxt[4:0], 5'd3);
						ypend    <= req_en;
						p        <= nxt[4:0];
						nxt      <= nxt - 6'd1;
						st       <= S_SCAN;
					end
				end

				default: st <= S_IDLE;
			endcase
		end
	end

	// ------------------------------------------------------------------
	// Salida: lo que hacia cada sprite_slot, ahora sobre los slots de linea.
	// El slot 0 es el de mas prioridad (el de mayor indice de sprite).
	// ------------------------------------------------------------------
	reg        o_hit;
	reg        o_pix;
	reg [7:0]  o_col;
	reg [8:0]  d;
	reg [2:0]  c;
	integer    k;
	always @(*) begin
		o_hit = 1'b0;
		o_pix = 1'b0;
		o_col = 8'hF0;
		for (k = K - 1; k >= 0; k = k - 1) begin
			d = pos_x - sl_x[k];
			c = d[2:0];
			if (sl_valid[k] && (d < 9'd8) && sl_msk[k][3'd7 - c]) begin
				o_hit = 1'b1;
				o_pix = sl_dat[k][3'd7 - c];
				o_col = sl_col[k];
			end
		end
	end

	assign hit   = o_hit;
	assign pixel = o_pix;
	assign color = o_col;

endmodule
