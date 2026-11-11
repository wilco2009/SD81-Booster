#include "MOUSE_BLE.h"

#if ESP_MOUSE_BLE

#include <stdarg.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>
#include <BLERemoteService.h>
#include <BLERemoteCharacteristic.h>
#include <BLERemoteDescriptor.h>
#include <BLESecurity.h>
#include <Preferences.h>
#include "WIFI_CLIENT.h"
#if __has_include("host/ble_store.h")
#include "host/ble_store.h"
#define HAVE_BLE_STORE 1
#endif
#include "SD_LOG.h"      // redefine Serial: lo que salga por Serial se copia tambien a /SYS/ESP32LOG.TXT (si SDLOG=1)

// ------------------------------------------------------------------------------------------------------------------------------------
// Descriptor de informes HID: donde esta cada campo del informe del raton
// ------------------------------------------------------------------------------------------------------------------------------------
struct Field {
  bool     ok   = false;
  uint16_t bit  = 0;       // posicion del primer bit dentro del informe (sin contar el byte del ID)
  uint8_t  size = 0;       // bits
  bool     sign = false;
};

struct Layout {
  bool    ok     = false;
  bool    has_id = false;  // el descriptor usa Report ID
  uint8_t id     = 0;
  Field   x, y, wheel;
  Field   btn[8];
};

static int32_t get_bits(const uint8_t* d, size_t n, uint16_t bit, uint8_t size, bool sign) {
  uint32_t v = 0;
  for (uint8_t i = 0; i < size && i < 32; i++) {
    uint16_t b = bit + i;
    if ((size_t)(b >> 3) >= n) return 0;                      // el informe es mas corto de lo que dice el descriptor
    if (d[b >> 3] & (1u << (b & 7))) v |= (1u << i);
  }
  if (sign && size > 0 && size < 32 && (v & (1u << (size - 1)))) v |= (~0u << size);
  return (int32_t)v;
}

// Recorre el descriptor y rellena `out` con el primer informe de entrada que tenga X e Y. Devuelve false si no hay ninguno.
static bool parse_report_map(const uint8_t* d, size_t n, Layout* out) {
  uint32_t usage_page = 0, rsize = 0, rcount = 0;
  int32_t  lmin = 0;
  uint8_t  rid = 0;
  bool     has_id = false;
  uint16_t usages[24];
  uint8_t  nus = 0;
  uint32_t umin = 0, umax = 0;
  bool     have_range = false;

  Layout   lay[8];                       // un Layout y una posicion de bit por Report ID (hasta 8 distintos)
  uint8_t  ids[8];
  uint16_t pos[8];
  uint8_t  nids = 0;
  auto slot = [&](uint8_t id) -> uint8_t {
    for (uint8_t i = 0; i < nids; i++) if (ids[i] == id) return i;
    if (nids < 8) { ids[nids] = id; pos[nids] = 0; lay[nids].has_id = has_id; lay[nids].id = id; return nids++; }
    return 7;
  };

  size_t i = 0;
  while (i < n) {
    uint8_t b = d[i++];
    if (b == 0xFE) {                     // item largo: se salta
      if (i >= n) break;
      i += 2 + d[i];
      continue;
    }
    uint8_t sz = b & 3; if (sz == 3) sz = 4;
    uint8_t type = (b >> 2) & 3, tag = b >> 4;
    if (i + sz > n) break;
    uint32_t v = 0;
    for (uint8_t k = 0; k < sz; k++) v |= (uint32_t)d[i + k] << (8 * k);
    int32_t sv = (int32_t)v;
    if (sz && sz < 4 && (v & (1u << (8 * sz - 1)))) sv = (int32_t)(v | (~0u << (8 * sz)));
    i += sz;

    if (type == 1) {                     // global
      switch (tag) {
        case 0: usage_page = v; break;
        case 1: lmin = sv; break;
        case 7: rsize = v; break;
        case 8: rid = (uint8_t)v; has_id = true; break;
        case 9: rcount = v; break;
        default: break;
      }
    } else if (type == 2) {              // local
      switch (tag) {
        case 0: if (nus < 24) usages[nus++] = (uint16_t)v; break;
        case 1: umin = v; have_range = true; break;
        case 2: umax = v; have_range = true; break;
        default: break;
      }
    } else if (type == 0) {              // main
      if (tag == 8) {                    // Input
        uint8_t s = slot(rid);
        bool constant = (v & 1) != 0;
        for (uint32_t k = 0; k < rcount; k++) {
          if (!constant) {
            int32_t u = -1;
            if (have_range) { u = (int32_t)umin + (int32_t)k; if ((uint32_t)u > umax) u = (int32_t)umax; }
            else if (nus) u = usages[k < nus ? k : nus - 1];
            Field f; f.ok = true; f.bit = pos[s]; f.size = (uint8_t)rsize; f.sign = lmin < 0;
            if (usage_page == 0x09 && u >= 1 && u <= 8)  lay[s].btn[u - 1] = f;
            else if (usage_page == 0x01 && u == 0x30)    lay[s].x = f;
            else if (usage_page == 0x01 && u == 0x31)    lay[s].y = f;
            else if (usage_page == 0x01 && u == 0x38)    lay[s].wheel = f;
          }
          pos[s] += (uint16_t)rsize;
        }
      }
      nus = 0; have_range = false;       // los locales se borran tras cada item main
    }
  }

  for (uint8_t s = 0; s < nids; s++) {
    if (lay[s].x.ok && lay[s].y.ok) { *out = lay[s]; out->ok = true; return true; }
  }
  return false;
}

