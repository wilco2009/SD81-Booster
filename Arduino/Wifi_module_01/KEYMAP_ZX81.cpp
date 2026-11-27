// ============================================================================
// KEYMAP_ZX81.cpp -- teclado de PC (HID, pagina 0x07) -> matriz 8x5 del ZX81
// ============================================================================
// Contrato: claude/planning/teclado_gamepad_plan.md (KEYMAP_ZX81); razonamiento
// en claude/planning/teclado_gamepad_bluetooth.md §4. Prueba en el PC:
// claude/keymaptest/build.sh.
//
// TRADUCCION
// ----------
// * Por POSICION (es un teclado de ZX81: en modo K la "P" da PRINT):
//   letras, cifras, Espacio y Enter (NEWLINE). Mayus del PC = SHIFT del ZX81
//   tambien con ellas: Mayus+P = ", Mayus+1 = EDIT, Mayus+2 = AND, Mayus+Enter
//   = FUNCTION, Mayus+Espacio = £... Asi TODO el teclado del ZX81 se alcanza
//   como en la maquina real, aunque el PC lleve otra cosa en esa tecla.
//   Consecuencia en la distribucion espanola: " $ ( ) = / estan en el PC sobre
//   las cifras (Mayus+2, 4, 8, 9, 0, 7), asi que esas teclas dan AND, TO, ->,
//   GRAPHICS, RUBOUT, ^ del ZX81 y esos signos se escriben en su sitio del ZX81
//   (Mayus+P, U, I, O, L, V) o, el /, en el teclado numerico.
// * Por CARACTER, segun la distribucion: las demas teclas de signos. El
//   caracter que da el PC (con o sin Mayus) se busca en los signos del ZX81
//   (todos con SHIFT salvo el punto):
//
//     signo  ZX81          signo  ZX81          signo  ZX81
//     :      SHIFT+Z       (      SHIFT+I       -      SHIFT+J
//     ;      SHIFT+X       )      SHIFT+O       +      SHIFT+K
//     ?      SHIFT+C       $      SHIFT+U       =      SHIFT+L
//     /      SHIFT+V       "      SHIFT+P       ^      SHIFT+H (**, potencia)
//     ,      SHIFT+.       <      SHIFT+N       *      SHIFT+B
//     .      .             >      SHIFT+M       (£ = SHIFT+SPACE, sin tecla en ES/US)
//
//   El resto de SHIFT del ZX81 son palabras clave o funciones (Q "", W OR,
//   E STEP, R <=, T <>, Y >=, A STOP, S LPRINT, D SLOW, F FAST, G LLIST,
//   1 EDIT, 2 AND, 3 THEN, 4 TO, 5-8 cursores, 9 GRAPHICS, 0 RUBOUT,
//   NEWLINE FUNCTION): solo por posicion.
//
//   Teclas de signos (codigo HID -> caracter sin Mayus / con Mayus):
//
//     HID   espanola (0)        US (1)
//     0x2D  '  ?                -  _
//     0x2E  ¡  ¿                =  +
//     0x2F  `  ^                [  {
//     0x30  +  *                ]  }
//     0x31  ç  Ç  (o 0x32)      \  |
//     0x32  ç  Ç  (ISO)         \  |
//     0x33  ñ  Ñ                ;  :
//     0x34  ´  ¨                '  "
//     0x35  º  ª                `  ~
//     0x36  ,  ;                ,  <
//     0x37  .  :                .  >
//     0x38  -  _                /  ?
//     0x64  <  >  (ISO)         \  |
//     teclado numerico 0x54-0x57, 0x63:  /  *  -  +  .  (en las dos)
//
//   Los que no estan en el ZX81 (' ¡ ¿ ` ç ñ ´ ¨ º ª _ [ ] { } \ | ~) no hacen
//   nada (el ZX81 no tiene "_" ni minusculas).
// * Teclas especiales: Retroceso y Supr -> SHIFT+0 (RUBOUT); cursores ->
//   SHIFT+5/6/7/8 (en modo juego, 5/6/7/8 sin SHIFT: los juegos los leen asi,
//   INKEY$="5"); Esc -> SPACE (BREAK); F1 EDIT (SHIFT+1), F2 FUNCTION
//   (SHIFT+NEWLINE), F3 GRAPHICS (SHIFT+9). Teclado numerico: cifras y Enter
//   como las normales (sin Mayus). Tab, F4-F12 y demas: nada.
// * Con Ctrl, Alt o GUI (y AltGr) pulsados, en modo texto la tecla no escribe
//   nada (atajos del PC; lo de AltGr en la espanola, @ # | € [ ] { } \ ~, no
//   esta en el ZX81). En modo juego se ignoran esos modificadores.
// * Ctrl+Alt+Espacio (cualquier Ctrl y cualquier Alt) alterna texto/juego; ese
//   Espacio no llega al ZX81 aunque siga pulsado.
// * Un informe con ErrorRollOver (codigo 0x01: demasiadas teclas) se ignora
//   entero (se queda el estado anterior). Codigos 0xE0-0xE7 en la lista se
//   toman como modificadores.
//
// MODO TEXTO (por defecto)
// ------------------------
// La ROM del ZX81 descarta la lectura si hay dos teclas (que no sean SHIFT) a
// la vez, y al escribir rapido en un PC las teclas se solapan casi siempre.
// Cada tecla que se PULSA (flanco) entra en una cola (KM_QUEUE_LEN; llena =>
// se descarta la nueva) con su combinacion (tecla + SHIFT si toca), calculada
// con los modificadores del momento de la pulsacion. La cola se sirve de una
// en una: la combinacion se ve en la matriz KM_HOLD_MS y despues la matriz
// queda vacia KM_GAP_MS antes de la siguiente. SHIFT y su tecla aparecen y
// desaparecen juntos, en la misma llamada a keymap_tick.
//
// Autorrepeticion: el teclado BLE no repite (repetir es cosa del sistema del
// PC, el informe HID solo dice "sigue pulsada"), y la ROM del ZX81 ya repite
// sola mientras ve la tecla mantenida. Asi que aqui NO se genera ninguna
// repeticion: pasado el KM_HOLD_MS, si la tecla que se esta mostrando sigue
// pulsada en el PC y no hay nada esperando en la cola, se mantiene en la
// matriz mientras siga pulsada, y la ROM repite a su ritmo (mantener
// Retroceso borra, mantener un cursor lo mueve). En cuanto llega otra
// pulsacion se suelta (hueco KM_GAP_MS) y pasa la nueva; la anterior ya no
// vuelve aunque siga pulsada: manda la ultima, como en el PC.
//
// MODO JUEGO
// ----------
// La matriz tal cual: la union de todas las teclas pulsadas (Q + espacio a la
// vez), con la Mayus del PC como SHIFT del ZX81 suelta (para los juegos que
// la usan). Una tecla soltada antes de KM_MIN_PRESS_MS desde que se pulso se
// mantiene hasta cumplirlos, para que un toque corto no se pierda entre dos
// lecturas del juego. Sin cola.
//
// Al cambiar de modo se vacia la cola y las teclas que siguen pulsadas no
// cuentan hasta que se suelten.
// ============================================================================
#include "KEYMAP_ZX81.h"
#include <string.h>

