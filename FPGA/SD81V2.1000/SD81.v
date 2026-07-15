`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date:    23:01:17 12/26/2022 
// Design Name: 
// Module Name:    SD81 
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
module SD81(
	 // Z80 Addr lines
	 input wire A0,
    input wire A1,
    input wire A2,
    input wire A3,
    input wire A4,
    input wire A5,
    input wire A6,
    input wire A7,
    input wire A8,
    input wire A9,
    input wire A10,
    input wire A11,
    input wire A12,
    input wire A13,
    input wire A14,
    input wire A15,
	 // Memory Addr lines
	 inout wire A0x,
    inout wire A1x,
    inout wire A2x,
    inout wire A3x,
    inout wire A4x,
    inout wire A5x,
    inout wire A6x,
    inout wire A7x,
    inout wire A8x,
    inout wire A9x,
    inout wire A10x,
    inout wire A11x,
    inout wire A12x,
    inout wire A13x,
    inout wire A14x,
    inout wire A15x,
    inout wire A16x,
    inout wire A17x,
    inout wire A18x,
    inout wire nWRx,
	 // DATA BUS
	 inout wire D0,		
	 inout wire D1,
	 inout wire D2,
	 inout wire D3,
	 inout wire D4,
	 inout wire D5,
	 inout wire D6,
	 inout wire D7,
	 // OTHER Z80 SIGNALS
	 input wire nCLOCK,
    input wire nIORQ,
    input wire nRD,
    input wire nWR,
    input wire nMREQ,
    input wire nM1,
    input wire nRFSH,
	 input wire nRESET,
	 inout wire nHALT,
	 output wire nBUSRQ,
	 output wire nBUSAK,
	 
	 output wire B0,
	 output wire B1,
	 output wire B2,
	 output wire B3,
	 output wire B4,
	 output wire B5,
	 output wire B6,
	 output wire B7,
	 input wire nOE_OL,
	 
	 input wire O0,
	 input wire O1,
	 input wire O2,
	 input wire O3,
	 input wire O4,
	 input wire O5,
	 input wire O6,
	 input wire O7,
	 
	 // VIDEO SIGNALS
	 output wire VRED,
	 output wire VGREEN,
	 output wire VBLUE,
	 output wire BRIGHT,
	 output wire CSYNC,
	 
	 // COMMS SIGNALS
    output wire GET_CTRL_REG,		// New control info ready on latch
    input wire nRST_CTRL_REG,		// Ctrl info readed 
    output wire GET_DATA_REG,		// New data ready on latch
    input wire nRST_DATA_REG,		// Data readed
    input wire CTRL_CLK,				// Output Enable CTRL Clock Latch
//	 input LE_IL,
//    output nMEM_CE,			// Chip Enable External MEM
	 output wire nMEM_OE,				// Output Enable External MEM
//    output RAMCS,				// Output Enable RAMCS latch
//    output ROMCS,				// Output Enable ROMCS latch
	 input wire nMEM_OEm,				// nMEMOE signal from microcontroler (conectar directamente en la siguiente version)
	 input wire CFG_RESET,
	 input wire CFG_CLK,
	 input wire CFG_DATA,
	 input wire UP,
	 input wire DOWN,
	 input wire LEFT,
	 input wire RIGHT,
	 input wire FIRE,
	 
	 output wire BCK,
	 output wire DIN,
	 output wire LRCK,
	 output wire DEBUG_RDY,
	 input wire MICRO_DB_CLK,
	 input wire SPARE0,
	 
	 input wire CLK100
	 
//	 output FLASH_SCLK,
//	 output FLASH_MISO,
//	 output FLASH_MOSI,
//	 output FLASH_CS
	 
    );
	 
		parameter CMD_BITS = 4;
		parameter MAX_CFG = 29+CMD_BITS;
	wire CLK104;
	clk104 clk104inst(
		.CLK_IN1  (CLK100), 
		.CLK_OUT1 (CLK104),              
		.RESET    (1'b0) 
//		.LOCKED   (locked_clk104)
    );
	 
	reg data_dir;	
	reg old_nCLOCK = 0;	
	wire newCLK6_5;
	wire newCLK3_25;
	assign nBUSRQ = GET_DATA_REG;
	assign nBUSAK = CTRL_CLK;
	
	wire SEL_SPI_FLASH = nRST_CTRL_REG;			// alternative use while RESET 
	

	wire clkflange = nCLOCK && ~old_nCLOCK;
	reg [5:0] pulse_cnt = 0;
	always @(posedge CLK104) begin
		old_nCLOCK <= nCLOCK;
		if (clkflange) pulse_cnt <= 0;
		else pulse_cnt<= pulse_cnt + 1;
//		if (~pulse_cnt[4]) newCLK <= 1'b1;
//		else newCLK <= 1'b0;
	end

	assign newCLK6_5 = pulse_cnt[4];
	assign newCLK3_25 = pulse_cnt[5];
		
	reg FULL_PAGING = 0;
	wire clk6_5;// = ~nclk6_5;
	// nMODE48K=1 ->32K mode - nMODE48K==0 -> 48/56K mode
	reg nMODE48K = 1'b0;		// initially enabled
	reg EN_MC45 = 1'b0;	// initially disabled
	reg sfast_mode_en  = 1'b0; 
	reg SEL_128CHARS = 1'b0;
	

	reg [15:0] DFILE = 16'd0;
	reg [15:0] FRAMES = 16'd0;
	wire [15:0] DFILE_addr = 16'd16396;
	reg [15:0] HFILE = 16'd0;
	reg sfHR_en = 1'b0;
	wire cs_SPULA = sfSP_en && (nIORQ==1'b0) && (nWR==1'b0) && (Addr[7:0]==8'hfb); // Spectrum mode ULA port is here FBh (zxprinter port)
	reg sfSP_en = 1'b0;
	reg [2:0] sp_border = 3'd7;

	// ----------------------------------------------------------------
	// DOUBLE BUFFER (present-blit) — POKE 2057
	//   POKE 2057,168+blk (10101BBB) -> enable, front buffer = bloque logico BBB de la BRAM
	//   POKE 2057,85                 -> disable
	// El video (Spectrum/HiRes) lee siempre el bloque front (BRAM privada,
	// enmascarada de escrituras CPU). En cada blanking una FSM copia el bloque
	// shadow (HFILE) -> front dentro de la BRAM: la pantalla muestra siempre
	// el snapshot del ultimo VSYNC (sin tearing, una sola superficie de dibujo).
	// ----------------------------------------------------------------
	reg dbuf_en = 1'b0;
	reg [2:0] front_blk = 3'd5;

	// ----------------------------------------------------------------
	// WRX en los segundos 8KB — POKE 2058
	//   POKE 2058,170 -> ON: durante el refresh con A13=1 (I en $20-$3F) la
	//                    direccion I*256+R pasa TAL CUAL a la SRAM (RAM plana
	//                    en 8-16K, como un ZX81 con ampliacion en esa zona).
	//   POKE 2058,85  -> OFF (defecto): comportamiento actual, esa region se
	//                    trata como generador de caracteres en RAM (chr RAM).
	// Un ZX81 real con RAM en 8-16K hace WRX; uno con placa de chargen hace
	// tabla — son dos hardwares distintos sobre la misma region y no es
	// autodetectable: el propio emulador tambien lo pide como opcion (WRX +
	// RAM 8-16K). Ejemplo que lo necesita: Hi-res Chess de Psion (I=$2xxx).
	// ----------------------------------------------------------------
	reg wrx_en = 1'b0;
	reg blit_run = 1'b0;
	reg blit_phase = 1'b0;
	reg [12:0] blit_cnt = 13'd0;
	reg old_blit_start = 1'b0;
	wire [2:0] vpage = dbuf_en ? front_blk : HFILE[15:13];
	wire [15:0] FRAMES_addr = 16'd16436;
	wire lFRAMES_read = (nMREQ==0) && (nRD==0) && (Addr==FRAMES_addr) && (sfast_mode_en==1) && (sfSP_en==0) && !block0Writable;
	wire hFRAMES_read = (nMREQ==0) && (nRD==0) && (Addr==FRAMES_addr+1) && (sfast_mode_en==1) && (sfSP_en==0) && !block0Writable;
	wire micro_wr = ~nRESET & ~nWRx; 
	wire micro_rd = ~nRESET & nWRx; 
	wire [15:0] Addr = {A15,A14,A13,A12,A11,A10,A9,A8,A7,A6,A5,A4,A3,A2,A1,A0};
	wire [15:0] Addrx = {A15x,A14x,A13x,A12x,A11x,A10x,A9x,A8x,A7x,A6x,A5x,A4x,A3x,A2x,A1x,A0x};
	wire [7:0] data = {D7,D6,D5,D4,D3,D2,D1,D0};
	wire pixel_clk = clk6_5;
		
	reg [8:0] pixel_cnt = 0;
	wire [7:0] HSYNCcnt = pixel_cnt[8:1];
	reg [7:0] shift_register_fast_debug;
	reg [15:0] shift_register_fast_addr_debug;
	reg [7:0] char_latch_fast_debug;
	reg [15:0] char_latch_fast_addr_debug;
	reg [7:0] attr_latch_fast_debug;
	reg [15:0] attr_latch_fast_addr_debug;
	reg [7:0] debug_data = 0;
	reg [7:0] debug_rdy_delayed = 0;
	
	reg [7:0] debug_param1 = 0;
	reg [7:0] debug_param2 = 0;
	reg [7:0] debug_param3 = 0;
	reg [7:0] debug_param4 = 0;
	wire debug_rdy_long;
	wire debug_mode;
	reg debug_capture_done;
	wire debug_capture_start;
	reg debug_capture_mode = 0;
	reg [7:0] debug_capture_command = 0;
	reg [7:0] debug_capture_value = 0;
	reg [7:0] debug_capture_param = 0;

	wire [2:0] col_cnt = {pixel_cnt+(pixel_cnt>31)}[2:0];
	reg forced_NOP_cycle=0;
	wire inverse_video;
	assign inverse_video = 1'b0;
	reg [7:0] shift_register;
	reg serial_output = 0;
	reg serial_output_fast = 0;
	reg old_load_enable = 0;
	reg old_load_enable_fast = 0;
	reg [7:0] char_latch = 0;			
	reg [7:0] char_latch_fast = 0;			
	reg [8:0] line_cnt = 0;
	
	
	wire char_latch_enable = ~nMREQ & nRFSH;
	wire forced_NOP_start = (~D6 & A15 & nHALT);
	wire NOP_en = forced_NOP_cycle & m1cycle_delayed;
	
	reg nQS_en = 1'b1; // initially QS interface disabled // test_NMI;
	
	reg block0Writable = 1'b0;
	
		 
// ***************************************************
//      CLOCK GENERATION
// **************************************************
		wire clk_26;
		// Wires para conectar los DCMs y las señales de control
		wire clk_intermediate_13mhz; // Salida del primer DCM, entrada del segundo
		wire locked_stage1;
		wire locked_stage2;
		wire system_reset_n; // Señal de reset activa a nivel bajo

		// El reset del sistema se libera (pasa a '1') solo cuando AMBOS DCMs están estables
		assign system_reset_n = locked_stage1 & locked_stage2;
    //================================================================
    // ETAPA 1: DCM "Booster" para pasar de 3.25MHz a 13MHz
    // Instanciación manual de un DCM_SP
    //================================================================
    DCM_SP dcm_booster_inst (
        // Salidas
        .CLKFX     (clk_intermediate_13mhz), // Salida de 13 MHz
        .LOCKED    (locked_stage1),
        // Salidas no utilizadas
        .CLK0      (),
        .CLK180    (),
        .CLK270    (),
        .CLK2X     (),
        .CLK2X180  (),
        .CLK90     (),
        .CLKDV     (),
        .PSDONE    (),
        // Entradas
        .CLKIN     (nCLOCK),
        .CLKFB     (1'b0), // Feedback no necesario para síntesis de frecuencia simple
        .PSCLK     (1'b0),
        .PSEN      (1'b0),
        .PSINCDEC  (1'b0),
        .RST       (1'b0)  // reset global
    );
    // Definición de parámetros para el DCM_SP

    defparam dcm_booster_inst.CLKFX_DIVIDE    = 1;       // Divisor de la salida CLKFX
    defparam dcm_booster_inst.CLKFX_MULTIPLY  = 4;       // Multiplicador (3.25 * 4 = 13 MHz)
    defparam dcm_booster_inst.CLKIN_PERIOD    = 307.692; // Período de entrada en ns (1 / 3.25e6 * 1e9)
    //================================================================
    // ETAPA 2: DCM generado por el Wizard para pasar de 13MHz a 26MHz
    //================================================================
    wire system_clk;
    clk26 dcm_final_inst (
		.CLK_IN1  (clk_intermediate_13mhz), // Conecta la salida del primer DCM aquí
		.CLK_OUT1 (system_clk),              // Tu reloj final de 26 MHz
		.RESET    (1'b0), 
		.LOCKED   (locked_stage2)
    );
	 
	 
//	 reg [2:0] cnt26 = 3'b000;
	 reg [25:0] cnt26 = 0;
	 
	 always@(posedge system_clk) begin
			cnt26 <= cnt26+1'b1;
	 end
	 assign clk6_5 = cnt26[1];
	 
//	 wire iclock = ~cnt26[2];
//	 wire iclock = ~newCLK3_25;
	 wire iclock = nCLOCK;

	 
	wire flassing_period = cnt26[23]==1;	
//// ***************************************************
////		CHROMA 81 INTERFACE
//// ***************************************************
/*
Port $7FEF (01111111 11101111) - OUT:

+---+---+---+---+---+---+---+---+
| 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
+---+---+---+---+---+---+---+---+
  |   |   |   |   |   |   |   |
  |   |   |   |   |   +---+---+-------- Border colour (format: GRB).
  |   |   |   |   +-------------------- Border colour bright bit.
  |   |   |   +------------------------ Mode (0=Character code, 1=Attribute file).
  |   |   +---------------------------- 1=Enable colour mode.
  +---+-------------------------------- Reserved for future use (always set to 0).
Port $7FEF (01111111 11101111) - IN:

+---+---+---+---+---+---+---+---+
| 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
+---+---+---+---+---+---+---+---+
  |   |   |   |   |   |   |   |
  |   |   |   +---+---+---+---+-------- X=Not used (reserved for future use).
  |   |   +---------------------------- 0=Colour modes available, allways 0is set to ON.
  +---+-------------------------------- X=Not used (reserved for future use).	reg [7:0] attr_latch = 8'b11111100;
*/
	reg [7:0] attr_latch = 8'b11111100;
	reg [7:0] attr_latch_fast = 8'b11111100;
	wire [3:0] ink_active; // = 4'b1100;
	wire [3:0] paper_active; // = 4'b1111;
	reg [7:0] chroma_mode_reg = 8'b00101100;
	wire [3:0] border_color = chroma_mode_reg[3:0];
	wire color_mode = chroma_mode_reg[4];
	wire color_enable = chroma_mode_reg[5];
	
	wire chroma_mode_wr = ~nIOWR & (Addr==16'h7FEF);
	wire chroma_mode_rd = ~nIORD & (Addr==16'h7FEF);
	always@(posedge chroma_mode_wr or negedge nRESET) 
	begin
		if (nRESET==0) begin
			chroma_mode_reg <= 8'b00001100;
		end else begin
			chroma_mode_reg <= data;
		end
	end
	
	reg [7:0] border_char[0:7];
	initial begin
		 border_char[0] = 8'b00000000;
		 border_char[1] = 8'b00000000;
		 border_char[2] = 8'b00000000;
		 border_char[3] = 8'b00000000;
		 border_char[4] = 8'b00000000;
		 border_char[5] = 8'b00000000;
		 border_char[6] = 8'b00000000;
		 border_char[7] = 8'b00000000;
	end
	reg [3:0] border_ink = 4'b0000;
	reg bpattern_en = 1'b0;
	reg [2:0] border_pixel_cnt = 3'b000;

	wire poke_wr = !block0Writable && (nMREQ==1'b0) && (nWR==1'b0) && (Addr >= 16'd2041) && (Addr < 16'd2059);

	always@(negedge nMREQ)
		if (~nRFSH) ROMTABLE[15:8] = Addr[15:8];
	
	always@(posedge poke_wr or negedge nRESET) begin
		if (nRESET == 1'b0) begin
			border_char[0] <= 8'b00000000;
			border_char[1] <= 8'b00111100;
			border_char[2] <= 8'b01000010;
			border_char[3] <= 8'b01000010;
			border_char[4] <= 8'b01111110;
			border_char[5] <= 8'b01000010;
			border_char[6] <= 8'b01000010;
			border_char[7] <= 8'b00000000;
			bpattern_en <= 1'b0;
			border_ink <= 4'b1111;
			sfast_mode_en <= 1'b0;
			sfHR_en <= 1'b0;
			sfSP_en <= 1'b0;
			block0Writable <= 1'b0;		// reset: bloque 0 protegido (ROM)
			wrx_en <= 1'b0;
		end else begin
			// POKE 2041,ROMTABLE_low	-> set low part of ROMTABLE addr
			// POKE 2042,ROMTABLE_high	-> set high part of ROMTABLE addr
			// POKE 2043,HFILE_low		-> set low part of HFILE addr
			// POKE 2044,HFILE_high		-> set high part of HFILE addr
			// POKE 2045,170 				-> enable super fast text mode
			// POKE 2045,171 				-> enable super fast HiRes native mode
			// POKE 2045,172 				-> enable super fast HiRes spectrum mode
			// POKE 2045,85				-> disable super fast mode
			// POKE 2046,border_attr	-> change attributes for border 
			// POKE 2047,170 				-> enable border pattern
			// POKE 2047,85				-> disable border pattern
			// POKE 2048..2055			-> define border pattern (8 bytes)
//			if (Addr == 16'd2041) ROMTABLE[7:0] = data;
//			if (Addr == 16'd2042) ROMTABLE[15:8] = data;
			if (Addr == 16'd2043) HFILE[7:0] = data;
			if (Addr == 16'd2044) HFILE[15:8] = data;
			if ((Addr == 16'd2045) && (data==8'd170)) begin
				sfast_mode_en <= 1'b1;
				sfHR_en <= 1'b0;
				sfSP_en <= 1'b0;
			end;
			if ((Addr == 16'd2045) && (data==8'd171)) begin
				sfast_mode_en <= 1'b1;
				sfHR_en <= 1'b1;
				sfSP_en <= 1'b0;
			end
			if ((Addr == 16'd2045) && (data==8'd172)) begin
				sfast_mode_en <= 1'b1;
				sfSP_en <= 1'b1;
				sfHR_en <= 1'b0;
			end
			if ((Addr == 16'd2045) && (data==8'd85)) begin
				sfast_mode_en <= 1'b0;
				sfHR_en <= 1'b0;
				sfSP_en <= 1'b0;
			end
			if (Addr == 16'd2046) border_ink <= data[3:0];
			if ((Addr == 16'd2047) && (data==8'd170)) bpattern_en <= 1'b1;
			if ((Addr == 16'd2047) && (data==8'd85)) bpattern_en <= 1'b0;
			if ((Addr >= 16'd2048) && (Addr<2056)) border_char[Addr[2:0]] <= data;
			//if ((Addr == 16'd2047)) block0Writable <= 1'b0;
			if ((Addr == 16'd2056)) block0Writable <= 1'b1;	// CP/M: desproteger bloque 0
			// POKE 2058,170 -> WRX en 8-16K ON; POKE 2058,85 -> OFF
			if ((Addr == 16'd2058) && (data==8'd170)) wrx_en <= 1'b1;
			if ((Addr == 16'd2058) && (data==8'd85))  wrx_en <= 1'b0;
			// (POKE 2057 = double buffer: se decodifica en el bloque de
			//  control dbuf a system_clk, junto al pseudo-bloque 8 del mapper)
		end
	end
	
	wire isAttrMem = (nM1==1'b1)&&(nMREQ==1'b0) || (nRESET==1'b0);
		
	wire [7:0] shadowram_dout;
	reg [15:0] ROMTABLE = 16'h1c00;
	wire [7:0] v_dout;
	reg [15:0] v_addr;

	// --- double buffer: blit shadow->front por el puerto A (arbitrado con la CPU) ---
	wire cpu_sh_wr = isAttrMem & ~nWR & nRESET;					// escritura CPU en curso (puerto A ocupado)
	wire blit_we = blit_run & blit_phase & ~cpu_sh_wr;
	wire [15:0] blit_raddr = {HFILE[15:13], blit_cnt};			// origen: bloque shadow (HFILE)
	wire [15:0] blit_waddr = {front_blk, blit_cnt};				// destino: bloque front (BRAM privada)
	wire dbuf_wr_mask = dbuf_en & (Addr[15:13]==front_blk);	// front: enmascarar escrituras CPU en BRAM

	wire shadowram_we = ~nRESET?~nWRx: blit_we?1'b1: (isAttrMem & ~dbuf_wr_mask)? ~nWR:1'b0;
	wire [15:0] shadowram_addr = ~nRESET?Addrx[15:0]: blit_we?blit_waddr: nRFSH?Addr[15:0]:{6'b110000,char_latch[7],char_latch[5:0],line_cnt[2:0]};
	wire [7:0] shadowram_din = blit_we? v_dout: data;
	wire [8:0] SCR_START_Y = 62;
	wire [8:0] SCR_START_X = 122;
	wire [8:0] SCR_END_Y = SCR_START_Y+191;
	wire [8:0] SCR_END_X = SCR_START_X+33*8-1;
	
	reg [4:0] scr_row; 
	reg [4:0] scr_col;
	reg load_enable_fast;
	
	reg [4:0] scr_col_debug;
	reg [4:0] scr_row_debug;
	reg [7:0] char_latch_fast_cur;
	reg [15:0] char_latch_fast_addr_cur;
	reg [7:0] attr_latch_fast_cur;
	reg [15:0] attr_latch_fast_addr_cur;
	reg isborder_debug;
	reg isborder_cur;
	
	
	bramdp_w shadowram (
		.clka(system_clk),
		.wea(shadowram_we),
		.addra(shadowram_addr), 
		.dina(shadowram_din), 
		.douta(shadowram_dout),
		.clkb(system_clk),
		.web(1'b0),						// channel only for read
		.addrb(blit_run ? blit_raddr : v_addr),	// blit lee el shadow durante el blanking
		.doutb(v_dout)
	);

	// --- double buffer: FSM de blit (auto-present en cada blanking) ---
	// Copia 8KB {HFILE,offset} -> {front_blk,offset} en ~630us (2 ciclos/byte
	// a 26MHz + stalls por escrituras CPU). Ventana disponible: lineas 254..61
	// (~7.6ms). El video esta ocioso fuera del area activa, asi que el puerto B
	// es del blit; el puerto A se roba solo cuando la CPU no escribe.
	wire blit_start_line = (line_cnt == 9'd254);		// justo tras el area activa (SCR_END_Y=253)
	always @(posedge system_clk) begin
		old_blit_start <= blit_start_line;
		if (~dbuf_en | ~sfast_mode_en) blit_run <= 1'b0;
		else if (blit_start_line & ~old_blit_start) begin
			blit_run <= 1'b1;
			blit_cnt <= 13'd0;
			blit_phase <= 1'b0;
		end else if (blit_run) begin
			if (~blit_phase) blit_phase <= 1'b1;		// ciclo de direccion: doutb valido el siguiente
			else if (~cpu_sh_wr) begin						// puerto A libre: escribir y avanzar
				blit_phase <= 1'b0;
				blit_cnt <= blit_cnt + 1'b1;
				if (blit_cnt == 13'h1FFF) blit_run <= 1'b0;
			end
		end
	end
	
	wire [2:0] col_cnt_b = {pixel_cnt+(pixel_cnt>31)-SCR_START_X}[2:0];
	wire [2:0] line_cnt_b = {line_cnt - SCR_START_Y}[2:0];
	reg [4:0]scr_col1=0;
	reg [4:0]scr_col2=0;
	
	wire [12:0] hr_addr = {scr_row,line_cnt_b,scr_col1};
	wire [15:0] attr_addr_m0 = sfHR_en?{3'b110,hr_addr}:	// attribute area for superfast Hires native mode
										sfSP_en?{vpage,3'b110,scr_row,scr_col1}:	// attribute area for superfast spectrum mode (front si dbuf)
										{6'b110000,char_latch_fast[7],char_latch_fast[5:0],line_cnt_b}; // attr area for superfast text mode
										
	wire [15:0] attr_addr_m1 = {1'b1,{DFILE+16'd1+{scr_row,5'b00000} + scr_row+scr_col2}[14:0]};//16'hc0001+{scr_row,5'b00000} + scr_row+scr_col;
	
	wire [15:0] char_addr = DFILE+16'd1+{scr_row,5'b00000} + scr_row+scr_col;
	wire [15:0] scan_addr = sfHR_en? {vpage,hr_addr}:	// superfast HR native mode (front si dbuf)
									sfSP_en?	{vpage,hr_addr[12:11],hr_addr[7:5],hr_addr[10:8],hr_addr[4:0]}: // superfast HR spectrum mode (front si dbuf)
									{ROMTABLE[15:10],SEL_128CHARS?char_latch_fast[7]:ROMTABLE[9],char_latch_fast[5:0],line_cnt_b}; //superfast textmode
									
	reg [1:0] beeper_reg = 0;
	always@(negedge cs_SPULA or negedge nRESET)
	begin
		if (nRESET==0) begin
			sp_border = 3'h7;
			beeper_reg = 2'b0;
		end else begin 
			sp_border = data[2:0];
			beeper_reg = data[4:3];
		end
	end
	
	always@(posedge pixel_clk)
	begin
		if (pixel_cnt_debug==pixel_cnt)
		begin
			// 3 = low attr address
			// 4 = high attr address
			// 5 = low char address
			// 6 = high high address
			// 7 = scr_col 
			// 8 = scr_row 
			// 9 = attr
			// 10 = char
			// 11 = isborder
			scr_col_debug <= scr_col;
			scr_row_debug <= scr_row;
			char_latch_fast_debug <= char_latch_fast_cur;
			char_latch_fast_addr_debug <= char_latch_fast_addr_cur;
			attr_latch_fast_debug <= attr_latch_fast_cur;
			attr_latch_fast_addr_debug <= attr_latch_fast_addr_cur;
			isborder_debug <= isborder_cur;
		end
		if (((pixel_cnt-4) >= SCR_START_X) && ((pixel_cnt-4) <= SCR_END_X) && 
			(line_cnt >= SCR_START_Y) && (line_cnt <= SCR_END_Y))
		begin
			case (col_cnt_b)
				6: begin
				   // cambiar por 7:3 para eliminar el warning
					scr_row = {line_cnt-SCR_START_Y}[8:3];
					scr_col = {pixel_cnt+0-SCR_START_X}[8:3];
					scr_col1 = {pixel_cnt-6-SCR_START_X}[8:3];
					scr_col2 = {pixel_cnt-6-SCR_START_X}[8:3];
					v_addr = char_addr;
				end
				7: begin
					v_addr = char_addr;
					char_latch_fast  = v_dout;
					char_latch_fast_cur = v_dout;
					char_latch_fast_addr_cur = char_addr;
				end
				0: if (color_mode==1'b0) v_addr = attr_addr_m0;
					else v_addr = attr_addr_m1;
				1: begin
					if (color_mode==1'b0) v_addr = attr_addr_m0;
					else v_addr = attr_addr_m1;
					attr_latch_fast = v_dout;
					attr_latch_fast_cur = v_dout;
					attr_latch_fast_addr_cur = v_addr;
				end
				2: v_addr = scan_addr;
				3: begin
					v_addr = scan_addr;
					if (pixel_cnt >= SCR_START_X+12) load_enable_fast = 1'b1;
				end
				5: load_enable_fast = 1'b0;
			endcase
		end else begin
			char_latch_fast = 0;
		end
	end

	wire [1:0] int_state;
`ifdef TRACE
//	parameter trace_depth = 32;
//	parameter trace_width = 4;
//
//	reg sending_trace = 0;
//	reg trace_data_rdy = 0;
//	reg prev_micro_db_clk = 1;
//	reg [7:0] trace [0:trace_depth*trace_width-1];
//	reg [8:0] trace_pos = 0;
//	reg trace_mode = 0;
//	reg [15:0] trace_ini_addr = 0;
//	
//	wire trace_start = ~nM1 && ~nMREQ && int_out_en;  //(Addr ==  trace_ini_addr);
//	reg old_nRD = 0;
//	
//	always@(posedge nCLOCK or negedge nRESET)
//	begin
//		if (~nRESET) begin
//			trace_mode <= 0;
//			trace_pos <= 0;
//			sending_trace <= 0;
//			trace_data_rdy <= 0;
//		end else begin
//			old_nRD <= nRD;
//			if (old_nRD & ~nRD) begin
//				if (trace_start) begin
//					trace_mode <= 1;
//					trace[0] <= Addr[15:8];
//					trace[1] <= Addr[7:0];
//					trace[2] <= data;
//					trace[3] <= {nM1,nRD,nWR,nRFSH,nIORQ,int_state};
//					trace_pos <= trace_width;
//				end else if (~nMREQ & trace_mode) begin
//					trace[trace_pos] <= Addr[15:8];
//					trace[trace_pos+1] <= Addr[7:0];
//					trace[trace_pos+2] <= data;
//					trace[trace_pos+3] <= {nM1,nRD,nWR,nRFSH,nIORQ,int_state};
//					if(trace_pos>=trace_depth*trace_width) begin
//						trace_mode<=0;
//						sending_trace <= 1;
//						trace_pos <= 0;
//						prev_micro_db_clk <= ~MICRO_DB_CLK;
//						trace_data_rdy <= 1;
//					end else trace_pos <= trace_pos+trace_width;
//				end
//			end
//			if (sending_trace) begin
//				if (trace_pos >= trace_depth*trace_width) begin
//					sending_trace <= 0;
//					trace_pos <= 0;
//					trace_data_rdy <= 0;
//				end else	if (MICRO_DB_CLK!=prev_micro_db_clk) begin 
//					prev_micro_db_clk <= MICRO_DB_CLK;
//					debug_data <= trace[trace_pos];
//					trace_pos <= trace_pos+1;
//					trace_data_rdy <= 1;
//				end else trace_data_rdy <= 0;
//			end
//			debug_rdy_delayed[7:0] <= {debug_rdy_delayed[6:0], DEBUG_RDY};
//			if (debug_mode)
//			begin
//				// POKE 1024,x
//				// x = 0 -> DFILE low
//				// x = 1 -> DFILE high
//				// x = 2 -> char_latch fast addr low
//				// x = 3 -> char_latch fast addr high
//				// x = 4 -> char_latch fast
//				// x = 5 -> shift register fast addr low
//				// x = 6 -> shift register fast addr high
//				// x = 7 -> shift register fast 
//				if (Addr==1024)
//				begin
//					if (data==0) debug_data <= DFILE[7:0];
//					if (data==1) debug_data <= DFILE[15:8];
//					if (data==2) debug_data <= char_latch_fast_addr_debug[7:0];
//					if (data==3) debug_data <= char_latch_fast_addr_debug[15:8];
//					if (data==4) debug_data <= char_latch_fast_debug;
//					if (data==5) debug_data <= shift_register_fast_addr_debug[7:0];
//					if (data==6) debug_data <= shift_register_fast_addr_debug[15:8];
//					if (data==7) debug_data <= shift_register_fast_debug;
//					if (data==8) debug_data <= attr_latch_fast_debug;
//					if (data==9) debug_data <= attr_latch_fast_addr_debug[7:0];
//					if (data==10) debug_data <= attr_latch_fast_addr_debug[15:8];
//				end
//			end
//			// 1100/1 = pixel_cnt, 1102/3=line_cnt
//			if ((Addr==1100)&&(nMREQ==0)&&(nWR==0)) debug_param1 <= data;
//			if ((Addr==1101)&&(nMREQ==0)&&(nWR==0)) debug_param2 <= data;
//			if ((Addr==1102)&&(nMREQ==0)&&(nWR==0)) debug_param3 <= data;
//			if ((Addr==1103)&&(nMREQ==0)&&(nWR==0)) debug_param4 <= data;
//		end
//	end
//	
//	assign debug_mode = ((Addr==1024) &&(nMREQ==0)&&(nWR==0))||debug_capture_done;  
//	assign DEBUG_RDY = debug_mode || trace_data_rdy;
//	
//	assign debug_rdy_long = |(debug_rdy_delayed);
`else 
assign DEBUG_RDY = 1'b0;
`endif
	// char counters
	reg [5:0] nM1_clk_cnt = 0;
	reg nM1_falling;
	reg prev_nM1;
	always @(posedge system_clk)
	begin
		if ((nM1==0) && (prev_nM1==1))
		begin
			nM1_falling <= 1;
			prev_nM1 <= 0;
		end else begin
			if (nM1==1) prev_nM1 <= 1;
			nM1_falling <= 0;
		end
	end

	always @(posedge clk6_5 or posedge nM1_falling) 
	begin
		if (nM1_falling==1) nM1_clk_cnt <= 1'b0;
		else nM1_clk_cnt <= nM1_clk_cnt+1'b1;
	end
	
	wire m1_cycle = ~(nMREQ | nM1);
	reg m1cycle_delayed = 0;
	reg m1_cycle_T3 = 0;
	reg m1_cycle_T4 = 0;

	always@(posedge iclock)
	begin
		m1cycle_delayed <= m1_cycle;
	end
	
	always@(posedge iclock)
	begin
		m1_cycle_T3 <= m1cycle_delayed;
		m1_cycle_T4 <= m1_cycle_T3;
	end

