// tb_sprite_engine.v -- el motor de sprites por linea (formato B, 4 bits por pixel)
//
// Compara la salida del motor, pixel a pixel, con la especificacion: de los
// sprites que cortan la linea, los K de indice mas alto; entre ellos gana el de
// indice mas alto cuyo pixel no sea 0 (el 0 es transparente). Las hojas de
// graficos estan en la RAM de sombra, en un bloque de 8 KB que cambia en cada
// cuadro, y varios sprites comparten pagina.
//  - dense=0: tablas al azar con como mucho K sprites por linea.
//  - dense=1: tablas al azar sin limite.
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
	// En 80 columnas (modos 160) la imagen empieza en pixel_cnt 246 y cada pixel de sprite dura 3 ciclos de
	// pixel_cnt (13 MHz): 160 pixeles (32..191 en coordenadas de sprite)
	wire [9:0] x160 = (pixel_cnt >= 10'd248) ? (pixel_cnt - 10'd248) / 3 : 10'd1023;
	wire [8:0] spr_screen_x = (mode80 != 0) ? x160[8:0] : pixel_cnt - 9'd143;
	wire [8:0] spr_screen_y = line_cnt - 9'd62;
	wire [8:0] pos_x = spr_screen_x + 9'd32;
	wire [8:0] pos_y = spr_screen_y + 9'd32;
	wire in_display = ((mode80 != 0) ? (x160 < 10'd160) : (spr_screen_x < 9'd256)) && (spr_screen_y < 9'd192);
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
	wire        e_hit;
	wire [3:0]  e_pix;

	sprite_engine #(.K(K), .NSPR(NSPR), .SB(6)) dut (
		.clk(sysclk), .reset(reset), .spr_en(spr_en), .sheet_base(sheet_base),
		.hsync(hsync), .pixel_cnt(pixel_cnt),
		.limit((mode80 != 0) ? lim80[9:0] : LIM32[9:0]),
		.blit_busy(1'b0),
		.pos_x(pos_x), .pos_y(pos_y), .row_ok(row_ok), .v_dout(v_dout),
		.ev_rd(ev_rd), .ev_raddr(ev_raddr),
		.hit(e_hit), .pix(e_pix)
	);

	// ---- la tabla ----
	reg [8:0] t_x [0:NSPR-1];
	reg [7:0] t_y [0:NSPR-1];
	reg [7:0] t_pg [0:NSPR-1];

	// direccion de la tabla de un sprite en la sombra
	function [15:0] sbase;
		input integer i;
		begin
			sbase = (i < 32) ? (16'h0C00 + i*32) : (16'h1800 + (i-32)*32);
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

	// Especificacion: K sprites de mas indice que cortan la linea, gana el de mas indice con pixel != 0
	function [4:0] spec;	// {hit, pix}
		input [8:0] px;
		input [8:0] py;
		integer i, n;
		reg [8:0] yd, xd;
		reg [3:0] nb;
		reg got;
		begin
			n = 0; got = 0;
			spec = 5'd0;
			for (i = NSPR - 1; i >= 0; i = i - 1) begin
				yd = py - {1'b0, t_y[i]};
				if (spr_en[i] && yd < 8 && n < K) begin
					n = n + 1;
					xd = px - t_x[i];
					if (!got && xd < 8) begin
						nb = sheet_nib(t_pg[i], yd[2:0], xd[2:0]);
						if (nb != 4'd0) begin
							got = 1;
							spec = {1'b1, nb};
						end
					end
				end
			end
		end
	endfunction

	// escribe la cabecera en la RAM de sombra
	task load_table;
		integer i;
		begin
			for (i = 0; i < NSPR; i = i + 1) begin
				mem[sbase(i) + 0] = spr_en[i];
				mem[sbase(i) + 1] = t_x[i][7:0];
				mem[sbase(i) + 2] = {7'd0, t_x[i][8]};
				mem[sbase(i) + 3] = t_y[i];
				mem[sbase(i) + 4] = t_pg[i];
			end
		end
	endtask

	// una hoja al azar: 256 celdas de 32 bytes; un cuarto de los bytes a 0 y otro cuarto con un nibble a 0
	task load_sheet;
		integer a;
		reg [7:0] b;
		reg [1:0] m;
		begin
			for (a = 0; a < 8192; a = a + 1) begin
				b = $random(seed);
				m = $random(seed);
				if (m == 2'd0) b = 8'h00;
				else if (m == 2'd1) b = b & 8'h0F;
				else if (m == 2'd2) b = b & 8'hF0;
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
		integer i, tries, ysp;
		reg ok;
		begin
			ok = 0; tries = 0;
			// el bloque de la hoja: 2..7 (el 0 y el 1 contienen las tablas, en $0C00 y $1800)
			sheet_base = 3'd2 + (($random(seed) & 32'hFF) % 6);
			load_sheet;
			while (!ok) begin
				tries = tries + 1;
				ysp = dense ? 160 : 200;
				for (i = 0; i < NSPR; i = i + 1) begin
					spr_en[i] = (($random(seed) & 7) != 0);
					t_x[i] = ($random(seed) & 32'h1FF) % 330;
					if (($random(seed) & 31) == 0) t_x[i] = 9'd500 + ($random(seed) & 7);
					t_y[i] = 8'd20 + (($random(seed) & 32'hFF) % ysp);
					// pocas paginas distintas, para que los sprites compartan grafico
					t_pg[i] = (($random(seed) & 3) == 0) ? 8'd255 : (($random(seed) & 32'hFF) % 12);
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
	reg [4:0] sp;
	always @(negedge pixel_clk) begin
		if (!reset && in_display && frame > 0) begin
			compared = compared + 1;
			sp = spec(pos_x, pos_y);
			if (sp[4] !== e_hit || sp[3:0] !== e_pix) begin
				errors = errors + 1;
				if (errors < 10) $display("DIF linea=%0d x=%0d  espec hit=%b pix=%h | motor hit=%b pix=%h",
					line_cnt, pos_x, sp[4], sp[3:0], e_hit, e_pix);
			end
			if (sp[4]) hits = hits + 1;
		end
	end

	initial begin
		if ($value$plusargs("mode80=%d", mode80)) ;
		if ($value$plusargs("dense=%d", dense)) ;
		if ($value$plusargs("frames=%d", frames)) ;
		if ($value$plusargs("seed=%d", seed)) ;
		if ($value$plusargs("worst=%d", worst)) ;
		if ($value$plusargs("lim80=%d", lim80)) ;
		repeat (8) @(posedge pixel_clk);
		reset = 0;
		while (frame < frames) begin
			wait (line_cnt == 9'd262);
			random_table;
			load_table;
			frame = frame + 1;
			wait (line_cnt == 9'd5);
		end
		wait (line_cnt == 9'd260);
		$display("RESULTADO mode80=%0d lim80=%0d dense=%0d worst=%0d: %0d cuadros, %0d pixeles comparados, %0d con sprite, %0d diferencias",
			mode80, lim80, dense, worst, frame, compared, hits, errors);
		if (errors == 0 && hits > 1000) $display("OK");
		else $display("FALLO");
		$finish;
	end
endmodule
