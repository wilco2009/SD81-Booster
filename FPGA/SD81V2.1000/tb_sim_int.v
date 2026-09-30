`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// tb_sim_int -- banco de pruebas de las interrupciones simuladas (sim_int +
// m1_tracker, conectados como en SD81.v). No forma parte del proyecto de
// ISE: se simula aparte.
//
//   iverilog -o tb tb_sim_int.v sim_int.v && vvp tb
//
// El "Z80" es un guion: hace los ciclos de bus que haria un Z80 real con
// los bytes que recibe (RST 38h -> dos escrituras en la pila y M1 en $0038;
// CALL -> dos lecturas, dos escrituras y M1 en la rutina; etc.) y comprueba
// en cada lectura si el byte lo sirve la FPGA y cual es.
//////////////////////////////////////////////////////////////////////////////////
module tb_sim_int;

	reg clk = 0;					// reloj del Z80, ~3,25 MHz
	always #154 clk = ~clk;
	wire #40 iclock = ~clk;			// la FPGA ve el reloj con retraso (ver tb_m1_tracker)

	reg nreset = 0;
	reg nM1 = 1, nMREQ = 1, nRD = 1, nWR = 1, nIORQ = 1, nRFSH = 1;
	reg [15:0] addr = 16'h0000;
	reg [7:0] mem_data = 8'h00;		// lo que pondria la SRAM
	reg vsync = 0;
	reg superfast = 1;
	reg en_pulse = 0, dis_pulse = 0;

	wire [7:0] fpga_data;
	wire fpga_en;
	wire [7:0] bus = fpga_en ? fpga_data : mem_data;	// lo que ve la CPU
	wire boundary;
	wire [7:0] dbg_out, si_status;
	wire [15:0] si_count;
	wire [1:0] si_state;
	wire si_enabled;
	integer errors = 0;

	m1_tracker trk (
		.iclock(iclock), .nreset(nreset), .nM1(nM1), .nMREQ(nMREQ), .nRFSH(nRFSH),
		.nIORQ(nIORQ), .nWR(nWR), .addr(addr), .data(bus),
		.si_status(si_status), .si_count(si_count),
		.boundary(boundary), .dbg_out(dbg_out));

	sim_int si (
		.iclock(iclock), .nreset(nreset), .enable_int(en_pulse), .disable_int(dis_pulse),
		.superfast_mode(superfast), .vsync(vsync), .boundary(boundary),
		.int_addr(16'h7000), .addr(addr), .nM1(nM1), .nRD(nRD), .nMREQ(nMREQ), .nRFSH(nRFSH),
		.data_out(fpga_data), .enable_out(fpga_en), .state(si_state),
		.enabled(si_enabled), .status(si_status), .count(si_count));

	reg [7:0] got;			// el byte que ha leido la CPU en la ultima lectura
	reg got_fpga;			// si lo servia la FPGA

	// M1 en a con el byte de memoria m; deja en got lo que leyo la CPU
	task m1(input [15:0] a, input [7:0] m);
	begin
		@(posedge clk); #10 nM1 = 0; addr = a;
		@(negedge clk); #10 nMREQ = 0; nRD = 0;
		#60 mem_data = m;
		@(posedge clk);										// T2
		@(posedge clk); got = bus; got_fpga = fpga_en;		// T3: la CPU latchea
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
		@(negedge clk); got = bus; got_fpga = fpga_en;		// la CPU latchea en la bajada de T3
		#10 nMREQ = 1; nRD = 1;
	end
	endtask

	task mem_wr(input [15:0] a);
	begin
		@(posedge clk); #10 addr = a;
		@(negedge clk); #10 nMREQ = 0;
		@(posedge clk);
		@(negedge clk); #10 nWR = 0;
		@(posedge clk);
		@(negedge clk); #10 nMREQ = 1; nWR = 1;
	end
	endtask

	task expect_fpga(input [7:0] want, input [8*28-1:0] what);
	begin
		if (!got_fpga || got !== want) begin
			$display("ERROR %0s: %h (FPGA=%0d), esperaba %h de la FPGA", what, got, got_fpga, want);
			errors = errors + 1;
		end else
			$display("ok    %0s: %h", what, got);
	end
	endtask

	task expect_mem(input [7:0] want, input [8*28-1:0] what);
	begin
		if (got_fpga || got !== want) begin
			$display("ERROR %0s: %h (FPGA=%0d), esperaba %h de la memoria", what, got, got_fpga, want);
			errors = errors + 1;
		end else
			$display("ok    %0s: %h", what, got);
	end
	endtask

	task pulse_vsync;
	begin
		vsync = 1; #3000 vsync = 0;
	end
	endtask

	// Toda la secuencia de una interrupcion, a partir de la M1 en X que ha
	// recibido el RST. La rutina en $7000 es un NOP, una lectura de $0039
	// (como si fuera LD A,($0039): ya no se sirve) y un RET.
	task run_interrupt(input [15:0] x);
	begin
		expect_fpga(8'hFF, "RST 38h inyectado");
		mem_wr(16'h7FFF); mem_wr(16'h7FFE);				// RST: guarda X+1
		m1(16'h0038, 8'hF5);  expect_fpga(8'hCD, "CALL en $0038");
		mem_rd(16'h0039, 8'h00); expect_fpga(8'h00, "CALL lo");
		mem_rd(16'h003A, 8'h00); expect_fpga(8'h70, "CALL hi");
		mem_wr(16'h7FFD); mem_wr(16'h7FFC);				// CALL: guarda $003B
		m1(16'h7000, 8'h00);  expect_mem(8'h00, "rutina: NOP");
		mem_rd(16'h0039, 8'h11); expect_mem(8'h11, "rutina lee $0039: ROM");
		m1(16'h7001, 8'hC9);  expect_mem(8'hC9, "rutina: RET");
		mem_rd(16'h7FFC, 8'h3B); mem_rd(16'h7FFD, 8'h00);
		m1(16'h003B, 8'hFF);  expect_fpga(8'hE3, "epilogo EX (SP),HL");
		mem_rd(16'h7FFE, 8'h01); mem_rd(16'h7FFF, 8'h60);
		mem_wr(16'h7FFF); mem_wr(16'h7FFE);
		m1(16'h003C, 8'hFF);  expect_fpga(8'h2B, "epilogo DEC HL");
		m1(16'h003D, 8'hFF);  expect_fpga(8'hE3, "epilogo EX (SP),HL");
		mem_rd(16'h7FFE, 8'h00); mem_rd(16'h7FFF, 8'h60);
		mem_wr(16'h7FFF); mem_wr(16'h7FFE);
		m1(16'h003E, 8'hFF);  expect_fpga(8'hC9, "epilogo RET");
		mem_rd(16'h7FFE, 8'h00); mem_rd(16'h7FFF, 8'h60);
		m1(x, 8'h00);         expect_mem(8'h00, "vuelta a X, sin tocar");
	end
	endtask

	initial begin
		#1000 nreset = 1;
		#1000 en_pulse = 1; #400 en_pulse = 0;			// POKE 2040,1

		// --- sin VSYNC no pasa nada ---
		m1(16'h6000, 8'h00); expect_mem(8'h00, "sin VSYNC: NOP normal");

		// --- VSYNC con el programa en NOPs: RST en la siguiente M1 ---
		pulse_vsync;
		m1(16'h6000, 8'h00);
		run_interrupt(16'h6000);

		// --- VSYNC en mitad de CB xx: el RST espera a la instruccion siguiente ---
		m1(16'h6001, 8'hCB); expect_mem(8'hCB, "CB (antes del VSYNC)");
		pulse_vsync;
		m1(16'h6002, 8'h00); expect_mem(8'h00, "2o byte de CB: sin tocar");
		m1(16'h6003, 8'h00);
		run_interrupt(16'h6003);

		// --- DD CB d op: d y op son lecturas; la M1 siguiente si es limite ---
		m1(16'h6004, 8'hDD); expect_mem(8'hDD, "DD");
		pulse_vsync;
		m1(16'h6005, 8'hCB); expect_mem(8'hCB, "CB tras DD: sin tocar");
		mem_rd(16'h6006, 8'h02); mem_rd(16'h6007, 8'h06);
		m1(16'h6008, 8'h00);
		run_interrupt(16'h6008);

		// --- sin anidar: un VSYNC durante la rutina espera a que acabe ---
		pulse_vsync;
		m1(16'h6009, 8'h00);
		expect_fpga(8'hFF, "RST 38h inyectado");
		mem_wr(16'h7FFF); mem_wr(16'h7FFE);
		m1(16'h0038, 8'hF5); mem_rd(16'h0039, 0); mem_rd(16'h003A, 0);
		mem_wr(16'h7FFD); mem_wr(16'h7FFC);
		pulse_vsync;										// llega otro VSYNC
		m1(16'h7000, 8'h00); expect_mem(8'h00, "rutina: no se anida");
		m1(16'h7001, 8'hC9); mem_rd(16'h7FFC, 8'h3B); mem_rd(16'h7FFD, 8'h00);
		m1(16'h003B, 8'hFF); mem_rd(16'h7FFE, 1); mem_rd(16'h7FFF, 8'h60); mem_wr(16'h7FFF); mem_wr(16'h7FFE);
		m1(16'h003C, 8'hFF); m1(16'h003D, 8'hFF); mem_rd(16'h7FFE, 0); mem_rd(16'h7FFF, 8'h60); mem_wr(16'h7FFF); mem_wr(16'h7FFE);
		m1(16'h003E, 8'hFF); mem_rd(16'h7FFE, 0); mem_rd(16'h7FFF, 8'h60);
		m1(16'h6009, 8'h00);
		run_interrupt(16'h6009);							// el pendiente, al acabar

		// --- $0038 fuera de una interrupcion: la ROM de siempre ---
		m1(16'h0038, 8'hF5); expect_mem(8'hF5, "$0038 sin interrupcion: ROM");

		// --- fuera de Superfast no se inyecta ---
		superfast = 0;
		pulse_vsync;
		m1(16'h600A, 8'h00); expect_mem(8'h00, "video nativo: sin RST");
		superfast = 1;

		// --- desactivadas no se inyecta ---
		#1000 dis_pulse = 1; #400 dis_pulse = 0;			// POKE 2040,0
		pulse_vsync;
		m1(16'h600B, 8'h00); expect_mem(8'h00, "desactivadas: sin RST");

		if (si_count !== 16'd5) begin
			$display("ERROR interrupciones contadas: %0d, esperaba 5", si_count);
			errors = errors + 1;
		end else
			$display("ok    interrupciones contadas: 5");

		if (errors == 0) $display("TODO OK");
		else $display("%0d ERRORES", errors);
		$finish;
	end

endmodule
