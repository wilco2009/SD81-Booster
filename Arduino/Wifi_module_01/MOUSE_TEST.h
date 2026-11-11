// MOUSE_TEST.h - prueba SINTETICA de latencia del raton Bluetooth (SD81-Booster/claude/planning/raton_bluetooth.md).
//
// Con ESP_MOUSE_TEST = 1 el ESP32 hace de raton BLE falso: una TAREA PROPIA de FreeRTOS (no loop(): asi no la frenan la web ni el volcado a la SD)
// manda, cada 1000/MOUSE_TEST_HZ ms, al STM32 un informe de raton (CMD_MOUSE_TEST,
// ver WIFI_PROTOCOL.h) por el MISMO camino que usaria uno real (UART a 921600, una peticion cada vez, con el sondeo del puente de red
// y el resto del loop() compitiendo), y mide el tiempo de ida y vuelta. Cada MOUSE_TEST_REPORT_S segundos saca un resumen por el
// puerto serie (que SD_LOG.h copia a /SYS/ESP32LOG.TXT si SDLOG=1 en /SYS/LOG.CFG, en escrituras pequenas) y lo guarda para la orden "esp mouse":
//   - RTT (us): minimo, media, maximo, percentiles 50/95/99 (cota superior del tramo del histograma) y el histograma;
//   - lo que cuenta el STM32: gap (lo que tardo su loop() en volver a atender el UART) y proc (recibir + contestar);
//   - el retraso del despertar de la tarea y los periodos que no pudo mandar a tiempo (saltados). La espera a que loop() acabe la peticion que
//     tenga en curso con el STM32 (mutex de wifi_client_request) cuenta dentro del RTT.
// Un informe de raton real tardaria ~RTT/2 en llegar al STM32 desde que el ESP32 lo tiene; el BLE (7,5-30 ms de intervalo) se suma aparte.
//
// Para medir: cambiar el 0 por 1 aqui abajo (el firmware lleva el codigo), compilar y grabar el ESP32 (el STM32 ya contesta siempre), y ponerla en marcha
// con MOUSE=1 en /SYS/LOG.CFG (por defecto esta PARADA) o con la orden "esp mouse on" de la consola web (/debug).
// Repetir con: el STM32 en reposo; WAV sonando; un LOAD grande; WiFi con MQTT/telnet activos (el caso peor).
// Con ESP_MOUSE_TEST = 0 (por defecto) no hay ni una instruccion mas.
#ifndef MOUSE_TEST_H
#define MOUSE_TEST_H

#include <Arduino.h>

#define ESP_MOUSE_TEST        0
#define MOUSE_TEST_HZ         100     // informes por segundo (un raton BLE tipico: 67-133)
#define MOUSE_TEST_REPORT_S   10

#if ESP_MOUSE_TEST
void mouse_test_tick();               // llamar en cada vuelta de loop()
String mouse_test_last_report();          // el ultimo resumen (3 lineas), para la orden "esp mouse" de la consola web
#define MOUSE_TEST_TICK()     mouse_test_tick()
#else
#define MOUSE_TEST_TICK()
#endif

#endif
