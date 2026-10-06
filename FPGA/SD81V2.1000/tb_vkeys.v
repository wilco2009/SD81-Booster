// tb_vkeys.v -- el teclado virtual (vkeys.v) en ModelSim
//   vlog vkeys.v tb_vkeys.v ; vsim -c tb_vkeys -do "run -all; quit"
`timescale 1ns/1ps
module tb_vkeys;
	reg        rst_n = 1;
	reg [3:0]  cmd = 0;
	reg [7:0]  data = 0;
	reg [7:0]  a_hi = 8'hFF;				// ninguna fila elegida (A8-A15 a 1)
	wire [4:0] pressed;
	integer errors = 0;

	vkeys dut (.cfg_rst_n(rst_n), .cmd(cmd), .data(data), .a_hi(a_hi), .pressed(pressed));

	// una orden del MCU: se prepara el dato y CFG_RESET baja
	task send(input [3:0] c, input [2:0] row, input [4:0] cols);
	begin
		cmd = c; data = {cols, row};
		#10 rst_n = 0; #10 rst_n = 1; #10;
	end
	endtask

	task expect(input [4:0] want, input [8*48-1:0] what);
	begin
		#1;
		if (pressed !== want) begin
			$display("ERROR %0s: %b, esperaba %b", what, pressed, want);
			errors = errors + 1;
		end else
			$display("ok    %0s: %b", what, pressed);
	end
	endtask

	initial begin
		a_hi = 8'hFE; expect(5'b00000, "sin pulsar nada");
		send(4'd13, 3'd0, 5'b00001);					// fila 0 (A8): SHIFT
		a_hi = 8'hFE; expect(5'b00001, "fila 0 elegida: SHIFT");
		a_hi = 8'hFD; expect(5'b00000, "otra fila: nada");
		a_hi = 8'hFF; expect(5'b00000, "ninguna fila elegida: nada");
		send(4'd13, 3'd7, 5'b10001);					// fila 7 (A15): SPACE y B
		a_hi = 8'h7F; expect(5'b10001, "fila 7: SPACE y B");
		a_hi = 8'hFE; expect(5'b00001, "la fila 0 sigue");
		a_hi = 8'h7E; expect(5'b10001, "dos filas a la vez (A8 y A15 a 0): las dos teclas suman");
		send(4'd13, 3'd3, 5'b00110);					// fila 3 (A11): 2 y 3
		a_hi = 8'hF7; expect(5'b00110, "fila 3: 2 y 3");
		send(4'd13, 3'd0, 5'b00000);					// soltar SHIFT
		a_hi = 8'hFE; expect(5'b00000, "SHIFT suelto");
		a_hi = 8'hF7; expect(5'b00110, "la fila 3 no cambia al soltar otra");
		send(4'd12, 3'd3, 5'b11111);					// otra orden (12): no toca el teclado
		a_hi = 8'hF7; expect(5'b00110, "una orden distinta no escribe");
		send(4'd13, 3'd3, 5'b00000);
		send(4'd13, 3'd7, 5'b00000);
		a_hi = 8'h00; expect(5'b00000, "todo suelto");
		if (errors == 0) $display("TODO OK"); else $display("%0d ERRORES", errors);
		$finish;
	end
endmodule
