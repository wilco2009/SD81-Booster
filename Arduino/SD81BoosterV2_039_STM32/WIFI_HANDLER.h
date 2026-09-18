#ifndef WIFI_HANDLER_H
#define WIFI_HANDLER_H

#include <Arduino.h>

void wifi_handler_init();
void wifi_handler_poll();   // llamar desde loop(); no bloquea si no hay trama pendiente

// --- Puente de red (BBS/telnet) --------------------------------------------
// Dos buffers circulares entre el modulo WiFi y el Z80. El STM32 no
// interpreta nada: el socket, el destino y los comandos AT viven en el ESP32.
// De aqui colgaran los comandos MCU 66/67 (cmd_net_read/cmd_net_write).

// Saca hasta `max` bytes de lo recibido de la red. Devuelve cuantos habia
// realmente, que puede ser 0.
uint16_t net_bridge_read(uint8_t* dst, uint16_t max);

// Encola bytes para enviar a la red. Devuelve cuantos ACEPTO: si el buffer
// esta lleno puede ser menor que `len`, y el llamante decide si reintenta el
// resto (el Z80 lo necesita para saber cuantos reenviar).
uint16_t net_bridge_write(const uint8_t* src, uint16_t len);

uint16_t net_bridge_available();   // bytes pendientes de leer
uint8_t  net_bridge_status();      // WifiProtoNetStatus, tal cual lo reporta el ESP32

#endif
