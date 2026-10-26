`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// tb_scroll_motor -- prueba de scroll_motor contra un modelo de software. No forma
// parte del proyecto de ISE: se simula aparte.
//
//   vlog scroll_motor.v tb_scroll_motor.v
//   vsim -c tb_scroll_motor -do "run -all; quit"
//
// La memoria es de 64 KB, con una lectura de un ciclo de latencia (puerto B de la BRAM de
// sombra) y escritura directa (puerto A). Se llena de bytes pseudoaleatorios para que cualquier
// escritura de mas se vea. Cada caso lanza una orden, espera el pulso `done` y compara TODA la
// memoria con la que sale del modelo (escrito con aritmetica ordinaria, sin concatenaciones):
//   - Spectrum, HiRes solo bitmap, HiRes con atributos de modo 0 y de modo 1
//   - izquierda y derecha
//   - mascaras de filas, bloques de pantalla, bases de atributos de modo 1
//   - con paradas aleatorias de sh_busy (la CPU escribiendo) y sin ellas
//   - ordenes sin efecto (texto, direccion 0 y 3) y una orden dada con el motor en marcha
//////////////////////////////////////////////////////////////////////////////////
module tb_scroll_motor;

	reg clk = 1'b0;
	always #5 clk = ~clk;
	reg rst = 1'b1;

	reg start = 1'b0;
	reg [1:0] dir = 2'd0;
	reg mode_sp = 1'b0, mode_hr = 1'b0, attr0_en = 1'b0, attr1_en = 1'b0;
	reg [2:0] blk = 3'd4;
	reg [15:0] a1_base = 16'h0000;
	reg [23:0] row_en = 24'hFFFFFF;
	reg sh_busy = 1'b0;
	wire run, we, done;
	wire [15:0] addr;
	wire [7:0] wdata;
	reg [7:0] rdata;

	scroll_motor dut (.clk(clk), .rst(rst), .start(start), .dir(dir), .mode_sp(mode_sp), .mode_hr(mode_hr),
		.attr0_en(attr0_en), .attr1_en(attr1_en), .blk(blk), .a1_base(a1_base), .row_en(row_en),
		.sh_busy(sh_busy), .rdata(rdata), .run(run), .addr(addr), .we(we), .wdata(wdata), .done(done));

	reg [7:0] mem [0:65535];
	reg [7:0] expm [0:65535];
	always @(posedge clk) begin
		rdata <= mem[addr];					// puerto B: un ciclo de latencia
		if (we) mem[addr] <= wdata;			// puerto A
	end

	integer errors = 0, cases = 0;
	integer seed = 12345;
	integer n_done = 0;
	always @(posedge clk) if (done) n_done <= n_done + 1;

	// paradas aleatorias de la CPU
	reg stalls = 1'b0;
	always @(posedge clk) sh_busy <= stalls && (($random(seed) & 3) == 0);

	integer a;
	task fill;
	begin
		for (a = 0; a < 65536; a = a + 1) mem[a] = $random(seed);
	end
	endtask

	// ---- modelo ----
	integer cfg_kind;		// 0 Spectrum, 1 HiRes bitmap, 2 HiRes + modo 0, 3 HiRes + modo 1
	integer cfg_dir;		// 1 izquierda, 2 derecha
	integer cfg_blk;
	integer cfg_a1;

	integer r, l, c, y;
	integer chain [0:33];	// direcciones de L, m0..m31, R  (-1 = no existe)
	reg [7:0] tmp [0:33];
	integer k;

	task move_chain;			// aplica el movimiento a la cadena `chain` sobre `expm`
		integer q;
	begin
		for (q = 0; q < 34; q = q + 1) tmp[q] = (chain[q] >= 0) ? expm[chain[q]] : 8'h00;
		if (cfg_dir == 1) begin				// izquierda: m_c <- m_{c+1}, m31 <- R, L <- m0
			for (q = 1; q <= 32; q = q + 1) expm[chain[q]] = tmp[q + 1];
			if (chain[0] >= 0) expm[chain[0]] = tmp[1];
		end else begin						// derecha: m_c <- m_{c-1}, m0 <- L, R <- m31
			for (q = 2; q <= 33; q = q + 1) expm[chain[q]] = tmp[q - 1];
			if (chain[0] >= 0) expm[chain[1]] = tmp[0];
			expm[chain[33]] = tmp[32];
		end
	end
	endtask

	integer base;
	task model;
	begin
		for (a = 0; a < 65536; a = a + 1) expm[a] = mem[a];
		base = cfg_blk * 8192;
		// bitmap
		for (r = 0; r < 24; r = r + 1) if (row_en[r])
			for (l = 0; l < 8; l = l + 1) begin
				y = r * 8 + l;
				chain[0] = base + 16'h1C00 + y;
				chain[33] = (cfg_kind == 0) ? base + 16'h1B00 + y : base + 16'h1800 + y;
				for (c = 0; c < 32; c = c + 1)
					chain[c + 1] = (cfg_kind == 0) ? base + (y / 64) * 2048 + (y % 8) * 256 + ((y / 8) % 8) * 32 + c
					                               : base + y * 32 + c;
				move_chain;
			end
		// atributos
		if (cfg_kind == 0)
			for (r = 0; r < 24; r = r + 1) if (row_en[r]) begin
				chain[0] = base + 16'h1CC0 + r;
				chain[33] = base + 16'h1BC0 + r;
				for (c = 0; c < 32; c = c + 1) chain[c + 1] = base + 16'h1800 + r * 32 + c;
				move_chain;
			end
		if (cfg_kind == 2)
			for (r = 0; r < 24; r = r + 1) if (row_en[r])
				for (l = 0; l < 8; l = l + 1) begin
					y = r * 8 + l;
					chain[0] = 16'hC000 + 16'h1C00 + y;
					chain[33] = 16'hC000 + 16'h1800 + y;
					for (c = 0; c < 32; c = c + 1) chain[c + 1] = 16'hC000 + y * 32 + c;
					move_chain;
				end
		if (cfg_kind == 3)
			for (r = 0; r < 24; r = r + 1) if (row_en[r]) begin
				chain[0] = -1;						// sin bufer L
				chain[33] = (cfg_a1 + r * 33 + 32) & 65535;
				for (c = 0; c < 32; c = c + 1) chain[c + 1] = (cfg_a1 + r * 33 + c) & 65535;     // la rejilla da la vuelta a 16 bits
				move_chain;
			end
	end
	endtask

	integer ncyc, ncyc_sp0;
	task run_case(input integer kind, input integer d, input integer b, input integer a1b, input [23:0] mask, input integer st);
		integer tmo;
	begin
		cfg_kind = kind; cfg_dir = d; cfg_blk = b; cfg_a1 = a1b;
		mode_sp = (kind == 0); mode_hr = (kind != 0);
		attr0_en = (kind == 2); attr1_en = (kind == 3);
		blk = b; a1_base = a1b; row_en = mask; stalls = st;
		fill;
		model;
		n_done = 0;
		@(negedge clk); dir = d; start = 1'b1;
		@(negedge clk); start = 1'b0;
		tmo = 0; ncyc = 0;
		while (n_done == 0 && tmo < 400000) begin @(negedge clk); tmo = tmo + 1; end
		ncyc = tmo;
		repeat (8) @(negedge clk);
		if (kind == 0 && !st && mask == 24'hFFFFFF && d == 1) ncyc_sp0 = ncyc;
		cases = cases + 1;
		if (n_done != 1) begin errors = errors + 1; $display("ERROR: %0d pulsos done (kind %0d dir %0d)", n_done, kind, d); end
		if (run) begin errors = errors + 1; $display("ERROR: sigue en marcha"); end
		begin : cmp
			integer bad;
			bad = 0;
			for (a = 0; a < 65536; a = a + 1)
				if (mem[a] !== expm[a]) begin
					if (bad < 4) $display("ERROR: kind %0d dir %0d blk %0d mask %h stalls %0d: mem[%h] = %h, esperado %h", kind, d, b, mask, st, a, mem[a], expm[a]);
					bad = bad + 1;
				end
			if (bad) begin errors = errors + 1; $display("       %0d bytes distintos", bad); end
		end
	end
	endtask

	integer kk, dd, mm, ss;
	reg [23:0] masks [0:3];
	integer blks [0:2];
	integer a1bs [0:2];

	initial begin
		masks[0] = 24'hFFFFFF; masks[1] = 24'h7FFFFE; masks[2] = 24'h000000; masks[3] = 24'hA5C3F0;
		blks[0] = 4; blks[1] = 0; blks[2] = 7;     // el bloque 7: la pantalla acaba en $FFFF
		a1bs[0] = 16'hA000; a1bs[1] = 16'h9FFF; a1bs[2] = 16'hFE00;
		repeat (4) @(negedge clk);
		rst = 1'b0;
		repeat (4) @(negedge clk);
		for (kk = 0; kk < 4; kk = kk + 1)
		for (dd = 1; dd <= 2; dd = dd + 1)
		for (mm = 0; mm < 4; mm = mm + 1)
		for (ss = 0; ss < 2; ss = ss + 1)
			run_case(kk, dd, blks[(mm + kk) % 3], a1bs[(mm + dd) % 3], masks[mm], ss);
		$display("duracion de un movimiento sin paradas, Spectrum, todas las filas: %0d ciclos", ncyc_sp0);

		// ordenes sin efecto: la memoria no cambia y done sale igual
		cfg_kind = 1; cfg_dir = 1; cfg_blk = 4; cfg_a1 = 0;
		for (kk = 0; kk < 4; kk = kk + 1) begin
			fill; for (a = 0; a < 65536; a = a + 1) expm[a] = mem[a];
			mode_sp = (kk == 1); mode_hr = (kk == 2); attr0_en = 1'b0; attr1_en = 1'b0; stalls = 1'b0;
			row_en = 24'hFFFFFF; n_done = 0;
			// 0: direccion 0, 1: texto (ni Spectrum ni HiRes), 2: direccion 3, 3: direccion 0 en Spectrum
			@(negedge clk); dir = (kk == 0) ? 2'd0 : (kk == 2) ? 2'd3 : (kk == 3) ? 2'd0 : 2'd1; start = 1'b1;
			if (kk == 1) begin mode_sp = 1'b0; mode_hr = 1'b0; end
			if (kk == 3) begin mode_sp = 1'b1; end
			@(negedge clk); start = 1'b0;
			repeat (20) @(negedge clk);
			cases = cases + 1;
			if (n_done != 1 || run) begin errors = errors + 1; $display("ERROR: orden sin efecto %0d: done %0d run %b", kk, n_done, run); end
			for (a = 0; a < 65536; a = a + 1) if (mem[a] !== expm[a]) begin errors = errors + 1; $display("ERROR: la orden sin efecto %0d toca mem[%h]", kk, a); a = 65536; end
		end

		// una orden dada con el motor en marcha se ignora (solo termina un movimiento)
		fill; cfg_kind = 1; cfg_dir = 1; cfg_blk = 4; cfg_a1 = 0; row_en = 24'hFFFFFF;
		mode_sp = 1'b0; mode_hr = 1'b1; attr0_en = 1'b0; attr1_en = 1'b0; blk = 4; stalls = 1'b0;
		model; n_done = 0;
		@(negedge clk); dir = 2'd1; start = 1'b1;
		@(negedge clk); start = 1'b0;
		repeat (50) @(negedge clk);
		dir = 2'd2; start = 1'b1; @(negedge clk); start = 1'b0;
		while (n_done == 0) @(negedge clk);
		repeat (10) @(negedge clk);
		cases = cases + 1;
		if (n_done != 1) begin errors = errors + 1; $display("ERROR: una orden con el motor en marcha no se ignora (done %0d)", n_done); end
		for (a = 0; a < 65536; a = a + 1) if (mem[a] !== expm[a]) begin errors = errors + 1; $display("ERROR: la segunda orden altera mem[%h]", a); a = 65536; end

		$display("%0d casos, %0d errores", cases, errors);
		if (errors == 0) $display("PASS"); else $display("FAIL");
		$finish;
	end
endmodule
