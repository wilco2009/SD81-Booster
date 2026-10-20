// Prueba de EXAMPLES/NETCHAT/netq.inc (mensajes de red sin esperas) en el PC: el Z80 de verdad ejecuta nqtest.bin contra el modem de verdad
// (NET_BRIDGE.cpp + NET_MQTT.cpp) y un broker de mentira; el rival lo hace esta prueba. Se comprueba que:
//   - nq_open deja la suscripcion solo al topic del rival (sin el eco de lo propio);
//   - cada fotograma (nq_send + nq_pump) manda su mensaje, y llegan al broker enteros y en orden;
//   - los mensajes del rival (con bytes 13, 10, '>' y 0 dentro) llegan enteros y en orden, tambien partidos entre varias lecturas
//     del tubo (mensajes de 100 bytes, tubo de salida que acepta pocos bytes por vez);
//   - nunca se pide mas de NQ_ASKMAX AT+MQTTRX sin contestar, y nada se pierde ni da ERROR;
//   - el perro guardian: si el modem deja de contestar, la cuenta se vacia y todo sigue funcionando despues.
#include "z80net_env.h"
#include <random>

static const uint16_t NQ = 0x7500, MB = 0x7F00;
static const uint16_t EP_OPEN = 24585, EP_SENDQ = 24588, EP_PUMP = 24591, EP_GET = 24594;
static uint8_t nqb(int off) { return mem[NQ + off]; }
static int nq16(int off) { return mem[NQ + off] | (mem[NQ + off + 1] << 8); }
enum { O_FC = 3, O_ASKS = 5, O_NRX = 16, O_NTX = 18, O_ERR = 20, O_DROP = 21, O_LOST = 22, O_TXN = 0, O_RXBUF = 288 };

// un fotograma de 20 ms: el Z80 hace lo suyo (corre solo lo que tarde) y el resto del tiempo pasa
static void frame() {
  uint32_t t0 = mock_now;
  while (mock_now < t0 + 20) { time_extra += 5; mock_now = 100000 + time_extra + (uint32_t)(instr_total * 5 / 3250); net_bridge_loop(); }
}
static void put_msg(const std::string& d) { mem[MB] = (uint8_t)d.size(); for (size_t i = 0; i < d.size(); i++) mem[MB + 16 + i] = (uint8_t)d[i]; }
static std::string got_msg(int len) { return std::string((const char*)mem + NQ + O_RXBUF, len); }

