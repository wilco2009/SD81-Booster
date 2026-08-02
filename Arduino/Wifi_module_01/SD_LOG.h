#ifndef SD_LOG_H
#define SD_LOG_H

#include <Arduino.h>

// Espeja toda la salida por Serial tambien a /SYS/ESP32_LOG.TXT en la SD -
// el conector USB del ESP32 normalmente no es accesible una vez montado el
// interface dentro de la carcasa, asi que sin esto no hay forma de ver los
// logs salvo desmontando la caja.
//
// Funciona redefiniendo "Serial" como macro hacia esta clase en CUALQUIER
// fichero que incluya esta cabecera - todos los Serial.print/println/printf
// ya existentes salen tambien al log sin tocar cada punto de uso. SD_LOG.cpp
// desactiva la redefinicion para si mismo (unico sitio que debe hablar con
// el Serial fisico de verdad) definiendo SD_LOG_IMPLEMENTATION antes de
// incluir esta cabecera.
class SdLogger : public Print {
public:
  void begin(unsigned long baud);
  size_t write(uint8_t c) override;
  size_t write(const uint8_t* buf, size_t size) override;
};

extern SdLogger SDLog;

// Intenta volcar a la SD lo que haya pendiente en el buffer. Sin bloqueo
// largo: si el enlace con el STM32 no esta listo todavia (arranque) o no
// hay nada pendiente, no hace nada. Llamar periodicamente desde loop() y
// una vez explicitamente en cuanto el STM32 responda en setup().
void sdlog_try_flush();

// Vuelca lo pendiente y cierra el handle - llamar justo antes de
// ESP.restart() para dejar el fichero bien finalizado.
void sdlog_close();

#ifndef SD_LOG_IMPLEMENTATION
#define Serial SDLog
#endif

#endif
