// ============================================================================
// sprite_array_demo.v -- Top de prueba SOLO para medir coste de sintesis
// ============================================================================
// No forma parte del diseno SD81.v. Instancia NUM_SPRITES sprite_slot con la
// logica de decodificacion de direcciones propuesta en sprite_slot.v, mas la
// composicion OR de todos los sprites activos (prioridad = el de mayor
// indice gana si dos se solapan; ver nota mas abajo).
//
// Uso: sintetizar este fichero como top temporal (junto con sprite_slot.v)
// en un proyecto ISE nuevo/aparte -- NO en el proyecto SD81.v real -- y leer
// el "Number of Slice LUTs" / "Number of Slice Registers" del reporte para
// saber el coste real de NUM_SPRITES antes de integrar nada.
//
// Cambia NUM_SPRITES aqui abajo y vuelve a sintetizar para ver como escala.
// ============================================================================

module sprite_array_demo #(
	parameter NUM_SPRITES = 24		// punto de partida acordado; cambiar aqui para reprobar
)(
	input  wire        clk,
	input  wire        reset,

	// Bus Z80 simplificado (ya decodificado a nivel de "es una escritura
	// dentro del rango de sprites"; en SD81.v real esto viene de poke_wr)
	input  wire [15:0] addr,
	input  wire [7:0]  data,
	input  wire        wr,			// 1 pulso de clk en la escritura real

	input  wire [8:0]  pixel_cnt,
	input  wire [8:0]  line_cnt,

	output wire        sprite_active,
	output wire        sprite_pixel
);

	localparam SEL_ADDR   = 16'd2100;
	localparam BASE_ADDR  = 16'd2101;
	localparam FIELD_BITS = 5;

	reg [7:0] sel_sprite = 8'd0;

	always @(posedge clk) begin
		if (reset) sel_sprite <= 8'd0;
		else if (wr && addr == SEL_ADDR) sel_sprite <= data;
	end

	wire        field_wr    = wr && (addr >= BASE_ADDR) && (addr < BASE_ADDR+20);
	wire [4:0]  field       = addr[4:0] - BASE_ADDR[4:0];

	wire [NUM_SPRITES-1:0] slot_active;
	wire [NUM_SPRITES-1:0] slot_pixel;

	genvar i;
	generate
		for (i = 0; i < NUM_SPRITES; i = i + 1) begin : SPRITES
			wire this_sel = field_wr && (sel_sprite == i);
			sprite_slot slot (
				.clk(clk),
				.reset(reset),
				.cfg_sel(this_sel),
				.cfg_field(field),
				.cfg_data(data),
				.cfg_we(field_wr),
				.pos_x(pixel_cnt),
				.pos_y(line_cnt),
				.active(slot_active[i]),
				.pixel_out(slot_pixel[i])
			);
		end
	endgenerate

	// Prioridad simple: el sprite de indice mas alto que este activo gana.
	// (placeholder -- cambiar por la regla de prioridad que se quiera)
	integer j;
	reg active_r, pixel_r;
	always @(*) begin
		active_r = 1'b0;
		pixel_r  = 1'b0;
		for (j = 0; j < NUM_SPRITES; j = j + 1) begin
			if (slot_active[j]) begin
				active_r = 1'b1;
				pixel_r  = slot_pixel[j];
			end
		end
	end

	assign sprite_active = active_r;
	assign sprite_pixel  = pixel_r;

endmodule
