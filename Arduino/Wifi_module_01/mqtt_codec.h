#ifndef MQTT_CODEC_H
#define MQTT_CODEC_H

// Codificador y decodificador de paquetes MQTT 3.1.1, sin dependencias de
// Arduino ni de red: solo bytes. Lo usa NET_MQTT.cpp y se prueba en el PC
// (claude/mqtttest). Solo lo que hace falta: CONNECT (con will, usuario y
// clave), PUBLISH (QoS 0 y 1), SUBSCRIBE, UNSUBSCRIBE, PINGREQ, DISCONNECT,
// PUBACK; y, al recibir, CONNACK, PUBLISH, PUBACK, SUBACK, UNSUBACK y PINGRESP.
// QoS 2 no se usa.

#include <stdint.h>
#include <stddef.h>
#include <string.h>

namespace mqtt {

enum {
  T_CONNECT = 1, T_CONNACK = 2, T_PUBLISH = 3, T_PUBACK = 4,
  T_SUBSCRIBE = 8, T_SUBACK = 9, T_UNSUBSCRIBE = 10, T_UNSUBACK = 11,
  T_PINGREQ = 12, T_PINGRESP = 13, T_DISCONNECT = 14
};

// ---------------------------------------------------------------- codificar
// Todas devuelven la longitud escrita, o 0 si no cabe en `cap`.

class Writer {
public:
  Writer(uint8_t* b, size_t c) : buf(b), cap(c), n(0), bad(false) {}
  void u8(uint8_t v) { if (n < cap) buf[n++] = v; else bad = true; }
  void u16(uint16_t v) { u8(v >> 8); u8(v & 0xFF); }
  void bytes(const uint8_t* p, size_t l) {
    if (n + l > cap) { bad = true; return; }
    if (l) memcpy(buf + n, p, l);
    n += l;
  }
  void str(const char* s) { size_t l = strlen(s); u16((uint16_t)l); bytes((const uint8_t*)s, l); }
  size_t size() const { return bad ? 0 : n; }
  uint8_t* buf; size_t cap, n; bool bad;
};

// Cabecera fija: tipo + banderas, y la longitud restante (1 a 4 bytes). El
// cuerpo ya esta en body (len bytes); se escribe todo en out.
static inline size_t frame(uint8_t* out, size_t cap, uint8_t type_flags,
                           const uint8_t* body, size_t len) {
  uint8_t hdr[5]; size_t h = 0;
  hdr[h++] = type_flags;
  size_t x = len;
  do {
    uint8_t d = x % 128; x /= 128;
    if (x) d |= 0x80;
    hdr[h++] = d;
  } while (x && h < 5);
  if (h + len > cap) return 0;
  memcpy(out, hdr, h);
  if (len) memcpy(out + h, body, len);
  return h + len;
}

struct ConnectOpts {
  const char* client_id;
  uint16_t keepalive;
  const char* user;            // NULL o "" = sin usuario
  const char* pass;            // NULL o "" = sin clave
  const char* will_topic;      // NULL o "" = sin will
  const uint8_t* will_data;
  uint16_t will_len;
  bool will_retain;
};

static inline size_t connect(uint8_t* out, size_t cap, const ConnectOpts& o) {
  uint8_t body[300];
  Writer w(body, sizeof(body));
  w.str("MQTT");
  w.u8(4);                                         // nivel 3.1.1
  bool has_user = o.user && o.user[0];
  bool has_pass = has_user && o.pass && o.pass[0];
  bool has_will = o.will_topic && o.will_topic[0];
  uint8_t flags = 0x02;                            // sesion limpia
  if (has_will) { flags |= 0x04; if (o.will_retain) flags |= 0x20; }
  if (has_user) flags |= 0x80;
  if (has_pass) flags |= 0x40;
  w.u8(flags);
  w.u16(o.keepalive);
  w.str(o.client_id ? o.client_id : "");
  if (has_will) {
    w.str(o.will_topic);
    w.u16(o.will_len);
    w.bytes(o.will_data, o.will_len);
  }
  if (has_user) w.str(o.user);
  if (has_pass) w.str(o.pass);
  if (!w.size()) return 0;
  return frame(out, cap, T_CONNECT << 4, body, w.size());
}

static inline size_t publish(uint8_t* out, size_t cap, const char* topic,
                             const uint8_t* data, size_t len, uint8_t qos,
                             bool retain, bool dup, uint16_t id) {
  uint8_t body[400];
  Writer w(body, sizeof(body));
  w.str(topic);
  if (qos) w.u16(id);
  w.bytes(data, len);
  if (!w.size()) return 0;
  uint8_t f = (T_PUBLISH << 4) | (qos << 1) | (retain ? 1 : 0) | (dup ? 8 : 0);
  return frame(out, cap, f, body, w.size());
}

static inline size_t subscribe(uint8_t* out, size_t cap, uint16_t id,
                               const char* topic, uint8_t qos) {
  uint8_t body[100];
  Writer w(body, sizeof(body));
  w.u16(id); w.str(topic); w.u8(qos);
  if (!w.size()) return 0;
  return frame(out, cap, (T_SUBSCRIBE << 4) | 2, body, w.size());
}

static inline size_t unsubscribe(uint8_t* out, size_t cap, uint16_t id, const char* topic) {
  uint8_t body[100];
  Writer w(body, sizeof(body));
  w.u16(id); w.str(topic);
  if (!w.size()) return 0;
  return frame(out, cap, (T_UNSUBSCRIBE << 4) | 2, body, w.size());
}

static inline size_t puback(uint8_t* out, size_t cap, uint16_t id) {
  uint8_t body[2] = { (uint8_t)(id >> 8), (uint8_t)id };
  return frame(out, cap, T_PUBACK << 4, body, 2);
}
static inline size_t pingreq(uint8_t* out, size_t cap)    { return frame(out, cap, T_PINGREQ << 4, 0, 0); }
static inline size_t disconnect(uint8_t* out, size_t cap) { return frame(out, cap, T_DISCONNECT << 4, 0, 0); }

// ----------------------------------------------------------------- decodificar

struct Packet {
  uint8_t type;
  uint8_t flags;
  const uint8_t* body;   // valido hasta el siguiente feed()
  uint16_t len;
};

// Analizador incremental: se le dan los bytes del socket de uno en uno.
class Parser {
public:
  enum { MAXBODY = 320 };
  enum Result { NEED_MORE = 0, READY = 1, DROPPED = 2, BAD = -1 };

