// Prueba de mqtt_codec.h en el PC: paquetes con los bytes que da la especificacion
// MQTT 3.1.1, y el analizador byte a byte.
//   g++ -std=c++11 -I../../Arduino/Wifi_module_01 test_codec.cpp -o test_codec && ./test_codec
#include "mqtt_codec.h"
#include <stdio.h>
#include <vector>

static int errors = 0;
#define CHECK(c, msg) do { if (!(c)) { printf("ERROR %s\n", msg); errors++; } } while (0)

static bool same(const uint8_t* got, size_t n, std::vector<uint8_t> want) {
  if (n != want.size()) { printf("  longitud %zu, esperada %zu\n", n, want.size()); return false; }
  for (size_t i = 0; i < n; i++) if (got[i] != want[i]) { printf("  byte %zu: %02X, esperado %02X\n", i, got[i], want[i]); return false; }
  return true;
}

int main() {
  uint8_t b[512];
  size_t n;

  mqtt::ConnectOpts o = {};
  o.client_id = "a"; o.keepalive = 60;
  n = mqtt::connect(b, sizeof(b), o);
  CHECK(same(b, n, {0x10, 0x0D, 0, 4, 'M', 'Q', 'T', 'T', 4, 0x02, 0, 60, 0, 1, 'a'}), "CONNECT basico");

  const uint8_t wd[] = {'0'};
  o.will_topic = "w"; o.will_data = wd; o.will_len = 1; o.will_retain = true;
  o.user = "u"; o.pass = "p";
  n = mqtt::connect(b, sizeof(b), o);
  CHECK(same(b, n, {0x10, 0x19, 0, 4, 'M', 'Q', 'T', 'T', 4, 0xE6, 0, 60, 0, 1, 'a',
                    0, 1, 'w', 0, 1, '0', 0, 1, 'u', 0, 1, 'p'}), "CONNECT con will retenido, usuario y clave");

  n = mqtt::publish(b, sizeof(b), "a/b", (const uint8_t*)"hi", 2, 0, false, false, 0);
  CHECK(same(b, n, {0x30, 0x07, 0, 3, 'a', '/', 'b', 'h', 'i'}), "PUBLISH QoS 0");

  n = mqtt::publish(b, sizeof(b), "a/b", (const uint8_t*)"hi", 2, 1, true, false, 5);
  CHECK(same(b, n, {0x33, 0x09, 0, 3, 'a', '/', 'b', 0, 5, 'h', 'i'}), "PUBLISH QoS 1 retenido");

  n = mqtt::publish(b, sizeof(b), "a/b", (const uint8_t*)"hi", 2, 1, false, true, 5);
  CHECK(b[0] == 0x3A, "PUBLISH QoS 1 con DUP");

  n = mqtt::publish(b, sizeof(b), "a/b", 0, 0, 0, true, false, 0);
  CHECK(same(b, n, {0x31, 0x05, 0, 3, 'a', '/', 'b'}), "PUBLISH vacio retenido (borra el retenido)");

  n = mqtt::subscribe(b, sizeof(b), 1, "a/b", 1);
  CHECK(same(b, n, {0x82, 0x08, 0, 1, 0, 3, 'a', '/', 'b', 1}), "SUBSCRIBE");

  n = mqtt::unsubscribe(b, sizeof(b), 2, "a/b");
  CHECK(same(b, n, {0xA2, 0x07, 0, 2, 0, 3, 'a', '/', 'b'}), "UNSUBSCRIBE");

  n = mqtt::puback(b, sizeof(b), 0x0102);
  CHECK(same(b, n, {0x40, 0x02, 1, 2}), "PUBACK");
  n = mqtt::pingreq(b, sizeof(b));
  CHECK(same(b, n, {0xC0, 0}), "PINGREQ");
  n = mqtt::disconnect(b, sizeof(b));
  CHECK(same(b, n, {0xE0, 0}), "DISCONNECT");

  // Una longitud restante de dos bytes (200 -> C8 01)
  uint8_t big[200]; memset(big, 'x', sizeof(big));
  n = mqtt::publish(b, sizeof(b), "t", big, 190, 0, false, false, 0);   // 2+1+190 = 193
  CHECK(n == 196 && b[1] == 0xC1 && b[2] == 0x01, "longitud restante de 2 bytes");

  // No cabe
  n = mqtt::publish(b, 10, "a/b", big, 100, 0, false, false, 0);
  CHECK(n == 0, "no cabe: 0");

  // ---- analizador, byte a byte
  mqtt::Parser p;
  mqtt::Packet pk;
  const uint8_t connack[] = {0x20, 0x02, 0x00, 0x00};
  int r = 0;
  for (size_t i = 0; i < sizeof(connack); i++) r = p.feed(connack[i], &pk);
  CHECK(r == mqtt::Parser::READY && pk.type == mqtt::T_CONNACK && pk.len == 2 && pk.body[1] == 0, "CONNACK");

  const uint8_t pingresp[] = {0xD0, 0x00};
  r = 0;
  for (size_t i = 0; i < sizeof(pingresp); i++) r = p.feed(pingresp[i], &pk);
  CHECK(r == mqtt::Parser::READY && pk.type == mqtt::T_PINGRESP && pk.len == 0, "PINGRESP (sin cuerpo)");

  // PUBLISH QoS 1 con retain: topic "sd81/x", id 7, datos "ABC"
  uint8_t in[64];
  n = mqtt::publish(in, sizeof(in), "sd81/x", (const uint8_t*)"ABC", 3, 1, true, false, 7);
  r = 0;
  for (size_t i = 0; i < n; i++) { r = p.feed(in[i], &pk); if (i + 1 < n) CHECK(r == 0, "no antes de tiempo"); }
  mqtt::PublishIn pi;
  CHECK(r == mqtt::Parser::READY && mqtt::parse_publish(pk, &pi), "PUBLISH recibido");
  CHECK(pi.topic_len == 6 && memcmp(pi.topic, "sd81/x", 6) == 0 && pi.data_len == 3 && memcmp(pi.data, "ABC", 3) == 0 &&
        pi.qos == 1 && pi.retain && pi.id == 7, "PUBLISH: topic, datos, qos, retain e id");

  // Uno demasiado grande se descarta entero y el siguiente se lee bien
  uint8_t huge[600]; memset(huge, 'h', sizeof(huge));
  uint8_t pkt[1024];
  size_t hn = 0;
  {
    // 2+1+500 = 503 de cuerpo: 0xF7 0x03
    pkt[hn++] = 0x30; pkt[hn++] = 0xF7; pkt[hn++] = 0x03;
    pkt[hn++] = 0; pkt[hn++] = 1; pkt[hn++] = 't';
    for (int i = 0; i < 500; i++) pkt[hn++] = 'h';
  }
  int dropped = 0, ready = 0;
  for (size_t i = 0; i < hn; i++) { r = p.feed(pkt[i], &pk); if (r == mqtt::Parser::DROPPED) dropped++; if (r == mqtt::Parser::READY) ready++; }
  CHECK(dropped == 1 && ready == 0, "PUBLISH enorme: descartado");
  for (size_t i = 0; i < sizeof(connack); i++) r = p.feed(connack[i], &pk);
  CHECK(r == mqtt::Parser::READY && pk.type == mqtt::T_CONNACK, "tras el descartado, el flujo sigue");

  // Longitud restante invalida (5 bytes con continuacion)
  p.reset();
  p.feed(0x30, &pk);
  r = 0;
  for (int i = 0; i < 5; i++) r = p.feed(0xFF, &pk);
  CHECK(r == mqtt::Parser::BAD, "longitud restante invalida");

  if (errors) printf("%d ERRORES\n", errors); else printf("TODO OK\n");
  return errors ? 1 : 0;
}
