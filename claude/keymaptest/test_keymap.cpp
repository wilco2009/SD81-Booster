// Prueba en el PC de Arduino/Wifi_module_01/KEYMAP_ZX81.cpp (teclado BLE -> matriz del ZX81).
// Reloj simulado; keymap_tick cada TICK ms. Un "observador" convierte la secuencia de matrices en pulsaciones
// (tramos no vacios) y comprueba que en modo texto nunca hay dos teclas a la vez salvo SHIFT, que cada pulsacion
// dura >= KM_HOLD_MS y que entre dos hay >= KM_GAP_MS de matriz vacia.
#include "KEYMAP_ZX81.h"
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include <set>

static int g_fail = 0, g_ok = 0;
#define CHECK(c, ...) do { if (c) g_ok++; else { g_fail++; printf("  FALLO %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static const char* ZX[8][5] = {
	{"SH", "Z", "X", "C", "V"}, {"A", "S", "D", "F", "G"}, {"Q", "W", "E", "R", "T"}, {"1", "2", "3", "4", "5"},
	{"0", "9", "8", "7", "6"}, {"P", "O", "I", "U", "Y"}, {"NL", "L", "K", "J", "H"}, {"SP", ".", "M", "N", "B"}};

static std::string name(const uint8_t r[8]) {
	std::string s;
	for (int i = 0; i < 8; i++)
		for (int j = 0; j < 5; j++)
			if (r[i] & (1 << j)) { if (!s.empty()) s += "+"; s += ZX[i][j]; }
	return s;
}
static int nonshift_keys(const uint8_t r[8]) {
	int n = 0;
	for (int i = 0; i < 8; i++)
		for (int j = 0; j < 5; j++)
			if ((r[i] & (1 << j)) && !(i == 0 && j == 0)) n++;
	return n;
}

// ---- simulador -------------------------------------------------------------------
static const uint32_t TICK = 5;
static uint32_t g_now;
static uint8_t g_mods;
static std::vector<uint8_t> g_keys;           // en orden de pulsacion
struct Span { std::string keys; uint32_t t0, t1; };
static std::vector<Span> g_spans;             // tramos con la matriz no vacia
static uint8_t g_cur[8];
static uint32_t g_cur_t0, g_empty_t0;
static bool g_text_checks = true;
static int g_changes;                         // veces que keymap_tick devolvio true

static void observe(const uint8_t r[8], bool changed) {
	if (changed) g_changes++;
	const bool was_empty = name(g_cur).empty();
	const bool empty = name(r).empty();
	const bool differs = memcmp(r, g_cur, 8) != 0;
	CHECK(changed == differs, "keymap_tick devolvio %d y la matriz %s ha cambiado", changed, differs ? "si" : "no");
	if (g_text_checks) CHECK(nonshift_keys(r) <= 1, "t=%u: dos teclas a la vez en modo texto: %s", g_now, name(r).c_str());
	if (!differs) return;
	if (!was_empty) {
		g_spans.push_back({name(g_cur), g_cur_t0, g_now});
		if (g_text_checks) {
			CHECK(g_now - g_cur_t0 >= KM_HOLD_MS, "t=%u: %s solo %u ms", g_now, name(g_cur).c_str(), g_now - g_cur_t0);
			CHECK(empty, "t=%u: de %s a %s sin hueco", g_now, name(g_cur).c_str(), name(r).c_str());
		}
	}
	if (empty) g_empty_t0 = g_now;
	else {
		if (g_text_checks && was_empty && !g_spans.empty())
			CHECK(g_now - g_empty_t0 >= KM_GAP_MS, "t=%u: hueco de solo %u ms antes de %s", g_now, g_now - g_empty_t0, name(r).c_str());
		g_cur_t0 = g_now;
	}
	memcpy(g_cur, r, 8);
}
static void tick() { uint8_t r[8]; bool c = keymap_tick(g_now, r); observe(r, c); }
static void report() {
	keymap_report(g_mods, g_keys.empty() ? 0 : &g_keys[0], (uint8_t)g_keys.size(), g_now);
	tick();
}
static void wait(uint32_t ms) { for (uint32_t t = 0; t < ms; t += TICK) { g_now += TICK; tick(); } }
static void down(uint8_t u) { g_keys.push_back(u); report(); }
static void up(uint8_t u) {
	for (size_t i = 0; i < g_keys.size(); i++) if (g_keys[i] == u) { g_keys.erase(g_keys.begin() + i); break; }
	report();
}
static void mods(uint8_t m) { g_mods = m; report(); }
// limpia el simulador y el modulo, y deja el reloj adelantado
static void fresh(uint8_t mode = KM_MODE_TEXT, uint8_t layout = KM_LAYOUT_ES) {
	g_keys.clear(); g_mods = 0;
	keymap_reset();
	keymap_set_mode(mode);
	keymap_set_layout(layout);
	g_text_checks = (mode == KM_MODE_TEXT);
	g_now += 1000; tick(); wait(200);
	g_spans.clear(); g_changes = 0;
}
static std::string typed() {      // las pulsaciones vistas, separadas por espacios (el tramo abierto incluido)
	std::string s;
	for (auto& sp : g_spans) { if (!s.empty()) s += " "; s += sp.keys; }
	if (!name(g_cur).empty()) { if (!s.empty()) s += " "; s += name(g_cur) + "~"; }
	return s;
}
// teclea una tecla sola (con Mayus si sh) y deja que se vacie la cola
static std::string type1(uint8_t u, bool sh = false) {
	g_spans.clear();
	if (sh) mods(0x02);
	down(u); wait(10); up(u);
	if (sh) mods(0);
	wait(300);
	return typed();
}

