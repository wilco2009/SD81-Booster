`timescale 1ns / 1ps
// tb_mode160_4bpp -- variante 160x200 a 4 bpp de mode160 (m4 = 1). Aparte del proyecto de ISE:
//   vlog mode160.v tb_mode160_4bpp.v
//   vsim -c tb_mode160_4bpp -do "run -all; quit"
// Comprueba: nibble alto = pixel de la izquierda, 80 bytes por fila, una fila por linea de TV (58..257), el nivel sale de pal16
// (lvl16_o) alineado con img_on_o (primer y ultimo ciclo incluidos), y que fuera de la ventana no hay imagen.
module tb_mode160_4bpp;
	parameter FULL = 0;
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

	reg rst = 1'b1, en = 1'b0, m4 = 1'b0;
	reg [2:0] base_blk = 3'd4;
	localparam [9:0] XS = 10'd246;
	reg pal16_hi_wr = 1'b0, pal_idx_wr = 1'b0, pal_lo_wr = 1'b0;
	reg [7:0] wdata = 8'd0;
	wire img_on;
	wire [15:0] lvl, lvl16;
	mode160 dut (
		.clk(clk), .clk104(clk104), .rst(rst), .en(en), .full(FULL != 0), .m4(m4), .spr_hit(1'b0), .spr_x(), .spr_y(), .txt_en(1'b0), .txt_col(8'd0), .txt_font(5'd0), .spr_row_ok(),
		.pixel_cnt(pixel_cnt), .line_cnt(line_cnt), .x_start(XS), .base_blk(base_blk),
		.v_dout(v_dout), .v_addr(v_addr),
		.wclk(iclk), .pal_idx_wr(pal_idx_wr), .pal_lo_wr(pal_lo_wr), .pal_hi_wr(1'b0), .pal16_hi_wr(pal16_hi_wr), .wdata(wdata),
		.blank_n(ncsync), .border(4'd0), .pal16_en(1'b0), .c4(4'd0),
		.vred(), .vgreen(), .vblue(), .vbright(),
		.img_on_o(img_on), .idx_cur_o(), .lvl_o(lvl), .lvl16_o(lvl16));

	reg [15:0] pm [0:15];
	task poke;	// which: 0 = puntero, 1 = bajo, 3 = alto (2066)
		input [1:0] which; input [7:0] v;
		begin
			@(posedge iclk); #3;
			wdata = v; pal_idx_wr = (which == 0); pal_lo_wr = (which == 1); pal16_hi_wr = (which == 3);
			@(negedge iclk); #3;
			pal_idx_wr = 0; pal_lo_wr = 0; pal16_hi_wr = 0;
		end
	endtask

	integer errors = 0, checks = 0;
	task check; input ok; input [255:0] msg;
		begin
			checks = checks + 1;
			if (!ok) begin errors = errors + 1; if (errors < 25) $display("ERROR: %0s (linea %0d pixel_cnt %0d)", msg, line_cnt, pixel_cnt); end
		end
	endtask

	function [3:0] nib;		// pixel x de la fila r
		input integer r; input integer x;
		reg [7:0] b;
		begin
			b = mem[{base_blk, 13'b0} + r * 80 + x / 2];
			nib = (x % 2 == 0) ? b[7:4] : b[3:0];
		end
	endfunction

	function [3:0] eff;		// indice esperado del fondo en la linea r de la rejilla (0..199), columna x: la imagen ocupa las lineas 8..175; fuera, el borde (0 en este banco)
		input integer r; input integer x;
		eff = FULL ? nib(r, x) : (r < 8 || r >= 176) ? 4'd0 : nib(r - 8, x);
	endfunction
	integer off, n, r, imgpix = 0, i;
	reg pc_chk = 1'b0;
	reg [9:0] pc_prev = 10'd0;
	reg img_prev = 1'b0;
	reg [15:0] lvl16_prev = 16'd0;
	integer edges_f = 0, edges_l = 0;
	always @(posedge clk) begin
		pc_prev <= pixel_cnt;
		img_prev <= img_on;
		lvl16_prev <= lvl16;
		if (pc_chk && en) begin
			r = line_cnt - 58;
			if (line_cnt >= 58 && line_cnt < 258) begin
				if (pixel_cnt != pc_prev) begin
					off = pixel_cnt - XS;
					if (off >= 2 && off < 482 && ((off - 2) % 3) == 0) begin
						n = (off - 2) / 3;
						check(img_on === 1'b1, "img_on en la ventana");
						check(lvl16 === pm[eff(r, n)], "nivel = pal16[nibble]");
						imgpix = imgpix + 1;
					end
					if (!(pixel_cnt > 20 && pixel_cnt < XS) ? 1'b0 : img_on) check(1'b0, "imagen antes de la ventana");
				end
				if (img_on && !img_prev) begin edges_f = edges_f + 1; check(lvl16 === pm[eff(r, 0)], "primer ciclo: nivel del primer pixel"); end
				if (!img_on && img_prev) begin edges_l = edges_l + 1; check(lvl16_prev === pm[eff(r, 159)], "ultimo ciclo: nivel del ultimo pixel"); end
			end else if (pixel_cnt > 20 && pixel_cnt < 800) check(img_on === 1'b0, "sin imagen fuera de las lineas");
		end
	end

	integer rr, cc;
	initial begin
		for (rr = 0; rr < 200; rr = rr + 1)
			for (cc = 0; cc < 80; cc = cc + 1)
				mem[{3'd4, 13'b0} + rr * 80 + cc] = (rr * 37 + cc * 11 + (rr >> 3)) & 255;
		for (i = 0; i < 16; i = i + 1) pm[i] = {4'(i), 4'(15 - i), 4'(i ^ 5), 4'(i * 3)};	// {BRIGHT,R | G,B} distinto por indice
		repeat (8) @(posedge clk);
		rst = 0;
		poke(0, 0);
		for (i = 0; i < 16; i = i + 1) begin
			poke(1, pm[i][7:0]);
			poke(3, pm[i][15:8]);
		end
		en = 1; m4 = 1;
		@(negedge hsync); @(posedge hsync);
		wait (line_cnt == 300); wait (line_cnt == 5);
		pc_chk = 1;
		wait (line_cnt == 270);
		pc_chk = 0;
		$display("pixeles comprobados %0d (esperados %0d), bordes %0d/%0d, comprobaciones %0d, errores %0d", imgpix, 200*160, edges_f, edges_l, checks, errors);
		if (errors == 0 && imgpix == 32000 && edges_f == 200 && edges_l == 200) $display("OK"); else $display("FALLO");
		$finish;
	end
endmodule