// ---- teclas del ZX81: indice = fila*5 + columna --------------------------------
enum {
	ZK_SHIFT = 0,  ZK_Z,   ZK_X, ZK_C, ZK_V,    // fila 0
	ZK_A,          ZK_S,   ZK_D, ZK_F, ZK_G,    // fila 1
	ZK_Q,          ZK_W,   ZK_E, ZK_R, ZK_T,    // fila 2
	ZK_1,          ZK_2,   ZK_3, ZK_4, ZK_5,    // fila 3
	ZK_0,          ZK_9,   ZK_8, ZK_7, ZK_6,    // fila 4
	ZK_P,          ZK_O,   ZK_I, ZK_U, ZK_Y,    // fila 5
	ZK_NL,         ZK_L,   ZK_K, ZK_J, ZK_H,    // fila 6
	ZK_SP,         ZK_DOT, ZK_M, ZK_N, ZK_B     // fila 7
};

// Combinacion: bits 0-5 la tecla, bit 6 = tambien SHIFT. KC_NONE = nada.
#define KC_SH    0x40
#define KC_NONE  0xFF

// Modificadores HID
#define MOD_CTRL   0x11
#define MOD_SHIFT  0x22
#define MOD_ALT    0x44
#define MOD_OTHER  0xDD     // todo menos las Mayus

