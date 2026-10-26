`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// tb_dbg -- banco de pruebas del depurador por hardware (sim_int + m1_tracker,
// conectados como en SD81.v). No forma parte del proyecto de ISE: se simula
// aparte (ModelSim, ISim o iverilog).
//
//   iverilog -o tb tb_dbg.v sim_int.v && vvp tb
//
// El "Z80" es un guion, como en tb_sim_int: hace los ciclos de bus que haria
// un Z80 con los bytes que recibe y comprueba en cada lectura si el byte lo
// sirve la FPGA, cual es, y si la ventana del monitor (bloque 1 -> pagina 63)
// esta puesta. El monitor de juguete vive en $2000: lee el motivo por el
// puerto $3FEF y sale con JP $003B.
//////////////////////////////////////////////////////////////////////////////////
module tb_dbg;

	reg clk = 0;					// reloj del Z80, ~3,25 MHz
	always #154 clk = ~clk;
	wire #40 iclock = ~clk;			// la FPGA ve el reloj con retraso (ver tb_m1_tracker)

	reg nreset = 0;
	reg nM1 = 1, nMREQ = 1, nRD = 1, nWR = 1, nIORQ = 1, nRFSH = 1;
	reg [15:0] addr = 16'h0000;
	reg [7:0] mem_data = 8'h00;		// lo que pondria la SRAM (o la CPU al escribir)
	reg vsync = 0;
	reg superfast = 1;
	reg en_pulse = 0, dis_pulse = 0;
	reg loaded = 0;					// orden 9
	reg tgl = 0;					// orden 8
	reg up_n = 1, down_n = 1;

	wire [7:0] fpga_data;
	wire fpga_en;
	wire [7:0] bus = fpga_en ? fpga_data : mem_data;	// lo que ve la CPU
	wire boundary, index_prefix;
	wire [7:0] port_out;
	wire [1:0] si_state;
	wire si_enabled, dbg_win, dbg_mon;
	wire bram_rd;
	wire bram_wr;
	wire [15:0] bram_ptr;
	// escrituras en la "BRAM" por el registro 7: cuantas, y la ultima
	integer bram_wr_n = 0;
	reg [15:0] bram_wr_addr = 0;
	reg [7:0] bram_wr_data = 0;
	always @(posedge bram_wr) begin
		bram_wr_n = bram_wr_n + 1;
		#5 bram_wr_addr = bram_ptr; bram_wr_data = mem_data;
	end
	wire [7:0] bram_data = bram_ptr[7:0] ^ 8'h5A;	// la "BRAM": un patron por direccion
	integer errors = 0;

	// la traza: trace_wr a system_clk (26 MHz) y la sombra que escribe
	reg sysclk = 0;
	always #19 sysclk = ~sysclk;
	reg [7:0] vid_st = 8'hA5;		// el estado del video que da SD81.v (indice 9)
	reg [2:0] vid_fr = 3'd5;		// y el bloque de delante del doble buffer (indice 10)
	reg blk0_w = 1'b0;				// el bloque 0 es RAM (indice 12)
	reg trace = 0;					// orden 12
	reg skipv = 0;					// tr_skip (video o HALT, de SD81.v)
	wire tr_go;
	wire [15:0] tr_pc;
	wire [9:0] tr_ptr;
	wire [2:0] scr_fine_y;		// registros del scroll (spec_scroll_addr.v)
	wire [4:0] scr_reg_top, scr_reg_bot;
	wire tr_we;
	wire [15:0] tr_waddr;
	wire [7:0] tr_wdata;
	reg [7:0] shadow [0:65535];
	always @(posedge sysclk) if (tr_we) shadow[tr_waddr] <= tr_wdata;

	// motor de scroll: sim_int (orden y estado) -> scroll_sync -> scroll_motor, con una "memoria" de 64 KB y un
	// pulso blit_edge (la linea 254) cada 20.000 ciclos de system_clk. Solo mueve la fila 0 del bitmap de HiRes
	wire [2:0] scr_cmd;
	wire scr_cmd_tog;
	wire m_tstart, m_tdone, m_go, m_run, m_we, m_done;
	wire [15:0] m_addr;
	wire [7:0] m_wdata;
	reg [7:0] mmem [0:65535];
	reg [7:0] m_rdata;
	function [7:0] pat(input [15:0] a);			// el contenido inicial de la memoria
		pat = a * 7 + (a >> 8) * 13 + 1;
	endfunction
	integer be_cnt = 0;
	reg be = 1'b0;
	always @(posedge sysclk) begin
		be_cnt <= (be_cnt == 19999) ? 0 : be_cnt + 1;
		be <= (be_cnt == 0);
		m_rdata <= mmem[m_addr];
		if (m_we) mmem[m_addr] <= m_wdata;
	end
	scroll_sync ssy (.clk(sysclk), .rst(~nreset), .cmd_tog(scr_cmd_tog), .blit_edge(be), .mot_run(m_run),
		.mot_done(m_done), .mot_go(m_go), .tstart(m_tstart), .tdone(m_tdone));
	scroll_motor smo (.clk(sysclk), .rst(~nreset), .start(m_go), .dir(scr_cmd[1:0]), .mode_sp(1'b0), .mode_hr(1'b1),
		.attr0_en(1'b0), .attr1_en(1'b0), .blk(3'd4), .a1_base(16'h0000), .row_en(24'h000001), .sh_busy(1'b0),
		.rdata(m_rdata), .run(m_run), .addr(m_addr), .we(m_we), .wdata(m_wdata), .done(m_done));
	trace_wr tw (.clk(sysclk), .tr_go(tr_go), .tr_pc(tr_pc), .ptr(tr_ptr), .we(tr_we), .waddr(tr_waddr), .wdata(tr_wdata));

	m1_tracker trk (
		.iclock(iclock), .nreset(nreset), .nM1(nM1), .nMREQ(nMREQ), .nRFSH(nRFSH),
		.data(bus), .boundary(boundary), .index_prefix(index_prefix));

	sim_int si (
		.iclock(iclock), .nreset(nreset), .enable_int(en_pulse), .disable_int(dis_pulse),
		.superfast_mode(superfast), .vsync(vsync), .boundary(boundary),
		.index_prefix(index_prefix), .int_addr(16'h7000), .addr(addr), .data(bus), .nM1(nM1),
		.nRD(nRD), .nWR(nWR), .nMREQ(nMREQ), .nIORQ(nIORQ), .nRFSH(nRFSH),
		.dbg_loaded(loaded), .dbg_pause_tgl(tgl), .joy_up_n(up_n), .joy_down_n(down_n),
		.data_out(fpga_data), .enable_out(fpga_en), .state(si_state),
		.enabled(si_enabled), .dbg_win(dbg_win), .dbg_mon(dbg_mon), .port_out(port_out),
		.bram_rd(bram_rd), .bram_wr(bram_wr), .bram_ptr(bram_ptr), .bram_data(bram_data), .chroma_reg(8'h3C), .ay_sel_a(8'h0D), .ay_sel_b(8'h07), .sram_wr(1'b0), .sram_page(6'd0), .dirty_clr(1'b0),
		.trace_en(trace), .tr_skip(skipv), .tr_ptr(tr_ptr), .vid_status(vid_st), .vid_front(vid_fr), .blk0_w(blk0_w), .cfg_flags(8'h5A), .cfg_raw(8'hA5), .tr_go(tr_go), .tr_pc(tr_pc),
		.scr_fine_y(scr_fine_y), .scr_reg_top(scr_reg_top), .scr_reg_bot(scr_reg_bot),
			.scr_cmd(scr_cmd), .scr_cmd_tog(scr_cmd_tog), .scr_tstart(m_tstart), .scr_tdone(m_tdone));

	reg [7:0] got;			// el byte que ha leido la CPU en la ultima lectura
	reg got_fpga;			// si lo servia la FPGA
	reg got_win;			// si la ventana del monitor estaba puesta en ese ciclo

	// ---------------- ciclos de bus ----------------
	task m1(input [15:0] a, input [7:0] m);
	begin
		@(posedge clk); #10 nM1 = 0; addr = a;
		@(negedge clk); #10 nMREQ = 0; nRD = 0;
		#60 mem_data = m;
		@(posedge clk);										// T2
		@(posedge clk); got = bus; got_fpga = fpga_en; got_win = dbg_win;	// T3: la CPU latchea
		#10 nM1 = 1; nMREQ = 1; nRD = 1; nRFSH = 0; addr = 16'h1E00; mem_data = 8'hFF;
		@(negedge clk); #10 nMREQ = 0;
		@(posedge clk);										// T4
		@(negedge clk); #10 nMREQ = 1;
		#10 nRFSH = 1;
	end
	endtask

	task mem_rd(input [15:0] a, input [7:0] m);
	begin
		@(posedge clk); #10 addr = a;
		@(negedge clk); #10 nMREQ = 0; nRD = 0;
		#60 mem_data = m;
		@(posedge clk);
		@(negedge clk); got = bus; got_fpga = fpga_en; got_win = dbg_win;	// la CPU latchea en la bajada de T3
		#10 nMREQ = 1; nRD = 1;
	end
	endtask

	task mem_wr(input [15:0] a, input [7:0] v);
	begin
		@(posedge clk); #10 addr = a; mem_data = v;
		@(negedge clk); #10 nMREQ = 0;
		@(posedge clk);
		@(negedge clk); #10 nWR = 0;
		@(posedge clk); got_win = dbg_win;
		@(negedge clk); #10 nMREQ = 1; nWR = 1;
	end
	endtask

	// OUT (C),A: T1, T2 (IORQ y WR bajan en la subida), TW, T3
	task io_out(input [15:0] a, input [7:0] v);
	begin
		@(posedge clk); #10 addr = a; mem_data = v;
		@(posedge clk); #10 nIORQ = 0; nWR = 0;
		@(posedge clk);										// TW
		@(posedge clk);										// T3
		@(negedge clk); #10 nIORQ = 1; nWR = 1;
	end
	endtask

	// IN A,(C): deja en got lo que devuelve el puerto $3FEF (y en got_bram si
	// la FPGA estaba leyendo la BRAM)
	reg got_bram;
	task io_in(input [15:0] a);
	begin
		@(posedge clk); #10 addr = a;
		@(posedge clk); #10 nIORQ = 0; nRD = 0;
		@(posedge clk);
		@(posedge clk);
		@(negedge clk); got = port_out; got_bram = bram_rd;
		#10 nIORQ = 1; nRD = 1;
	end
	endtask

	// ---------------- comprobaciones ----------------
	task expect_fpga(input [7:0] want, input [8*36-1:0] what);
	begin
		if (!got_fpga || got !== want) begin
			$display("ERROR %0s: %h (FPGA=%0d), esperaba %h de la FPGA", what, got, got_fpga, want);
			errors = errors + 1;
		end else
			$display("ok    %0s: %h", what, got);
	end
	endtask

	task expect_mem(input [7:0] want, input [8*36-1:0] what);
	begin
		if (got_fpga || got !== want) begin
			$display("ERROR %0s: %h (FPGA=%0d), esperaba %h de la memoria", what, got, got_fpga, want);
			errors = errors + 1;
		end else
			$display("ok    %0s: %h", what, got);
	end
	endtask

	task expect_val(input integer v, input integer want, input [8*36-1:0] what);
	begin
		if (v !== want) begin
			$display("ERROR %0s: %0d, esperaba %0d", what, v, want);
			errors = errors + 1;
		end else
			$display("ok    %0s: %0d", what, v);
	end
	endtask

	task pulse_vsync;
	begin
		vsync = 1; #3000 vsync = 0;
	end
	endtask

	// Programa un registro del depurador por el puerto
	task dreg(input [2:0] r, input [7:0] v);
	begin
		io_out(16'h3FEF, {5'b10000, r});
		io_out(16'h3FEF, v);
	end
	endtask

	// ---------------- secuencias del depurador ----------------
	// Desde la M1 en x que ha recibido el FF hasta que el monitor (en $2000)
	// ha leido el motivo, que tiene que ser why. sp_hi: byte alto de la pila.
	task dbg_enter(input [15:0] x, input [2:0] why);
	begin
		expect_fpga(8'hFF, "ruptura: FF en la M1");
		mem_wr(16'h7FFF, x[15:8]); mem_wr(16'h7FFE, x[7:0] + 8'd1);	// RST: guarda x+1
		m1(16'h0038, 8'hF5);  expect_fpga(8'hCD, "CALL en $0038");
		mem_rd(16'h0039, 8'h11); expect_fpga(8'h00, "CALL lo");
		mem_rd(16'h003A, 8'h22); expect_fpga(8'h20, "CALL hi");
		mem_wr(16'h7FFD, 8'h00); expect_val(got_win, 0, "1a escritura del CALL sin ventana");
		mem_wr(16'h7FFC, 8'h3B); expect_val(got_win, 0, "2a escritura del CALL sin ventana");
		m1(16'h2000, 8'hC3);  expect_val(got_win, 1, "M1 en $2000 con ventana");
		expect_mem(8'hC3, "monitor: JP entrada (de la SRAM)");
		mem_rd(16'h2001, 8'h10); mem_rd(16'h2002, 8'h20);
		expect_val(dbg_mon, 1, "monitor activo");
		// el monitor lee el motivo: OUT 0 (indice 0) e IN
		m1(16'h2010, 8'hED); m1(16'h2011, 8'h79);
		io_out(16'h3FEF, 8'h00);
		m1(16'h2012, 8'hED); m1(16'h2013, 8'h78);
		io_in(16'h3FEF);
		expect_val(got[2:0], why, "motivo");
		expect_val(got[7], 1, "armado");
	end
	endtask

	// JP $003B desde el monitor y epilogo hasta volver a x. El monitor ya ha
	// dejado SP como antes del CALL (sin el $003B): arriba esta x+1
	task dbg_exit(input [15:0] x);
	begin
		m1(16'h2020, 8'hC3); mem_rd(16'h2021, 8'h3B); mem_rd(16'h2022, 8'h00);
		expect_val(got_win, 1, "JP $003B: operandos en la ventana");
		m1(16'h003B, 8'hFF);  expect_fpga(8'hE3, "salida: EX (SP),HL");
		expect_val(dbg_mon, 0, "monitor ya no esta activo");
		mem_rd(16'h7FFE, x[7:0] + 8'd1); expect_val(got_win, 0, "EX (SP),HL lee sin ventana");
		mem_rd(16'h7FFF, x[15:8]);
		mem_wr(16'h7FFF, 8'h00); mem_wr(16'h7FFE, 8'h00);
		m1(16'h003C, 8'hFF);  expect_fpga(8'h2B, "salida: DEC HL");
		m1(16'h003D, 8'hFF);  expect_fpga(8'hE3, "salida: EX (SP),HL");
		mem_rd(16'h7FFE, x[7:0]); mem_rd(16'h7FFF, x[15:8]);
		mem_wr(16'h7FFF, 8'h00); mem_wr(16'h7FFE, 8'h00);
		m1(16'h003E, 8'hFF);  expect_fpga(8'hC9, "salida: RET");
		mem_rd(16'h7FFE, x[7:0]); mem_rd(16'h7FFF, x[15:8]);
	end
	endtask

	// Interrupcion simulada completa desde la M1 en x que recibio el FF; la
	// rutina en $7000 es un NOP y un RET
	task sim_interrupt(input [15:0] x);
	begin
		expect_fpga(8'hFF, "interrupcion simulada: FF");
		mem_wr(16'h7FFF, 8'h00); mem_wr(16'h7FFE, 8'h00);
		m1(16'h0038, 8'hF5);  expect_fpga(8'hCD, "sim: CALL");
		mem_rd(16'h0039, 0); mem_rd(16'h003A, 0);
		mem_wr(16'h7FFD, 0); mem_wr(16'h7FFC, 0);
		m1(16'h7000, 8'h00);  expect_mem(8'h00, "sim: rutina NOP");
		m1(16'h7001, 8'hC9);  mem_rd(16'h7FFC, 8'h3B); mem_rd(16'h7FFD, 8'h00);
		m1(16'h003B, 8'hFF);  expect_fpga(8'hE3, "sim: epilogo");
		mem_rd(16'h7FFE, 0); mem_rd(16'h7FFF, 0); mem_wr(16'h7FFF, 0); mem_wr(16'h7FFE, 0);
		m1(16'h003C, 8'hFF); m1(16'h003D, 8'hFF);
		mem_rd(16'h7FFE, 0); mem_rd(16'h7FFF, 0); mem_wr(16'h7FFF, 0); mem_wr(16'h7FFE, 0);
		m1(16'h003E, 8'hFF);  expect_fpga(8'hC9, "sim: RET");
		mem_rd(16'h7FFE, x[7:0]); mem_rd(16'h7FFF, x[15:8]);
	end
	endtask

	initial begin
		#1000 nreset = 1;

		// --- desarmado: nada rompe, ni un FF de memoria ---
		tgl = ~tgl;
		repeat (4) @(posedge clk);
		m1(16'h6000, 8'h00); expect_mem(8'h00, "desarmado: sin pausa");
		m1(16'h6001, 8'hFF); expect_mem(8'hFF, "desarmado: FF de memoria");
		mem_wr(16'h7FFF, 0); mem_wr(16'h7FFE, 0);
		m1(16'h0038, 8'hF5); expect_mem(8'hF5, "desarmado: $0038 de la ROM");
		io_out(16'h3FEF, 8'h0F); io_in(16'h3FEF);
		expect_val(got, 8'h52, "firma");

		// --- lectura de la BRAM de sombra (indice 2), no depende de armar ---
		dreg(3'd5, 8'hF6); dreg(3'd6, 8'h07);					// puntero = $07F6
		io_out(16'h3FEF, 8'h02);
		io_in(16'h3FEF);
		expect_val(got_bram, 1, "BRAM: lectura en curso durante el IN");
		expect_val(got, 8'hF6 ^ 8'h5A, "BRAM: byte en $07F6");
		io_in(16'h3FEF);
		expect_val(got, 8'hF7 ^ 8'h5A, "BRAM: el puntero avanza ($07F7)");
		io_in(16'h3FEF);
		repeat (2) @(posedge clk);								// avanza al acabar el IN
		expect_val(bram_ptr, 16'h07F9, "BRAM: puntero tras tres IN");
		io_out(16'h3FEF, 8'h03);
		io_in(16'h3FEF);
		expect_val(got, 8'h3C, "registro de Chroma (indice 3)");
		io_out(16'h3FEF, 8'h04);
		io_in(16'h3FEF);
		expect_val(got, 8'h0D, "registro elegido del AY A (indice 4)");
		io_out(16'h3FEF, 8'h05);
		io_in(16'h3FEF);
		expect_val(got, 8'h07, "registro elegido del AY B (indice 5)");
		io_out(16'h3FEF, 8'h0F);
		io_in(16'h3FEF);
		expect_val(got_bram, 0, "BRAM: otro indice no lee la BRAM");
		expect_val(bram_ptr, 16'h07F9, "BRAM: otro indice, mismo puntero");

		// --- escritura de la BRAM de sombra (registro 7) ---
		dreg(3'd5, 8'h00); dreg(3'd6, 8'h20);					// puntero = $2000
		expect_val(bram_wr_n, 0, "BRAM: cargar el puntero no escribe");
		io_out(16'h3FEF, 8'h87);
		expect_val(bram_wr_n, 0, "BRAM: el OUT $87 no escribe");
		io_out(16'h3FEF, 8'h11);
		expect_val(bram_wr_n, 1, "BRAM: el dato escribe una vez");
		expect_val(bram_wr_addr, 16'h2000, "BRAM: en el puntero");
		expect_val(bram_wr_data, 8'h11, "BRAM: el dato del bus");
		repeat (2) @(posedge clk);
		expect_val(bram_ptr, 16'h2001, "BRAM: el puntero avanza al escribir");
		dreg(3'd7, 8'h22);
		expect_val(bram_wr_n, 2, "BRAM: segunda escritura");
		expect_val(bram_wr_addr, 16'h2001, "BRAM: en el puntero siguiente");
		expect_val(bram_wr_data, 8'h22, "BRAM: segundo dato");
		io_out(16'h3FEF, 8'h02);
		io_out(16'h1FEF, 8'h87); io_out(16'h1FEF, 8'h33);		// otro puerto
		repeat (2) @(posedge clk);
		expect_val(bram_wr_n, 2, "BRAM: otro puerto no escribe");
		expect_val(bram_ptr, 16'h2002, "BRAM: puntero tras dos escrituras");
		dreg(3'd2, 8'h00);
		expect_val(bram_wr_n, 2, "BRAM: otro registro no escribe");

		loaded = 1;
		repeat (4) @(posedge clk);

		// --- pausa desde el MCU (orden 8) ---
		tgl = ~tgl;
		repeat (4) @(posedge clk);
		m1(16'h6100, 8'h00);
		dbg_enter(16'h6100, 3'd1);
		dbg_exit(16'h6100);
		m1(16'h6100, 8'h3E); expect_mem(8'h3E, "vuelta a X: la instruccion de verdad");
		mem_rd(16'h6101, 8'h05);
		m1(16'h6102, 8'h00); expect_mem(8'h00, "sigue sin romper");

		// --- breakpoint por software: FF en la memoria ---
		m1(16'h6200, 8'hFF);
		dbg_enter(16'h6200, 3'd7);
		dbg_exit(16'h6200);
		m1(16'h6200, 8'h00); expect_mem(8'h00, "tras el FF: el byte repuesto");

		// --- comparador de ejecucion ---
		dreg(3'd0, 8'h30); dreg(3'd1, 8'h63); dreg(3'd2, 8'd1);	// $6330, ejecucion
		m1(16'h6300, 8'h00); expect_mem(8'h00, "comparador: otra direccion");
		m1(16'h6330, 8'h00);
		dbg_enter(16'h6330, 3'd5);
		dbg_exit(16'h6330);
		m1(16'h6330, 8'h00); expect_mem(8'h00, "comparador: al continuar no rompe (skip)");
		m1(16'h6331, 8'h00); expect_mem(8'h00, "comparador: la siguiente tampoco");
		m1(16'h6330, 8'h00); expect_fpga(8'hFF, "comparador: la segunda vez si rompe");
		dbg_enter(16'h6330, 3'd5);
		// --- paso a paso: N = 2, y el comparador apagado ---
		dreg(3'd2, 8'd0);
		dreg(3'd3, 8'd2); dreg(3'd4, 8'd0);
		dbg_exit(16'h6330);
		m1(16'h6330, 8'h00); expect_mem(8'h00, "paso: 1a instruccion");
		m1(16'h6331, 8'hCB); expect_mem(8'hCB, "paso: 2a instruccion (CB)");
		m1(16'h6332, 8'h07); expect_mem(8'h07, "paso: segundo byte de CB, no cuenta");
		m1(16'h6333, 8'h00);
		dbg_enter(16'h6333, 3'd4);
		dbg_exit(16'h6333);
		m1(16'h6333, 8'h00); expect_mem(8'h00, "paso: sin N no vuelve a romper");
		m1(16'h6334, 8'h00); expect_mem(8'h00, "paso: la siguiente tampoco");

		// --- punto de vigilancia de escritura ---
		dreg(3'd0, 8'h00); dreg(3'd1, 8'h70); dreg(3'd2, 8'd3);	// escritura en $7000
		m1(16'h6400, 8'h32); mem_rd(16'h6401, 8'h00); mem_rd(16'h6402, 8'h71);
		mem_wr(16'h7100, 8'h55);								// otra direccion
		m1(16'h6403, 8'h32); expect_mem(8'h32, "vigilancia: otra direccion no rompe");
		mem_rd(16'h6404, 8'h00); mem_rd(16'h6405, 8'h70);
		mem_wr(16'h7000, 8'h55);								// LD ($7000),A
		m1(16'h6406, 8'h00);
		dbg_enter(16'h6406, 3'd6);
		dreg(3'd2, 8'd0);
		dbg_exit(16'h6406);
		m1(16'h6406, 8'h00); expect_mem(8'h00, "vigilancia: vuelta");

		// --- vigilancia de E/S (byte bajo del puerto) ---
		dreg(3'd0, 8'hFE); dreg(3'd2, 8'd4);
		dbg_exit_dummy;

		// --- trampa OUT $10 ---
		m1(16'h6500, 8'hED); m1(16'h6501, 8'h79);
		io_out(16'h3FEF, 8'h10);
		m1(16'h6502, 8'h00);
		dbg_enter(16'h6502, 3'd3);
		dbg_exit(16'h6502);
		m1(16'h6502, 8'h00); expect_mem(8'h00, "trampa: vuelta");

		// --- SLOW (ha habido NMI de linea, M1 en $0066, hace menos de un
		//     cuadro): solo rompe en la M1 de la NMI ---
		m1(16'h0066, 8'h08); expect_mem(8'h08, "SLOW: una NMI sin nada pendiente");
		m1(16'h0067, 8'h3C);
		io_out(16'h3FEF, 8'h10);
		m1(16'h6600, 8'h00); expect_mem(8'h00, "SLOW: fuera de la NMI no rompe");
		m1(16'h6601, 8'hFF); expect_fpga(8'h18, "SLOW: FF fuera de la NMI: JR $");
		mem_rd(16'h6602, 8'h77); expect_fpga(8'hFE, "SLOW: y su operando FE");
		repeat (160) @(posedge clk);
		m1(16'h6601, 8'hFF); expect_fpga(8'h18, "SLOW: el JR $ vuelve al FF");
		mem_rd(16'h6602, 8'h77); expect_fpga(8'hFE, "SLOW: operando otra vez");
		m1(16'h0066, 8'h08);									// la NMI siguiente: rompe
		dbg_enter(16'h0066, 3'd7);								// (el FF manda: motivo 7)
		io_out(16'h3FEF, 8'h00); io_in(16'h3FEF);
		expect_val(got[6], 1, "SLOW: en SLOW al parar");
		dbg_exit(16'h0066);
		m1(16'h0066, 8'h08); expect_mem(8'h08, "SLOW: vuelta: la NMI sigue");
		io_out(16'h3FEF, 8'h10);								// trampa: en la NMI siguiente
		m1(16'h6603, 8'h00); expect_mem(8'h00, "SLOW: la trampa espera a la NMI");
		m1(16'h0066, 8'h08);
		dbg_enter(16'h0066, 3'd3);
		dbg_exit(16'h0066);
		m1(16'h0066, 8'h08);
		// --- FAST: un cuadro entero sin NMI ---
		repeat (65600) @(posedge clk);
		io_out(16'h3FEF, 8'h10);
		m1(16'h6605, 8'h00);
		dbg_enter(16'h6605, 3'd3);
		io_out(16'h3FEF, 8'h00); io_in(16'h3FEF);
		expect_val(got[6], 0, "FAST: no en SLOW al parar");
		dbg_exit(16'h6605);
		m1(16'h6605, 8'h00);

		// --- joystick: arriba y abajo a la vez ya NO pausa (rev 0.18) ---
		up_n = 0; #2000 down_n = 0;
		repeat (4) @(posedge clk);
		m1(16'h6700, 8'h00); expect_mem(8'h00, "joystick: arriba+abajo no pausa");
		repeat (4) @(posedge clk);
		m1(16'h6701, 8'h00); expect_mem(8'h00, "joystick: sigue sin pausar");
		up_n = 1; down_n = 1;

		// --- anidado: breakpoint por software dentro de la rutina de interrupcion ---
		en_pulse = 1; #400 en_pulse = 0;						// POKE 2040,1
		pulse_vsync;
		m1(16'h6800, 8'h00);
		expect_fpga(8'hFF, "sim: FF");
		mem_wr(16'h7FFF, 8'h68); mem_wr(16'h7FFE, 8'h01);
		m1(16'h0038, 8'hF5);  expect_fpga(8'hCD, "sim: CALL");
		mem_rd(16'h0039, 0); expect_fpga(8'h00, "sim: CALL lo");
		mem_rd(16'h003A, 0); expect_fpga(8'h70, "sim: CALL hi");
		mem_wr(16'h7FFD, 0); mem_wr(16'h7FFC, 8'h3B);
		m1(16'h7000, 8'hFF);									// FF en la rutina
		dbg_enter(16'h7000, 3'd7);
		io_out(16'h3FEF, 8'h00); io_in(16'h3FEF);
		expect_val(got[5], 1, "anidado: nivel = rutina");
		expect_val(si_state, 1, "anidado: sim congelada en ENTRY");
		dbg_exit(16'h7000);
		m1(16'h7000, 8'h00); expect_mem(8'h00, "anidado: la rutina sigue (byte repuesto)");
		m1(16'h7001, 8'hC9); mem_rd(16'h7FFC, 8'h3B); mem_rd(16'h7FFD, 8'h00);
		m1(16'h003B, 8'hFF); expect_fpga(8'hE3, "anidado: epilogo de sim");
		mem_rd(16'h7FFE, 8'h01); mem_rd(16'h7FFF, 8'h68); mem_wr(16'h7FFF, 0); mem_wr(16'h7FFE, 0);
		m1(16'h003C, 8'hFF); expect_fpga(8'h2B, "anidado: DEC HL de sim");
		m1(16'h003D, 8'hFF);
		mem_rd(16'h7FFE, 0); mem_rd(16'h7FFF, 8'h68); mem_wr(16'h7FFF, 0); mem_wr(16'h7FFE, 0);
		m1(16'h003E, 8'hFF); expect_fpga(8'hC9, "anidado: RET de sim");
		mem_rd(16'h7FFE, 0); mem_rd(16'h7FFF, 8'h68);
		m1(16'h6800, 8'h00); expect_mem(8'h00, "anidado: vuelta al principal");

		// --- el paso se queda en su nivel: una interrupcion por medio no cuenta ---
		io_out(16'h3FEF, 8'h10);
		m1(16'h6900, 8'h00);
		dbg_enter(16'h6900, 3'd3);
		dreg(3'd3, 8'd1); dreg(3'd4, 8'd0);					// N = 1
		pulse_vsync;											// llega un VSYNC con el monitor parado
		dbg_exit(16'h6900);
		m1(16'h6900, 8'h00);									// la interrupcion entra aqui
		sim_interrupt(16'h6900);
		m1(16'h6900, 8'h00); expect_mem(8'h00, "nivel: la instruccion de X (cuenta)");
		m1(16'h6901, 8'h00);
		dbg_enter(16'h6901, 3'd4);
		dbg_exit(16'h6901);
		m1(16'h6901, 8'h00);

		// --- con la interrupcion simulada esperando en un HALT no se rompe ---
		m1(16'h6A00, 8'h76); expect_fpga(8'hFF, "HALT: FF de sim");
		mem_wr(16'h7FFF, 0); mem_wr(16'h7FFE, 0);
		tgl = ~tgl;											// pausa mientras espera
		repeat (4) @(posedge clk);
		m1(16'h0038, 8'hF5); expect_fpga(8'h18, "HALT: JR $ (no rompe)");
		mem_rd(16'h0039, 0); expect_fpga(8'hFE, "HALT: JR $ (FE)");
		pulse_vsync;
		m1(16'h0038, 8'hF5); expect_fpga(8'hCD, "HALT: CALL");
		mem_rd(16'h0039, 0); mem_rd(16'h003A, 0);
		mem_wr(16'h7FFD, 0); mem_wr(16'h7FFC, 0);
		m1(16'h7000, 8'h00);									// la pausa rompe aqui, dentro
		dbg_enter(16'h7000, 3'd1);
		dbg_exit(16'h7000);
		m1(16'h7000, 8'h00);
		m1(16'h7001, 8'hC9); mem_rd(16'h7FFC, 8'h3B); mem_rd(16'h7FFD, 8'h00);
		m1(16'h003B, 8'hFF); expect_fpga(8'hC9, "HALT: RET de sim tras la pausa");

		// --- estado del video (indices 9, 10 y 11): lo que la captura de pantalla no puede sacar de la sombra ---
		io_out(16'h3FEF, 8'h09); io_in(16'h3FEF); expect_val(port_out, 8'hA5, "indice 9: estado del video");
		vid_st = 8'h5A; io_in(16'h3FEF); expect_val(port_out, 8'h5A, "indice 9: sigue a SD81.v");
		io_out(16'h3FEF, 8'h0A); io_in(16'h3FEF); expect_val(port_out, 5, "indice 10: bloque de delante");
		vid_fr = 3'd4; io_in(16'h3FEF); expect_val(port_out, 4, "indice 10: sigue a SD81.v");
		io_out(16'h3FEF, 8'h0B); io_in(16'h3FEF); expect_val(port_out, 8'hC4, "indice 11: la FPGA los tiene");
		io_out(16'h3FEF, 8'h0C); io_in(16'h3FEF); expect_val(port_out, 8'hA8, "indice 12: $A8, bloque 0 en ROM");
		blk0_w = 1'b1; io_in(16'h3FEF); expect_val(port_out, 8'hA9, "indice 12: $A9, bloque 0 en RAM");
		io_out(16'h3FEF, 8'h0D); io_in(16'h3FEF); expect_val(port_out, 8'h5A, "indice 13: los registros de configuracion");
		io_out(16'h3FEF, 8'h0E); io_in(16'h3FEF); expect_val(port_out, 8'hA5, "indice 14: el registro de desplazamiento");
		blk0_w = 1'b0;
		io_out(16'h3FEF, 8'h0B); io_in(16'h3FEF); expect_val(port_out, 8'hC4, "indice 11: sigue valiendo $C4");
		io_out(16'h3FEF, 8'h00);

		// --- traza (orden 12): el PC de cada instruccion del programa ---
		dis_pulse = 1; @(posedge clk); dis_pulse = 0;
		m1(16'h4000, 8'h00); repeat (8) @(posedge sysclk);
		expect_val(tr_ptr, 0, "traza apagada: no apunta");
		trace = 1; repeat (4) @(posedge clk);
		m1(16'h4000, 8'h00);
		m1(16'h4001, 8'hCB); m1(16'h4002, 8'h47);				// BIT 0,A: dos M1, una instruccion
		m1(16'h4003, 8'h00);
		skipv = 1; m1(16'h8100, 8'h00); skipv = 0;				// video (NOP forzado): no
		repeat (8) @(posedge sysclk);
		expect_val(tr_ptr, 3, "traza: tres instrucciones");
		expect_val({shadow[16'h1001], shadow[16'h1000]}, 16'h4000, "traza: 4000");
		expect_val({shadow[16'h1003], shadow[16'h1002]}, 16'h4001, "traza: 4001 (CB 47, una)");
		expect_val({shadow[16'h1005], shadow[16'h1004]}, 16'h4003, "traza: 4003");
		tgl = ~tgl; repeat (4) @(posedge clk);					// una pausa
		m1(16'h4004, 8'h00);
		dbg_enter(16'h4004, 3'd1);
		expect_val(tr_ptr, 3, "traza: ni el FF de la ruptura ni el monitor");
		io_out(16'h3FEF, 8'h07); io_in(16'h3FEF);
		expect_val(got, 3, "traza: el puntero (indice 7)");
		io_out(16'h3FEF, 8'h08); io_in(16'h3FEF);
		expect_val(got, 0, "traza: los bits altos (indice 8)");
		dbg_exit(16'h4004);
		expect_val(tr_ptr, 3, "traza: ni el epilogo");
		m1(16'h4004, 8'h00); repeat (8) @(posedge sysclk);
		expect_val(tr_ptr, 4, "traza: al volver, la instruccion de la ruptura");
		expect_val({shadow[16'h1007], shadow[16'h1006]}, 16'h4004, "traza: 4004");
		trace = 0;

		// ---- registros del scroll: OUT $40+r elige el registro 8+r (fino Y $40, region $42 y $43); IN indices $20+n ----
		expect_val(scr_fine_y, 0, "scroll: fino Y por defecto");
		expect_val(scr_reg_top, 0, "scroll: region, primera fila por defecto");
		expect_val(scr_reg_bot, 23, "scroll: region, ultima fila por defecto");
		io_out(16'h3FEF, 8'h40); io_out(16'h3FEF, 8'h05);			// registro 8 (fino Y) = 5
		expect_val(scr_fine_y, 5, "scroll: fino Y = 5");
		io_out(16'h3FEF, 8'h40); io_out(16'h3FEF, 8'hFF);			// solo cuentan los 3 bits bajos
		expect_val(scr_fine_y, 7, "scroll: fino Y = 7 (enmascarado)");
		io_out(16'h3FEF, 8'h42); io_out(16'h3FEF, 8'h03);			// registro 10 (primera fila) = 3
		io_out(16'h3FEF, 8'h43); io_out(16'h3FEF, 8'h13);			// registro 11 (ultima fila) = 19
		expect_val(scr_reg_top, 3, "scroll: primera fila = 3");
		expect_val(scr_reg_bot, 19, "scroll: ultima fila = 19");
		io_out(16'h3FEF, 8'h42); io_out(16'h3FEF, 8'hFF);
		expect_val(scr_reg_top, 31, "scroll: primera fila enmascarada a 5 bits");
		io_out(16'h3FEF, 8'h42); io_out(16'h3FEF, 8'h03);
		io_out(16'h3FEF, 8'h21); io_in(16'h3FEF);
		expect_val(got, 7, "scroll: IN indice 17 = fino Y");
		io_out(16'h3FEF, 8'h22); io_in(16'h3FEF);
		expect_val(got, 3, "scroll: IN indice 18 = primera fila");
		io_out(16'h3FEF, 8'h23); io_in(16'h3FEF);
		expect_val(got, 19, "scroll: IN indice 19 = ultima fila");
		io_out(16'h3FEF, 8'h20); io_in(16'h3FEF);
		expect_val(got, 0, "scroll: IN indice 16 = estado (0)");
		io_out(16'h3FEF, 8'h4F); io_out(16'h3FEF, 8'hFF);			// registro 23: no existe
		expect_val(scr_fine_y, 7, "scroll: un registro sin uso no toca nada");
		expect_val(scr_reg_bot, 19, "scroll: ... ni la region");
		// lo de siempre sigue donde estaba
		io_out(16'h3FEF, 8'h0F); io_in(16'h3FEF);
		expect_val(got, 8'h52, "scroll: la firma (indice 15) sigue");
		io_out(16'h3FEF, 8'h0D); io_in(16'h3FEF);
		expect_val(got, 8'h5A, "scroll: indice 13 sigue");
		dreg(3'd0, 8'h34);											// $80+r: los registros de siempre
		expect_val(scr_fine_y, 7, "scroll: dreg no toca el fino Y");
		io_out(16'h3FEF, 8'h2F); io_in(16'h3FEF);
		expect_val(got, 8'h00, "scroll: indice 31 sin uso = 0");

		// ---- motor de movimiento del scroll: registro 9 (OUT $41 y el dato) e indice 16 (OUT $20) ----
		begin : motor
			integer q, ok, t;
			for (q = 0; q < 65536; q = q + 1) mmem[q] = pat(q);
			io_out(16'h3FEF, 8'h20); io_in(16'h3FEF);
			expect_val(got, 8'h00, "motor: libre al principio (16 = 0)");
			io_out(16'h3FEF, 8'h41); io_out(16'h3FEF, 8'h01);			// registro 9 = 1: izquierda
			io_out(16'h3FEF, 8'h20); io_in(16'h3FEF);
			expect_val(got, 8'hC0, "motor: ocupado y pendiente al instante");
			expect_val(mmem[16'h8000], pat(16'h8000), "motor: aun no se mueve nada");
			io_out(16'h3FEF, 8'h41); io_out(16'h3FEF, 8'h02);			// otra orden con el motor ocupado: se ignora
			expect_val(scr_cmd, 1, "motor: orden con el motor ocupado");
			t = 0;
			io_out(16'h3FEF, 8'h20); io_in(16'h3FEF);
			while (got[6] && t < 40) begin repeat (400) @(posedge sysclk); io_out(16'h3FEF, 8'h20); io_in(16'h3FEF); t = t + 1; end
			expect_val(got[7], 1, "motor: sigue ocupado (en marcha)");
			expect_val(got[6], 0, "motor: ya no esta pendiente");
			expect_val(got[5], 1, "motor: en marcha (16 = $A0)");
			t = 0;
			while (got[7] && t < 100) begin repeat (400) @(posedge sysclk); io_out(16'h3FEF, 8'h20); io_in(16'h3FEF); t = t + 1; end
			expect_val(got, 8'h00, "motor: libre al acabar");
			// izquierda: m_c <- m_{c+1} para c = 0..30, m31 <- R, L <- m0; solo la fila 0 de texto (8 lineas)
			ok = 1;
			for (q = 0; q < 8; q = q + 1) begin : lines
				integer c;
				for (c = 0; c < 31; c = c + 1)
					if (mmem[16'h8000 + q * 32 + c] !== pat(16'h8000 + q * 32 + c + 1)) ok = 0;
				if (mmem[16'h8000 + q * 32 + 31] !== pat(16'h9800 + q)) ok = 0;
				if (mmem[16'h9C00 + q] !== pat(16'h8000 + q * 32)) ok = 0;
			end
			expect_val(ok, 1, "motor: fila 0 un byte a la izquierda");
			expect_val(mmem[16'h8100], pat(16'h8100), "motor: la fila 1 no se toca (mascara)");
			// derecha
			io_out(16'h3FEF, 8'h41); io_out(16'h3FEF, 8'h02);
			io_out(16'h3FEF, 8'h20); io_in(16'h3FEF);
			expect_val(got, 8'hC0, "motor: segunda orden pendiente");
			t = 0;
			while (got[7] && t < 200) begin repeat (400) @(posedge sysclk); io_out(16'h3FEF, 8'h20); io_in(16'h3FEF); t = t + 1; end
			expect_val(got, 8'h00, "motor: libre al acabar la segunda");
			ok = 1;
			for (q = 0; q < 8; q = q + 1) begin : lines2
				integer c;
				for (c = 0; c < 32; c = c + 1)
					if (mmem[16'h8000 + q * 32 + c] !== pat(16'h8000 + q * 32 + c)) ok = 0;
			end
			expect_val(ok, 1, "motor: izquierda y derecha = como estaba");
			// la orden 0 no hace nada y la 3 se acepta y termina sin mover (reservada)
			io_out(16'h3FEF, 8'h41); io_out(16'h3FEF, 8'h00);
			io_out(16'h3FEF, 8'h20); io_in(16'h3FEF);
			expect_val(got, 8'h00, "motor: la orden 0 no hace nada");
			io_out(16'h3FEF, 8'h41); io_out(16'h3FEF, 8'h03);
			io_out(16'h3FEF, 8'h20); io_in(16'h3FEF);
			expect_val(got, 8'hC0, "motor: la orden 3 (reservada) entra");
			t = 0;
			while (got[7] && t < 200) begin repeat (400) @(posedge sysclk); io_out(16'h3FEF, 8'h20); io_in(16'h3FEF); t = t + 1; end
			expect_val(got, 8'h00, "motor: ... y termina");
			ok = 1;
			for (q = 0; q < 8; q = q + 1) if (mmem[16'h8000 + q * 32] !== pat(16'h8000 + q * 32)) ok = 0;
			expect_val(ok, 1, "motor: la orden 3 no mueve nada");
		end

		if (errors == 0) $display("TODO OK");
		else $display("%0d ERRORES", errors);
		$finish;
	end

	// vigilancia de E/S: un IN del puerto $FE rompe en la siguiente
	task dbg_exit_dummy;
	begin
		m1(16'h6450, 8'hDB); mem_rd(16'h6451, 8'hFE);
		io_in(16'hFEFE);
		m1(16'h6452, 8'h00);
		dbg_enter(16'h6452, 3'd6);
		dreg(3'd2, 8'd0);
		dbg_exit(16'h6452);
		m1(16'h6452, 8'h00); expect_mem(8'h00, "vigilancia E/S: vuelta");
	end
	endtask

endmodule
