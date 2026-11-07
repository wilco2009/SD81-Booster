// tb_sprite_engine.v -- el motor de sprites por linea contra los slots antiguos
//
//  +fmt=0: todos los sprites en formato A (pagina 0), con basura en el nibble alto de X alto, que aqui se ignora.
//  +fmt=1: todos en formato B; +fmt=2: mezclados. Con fmt != 0 se compara siempre con la especificacion.
//  Tinta y papel del motor salen ya resueltos al color del pixel: se compara ese color. Con +mono=1 (salida sin color)
//  solo cuenta el bit de pixel, y se compara ese bit.
//
//  - modo A (dense=0): tablas al azar con como mucho K sprites por linea; la
//    salida del motor tiene que ser IDENTICA a la de los NSPR sprite_slot (la
//    implementacion de la v1.6.0, con memoria propia), pixel a pixel.
//  - modo B (dense=1): tablas al azar sin limite; el motor tiene que dar lo que
//    dice la especificacion: de los sprites que cortan la linea, los K de
//    indice mas alto, y entre ellos gana el de indice mas alto.
//  - extra=M: M sprites extra (POKE 2131) en la hoja, del formato que diga fmt. Las Y van en franjas de 8 lineas
//    con como mucho kb sprites por franja: kb es lo que garantiza el tiempo (2 + 64 + M + coste * kb pasos, con
//    coste 8 en B o mezcla y 6 en A), o lo que diga +kb=N. Con worst=1, los kb que cortan la linea son los extra
//    de numero mas alto (los ultimos que mira el motor) y los 64 normales estan activos.
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
	integer   lim80 = 195;		// idem en 80 columnas (+lim80=N)
	parameter NSPR  = 64;
	parameter K     = 12;
	parameter NX    = 127;		// sprites extra como mucho
	localparam NT   = NSPR + NX;

	integer mode80 = 0;
	integer dense  = 0;
	integer worst  = 0;
	integer fmt    = 0;
	integer mono   = 0;
	integer frames = 20;
	integer xtra   = 0;		// sprites extra (+extra=M)
	integer kb     = 0;		// sprites por franja con extra
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
	// En 80 columnas (modos 160) la imagen empieza en pixel_cnt 246 y cada pixel de sprite dura 3 ciclos: con sprites al limite
	// de tiempo hay que mirar donde mira el video, no antes (fuera de esto, sprite_slot se compara con las coordenadas de 32 columnas)
	wire use160 = (mode80 != 0) && (dense != 0 || worst != 0 || fmt != 0 || xtra != 0);
	wire [9:0] x160 = (pixel_cnt >= 10'd248) ? (pixel_cnt - 10'd248) / 3 : 10'd1023;
	wire [8:0] spr_screen_x = use160 ? x160[8:0] : pixel_cnt - 9'd143;
	wire [8:0] spr_screen_y = line_cnt - 9'd62;
	wire [8:0] pos_x = spr_screen_x + 9'd32;
	wire [8:0] pos_y = spr_screen_y + 9'd32;
	wire in_display = (use160 ? (x160 < 10'd160) : (spr_screen_x < 9'd256)) && (spr_screen_y < 9'd192);
	wire row_ok = (spr_screen_y < 9'd192);

	// ---- RAM de sombra (puerto B, lectura sincrona) ----
	reg [7:0] mem [0:65535];
	reg [7:0] v_dout = 0;
	wire        ev_rd;
	wire [15:0] ev_raddr;
	always @(posedge sysclk) v_dout <= mem[ev_rd ? ev_raddr : 16'hFFFF];

	reg              reset = 1;
	reg  [NSPR-1:0]  spr_en = 0;
	reg  [2:0]       sheet_base = 3'd2;
	wire        e_hit, e_pix;
	wire [7:0]  e_col;

	sprite_engine #(.K(K), .NSPR(NSPR), .SB(6)) dut (
		.clk(sysclk), .reset(reset), .spr_en(spr_en), .sheet_base(sheet_base), .extra(xtra[6:0]), .color_on(mono == 0),
		.hsync(hsync), .pixel_cnt(pixel_cnt),
		.limit((mode80 != 0) ? lim80[9:0] : LIM32[9:0]),
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
	// 0..63 los normales; 64+j el extra j
	reg [8:0] t_x [0:NT-1];
	reg [7:0] t_y [0:NT-1];
	reg [7:0] t_pg [0:NT-1];	// pagina: 0 = formato A
	reg [3:0] t_tr [0:NT-1];	// color transparente (formato B)
	reg [7:0] t_col [0:NT*8-1];	// sprite*8 + fila
	reg [7:0] t_dat [0:NT*8-1];
	reg [7:0] t_msk [0:NT*8-1];

	// activo: los extra no tienen bit (con Y = 0 no cortan ninguna linea visible)
	function en;
		input integer i;
		begin
			en = (i < NSPR) ? spr_en[i] : 1'b1;
		end
	endfunction

	// direccion de la tabla de un sprite en la sombra
	function [15:0] sbase;
		input integer i;
		begin
			sbase = (i < 32) ? (16'h0C00 + i*32) : (i < NSPR) ? (16'h1800 + (i-32)*32) : ({sheet_base, 13'd0} + (i-NSPR)*32);
		end
	endfunction

	// Nibble de la hoja: fila r de la pagina pg, pixel c (0 = el de la izquierda)
	function [3:0] sheet_nib;
		input [7:0] pg;
		input [2:0] r;
		input [2:0] c;
		reg [7:0] b;
		begin
			b = mem[{sheet_base, pg, r, c[2:1]}];
			sheet_nib = c[0] ? b[3:0] : b[7:4];
		end
	endfunction

	// Especificacion: los K primeros que cortan la linea, en el orden del motor (63..0 y luego los extra 0..M-1)
	function [5:0] spec;	// {hit, pix, color resuelto}
		input [8:0] px;
		input [8:0] py;
		integer i, k, n;
		reg [8:0] yd, xd;
		reg [2:0] row, col;
		reg got, vis, pb;
		reg [3:0] nb;
		begin
			n = 0; got = 0;
			spec = 6'd0;
			for (k = 0; k < NSPR + xtra; k = k + 1) begin
				i = (k < NSPR) ? NSPR - 1 - k : k;
				yd = py - {1'b0, t_y[i]};
				if (en(i) && yd < 8 && n < K) begin
					n = n + 1;
					xd = px - t_x[i];
					row = yd[2:0]; col = xd[2:0];
					if (!got && xd < 8) begin
						vis = t_msk[i*8+row][7-col];
						if (t_pg[i] == 8'd0) begin
							pb = t_dat[i*8+row][7-col];
							nb = pb ? t_col[i*8+row][7:4] : t_col[i*8+row][3:0];
						end else begin
							nb = sheet_nib(t_pg[i], row, col);
							pb = nb[0];
							if (nb == t_tr[i]) vis = 1'b0;
						end
						if (vis) begin
							got = 1;
							spec = {1'b1, pb, nb};
						end
					end
				end
			end
		end
	endfunction

	// escribe la tabla en la RAM de sombra
	task load_table;
		integer i, r;
		begin
			for (i = 0; i < NSPR + xtra; i = i + 1) begin
				mem[sbase(i) + 0] = en(i);
				mem[sbase(i) + 1] = t_x[i][7:0];
				mem[sbase(i) + 2] = {t_tr[i], 3'd0, t_x[i][8]};
				mem[sbase(i) + 3] = t_y[i];
				mem[sbase(i) + 29] = t_pg[i];
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

	// una hoja al azar: 256 celdas de 32 bytes
	task load_sheet;
		integer a;
		reg [7:0] b;
		begin
			for (a = 0; a < 8192; a = a + 1) begin
				b = $random(seed);
				mem[{sheet_base, 13'd0} + a] = b;
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
		integer i, r, tries, ysp, b;
		integer bc [0:23];
		reg ok;
		begin
			ok = 0; tries = 0;
			// el bloque de la hoja: 2..7 (el 0 y el 1 contienen las tablas, en $0C00 y $1800)
			sheet_base = 3'd2 + (($random(seed) & 32'hFF) % 6);
			load_sheet;
			while (!ok) begin
				tries = tries + 1;
				ysp = dense ? 160 : 200;
				for (i = 0; i < NSPR + xtra; i = i + 1) begin
					if (i < NSPR) spr_en[i] = (($random(seed) & 7) != 0);
					t_x[i] = ($random(seed) & 32'h1FF) % 330;
					if (($random(seed) & 31) == 0) t_x[i] = 9'd500 + ($random(seed) & 7);
					t_y[i] = 8'd20 + (($random(seed) & 32'hFF) % ysp);
					t_tr[i] = (($random(seed) & 1) == 0) ? 4'd0 : $random(seed);
					// pocas paginas distintas, para que los sprites compartan grafico
					t_pg[i] = 8'd1 + xtra + (($random(seed) & 32'hFF) % 12);	// sin pisar las paginas de los extra
					if (($random(seed) & 3) == 0) t_pg[i] = 8'd255;
					if (fmt == 0 || (fmt == 2 && ($random(seed) & 1) == 0)) t_pg[i] = 8'd0;
					for (r = 0; r < 8; r = r + 1) begin
						t_col[i*8+r] = $random(seed);
						t_dat[i*8+r] = $random(seed);
						t_msk[i*8+r] = $random(seed) | $random(seed);
					end
				end
				if (worst && xtra != 0) begin
					for (i = 0; i < NSPR; i = i + 1) begin
						spr_en[i] = 1;
						t_y[i] = 8'd200;
					end
					for (i = NSPR; i < NSPR + xtra; i = i + 1)
						t_y[i] = (i >= NSPR + xtra - kb) ? 8'd60 : 8'd0;
				end else if (worst) begin
					for (i = 0; i < NSPR; i = i + 1) begin
						spr_en[i] = 1;
						t_y[i] = (i < K) ? 8'd60 : 8'd200;
					end
				end else if (xtra != 0) begin
					// franjas de 8 lineas, como mucho kb sprites en cada una
					if (($random(seed) & 7) == 0) for (i = NSPR; i < NSPR + xtra; i = i + 1) t_y[i] = 8'd0;
					for (b = 0; b < 24; b = b + 1) bc[b] = 0;
					for (i = 0; i < NSPR + xtra; i = i + 1)
						if (en(i) && (i < NSPR || t_y[i] != 0)) begin
							b = ($random(seed) & 32'hFF) % 24;
							if (bc[b] < kb) begin
								bc[b] = bc[b] + 1;
								t_y[i] = 8'd32 + b * 8;
							end else if (i < NSPR) spr_en[i] = 1'b0;
							else t_y[i] = 8'd0;
						end
				end
				ok = (dense || worst || xtra != 0) ? 1'b1 : (max_per_line(0) <= K);
			end
		end
	endtask

	// ---- comparacion ----
	integer errors = 0, compared = 0, hits = 0, frame = 0;
	reg [5:0] sp;
	wire [3:0] ref_nib = ref_pix ? ref_col[7:4] : ref_col[3:0];
	always @(negedge pixel_clk) begin
		if (!reset && in_display && frame > 0) begin
			compared = compared + 1;
			if (dense || worst || fmt != 0 || xtra != 0) begin
				sp = spec(pos_x, pos_y);
				if (sp[5] !== e_hit || (sp[5] && ((mono != 0) ? (sp[4] !== e_pix) : ({sp[3:0], sp[3:0]} !== e_col)))) begin
					errors = errors + 1;
					if (errors < 10) $display("DIF(B) linea=%0d x=%0d  espec hit=%b pix=%b col=%h | motor hit=%b pix=%b col=%02x",
						line_cnt, pos_x, sp[5], sp[4], sp[3:0], e_hit, e_pix, e_col);
				end
				if (sp[5]) hits = hits + 1;
			end else begin
				if (ref_hit !== e_hit || (ref_hit && ((mono != 0) ? (ref_pix !== e_pix) : ({ref_nib, ref_nib} !== e_col)))) begin
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
		if ($value$plusargs("fmt=%d", fmt)) ;
		if ($value$plusargs("mono=%d", mono)) ;
		if ($value$plusargs("lim80=%d", lim80)) ;
		if ($value$plusargs("extra=%d", xtra)) ;
		kb = (((mode80 != 0) ? (lim80 - 22) : (LIM32 - 32) * 2) - 4 - NSPR - xtra) / ((fmt == 0) ? 6 : 8);
		if (kb > K) kb = K;
		if ($value$plusargs("kb=%d", kb)) ;
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
		$display("RESULTADO mode80=%0d lim80=%0d fmt=%0d mono=%0d dense=%0d worst=%0d extra=%0d kb=%0d: %0d cuadros, %0d pixeles comparados, %0d con sprite, %0d diferencias",
			mode80, lim80, fmt, mono, dense, worst, xtra, kb, frame, compared, hits, errors);
		if (errors == 0 && hits > 1000) $display("OK");
		else $display("FALLO");
		$finish;
	end
endmodule
