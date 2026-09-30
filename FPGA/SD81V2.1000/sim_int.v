`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// sim_int -- interrupciones simuladas a 50 Hz (modos Superfast, con DI)
//
//  La FPGA no llega a /INT (en el ZX81 esta cableada a A6) ni a /NMI, asi
//  que la interrupcion se hace INYECTANDO instrucciones en el bus:
//
//   1. Con cada VSYNC queda una interrupcion pendiente.
//   2. En la primera M1 que empiece instruccion (boundary, de m1_tracker)
//      se sirve FF, RST 38h, en vez del opcode. La CPU guarda X+1 (ya ha
//      leido el opcode en X) y salta a $0038.
//   3. En $0038-$003A se sirve CALL int_addr (CD lo hi): la CPU guarda
//      $003B y salta a la rutina del usuario, que acaba con un RET normal.
//   4. Al volver, en $003B-$003E se sirve el epilogo
//         EX (SP),HL / DEC HL / EX (SP),HL / RET
//      que corrige X+1 -> X y vuelve a la instruccion interrumpida. La M1
//      en $003B marca el final de la rutina: hasta entonces no se inyecta
//      otra (un RST no respeta DI, asi que no hay otra proteccion).
//
//  HALT: con DI un HALT de verdad no acabaria nunca, asi que con las
//  interrupciones activas un 76 que empiece instruccion (o detras de
//  DD/FD: DD 76 tambien es HALT) se sirve como FF. La CPU guarda H+1, que
//  ya es la instruccion de detras; en $0038 se sirve JR $ (18 FE) hasta que
//  haya VSYNC pendiente, y entonces el CALL. Al volver, en $003B se sirve
//  directamente RET: sin el DEC HL. Si la interrupcion ya estaba pendiente
//  al llegar al HALT, el CALL va sin esperar, como en un Z80 con EI.
//
//  Todo se sirve por DIRECCION y solo en su fase: entre medias hay
//  lecturas y escrituras reales de la pila (las del RST, el CALL y los EX
//  (SP),HL) que no se tocan. Fuera de una interrupcion en curso, $0038 se
//  lee de la ROM como siempre. La rutina no puede tener HALT (con DI, en un
//  Z80 tampoco acabaria).
//
//  Solo con sfast_mode_en: en video nativo la INT, la NMI y los HALT son
//  parte de la generacion de la imagen. Y el programa tiene que tener DI:
//  con EI las interrupciones reales de A6 inundarian $0038.
//
//  El FF se decide en la subida de T2, con el opcode de la SRAM ya en el
//  bus (hace falta para ver los HALT), y desde ahi lo sirve la FPGA; la
//  CPU lo lee en la subida de T3. El resto de lo que se sirve depende de
//  registros que no cambian durante las lecturas.
//
//  POKE 2038/2039: direccion de la rutina (int_addr)
//  POKE 2040,1/0:  activa / desactiva
//
// Revision 0.05 - HALT (paso 3): un 76 se sirve como FF y en $0038 JR $
//                 hasta el VSYNC. El FF se decide en T2 con el opcode real
// Revision 0.04 - Todo el estado en el dominio del reloj del Z80 (iclock):
//                 a 26 MHz las lecturas asincronas podian perder una
//                 transicion y colgar el Z80 en el hardware
// Revision 0.03 - Inyeccion de RST 38h en limites de instruccion (paso 2)
// Revision 0.02 - JP int_addr en el vector $0038 (las interrupciones venian
//                 de A6 y no servian para nada)
// Revision 0.01 - File Created
//////////////////////////////////////////////////////////////////////////////////
module sim_int(
	input wire iclock,					// nCLOCK, como m1_tracker
	input wire nreset,
	input wire enable_int,
	input wire disable_int,
	input wire superfast_mode,			// sfast_mode_en (texto/HiRes/Spectrum)
	input wire vsync,
	input wire boundary,				// m1_tracker: la proxima M1 empieza instruccion
	input wire index_prefix,			// m1_tracker: la proxima M1 va detras de DD/FD
	input wire [15:0] int_addr,
	input wire [15:0] addr,
	input wire [7:0] data,				// el bus (para ver el opcode real)
	input wire nM1,
	input wire nRD,
	input wire nMREQ,
	input wire nRFSH,
	output reg [7:0] data_out,
	output reg enable_out,				// servir data_out en esta lectura
	output wire [1:0] state,
	output reg enabled,
	output wire [7:0] status,			// para el puerto de depuracion
	output reg [15:0] count				// interrupciones inyectadas
    );

	localparam [1:0]
		phIDLE  = 2'd0,		// sin interrupcion en curso
		phENTRY = 2'd1,		// RST inyectado: $0038-$003A (CALL, o JR $ si es un HALT)
		phISR   = 2'd2,		// rutina del usuario: esperar la M1 en $003B
		phEPI   = 2'd3;		// epilogo en $003C-$003E

	reg [1:0] phase = phIDLE;
	reg pending = 1'b0;			// VSYNC sin atender
	reg arm = 1'b0;				// hay que interrumpir en la proxima M1 que sea limite
	reg [1:0] vs_sync = 2'b00;	// VSYNC viene del dominio de system_clk
	reg vs_prev = 1'b0;
	reg m1_prev = 1'b0;
	reg [15:0] m1_addr = 16'd0;	// direccion de la ultima M1
	// Decision en la subida de T2 (bajada de iclock), con el opcode real
	reg m1_seen = 1'b0;			// ya se ha decidido en esta M1
	reg inj = 1'b0;				// esta M1 recibe el FF
	reg inj_halt = 1'b0;		// ... y lo que habia era un HALT
	reg halted = 1'b0;			// la interrupcion en curso viene de un HALT
	reg call_now = 1'b0;		// en $0038: CALL (1) o JR $ (0, esperando el VSYNC)
	reg took_call = 1'b0;		// lo que se sirvio en la ultima M1 en $0038

	assign state = phase;
	assign status = {enabled, pending, arm, superfast_mode, halted, call_now, phase};

	wire rd = ~(nMREQ | nRD);
	wire m1rd = rd & ~nM1;
	// Las M1 se siguen igual que en m1_tracker: muestras en la subida de
	// iclock (bajada del reloj del Z80) y el final se ve en la bajada de T3,
	// con el bus ya en el refresco. Todo el estado vive en este dominio: con
	// system_clk, /MREQ y /RD llegaban asincronos y un flanco que cayera
	// justo en el reloj podia verse distinto en unos biestables que en
	// otros, perderse el final de una lectura y quedarse la fase atascada
	// (y el RET de la rutina iba a la ROM de $003B).
	wire m1_sample = ~nMREQ & ~nM1 & nRFSH;
	wire m1_end = m1_prev & ~m1_sample;
	// la interrupcion se entrega: acaba la M1 del CALL en $0038
	wire call_taken = m1_end && (phase == phENTRY) && (m1_addr == 16'h0038) && call_now;

	// Un HALT que se puede cambiar por el FF: al empezar instruccion o
	// detras de DD/FD (DD 76 tambien es HALT; CB 76, ED 76 y DD CB d 76 no)
	wire halt_ok = enabled & superfast_mode & (phase == phIDLE) &
	               (boundary | index_prefix) & (data == 8'h76);

	// Lo que se sirve en ESTA lectura: combinacional, estable mientras dure
	always @(*) begin
		enable_out = 1'b0;
		data_out = 8'h00;
		if (m1rd && inj) begin
			enable_out = 1'b1;
			data_out = 8'hFF;						// RST 38h
		end else if (phase == phENTRY && rd) begin
			if (addr == 16'h0038 && ~nM1) begin
				enable_out = 1'b1;
				data_out = call_now ? 8'hCD : 8'h18;	// CALL nn / JR $
			end else if (addr == 16'h0039 && nM1) begin
				enable_out = 1'b1;
				data_out = took_call ? int_addr[7:0] : 8'hFE;
			end else if (addr == 16'h003A && nM1 && took_call) begin
				enable_out = 1'b1;
				data_out = int_addr[15:8];
			end
		end else if (phase == phISR && m1rd && addr == 16'h003B) begin
			enable_out = 1'b1;
			data_out = halted ? 8'hC9 : 8'hE3;		// RET a X+1 / EX (SP),HL
		end else if (phase == phEPI && m1rd) begin
			if (addr == 16'h003C) begin
				enable_out = 1'b1;
				data_out = 8'h2B;					// DEC HL
			end else if (addr == 16'h003D) begin
				enable_out = 1'b1;
				data_out = 8'hE3;					// EX (SP),HL
			end else if (addr == 16'h003E) begin
				enable_out = 1'b1;
				data_out = 8'hC9;					// RET
			end
		end
	end

	// La inyeccion se decide en la primera muestra de la M1 en la subida del
	// reloj del Z80, que es la de T2: MREQ bajo desde la bajada de T1 y el
	// dato de la SRAM ya valido. Asi se ve el opcode real (con el FF puesto
	// ya no se veria), y la CPU no lo lee hasta la subida de T3: queda medio
	// ciclo para que la FPGA tome el bus.
	always @(negedge iclock or negedge nreset) begin
		if (~nreset) begin
			m1_seen <= 1'b0;
			inj <= 1'b0;
			inj_halt <= 1'b0;
		end else if (~m1_sample) begin
			m1_seen <= 1'b0;
			if (~m1_prev) begin					// ya procesado el final de la M1
				inj <= 1'b0;					// (en la bajada de T3)
				inj_halt <= 1'b0;
			end
		end else if (~m1_seen) begin
			m1_seen <= 1'b1;
			inj <= arm | halt_ok;
			inj_halt <= halt_ok;
		end
	end

	always @(posedge iclock or negedge nreset) begin
		if (~nreset) begin
			enabled <= 1'b0;
			phase <= phIDLE;
			pending <= 1'b0;
			arm <= 1'b0;
			vs_sync <= 2'b00;
			vs_prev <= 1'b0;
			m1_prev <= 1'b0;
			m1_addr <= 16'd0;
			count <= 16'd0;
			halted <= 1'b0;
			call_now <= 1'b0;
			took_call <= 1'b0;
		end else begin
			vs_sync <= {vs_sync[0], vsync};
			vs_prev <= vs_sync[1];
			m1_prev <= m1_sample;
			if (m1_sample)
				m1_addr <= addr;					// se queda con la de T2

			if (enable_int)  enabled <= 1'b1;		// POKE 2040,1
			if (disable_int) enabled <= 1'b0;		// POKE 2040,0

			// Las decisiones solo cambian fuera de las M1. Se recalculan en
			// la bajada de T4 con el boundary y la fase ya al dia (los dos
			// cambian en la bajada de T3), antes de la M1 siguiente.
			if (~m1_sample) begin
				arm <= enabled & superfast_mode & pending & (phase == phIDLE) & boundary;
				call_now <= pending | ~halted;		// un HALT espera al VSYNC
			end

			// Transiciones: al acabar cada M1, por su direccion
			if (m1_end) begin
				case (phase)
					phIDLE:
						if (inj) begin						// el RST ya esta dentro
							phase <= phENTRY;
							halted <= inj_halt;
						end
					phENTRY:
						if (m1_addr == 16'h0038)
							took_call <= call_now;			// CALL, o una vuelta mas de JR $
						else
							phase <= phISR;					// ya en la rutina
					phISR:
						if (m1_addr == 16'h003B)			// la rutina ha vuelto
							phase <= halted ? phIDLE : phEPI;	// tras un HALT, RET y listo
					phEPI:
						if (m1_addr == 16'h003E)
							phase <= phIDLE;				// fin: vuelve a X
				endcase
			end

			// VSYNC -> interrupcion pendiente (solo activas y en Superfast);
			// se atiende con el CALL
			if (~enabled || ~superfast_mode)
				pending <= 1'b0;
			else if (vs_sync[1] & ~vs_prev)
				pending <= 1'b1;
			else if (call_taken)
				pending <= 1'b0;

			if (call_taken)
				count <= count + 1'b1;
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
//                8    interrupciones simuladas: activas, pendiente, arm,
//                     Superfast, -, -, fase (2 bits) -- en vivo
//                9/10 interrupciones inyectadas (bajo/alto) -- en vivo
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
	input wire [7:0] si_status,	// de sim_int, para el puerto de depuracion
	input wire [15:0] si_count,
	output wire boundary,		// 1: la proxima M1 empieza una instruccion
	output wire index_prefix,	// 1: la proxima M1 va detras de un DD/FD
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
	assign index_prefix = (state == stINDEX);

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
			4'd8:  dbg_out = si_status;
			4'd9:  dbg_out = si_count[7:0];
			4'd10: dbg_out = si_count[15:8];
			4'd15: dbg_out = 8'h51;
			default: dbg_out = 8'h00;
		endcase
	end

endmodule
