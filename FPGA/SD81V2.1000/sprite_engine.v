// ============================================================================
// sprite_engine.v -- Sprites 8x8 por LINEA, leidos de la RAM de sombra
// ============================================================================
// Sustituye a los slots de sprite con memoria propia (sprite_slot.v, que se
// conserva como referencia del banco de pruebas). La FPGA guarda una copia de
// los NSPR sprites en la RAM de sombra:
//
//     sprites  0..31:  $0C00 + sprite*32 + campo
//     sprites 32..63:  $1800 + (sprite-32)*32 + campo
//
// (campo 0 activo, 1 X bajo, 2 X alto, 3 Y, 4..11 color de cada fila, 12..19
// pixel de cada fila, 20..27 mascara de cada fila, 29 pagina de la hoja), la
// misma que usan los snapshots. El motor la lee en vez de duplicarla.
//
// Cada sprite elige formato con su pagina (POKE 2130):
//
//   pagina 0: formato A, el de siempre. Color y pixel de la tabla, 1 bit por
//       pixel y dos colores por fila.
//   pagina != 0: formato B, 4 bits por pixel y hoja de graficos. El grafico es
//       una celda de 32 bytes de la RAM de sombra (8 filas de 4 bytes, nibble
//       alto = pixel de la izquierda), en {sheet_base, pagina, fila, byte}.
//       Varios sprites pueden compartir celda, y animar es cambiar la pagina.
//       Cada nibble es directamente el color, sin transparente fijo: el 0 es
//       un color mas. El nibble alto del byte de X alto (POKE 2103, que hasta
//       ahora solo usaba el bit 0) dice que color es transparente en este
//       sprite; si nadie lo toca vale 0.
//
// En los dos formatos la mascara (campos 20..27) sale de la tabla, asi que es
// de cada sprite aunque la celda sea compartida: bit a 0, pixel invisible. En B
// un pixel se ve si su bit de mascara esta a 1 y no es el color transparente.
//
//  1. Cuando empieza el sincronismo horizontal (la linea nueva ya esta en
//     line_cnt) recorre los NSPR sprites de mayor a menor indice. Los que estan
//     activos (spr_en, que se borra con el reset) y cortan la linea que se va
//     a dibujar se cargan en uno de K slots de linea, con SU fila ya resuelta
//     a 8 pixeles (color de 4 bits y visible, los dos formatos por igual). Sin
//     color (color_on a 0, salida monocroma) el formato A se carga con tinta 15 y
//     papel 0, asi que el bit 0 del color es el bit de pixel.
//  2. Durante la linea, cada slot de linea compara su X con el pixel actual y
//     saca el bit de su fila.
//
// Sprites extra (POKE 2131 = M, de 0 a 127; 0 = ninguno): M sprites mas, de
// los dos formatos, que no estan en las tablas sino en la propia hoja. El
// extra j es la pagina j del bloque de las hojas, {sheet_base, j, campo}, con
// los mismos campos que un sprite normal (el 29 elige formato; un extra B no
// puede ser su propia celda). No tienen bit de activo: con Y = 0 no se ven.
// Se miran despues del sprite 0, del 0 al M-1, asi que tienen menos prioridad
// que los normales y, entre ellos, gana el de numero MAS BAJO. Cada extra
// cuesta un paso por linea aunque no se vea: si no da tiempo, los que se
// pierden son los extra de numero mas alto.
//
// Prioridad: gana el sprite de indice mas alto. Los sprites se cargan en ese
// orden, asi que el slot 0 es el de mas prioridad. Si en una linea hay mas de
// K sprites, se dibujan los K de indice mas alto y los demas no aparecen en
// ESA linea (como el limite de 8 sprites por linea de la NES, aqui 12).
//
// Reloj: el motor va a system_clk (26 MHz), 4 veces el reloj de pixel en 32
// columnas y 2 en 80, y da un paso cada DOS ciclos: la BRAM tiene un ciclo
// de latencia (la direccion se pone en un paso, se muestrea en el ciclo
// siguiente y el dato se lee en el paso que viene). Una lectura por paso.
//
// Puerto B de la BRAM: lo usa el motor entre el sincronismo horizontal y el
// pixel `limit` (el video no empieza a leer hasta el 126 en 32 columnas y el
// 206 en 80), y no si el doble buffer esta copiando (blit_busy). El caso peor
// son 2 + NSPR + M + 8*K pasos (hay que mirar todos y luego cargar K sprites de
// formato B; uno de formato A son 6 pasos); si no da tiempo (pixel_cnt >= limit)
// el motor se para y quedan sin cargar los sprites de menor prioridad.
// ============================================================================

