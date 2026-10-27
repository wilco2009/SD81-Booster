#include "DIAG.h"

#if ESP_DIAG

#include <WiFi.h>
#include <stdarg.h>
#include "SD_LOG.h"
#include "NET_MQTT.h"

// OJO: SD_LOG.h redefine "Serial"; este fichero no escribe en el Serial (para no mezclarse con el log normal).

uint8_t diag_fail_why = 0;

static const char* const SEC_NAME[DG_N] = { "web", "ntp/wifi", "mqtt", "poll", "sdlog" };

// Historial circular de texto: `wr` cuenta todos los bytes escritos (crece siempre) y `dumped` los ya volcados a la SD. La pagina /diag
// sirve el historial entero (los ultimos DIAG_RING bytes), que no se borra al volcar.
static char     hist[DIAG_RING];
static uint32_t wr = 0, dumped = 0;
static uint32_t dropped = 0;           // bytes que se sobrescribieron sin haberse podido volcar
static bool     busy = false;          // volcando: lo que ocurra en la propia escritura a la SD no se apunta

// estadisticas del periodo del resumen
static uint32_t st_req = 0, st_retry = 0, st_ferr = 0, st_stale = 0, st_reqmax = 0, st_fail = 0, st_loopmax = 0;
static uint32_t st_sec[DG_N];
static uint32_t last_loop = 0, last_summary = 0, last_dump = 0;
static uint32_t last_dump_ms = 0, last_dump_len = 0; static bool last_dump_ok = true, have_dump = false;

static void put(const char* s, size_t n) {
  for (size_t i = 0; i < n; i++) hist[(wr + i) % DIAG_RING] = s[i];
  wr += n;
  if (wr - dumped > DIAG_RING) { dropped += (wr - dumped) - DIAG_RING; dumped = wr - DIAG_RING; }
}

void diag_event(const char* fmt, ...) {
  if (busy) return;
  char line[300];
  uint32_t t = millis();
  int n = snprintf(line, sizeof line, "[D %lu.%03lu] ", (unsigned long)(t / 1000), (unsigned long)(t % 1000));
  va_list ap; va_start(ap, fmt);
  int m = vsnprintf(line + n, sizeof line - n - 2, fmt, ap);
  va_end(ap);
  if (m < 0) return;
  n += m; if (n > (int)sizeof line - 2) n = sizeof line - 2;
  line[n++] = '\n';
  put(line, n);
}

void diag_req(uint8_t cmd, uint8_t attempts, uint32_t ms, bool ok, uint8_t frame_err, uint8_t stale) {
  if (busy) return;
  st_req++;
  if (attempts > 1) st_retry += attempts - 1;
  st_ferr += frame_err; st_stale += stale;
  if (!ok) st_fail++;
  if (ms > st_reqmax) st_reqmax = ms;
  if (attempts > 1 || ms >= DIAG_SLOW_MS || !ok || frame_err || stale)
    diag_event("UART cmd=%u intentos=%u %lu ms ok=%d frame_error=%u respuestas_ajenas=%u motivo_fallo=%u",
               cmd, attempts, (unsigned long)ms, ok ? 1 : 0, frame_err, stale, diag_fail_why);
}

void diag_section(uint8_t id, uint32_t ms) {
  if (busy || id >= DG_N) return;
  if (ms > st_sec[id]) st_sec[id] = ms;
  if (ms >= DIAG_SLOW_MS) diag_event("LENTO tramo=%s %lu ms", SEC_NAME[id], (unsigned long)ms);
}

// Vuelca a la SD lo que no se haya volcado ya (hasta dos trozos si el historial ha dado la vuelta).
static void dump_now() {
  if (wr == dumped) return;
  busy = true;
  uint32_t t0 = millis();
  uint32_t total = wr - dumped;
  bool ok = true;
  uint32_t pos = dumped;
  while (ok && pos < wr) {
    uint32_t off = pos % DIAG_RING;
    uint32_t n = wr - pos; if (n > DIAG_RING - off) n = DIAG_RING - off;
    ok = sdlog_write_big((const uint8_t*)hist + off, n);
    if (ok) pos += n;
  }
  uint32_t dt = millis() - t0;
  busy = false;
  dumped = pos;
  last_dump = millis();
  last_dump_ms = dt; last_dump_len = total; last_dump_ok = ok; have_dump = true;   // se apunta en el resumen siguiente
}

void diag_flush() {
  if (!busy) dump_now();
}

size_t diag_read(char* out, size_t max) {
  uint32_t have = wr < DIAG_RING ? wr : DIAG_RING;
  if (have > max) have = max;
  uint32_t start = wr - have;
  for (uint32_t i = 0; i < have; i++) out[i] = hist[(start + i) % DIAG_RING];
  return have;
}

void diag_loop() {
  if (busy) return;
  uint32_t now = millis();
  if (last_loop) {
    uint32_t gap = now - last_loop;
    if (gap > st_loopmax) st_loopmax = gap;
    if (gap >= DIAG_SLOW_MS) diag_event("VUELTA lenta de loop(): %lu ms entre dos pasadas", (unsigned long)gap);
  }
  last_loop = now;

  if (last_summary == 0) {
    last_summary = now;
    last_dump = now - (DIAG_DUMP_S * 1000UL - 10000UL);   // el primer volcado, a los 10 s
    diag_event("ACTIVO umbral=%d ms resumen=%d s volcado=%d s historial=%d bytes (tambien en http://<IP>/diag)",
               DIAG_SLOW_MS, DIAG_SUMMARY_S, DIAG_DUMP_S, DIAG_RING);
  }
  if ((uint32_t)(now - last_summary) >= DIAG_SUMMARY_S * 1000UL) {
    last_summary = now;
    diag_event("RESUMEN canal=%d ap=%s rssi=%d wifi=%d mqtt=%d req=%lu reintentos=%lu frame_err=%lu ajenas=%lu fallos=%lu reqmax=%lu ms "
               "vueltamax=%lu ms tramos[web=%lu ntp/wifi=%lu mqtt=%lu poll=%lu sdlog=%lu] heap=%u min=%u perdidos=%lu",
               (int)WiFi.channel(), WiFi.BSSIDstr().c_str(), WiFi.RSSI(), (int)WiFi.status(), net_mqtt_active() ? 1 : 0,
               (unsigned long)st_req, (unsigned long)st_retry, (unsigned long)st_ferr, (unsigned long)st_stale, (unsigned long)st_fail,
               (unsigned long)st_reqmax, (unsigned long)st_loopmax,
               (unsigned long)st_sec[DG_WEB], (unsigned long)st_sec[DG_NTPWIFI], (unsigned long)st_sec[DG_MQTT],
               (unsigned long)st_sec[DG_POLL], (unsigned long)st_sec[DG_SDLOG],
               (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMinFreeHeap(), (unsigned long)dropped);
    if (have_dump) {
      diag_event("VOLCADO anterior: %lu bytes en %lu ms ok=%d", (unsigned long)last_dump_len, (unsigned long)last_dump_ms, last_dump_ok ? 1 : 0);
      have_dump = false;
    }
    st_req = st_retry = st_ferr = st_stale = st_reqmax = st_fail = st_loopmax = 0;
    for (int i = 0; i < DG_N; i++) st_sec[i] = 0;
  }

  if (wr != dumped && ((uint32_t)(now - last_dump) >= DIAG_DUMP_S * 1000UL || wr - dumped > (DIAG_RING * 3) / 4)) dump_now();
}

#endif
