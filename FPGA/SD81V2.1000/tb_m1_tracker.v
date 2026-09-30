`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// tb_m1_tracker -- banco de pruebas de m1_tracker (sim_int.v). No forma
// parte del proyecto de ISE: se simula aparte (iverilog o ISim).
//
//   iverilog -o tb tb_m1_tracker.v sim_int.v && vvp tb
//
// Simula ciclos de bus del Z80 con su temporizacion (reloj de 3,25 MHz):
// M1 (T1-T4, con MREQ/RD de T1 a T3 y refresco en T3-T4), OUT e IN al
// puerto $3FEF. Ejecuta la misma secuencia de opcodes que el cuerpo de
// EXAMPLES/SIMINT/m1test.asm y comprueba los contadores: 35 M1, 18
// instrucciones y 2 DD/FD CB.
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
	reg nM1 = 1, nMREQ = 1, nRD = 1, nWR = 1, nIORQ = 1, nRFSH = 1;
	reg [15:0] addr = 16'h0000;
	reg [7:0] data = 8'h00;
	wire boundary;
	wire [7:0] dbg_out;
	integer errors = 0;

	m1_tracker dut (
		.iclock(iclock), .nreset(nreset), .nM1(nM1), .nMREQ(nMREQ), .nRFSH(nRFSH),
		.nIORQ(nIORQ), .nWR(nWR), .addr(addr), .data(data),
		.boundary(boundary), .dbg_out(dbg_out));

	// M1: T1 sube el reloj con la direccion y M1; en la bajada de T1 bajan
	// MREQ y RD; el opcode esta en el bus durante T2; en la subida de T3
	// suben M1, MREQ y RD y empieza el refresco (RFSH, y MREQ otra vez en
	// la bajada de T3)
	task m1(input [7:0] op);
	begin
		@(posedge clk); #10 nM1 = 0; addr = 16'h6000;
		@(negedge clk); #10 nMREQ = 0; nRD = 0;
		#60 data = op;								// la SRAM tarda en poner el opcode
		@(posedge clk);								// T2
		@(posedge clk); #10 nM1 = 1; nMREQ = 1; nRD = 1; nRFSH = 0; addr = 16'h1E00; data = 8'hFF;
		@(negedge clk); #10 nMREQ = 0;
		@(posedge clk);								// T4
		@(negedge clk); #10 nMREQ = 1;
		#10 nRFSH = 1;
	end
	endtask

	// lectura de memoria normal (operandos, d y op de DD CB d op): 3 ciclos
	task mem_rd(input [7:0] v);
	begin
		@(posedge clk); #10 addr = 16'h6001;
		@(negedge clk); #10 nMREQ = 0; nRD = 0; data = v;
		@(posedge clk);
		@(posedge clk);
		@(negedge clk); #10 nMREQ = 1; nRD = 1;
	end
	endtask

	// OUT (C),A al puerto $3FEF: T1, T2 (IORQ y WR bajan en la subida), TW, T3
	task io_out(input [7:0] v);
	begin
		@(posedge clk); #10 addr = 16'h3FEF; data = v;
		@(posedge clk); #10 nIORQ = 0; nWR = 0;
		@(posedge clk);								// TW
		@(posedge clk);								// T3
		@(negedge clk); #10 nIORQ = 1; nWR = 1;
	end
	endtask

	task check(input [3:0] i, input [7:0] want, input [8*24-1:0] what);
	begin
		io_out({4'd0, i});
		#50;
		if (dbg_out !== want) begin
			$display("ERROR %0s: %0d, esperaba %0d", what, dbg_out, want);
			errors = errors + 1;
		end else
			$display("ok    %0s: %0d", what, dbg_out);
	end
	endtask

	initial begin
		#1000 nreset = 1;

		check(15, 8'h51, "firma");

		io_out(8'h80);				// borrar
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
		io_out(8'h40);				// congelar

		check(0, 8'd35, "M1 (bajo)");
		check(1, 8'd0,  "M1 (alto)");
		check(2, 8'd18, "instrucciones (bajo)");
		check(3, 8'd0,  "instrucciones (alto)");
		check(4, 8'd2,  "DD/FD CB");
		check(6, 8'h44, "ultimo opcode");
		check(7, 8'd0,  "estado");

		// estado a mitad de instruccion: tras un CB no es limite
		m1(8'hCB);
		if (boundary !== 1'b0) begin $display("ERROR limite tras CB"); errors = errors + 1; end
		else $display("ok    no es limite tras CB");
		m1(8'h00);
		if (boundary !== 1'b1) begin $display("ERROR limite tras CB 00"); errors = errors + 1; end
		else $display("ok    limite tras CB 00");

		if (errors == 0) $display("TODO OK");
		else $display("%0d ERRORES", errors);
		$finish;
	end

endmodule