// HID de una letra/cifra
static uint8_t hid_letter(char c) { return (uint8_t)(0x04 + (c - 'A')); }
static uint8_t hid_digit(char c) { return c == '0' ? 0x27 : (uint8_t)(0x1E + (c - '1')); }
static uint8_t hid_char(char c) {
	if (c >= 'A' && c <= 'Z') return hid_letter(c);
	if (c >= '0' && c <= '9') return hid_digit(c);
	if (c == ' ') return 0x2C;
	return 0;
}

// ---- casos ----------------------------------------------------------------------
static void t_letters_digits() {
	printf("letras y cifras (con y sin Mayus)\n");
	fresh();
	for (char c = 'A'; c <= 'Z'; c++) {
		std::string want(1, c);
		std::string got = type1(hid_letter(c));
		CHECK(got == want, "%c -> '%s'", c, got.c_str());
		got = type1(hid_letter(c), true);
		CHECK(got == "SH+" + want, "Mayus+%c -> '%s'", c, got.c_str());
	}
	for (char c = '0'; c <= '9'; c++) {
		std::string want(1, c);
		std::string got = type1(hid_digit(c));
		CHECK(got == want, "%c -> '%s'", c, got.c_str());
		got = type1(hid_digit(c), true);
		CHECK(got == "SH+" + want, "Mayus+%c -> '%s'", c, got.c_str());
	}
	// fila 0: SHIFT Z X C V ... fila 7: SPACE . M N B (bit 0 = la primera)
	uint8_t r[8];
	g_spans.clear(); down(hid_letter('Z')); tick(); memcpy(r, g_cur, 8); CHECK(r[0] == 0x02, "Z -> fila 0 = %02X", r[0]); up(hid_letter('Z')); wait(300);
	down(hid_letter('B')); tick(); memcpy(r, g_cur, 8); CHECK(r[7] == 0x10, "B -> fila 7 = %02X", r[7]); up(hid_letter('B')); wait(300);
	down(hid_digit('6')); tick(); memcpy(r, g_cur, 8); CHECK(r[4] == 0x10, "6 -> fila 4 = %02X", r[4]); up(hid_digit('6')); wait(300);
	down(0x28); tick(); memcpy(r, g_cur, 8); CHECK(r[6] == 0x01, "Enter -> fila 6 = %02X", r[6]); up(0x28); wait(300);
}

