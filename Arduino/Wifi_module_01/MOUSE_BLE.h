// MOUSE_BLE.h - raton Bluetooth LE (HID sobre GATT) leido por el ESP32 y mandado al STM32 (CMD_MOUSE, ver WIFI_PROTOCOL.h).
//
// El ESP32 hace de cliente BLE: busca un raton (servicio HID 0x1812 con apariencia de raton, o cualquier HID si es el unico), lo empareja (con
// "bonding": la clave queda guardada en la NVS) y se suscribe a sus informes de entrada. Como cada raton pone X, Y, rueda y botones en un sitio
// distinto del informe, lee el DESCRIPTOR de informes (Report Map, 0x2A4B) y localiza esos campos; si no lo consigue, usa el informe "boot" (0x2A33).
// Cada notificacion se ACUMULA (dx, dy, rueda) y una tarea aparte (prioridad mayor que loop()) la manda al STM32 en cuanto llega: ni el
// loop() del ESP32 (web, NTP...) ni el BLE (que corre en su propia tarea) bloquean el envio, y no se pierde movimiento si el STM32 tarda.
//
// Se activa con "esp ble on" en la consola web (/debug); se guarda en la NVS, asi que sigue activo tras reiniciar. "esp ble" da el estado y
// las latencias (de la notificacion BLE a la respuesta del STM32). Con ESP_MOUSE_BLE = 0 no hay ni una instruccion mas.
#ifndef MOUSE_BLE_H
#define MOUSE_BLE_H

#include <Arduino.h>

#define ESP_MOUSE_BLE        1
#define MOUSE_BLE_SCAN_S     4      // segundos de cada busqueda
#define MOUSE_BLE_MIN_GAP_MS 4      // pausa minima entre dos tramas al STM32 (junta los informes que lleguen casi a la vez)

#if ESP_MOUSE_BLE
void   mouse_ble_begin();                       // llamar una vez en setup(), con el WiFi ya conectado: lee la NVS y crea las tareas
String mouse_ble_console(const String& args);   // orden "esp ble ..." de la consola web
#define MOUSE_BLE_BEGIN()    mouse_ble_begin()
#else
#define MOUSE_BLE_BEGIN()
#endif

#endif
