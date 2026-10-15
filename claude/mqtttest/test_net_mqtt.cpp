// NET_MQTT.cpp en el PC, contra un broker falso: los comandos AT, la sesion, la cola de
// recibidos, QoS 1, la reconexion y el ping. build.sh lo compila y lo ejecuta.
#include "mock.h"
#include "NET_MQTT.h"
#include "mqtt_codec.h"

// ---------------------------------------------------------------- mocks
uint32_t mock_now = 100000;
SerialMock Serial;
WiFiMock WiFi;
bool WiFiMock::mock_wifi_ok = true;
std::vector<uint8_t> mock_md_in, mock_md_key;
bool mock_cfg_present = false; std::string mock_cfg;
std::string mock_txq; uint16_t mock_txq_room = 256;
uint16_t mock_raw_len = 0; bool mock_raw_pending = false; net_raw_done_t mock_raw_cb = nullptr;
std::deque<uint8_t> mock_from_broker;
bool mock_connected = false, mock_tcp_fail = false;

void net_bridge_txq_push_str(const char* s) { mock_txq += s; }
void net_bridge_txq_push_bytes(const uint8_t* b, uint16_t n) { mock_txq.append((const char*)b, n); }
uint16_t net_bridge_txq_free() { return mock_txq_room; }
bool net_bridge_begin_raw(uint16_t len, net_raw_done_t done) {
  if (mock_raw_pending || len == 0 || len > 256) return false;
  mock_raw_pending = true; mock_raw_len = len; mock_raw_cb = done; mock_txq += ">";
  return true;
}
// el Z80 manda los bytes crudos
static void feed_raw(const std::string& s) {
  if (!mock_raw_pending || s.size() != mock_raw_len) { printf("  feed_raw: nada pendiente o longitud distinta\n"); return; }
  mock_raw_pending = false;
  mock_raw_cb((const uint8_t*)s.data(), (uint16_t)s.size(), true);
}

bool mock_quiet = false;

// ---------------------------------------------------------------- broker falso
static mqtt::Parser bparser;
struct Seen { std::string kind, topic, data; uint8_t qos = 0; bool retain = false, dup = false; uint8_t flags = 0; };
static std::vector<Seen> seen;
static bool broker_acks = true, broker_refuses = false;
static std::string will_seen;

static void send_to_client(const uint8_t* b, size_t n) { for (size_t i = 0; i < n; i++) mock_from_broker.push_back(b[i]); }

void mock_broker_rx(const uint8_t* b, size_t n) {
  for (size_t i = 0; i < n; i++) {
    mqtt::Packet pk;
    if (bparser.feed(b[i], &pk) != mqtt::Parser::READY) continue;
    Seen s;
    uint8_t out[64];
    switch (pk.type) {
      case mqtt::T_CONNECT: {
        s.kind = "CONNECT";
        // "MQTT"(6) nivel(1) flags(1) keepalive(2) clientid...
        s.flags = pk.body[7];
        uint16_t cl = (uint16_t)((pk.body[10] << 8) | pk.body[11]);
        s.data = std::string((const char*)pk.body + 12, cl);       // clientid
        uint16_t pos = 12 + cl;
        if (s.flags & 0x04) {
          uint16_t tl = (uint16_t)((pk.body[pos] << 8) | pk.body[pos + 1]);
          s.topic = std::string((const char*)pk.body + pos + 2, tl);
        }
        uint8_t ca[4] = {0x20, 0x02, 0x00, (uint8_t)(broker_refuses ? 5 : 0)};
        if (broker_acks) send_to_client(ca, 4);
        break;
      }
      case mqtt::T_SUBSCRIBE: {
        s.kind = "SUBSCRIBE";
        uint16_t tl = (uint16_t)((pk.body[2] << 8) | pk.body[3]);
        s.topic = std::string((const char*)pk.body + 4, tl);
        s.qos = pk.body[4 + tl];
        uint8_t sa[5] = {0x90, 0x03, pk.body[0], pk.body[1], s.qos};
        if (broker_acks) send_to_client(sa, 5);
        break;
      }
      case mqtt::T_UNSUBSCRIBE: {
        s.kind = "UNSUBSCRIBE";
        uint16_t tl = (uint16_t)((pk.body[2] << 8) | pk.body[3]);
        s.topic = std::string((const char*)pk.body + 4, tl);
        break;
      }
      case mqtt::T_PUBLISH: {
        mqtt::PublishIn p; mqtt::parse_publish(pk, &p);
        s.kind = "PUBLISH"; s.topic = std::string((const char*)p.topic, p.topic_len);
        s.data = std::string((const char*)p.data, p.data_len);
        s.qos = p.qos; s.retain = p.retain; s.dup = pk.flags & 8;
        if (p.qos == 1 && broker_acks) { size_t k = mqtt::puback(out, sizeof(out), p.id); send_to_client(out, k); }
        break;
      }
      case mqtt::T_PUBACK:   s.kind = "PUBACK"; break;
      case mqtt::T_PINGREQ:  { s.kind = "PINGREQ"; uint8_t pr[2] = {0xD0, 0}; send_to_client(pr, 2); break; }
      case mqtt::T_DISCONNECT: s.kind = "DISCONNECT"; break;
      default: s.kind = "?"; break;
    }
    seen.push_back(s);
  }
}