struct SignCase { uint8_t usage; bool sh; const char* want; };
static void t_signs(uint8_t layout, const char* title, const SignCase* cs) {
	printf("signos, distribucion %s\n", title);
	fresh(KM_MODE_TEXT, layout);
	for (; cs->usage; cs++) {
		std::string got = type1(cs->usage, cs->sh);
		CHECK(got == cs->want, "%s HID %02X%s -> '%s' (esperado '%s')", title, cs->usage, cs->sh ? " +Mayus" : "", got.c_str(), cs->want);
	}
}
static const SignCase SIGNS_ES[] = {
	{0x2D, false, ""}, {0x2D, true, "SH+C"},            // ' ?
	{0x2E, false, ""}, {0x2E, true, ""},                // ¡ ¿
	{0x2F, false, ""}, {0x2F, true, "SH+H"},            // ` ^(**)
	{0x30, false, "SH+K"}, {0x30, true, "SH+B"},        // + *
	{0x31, false, ""}, {0x32, true, ""},                // ç Ç
	{0x33, false, ""}, {0x33, true, ""},                // ñ Ñ
	{0x34, false, ""}, {0x35, true, ""},                // ´ ª
	{0x36, false, "SH+."}, {0x36, true, "SH+X"},        // , ;
	{0x37, false, "."}, {0x37, true, "SH+Z"},           // . :
	{0x38, false, "SH+J"}, {0x38, true, ""},            // - _
	{0x64, false, "SH+N"}, {0x64, true, "SH+M"},        // < >
	{0x1F, true, "SH+2"},                               // Mayus+2 (") es AND del ZX81: posicion
	{0x25, true, "SH+8"},                               // Mayus+8 (() es -> del ZX81
	{0x54, false, "SH+V"}, {0x55, false, "SH+B"}, {0x56, false, "SH+J"}, {0x57, false, "SH+K"}, {0x63, false, "."},   // numerico / * - + .
	{0, false, 0}};
static const SignCase SIGNS_US[] = {
	{0x2D, false, "SH+J"}, {0x2D, true, ""},            // - _
	{0x2E, false, "SH+L"}, {0x2E, true, "SH+K"},        // = +
	{0x2F, false, ""}, {0x30, true, ""},                // [ }
	{0x31, false, ""}, {0x32, true, ""}, {0x64, false, ""},   // \ |
	{0x33, false, "SH+X"}, {0x33, true, "SH+Z"},        // ; :
	{0x34, false, ""}, {0x34, true, "SH+P"},            // ' "
	{0x35, false, ""}, {0x35, true, ""},                // ` ~
	{0x36, false, "SH+."}, {0x36, true, "SH+N"},        // , <
	{0x37, false, "."}, {0x37, true, "SH+M"},           // . >
	{0x38, false, "SH+V"}, {0x38, true, "SH+C"},        // / ?
	{0x1F, true, "SH+2"},                               // Mayus+2 (@) es AND
	{0, false, 0}};

static void t_specials() {
	printf("teclas especiales\n");
	fresh();
	struct { uint8_t u; bool sh; const char* want; } cs[] = {
		{0x28, false, "NL"}, {0x28, true, "SH+NL"}, {0x2C, false, "SP"}, {0x2C, true, "SH+SP"},
		{0x29, false, "SP"}, {0x2A, false, "SH+0"}, {0x4C, false, "SH+0"},
		{0x50, false, "SH+5"}, {0x51, false, "SH+6"}, {0x52, false, "SH+7"}, {0x4F, false, "SH+8"},
		{0x3A, false, "SH+1"}, {0x3B, false, "SH+NL"}, {0x3C, false, "SH+9"},
		{0x2B, false, ""}, {0x3D, false, ""}, {0x58, false, "NL"}, {0x59, false, "1"}, {0x62, false, "0"}, {0x59, true, "1"}};
	for (auto& c : cs) {
		std::string got = type1(c.u, c.sh);
		CHECK(got == c.want, "HID %02X%s -> '%s' (esperado '%s')", c.u, c.sh ? " +Mayus" : "", got.c_str(), c.want);
	}
	// Ctrl / Alt / GUI + tecla: no escribe; la Mayus sola tampoco
	g_spans.clear();
	mods(0x01); down(hid_letter('C')); wait(10); up(hid_letter('C')); mods(0);
	mods(0x40); down(hid_digit('2')); wait(10); up(hid_digit('2')); mods(0);
	mods(0x08); down(hid_letter('R')); wait(10); up(hid_letter('R')); mods(0);
	mods(0x02); wait(200); mods(0);
	wait(300);
	CHECK(typed() == "", "Ctrl+C, AltGr+2, GUI+R, Mayus sola -> '%s'", typed().c_str());
	// 0xE1 en la lista (NKRO) vale como Mayus
	g_spans.clear();
	g_keys.push_back(0xE1); report(); down(hid_letter('P')); wait(10); g_keys.clear(); report(); wait(300);
	CHECK(typed() == "SH+P", "0xE1 en la lista + P -> '%s'", typed().c_str());
	// ErrorRollOver: el informe se ignora (A sigue pulsada y no se repite nada)
	g_spans.clear();
	down(hid_letter('A')); wait(10);
	{ uint8_t err[6] = {1, 1, 1, 1, 1, 1}; keymap_report(0, err, 6, g_now); tick(); }
	wait(100); up(hid_letter('A')); wait(300);
	CHECK(typed() == "A", "ErrorRollOver -> '%s'", typed().c_str());
}

