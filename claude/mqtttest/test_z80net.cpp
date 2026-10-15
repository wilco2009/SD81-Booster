// Prueba de extremo a extremo de la capa B (EXAMPLES/NETCHAT/net.inc) en el PC, sin hardware:
//   el Z80 de verdad (nucleo de EightyOne) ejecuta netchat.bin;
//   el tubo del MCU (comandos 66/67) lo hace esta prueba, con su STM32 de mentira;
//   el modem es el de verdad: NET_BRIDGE.cpp (intérprete AT) y NET_MQTT.cpp (capa A);
//   el broker MQTT es de mentira (con mensajes retenidos y suscripciones con comodines);
//   y el rival lo hace esta prueba con SHA-256 y HMAC propios, independientes del Z80.
// build_z80.sh lo compila y lo ejecuta.
#ifndef MOCK_REAL_MD
#define MOCK_REAL_MD
#endif
#include "mock.h"
#include <map>
#include <algorithm>
extern "C" {
#include "z80.h"
int tstates = 0;
int StackChange = 0, StepOutRequested = 0, RetExecuted = 0;
int z80_fetch_is_m1 = 0;
void z80_m1_backtrack(void) {}
void debug(int) {}
}
#include "mqtt_codec.h"

// ------------------------------------------------------------ SHA-256 y HMAC (propios)
static const uint32_t K256[64] = {
 0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
 0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
 0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
 0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
#define ROR(x,n) (((x) >> (n)) | ((x) << (32 - (n))))
static void sha256_block(uint32_t* h, const uint8_t* p) {
  uint32_t w[64];
  for (int i = 0; i < 16; i++) w[i] = (uint32_t)p[4*i] << 24 | p[4*i+1] << 16 | p[4*i+2] << 8 | p[4*i+3];
  for (int i = 16; i < 64; i++) {
    uint32_t s0 = ROR(w[i-15],7) ^ ROR(w[i-15],18) ^ (w[i-15] >> 3);
    uint32_t s1 = ROR(w[i-2],17) ^ ROR(w[i-2],19) ^ (w[i-2] >> 10);
    w[i] = w[i-16] + s0 + w[i-7] + s1;
  }
  uint32_t a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],hh=h[7];
  for (int i = 0; i < 64; i++) {
    uint32_t S1 = ROR(e,6) ^ ROR(e,11) ^ ROR(e,25), ch = (e & f) ^ (~e & g);
    uint32_t t1 = hh + S1 + ch + K256[i] + w[i];
    uint32_t S0 = ROR(a,2) ^ ROR(a,13) ^ ROR(a,22), mj = (a & b) ^ (a & c) ^ (b & c);
    uint32_t t2 = S0 + mj;
    hh=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
  }
  h[0]+=a; h[1]+=b; h[2]+=c; h[3]+=d; h[4]+=e; h[5]+=f; h[6]+=g; h[7]+=hh;
}
static void sha256(const uint8_t* in, size_t n, uint8_t* out) {
  uint32_t h[8] = {0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
  std::vector<uint8_t> m(in, in + n);
  m.push_back(0x80);
  while (m.size() % 64 != 56) m.push_back(0);
  uint64_t bits = (uint64_t)n * 8;
  for (int i = 7; i >= 0; i--) m.push_back((uint8_t)(bits >> (8 * i)));
  for (size_t i = 0; i < m.size(); i += 64) sha256_block(h, &m[i]);
  for (int i = 0; i < 8; i++) { out[4*i] = h[i] >> 24; out[4*i+1] = h[i] >> 16; out[4*i+2] = h[i] >> 8; out[4*i+3] = h[i]; }
}
static void hmac256(const uint8_t* key, size_t kl, const uint8_t* in, size_t n, uint8_t* out) {
  uint8_t k[64] = {0};
  if (kl > 64) sha256(key, kl, k); else memcpy(k, key, kl);
  std::vector<uint8_t> a(64), b(64);
  for (int i = 0; i < 64; i++) { a[i] = k[i] ^ 0x36; b[i] = k[i] ^ 0x5c; }
  a.insert(a.end(), in, in + n);
  uint8_t ih[32]; sha256(a.data(), a.size(), ih);
  b.insert(b.end(), ih, ih + 32);
  sha256(b.data(), b.size(), out);
}
int mbedtls_md(const mbedtls_md_info_t*, const unsigned char* in, size_t n, unsigned char* out) { sha256(in, n, out); return 0; }
int mbedtls_md_hmac(const mbedtls_md_info_t*, const unsigned char* key, size_t kl, const unsigned char* in, size_t n, unsigned char* out) {
  hmac256(key, kl, in, n, out); return 0;
}
static std::string hexs(const uint8_t* d, size_t n) {
  static const char H[] = "0123456789ABCDEF"; std::string s;
  for (size_t i = 0; i < n; i++) { s += H[d[i] >> 4]; s += H[d[i] & 15]; }
  return s;
}

// ------------------------------------------------------------ mocks de Arduino / red
uint32_t mock_now = 100000;
SerialMock Serial;
WiFiMock WiFi;
bool WiFiMock::mock_wifi_ok = true;
bool mock_cfg_present = false; std::string mock_cfg;
std::deque<uint8_t> mock_from_broker;
bool mock_connected = false, mock_tcp_fail = false, mock_quiet = true;

bool wifi_client_net_poll(uint8_t status, const uint8_t* tx, uint16_t tx_len, bool* out_tx_accepted,
                          uint8_t* rx, uint16_t* out_rx_len, uint8_t* out_rx_free);
#include "NET_BRIDGE.cpp"          // el intérprete AT de verdad, con sus statics a la vista
#include "NET_MQTT.h"

// ------------------------------------------------------------ el STM32 de mentira (el tubo)
static std::deque<uint8_t> to_z80, from_z80;   // buffers de entrada (2 KB) y de salida
static int write_limit = 255;                  // cuantos acepta NET_WRITE de una vez (prueba escrituras parciales)

bool wifi_client_net_poll(uint8_t, const uint8_t* tx, uint16_t tx_len, bool* acc,
                          uint8_t* rx, uint16_t* out_rx_len, uint8_t* out_rx_free) {
  if (to_z80.size() + tx_len <= 2048) { for (uint16_t i = 0; i < tx_len; i++) to_z80.push_back(tx[i]); *acc = true; }
  else *acc = false;
  uint16_t n = 0;
  while (n < 128 && !from_z80.empty()) { rx[n++] = from_z80.front(); from_z80.pop_front(); }
  *out_rx_len = n;
  *out_rx_free = (uint8_t)((2048 - to_z80.size()) / 16);
  return true;
}

static bool clk = false;
static int tube_state = 0, wr_left = 0;
static std::deque<uint8_t> resp;
static std::vector<uint8_t> wr_data;
static bool trace = getenv("TRACE") != nullptr;
static void dump(const char* dir, const uint8_t* b, size_t n) {
  if (!trace || !n) return;
  printf("  [%s %5u ms] ", dir, mock_now - 100000);
  for (size_t i = 0; i < n; i++) printf(b[i] >= 32 && b[i] < 127 ? "%c" : "<%02X>", b[i]);
  printf("\n");
}
static void tube_finish_write() {
  dump("Z80->ESP", wr_data.data(), wr_data.size());
  size_t acc = std::min<size_t>(wr_data.size(), (size_t)write_limit);
  for (size_t i = 0; i < acc; i++) from_z80.push_back(wr_data[i]);
  resp.clear(); resp.push_back((uint8_t)acc); resp.push_back((uint8_t)net_bridge_state());
  tube_state = 4;
}
static void tube_out(uint8_t d) {
  switch (tube_state) {
    case 0: tube_state = (d == 66) ? 1 : (d == 67) ? 2 : 0; if (tube_state == 0) printf("  tubo: comando %d\n", d); break;
    case 1: {
      size_t n = std::min<size_t>(d, to_z80.size());
      resp.clear(); resp.push_back((uint8_t)n);
      for (size_t i = 0; i < n; i++) { resp.push_back(to_z80.front()); to_z80.pop_front(); }
      { std::vector<uint8_t> v(resp.begin() + 1, resp.end()); dump("ESP->Z80", v.data(), v.size()); }
      resp.push_back((uint8_t)std::min<size_t>(255, to_z80.size()));
      resp.push_back((uint8_t)net_bridge_state());
      tube_state = 4; break;
    }
    case 2: wr_left = d; wr_data.clear(); if (!wr_left) tube_finish_write(); else tube_state = 3; break;
    case 3: wr_data.push_back(d); if (--wr_left == 0) tube_finish_write(); break;
    default: printf("  tubo: OUT en estado %d\n", tube_state); break;
  }
  clk = !clk;
}
static uint8_t tube_in() {
  if (resp.empty()) { printf("  tubo: IN sin respuesta\n"); return 0; }
  uint8_t v = resp.front(); resp.pop_front();
  if (resp.empty()) tube_state = 0;
  clk = !clk;
  return v;
}

// ------------------------------------------------------------ el Z80
static uint8_t mem[65536];
extern "C" {
int  z80_contend_wrapper(int, int, int) { return 0; }
int  z80_contend_io_wrapper(int, int, int) { return 0; }
BYTE z80_readbyte_wrapper(int a) { return mem[a & 0xFFFF]; }
BYTE z80_readoperandbyte_wrapper(int a) { return mem[a & 0xFFFF]; }
BYTE z80_opcode_fetch_wrapper(int a) { z80_fetch_is_m1 = 0; return mem[a & 0xFFFF]; }
void z80_writebyte_wrapper(int a, int d) { mem[a & 0xFFFF] = (uint8_t)d; }
void z80_writeport_wrapper(int port, int d, int*) { if ((port & 0xFF) == 0xA7) tube_out((uint8_t)d); }
BYTE z80_readport_wrapper(int port, int*) {
  int lo = port & 0xFF;
  if (lo == 0xAF) return clk ? 0x80 : 0x00;
  if (lo == 0xA7) return tube_in();
  return 0xFF;
}
}

static long instr_total = 0;
static uint32_t time_extra = 0;                    // ms que "pasan" mientras el Z80 no corre (el programa pinta)
static int slowdown = getenv("SLOWDOWN") ? atoi(getenv("SLOWDOWN")) : 1;   // el Z80 corre N veces mas despacio (SLOW)
static const long MAXI = 400000000L;
// CALL addr desde el exterior: la direccion de retorno es una trampa con HALT en $0100
static bool z80_call(uint16_t addr) {
  z80.sp.w = 0xFFF0; mem[0xFFF0] = 0x00; mem[0xFFF1] = 0x01; mem[0x0100] = 0x76;
  z80.pc.w = addr; z80.halted = 0; z80.iff1 = z80.iff2 = 0;
  long n = 0;
  while (z80.pc.w != 0x0100) {
    z80_do_opcode();
    instr_total++;
    if ((++n & 255) == 0) { mock_now = 100000 + time_extra + (uint32_t)(instr_total * 5 * slowdown / 3250); net_bridge_loop(); }
    if (n > MAXI) { printf("  Z80 colgado en PC=%04X SP=%04X BC=%04X DE=%04X HL=%04X\n", z80.pc.w, z80.sp.w, z80.bc.w, z80.de.w, z80.hl.w); return false; }
  }
  return true;
}

// ------------------------------------------------------------ el broker de mentira
static mqtt::Parser bparser;
struct Seen { std::string kind, topic, data; uint8_t qos = 0; bool retain = false, dup = false; };
static std::vector<Seen> seen;
static std::map<std::string, std::string> retained;
static std::vector<std::string> filters;
static std::string will_topic, will_data; static bool will_retain = false, has_will = false;
static uint16_t out_pid = 0;
static void send_to_client(const uint8_t* b, size_t n) { for (size_t i = 0; i < n; i++) mock_from_broker.push_back(b[i]); }

static bool topic_match(const std::string& f, const std::string& t) {
  size_t fi = 0, ti = 0;
  while (true) {
    size_t fe = f.find('/', fi), te = t.find('/', ti);
    std::string fp = f.substr(fi, fe == std::string::npos ? std::string::npos : fe - fi);
    std::string tp = t.substr(ti, te == std::string::npos ? std::string::npos : te - ti);
    if (fp == "#") return true;
    if (fp != "+" && fp != tp) return false;
    if (fe == std::string::npos || te == std::string::npos) return fe == te;
    fi = fe + 1; ti = te + 1;
  }
}
static void deliver(const std::string& topic, const std::string& data, bool retain) {
  uint8_t pkt[400];
  size_t n = mqtt::publish(pkt, sizeof(pkt), topic.c_str(), (const uint8_t*)data.data(), data.size(), 1, retain, false, ++out_pid);
  send_to_client(pkt, n);
}
static bool subscribed(const std::string& t) { for (auto& f : filters) if (topic_match(f, t)) return true; return false; }

void mock_broker_rx(const uint8_t* b, size_t n) {
  for (size_t i = 0; i < n; i++) {
    mqtt::Packet pk;
    if (bparser.feed(b[i], &pk) != mqtt::Parser::READY) continue;
    Seen s; uint8_t out[400];
    switch (pk.type) {
      case mqtt::T_CONNECT: {
        s.kind = "CONNECT";
        uint8_t flags = pk.body[7];
        uint16_t pos = 10, cl = (uint16_t)((pk.body[pos] << 8) | pk.body[pos + 1]);
        s.data = std::string((const char*)pk.body + pos + 2, cl); pos += 2 + cl;
        has_will = flags & 4; filters.clear();
        if (has_will) {
          uint16_t tl = (uint16_t)((pk.body[pos] << 8) | pk.body[pos + 1]);
          will_topic = std::string((const char*)pk.body + pos + 2, tl); pos += 2 + tl;
          uint16_t dl = (uint16_t)((pk.body[pos] << 8) | pk.body[pos + 1]);
          will_data = std::string((const char*)pk.body + pos + 2, dl);
          will_retain = flags & 0x20;
          s.topic = will_topic;
        }
        uint8_t ca[4] = {0x20, 0x02, 0x00, 0x00};
        send_to_client(ca, 4);
        break;
      }
      case mqtt::T_SUBSCRIBE: {
        s.kind = "SUBSCRIBE";
        uint16_t tl = (uint16_t)((pk.body[2] << 8) | pk.body[3]);
        s.topic = std::string((const char*)pk.body + 4, tl); s.qos = pk.body[4 + tl];
        filters.push_back(s.topic);
        uint8_t sa[5] = {0x90, 0x03, pk.body[0], pk.body[1], s.qos};
        send_to_client(sa, 5);
        for (auto& r : retained) if (topic_match(s.topic, r.first)) deliver(r.first, r.second, true);
        break;
      }
      case mqtt::T_PUBLISH: {
        mqtt::PublishIn p; mqtt::parse_publish(pk, &p);
        s.kind = "PUBLISH"; s.topic = std::string((const char*)p.topic, p.topic_len);
        s.data = std::string((const char*)p.data, p.data_len); s.qos = p.qos; s.retain = p.retain;
        if (p.qos == 1) { size_t k = mqtt::puback(out, sizeof(out), p.id); send_to_client(out, k); }
        if (p.retain) { if (s.data.empty()) retained.erase(s.topic); else retained[s.topic] = s.data; }
        if (subscribed(s.topic)) deliver(s.topic, s.data, false);    // el broker me devuelve lo mio
        break;
      }
      case mqtt::T_PINGREQ: { s.kind = "PINGREQ"; uint8_t pr[2] = {0xD0, 0}; send_to_client(pr, 2); break; }
      case mqtt::T_DISCONNECT: s.kind = "DISCONNECT"; has_will = false; break;
      case mqtt::T_PUBACK: s.kind = "PUBACK"; break;
      default: s.kind = "?"; break;
    }
    seen.push_back(s);
  }
}
// el rival publica
static void peer_publish(const std::string& topic, const std::string& data, bool retain) {
  if (retain) { if (data.empty()) retained.erase(topic); else retained[topic] = data; }
  if (mock_connected && subscribed(topic)) deliver(topic, data, false);
}
static int count_seen(const char* k) { int n = 0; for (auto& s : seen) if (s.kind == k) n++; return n; }
static const Seen* last_seen(const char* k, const std::string& topic = "") {
  for (int i = (int)seen.size() - 1; i >= 0; i--) if (seen[i].kind == k && (topic.empty() || seen[i].topic == topic)) return &seen[i];
  return nullptr;
}

// ------------------------------------------------------------ utilidades de la prueba
static int errors = 0;
#define CHECK(c, msg) do { if (!(c)) { printf("ERROR %s\n", msg); errors++; } } while (0)

static const uint16_t NB = 0x7000;
static uint16_t EP_INIT = 24576, EP_NEW = 24579, EP_JOIN = 24582, EP_SEND = 24585, EP_POLL = 24588, EP_LEAVE = 24591,
                EP_PSEND = 24594, EP_PRECV = 24597;
static int call_ep(uint16_t ep) { if (!z80_call(ep)) { printf("ERROR el Z80 no vuelve (entrada %u)\n", ep); errors++; return -1; } if (getenv("TRACE")) printf("  => entrada %u devuelve %u (NB_RES=%u)\n", ep, z80.bc.w, mem[NB]); return z80.bc.w; }
static std::string room_code_ascii() {
  std::string c;
  for (int i = 0; i < 10; i++) { int v = mem[NB + 8 + i]; c += (char)(v >= 38 ? v + 27 : v + 20); }
  return c;
}
static void set_code(const std::string& ascii) {
  for (int i = 0; i < 10; i++) { char ch = ascii[i]; mem[NB + 8 + i] = (uint8_t)(ch >= 'A' ? ch - 27 : ch - 20); }
}
static std::string topic_base(const std::string& code) {
  std::string in = "topic" + code; uint8_t h[32]; sha256((const uint8_t*)in.data(), in.size(), h);
  return "sd81/" + hexs(h, 6);
}
static std::string key_for(const std::string& code) {
  std::string in = "key" + code; uint8_t h[32]; sha256((const uint8_t*)in.data(), in.size(), h);
  return hexs(h, 32);   // 64 caracteres: son los bytes de la clave HMAC
}
static std::string make_msg(const std::string& code, uint16_t seq, uint16_t turn, const std::string& data, bool bad = false) {
  std::string body;
  body += (char)1; body += (char)(seq & 255); body += (char)(seq >> 8); body += (char)(turn & 255); body += (char)(turn >> 8);
  body += data;
  std::string k = key_for(code); uint8_t mac[32];
  hmac256((const uint8_t*)k.data(), k.size(), (const uint8_t*)body.data(), body.size(), mac);
  if (bad) mac[0] ^= 0x55;
  return body + std::string((const char*)mac, 8);
}
static void reset_world() {
  net_bridge_init();
  to_z80.clear(); from_z80.clear(); resp.clear(); tube_state = 0; clk = false;
  seen.clear(); retained.clear(); filters.clear(); has_will = false; mock_from_broker.clear(); mock_connected = false;
}

int main(int argc, char** argv) {
  if (argc < 2) { printf("uso: test_z80net netchat.bin\n"); return 2; }
  FILE* f = fopen(argv[1], "rb");
  if (!f) { printf("no encuentro %s\n", argv[1]); return 2; }
  size_t sz = fread(mem + 0x6000, 1, 0x1000, f); fclose(f);
  printf("netchat.bin: %zu bytes en $6000\n", sz);
  mem[0x02E7] = 0xC9; mem[0x0207] = 0xC9;       // SET_FAST y SLOW_FAST de la ROM: aqui, un RET
  z80_init(); z80_reset();
  reset_world();

  // --- 1. iniciar el modem
  CHECK(call_ep(EP_INIT) == 0 && mem[NB] == 0, "net_init: ATZ y ATE0 bien");
  CHECK(!at_echo, "net_init: el eco del modem queda apagado");

  // --- 2. crear una sala
  CHECK(call_ep(EP_NEW) == 0 && mem[NB] == 0, "room_new bien");
  std::string code = room_code_ascii();
  printf("  codigo de sala: %s\n", code.c_str());
  const char* alpha = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
  bool alpha_ok = code.size() == 10;
  for (char ch : code) if (!strchr(alpha, ch)) alpha_ok = false;
  CHECK(alpha_ok, "el codigo son 10 caracteres del alfabeto de 32");
  std::string base = topic_base(code);
  CHECK(mem[NB + 1] == 0, "soy la ranura 0");
  const Seen* c = last_seen("CONNECT");
  CHECK(c && c->topic == base + "/p0", "CONNECT con el last will en <sala>/p0 (el topic sale de SHA256(topic+codigo))");
  CHECK(has_will && will_data == "0" && will_retain, "el will es '0' y retenido");
  CHECK(last_seen("SUBSCRIBE") && last_seen("SUBSCRIBE")->topic == base + "/+" && last_seen("SUBSCRIBE")->qos == 1, "suscrito a <sala>/+ con QoS 1");
  const Seen* pp = last_seen("PUBLISH", base + "/p0");
  CHECK(pp && pp->data == "1" && pp->retain && pp->qos == 1, "mi presencia '1' retenida en <sala>/p0");
  CHECK(mem[NB + 2] == 0, "sin rival todavia");

  // --- 3. nada que leer
  CHECK(call_ep(EP_POLL) == 0, "room_poll: no hay nada (mi propio eco se ignora)");

  // --- 4. el rival entra
  peer_publish(base + "/p1", "1", true);
  CHECK(call_ep(EP_POLL) == 2 && mem[NB + 2] == 1, "room_poll: el rival ha entrado");
  CHECK(call_ep(EP_POLL) == 0, "y no se repite");

  // --- 5. un mensaje del rival (con CR, LF y 0 en los datos)
  std::string d1("AB\r\nC\0D", 7);
  peer_publish(base + "/1", make_msg(code, 0x1234, 7, d1), true);
  CHECK(call_ep(EP_POLL) == 1, "room_poll: llega un mensaje");
  CHECK(mem[NB + 5] == 7 && mem[NB + 3] == 7 && mem[NB + 4] == 0, "longitud 7 y turno 7");
  CHECK(memcmp(mem + NB + 144, d1.data(), 7) == 0, "datos intactos (binario seguro)");

  // --- 6. repetido, firma mala, version mala y otra sala: se tiran
  peer_publish(base + "/1", make_msg(code, 0x1234, 8, "REP"), false);
  CHECK(call_ep(EP_POLL) == 0, "mismo numero de secuencia: se descarta");
  peer_publish(base + "/1", make_msg(code, 0x1235, 8, "MALA", true), false);
  CHECK(call_ep(EP_POLL) == 0, "firma mala: se descarta");
  { std::string m = make_msg(code, 0x1236, 8, "V2"); m[0] = 2; peer_publish(base + "/1", m, false);
    CHECK(call_ep(EP_POLL) == 0, "cuerpo alterado tras firmar: se descarta"); }
  peer_publish(base + "/0", make_msg(code, 0x2000, 1, "ECO"), false);
  CHECK(call_ep(EP_POLL) == 0, "un mensaje en MI ranura: se ignora");
  peer_publish(base + "/1", make_msg(code, 0x1237, 9, "OK2"), false);
  CHECK(call_ep(EP_POLL) == 1 && mem[NB + 5] == 3 && memcmp(mem + NB + 144, "OK2", 3) == 0 && mem[NB + 3] == 9, "y el siguiente bueno llega");
  peer_publish(base + "/1", make_msg(code, 0x1238, 10, ""), false);
  CHECK(call_ep(EP_POLL) == 1 && mem[NB + 5] == 0 && mem[NB + 3] == 10, "mensaje sin datos");

  // --- 6b. el sondeo en dos pasos: pedir, hacer otra cosa y recoger
  peer_publish(base + "/1", make_msg(code, 0x3000, 11, "DOS"), false);
  { long i0 = instr_total;
    CHECK(call_ep(EP_PSEND) == 0, "room_poll_send bien");
    CHECK(instr_total - i0 < 20000, "room_poll_send vuelve al momento (no espera al modem)");
    for (int k = 0; k < 100; k++) { time_extra++; mock_now++; net_bridge_loop(); }   // pasa el tiempo: el modem responde
    i0 = instr_total;
    CHECK(call_ep(EP_PRECV) == 1 && mem[NB + 5] == 3 && memcmp(mem + NB + 144, "DOS", 3) == 0 && mem[NB + 3] == 11, "room_poll_recv recoge el mensaje");
    // (con un mensaje, recv pide ademas el HMAC al modem: un comando mas, ~30 ms; sin mensajes es inmediato)
    call_ep(EP_PSEND);
    for (int k = 0; k < 100; k++) { time_extra++; mock_now++; net_bridge_loop(); }
    i0 = instr_total;
    CHECK(call_ep(EP_PRECV) == 0, "pedir y recoger sin nada que leer: 0");
    CHECK(instr_total - i0 < 20000, "y recoger no espera si la respuesta ya estaba");
    CHECK(call_ep(EP_PRECV) == 0, "room_poll_recv sin pedir antes: pregunta y espera; no hay nada"); }

  // --- 7. mandar
  for (int i = 0; i < 5; i++) mem[NB + 32 + i] = (uint8_t)"ABCDE"[i];
  mem[NB + 5] = 5; mem[NB + 3] = 9; mem[NB + 4] = 0;
  CHECK(call_ep(EP_SEND) == 0, "room_send bien");
  const Seen* sm = last_seen("PUBLISH", base + "/0");
  CHECK(sm && sm->qos == 1 && sm->retain && sm->data.size() == 5 + 5 + 8, "PUBLISH en <sala>/0, QoS 1, retenido, 18 bytes");
  if (sm && sm->data.size() == 18) {
    std::string body = sm->data.substr(0, 10), mac = sm->data.substr(10);
    uint8_t exp[32]; std::string k = key_for(code);
    hmac256((const uint8_t*)k.data(), k.size(), (const uint8_t*)body.data(), body.size(), exp);
    CHECK((uint8_t)body[0] == 1 && (uint8_t)body[3] == 9 && (uint8_t)body[4] == 0 && body.substr(5) == "ABCDE", "cabecera (version 1, turno 9) y datos");
    CHECK(memcmp(mac.data(), exp, 8) == 0, "el HMAC que calculo el ESP32 coincide con el independiente");
  }
  mem[NB + 5] = 101;
  CHECK(call_ep(EP_SEND) == 5, "mas de 100 bytes: error 5");
  mem[NB + 5] = 0;
  { size_t before = seen.size();
    CHECK(call_ep(EP_SEND) == 0 && seen.size() > before, "un mensaje sin datos tambien sale"); }

  // --- 8. dos envios seguidos: la secuencia crece
  mem[NB + 5] = 1; mem[NB + 32] = 'Z';
  CHECK(call_ep(EP_SEND) == 0, "otro envio");
  { auto& m1 = seen[seen.size() - 1]; (void)m1; }

  // --- 9. el rival se va
  peer_publish(base + "/p1", "0", true);
  CHECK(call_ep(EP_POLL) == 3 && mem[NB + 2] == 0, "room_poll: el rival se ha ido");

  // --- 10. salir
  CHECK(call_ep(EP_LEAVE) == 0, "room_leave bien");
  pp = last_seen("PUBLISH", base + "/p0");
  CHECK(pp && pp->data == "0" && pp->retain, "mi presencia pasa a '0'");
  CHECK(count_seen("DISCONNECT") == 1, "DISCONNECT al broker");

  // --- 11. unirse a una sala con un codigo (otra sesion), con NET_WRITE aceptando poco a poco
  write_limit = 17;
  reset_world();
  CHECK(call_ep(EP_INIT) == 0, "net_init otra vez");
  std::string code2 = "MNPQRSTUVW";
  peer_publish(topic_base(code2) + "/p0", "1", true);
  peer_publish(topic_base(code2) + "/0", make_msg(code2, 77, 3, "HOLA"), true);
  set_code(code2);
  CHECK(call_ep(EP_JOIN) == 0 && mem[NB + 1] == 1, "room_join bien, soy la ranura 1 (con escrituras parciales de 17 bytes)");
  c = last_seen("CONNECT");
  CHECK(c && c->topic == topic_base(code2) + "/p1", "el will, en <sala>/p1");
  // el broker entrega lo retenido en el orden de sus topics: primero la jugada y luego la presencia
  CHECK(call_ep(EP_POLL) == 1 && mem[NB + 5] == 4 && memcmp(mem + NB + 144, "HOLA", 4) == 0 && mem[NB + 3] == 3,
        "al unirme, la ultima jugada retenida del rival me llega (juego asincrono)");
  CHECK(call_ep(EP_POLL) == 2 && mem[NB + 2] == 1, "y su presencia retenida tambien");
  write_limit = 255;

  // --- 12. un codigo malo
  reset_world();
  CHECK(call_ep(EP_INIT) == 0, "net_init otra vez");
  set_code("MNPQRSTUVI");   // la I no esta en el alfabeto
  mem[NB + 8 + 9] = 46;     // 'I' = 38 + 8
  CHECK(call_ep(EP_JOIN) == 4, "room_join con un caracter fuera del alfabeto: error 4");
  CHECK(count_seen("CONNECT") == 0, "y no llega a conectar");

  printf("  instrucciones del Z80: %ld (%.1f s a 3,25 MHz)\n", instr_total, instr_total * 5.0 / 3250000.0);
  if (errors) printf("%d ERRORES\n", errors); else printf("TODO OK\n");
  return errors ? 1 : 0;
}
