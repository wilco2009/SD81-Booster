// ============================================================================
// vkeys.v -- teclado virtual: las teclas que el MCU "pulsa" desde fuera
// ============================================================================
// Una matriz de 8 filas x 5 columnas, la misma del ZX81: la fila i se elige
// con la linea de direccion A(8+i) a 0 al leer el puerto $FE, y las 5 columnas
// salen por D0-D4 (a 0 = pulsada). Esta matriz se SUMA a las teclas del
// joystick (SD81.v: kbd_data): una tecla virtual pulsada pone su columna a 0
// en esa fila, igual que si estuviera apretada de verdad.
//
// El MCU la escribe fila a fila por el canal de configuracion (orden 13):
//     cfg_reg[CMD_BITS+2 : CMD_BITS]      la fila (0-7)
//     cfg_reg[CMD_BITS+7 : CMD_BITS+3]    sus 5 columnas, 1 = pulsada
// y la FPGA lo toma cuando termina la orden (al bajar CFG_RESET, como las
// demas). Al arrancar no hay ninguna tecla pulsada.
// ============================================================================
module vkeys #(
	parameter CMD = 4'd13
) (
	input  wire        cfg_rst_n,		// CFG_RESET: la orden se toma al bajar
	input  wire [3:0]  cmd,				// la orden recibida (cfg_reg[CMD_BITS-1:0])
	input  wire [7:0]  data,			// su dato (cfg_reg[CMD_BITS+7:CMD_BITS])
	input  wire [7:0]  a_hi,			// A15..A8 del bus del Z80 (a_hi[i] = A(8+i))
	output wire [4:0]  pressed			// columnas a 0 en el puerto $FE por las teclas virtuales (1 = pulsada)
);
	reg [4:0] k0 = 5'd0, k1 = 5'd0, k2 = 5'd0, k3 = 5'd0, k4 = 5'd0, k5 = 5'd0, k6 = 5'd0, k7 = 5'd0;

	// SD81.v toma las ordenes en el flanco de bajada de CFG_RESET (always @(posedge CFG_CLK or
	// negedge CFG_RESET): la rama de CFG_RESET==0); aqui igual
	always @(negedge cfg_rst_n) begin
		if (cmd == CMD) begin
			case (data[2:0])
				3'd0: k0 <= data[7:3];
				3'd1: k1 <= data[7:3];
				3'd2: k2 <= data[7:3];
				3'd3: k3 <= data[7:3];
				3'd4: k4 <= data[7:3];
				3'd5: k5 <= data[7:3];
				3'd6: k6 <= data[7:3];
				default: k7 <= data[7:3];
			endcase
		end
	end

	assign pressed = ({5{~a_hi[0]}} & k0) | ({5{~a_hi[1]}} & k1) | ({5{~a_hi[2]}} & k2) | ({5{~a_hi[3]}} & k3) |
	                 ({5{~a_hi[4]}} & k4) | ({5{~a_hi[5]}} & k5) | ({5{~a_hi[6]}} & k6) | ({5{~a_hi[7]}} & k7);
endmodule