// Informe "boot" del raton (HID 1.11, apendice B): byte 0 = botones, 1 = dx, 2 = dy, 3 = rueda (opcional)
static Layout boot_layout() {
  Layout l;
  l.ok = true;
  for (uint8_t i = 0; i < 3; i++) { l.btn[i].ok = true; l.btn[i].bit = i; l.btn[i].size = 1; }
  l.x.ok = true;     l.x.bit = 8;      l.x.size = 8;     l.x.sign = true;
  l.y.ok = true;     l.y.bit = 16;     l.y.size = 8;     l.y.sign = true;
  l.wheel.ok = true; l.wheel.bit = 24; l.wheel.size = 8; l.wheel.sign = true;
  return l;
}

// ------------------------------------------------------------------------------------------------------------------------------------
// Estado compartido: lo escribe la tarea BLE (notificaciones) y lo recoge la tarea que manda al STM32
// ------------------------------------------------------------------------------------------------------------------------------------
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static int32_t  s_acc_dx = 0, s_acc_dy = 0, s_acc_wheel = 0;
static uint8_t  s_buttons = 0;
static bool     s_dirty = false;
static uint32_t s_first_us = 0;             // micros() del evento sin enviar mas antiguo (para medir la latencia)
static TaskHandle_t s_tx_task = nullptr;

static volatile bool s_enabled   = false;
static volatile bool s_connected = false;
static char     s_state[64] = "parado";
static char     s_devname[40] = "";
static char     s_devaddr[24] = "";
static int      s_rssi = 0;
static char     s_mode[16] = "-";
static char     s_layout_txt[96] = "";

static uint32_t s_notif = 0, s_tx_ok = 0, s_tx_fail = 0;
static uint32_t s_lat_n = 0, s_lat_sum = 0, s_lat_max = 0;      // us, de la notificacion BLE a la respuesta del STM32
static uint16_t s_st_x = 0, s_st_y = 0; static uint8_t s_st_btn = 0; static bool s_st_valid = false;
static uint32_t s_conn_count = 0;

