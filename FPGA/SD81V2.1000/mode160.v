`timescale 1ns / 1ps
// Modo de video 160x100 a 8 bits por pixel (POKE 2045,175), 256 colores a la vez.
//
//  - Pantalla: 160 x 100 bytes ({HFILE[15:13],13'b0} + fila*160 + columna, 16.000 bytes en dos bloques de 8 KB),
//    un byte por pixel = indice de la paleta. Cada fila se repite en dos lineas de TV (200 lineas, desde la 58).
//  - Pixel: 3 ciclos de pixel_clk a 13 MHz (4,33 MHz, 480 ciclos por linea); la ventana empieza en x_start.
//    Usa la temporizacion de 80 columnas (sf80_en = 1), solo cambia lo que se saca en la ventana.
//  - Paleta: 256 entradas de 16 bits, RAM distribuida, que el Z80 carga con tres POKEs:
//        2063,n    puntero de la paleta
//        2064,lo   byte bajo  {G[3:0],B[3:0]}  (se guarda)
//        2065,hi   byte alto  {BRIGHT[3:0],R[3:0]}: escribe la entrada y el puntero avanza
//    Cada campo es la densidad de pulsos (0-15 -> n/15) de su pin: R, G, B y BRIGHT.
//  - Salida: un modulador de densidad de pulsos de primer orden por pin (VRED, VGREEN, VBLUE, BRIGHT), a 208 MHz
//    (CLK104: el nombre es historico, el DCM de CLK100 da 208 MHz), de forma que la red de resistencias y el televisor dan niveles intermedios (ver claude/planning/pdm_color.md).
//    Fuera de la ventana sale el color del borde (los 4 bits de Chroma), y en el borrado todo a 0.
//  - Paleta de 16 colores para los demas modos (texto, HiRes, Spectrum, sprites, borde): tabla de 16 entradas, indexada por el
//    color de 4 bits {BRIGHT,G,R,B} que sale del camino normal (c4) y con el mismo formato de entrada que la de 256. Se carga con
//    2063 (puntero, 4 bits), 2064 (byte bajo) y 2066 (byte alto: escribe y avanza). pal16_en la activa (POKE 2067,1); desactivada, el
//    camino normal sigue saliendo por los pines sin tocar (colores por defecto). La tabla arranca con los colores digitales de Chroma
//    (0 o 15 en cada canal), de modo que activarla sin cargar nada no cambia nada.
//
// Variante 4 bpp (POKE 2045,176, entrada m4): 160x200, 80 bytes por fila, 16.000 bytes; el nibble alto es el pixel de la izquierda y
// el indice (0-15) entra en pal16 (misma tabla y misma carga que los demas modos). Las 200 filas son lineas de TV distintas.
// Con m4 = 0 el comportamiento es el de siempre.
// Plano de texto (solo 4 bpp, txt_en): rejilla de 40x25 celdas de 4x8 pixeles de imagen (6x8 de glifo a 2 ciclos de 13 MHz, 12 ciclos
// por celda), encima de la imagen y de los sprites. En el bloque txt_blk: codigos a +0 (1.000 bytes, fila*40+columna) y fuente a +$0300
// (+codigo*8+fila de glifo, codigos 32-255 = 224 glifos propios, bit 7 del byte = pixel de la izquierda, 6 pixeles). Codigo < 32 = celda
// transparente; el bit 7 del codigo ademas invierte el glifo (tinta y papel cambiados), sea cual sea su definicion; el resto, opacos. Las lecturas del texto usan los huecos de
// los pixeles impares, en los que el 4 bpp ya no necesita leer la imagen.
// Sprites (solo 4 bpp): spr_x / spr_y / spr_row_ok sustituyen a la posicion que usa el motor de sprites (sprite_engine.v) y el pixel
// del sprite (spr_hit, color en c4) pisa al del fondo; coordenadas del sprite = pixel de la imagen + 32.
//
// El motor lee la sombra por el puerto B (v_addr), un byte por pixel, con un pixel de adelanto.
module mode160 (
	input clk,					// system_clk, 26 MHz
	input clk104,				// 208 MHz (CLK104, el DCM de CLK100; sin relacion de fase con clk)
	input rst,					// activo a 1
	input en,					// modo activo (sf160_en)
	input m4,					// 1 = 160x200 a 4 bpp (POKE 2045,176): dos pixeles por byte (nibble alto primero), color por pal16; solo cuenta si en = 1
	input [9:0] pixel_cnt,		// contador de pixel_clk (13 MHz en este modo), 0..827
	input [8:0] line_cnt,		// linea de TV (0..311)
	input [9:0] x_start,		// ciclo de pixel_cnt en que sale el primer pixel
	input [2:0] base_blk,		// bloque de 8 KB donde empieza la imagen (HFILE[15:13])
	input [7:0] v_dout,			// puerto B de la sombra
	output reg [15:0] v_addr,
	// paleta: POKEs capturados en el flanco de bajada de wclk (iclock)
	input wclk,
	input pal_idx_wr,
	input pal_lo_wr,
	input pal_hi_wr,
	input pal16_hi_wr,			// byte alto de la paleta de 16 (escribe pal16[pal_idx[3:0]] y avanza el puntero)
	input [7:0] wdata,
	input blank_n,				// 0 durante sincronismos y back porch
	input [3:0] border,			// color del borde: {bright,G,R,B} (formato Chroma)
	input pal16_en,				// paleta de 16 colores activa (solo cuenta si en = 0)
	input [3:0] c4,				// color de 4 bits del camino normal: {bright,G,R,B} (con spr_hit, el del sprite)
	input spr_hit,				// 4 bpp: el motor de sprites pone un pixel ahora (combinacional con spr_x); su color llega por c4
	output reg [8:0] spr_x,		// 4 bpp: posicion del pixel en pantalla en coordenadas de sprite (columna + 32)
	output [8:0] spr_y,			// 4 bpp: fila de la linea en coordenadas de sprite (fila + 32)
	output spr_row_ok,			// la linea es de la imagen
	input txt_en,				// 4 bpp: plano de texto 40x25 activo (POKE 2069)
	input [7:0] txt_col,		// color del texto: {papel[3:0], tinta[3:0]} (indices de pal16) (POKE 2068)
	input [2:0] txt_blk,		// bloque de 8 KB con los codigos y la fuente del texto (POKE 2069)
output reg vred,
	output reg vgreen,
	output reg vblue,
	output reg vbright,
	// observacion (banco de pruebas)
	output img_on_o,
	output [7:0] idx_cur_o,
	output [15:0] lvl_o,
	output [15:0] lvl16_o
);
	localparam [8:0] Y0 = 9'd58;				// primera linea de la imagen (centrada en 62..253)
	localparam [8:0] LINES = 9'd200;

	// ---------------- paleta ----------------
	// Por defecto RGB 3-3-2 sin brillo (rojo y verde 0-7, azul 0-3) para ver algo sin cargar la paleta
	function [3:0] l3;
		input [2:0] v;
		case (v)
			3'd0: l3 = 4'd0;   3'd1: l3 = 4'd2;   3'd2: l3 = 4'd4;   3'd3: l3 = 4'd6;
			3'd4: l3 = 4'd9;   3'd5: l3 = 4'd11;  3'd6: l3 = 4'd13;  default: l3 = 4'd15;
		endcase
	endfunction
	function [15:0] def_entry;
		input [7:0] k;
		def_entry = {4'd0, l3(k[7:5]), l3(k[4:2]), (k[1:0] == 2'd0 ? 4'd0 : k[1:0] == 2'd1 ? 4'd5 : k[1:0] == 2'd2 ? 4'd10 : 4'd15)};
	endfunction
	reg [15:0] pal [0:255];
	integer k;
	initial for (k = 0; k < 256; k = k + 1) pal[k] = def_entry(k[7:0]);

	// Paleta de 16: por defecto, los colores digitales de Chroma (cada canal a 0 o 15, BRIGHT a 0 o 15)
	reg [15:0] pal16 [0:15];
	integer k16;
	initial for (k16 = 0; k16 < 16; k16 = k16 + 1)
		pal16[k16] = {(k16[3] ? 4'd15 : 4'd0), (k16[1] ? 4'd15 : 4'd0), (k16[2] ? 4'd15 : 4'd0), (k16[0] ? 4'd15 : 4'd0)};

	reg [7:0] pal_idx = 8'd0;
	reg [7:0] pal_lo = 8'd0;
	always @(negedge wclk) begin
		if (pal_hi_wr) pal[pal_idx] <= {wdata, pal_lo};
		if (pal16_hi_wr) pal16[pal_idx[3:0]] <= {wdata, pal_lo};
	end
	always @(negedge wclk or posedge rst) begin
		if (rst) begin
			pal_idx <= 8'd0;
			pal_lo <= 8'd0;
		end else begin
			if (pal_idx_wr) pal_idx <= wdata;
			if (pal_lo_wr) pal_lo <= wdata;
			if (pal_hi_wr | pal16_hi_wr) pal_idx <= pal_idx + 8'd1;
		end
	end

	// ---------------- captacion de pixeles (clk, 26 MHz) ----------------
	reg [9:0] pc_d = 10'd0;
	reg tick = 1'b0;						// un ciclo de clk despues de cada cambio de pixel_cnt (cada ciclo de 13 MHz)
	reg run = 1'b0;						// de un pixel antes de la ventana hasta el final de la imagen
	reg [1:0] sub = 2'd0;					// ciclo dentro del pixel (0..2)
	reg [7:0] pcnt = 8'd0;				// pixel de la linea: 0 = lectura del primero, 1..160 = los pixeles, 161 = fin
	reg [15:0] ptr = 16'd0;
	reg [2:0] sc = 3'd0;
	reg rd_pend = 1'b0;
	reg [7:0] idx_next = 8'd0;
	reg [7:0] idx_cur = 8'd0;
	reg [9:0] tptr = 10'd0;				// texto: siguiente codigo a pedir
	reg [7:0] tcode = 8'd0;				// texto: codigo de la celda que se prepara
	reg [7:0] tglyph = 8'd0;			// texto: fila de glifo de esa celda
	reg [7:0] gsh = 8'd0;				// texto: glifo que sale (bit 7 = pixel actual)
	reg cact = 1'b0, cinv = 1'b0, gph = 1'b0;
reg img_on = 1'b0;
	reg img_v = 1'b0;						// img_on retrasado un ciclo: el que viaja con lvl a la salida
	reg ev_d = 1'b0;
	reg [15:0] lvl = 16'd0;
	reg [15:0] lvl16 = 16'd0;				// color del camino normal pasado por pal16

	wire [8:0] ly = line_cnt - Y0;
	wire in_y = (line_cnt >= Y0) && (line_cnt < Y0 + LINES);
	wire m4e = m4 & en;
	wire [7:0] row = m4e ? ly[7:0] : {ly[7:1], 1'b0};			// 4 bpp: una fila por linea; 8 bpp: cada fila se repite en dos lineas
	wire [13:0] rowoff = {row, 6'b0} + {row, 4'b0};				// fila * 80 (8 bpp: fila_doble * 80 = fila * 160)
	wire [15:0] rowbase = {base_blk, 13'b0} + {2'b0, rowoff};
	wire [9:0] x_arm = x_start - (m4e ? 10'd16 : 10'd4);		// 4 bpp: cuatro pixeles mas pronto, para pedir el texto de la primera celda
	wire te = txt_en & m4e;
	wire [4:0] trow = ly[7:3];
	wire [9:0] trowoff = {trow, 5'b0} + {2'b0, trow, 3'b0};		// fila de texto * 40
	wire [11:0] gaddr = {tcode, ly[2:0]} + 12'h300;
	wire txt_on = te & img_on & cact;
	wire tg = gsh[7] ^ cinv;
	assign spr_y = line_cnt - (Y0 - 9'd32);
	assign spr_row_ok = in_y;
	wire ev = tick & run & (sub == 2'd0);							// principio de pixel (el primero, un pixel antes de la ventana)

	always @(posedge clk) begin
		pc_d <= pixel_cnt;
		tick <= (pixel_cnt != pc_d);
		ev_d <= ev;
		if (rst | ~en) begin
			run <= 1'b0;
			img_on <= 1'b0;
			rd_pend <= 1'b0;
		end else begin
			if (tick) begin
				if (!run) begin
					if (pixel_cnt == x_arm && in_y) begin
						run <= 1'b1;
						sub <= 2'd0;
						pcnt <= m4e ? 8'd252 : 8'd0;		// 4 bpp: -4 .. -1 pide solo texto
							ptr <= rowbase;
							tptr <= trowoff;
end
				end else begin
					if (sub == 2'd2) begin
						sub <= 2'd0;
						pcnt <= pcnt + 8'd1;
					end else sub <= sub + 2'd1;
						if (te) begin
							gph <= ~gph;
							if (gph) gsh <= {gsh[6:0], 1'b0};
						end
end
			end
			if (ev) begin
				if (pcnt <= 8'd159 && !(m4e && pcnt[0])) begin		// pedir el pixel siguiente (4 bpp: un byte cada dos pixeles)
						v_addr <= ptr;
						ptr <= ptr + 16'd1;
						rd_pend <= 1'b1;
						sc <= 3'd0;
					end else if (te && pcnt[0] && (pcnt >= 8'd250 || pcnt <= 8'd155)) begin		// hueco impar: codigo (4k-3) y fila de glifo (4k-1) de la celda k
						if (pcnt[1]) v_addr <= {txt_blk, 1'b0, gaddr};
						else begin
							v_addr <= {txt_blk, 3'b000, tptr};
							tptr <= tptr + 10'd1;
						end
						rd_pend <= 1'b1;
						sc <= 3'd0;
					end
					if (te && pcnt < 8'd160 && pcnt[1:0] == 2'b01) begin		// principio de celda: el glifo y su codigo pasan a la salida
						gsh <= tglyph;
						cact <= (tcode[7:5] != 3'b000);
						cinv <= tcode[7];
						gph <= 1'b0;
					end
if (pcnt >= 8'd1) begin
						idx_cur <= m4e ? {4'd0, pcnt[0] ? idx_next[7:4] : idx_next[3:0]} : idx_next;
						spr_x <= (pcnt == 8'd1) ? 9'd32 : spr_x + 9'd1;		// coordenada del pixel que entra en idx_cur
					end
				if (pcnt == 8'd1) img_on <= 1'b1;
				if (pcnt == 8'd161) begin
					img_on <= 1'b0;
					run <= 1'b0;
				end
			end
			if (rd_pend) begin
				sc <= sc + 3'd1;
				if (sc == 3'd2) begin				// el BRAM tarda dos flancos en dar el dato
					if (m4e && pcnt[0]) begin
							if (pcnt[1]) tglyph <= v_dout;
							else tcode <= v_dout;
						end else idx_next <= v_dout;
					rd_pend <= 1'b0;
				end
			end
		end
		if (ev_d) lvl <= pal[idx_cur];
		img_v <= img_on;
		lvl16 <= pal16[m4e ? (txt_on ? (tg ? txt_col[3:0] : txt_col[7:4]) : (spr_hit & img_on) ? c4 : idx_cur[3:0]) : c4];
	end

	// img_on sube un ciclo de clk antes de que lvl tome el nivel del pixel (idx_cur -> ev_d -> lvl): sin este retardo,
	// el primer ciclo de cada fila salia con el nivel del ultimo pixel de la fila anterior (una columna a la izquierda)
	assign img_on_o = img_v;
	assign idx_cur_o = idx_cur;
	assign lvl_o = lvl;
	assign lvl16_o = lvl16;

	// ---------------- paso a 208 MHz y modulacion ----------------
	reg blank_r = 1'b0;
	reg [3:0] border_r = 4'd0;
	reg sel16 = 1'b0;						// la salida sale de pal16: use16, o el modo 160 a 4 bpp
	reg use16 = 1'b0;						// paleta de 16 activa (y no estamos en el modo 160)
	always @(posedge clk) begin
		blank_r <= blank_n;
		border_r <= border;
		use16 <= pal16_en & ~en;
			sel16 <= (pal16_en & ~en) | m4e;
	end
	reg [21:0] s1 = 22'd0;
	reg [21:0] s2 = 22'd0;
	always @(posedge clk104) begin
		s1 <= {sel16 ? lvl16 : lvl, use16 | img_v, blank_r, border_r};
		s2 <= s1;
	end
	wire [3:0] lev_bright = s2[21:18];
	wire [3:0] lev_r = s2[17:14];
	wire [3:0] lev_g = s2[13:10];
	wire [3:0] lev_b = s2[9:6];
	wire s2_img = s2[5];
	wire s2_blank = s2[4];
	wire [3:0] s2_border = s2[3:0];

	reg [3:0] acc_r = 4'd0, acc_g = 4'd0, acc_b = 4'd0, acc_w = 4'd0;
	reg pd_r = 1'b0, pd_g = 1'b0, pd_b = 1'b0, pd_w = 1'b0;
	wire [4:0] sum_r = acc_r + lev_r;
	wire [4:0] sum_g = acc_g + lev_g;
	wire [4:0] sum_b = acc_b + lev_b;
	wire [4:0] sum_w = acc_w + lev_bright;
	wire wr = (sum_r >= 5'd15);
	wire wg = (sum_g >= 5'd15);
	wire wb = (sum_b >= 5'd15);
	wire ww = (sum_w >= 5'd15);
	reg img_d = 1'b0, blank_d = 1'b0;
	reg [3:0] border_d = 4'd0;
	always @(posedge clk104) begin
		acc_r <= wr ? sum_r - 5'd15 : sum_r;
		acc_g <= wg ? sum_g - 5'd15 : sum_g;
		acc_b <= wb ? sum_b - 5'd15 : sum_b;
		acc_w <= ww ? sum_w - 5'd15 : sum_w;
		pd_r <= wr;
		pd_g <= wg;
		pd_b <= wb;
		pd_w <= ww;
		img_d <= s2_img;
		blank_d <= s2_blank;
		border_d <= s2_border;
		// borde: R = bit 1, G = bit 2, B = bit 0, BRIGHT = bit 3 (como el camino normal de Chroma)
		vred    <= blank_d ? (img_d ? pd_r : border_d[1]) : 1'b0;
		vgreen  <= blank_d ? (img_d ? pd_g : border_d[2]) : 1'b0;
		vblue   <= blank_d ? (img_d ? pd_b : border_d[0]) : 1'b0;
		vbright <= blank_d ? (img_d ? pd_w : border_d[3]) : 1'b0;
	end
endmodule