//*************** nRFSH *******************************
//	wire M1clk = nM1 & iclock;
//	reg [1:0] rfsh_cnt;
//	always @(posedge M1clk or negedge nM1) begin
//		if (~nM1) rfsh_cnt <= 0;
//		else if (rfsh_cnt != 3) rfsh_cnt <= rfsh_cnt + 1'b0;
//	end  
//	wire rfsh = rfsh_cnt[1] ^ rfsh_cnt[0]; // CNT == 1 or CNT == 2
//	wire nrfsh = ~rfsh;
	
	// IO-READ-WRITE
	wire nIORD = nRD | nIORQ;
	wire nIOWR = nWR | nIORQ;

	reg NMIon = 0;
	
	wire hsync;
	wire nHSYNC = ~hsync;
	reg VSYNC_zx81 = 1;
	wire VSYNC_gen = line_cnt < 8;
	wire vsync = ~sfast_mode_en ? VSYNC_zx81 : VSYNC_gen;
	wire backporch;

	// HSYNC GENERATOR
	wire HSYNCcnt_reset = sfast_mode_en?1'b1:(nM1 | nIORQ);
	wire HSYNCcnt_clk =  ~iclock;
	
	always @(posedge pixel_clk or negedge HSYNCcnt_reset)
	begin
		if (HSYNCcnt_reset==0) pixel_cnt <= 0;
		else pixel_cnt <= (pixel_cnt == 9'd413)? 9'd0: pixel_cnt + 1'b1;	
	end
	
	assign hsync = (HSYNCcnt>=16) &&  (HSYNCcnt<=31);
	// VSYNC & NMI CONTROL
	wire nFE_port_RD = nIORD | A0;
	wire VSYNCset = ~(nFE_port_RD | NMIon);
	wire VSYNCreset = ~nIOWR;
	always @(posedge VSYNCset or posedge VSYNCreset)
	begin 
		if (VSYNCreset == 1) VSYNC_zx81 <= 0;
		else VSYNC_zx81 <= 1;
	end
	
	wire NMIset = ~A0 & ~nIOWR;
	wire NMIreset = ~A1 & ~nIOWR;
	always @(posedge NMIset or posedge NMIreset)
	begin
		if (NMIreset==1) NMIon  <= 0;
		else NMIon <= 1;
	end
	
	reg old_vsync;
	reg old_hsync;
	always @(posedge system_clk)
	begin
		old_vsync <= VSYNC_zx81;	
		if (old_vsync & ~VSYNC_zx81) begin
			if (~sfast_mode_en) line_cnt <= 9'b0;
		end else begin
			old_hsync <= hsync;
			if (~old_hsync & hsync)
			begin
				line_cnt <= (line_cnt == 9'd311)?9'b0:line_cnt + 9'b1; 
			end;
		end
	end

	// normal video
	always @(posedge system_clk)
	begin
		if (char_latch_enable)
		begin
			if (~NOP_en) begin
				char_latch <= data;
				attr_latch <= shadowram_dout;
			end
			forced_NOP_cycle <= forced_NOP_start;
		end
	end

	wire forced_NOP_T4 = forced_NOP_cycle & m1_cycle_T4;
	wire load_enable = forced_NOP_T4 & nMREQ;
	
	// border is active 7 pixelclocks after NOP cycle
	reg [7:0] forced_nop_delayed = 0;
	always@(posedge pixel_clk)
	begin
		forced_nop_delayed[0] <= forced_NOP_T4; 
		forced_nop_delayed[1] <= forced_nop_delayed[0]; 
		forced_nop_delayed[2] <= forced_nop_delayed[1]; 
		forced_nop_delayed[3] <= forced_nop_delayed[2]; 
		forced_nop_delayed[4] <= forced_nop_delayed[3]; 
		forced_nop_delayed[5] <= forced_nop_delayed[4]; 
		forced_nop_delayed[6] <= forced_nop_delayed[5]; 
		forced_nop_delayed[7] <= forced_nop_delayed[6]; 
	end
	
	reg isborder = 1;
	wire isborder_sp;
	reg [7:0] ishalt_delayed = 0;
	always@(posedge pixel_clk)
	begin
		ishalt_delayed[0] <= char_latch[6]; 
		ishalt_delayed[1] <= ishalt_delayed[0]; 
		ishalt_delayed[2] <= ishalt_delayed[1]; 
		ishalt_delayed[3] <= ishalt_delayed[2]; 
		ishalt_delayed[4] <= ishalt_delayed[3]; 
		ishalt_delayed[5] <= ishalt_delayed[4]; 
		ishalt_delayed[6] <= ishalt_delayed[5]; 
		ishalt_delayed[7] <= ishalt_delayed[6]; 
	end
	
	wire border_area = ~forced_nop_delayed[5];
	
	assign isborder_sp = !((pixel_cnt>=SCR_START_X+20) && (pixel_cnt<SCR_END_X+13) && 
					(line_cnt >=SCR_START_Y) && (line_cnt<=SCR_END_Y));
					
	wire sp_inv = inverse_video || (sfSP_en && flassing_period && current_attr [7]);
	wire [3:0] sp_paper = {current_attr [5:3]!=0?current_attr [6]:0,current_attr [5:3]};
	wire [3:0] sp_ink = {current_attr [2:0]!=0?current_attr [6]:0,current_attr [2:0]};
	
	wire isborder_b = sfSP_en? isborder_sp: isborder;
	wire inv_b = sfSP_en? sp_inv: inverse_video;

	wire [4:0] attr_cond = {sfSP_en,sfast_mode_en,isborder_b,bpattern_en,inv_b};
	reg [7:0] attr_o;
	always@(posedge pixel_clk) begin
		casex (attr_cond) 
			// native/superfast mode paper area
			5'b0x0x0: attr_o <= {current_attr [3:0], 	current_attr [7:4]}; // normal
			5'b0x0x1: attr_o <= {current_attr [7:4], 	current_attr [3:0]};	// inverse
			// native/superfast mode border area
			5'b0x10x: attr_o <= {border_color, 			border_color};			// no pattern
			5'b0x110: attr_o <= {border_ink, 			border_color};			// pattern, normal
			5'b0x111: attr_o <= {border_ink, 			border_color};			// pattern, inverse
			// spectrum mode paper area
			5'b1x0x0: attr_o <= {sp_ink, 					sp_paper}; 				// normal
			5'b1x0x1: attr_o <= {sp_paper, 				sp_ink};					// inverse
			// spectrum mode border area
			5'b1x10x: attr_o <= {sp_border, 				sp_border};				// no pattern
			5'b1x110: attr_o <= {border_ink, 			sp_border};				// pattern, normal
			5'b1x111: attr_o <= {border_ink, 			sp_border};				// pattern, inverse
		endcase
	end
	assign {ink_active,paper_active} = attr_o;
						
	
	reg [7:0] current_attr;
	always @(posedge pixel_clk)
	begin
		if (~sfast_mode_en)											// standard zx81 video mode
		begin	
			serial_output <= inverse_video?shift_register[7]:~shift_register[7];
			shift_register <= {shift_register[6:0],1'b0};
			old_load_enable <= load_enable;	
			if (~old_load_enable & load_enable) begin
				if (char_latch[7]) shift_register <= ~data;
				else shift_register <= data;
				if (color_mode==1'b0) current_attr <= shadowram_dout;
				else current_attr <= attr_latch;
				isborder <= 0; 
				border_pixel_cnt <= 1;
			end else begin
				if (ishalt_delayed[5]) begin
					isborder <= 1;
				end
				if (bpattern_en) begin
					if ((isborder || ishalt_delayed[5]) && (border_pixel_cnt==3'b000))
						shift_register <= border_char[line_cnt[2:0]];
				end
				if (HSYNCcnt==16'd31) border_pixel_cnt <= border_pixel_cnt + 2'd2;
				else border_pixel_cnt <= border_pixel_cnt + 1'd1;
			end
		end
		// super fast mode
		else begin
			serial_output <= inverse_video?shift_register[7]:~shift_register[7];
			shift_register <= {shift_register[6:0],1'b0};
			old_load_enable_fast <= load_enable_fast;	
			if (~old_load_enable_fast & load_enable_fast) begin
				current_attr <= attr_latch_fast;
				
				if (char_latch_fast[7] && !sfSP_en && !sfHR_en) shift_register <= ~v_dout;	// inverse solo en TEXTO; en Spectrum/HiRes v_dout es pixel raw
				else shift_register <= v_dout;
				border_pixel_cnt <= 0;
			end else begin
				if (bpattern_en) begin
					if ((isborder_sp/* || ishalt_delayed[5]*/) && (border_pixel_cnt==3'b111))
						shift_register <= border_char[line_cnt_b[2:0]];
				end
				if (HSYNCcnt==16'd31) border_pixel_cnt <= border_pixel_cnt + 2'd2;
				else border_pixel_cnt <= border_pixel_cnt + 1'd1;
			end
		end
	end
	
	assign backporch = HSYNCcnt >=  32 && HSYNCcnt <= 48; //HSYNCcnt[7:4]==7'b0010;
	wire video = serial_output; //~sfast_mode_en?serial_output:serial_output_fast;
	wire luminance =  ~(hsync | vsync );
	
	wire cred = ~video? ink_active[1]: paper_active[1];
	wire cgreen = ~video? ink_active[2]: paper_active[2];
	wire cblue = ~video? ink_active[0]: paper_active[0];
	wire cbright = ~video? ink_active[3]: paper_active[3];
//	wire cred = ~video;
//	wire cgreen = ~video;
//	wire cblue = ~video;
//	wire cbright = ~video;

	wire ncsync = ~vsync&~hsync&~backporch;
	assign VRED = color_enable? cred &ncsync: video&ncsync;
	assign VGREEN = color_enable? cgreen&ncsync: video&ncsync;
	assign VBLUE = color_enable? cblue&ncsync: video&ncsync;
	assign BRIGHT = color_enable? cbright&ncsync:1'b0&ncsync;
	
	assign CSYNC = luminance;
/********************************************/


// ***************************************************
//    INTERRUPTS SIMULATION
// ***************************************************

	wire enable_int = ~nMREQ&&~nWR&&(Addr==16'd2040)&&D0;
	wire disable_int = ~nMREQ&&~nWR&&(Addr==16'd2040)&&~D0;
	reg [15:0] int_addr = 16514;
	wire [7:0] int_data_out;
	wire int_out_en;
	wire int_enabled;
	wire int_dataout_en = int_out_en & ~nRD & ~nMREQ;
	reg int_mode = 1'b0;
	wire int_signal;
	wire vborder = !((line_cnt >=SCR_START_Y) && (line_cnt<=SCR_END_Y));

	assign int_signal = (int_mode==1'b0)? vsync:vborder;

	wire poke_wr_int = (nMREQ==1'b0) && (nWR==1'b0) && (Addr >= 16'd2038) && (Addr <= 16'd2039);

	// POKE 2038,int_addr_low	-> set low part of la rutina de interrupciones simuladas
	// POKE 2039,int_addr_high	-> set high part de la rutina de interrupciones simuladas
	// POKE 2040,1/0			-> enable_int / disable_int (interrupciones simuladas SUPERFAST/SPECTRUM)
	always@(posedge iclock or negedge nRESET) begin
		if (nRESET == 1'b0) begin
			int_mode <= 0;
		end else if (poke_wr_int) begin
			if (Addr == 16'd2038) int_addr[7:0] <= data;
			if (Addr == 16'd2039) int_addr[15:8] <= data;
		end
	end
	
	
	sim_int sim_int_inst (
		.clk(system_clk),
		.nreset(nRESET),
		.enable_int(enable_int),
		.disable_int(disable_int),
		.superfast_mode(sfast_mode_en),
		.int_addr(int_addr),
		.addr(Addr),
		.nRD(nRD),
		.nM1(nM1),
		.nMREQ(nMREQ),
		.data_out(int_data_out),
		.enable_out(int_out_en),
		.enabled(int_enabled),
		.state(int_state)
	);
		
	
// ***************************************************
//		MEMORY MAPPER
// ***************************************************
		wire mapper_port_wr = A7&A6&A5&~A4&~A3&A2&A1&A0&~nIORQ&~nWR;
		wire mapper_port_rd = A7&A6&A5&~A4&~A3&A2&A1&A0&~nIORQ&~nRD;
	 
		reg [5:0] block[0:7];
		reg [2:0] rowcnt;
		reg [6:0] ram_data_latch;
		
		// Pseudo-bloque 8 (data=08h, D3=1) = control del double buffer por puerto.
		// Solo con FULL_PAGING o tras $2056 (block0Writable), para no colisionar
		// con escrituras legitimas de half paging (pagina impar -> bloque 0
		// comparte el patron x8h).
		wire dbuf_port_wr = mapper_port_wr && (D3&~D2&~D1&~D0) && (FULL_PAGING || block0Writable);

		// mapper port
		always @(posedge mapper_port_wr or negedge nRESET)
		begin
			if (nRESET==1'b0)
			begin
				block[0] <= 6'd0;
				block[1] <= 6'd1;
				block[2] <= 6'd2;
				block[3] <= 6'd3;
				block[4] <= 6'd4;
				block[5] <= 6'd5;
				block[6] <= ~nMODE48K?6'd6:6'd2;
				block[7] <= ~nMODE48K?6'd7:6'd3;
			end else begin
				// pseudo-bloque 8: no tocar la tabla de bloques
				if (!((D3&~D2&~D1&~D0) && (FULL_PAGING || block0Writable)))
					block[{D2,D1,D0}] <= FULL_PAGING ? {A13,A12,A11,A10,A9,A8} : {1'b0,D7,D6,D5,D4,D3};
			end
		end

		wire [7:0] mapper_data = block[{A10,A9,A8}];

		// --- DOUBLE BUFFER: registro de control (unico driver de dbuf_en/front_blk) ---
		// Dos vias de escritura:
		//  a) POKE 2057 via MMIO (muere tras $2056):  168+blk = ON, 85 = OFF
		//  b) pseudo-bloque 8 del mapper: OUT (C),A con A=08h y B=valor
		//     valor: bit5 (32) = enable, bits2:0 = front_blk;  B=0 = OFF
		reg old_poke2057 = 1'b0;
		reg old_dbufport = 1'b0;
		wire poke2057 = poke_wr && (Addr == 16'd2057);
		always @(posedge system_clk) begin
			old_poke2057 <= poke2057;
			old_dbufport <= dbuf_port_wr;
			if (~nRESET) begin
				dbuf_en <= 1'b0;
			end else begin
				if (poke2057 & ~old_poke2057) begin
					if (data[7:3]==5'b10101) begin
						dbuf_en   <= 1'b1;
						front_blk <= data[2:0];
					end
					if (data==8'd85) dbuf_en <= 1'b0;
				end
				if (dbuf_port_wr & ~old_dbufport) begin
					if (A13) begin							// bit5 del valor (B) = enable
						dbuf_en   <= 1'b1;
						front_blk <= {A10,A9,A8};		// bits2:0 del valor = front_blk
					end else dbuf_en <= 1'b0;
				end
			end
		end
// ************************************************
// 	CHAR GENERATOR
// ************************************************
		wire row_rst;
		wire sync;
		reg sync_clr;
		reg IC19B;
		reg IC18A;
		
// ************************************************
// 	MC45/M1NOT
// ************************************************
		wire M1NOT_signal = ~nM1 & ~nMREQ & ~nRD & A15 & ~A14;
		assign nHALT = M1NOT_signal & EN_MC45? 1'b0: 1'bz;


// ************************************************
// 	ADDRESSING
// ************************************************

	reg old_vsync2;
	always@(posedge nCLOCK or negedge nRESET)
	begin
		if (nRESET == 0) begin
			DFILE <= 0;
			FRAMES <= 0;
		end else begin
			if ((nMREQ==0) && (nWR==0))
			begin
				if (Addr==DFILE_addr) DFILE[7:0] <= data;
				if (Addr==(DFILE_addr+1)) DFILE[15:8] <= data;
				if (Addr==FRAMES_addr) FRAMES[7:0] <= data;
				if (Addr==(FRAMES_addr+1)) FRAMES[15:8] <= data;
			end;
			old_vsync2 <= vsync;
			if (~old_vsync2 & vsync & sfast_mode_en) FRAMES<={FRAMES[15],FRAMES[14:0]-15'b1};
		end
	end

	wire[7:0] ram_Dlatch = char_latch[7:0];
		// lines are directly connected except for the case /nRESET==0
	assign {A12x,A11x,A10x} = 
		~nRESET?3'bzzz:				// access to RAM from micro on boot
		~nQS_en & ~nRFSH? 3'b001:	// access to char table on QS mode
		{A12,A11,A10}; 				// normal access
	assign {A9x,A8x,A7x,A6x,A5x,A4x,A3x,A2x,A1x,A0x} =
		~nRESET?10'bzzzzzzzzzz:																						// access to RAM from micro on boot
		(nQS_en & (nRFSH | A14 | A15 | (wrx_en & A13)) || (~nQS_en & nRFSH))? {A9,A8,A7,A6,A5,A4,A3,A2,A1,A0}:  // normal access (wrx_en: WRX con I en $20-$3F, POKE 2058)
		(~nQS_en & ~nRFSH)||SEL_128CHARS?{ram_Dlatch[7],ram_Dlatch[5:0],line_cnt[2:0]}:			// access  to char table on 128CHAR or QS mode
		{A9,ram_Dlatch[5:0],line_cnt[2:0]};																		// access to char table on 64CHAR mode
	assign {A18x,A17x,A16x,A15x,A14x,A13x} = ~nRESET?6'bzzzzzz: // access to RAM from micro on boot
		(~nQS_en & ~nRFSH)?block[3'b100]:								// access to char table on QS mode
		(~nMODE48K &~nM1 & A15 & A14)? block[{1'b0,A14,A13}]:		// access to execute at C000-FFFF in 48K mode
		block[{ A15,A14,A13}];												// normal access

		// LOW ROM is write protected
		assign nWRx =~nRESET?1'bz:(nWR | nMREQ | ((~A13&~A14&~A15)&~block0Writable) );
		
		//external MEM active for RAM and ROM
		assign nMEM_OE = ~nRESET?nMEM_OEm:int_dataout_en?1'b1:&nRD&nRFSH|nMREQ&nRFSH;
		
// ************************************************
//		AY-8912
// ************************************************

	// decoding scheme *x0xc11x  * reg/data, c = chip number
	// distinto de A7, AF y E7
	// A7=selector reg/data, A3 selector de chip

//--   BDIR  BC1     State
//--     0    0    Inactive
//--     0    1    Read from PSG
//--     1    0    Write to PSG
//--     1    1    Latch address
	wire [1:0] ay_cs;
	wire [1:0] ay_bc1;
	wire [1:0] ay_bdir;
	wire [1:0] ay_port[0:1];
	wire ay_port_access;
	assign ay_port_access = (~A5&A2&A1) && ~(nIORQ | (nWR & nRD));
	wire [1:0] ay_psg_read;
	wire [1:0] ay_psg_write;
	wire [1:0] ay_latch;
	
	wire [13:0] pcm14s[0:1];
	wire clock_ym2149 = cnt26[3:0] == 0;
	wire signed [13:0] cha_s[0:1];
	wire signed [13:0] chb_s[0:1];
	wire signed [13:0] chc_s[0:1];
	wire [11:0] ay_cha_o[0:1];
	wire [11:0] ay_chb_o[0:1];
	wire [11:0] ay_chc_o[0:1];	
	wire [7:0] ay_data_o[0:1];
	wire [1:0] ay_oe_n;
	wire [7:0] port_a_dout[0:1];
	wire [1:0] port_a_oe_n;
	genvar j;
	generate
		for (j=0;j<=1;j=j+1) begin: aychips
			assign ay_cs[j] = ay_port_access && (A3 == j);
			assign ay_bc1[j] = A7 & ay_cs[j];
			assign ay_bdir[j] = nRD & ay_cs[j];
			assign ay_psg_read[j] 	= ~ay_bdir[j] &  ay_bc1[j];
			assign ay_psg_write[j] 	=  ay_bdir[j] & ~ay_bc1[j];
			assign ay_latch[j] 		=  ay_bdir[j] &  ay_bc1[j];
			ay_3_8192 ay(
			  .clk(system_clk),
			  .clken(clock_ym2149),
			  .rst_n(nRESET),
			  .a8(1'b1),
			  .bdir(ay_bdir[j]),
			  .bc1(ay_bc1[j]),
			  .bc2(1'b1),
			  .din(data),
			  .dout(ay_data_o[j]),
			  .oe_n(ay_oe_n[j]),
			  .channel_a(cha_s[j]),
			  .channel_b(chb_s[j]),
			  .channel_c(chc_s[j]),
			  .port_a_din(8'd0),
			  .port_a_dout(port_a_dout[j]),
			  .port_a_oe_n(port_a_oe_n[j])
			  );
		end
	endgenerate	
	
	wire signed [12:0] beeper_dout;
	
	beeper beeper_inst(
		.clk(clock_ym2149),
		.nreset(nRESET),
		.cs(cs_SPULA),
		.data_in(data[4:3]),
		.data_out(beeper_dout)
	);
	
	wire bclk;
	wire lrclk;
	wire sdata;
	
//	wire signed [15:0] cha_s[0:1];// = {4'd0, ay_cha_o[0]} - 16'd2048;
//	wire signed [15:0] chb_s[0:1];// = {4'd0, ay_chb_o[0]} - 16'd2048;
//	wire signed [15:0] chc_s[0:1];// = {4'd0, ay_chc_o[1]} - 16'd2048;
	
//	assign cha_s[0] = $signed(ay_cha_o[0]);
//	assign cha_s[1] = $signed(ay_cha_o[1]);
//	assign chb_s[0] = $signed(ay_chb_o[0]);
//	assign chb_s[1] = $signed(ay_chb_o[1]);
//	assign chc_s[0] = $signed(ay_chc_o[0]);
//	assign chc_s[1] = $signed(ay_chc_o[1]);
//	assign cha_s[0] = {4'd0, ay_cha_o[0]};
//	assign cha_s[1] = {4'd0, ay_cha_o[1]};
//	assign chb_s[0] = {4'd0, ay_chb_o[0]};
//	assign chb_s[1] = {4'd0, ay_chb_o[1]};
//	assign chc_s[0] = {4'd0, ay_chc_o[0]};
//	assign chc_s[1] = {4'd0, ay_chc_o[1]};
	

//	// Mezcla de canales y centrado en cero
	wire signed [15:0] beeper_s = beeper_dout;	// extension de signo 13->16 bits
												// (dentro de {} la concatenacion es unsigned y
												// los valores negativos del beeper se corrompian)
	wire signed [15:0] sample_l = {cha_s[0] + chc_s[0] + chc_s[1] + cha_s[1]+beeper_s+15'sd0};
	wire signed [15:0] sample_r = {chb_s[0] + chc_s[0] + chc_s[1] + chb_s[1]+beeper_s+15'sd0};

	i2s_tx DAC(
		.clk(system_clk),
		.rst(~nRESET),
		.sample_l(sample_l),
		.sample_r(sample_r),
		.sample_valid(1'b1),
		.bclk(bclk),
		.lrclk(lrclk),
		.sdata(sdata)
	);
	
	assign DIN = sdata;
	assign BCK = bclk;
	assign LRCK = lrclk;
//	assign test_NMI = int_enabled;
//	assign test2 = nM1;
//	assign test1 = int_out_en;
	
	

// ************************************************
// 	COMMS WITH MICROCONTROLER
// ************************************************
 
		reg DATA_REG = 1'b0;			// Data LATCH ready
///		reg DATA_REG_in = 1'b0;
		reg CTRL_REG = 1'b0;			// CTRL LATCH ready
		wire port_CTRL_REG;			// Control I/O Port
		wire port_DATA_REG;			// Data I/O Port
		wire DATA_in_out;				// in/out 0xA7 detected
		
		wire DATA_out;					// out 0xA7 detected
	   wire DATA_in;					// in 0xA7 detected
		 
		wire CTRL_out;					// out 0xAF detected
		wire CTRL_in;					// in 0xA7 detected
			 
		assign port_CTRL_REG = A7 & ~A6 & A5 & ~A4 & A3 & A2 & A1 & ~nIORQ ;  	// 0xAF
		assign port_DATA_REG = A7 & ~A6 & A5 & ~A4 & ~A3 & A2 & A1 & ~nIORQ ;  	// 0xA7
		
		assign DATA_out = port_DATA_REG  & ~nWR;
		assign DATA_in  = port_DATA_REG  & ~nRD;
		assign DATA_in_out = DATA_in | DATA_out;
		 
		assign CTRL_out = port_CTRL_REG  & ~nWR;
		assign CTRL_in  = port_CTRL_REG  & ~nRD;

		// D Reg
		// Reset = nRST_CTRL_REG
		// CLK =  A7 & A5 & A2 & A1 & !nWR & !IORQ & !A6 & !A4 & A3
		// D = Vcc
		always @(posedge CTRL_out or negedge nRST_CTRL_REG) begin  
			if (nRST_CTRL_REG==1'b0)
				CTRL_REG = 1'b0;
			else
				CTRL_REG = 1'b1;
		end 

		// D Reg
		// Reset = nRST_DATA_REG
		// CLK =  A7 & A5 & A2 & A1 & !nWR & !IORQ & !A6 & !A4 & !A3
		// D = Vcc
		always @(posedge DATA_in_out or negedge nRST_DATA_REG) begin
			if (nRST_DATA_REG==1'b0)
				DATA_REG = 1'b0;
			else
				DATA_REG = 1'b1;
		end
			
		wire LE_OL;					// Latch Enable Output Latch
		wire nOE_IL;					// Output Enable Input Latch

		// OUTPUT LATCH FROM DATABUS TO MICRO
		reg [7:0] output_latch = 0;
		always@(posedge LE_OL) begin
			output_latch <= data;
		end

		// INPUT LATCH FROM MICRO TO DATABUS 
		wire [7:0] input_latch = {O7,O6,O5,O4,O3,O2,O1,O0};

		assign {B7,B6,B5,B4,B3,B2,B1,B0} = debug_rdy_long? debug_data:micro_rd? data:!nOE_OL? output_latch: 8'bz;
		
		
		//* CONFIGURATION STRING
		reg [MAX_CFG:0] cfg_reg = 0;
		reg [5:0] cfg_cnt;
		wire [CMD_BITS-1:0]comm_cmd = cfg_reg[CMD_BITS-1:0];
		reg [29:0] joy_cnf = {
			3'd4,3'd3, 
			3'd4,3'd4, 
			3'd3,3'd4, 
			3'd4,3'd2, 
			3'd4,3'd0};
		wire [2:0] row_UP 	= joy_cnf[2:0]; 	//{4}
		wire [2:0] col_UP 	= joy_cnf[5:3];   //{3}; 
		wire [2:0] row_DOWN 	= joy_cnf[8:6];   //{4};
		wire [2:0] col_DOWN	= joy_cnf[11:9];  //{4};
		wire [2:0] row_LEFT	= joy_cnf[14:12]; //{4};
		wire [2:0] col_LEFT	= joy_cnf[17:15]; //{2};
		wire [2:0] row_RIGHT	= joy_cnf[20:18]; //{3};
		wire [2:0] col_RIGHT	= joy_cnf[23:21]; //{4};
		wire [2:0] row_FIRE 	= joy_cnf[26:24]; //{4};
		wire [2:0] col_FIRE	= joy_cnf[29:27]; //{0};
		
		reg [8:0]pixel_cnt_debug = 0;
		reg [8:0]line_cnt_debug = 0;
		reg [CMD_BITS-1:0] cmd_debug = 0;
		reg cmd_debug_rdy = 1;
		

		always @(posedge CFG_CLK or negedge CFG_RESET) begin
			if (CFG_RESET==0) begin
				cfg_cnt <= 0;
				if 		(comm_cmd==0) joy_cnf <= cfg_reg[MAX_CFG:CMD_BITS]; 	// JOYSTICK CONFIGURATION
				else if 	(comm_cmd==1) EN_MC45 <= cfg_reg[CMD_BITS];				// MC45 alias M1NOT
				else if 	(comm_cmd==2) nMODE48K <= cfg_reg[CMD_BITS];				// 1=32K mode - 0=48/56 mode
				else if 	(comm_cmd==3) nQS_en <= cfg_reg[CMD_BITS];				// Quick Silva 1=disabled, 0=enabled
				else if 	(comm_cmd==4) FULL_PAGING <= cfg_reg[CMD_BITS];		// 1=HIGHER HALF RAM, 0=LOWER HALF RAM			end else begin
				else if 	(comm_cmd==5) SEL_128CHARS <= cfg_reg[CMD_BITS];		// 1=HIGHER HALF RAM, 0=LOWER HALF RAM			end else begin
			end else begin
				cfg_reg[cfg_cnt]<=CFG_DATA;
				if (cfg_cnt < MAX_CFG) cfg_cnt <= cfg_cnt+1'b1;
				else cfg_cnt <= 1'b0;
			end
		end
		
		//* JOYSTICK CONTROL
		wire kbdint = ~nIORQ & ~nRD & ~A0;
		wire [15:8] A = {A15,A14,A13,A12,A11,A10,A9,A8};
		reg [4:0] kbd_data = 5'b11111;
		
		integer i;
		always@(posedge kbdint) begin
			kbd_data = 5'b11111;
			for (i=7;i>=0;i=i-1) begin
				if (!A[8+i]) begin
					if (!UP && (row_UP==i)) kbd_data = ~(kbd_data & (1'b1 << col_UP));
					if (!DOWN && (row_DOWN==i)) kbd_data = ~(kbd_data & (1'b1 << col_DOWN));
					if (!LEFT && (row_LEFT==i)) kbd_data = ~(kbd_data & (1'b1 << col_LEFT));
					if (!RIGHT && (row_RIGHT==i)) kbd_data = ~(kbd_data & (1'b1 << col_RIGHT));
					if (!FIRE && (row_FIRE==i)) kbd_data = ~(kbd_data & (1'b1 << col_FIRE));
				end
			end
		end

		assign LE_OL = DATA_out; 
		assign nOE_IL = ~DATA_in;
		
		wire nOE_CTRL_CLOCK = ~CTRL_in; // Enable D7 if in 0xAF detected

		assign GET_CTRL_REG = CTRL_REG;

		assign GET_DATA_REG = DATA_REG;

		// VSYNC edge counter for port $AF (D6..D1)
		reg [5:0] vsync_cnt = 6'b0;
		reg vsync_af_prev   = 1'b0;
		reg ctrl_in_prev    = 1'b0;

		always @(posedge system_clk) begin
			vsync_af_prev <= vsync;
			ctrl_in_prev  <= CTRL_in;
			if (ctrl_in_prev & ~CTRL_in)        // flanco bajada de lectura $AF: reset
				vsync_cnt <= 6'b0;
			else if (vsync & ~vsync_af_prev)    // flanco subida de VSYNC: incrementa
				vsync_cnt <= vsync_cnt + 1'b1;
		end
		
		wire kbd_data_out =  {1'bz,1'bz,1'bz,
			!kbd_data[4]?0:1'bz,
			!kbd_data[3]?0:1'bz,
			!kbd_data[2]?0:1'bz,
			!kbd_data[1]?0:1'bz,
			!kbd_data[0]?0:1'bz};

//		assign {D7,D6,D5,D4,D3,D2,D1,D0} = 
//			lFRAMES_read?FRAMES[7:0]:
//			hFRAMES_read?FRAMES[15:8]:
//			ay_psg_read[0]?ay_data_o[0]:
//			ay_psg_read[1]?ay_data_o[1]:
//			micro_wr?{O7,O6,O5,O4,O3,O2,O1,O0}:
//			micro_rd?8'bz:
//			~nOE_CTRL_CLOCK? {CTRL_CLK,1'b0,1'b0,1'b0,1'b0,1'b0,CTRL_CLK,int_signal}:
//			!nOE_IL? input_latch :
//			mapper_port_rd? mapper_data: 
//			kbdint ? kbd_data_out:
//			chroma_mode_rd? 8'bzz0zzzzz:					// Colour modes availables, chroma switch 6 allways on
//			int_dataout_en? int_data_out:
//			8'bz;

		always @(*) begin
			if (lFRAMES_read || hFRAMES_read || ay_psg_read[0] || ay_psg_read[1] ||
				micro_wr) data_dir <= 1'b0;
			else if (micro_rd) data_dir<= 1'b1;
			else if (~nOE_CTRL_CLOCK || !nOE_IL || mapper_port_rd || kbdint || 
				chroma_mode_rd || int_dataout_en) data_dir <= 1'b0;
			else data_dir<= 1'b1;
		end
		
			
		assign D7 = 
						!nOE_IL? input_latch[7] :
						~nOE_CTRL_CLOCK? CTRL_CLK:
						lFRAMES_read?FRAMES[7]:
						hFRAMES_read?FRAMES[15]:
						ay_psg_read[0]?ay_data_o[0][7]:
						ay_psg_read[1]?ay_data_o[1][7]:
						micro_wr?O7:
						micro_rd?1'bz:
						mapper_port_rd? mapper_data[7] : 
						int_dataout_en? int_data_out[7]:
						1'bz;
		assign D6 =
						!nOE_IL? input_latch[6] :
						~nOE_CTRL_CLOCK? vsync_cnt[5]:
						lFRAMES_read?FRAMES[6]:
						hFRAMES_read?FRAMES[14]:
						ay_psg_read[0]?ay_data_o[0][6]:
						ay_psg_read[1]?ay_data_o[1][6]:
						micro_wr?O6:
						micro_rd?1'bz:
						mapper_port_rd? mapper_data[6] : 
						int_dataout_en? int_data_out[6]:
						1'bz;
		assign D5 =
						!nOE_IL? input_latch[5] :
						~nOE_CTRL_CLOCK? vsync_cnt[4]:
						lFRAMES_read?FRAMES[5]:
						hFRAMES_read?FRAMES[13]:
						ay_psg_read[0]?ay_data_o[0][5]:
						ay_psg_read[1]?ay_data_o[1][5]:
						micro_wr?O5:
						micro_rd?1'bz:
						mapper_port_rd? mapper_data[5] : 
						chroma_mode_rd? 1'b0:					// Colour modes availables, chroma switch 6 allways on
						int_dataout_en? int_data_out[5]:
						1'bz;
		assign D4 =
						!nOE_IL? input_latch[4] :
						~nOE_CTRL_CLOCK? vsync_cnt[3]:
						lFRAMES_read?FRAMES[4]:
						hFRAMES_read?FRAMES[12]:
						ay_psg_read[0]?ay_data_o[0][4]:
						ay_psg_read[1]?ay_data_o[1][4]:
						micro_wr?O4:
						micro_rd?1'bz:
						mapper_port_rd? mapper_data[4] : 
						kbdint & !kbd_data[4]?  1'b0:
						int_dataout_en? int_data_out[4]:
						1'bz;
		assign D3 =
						!nOE_IL? input_latch[3] :
						~nOE_CTRL_CLOCK? vsync_cnt[2]:
						lFRAMES_read?FRAMES[3]:
						hFRAMES_read?FRAMES[11]:
						ay_psg_read[0]?ay_data_o[0][3]:
						ay_psg_read[1]?ay_data_o[1][3]:
						micro_wr?O3:
						micro_rd?1'bz: 
						mapper_port_rd? mapper_data[3] : 
						kbdint & !kbd_data[3]?  1'b0:
						int_dataout_en? int_data_out[3]:
						1'bz;
		assign D2 =
						!nOE_IL? input_latch[2] :
						~nOE_CTRL_CLOCK? vsync_cnt[1]:
						lFRAMES_read?FRAMES[2]:
						hFRAMES_read?FRAMES[10]:
						ay_psg_read[0]?ay_data_o[0][2]:
						ay_psg_read[1]?ay_data_o[1][2]:
						micro_wr?O2:
						micro_rd?1'bz:
						mapper_port_rd? mapper_data[2] : 
						kbdint & !kbd_data[2]?  1'b0:
						int_dataout_en? int_data_out[2]:
						1'bz;
		assign D1 =
						!nOE_IL? input_latch[1] :
						~nOE_CTRL_CLOCK? vsync_cnt[0]:
						lFRAMES_read?FRAMES[1]:
						hFRAMES_read?FRAMES[9]:
						ay_psg_read[0]?ay_data_o[0][1]:
						ay_psg_read[1]?ay_data_o[1][1]:
						micro_wr?O1:
						micro_rd?1'bz:
						mapper_port_rd? mapper_data[1] : 
						kbdint & !kbd_data[1]?  1'b0:
						int_dataout_en? int_data_out[1]:
						1'bz;

		assign D0 =
						!nOE_IL? input_latch[0] :
						~nOE_CTRL_CLOCK? int_signal:
						lFRAMES_read?FRAMES[0]:
						hFRAMES_read?FRAMES[8]:
						ay_psg_read[0]?ay_data_o[0][0]:
						ay_psg_read[1]?ay_data_o[1][0]:
						micro_wr?O0:
						micro_rd?1'bz:
						mapper_port_rd? mapper_data[0] : 
						kbdint & !kbd_data[0]?  1'b0:
						int_dataout_en? int_data_out[0]:
						1'bz;
						
//		reg D0_en;
//		reg D0_reg = 0;
//		always@(*)begin
//			if		  (!nOE_IL)	begin
//				D0_reg <= input_latch[0];
//				D0_en <= 1'b1;
//			end else if (~nOE_CTRL_CLOCK) begin
//				D0_reg <= int_signal;
//				D0_en <= 1'b1;
//			end else if (lFRAMES_read) begin
//				D0_reg <= FRAMES[0];
//				D0_en <= 1'b1;
//			end else if (hFRAMES_read) begin
//				D0_reg <= FRAMES[8];
//				D0_en <= 1'b1;
//			end else if (ay_psg_read[0]) begin
//				D0_reg <= ay_data_o[0][0];
//				D0_en <= 1'b1;
//			end else if (ay_psg_read[1]) begin
//				D0_reg <= ay_data_o[1][0];
//				D0_en <= 1'b1;
//			end else if (micro_wr) begin
//				D0_reg <= O0;
//				D0_en <= 1'b1;
//			end else if (micro_rd) begin
//				D0_reg <= 1'bz;
//				D0_en <= 1'b0;
//			end else if (mapper_port_rd) begin
//				D0_reg <= mapper_data[0]; 
//				D0_en <= 1'b1;
//			end else if (kbdint & !kbd_data[0])	begin
//				D0_reg <= 1'b0;
//				D0_en <= 1'b1;
//			end else if (int_dataout_en) begin
//				D0_reg <= int_data_out[0];
//				D0_en <= 1'b1;
//			end else begin
//				D0_reg <= 1'bz;
//				D0_en <= 1'b1;
//			end
//		end
//		
//		assign D0 = D0_en? D0_reg : 1'bz;


//		assign D0 = 
//						!nOE_IL? input_latch[0] :
//						~nOE_CTRL_CLOCK? int_signal:
//						lFRAMES_read?FRAMES[0]:
//						hFRAMES_read?FRAMES[8]:
//						ay_psg_read[0]?ay_data_o[0][0]:
//						ay_psg_read[1]?ay_data_o[1][0]:
//						micro_wr?O0:
//						micro_rd?1'bz:
//						mapper_port_rd? mapper_data[0] : 
//						kbdint & !kbd_data[0]?  1'b0:
//						int_dataout_en? int_data_out[0]:
//						1'bz;
//						
						
						

endmodule