static void set_state(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
static void set_state(const char* fmt, ...) {
  va_list ap; va_start(ap, fmt);
  vsnprintf(s_state, sizeof s_state, fmt, ap);
  va_end(ap);
  Serial.print("BLE raton: "); Serial.println(s_state);
}

static void on_input(const uint8_t* d, size_t n, const Layout& L) {
  int32_t dx = L.x.ok ? get_bits(d, n, L.x.bit, L.x.size, L.x.sign) : 0;
  int32_t dy = L.y.ok ? get_bits(d, n, L.y.bit, L.y.size, L.y.sign) : 0;
  int32_t wh = L.wheel.ok ? get_bits(d, n, L.wheel.bit, L.wheel.size, L.wheel.sign) : 0;
  uint8_t btn = 0;
  for (uint8_t i = 0; i < 8; i++) if (L.btn[i].ok && get_bits(d, n, L.btn[i].bit, L.btn[i].size, false)) btn |= (uint8_t)(1u << i);

  portENTER_CRITICAL(&s_mux);
  s_acc_dx += dx; s_acc_dy += dy; s_acc_wheel += wh;
  s_buttons = btn;
  s_dirty = true;
  if (!s_first_us) s_first_us = micros();
  s_notif++;
  portEXIT_CRITICAL(&s_mux);
  if (s_tx_task) xTaskNotifyGive(s_tx_task);
}

// ------------------------------------------------------------------------------------------------------------------------------------
// Tarea que manda al STM32 (prioridad 5, por encima de loop())
// ------------------------------------------------------------------------------------------------------------------------------------
static int16_t clamp16(int32_t v) { return v < -32768 ? -32768 : (v > 32767 ? 32767 : (int16_t)v); }

static void ble_stm_sync();     // CMD_BLE: estado hacia el STM32 y ordenes del Z80/BASIC (mas abajo)

static void tx_task(void*) {
  for (;;) {
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(250));      // se despierta con cada informe; el timeout solo es para revisar de vez en cuando
    int32_t dx, dy, wh; uint8_t btn; uint32_t t_first;
    bool dirty;
    portENTER_CRITICAL(&s_mux);
    dirty = s_dirty;
    dx = s_acc_dx; dy = s_acc_dy; wh = s_acc_wheel; btn = s_buttons; t_first = s_first_us;
    if (dirty) { s_acc_dx = s_acc_dy = s_acc_wheel = 0; s_dirty = false; s_first_us = 0; }
    portEXIT_CRITICAL(&s_mux);
    ble_stm_sync();                                    // como mucho cada 0,5 s (o antes si cambia el estado)
    if (!dirty) continue;

    uint8_t req[6];
    req[0] = btn;
    int16_t cx = clamp16(dx), cy = clamp16(dy);
    memcpy(&req[1], &cx, 2);
    memcpy(&req[3], &cy, 2);
    req[5] = (uint8_t)(int8_t)(wh < -128 ? -128 : (wh > 127 ? 127 : wh));

    WifiProtoResp r = wifi_client_request(CMD_MOUSE, req, sizeof req);
    uint32_t t1 = micros();
    if (r.ok && r.len >= 6 && r.payload[0] == ST_OK) {
      uint32_t lat = t1 - t_first;
      portENTER_CRITICAL(&s_mux);
      s_tx_ok++;
      s_lat_n++; s_lat_sum += lat; if (lat > s_lat_max) s_lat_max = lat;
      memcpy(&s_st_x, &r.payload[1], 2); memcpy(&s_st_y, &r.payload[3], 2); s_st_btn = r.payload[5]; s_st_valid = true;
      // lo que no cabia en un int16 (nunca en la practica) vuelve al acumulador
      s_acc_dx += dx - cx; s_acc_dy += dy - cy;
      portEXIT_CRITICAL(&s_mux);
    } else {
      portENTER_CRITICAL(&s_mux);                       // el STM32 no contesto: no se pierde movimiento, vuelve al acumulador
      s_tx_fail++;
      s_acc_dx += dx; s_acc_dy += dy; s_acc_wheel += wh;
      s_dirty = true;
      if (!s_first_us) s_first_us = t_first;
      portEXIT_CRITICAL(&s_mux);
      vTaskDelay(pdMS_TO_TICKS(20));
    }
    vTaskDelay(pdMS_TO_TICKS(MOUSE_BLE_MIN_GAP_MS));    // junta los informes que lleguen casi a la vez
    if (s_dirty) xTaskNotifyGive(xTaskGetCurrentTaskHandle());
  }
}

// ------------------------------------------------------------------------------------------------------------------------------------
// Conexion BLE
// ------------------------------------------------------------------------------------------------------------------------------------
static char s_saved_addr[24] = "";
static char s_saved_name[40] = "";     // nombre del raton guardado: sirve para reconocerlo si cambia de direccion al reiniciarse (el M240 lo hace)

