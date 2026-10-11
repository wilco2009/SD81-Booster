#include "NET_MQTT.h"
#include "NET_BRIDGE.h"
#include "WIFI_CLIENT.h"
#include "mqtt_codec.h"
#include <ctype.h>
#include <strings.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <mbedtls/md.h>
#include <esp_system.h>
#if __has_include(<esp_random.h>)
#include <esp_random.h>
#endif
#include "SD_LOG.h"   // ultimo a proposito, ver Wifi_module_01.ino

// Cliente MQTT 3.1.1 propio (sin libreria): una sola sesion, QoS 0 y 1,
// retained, will, reconexion y una cola de mensajes recibidos que el Z80 vacia
// con AT+MQTTRX. Solo se loguea en eventos (conectar, caerse, reconectar),
// nunca por mensaje.

#define MQ_TOPIC        64     // longitud maxima de un topic
#define MQ_PAYLOAD      120    // bytes maximos de un mensaje (la trama del puente es de 128)
#define MQ_RXQ          8      // mensajes recibidos que se guardan
#define MQ_SUBS         8
#define MQ_BROKERS      6
#define MQ_KEEPALIVE_S  60
#define MQ_CONNECT_MS   4000   // abrir el TCP
#define MQ_CONNACK_MS   4000
#define MQ_ACK_MS       3000   // PUBACK / SUBACK
#define MQ_RAW_MAX      256

enum { MQ_OFF = 0, MQ_CONNECTING = 1, MQ_UP = 2, MQ_RECONNECT = 3 };

// Codigos de error (el ultimo se ve con AT+MQTT?)
enum {
  E_NONE = 0, E_NOWIFI = 1, E_TCP = 2, E_REFUSED = 3, E_TIMEOUT = 4,
  E_ARG = 5, E_FULL = 6, E_NOTCONN = 7, E_PUBACK = 8
};

struct Broker { char host[48]; uint16_t port; bool tls; };
struct RxMsg  { char topic[MQ_TOPIC + 1]; uint8_t data[MQ_PAYLOAD]; uint8_t len; };
struct Sub    { char topic[MQ_TOPIC + 1]; uint8_t qos; bool used; };

static WiFiClient        plain;
static WiFiClientSecure* secure = nullptr;
static Client*           tcp = nullptr;

static uint8_t  state = MQ_OFF;
static uint8_t  last_err = E_NONE;

static Broker   list[MQ_BROKERS];
static uint8_t  list_n = 0;
static uint8_t  list_idx = 0;       // el que se usa ahora
static uint8_t  last_good = 0;      // el ultimo que funciono (la proxima vez se empieza por el)
static bool     list_from_cfg = false;

static char     user[33] = "", pass[33] = "";
static char     clientid[24] = "";
static bool     id_fixed = false;
static char     will_topic[MQ_TOPIC + 1] = "";
static uint8_t  will_data[33];
static uint8_t  will_len = 0;
static bool     will_retain = false;
static Sub      subs[MQ_SUBS];

static mqtt::Parser parser;
static RxMsg    rxq[MQ_RXQ];
static uint8_t  rx_head = 0, rx_count = 0;
static uint16_t rx_lost = 0;

static uint32_t last_tx = 0, last_rx = 0, ping_sent = 0;
static bool     ping_out = false;
static uint32_t next_try = 0;
static uint8_t  fail_count = 0;
static uint16_t next_id = 1;

// Lo que llega mientras se espera una respuesta concreta
static bool     got_connack = false; static uint8_t connack_rc = 0;
static bool     got_suback = false;  static uint8_t suback_rc = 0;
static uint16_t acked_id = 0;

static uint16_t new_id() { if (++next_id == 0) next_id = 1; return next_id; }

// --- respuestas AT ---------------------------------------------------------
static void say(const char* s) { net_bridge_txq_push_str(s); }
static void ok()               { say("OK\r\n"); }
static void fail(uint8_t code) { last_err = code; say("ERROR\r\n"); }

// --- aleatorios y resumenes -------------------------------------------------
static void to_hex(const uint8_t* d, size_t n, char* out) {
  static const char H[] = "0123456789ABCDEF";
  for (size_t i = 0; i < n; i++) { out[2 * i] = H[d[i] >> 4]; out[2 * i + 1] = H[d[i] & 15]; }
  out[2 * n] = 0;
}

