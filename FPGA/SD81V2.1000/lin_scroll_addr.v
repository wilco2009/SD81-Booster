`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// lin_scroll_addr -- direcciones del modo Superfast HiRes nativo (bitmap lineal)
//                    con scroll fino vertical y la columna / fila extra
//
//  Diseno: claude/planning/scroll_hardware.md (apartado HiRes nativo)
//
//  Gemelo de spec_scroll_addr.v para el bitmap lineal:
//      bitmap   {y, col}              y = {row, line}, 8 bits + 5 de columna
//
//  Sin scroll (fine_y = 0, col < 32) da EXACTAMENTE la direccion de siempre.
//
//  El atributo de MODO 0 (8x1, area paralela de 6 KB en $C000) tiene una
//  direccion por byte de linea, igual que el bitmap, asi que usa la MISMA
//  salida (off) con otra base: {vpage, off} para el bitmap, {3'b110, off} para
//  el atributo. Los bufers de entrada tambien se repiten en las dos areas.
//
//  El atributo de MODO 1 (8x8, relativo al DFILE, paso 33) no sale de aqui:
//  solo necesita saber de que fila de origen es el byte (src_row) y si viene
//  del bufer de abajo (below), porque entonces es la fila 25 fija (24). La
//  columna 32 es el hueco del NEWLINE (paso 33), que ya existe.
//
//  Bufers de entrada, en los 2 KB que sobran de cada area ($1800-$1FFF):
//      $1800-$18FF  derecha (uno por linea; se usan 192)
//      $1900-$19DF  abajo: 7 lineas de 32 bytes
//      $1A00-$1A06  esquina abajo-derecha (una por linea de abajo)
//      $1C00-$1CFF  izquierda (uno por linea)          } los lee el motor de
//      $1D00-$1DFF  arriba: 8 lineas de 32 bytes       } movimiento, no el video
//  Alineados a proposito: sin sumadores, solo concatenaciones.
//////////////////////////////////////////////////////////////////////////////////
module lin_scroll_addr(
	input wire [4:0] row,			// fila de texto de la pantalla (0-23)
	input wire [2:0] line,			// linea dentro de la fila (0-7)
	input wire [5:0] col,			// byte de la linea: 0-31, 32 = la columna extra de la derecha
	input wire [2:0] fine_y,		// scroll fino vertical (0-7)
	input wire [4:0] reg_top,		// primera fila de la region que se mueve
	input wire [4:0] reg_bot,		// ultima
	output wire [12:0] off,			// bitmap y atributo de modo 0 (misma direccion, distinta base)
	output wire [4:0] src_row,		// fila de origen del atributo de modo 1 (24 = fila de entrada de abajo)
	output wire below				// la linea viene del bufer de abajo
    );

	wire [7:0] y = {row, line};
	wire in_region = (row >= reg_top) && (row <= reg_bot);
	// 198 como mucho (191 + 7): cabe en 8 bits
	wire [7:0] ys = y + (in_region ? {5'd0, fine_y} : 8'd0);
	assign below = in_region && (ys[7:3] > reg_bot);
	wire ext = (col == 6'd32);
	wire [4:0] c = col[4:0];

	assign off =
		below ? (ext ? {10'b1101000000, ys[2:0]}              // esquina    $1A00 + k
		              : {5'b11001, ys[2:0], c}) :             // abajo      $1900 + k*32 + c
		        (ext ? {5'b11000, ys} :                       // derecha    $1800 + ys
		               {ys, c});                              // el bitmap de siempre

	assign src_row = below ? 5'd24 : ys[7:3];

endmodule
