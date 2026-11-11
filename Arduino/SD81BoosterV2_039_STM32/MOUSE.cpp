#include <Arduino.h>
#include "MOUSE.h"

// Pantalla del ZX81 en alta resolucion: 256 x 192. Un raton BLE da del orden de 1000 cuentas por pulgada: con div = 4 hacen falta ~1000 cuentas
// para cruzar la pantalla, que es un recorrido comodo.
static int16_t s_max_x = 255, s_max_y = 191;
static int16_t s_div   = 4;

static MouseState s_st = { 128, 96, 0, 0, 0, 0 };
static int32_t s_pdx = 0, s_pdy = 0;     // movimiento en pixeles desde la ultima lectura (mouse_take_motion)
static bool    s_new = false;           // ha llegado algo desde la ultima lectura (mouse_take_new)
static uint8_t s_rawx = 0, s_rawy = 0, s_wheel8 = 0;   // contadores que dan la vuelta, para la FPGA
static uint32_t s_gen = 0;               // cambia con cualquier cambio visible
static int16_t s_rx = 0, s_ry = 0;       // el resto de las cuentas que no llegan a un pixel: no se pierde, se suma al siguiente movimiento

static uint8_t s_ble_state = 0, s_ble_left = 0;              // lo que cuenta el ESP32 (CMD_BLE)
static uint8_t s_ble_order = 0, s_ble_arg = 0;                // orden pendiente para el ESP32

static int16_t clamp16(int32_t v, int16_t lo, int16_t hi) { return v < lo ? lo : (v > hi ? hi : (int16_t)v); }

void mouse_apply(int16_t dx, int16_t dy, int8_t wheel, uint8_t buttons) {
  int32_t ax = (int32_t)s_rx + dx;
  int32_t ay = (int32_t)s_ry + dy;
  int32_t px = ax / s_div;               // C trunca hacia cero: el resto conserva el signo
  int32_t py = ay / s_div;
  s_rx = (int16_t)(ax - px * s_div);
  s_ry = (int16_t)(ay - py * s_div);

  s_pdx += px; s_pdy += py;
  s_rawx = (uint8_t)(s_rawx + px); s_rawy = (uint8_t)(s_rawy + py); s_wheel8 = (uint8_t)(s_wheel8 + wheel);
  s_gen++;
  s_new = true;
  s_st.x = clamp16((int32_t)s_st.x + px, 0, s_max_x);
  s_st.y = clamp16((int32_t)s_st.y + py, 0, s_max_y);
  s_st.buttons = buttons;
  s_st.wheel = (int16_t)clamp16((int32_t)s_st.wheel + wheel, -32768, 32767);
  s_st.frames++;
  s_st.last_ms = millis();
}

MouseState mouse_get() { return s_st; }

void mouse_take_motion(int16_t* dx, int16_t* dy) {
  *dx = clamp16(s_pdx, -32768, 32767);
  *dy = clamp16(s_pdy, -32768, 32767);
  s_pdx = s_pdy = 0;
}

bool mouse_take_new() {
  bool n = s_new;
  s_new = false;
  return n;
}

int16_t mouse_wheel_take() {
  int16_t w = s_st.wheel;
  s_st.wheel = 0;
  return w;
}

void mouse_config(int16_t max_x, int16_t max_y, int16_t div) {
  if (max_x > 0) s_max_x = max_x;
  if (max_y > 0) s_max_y = max_y;
  if (div >= 1)  s_div = div;
  s_st.x = clamp16(s_st.x, 0, s_max_x);
  s_st.y = clamp16(s_st.y, 0, s_max_y);
  s_gen++;
}

void mouse_center() {
  s_st.x = s_max_x / 2;
  s_st.y = s_max_y / 2;
  s_rx = s_ry = 0;
  s_gen++;
}

MouseFpga mouse_fpga_state() {
  MouseFpga f;
  f.x = (uint8_t)s_st.x; f.y = (uint8_t)s_st.y; f.buttons = s_st.buttons;
  f.wheel = s_wheel8; f.rawx = s_rawx; f.rawy = s_rawy;
  f.connected = (s_ble_state & BLE_ST_KNOWN) ? (s_ble_state & BLE_ST_CONNECTED) != 0 : s_st.frames != 0;   // el real si el ESP32 lo cuenta; si no, "ha llegado algo"
  f.gen = s_gen;
  return f;
}

void mouse_ble_set_state(uint8_t state, uint8_t pair_left_s) {
  state |= BLE_ST_KNOWN;
  if ((state & BLE_ST_CONNECTED) != (s_ble_state & BLE_ST_CONNECTED) || !(s_ble_state & BLE_ST_KNOWN)) s_gen++;   // la FPGA debe ver el cambio de "conectado"
  s_ble_state = state;
  s_ble_left = pair_left_s;
}

uint8_t mouse_ble_state() { return s_ble_state; }
uint8_t mouse_ble_pair_left() { return s_ble_left; }

bool mouse_ble_order(uint8_t order, uint8_t arg) {
  if (order < BLE_ORD_PAIR || order > BLE_ORD_OFF) return false;
  s_ble_order = order;
  s_ble_arg = arg;
  return true;
}

void mouse_ble_take_order(uint8_t* order, uint8_t* arg) {
  *order = s_ble_order; *arg = s_ble_arg;
  s_ble_order = 0; s_ble_arg = 0;
}