// --- socket -----------------------------------------------------------------
static void tcp_close() {
  if (tcp) tcp->stop();
  tcp = nullptr;
  if (secure) { delete secure; secure = nullptr; }
}

static bool tcp_open(const Broker& b) {
  tcp_close();
  if (WiFi.status() != WL_CONNECTED) { last_err = E_NOWIFI; return false; }
  bool opened;
  if (b.tls) {
    secure = new WiFiClientSecure();
    if (!secure) { last_err = E_TCP; return false; }
    secure->setInsecure();      // las jugadas no son secretas; ver juego_en_red.md
    opened = secure->connect(b.host, b.port, MQ_CONNECT_MS);
    tcp = secure;
  } else {
    opened = plain.connect(b.host, b.port, MQ_CONNECT_MS);
    if (opened) plain.setNoDelay(true);
    tcp = &plain;
  }
  if (!opened) { tcp_close(); last_err = E_TCP; return false; }
  return true;
}

static bool tx(const uint8_t* b, size_t n) {
  if (!tcp || n == 0) return false;
  if (tcp->write(b, n) != n) return false;
  last_tx = millis();
  return true;
}

static void mark_down() {
  bool was_up = (state == MQ_UP);
  tcp_close();
  ping_out = false;
  if (was_up) {
    state = MQ_RECONNECT;
    fail_count = 0;
    next_try = millis() + 2000;
    Serial.println("MQTT: conexion perdida, reintentando");
  }
}

// --- cola de recibidos ------------------------------------------------------
static void rx_push(const mqtt::PublishIn& p) {
  if (p.topic_len > MQ_TOPIC || p.data_len > MQ_PAYLOAD) { rx_lost++; return; }
  if (rx_count == MQ_RXQ) {                      // llena: se pierde el mas antiguo
    rx_head = (rx_head + 1) % MQ_RXQ; rx_count--; rx_lost++;
  }
  RxMsg& m = rxq[(rx_head + rx_count) % MQ_RXQ];
  memcpy(m.topic, p.topic, p.topic_len); m.topic[p.topic_len] = 0;
  memcpy(m.data, p.data, p.data_len);    m.len = (uint8_t)p.data_len;
  rx_count++;
}

static void handle_packet(const mqtt::Packet& pk) {
  last_rx = millis();
  switch (pk.type) {
    case mqtt::T_CONNACK:
      got_connack = true; connack_rc = pk.len >= 2 ? pk.body[1] : 0xFF;
      break;
    case mqtt::T_PUBLISH: {
      mqtt::PublishIn p;
      if (!mqtt::parse_publish(pk, &p)) break;
      rx_push(p);
      if (p.qos == 1) {
        uint8_t a[4];
        size_t n = mqtt::puback(a, sizeof(a), p.id);
        tx(a, n);
      }
      break;
    }
    case mqtt::T_PUBACK:
      if (pk.len >= 2) acked_id = (uint16_t)((pk.body[0] << 8) | pk.body[1]);
      break;
    case mqtt::T_SUBACK:
      got_suback = true; suback_rc = pk.len >= 3 ? pk.body[2] : 0x80;
      break;
    case mqtt::T_PINGRESP:
      ping_out = false;
      break;
    default: break;
  }
}

// Lee lo que haya en el socket (sin esperar) y lo procesa
static void pump_input() {
  if (!tcp) return;
  uint8_t buf[64];
  int guard = 8;
  while (tcp && tcp->available() > 0 && guard-- > 0) {
    int got = tcp->read(buf, sizeof(buf));
    if (got <= 0) break;
    for (int i = 0; i < got; i++) {
      mqtt::Packet pk;
      int r = parser.feed(buf[i], &pk);
      if (r == mqtt::Parser::READY) handle_packet(pk);
      else if (r == mqtt::Parser::DROPPED) rx_lost++;
      else if (r == mqtt::Parser::BAD) { mark_down(); return; }
    }
  }
}

// Espera a que `flag` pase a true (lo pone handle_packet) o venza el tiempo
static bool wait_flag(const bool* flag, uint32_t ms) {
  uint32_t t0 = millis();
  while ((uint32_t)(millis() - t0) < ms) {
    pump_input();
    if (*flag) return true;
    if (!tcp || !tcp->connected()) return false;
    delay(2);
  }
  return false;
}

// --- sesion -----------------------------------------------------------------
static bool send_subscribe(const Sub& s) {
  uint8_t pkt[100];
  size_t n = mqtt::subscribe(pkt, sizeof(pkt), new_id(), s.topic, s.qos);
  return n && tx(pkt, n);
}