// Ventana de emparejado: SOLO mientras esta abierta ("esp ble pair") se acepta un raton nuevo; fuera de ella solo se conecta al guardado.
// Asi un movil, un teclado o el raton del vecino no se emparejan solos.
static volatile uint32_t s_pair_until = 0;                                       // millis() en que se cierra; 0 = cerrada
static bool pairing() { uint32_t u = s_pair_until; return u && (int32_t)(millis() - u) < 0; }
static uint32_t pair_left_s() { return pairing() ? (s_pair_until - millis() + 999) / 1000 : 0; }

static void nvs_load() {
  Preferences p;
  if (p.begin("sd81mouse", true)) {
    s_enabled = p.getBool("ble", false);
    String a = p.getString("addr", "");
    a.toCharArray(s_saved_addr, sizeof s_saved_addr);
    String n = p.getString("name", "");
    n.toCharArray(s_saved_name, sizeof s_saved_name);
    p.end();
  }
}
static void nvs_save_name(const char* n) {
  strncpy(s_saved_name, n, sizeof s_saved_name - 1); s_saved_name[sizeof s_saved_name - 1] = 0;
  Preferences p; if (p.begin("sd81mouse", false)) { p.putString("name", n); p.end(); }
}
static void nvs_forget_all() {
  s_saved_addr[0] = 0; s_saved_name[0] = 0;
  Preferences p; if (p.begin("sd81mouse", false)) { p.remove("addr"); p.remove("name"); p.end(); }
}
static void nvs_save_enabled() { Preferences p; if (p.begin("sd81mouse", false)) { p.putBool("ble", s_enabled); p.end(); } }
static void nvs_save_addr(const char* a) {
  strncpy(s_saved_addr, a, sizeof s_saved_addr - 1); s_saved_addr[sizeof s_saved_addr - 1] = 0;
  Preferences p; if (p.begin("sd81mouse", false)) { p.putString("addr", a); p.end(); }
}

static BLEClient* s_client = nullptr;

class CliCb : public BLEClientCallbacks {
 public:
  void onConnect(BLEClient*) override {}
  void onDisconnect(BLEClient*) override {
    s_connected = false;
    set_state("desconectado");
  }
};
static CliCb s_cli_cb;

// El mejor candidato de la busqueda actual (lo rellena el callback del escaneo, que corre en la tarea del host BLE)
static bool       s_cand_ok = false;
static BLEAddress s_cand_addr;
static uint8_t    s_cand_type = 0;
static int        s_cand_score = -1000;
static char       s_cand_name[40] = "";
static int        s_cand_rssi = 0;

// Los HID vistos en la busqueda en curso (solo para diagnostico: "esp ble" los lista). Si el guardado no aparece, aqui se ve que anuncia el raton.
struct SeenDev { char addr[18]; uint8_t type; int rssi; char name[24]; bool hid, mouse; };
static SeenDev s_seen[4];
static uint8_t s_seen_n = 0;
static void seen_add(const String& addr, uint8_t type, int rssi, const String& name, bool hid, bool mouse) {
  for (uint8_t i = 0; i < s_seen_n; i++) if (addr.equalsIgnoreCase(s_seen[i].addr)) return;
  if (s_seen_n >= 4) return;
  SeenDev& s = s_seen[s_seen_n++];
  addr.toCharArray(s.addr, sizeof s.addr);
  s.type = type; s.rssi = rssi; s.hid = hid; s.mouse = mouse;
  name.toCharArray(s.name, sizeof s.name);
}

