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
enum { BLE_ORD_NONE = 0, BLE_ORD_PAIR = 1, BLE_ORD_CONNECT = 2, BLE_ORD_FORGET = 3, BLE_ORD_ON = 4, BLE_ORD_OFF = 5,
       BLE_ORD_KB_TEXT = 6, BLE_ORD_KB_GAME = 7,                       // 6 y 7: modo del teclado (solo dispositivo 1)
       BLE_ORD_KB_ES = 8, BLE_ORD_KB_US = 9 };                         // 8 y 9: distribucion del teclado, espanola / US (solo dispositivo 1)

// Los tres dispositivos BLE (CMD_BLE ampliado, comando 78): estado y segundos de cada uno, la clave de emparejado del teclado y UNA orden pendiente
// por dispositivo. El raton es el dispositivo 0: las funciones mouse_ble_* de abajo son las de siempre (comando 77) sobre el dispositivo 0.
enum { BLE_DEV_MOUSE = 0, BLE_DEV_KEYB = 1, BLE_DEV_PAD = 2, BLE_NDEV = 3 };
void     ble_set_state(uint8_t dev, uint8_t state, uint8_t pair_left_s);   // lo llama el handler de CMD_BLE
uint8_t  ble_state(uint8_t dev);                                        // BLE_ST_* (con BLE_ST_KNOWN si el ESP32 lo ha contado); 0 si dev no valido
uint8_t  ble_pair_left(uint8_t dev);
void     ble_set_passkey(uint32_t key);                                 // clave de 6 cifras que hay que teclear en el teclado (0 = ninguna)
uint32_t ble_passkey();
bool     ble_order(uint8_t dev, uint8_t order, uint8_t arg);            // deja la orden pendiente de ese dispositivo; false si no es valida
// Entrega UNA orden pendiente (por turno entre los dispositivos) y la borra; *order = 0 si no hay. max_dev: el ultimo dispositivo que entiende
// el ESP32 (0 con un ESP32 viejo, que solo conoce el raton: las ordenes del teclado y del gamepad se quedan esperando).
void     ble_take_order(uint8_t max_dev, uint8_t* order, uint8_t* arg, uint8_t* dev);
// El estado como texto (16 caracteres + 0, ASCII; sin rellenar): CONECTADO, EMPAREJANDO 45S, CLAVE 123456, BUSCANDO, PARADO, SIN RATON/TECLADO/MANDO,
// SIN MODULO WIFI. Es el op 5 de los comandos 77 y 78.
void     ble_text(uint8_t dev, char* txt17);

void    mouse_ble_set_state(uint8_t state, uint8_t pair_left_s);     // = ble_set_state(BLE_DEV_MOUSE, ...)
uint8_t mouse_ble_state();                                            // BLE_ST_* (con BLE_ST_KNOWN si el ESP32 ha contestado)
uint8_t mouse_ble_pair_left();
bool    mouse_ble_order(uint8_t order, uint8_t arg);                  // deja una orden pendiente para el raton; false si orden no valida (1..5)

#endif
