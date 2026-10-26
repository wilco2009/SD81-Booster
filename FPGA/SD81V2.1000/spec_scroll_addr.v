`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// spec_scroll_addr -- direcciones del bitmap y del atributo del modo Superfast
//                     Spectrum, con scroll fino vertical y la columna / fila extra
//
//  Diseno: claude/planning/scroll_hardware.md
//
//  Sin scroll (fine_y = 0, col < 32) da EXACTAMENTE las direcciones de siempre:
//      bitmap   {y[7:6], y[2:0], y[5:3], col}      y = {row, line}
//      atributo {110, y[7:3], col}
//  Direcciones dentro del bloque de 8 KB de la pantalla (el que elige vpage).
//
//  Scroll fino vertical (fine_y, 0-7): las filas de texto reg_top..reg_bot
//  (la region que se mueve; las demas se quedan fijas) muestran la linea de
//  ORIGEN ys = y + fine_y. Si ys cae por debajo de la region (fila de origen
//  > reg_bot) la linea no esta en el bitmap, esta en el bufer de ENTRADA de
//  abajo. El atributo sigue a su linea de origen (fila ys[7:3]), asi que el
//  color viaja con el bitmap tambien en vertical.
//
//  Columna 32 (col = 32): el byte de mas alla de la derecha, que no esta en
//  el bitmap sino en el bufer de entrada de la derecha. La da el motor de
//  captacion de SD81.v cuando el scroll fino horizontal esta activo.
//
//  Bufers de entrada, en los 1.280 bytes que sobran del bloque de 8 KB
//  ($1B00-$1FFF; el bitmap ocupa $0000-$17FF y los atributos $1800-$1AFF):
//      $1B00-$1BBF  derecha, bitmap (192, uno por linea)
//      $1BC0-$1BD7  derecha, atributos (24, uno por fila)
//      $1C00-$1CBF  izquierda, bitmap (192)     } los lee el motor de
//      $1CC0-$1CD7  izquierda, atributos (24)   } movimiento, no el video
//      $1D00-$1DFF  abajo, bitmap (8 lineas de 32 bytes; se usan 7)
//      $1E00-$1E1F  abajo, atributos (32)
//      $1E20-$1E3F  arriba, atributos (32)      } el motor
//      $1E40-$1E47  esquina abajo-derecha, bitmap (una por linea de abajo)
//      $1E48        esquina abajo-derecha, atributo
//      $1F00-$1FFF  arriba, bitmap (8 lineas de 32 bytes)  } el motor
//  Alineados a propósito: no hay sumadores, solo concatenaciones.
//////////////////////////////////////////////////////////////////////////////////
module spec_scroll_addr(
	input wire [4:0] row,			// fila de texto de la pantalla (0-23)
	input wire [2:0] line,			// linea dentro de la fila (0-7)
	input wire [5:0] col,			// byte de la linea: 0-31, 32 = la columna extra de la derecha
	input wire [2:0] fine_y,		// scroll fino vertical (0-7)
	input wire [4:0] reg_top,		// primera fila de la region que se mueve
	input wire [4:0] reg_bot,		// ultima
	output wire [12:0] bm_addr,		// byte del bitmap
	output wire [12:0] at_addr		// byte del atributo
    );

	wire [7:0] y = {row, line};
	wire in_region = (row >= reg_top) && (row <= reg_bot);
	// 198 como mucho (191 + 7): cabe en 8 bits
	wire [7:0] ys = y + (in_region ? {5'd0, fine_y} : 8'd0);
	// la linea de origen esta por debajo de la region: viene del bufer de abajo
	wire below = in_region && (ys[7:3] > reg_bot);
	wire ext = (col == 6'd32);
	wire [4:0] c = col[4:0];

	assign bm_addr =
		below ? (ext ? {10'b1111001000, ys[2:0]}              // esquina
		              : {5'b11101, ys[2:0], c}) :             // abajo
		        (ext ? {5'b11011, ys} :                       // derecha
		               {ys[7:6], ys[2:0], ys[5:3], c});       // el bitmap de siempre

	assign at_addr =
		below ? (ext ? {10'b1111001001, 3'b000}               // esquina
		              : {8'b11110000, c}) :                   // abajo
		        (ext ? {7'b1101111, 1'b0, ys[7:3]} :          // derecha
		               {3'b110, ys[7:3], c});                 // el atributo de siempre

endmodule
