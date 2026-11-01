`timescale 1ns / 1ps
// tb_mode160_spr -- mode160 en 4 bpp con el sprite_engine real: sprites 8x8 sobre la imagen 160x200.
//   vlog mode160.v sprite_engine.v tb_mode160_spr.v ; vsim -c tb_mode160_spr -do "run -all; quit"
// Tabla de sprites al azar en la sombra ($0C00 + n*32 + campo, como SD81.v); se comprueba cada pixel de la imagen
// (a mitad de pixel) contra el modelo: el sprite de indice mas alto que lo cubre (pixel ? tinta : papel, tinta = nibble alto
// del color de la fila) o, si no, el nibble de la imagen. Todo pasado por pal16.
module tb_mode160_spr;
	parameter WF = 1;
	parameter WL = 22;
	localparam NSPR = 8;
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
	wire ev_rd;
	wire [15:0] ev_raddr;
	reg [7:0] v_dout = 8'd0;
	always @(posedge clk) v_dout <= mem[ev_rd ? ev_raddr : v_addr];

	reg rst = 1'b1, en = 1'b0;
	reg [NSPR-1:0] spr_en = 0;
	reg [2:0] base_blk = 3'd4;
	localparam [9:0] XS = 10'd246;
	reg pal16_hi_wr = 1'b0, pal_idx_wr = 1'b0, pal_lo_wr = 1'b0;
	reg [7:0] wdata = 8'd0;
	wire img_on;
	wire [15:0] lvl16;
	wire spr_hit, spr_pix;
	wire [7:0] spr_col;
	wire [8:0] spr_x, spr_y;
	wire row_ok;
	wire [3:0] c4 = spr_pix ? spr_col[7:4] : spr_col[3:0];		// el camino normal con sprite_active_final
	mode160 dut (
		.clk(clk), .clk104(clk104), .rst(rst), .en(en), .win_first(WF), .win_last(WL), .m4(1'b1),
		.pixel_cnt(pixel_cnt), .line_cnt(line_cnt), .x_start(XS), .base_blk(base_blk),
		.v_dout(v_dout), .v_addr(v_addr),
		.wclk(iclk), .pal_idx_wr(pal_idx_wr), .pal_lo_wr(pal_lo_wr), .pal_hi_wr(1'b0), .pal16_hi_wr(pal16_hi_wr), .wdata(wdata),
		.blank_n(ncsync), .border(4'd0), .pal16_en(1'b0), .c4(c4),
		.spr_hit(spr_hit), .spr_x(spr_x), .spr_y(spr_y), .txt_en(1'b0), .txt_col(8'd0), .txt_font(5'd0), .spr_row_ok(row_ok),
		.vred(), .vgreen(), .vblue(), .vbright(),
		.img_on_o(img_on), .idx_cur_o(), .lvl_o(), .lvl16_o(lvl16));
	sprite_engine #(.K(12), .NSPR(NSPR), .SB(6)) eng (
		.clk(clk), .reset(rst), .spr_en(spr_en), .hsync(hsync), .pixel_cnt(pixel_cnt), .limit(10'd195),
		.blit_busy(1'b0), .pos_x(spr_x), .pos_y(spr_y), .row_ok(row_ok), .v_dout(v_dout),
		.ev_rd(ev_rd), .ev_raddr(ev_raddr), .hit(spr_hit), .pixel(spr_pix), .color(spr_col));

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

	integer errors = 0, checks = 0, spr_pixels = 0;
	task check; input ok; input [255:0] msg;
		begin
			checks = checks + 1;
			if (!ok) begin errors = errors + 1; if (errors < 25) $display("ERROR: %0s (linea %0d pixel_cnt %0d)", msg, line_cnt, pixel_cnt); end
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

	function [3:0] eff;		// indice esperado del fondo en la linea r de la rejilla (0..199), columna x: la imagen ocupa las lineas 8..175; fuera, el borde (0 en este banco)
		input integer r; input integer x;
		eff = (r / 8 >= WF && r / 8 < WL) ? nib(r - 8 * WF, x) : 4'd0;
	endfunction
	integer sx [0:NSPR-1];
	integer sy [0:NSPR-1];
	function [4:0] expected;		// {es_sprite, color} del pixel (x, r) de la imagen
		input integer r; input integer x;
		integer s, dx, dy, a;
		reg [7:0] pxb, mskb, colb;
		reg found;
		reg [3:0] c;
		begin
			found = 0; c = eff(r, x);
			for (s = NSPR - 1; s >= 0; s = s - 1) begin
				dx = x + 32 - sx[s]; dy = r - 8 * WF + 32 - sy[s];
				if (!found && r / 8 >= WF && r / 8 < WL && spr_en[s] && dx >= 0 && dx < 8 && dy >= 0 && dy < 8) begin
					a = 16'h0C00 + s * 32;
					colb = mem[a + 4 + dy]; pxb = mem[a + 12 + dy]; mskb = mem[a + 20 + dy];
					if (mskb[7 - dx]) begin found = 1; c = pxb[7 - dx] ? colb[7:4] : colb[3:0]; end
				end
			end
			expected = {found, c};
		end
	endfunction

	integer off, n, r, imgpix = 0, i;
	reg pc_chk = 1'b0;
	reg [9:0] pc_prev = 10'd0;
	reg [4:0] ex;
	always @(posedge clk) begin
		pc_prev <= pixel_cnt;
		if (pc_chk && en && line_cnt >= 58 && line_cnt < 258 && pixel_cnt != pc_prev) begin
			r = line_cnt - 58;
			off = pixel_cnt - XS;
			if (off >= 2 && off < 482 && ((off - 2) % 3) == 0) begin
				n = (off - 2) / 3;
				ex = expected(r, n);
				check(lvl16 === pm[ex[3:0]], "color = pal16[sprite o fondo]");
				if (ex[4]) spr_pixels = spr_pixels + 1;
				imgpix = imgpix + 1;
			end
		end
	end

	integer rr, cc, s, f, seed;
	initial begin
		seed = 77;
		for (rr = 0; rr < 200; rr = rr + 1)
			for (cc = 0; cc < 80; cc = cc + 1)
				mem[{3'd4, 13'b0} + rr * 80 + cc] = (rr * 37 + cc * 11 + (rr >> 3)) & 255;
		for (s = 0; s < NSPR; s = s + 1) begin
			sy[s] = 32 + (s * 25) + ($random(seed) & 7) - 4;
			sx[s] = (s == 0) ? 32 - 5 : (s == 1) ? 32 + 156 : 32 + ($random(seed) & 127);
			if (s == 2) sx[s] = 32 + 40;
			if (s == 3) sx[s] = 32 + 44;
			if (s == 3) sy[s] = sy[2] + 3;
			mem[16'h0C00 + s * 32 + 0] = 1;
			mem[16'h0C00 + s * 32 + 1] = sx[s] & 255;
			mem[16'h0C00 + s * 32 + 2] = (sx[s] >> 8) & 1;
			mem[16'h0C00 + s * 32 + 3] = sy[s];
			for (f = 0; f < 8; f = f + 1) begin
				mem[16'h0C00 + s * 32 + 4 + f] = $random(seed);
				mem[16'h0C00 + s * 32 + 12 + f] = $random(seed);
				mem[16'h0C00 + s * 32 + 20 + f] = $random(seed) | $random(seed);
			end
		end
		for (i = 0; i < 16; i = i + 1) pm[i] = {i[3:0], 4'd15 - i[3:0], i[3:0] ^ 4'd5, i[3:0] * 4'd3};
		repeat (8) @(posedge clk);
		rst = 0;
		spr_en = {NSPR{1'b1}};
		poke(0, 0);
		for (i = 0; i < 16; i = i + 1) begin poke(1, pm[i][7:0]); poke(3, pm[i][15:8]); end
		en = 1;
		wait (line_cnt == 300); wait (line_cnt == 5);
		pc_chk = 1;
		wait (line_cnt == 270);
		pc_chk = 0;
		$display("pixeles %0d (de sprite %0d), comprobaciones %0d, errores %0d", imgpix, spr_pixels, checks, errors);
		if (errors == 0 && imgpix == 32000 && spr_pixels > 100) $display("OK"); else $display("FALLO");
		$finish;
	end
endmodule
