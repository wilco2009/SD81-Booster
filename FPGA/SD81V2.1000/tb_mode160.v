`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// tb_mode160 -- prueba de mode160 (160x100 a 8 bpp con paleta y PDM). No forma parte del proyecto
// de ISE: se simula aparte.
//
//   vlog mode160.v tb_mode160.v
//   vsim -c tb_mode160 -do "run -all; quit"
//
// Genera pixel_cnt (13 MHz, 0..827) y line_cnt como la FPGA en modo de 80 columnas, con una BRAM de
// 64 KB de lectura sincrona (un ciclo, como el puerto B de la sombra) y un reloj de 208 MHz sin relacion
// de fase con el de 26 MHz. Comprueba:
//   - la paleta por defecto (RGB 3-3-2) y la que carga el Z80 (2063/2064/2065, con autoincremento)
//   - la geometria: 160 pixeles por fila, de pixel_cnt = x_start, 100 filas repetidas en dos lineas
//     (de la 58 a la 257), cada pixel con el byte {base + fila*160 + columna}; fuera de la ventana no hay imagen
//   - la densidad de pulsos de cada pin sobre un color constante (n/15 de R, G, B y BRIGHT)
//   - el borde (4 bits de Chroma) fuera de la ventana y el borrado a 0 en sincronismos y back porch
//   - que apagar el modo (en = 0) lo para y lo vuelve a arrancar limpio
//////////////////////////////////////////////////////////////////////////////////
module tb_mode160;

	// ---- relojes ----
	reg clk = 1'b0;							// 26 MHz
	always #19.23 clk = ~clk;
	reg clk104 = 1'b0;						// 208 MHz (CLK104 de la FPGA), sin relacion de fase
	always #2.4 clk104 = ~clk104;
	reg [25:0] cnt26 = 26'd0;
	always @(posedge clk) cnt26 <= cnt26 + 1'b1;
	wire pclk = cnt26[0];					// 13 MHz
	wire iclk = cnt26[2];					// 3,25 MHz (iclock)

	// ---- temporizacion de 80 columnas ----
	reg [9:0] pixel_cnt = 10'd0;
	always @(posedge pclk) pixel_cnt <= (pixel_cnt == 10'd827) ? 10'd0 : pixel_cnt + 1'b1;
	wire [8:0] HSYNCcnt = pixel_cnt[9:1];
	wire hsync = (HSYNCcnt >= 11) && (HSYNCcnt <= 42);
	reg [8:0] line_cnt = 9'd0;
	reg old_hsync = 1'b0;
	always @(posedge clk) begin
		old_hsync <= hsync;
		if (~old_hsync & hsync) line_cnt <= (line_cnt == 9'd311) ? 9'd0 : line_cnt + 1'b1;
	end
	wire vsync = line_cnt < 8;
	wire backporch = (HSYNCcnt >= 43) && (HSYNCcnt <= 76);
	wire ncsync = ~vsync & ~hsync & ~backporch;

	// ---- memoria ----
	reg [7:0] mem [0:65535];
	wire [15:0] v_addr;
	reg [7:0] v_dout = 8'd0;
	always @(posedge clk) v_dout <= mem[v_addr];

	// ---- DUT ----
	reg rst = 1'b1;
	reg en = 1'b0;
	reg [2:0] base_blk = 3'd4;
	localparam [9:0] XS = 10'd246;
	reg [3:0] border = 4'b1010;				// bright, G, R, B = 1,0,1,0 -> R = bit1, G = bit2, B = bit0, BRIGHT = bit3
	reg pal_idx_wr = 1'b0, pal_lo_wr = 1'b0, pal_hi_wr = 1'b0;
	reg [7:0] wdata = 8'd0;
	wire vred, vgreen, vblue, vbright;
	wire img_on;
	wire [7:0] idx_cur;
	wire [15:0] lvl;
	mode160 dut (
		.clk(clk), .clk104(clk104), .rst(rst), .en(en),
		.pixel_cnt(pixel_cnt), .line_cnt(line_cnt), .x_start(XS), .base_blk(base_blk),
		.v_dout(v_dout), .v_addr(v_addr),
		.wclk(iclk), .pal_idx_wr(pal_idx_wr), .pal_lo_wr(pal_lo_wr), .pal_hi_wr(pal_hi_wr), .wdata(wdata),
		.blank_n(ncsync), .border(border),
		.vred(vred), .vgreen(vgreen), .vblue(vblue), .vbright(vbright),
		.img_on_o(img_on), .idx_cur_o(idx_cur), .lvl_o(lvl));

	// ---- POKEs de la paleta: un pulso de un ciclo de iclock alrededor de su flanco de bajada ----
	task poke;			// which: 0 = 2063 (puntero), 1 = 2064 (bajo), 2 = 2065 (alto)
		input [1:0] which;
		input [7:0] v;
		begin
			@(posedge iclk);
			#3;
			wdata = v;
			pal_idx_wr = (which == 2'd0);
			pal_lo_wr = (which == 2'd1);
			pal_hi_wr = (which == 2'd2);
			@(negedge iclk);
			#3;
			pal_idx_wr = 1'b0; pal_lo_wr = 1'b0; pal_hi_wr = 1'b0;
		end
	endtask

	integer errors = 0;
	integer checks = 0;
	task check;
		input ok;
		input [255:0] msg;
		begin
			checks = checks + 1;
			if (!ok) begin
				errors = errors + 1;
				if (errors < 25) $display("ERROR: %0s (t=%0t linea %0d pixel_cnt %0d)", msg, $time, line_cnt, pixel_cnt);
			end
		end
	endtask

	// ---- modelo ----
	function [7:0] pix;			// byte de la imagen
		input integer r;
		input integer x;
		pix = (r * 7 + x * 3 + 5) & 255;
	endfunction
	reg [15:0] pal_model [0:255];
	function [15:0] def_pal;
		input [7:0] k;
		reg [3:0] rr, gg, bb;
		begin
			case (k[7:5]) 0: rr = 0; 1: rr = 2; 2: rr = 4; 3: rr = 6; 4: rr = 9; 5: rr = 11; 6: rr = 13; default: rr = 15; endcase
			case (k[4:2]) 0: gg = 0; 1: gg = 2; 2: gg = 4; 3: gg = 6; 4: gg = 9; 5: gg = 11; 6: gg = 13; default: gg = 15; endcase
			bb = k[1:0] * 5;
			def_pal = {4'd0, rr, gg, bb};
		end
	endfunction
	reg use_def = 1'b1;

	// ---- comprobacion de la geometria: a mitad de cada pixel, lo que sale en el pixel_cnt de ahora ----
	integer imgpix = 0;				// pixeles comprobados
	integer rows_seen = 0;
	reg [9:0] pc_prev = 10'd0;
	reg [8:0] line_at_start [0:0];
	integer off, n, row, ly, exp_idx;
	reg [15:0] exp_lvl;
	reg geom_en = 1'b0;
	always @(posedge clk) begin
		pc_prev <= pixel_cnt;
		if (geom_en && en && pixel_cnt != pc_prev) begin
			off = pixel_cnt - XS;
			ly = line_cnt - 58;
			if (line_cnt >= 58 && line_cnt < 258 && off >= 2 && off < 482 && ((off - 2) % 3) == 0) begin
				n = (off - 2) / 3;
				row = ly / 2;
				exp_idx = mem[{base_blk, 13'b0} + row * 160 + n];
				exp_lvl = use_def ? def_pal(exp_idx[7:0]) : pal_model[exp_idx];
				check(img_on === 1'b1, "img_on durante la ventana");
				check(idx_cur === exp_idx[7:0], "indice del pixel");
				check(lvl === exp_lvl, "nivel de la paleta");
				imgpix = imgpix + 1;
				if (n == 159) rows_seen = rows_seen + 1;
			end
			// fuera de la ventana horizontal o vertical: sin imagen (con margen de un pixel en los extremos)
			if (!(line_cnt >= 58 && line_cnt < 258) && pixel_cnt > 20 && pixel_cnt < 800)
				check(img_on === 1'b0, "sin imagen fuera de las lineas");
			if ((line_cnt >= 58 && line_cnt < 258) && ((pixel_cnt > 20 && pixel_cnt < XS) || (pixel_cnt > XS + 484 && pixel_cnt < 800)))
				check(img_on === 1'b0, "sin imagen fuera de las columnas");
		end
	end

	// ---- bordes de la imagen: en el primer y el ultimo ciclo con img_on, el nivel ya es el del primer / ultimo pixel ----
	// (la comprobacion de arriba mira a mitad de pixel y no los ve: un nivel viejo en el primer ciclo se vio en la placa
	// como una columna a la izquierda con el color de los pixeles de la derecha)
	reg img_prev = 1'b0;
	reg [15:0] lvl_prev = 16'd0;
	integer edge_first = 0, edge_last = 0;
	reg [15:0] exp_first, exp_last;
	always @(posedge clk) begin
		img_prev <= img_on;
		lvl_prev <= lvl;
		if (geom_en && en && line_cnt >= 58 && line_cnt < 258) begin
			row = (line_cnt - 58) / 2;
			exp_first = use_def ? def_pal(mem[{base_blk, 13'b0} + row * 160]) : pal_model[mem[{base_blk, 13'b0} + row * 160]];
			exp_last  = use_def ? def_pal(mem[{base_blk, 13'b0} + row * 160 + 159]) : pal_model[mem[{base_blk, 13'b0} + row * 160 + 159]];
			if (img_on && !img_prev) begin
				edge_first = edge_first + 1;
				check(lvl === exp_first, "primer ciclo de la imagen: nivel del primer pixel");
			end
			if (!img_on && img_prev) begin
				edge_last = edge_last + 1;
				check(lvl_prev === exp_last, "ultimo ciclo de la imagen: nivel del ultimo pixel");
			end
		end
	end

	// ---- pines: borde, borrado y densidad ----
	integer slots = 0, hr = 0, hg = 0, hb = 0, hw = 0;
	reg meas = 1'b0;
	always @(posedge clk104) if (meas) begin
		slots = slots + 1;
		hr = hr + vred; hg = hg + vgreen; hb = hb + vblue; hw = hw + vbright;
	end
	reg pins_en = 1'b0;
	reg [9:0] pc_prev2 = 10'd0;
	always @(posedge clk104) begin
		pc_prev2 <= pixel_cnt;
		if (pins_en && en) begin
			// en las lineas de arriba de la imagen, fuera del borrado: sale el borde
			if (line_cnt >= 20 && line_cnt < 50 && pixel_cnt > 160 && pixel_cnt < 820)
				check({vbright, vgreen, vred, vblue} === {border[3], border[2], border[1], border[0]}, "borde");
			// sincronismo y back porch: todo a 0
			if (line_cnt >= 20 && pixel_cnt > 30 && pixel_cnt < 150)
				check({vbright, vgreen, vred, vblue} === 4'b0000, "borrado en el sincronismo");
			if (line_cnt < 8 && pixel_cnt > 200 && pixel_cnt < 700)
				check({vbright, vgreen, vred, vblue} === 4'b0000, "borrado en el vsync");
		end
	end

	integer i, f, lastline;
	reg [15:0] ent;
	reg [7:0] hi8, lo8;
	integer dB, dW, tol;
	integer errors_before;
	initial begin
		for (i = 0; i < 65536; i = i + 1) mem[i] = 8'hxx;
		// imagen de prueba en el bloque 4 ($8000), 100 filas de 160 bytes; el resto, un valor distinto para ver lecturas de mas
		for (i = 0; i < 16384; i = i + 1) mem[16'h8000 + i] = 8'h5A;
		for (i = 0; i < 100; i = i + 1)
			for (f = 0; f < 160; f = f + 1) mem[16'h8000 + i * 160 + f] = pix(i, f);
		for (i = 0; i < 256; i = i + 1) pal_model[i] = 16'h0000;

		#200 rst = 1'b0;
		// ---- 1. paleta por defecto y geometria, dos cuadros ----
		en = 1'b1;
		geom_en = 1'b1;
		pins_en = 1'b1;
		@(negedge hsync);
		wait (line_cnt == 9'd5);
		wait (line_cnt == 9'd5 && pixel_cnt == 10'd300);
		wait (line_cnt == 9'd290);		// cuadro completo
		wait (line_cnt == 9'd5 && pixel_cnt > 10'd300);
		$display("geometria (paleta por defecto): %0d pixeles comprobados, %0d filas, %0d errores", imgpix, rows_seen, errors);
		check(imgpix == 160 * 200, "pixeles de un cuadro entero = 160 * 200 lineas -> 32000");
		check(rows_seen == 200, "una fila completa por linea de TV");

		// ---- 2. paleta cargada por el Z80, con autoincremento, empezando por la entrada 0 ----
		use_def = 1'b0;
		border = 4'b1111;					// todos los pines a 1 en el borde: el borrado se ve en los cuatro
		poke(2'd0, 8'd0);
		for (i = 0; i < 256; i = i + 1) begin
			hi8 = i * 11 + 3;
			lo8 = i * 29 + 7;
			ent = {hi8, lo8};
			pal_model[i] = ent;
			poke(2'd1, ent[7:0]);
			poke(2'd2, ent[15:8]);
		end
		// el puntero ha dado la vuelta: una escritura mas va a la entrada 0
		poke(2'd1, 8'h11);
		poke(2'd2, 8'h22);
		pal_model[0] = 16'h2211;
		// y escribir el puntero a mano (sin tocar las demas)
		poke(2'd0, 8'd77);
		poke(2'd1, 8'hAB);
		poke(2'd2, 8'hCD);
		pal_model[77] = 16'hCDAB;
		wait (line_cnt == 9'd290);
		wait (line_cnt == 9'd5 && pixel_cnt > 10'd300);
		imgpix = 0; rows_seen = 0;
		wait (line_cnt == 9'd290);
		wait (line_cnt == 9'd5 && pixel_cnt > 10'd300);
		$display("geometria (paleta cargada): %0d pixeles comprobados, %0d filas, %0d errores", imgpix, rows_seen, errors);
		check(imgpix == 160 * 200, "pixeles con paleta cargada");

		// ---- 3. otra base (bloque 2) y la misma imagen: las lecturas siguen a HFILE ----
		for (i = 0; i < 100; i = i + 1)
			for (f = 0; f < 160; f = f + 1) mem[16'h4000 + i * 160 + f] = pix(i + 31, f + 17);
		base_blk = 3'd2;
		border = 4'b0101;
		wait (line_cnt == 9'd290);
		wait (line_cnt == 9'd5 && pixel_cnt > 10'd300);
		imgpix = 0;
		wait (line_cnt == 9'd290);
		wait (line_cnt == 9'd5 && pixel_cnt > 10'd300);
		check(imgpix == 160 * 200, "pixeles con otra base");
		$display("base 2: %0d pixeles, %0d errores", imgpix, errors);

		// ---- 4. densidad de pulsos sobre un color constante ----
		geom_en = 1'b0;
		border = 4'b0000;					// sin borde, para que los pocos ciclos de retardo en los extremos no cuenten
		for (i = 0; i < 16384; i = i + 1) mem[16'h4000 + i] = 8'd200;
		pal_model[200] = {4'd6, 4'd15, 4'd0, 4'd8};		// BRIGHT 6/15, R 15/15, G 0, B 8/15
		poke(2'd0, 8'd200);
		poke(2'd1, 8'h08);
		poke(2'd2, 8'h6F);
		wait (line_cnt == 9'd100 && pixel_cnt == 10'd100);
		wait (img_on === 1'b1);
		meas = 1'b1;
		wait (img_on === 1'b0);
		meas = 1'b0;
		$display("densidad: %0d ranuras: R %0d G %0d B %0d W %0d", slots, hr, hg, hb, hw);
		check(hr >= slots - 8, "densidad R = 15/15");
		check(hg == 0, "densidad G = 0");
		tol = slots * 3 / 10 + 15;					// 2 % de las ranuras, en unidades de 1/15
		dB = hb * 15 - slots * 8;
		dW = hw * 15 - slots * 6;
		check(dB <= tol && -dB <= tol, "densidad B = 8/15");
		check(dW <= tol && -dW <= tol, "densidad BRIGHT = 6/15");

		// ---- 5. apagar el modo: nada de imagen, y al volver a encender sale limpio ----
		en = 1'b0;
		geom_en = 1'b0;
		pins_en = 1'b0;
		repeat (2000) @(posedge clk);
		check(img_on === 1'b0, "apagado: sin imagen");
		for (i = 0; i < 100; i = i + 1)
			for (f = 0; f < 160; f = f + 1) mem[16'h4000 + i * 160 + f] = pix(i + 31, f + 17);
		en = 1'b1;
		geom_en = 1'b1;
		pins_en = 1'b1;
		border = 4'b1100;
		wait (line_cnt == 9'd290);
		wait (line_cnt == 9'd5 && pixel_cnt > 10'd300);
		imgpix = 0;
		wait (line_cnt == 9'd290);
		wait (line_cnt == 9'd5 && pixel_cnt > 10'd300);
		check(imgpix == 160 * 200, "pixeles tras reencender");

		$display("-----");
		$display("%0d comprobaciones, %0d errores", checks, errors);
		if (errors == 0) $display("TODO OK");
		else $display("FALLOS");
		$finish;
	end
endmodule
