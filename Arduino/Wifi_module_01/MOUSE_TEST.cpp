#include "MOUSE_TEST.h"

#if ESP_MOUSE_TEST

#include "WIFI_CLIENT.h"
#include "LOGCFG.h"
#include "SD_LOG.h"      // redefine Serial: lo que salga por Serial se copia tambien a /SYS/ESP32LOG.TXT (si SDLOG=1)

// El envio corre en su PROPIA tarea de FreeRTOS (prioridad mayor que la de loop()), de modo que las paradas de loop() (web de depuracion,
// volcado a la SD...) no retrasan los informes. La UART se comparte con el resto del firmware por el mutex de wifi_client_request().
// El resumen sigue saliendo desde loop() (mouse_test_tick): las estadisticas se pasan de una tarea a otra dentro de una seccion critica.

// Tramos del histograma de RTT (limite superior de cada uno, en us); el ultimo es "mas de 50 ms"
static const uint32_t EDGE_US[] = { 500, 1000, 2000, 3000, 5000, 8000, 12000, 20000, 50000 };
static const int NB = sizeof(EDGE_US) / sizeof(EDGE_US[0]) + 1;

struct Acc {
  uint32_t n = 0, sum = 0, mx = 0;
  uint32_t mn = 0xFFFFFFFFu;
  void add(uint32_t v) { n++; sum += v; if (v > mx) mx = v; if (v < mn) mn = v; }
  uint32_t avg() const { return n ? sum / n : 0; }
};

struct Stats {
  Acc rtt, gap, proc, late;
  uint32_t hist[10] = {0};
  uint32_t sent = 0, ok = 0, fail = 0, skipped = 0, bad = 0;
};

static Stats S;                                   // lo escribe la tarea del raton, lo recoge y pone a cero loop()
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static TaskHandle_t s_task = nullptr;

static uint32_t percentile_us(const Stats& st, int pct) {   // cota superior del tramo donde cae el percentil; 0 = no hay datos
  uint32_t total = 0;
  for (int i = 0; i < NB; i++) total += st.hist[i];
  if (!total) return 0;
  uint32_t want = (total * pct + 99) / 100, acc = 0;
  for (int i = 0; i < NB; i++) {
    acc += st.hist[i];
    if (acc >= want) return i < NB - 1 ? EDGE_US[i] : st.rtt.mx;
  }
  return st.rtt.mx;
}

static char s_last[640] = "";                  // el ultimo resumen, lo lee la orden "esp mouse" (la misma tarea que lo escribe: loop())

static void report(const Stats& st, uint32_t secs) {
  char l1[200], l2[200], l3[160];
  snprintf(l1, sizeof l1, "[MOUSE] %uHz %us (tarea): enviados=%lu ok=%lu fallos=%lu seq_mal=%lu saltados=%lu | RTT us min/med/max=%lu/%lu/%lu p50/95/99<=%lu/%lu/%lu",
           (unsigned)MOUSE_TEST_HZ, (unsigned)secs, (unsigned long)st.sent, (unsigned long)st.ok, (unsigned long)st.fail, (unsigned long)st.bad, (unsigned long)st.skipped,
           (unsigned long)(st.rtt.n ? st.rtt.mn : 0), (unsigned long)st.rtt.avg(), (unsigned long)st.rtt.mx,
           (unsigned long)percentile_us(st, 50), (unsigned long)percentile_us(st, 95), (unsigned long)percentile_us(st, 99));
  snprintf(l2, sizeof l2, "[MOUSE] hist(<0,5 <1 <2 <3 <5 <8 <12 <20 <50 >=50 ms)= %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu",
           (unsigned long)st.hist[0], (unsigned long)st.hist[1], (unsigned long)st.hist[2], (unsigned long)st.hist[3], (unsigned long)st.hist[4],
           (unsigned long)st.hist[5], (unsigned long)st.hist[6], (unsigned long)st.hist[7], (unsigned long)st.hist[8], (unsigned long)st.hist[9]);
  snprintf(l3, sizeof l3, "[MOUSE] STM32 gap med/max=%lu/%lu us  proc med/max=%lu/%lu us | retraso del despertar med/max=%lu/%lu us",
           (unsigned long)st.gap.avg(), (unsigned long)st.gap.mx, (unsigned long)st.proc.avg(), (unsigned long)st.proc.mx,
           (unsigned long)st.late.avg(), (unsigned long)st.late.mx);
  Serial.println(l1); Serial.println(l2); Serial.println(l3);
  snprintf(s_last, sizeof s_last, "%s\n%s\n%s\n", l1, l2, l3);
}

