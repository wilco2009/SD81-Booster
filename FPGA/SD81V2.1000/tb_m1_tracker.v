`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// tb_m1_tracker -- banco de pruebas de m1_tracker (sim_int.v). No forma
// parte del proyecto de ISE: se simula aparte (ModelSim, ISim o iverilog).
//
//   iverilog -o tb tb_m1_tracker.v sim_int.v && vvp tb
//
// Simula ciclos de bus del Z80 con su temporizacion (reloj de 3,25 MHz):
// M1 (T1-T4, con MREQ/RD de T1 a T3 y refresco en T3-T4) y lecturas de
// memoria. Ejecuta la misma secuencia de opcodes que el cuerpo de
// EXAMPLES/SIMINT/m1test.asm (que ya no existe: la FPGA no tiene contadores)
// y cuenta en el propio banco las M1 que empiezan instruccion, mirando
// boundary al empezar cada una: tienen que ser 18. Comprueba tambien
// index_prefix.
//////////////////////////////////////////////////////////////////////////////////
module tb_m1_tracker;

	reg clk = 0;				// reloj del Z80
	always #154 clk = ~clk;		// ~3,25 MHz
	// En la placa iclock = nCLOCK sube cuando baja el reloj del Z80, pero la
	// FPGA lo ve con retraso: en el flanco de bajada de T1 MREQ ya ha bajado
	// y la condicion de M1 se cumple DOS veces (T1 con el dato aun sin
	// llegar, y T2). Se simula con 40 ns de retraso: sin eso el banco de
	// pruebas daba bien lo que en el hardware salia doble.
	wire #40 iclock = ~clk;

	reg nreset = 0;
	reg nM1 = 1, nMREQ = 1, nRFSH = 1;
	reg [7:0] data = 8'h00;
	wire boundary, index_prefix;
	integer errors = 0;
	integer insns = 0;			// M1 que empiezan instruccion
	integer m1s = 0;

	m1_tracker dut (
		.iclock(iclock), .nreset(nreset), .nM1(nM1), .nMREQ(nMREQ), .nRFSH(nRFSH),
		.data(data), .boundary(boundary), .index_prefix(index_prefix));

	// M1: T1 sube el reloj con M1; en la bajada de T1 bajan MREQ y RD; el
	// opcode esta en el bus durante T2; en la subida de T3 suben M1, MREQ y
	// RD y empieza el refresco (RFSH, y MREQ otra vez en la bajada de T3)
	task m1(input [7:0] op);
	begin
		@(posedge clk); #10 nM1 = 0;
		m1s = m1s + 1;
		if (boundary) insns = insns + 1;
		@(negedge clk); #10 nMREQ = 0;
		#60 data = op;								// la SRAM tarda en poner el opcode
		@(posedge clk);								// T2
		@(posedge clk); #10 nM1 = 1; nMREQ = 1; nRFSH = 0; data = 8'hFF;
		@(negedge clk); #10 nMREQ = 0;
		@(posedge clk);								// T4
		@(negedge clk); #10 nMREQ = 1;
		#10 nRFSH = 1;
	end
	endtask

	// lectura de memoria normal (operandos, d y op de DD CB d op): 3 ciclos
	task mem_rd(input [7:0] v);
	begin
		@(posedge clk);
		@(negedge clk); #10 nMREQ = 0; data = v;
		@(posedge clk);
		@(posedge clk);
		@(negedge clk); #10 nMREQ = 1;
	end
	endtask

	task check(input integer got, input integer want, input [8*24-1:0] what);
	begin
		if (got !== want) begin
			$display("ERROR %0s: %0d, esperaba %0d", what, got, want);
			errors = errors + 1;
		end else
			$display("ok    %0s: %0d", what, got);
	end
	endtask

	initial begin
		#1000 nreset = 1;

		m1s = 0; insns = 0;
		// el cuerpo de m1test.asm
		m1(8'h00);									// NOP
		m1(8'h3E); mem_rd(8'h05);					// LD A,5
		m1(8'hCB); m1(8'h00);						// RLC B
		m1(8'hED); m1(8'h44);						// NEG
		m1(8'h21); mem_rd(0); mem_rd(0);			// LD HL,nn
		m1(8'h11); mem_rd(0); mem_rd(0);			// LD DE,nn
		m1(8'h01); mem_rd(4); mem_rd(0);			// LD BC,4
		repeat (4) begin m1(8'hED); m1(8'hB0); end	// LDIR, 4 vueltas
		m1(8'hDD); m1(8'h21); mem_rd(0); mem_rd(0);	// LD IX,nn
		m1(8'hDD); m1(8'h7E); mem_rd(1);			// LD A,(IX+1)
		m1(8'hDD); m1(8'hCB); mem_rd(2); mem_rd(8'h06);	// RLC (IX+2)
		m1(8'hFD); m1(8'hCB); mem_rd(1); mem_rd(8'h46);	// BIT 0,(IY+1)
		m1(8'hDD); m1(8'hDD); m1(8'hDD); m1(8'h7E); mem_rd(0);	// DD DD LD A,(IX+0)
		m1(8'hFD); m1(8'hDD); m1(8'h21); mem_rd(0); mem_rd(0);	// FD DD LD IX,nn
		m1(8'hDD); m1(8'hED); m1(8'h44);			// DD NEG

		check(m1s, 35, "M1");
		check(insns, 18, "instrucciones");

		// estado a mitad de instruccion
		m1(8'hCB);
		check(boundary, 0, "no es limite tras CB");
		m1(8'h00);
		check(boundary, 1, "limite tras CB 00");
		m1(8'hDD);
		check(index_prefix, 1, "detras de DD");
		m1(8'hFD);
		check(index_prefix, 1, "detras de DD FD");
		m1(8'hED);
		check(boundary + 2*index_prefix, 0, "DD FD ED: segundo byte");
		m1(8'h44);
		check(boundary, 1, "limite tras DD FD ED 44");

		if (errors == 0) $display("TODO OK");
		else $display("%0d ERRORES", errors);
		$finish;
	end

endmodule
