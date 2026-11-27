// ============================================================================
// KEYMAP_ZX81.h -- teclado de PC (HID, pagina 0x07) -> matriz 8x5 del ZX81
// ============================================================================
// C++ puro: sin Arduino y sin memoria dinamica (se prueba en el PC con
// claude/keymaptest). Contrato en claude/planning/teclado_gamepad_plan.md
// (seccion KEYMAP_ZX81); las reglas y las tablas, en KEYMAP_ZX81.cpp.
//
// Matriz (la de FPGA vkeys.v y la del teclado virtual de la web): rows[i] es
// la fila que se lee con A(8+i) a 0, bit j = D(j), 1 = pulsada:
//   fila 0: SHIFT Z X C V      fila 4: 0 9 8 7 6
//   fila 1: A S D F G          fila 5: P O I U Y
//   fila 2: Q W E R T          fila 6: NEWLINE L K J H
//   fila 3: 1 2 3 4 5          fila 7: SPACE . M N B
// ============================================================================
#ifndef KEYMAP_ZX81_H
#define KEYMAP_ZX81_H

#include <stdint.h>

// Tiempos (ms), a ajustar en la placa
#ifndef KM_HOLD_MS
#define KM_HOLD_MS        60    // modo texto: cada pulsacion se ve al menos esto
#endif
#ifndef KM_GAP_MS
#define KM_GAP_MS         40    // modo texto: matriz vacia entre dos pulsaciones
#endif
#ifndef KM_MIN_PRESS_MS
#define KM_MIN_PRESS_MS   40    // modo juego: pulsacion minima (toques cortos)
#endif
#ifndef KM_QUEUE_LEN
#define KM_QUEUE_LEN      32    // modo texto: pulsaciones en cola (si se llena, se descarta lo nuevo)
#endif
#ifndef KM_MAX_KEYS
#define KM_MAX_KEYS       16    // teclas del PC pulsadas a la vez que se siguen
#endif

#define KM_LAYOUT_ES      0
#define KM_LAYOUT_US      1
#define KM_MODE_TEXT      0
#define KM_MODE_GAME      1

void    keymap_reset();                                   // todo suelto, cola vacia (al conectar/desconectar)
void    keymap_set_layout(uint8_t layout);                // 0 espanol (defecto), 1 US
void    keymap_set_mode(uint8_t mode);                    // 0 texto (defecto), 1 juego
uint8_t keymap_mode();
// Un informe del teclado: modificadores (byte 0 del informe boot: bit0 Ctrl izq... bit7 GUI der) y los codigos de uso (pagina 0x07) pulsados AHORA.
// El que lee el BLE convierte el formato NKRO (mapa de bits) a esta lista.
void    keymap_report(uint8_t modifiers, const uint8_t* keys, uint8_t nkeys, uint32_t now_ms);
// Avanza la cola del modo texto y deja en rows[8] la matriz que debe ver el ZX81 ahora (5 bits por fila, 1 = pulsada).
// Devuelve true si la matriz ha cambiado desde la llamada anterior. Llamarla cada ~5-10 ms y tras cada keymap_report.
bool    keymap_tick(uint32_t now_ms, uint8_t rows[8]);

#endif