// Un informe falso por periodo. RTT = desde que la tarea llama a wifi_client_request() hasta tener la respuesta: INCLUYE esperar al mutex si
// loop() esta a mitad de una peticion al STM32. "Retraso" = lo que se retrasa el despertar de la tarea respecto a su hora (planificador).
static void mouse_task(void*) {
  const uint32_t PERIOD_US = 1000000UL / MOUSE_TEST_HZ;
  const TickType_t PERIOD_TICKS = pdMS_TO_TICKS(1000 / MOUSE_TEST_HZ);
  uint16_t seq = 0;
  uint32_t n = 0;                                 // numero de periodo
  uint32_t t0 = micros();                         // hora teorica del periodo 0
  int32_t min_off = 0x7FFFFFFF;                   // el desfase menor visto (reloj de ticks frente a micros()): el cero del retraso
  TickType_t last = xTaskGetTickCount();
  bool paused = false;

  for (;;) {
    if (!g_logcfg.mouse) {                        // parada (MOUSE=0 en LOG.CFG o "esp mouse off"): no manda nada
      paused = true;
      vTaskDelay(pdMS_TO_TICKS(50));
      continue;
    }
    if (paused) {                                 // al reanudar se empieza de cero (si no, saldrian como perdidos todos los periodos de la pausa)
      paused = false;
      t0 = micros(); n = 0; min_off = 0x7FFFFFFF; last = xTaskGetTickCount();
    }
    vTaskDelayUntil(&last, PERIOD_TICKS);
    uint32_t now = micros();
    int32_t off = (int32_t)(now - (t0 + n * PERIOD_US));
    if (off < min_off) min_off = off;
    uint32_t behind = (uint32_t)(off - min_off);  // cuanto se ha retrasado el despertar respecto al mejor caso
    uint32_t missed = behind / PERIOD_US;         // periodos enteros perdidos (la tarea estaba bloqueada)
    if (missed) { n += missed; last += missed * PERIOD_TICKS; min_off = 0x7FFFFFFF; }   // salta los periodos perdidos y vuelve a tomar el cero del retraso (tras una parada larga se vio un desfase fijo de 5-8 ms)
    n++;

    uint8_t req[8];
    req[0] = (uint8_t)(seq & 0xFF); req[1] = (uint8_t)(seq >> 8);
    req[2] = (uint8_t)(seq >> 5) & 3;
    int16_t dx = (int16_t)(seq % 7) - 3, dy = (int16_t)(seq % 5) - 2;
    memcpy(&req[3], &dx, 2); memcpy(&req[5], &dy, 2);
    req[7] = 0;

    uint32_t t0r = micros();
    WifiProtoResp r = wifi_client_request(CMD_MOUSE_TEST, req, sizeof req);
    uint32_t dt = micros() - t0r;
    uint32_t sent_late = behind % PERIOD_US;

    bool good = r.ok && r.len >= 11 && r.payload[0] == ST_OK;
    uint16_t echo = good ? ((uint16_t)r.payload[1] | ((uint16_t)r.payload[2] << 8)) : 0;
    uint32_t g = 0, p = 0;
    if (good) { memcpy(&g, &r.payload[3], 4); memcpy(&p, &r.payload[7], 4); }

    portENTER_CRITICAL(&s_mux);
    S.sent++;
    S.skipped += missed;
    S.late.add(sent_late);
    if (!good) {
      S.fail++;
    } else {
      if (echo != seq) S.bad++;
      S.ok++;
      S.rtt.add(dt);
      int b = 0;
      while (b < NB - 1 && dt >= EDGE_US[b]) b++;
      S.hist[b]++;
      S.gap.add(g); S.proc.add(p);
    }
    portEXIT_CRITICAL(&s_mux);
    seq++;
  }
}

String mouse_test_last_report() { return String(s_last); }

void mouse_test_tick() {
  static uint32_t report_ms = 0;

  if (!s_task) {                                  // 3 s de margen tras el arranque (WiFi, SD, STM32 listo); y solo si esta activada
    if (!g_logcfg.mouse || millis() < 3000) return;
    report_ms = millis();
    xTaskCreate(mouse_task, "mouse_test", 4096, nullptr, 5, &s_task);   // prioridad 5: por encima de loop() (1), por debajo de WiFi/lwip
    return;
  }

  if (!g_logcfg.mouse) {                          // parada: nada que resumir; al reanudar se parte de estadisticas limpias
    portENTER_CRITICAL(&s_mux);
    S = Stats();
    portEXIT_CRITICAL(&s_mux);
    report_ms = millis();
    return;
  }

  if ((uint32_t)(millis() - report_ms) >= MOUSE_TEST_REPORT_S * 1000UL) {
    Stats snap;
    portENTER_CRITICAL(&s_mux);
    snap = S;
    S = Stats();
    portEXIT_CRITICAL(&s_mux);
    report(snap, (millis() - report_ms) / 1000);
    report_ms = millis();
  }
}

#endif