static bool session_connect(const Broker& b) {
  if (!tcp_open(b)) return false;
  parser.reset();
  got_connack = false;
  if (!id_fixed) snprintf(clientid, sizeof(clientid), "sd81-%08X", (unsigned)esp_random());
  mqtt::ConnectOpts o = {};
  o.client_id = clientid; o.keepalive = MQ_KEEPALIVE_S;
  o.user = user; o.pass = pass;
  o.will_topic = will_topic; o.will_data = will_data; o.will_len = will_len; o.will_retain = will_retain;
  uint8_t pkt[300];
  size_t n = mqtt::connect(pkt, sizeof(pkt), o);
  if (!n || !tx(pkt, n)) { tcp_close(); last_err = E_TCP; return false; }
  if (!wait_flag(&got_connack, MQ_CONNACK_MS)) { tcp_close(); last_err = E_TIMEOUT; return false; }
  if (connack_rc != 0) { tcp_close(); last_err = E_REFUSED; return false; }
  state = MQ_UP;
  last_err = E_NONE;                 // un error es del intento que fallo, no de esta sesion
  last_rx = last_tx = millis();
  ping_out = false;
  for (uint8_t i = 0; i < MQ_SUBS; i++) if (subs[i].used) send_subscribe(subs[i]);
  Serial.printf("MQTT: conectado a %s:%u\n", b.host, b.port);
  return true;
}

static void load_brokers() {
  list_n = 0;
  String cfg;
  if (wifi_client_read_text_file("/SYS/NET.CFG", &cfg) && cfg.length() > 0) {
    int pos = 0;
    while (pos < (int)cfg.length() && list_n < MQ_BROKERS) {
      int e = cfg.indexOf('\n', pos);
      if (e < 0) e = cfg.length();
      String line = cfg.substring(pos, e);
      pos = e + 1;
      line.trim();
      if (line.length() == 0 || line[0] == '#') continue;
      char host[48]; unsigned port = 0; char tlsw[8] = "";
      char word[16];
      int k = sscanf(line.c_str(), "%15s %47s %u %7s", word, host, &port, tlsw);
      if (k < 3 || strcasecmp(word, "broker") != 0 || port == 0 || port > 65535) continue;
      Broker& b = list[list_n++];
      snprintf(b.host, sizeof(b.host), "%s", host);
      b.port = (uint16_t)port;
      b.tls = (k == 4 && strcasecmp(tlsw, "tls") == 0);
    }
  }
  list_from_cfg = list_n > 0;
  if (list_n == 0) {                              // sin fichero o vacio: los de siempre
    static const char* const def[] = { "test.mosquitto.org", "broker.emqx.io", "broker.hivemq.com" };
    for (uint8_t i = 0; i < 3; i++) {
      snprintf(list[i].host, sizeof(list[i].host), "%s", def[i]);
      list[i].port = 1883; list[i].tls = false;
    }
    list_n = 3;
  }
}

// Prueba los brokers de la lista empezando por el ultimo que funciono
static bool connect_list() {
  if (last_good >= list_n) last_good = 0;
  for (uint8_t k = 0; k < list_n; k++) {
    uint8_t i = (last_good + k) % list_n;
    list_idx = i;
    if (session_connect(list[i])) { last_good = i; return true; }
    Serial.printf("MQTT: %s:%u no responde (error %u)\n", list[i].host, list[i].port, last_err);
  }
  return false;
}

// --- bucle ------------------------------------------------------------------
void net_mqtt_init() {
  state = MQ_OFF;
  memset(subs, 0, sizeof(subs));
  rx_head = rx_count = 0; rx_lost = 0;
  last_err = E_NONE;
}

bool net_mqtt_active() { return state != MQ_OFF; }

