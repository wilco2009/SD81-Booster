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


//////////////////////////////////////////////////////////////////////////////////
// m1_tracker -- detector de limites de instruccion (paso 1 de las
// interrupciones simuladas por inyeccion).
//
//  Para inyectar un RST 38h sin romper nada hay que hacerlo en la M1 que
//  EMPIEZA una instruccion, nunca en la segunda M1 de una con prefijo:
//
//    CB xx, ED xx       dos M1: la segunda no es limite
//    DD xx, FD xx       dos M1: la segunda no es limite
//    DD CB d op         dos M1 (DD y CB); d y op son lecturas normales, asi
//                       que la M1 siguiente ya es otra instruccion
//    DD DD ... , FD DD  cada prefijo repetido sigue sin ser limite (el Z80
//                       tampoco acepta interrupciones entre ellos)
//    DD ED xx           el DD se ignora y queda un ED xx
//    LDIR y compañia    vuelven a leer ED xx en cada vuelta: la M1 del ED
//                       es limite, como en un Z80 real
//
//  Cada M1 se procesa UNA sola vez, AL TERMINAR: en cada subida de iclock
//  (bajada del reloj del Z80) con MREQ y M1 bajos y RFSH alto se guarda el
//  dato del bus, y cuando esa condicion deja de cumplirse (la bajada de
//  T3) se procesa el ultimo guardado, que es el de T2. No vale procesar en
//  cada muestra: en la placa la FPGA ve el reloj con retraso y en la
//  bajada de T1 MREQ ya esta bajo, asi que la condicion se cumple dos
//  veces, la primera con el opcode todavia sin llegar (comprobado en
//  hardware: salian el doble de M1 y las instrucciones sin sentido). Las
//  M1 de reconocimiento de interrupcion (con IORQ y sin MREQ) no cuentan.
//
//  Puerto de depuracion $3FEF (direccion completa de 16 bits, como el de
//  Chroma en $7FEF):
//    OUT 80h   borra los contadores
//    OUT 40h   los congela en una copia (la que se lee), para que no
//              cambien mientras se leen
//    OUT n     (0-15) elige que devuelve IN:
//                0/1  M1 (bajo/alto)
//                2/3  instrucciones (M1 que empiezan instruccion)
//                4/5  instrucciones DD CB / FD CB
//                6    ultimo opcode leido en una M1
//                7    estado (0 normal, 1 segundo byte de CB/ED, 2 tras DD/FD)
//                15   firma 51h: el detector esta presente
//////////////////////////////////////////////////////////////////////////////////
module m1_tracker(
	input wire iclock,			// nCLOCK: su subida es la BAJADA del reloj del Z80
	input wire nreset,
	input wire nM1,
	input wire nMREQ,
	input wire nRFSH,
	input wire nIORQ,
	input wire nWR,
	input wire [15:0] addr,
	input wire [7:0] data,
	output wire boundary,		// 1: la proxima M1 empieza una instruccion
	output reg [7:0] dbg_out	// lo que devuelve IN del puerto $3FEF
    );

	localparam [1:0]
		stNORMAL = 2'd0,		// la proxima M1 empieza una instruccion
		stSECOND = 2'd1,		// la proxima M1 es el segundo byte de CB xx / ED xx
		stINDEX  = 2'd2;		// la proxima M1 va detras de un DD/FD

	reg [1:0] state = stNORMAL;
	reg [15:0] m1_cnt = 16'd0;
	reg [15:0] insn_cnt = 16'd0;
	reg [15:0] idxcb_cnt = 16'd0;
	reg [7:0] last_op = 8'd0;
	// copia congelada con OUT 40h: es lo que se lee
	reg [15:0] s_m1 = 16'd0;
	reg [15:0] s_insn = 16'd0;
	reg [15:0] s_idxcb = 16'd0;
	reg [7:0] s_last = 8'd0;
	reg [1:0] s_state = 2'd0;
	reg [3:0] idx = 4'd0;
	reg m1_prev = 1'b0;			// la condicion de M1 en la muestra anterior
	reg [7:0] op_latch = 8'd0;	// el dato de la ultima muestra de la M1

	assign boundary = (state == stNORMAL);

	wire m1_sample = ~nMREQ & ~nM1 & nRFSH;
	// OUT al puerto: IORQ y WR bajos en los flancos de bajada de T2 y TW
	// (y quiza T3). Borrar, congelar y elegir son idempotentes, y durante
	// un ciclo de E/S no hay M1, asi que da igual cuantas veces se vea.
	wire port_wr = ~nIORQ & ~nWR & (addr == 16'h3FEF);

	wire m1_end = m1_prev & ~m1_sample;	// la M1 acaba de terminar

	always @(posedge iclock or negedge nreset) begin
		if (~nreset) begin
			m1_prev <= 1'b0;
			op_latch <= 8'd0;
		end else begin
			m1_prev <= m1_sample;
			if (m1_sample)
				op_latch <= data;		// se queda con la ultima muestra (T2)
		end
	end

	always @(posedge iclock or negedge nreset) begin
		if (~nreset) begin
			state <= stNORMAL;
			m1_cnt <= 16'd0;
			insn_cnt <= 16'd0;
			idxcb_cnt <= 16'd0;
			last_op <= 8'd0;
			idx <= 4'd0;
		end else if (port_wr) begin
			if (data[6]) begin
				s_m1 <= m1_cnt;
				s_insn <= insn_cnt;
				s_idxcb <= idxcb_cnt;
				s_last <= last_op;
				s_state <= state;
			end
			if (data[7]) begin
				m1_cnt <= 16'd0;
				insn_cnt <= 16'd0;
				idxcb_cnt <= 16'd0;
			end
			if (~data[7] & ~data[6])
				idx <= data[3:0];
		end else if (m1_end) begin		// una vez por M1, con el opcode de T2
			m1_cnt <= m1_cnt + 1'b1;
			last_op <= op_latch;
			case (state)
				stNORMAL: begin
					insn_cnt <= insn_cnt + 1'b1;
					if (op_latch == 8'hCB || op_latch == 8'hED)
						state <= stSECOND;
					else if (op_latch == 8'hDD || op_latch == 8'hFD)
						state <= stINDEX;
				end
				stSECOND:
					state <= stNORMAL;
				stINDEX: begin
					if (op_latch == 8'hCB) begin		// DD CB d op: d y op no son M1
						idxcb_cnt <= idxcb_cnt + 1'b1;
						state <= stNORMAL;
					end else if (op_latch == 8'hED)		// DD ED xx: queda un ED xx
						state <= stSECOND;
					else if (op_latch == 8'hDD || op_latch == 8'hFD)
						state <= stINDEX;			// otro prefijo: sigue sin ser limite
					else
						state <= stNORMAL;
				end
				default:
					state <= stNORMAL;
			endcase
		end
	end

	always @(*) begin
		case (idx)
			4'd0:  dbg_out = s_m1[7:0];
			4'd1:  dbg_out = s_m1[15:8];
			4'd2:  dbg_out = s_insn[7:0];
			4'd3:  dbg_out = s_insn[15:8];
			4'd4:  dbg_out = s_idxcb[7:0];
			4'd5:  dbg_out = s_idxcb[15:8];
			4'd6:  dbg_out = s_last;
			4'd7:  dbg_out = {6'd0, s_state};
			4'd15: dbg_out = 8'h51;
			default: dbg_out = 8'h00;
		endcase
	end

endmodule
