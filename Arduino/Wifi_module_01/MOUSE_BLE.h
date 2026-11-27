// MOUSE_BLE.h - raton, teclado y mando Bluetooth LE (HID sobre GATT) leidos por el ESP32 y mandados al STM32 (CMD_MOUSE, CMD_KEYB, CMD_PAD
// y CMD_BLE, ver WIFI_PROTOCOL.h). Plan: claude/planning/teclado_gamepad_plan.md.
//
// El ESP32 hace de cliente BLE con TRES huecos (raton, teclado, mando), cada uno con su conexion, su dispositivo guardado (direccion y nombre
// en la NVS) y su ventana de emparejado. Al emparejar se clasifica por la apariencia del anuncio (0x03C1 teclado, 0x03C2 raton, 0x03C3/0x03C4
// joystick/mando) y, si no la trae, por las colecciones del descriptor de informes (Report Map, 0x2A4B) tras conectar. Un dispositivo ya
// guardado en un hueco no se acepta en otro. El C3 admite 3 conexiones y 3 vinculos: justo uno por hueco.
//
// Raton: como cada raton pone X, Y, rueda y botones en un sitio distinto del informe, se lee el descriptor y se localizan esos campos; si no,
// el informe "boot" (0x2A33). Cada notificacion se ACUMULA (dx, dy, rueda) y una tarea aparte (prioridad mayor que loop()) la manda al STM32.
// Teclado: informes de la pagina 0x07 (array tipo boot o mapa de bits NKRO; respaldo: informe boot 0x2A22) -> KEYMAP_ZX81 -> CMD_KEYB.
// Mando: botones (pagina 9), ejes, hat y gatillos -> CMD_PAD normalizado, con una tabla de botones por mando (por nombre).
// Teclado y mando se mandan al cambiar y cada 100 ms como latido; al desconectarse, una trama con todo suelto.
//
// Consola web (/debug): "esp ble" da el estado de los tres; "esp ble [mouse|kbd|pad] pair|connect|forget|on|off" (sin dispositivo = raton,
// como antes). Con ESP_MOUSE_BLE = 0 no hay ni una instruccion mas.
#ifndef MOUSE_BLE_H
#define MOUSE_BLE_H

#include <Arduino.h>

#define ESP_MOUSE_BLE        1
#define MOUSE_BLE_SCAN_S     4      // segundos de cada busqueda
#define MOUSE_BLE_MIN_GAP_MS 4      // pausa minima entre dos tramas al STM32 (junta los informes que lleguen casi a la vez)
#define BLE_KBD_TICK_MS      5      // cada cuanto se llama a keymap_tick() con el teclado conectado
#define BLE_HEARTBEAT_MS     100    // latido de CMD_KEYB y CMD_PAD (el STM32 suelta todo a los 300 ms sin trama)
#define BLE_RETRY_MS         2500   // espera entre dos busquedas fallidas (la de siempre del raton)
#define BLE_RETRY_BUSY_MS    8000   // la misma espera si ya hay otro dispositivo conectado (no estorbar a su radio)
#define BLE_SECURE_KBD_MS    90000  // plazo del emparejado del teclado (hay que teclear la clave); pasado, se corta la conexion
#define BLE_SECURE_MS        30000  // plazo del emparejado del raton y del mando
#define BLE_MIN_FREE_HEAP    22000  // memoria libre minima para conectar un 2o o 3er dispositivo (medido: raton ~10 KB, mando ~12, teclado ~15)
#define BLE_MIN_FREE_BLOCK   8192   // y bloque libre mas grande
#define BLE_MIN_SCAN_HEAP    16384  // por debajo de esto no se busca (cada anuncio oido se copia en memoria)

#if ESP_MOUSE_BLE
void   mouse_ble_begin();                       // llamar una vez en setup(), con el WiFi ya conectado: lee la NVS y crea las tareas
String mouse_ble_console(const String& args);   // orden "esp ble ..." de la consola web
#define MOUSE_BLE_BEGIN()    mouse_ble_begin()
#else
#define MOUSE_BLE_BEGIN()
#endif

#endif
