#ifndef WIFI_HANDLER_H
#define WIFI_HANDLER_H

#include <Arduino.h>

void wifi_handler_init();
void wifi_handler_poll();   // llamar desde loop(); no bloquea si no hay trama pendiente

#endif