// escritura rapida: cada tecla se pulsa 'every' ms despues de la anterior y se mantiene 'hold' ms (solapes)
static void type_fast(const char* text, uint32_t every, uint32_t hold) {
	struct Ev { uint32_t t; uint8_t u; bool dn; };
	std::vector<Ev> ev;
	for (int i = 0; text[i]; i++) {
		ev.push_back({i * every, hid_char(text[i]), true});
		uint32_t h = hold;                     // una tecla no se puede pulsar otra vez sin soltarla antes
		for (int j = i + 1; text[j]; j++)
			if (text[j] == text[i]) { if ((j - i) * every - 1 < h) h = (j - i) * every - 1; break; }
		ev.push_back({i * every + h, hid_char(text[i]), false});
	}
	uint32_t t0 = g_now;
	for (uint32_t t = 0;; t++) {
		bool any = false, left = false;
		for (auto& e : ev) {
			if (e.t == t) {
				if (e.dn) g_keys.push_back(e.u);
				else for (size_t i = 0; i < g_keys.size(); i++) if (g_keys[i] == e.u) { g_keys.erase(g_keys.begin() + i); break; }
				any = true;
			}
			if (e.t > t) left = true;
		}
		g_now = t0 + t;
		if (any) report();
		else if (t % TICK == 0) tick();
		if (!left) break;
	}
	wait(KM_QUEUE_LEN * (KM_HOLD_MS + KM_GAP_MS) + 200);
}
static std::string expect_of(const char* text) {
	std::string s;
	for (int i = 0; text[i]; i++) { if (!s.empty()) s += " "; s += text[i] == ' ' ? "SP" : std::string(1, text[i]); }
	return s;
}
static void t_fast_typing() {
	printf("escritura rapida con solapes\n");
	const char* texts[] = {"HELLO WORLD", "10 PRINT AT 21", "LLOOPP", "QWERTYUIOP ASDFGHJKL ZXCVBNM"};
	const uint32_t speeds[][2] = {{30, 80}, {15, 120}, {50, 60}, {8, 40}, {100, 30}};
	for (auto* tx : texts)
		for (auto& sp : speeds) {
			if (strlen(tx) > KM_QUEUE_LEN) continue;
			fresh();
			type_fast(tx, sp[0], sp[1]);
			CHECK(typed() == expect_of(tx), "'%s' cada %u ms, %u ms pulsada -> '%s'", tx, sp[0], sp[1], typed().c_str());
		}
	// la mas larga (28) cabe en la cola aunque se teclee todo de golpe
	fresh();
	type_fast("QWERTYUIOP ASDFGHJKL ZXCVBNM", 1, 3);
	CHECK(typed() == expect_of("QWERTYUIOP ASDFGHJKL ZXCVBNM"), "28 teclas en 28 ms -> '%s'", typed().c_str());
	// Mayus en medio: "P" con Mayus (") y "A" sin ella, solapadas
	fresh();
	mods(0x02); down(hid_letter('P')); g_now += 5; mods(0); down(hid_letter('A')); g_now += 5; up(hid_letter('P')); up(hid_letter('A')); wait(400);
	CHECK(typed() == "SH+P A", "Mayus+P solapada con A -> '%s'", typed().c_str());
}