class AdvCb : public BLEAdvertisedDeviceCallbacks {
 public:
  void onResult(BLEAdvertisedDevice d) override {
    if (!d.isConnectable()) return;
    bool is_hid = d.isAdvertisingService(BLEUUID((uint16_t)0x1812));
    bool is_mouse = d.haveAppearance() && d.getAppearance() == 0x03C2;       // 0x03C2 = HID mouse
    String addr = d.getAddress().toString();
    const bool known = s_saved_addr[0] && addr.equalsIgnoreCase(s_saved_addr);
    String nm = d.haveName() ? d.getName() : String("");
    if (is_hid || is_mouse || known) seen_add(addr, d.getAddressType(), d.haveRSSI() ? d.getRSSI() : 0, nm, is_hid, is_mouse);
    if (!is_hid && !is_mouse && !known) return;                               // el guardado se acepta aunque en este paquete no anuncie HID
    // El mismo raton con otra direccion (el Logitech M240 cambia el ultimo byte al reiniciarse): mismo nombre, anuncia HID y mismos 5 primeros bytes
    bool same_dev = false;
    if (!known && !pairing() && s_saved_addr[0] && s_saved_name[0] && nm.length() && nm.equals(s_saved_name) && (is_hid || is_mouse))
      same_dev = strncasecmp(addr.c_str(), s_saved_addr, 14) == 0;            // "d4:8e:0e:86:cc" = 14 caracteres
    if (!known && !same_dev && !pairing()) return;                            // fuera de la ventana de emparejado, solo el guardado
    int score = d.haveRSSI() ? d.getRSSI() : -100;
    if (is_mouse) score += 20;                                                // un raton declarado antes que un HID cualquiera
    if (known) score += 200;                                                  // el que ya se conocia, antes que nada
    else if (same_dev) score += 150;
    if (score > s_cand_score) {
      s_cand_score = score;
      s_cand_ok = true;
      s_cand_addr = d.getAddress();
      s_cand_type = d.getAddressType();
      s_cand_rssi = d.haveRSSI() ? d.getRSSI() : 0;
      String nm = d.haveName() ? d.getName() : String("(sin nombre)");
      nm.toCharArray(s_cand_name, sizeof s_cand_name);
    }
  }
};
static AdvCb s_adv_cb;

