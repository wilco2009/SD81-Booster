`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// sim_int -- interrupciones simuladas a 50 Hz (modos Superfast, con DI) y
//            depurador por hardware
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
//  DEPURADOR (ver claude/planning/hw_debugger_plan.md)
//
//  Usa el mismo mecanismo con su propia maquina de estados, que tiene
//  prioridad: mientras esta activa es la duena de $0038-$003E y la de las
//  interrupciones simuladas se queda congelada (anidado: se puede romper
//  dentro de la rutina de interrupcion). Solo con el depurador ARMADO (orden
//  9 del MCU, "monitor cargado": el monitor esta en la pagina 63) y con el
//  generador de NMI apagado (FAST o Superfast).
//
//   1. Ruptura en una M1 que empieza una instruccion del programa (no las
//      que sirve la FPGA): se sirve FF. Motivos: pausa (orden 8 del MCU, el
//      joystick arriba+abajo o la trampa OUT $10), el comparador (ejecucion
//      en esa direccion, o lectura/escritura/E/S, que rompe en la siguiente),
//      el contador de pasos o un FF leido de la memoria (breakpoint por
//      software: es un RST 38h de verdad, la FPGA solo lo ve).
//   2. En $0038-$003A, CALL $2000. Tras las escrituras del CALL, el bloque 1
//      pasa a la pagina 63 (dbg_win): la primera M1 en $2000 ya es del
//      monitor. Mientras el monitor esta activo (dbg_mon) el bloque 0 se
//      puede escribir y no se copia nada a la BRAM (lo hace SD81.v).
//   3. El monitor sale con JP $003B: en esa M1 se sirve EX (SP),HL y al
//      acabarla vuelve la pagina del programa; despues DEC HL / EX (SP),HL /
//      RET, y se vuelve a X. La primera instruccion al volver no rompe por
//      pausa ni por el comparador (skip).
//
//  El paso a paso (contador N) se queda en el nivel donde se paro: si fue en
//  el programa principal, la rutina de interrupcion se ejecuta entera sin
//  contar.
//
//  Puerto $3FEF (OUT, los 16 bits de la direccion):
//    $00-$0F   elige lo que devuelve IN
//    $10       trampa: pausa en la instruccion siguiente
//    $80+r     el OUT siguiente es el dato del registro r:
//                0/1 comparador (direccion baja/alta)
//                2   modo del comparador: 0 apagado, 1 ejecucion, 2 lectura,
//                    3 escritura, 4 E/S (byte bajo del puerto)
//                3/4 N (bajo/alto): romper tras N instrucciones (0 = no);
//                    se carga al escribir el alto
//  IN:  0 estado: armado, NMI encendida, nivel (1 = dentro de la rutina de
//         interrupcion), -, -, motivo (1 MCU, 2 joystick, 3 trampa, 4 paso,
//         5 comparador de ejecucion, 6 punto de vigilancia, 7 FF de memoria)
//       1 interrupciones simuladas: activas, pendiente, arm, Superfast,
//         halted, call_now, fase
//       15 firma 52h
//
// Revision 0.06 - Depurador por hardware (fase 1). Sin contadores de prueba
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
	input wire nWR,
	input wire nMREQ,
	input wire nIORQ,
	input wire nRFSH,
	input wire dbg_loaded,				// orden 9 del MCU: monitor en la pagina 63
	input wire dbg_pause_tgl,			// orden 8 del MCU: pausa (al conmutar)
	input wire joy_up_n,				// joystick: arriba+abajo a la vez = pausa
	input wire joy_down_n,
	output reg [7:0] data_out,
	output reg enable_out,				// servir data_out en esta lectura
	output wire [1:0] state,
	output reg enabled,
	output wire dbg_win,				// bloque 1 -> pagina 63
	output wire dbg_mon,				// el monitor esta activo
	output reg [7:0] port_out			// lo que devuelve IN de $3FEF
    );

	// ---------------------------------------------------------------
	// Interrupciones simuladas
	// ---------------------------------------------------------------
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
	reg inj = 1'b0;				// esta M1 recibe el FF (interrupcion simulada)
	reg inj_halt = 1'b0;		// ... y lo que habia era un HALT
	reg halted = 1'b0;			// la interrupcion en curso viene de un HALT
	reg call_now = 1'b0;		// en $0038: CALL (1) o JR $ (0, esperando el VSYNC)
	reg took_call = 1'b0;		// lo que se sirvio en la ultima M1 en $0038

	// ---------------------------------------------------------------
	// Depurador
	// ---------------------------------------------------------------
	localparam [2:0]
		dIDLE   = 3'd0,		// el programa corre
		dENTRY  = 3'd1,		// FF servido: $0038-$003A (CALL $2000)
		dCALLED = 3'd2,		// el CALL ya ha leido $003A: falta su M1 en $2000
		dMON    = 3'd3,		// el monitor corre (ventana en el bloque 1)
		dEPI    = 3'd4;		// epilogo en $003C-$003E

	reg [2:0] dphase = dIDLE;
	reg [2:0] dphase_n = dIDLE;	// dphase vista en la bajada anterior
	reg dinj = 1'b0;			// esta M1 recibe el FF del depurador
	reg [2:0] reason = 3'd0;	// motivo de la ultima ruptura
	reg lvl = 1'b0;				// la ultima ruptura fue dentro de una interrupcion
	reg skip = 1'b0;			// la primera instruccion al continuar no rompe
	reg pause_pend = 1'b0;		// pausa pendiente
	reg [1:0] pause_src = 2'd0;	// 1 MCU, 2 joystick, 3 trampa
	reg watch_pend = 1'b0;		// punto de vigilancia disparado
	reg step_on = 1'b0;			// contando instrucciones
	reg [15:0] step_cnt = 16'd0;
	reg [7:0] step_lo = 8'd0;
	reg [15:0] cmp_addr = 16'd0;
	reg [2:0] cmp_mode = 3'd0;
	reg [3:0] ridx = 4'd0;		// lo que devuelve IN
	reg wexp = 1'b0;			// el OUT siguiente es el dato de wreg
	reg [2:0] wreg = 3'd0;
	reg nmi_on = 1'b0;			// generador de NMI encendido (SLOW): no se rompe
	reg io_prev = 1'b0;
	reg [1:0] armed_s = 2'b00;	// dbg_loaded viene del dominio de CFG_CLK
	reg [2:0] tgl_s = 3'b000;
	reg [2:0] joy_s = 3'b000;

	wire armed = armed_s[1];
	wire dbg_idle = (dphase == dIDLE);

	assign state = phase;
	wire [7:0] sim_status = {enabled, pending, arm, superfast_mode, halted, call_now, phase};

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
	// la interrupcion se entrega: acaba la M1 del CALL en $0038 (no cuentan
	// las M1 del depurador)
	wire call_taken = m1_end && ~dinj && dbg_idle && (phase == phENTRY) &&
	                  (m1_addr == 16'h0038) && call_now;

	// Las M1 que sirve la FPGA (interrupciones simuladas o depurador) y las
	// del monitor no son instrucciones del programa: ahi no se rompe ni se
	// cuenta
	wire sim_serves_m1 = (phase == phENTRY && addr == 16'h0038) ||
	                     (phase == phISR && addr == 16'h003B) ||
	                     (phase == phEPI && (addr == 16'h003C || addr == 16'h003D || addr == 16'h003E));
	wire prog_m1 = boundary & ~sim_serves_m1 & (dphase == dIDLE);

	// Un HALT que se puede cambiar por el FF: al empezar instruccion o
	// detras de DD/FD (DD 76 tambien es HALT; CB 76, ED 76 y DD CB d 76 no)
	wire halt_ok = enabled & superfast_mode & (phase == phIDLE) & dbg_idle &
	               (boundary | index_prefix) & (data == 8'h76);
	wire sim_inj = arm | halt_ok;

	// Ruptura del depurador en esta M1 (se evalua en la subida de T2)
	wire cmp_exec = (cmp_mode == 3'd1) && (addr == cmp_addr);
	wire counted = prog_m1 & ~sim_inj & (lvl | (phase == phIDLE));	// el paso se queda en su nivel
	wire step_brk = step_on & counted & (step_cnt == 16'd0);
	wire swbp = (data == 8'hFF);		// RST 38h de verdad: breakpoint por software
	wire take = armed & ~nmi_on & dbg_idle & prog_m1 &
	            (swbp | step_brk | (~skip & (pause_pend | watch_pend | cmp_exec)));
	wire [2:0] take_reason = swbp ? 3'd7 :
	                         (~skip & cmp_exec) ? 3'd5 :
	                         (~skip & watch_pend) ? 3'd6 :
	                         step_brk ? 3'd4 : {1'b0, pause_src};

	// Accesos que mira el comparador (en la subida del reloj del Z80, como
	// las capturas de POKEs: /WR e /IORQ ya estan estables)
	wire mrd_n = ~nMREQ & ~nRD & nM1 & nRFSH;
	wire mwr_n = ~nMREQ & ~nWR;
	wire io_any = ~nIORQ & nM1;				// IN u OUT (no el reconocimiento de INT)
	wire io_wr = ~nIORQ & ~nWR & nM1;
	wire watch_hit = ((cmp_mode == 3'd2) & mrd_n & (addr == cmp_addr)) |
	                 ((cmp_mode == 3'd3) & mwr_n & (addr == cmp_addr)) |
	                 ((cmp_mode == 3'd4) & io_any & (addr[7:0] == cmp_addr[7:0]));

	assign dbg_win = (dphase == dMON) | ((dphase == dCALLED) & ~nM1);
	assign dbg_mon = (dphase == dMON);

	// Lo que se sirve en ESTA lectura: combinacional, estable mientras dure
	always @(*) begin
		enable_out = 1'b0;
		data_out = 8'h00;
		if (~dbg_idle) begin								// --- depurador ---
			if (dphase == dENTRY && m1rd && addr == 16'h0038) begin
				enable_out = 1'b1;
				data_out = 8'hCD;					// CALL $2000
			end else if ((dphase == dENTRY || dphase == dCALLED) && rd && nM1 && addr == 16'h0039) begin
				enable_out = 1'b1;
				data_out = 8'h00;
			end else if ((dphase == dENTRY || dphase == dCALLED) && rd && nM1 && addr == 16'h003A) begin
				enable_out = 1'b1;
				data_out = 8'h20;
			end else if (dphase == dMON && m1rd && addr == 16'h003B) begin
				enable_out = 1'b1;
				data_out = 8'hE3;					// EX (SP),HL
			end else if (dphase == dEPI && m1rd) begin
				if (addr == 16'h003C) begin
					enable_out = 1'b1;
					data_out = 8'h2B;				// DEC HL
				end else if (addr == 16'h003D) begin
					enable_out = 1'b1;
					data_out = 8'hE3;				// EX (SP),HL
				end else if (addr == 16'h003E) begin
					enable_out = 1'b1;
					data_out = 8'hC9;				// RET
				end
			end
		end else if (m1rd && (dinj || inj)) begin
			enable_out = 1'b1;
			data_out = 8'hFF;						// RST 38h
		end else if (phase == phENTRY && rd) begin			// --- interrupciones simuladas ---
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

	always @(*) begin
		case (ridx)
			4'd0:  port_out = {armed, nmi_on, lvl, 2'b00, reason};
			4'd1:  port_out = sim_status;
			4'd15: port_out = 8'h52;
			default: port_out = 8'h00;
		endcase
	end

	// Subida del reloj del Z80 (bajada de iclock): la decision en la primera
	// muestra de la M1, que es la de T2 (MREQ bajo desde la bajada de T1 y el
	// dato de la SRAM ya valido; la CPU no lo lee hasta la subida de T3), y
	// las escrituras de memoria y E/S, que en T3 ya estan estables.
	always @(negedge iclock or negedge nreset) begin
		if (~nreset) begin
			m1_seen <= 1'b0;
			inj <= 1'b0;
			inj_halt <= 1'b0;
			dinj <= 1'b0;
			dphase_n <= dIDLE;
			reason <= 3'd0;
			lvl <= 1'b0;
			skip <= 1'b0;
			pause_pend <= 1'b0;
			pause_src <= 2'd0;
			watch_pend <= 1'b0;
			step_on <= 1'b0;
			step_cnt <= 16'd0;
			step_lo <= 8'd0;
			cmp_addr <= 16'd0;
			cmp_mode <= 3'd0;
			ridx <= 4'd0;
			wexp <= 1'b0;
			wreg <= 3'd0;
			nmi_on <= 1'b0;
			io_prev <= 1'b0;
			armed_s <= 2'b00;
			tgl_s <= 3'b000;
			joy_s <= 3'b000;
		end else begin
			armed_s <= {armed_s[0], dbg_loaded};
			tgl_s <= {tgl_s[1:0], dbg_pause_tgl};
			joy_s <= {joy_s[1:0], ~joy_up_n & ~joy_down_n};
			dphase_n <= dphase;

			// al volver del monitor, la primera instruccion no rompe
			if (dbg_idle && dphase_n == dEPI)
				skip <= 1'b1;

			// pausa desde el MCU (al conmutar) o el joystick (al pulsar)
			if (armed & dbg_idle & (tgl_s[2] ^ tgl_s[1])) begin
				pause_pend <= 1'b1;
				pause_src <= 2'd1;
			end
			if (armed & dbg_idle & joy_s[1] & ~joy_s[2]) begin
				pause_pend <= 1'b1;
				pause_src <= 2'd2;
			end

			// puntos de vigilancia (lectura, escritura, E/S)
			if (armed & dbg_idle & watch_hit)
				watch_pend <= 1'b1;

			// OUT: generador de NMI (como la ULA) y puerto $3FEF, una vez por ciclo
			io_prev <= io_wr;
			if (io_wr & ~io_prev) begin
				if (~addr[1]) nmi_on <= 1'b0;			// OUT ($FD): NMI apagada (FAST)
				else if (~addr[0]) nmi_on <= 1'b1;		// OUT ($FE): NMI encendida (SLOW)
				if (addr == 16'h3FEF) begin
					if (wexp) begin
						wexp <= 1'b0;
						case (wreg)
							3'd0: cmp_addr[7:0] <= data;
							3'd1: cmp_addr[15:8] <= data;
							3'd2: cmp_mode <= data[2:0];
							3'd3: step_lo <= data;
							3'd4: begin
								step_cnt <= {data, step_lo};
								step_on <= ({data, step_lo} != 16'd0);
							end
							default: ;
						endcase
					end else if (data[7]) begin
						wexp <= 1'b1;
						wreg <= data[2:0];
					end else if (data == 8'h10) begin
						if (armed & dbg_idle) begin		// trampa
							pause_pend <= 1'b1;
							pause_src <= 2'd3;
						end
					end else if (data[7:4] == 4'd0)
						ridx <= data[3:0];
				end
			end

			// M1: inyectar o no, en la primera muestra
			if (~m1_sample) begin
				m1_seen <= 1'b0;
				if (~m1_prev) begin					// ya procesado el final de la M1
					inj <= 1'b0;					// (en la bajada de T3)
					inj_halt <= 1'b0;
					dinj <= 1'b0;
				end
			end else if (~m1_seen) begin
				m1_seen <= 1'b1;
				inj <= sim_inj & ~take;
				inj_halt <= halt_ok & ~take;
				dinj <= take;
				if (take) begin
					reason <= take_reason;
					lvl <= (phase != phIDLE);
					pause_pend <= 1'b0;
					watch_pend <= 1'b0;
					step_on <= 1'b0;
					skip <= 1'b0;
				end else if (prog_m1 & ~sim_inj) begin	// una instruccion del programa
					skip <= 1'b0;
					if (counted & step_on)
						step_cnt <= step_cnt - 1'b1;
				end
			end
		end
	end

	// Bajada del reloj del Z80 (subida de iclock): las fases
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
			halted <= 1'b0;
			call_now <= 1'b0;
			took_call <= 1'b0;
			dphase <= dIDLE;
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
				arm <= enabled & superfast_mode & pending & (phase == phIDLE) & boundary & dbg_idle;
				call_now <= pending | ~halted;		// un HALT espera al VSYNC
			end

			// Interrupciones simuladas: al acabar cada M1, por su direccion.
			// Las M1 del depurador no cuentan: mientras esta activo, la fase
			// se queda congelada.
			if (m1_end && ~dinj && dbg_idle) begin
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

			// Depurador
			case (dphase)
				dIDLE:
					if (m1_end & dinj)
						dphase <= dENTRY;					// el RST ya esta dentro
				dENTRY:
					if (mrd_n && addr == 16'h003A)
						dphase <= dCALLED;					// el CALL lee su ultimo byte
					else if (m1_end && m1_addr != 16'h0038)
						dphase <= dIDLE;					// el RST no llego (CPU en HALT)
				dCALLED:
					if (m1_sample)
						dphase <= dMON;						// primera M1 del monitor
				dMON:
					if (m1_end && m1_addr == 16'h003B)
						dphase <= dEPI;						// el monitor ha salido
				dEPI:
					if (m1_end && m1_addr == 16'h003E)
						dphase <= dIDLE;					// fin: vuelve a X
				default:
					dphase <= dIDLE;
			endcase
		end
	end

endmodule

//////////////////////////////////////////////////////////////////////////////////
// m1_tracker -- detector de limites de instruccion
//
//  Sigue las M1 con el byte que recibe la CPU (el de la memoria o el que
//  sirva la FPGA) y sabe si la M1 siguiente empieza una instruccion
//  (prefijos CB, ED, DD, FD, DD CB d op, prefijos repetidos, DD ED).
//
//  En la placa iclock = nCLOCK sube cuando baja el reloj del Z80, pero la
//  FPGA lo ve con retraso: en el flanco de bajada de T1 MREQ ya ha bajado y
//  la condicion de M1 se cumple DOS veces (T1 con el dato aun sin llegar, y
//  T2). Por eso cada M1 se procesa una sola vez, al terminar, con el opcode
//  de la ultima muestra (la de T2).
//////////////////////////////////////////////////////////////////////////////////
module m1_tracker(
	input wire iclock,			// nCLOCK: su subida es la BAJADA del reloj del Z80
	input wire nreset,
	input wire nM1,
	input wire nMREQ,
	input wire nRFSH,
	input wire [7:0] data,
	output wire boundary,		// 1: la proxima M1 empieza una instruccion
	output wire index_prefix	// 1: la proxima M1 va detras de un DD/FD
    );

	localparam [1:0]
		stNORMAL = 2'd0,		// la proxima M1 empieza una instruccion
		stSECOND = 2'd1,		// la proxima M1 es el segundo byte de CB xx / ED xx
		stINDEX  = 2'd2;		// la proxima M1 va detras de un DD/FD

	reg [1:0] state = stNORMAL;
	reg m1_prev = 1'b0;			// la condicion de M1 en la muestra anterior
	reg [7:0] op_latch = 8'd0;	// el dato de la ultima muestra de la M1

	assign boundary = (state == stNORMAL);
	assign index_prefix = (state == stINDEX);

	wire m1_sample = ~nMREQ & ~nM1 & nRFSH;
	wire m1_end = m1_prev & ~m1_sample;	// la M1 acaba de terminar

	always @(posedge iclock or negedge nreset) begin
		if (~nreset) begin
			m1_prev <= 1'b0;
			op_latch <= 8'd0;
			state <= stNORMAL;
		end else begin
			m1_prev <= m1_sample;
			if (m1_sample)
				op_latch <= data;		// se queda con la ultima muestra (T2)
			if (m1_end) begin			// una vez por M1, con el opcode de T2
				case (state)
					stNORMAL:
						if (op_latch == 8'hCB || op_latch == 8'hED)
							state <= stSECOND;
						else if (op_latch == 8'hDD || op_latch == 8'hFD)
							state <= stINDEX;
					stSECOND:
						state <= stNORMAL;
					stINDEX:
						if (op_latch == 8'hED)			// DD ED xx: queda un ED xx
							state <= stSECOND;
						else if (op_latch == 8'hDD || op_latch == 8'hFD)
							state <= stINDEX;			// otro prefijo: sigue sin ser limite
						else
							state <= stNORMAL;			// DD CB d op (d y op no son M1) o DD xx
					default:
						state <= stNORMAL;
				endcase
			end
		end
	end

endmodule
