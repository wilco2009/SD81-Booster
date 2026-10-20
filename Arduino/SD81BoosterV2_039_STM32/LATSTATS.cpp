// LATSTATS.cpp - ver LATSTATS.h. Solo compila algo con LAT_STATS = 1.
#include "LATSTATS.h"

#if LAT_STATS

#ifndef LAT_REPORT_MS
#define LAT_REPORT_MS 10000
#endif

struct LatAcc {
  uint32_t n, sum, mx;
  void clear() { n = 0; sum = 0; mx = 0; }
  void add(uint32_t v) { n++; sum += v; if (v > mx) mx = v; }
  uint32_t avg() const { return n ? sum / n : 0; }
};

static LatAcc a_loop, a_sec[LAT_S_N], a_gap, a_poll, a_w2o, a_o2i, a_i2r, a_tot;
static uint32_t over_700, over_1000, over_2000, over_5000, over_10000;
static uint32_t t_loop_prev = 0, t_mark = 0, t_poll_prev = 0, t_poll_start = 0;
static bool have_loop_prev = false, have_poll_prev = false;
static uint32_t t_w = 0, t_off = 0, t_in = 0;
static uint8_t chain = 0;                     // 0 nada, 1 el Z80 escribio, 2 ofrecido al ESP32, 3 llego la respuesta
static uint32_t t_report = 0;
static uint32_t gap_over_20ms = 0;

static void clear_all() {
  a_loop.clear(); a_gap.clear(); a_poll.clear(); a_w2o.clear(); a_o2i.clear(); a_i2r.clear(); a_tot.clear();
  for (uint8_t i = 0; i < LAT_S_N; i++) a_sec[i].clear();
  over_700 = over_1000 = over_2000 = over_5000 = over_10000 = 0;
  gap_over_20ms = 0;
  // imprimir el informe por USB tarda: esa vuelta y ese intervalo de tramas no cuentan (serian un maximo falso cada 10 s)
  have_loop_prev = false;
  have_poll_prev = false;
}

void lat_loop_begin() {
  uint32_t now = micros();
  if (have_loop_prev) {
    uint32_t d = now - t_loop_prev;
    a_loop.add(d);
    if (d > 700) over_700++;
    if (d > 1000) over_1000++;
    if (d > 2000) over_2000++;
    if (d > 5000) over_5000++;
    if (d > 10000) over_10000++;
  }
  have_loop_prev = true;
  t_loop_prev = now;
  t_mark = now;
}

void lat_end(uint8_t section) {
  uint32_t now = micros();
  if (section < LAT_S_N) a_sec[section].add(now - t_mark);
  t_mark = now;
}

static void print_line(const char* s) { Serial.println(s); }

void lat_loop_end() {
  uint32_t ms = millis();
  if (t_report == 0) t_report = ms;
  if (ms - t_report < LAT_REPORT_MS) return;
  t_report = ms;
  char b[200];
  static const char* names[LAT_S_N] = {"leer cmd", "ejecutar", "serie", "wifi", "musica", "ocioso"};
  snprintf(b, sizeof b, "[LAT] bucle: %lu vueltas, media %lu us, maxima %lu us | pasan de 0,7 ms: %lu, 1 ms: %lu, 2 ms: %lu, 5 ms: %lu, 10 ms: %lu",
           (unsigned long)a_loop.n, (unsigned long)a_loop.avg(), (unsigned long)a_loop.mx, (unsigned long)over_700, (unsigned long)over_1000,
           (unsigned long)over_2000, (unsigned long)over_5000, (unsigned long)over_10000);
  print_line(b);
  for (uint8_t i = 0; i < LAT_S_N; i++) {
    snprintf(b, sizeof b, "[LAT]   %-9s media %lu us, maxima %lu us", names[i], (unsigned long)a_sec[i].avg(), (unsigned long)a_sec[i].mx);
    print_line(b);
  }
  if (a_gap.n) {
    snprintf(b, sizeof b, "[LAT] tramas NET_POLL del ESP32: %lu; entre una y la siguiente: media %lu us, maxima %lu us (mas de 20 ms: %lu); contestarlas: media %lu us, maxima %lu us",
             (unsigned long)a_poll.n, (unsigned long)a_gap.avg(), (unsigned long)a_gap.mx, (unsigned long)gap_over_20ms, (unsigned long)a_poll.avg(),
             (unsigned long)a_poll.mx);
    print_line(b);
  }
  if (a_tot.n) {
    snprintf(b, sizeof b, "[LAT] puente (%lu pasadas) escribir->ofrecer %lu/%lu us, ofrecer->respuesta %lu/%lu us, respuesta->leer %lu/%lu us, TOTAL %lu/%lu us (media/maxima)",
             (unsigned long)a_tot.n, (unsigned long)a_w2o.avg(), (unsigned long)a_w2o.mx, (unsigned long)a_o2i.avg(), (unsigned long)a_o2i.mx,
             (unsigned long)a_i2r.avg(), (unsigned long)a_i2r.mx, (unsigned long)a_tot.avg(), (unsigned long)a_tot.mx);
    print_line(b);
  }
  clear_all();
}

void lat_poll_begin() {
  uint32_t now = micros();
  if (have_poll_prev) {
    uint32_t g = now - t_poll_prev;
    a_gap.add(g);
    if (g > 20000) gap_over_20ms++;
  }
  have_poll_prev = true;
  t_poll_prev = now;
  t_poll_start = now;
}

void lat_poll_end() { a_poll.add(micros() - t_poll_start); }

void lat_net_write() { t_w = micros(); chain = 1; }

void lat_net_offer() {
  if (chain != 1) return;
  t_off = micros();
  a_w2o.add(t_off - t_w);
  chain = 2;
}

void lat_net_in() {
  if (chain != 2) return;
  t_in = micros();
  a_o2i.add(t_in - t_off);
  chain = 3;
}

void lat_net_read() {
  if (chain != 3) return;
  uint32_t now = micros();
  a_i2r.add(now - t_in);
  a_tot.add(now - t_w);
  chain = 0;
}

#endif