// Busca un raton, lo conecta, lo empareja y se suscribe a sus informes. true si todo salio bien.
static bool connect_once() {
  s_cand_ok = false; s_cand_score = -1000; s_seen_n = 0;
  if (!pairing() && !s_saved_addr[0]) { set_state("sin raton guardado (esp ble pair para emparejar uno)"); return false; }
  if (pairing()) set_state("buscando para emparejar (%us)", (unsigned)pair_left_s());
  else set_state("buscando el guardado %s", s_saved_addr);
  BLEScan* sc = BLEDevice::getScan();
  sc->setAdvertisedDeviceCallbacks(&s_adv_cb, false, true);
  sc->setActiveScan(true);
  sc->setInterval(100);
  sc->setWindow(50);
  sc->start(MOUSE_BLE_SCAN_S, false);
  sc->clearResults();
  if (!s_enabled) return false;
  if (!s_cand_ok) {
    if (pairing()) set_state("no hay raton cerca (en modo emparejado?)");
    else set_state("el raton guardado no esta cerca (encendido?; HID vistos: %u)", (unsigned)s_seen_n);
    return false;
  }

  strncpy(s_devname, s_cand_name, sizeof s_devname - 1); s_devname[sizeof s_devname - 1] = 0;
  String a = s_cand_addr.toString();
  a.toCharArray(s_devaddr, sizeof s_devaddr);
  s_rssi = s_cand_rssi;

  if (!s_client) {
    s_client = BLEDevice::createClient();
    if (!s_client) { set_state("sin memoria para el cliente BLE"); return false; }
    s_client->setClientCallbacks(&s_cli_cb);
  }
  set_state("conectando a %s", s_devname);
  if (!s_client->connect(s_cand_addr, s_cand_type, 10000)) { set_state("no conecta (%s)", s_devname); return false; }

  set_state("emparejando");
  if (!s_client->secureConnection()) {
    // Algunos ratones no piden cifrado hasta el primer acceso protegido: se sigue, y si hace falta lo pedira la lectura (readValue reintenta con seguridad)
    Serial.println("BLE raton: secureConnection no completo, se sigue");
  }

  BLERemoteService* hid = s_client->getService(BLEUUID((uint16_t)0x1812));
  if (!hid) { set_state("sin servicio HID"); s_client->disconnect(); return false; }

  std::map<uint16_t, BLERemoteCharacteristic*>* cm = nullptr;
  hid->getCharacteristics(&cm);

  // 1) Descriptor de informes
  Layout lay; bool have_lay = false;
  BLERemoteCharacteristic* rm = hid->getCharacteristic(BLEUUID((uint16_t)0x2A4B));
  if (rm) {
    String m = rm->readValue();
    have_lay = parse_report_map((const uint8_t*)m.c_str(), m.length(), &lay);
    snprintf(s_layout_txt, sizeof s_layout_txt, "mapa %u B: %s", (unsigned)m.length(), have_lay ? "X/Y encontrados" : "sin X/Y");
  } else {
    snprintf(s_layout_txt, sizeof s_layout_txt, "sin mapa de informes");
  }

  int nsub = 0;
  if (have_lay && cm) {
    for (auto& kv : *cm) {
      BLERemoteCharacteristic* ch = kv.second;
      if (!ch->getUUID().equals(BLEUUID((uint16_t)0x2A4D))) continue;     // Report
      if (!ch->canNotify()) continue;
      uint8_t rid = 0, rtype = 1;
      BLERemoteDescriptor* rr = ch->getDescriptor(BLEUUID((uint16_t)0x2908));   // Report Reference: [ID, tipo (1 = entrada)]
      if (rr) { String v = rr->readValue(); if (v.length() >= 2) { rid = (uint8_t)v[0]; rtype = (uint8_t)v[1]; } }
      if (rtype != 1) continue;
      if (lay.has_id && rid != lay.id) continue;
      Layout copy = lay;
      if (ch->subscribe(true, [copy](BLERemoteCharacteristic*, uint8_t* d, size_t n, bool) { on_input(d, n, copy); }, true)) nsub++;
    }
    if (nsub) {
      snprintf(s_mode, sizeof s_mode, "informes");
      char t[96];
      snprintf(t, sizeof t, "%s; X %u/%u b, Y %u/%u b, rueda %s, id %u", s_layout_txt, lay.x.bit, lay.x.size, lay.y.bit, lay.y.size, lay.wheel.ok ? "si" : "no", lay.has_id ? lay.id : 0);
      strncpy(s_layout_txt, t, sizeof s_layout_txt - 1); s_layout_txt[sizeof s_layout_txt - 1] = 0;
    }
  }

  // 2) Si no, el informe "boot"
  if (!nsub) {
    BLERemoteCharacteristic* bm = hid->getCharacteristic(BLEUUID((uint16_t)0x2A33));
    if (bm && bm->canNotify()) {
      BLERemoteCharacteristic* pm = hid->getCharacteristic(BLEUUID((uint16_t)0x2A4E));   // Protocol Mode: 0 = boot
      if (pm) { uint8_t z = 0; pm->writeValue(&z, 1, false); }
      Layout bl = boot_layout();
      if (bm->subscribe(true, [bl](BLERemoteCharacteristic*, uint8_t* d, size_t n, bool) { on_input(d, n, bl); }, true)) {
        nsub++;
        snprintf(s_mode, sizeof s_mode, "boot");
        snprintf(s_layout_txt, sizeof s_layout_txt, "informe boot (botones, dx, dy, rueda de 8 bits)");
      }
    }
  }

  if (!nsub) { set_state("no se pudo suscribir a ningun informe"); s_client->disconnect(); return false; }

  s_connected = true;
  s_conn_count++;
  s_pair_until = 0;                       // emparejado: la ventana se cierra
  nvs_save_addr(s_devaddr);
  if (s_devname[0] && strcmp(s_devname, "(sin nombre)") != 0) nvs_save_name(s_devname);
  set_state("conectado a %s (%s)", s_devname, s_mode);
  return true;
}