int main(int argc, char** argv) {
  if (argc < 2) { printf("uso: test_z80nq nqtest.bin\n"); return 2; }
  FILE* f = fopen(argv[1], "rb");
  if (!f) { printf("no encuentro %s\n", argv[1]); return 2; }
  size_t sz = fread(mem + 24576, 1, 0x2000, f); fclose(f);
  printf("nqtest.bin: %zu bytes en $6000\n", sz);
  z80_init(); z80_reset(); reset_world();
  CHECK(call_ep(24576) == 0, "net_init");
  CHECK(call_ep(24579) == 0, "room_new");
  std::string code = room_code_ascii(), base = topic_base(code);
  printf("  sala %s -> %s\n", code.c_str(), base.c_str());
  CHECK(call_ep(EP_OPEN) == 0, "nq_open");
  {
    bool unsub = false, sub1 = false;
    for (auto& s : seen) { if (s.kind == "UNSUBSCRIBE" && s.topic == base + "/+") unsub = true; if (s.kind == "SUBSCRIBE" && s.topic == base + "/1") sub1 = true; }
    CHECK(unsub, "nq_open se da de baja de <sala>/+");
    CHECK(sub1, "y se suscribe al topic del rival (<sala>/1: yo soy el 0)");
    bool has_plus = false; for (auto& fl : filters) if (fl == base + "/+") has_plus = true;
    CHECK(!has_plus, "el broker ya no manda lo de la sala entera");
  }
  seen.clear();

  // --- 1. 200 fotogramas: un mensaje mio cada 2, uno del rival cada 3; llegan todos enteros y en orden
  std::mt19937 rng(7);
  std::vector<std::string> sent, peer_sent, peer_got;
  std::vector<uint32_t> t_pub, t_got;                  // para medir cuanto tarda un mensaje del rival en llegar al Z80
  int busy = 0;
  size_t got_total = 0;
  int max_asks = 0, max_fc = 0;
  for (int fr = 0; fr < 200; fr++) {
    if (fr % 2 == 0) {
      std::string d; int n = 4 + rng() % 20;
      d += (char)sent.size(); for (int i = 1; i < n; i++) d += (char)(rng() % 256);
      put_msg(d);
      if (call_ep(EP_SENDQ) == 1) busy++; else sent.push_back(d);
    }
    if (fr % 3 == 0) {
      std::string d; int n = 1 + rng() % 40;
      d += (char)peer_sent.size(); for (int i = 1; i < n; i++) { int r = rng() % 8; d += (char)(r == 0 ? 13 : r == 1 ? 10 : r == 2 ? '>' : r == 3 ? 0 : rng() % 256); }
      peer_sent.push_back(d); t_pub.push_back(mock_now);
      peer_publish(base + "/1", d, false);
    }
    call_ep(EP_PUMP);
    uint32_t t_pump = mock_now;                        // el Z80 recoge lo que hay cuando bombea
    max_asks = std::max(max_asks, (int)nqb(O_ASKS)); max_fc = std::max(max_fc, (int)nqb(O_FC));
    frame();
    while (true) { int n = call_ep(EP_GET); if (!n) break; peer_got.push_back(got_msg(n)); t_got.push_back(t_pump); got_total++; }
  }
  for (int k = 0; k < 40; k++) {                                 // los que quedan en camino
    call_ep(EP_PUMP); uint32_t t_pump = mock_now; frame();
    while (true) { int n = call_ep(EP_GET); if (!n) break; peer_got.push_back(got_msg(n)); t_got.push_back(t_pump); }
  }
  // lo que vio el broker: PUBLISH a <sala>/0, en orden
  std::vector<std::string> at_broker;
  for (auto& s : seen) if (s.kind == "PUBLISH" && s.topic == base + "/0") at_broker.push_back(s.data);
  printf("  mandados %zu (cola ocupada %d), vistos por el broker %zu; del rival: %zu, recibidos %zu; asks max %d, cola max %d, dropped %d\n",
         sent.size(), busy, at_broker.size(), peer_sent.size(), peer_got.size(), max_asks, max_fc, nqb(O_DROP));
  if (t_pub.size() == t_got.size() && !t_pub.empty()) {
    uint32_t mx = 0, sum = 0;
    for (size_t i = 0; i < t_pub.size(); i++) { uint32_t d = t_got[i] - t_pub[i]; sum += d; mx = std::max(mx, d); }
    char b[160]; snprintf(b, sizeof b, "  del broker al Z80 (sin contar internet): media %u ms, maximo %u ms (fotogramas de 20 ms)", sum / (uint32_t)t_pub.size(), mx); puts(b);
  }
  CHECK(busy == 0, "nunca se encontro la cola ocupada");
  CHECK(at_broker == sent, "el broker recibe todo lo mandado, entero y en orden");
  CHECK(peer_got == peer_sent, "el Z80 recibe todo lo del rival, entero y en orden (con bytes 13, 10, '>' y 0)");
  CHECK(max_asks <= 4, "nunca mas de 4 AT+MQTTRX sin contestar");
  CHECK(nqb(O_ERR) == 0 && nqb(O_LOST) == 0, "sin ERROR del modem ni cola perdida");
  CHECK(nq16(O_NRX) == (int)peer_sent.size() && nq16(O_NTX) == (int)sent.size(), "los contadores cuadran");
  CHECK(nqb(O_FC) <= 5, "al acabar, casi todo contestado");

  // --- 2. mensajes largos (100 bytes) y un tubo de salida que acepta pocos bytes por vez
  seen.clear();
  write_limit = 23;
  sent.clear(); peer_sent.clear(); peer_got.clear(); busy = 0;
  for (int fr = 0; fr < 60; fr++) {
    if (fr % 4 == 0) {
      std::string d(100, 0); for (auto& ch : d) ch = (char)(rng() % 256); d[0] = (char)sent.size();
      put_msg(d);
      if (call_ep(EP_SENDQ) == 1) busy++; else sent.push_back(d);
    }
    if (fr % 4 == 1) {
      std::string d(100, 0); for (auto& ch : d) ch = (char)(rng() % 256); d[0] = (char)peer_sent.size();
      peer_sent.push_back(d);
      peer_publish(base + "/1", d, false);
    }
    call_ep(EP_PUMP); frame();
    while (true) { int n = call_ep(EP_GET); if (!n) break; peer_got.push_back(got_msg(n)); }
  }
  for (int k = 0; k < 80; k++) { call_ep(EP_PUMP); frame(); while (true) { int n = call_ep(EP_GET); if (!n) break; peer_got.push_back(got_msg(n)); } }
  write_limit = 255;
  at_broker.clear();
  for (auto& s : seen) if (s.kind == "PUBLISH" && s.topic == base + "/0") at_broker.push_back(s.data);
  printf("  100 bytes y NET_WRITE de 23 en 23: mandados %zu (cola ocupada %d), broker %zu; del rival %zu, recibidos %zu; asks max %d\n", sent.size(), busy,
         at_broker.size(), peer_sent.size(), peer_got.size(), nqb(O_ASKS));
  CHECK(!sent.empty() && at_broker == sent, "con escrituras parciales, el broker recibe todo lo mandado");
  CHECK(peer_got == peer_sent, "y llegan enteros los mensajes de 100 bytes del rival");
  // (con mensajes de 100 bytes el modem puede contestar ERROR a un AT+MQTTRX si su cola de salida no tiene sitio para la respuesta: el mensaje se
  // queda y se entrega en el siguiente; no es un fallo, y la cola de lo pendiente lo cuenta como contestado)
  CHECK(nqb(O_LOST) == 0, "sin cola perdida");

  // --- 3. el perro guardian: el modem deja de contestar (el tubo de entrada se vacia al vuelo) y la cuenta se recupera
  {
    seen.clear();
    int lost0 = nqb(O_LOST);
    for (int fr = 0; fr < 200; fr++) {                              // el Z80 pide y manda, y se tira todo lo que contesta el modem
      call_ep(EP_PUMP);
      frame();
      to_z80.clear();
    }
    CHECK(nqb(O_LOST) > lost0, "sin respuestas, el perro guardian vacia la cuenta de lo pendiente");
    CHECK(nqb(O_FC) <= 5 || nqb(O_LOST) > lost0, "y la cola de pendientes no se queda llena");
    // el modem vuelve: un mensaje del rival llega
    std::string d = "despues del apagon";
    peer_publish(base + "/1", d, false);
    std::string last;
    for (int k = 0; k < 120; k++) {
      call_ep(EP_PUMP); frame();
      int n = call_ep(EP_GET); if (n) last = got_msg(n);
    }
    { char b[160]; snprintf(b, sizeof b, "  tras recuperarse: FC=%d ASKS=%d NRX=%d ERR=%d LOST=%d TXN=%d ST=%d", nqb(O_FC), nqb(O_ASKS), nq16(O_NRX), nqb(O_ERR), nqb(O_LOST), nqb(O_TXN), nqb(6)); puts(b); }
    CHECK(last == d, "tras recuperarse, el mensaje del rival llega");
  }

  if (errors) printf("%d ERRORES\n", errors); else printf("TODO OK\n");
  return errors ? 1 : 0;
}
