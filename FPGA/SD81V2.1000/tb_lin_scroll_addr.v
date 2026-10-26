`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// tb_lin_scroll_addr -- prueba exhaustiva de lin_scroll_addr contra un modelo
// escrito con aritmetica ordinaria (sin concatenaciones). No forma parte del
// proyecto de ISE: se simula aparte.
//
//   vlog lin_scroll_addr.v tb_lin_scroll_addr.v
//   vsim -c tb_lin_scroll_addr -do "run -all; quit"
//
// Comprueba, para todas las filas, lineas, columnas (0-32), fine_y (0-7) y un
// juego de regiones (reg_top, reg_bot):
//   - el modelo (direccion exacta de cada byte y fila de origen del atributo);
//   - que sin scroll (fine_y = 0, col < 32) la direccion es y*32 + col, la de
//     siempre, tambien con cualquier region;
//   - que los bufers de entrada caen en $1800-$1FFF, sin pisar el bitmap;
//   - que la linea de origen por debajo de la region nunca pasa de 6 lineas.
//////////////////////////////////////////////////////////////////////////////////
module tb_lin_scroll_addr;

	reg  [4:0] row;
	reg  [2:0] line;
	reg  [5:0] col;
	reg  [2:0] fy;
	reg  [4:0] rtop, rbot;
	wire [12:0] off;
	wire [4:0] srow;
	wire below_w;

	lin_scroll_addr dut (.row(row), .line(line), .col(col), .fine_y(fy),
		.reg_top(rtop), .reg_bot(rbot), .off(off), .src_row(srow), .below(below_w));

	integer errors = 0;
	integer checks = 0;
	integer r, l, c, f, t, b;
	integer y, ys, k;
	integer exp_off, exp_row;
	reg inreg, below;

	task fail(input [255:0] why);
	begin
		errors = errors + 1;
		if (errors < 20)
			$display("ERROR %0s: row=%0d line=%0d col=%0d fy=%0d top=%0d bot=%0d -> off=%h srow=%0d below=%b (esperado %h %0d)",
				why, row, line, col, fy, rtop, rbot, off, srow, below_w, exp_off[12:0], exp_row);
	end
	endtask

	initial begin
		for (t = 0; t < 24; t = t + 1)
		for (b = t - 1; b < 24; b = b + 1) begin      // incluye b = t-1: region vacia
			if (t == 0 || t == 2 || t == 5 || t == 17 || t == 23)
			if (b == t - 1 || b == t || b == 23 || b == 15 || b == 19 || b == 7)
			if (b >= 0) begin
				rtop = t; rbot = b;
				for (r = 0; r < 24; r = r + 1)
				for (l = 0; l < 8; l = l + 1)
				for (c = 0; c <= 32; c = c + 1)
				for (f = 0; f < 8; f = f + 1) begin
					row = r; line = l; col = c; fy = f;
					#1;
					y = r * 8 + l;
					inreg = (r >= t) && (r <= b);
					ys = y + (inreg ? f : 0);
					below = inreg && ((ys / 8) > b);
					if (!below) begin
						exp_row = ys / 8;
						if (c < 32) exp_off = ys * 32 + c;
						else        exp_off = 16'h1800 + ys;
					end else begin
						k = ys - 8 * (b + 1);
						if (k < 0 || k > 6) fail("k fuera de 0-6");
						exp_row = 24;
						if (c < 32) exp_off = 16'h1900 + k * 32 + c;
						else        exp_off = 16'h1A00 + k;
					end
					checks = checks + 1;
					if (off !== exp_off[12:0]) fail("direccion");
					if (srow !== exp_row[4:0]) fail("fila de origen");
					if (below_w !== below) fail("below");
					// sin scroll vertical y dentro del bitmap: la direccion de siempre, en cualquier region
					if (f == 0 && c < 32) begin
						if (off !== (y * 32 + c)) fail("regresion bitmap");
						if (srow !== r) fail("regresion fila");
					end
					// los bufers no pisan el bitmap (< $1800)
					if (c == 32 || below) begin
						if (off < 13'h1800) fail("bufer dentro del bitmap");
					end else begin
						if (off >= 13'h1800) fail("bitmap fuera de $0000-$17FF");
					end
					// los bufers de video caben en la zona libre y no se pisan entre si
					if (c == 32 && !below && !(off >= 13'h1800 && off <= 13'h18BF)) fail("derecha fuera de rango");
					if (below && c < 32 && !(off >= 13'h1900 && off <= 13'h19DF)) fail("abajo fuera de rango");
					if (below && c == 32 && !(off >= 13'h1A00 && off <= 13'h1A06)) fail("esquina fuera de rango");
				end
			end
		end
		$display("%0d comprobaciones, %0d errores", checks, errors);
		if (errors == 0) $display("PASS");
		else $display("FAIL");
		$finish;
	end
endmodule
