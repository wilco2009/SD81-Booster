`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// tb_spec_scroll_addr -- prueba exhaustiva de spec_scroll_addr contra un modelo
// escrito con aritmetica ordinaria (sin concatenaciones). No forma parte del
// proyecto de ISE: se simula aparte.
//
//   vlog spec_scroll_addr.v tb_spec_scroll_addr.v
//   vsim -c tb_spec_scroll_addr -do "run -all; quit"
//
// Comprueba, para todas las filas, lineas, columnas (0-32), fine_y (0-7) y un
// juego de regiones (reg_top, reg_bot):
//   - el modelo (direccion exacta de cada byte);
//   - que sin scroll (fine_y = 0, col < 32) las direcciones son las de siempre
//     (Spectrum: bitmap y atributos), tambien con cualquier region;
//   - que los bufers de entrada caen en $1B00-$1FFF, sin pisar bitmap ni atributos;
//   - que la linea de origen por debajo de la region nunca pasa de 6 lineas.
//////////////////////////////////////////////////////////////////////////////////
module tb_spec_scroll_addr;

	reg  [4:0] row;
	reg  [2:0] line;
	reg  [5:0] col;
	reg  [2:0] fy;
	reg  [4:0] rtop, rbot;
	wire [12:0] bm, at;

	spec_scroll_addr dut (.row(row), .line(line), .col(col), .fine_y(fy),
		.reg_top(rtop), .reg_bot(rbot), .bm_addr(bm), .at_addr(at));

	integer errors = 0;
	integer checks = 0;
	integer r, l, c, f, t, b;
	integer y, ys, k;
	integer exp_bm, exp_at;
	reg inreg, below;

	task fail(input [255:0] why);
	begin
		errors = errors + 1;
		if (errors < 20)
			$display("ERROR %0s: row=%0d line=%0d col=%0d fy=%0d top=%0d bot=%0d -> bm=%h at=%h (esperado %h %h)",
				why, row, line, col, fy, rtop, rbot, bm, at, exp_bm[12:0], exp_at[12:0]);
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
						if (c < 32) begin
							exp_bm = ((ys / 64) % 4) * 2048 + (ys % 8) * 256 + ((ys / 8) % 8) * 32 + c;
							exp_at = 6144 + (ys / 8) * 32 + c;
						end else begin
							exp_bm = 16'h1B00 + ys;
							exp_at = 16'h1BC0 + (ys / 8);
						end
					end else begin
						k = ys - 8 * (b + 1);
						if (k < 0 || k > 6) fail("k fuera de 0-6");
						if (c < 32) begin
							exp_bm = 16'h1D00 + k * 32 + c;
							exp_at = 16'h1E00 + c;
						end else begin
							exp_bm = 16'h1E40 + k;
							exp_at = 16'h1E48;
						end
					end
					checks = checks + 1;
					if (bm !== exp_bm[12:0]) fail("bitmap");
					if (at !== exp_at[12:0]) fail("atributo");
					// sin scroll vertical y dentro del bitmap: las direcciones de siempre, en cualquier region
					if (f == 0 && c < 32) begin
						if (bm !== (((y / 64) % 4) * 2048 + (y % 8) * 256 + ((y / 8) % 8) * 32 + c)) fail("regresion bitmap");
						if (at !== (6144 + (y / 8) * 32 + c)) fail("regresion atributo");
					end
					// los bufers no pisan el bitmap (< $1800) ni los atributos ($1800-$1AFF)
					if (c == 32 || below) begin
						if (bm < 13'h1B00 || at < 13'h1B00) fail("bufer dentro de la pantalla");
					end else begin
						if (bm >= 13'h1800) fail("bitmap fuera de $0000-$17FF");
						if (at < 13'h1800 || at >= 13'h1B00) fail("atributo fuera de $1800-$1AFF");
					end
				end
			end
		end
		$display("%0d comprobaciones, %0d errores", checks, errors);
		if (errors == 0) $display("PASS");
		else $display("FAIL");
		$finish;
	end
endmodule
