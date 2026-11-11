// LOGCFG.h - configuracion de los logs del ESP32 desde /SYS/LOG.CFG (se lee una vez al arrancar) y orden "esp" de la consola web.
//
// El mismo fichero lo lee tambien el STM32 (clave STM32LOG, ver SD81BoosterV2_039_STM32/LOGCFG.cpp): cada firmware ignora las claves del otro.
// Formato: lineas CLAVE=VALOR, `#` o `;` al principio = comentario, mayusculas o minusculas.
//
//   SDLOG=1          copiar todo lo que sale por Serial a /SYS/ESP32LOG.TXT (por defecto 1)
//   DIAG=0           diagnostico (DIAG.h) activo (por defecto 0). Hace falta que el firmware lleve el codigo (ESP_DIAG = 1)
//   DIAG_DUMP_S=30   cada cuantos segundos vuelca el diagnostico a la SD; 0 = nunca (solo en RAM: orden "esp diag" y http://<IP>/diag)
//   MOUSE=0          prueba sintetica del raton (MOUSE_TEST.h) en marcha (por defecto 0). Hace falta que el firmware lleve el codigo (ESP_MOUSE_TEST = 1)
//   STM32LOG=2       (lo lee el STM32) nivel de log del STM32 por USB: 0 nada, 1 errores, 2 errores+info (por defecto)
//
// Los flags de compilacion (ESP_DIAG, ESP_MOUSE_TEST) deciden si el CODIGO existe; este fichero decide si FUNCIONA.
#ifndef LOGCFG_H
#define LOGCFG_H

#include <Arduino.h>

struct LogCfg {
  bool     sdlog;
  bool     diag;
  uint16_t diag_dump_s;
  bool     mouse;      // prueba sintetica del raton (MOUSE_TEST.h) en marcha
};

extern LogCfg g_logcfg;

// Lee /SYS/LOG.CFG por el STM32 (hay que llamarla cuando el STM32 ya contesta). Sin fichero o sin las claves, quedan los valores por defecto.
void logcfg_load();

// Orden "esp ..." de la consola web (/debug). `args` es lo que va detras de "esp". Devuelve el texto de respuesta (acabado en \n).
String esp_console_cmd(const String& args);

#endif
