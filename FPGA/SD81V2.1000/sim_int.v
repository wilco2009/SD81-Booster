`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company:
// Engineer:
//
// Create Date:    23:36:41 10/09/2025
// Design Name:
// Module Name:    sim_int
// Project Name:
// Target Devices:
// Tool versions:
// Description:
//
//  Interrupciones simuladas para modo SUPERFAST/SPECTRUM.
//
//  El vector RST 38h del ZX81 real solo tiene sentido en modo de video
//  normal (SLOW/FAST), donde la propia CPU ejecuta la rutina de video
//  temporizada a ciclo exacto. En modo SUPERFAST/SPECTRUM el video lo
//  genera la FPGA (VSYNC_gen) y la CPU nunca necesita pasar por ese
//  vector para pintar nada, asi que queda libre para nuestro uso.
//
//  El ZX81 trabaja en modo de interrupcion IM1: cualquier /INT
//  reconocido (incluido el generado de forma nativa por el propio
//  refresco de memoria -R- realimentado en A6, que sigue funcionando
//  incluso con la CPU parada en HALT) fuerza siempre un fetch real en
//  $0038, con la direccion de retorno correcta ya apilada por el propio
//  hardware. No hace falta simular nada de eso: solo hay que vigilar
//  ese fetch y, si estamos en modo SUPERFAST/SPECTRUM con las
//  interrupciones simuladas activas, sustituir lo que se lee ahi por
//  un "JP int_addr" (3 bytes, sin tocar la pila) en vez del contenido
//  real de la ROM.
//
//  Al ser siempre un fetch M1 en una direccion fija alcanzada solo por
//  salto (real /INT o un RST 38 explicito), es por construccion un
//  limite de instruccion limpio: no hace falta esperar flancos de M1
//  arbitrarios, ni encadenar prefijos DD/FD/CB/ED, ni tratar el HALT
//  como caso especial. Fuera de SUPERFAST/SPECTRUM esta logica no
//  interviene nunca, asi que no anade ciclos ni riesgo al modo de
//  video normal.
//
//  Como la direccion de retorno real ya la pone la CPU en la pila al
//  aceptar la interrupcion, se usa JP (no CALL): no se apila nada
//  extra, y la rutina de usuario en int_addr solo tiene que terminar
//  con EI:RET para reanudar el programa interrumpido.
//
// Dependencies:
//
// Revision:
// Revision 0.02 - Sustituido el enganche por vsync/M1 arbitrario por
//                 enganche fijo en el vector $0038 (solo SUPERFAST/SPECTRUM)
// Revision 0.01 - File Created
// Additional Comments:
//
//////////////////////////////////////////////////////////////////////////////////
module sim_int(
	input wire clk,
	input wire nreset,
	input wire enable_int,
	input wire disable_int,
	input wire superfast_mode,			// sfast_mode_en (cubre texto/HiRes/Spectrum)
	input wire [15:0] int_addr,
	input wire [15:0] addr,
	input wire nM1,
	input wire nRD,
	input wire nMREQ,
	output reg [7:0] data_out,
	output reg enable_out,
	output reg [1:0] state,
	output reg enabled
    );

	localparam [1:0]
		stIDLE 		= 0,		// esperando el fetch M1 en $0038
		stOP_LOW	= 1,		// insertando byte bajo de int_addr
		stOP_HIGH	= 2;		// insertando byte alto de int_addr

	wire m1rd = ~(nMREQ|nRD|nM1);
	wire rd   = ~(nMREQ|nRD);
	reg old_rd = 0;
	wire rdflange = ~rd && old_rd;					// fin del ciclo de lectura actual

	wire hit_vector = enabled && superfast_mode && (addr==16'h0038) && m1rd;

	always @(posedge clk or negedge nreset) begin
		if (~nreset) begin
			state <= stIDLE;
			old_rd <= 0;
			enable_out <= 0;
			data_out <= 0;
			enabled <= 0;
		end else begin
			old_rd <= rd;
			if (enable_int)  enabled <= 1'b1;		// POKE 2040,1 -> activa interrupciones simuladas
			if (disable_int) enabled <= 1'b0;		// POKE 2040,0 -> las desactiva

			case (state)
				stIDLE: begin
					if (hit_vector) begin
						data_out   <= 8'hC3;		// JP nn (no toca la pila)
						enable_out <= 1'b1;
					end
					if (rdflange && enable_out) begin	// espera a que termine este M1 antes de cambiar de byte
						enable_out <= 1'b0;
						state      <= stOP_LOW;
					end
				end
				stOP_LOW: begin
					if (rd) begin
						data_out   <= int_addr[7:0];
						enable_out <= 1'b1;
					end
					if (rdflange) begin
						enable_out <= 1'b0;
						state      <= stOP_HIGH;
					end
				end
				stOP_HIGH: begin
					if (rd) begin
						data_out   <= int_addr[15:8];
						enable_out <= 1'b1;
					end
					if (rdflange) begin
						enable_out <= 1'b0;
						state      <= stIDLE;
					end
				end
			endcase
		end
	end

endmodule