static void ble_task(void*) {
  bool inited = false;
  for (;;) {
    if (!s_enabled) {
      if (s_client && s_client->isConnected()) s_client->disconnect();
      s_connected = false;
      snprintf(s_state, sizeof s_state, "parado");
      vTaskDelay(pdMS_TO_TICKS(300));
      continue;
    }
    if (!inited) {
      BLEDevice::init("SD81");
      BLESecurity::setAuthenticationMode(true, false, true);            // bonding, sin MITM, conexiones seguras
      BLESecurity::setCapability(ESP_IO_CAP_NONE);                      // "Just Works": los ratones no tienen pantalla ni teclado
      BLESecurity::setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
      BLESecurity::setRespEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
      inited = true;
    }
    if (s_client && s_client->isConnected() && s_connected) {
      s_rssi = s_client->getRssi();
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }
    if (s_client && s_client->isConnected()) s_client->disconnect();    // a medias de una conexion fallida
    s_connected = false;
    if (!connect_once()) vTaskDelay(pdMS_TO_TICKS(2500));
  }
}

void mouse_ble_begin() {
  nvs_load();
  xTaskCreate(tx_task, "mouse_tx", 6144, nullptr, 5, &s_tx_task);
  xTaskCreate(ble_task, "mouse_ble", 8192, nullptr, 4, nullptr);
  Serial.printf("BLE raton: %s (esp ble on|off en la consola web)\n", s_enabled ? "activo" : "parado");
}

// ------------------------------------------------------------------------------------------------------------------------------------
// Orden "esp ble ..." de la consola web
// ------------------------------------------------------------------------------------------------------------------------------------
static String status_text() {
  String s;
  s += String("BLE raton: ") + (s_enabled ? "ACTIVO" : "parado") + " - " + s_state + "\n";
  if (s_devname[0]) s += String("  dispositivo: ") + s_devname + " " + s_devaddr + (s_connected ? "  RSSI " + String(s_rssi) + " dBm" : "") + "\n";
  if (s_connected) s += String("  modo: ") + s_mode + " (" + s_layout_txt + ")\n";
  s += "  conexiones: " + String(s_conn_count) + "  notificaciones: " + String(s_notif) + "  tramas al STM32: ok " + String(s_tx_ok) + ", fallo " + String(s_tx_fail) + "\n";
  uint32_t n, sum, mx;
  portENTER_CRITICAL(&s_mux); n = s_lat_n; sum = s_lat_sum; mx = s_lat_max; portEXIT_CRITICAL(&s_mux);
  if (n) s += "  latencia notificacion->STM32: media " + String(sum / n) + " us, maxima " + String(mx) + " us (" + String(n) + " tramas)\n";
  if (s_st_valid) s += "  STM32: x=" + String(s_st_x) + " y=" + String(s_st_y) + " botones=" + String(s_st_btn) + "\n";
  if (s_saved_addr[0]) s += String("  guardado: ") + s_saved_addr + (s_saved_name[0] ? String(" (") + s_saved_name + ")" : String("")) + "\n";
  else s += "  guardado: ninguno\n";
  if (!s_connected) {                                   // diagnostico: lo que anuncia lo que hay cerca (si el guardado no sale aqui, no lo oye; si sale con otra direccion, cambia)
    for (uint8_t i = 0; i < s_seen_n; i++)
      s += String("  visto: ") + s_seen[i].addr + " tipo " + s_seen[i].type + " " + s_seen[i].rssi + " dBm \"" + s_seen[i].name + "\"" + (s_seen[i].hid ? " HID" : "") + (s_seen[i].mouse ? " raton" : "") + "\n";
  }
  if (pairing()) s += "  EMPAREJANDO: la ventana se cierra en " + String(pair_left_s()) + " s\n";
  return s;
}