void net_mqtt_loop() {
  if (state == MQ_UP) {
    pump_input();
    if (state != MQ_UP) return;
    if (!tcp || !tcp->connected()) { mark_down(); return; }
    uint32_t now = millis();
    if (!ping_out && (uint32_t)(now - last_tx) > (MQ_KEEPALIVE_S * 1000UL) / 2) {
      uint8_t a[2];
      size_t n = mqtt::pingreq(a, sizeof(a));
      if (tx(a, n)) { ping_out = true; ping_sent = now; } else mark_down();
    } else if (ping_out && (uint32_t)(now - ping_sent) > 15000) {
      mark_down();            // el broker no contesta
    }
  } else if (state == MQ_RECONNECT) {
    uint32_t now = millis();
    if ((int32_t)(now - next_try) < 0) return;
    if (session_connect(list[list_idx])) return;
    fail_count++;
    if (list_n > 1) list_idx = (list_idx + 1) % list_n;   // el siguiente broker
    uint32_t back = 2000UL << (fail_count < 4 ? fail_count : 4);   // 4, 8, 16, 32 s
    next_try = millis() + (back > 30000 ? 30000 : back);
  }
}

// Cierra la sesion (DISCONNECT) y olvida suscripciones y mensajes, pero no la
// configuracion que se puso antes de conectar (will, clientid, usuario)
static void session_stop() {
  if (state == MQ_UP && tcp) {
    uint8_t a[2];
    size_t n = mqtt::disconnect(a, sizeof(a));
    tx(a, n);
  }
  tcp_close();
  state = MQ_OFF;
  memset(subs, 0, sizeof(subs));
  rx_head = rx_count = 0;
}

void net_mqtt_close() {
  session_stop();
  will_topic[0] = 0; will_len = 0; id_fixed = false;
  user[0] = pass[0] = 0;
}

// --- comandos AT --------------------------------------------------------------
// Parte "a,b,c" en trozos (las comillas dobles se quitan). Modifica s.
static int split(char* s, char** argv, int max) {
  int n = 0;
  while (n < max) {
    while (*s == ' ') s++;
    bool q = (*s == '"');
    if (q) s++;
    argv[n++] = s;
    while (*s && ((q && *s != '"') || (!q && *s != ','))) s++;
    if (q && *s == '"') *s++ = 0;
    while (*s && *s != ',') s++;
    if (*s == ',') { *s++ = 0; continue; }
    break;
  }
  return n;
}

static bool num(const char* s, long lo, long hi, long* out) {
  if (!*s) return false;
  char* e; long v = strtol(s, &e, 10);
  if (*e || v < lo || v > hi) return false;
  *out = v;
  return true;
}

static bool topic_ok(const char* t, bool allow_wild) {
  size_t n = strlen(t);
  if (n == 0 || n > MQ_TOPIC) return false;
  if (!allow_wild && (strchr(t, '+') || strchr(t, '#'))) return false;
  return true;
}

// Publicar con espera de PUBACK (QoS 1): dos intentos
static bool publish_now(const char* topic, const uint8_t* data, size_t len, uint8_t qos, bool retain) {
  uint8_t pkt[400];
  uint16_t id = qos ? new_id() : 0;
  for (int attempt = 0; attempt < 2; attempt++) {
    size_t n = mqtt::publish(pkt, sizeof(pkt), topic, data, len, qos, retain, attempt > 0, id);
    if (!n || !tx(pkt, n)) { mark_down(); last_err = E_NOTCONN; return false; }
    if (qos == 0) return true;
    uint32_t t0 = millis();
    while ((uint32_t)(millis() - t0) < MQ_ACK_MS) {
      pump_input();
      if (acked_id == id) return true;
      if (state != MQ_UP) { last_err = E_NOTCONN; return false; }
      delay(2);
    }
  }
  last_err = E_PUBACK;
  return false;
}

// Datos crudos de AT+MQTTPUB
static char    pub_topic[MQ_TOPIC + 1];
static uint8_t pub_qos; static bool pub_retain;

static void pub_done(const uint8_t* d, uint16_t len, bool good) {
  if (!good) { fail(E_TIMEOUT); return; }
  if (state != MQ_UP) { fail(E_NOTCONN); return; }
  if (publish_now(pub_topic, d, len, pub_qos, pub_retain)) ok(); else fail(last_err);
}

static void sha_done(const uint8_t* d, uint16_t len, bool good) {
  if (!good) { fail(E_TIMEOUT); return; }
  uint8_t h[32]; char hex[65];
  mbedtls_md(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), d, len, h);
  to_hex(h, 32, hex);
  say("+SHA256:"); say(hex); say("\r\n"); ok();
}

static uint16_t hmac_keylen;
static void hmac_done(const uint8_t* d, uint16_t len, bool good) {
  if (!good) { fail(E_TIMEOUT); return; }
  uint8_t h[32]; char hex[65];
  mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
                  d, hmac_keylen, d + hmac_keylen, len - hmac_keylen, h);
  to_hex(h, 32, hex);
  say("+HMAC:"); say(hex); say("\r\n"); ok();
}

