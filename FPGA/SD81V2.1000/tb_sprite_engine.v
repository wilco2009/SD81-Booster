// tb_sprite_engine.v -- el motor de sprites por linea contra los slots antiguos
//
//  - modo A (dense=0): tablas al azar con como mucho K sprites por linea; la
//    salida del motor tiene que ser IDENTICA a la de los NSPR sprite_slot (la
//    implementacion de la v1.6.0, con memoria propia), pixel a pixel.
//  - modo B (dense=1): tablas al azar sin limite; el motor tiene que dar lo que
//    dice la especificacion: de los sprites que cortan la linea, los K de
//    indice mas alto, y entre ellos gana el de indice mas alto.
//  - worst=1: el caso peor de tiempo: todos los sprites activos y los K que
//    cortan las lineas visibles son los de MENOR indice (los ultimos que mira
//    el motor).
//
// Se reproduce lo que hace SD81.v: pixel_clk = cnt26[1] (6,5 MHz) o cnt26[0]
// (13 MHz, mode80=1), pixel_cnt hasta 413 u 827, el sincronismo horizontal de
// SD81.v, line_cnt que sube con el flanco del sincronismo en el dominio de
// system_clk, y la BRAM de sombra con lectura sincrona de un ciclo. El motor
// va a system_clk (26 MHz).
//
// Uso (ModelSim):  vsim -c tb_sprite_engine +mode80=0 +dense=0 +frames=20
`timescale 1ns/1ps

module tb_sprite_engine;
	parameter LIM32 = 120;		// pixel_cnt a partir del cual el motor deja de leer (32 col)
	parameter LIM80 = 195;		// idem en 80 columnas
	parameter NSPR  = 64;
	parameter K     = 8;

	integer mode80 = 0;
	integer dense  = 0;
	integer worst  = 0;
	integer frames = 20;
	integer seed   = 4242;

	reg sysclk = 0;
	always #20 sysclk = ~sysclk;

	reg [25:0] cnt = 0;
	always @(posedge sysclk) cnt <= cnt + 1'b1;
	wire pixel_clk = (mode80 != 0) ? cnt[0] : cnt[1];

	// ---- contador de pixel, sincronismo y linea (como SD81.v) ----
	reg [9:0] pixel_cnt = 0;
	always @(posedge pixel_clk)
		pixel_cnt <= (pixel_cnt == ((mode80 != 0) ? 10'd827 : 10'd413)) ? 10'd0 : pixel_cnt + 1'b1;
	wire [8:0] HSYNCcnt = pixel_cnt[9:1];
	wire hsync = (mode80 != 0) ? ((HSYNCcnt >= 11) && (HSYNCcnt <= 42))
	                           : ((HSYNCcnt >= 16) && (HSYNCcnt <= 31));

	reg [8:0] line_cnt = 0;
	reg old_hsync = 0;
	always @(posedge sysclk) begin
		old_hsync <= hsync;
		if (~old_hsync & hsync)
			line_cnt <= (line_cnt == 9'd311) ? 9'd0 : line_cnt + 9'd1;
	end

	// Superfast texto: SCR_START_X = 122 + 21, SCR_START_Y = 62 (sin fudge en Y)
	wire [8:0] spr_screen_x = pixel_cnt - 9'd143;
	wire [8:0] spr_screen_y = line_cnt - 9'd62;
	wire [8:0] pos_x = spr_screen_x + 9'd32;
	wire [8:0] pos_y = spr_screen_y + 9'd32;
	wire in_display = (spr_screen_x < 9'd256) && (spr_screen_y < 9'd192);
	wire row_ok = (spr_screen_y < 9'd192);

	// ---- RAM de sombra (puerto B, lectura sincrona) ----
	reg [7:0] mem [0:65535];
	reg [7:0] v_dout = 0;
	wire        ev_rd;
	wire [15:0] ev_raddr;
	always @(posedge sysclk) v_dout <= mem[ev_rd ? ev_raddr : 16'hFFFF];

	reg              reset = 1;
	reg  [NSPR-1:0]  spr_en = 0;
	wire        e_hit, e_pix;
	wire [7:0]  e_col;

	sprite_engine #(.K(K), .NSPR(NSPR), .SB(6)) dut (
		.clk(sysclk), .reset(reset), .spr_en(spr_en),
		.hsync(hsync), .pixel_cnt(pixel_cnt),
		.limit((mode80 != 0) ? LIM80[9:0] : LIM32[9:0]),
		.blit_busy(1'b0),
		.pos_x(pos_x), .pos_y(pos_y), .row_ok(row_ok), .v_dout(v_dout),
		.ev_rd(ev_rd), .ev_raddr(ev_raddr),
		.hit(e_hit), .pixel(e_pix), .color(e_col)
	);

	// ---- referencia: los slots de sprite de la v1.6.0 ----
	reg [NSPR-1:0] c_sel = 0;
	reg [4:0]  c_field = 0;
	reg [7:0]  c_data = 0;
	reg        c_we = 0;
	wire [NSPR-1:0] r_act, r_pix;
	wire [7:0]  r_col [0:NSPR-1];
	genvar gi;
	generate
		for (gi = 0; gi < NSPR; gi = gi + 1) begin : REF
			sprite_slot s (
				.clk(pixel_clk), .reset(reset),
				.cfg_sel(c_sel[gi] & c_we), .cfg_field(c_field), .cfg_data(c_data), .cfg_we(c_we),
				.pos_x(pos_x), .pos_y(pos_y),
				.active(r_act[gi]), .pixel_out(r_pix[gi]), .color_out(r_col[gi])
			);
		end
	endgenerate
	reg        ref_hit;
	reg        ref_pix;
	reg [7:0]  ref_col;
	integer    ri;
	always @(*) begin
		ref_hit = 0; ref_pix = 0; ref_col = 8'hF0;
		for (ri = 0; ri < NSPR; ri = ri + 1)
			if (r_act[ri]) begin ref_hit = 1; ref_pix = r_pix[ri]; ref_col = r_col[ri]; end
	end

	// ---- la tabla ----
	reg [8:0] t_x [0:NSPR-1];
	reg [7:0] t_y [0:NSPR-1];
	reg [7:0] t_col [0:NSPR*8-1];	// sprite*8 + fila
	reg [7:0] t_dat [0:NSPR*8-1];
	reg [7:0] t_msk [0:NSPR*8-1];

	// direccion de la tabla de un sprite en la sombra
	function [15:0] sbase;
		input integer i;
		begin
			sbase = (i < 32) ? (16'h0C00 + i*32) : (16'h1800 + (i-32)*32);
		end
	endfunction

	// Especificacion (modo B): K sprites de mas indice que cortan la linea
	function [9:0] spec;	// {hit, pix, color}
		input [8:0] px;
		input [8:0] py;
		integer i, n;
		reg [8:0] yd, xd;
		reg [2:0] row, col;
		reg got;
		begin
			n = 0; got = 0;
			spec = 10'h0F0;
			for (i = NSPR - 1; i >= 0; i = i - 1) begin
				yd = py - {1'b0, t_y[i]};
				if (spr_en[i] && yd < 8 && n < K) begin
					n = n + 1;
					xd = px - t_x[i];
					row = yd[2:0]; col = xd[2:0];
					if (!got && xd < 8 && t_msk[i*8+row][7-col]) begin
						got = 1;
						spec = {1'b1, t_dat[i*8+row][7-col], t_col[i*8+row]};
					end
				end
			end
		end
	endfunction

	// escribe la tabla en la RAM de sombra
	task load_table;
		integer i, r;
		begin
			for (i = 0; i < NSPR; i = i + 1) begin
				mem[sbase(i) + 0] = spr_en[i];
				mem[sbase(i) + 1] = t_x[i][7:0];
				mem[sbase(i) + 2] = {7'd0, t_x[i][8]};
				mem[sbase(i) + 3] = t_y[i];
				for (r = 0; r < 8; r = r + 1) begin
					mem[sbase(i) + 4  + r] = t_col[i*8+r];
					mem[sbase(i) + 12 + r] = t_dat[i*8+r];
					mem[sbase(i) + 20 + r] = t_msk[i*8+r];
				end
			end
		end
	endtask

	task cfg_write;
		input integer i;
		input [4:0] f;
		input [7:0] d;
		begin
			@(negedge pixel_clk);
			c_sel = {{(NSPR-1){1'b0}}, 1'b1} << i; c_field = f; c_data = d; c_we = 1;
			@(negedge pixel_clk);
			c_we = 0;
		end
	endtask

	task load_ref;
		integer i, r;
		begin
			for (i = 0; i < NSPR; i = i + 1) begin
				cfg_write(i, 5'd0, {7'd0, spr_en[i]});
				cfg_write(i, 5'd1, t_x[i][7:0]);
				cfg_write(i, 5'd2, {7'd0, t_x[i][8]});
				cfg_write(i, 5'd3, t_y[i]);
				for (r = 0; r < 8; r = r + 1) begin
					cfg_write(i, 5'd4  + r, t_col[i*8+r]);
					cfg_write(i, 5'd12 + r, t_dat[i*8+r]);
					cfg_write(i, 5'd20 + r, t_msk[i*8+r]);
				end
			end
		end
	endtask

	// maximo de sprites que cortan una misma linea visible
	function integer max_per_line;
		input dummy;
		integer ln, i, n, m;
		reg [8:0] yd;
		begin
			m = 0;
			for (ln = 32; ln < 224; ln = ln + 1) begin
				n = 0;
				for (i = 0; i < NSPR; i = i + 1) begin
					yd = ln[8:0] - {1'b0, t_y[i]};
					if (spr_en[i] && yd < 8) n = n + 1;
				end
				if (n > m) m = n;
			end
			max_per_line = m;
		end
	endfunction

	task random_table;
		integer i, r, tries, ysp;
		reg ok;
		begin
			ok = 0; tries = 0;
			while (!ok) begin
				tries = tries + 1;
				ysp = dense ? 160 : 200;
				for (i = 0; i < NSPR; i = i + 1) begin
					spr_en[i] = (($random(seed) & 7) != 0);
					t_x[i] = ($random(seed) & 32'h1FF) % 330;
					if (($random(seed) & 31) == 0) t_x[i] = 9'd500 + ($random(seed) & 7);
					t_y[i] = 8'd20 + (($random(seed) & 32'hFF) % ysp);
					for (r = 0; r < 8; r = r + 1) begin
						t_col[i*8+r] = $random(seed);
						t_dat[i*8+r] = $random(seed);
						t_msk[i*8+r] = $random(seed) | $random(seed);
					end
				end
				if (worst) begin
					for (i = 0; i < NSPR; i = i + 1) begin
						spr_en[i] = 1;
						t_y[i] = (i < K) ? 8'd60 : 8'd200;
					end
				end
				ok = (dense || worst) ? 1'b1 : (max_per_line(0) <= K);
			end
		end
	endtask

	// ---- comparacion ----
	integer errors = 0, compared = 0, hits = 0, frame = 0;
	reg [9:0] sp;
	always @(negedge pixel_clk) begin
		if (!reset && in_display && frame > 0) begin
			compared = compared + 1;
			if (dense || worst) begin
				sp = spec(pos_x, pos_y);
				if (sp[9] !== e_hit || (sp[9] && (sp[8] !== e_pix || sp[7:0] !== e_col)) || (!sp[9] && e_col !== 8'hF0)) begin
					errors = errors + 1;
					if (errors < 10) $display("DIF(B) linea=%0d x=%0d  espec hit=%b pix=%b col=%02x | motor hit=%b pix=%b col=%02x",
						line_cnt, pos_x, sp[9], sp[8], sp[7:0], e_hit, e_pix, e_col);
				end
				if (sp[9]) hits = hits + 1;
			end else begin
				if (ref_hit !== e_hit || (ref_hit && (ref_pix !== e_pix || ref_col !== e_col)) || (!ref_hit && e_col !== 8'hF0)) begin
					errors = errors + 1;
					if (errors < 10) $display("DIF(A) linea=%0d x=%0d  ref hit=%b pix=%b col=%02x | motor hit=%b pix=%b col=%02x",
						line_cnt, pos_x, ref_hit, ref_pix, ref_col, e_hit, e_pix, e_col);
				end
				if (ref_hit) hits = hits + 1;
			end
		end
	end

	initial begin
		if ($value$plusargs("mode80=%d", mode80)) ;
		if ($value$plusargs("dense=%d", dense)) ;
		if ($value$plusargs("frames=%d", frames)) ;
		if ($value$plusargs("seed=%d", seed)) ;
		if ($value$plusargs("worst=%d", worst)) ;
		repeat (8) @(posedge pixel_clk);
		reset = 0;
		while (frame < frames) begin
			wait (line_cnt == 9'd262);
			random_table;
			load_table;
			load_ref;
			frame = frame + 1;
			wait (line_cnt == 9'd5);
		end
		wait (line_cnt == 9'd260);
		$display("RESULTADO mode80=%0d dense=%0d worst=%0d: %0d cuadros, %0d pixeles comparados, %0d con sprite, %0d diferencias",
			mode80, dense, worst, frame, compared, hits, errors);
		if (errors == 0 && hits > 1000) $display("OK");
		else $display("FALLO");
		$finish;
	end
endmodule
