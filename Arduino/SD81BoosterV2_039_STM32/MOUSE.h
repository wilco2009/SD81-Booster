// MOUSE.h - estado del raton BLE en el STM32 (lo alimenta el ESP32 con CMD_MOUSE, ver WIFI_PROTOCOL.h).
//
// El STM32 integra los movimientos relativos (dx, dy) en una posicion ABSOLUTA (x, y) recortada al rango de la pantalla, y guarda los botones y la rueda.
// Es la fuente unica del raton: de aqui lo leera despues el Z80 (un comando del MCU, o un puerto de la FPGA).
#ifndef MOUSE_H
#define MOUSE_H

#include <stdint.h>

struct MouseState {
  int16_t  x, y;          // posicion absoluta, 0..max_x / 0..max_y
  uint8_t  buttons;       // bit0 izquierdo, bit1 derecho, bit2 central...
  int16_t  wheel;         // rueda acumulada desde la ultima vez que se puso a cero (mouse_wheel_take)
  uint32_t frames;        // tramas CMD_MOUSE recibidas
  uint32_t last_ms;       // millis() de la ultima
};

// Integra un movimiento (cuentas del raton, ya acumuladas por el ESP32).
void mouse_apply(int16_t dx, int16_t dy, int8_t wheel, uint8_t buttons);

MouseState mouse_get();

// Devuelve la rueda acumulada y la pone a cero (para que cada giro lo vea el Z80 una sola vez).
int16_t mouse_wheel_take();

// Movimiento relativo desde la ultima llamada, en pixeles (ya con el divisor, sin recortar a la pantalla); lo pone a cero. Para juegos que quieren
// dx/dy y no la posicion.
void mouse_take_motion(int16_t* dx, int16_t* dy);

// true si ha llegado algo del raton desde la ultima llamada; lo borra.
bool mouse_take_new();

// Rango de la pantalla y sensibilidad: cuentas del raton por pixel (div >= 1). Por defecto 0..255 x 0..191 y div = 4.
void mouse_config(int16_t max_x, int16_t max_y, int16_t div);

// Pone el cursor en el centro.
void mouse_center();


// Lo que la FPGA necesita ver (mouse_regs.v, ordenes 14 y 15 del canal de configuracion): posicion absoluta, botones y tres contadores de 8 bits que
// dan la vuelta (rueda y movimiento en pixeles, sin recortar): el Z80 resta la lectura anterior y obtiene el movimiento relativo.
// gen cambia con cualquier cambio (raton, centrar, configurar): COMMS.cpp manda un paquete cuando cambia.
struct MouseFpga {
  uint8_t  x, y, buttons, wheel, rawx, rawy;
  bool     connected;      // ha llegado alguna trama del raton desde el arranque
  uint32_t gen;
};
MouseFpga mouse_fpga_state();

// Control del raton BLE (emparejar, conectar, olvidar). El estado lo cuenta el ESP32 (CMD_BLE) y la orden la deja el Z80 (comando 77, ops 3 y 4);
// el ESP32 la recoge en su siguiente CMD_BLE. Ver WIFI_PROTOCOL.h.
#define BLE_ST_CONNECTED  0x01
#define BLE_ST_PAIRING    0x02
#define BLE_ST_ENABLED    0x04
#define BLE_ST_SAVED      0x08
#define BLE_ST_KNOWN      0x80      // el ESP32 ya ha contado su estado alguna vez (si no, 0 = "no se sabe")
enum { BLE_ORD_NONE = 0, BLE_ORD_PAIR = 1, BLE_ORD_CONNECT = 2, BLE_ORD_FORGET = 3, BLE_ORD_ON = 4, BLE_ORD_OFF = 5 };
void    mouse_ble_set_state(uint8_t state, uint8_t pair_left_s);     // lo llama el handler de CMD_BLE
uint8_t mouse_ble_state();                                            // BLE_ST_* (con BLE_ST_KNOWN si el ESP32 ha contestado)
uint8_t mouse_ble_pair_left();
bool    mouse_ble_order(uint8_t order, uint8_t arg);                  // deja una orden pendiente; false si orden no valida
void    mouse_ble_take_order(uint8_t* order, uint8_t* arg);           // la entrega y la borra

#endif