  Parser() { reset(); }
  void reset() { st = S_HDR; mult = 1; rem = 0; got = 0; skip = false; }

  // READY: *out es un paquete completo. DROPPED: era mas grande de lo que cabe
  // y se ha descartado entero (el resto del flujo sigue bien). BAD: la longitud
  // restante es invalida; el flujo esta perdido y hay que reconectar.
  int feed(uint8_t b, Packet* out) {
    switch (st) {
      case S_HDR:
        hdr = b; st = S_LEN; mult = 1; rem = 0; nlen = 0;
        return NEED_MORE;
      case S_LEN:
        rem += (uint32_t)(b & 0x7F) * mult;
        mult *= 128;
        if (++nlen > 4) return BAD;
        if (b & 0x80) return NEED_MORE;
        got = 0;
        skip = rem > MAXBODY;
        if (rem == 0) { st = S_HDR; return finish(out); }
        st = S_BODY;
        return NEED_MORE;
      case S_BODY:
        if (!skip) body[got] = b;
        got++;
        if (got < rem) return NEED_MORE;
        st = S_HDR;
        return finish(out);
    }
    return BAD;
  }

private:
  int finish(Packet* out) {
    if (skip) { skip = false; return DROPPED; }
    out->type = hdr >> 4;
    out->flags = hdr & 0x0F;
    out->body = body;
    out->len = (uint16_t)rem;
    return READY;
  }
  enum { S_HDR, S_LEN, S_BODY } st;
  uint8_t hdr;
  uint32_t mult, rem, got;
  uint8_t nlen;
  bool skip;
  uint8_t body[MAXBODY];
};

// Desmonta un PUBLISH recibido. topic y data apuntan dentro del cuerpo.
struct PublishIn {
  const uint8_t* topic; uint16_t topic_len;
  const uint8_t* data;  uint16_t data_len;
  uint8_t qos; bool retain; uint16_t id;
};

static inline bool parse_publish(const Packet& p, PublishIn* o) {
  if (p.type != T_PUBLISH || p.len < 2) return false;
  uint16_t tl = (uint16_t)((p.body[0] << 8) | p.body[1]);
  uint16_t pos = 2;
  if ((uint32_t)pos + tl > p.len) return false;
  o->topic = p.body + pos; o->topic_len = tl; pos += tl;
  o->qos = (p.flags >> 1) & 3;
  o->retain = p.flags & 1;
  o->id = 0;
  if (o->qos) {
    if (pos + 2 > p.len) return false;
    o->id = (uint16_t)((p.body[pos] << 8) | p.body[pos + 1]);
    pos += 2;
  }
  o->data = p.body + pos; o->data_len = p.len - pos;
  return true;
}

}  // namespace mqtt

#endif
