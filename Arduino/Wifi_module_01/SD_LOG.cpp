#define SD_LOG_IMPLEMENTATION
#include "SD_LOG.h"
#include "WIFI_CLIENT.h"

#define SD_LOG_PATH            "/SYS/ESP32LOG.TXT"
// DEBE caber en un unico WRITE_CHUNK - wifi_client_write_chunk() tiene un
// buffer interno de tamano fijo (3 + WIFI_PROTO_CHUNK_SIZE); pasarle mas de
// WIFI_PROTO_CHUNK_SIZE bytes de una vez desborda ese buffer en la pila.
#define SD_LOG_BUF_SIZE        WIFI_PROTO_CHUNK_SIZE
#define SD_LOG_FLUSH_MS        3000   // frecuencia de sondeo normal desde loop()
#define SD_LOG_BACKOFF_MS      30000  // frecuencia tras varios fallos seguidos (ver mas abajo)
#define SD_LOG_BACKOFF_AFTER   3      // fallos consecutivos que activan el backoff

SdLogger SDLog;

static uint8_t  g_buf[SD_LOG_BUF_SIZE];
static size_t   g_buf_len = 0;
static bool     g_handle_open = false;
static uint8_t  g_handle = 0;
static uint32_t g_last_flush_attempt = 0;
static uint8_t  g_consecutive_failures = 0;

void SdLogger::begin(unsigned long baud) {
  Serial.begin(baud); // aqui "Serial" es el de verdad (SD_LOG_IMPLEMENTATION arriba)
}

size_t SdLogger::write(uint8_t c) {
  Serial.write(c);
  if (g_buf_len < SD_LOG_BUF_SIZE) g_buf[g_buf_len++] = c;
  // buffer lleno y sin poder volcar todavia: se descartan bytes nuevos en
  // vez de crecer sin limite - perdida aceptable en un log de diagnostico.
  return 1;
}

size_t SdLogger::write(const uint8_t* buf, size_t size) {
  Serial.write(buf, size);
  size_t room = SD_LOG_BUF_SIZE - g_buf_len;
  size_t take = (size < room) ? size : room;
  memcpy(g_buf + g_buf_len, buf, take);
  g_buf_len += take;
  return size;
}

// Se mantiene el handle abierto durante toda la sesion (el protocolo actual
// no tiene modo "append", solo truncar-al-abrir) para no perder lo ya
// escrito en vuelcos anteriores. Consume 1 de los 2 handles del STM32 de
// forma permanente - aceptable para un uso tipico de un solo navegador a
// la vez.
//
// El log a SD es un "extra", nunca debe perjudicar la consola por USB (que
// sigue funcionando siempre, de forma inmediata, sin pasar por nada de
// esto - ver SdLogger::write). El riesgo real es otro: cada llamada
// wifi_client_* tiene su propio reintento/timeout interno (hasta ~1.5s) si
// el STM32 no responde, y como este flush se ejecuta dentro de loop(), un
// enlace caido haria que CADA vuelta de loop() se congelase ese rato,
// retrasando tambien server.handleClient() y la siguiente linea de log -
// justo el peor momento si lo que se esta depurando es un problema de red.
// Por eso, tras varios fallos seguidos, se espacian mucho mas los
// reintentos (SD_LOG_BACKOFF_MS) en vez de insistir cada SD_LOG_FLUSH_MS.
void sdlog_try_flush() {
  if (g_buf_len == 0) return;
  uint32_t now = millis();
  uint32_t interval = (g_consecutive_failures >= SD_LOG_BACKOFF_AFTER) ? SD_LOG_BACKOFF_MS : SD_LOG_FLUSH_MS;
  if (now - g_last_flush_attempt < interval) return;
  g_last_flush_attempt = now;

  if (!g_handle_open) {
    if (!wifi_client_write_open(SD_LOG_PATH, &g_handle)) {
      if (g_consecutive_failures < 255) g_consecutive_failures++;
      return; // STM32 no listo aun / enlace caido, reintentar mas tarde
    }
    g_handle_open = true;
  }

  if (wifi_client_write_chunk(g_handle, g_buf, g_buf_len)) {
    g_buf_len = 0;
    g_consecutive_failures = 0; // enlace ok, volver a la frecuencia normal
    wifi_client_write_sync(g_handle); // para que el tamano sea visible sin cerrar el handle
  } else {
    if (g_consecutive_failures < 255) g_consecutive_failures++;
  }
  // si falla el chunk, se deja el buffer tal cual para reintentar despues
}

void sdlog_close() {
  g_last_flush_attempt = 0; // fuerza el volcado aunque no haya pasado SD_LOG_FLUSH_MS
  sdlog_try_flush();
  if (g_handle_open) {
    uint32_t total;
    wifi_client_write_close(g_handle, &total);
    g_handle_open = false;
  }
}