static void t_queue_full() {
	printf("cola llena\n");
	fresh();
	std::string want;
	for (int i = 0; i < KM_QUEUE_LEN + 8; i++) {   // 40 pulsaciones sin que pase el tiempo
		uint8_t u = hid_letter((char)('A' + i % 26));
		g_keys.push_back(u); keymap_report(0, &g_keys[0], 1, g_now);
		g_keys.clear(); keymap_report(0, 0, 0, g_now);
		if (i < KM_QUEUE_LEN) { if (!want.empty()) want += " "; want += (char)('A' + i % 26); }
	}
	tick();
	wait((KM_QUEUE_LEN + 10) * (KM_HOLD_MS + KM_GAP_MS));
	CHECK(typed() == want, "40 en cola -> %zu pulsaciones: '%s'", g_spans.size(), typed().c_str());
	CHECK(g_spans.size() == KM_QUEUE_LEN, "salen %zu, esperadas %d", g_spans.size(), KM_QUEUE_LEN);
}

static void t_hold_repeat() {
	printf("tecla mantenida (la ROM repite) y la ultima manda\n");
	// mantener Retroceso 1 s: un solo tramo de ~1 s, sin repeticiones generadas aqui
	fresh();
	down(0x2A); wait(1000); up(0x2A); wait(200);
	CHECK(g_spans.size() == 1 && g_spans[0].keys == "SH+0", "Retroceso mantenido -> '%s'", typed().c_str());
	if (g_spans.size() == 1) CHECK(g_spans[0].t1 - g_spans[0].t0 >= 995, "Retroceso mantenido %u ms", g_spans[0].t1 - g_spans[0].t0);
	// toque corto: se ve KM_HOLD_MS
	fresh();
	down(hid_letter('K')); wait(5); up(hid_letter('K')); wait(300);
	CHECK(g_spans.size() == 1 && g_spans[0].t1 - g_spans[0].t0 >= KM_HOLD_MS && g_spans[0].t1 - g_spans[0].t0 <= KM_HOLD_MS + TICK,
	      "toque de 5 ms -> '%s' %u ms", typed().c_str(), g_spans.empty() ? 0 : g_spans[0].t1 - g_spans[0].t0);
	// A mantenida y B pulsada a los 500 ms: A hasta B, hueco, B; A no vuelve aunque siga pulsada
	fresh();
	uint32_t t0 = g_now;
	down(hid_letter('A')); wait(500); down(hid_letter('B')); wait(30); up(hid_letter('B')); wait(500); up(hid_letter('A')); wait(300);
	CHECK(typed() == "A B", "A mantenida + B -> '%s'", typed().c_str());
	if (g_spans.size() == 2) {
		CHECK(g_spans[0].t1 - t0 >= 500 && g_spans[0].t1 - t0 <= 500 + TICK, "A se suelta a los %u ms", g_spans[0].t1 - t0);
		CHECK(g_spans[1].t1 - g_spans[1].t0 <= KM_HOLD_MS + TICK, "B (soltada) %u ms", g_spans[1].t1 - g_spans[1].t0);
	}
	// B mantenida despues de A: B se queda mientras siga pulsada
	fresh();
	down(hid_letter('A')); wait(20); down(hid_letter('B')); wait(20); up(hid_letter('A')); wait(800);
	CHECK(typed() == "A B~", "A y luego B mantenida -> '%s'", typed().c_str());
	up(hid_letter('B')); wait(200);
	CHECK(typed() == "A B", "al soltar B -> '%s'", typed().c_str());
	// la misma tecla dos veces seguidas: dos pulsaciones con hueco
	fresh();
	down(hid_letter('L')); wait(10); up(hid_letter('L')); wait(10); down(hid_letter('L')); wait(10); up(hid_letter('L')); wait(400);
	CHECK(typed() == "L L", "L L rapidas -> '%s'", typed().c_str());
}