// CMD_BLE: el ESP32 cuenta su estado al STM32 (para que el Z80/BASIC lo pueda leer y la FPGA sepa si el raton esta conectado) y recoge la orden
// pendiente (comando 77 del MCU, ops 3 y 4). El STM32 no puede iniciar tramas, por eso se pregunta desde aqui. Ver WIFI_PROTOCOL.h.
static void ble_stm_sync() {
  static uint32_t last_ms = 0;
  static int last_state = -1;
  uint8_t st = 0;
  if (s_connected)    st |= 0x01;
  if (pairing())      st |= 0x02;
  if (s_enabled)      st |= 0x04;
  if (s_saved_addr[0]) st |= 0x08;
  const uint32_t now = millis();
  if (st == last_state && now - last_ms < 500) return;
  last_ms = now;
  last_state = st;
  uint32_t left = pair_left_s();
  uint8_t req[2] = { st, (uint8_t)(left > 255 ? 255 : left) };
  WifiProtoResp r = wifi_client_request(CMD_BLE, req, sizeof req);
  if (!r.ok || r.len < 3 || r.payload[0] != ST_OK) { last_state = -1; return; }     // no contesto (version vieja del STM32?): reintenta en la proxima
  const uint8_t order = r.payload[1], arg = r.payload[2];
  String cmd;
  switch (order) {
    case 1: cmd = arg ? String("pair ") + arg : String("pair"); break;
    case 2: cmd = "connect"; break;
    case 3: cmd = "forget"; break;
    case 4: cmd = "on"; break;
    case 5: cmd = "off"; break;
    default: return;
  }
  Serial.printf("BLE raton: orden del Z80: esp ble %s\n", cmd.c_str());
  mouse_ble_console(cmd);
  last_state = -1;                                      // el estado habra cambiado: se cuenta enseguida
}

String mouse_ble_console(const String& args) {
  String a = args; a.trim(); a.toLowerCase();
  if (a == "" || a == "status") return status_text();
  if (a == "on") {
    s_enabled = true; nvs_save_enabled();
    if (!s_saved_addr[0]) return "BLE raton activado (se guarda), pero no hay ningun raton guardado: esp ble pair para emparejar uno\n";
    return String("BLE raton activado (se guarda): se conecta al raton guardado ") + s_saved_addr + "\n";
  }
  if (a == "off") { s_enabled = false; nvs_save_enabled(); return "BLE raton parado (se guarda). El raton guardado sigue guardado\n"; }
  if (a == "connect") {
    if (!s_saved_addr[0]) return "no hay ningun raton guardado: esp ble pair para emparejar uno\n";
    s_enabled = true; nvs_save_enabled();
    if (s_connected) return "ya esta conectado\n";
    return String("buscando el raton guardado ") + s_saved_addr + " (enciendelo; mira esp ble)\n";
  }
  if (a == "pair" || a.startsWith("pair ")) {
    int secs = 60;
    if (a.length() > 5) secs = a.substring(5).toInt();
    if (secs < 10) secs = 10;
    if (secs > 300) secs = 300;
    // Un raton nuevo sustituye al guardado: se desconecta y se olvida el anterior (y sus claves) antes de buscar
    if (s_client && s_client->isConnected()) s_client->disconnect();
    s_connected = false;
    nvs_forget_all();
#ifdef HAVE_BLE_STORE
    ble_store_clear();
#endif
    s_devname[0] = 0; s_devaddr[0] = 0;
    s_enabled = true; nvs_save_enabled();
    s_pair_until = millis() + (uint32_t)secs * 1000UL;
    if (!s_pair_until) s_pair_until = 1;
    return String("emparejando durante ") + secs + " s: pon el raton en modo emparejado (suele ser un boton debajo) y mira esp ble. Sustituye al anterior\n";
  }
  if (a == "reset") {
    portENTER_CRITICAL(&s_mux); s_lat_n = 0; s_lat_sum = 0; s_lat_max = 0; portEXIT_CRITICAL(&s_mux);
    s_notif = 0; s_tx_ok = 0; s_tx_fail = 0;
    return "estadisticas a cero\n";
  }
  if (a == "forget") {
    if (s_client && s_client->isConnected()) s_client->disconnect();
    s_connected = false;
    s_pair_until = 0;
    nvs_forget_all();
    s_devname[0] = 0; s_devaddr[0] = 0;
#ifdef HAVE_BLE_STORE
    ble_store_clear();                                  // olvida tambien los emparejados de la pila BLE
    return "olvidado el raton guardado y borrados sus emparejados. esp ble pair para emparejar otro\n";
#else
    return "olvidada la direccion guardada (los emparejados de la pila BLE no se pueden borrar desde aqui)\n";
#endif
  }
  return "esp ble: pair [s], connect, forget, on, off, reset o nada (estado)\n";
}

#endif