// HID 0x04-0x1D: a-z
static const uint8_t LETTER[26] = {
	ZK_A, ZK_B, ZK_C, ZK_D, ZK_E, ZK_F, ZK_G, ZK_H, ZK_I, ZK_J, ZK_K, ZK_L, ZK_M,
	ZK_N, ZK_O, ZK_P, ZK_Q, ZK_R, ZK_S, ZK_T, ZK_U, ZK_V, ZK_W, ZK_X, ZK_Y, ZK_Z
};
// HID 0x1E-0x27 (y teclado numerico 0x59-0x62): 1-9, 0
static const uint8_t DIGIT[10] = { ZK_1, ZK_2, ZK_3, ZK_4, ZK_5, ZK_6, ZK_7, ZK_8, ZK_9, ZK_0 };

// Teclas de signos: codigo HID, caracter sin Mayus, con Mayus (ver la tabla de arriba).
// Los caracteres que no son ASCII van como 0: no estan en el ZX81.
struct SignKey { uint8_t usage; char normal, shifted; };

static const SignKey SIGNS_ES[] = {
	{0x2D, '\'', '?'}, {0x2E, 0, 0},      /* ¡ ¿ */  {0x2F, '`', '^'},  {0x30, '+', '*'},
	{0x31, 0, 0},      /* ç Ç */          {0x32, 0, 0},  /* ç Ç */   {0x33, 0, 0},     /* ñ Ñ */
	{0x34, 0, 0},      /* ´ ¨ */          {0x35, 0, 0},  /* º ª */   {0x36, ',', ';'},
	{0x37, '.', ':'},  {0x38, '-', '_'},  {0x64, '<', '>'},
	{0, 0, 0}
};
static const SignKey SIGNS_US[] = {
	{0x2D, '-', '_'},  {0x2E, '=', '+'},  {0x2F, '[', '{'},  {0x30, ']', '}'},
	{0x31, '\\', '|'}, {0x32, '\\', '|'}, {0x33, ';', ':'},  {0x34, '\'', '"'},
	{0x35, '`', '~'},  {0x36, ',', '<'},  {0x37, '.', '>'},  {0x38, '/', '?'},
	{0x64, '\\', '|'},
	{0, 0, 0}
};
static const SignKey SIGNS_KEYPAD[] = {
	{0x54, '/', '/'}, {0x55, '*', '*'}, {0x56, '-', '-'}, {0x57, '+', '+'}, {0x63, '.', '.'},
	{0, 0, 0}
};

static uint8_t char_combo(char ch) {
	switch (ch) {
		case ':':  return KC_SH | ZK_Z;
		case ';':  return KC_SH | ZK_X;
		case '?':  return KC_SH | ZK_C;
		case '/':  return KC_SH | ZK_V;
		case '"':  return KC_SH | ZK_P;
		case ')':  return KC_SH | ZK_O;
		case '(':  return KC_SH | ZK_I;
		case '$':  return KC_SH | ZK_U;
		case '=':  return KC_SH | ZK_L;
		case '+':  return KC_SH | ZK_K;
		case '-':  return KC_SH | ZK_J;
		case '^':  return KC_SH | ZK_H;      // ** (potencia)
		case ',':  return KC_SH | ZK_DOT;
		case '>':  return KC_SH | ZK_M;
		case '<':  return KC_SH | ZK_N;
		case '*':  return KC_SH | ZK_B;
		case '.':  return ZK_DOT;
		default:   return KC_NONE;
	}
}

static uint8_t s_layout = KM_LAYOUT_ES;
static uint8_t s_mode = KM_MODE_TEXT;

static uint8_t sign_combo(const SignKey* t, uint8_t usage, bool shift, bool* found) {
	for (; t->usage; t++)
		if (t->usage == usage) { *found = true; return char_combo(shift ? t->shifted : t->normal); }
	return KC_NONE;
}