static void t_game_mode() {
	printf("modo juego\n");
	fresh(KM_MODE_GAME);
	CHECK(keymap_mode() == KM_MODE_GAME, "modo %u", keymap_mode());
	uint8_t r[8];
	down(hid_letter('Q')); down(0x2C); wait(50);
	tick(); memcpy(r, g_cur, 8); CHECK(name(r) == "Q+SP", "Q + espacio -> '%s'", name(r).c_str());
	down(hid_letter('P')); down(hid_digit('7'));
	tick(); memcpy(r, g_cur, 8); CHECK(name(r) == "Q+7+P+SP", "Q + espacio + P + 7 -> '%s'", name(r).c_str());
	up(hid_letter('Q')); up(0x2C); up(hid_letter('P')); up(hid_digit('7')); wait(50);
	tick(); memcpy(r, g_cur, 8); CHECK(name(r) == "", "todo suelto (pulsadas > 40 ms) -> '%s'", name(r).c_str());
	// Mayus como tecla propia; cursores sin SHIFT; signos con su SHIFT
	mods(0x20); tick(); memcpy(r, g_cur, 8); CHECK(name(r) == "SH", "Mayus der -> '%s'", name(r).c_str());
	down(hid_letter('A')); tick(); memcpy(r, g_cur, 8); CHECK(name(r) == "SH+A", "Mayus+A -> '%s'", name(r).c_str());
	up(hid_letter('A')); mods(0); wait(50);
	down(0x50); down(0x4F); tick(); memcpy(r, g_cur, 8); CHECK(name(r) == "5+8", "izq + der -> '%s'", name(r).c_str());
	up(0x50); up(0x4F); wait(50);
	down(0x30); tick(); memcpy(r, g_cur, 8); CHECK(name(r) == "SH+K", "+ -> '%s'", name(r).c_str());
	up(0x30); wait(50);
	// Ctrl no estorba en modo juego
	mods(0x01); down(hid_letter('M')); tick(); memcpy(r, g_cur, 8); CHECK(name(r) == "M", "Ctrl+M -> '%s'", name(r).c_str());
	up(hid_letter('M')); mods(0); wait(50);

	printf("modo juego: toques cortos\n");
	g_spans.clear();
	uint32_t t0 = g_now;
	down(hid_letter('X')); g_now += 3; up(hid_letter('X'));
	wait(100);
	CHECK(g_spans.size() == 1 && g_spans[0].keys == "X", "toque de 3 ms -> '%s'", typed().c_str());
	if (g_spans.size() == 1) CHECK(g_spans[0].t1 - t0 >= KM_MIN_PRESS_MS && g_spans[0].t1 - t0 <= KM_MIN_PRESS_MS + TICK, "dura %u ms", g_spans[0].t1 - t0);
	// toque corto y la misma tecla otra vez dentro de los 40 ms
	g_spans.clear();
	down(hid_letter('X')); g_now += 3; up(hid_letter('X')); wait(20); down(hid_letter('X')); wait(100); up(hid_letter('X')); wait(100);
	CHECK(g_spans.size() == 1 && g_spans[0].keys == "X", "X corta + X otra vez -> '%s'", typed().c_str());
	// toque corto de A con B mantenida
	g_spans.clear();
	down(hid_letter('B')); wait(20); down(hid_letter('A')); g_now += 2; up(hid_letter('A'));
	tick(); memcpy(r, g_cur, 8); CHECK(name(r) == "A+B", "toque de A con B -> '%s'", name(r).c_str());
	wait(100); tick(); memcpy(r, g_cur, 8); CHECK(name(r) == "B", "despues -> '%s'", name(r).c_str());
	up(hid_letter('B')); wait(100);
}

