// ============================================================================
// mouse_regs.v -- el raton Bluetooth: lo ultimo que le ha dicho el MCU
// ============================================================================
// El ESP32 lee el raton por BLE, el STM32 integra sus movimientos y los empuja aqui por el canal de
// configuracion (como vkeys.v). El Z80 lo lee sin esperar con el indice 20 del puerto $3FEF (sim_int.v:
// OUT $24 y despues IN, IN, ... uno por byte). Dos ordenes, cada una con hasta 29 bits de dato:
//
//   orden 14 (A):  dato[7:0] x   dato[15:8] y   dato[18:16] botones (bit0 izq, bit1 der, bit2 central)   dato[26:19] rueda
//   orden 15 (B):  dato[7:0] rawx   dato[15:8] rawy   dato[23:16] seq   dato[24] conectado
//
// y los 8 bytes que ve el Z80, en este orden:
//   0 x         posicion absoluta 0..255 (cursor)
//   1 y         posicion absoluta (0..191 por defecto)
//   2 botones   bit0 izquierdo, bit1 derecho, bit2 central
//   3 rueda     contador que da la vuelta (8 bits): el Z80 resta la lectura anterior
//   4 rawx      contador de movimiento en X, que da la vuelta; resta = movimiento relativo (juegos)
//   5 rawy      lo mismo en Y
//   6 seq       cuenta los paquetes que ha mandado el MCU: si no cambia, no hay nada nuevo
//   7 estado    bit0: el raton esta conectado
// Las dos ordenes son independientes: x, y y botones pueden estar un paquete por delante de rawx/rawy.
//
// El MCU las toma, como el resto, cuando CFG_RESET baja. Al arrancar todo a 0.
// ============================================================================
module mouse_regs #(
	parameter CMD_A = 4'd14,
	parameter CMD_B = 4'd15
) (
	input  wire        cfg_rst_n,		// CFG_RESET: la orden se toma al bajar
	input  wire [3:0]  cmd,				// la orden recibida (cfg_reg[CMD_BITS-1:0])
	input  wire [28:0] data,			// su dato (cfg_reg[CMD_BITS+28:CMD_BITS])
	output wire [63:0] regs				// los 8 bytes, el 0 en los bits 7:0
);
	reg [7:0] r_x = 8'd0, r_y = 8'd0, r_btn = 8'd0, r_wheel = 8'd0;
	reg [7:0] r_rawx = 8'd0, r_rawy = 8'd0, r_seq = 8'd0, r_stat = 8'd0;

	always @(negedge cfg_rst_n) begin
		if (cmd == CMD_A) begin
			r_x     <= data[7:0];
			r_y     <= data[15:8];
			r_btn   <= {5'd0, data[18:16]};
			r_wheel <= data[26:19];
		end else if (cmd == CMD_B) begin
			r_rawx <= data[7:0];
			r_rawy <= data[15:8];
			r_seq  <= data[23:16];
			r_stat <= {7'd0, data[24]};
		end
	end

	assign regs = {r_stat, r_seq, r_rawy, r_rawx, r_wheel, r_btn, r_y, r_x};
endmodule