// Combinacion del ZX81 para una tecla del PC. shift = Mayus del PC pulsada.
// En modo juego las teclas por posicion van sin SHIFT (la Mayus del PC entra en la matriz como tecla propia).
static uint8_t translate(uint8_t u, bool shift, bool game) {
	const uint8_t sh = (shift && !game) ? KC_SH : 0;
	const uint8_t arrow_sh = game ? 0 : KC_SH;
	if (u >= 0x04 && u <= 0x1D) return LETTER[u - 0x04] | sh;
	if (u >= 0x1E && u <= 0x27) return DIGIT[u - 0x1E] | sh;
	if (u >= 0x59 && u <= 0x62) return DIGIT[u - 0x59];
	switch (u) {
		case 0x28: return ZK_NL | sh;          // Enter
		case 0x58: return ZK_NL;               // Enter del teclado numerico
		case 0x2C: return ZK_SP | sh;          // Espacio (Mayus+Espacio = £)
		case 0x29: return ZK_SP;               // Esc -> BREAK
		case 0x2A: return KC_SH | ZK_0;        // Retroceso -> RUBOUT
		case 0x4C: return KC_SH | ZK_0;        // Supr -> RUBOUT
		case 0x50: return arrow_sh | ZK_5;     // izquierda
		case 0x51: return arrow_sh | ZK_6;     // abajo
		case 0x52: return arrow_sh | ZK_7;     // arriba
		case 0x4F: return arrow_sh | ZK_8;     // derecha
		case 0x3A: return KC_SH | ZK_1;        // F1 EDIT
		case 0x3B: return KC_SH | ZK_NL;       // F2 FUNCTION
		case 0x3C: return KC_SH | ZK_9;        // F3 GRAPHICS
		case 0xE1: case 0xE5: return game ? (uint8_t)ZK_SHIFT : (uint8_t)KC_NONE;   // Mayus (seudo tecla)
	}
	bool found = false;
	uint8_t c = sign_combo(s_layout == KM_LAYOUT_US ? SIGNS_US : SIGNS_ES, u, shift, &found);
	if (found) return c;
	return sign_combo(SIGNS_KEYPAD, u, shift, &found);
}

// ---- estado -----------------------------------------------------------------------
struct Slot {               // una tecla del PC pulsada (o, en modo juego, mantenida por la pulsacion minima)
	uint8_t  used, held, supp, usage, combo;
	uint32_t t;             // cuando se pulso
};
static Slot     s_slots[KM_MAX_KEYS];

struct QItem { uint8_t combo, usage; };
static QItem    s_q[KM_QUEUE_LEN];
static uint8_t  s_qhead, s_qcount;

enum { ST_IDLE, ST_PRESS, ST_GAP };
static uint8_t  s_state = ST_IDLE;
static QItem    s_act;
static uint32_t s_t;

static uint8_t  s_last[8];

static void clear_queue() {
	s_qhead = s_qcount = 0;
	s_state = ST_IDLE;
}

void keymap_reset() {
	memset(s_slots, 0, sizeof(s_slots));
	clear_queue();
}

void keymap_set_layout(uint8_t layout) {
	s_layout = (layout == KM_LAYOUT_US) ? KM_LAYOUT_US : KM_LAYOUT_ES;
}

void keymap_set_mode(uint8_t mode) {
	mode = mode ? KM_MODE_GAME : KM_MODE_TEXT;
	if (mode == s_mode) return;
	s_mode = mode;
	clear_queue();
	for (int i = 0; i < KM_MAX_KEYS; i++) {
		Slot& s = s_slots[i];
		if (!s.used) continue;
		if (s.held) { s.supp = 1; s.combo = KC_NONE; }   // no cuenta hasta soltarla
		else s.used = 0;                                 // fuera lo que se mantenia por la pulsacion minima
	}
}

uint8_t keymap_mode() { return s_mode; }

static Slot* find_held(uint8_t u) {
	for (int i = 0; i < KM_MAX_KEYS; i++)
		if (s_slots[i].used && s_slots[i].held && s_slots[i].usage == u) return &s_slots[i];
	return 0;
}

static Slot* alloc_slot(uint8_t u) {
	for (int i = 0; i < KM_MAX_KEYS; i++)                 // la misma tecla aun mantenida (modo juego): se reutiliza
		if (s_slots[i].used && s_slots[i].usage == u) return &s_slots[i];
	for (int i = 0; i < KM_MAX_KEYS; i++)
		if (!s_slots[i].used) return &s_slots[i];
	return 0;
}

static void enqueue(uint8_t combo, uint8_t usage) {
	if (s_qcount >= KM_QUEUE_LEN) return;                 // llena: se descarta lo nuevo
	QItem& q = s_q[(s_qhead + s_qcount) % KM_QUEUE_LEN];
	q.combo = combo;
	q.usage = usage;
	s_qcount++;
}

