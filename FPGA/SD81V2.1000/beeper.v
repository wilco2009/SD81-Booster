`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company:
// Engineer:
//
// Create Date:    13:32:40 10/03/2025
// Design Name:
// Module Name:    beeper
// Project Name:
// Target Devices:
// Tool versions:
// Description:
//
// Dependencies:
//
// Revision:
// Revision 0.01 - File Created
// Additional Comments:
//
//////////////////////////////////////////////////////////////////////////////////
module beeper(
	input clk,
	input nreset,
	input wire cs,
	input wire [1:0] data_in,
	output wire signed [12:0] data_out
    );

	// Niveles del beeper (data_in = {EAR,MIC} = data[4:3]).
	// Manic Miner toca el Danubio con OUT(254),A + XOR 24 (=EAR+MIC juntos):
	// los dos bits conmutan SIEMPRE a la vez, asi que cada nota oscila o bien
	// 00<->11 o bien 01<->10. Para que ambos tipos suenen igual de fuerte
	// hacen falta |t3-t0| = |t2-t1| = 4095, lo que equivale a que mande solo
	// el EAR e ignorar el MIC (igual que el emulador: (Data>>4)&1). El efecto
	// a 2 voces ya esta en el propio flujo de bits del EAR.
	reg signed [12:0] beeper_table [0:3];
	initial begin
		beeper_table[0] = 0;    // 00  EAR=0
		beeper_table[1] = 300;    // 01  EAR=0 (MIC ignorado)
		beeper_table[2] = 3795; // 10  EAR=1
		beeper_table[3] = 4095; // 11  EAR=1 (MIC ignorado)
	end

	// Base con decay: salida registrada simple (outreg), un unico cambio por
	// flanco -> sin glitches hacia el i2s (a diferencia del pasa-altos
	// combinacional, que metia "laser" en cada transicion de nota).
	// El decay lento (1 de cada 256) solo bloquea la DC, fuera de banda de audio.
	parameter delta=2;

	reg signed [12:0] outreg;
	reg [1:0] old_linear;
	reg [1:0] linear;
	reg [7:0] decay_cnt = 0;
	always@(posedge clk) begin
	 if (~nreset) outreg <= 0;
	 old_linear <= linear;
	 if (cs) linear <= data_in;  // o {data_in[1], data_in[0]} o lo que corresponda
	 if (linear != old_linear) outreg <= beeper_table[linear];
	 else begin
		decay_cnt <= decay_cnt + 1'b1;
		if (decay_cnt == 8'd0) begin
			// si queda menos que delta, ir a 0 exacto: con niveles que no son
			// multiplo de delta el decay nunca llegaba a 0 y quedaba oscilando
			// +1/-1 -> pitido continuo al parar la musica
			if (outreg > delta) outreg <= outreg - delta;
			else if (outreg < -delta) outreg <= outreg + delta;
			else outreg <= 0;
		end
	 end
	end

	assign data_out = outreg;

endmodule
