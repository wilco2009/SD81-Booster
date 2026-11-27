#include <Arduino.h>
#include "PAD.h"
#include "COMMS.h"                  // vkeys_set_src, joy_cfg

extern uint8_t keyb_coords[64][2];  // COMMS.cpp: caracter ZX81 (0..63) -> {fila, columna} de la matriz; fila 8 = no es tecla

#define PAD_AXIS_THRESHOLD  64      // ~50 % de 127: a partir de aqui el stick cuenta como direccion

static PadState s_pad;              // ultimo estado recibido
static uint8_t  s_pad_flags = 0;
static uint32_t s_pad_t = 0;        // millis() de la ultima trama
static bool     s_pad_any = false;  // ha llegado alguna trama
static bool     s_pad_new = false;
static bool     s_pad_keys = true;  // gamepad como teclas: activado por defecto
static uint8_t  s_pad_code[PAD_NBUTTONS];
static bool     s_pad_code_init = false;
static uint8_t  s_dir_code[PAD_NDIRS] = { PAD_KEY_JOYFIRE, PAD_KEY_JOYFIRE, PAD_KEY_JOYFIRE, PAD_KEY_JOYFIRE };   // 255 = la de LOAD *JOY

static uint8_t  s_kb_flags = 0;
static uint32_t s_kb_t = 0;
static bool     s_kb_any = false;

// ---- codigos de tecla ----

// Codigo -> {fila, columna}. false si no es ninguna tecla. joy: la posicion de joy_cfg que vale el 255 (0-3 una direccion, 4 el fuego).
static bool code_to_rc(uint8_t code, uint8_t joy, uint8_t* row, uint8_t* col) {
  if (code == PAD_KEY_NONE) return false;
  if (code == PAD_KEY_JOYFIRE) { *row = joy_cfg[joy][0]; *col = joy_cfg[joy][1]; return *row < 8 && *col < 5; }
  if (code == 118) code = PAD_KEY_NEWLINE;
  code &= 0x7F;
  if (code == 27) { *row = 7; *col = 1; return true; }   // "." : tecla del ZX81 que keyb_coords (LOAD *JOY) no tiene
  if (code >= 64) return false;
  *row = keyb_coords[code][0]; *col = keyb_coords[code][1];
  return *row < 8 && *col < 5;
}

static bool code_valid(uint8_t code) {
  uint8_t r, c;
  return code == PAD_KEY_NONE || code_to_rc(code, 4, &r, &c);
}

void pad_keys_defaults() {
  memset(s_pad_code, PAD_KEY_NONE, sizeof s_pad_code);
  s_pad_code[0] = PAD_KEY_JOYFIRE;  // A
  s_pad_code[1] = PAD_KEY_JOYFIRE;  // B
  s_pad_code[9] = PAD_KEY_NEWLINE;  // START
  s_pad_code_init = true;
}

static void codes_init() { if (!s_pad_code_init) pad_keys_defaults(); }

static void press(uint8_t* rows, uint8_t row, uint8_t col) { if (row < 8 && col < 5) rows[row] |= (uint8_t)(1 << col); }

// La matriz que el gamepad pide ahora, al teclado virtual (o nada si esta desactivado o no hay mando)
static void pad_update_keys() {
  codes_init();
  if (!s_pad_keys || !pad_connected()) { vkeys_set_src(VK_SRC_PAD, NULL); return; }
  uint8_t rows[8] = { 0 };
  const PadState& p = s_pad;
  // Direcciones, en el orden de joy_cfg (UP DOWN LEFT RIGHT): cruceta o stick izquierdo; cada una con su codigo (255 = la de LOAD *JOY)
  const bool dir[PAD_NDIRS] = { (p.dpad & 1) || p.ly <= -PAD_AXIS_THRESHOLD, (p.dpad & 2) || p.ly >= PAD_AXIS_THRESHOLD,
                                (p.dpad & 4) || p.lx <= -PAD_AXIS_THRESHOLD, (p.dpad & 8) || p.lx >= PAD_AXIS_THRESHOLD };
  uint8_t r, c;
  for (uint8_t d = 0; d < PAD_NDIRS; d++)
    if (dir[d] && code_to_rc(s_dir_code[d], d, &r, &c)) press(rows, r, c);
  for (uint8_t b = 0; b < PAD_NBUTTONS; b++) {
    if (!(p.buttons & (1u << b))) continue;
    if (code_to_rc(s_pad_code[b], 4, &r, &c)) press(rows, r, c);
  }
  vkeys_set_src(VK_SRC_PAD, rows);
}

