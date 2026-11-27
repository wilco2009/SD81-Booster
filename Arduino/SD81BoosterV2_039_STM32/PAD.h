// PAD.h - gamepad y teclado BLE en el STM32 (los alimenta el ESP32 con CMD_PAD y CMD_KEYB, ver WIFI_PROTOCOL.h y
// claude/planning/teclado_gamepad_plan.md). El Z80 los lee y configura con el comando 78 (cmd_btdev en COMMANDS.cpp).
//
// Las teclas llegan a la FPGA por el teclado virtual (COMMS.cpp, vkeys_set_src): el teclado BLE ya viene traducido a la matriz del ZX81, y el gamepad
// se convierte aqui (si "gamepad como teclas" esta activado, que es lo de por defecto): cruceta o stick izquierdo (pasado el 50 %) -> las 4 direcciones
// (por defecto las de LOAD *JOY), y cada boton -> la tecla que diga su codigo (pad_set_keys, pad_set_all).
//
// Codigo de tecla de un boton o una direccion (ops 1 y 7 del comando 78): el mismo codigo de caracter ZX81 que usa LOAD *JOY, con estas reglas:
//   0          ninguna tecla (por eso el ESPACIO, que en LOAD *JOY es el 0, aqui va como 128: el bit 7, video inverso, no cuenta)
//   1..63      la tecla de ese caracter, como en LOAD *JOY: letras, cifras, "<" (19) = NEW LINE, "*" (23) = SHIFT; los demas signos no son teclas
//   128..191   lo mismo que 0..63 (el bit 7 no cuenta): 128 = ESPACIO
//   118        NEW LINE (CHR$ 118, el caracter de fin de linea del ZX81): igual que 19
//   27         "." (LOAD *JOY no la tiene, aqui si)
//   255        "como LOAD *JOY": en un boton, el FUEGO de LOAD *JOY; en una direccion (op 7), esa direccion de LOAD *JOY (sigue a LOAD *JOY si cambia)
//   otro       no valido (los ops 1 y 7 contestan con el bit 7 puesto y dejan esa posicion sin tecla)
// Por defecto: las 4 direcciones = 255 (las de LOAD *JOY), A (bit 0) y B (bit 1) = 255 (el fuego de LOAD *JOY), START (bit 9) = 19 (NEW LINE),
// los demas 0. Sin configurar nada, el mando va como LOAD *JOY; con el op 7 (LOAD *PAD "cadena") el mando y el joystick DB9 van por separado.
#ifndef PAD_H
#define PAD_H

#include <stdint.h>

#define PAD_TIMEOUT_MS   300        // sin trama en este tiempo, el mando se da por suelto (todo a cero)
#define PAD_NBUTTONS     16
#define PAD_NDIRS        4          // ARRIBA, ABAJO, IZQUIERDA, DERECHA (el orden de LOAD *JOY)
#define PAD_ALL_BUTTONS  13         // los botones del op 7 del comando 78: A..HOME (bits 0-12)
#define PAD_KEY_NONE     0
#define PAD_KEY_JOYFIRE  255
#define PAD_KEY_NEWLINE  19         // "<" del juego de caracteres del ZX81, como en LOAD *JOY

struct PadState {
  uint16_t buttons;                 // bit0 A, 1 B, 2 X, 3 Y, 4 LB, 5 RB, 6 LT, 7 RT, 8 SELECT, 9 START, 10 L3, 11 R3, 12 HOME
  uint8_t  dpad;                    // bit0 arriba, bit1 abajo, bit2 izquierda, bit3 derecha
  int8_t   lx, ly, rx, ry;          // -127..127, hacia abajo positivo
  uint8_t  lt, rt;                  // 0..255
};

// CMD_PAD: los 12 bytes de la trama (ya comprobada la longitud)
void pad_apply(const uint8_t* p);
// Estado actual (todo a cero si no hay mando o han pasado PAD_TIMEOUT_MS sin trama)
PadState pad_get();
bool     pad_connected();
bool     pad_take_new();            // ha cambiado algo desde la llamada anterior; lo borra

// Gamepad como teclas
void    pad_keys_enable(bool on);
bool    pad_keys_enabled();
// n codigos (del boton 0 al n-1; los demas no cambian). Devuelve false si alguno no es valido (ese boton queda sin tecla).
bool    pad_set_keys(uint8_t n, const uint8_t* codes);
void    pad_keys_defaults();         // solo los botones (op 1 con n = 0)
// Op 7: n (0-17) codigos en el orden ARRIBA, ABAJO, IZQUIERDA, DERECHA, A, B, X, Y, LB, RB, LT, RT, SELECT, START, L3, R3, HOME.
// Los que pasan de n no cambian; n = 0 vuelve a los de por defecto (direcciones y botones). false si alguno no es valido (ese queda sin tecla).
bool    pad_set_all(uint8_t n, const uint8_t* codes);

// CMD_KEYB: los 9 bytes de la trama (filas[8] + flags)
void    keyb_apply(const uint8_t* p);
bool    keyb_connected();           // ha llegado una trama con "conectado" hace menos de PAD_TIMEOUT_MS
uint8_t keyb_flags();               // los flags de la ultima trama (bit0 conectado, bit1 modo juego); 0 si no hay teclado

#endif
