#ifndef NET_BRIDGE_H
#define NET_BRIDGE_H

#include <Arduino.h>

// Puente entre un socket TCP y el Z80, pasando por el STM32 (CMD_NET_POLL).
// El ESP32 hace de modem ya conectado: el Z80 solo ve un flujo de bytes, como
// si tuviera un puerto serie delante, y por eso los programas de BBS
// existentes funcionan sin saber nada de red.
//
// Socket crudo, sin negociacion telnet (IAC/DO/WILL). Si algun dia hace falta,
// se mastica aqui y al Z80 le siguen llegando datos limpios.

void net_bridge_init();

// Llamar desde loop(). Lleva su propia cadencia: sondea el STM32 cada ~250 ms
// en reposo y cada ~15 ms con conexion abierta. La de reposo se mantiene baja
// a proposito porque el loop() del STM32 lo comparte con el AY, la voz y el
// WAV -- pero no puede desaparecer: es la unica via por la que sale lo que el
// Z80 escribe, incluido el "ATDT host:puerto" del interprete AT (ver
// NET_BRIDGE.cpp: modo comando/datos, ATDT/ATH/ATO/ATE/ATZ, "+++").
void net_bridge_loop();

// Abre/cierra la conexion. Es el UNICO sitio que toca el modo comando/datos
// del interprete AT (net_bridge_loop llama a estas mismas, tanto desde ATDT/
// ATH/ATZ como desde el handler de la pagina /telnet), asi que da igual por
// donde se abra o cierre la conexion, el modo queda consistente.
bool net_bridge_connect(const char* host, uint16_t port);
void net_bridge_disconnect();

// Para NET_MQTT.cpp: respuestas a comandos AT y datos crudos del Z80.
// La cola de salida es la misma que usa el interprete AT.
void     net_bridge_txq_push_str(const char* s);
void     net_bridge_txq_push_bytes(const uint8_t* b, uint16_t n);
uint16_t net_bridge_txq_free();

// Recibir `len` bytes (1..256) del Z80 sin interpretarlos ni hacerles eco, tras
// mandarle ">". Al completarse llama a done(datos, len, true); si el Z80 no los
// manda en 5 s, done(NULL, 0, false). Devuelve false si ya habia una recepcion
// en curso o len no vale (en ese caso done no se llama).
typedef void (*net_raw_done_t)(const uint8_t* data, uint16_t len, bool ok);
bool net_bridge_begin_raw(uint16_t len, net_raw_done_t done);

uint8_t     net_bridge_state();       // WifiProtoNetStatus
const char* net_bridge_host();        // ultimo destino, "" si nunca hubo
uint16_t    net_bridge_port();

#endif
