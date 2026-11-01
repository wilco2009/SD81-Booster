`timescale 1ns / 1ps
// tb_mode160_txt -- plano de texto 40x25 de mode160 (4 bpp) sobre la imagen. Codigos y fuente al azar en el bloque 6.
//   vlog mode160.v tb_mode160_txt.v ; vsim -c tb_mode160_txt -do "run -all; quit"
// Celdas de 12 ciclos de 13 MHz (offset = pixel_cnt - XS): el pixel de glifo j de la celda k ocupa offset 12k+2j+2 .. +3;
// se muestrea en el primero (con GOFF=3 tambien pasa). Celda opaca (codigo >= 32): color = bit ? tinta : papel (bit 7 del codigo invierte);
// transparente: sale la imagen (muestreada a mitad de pixel, offset 3n+2).
module tb_mode160_txt;
	parameter WF = 1;
	parameter WL = 22;
	parameter GOFF = 2;
	reg clk = 1'b0;  always #19.23 clk = ~clk;
	reg clk104 = 1'b0;  always #2.4 clk104 = ~clk104;
	reg [25:0] cnt26 = 26'd0;
	always @(posedge clk) cnt26 <= cnt26 + 1'b1;
	wire pclk = cnt26[0];
	wire iclk = cnt26[2];
	reg [9:0] pixel_cnt = 10'd0;
	always @(posedge pclk) pixel_cnt <= (pixel_cnt == 10'd827) ? 10'd0 : pixel_cnt + 1'b1;
	wire [8:0] HSYNCcnt = pixel_cnt[9:1];
	wire hsync = (HSYNCcnt >= 11) && (HSYNCcnt <= 42);
	reg [8:0] line_cnt = 9'd0;
	reg old_hsync = 1'b0;
	always @(posedge clk) begin
		old_hsync <= hsync;
		if (~old_hsync & hsync) line_cnt <= (line_cnt == 9'd311) ? 9'd0 : line_cnt + 1'b1;
	end
	wire vsync = line_cnt < 8;
	wire backporch = (HSYNCcnt >= 43) && (HSYNCcnt <= 76);
	wire ncsync = ~vsync & ~hsync & ~backporch;

	reg [7:0] mem [0:65535];
	wire [15:0] v_addr;
	reg [7:0] v_dout = 8'd0;
	always @(posedge clk) v_dout <= mem[v_addr];

	reg rst = 1'b1, en = 1'b0, txt_en = 1'b0;
	reg [2:0] base_blk = 3'd4;
	localparam [9:0] XS = 10'd246;
	localparam [4:0] FONT = 5'd4;				// tabla de caracteres en $2000 (ROMTABLE[15:11])
	localparam [15:0] CODES = 16'h8000 + (WL - WF) * 640;		// codigos detras de la imagen
	localparam [7:0] TCOL = 8'h3C;				// papel 3, tinta 12
	reg pal16_hi_wr = 1'b0, pal_idx_wr = 1'b0, pal_lo_wr = 1'b0;
	reg [7:0] wdata = 8'd0;
	wire img_on;
	wire [15:0] lvl16;
	mode160 dut (
		.clk(clk), .clk104(clk104), .rst(rst), .en(en), .win_first(WF), .win_last(WL), .m4(1'b1),
		.pixel_cnt(pixel_cnt), .line_cnt(line_cnt), .x_start(XS), .base_blk(base_blk),
		.v_dout(v_dout), .v_addr(v_addr),
		.wclk(iclk), .pal_idx_wr(pal_idx_wr), .pal_lo_wr(pal_lo_wr), .pal_hi_wr(1'b0), .pal16_hi_wr(pal16_hi_wr), .wdata(wdata),
		.blank_n(ncsync), .border(4'd0), .pal16_en(1'b0), .c4(4'd0),
		.spr_hit(1'b0), .spr_x(), .spr_y(), .txt_en(txt_en), .txt_col(TCOL), .txt_font(FONT), .spr_row_ok(),
		.vred(), .vgreen(), .vblue(), .vbright(),
		.img_on_o(img_on), .idx_cur_o(), .lvl_o(), .lvl16_o(lvl16));

	reg [15:0] pm [0:15];
	task poke;
		input [1:0] which; input [7:0] v;
		begin
			@(posedge iclk); #3;
			wdata = v; pal_idx_wr = (which == 0); pal_lo_wr = (which == 1); pal16_hi_wr = (which == 3);
			@(negedge iclk); #3;
			pal_idx_wr = 0; pal_lo_wr = 0; pal16_hi_wr = 0;
		end
	endtask

	integer errors = 0, checks = 0, tpix = 0, ipix = 0;
	task check; input ok; input [255:0] msg;
		begin
			checks = checks + 1;
			if (!ok) begin errors = errors + 1; if (errors < 25) $display("ERROR: %0s (linea %0d offset %0d)", msg, line_cnt, pixel_cnt - XS); end
		end
	endtask

	function [3:0] nib;
		input integer r; input integer x;
		reg [7:0] b;
		begin
			b = mem[{base_blk, 13'b0} + r * 80 + x / 2];
			nib = (x % 2 == 0) ? b[7:4] : b[3:0];
		end
	endfunction

	integer off, k, j, r, n, i;
	function [3:0] eff;		// indice esperado del fondo en la linea r de la rejilla (0..199), columna x: la imagen ocupa las lineas 8..175; fuera, el borde (0 en este banco)
		input integer r; input integer x;
		eff = (r / 8 >= WF && r / 8 < WL) ? nib(r - 8 * WF, x) : 4'd0;
	endfunction
	reg [7:0] code, glyph;
	reg bitv;
	reg [9:0] pc_prev = 10'd0;
	reg pc_chk = 1'b0;
	always @(posedge clk) begin
		pc_prev <= pixel_cnt;
		if (pc_chk && en && line_cnt >= 58 && line_cnt < 258 && pixel_cnt != pc_prev) begin
			r = line_cnt - 58;
			off = pixel_cnt - XS;
			if (off >= GOFF && off < 480 + GOFF && ((off - GOFF) % 2) == 0) begin
				k = (off - GOFF) / 12;
				j = ((off - GOFF) % 12) / 2;
				code = mem[CODES + (r / 8) * 40 + k];
				if (code[7:5] != 3'b000) begin
					glyph = mem[{FONT, 11'b0} + code[7:0] * 8 + (r % 8)];
					bitv = glyph[7 - j] ^ code[7];
					check(lvl16 === pm[bitv ? TCOL[3:0] : TCOL[7:4]], "pixel de glifo");
					tpix = tpix + 1;
				end
			end
			if (off >= 2 && off < 482 && ((off - 2) % 3) == 0) begin
				n = (off - 2) / 3;
				code = mem[CODES + (r / 8) * 40 + n / 4];
				if (code[7:5] == 3'b000) begin
					check(lvl16 === pm[eff(r, n)], "imagen en celda transparente");
					ipix = ipix + 1;
				end
			end
		end
	end

	integer rr, cc, s, seed;
	initial begin
		seed = 5;
		for (rr = 0; rr < 200; rr = rr + 1)
			for (cc = 0; cc < 80; cc = cc + 1)
				mem[{3'd4, 13'b0} + rr * 80 + cc] = (rr * 37 + cc * 11 + (rr >> 3)) & 255;
		for (s = 0; s < 2048; s = s + 1) mem[{FONT, 11'b0} + s] = $random(seed);
		for (s = 0; s < 1000; s = s + 1)
			mem[CODES + s] = ($random(seed) & 3) == 0 ? ($random(seed) & 31) : ($random(seed) & 8'hFF);
		for (i = 0; i < 16; i = i + 1) pm[i] = {i[3:0], 4'd15 - i[3:0], i[3:0] ^ 4'd5, i[3:0] * 4'd3};
		repeat (8) @(posedge clk);
		rst = 0;
		poke(0, 0);
		for (i = 0; i < 16; i = i + 1) begin poke(1, pm[i][7:0]); poke(3, pm[i][15:8]); end
		en = 1; txt_en = 1;
		wait (line_cnt == 300); wait (line_cnt == 5);
		pc_chk = 1;
		wait (line_cnt == 270);
		pc_chk = 0;
		$display("pixeles de glifo %0d, de imagen %0d, comprobaciones %0d, errores %0d", tpix, ipix, checks, errors);
		if (errors == 0 && tpix > 15000 && ipix > 1000) $display("OK"); else $display("FALLO");
		$finish;
	end
endmodule
