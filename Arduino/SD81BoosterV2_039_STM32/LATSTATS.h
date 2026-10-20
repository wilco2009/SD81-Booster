// LATSTATS.h - medidas de tiempo del STM32 para estudiar la latencia de la red (EXAMPLES/NETLAT/README.md).
//
// Con LAT_STATS = 1 el firmware mide y, cada 10 s, saca por el puerto serie (lineas "[LAT] ..."):
//   - el bucle principal (loop()): cuanto tarda una vuelta (media y maximo) y cuantas pasan de 0,7, 1, 2, 5 y 10 ms: es lo que
//     espera cualquier trama del ESP32 o tarea de loop() mientras el STM32 hace otra cosa;
//   - cuanto tarda cada parte del bucle: leer el comando del Z80, ejecutarlo, los comandos serie, wifi_handler_poll (las tramas del
//     ESP32), el parser de musica y las tareas ociosas;
//   - las tramas NET_POLL del ESP32: el tiempo entre una y la siguiente (su ritmo real, con la espera del STM32 incluida) y lo que
//     tarda en contestarlas;
//   - el puente de red, descompuesto en las tres esperas de un comando AT: el Z80 escribe -> el STM32 se lo ofrece al ESP32 en su
//     siguiente trama -> el ESP32 contesta en la que sigue -> el Z80 lo lee.
// Con LAT_STATS = 0 (por defecto) no hay ni una instruccion mas: todo son macros vacias.
//
// Para medir: cambiar el 0 por 1 aqui abajo, compilar, grabar y abrir el monitor serie (con el Scrabble, NETCHAT o NETLAT en marcha).
#ifndef LATSTATS_H
#define LATSTATS_H

#include <Arduino.h>

#define LAT_STATS 0

#if LAT_STATS
enum { LAT_S_READ, LAT_S_CMD, LAT_S_SER, LAT_S_WIFI, LAT_S_PLAY, LAT_S_IDLE, LAT_S_N };

void lat_loop_begin();            // empieza una vuelta de loop()
void lat_end(uint8_t section);    // acaba la parte (desde la marca anterior) y la apunta
void lat_loop_end();              // acaba la vuelta; cada 10 s imprime y pone a cero
void lat_poll_begin();            // empieza handle_net_poll
void lat_poll_end();
void lat_net_write();             // el Z80 ha escrito datos en el puente (cmd 67)
void lat_net_offer();             // handle_net_poll ofrece datos NUEVOS al ESP32
void lat_net_in();                // llegan datos del ESP32 al puente
void lat_net_read();              // el Z80 ha leido datos (cmd 66)

struct LatPollScope { LatPollScope() { lat_poll_begin(); } ~LatPollScope() { lat_poll_end(); } };

#define LAT_LOOP_BEGIN()    lat_loop_begin()
#define LAT_END(s)          lat_end(s)
#define LAT_LOOP_END()      lat_loop_end()
#define LAT_POLL_SCOPE()    LatPollScope lat_poll_scope_
#define LAT_NET_WRITE()     lat_net_write()
#define LAT_NET_OFFER()     lat_net_offer()
#define LAT_NET_IN()        lat_net_in()
#define LAT_NET_READ()      lat_net_read()
#else
#define LAT_LOOP_BEGIN()
#define LAT_END(s)
#define LAT_LOOP_END()
#define LAT_POLL_SCOPE()
#define LAT_NET_WRITE()
#define LAT_NET_OFFER()
#define LAT_NET_IN()
#define LAT_NET_READ()
#endif

#endif
