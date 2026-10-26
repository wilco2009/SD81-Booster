`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// scroll_sync -- la orden del motor de scroll pasa del reloj del Z80 (sim_int) a system_clk
//
//  Tres "toggles": el de la orden (cmd_tog, lo cambia sim_int al aceptar una orden), el de "empezar"
//  (tstart) y el de "acabar" (tdone), que copian cmd_tog. sim_int los vuelve a sincronizar y de ahi
//  salen el pendiente (cmd_tog != tstart), el en marcha (tstart != tdone) y el ocupado
//  (cmd_tog != tdone) del indice 16 del puerto $3FEF. sim_int no acepta una orden mientras hay una
//  en curso, asi que nunca hay dos toggles de la orden sin atender.
//
//  El motor empieza en el flanco de la linea 254 (blit_edge) si hay una orden pendiente y no esta
//  ya en marcha; si no, la orden espera al cuadro siguiente.
//////////////////////////////////////////////////////////////////////////////////
module scroll_sync(
	input wire clk,				// system_clk
	input wire rst,				// activo alto
	input wire cmd_tog,			// de sim_int
	input wire blit_edge,		// pulso: empieza la linea 254
	input wire mot_run,
	input wire mot_done,		// pulso del motor
	output wire mot_go,			// pulso: arrancar el motor con la orden pendiente
	output reg tstart = 1'b0,
	output reg tdone = 1'b0
    );

	reg [1:0] s = 2'b00;
	wire pend = s[1] ^ tstart;
	assign mot_go = pend & blit_edge & ~mot_run;

	always @(posedge clk) begin
		if (rst) begin
			s <= 2'b00;
			tstart <= 1'b0;
			tdone <= 1'b0;
		end else begin
			s <= {s[0], cmd_tog};
			if (mot_go) tstart <= s[1];
			if (mot_done) tdone <= tstart;
		end
	end

endmodule
