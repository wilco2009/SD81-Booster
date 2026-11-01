// DIAG.h - diagnostico de tiempos del ESP32 para cazar paradas del puente de red (hermano de LAT_STATS en el STM32).
//
// Con ESP_DIAG = 1 el ESP32 apunta EN MEMORIA (sin tocar la SD ni el USB mientras se juega):
//   - cada peticion al STM32 que tarde mas de DIAG_SLOW_MS, necesite reintentos, reciba CMD_FRAME_ERROR o respuestas de otro
//     comando, o falle: el comando, los intentos, cuanto tardo y POR QUE se perdio la trama (timeout buscando el SOF, byte que
//     falta, CRC malo...);
//   - cada tramo de loop() (servidor web, NTP/WiFi, MQTT, NET_POLL con el STM32, log) que pase de DIAG_SLOW_MS, y cada vuelta
//     entera que pase de ese tiempo;
//   - un resumen cada DIAG_SUMMARY_S segundos: RSSI del WiFi, peticiones, reintentos, tiempos maximos de cada tramo y memoria libre.
// El historial tambien se puede leer SIN la SD desde el navegador: http://<IP del ESP32>/diag (404 = ESP_DIAG sigue a 0).
// Nada mas arrancar apunta una linea "ACTIVO ..." y a los 10 s hace el primer volcado: si /SYS/ESP32LOG.TXT no trae esa linea, el
// diagnostico no esta compilado (ESP_DIAG sigue a 0) o el ESP32 no llego a 10 s. Despues vuelca cada DIAG_DUMP_S segundos (o si el
// buzon se llena, o antes de reiniciar) a /SYS/ESP32LOG.TXT, por el fichero que ya tiene abierto el log. En ese momento hay una parada (unas decenas de ms) y se apunta ("VOLCADO ... ms"); hasta entonces la medida no se perturba.
//
// Para medir: cambiar el 0 por 1 aqui abajo, compilar y grabar el ESP32 y jugar; despues leer ESP32LOG.TXT de la SD.
// Con ESP_DIAG = 0 (por defecto) todas las macros son vacias: ni una instruccion mas.
#ifndef DIAG_H
#define DIAG_H

#include <Arduino.h>

#define ESP_DIAG          1
#define DIAG_SLOW_MS      40     // a partir de aqui un tramo o una peticion se apuntan como lentos
#define DIAG_SUMMARY_S    10     // resumen periodico
#define DIAG_DUMP_S       30     // volcado a la SD (el primero, a los 10 s del arranque, para comprobar que funciona)
#define DIAG_RING         6144   // bytes de buzon en RAM

#if ESP_DIAG
enum { DG_WEB, DG_NTPWIFI, DG_MQTT, DG_POLL, DG_SDLOG, DG_N };

void diag_event(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void diag_req(uint8_t cmd, uint8_t attempts, uint32_t ms, bool ok, uint8_t frame_err, uint8_t stale);
void diag_section(uint8_t id, uint32_t ms);
void diag_loop();                    // al final de cada vuelta de loop(): vueltas lentas, resumen y volcado
void diag_flush();                   // vuelca ya lo pendiente (lo llama sdlog_close() antes de reiniciar)
size_t diag_read(char* out, size_t max);   // el historial (texto) para la pagina /diag

extern uint8_t diag_fail_why;        // motivo del ultimo fallo al leer una trama del STM32 (ver DIAG_FAIL en WIFI_CLIENT.cpp)

#define DIAG_FAIL(n)     (diag_fail_why = (n))
#define DIAG_T0()        uint32_t diag_t0_ = millis()
#define DIAG_SEC(id)     diag_section((id), millis() - diag_t0_)
#define DIAG_TIMED(id, call) do { uint32_t diag_t_ = millis(); call; diag_section((id), millis() - diag_t_); } while (0)
#define DIAG_LOOP()      diag_loop()
#define DIAG_FLUSH()     diag_flush()
#else
#define DIAG_FAIL(n)
#define DIAG_T0()
#define DIAG_SEC(id)
#define DIAG_TIMED(id, call) call
#define DIAG_LOOP()
#define DIAG_FLUSH()
#endif

#endif
