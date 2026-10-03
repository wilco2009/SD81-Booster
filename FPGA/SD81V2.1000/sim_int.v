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
//                5/6 puntero de la BRAM de sombra (bajo/alto): se carga
//                    al escribir el alto
//                7   escribe el dato en la BRAM de sombra, en el puntero, y
//                    el puntero avanza (para cargar snapshots: la sombra
//                    tiene que quedar como la memoria que ve el programa)
//  IN:  0 estado: armado, NMI encendida, nivel (1 = dentro de la rutina de
//         interrupcion), -, -, motivo (1 MCU, 2 joystick, 3 trampa, 4 paso,
//         5 comparador de ejecucion, 6 punto de vigilancia, 7 FF de memoria)
//       1 interrupciones simuladas: activas, pendiente, arm, Superfast,
//         halted, call_now, fase
//       2 el byte de la BRAM de sombra en el puntero; el puntero avanza al
//         acabar cada IN (para los snapshots: ahi estan los ultimos valores
//         de los POKEs de control, que en la FPGA son de solo escritura)
//       3 el registro de Chroma81 (lo escrito con OUT $7FEF, que no se puede
//         leer por su puerto)
//       4/5 el registro seleccionado del AY A / B (el latch de direccion, que
//         no se puede leer por sus puertos)
//       6 las paginas escritas por la CPU, de una en una: bit 0 = la pagina
//         del puntero, que vuelve a 0 al elegir el indice 6 y avanza al acabar
//         cada IN (64 IN seguidos: las 64 paginas). Para que los snapshots
//         guarden solo las que usa el programa
//       15 firma 52h
//
// Revision 0.12 - SLOW: se rompe en la entrada de la NMI (la M1 en $0066),
//                 sin ventana de tiempos: la rutina aun no ha tocado AF' y la
//                 siguiente NMI esta a ~200 ciclos. El MCU deshace la NMI
//                 (PC = [SP], SP+2). Un FF se sirve como JR $ hasta la NMI
//                 siguiente y rompe ahi con el motivo 7
// Revision 0.11 - SLOW se decide por el tiempo desde la ultima NMI (menos
//                 de un cuadro), no por los OUT ($FE)/($FD): la ROM apaga la
//                 NMI mientras dibuja y ahi no se puede parar (la INT de cada
//                 linea entraria en el monitor)
// Revision 0.10 - SLOW: con la NMI encendida solo se rompe en una ventana
//                 justo despues de la NMI de una linea (48-140 ciclos tras su
//                 M1 en $0066), para que el monitor la apague antes de la
//                 siguiente. Un FF fuera de la ventana se sirve como JR $
//                 (18 FE) hasta que se abra. El bit 6 del estado dice si la
//                 NMI estaba encendida al parar
// Revision 0.09 - Indice 6: las paginas escritas, en RAM distribuida de
//                 64x1 (orden 11 de SD81.v para borrarlas). Con 64 registros
//                 no cabia
// Revision 0.08 - Escritura de la BRAM de sombra por el puerto (registro 7,
//                 carga de snapshots). Indices 4/5: el registro elegido de
//                 cada AY
// Revision 0.07 - Lectura de la BRAM de sombra por el puerto (snapshots)
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
	output reg [7:0] port_out,			// lo que devuelve IN de $3FEF
	output wire bram_rd,				// IN del indice 2: SD81.v lee la BRAM en bram_ptr
	output wire bram_wr,				// OUT del registro 7: SD81.v escribe el bus en bram_ptr
	output reg [15:0] bram_ptr = 16'd0,
	input wire [7:0] bram_data,			// el byte leido (douta del puerto A)
	input wire [7:0] chroma_reg,		// registro de Chroma81 ($7FEF)
	input wire [7:0] ay_sel_a,			// registro elegido del AY A (A3=1) y del B
	input wire [7:0] ay_sel_b,
	input wire sram_wr,					// la CPU escribe en la SRAM (SD81.v: ~nWRx)
	input wire [5:0] sram_page,			// en esta pagina
	input wire dirty_clr				// orden 11: borrar las paginas escritas (al subir)
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
	reg nmi_brk = 1'b0;			// el programa estaba en SLOW al parar (bit 6 del estado)
	reg [15:0] nmi_age = 16'hFFFF;	// ciclos desde la ultima M1 en $0066 (la NMI de una linea)
	reg dspin = 1'b0;			// esta M1 (un FF fuera de la ventana) recibe JR $
	reg dspin_op = 1'b0;		// y la lectura siguiente, su operando FE
	reg spin_pend = 1'b0;		// un FF esperando a la NMI (rompe en ella, motivo 7)
	reg io_prev = 1'b0;
	reg [1:0] armed_s = 2'b00;	// dbg_loaded viene del dominio de CFG_CLK
	// paginas escritas: RAM distribuida de 64x1 con dos puertos (2 LUTs);
	// se borra recorriendola con un contador (64 ciclos)
	reg dmem [0:63];
	integer di;
	initial for (di = 0; di < 64; di = di + 1) dmem[di] = 1'b0;
	reg [2:0] dclr_s = 3'b000;	// la orden 11 viene del dominio de CFG_CLK
	reg dclr_run = 1'b0;
	reg [5:0] dclr_cnt = 6'd0;
	reg [5:0] dptr = 6'd0;		// la pagina que devuelve el indice 6
	reg dirty_rd_prev = 1'b0;
	reg [2:0] tgl_s = 3'b000;
	reg [2:0] joy_s = 3'b000;
	reg [7:0] bptr_lo = 8'd0;
	reg bram_rd_prev = 1'b0;
	reg bram_wr_go = 1'b0;		// este OUT es el dato del registro 7
	reg bram_wr_prev = 1'b0;

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
	// SLOW: la NMI llega cada 207 ciclos y su rutina ($0066) usa AF', asi que
	// no puede saltar mientras el monitor entra (RST, CALL, JP y su OUT
	// ($FD),A, que la apaga). En SLOW solo se rompe en la primera M1 de la
	// NMI ($0066): la rutina aun no ha hecho nada y la siguiente NMI esta
	// lejos. Mientras se dibuja la pantalla no hay NMI (la ROM la apaga, con
	// la INT habilitada: ahi no se puede parar). SLOW es "ha habido una NMI
	// hace menos de un cuadro" (192 lineas sin NMI son unos 40000 ciclos; el
	// contador llega a 65535), no lo que digan los OUT ($FE)/($FD)
	wire slow = (nmi_age != 16'hFFFF);
	wire at_nmi = (addr == 16'h0066);
	wire brk_ok = ~slow | at_nmi;
	wire take = armed & brk_ok & dbg_idle & prog_m1 &
	            (swbp | spin_pend | step_brk | (~skip & (pause_pend | watch_pend | cmp_exec)));
	// Un FF (breakpoint) en SLOW fuera de la NMI no se puede dejar pasar
	// (seria un RST de verdad): se sirve JR $ y la CPU vuelve a el hasta que
	// llega la NMI, donde se rompe (spin_pend) y el MCU la deshace
	wire spin = armed & ~brk_ok & dbg_idle & prog_m1 & swbp;
	wire [2:0] take_reason = (swbp | spin_pend) ? 3'd7 :
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
	// IN del indice 2 de $3FEF: mientras dura, el puerto A de la BRAM lee en
	// bram_ptr (lo hace SD81.v; el blit del doble buffer espera)
	assign bram_rd = ~nIORQ & ~nRD & nM1 & (addr == 16'h3FEF) & (ridx == 4'd2);
	wire dirty_rd = ~nIORQ & ~nRD & nM1 & (addr == 16'h3FEF) & (ridx == 4'd6);
	wire dirty_bit = dmem[dptr];
	always @(negedge iclock)
		if (dclr_run | sram_wr)
			dmem[dclr_run ? dclr_cnt : sram_page] <= ~dclr_run;
	// OUT del dato del registro 7: desde la muestra que lo reconoce hasta que
	// acaba el ciclo, el puerto A de la BRAM escribe el bus en bram_ptr
	assign bram_wr = bram_wr_go & io_wr;
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
		end else if (m1rd && dspin) begin
			enable_out = 1'b1;
			data_out = 8'h18;						// JR $ (SLOW, fuera de la ventana)
		end else if (rd && nM1 && dspin_op) begin
			enable_out = 1'b1;
			data_out = 8'hFE;
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
			4'd0:  port_out = {armed, nmi_brk, lvl, 2'b00, reason};
			4'd1:  port_out = sim_status;
			4'd2:  port_out = bram_data;
			4'd3:  port_out = chroma_reg;
			4'd4:  port_out = ay_sel_a;
			4'd5:  port_out = ay_sel_b;
			4'd6:  port_out = {7'd0, dirty_bit};
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
			nmi_brk <= 1'b0;
			nmi_age <= 16'hFFFF;
			dspin <= 1'b0;
			dspin_op <= 1'b0;
			spin_pend <= 1'b0;
			io_prev <= 1'b0;
			armed_s <= 2'b00;
			tgl_s <= 3'b000;
			joy_s <= 3'b000;
			bptr_lo <= 8'd0;
			bram_ptr <= 16'd0;
			bram_rd_prev <= 1'b0;
			bram_wr_go <= 1'b0;
			bram_wr_prev <= 1'b0;
			dclr_s <= 3'b000;
			dclr_run <= 1'b0;
			dclr_cnt <= 6'd0;
			dptr <= 6'd0;
			dirty_rd_prev <= 1'b0;
		end else begin
			armed_s <= {armed_s[0], dbg_loaded};
			tgl_s <= {tgl_s[1:0], dbg_pause_tgl};
			joy_s <= {joy_s[1:0], ~joy_up_n & ~joy_down_n};
			dphase_n <= dphase;

			// el puntero de la BRAM avanza al acabar cada IN del indice 2
			bram_rd_prev <= bram_rd;
			bram_wr_prev <= bram_wr;

			// las paginas escritas: borrarlas al subir la orden 11, y el
			// puntero de lectura avanza al acabar cada IN del indice 6
			dclr_s <= {dclr_s[1:0], dirty_clr};
			if (dclr_s[1] & ~dclr_s[2]) begin
				dclr_run <= 1'b1;
				dclr_cnt <= 6'd0;
			end else if (dclr_run) begin
				dclr_cnt <= dclr_cnt + 1'b1;
				if (dclr_cnt == 6'd63) dclr_run <= 1'b0;
			end
			dirty_rd_prev <= dirty_rd;

			// SLOW: ciclos desde la NMI de la ultima linea (su M1 en $0066)
			if (m1_sample && addr == 16'h0066) nmi_age <= 16'd0;
			else if (slow) nmi_age <= nmi_age + 1'b1;
			if (dirty_rd_prev & ~dirty_rd)
				dptr <= dptr + 1'b1;
			if ((bram_rd_prev & ~bram_rd) | (bram_wr_prev & ~bram_wr))
				bram_ptr <= bram_ptr + 1'b1;
			if (~io_wr)
				bram_wr_go <= 1'b0;

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

			// OUT al puerto $3FEF, una vez por ciclo
			io_prev <= io_wr;
			if (io_wr & ~io_prev) begin
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
							3'd5: bptr_lo <= data;
							3'd6: bram_ptr <= {data, bptr_lo};
							3'd7: bram_wr_go <= 1'b1;
						endcase
					end else if (data[7]) begin
						wexp <= 1'b1;
						wreg <= data[2:0];
					end else if (data == 8'h10) begin
						if (armed & dbg_idle) begin		// trampa
							pause_pend <= 1'b1;
							pause_src <= 2'd3;
						end
					end else if (data[7:4] == 4'd0) begin
						ridx <= data[3:0];
						if (data[3:0] == 4'd6) dptr <= 6'd0;	// las paginas, desde la 0
					end
				end
			end

			// M1: inyectar o no, en la primera muestra
			if (~m1_sample) begin
				m1_seen <= 1'b0;
				if (~m1_prev) begin					// ya procesado el final de la M1
					inj <= 1'b0;					// (en la bajada de T3)
					inj_halt <= 1'b0;
					dinj <= 1'b0;
					if (dspin) dspin_op <= 1'b1;	// detras viene la lectura del operando
					dspin <= 1'b0;
				end
			end else if (~m1_seen) begin
				m1_seen <= 1'b1;
				inj <= sim_inj & ~take & ~spin;
				inj_halt <= halt_ok & ~take & ~spin;
				dinj <= take;
				dspin <= spin;
				dspin_op <= 1'b0;
				if (spin) spin_pend <= 1'b1;
				if (take) begin
					spin_pend <= 1'b0;
					nmi_brk <= slow;
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
