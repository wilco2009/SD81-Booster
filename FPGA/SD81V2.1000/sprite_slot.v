// ============================================================================
// sprite_slot.v -- Slot de un sprite 8x8 (1 byte/scanline + mascara)
// ============================================================================
// Almacenamiento: 8 bytes de pixel + 8 bytes de mascara = 16 bytes, en dos
// distributed RAM de 8 posiciones x 8 bits (el sintetizador las mapea a LUTs,
// no a BRAM -- la BRAM esta a 32/32, sin margen).
// Mascara: bit=1 -> el pixel del sprite se dibuja (sustituye al fondo).
//          bit=0 -> transparente, se ve el fondo.
//
// Posicionamiento libre por pixel en AMBOS ejes (no alineado a caracter).
//
// Sistema de coordenadas: desplazado 32 pixeles respecto a la pantalla, para
// permitir que un sprite entre/salga por los bordes con recorte suave:
//
//     coordenada sprite 32  ==  pixel 0 de pantalla
//     X: 0..318   (32 fuera por la izquierda, 256 visibles, 32 por la derecha)
//     Y: 0..255   (32 fuera por arriba,      192 visibles, 32 por abajo)
//
// Por eso X necesita 9 bits (dos POKEs: parte baja + bit alto) mientras que Y
// cabe en un byte. El recorte al area visible (0,0)-(255,191) NO se hace aqui
// sino una sola vez en SD81.v (una puerta para los 24 sprites en vez de una
// por sprite).
// ============================================================================

module sprite_slot(
	input  wire       clk,			// pixel_clk
	input  wire       reset,			// activo alto

	// --- Configuracion desde el Z80 (ver mapa de POKEs en SD81.v) ---
	input  wire        cfg_sel,		// este slot esta seleccionado
	input  wire [4:0]  cfg_field,		// que campo se escribe (ver localparams)
	input  wire [7:0]  cfg_data,
	input  wire        cfg_we,		// strobe de escritura

	// --- Posicion de barrido actual, YA en coordenadas de sprite
	//     (pixel de pantalla + 32, ver cabecera) ---
	input  wire [8:0]  pos_x,
	input  wire [8:0]  pos_y,

	// --- Salida hacia el compositor ---
	output wire        active,		// este sprite pone un pixel AHORA
	output wire        pixel_out,		// valor del pixel (solo valido si active)
	output wire [7:0]  color_out		// {tinta[3:0],papel[3:0]} -- mismo formato que attr_o en SD81.v
);

	// Offsets de campo dentro del bloque de configuracion del sprite
	localparam FIELD_ENABLE = 5'd0;
	localparam FIELD_XPOS_L = 5'd1;	// X, 8 bits bajos
	localparam FIELD_XPOS_H = 5'd2;	// X, bit 8 (0 o 1)
	localparam FIELD_YPOS   = 5'd3;
	localparam FIELD_COLOR  = 5'd4;	// {tinta[3:0],papel[3:0]}, valido en CHROMA y SPECTRUM
	localparam FIELD_DATA0  = 5'd5;	// filas 0..7  -> offsets 5..12
	localparam FIELD_MASK0  = 5'd13;	// filas 0..7  -> offsets 13..20

	reg        enable = 1'b0;
	reg [8:0]  x_pos  = 9'd0;
	reg [7:0]  y_pos  = 8'd0;
	reg [7:0]  color  = 8'hF0;		// por defecto: tinta blanca (15), papel negro (0)

	reg [7:0]  data_mem [0:7];
	reg [7:0]  mask_mem [0:7];

	assign color_out = color;

	always @(posedge clk) begin
		if (reset) begin
			enable <= 1'b0;
		end else if (cfg_sel && cfg_we) begin
			case (cfg_field)
				FIELD_ENABLE: enable      <= cfg_data[0];
				FIELD_XPOS_L: x_pos[7:0]  <= cfg_data;
				FIELD_XPOS_H: x_pos[8]    <= cfg_data[0];
				FIELD_YPOS:   y_pos       <= cfg_data;
				FIELD_COLOR:  color       <= cfg_data;
				default: begin
					if (cfg_field >= FIELD_DATA0 && cfg_field < FIELD_DATA0+8)
						data_mem[cfg_field-FIELD_DATA0] <= cfg_data;
					else if (cfg_field >= FIELD_MASK0 && cfg_field < FIELD_MASK0+8)
						mask_mem[cfg_field-FIELD_MASK0] <= cfg_data;
				end
			endcase
		end
	end

	// --- Comparacion de posicion (identica en los dos ejes) ---
	// Si pos < x_pos la resta da la vuelta y queda un valor grande, con lo que
	// la comparacion <8 falla sola: no hacen falta comparaciones con signo.
	wire [8:0]  x_diff  = pos_x - x_pos;
	wire [8:0]  y_diff  = pos_y - {1'b0, y_pos};
	wire        x_match = (x_diff < 9'd8);
	wire        y_match = (y_diff < 9'd8);

	wire [2:0]  col = x_diff[2:0];
	wire [2:0]  row = y_diff[2:0];

	wire [7:0]  row_data = data_mem[row];
	wire [7:0]  row_mask = mask_mem[row];
	wire        mask_bit = row_mask[3'd7-col];

	assign active    = enable && x_match && y_match && mask_bit;
	assign pixel_out = row_data[3'd7-col];

endmodule