// ---- gamepad ----

static bool pad_eq(const PadState& a, const PadState& b) {   // campo a campo: memcmp veria el relleno de la estructura
  return a.buttons == b.buttons && a.dpad == b.dpad && a.lx == b.lx && a.ly == b.ly && a.rx == b.rx && a.ry == b.ry &&
         a.lt == b.lt && a.rt == b.rt;
}

void pad_apply(const uint8_t* p) {
  PadState n;
  memset(&n, 0, sizeof n);
  n.buttons = (uint16_t)p[0] | ((uint16_t)p[1] << 8);
  n.dpad = p[2] & 0x0F;
  n.lx = (int8_t)p[3]; n.ly = (int8_t)p[4]; n.rx = (int8_t)p[5]; n.ry = (int8_t)p[6];
  n.lt = p[7]; n.rt = p[8];
  const uint8_t flags = p[9];
  if (!(flags & 1)) memset(&n, 0, sizeof n);         // desconectado: nada pulsado
  const PadState old = pad_get();
  s_pad = n;
  s_pad_flags = flags;
  s_pad_t = millis();
  s_pad_any = true;
  if (!pad_eq(old, n)) s_pad_new = true;
  pad_update_keys();
}

bool pad_connected() {
  return s_pad_any && (s_pad_flags & 1) && (millis() - s_pad_t < PAD_TIMEOUT_MS);
}

PadState pad_get() {
  PadState z;
  if (pad_connected()) return s_pad;
  memset(&z, 0, sizeof z);
  return z;
}

bool pad_take_new() {
  bool n = s_pad_new;
  s_pad_new = false;
  return n;
}

void pad_keys_enable(bool on) {
  s_pad_keys = on;
  pad_update_keys();
}

bool pad_keys_enabled() { return s_pad_keys; }

bool pad_set_keys(uint8_t n, const uint8_t* codes) {
  codes_init();
  bool ok = true;
  if (n > PAD_NBUTTONS) n = PAD_NBUTTONS;
  for (uint8_t i = 0; i < n; i++) {
    if (code_valid(codes[i])) s_pad_code[i] = codes[i];
    else { s_pad_code[i] = PAD_KEY_NONE; ok = false; }
  }
  pad_update_keys();
  return ok;
}

bool pad_set_all(uint8_t n, const uint8_t* codes) {
  codes_init();
  bool ok = true;
  if (n == 0) {                                      // los de por defecto: direcciones y botones
    memset(s_dir_code, PAD_KEY_JOYFIRE, sizeof s_dir_code);
    pad_keys_defaults();
  }
  if (n > PAD_NDIRS + PAD_ALL_BUTTONS) n = PAD_NDIRS + PAD_ALL_BUTTONS;
  for (uint8_t i = 0; i < n; i++) {
    uint8_t* slot = i < PAD_NDIRS ? &s_dir_code[i] : &s_pad_code[i - PAD_NDIRS];
    if (code_valid(codes[i])) *slot = codes[i];
    else { *slot = PAD_KEY_NONE; ok = false; }
  }
  pad_update_keys();
  return ok;
}

// ---- teclado ----

void keyb_apply(const uint8_t* p) {
  s_kb_flags = p[8];
  s_kb_t = millis();
  s_kb_any = true;
  vkeys_set_src(VK_SRC_KEYB, (s_kb_flags & 1) ? p : NULL);   // el plazo de 300 ms sin trama lo lleva vkeys_poll
}

bool keyb_connected() {
  return s_kb_any && (s_kb_flags & 1) && (millis() - s_kb_t < PAD_TIMEOUT_MS);
}

uint8_t keyb_flags() { return keyb_connected() ? s_kb_flags : 0; }