static bool name_is(const char* name, size_t nlen, const char* want) {
  return strlen(want) == nlen && strncasecmp(name, want, nlen) == 0;
}

static void cmd_connect(char* rest) {
  if (state != MQ_OFF) session_stop();
  if (rest[0] != '=') load_brokers();
  else {                          // host,puerto[,tls[,usuario,clave]]
    char* a[5];
    int n = split(rest + 1, a, 5);
    long port, tls = 0;
    if (n < 2 || !a[0][0] || strlen(a[0]) >= sizeof(list[0].host) || !num(a[1], 1, 65535, &port) ||
        (n >= 3 && !num(a[2], 0, 1, &tls)) || (n == 4)) { fail(E_ARG); return; }
    snprintf(list[0].host, sizeof(list[0].host), "%s", a[0]);
    list[0].port = (uint16_t)port; list[0].tls = tls != 0;
    list_n = 1; list_from_cfg = false; last_good = 0;
    user[0] = pass[0] = 0;
    if (n == 5) { snprintf(user, sizeof(user), "%s", a[3]); snprintf(pass, sizeof(pass), "%s", a[4]); }
  }
  state = MQ_CONNECTING;
  bool good = connect_list();
  if (good) ok();
  else { state = MQ_OFF; fail(last_err ? last_err : E_TCP); }
}