static void broker_publish(const char* topic, const std::string& data, uint8_t qos, bool retain, uint16_t id) {
  uint8_t pkt[400];
  size_t n = mqtt::publish(pkt, sizeof(pkt), topic, (const uint8_t*)data.data(), data.size(), qos, retain, false, id);
  send_to_client(pkt, n);
}

// ---------------------------------------------------------------- utilidades de la prueba
static int errors = 0;
#define CHECK(c, msg) do { if (!(c)) { printf("ERROR %s\n", msg); errors++; } } while (0)

static std::string at(const char* args) {          // args = lo que va tras "AT"
  mock_txq.clear();
  net_mqtt_at(args, (uint8_t)strlen(args));
  std::string r = mock_txq; mock_txq.clear();
  return r;
}
static int count_seen(const char* kind) { int n = 0; for (auto& s : seen) if (s.kind == kind) n++; return n; }
static const Seen* last_seen(const char* kind) { for (int i = (int)seen.size() - 1; i >= 0; i--) if (seen[i].kind == kind) return &seen[i]; return nullptr; }
static void loop_for(uint32_t ms) { for (uint32_t t = 0; t < ms; t += 100) { mock_now += 100; net_mqtt_loop(); } }

int main() {
  net_mqtt_init();

  CHECK(at("+MQTT?") == "+MQTT:0,-,0,0,0\r\nOK\r\n", "estado inicial");

  // argumentos malos
  CHECK(at("+MQTT=host") == "ERROR\r\n", "MQTT sin puerto: ERROR");
  CHECK(at("+MQTT=host,0") == "ERROR\r\n", "MQTT puerto 0: ERROR");
  CHECK(at("+MQTTSUB=a/b") == "ERROR\r\n", "SUB sin sesion: ERROR");
  CHECK(at("+MQTTPUB=a,0,0,1") == "ERROR\r\n", "PUB sin sesion: ERROR");
  CHECK(at("+NOSE") == "", "comando desconocido: net_mqtt_at devuelve false (el puente contesta ERROR)");

  // will + clientid + conectar
  CHECK(at("+MQTTWILL=Sd81/ABC/p0,1,0") == "OK\r\n", "WILL");
  CHECK(at("+MQTTID=Jugador1") == "OK\r\n", "ID");
  CHECK(at("+MQTT=broker.test,1883") == "OK\r\n", "MQTT=host,puerto conecta");
  {
    const Seen* c = last_seen("CONNECT");
    CHECK(c && c->data == "Jugador1", "CONNECT con el clientid puesto (mayusculas respetadas)");
    CHECK(c && (c->flags & 0x04) && (c->flags & 0x20) && c->topic == "Sd81/ABC/p0", "CONNECT con will retenido y topic con mayusculas");
  }
  CHECK(at("+MQTT?") == "+MQTT:2,broker.test,0,0,0\r\nOK\r\n", "estado: conectado");

  // suscribir
  CHECK(at("+MQTTSUB=Sd81/ABC/+,1") == "OK\r\n", "SUB con SUBACK");
  CHECK(last_seen("SUBSCRIBE") && last_seen("SUBSCRIBE")->topic == "Sd81/ABC/+" && last_seen("SUBSCRIBE")->qos == 1, "el broker vio la suscripcion");
  CHECK(at("+MQTTSUB=") == "ERROR\r\n", "SUB sin topic");

  // recibir un mensaje retenido QoS 1
  broker_publish("Sd81/ABC/1", "HOLA", 1, true, 3);
  loop_for(100);
  { std::string q = at("+MQTT?"); CHECK(q == "+MQTT:2,broker.test,1,0,5\r\nOK\r\n", "1 mensaje en cola (ultimo error: el 5 de la SUB mal escrita)"); }
  CHECK(count_seen("PUBACK") == 1, "PUBACK al mensaje QoS 1");
  CHECK(at("+MQTTRX") == "+MQTTRX:Sd81/ABC/1,4\r\nHOLA\r\nOK\r\n", "MQTTRX entrega topic, longitud y datos");
  CHECK(at("+MQTTRX") == "OK\r\n", "MQTTRX con la cola vacia: OK");

  // binario seguro: bytes 0, CR, LF y 0xFF
  {
    std::string bin("\x00\r\n\xFF\x01", 5);
    broker_publish("Sd81/ABC/1", bin, 0, false, 0);
    loop_for(100);
    std::string r = at("+MQTTRX");
    CHECK(r == std::string("+MQTTRX:Sd81/ABC/1,5\r\n") + bin + "\r\nOK\r\n", "MQTTRX binario seguro");
  }

  // cola llena: 10 mensajes, caben 8
  for (int i = 0; i < 10; i++) broker_publish("Sd81/ABC/1", std::string(1, (char)('0' + i)), 0, false, 0);
  loop_for(100);
  { std::string q = at("+MQTT?"); CHECK(q == "+MQTT:2,broker.test,8,2,5\r\nOK\r\n", "cola llena: 8 en cola, 2 perdidos"); }
  CHECK(at("+MQTTRX") == "+MQTTRX:Sd81/ABC/1,1\r\n2\r\nOK\r\n", "se perdieron los mas antiguos (0 y 1)");
  for (int i = 0; i < 7; i++) at("+MQTTRX");
  // sin sitio en la cola de salida del puente: el mensaje se queda
  broker_publish("Sd81/ABC/1", "X", 0, false, 0);
  loop_for(100);
  mock_txq_room = 10;
  CHECK(at("+MQTTRX") == "ERROR\r\n", "MQTTRX sin sitio: ERROR y el mensaje sigue");
  mock_txq_room = 256;
  CHECK(at("+MQTTRX") == "+MQTTRX:Sd81/ABC/1,1\r\nX\r\nOK\r\n", "y luego sale");

  // publicar QoS 1 con retain
  CHECK(at("+MQTTPUB=Sd81/ABC/0,1,1,5") == ">", "PUB pide los datos con '>'");
  mock_txq.clear();
  feed_raw("hello");
  CHECK(mock_txq == "OK\r\n", "PUB: OK tras el PUBACK");
  {
    const Seen* p = last_seen("PUBLISH");
    CHECK(p && p->topic == "Sd81/ABC/0" && p->data == "hello" && p->qos == 1 && p->retain && !p->dup, "el broker recibio el PUBLISH");
  }
  CHECK(at("+MQTTPUB=a/+,0,0,1") == "ERROR\r\n", "PUB con comodin: ERROR");
  CHECK(at("+MQTTPUB=a,2,0,1") == "ERROR\r\n", "PUB QoS 2: ERROR");
  CHECK(at("+MQTTPUB=a,0,0,121") == "ERROR\r\n", "PUB de mas de 120: ERROR");
  // mensaje vacio: borra el retenido, sin '>'
  CHECK(at("+MQTTPUB=Sd81/ABC/0,0,1,0") == "OK\r\n", "PUB vacio: OK sin datos");
  CHECK(last_seen("PUBLISH")->data == "" && last_seen("PUBLISH")->retain, "el vacio retenido llego");

  // el broker no contesta al PUBACK
  broker_acks = false;
  CHECK(at("+MQTTPUB=Sd81/ABC/0,1,0,2") == ">", "PUB sin ack: pide datos");
  mock_txq.clear();
  int before = count_seen("PUBLISH");
  feed_raw("ab");
  CHECK(mock_txq == "ERROR\r\n", "sin PUBACK: ERROR");
  CHECK(count_seen("PUBLISH") == before + 2 && last_seen("PUBLISH")->dup, "dos intentos, el segundo con DUP");
  CHECK(at("+MQTT?").find(",8\r\n") != std::string::npos, "el ultimo error es 8 (PUBACK)");
  broker_acks = true;

  // el ping
  int pings = count_seen("PINGREQ");
  loop_for(31000);
  CHECK(count_seen("PINGREQ") == pings + 1, "PINGREQ a la mitad del keepalive");
  CHECK(at("+MQTT?").substr(0, 8) == "+MQTT:2,", "y sigue conectado (PINGRESP)");

  // se cae el broker: reconexion y resuscripcion
  int subs_before = count_seen("SUBSCRIBE");
  mock_connected = false;
  loop_for(100);
  CHECK(at("+MQTT?").substr(0, 8) == "+MQTT:3,", "caida: estado 3 (reconectando)");
  loop_for(2500);
  CHECK(at("+MQTT?").substr(0, 8) == "+MQTT:2,", "reconectado");
  CHECK(count_seen("SUBSCRIBE") == subs_before + 1, "se vuelve a suscribir solo");

  // cerrar
  CHECK(at("+MQTTCLOSE") == "OK\r\n", "CLOSE");
  CHECK(count_seen("DISCONNECT") == 1, "DISCONNECT al broker");
  CHECK(at("+MQTT?") == "+MQTT:0,-,0,0,6\r\nOK\r\n" || at("+MQTT?").substr(0, 8) == "+MQTT:0,", "estado: cerrado");

  // el broker rechaza (CONNACK 5) y el TCP falla; sin NET.CFG: los 3 por defecto
  broker_refuses = true;
  CHECK(at("+MQTT=otro.test,1883") == "ERROR\r\n", "CONNACK 5: ERROR");
  broker_refuses = false; mock_tcp_fail = true;
  CHECK(at("+MQTT") == "ERROR\r\n", "sin brokers que respondan: ERROR");
  mock_tcp_fail = false;
  seen.clear();
  CHECK(at("+MQTT") == "OK\r\n", "AT+MQTT sin argumentos: usa la lista por defecto");
  CHECK(at("+MQTT?").find("test.mosquitto.org") != std::string::npos, "el primero por defecto: test.mosquitto.org");
  at("+MQTTCLOSE");

  // NET.CFG
  mock_cfg_present = true;
  mock_cfg = "# brokers\nbroker uno.test 1884\r\nbroker dos.test 8883 tls\n\nmal formado\n";
  mock_tcp_fail = false;
  CHECK(at("+MQTT") == "OK\r\n", "con NET.CFG");
  CHECK(at("+MQTT?").find("uno.test") != std::string::npos, "se usa el primero del fichero");
  at("+MQTTCLOSE");

  // usuario y clave
  CHECK(at("+MQTT=b.test,1883,0,pepe,Clave") == "OK\r\n", "con usuario y clave");
  CHECK(last_seen("CONNECT") && (last_seen("CONNECT")->flags & 0xC0) == 0xC0, "CONNECT con usuario y clave");
  CHECK(at("+MQTT=b.test,1883,0,pepe") == "ERROR\r\n", "usuario sin clave: ERROR");
  at("+MQTTCLOSE");

  // utilidades criptograficas y aleatorios
  CHECK(at("+SHA256=3") == ">", "SHA256 pide datos");
  mock_txq.clear(); feed_raw("abc");
  CHECK(mock_txq == "+SHA256:A0A1A2A3A4A5A6A7A8A9AAABACADAEAFB0B1B2B3B4B5B6B7B8B9BABBBCBDBEBF\r\nOK\r\n", "SHA256: respuesta en hexadecimal");
  CHECK(std::string(mock_md_in.begin(), mock_md_in.end()) == "abc", "SHA256 recibio los datos");
  CHECK(at("+HMAC=3,5") == ">", "HMAC pide datos");
  mock_txq.clear(); feed_raw("keyDATA!");
  CHECK(mock_txq.substr(0, 14) == "+HMAC:10111213", "HMAC: respuesta en hexadecimal");
  CHECK(std::string(mock_md_key.begin(), mock_md_key.end()) == "key" && std::string(mock_md_in.begin(), mock_md_in.end()) == "DATA!", "HMAC: clave y datos separados por keylen");
  CHECK(at("+HMAC=0,5") == "ERROR\r\n" && at("+HMAC=200,100") == "ERROR\r\n" && at("+SHA256=0") == "ERROR\r\n", "argumentos malos de SHA256 / HMAC");
  {
    std::string r = at("+RAND=4");
    CHECK(r.size() == strlen("+RAND:") + 8 + 2 + 4 && r.substr(0, 6) == "+RAND:" && r.substr(r.size() - 4) == "OK\r\n", "RAND=4: 8 caracteres hexadecimales");
    CHECK(at("+RAND=33") == "ERROR\r\n" && at("+RAND=0") == "ERROR\r\n", "RAND fuera de rango");
  }

  if (errors) printf("%d ERRORES\n", errors); else printf("TODO OK\n");
  return errors ? 1 : 0;
}