module sprite_engine #(
	parameter K    = 12,		// sprites por linea
	parameter NSPR = 64,		// sprites
	parameter SB   = 6		// bits para numerar los sprites (NSPR <= 2**SB)
) (
	input  wire            clk,			// system_clk
	input  wire            reset,		// activo alto
	input  wire [NSPR-1:0] spr_en,		// sprite activo (POKE 2101, ver SD81.v)
	input  wire [2:0]      sheet_base,	// bloque de 8 KB de las hojas (POKE 2129)
	input  wire [6:0]      extra,		// sprites extra (POKE 2131), en la hoja
	input  wire            color_on,		// salida en color (color_enable de SD81.v)
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
	output wire            hit,			// algun sprite pone un pixel AHORA
	output wire            pixel,		// valor del pixel del sprite que gana: solo cuenta en monocromo
	output wire [7:0]      color		// color del pixel que gana, repetido en tinta y papel
);

	// Direccion en la sombra de un campo de un sprite. Los 32 primeros en
	// $0C00 (6'b000011 es $0C00 >> 10) y los otros 32 en $1800 (6'b000110).
	// Con el bit 7 a 1 es un extra: ~spr[6:0] es su numero y su pagina.
	function [15:0] tbl;
		input [7:0]    spr;
		input [4:0]    fld;
		reg   [5:0]    base;
		begin
			base = spr[5] ? 6'b000110 : 6'b000011;
			tbl  = spr[7] ? {sheet_base, 1'b0, ~spr[6:0], fld} : {base, spr[4:0], fld};
		end
	endfunction

	// ------------------------------------------------------------------
	// Slots de linea
	// ------------------------------------------------------------------
	reg [K-1:0] sl_valid = {K{1'b0}};
	reg [8:0]   sl_x   [0:K-1];
	reg [31:0]  sl_nib [0:K-1];		// color de los 8 pixeles, el de la izquierda en [31:28]
	reg [7:0]   sl_vis [0:K-1];		// pixel visible (bit 7 = el de la izquierda)

	// ------------------------------------------------------------------
	// Maquina de carga (un paso cada dos ciclos)
	// ------------------------------------------------------------------
	localparam [3:0] S_IDLE = 4'd0,	// espera el sincronismo
	                 S_WAIT = 4'd1,	// deja que line_cnt se actualice
	                 S_SCAN = 4'd2,	// pide la Y de cada sprite y mira si corta
	                 S_LX1  = 4'd3,	// X bajo
	                 S_LX2  = 4'd4,	// X alto
	                 S_LP   = 4'd5,	// pagina
	                 S_LM   = 4'd6,	// mascara de la fila
	                 S_LC   = 4'd7,	// A: color de la fila
	                 S_LD   = 4'd8,	// A: pixel de la fila y carga del slot
	                 S_D0   = 4'd9,	// B: bytes 0..2 de la fila
	                 S_D1   = 4'd10,
	                 S_D2   = 4'd11,
	                 S_D3   = 4'd12;	// B: byte 3 de la fila y carga del slot

	reg [3:0]    st = S_IDLE;
	reg          tog = 1'b0;
	reg          hs_d = 1'b0;
	reg          hs_pend = 1'b0;
	reg [1:0]    wcnt;
	reg [8:0]    tgt_y;
	reg [7:0]    nxt;			// siguiente sprite a pedir: 63..0 y luego -1..-M (los extra 0..M-1)
	reg [7:0]    p;				// sprite cuya Y esta llegando (como nxt)
	reg          ypend;			// hay una lectura de Y en vuelo (de p)
	reg [3:0]    kn;			// slots cargados
	reg [2:0]    row;
	reg [7:0]    lx_l, lmsk, d0, d1, d2;	// d0: en A, el color de la fila
	reg          lx_h;
	reg [3:0]    ltr;			// color transparente del sprite (formato B)
	reg [7:0]    pg;
	wire         fmt_a = (pg == 8'd0);

	// La fila que se esta cargando, resuelta a 8 pixeles. Solo vale en el ultimo paso (S_LD o S_D3), con el
	// ultimo byte en v_dout.
	wire [31:0]  raw = {d0, d1, d2, v_dout};
	wire [7:0]   lcol = color_on ? d0 : 8'hF0;
	reg  [31:0]  ld_nib;
	reg  [7:0]   ld_vis;
	integer      i;
	always @(*) begin
		for (i = 0; i < 8; i = i + 1) begin
			if (fmt_a) begin
				ld_nib[i*4 +: 4] = v_dout[i] ? lcol[7:4] : lcol[3:0];
				ld_vis[i]        = lmsk[i];
			end else begin
				ld_nib[i*4 +: 4] = raw[i*4 +: 4];
				ld_vis[i]        = lmsk[i] & (raw[i*4 +: 4] != ltr);
			end
		end
	end

	wire [8:0] y_diff = tgt_y - {1'b0, v_dout};
	wire       y_hit  = ypend && (y_diff < 9'd8);

	wire       scan_end = nxt[7] && ((~nxt[6:0] & 7'h7F) == extra);	// ya se miraron los M extra
	wire       req_en = ~scan_end && (nxt[7] || spr_en[nxt[5:0]]);

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
							nxt   <= 8'd63;
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
					end else if (~scan_end) begin
						// pide la Y del siguiente (si esta activo)
						ev_rd    <= req_en;
						ev_raddr <= tbl(nxt, 5'd3);
						ypend    <= req_en;
						p        <= nxt;
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
					ltr      <= v_dout[7:4];
					ev_rd    <= 1'b1;
					ev_raddr <= tbl(p, 5'd29);					// pagina (campo 29)
					st       <= S_LP;
				end

				S_LP: begin
					pg       <= v_dout;
					ev_rd    <= 1'b1;
					ev_raddr <= tbl(p, 5'd20 + {2'b00, row});		// mascara de la fila (campo 20 + fila)
					st       <= S_LM;
				end

				S_LM: begin
					lmsk     <= v_dout;
					ev_rd    <= 1'b1;
					if (fmt_a) begin
						ev_raddr <= tbl(p, 5'd4 + {2'b00, row});	// color de la fila (campo 4 + fila)
						st       <= S_LC;
					end else begin
						ev_raddr <= {sheet_base, pg, row, 2'd0};	// byte 0 de la fila
						st       <= S_D0;
					end
				end

				S_LC: begin
					d0       <= v_dout;
					ev_rd    <= 1'b1;
					ev_raddr <= tbl(p, 5'd12 + {2'b00, row});		// pixel de la fila (campo 12 + fila)
					st       <= S_LD;
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

				S_LD, S_D3: begin
					for (j = 0; j < K; j = j + 1)
						if (kn == j) begin
							sl_x[j]   <= {lx_h, lx_l};
							sl_nib[j] <= ld_nib;
							sl_vis[j] <= ld_vis;
							sl_valid[j] <= 1'b1;
						end
					kn <= kn + 4'd1;
					if (kn == K - 1 || scan_end) st <= S_IDLE;	// los K slots llenos, o ya no quedan sprites que mirar
					// (scan_end es una igualdad: si se pasara de largo, seguiria leyendo detras del ultimo extra)
					else begin
						// sigue con el siguiente sprite sin perder un paso
						ev_rd    <= req_en;
						ev_raddr <= tbl(nxt, 5'd3);
						ypend    <= req_en;
						p        <= nxt;
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
	// Salida: lo que hacia cada slot de sprite, ahora sobre los slots de linea.
	// El slot 0 es el de mas prioridad (el de mayor indice de sprite).
	// ------------------------------------------------------------------
	reg        o_hit;
	reg [3:0]  o_nib;
	reg [8:0]  d;
	reg [2:0]  c;
	integer    k;
	always @(*) begin
		o_hit = 1'b0;
		o_nib = 4'd0;
		for (k = K - 1; k >= 0; k = k - 1) begin
			d = pos_x - sl_x[k];
			c = d[2:0];
			if (sl_valid[k] && (d < 9'd8) && sl_vis[k][3'd7 - c]) begin
				o_hit = 1'b1;
				o_nib = sl_nib[k][(3'd7 - c) * 4 +: 4];
			end
		end
	end

	assign hit   = o_hit;
	assign pixel = o_nib[0];
	assign color = {o_nib, o_nib};		// tinta y papel: el pixel ya esta resuelto a su color

endmodule