static void t_mode_switch() {
	printf("cambio de modo (Ctrl+Alt+Espacio)\n");
	fresh();
	uint8_t r[8];
	mods(0x05); down(0x2C);                                // Ctrl izq + Alt izq + Espacio
	CHECK(keymap_mode() == KM_MODE_GAME, "texto -> juego: modo %u", keymap_mode());
	g_text_checks = false;
	wait(200); tick(); memcpy(r, g_cur, 8);
	CHECK(name(r) == "", "el espacio del cambio no llega (sigue pulsado) -> '%s'", name(r).c_str());
	up(0x2C); mods(0); wait(50);
	down(0x2C); tick(); memcpy(r, g_cur, 8); CHECK(name(r) == "SP", "espacio despues -> '%s'", name(r).c_str());
	up(0x2C); wait(50);
	// tecla de juego mantenida al volver a texto: no se escribe
	down(hid_letter('Q'));
	mods(0x50); down(0x2C);                                // Ctrl der + AltGr + Espacio
	CHECK(keymap_mode() == KM_MODE_TEXT, "juego -> texto: modo %u", keymap_mode());
	g_text_checks = true;
	up(0x2C); mods(0); wait(300); up(hid_letter('Q')); wait(300);
	tick(); memcpy(r, g_cur, 8);
	CHECK(name(r) == "", "Q mantenida al cambiar -> '%s'", name(r).c_str());
	// cola pendiente al cambiar: se vacia
	g_spans.clear();
	down(hid_letter('A')); up(hid_letter('A')); down(hid_letter('S')); up(hid_letter('S')); down(hid_letter('D')); up(hid_letter('D'));
	keymap_set_mode(KM_MODE_GAME); g_text_checks = false; wait(300);
	CHECK(typed() == "A", "cola A S D y cambio a juego -> '%s'", typed().c_str());
	keymap_set_mode(KM_MODE_TEXT); g_text_checks = true;
	// keymap_set_mode con el mismo modo no toca la cola
	g_spans.clear(); wait(100);
	down(hid_letter('A')); up(hid_letter('A')); down(hid_letter('S')); up(hid_letter('S'));
	keymap_set_mode(KM_MODE_TEXT); wait(400);
	CHECK(typed() == "A S", "set_mode(texto) estando en texto -> '%s'", typed().c_str());
	// Ctrl+Alt+Espacio otra vez en modo texto no escribe espacio
	g_spans.clear();
	mods(0x05); down(0x2C); up(0x2C); mods(0); mods(0x05); down(0x2C); up(0x2C); mods(0); wait(300);
	CHECK(keymap_mode() == KM_MODE_TEXT && typed() == "", "dos cambios -> modo %u, '%s'", keymap_mode(), typed().c_str());
}

static void t_reset() {
	printf("reset\n");
	fresh();
	uint8_t r[8];
	for (char c : std::string("ABCDE")) { down(hid_letter(c)); up(hid_letter(c)); }
	wait(30);
	keymap_reset();
	bool changed = keymap_tick(g_now, r);
	CHECK(name(r) == "" && changed, "reset con A en la matriz -> '%s', cambio %d", name(r).c_str(), changed);
	memcpy(g_cur, r, 8);
	g_spans.clear();
	wait(600);
	CHECK(typed() == "", "la cola se vacio -> '%s'", typed().c_str());
	// la tecla aun pulsada en el siguiente informe cuenta como pulsacion nueva (al reconectar)
	g_keys.push_back(hid_letter('Z')); report(); g_keys.clear(); report(); wait(300);
	CHECK(typed() == "Z", "tras reset -> '%s'", typed().c_str());
	// reset en modo juego: suelta todo y el modo sigue
	fresh(KM_MODE_GAME);
	down(hid_letter('Q')); keymap_reset(); tick(); memcpy(r, g_cur, 8);
	CHECK(name(r) == "" && keymap_mode() == KM_MODE_GAME, "reset en juego -> '%s', modo %u", name(r).c_str(), keymap_mode());
	g_keys.clear(); keymap_set_mode(KM_MODE_TEXT);
}

static void t_tick_changed() {
	printf("valor de keymap_tick\n");
	fresh();
	g_changes = 0;
	down(hid_letter('A')); up(hid_letter('A')); wait(400);
	CHECK(g_changes == 2, "una pulsacion -> %d cambios (esperados 2)", g_changes);
}

int main() {
	t_letters_digits();
	t_signs(KM_LAYOUT_ES, "ES", SIGNS_ES);
	t_signs(KM_LAYOUT_US, "US", SIGNS_US);
	t_specials();
	t_fast_typing();
	t_queue_full();
	t_hold_repeat();
	t_game_mode();
	t_mode_switch();
	t_reset();
	t_tick_changed();
	printf("\n%d comprobaciones, %d fallos\n", g_ok + g_fail, g_fail);
	return g_fail ? 1 : 0;
}