bool net_mqtt_at(const char* args, uint8_t alen) {
  if (alen < 2 || args[0] != '+') return false;
  const char* name = args + 1;
  size_t nlen = 0;
  while (1 + nlen < alen && isalnum((unsigned char)name[nlen])) nlen++;
  char line[80];
  uint8_t rl = (uint8_t)(alen - 1 - nlen);
  if (rl >= sizeof(line)) rl = sizeof(line) - 1;
  memcpy(line, name + nlen, rl); line[rl] = 0;     // lo que va tras el nombre: "=...", "?" o vacio
  char* rest = line;

  if (name_is(name, nlen, "MQTT")) {
    if (rest[0] == '?') {
      char b[80];
      snprintf(b, sizeof(b), "+MQTT:%u,%s,%u,%u,%u\r\n", state,
               (state == MQ_OFF || list_n == 0) ? "-" : list[list_idx].host, rx_count, rx_lost, last_err);
      say(b); ok();
    } else if (rest[0] == 0 || rest[0] == '=') cmd_connect(rest);
    else return false;
    return true;
  }
  if (name_is(name, nlen, "MQTTID")) {
    if (rest[0] != '=' || !rest[1] || strlen(rest + 1) >= sizeof(clientid)) { fail(E_ARG); return true; }
    snprintf(clientid, sizeof(clientid), "%s", rest + 1); id_fixed = true; ok();
    return true;
  }
  if (name_is(name, nlen, "MQTTWILL")) {           // topic,retain,texto (el texto puede llevar comas)
    char* c1 = rest[0] == '=' ? strchr(rest + 1, ',') : nullptr;
    char* c2 = c1 ? strchr(c1 + 1, ',') : nullptr;
    long rt;
    if (!c2) { fail(E_ARG); return true; }
    *c1 = 0; *c2 = 0;
    if (!topic_ok(rest + 1, false) || !num(c1 + 1, 0, 1, &rt) || strlen(c2 + 1) > sizeof(will_data) - 1) { fail(E_ARG); return true; }
    snprintf(will_topic, sizeof(will_topic), "%s", rest + 1);
    will_len = (uint8_t)strlen(c2 + 1); memcpy(will_data, c2 + 1, will_len);
    will_retain = rt != 0;
    ok();
    return true;
  }
  if (name_is(name, nlen, "MQTTSUB") || name_is(name, nlen, "MQTTUNSUB")) {
    bool sub = nlen == 7;
    char* a[2];
    int n = rest[0] == '=' ? split(rest + 1, a, 2) : 0;
    long qos = 0;
    if (n < 1 || !topic_ok(a[0], true) || (n == 2 && !num(a[1], 0, 1, &qos))) { fail(E_ARG); return true; }
    if (state != MQ_UP && state != MQ_RECONNECT) { fail(E_NOTCONN); return true; }
    int slot = -1, freeslot = -1;
    for (int i = 0; i < MQ_SUBS; i++) {
      if (subs[i].used && strcmp(subs[i].topic, a[0]) == 0) slot = i;
      if (!subs[i].used && freeslot < 0) freeslot = i;
    }
    if (sub) {
      if (slot < 0) slot = freeslot;
      if (slot < 0) { fail(E_FULL); return true; }
      snprintf(subs[slot].topic, sizeof(subs[slot].topic), "%s", a[0]);
      subs[slot].qos = (uint8_t)qos; subs[slot].used = true;
      if (state == MQ_UP) {
        got_suback = false;
        if (!send_subscribe(subs[slot])) { mark_down(); fail(E_NOTCONN); return true; }
        if (!wait_flag(&got_suback, MQ_ACK_MS)) { fail(E_TIMEOUT); return true; }
        if (suback_rc & 0x80) { subs[slot].used = false; fail(E_REFUSED); return true; }
      }
      ok();
    } else {
      if (slot >= 0) subs[slot].used = false;
      if (state == MQ_UP) {
        uint8_t pkt[100];
        size_t k = mqtt::unsubscribe(pkt, sizeof(pkt), new_id(), a[0]);
        if (k) tx(pkt, k);
      }
      ok();
    }
    return true;
  }
  if (name_is(name, nlen, "MQTTPUB")) {
    char* a[4];
    int n = rest[0] == '=' ? split(rest + 1, a, 4) : 0;
    long qos, rt, len;
    if (n != 4 || !topic_ok(a[0], false) || !num(a[1], 0, 1, &qos) || !num(a[2], 0, 1, &rt) ||
        !num(a[3], 0, MQ_PAYLOAD, &len)) { fail(E_ARG); return true; }
    if (state != MQ_UP) { fail(E_NOTCONN); return true; }
    snprintf(pub_topic, sizeof(pub_topic), "%s", a[0]);
    pub_qos = (uint8_t)qos; pub_retain = rt != 0;
    if (len == 0) {                                // mensaje vacio (borra el retenido): sin datos
      if (publish_now(pub_topic, nullptr, 0, pub_qos, pub_retain)) ok(); else fail(last_err);
    } else if (!net_bridge_begin_raw((uint16_t)len, pub_done)) fail(E_FULL);
    return true;
  }
  if (name_is(name, nlen, "MQTTRX")) {
    if (rx_count == 0) { ok(); return true; }
    RxMsg& m = rxq[rx_head];
    char hdr[MQ_TOPIC + 24];
    int hl = snprintf(hdr, sizeof(hdr), "+MQTTRX:%s,%u\r\n", m.topic, m.len);
    if (net_bridge_txq_free() < (uint16_t)(hl + m.len + 8)) { fail(E_FULL); return true; }   // se queda en la cola
    say(hdr);
    net_bridge_txq_push_bytes(m.data, m.len);
    say("\r\n"); ok();
    rx_head = (rx_head + 1) % MQ_RXQ; rx_count--;
    return true;
  }
  if (name_is(name, nlen, "MQTTCLOSE")) {
    net_mqtt_close(); ok();
    return true;
  }
  if (name_is(name, nlen, "SHA256")) {
    long len;
    if (rest[0] != '=' || !num(rest + 1, 1, MQ_RAW_MAX, &len)) { fail(E_ARG); return true; }
    if (!net_bridge_begin_raw((uint16_t)len, sha_done)) fail(E_FULL);
    return true;
  }
  if (name_is(name, nlen, "HMAC")) {
    char* a[2];
    int n = rest[0] == '=' ? split(rest + 1, a, 2) : 0;
    long kl, dl;
    if (n != 2 || !num(a[0], 1, MQ_RAW_MAX - 1, &kl) || !num(a[1], 1, MQ_RAW_MAX, &dl) || kl + dl > MQ_RAW_MAX) { fail(E_ARG); return true; }
    hmac_keylen = (uint16_t)kl;
    if (!net_bridge_begin_raw((uint16_t)(kl + dl), hmac_done)) fail(E_FULL);
    return true;
  }
  if (name_is(name, nlen, "RAND")) {
    long n;
    if (rest[0] != '=' || !num(rest + 1, 1, 32, &n)) { fail(E_ARG); return true; }
    uint8_t r[32]; char hex[65];
    esp_fill_random(r, (size_t)n);
    to_hex(r, (size_t)n, hex);
    say("+RAND:"); say(hex); say("\r\n"); ok();
    return true;
  }
  return false;
}
