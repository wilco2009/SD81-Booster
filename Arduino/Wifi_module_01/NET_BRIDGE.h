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

uint8_t     net_bridge_state();       // WifiProtoNetStatus
const char* net_bridge_host();        // ultimo destino, "" si nunca hubo
uint16_t    net_bridge_port();

#endif