void keymap_report(uint8_t modifiers, const uint8_t* keys, uint8_t nkeys, uint32_t now_ms) {
	uint8_t mods = modifiers;
	uint8_t list[KM_MAX_KEYS + 2];
	uint8_t n = 0;
	for (uint8_t i = 0; i < nkeys; i++) {
		const uint8_t u = keys[i];
		if (u == 0x01) return;                            // ErrorRollOver: se ignora el informe
		if (u < 0x04) continue;                           // 0 = nada; 0x02/0x03 errores
		if (u >= 0xE0 && u <= 0xE7) { mods |= (uint8_t)(1 << (u - 0xE0)); continue; }
		bool dup = false;
		for (uint8_t j = 0; j < n; j++) if (list[j] == u) dup = true;
		if (!dup && n < KM_MAX_KEYS) list[n++] = u;
	}
	if (mods & 0x02) list[n++] = 0xE1;                    // las Mayus, como seudo teclas (modo juego)
	if (mods & 0x20) list[n++] = 0xE5;

	// sueltas
	for (int i = 0; i < KM_MAX_KEYS; i++) {
		Slot& s = s_slots[i];
		if (!s.used || !s.held) continue;
		bool still = false;
		for (uint8_t j = 0; j < n; j++) if (list[j] == s.usage) still = true;
		if (still) continue;
		s.held = 0;
		if (s_mode == KM_MODE_TEXT || s.supp) s.used = 0;  // en modo juego se queda hasta la pulsacion minima
	}

	// nuevas
	const bool shift = (mods & MOD_SHIFT) != 0;
	for (uint8_t j = 0; j < n; j++) {
		const uint8_t u = list[j];
		if (find_held(u)) continue;
		if (u == 0x2C && (mods & MOD_CTRL) && (mods & MOD_ALT)) {   // Ctrl+Alt+Espacio: cambio de modo
			keymap_set_mode(s_mode == KM_MODE_TEXT ? KM_MODE_GAME : KM_MODE_TEXT);
			Slot* s = alloc_slot(u);
			if (s) { s->used = 1; s->held = 1; s->supp = 1; s->usage = u; s->combo = KC_NONE; s->t = now_ms; }
			continue;
		}
		Slot* s = alloc_slot(u);
		if (!s) continue;
		s->used = 1; s->held = 1; s->supp = 0; s->usage = u; s->t = now_ms;
		if (s_mode == KM_MODE_GAME) {
			s->combo = translate(u, shift, true);
		} else {
			s->combo = KC_NONE;
			if (mods & MOD_OTHER) continue;                 // Ctrl/Alt/GUI: atajo del PC, no escribe
			const uint8_t c = translate(u, shift, false);
			if (c != KC_NONE) enqueue(c, u);
		}
	}
}

static void apply(uint8_t rows[8], uint8_t combo) {
	if (combo == KC_NONE) return;
	const uint8_t k = combo & 0x3F;
	rows[k / 5] |= (uint8_t)(1 << (k % 5));
	if (combo & KC_SH) rows[0] |= 1;
}

bool keymap_tick(uint32_t now_ms, uint8_t rows[8]) {
	memset(rows, 0, 8);
	if (s_mode == KM_MODE_GAME) {
		for (int i = 0; i < KM_MAX_KEYS; i++) {
			Slot& s = s_slots[i];
			if (!s.used || s.supp) continue;
			if (s.held || (uint32_t)(now_ms - s.t) < KM_MIN_PRESS_MS) apply(rows, s.combo);
			else s.used = 0;
		}
	} else {
		if (s_state == ST_GAP && (uint32_t)(now_ms - s_t) >= KM_GAP_MS) s_state = ST_IDLE;
		if (s_state == ST_IDLE) {
			if (s_qcount) {
				s_act = s_q[s_qhead];
				s_qhead = (uint8_t)((s_qhead + 1) % KM_QUEUE_LEN);
				s_qcount--;
				s_state = ST_PRESS;
				s_t = now_ms;
			}
		} else if (s_state == ST_PRESS && (uint32_t)(now_ms - s_t) >= KM_HOLD_MS) {
			// se mantiene mientras siga pulsada y no espere nada (la ROM repite sola)
			const Slot* s = find_held(s_act.usage);
			if (s_qcount || !s || s->supp) { s_state = ST_GAP; s_t = now_ms; }
		}
		if (s_state == ST_PRESS) apply(rows, s_act.combo);
	}
	const bool changed = memcmp(rows, s_last, 8) != 0;
	memcpy(s_last, rows, 8);
	return changed;
}
