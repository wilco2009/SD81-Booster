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
#include <esp_system.h>
#include <esp_attr.h>
#include <esp_heap_caps.h>
#if __has_include(<esp_core_dump.h>)
#include <esp_core_dump.h>
#endif
#if defined(CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH) && defined(CONFIG_ESP_COREDUMP_DATA_FORMAT_ELF) && defined(__riscv)
#define HAVE_CRASH_SUMMARY 1
#endif
#include "WIFI_CLIENT.h"
#include "KEYMAP_ZX81.h"
#if __has_include("host/ble_store.h")
#include "host/ble_store.h"
#include "host/ble_gap.h"
#include "host/ble_sm.h"
#include "host/ble_hs.h"
#include "host/ble_hs_adv.h"
#include "host/ble_hs_id.h"
#include "host/ble_gatt.h"
#include "os/os_mbuf.h"
#include "host/ble_hs_pvcy.h"
#include "mbedtls/aes.h"
#if __has_include("nimble/hci_common.h")
#include "nimble/hci_common.h"
#endif
#define HAVE_BLE_STORE 1
#endif
#include "SD_LOG.h"      // redefine Serial: lo que salga por Serial se copia tambien a /SYS/ESP32LOG.TXT (si SDLOG=1)

// Los tres huecos. El numero es el "dispositivo" de CMD_BLE y del comando 78 (0 raton, 1 teclado, 2 mando)
enum { DEV_MOUSE = 0, DEV_KBD = 1, DEV_PAD = 2, DEV_N = 3 };

// ------------------------------------------------------------------------------------------------------------------------------------
// Descriptor de informes HID: donde esta cada campo del informe del raton (sin cambios respecto al firmware solo de raton)
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
// Descriptor de informes, recorrido generico (teclado, mando y clasificacion; el raton sigue con su parser de arriba)
// ------------------------------------------------------------------------------------------------------------------------------------
struct HidField {
  uint8_t  rid;            // Report ID (0 = el descriptor no usa IDs)
  uint16_t page;
  uint16_t usage;          // campo variable: su uso; array: 0
  uint16_t umin;           // array: el primer uso del rango (valor del campo = umin + (v - lmin))
  uint16_t bit;            // posicion dentro del informe (sin el byte del ID)
  uint8_t  size;
  int32_t  lmin, lmax;
  bool     var;
};
#define HID_MAX_IDS 8

// Llama a on_field con cada elemento de entrada no constante. app_mask (si no es nulo): bit DEV_x por cada coleccion de aplicacion del
// escritorio generico de raton (0x02), teclado (0x06/0x07) o joystick/mando (0x04/0x05).
template <class F>
static void hid_walk(const uint8_t* d, size_t n, uint8_t* app_mask, F on_field) {
  uint32_t usage_page = 0, rsize = 0, rcount = 0, lmax_u = 0;
  int32_t  lmin = 0, lmax_s = 0;
  uint8_t  rid = 0;
  uint16_t usages[24];
  uint8_t  nus = 0;
  uint32_t umin = 0, umax = 0;
  bool     have_range = false;
  uint8_t  depth = 0;
  uint8_t  ids[HID_MAX_IDS];
  uint16_t pos[HID_MAX_IDS];
  uint8_t  nids = 0;
  if (app_mask) *app_mask = 0;
  auto slot = [&](uint8_t id) -> uint8_t {
    for (uint8_t i = 0; i < nids; i++) if (ids[i] == id) return i;
    if (nids < HID_MAX_IDS) { ids[nids] = id; pos[nids] = 0; return nids++; }
    return HID_MAX_IDS - 1;
  };

  size_t i = 0;
  while (i < n) {
    uint8_t b = d[i++];
    if (b == 0xFE) { if (i >= n) break; i += 2 + d[i]; continue; }
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
        case 2: lmax_s = sv; lmax_u = v; break;
        case 7: rsize = v; break;
        case 8: rid = (uint8_t)v; break;
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
      if (tag == 0xA) {                  // Collection
        if (v == 1 && depth == 0 && usage_page == 0x01 && app_mask && nus) {
          uint16_t u = usages[0];
          if (u == 0x02) *app_mask |= 1 << DEV_MOUSE;
          else if (u == 0x06 || u == 0x07) *app_mask |= 1 << DEV_KBD;
          else if (u == 0x04 || u == 0x05) *app_mask |= 1 << DEV_PAD;
        }
        if (depth < 255) depth++;
      } else if (tag == 0xC) {           // End Collection
        if (depth) depth--;
      } else if (tag == 8) {             // Input
        uint8_t s = slot(rid);
        const bool constant = (v & 1) != 0, var = (v & 2) != 0;
        // Logical Maximum de 16 bits escrito en 2 bytes (65535 = "26 FF FF") sale negativo: se toma sin signo
        const int32_t lmax = (lmax_s < lmin) ? (int32_t)lmax_u : lmax_s;
        for (uint32_t k = 0; k < rcount; k++) {
          if (!constant) {
            HidField f;
            f.rid = rid; f.page = (uint16_t)usage_page; f.bit = pos[s]; f.size = (uint8_t)rsize; f.lmin = lmin; f.lmax = lmax; f.var = var;
            f.usage = 0; f.umin = 0;
            bool call = true;
            if (var) {
              int32_t u = -1;
              if (have_range) { u = (int32_t)umin + (int32_t)k; if ((uint32_t)u > umax) u = (int32_t)umax; }
              else if (nus) u = usages[k < nus ? k : nus - 1];
              if (u < 0) call = false; else f.usage = (uint16_t)u;
            } else {
              f.umin = have_range ? (uint16_t)umin : (nus ? usages[0] : 0);
            }
            if (call) on_field(f);
          }
          pos[s] += (uint16_t)rsize;
        }
      }
      nus = 0; have_range = false;       // los locales se borran tras cada item main
    }
  }
}

static uint8_t report_map_kinds(const uint8_t* d, size_t n) {
  uint8_t m = 0;
  hid_walk(d, n, &m, [](const HidField&) {});
  return m;
}

// --- Teclado ---
struct KArr { uint16_t bit; uint8_t size; uint8_t count; uint16_t umin; int32_t lmin; };   // array de codigos (tipo boot)
struct KBm  { uint16_t bit; uint16_t umin; uint16_t count; };                             // mapa de bits (NKRO): bit k = uso umin + k
struct KbdLayout {
  bool    ok;
  bool    has_id;
  uint8_t id;
  int16_t mod[8];          // bit de cada modificador (usos 0xE0-0xE7), -1 = no esta
  KArr    arr[2];
  uint8_t narr;
  KBm     bm[4];
  uint8_t nbm;
};
static void kbd_clear(KbdLayout& l) { memset(&l, 0, sizeof l); for (uint8_t m = 0; m < 8; m++) l.mod[m] = -1; }

static bool parse_kbd_map(const uint8_t* d, size_t n, KbdLayout* out) {
  // De la memoria dinamica solo mientras se analiza (la RAM del C3 con WiFi + BLE va justa: no se reserva para siempre)
  KbdLayout* L = (KbdLayout*)calloc(HID_MAX_IDS, sizeof(KbdLayout));
  if (!L) return false;
  uint8_t nl = 0;
  auto get = [&](uint8_t id) -> KbdLayout& {
    for (uint8_t i = 0; i < nl; i++) if (L[i].id == id) return L[i];
    uint8_t i = nl < HID_MAX_IDS ? nl++ : HID_MAX_IDS - 1;
    kbd_clear(L[i]); L[i].id = id; L[i].has_id = id != 0;
    return L[i];
  };
  hid_walk(d, n, nullptr, [&](const HidField& f) {
    if (f.page != 0x07 || f.size == 0) return;
    KbdLayout& l = get(f.rid);
    if (f.var) {
      if (f.size != 1) return;
      if (f.usage >= 0xE0 && f.usage <= 0xE7) { l.mod[f.usage - 0xE0] = (int16_t)f.bit; return; }
      KBm* b = l.nbm ? &l.bm[l.nbm - 1] : nullptr;
      if (b && b->bit + b->count == f.bit && b->umin + b->count == f.usage) b->count++;
      else if (l.nbm < 4) { KBm nb = { f.bit, f.usage, 1 }; l.bm[l.nbm++] = nb; }
    } else {
      if (f.size > 16) return;
      KArr* a = l.narr ? &l.arr[l.narr - 1] : nullptr;
      if (a && a->size == f.size && a->umin == f.umin && a->lmin == f.lmin && a->count < 255 && a->bit + (uint16_t)a->size * a->count == f.bit) a->count++;
      else if (l.narr < 2) { KArr na = { f.bit, f.size, 1, f.umin, f.lmin }; l.arr[l.narr++] = na; }
    }
  });
  bool found = false;
  for (uint8_t i = 0; i < nl && !found; i++) {
    if (L[i].narr || L[i].nbm) { *out = L[i]; out->ok = true; found = true; }
  }
  free(L);
  return found;
}

// Informe boot del teclado (0x2A22): byte 0 modificadores, byte 1 reservado, bytes 2-7 seis codigos
static KbdLayout kbd_boot_layout() {
  KbdLayout l; kbd_clear(l);
  l.ok = true;
  for (uint8_t m = 0; m < 8; m++) l.mod[m] = m;
  KArr a = { 16, 8, 6, 0, 0 };
  l.arr[0] = a; l.narr = 1;
  return l;
}

// --- Mando ---
struct PField { bool ok; uint16_t bit; uint8_t size; int32_t lmin, lmax; };
struct AuxBit { uint8_t rid; PField f; uint8_t bit; };                      // boton suelto de otro informe -> bit de CMD_PAD
struct AuxArr { uint8_t rid; PField f; uint8_t n; uint16_t umin; };        // array de codigos de consumo de otro informe
struct PadLayout {
  bool    ok;
  bool    has_id;
  uint8_t id;
  PField  lx, ly, rx, ry, lt, rt, hat;
  PField  dp[4];           // cruceta como botones (pagina 1, usos 0x90-0x93: arriba, abajo, derecha, izquierda), poco frecuente
  PField  btn[16];         // botones HID 1..16
  const uint8_t* map;      // tabla de botones del mando (boton HID - 1 -> bit de CMD_PAD; 0xFF = ninguno)
  char    table[16];
  char    axes[72];
  PField  cb[6];           // botones de la pagina de consumo (0x0C) como bits sueltos...
  uint8_t cbt[6];          // ...y el bit de CMD_PAD de cada uno
  uint8_t ncb;
  PField  ca;              // o como array de codigos (valor = uso)
  uint8_t ca_n;
  uint16_t ca_umin;
  char    cons[40];        // usos de consumo encontrados (para el estado)
  AuxBit  aux[8];          // los botones de consumo de los OTROS informes del mando (el Luna pone alguno fuera del principal)
  uint8_t naux;
  AuxArr  auxa[3];
  uint8_t nauxa;
};
struct PadRaw { uint8_t id; PField ax[6], hat, acc, brk, dp[4], btn[16]; uint8_t nbtn; PField cb[6]; uint8_t cbt[6]; uint16_t cbu[6]; uint8_t ncb;
                PField ca; uint8_t ca_n; uint16_t ca_umin; };

// Botones de la pagina de consumo de algunos mandos (el Luna pone ahi View, el boton Luna y el del microfono): uso -> bit de CMD_PAD
static int8_t consumer_bit(uint16_t u) {
  switch (u) {
    case 0x224: return 8;            // AC Back: SELECT / View
    case 0x223: return 12;           // AC Home: HOME / Guia
    case 0x040: return 9;            // Menu: START
    case 0x0CF: case 0x221: return 13;   // Voice Command / AC Search: microfono (bit 13, libre en CMD_PAD)
    case 0x0B2: return 14;           // Record: "compartir" (Xbox Share), bit 14
    default:    return -1;
  }
}

static bool parse_pad_map(const uint8_t* d, size_t n, PadLayout* out) {
  PadRaw* R = (PadRaw*)calloc(HID_MAX_IDS, sizeof(PadRaw));   // ~2,8 KB solo mientras se analiza
  if (!R) return false;
  uint8_t nr = 0;
  auto get = [&](uint8_t id) -> PadRaw& {
    for (uint8_t i = 0; i < nr; i++) if (R[i].id == id) return R[i];
    uint8_t i = nr < HID_MAX_IDS ? nr++ : HID_MAX_IDS - 1;
    memset(&R[i], 0, sizeof R[i]); R[i].id = id;
    return R[i];
  };
  hid_walk(d, n, nullptr, [&](const HidField& f) {
    if (f.size == 0 || f.size > 31) return;
    if (f.page == 0x0C) {                            // consumo: bits sueltos con uso conocido, o un array de codigos
      PadRaw& r = get(f.rid);
      if (f.var && f.size == 1) {
        const int8_t b = consumer_bit(f.usage);
        if (b >= 0 && r.ncb < 6) { PField p = { true, f.bit, f.size, f.lmin, f.lmax }; r.cb[r.ncb] = p; r.cbt[r.ncb] = (uint8_t)b; r.cbu[r.ncb] = f.usage; r.ncb++; }
      } else if (!f.var && f.size <= 16) {
        if (!r.ca.ok) { PField p = { true, f.bit, f.size, f.lmin, f.lmax }; r.ca = p; r.ca_n = 1; r.ca_umin = f.umin; }
        else if (r.ca.size == f.size && r.ca.bit + (uint16_t)r.ca.size * r.ca_n == f.bit && r.ca_n < 4) r.ca_n++;
      }
      return;
    }
    if (!f.var) return;
    PField p = { true, f.bit, f.size, f.lmin, f.lmax };
    if (f.page == 0x01) {
      if (f.usage >= 0x30 && f.usage <= 0x35) { PadRaw& r = get(f.rid); if (!r.ax[f.usage - 0x30].ok) r.ax[f.usage - 0x30] = p; }
      else if (f.usage == 0x39) { PadRaw& r = get(f.rid); if (!r.hat.ok) r.hat = p; }
      else if (f.usage >= 0x90 && f.usage <= 0x93) { PadRaw& r = get(f.rid); r.dp[f.usage - 0x90] = p; }
    } else if (f.page == 0x09 && f.usage >= 1 && f.usage <= 16) {
      PadRaw& r = get(f.rid); if (!r.btn[f.usage - 1].ok) { r.btn[f.usage - 1] = p; r.nbtn++; }
    } else if (f.page == 0x02 && (f.usage == 0xC4 || f.usage == 0xC5)) {
      PadRaw& r = get(f.rid); if (f.usage == 0xC4) r.acc = p; else r.brk = p;
    }
  });
  int best = -1, best_score = 0;
  for (uint8_t i = 0; i < nr; i++) {
    const PadRaw& r = R[i];
    int sc = r.nbtn + ((r.ax[0].ok && r.ax[1].ok) ? 20 : 0) + (r.hat.ok ? 10 : 0) + (r.dp[0].ok ? 5 : 0);
    if (!(r.ax[0].ok && r.ax[1].ok) && !r.hat.ok && !r.dp[0].ok && !r.nbtn) continue;
    if (sc > best_score) { best_score = sc; best = i; }
  }
  if (best < 0) { free(R); return false; }
  memset(out, 0, sizeof *out);
  char ocons[24] = "";                     // usos de consumo de los otros informes (para el estado)
  for (uint8_t i = 0; i < nr; i++) {
    if ((int)i == best) continue;
    const PadRaw& q = R[i];
    for (uint8_t k = 0; k < q.ncb && out->naux < 8; k++) {
      AuxBit& a = out->aux[out->naux++];
      a.rid = q.id; a.f = q.cb[k]; a.bit = q.cbt[k];
      size_t o = strlen(ocons);
      if (o + 10 < sizeof ocons) snprintf(ocons + o, sizeof ocons - o, " %X(id%u)", (unsigned)q.cbu[k], (unsigned)q.id);
    }
    if (q.ca.ok && out->nauxa < 3) {
      AuxArr& a = out->auxa[out->nauxa++];
      a.rid = q.id; a.f = q.ca; a.n = q.ca_n; a.umin = q.ca_umin;
      size_t o = strlen(ocons);
      if (o + 12 < sizeof ocons) snprintf(ocons + o, sizeof ocons - o, " array(id%u)", (unsigned)q.id);
    }
  }
  const PadRaw r = R[best];                // copia (~350 B en la pila de ble_task) y se suelta el resto
  free(R);
  out->ok = true; out->id = r.id; out->has_id = r.id != 0;
  out->lx = r.ax[0]; out->ly = r.ax[1]; out->hat = r.hat;
  for (uint8_t k = 0; k < 4; k++) out->dp[k] = r.dp[k];
  for (uint8_t k = 0; k < 16; k++) out->btn[k] = r.btn[k];
  out->ncb = r.ncb;
  for (uint8_t k = 0; k < r.ncb; k++) { out->cb[k] = r.cb[k]; out->cbt[k] = r.cbt[k]; }
  out->ca = r.ca; out->ca_n = r.ca_n; out->ca_umin = r.ca_umin;
  {
    size_t o = 0;
    out->cons[0] = 0;
    for (uint8_t k = 0; k < r.ncb && o + 6 < sizeof out->cons; k++) o += snprintf(out->cons + o, sizeof out->cons - o, "%s%X", o ? "," : "", (unsigned)r.cbu[k]);
    if (r.ca.ok && o + 12 < sizeof out->cons) { snprintf(out->cons + o, sizeof out->cons - o, "%sarray x%u", o ? "," : "", (unsigned)r.ca_n); o = strlen(out->cons); }
    if (ocons[0] && o + 2 < sizeof out->cons) snprintf(out->cons + o, sizeof out->cons - o, ";%s", ocons);
  }
  // Stick derecho y gatillos. Gatillos de la pagina 2 (Freno = LT, Acelerador = RT; asi el Xbox BLE) si los hay. Stick derecho: Z/Rz (Android
  // y Xbox BLE) si estan los dos; si no, Rx/Ry. Sin gatillos de la pagina 2, los ejes que sobren hacen de gatillos. Por confirmar con cada mando.
  const char* rs = "-"; const char* tr = "-";
  const bool p2 = r.acc.ok || r.brk.ok;
  if (r.ax[2].ok && r.ax[5].ok) {
    out->rx = r.ax[2]; out->ry = r.ax[5]; rs = "Z/Rz";
    if (!p2 && (r.ax[3].ok || r.ax[4].ok)) { out->lt = r.ax[3]; out->rt = r.ax[4]; tr = "Rx/Ry"; }
  } else if (r.ax[3].ok && r.ax[4].ok) {
    out->rx = r.ax[3]; out->ry = r.ax[4]; rs = "Rx/Ry";
    if (!p2 && (r.ax[2].ok || r.ax[5].ok)) { out->lt = r.ax[2]; out->rt = r.ax[5]; tr = "Z/Rz"; }
  } else if (!p2 && (r.ax[2].ok || r.ax[5].ok)) {
    out->lt = r.ax[2]; out->rt = r.ax[5]; tr = "Z/Rz";
  }
  if (p2) { out->lt = r.brk; out->rt = r.acc; tr = "freno/acelerador"; }
  snprintf(out->axes, sizeof out->axes, "stick izq %s, der %s, gatillos %s, hat %s, %u botones, id %u",
           (r.ax[0].ok && r.ax[1].ok) ? "X/Y" : "-", rs, tr, r.hat.ok ? "si" : (r.dp[0].ok ? "cruceta" : "no"), (unsigned)r.nbtn, (unsigned)r.id);
  return true;
}

// Tablas de botones por mando: boton HID (1..16) -> bit de CMD_PAD (0 A, 1 B, 2 X, 3 Y, 4 LB, 5 RB, 6 LT, 7 RT, 8 SELECT, 9 START, 10 L3,
// 11 R3, 12 HOME). Se elige por el nombre del mando (la primera cuyo texto este contenido en el nombre, sin distinguir mayusculas).
#define NB 0xFF
// Por defecto: la numeracion de los mandos HID de Android (A 1, B 2, X 4, Y 5, L1 7, R1 8, L2 9, R2 10, Select 11, Start 12, Home 13,
// L3 14, R3 15), que siguen tambien el Xbox BLE y muchos genericos
static const uint8_t PAD_MAP_DEFAULT[16] = { 0, 1, NB, 2, 3, NB, 4, 5, 6, 7, 8, 9, 12, 10, 11, NB };
// Xbox Wireless por BLE: 1 A, 2 B, 4 X, 5 Y, 7 LB, 8 RB, 11 View, 12 Menu, 14 L3, 15 R3 (13 = boton Xbox en algunos firmwares: sin confirmar)
static const uint8_t PAD_MAP_XBOX[16]    = { 0, 1, NB, 2, 3, NB, 4, 5, NB, NB, 8, 9, 12, 10, 11, NB };
// Puente propio Arduino/Gamepad_BLE_megadrive ("SD81 MD Pad"): 1 B de la Mega Drive, 2 C, 3 A, 4 Start
static const uint8_t PAD_MAP_MDPAD[16]   = { 0, 1, 2, 9, NB, NB, NB, NB, NB, NB, NB, NB, NB, NB, NB, NB };
struct PadTable { const char* match; const char* label; const uint8_t* map; };
static const PadTable PAD_TABLES[] = {
  { "xbox",        "Xbox",        PAD_MAP_XBOX },
  { "sd81 md pad", "SD81 MD Pad", PAD_MAP_MDPAD },
  // Amazon Luna ("Luna Gamepad"), comprobado en la placa el 26/11/2026: 1 A, 2 B, 4 X, 5 Y, 7 LB, 8 RB, 12 Menu, 14 L3, 15 R3 = la numeracion
  // de Android; gatillos por freno/acelerador, cruceta por hat. Sin comprobar: View (11?) y los botones Luna y microfono (pagina de consumo?)
  { "luna",        "Luna",        PAD_MAP_DEFAULT },
};

static bool contains_nocase(const char* hay, const char* needle) {
  size_t nh = strlen(hay), nn = strlen(needle);
  for (size_t i = 0; i + nn <= nh; i++) if (strncasecmp(hay + i, needle, nn) == 0) return true;
  return false;
}
static void pad_pick_table(const char* name, PadLayout* L) {
  for (const PadTable& t : PAD_TABLES) {
    if (contains_nocase(name, t.match)) { L->map = t.map; strncpy(L->table, t.label, sizeof L->table - 1); return; }
  }
  L->map = PAD_MAP_DEFAULT; strncpy(L->table, "por defecto", sizeof L->table - 1);
}

// ------------------------------------------------------------------------------------------------------------------------------------
// Los huecos
// ------------------------------------------------------------------------------------------------------------------------------------
struct Slot {
  const char* tag;          // para los mensajes: "raton", "teclado", "mando"
  const char* word;         // para la consola: "mouse", "kbd", "pad"
  const char* nvs;          // espacio de la NVS (el del raton es el de siempre: no se pierde el emparejado)
  volatile bool enabled;
  volatile bool connected;
  char     state[64];
  char     devname[40];
  char     devaddr[24];
  int      rssi;
  char     mode[16];
  char     layout_txt[160];
  char     saved_addr[24];
  char     saved_name[40];  // sirve para reconocerlo si cambia de direccion al reiniciarse (el M240 lo hace)
  uint8_t  ida[7];          // direccion de identidad del vinculo (tipo + 6 bytes, orden de NimBLE): para olvidar SOLO su vinculo
  bool     ida_ok;
  volatile uint32_t pair_until;   // ventana de emparejado: millis() en que se cierra; 0 = cerrada
  uint32_t next_try;
  BLEClient* client;
  uint32_t conn_count;
  uint32_t notif;
  uint32_t tx_ok, tx_fail;  // teclado y mando (el raton lleva sus propios contadores)
  int32_t  heap_used;       // memoria que se llevo la ultima conexion (libre antes - libre despues), para el presupuesto de RAM
  char     last_pair[112];  // como acabo el ultimo emparejado (el estado se sobrescribe enseguida con "buscando...")
  // el mejor candidato de la busqueda en curso (lo rellena el callback del escaneo, en la tarea del host BLE)
  bool       cand_ok;
  BLEAddress cand_addr;
  uint8_t    cand_type;
  int        cand_score;
  char       cand_name[40];
  int        cand_rssi;
  uint16_t   cand_app;
};
static Slot SL[DEV_N];

static void slots_init() {
  static const char* tags[DEV_N]  = { "raton", "teclado", "mando" };
  static const char* words[DEV_N] = { "mouse", "kbd", "pad" };
  static const char* nvss[DEV_N]  = { "sd81mouse", "sd81kbd", "sd81pad" };
  for (uint8_t s = 0; s < DEV_N; s++) {
    SL[s].tag = tags[s]; SL[s].word = words[s]; SL[s].nvs = nvss[s];
    SL[s].enabled = false; SL[s].connected = false;
    strcpy(SL[s].state, "parado"); strcpy(SL[s].mode, "-");
    SL[s].devname[0] = SL[s].devaddr[0] = SL[s].layout_txt[0] = SL[s].saved_addr[0] = SL[s].saved_name[0] = 0;
    SL[s].rssi = 0; SL[s].ida_ok = false; SL[s].pair_until = 0; SL[s].next_try = 0; SL[s].client = nullptr;
    SL[s].conn_count = SL[s].notif = SL[s].tx_ok = SL[s].tx_fail = 0;
    SL[s].cand_ok = false; SL[s].cand_score = -1000; SL[s].last_pair[0] = 0; SL[s].heap_used = 0;
  }
}

static bool pairing(uint8_t s) { uint32_t u = SL[s].pair_until; return u && (int32_t)(millis() - u) < 0; }
static uint32_t pair_left_s(uint8_t s) { return pairing(s) ? (SL[s].pair_until - millis() + 999) / 1000 : 0; }

// Historial de estados de los tres dispositivos en memoria RTC que NO se borra al reiniciar (fallo, perro guardian, caida de tension): si el
// ESP32 se reinicia en mitad de una conexion, "esp ble kbd" ensena lo ultimo que hizo antes. Se pierde al quitar la alimentacion.
#define BLE_PH_N     40
#define BLE_PH_MAGIC 0x53443833u          // cambia si cambia BlePh: asi no se lee basura de un firmware anterior
struct BlePhEntry { uint32_t boot; uint32_t ms; uint16_t heap_kb, min_kb; uint8_t dev; char txt[59]; };   // dev 3 = memoria (para todos)
struct BlePh { uint32_t magic; uint32_t boot; uint32_t n; BlePhEntry e[BLE_PH_N]; };
static RTC_NOINIT_ATTR BlePh s_ph;
static portMUX_TYPE s_hist_mux = portMUX_INITIALIZER_UNLOCKED;
static void ph_mem(const char* why, uint32_t free_b) {
  const uint16_t hk = (uint16_t)(free_b / 1024), mk = (uint16_t)(heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT) / 1024);
  portENTER_CRITICAL(&s_hist_mux);
  BlePhEntry& e = s_ph.e[s_ph.n % BLE_PH_N];
  e.boot = s_ph.boot; e.ms = millis(); e.dev = 3; e.heap_kb = hk; e.min_kb = mk;
  strncpy(e.txt, why, sizeof e.txt - 1); e.txt[sizeof e.txt - 1] = 0;
  s_ph.n++;
  portEXIT_CRITICAL(&s_hist_mux);
}
static void ph_memf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
static void ph_memf(const char* fmt, ...) {
  char t[64];
  va_list ap; va_start(ap, fmt);
  vsnprintf(t, sizeof t, fmt, ap);
  va_end(ap);
  ph_mem(t, heap_caps_get_free_size(MALLOC_CAP_8BIT));
}
static void ph_begin() {
  if (s_ph.magic != BLE_PH_MAGIC || s_ph.n > 100000000u) { memset(&s_ph, 0, sizeof s_ph); s_ph.magic = BLE_PH_MAGIC; }
  s_ph.boot++;
}
static const char* reset_text(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON:   return "encendido";
    case ESP_RST_EXT:       return "patilla de reset";
    case ESP_RST_SW:        return "reinicio por programa (actualizacion, esp restart...)";
    case ESP_RST_PANIC:     return "FALLO DEL PROGRAMA (excepcion o assert)";
    case ESP_RST_INT_WDT:   return "PERRO GUARDIAN de interrupciones";
    case ESP_RST_TASK_WDT:  return "PERRO GUARDIAN de tareas";
    case ESP_RST_WDT:       return "PERRO GUARDIAN";
    case ESP_RST_DEEPSLEEP: return "salida de sueno profundo";
    case ESP_RST_BROWNOUT:  return "CAIDA DE TENSION (brownout)";
    case ESP_RST_SDIO:      return "SDIO";
    default:                return "desconocido";
  }
}
static void set_state(uint8_t s, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
static void set_state(uint8_t s, const char* fmt, ...) {
  char t[sizeof SL[0].state];
  va_list ap; va_start(ap, fmt);
  vsnprintf(t, sizeof t, fmt, ap);
  va_end(ap);
  if (strcmp(t, SL[s].state) == 0) return;            // no repetir en el log el mismo estado cada pocos segundos
  memcpy(SL[s].state, t, sizeof t);
  const uint16_t hk = (uint16_t)(heap_caps_get_free_size(MALLOC_CAP_8BIT) / 1024), mk = (uint16_t)(heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT) / 1024);
  portENTER_CRITICAL(&s_hist_mux);                    // lo llaman la tarea BLE y la del host (desconexion, clave)
  BlePhEntry& e = s_ph.e[s_ph.n % BLE_PH_N];
  e.boot = s_ph.boot; e.ms = millis(); e.dev = s; e.heap_kb = hk; e.min_kb = mk;
  strncpy(e.txt, t, sizeof e.txt - 1); e.txt[sizeof e.txt - 1] = 0;
  s_ph.n++;
  portEXIT_CRITICAL(&s_hist_mux);
  Serial.printf("BLE %s: %s\n", SL[s].tag, SL[s].state);
}

// ------------------------------------------------------------------------------------------------------------------------------------
// Raton: estado compartido; lo escribe la tarea BLE (notificaciones) y lo recoge la tarea que manda al STM32
// ------------------------------------------------------------------------------------------------------------------------------------
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static int32_t  s_acc_dx = 0, s_acc_dy = 0, s_acc_wheel = 0;
static uint8_t  s_buttons = 0;
static bool     s_dirty = false;
static uint32_t s_first_us = 0;             // micros() del evento sin enviar mas antiguo (para medir la latencia)
static TaskHandle_t s_tx_task = nullptr;

static uint32_t s_notif = 0, s_tx_ok = 0, s_tx_fail = 0;
static uint32_t s_lat_n = 0, s_lat_sum = 0, s_lat_max = 0;      // us, de la notificacion BLE a la respuesta del STM32
static uint16_t s_st_x = 0, s_st_y = 0; static uint8_t s_st_btn = 0; static bool s_st_valid = false;

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
// Teclado y mando: lo que llega del BLE (tarea del host) pasa por aqui a la tarea hid_task, que es la unica que toca KEYMAP_ZX81
// ------------------------------------------------------------------------------------------------------------------------------------
struct KbdEvt { uint32_t t; uint8_t mods; uint8_t n; uint8_t keys[KM_MAX_KEYS]; };
static QueueHandle_t s_kbd_q = nullptr;
static TaskHandle_t  s_hid_task = nullptr;
static volatile int8_t  s_kbd_req_mode = -1;      // orden pendiente para keymap_set_mode (de la consola o de CMD_BLE)
static volatile int8_t  s_kbd_req_layout = -1;    // idem keymap_set_layout
static volatile uint8_t s_kbd_mode = KM_MODE_TEXT; // copia de keymap_mode() para el estado
static uint8_t s_kbd_layout = KM_LAYOUT_ES;
static uint8_t s_kbd_rows[8];                      // la ultima matriz mandada (para el estado)
static uint32_t s_kbd_reports = 0, s_kbd_phantom = 0, s_kbd_qfull = 0;   // solo estadisticas

static void kbd_on_input(const uint8_t* d, size_t n, const KbdLayout& L) {
  KbdEvt e; e.t = millis(); e.mods = 0; e.n = 0;
  for (uint8_t m = 0; m < 8; m++) if (L.mod[m] >= 0 && get_bits(d, n, (uint16_t)L.mod[m], 1, false)) e.mods |= (uint8_t)(1u << m);
  auto add = [&](int32_t u) -> bool {          // false = informe fantasma (ErrorRollOver): se ignora entero
    if (u == 0x01) return false;
    if (u < 0x04 || u > 0xFF) return true;     // 0 nada, 2-3 errores
    if (u >= 0xE0 && u <= 0xE7) { e.mods |= (uint8_t)(1u << (u - 0xE0)); return true; }
    for (uint8_t i = 0; i < e.n; i++) if (e.keys[i] == (uint8_t)u) return true;
    if (e.n < KM_MAX_KEYS) e.keys[e.n++] = (uint8_t)u;
    return true;
  };
  for (uint8_t a = 0; a < L.narr; a++) {
    const KArr& A = L.arr[a];
    for (uint8_t k = 0; k < A.count; k++) {
      int32_t v = get_bits(d, n, (uint16_t)(A.bit + (uint16_t)A.size * k), A.size, false);
      // casilla vacia: uso 0, que add() ignora
      if (!add((int32_t)A.umin + (v - A.lmin))) { s_kbd_phantom++; return; }
    }
  }
  for (uint8_t b = 0; b < L.nbm; b++) {
    const KBm& B = L.bm[b];
    for (uint16_t k = 0; k < B.count; k++) {
      if (get_bits(d, n, (uint16_t)(B.bit + k), 1, false) && !add((int32_t)B.umin + k)) { s_kbd_phantom++; return; }
    }
  }
  s_kbd_reports++;
  SL[DEV_KBD].notif++;
  if (s_kbd_q) {
    if (xQueueSend(s_kbd_q, &e, 0) != pdTRUE) {  // llena: se tira el mas viejo (el ultimo informe es el que manda)
      KbdEvt old;
      xQueueReceive(s_kbd_q, &old, 0);
      xQueueSend(s_kbd_q, &e, 0);
      s_kbd_qfull++;
    }
  }
  if (s_hid_task) xTaskNotifyGive(s_hid_task);
}

// Mando: el estado ya normalizado, en el orden de CMD_PAD (sin el byte de flags)
static uint8_t s_pad_now[9];
static bool    s_pad_dirty = false;

static int8_t pad_axis(const uint8_t* d, size_t n, const PField& f) {
  if (!f.ok) return 0;
  const bool sign = f.lmin < 0;
  int64_t v = get_bits(d, n, f.bit, f.size, sign);
  int64_t lo = f.lmin, hi = f.lmax;
  if (hi <= lo) {                                        // sin limites en el descriptor: los del tamano del campo
    if (sign) { hi = ((int64_t)1 << (f.size - 1)) - 1; lo = -hi - 1; }
    else      { lo = 0; hi = ((int64_t)1 << f.size) - 1; }
  }
  int64_t r = ((2 * v - (lo + hi)) * 127) / (hi - lo);
  return (int8_t)(r < -127 ? -127 : (r > 127 ? 127 : r));
}
static uint8_t pad_trigger(const uint8_t* d, size_t n, const PField& f) {
  if (!f.ok) return 0;
  const bool sign = f.lmin < 0;
  int64_t v = get_bits(d, n, f.bit, f.size, sign);
  int64_t lo = f.lmin, hi = f.lmax;
  if (hi <= lo) { lo = 0; hi = ((int64_t)1 << f.size) - 1; }
  int64_t r = ((v - lo) * 255) / (hi - lo);
  return (uint8_t)(r < 0 ? 0 : (r > 255 ? 255 : r));
}
static uint8_t pad_hat(const uint8_t* d, size_t n, const PField& f) {
  static const uint8_t HAT8[8] = { 0x1, 0x1 | 0x8, 0x8, 0x2 | 0x8, 0x2, 0x2 | 0x4, 0x4, 0x1 | 0x4 };   // N NE E SE S SO O NO
  static const uint8_t HAT4[4] = { 0x1, 0x8, 0x2, 0x4 };                                                // N E S O
  if (!f.ok) return 0;
  int32_t v = get_bits(d, n, f.bit, f.size, false) - f.lmin;      // fuera del rango = suelto ("null state")
  const int32_t span = f.lmax - f.lmin;
  if (span == 7 && v >= 0 && v <= 7) return HAT8[v];
  if (span == 3 && v >= 0 && v <= 3) return HAT4[v];
  return 0;
}

static uint8_t s_pad_last[24];                    // el ultimo informe del mando, en bruto (para el estado)
static uint8_t s_pad_last_n = 0;
static uint16_t s_pad_main_btn = 0;               // botones del informe principal
static uint8_t  s_pad_aux_rid[4];                 // y los de cada informe secundario (por Report ID)
static uint16_t s_pad_aux_btn[4];
static uint8_t  s_pad_aux_n = 0;
struct PadRep { uint8_t rid; uint8_t n; uint16_t count; uint8_t d[16]; };   // ultimo contenido de cada informe (diagnostico, esp ble pad)
static PadRep   s_pad_rep[6];
static uint8_t  s_pad_rep_n = 0;
static uint16_t pad_aux_or() { uint16_t b = 0; for (uint8_t i = 0; i < s_pad_aux_n; i++) b |= s_pad_aux_btn[i]; return b; }
static void pad_rep_note(uint8_t rid, const uint8_t* d, size_t n) {   // (con s_mux)
  PadRep* e = nullptr;
  for (uint8_t i = 0; i < s_pad_rep_n; i++) if (s_pad_rep[i].rid == rid) { e = &s_pad_rep[i]; break; }
  if (!e) { if (s_pad_rep_n >= 6) return; e = &s_pad_rep[s_pad_rep_n++]; e->rid = rid; e->count = 0; }
  e->n = (uint8_t)(n < sizeof e->d ? n : sizeof e->d);
  memcpy(e->d, d, e->n);
  if (e->count < 65535) e->count++;
}
static void pad_reset_aux() { portENTER_CRITICAL(&s_mux); s_pad_main_btn = 0; s_pad_aux_n = 0; s_pad_rep_n = 0; portEXIT_CRITICAL(&s_mux); }
static volatile uint16_t s_pad_raw = 0;            // botones HID sin traducir (bit k = boton k+1): para hacer la tabla de cada mando

static void pad_on_input(const uint8_t* d, size_t n, const PadLayout& L) {
  uint16_t btn = 0, raw = 0;
  for (uint8_t k = 0; k < 16; k++) {
    if (!L.btn[k].ok || !get_bits(d, n, L.btn[k].bit, L.btn[k].size, false)) continue;
    raw |= (uint16_t)(1u << k);
    uint8_t t = L.map ? L.map[k] : k;
    if (t < 16) btn |= (uint16_t)(1u << t);
  }
  s_pad_raw = raw;
  for (uint8_t k = 0; k < L.ncb; k++) if (get_bits(d, n, L.cb[k].bit, 1, false)) btn |= (uint16_t)(1u << L.cbt[k]);
  for (uint8_t k = 0; L.ca.ok && k < L.ca_n; k++) {
    const int32_t v = get_bits(d, n, (uint16_t)(L.ca.bit + (uint16_t)L.ca.size * k), L.ca.size, false);
    if (v == 0 || v < L.ca.lmin) continue;           // casilla vacia
    const int8_t b = consumer_bit((uint16_t)(L.ca_umin + (v - L.ca.lmin)));
    if (b >= 0) btn |= (uint16_t)(1u << b);
  }
  portENTER_CRITICAL(&s_mux);
  s_pad_last_n = (uint8_t)(n < sizeof s_pad_last ? n : sizeof s_pad_last);
  memcpy(s_pad_last, d, s_pad_last_n);
  portEXIT_CRITICAL(&s_mux);
  uint8_t dp = pad_hat(d, n, L.hat);
  static const uint8_t DPB[4] = { 0x1, 0x2, 0x8, 0x4 };     // usos 0x90-0x93: arriba, abajo, derecha, izquierda
  for (uint8_t k = 0; k < 4; k++) if (L.dp[k].ok && get_bits(d, n, L.dp[k].bit, L.dp[k].size, false)) dp |= DPB[k];
  const uint8_t lt = pad_trigger(d, n, L.lt), rt = pad_trigger(d, n, L.rt);
  if (lt > 100) btn |= 1u << 6;                             // gatillos analogicos tambien como botones digitales
  if (rt > 100) btn |= 1u << 7;
  uint8_t p[9];
  p[2] = dp;
  p[3] = (uint8_t)pad_axis(d, n, L.lx); p[4] = (uint8_t)pad_axis(d, n, L.ly);
  p[5] = (uint8_t)pad_axis(d, n, L.rx); p[6] = (uint8_t)pad_axis(d, n, L.ry);
  p[7] = lt; p[8] = rt;
  bool changed;
  portENTER_CRITICAL(&s_mux);
  s_pad_main_btn = btn;
  btn |= pad_aux_or();                                      // mas los botones de los informes secundarios
  p[0] = (uint8_t)btn; p[1] = (uint8_t)(btn >> 8);
  pad_rep_note(L.id, d, n);
  changed = memcmp(p, s_pad_now, sizeof p) != 0;
  if (changed) { memcpy(s_pad_now, p, sizeof p); s_pad_dirty = true; }
  SL[DEV_PAD].notif++;
  portEXIT_CRITICAL(&s_mux);
  if (changed && s_hid_task) xTaskNotifyGive(s_hid_task);
}

// Un informe secundario del mando (otro Report ID): sus botones de consumo se suman a los del principal
static void pad_on_aux(uint8_t rid, const uint8_t* d, size_t n, const PadLayout& L) {
  uint16_t b = 0;
  for (uint8_t k = 0; k < L.naux; k++)
    if (L.aux[k].rid == rid && get_bits(d, n, L.aux[k].f.bit, 1, false)) b |= (uint16_t)(1u << L.aux[k].bit);
  for (uint8_t k = 0; k < L.nauxa; k++) {
    const AuxArr& a = L.auxa[k];
    if (a.rid != rid) continue;
    for (uint8_t j = 0; j < a.n; j++) {
      const int32_t v = get_bits(d, n, (uint16_t)(a.f.bit + (uint16_t)a.f.size * j), a.f.size, false);
      if (v == 0 || v < a.f.lmin) continue;
      const int8_t t = consumer_bit((uint16_t)(a.umin + (v - a.f.lmin)));
      if (t >= 0) b |= (uint16_t)(1u << t);
    }
  }
  bool changed;
  portENTER_CRITICAL(&s_mux);
  pad_rep_note(rid, d, n);
  uint8_t i = 0;
  while (i < s_pad_aux_n && s_pad_aux_rid[i] != rid) i++;
  if (i == s_pad_aux_n && i < 4) { s_pad_aux_rid[i] = rid; s_pad_aux_btn[i] = 0; s_pad_aux_n++; }
  if (i < 4) s_pad_aux_btn[i] = b;
  const uint16_t btn = (uint16_t)(s_pad_main_btn | pad_aux_or() | ((L.lt.ok && s_pad_now[7] > 100) ? 0x40 : 0) | ((L.rt.ok && s_pad_now[8] > 100) ? 0x80 : 0));
  changed = (uint8_t)btn != s_pad_now[0] || (uint8_t)(btn >> 8) != s_pad_now[1];
  if (changed) { s_pad_now[0] = (uint8_t)btn; s_pad_now[1] = (uint8_t)(btn >> 8); s_pad_dirty = true; }
  SL[DEV_PAD].notif++;
  portEXIT_CRITICAL(&s_mux);
  if (changed && s_hid_task) xTaskNotifyGive(s_hid_task);
}

// ------------------------------------------------------------------------------------------------------------------------------------
// Tarea que manda el raton al STM32 (prioridad 5, por encima de loop()); tambien hace el CMD_BLE
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
// Emparejado: clave del teclado y plazo
// ------------------------------------------------------------------------------------------------------------------------------------
static volatile uint32_t s_passkey = 0;           // la clave que hay que teclear en el teclado (0 = ninguna)
static volatile int8_t   s_secure_slot = -1;      // hueco que esta emparejando ahora (secureConnection en curso)
static volatile uint32_t s_secure_deadline = 0;
static volatile uint32_t s_passkey_ms = 0;        // millis() en que se enseno la clave (0 = no se ha pedido)
#define BLE_SMP_TIMEOUT_S 30                       // plazo del protocolo de emparejado: la clave hay que teclearla antes

#ifdef HAVE_BLE_STORE
// Lo que dice la pila de la conexion que se esta emparejando: resultado del cifrado y motivo de la desconexion (para explicar los fallos)
static struct ble_gap_event_listener s_gap_listener;
static volatile uint16_t s_sec_conn = 0xFFFF;
static volatile int      s_sec_enc_status = -1;   // -1 = no ha llegado; 0 = cifrado; otro = codigo de error de NimBLE
static volatile int      s_sec_disc_reason = -1;
static volatile int      s_last_disc_reason = -1;   // motivo de la ultima desconexion (0x213 la corto el otro, 0x208 se perdio la senal, 0x216 la cortamos)

// Informes del teclado y del mando suscritos a mano (subscribe_raw): conexion y handle del valor -> dispositivo. Los reparte gap_listener.
struct NotiSub { uint16_t conn; uint16_t handle; uint8_t dev; uint8_t rid; };
#define NSUB_MAX 8
static NotiSub s_nsub[NSUB_MAX];
static uint8_t s_nsub_n = 0;
static portMUX_TYPE s_nsub_mux = portMUX_INITIALIZER_UNLOCKED;
static Layout    s_mouse_lay;                    // formato de los informes del raton conectado
static KbdLayout s_kbd_lay;                      // formato de los informes del teclado conectado
static PadLayout s_pad_lay;                      // y del mando
static void nsub_add(uint16_t conn, uint16_t handle, uint8_t dev, uint8_t rid) {
  portENTER_CRITICAL(&s_nsub_mux);
  if (s_nsub_n < NSUB_MAX) { s_nsub[s_nsub_n].conn = conn; s_nsub[s_nsub_n].handle = handle; s_nsub[s_nsub_n].dev = dev; s_nsub[s_nsub_n].rid = rid; s_nsub_n++; }
  portEXIT_CRITICAL(&s_nsub_mux);
}
static void nsub_clear(uint8_t dev) {
  portENTER_CRITICAL(&s_nsub_mux);
  uint8_t j = 0;
  for (uint8_t i = 0; i < s_nsub_n; i++) if (s_nsub[i].dev != dev) s_nsub[j++] = s_nsub[i];
  s_nsub_n = j;
  portEXIT_CRITICAL(&s_nsub_mux);
}

static int gap_listener(struct ble_gap_event* ev, void*) {
  if (ev->type == BLE_GAP_EVENT_NOTIFY_RX) {
    if (!ev->notify_rx.om) return 0;
    int dev = -1;
    uint8_t rid = 0;
    portENTER_CRITICAL(&s_nsub_mux);
    for (uint8_t i = 0; i < s_nsub_n; i++)
      if (s_nsub[i].conn == ev->notify_rx.conn_handle && s_nsub[i].handle == ev->notify_rx.attr_handle) { dev = s_nsub[i].dev; rid = s_nsub[i].rid; break; }
    portEXIT_CRITICAL(&s_nsub_mux);
    if (dev < 0) return 0;                       // no es de ningun informe suscrito
    uint8_t buf[64];
    uint16_t n = OS_MBUF_PKTLEN(ev->notify_rx.om);
    if (n > sizeof buf) n = sizeof buf;
    os_mbuf_copydata(ev->notify_rx.om, 0, n, buf);
    if (dev == DEV_MOUSE) on_input(buf, n, s_mouse_lay);
    else if (dev == DEV_KBD) kbd_on_input(buf, n, s_kbd_lay);
    else if (dev == DEV_PAD) {
      if (!s_pad_lay.has_id || rid == s_pad_lay.id) pad_on_input(buf, n, s_pad_lay);
      else pad_on_aux(rid, buf, n, s_pad_lay);
    }
    return 0;
  }
  if (ev->type == BLE_GAP_EVENT_ENC_CHANGE && ev->enc_change.conn_handle == s_sec_conn) s_sec_enc_status = ev->enc_change.status;
  else if (ev->type == BLE_GAP_EVENT_DISCONNECT) {
    s_last_disc_reason = ev->disconnect.reason;   // para el historial ("desconectado (motivo ...)")
    if (ev->disconnect.conn.conn_handle == s_sec_conn) s_sec_disc_reason = ev->disconnect.reason;
  }
  return 0;
}
#endif

class SecCb : public BLESecurityCallbacks {
 public:
  uint32_t onPassKeyRequest() override { return 0; }
  void onPassKeyNotify(uint32_t k) override {
    s_passkey = k;
    s_passkey_ms = millis();
    int8_t s = s_secure_slot;
    set_state(s >= 0 ? (uint8_t)s : DEV_KBD, "CLAVE %06lu: tecleala en el teclado y pulsa Enter (%u s)", (unsigned long)k, (unsigned)BLE_SMP_TIMEOUT_S);
  }
  bool onSecurityRequest() override { return true; }
  bool onConfirmPIN(uint32_t) override { return false; }     // comparacion numerica: no la usamos (somos DisplayOnly)
  bool onAuthorizationRequest(uint16_t, uint16_t, bool) override { return true; }
#if defined(CONFIG_NIMBLE_ENABLED)
  void onAuthenticationComplete(ble_gap_conn_desc*) override {}
#elif defined(CONFIG_BLUEDROID_ENABLED)
  void onAuthenticationComplete(esp_ble_auth_cmpl_t) override {}
#endif
};
static SecCb s_sec_cb;

// secureConnection() espera sin plazo: si el emparejado se queda colgado (clave no tecleada, cifrado que no llega), se corta la conexion y la
// espera termina. Lo vigila hid_task.
static void secure_watchdog(uint32_t now) {
  int8_t s = s_secure_slot;
  if (s < 0 || (int32_t)(now - s_secure_deadline) < 0) return;
  s_secure_deadline = now + 5000;                 // no repetir enseguida
  Serial.printf("BLE %s: el emparejado no termina; se corta la conexion\n", SL[s].tag);
  if (SL[s].client && SL[s].client->isConnected()) SL[s].client->disconnect();
}

// ------------------------------------------------------------------------------------------------------------------------------------
// NVS (por hueco; el raton conserva su espacio y sus claves de siempre: "sd81mouse", "ble", "addr", "name")
// ------------------------------------------------------------------------------------------------------------------------------------
static void nvs_load(uint8_t s) {
  Preferences p;
  if (p.begin(SL[s].nvs, true)) {
    SL[s].enabled = p.getBool("ble", false);
    String a = p.getString("addr", "");
    a.toCharArray(SL[s].saved_addr, sizeof SL[s].saved_addr);
    String n = p.getString("name", "");
    n.toCharArray(SL[s].saved_name, sizeof SL[s].saved_name);
    SL[s].ida_ok = p.isKey("ida") && p.getBytes("ida", SL[s].ida, sizeof SL[s].ida) == sizeof SL[s].ida;
    if (s == DEV_KBD) s_kbd_layout = p.getUChar("lay", KM_LAYOUT_ES);
    p.end();
  }
}
static void nvs_save_name(uint8_t s, const char* n) {
  strncpy(SL[s].saved_name, n, sizeof SL[s].saved_name - 1); SL[s].saved_name[sizeof SL[s].saved_name - 1] = 0;
  Preferences p; if (p.begin(SL[s].nvs, false)) { p.putString("name", n); p.end(); }
}
static void nvs_forget_all(uint8_t s) {
  SL[s].saved_addr[0] = 0; SL[s].saved_name[0] = 0; SL[s].ida_ok = false;
  Preferences p; if (p.begin(SL[s].nvs, false)) { p.remove("addr"); p.remove("name"); p.remove("ida"); p.end(); }
}
static void nvs_save_enabled(uint8_t s) { Preferences p; if (p.begin(SL[s].nvs, false)) { p.putBool("ble", SL[s].enabled); p.end(); } }
static void nvs_save_addr(uint8_t s, const char* a) {
  strncpy(SL[s].saved_addr, a, sizeof SL[s].saved_addr - 1); SL[s].saved_addr[sizeof SL[s].saved_addr - 1] = 0;
  Preferences p; if (p.begin(SL[s].nvs, false)) { p.putString("addr", a); p.end(); }
}
static void nvs_save_ida(uint8_t s) { Preferences p; if (p.begin(SL[s].nvs, false)) { p.putBytes("ida", SL[s].ida, sizeof SL[s].ida); p.end(); } }
static void nvs_save_layout() { Preferences p; if (p.begin(SL[DEV_KBD].nvs, false)) { p.putUChar("lay", s_kbd_layout); p.end(); } }

// ------------------------------------------------------------------------------------------------------------------------------------
// Vinculos de la pila BLE: olvidar solo los de un hueco (ble_store_clear() borraria los de todos)
// ------------------------------------------------------------------------------------------------------------------------------------
static bool parse_addr(const char* str, uint8_t out[6]) {        // "aa:bb:cc:dd:ee:ff" -> out[5] = aa ... out[0] = ff (orden de NimBLE)
  unsigned v[6];
  if (!str || sscanf(str, "%x:%x:%x:%x:%x:%x", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) != 6) return false;
  for (uint8_t i = 0; i < 6; i++) out[5 - i] = (uint8_t)v[i];
  return true;
}

#ifdef HAVE_BLE_STORE
static bool slot_owns(uint8_t s, const uint8_t val[6]) {
  uint8_t b[6];
  if (SL[s].saved_addr[0] && parse_addr(SL[s].saved_addr, b) && memcmp(b, val, 6) == 0) return true;
  if (SL[s].ida_ok && memcmp(SL[s].ida + 1, val, 6) == 0) return true;
  if (SL[s].devaddr[0] && parse_addr(SL[s].devaddr, b) && memcmp(b, val, 6) == 0) return true;
  return false;
}
// Borra los vinculos guardados de la pila. mode 0: solo los del hueco s. mode 1: todos los que no sean de OTRO hueco (los del hueco s y los
// huerfanos: un raton que cambio de direccion, pruebas antiguas...), para que al emparejar uno nuevo no falte sitio (son 3 como mucho y, si
// se llena, la pila borraria el mas antiguo, que podria ser el de otro hueco). Devuelve cuantos ha borrado.
static int bonds_delete(uint8_t s, uint8_t mode) {
  ble_addr_t peers[8];
  int np = 0;
  if (ble_store_util_bonded_peers(peers, &np, 8) != 0) return 0;
  int del = 0;
  for (int i = 0; i < np; i++) {
    bool other = false;
    for (uint8_t o = 0; o < DEV_N; o++) if (o != s && slot_owns(o, peers[i].val)) other = true;
    if (other) continue;
    if (mode == 0 && !slot_owns(s, peers[i].val)) continue;
    if (ble_store_util_delete_peer(&peers[i]) == 0) del++;
  }
  return del;
}
static int bonds_count() { ble_addr_t peers[8]; int np = 0; return ble_store_util_bonded_peers(peers, &np, 8) == 0 ? np : -1; }
// Borra los vinculos con esos 6 bytes de direccion (sea cual sea el tipo)
static int bonds_delete_val(const uint8_t val[6]) {
  ble_addr_t peers[8];
  int np = 0, del = 0;
  if (ble_store_util_bonded_peers(peers, &np, 8) != 0) return 0;
  for (int i = 0; i < np; i++) if (memcmp(peers[i].val, val, 6) == 0 && ble_store_util_delete_peer(&peers[i]) == 0) del++;
  return del;
}
#endif

// La pila BLE solo se arranca cuando hay algun hueco activado. Si se olvida o se empareja antes, los vinculos se borran justo despues de
// arrancarla (antes de cualquier conexion): se apuntan aqui las direcciones del hueco y, para emparejar, la limpieza de huerfanos.
static volatile bool s_inited = false;
static uint8_t s_pend_val[6][6];
static uint8_t s_pend_n = 0;
static uint8_t s_pend_sweep = 0;                  // bit DEV_x: bonds_delete(x, 1) pendiente

// Olvida los vinculos del hueco s (mode como bonds_delete). Llamar ANTES de borrar su direccion de la NVS. Devuelve cuantos ha borrado (-1 = luego).
static int bonds_forget_slot(uint8_t s, uint8_t mode) {
#ifdef HAVE_BLE_STORE
  if (s_inited) return bonds_delete(s, mode);
  uint8_t b[6];
  auto pend = [&](const uint8_t* v) { if (s_pend_n < 6) memcpy(s_pend_val[s_pend_n++], v, 6); };
  if (SL[s].saved_addr[0] && parse_addr(SL[s].saved_addr, b)) pend(b);
  if (SL[s].ida_ok) pend(SL[s].ida + 1);
  if (mode == 1) s_pend_sweep |= (uint8_t)(1u << s);
  return -1;
#else
  (void)s; (void)mode;
  return 0;
#endif
}
static void bonds_pending_run() {
#ifdef HAVE_BLE_STORE
  for (uint8_t i = 0; i < s_pend_n; i++) {
    bool other = false;
    for (uint8_t o = 0; o < DEV_N; o++) if (slot_owns(o, s_pend_val[i])) other = true;   // ya es de un hueco (se volvio a emparejar)
    if (!other) bonds_delete_val(s_pend_val[i]);
  }
  s_pend_n = 0;
  for (uint8_t s = 0; s < DEV_N; s++) if (s_pend_sweep & (1u << s)) bonds_delete(s, 1);
  s_pend_sweep = 0;
#endif
}

// Direcciones rechazadas en la ventana de emparejado en curso (resultaron ser de otro tipo al leer su descriptor)
static char    s_rej[4][18];
static uint8_t s_rej_n = 0;
static bool rejected(const String& a) { for (uint8_t i = 0; i < s_rej_n; i++) if (a.equalsIgnoreCase(s_rej[i])) return true; return false; }
static void reject_add(const char* a) {
  if (s_rej_n >= 4) { memmove(s_rej[0], s_rej[1], sizeof s_rej[0] * 3); s_rej_n = 3; }
  strncpy(s_rej[s_rej_n], a, sizeof s_rej[0] - 1); s_rej[s_rej_n][sizeof s_rej[0] - 1] = 0; s_rej_n++;
}

// ------------------------------------------------------------------------------------------------------------------------------------
// Conexion BLE
// ------------------------------------------------------------------------------------------------------------------------------------
class CliCb : public BLEClientCallbacks {
 public:
  uint8_t dev = 0;
  void onConnect(BLEClient*) override {}
  void onDisconnect(BLEClient*) override {
    SL[dev].connected = false;
#ifdef HAVE_BLE_STORE
    nsub_clear(dev);
#endif
#ifdef HAVE_BLE_STORE
    set_state(dev, "desconectado (motivo 0x%X)", (unsigned)s_last_disc_reason);
#else
    set_state(dev, "desconectado");
#endif
  }
};
static CliCb s_cli_cb[DEV_N];

static int8_t app_to_dev(uint16_t app) {
  if (app == 0x03C2) return DEV_MOUSE;
  if (app == 0x03C1) return DEV_KBD;
  if (app == 0x03C3 || app == 0x03C4) return DEV_PAD;
  return -1;                                       // 0x03C0 (HID generico) u otra cosa: se decide por el descriptor
}

// ¿Es esta direccion la de otro hueco (guardada o conectada ahora)?
static bool addr_of_other(uint8_t s, const String& a) {
  for (uint8_t o = 0; o < DEV_N; o++) {
    if (o == s) continue;
    if (SL[o].saved_addr[0] && a.equalsIgnoreCase(SL[o].saved_addr)) return true;
    if (SL[o].connected && SL[o].devaddr[0] && a.equalsIgnoreCase(SL[o].devaddr)) return true;
  }
  return false;
}

// Los HID vistos en la busqueda en curso (solo para diagnostico: "esp ble" los lista). Si el guardado no aparece, aqui se ve que anuncia.
struct SeenDev { char addr[18]; uint8_t type; int rssi; char name[24]; bool hid; uint16_t app; uint8_t flags; bool dir; };
static SeenDev s_seen[6];
static uint8_t s_seen_n = 0;
static void seen_add(const String& addr, uint8_t type, int rssi, const String& name, bool hid, uint16_t app, uint8_t flags, bool dir) {
  for (uint8_t i = 0; i < s_seen_n; i++) {
    if (!addr.equalsIgnoreCase(s_seen[i].addr)) continue;
    SeenDev& s = s_seen[i];                         // el nombre suele llegar despues, en la respuesta al escaneo
    s.rssi = rssi; s.hid = s.hid || hid; if (app) s.app = app; if (flags) s.flags = flags; s.dir = s.dir || dir;
    if (name.length()) name.toCharArray(s.name, sizeof s.name);
    return;
  }
  if (s_seen_n >= 6) return;
  SeenDev& s = s_seen[s_seen_n++];
  addr.toCharArray(s.addr, sizeof s.addr);
  s.type = type; s.rssi = rssi; s.hid = hid; s.app = app; s.flags = flags; s.dir = dir;
  name.toCharArray(s.name, sizeof s.name);
}

static volatile uint8_t s_scan_want = 0;          // huecos que buscan en el escaneo en curso (bit DEV_x)

// Busqueda propia sobre NimBLE (ble_gap_disc), sin la clase BLEScan: BLEScan guarda una copia (con sus String y vectores) de CADA dispositivo que
// oye durante toda la busqueda, y con moviles y balizas alrededor eso eran ~40 KB de golpe (minimo de 2 KB libres medido en la placa: los fallos
// por falta de memoria). Aqui cada anuncio se analiza al vuelo y solo se guardan, en una tabla fija, los que pueden interesar (HID, apariencia
// de raton/teclado/mando o una direccion guardada), juntando el anuncio y la respuesta al escaneo (donde suele ir el nombre).
struct ScanDev { ble_addr_t addr; bool hid; bool dir; int8_t rpa_slot; uint16_t app; int8_t rssi; uint8_t flags; char name[40]; };   // rpa_slot: direccion privada de ese hueco   // dir: anuncio dirigido a nosotros   // flags: las del anuncio (0 = no venian)
#define SCAN_MAX 16
static ScanDev s_sd[SCAN_MAX];
static uint8_t s_sd_n = 0;
static SemaphoreHandle_t s_scan_done = nullptr;

// Analizador tolerante de los datos del anuncio (estructuras [longitud][tipo][datos]). No se usa ble_hs_adv_parse_fields() porque rechaza el
// anuncio ENTERO si un campo esta mal formado, y el del raton M240 lo esta (el analizador de BLEScan lo aceptaba y por eso funcionaba): un campo
// que dice ser mas largo que lo que queda se toma hasta el final del paquete.
struct AdvInfo { bool hid; uint16_t app; uint8_t flags; const uint8_t* name; uint8_t name_len; };
static void adv_parse(const uint8_t* p, uint8_t n, AdvInfo* a) {
  memset(a, 0, sizeof *a);
  uint16_t i = 0;
  while (i + 1 < n) {
    uint16_t len = p[i];                             // incluye el byte de tipo
    if (len == 0) break;
    if (i + 1 + len > n) len = (uint16_t)(n - i - 1);  // mas largo que el paquete: hasta el final
    const uint8_t type = p[i + 1];
    const uint8_t* d = p + i + 2;
    const uint16_t dl = len - 1;
    switch (type) {
      case 0x01: if (dl >= 1) a->flags = d[0]; break;                                       // banderas
      case 0x02: case 0x03:                                                                 // UUID de 16 bits
        for (uint16_t k = 0; k + 1 < dl; k += 2) if ((uint16_t)(d[k] | (d[k + 1] << 8)) == 0x1812) a->hid = true;
        break;
      case 0x08: case 0x09: a->name = d; a->name_len = (uint8_t)dl; break;                  // nombre (corto o completo)
      case 0x19: if (dl >= 2) a->app = (uint16_t)(d[0] | (d[1] << 8)); break;               // apariencia
      default: break;
    }
    i += 1 + len;
  }
}

// Copia un nombre del anuncio quedandose con lo imprimible del principio (algunos dispositivos meten bytes raros)
static void name_copy(char* dst, size_t cap, const uint8_t* src, uint8_t n) {
  size_t k = 0;
  while (k < n && k + 1 < cap && src[k] >= 0x20 && src[k] <= 0x7E) { dst[k] = (char)src[k]; k++; }
  dst[k] = 0;
}

// Mismo nombre: la parte imprimible del principio de los dos coincide (al menos 3 caracteres). Asi vale tambien un nombre guardado por el
// firmware anterior, que podia llevar basura detras (el analizador de BLEScan leia mas alla del paquete).
static bool name_match(const char* a, const char* b) {
  size_t k = 0;
  while (a[k] && b[k] && a[k] >= 0x20 && a[k] <= 0x7E && a[k] == b[k]) k++;
  const bool a_end = !a[k] || a[k] < 0x20 || a[k] > 0x7E, b_end = !b[k] || b[k] < 0x20 || b[k] > 0x7E;
  return k >= 3 && a_end && b_end;
}

// Los 5 primeros bytes de la direccion iguales a los de alguna guardada: los Logitech (M240, Pebble) cambian solo el ultimo al reemparejar
static bool prefix_of_saved(const char* a) {
  for (uint8_t s = 0; s < DEV_N; s++) if (SL[s].saved_addr[0] && strncasecmp(a, SL[s].saved_addr, 14) == 0) return true;   // "d4:8e:0e:86:cc"
  return false;
}

// ------------------------------------------------------------------------------------------------------------------------------------
// Direcciones privadas (RPA). Los Logitech, una vez vinculados, vuelven anunciandose con una direccion privada resoluble que cambia cada pocos
// minutos y no se parece a la suya: solo la reconoce quien tiene la clave de identidad (IRK) que entregaron al emparejar. Lo normal es que la
// resuelva el controlador (NimBLE mete las IRK de los vinculos en su lista de resolucion; ver ble_task) y entonces el anuncio llega con la
// direccion de identidad (tipo 2/3). Si no, se resuelve aqui con las IRK guardadas (AES-128, Core spec vol 3 parte H 2.2.2).
// ------------------------------------------------------------------------------------------------------------------------------------
struct IrkEnt { ble_addr_t id; uint8_t irk[16]; int8_t slot; };
static IrkEnt  s_irk[4];
static uint8_t s_irk_n = 0;
static uint16_t s_rpa_seen = 0, s_rpa_resolved = 0;   // diagnostico: direcciones privadas oidas en la ultima busqueda / resueltas con una IRK

static void irk_load() {                          // las IRK de los vinculos, al empezar cada busqueda (ble_task)
  s_irk_n = 0;
#ifdef HAVE_BLE_STORE
  ble_addr_t peers[8]; int np = 0;
  if (ble_store_util_bonded_peers(peers, &np, 8) != 0) return;
  for (int i = 0; i < np && s_irk_n < 4; i++) {
    struct ble_store_key_sec k; memset(&k, 0, sizeof k); k.peer_addr = peers[i];
    struct ble_store_value_sec v;
    if (ble_store_read_peer_sec(&k, &v) != 0 || !v.irk_present) continue;
    int8_t slot = -1;
    for (uint8_t o = 0; o < DEV_N; o++) if (slot_owns(o, peers[i].val)) slot = (int8_t)o;
    IrkEnt& e = s_irk[s_irk_n++];
    e.id = peers[i]; memcpy(e.irk, v.irk, 16); e.slot = slot;
  }
#endif
}

// ah(irk, prand) == hash. NimBLE guarda las claves y las direcciones al reves (LSB primero): se dan la vuelta para el AES y para el resultado
static bool rpa_matches1(const uint8_t key[16], const uint8_t a[6]) {
  uint8_t in[16], out[16];
  memset(in, 0, sizeof in);
  in[13] = a[5]; in[14] = a[4]; in[15] = a[3];   // prand (los 3 bytes altos de la direccion), al final del bloque en big endian
  mbedtls_aes_context ctx;
  mbedtls_aes_init(&ctx);
  bool ok = mbedtls_aes_setkey_enc(&ctx, key, 128) == 0 && mbedtls_aes_crypt_ecb(&ctx, MBEDTLS_AES_ENCRYPT, in, out) == 0;
  mbedtls_aes_free(&ctx);
  return ok && out[15] == a[0] && out[14] == a[1] && out[13] == a[2];   // hash = los 3 bytes bajos
}
// Se prueban los dos ordenes de la clave: no esta claro en cual la guarda NimBLE (al pasarla al controlador le da la vuelta)
static bool rpa_matches(const uint8_t irk[16], const uint8_t a[6]) {
  uint8_t rev[16];
  for (uint8_t k = 0; k < 16; k++) rev[k] = irk[15 - k];
  return rpa_matches1(rev, a) || rpa_matches1(irk, a);
}

// Hueco al que pertenece una direccion privada resoluble (-1 si no es de ninguno de los vinculos con IRK)
static int8_t rpa_slot(const ble_addr_t& a) {
  if (a.type != BLE_ADDR_RANDOM || (a.val[5] & 0xC0) != 0x40) return -1;   // RPA: aleatoria con los dos bits altos a 01
  s_rpa_seen++;
  for (uint8_t i = 0; i < s_irk_n; i++) if (rpa_matches(s_irk[i].irk, a.val)) { s_rpa_resolved++; return s_irk[i].slot >= 0 ? s_irk[i].slot : (int8_t)DEV_N; }
  return -1;
}

// La direccion de identidad del vinculo del hueco s (la que da el controlador si resuelve la privada)
static bool is_identity_of(uint8_t s, const ble_addr_t& a) {
  return SL[s].ida_ok && memcmp(SL[s].ida + 1, a.val, 6) == 0;
}

// Lo que antes hacia el callback de BLEScan con cada anuncio: decide si es candidato para alguno de los huecos que buscan
static void scan_consider(const ScanDev& v) {
  const bool is_hid = v.hid;
  const uint16_t app = v.app;
  const int8_t app_dev = app_to_dev(app);
  String addr = BLEAddress(v.addr).toString();
  String nm(v.name);
  const int rssi = v.rssi;
  bool known_any = false;
  for (uint8_t s = 0; s < DEV_N; s++) if (SL[s].saved_addr[0] && addr.equalsIgnoreCase(SL[s].saved_addr)) known_any = true;
  for (uint8_t s = 0; s < DEV_N; s++) if (is_identity_of(s, v.addr) || v.rpa_slot == (int8_t)s) known_any = true;
  if (is_hid || app_dev >= 0 || known_any || prefix_of_saved(addr.c_str()) || v.addr.type >= 2) seen_add(addr, v.addr.type, rssi, nm, is_hid, app, v.flags, v.dir);

  for (uint8_t s = 0; s < DEV_N; s++) {
    if (!(s_scan_want & (1u << s))) continue;
    Slot& L = SL[s];
    const bool known = (L.saved_addr[0] && addr.equalsIgnoreCase(L.saved_addr)) || is_identity_of(s, v.addr) || v.rpa_slot == (int8_t)s;
    const bool prefix = !known && L.saved_addr[0] && strncasecmp(addr.c_str(), L.saved_addr, 14) == 0;
    const bool of_type = app_dev == (int8_t)s;
    if (!is_hid && !of_type && !known && !prefix) continue;       // el guardado (o el mismo con otra direccion) se acepta aunque no anuncie HID
    if (!known && addr_of_other(s, addr)) continue;               // ya es de otro hueco
    if (!known && app_dev >= 0 && !of_type) continue;             // se anuncia como otro tipo de dispositivo
    if (!known && rejected(addr)) continue;
    // El mismo dispositivo con otra direccion (los Logitech cambian el ultimo byte): mismos 5 primeros bytes y el mismo nombre; si no ha llegado
    // el nombre (anuncio dirigido, o sin respuesta al escaneo), basta con que anuncie HID o su tipo, o que el anuncio vaya dirigido a nosotros
    bool same_dev = false;
    if (prefix && !pairing(s)) {
      if (nm.length() && L.saved_name[0]) same_dev = name_match(nm.c_str(), L.saved_name);
      else same_dev = is_hid || of_type || v.dir;
    }
    if (!known && !same_dev && !pairing(s)) continue;             // fuera de la ventana de emparejado, solo el guardado
    // Uno nuevo solo si esta en modo emparejado: se anuncia "descubrible" (bandera general o limitada). Un teclado o raton que solo busca volver
    // a su PC no lleva ninguna de las dos. Si el anuncio no trae banderas (raro), se acepta como antes.
    const bool discoverable = (v.flags & (BLE_HS_ADV_F_DISC_LTD | BLE_HS_ADV_F_DISC_GEN)) != 0;
    if (!known && !same_dev && v.flags && !discoverable) continue;
    int score = rssi;
    if (of_type) score += 20;                                     // uno del tipo declarado antes que un HID cualquiera
    if (v.flags & BLE_HS_ADV_F_DISC_LTD) score += 10;             // "descubrible limitado": el que se acaba de poner en modo emparejado
    if (known) score += 200;                                      // el que ya se conocia, antes que nada
    else if (same_dev) score += 150;
    // ">=": el mismo dispositivo vuelve a pasar por aqui cuando llega su nombre (respuesta al escaneo) y debe quedarse con el
    if (score > L.cand_score || (L.cand_ok && score == L.cand_score && L.cand_type == v.addr.type && L.cand_addr.equals(BLEAddress(v.addr)))) {
      L.cand_score = score;
      L.cand_ok = true;
      L.cand_addr = BLEAddress(v.addr);
      L.cand_type = v.addr.type;
      L.cand_rssi = rssi;
      L.cand_app = app;
      if (v.name[0]) { strncpy(L.cand_name, v.name, sizeof L.cand_name - 1); L.cand_name[sizeof L.cand_name - 1] = 0; }
      else strcpy(L.cand_name, "(sin nombre)");
    }
  }
}

// Eventos de la busqueda (tarea del host NimBLE)
// Modo escucha ("esp ble sniff"): la busqueda siguiente apunta TODOS los anuncios que oye, sin filtrar, para ver que hace un dispositivo
struct SniffEnt { ble_addr_t addr; uint8_t evmask; uint8_t flags; bool hid; uint16_t app; int8_t rssi; uint16_t count; char name[20]; };
#define SNIFF_MAX 24
static SniffEnt s_sniff[SNIFF_MAX];
static uint8_t  s_sniff_n = 0;
static volatile uint8_t s_sniff_state = 0;          // 0 nada, 1 pedido (la proxima busqueda), 2 escuchando, 3 hecho
static uint16_t s_sniff_lost = 0;
static void sniff_add(const struct ble_gap_disc_desc& d, const AdvInfo& f) {
  SniffEnt* e = nullptr;
  for (uint8_t i = 0; i < s_sniff_n; i++)
    if (s_sniff[i].addr.type == d.addr.type && memcmp(s_sniff[i].addr.val, d.addr.val, 6) == 0) { e = &s_sniff[i]; break; }
  if (!e) {
    if (s_sniff_n >= SNIFF_MAX) { s_sniff_lost++; return; }
    e = &s_sniff[s_sniff_n++];
    memset(e, 0, sizeof *e);
    e->addr = d.addr;
  }
  if (d.event_type < 8) e->evmask |= (uint8_t)(1u << d.event_type);
  e->rssi = d.rssi;
  if (e->count < 65535) e->count++;
  if (f.flags) e->flags = f.flags;
  if (f.hid) e->hid = true;
  if (f.app) e->app = f.app;
  if (f.name && f.name_len) { char nm[sizeof e->name]; name_copy(nm, sizeof nm, f.name, f.name_len); if (nm[0]) memcpy(e->name, nm, sizeof nm); }
}

static int scan_gap_cb(struct ble_gap_event* ev, void*) {
  if (ev->type == BLE_GAP_EVENT_DISC_COMPLETE) {
    if (s_scan_done) xSemaphoreGive(s_scan_done);
    return 0;
  }
  if (ev->type != BLE_GAP_EVENT_DISC) return 0;
  const struct ble_gap_disc_desc& d = ev->disc;
  const bool rsp = d.event_type == BLE_HCI_ADV_RPT_EVTYPE_SCAN_RSP;
  AdvInfo f;
  adv_parse(d.data, d.length_data, &f);
  if (s_sniff_state == 2) sniff_add(d, f);          // modo escucha: todo, antes de cualquier filtro
  if (!rsp && d.event_type != BLE_HCI_ADV_RPT_EVTYPE_ADV_IND && d.event_type != BLE_HCI_ADV_RPT_EVTYPE_DIR_IND) return 0;   // no conectable
  const bool hid = f.hid;
  const uint16_t app = f.app;

  ScanDev* e = nullptr;
  for (uint8_t i = 0; i < s_sd_n; i++)
    if (s_sd[i].addr.type == d.addr.type && memcmp(s_sd[i].addr.val, d.addr.val, 6) == 0) { e = &s_sd[i]; break; }
  if (!e) {
    // Interesa si anuncia HID o apariencia de raton/teclado/mando (en el anuncio o en la respuesta al escaneo: muchos ponen ahi el UUID y el
    // nombre) o si su direccion es una guardada o comparte con ella los 5 primeros bytes (los Logitech solo cambian el ultimo)
    const int8_t rs = rpa_slot(d.addr);
    bool keep = hid || app_to_dev(app) >= 0 || d.addr.type >= 2 || rs >= 0;   // tipo 2/3: el controlador la resolvio (es un vinculo)
    for (uint8_t s = 0; s < DEV_N && !keep; s++) if (is_identity_of(s, d.addr)) keep = true;
    if (!keep) {
      String a = BLEAddress(d.addr).toString();
      if (!prefix_of_saved(a.c_str())) return 0;     // (incluye la direccion exacta)
    }
    if (s_sd_n >= SCAN_MAX) return 0;
    e = &s_sd[s_sd_n++];
    memset(e, 0, sizeof *e);
    e->addr = d.addr;
    e->rpa_slot = rs;
  }
  e->rssi = d.rssi;
  if (hid) e->hid = true;
  if (d.event_type == BLE_HCI_ADV_RPT_EVTYPE_DIR_IND) e->dir = true;
  if (!rsp && f.flags) e->flags = f.flags;
  if (app) e->app = app;
  if (f.name && f.name_len) {
    char nm[sizeof e->name];
    name_copy(nm, sizeof nm, f.name, f.name_len);
    if (nm[0]) memcpy(e->name, nm, sizeof nm);
  }
  scan_consider(*e);
  return 0;
}

// Una busqueda para todos los huecos que la necesitan. light: ya hay algo conectado y nadie empareja: ventana mas corta (menos radio robada)
static void scan_once(uint8_t want, bool light) {
  for (uint8_t s = 0; s < DEV_N; s++) { SL[s].cand_ok = false; SL[s].cand_score = -1000; }
  s_seen_n = 0;
  s_sd_n = 0;
  for (uint8_t s = 0; s < DEV_N; s++) {
    if (!(want & (1u << s))) continue;
    if (pairing(s)) set_state(s, "buscando para emparejar (%us)", (unsigned)pair_left_s(s));
    else set_state(s, "buscando el guardado %s", SL[s].saved_addr);
  }
  if (!s_scan_done) s_scan_done = xSemaphoreCreateBinary();
  if (!s_scan_done) return;
  xSemaphoreTake(s_scan_done, 0);                    // por si quedo dado de una busqueda anterior
  irk_load();
  s_rpa_seen = 0; s_rpa_resolved = 0;
  if (s_sniff_state == 1) { s_sniff_n = 0; s_sniff_lost = 0; s_sniff_state = 2; }
  // Siempre con la direccion PUBLICA del ESP32, como BLEScan (BLEDevice::m_ownAddrType): un dispositivo vinculado puede volver con anuncios
  // DIRIGIDOS a la direccion que conoce del host, y el controlador solo los pasa si buscamos con esa misma direccion.
  const uint8_t own = BLE_OWN_ADDR_PUBLIC;
  struct ble_gap_disc_params dp;
  memset(&dp, 0, sizeof dp);
  dp.itvl = 160;                                     // 100 ms (unidades de 0,625 ms), como antes
  dp.window = light ? 48 : 80;                       // 30 o 50 ms
  dp.filter_duplicates = 1;                          // cada dispositivo una vez (anuncio + respuesta)
  dp.passive = 0;                                    // activa: pide la respuesta al escaneo, donde va el nombre
  s_scan_want = want;
  const int rc = ble_gap_disc(own, MOUSE_BLE_SCAN_S * 1000, &dp, scan_gap_cb, nullptr);
  if (rc == 0) {
    if (xSemaphoreTake(s_scan_done, pdMS_TO_TICKS(MOUSE_BLE_SCAN_S * 1000 + 1500)) != pdTRUE) {
      ble_gap_disc_cancel();
      vTaskDelay(pdMS_TO_TICKS(50));
    }
  } else {
    Serial.printf("BLE: no se pudo empezar la busqueda (rc %d)\n", rc);
    vTaskDelay(pdMS_TO_TICKS(MOUSE_BLE_SCAN_S * 1000));
  }
  s_scan_want = 0;
  if (s_sniff_state == 2) s_sniff_state = 3;
}

// ------------------------------------------------------------------------------------------------------------------------------------
// GATT a mano (los tres dispositivos). La libreria crea un objeto (con dos o tres semaforos y varios String) por cada servicio, caracteristica
// y descriptor que descubre, y getDescriptor()/subscribe() buscan los descriptores hasta el FINAL DEL SERVICIO creando objetos tambien para los
// atributos de las caracteristicas siguientes. Medido en la placa: ~10 KB de servicios y caracteristicas por dispositivo y 10-12 KB por cada
// informe suscrito; con los tres conectados la web se quedaba sin memoria. Aqui todo va con las funciones de NimBLE y una tabla de handles: el
// servicio HID, sus caracteristicas, el mapa de informes (lectura larga), los descriptores (solo hasta la caracteristica siguiente), el Report
// Reference y el CCCD. La libreria solo conecta y cifra. Las notificaciones llegan por el listener de eventos GAP (gap_listener).
// ------------------------------------------------------------------------------------------------------------------------------------
struct GattOp { volatile bool done; volatile int status; volatile uint16_t h2902, h2908; uint8_t buf[8]; volatile uint16_t len; };
static GattOp s_gop;
static SemaphoreHandle_t s_gop_sem = nullptr;

// Las caracteristicas del servicio HID del dispositivo que se esta conectando (solo ble_task)
struct GChr { uint16_t uuid16; uint16_t def_h, val_h; uint8_t props; };
#define GCHR_MAX 24
struct GTab { uint16_t svc_start, svc_end; GChr c[GCHR_MAX]; uint8_t n; };
static GTab s_gtab;
static uint8_t  s_rmap[512];                       // el mapa de informes (512 B como mucho, por la especificacion)
static uint16_t s_rmap_len = 0;

static void gop_finish(int status) {
  if (s_gop.done) return;                            // una sola respuesta por operacion
  s_gop.status = status;
  s_gop.done = true;
  xSemaphoreGive(s_gop_sem);
}
static int gop_svc_cb(uint16_t, const struct ble_gatt_error* err, const struct ble_gatt_svc* svc, void*) {
  if (s_gop.done) return 1;
  if (err->status == 0 && svc) {
    if (!s_gtab.svc_end) { s_gtab.svc_start = svc->start_handle; s_gtab.svc_end = svc->end_handle; }   // el primero, si hubiera varios
    return 0;
  }
  gop_finish(err->status == BLE_HS_EDONE ? 0 : err->status);
  return 0;
}
static int gop_chr_cb(uint16_t, const struct ble_gatt_error* err, const struct ble_gatt_chr* chr, void*) {
  if (s_gop.done) return 1;
  if (err->status == 0 && chr) {
    if (s_gtab.n < GCHR_MAX) {
      GChr& c = s_gtab.c[s_gtab.n++];
      c.uuid16 = chr->uuid.u.type == BLE_UUID_TYPE_16 ? chr->uuid.u16.value : 0;
      c.def_h = chr->def_handle; c.val_h = chr->val_handle; c.props = chr->properties;
    }
    return 0;
  }
  gop_finish(err->status == BLE_HS_EDONE ? 0 : err->status);
  return 0;
}
static int gop_dsc_cb(uint16_t, const struct ble_gatt_error* err, uint16_t, const struct ble_gatt_dsc* dsc, void*) {
  if (s_gop.done) return 1;
  if (err->status == 0 && dsc) {
    if (dsc->uuid.u.type == BLE_UUID_TYPE_16) {
      const uint16_t u = dsc->uuid.u16.value;
      if (u == 0x2803 || u == 0x2800 || u == 0x2801) { gop_finish(0); return 1; }   // ya es la caracteristica (o el servicio) siguiente
      if (u == 0x2902 && !s_gop.h2902) s_gop.h2902 = dsc->handle;
      if (u == 0x2908 && !s_gop.h2908) s_gop.h2908 = dsc->handle;
    }
    return 0;
  }
  gop_finish(err->status == BLE_HS_EDONE ? 0 : err->status);
  return 0;
}
static int gop_attr_cb(uint16_t, const struct ble_gatt_error* err, struct ble_gatt_attr* attr, void*) {
  if (s_gop.done) return 0;
  if (err->status == 0 && attr && attr->om) {
    uint16_t n = OS_MBUF_PKTLEN(attr->om);
    if (n > sizeof s_gop.buf) n = sizeof s_gop.buf;
    os_mbuf_copydata(attr->om, 0, n, s_gop.buf);
    s_gop.len = n;
  }
  gop_finish(err->status);
  return 0;
}
static int gop_long_cb(uint16_t, const struct ble_gatt_error* err, struct ble_gatt_attr* attr, void*) {   // lectura larga: un trozo por llamada
  if (s_gop.done) return 1;
  if (err->status == 0 && attr && attr->om) {
    const uint16_t off = attr->offset;
    uint16_t n = OS_MBUF_PKTLEN(attr->om);
    if (off >= sizeof s_rmap) n = 0;
    else if (off + n > sizeof s_rmap) n = (uint16_t)(sizeof s_rmap - off);
    if (n) os_mbuf_copydata(attr->om, 0, n, s_rmap + off);
    if (off + n > s_rmap_len) s_rmap_len = off + n;
    return 0;
  }
  gop_finish(err->status == BLE_HS_EDONE ? 0 : err->status);
  return 0;
}
static bool gop_begin() {
  if (!s_gop_sem) s_gop_sem = xSemaphoreCreateBinary();
  if (!s_gop_sem) return false;
  xSemaphoreTake(s_gop_sem, 0);
  s_gop.done = false; s_gop.status = -1; s_gop.h2902 = 0; s_gop.h2908 = 0; s_gop.len = 0;
  return true;
}
static bool gop_end(int rc) {
  if (rc != 0) { s_gop.done = true; return false; }
  const bool got = xSemaphoreTake(s_gop_sem, pdMS_TO_TICKS(5000)) == pdTRUE;
  s_gop.done = true;                                 // si no llego a tiempo, lo que llegue despues se ignora
  return got && s_gop.status == 0;
}

// El servicio HID (0x1812) y sus caracteristicas, en s_gtab
static bool gatt_discover_hid(uint16_t conn) {
  memset(&s_gtab, 0, sizeof s_gtab);
  if (!gop_begin()) return false;
  static const ble_uuid16_t UUID_HID = { { BLE_UUID_TYPE_16 }, 0x1812 };   // (BLE_UUID16_DECLARE es un literal compuesto de C: no vale en C++)
  if (!gop_end(ble_gattc_disc_svc_by_uuid(conn, &UUID_HID.u, gop_svc_cb, nullptr)) || !s_gtab.svc_end) return false;
  if (!gop_begin()) return false;
  return gop_end(ble_gattc_disc_all_chrs(conn, s_gtab.svc_start, s_gtab.svc_end, gop_chr_cb, nullptr)) && s_gtab.n;
}
static const GChr* gtab_find(uint16_t uuid16) {
  for (uint8_t i = 0; i < s_gtab.n; i++) if (s_gtab.c[i].uuid16 == uuid16) return &s_gtab.c[i];
  return nullptr;
}
static bool gatt_read_long(uint16_t conn, uint16_t h) {        // en s_rmap / s_rmap_len
  s_rmap_len = 0;
  if (!gop_begin()) return false;
  return gop_end(ble_gattc_read_long(conn, h, 0, gop_long_cb, nullptr));
}
// CCCD (0x2902) y Report Reference (0x2908) de la caracteristica con el valor en val_h, buscando hasta end_h
static bool gatt_find_dscs(uint16_t conn, uint16_t val_h, uint16_t end_h, uint16_t* cccd, uint16_t* rref) {
  *cccd = 0; *rref = 0;
  if (end_h <= val_h) return true;                   // sin descriptores
  if (!gop_begin()) return false;
  const bool ok = gop_end(ble_gattc_disc_all_dscs(conn, val_h, end_h, gop_dsc_cb, nullptr));
  *cccd = s_gop.h2902; *rref = s_gop.h2908;
  return ok;
}
static bool gatt_read_small(uint16_t conn, uint16_t h, uint8_t* out, uint16_t* n) {
  if (!gop_begin()) return false;
  const bool ok = gop_end(ble_gattc_read(conn, h, gop_attr_cb, nullptr));
  *n = s_gop.len;
  memcpy(out, s_gop.buf, s_gop.len);
  return ok;
}
static bool gatt_write_cccd(uint16_t conn, uint16_t h, uint16_t val) {
  const uint8_t v[2] = { (uint8_t)val, (uint8_t)(val >> 8) };
  if (!gop_begin()) return false;
  return gop_end(ble_gattc_write_flat(conn, h, v, 2, gop_attr_cb, nullptr));
}
// Protocol Mode (0x2A4E) a 0 = boot: escritura sin respuesta, como hacia la libreria
static void gatt_set_boot_protocol(uint16_t conn) {
  const GChr* pm = gtab_find(0x2A4E);
  if (!pm) return;
  const uint8_t z = 0;
  ble_gattc_write_no_rsp_flat(conn, pm->val_h, &z, 1);
}

// Se suscribe a las caracteristicas uuid16 (0x2A4D Report, 0x2A22/0x2A33 informes boot) con notificacion; con Report, solo a las de entrada y,
// si el descriptor usa Report ID, a la de want_id. Las apunta para gap_listener. Devuelve cuantas.
static int subscribe_raw(Slot& L, uint8_t dev, uint16_t uuid16, bool has_id, uint8_t want_id, bool all_ids = false) {
  const uint16_t conn = L.client->getConnId();
  int nsub = 0;
  for (uint8_t i = 0; i < s_gtab.n; i++) {                  // van en orden de handle
    if (!L.client->isConnected()) break;                    // se ha desconectado a mitad: no seguir (lo dice connect_slot)
    const GChr& c = s_gtab.c[i];
    if (c.uuid16 != uuid16 || !(c.props & BLE_GATT_CHR_PROP_NOTIFY)) continue;
    // sus descriptores acaban justo antes de la declaracion de la siguiente; la ultima, al final del servicio
    const uint16_t end = (i + 1 < s_gtab.n) ? (uint16_t)(s_gtab.c[i + 1].def_h - 1) : s_gtab.svc_end;
    uint16_t cccd = 0, rref = 0;
    gatt_find_dscs(conn, c.val_h, end, &cccd, &rref);
    uint8_t rid = 0, rtype = 1;
    if (uuid16 == 0x2A4D && rref) {
      uint8_t b[8]; uint16_t n = 0;
      if (gatt_read_small(conn, rref, b, &n) && n >= 2) { rid = b[0]; rtype = b[1]; }
    }
    ph_memf("%s: informe id %u tipo %u (cccd %u)", L.tag, (unsigned)rid, (unsigned)rtype, (unsigned)cccd);
    if (rtype != 1 || !cccd) continue;
    if (has_id && !all_ids && rid != want_id) continue;
    nsub_add(conn, c.val_h, dev, rid);                           // antes de activarla, para no perder el primer informe
    if (gatt_write_cccd(conn, cccd, 0x0001)) nsub++;
  }
  return nsub;
}

// Raton: el mismo analisis del descriptor de siempre (parse_report_map / boot_layout) y la misma acumulacion (on_input); solo cambia como se
// descubre y se suscribe (a mano) y que los informes llegan por gap_listener.
static int subscribe_mouse(Slot& L, const String& m) {
  Layout lay; bool have_lay = false;
  if (m.length()) {
    have_lay = parse_report_map((const uint8_t*)m.c_str(), m.length(), &lay);
    snprintf(L.layout_txt, sizeof L.layout_txt, "mapa %u B: %s", (unsigned)m.length(), have_lay ? "X/Y encontrados" : "sin X/Y");
  } else {
    snprintf(L.layout_txt, sizeof L.layout_txt, "sin mapa de informes");
  }
  int nsub = 0;
  if (have_lay) {
    s_mouse_lay = lay;                                       // lo usa gap_listener con cada informe
    nsub = subscribe_raw(L, DEV_MOUSE, 0x2A4D, lay.has_id, lay.id);
    if (nsub) {
      snprintf(L.mode, sizeof L.mode, "informes");
      char t[sizeof L.layout_txt];
      snprintf(t, sizeof t, "%s; X %u/%u b, Y %u/%u b, rueda %s, id %u", L.layout_txt, lay.x.bit, lay.x.size, lay.y.bit, lay.y.size, lay.wheel.ok ? "si" : "no", lay.has_id ? lay.id : 0);
      strncpy(L.layout_txt, t, sizeof L.layout_txt - 1); L.layout_txt[sizeof L.layout_txt - 1] = 0;
    }
  }
  if (!nsub) {                                               // si no, el informe "boot" (0x2A33)
    const GChr* bm = gtab_find(0x2A33);
    if (bm && (bm->props & BLE_GATT_CHR_PROP_NOTIFY)) {
      gatt_set_boot_protocol(L.client->getConnId());
      s_mouse_lay = boot_layout();
      nsub = subscribe_raw(L, DEV_MOUSE, 0x2A33, false, 0);
      if (nsub) {
        snprintf(L.mode, sizeof L.mode, "boot");
        snprintf(L.layout_txt, sizeof L.layout_txt, "informe boot (botones, dx, dy, rueda de 8 bits)");
      }
    }
  }
  return nsub;
}

static int subscribe_kbd(Slot& L, const String& m) {
  KbdLayout lay; kbd_clear(lay);
  bool have_lay = m.length() && parse_kbd_map((const uint8_t*)m.c_str(), m.length(), &lay);
  int nsub = 0;
  if (have_lay) {
    s_kbd_lay = lay;                                         // lo usa gap_listener con cada informe
    nsub = subscribe_raw(L, DEV_KBD, 0x2A4D, lay.has_id, lay.id);
    if (nsub) {
      unsigned nbits = 0; for (uint8_t b = 0; b < lay.nbm; b++) nbits += lay.bm[b].count;
      unsigned nkeys = 0; for (uint8_t a = 0; a < lay.narr; a++) nkeys += lay.arr[a].count;
      snprintf(L.mode, sizeof L.mode, "informes");
      snprintf(L.layout_txt, sizeof L.layout_txt, "mapa %u B: %u casillas de codigo, %u bits NKRO, modificadores %s, id %u",
               (unsigned)m.length(), nkeys, nbits, lay.mod[0] >= 0 ? "si" : "no", (unsigned)lay.id);
    }
  }
  if (!nsub && gtab_find(0x2A22)) {                                     // respaldo: informe boot del teclado (Protocol Mode solo si lo hay)
    gatt_set_boot_protocol(L.client->getConnId());
    s_kbd_lay = kbd_boot_layout();
    nsub = subscribe_raw(L, DEV_KBD, 0x2A22, false, 0);
    if (nsub) {
      snprintf(L.mode, sizeof L.mode, "boot");
      snprintf(L.layout_txt, sizeof L.layout_txt, "informe boot del teclado (modificadores + 6 codigos)%s", m.length() ? "; el mapa no tenia pagina 0x07" : "");
    }
  }
  return nsub;
}

static int subscribe_pad(Slot& L, const String& m) {
  static PadLayout lay;                       // grande para la pila: solo lo usa ble_task
  if (!m.length() || !parse_pad_map((const uint8_t*)m.c_str(), m.length(), &lay)) {
    if (m.length()) snprintf(L.layout_txt, sizeof L.layout_txt, "mapa %u B sin ejes, hat ni botones", (unsigned)m.length());
    else snprintf(L.layout_txt, sizeof L.layout_txt, "sin mapa de informes");
    return 0;
  }
  pad_pick_table(L.devname, &lay);
  s_pad_lay = lay;                            // lo usa gap_listener con cada informe
  pad_reset_aux();
  const int nsub = subscribe_raw(L, DEV_PAD, 0x2A4D, lay.has_id, lay.id, true);   // todos sus informes de entrada (botones fuera del principal)
  if (nsub) {
    snprintf(L.mode, sizeof L.mode, "informes");
    snprintf(L.layout_txt, sizeof L.layout_txt, "mapa %u B: %s; tabla %s; consumo %s", (unsigned)m.length(), lay.axes, lay.table, lay.cons[0] ? lay.cons : "-");
  } else {
    snprintf(L.layout_txt, sizeof L.layout_txt, "mapa %u B: %s; ningun informe con notificacion", (unsigned)m.length(), lay.axes);
  }
  return nsub;
}

// Deja en last_pair (y en el log) como ha acabado el emparejado del hueco s
static void pair_note(uint8_t s, bool still_connected, bool secure_ok) {
  Slot& L = SL[s];
  char when[40] = "";
  const uint32_t pk = s_passkey_ms;
  if (pk) snprintf(when, sizeof when, ", %lu s despues de ensenar la clave", (unsigned long)((millis() - pk) / 1000));
  int st = -1;
#ifdef HAVE_BLE_STORE
  st = s_sec_enc_status;
  const int dr = s_sec_disc_reason;
  const int us = BLE_HS_ERR_SM_US_BASE, peer = BLE_HS_ERR_SM_PEER_BASE;
  if (st == us + BLE_SM_ERR_CONFIRM_MISMATCH || st == peer + BLE_SM_ERR_CONFIRM_MISMATCH)
    snprintf(L.last_pair, sizeof L.last_pair, "FALLO: la clave no coincide (mal tecleada o de otro intento)%s", when);
  else if (st == us + BLE_SM_ERR_PASSKEY || st == peer + BLE_SM_ERR_PASSKEY)
    snprintf(L.last_pair, sizeof L.last_pair, "FALLO: el teclado cancelo o no acepto la clave%s", when);
  else if (st == BLE_HS_ETIMEOUT)
    snprintf(L.last_pair, sizeof L.last_pair, "FALLO: se acabo el tiempo (%u s) para teclear la clave%s", (unsigned)BLE_SMP_TIMEOUT_S, when);
  else if (st > 0)
    snprintf(L.last_pair, sizeof L.last_pair, "FALLO: error 0x%X del emparejado%s", (unsigned)st, when);
  else if (!still_connected)
    snprintf(L.last_pair, sizeof L.last_pair, "FALLO: se desconecto antes de acabar (motivo 0x%X)%s", (unsigned)dr, when);
  else
#endif
  if (!still_connected) snprintf(L.last_pair, sizeof L.last_pair, "FALLO: se desconecto antes de acabar%s", when);
  else if (st == 0 || secure_ok) snprintf(L.last_pair, sizeof L.last_pair, "bien: cifrado%s%s", pk ? " con clave" : "", when);
  else snprintf(L.last_pair, sizeof L.last_pair, "sin cifrar: el dispositivo no contesto a la seguridad (se sigue)");
  Serial.printf("BLE %s: emparejado: %s\n", L.tag, L.last_pair);
}

// Conecta el hueco s con el candidato de la busqueda, lo empareja y se suscribe a sus informes. true si todo salio bien.
static bool connect_slot(uint8_t s) {
  Slot& L = SL[s];
  String a = L.cand_addr.toString();
  const bool known = L.saved_addr[0] && a.equalsIgnoreCase(L.saved_addr);
  if (!known && addr_of_other(s, a)) { set_state(s, "%s ya es de otro hueco", a.c_str()); return false; }

  // Memoria: cada conexion se lleva varios KB (la libreria BLE crea objetos y semaforos por cada servicio, caracteristica y descriptor del
  // dispositivo, y si un malloc falla aborta: el ESP32 se reinicia). Con otro dispositivo ya conectado, si no hay margen no se intenta.
  bool other_conn = false;
  for (uint8_t o = 0; o < DEV_N; o++) if (o != s && SL[o].connected) other_conn = true;
  const uint32_t free0 = heap_caps_get_free_size(MALLOC_CAP_8BIT), big0 = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  if (other_conn && (free0 < BLE_MIN_FREE_HEAP || big0 < BLE_MIN_FREE_BLOCK)) {
    set_state(s, "POCA MEMORIA: %lu B libres (bloque mayor %lu): no se conecta", (unsigned long)free0, (unsigned long)big0);
    return false;
  }

  const bool no_name = !L.cand_name[0] || strcmp(L.cand_name, "(sin nombre)") == 0;
  strncpy(L.devname, (no_name && L.saved_name[0]) ? L.saved_name : L.cand_name, sizeof L.devname - 1); L.devname[sizeof L.devname - 1] = 0;
  a.toCharArray(L.devaddr, sizeof L.devaddr);
  L.rssi = L.cand_rssi;

#ifdef HAVE_BLE_STORE
  nsub_clear(s);
#endif
  if (!L.client) {
    L.client = BLEDevice::createClient();
    if (!L.client) { set_state(s, "sin memoria para el cliente BLE"); return false; }
    L.client->setClientCallbacks(&s_cli_cb[s]);
  }
  // Seguridad: el teclado se declara DisplayOnly (puede pedir que se teclee una clave de 6 cifras, que sale en esp ble); el raton y el mando,
  // sin pantalla ni teclado ("Just Works", lo de siempre). Es global en la pila, pero las conexiones se hacen de una en una desde esta tarea.
  BLESecurity::setCapability(s == DEV_KBD ? ESP_IO_CAP_OUT : ESP_IO_CAP_NONE);
  BLESecurity::resetSecurity();             // si no, con otro dispositivo ya conectado la pila no inicia la seguridad de esta conexion
  s_passkey = 0;
  set_state(s, "conectando a %s", L.devname);
  if (!L.client->connect(L.cand_addr, L.cand_type, 10000)) { set_state(s, "no conecta (%s)", L.devname); return false; }

  set_state(s, "emparejando");
  bool enc = false;
#ifdef HAVE_BLE_STORE
  struct ble_gap_conn_desc desc;
  if (ble_gap_conn_find(L.client->getConnId(), &desc) == 0) enc = desc.sec_state.encrypted;
#endif
  if (!enc) {
#ifdef HAVE_BLE_STORE
    s_sec_enc_status = -1; s_sec_disc_reason = -1; s_sec_conn = L.client->getConnId();
#endif
    s_passkey_ms = 0;
    s_secure_deadline = millis() + (s == DEV_KBD ? BLE_SECURE_KBD_MS : BLE_SECURE_MS);
    s_secure_slot = (int8_t)s;
    // Algunos dispositivos no piden cifrado hasta el primer acceso protegido: se sigue, y si hace falta lo pedira la lectura (readValue reintenta con seguridad)
    const bool sec_ok = L.client->secureConnection();
    if (!sec_ok) Serial.printf("BLE %s: secureConnection no completo, se sigue\n", L.tag);
    s_secure_slot = -1;
    pair_note(s, L.client->isConnected(), sec_ok);
    ph_memf("%s: tras cifrar", L.tag);
#ifdef HAVE_BLE_STORE
    s_sec_conn = 0xFFFF;
#endif
  }
  s_passkey = 0;
  s_passkey_ms = 0;
  if (!L.client->isConnected()) { set_state(s, "se corto al emparejar (%s)", L.devname); return false; }

  ph_memf("%s: antes de los servicios", L.tag);
  if (!gatt_discover_hid(L.client->getConnId())) {
    if (!L.client->isConnected()) { set_state(s, "se desconecto al buscar sus servicios (%s)", L.devname); return false; }
    set_state(s, "sin servicio HID"); L.client->disconnect(); return false;
  }
  ph_memf("%s: servicio HID con %u caracteristicas", L.tag, (unsigned)s_gtab.n);

  String m;
  const GChr* rmc = gtab_find(0x2A4B);                        // descriptor de informes (Report Map)
  if (rmc && gatt_read_long(L.client->getConnId(), rmc->val_h) && s_rmap_len) m.concat((const char*)s_rmap, s_rmap_len);
  ph_memf("%s: tras leer el mapa (%u B)", L.tag, (unsigned)m.length());

  // Clasificacion: si el anuncio no decia el tipo, lo dicen las colecciones del descriptor. Uno de otro tipo no se queda en este hueco.
  if (!known && app_to_dev(L.cand_app) < 0 && m.length()) {
    const uint8_t kinds = report_map_kinds((const uint8_t*)m.c_str(), m.length());
    if (kinds && !(kinds & (1u << s))) {
      static const char* kn[DEV_N] = { "raton", "teclado", "mando" };
      int k = (kinds & 1) ? 0 : ((kinds & 2) ? 1 : 2);
      set_state(s, "%s es un %s, no un %s: descartado", L.devname, kn[k], L.tag);
#ifdef HAVE_BLE_STORE
      // el emparejado de este intento sobra: se borra (por su identidad y por la direccion con la que se conecto)
      if (ble_gap_conn_find(L.client->getConnId(), &desc) == 0) bonds_delete_val(desc.peer_id_addr.val);
      uint8_t v[6];
      if (parse_addr(L.devaddr, v)) bonds_delete_val(v);
#endif
      L.client->disconnect();
      reject_add(L.devaddr);
      L.devname[0] = 0; L.devaddr[0] = 0;
      return false;
    }
  }

  int nsub = 0;
  if (s == DEV_MOUSE)    nsub = subscribe_mouse(L, m);
  else if (s == DEV_KBD) nsub = subscribe_kbd(L, m);
  else                   nsub = subscribe_pad(L, m);

  if (!L.client->isConnected()) { set_state(s, "se desconecto al suscribirse a los informes (%s)", L.devname); return false; }
  if (!nsub) { set_state(s, "no se pudo suscribir a ningun informe"); L.client->disconnect(); return false; }
  if (s == DEV_KBD) {             // 15-30 ms (unidades de 1,25 ms): al teclear no se nota y el WiFi respira; raton y mando, lo que pidan ellos
    const bool up = L.client->updateConnParams(12, 24, 0, 400);
    ph_memf("teclado: intervalo 15-30 ms %s", up ? "pedido" : "NO pedido");
  }

#ifdef HAVE_BLE_STORE
  if (ble_gap_conn_find(L.client->getConnId(), &desc) == 0) {     // direccion de identidad del vinculo: para olvidarlo a el solo
    uint8_t ida[7]; ida[0] = desc.peer_id_addr.type; memcpy(ida + 1, desc.peer_id_addr.val, 6);
    if (!L.ida_ok || memcmp(ida, L.ida, 7) != 0) { memcpy(L.ida, ida, 7); L.ida_ok = true; nvs_save_ida(s); }
  }
#endif
  L.connected = true;
  L.conn_count++;
  L.pair_until = 0;                       // emparejado: la ventana se cierra
  nvs_save_addr(s, L.devaddr);
  if (L.devname[0] && strcmp(L.devname, "(sin nombre)") != 0) nvs_save_name(s, L.devname);
  L.heap_used = (int32_t)free0 - (int32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
  set_state(s, "conectado a %s (%s)", L.devname, L.mode);
  if (s_hid_task) xTaskNotifyGive(s_hid_task);
  return true;
}

static void ble_task(void*) {
  bool inited = false;
  uint32_t rssi_ms = 0;
  for (;;) {
    bool any = false;
    for (uint8_t s = 0; s < DEV_N; s++) {
      if (SL[s].enabled) { any = true; continue; }
      if (SL[s].client && SL[s].client->isConnected()) SL[s].client->disconnect();
      SL[s].connected = false;
      snprintf(SL[s].state, sizeof SL[s].state, "parado");
    }
    if (!any) { vTaskDelay(pdMS_TO_TICKS(300)); continue; }
    if (!inited) {
      BLEDevice::init("SD81");
      BLESecurity::setAuthenticationMode(true, false, true);            // bonding, sin MITM (lo pide el teclado si lo quiere), conexiones seguras
      BLESecurity::setCapability(ESP_IO_CAP_NONE);                      // "Just Works": los ratones no tienen pantalla ni teclado
      BLESecurity::setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
      BLESecurity::setRespEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
      BLESecurity::setPassKey(false);                                   // clave aleatoria para los teclados (DisplayOnly), la misma en
      BLESecurity::regenPassKeyOnConnect(false);                        // todos los intentos de una ventana: nueva con cada "pair"
#ifdef HAVE_BLE_STORE
      ble_gap_event_listener_register(&s_gap_listener, gap_listener, nullptr);
      {
        const int rr = ble_hs_pvcy_set_resolve_enabled(1);   // el controlador resuelve las direcciones privadas de los vinculos
        ph_memf("BLE: resolucion de direcciones privadas %s (rc %d)", rr == 0 ? "activada" : "NO activada", rr);
      }
#endif
      BLEDevice::setSecurityCallbacks(&s_sec_cb);
      inited = true;
      s_inited = true;
      bonds_pending_run();                                              // lo que se olvido o se empezo a emparejar con la pila parada
    }

    const uint32_t now = millis();
    uint8_t want = 0;
    bool any_conn = false, any_pair = false;
    const bool do_rssi = now - rssi_ms >= 1000;
    if (do_rssi) rssi_ms = now;
    for (uint8_t s = 0; s < DEV_N; s++) {
      Slot& L = SL[s];
      if (!L.enabled) continue;
      if (L.client && L.client->isConnected() && L.connected) {
        if (do_rssi) L.rssi = L.client->getRssi();
        any_conn = true;
        continue;
      }
      if (L.client && L.client->isConnected()) L.client->disconnect();   // a medias de una conexion fallida
      L.connected = false;
      if (!pairing(s) && !L.saved_addr[0]) { set_state(s, "sin %s guardado (esp ble %s pair para emparejar uno)", L.tag, L.word); continue; }
      if (pairing(s)) any_pair = true;
      if (L.next_try && (int32_t)(now - L.next_try) < 0) continue;
      want |= (uint8_t)(1u << s);
    }
    if (!want) { vTaskDelay(pdMS_TO_TICKS(300)); continue; }

    {
      static uint32_t mem_ref = 0;
      const uint32_t fr = heap_caps_get_free_size(MALLOC_CAP_8BIT);
      if (!mem_ref || fr > mem_ref + 8192) mem_ref = fr;
      else if (fr + 4096 < mem_ref) { ph_mem("baja la memoria libre", fr); mem_ref = fr; }
      if (fr < BLE_MIN_SCAN_HEAP) {                    // una busqueda copia cada anuncio que oye: con la memoria al limite, se espera
        for (uint8_t s = 0; s < DEV_N; s++)
          if (want & (1u << s)) set_state(s, "POCA MEMORIA: no se busca (la libre sale en esp ble)");   // sin el numero: no llena el historial
        vTaskDelay(pdMS_TO_TICKS(2000));
        continue;
      }
    }
    // Siempre la busqueda completa y la misma espera: con la "ligera" (ventana corta y cada 8 s si ya habia algo conectado) se escapaba el
    // anuncio corto con el que vuelve el Luna al encenderse
    (void)any_conn; (void)any_pair;
    scan_once(want, false);
    const uint32_t retry = BLE_RETRY_MS;
    for (uint8_t s = 0; s < DEV_N; s++) {
      if (!(want & (1u << s))) continue;
      Slot& L = SL[s];
      if (!L.enabled) continue;
      bool ok = false;
      if (!L.cand_ok) {
        if (pairing(s)) set_state(s, "no hay %s cerca (en modo emparejado?)", L.tag);
        else set_state(s, "el %s guardado no esta cerca (encendido?; HID vistos: %u)", L.tag, (unsigned)s_seen_n);
      } else {
        ok = connect_slot(s);
        if (!ok && L.client && L.client->isConnected()) L.client->disconnect();
      }
      L.next_try = ok ? 0 : (millis() + retry);
      if (!L.next_try && !ok) L.next_try = 1;
    }
  }
}

// ------------------------------------------------------------------------------------------------------------------------------------
// Tarea del teclado y del mando: KEYMAP_ZX81, CMD_KEYB y CMD_PAD (prioridad 5, como la del raton). Vigila tambien el plazo del emparejado.
// ------------------------------------------------------------------------------------------------------------------------------------
static bool send_keyb(const uint8_t rows[8], uint8_t flags) {
  uint8_t req[9];
  memcpy(req, rows, 8);
  req[8] = flags;
  WifiProtoResp r = wifi_client_request(CMD_KEYB, req, sizeof req);
  const bool ok = r.ok && r.len >= 1 && r.payload[0] == ST_OK;
  if (ok) SL[DEV_KBD].tx_ok++; else SL[DEV_KBD].tx_fail++;
  return ok;
}
static bool send_pad(const uint8_t p[9], uint8_t flags) {
  uint8_t req[10];                          // botones(2), cruceta, lx, ly, rx, ry, lt, rt, flags
  memcpy(req, p, 9);
  req[9] = flags;
  WifiProtoResp r = wifi_client_request(CMD_PAD, req, sizeof req);
  const bool ok = r.ok && r.len >= 1 && r.payload[0] == ST_OK;
  if (ok) SL[DEV_PAD].tx_ok++; else SL[DEV_PAD].tx_fail++;
  return ok;
}

static void hid_task(void*) {
  uint8_t rows[8] = { 0 };
  uint32_t kb_last = 0, pad_last = 0;
  bool kb_was = false, pad_was = false;
  uint8_t kb_release = 0, pad_release = 0;          // intentos que quedan de la trama de "todo suelto" tras desconectar
  for (;;) {
    const bool kc = SL[DEV_KBD].connected, pc = SL[DEV_PAD].connected;
    uint32_t wait = 100;
    if (kc) wait = BLE_KBD_TICK_MS; else if (pc || kb_release || pad_release) wait = 20;
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(wait));
    const uint32_t now = millis();
    secure_watchdog(now);

    // --- teclado ---
    int8_t lay = s_kbd_req_layout;
    if (lay >= 0) { s_kbd_req_layout = -1; keymap_set_layout((uint8_t)lay); }
    int8_t md = s_kbd_req_mode;
    if (md >= 0) { s_kbd_req_mode = -1; keymap_set_mode((uint8_t)md); }
    if (kc != kb_was) {                               // al conectar y al desconectar: todo suelto
      keymap_reset();
      if (s_kbd_q) xQueueReset(s_kbd_q);
      memset(rows, 0, sizeof rows);
      if (kc) { kb_last = now - BLE_HEARTBEAT_MS; kb_release = 0; }   // manda enseguida
      else kb_release = 5;
      kb_was = kc;
    }
    bool kchanged = false;
    KbdEvt e;
    while (s_kbd_q && xQueueReceive(s_kbd_q, &e, 0) == pdTRUE) {
      if (!kc) continue;
      keymap_report(e.mods, e.keys, e.n, e.t);
      if (keymap_tick(millis(), rows)) kchanged = true;
    }
    if (kc && keymap_tick(millis(), rows)) kchanged = true;
    s_kbd_mode = keymap_mode();
    if (kc && (kchanged || now - kb_last >= BLE_HEARTBEAT_MS)) {
      const uint8_t flags = 0x01 | (s_kbd_mode == KM_MODE_GAME ? 0x02 : 0x00);
      if (send_keyb(rows, flags)) { kb_last = now; memcpy(s_kbd_rows, rows, 8); }
      else kb_last = now - BLE_HEARTBEAT_MS + 20;     // reintenta en 20 ms
    } else if (!kc && kb_release) {
      const uint8_t z[8] = { 0 };
      if (send_keyb(z, 0)) kb_release = 0; else kb_release--;
      memset(s_kbd_rows, 0, 8);
    }

    // --- mando ---
    if (pc != pad_was) {
      portENTER_CRITICAL(&s_mux); memset(s_pad_now, 0, sizeof s_pad_now); s_pad_dirty = false; portEXIT_CRITICAL(&s_mux);
      if (pc) { pad_last = now - BLE_HEARTBEAT_MS; pad_release = 0; } else pad_release = 5;
      pad_was = pc;
    }
    if (pc) {
      uint8_t p[9]; bool dirty;
      portENTER_CRITICAL(&s_mux); memcpy(p, s_pad_now, sizeof p); dirty = s_pad_dirty; s_pad_dirty = false; portEXIT_CRITICAL(&s_mux);
      if (dirty || now - pad_last >= BLE_HEARTBEAT_MS) {
        if (send_pad(p, 0x01)) pad_last = now;
        else { pad_last = now - BLE_HEARTBEAT_MS + 20; }
      }
    } else if (pad_release) {
      const uint8_t z[9] = { 0 };
      if (send_pad(z, 0)) pad_release = 0; else pad_release--;
    }
  }
}

void mouse_ble_begin() {
  ph_begin();
  slots_init();
  for (uint8_t s = 0; s < DEV_N; s++) { nvs_load(s); s_cli_cb[s].dev = s; }
  s_kbd_req_layout = (int8_t)s_kbd_layout;
  s_kbd_q = xQueueCreate(16, sizeof(KbdEvt));
  xTaskCreate(tx_task, "mouse_tx", 6144, nullptr, 5, &s_tx_task);
  xTaskCreate(hid_task, "ble_hid_tx", 4096, nullptr, 5, &s_hid_task);   // la RAM va justa: no mas pila de la necesaria
  xTaskCreate(ble_task, "mouse_ble", 8192, nullptr, 4, nullptr);
  Serial.printf("BLE raton: %s, teclado: %s, mando: %s (esp ble [mouse|kbd|pad] on|off en la consola web)\n",
                SL[DEV_MOUSE].enabled ? "activo" : "parado", SL[DEV_KBD].enabled ? "activo" : "parado", SL[DEV_PAD].enabled ? "activo" : "parado");
}

// ------------------------------------------------------------------------------------------------------------------------------------
// Orden "esp ble ..." de la consola web
// ------------------------------------------------------------------------------------------------------------------------------------
static String slot_status(uint8_t s, bool detail = false) {
  Slot& L = SL[s];
  String r;
  r += String("BLE ") + L.tag + ": " + (L.enabled ? "ACTIVO" : "parado") + " - " + L.state + "\n";
  if (L.devname[0]) r += String("  dispositivo: ") + L.devname + " " + L.devaddr + (L.connected ? "  RSSI " + String(L.rssi) + " dBm" : "") + "\n";
  if (L.connected) r += String("  modo: ") + L.mode + " (" + L.layout_txt + ")\n";
  if (L.connected) r += String("  memoria que se llevo al conectar: ") + L.heap_used + " B\n";
  if (s == DEV_MOUSE) {
    r += "  conexiones: " + String(L.conn_count) + "  notificaciones: " + String(s_notif) + "  tramas al STM32: ok " + String(s_tx_ok) + ", fallo " + String(s_tx_fail) + "\n";
    uint32_t n, sum, mx;
    portENTER_CRITICAL(&s_mux); n = s_lat_n; sum = s_lat_sum; mx = s_lat_max; portEXIT_CRITICAL(&s_mux);
    if (n) r += "  latencia notificacion->STM32: media " + String(sum / n) + " us, maxima " + String(mx) + " us (" + String(n) + " tramas)\n";
    if (s_st_valid) r += "  STM32: x=" + String(s_st_x) + " y=" + String(s_st_y) + " botones=" + String(s_st_btn) + "\n";
  } else {
    r += "  conexiones: " + String(L.conn_count) + "  informes: " + String(L.notif) + "  tramas al STM32: ok " + String(L.tx_ok) + ", fallo " + String(L.tx_fail) + "\n";
  }
  if (s == DEV_KBD) {
    r += String("  modo del ZX81: ") + (s_kbd_mode == KM_MODE_GAME ? "juego" : "texto") + " (Ctrl+Alt+Espacio lo cambia), distribucion: " +
         (s_kbd_layout == KM_LAYOUT_US ? "US" : "espanola") + "\n";
    if (s_kbd_phantom || s_kbd_qfull) r += "  informes fantasma (demasiadas teclas): " + String(s_kbd_phantom) + ", cola llena: " + String(s_kbd_qfull) + "\n";
    if (L.connected) {
      char t[48]; snprintf(t, sizeof t, "  matriz: %02X %02X %02X %02X %02X %02X %02X %02X\n",
                           s_kbd_rows[0], s_kbd_rows[1], s_kbd_rows[2], s_kbd_rows[3], s_kbd_rows[4], s_kbd_rows[5], s_kbd_rows[6], s_kbd_rows[7]);
      r += t;
    }
    uint32_t k = s_passkey, k_ms = s_passkey_ms;
    if (k) {
      uint32_t el = k_ms ? (millis() - k_ms) / 1000 : 0;
      char t[112]; snprintf(t, sizeof t, "  CLAVE %06lu: tecleala en el teclado y pulsa Enter (quedan %u s)\n", (unsigned long)k,
                            (unsigned)(el < BLE_SMP_TIMEOUT_S ? BLE_SMP_TIMEOUT_S - el : 0));
      r += t;
    }
  }
  if (s == DEV_PAD && L.connected) {
    uint8_t p[9];
    portENTER_CRITICAL(&s_mux); memcpy(p, s_pad_now, sizeof p); portEXIT_CRITICAL(&s_mux);
    char t[112];
    snprintf(t, sizeof t, "  botones %04X cruceta %X  izq %d,%d  der %d,%d  gatillos %u,%u\n", (unsigned)(p[0] | (p[1] << 8)), (unsigned)p[2],
             (int)(int8_t)p[3], (int)(int8_t)p[4], (int)(int8_t)p[5], (int)(int8_t)p[6], (unsigned)p[7], (unsigned)p[8]);
    r += t;
    const uint16_t raw = s_pad_raw;
    String hb;
    for (uint8_t k = 0; k < 16; k++) if (raw & (1u << k)) hb += String(" ") + (k + 1);
    r += String("  botones HID pulsados ahora (sin traducir):") + (hb.length() ? hb : String(" ninguno")) + "\n";
    uint8_t lr[sizeof s_pad_last]; uint8_t ln;
    portENTER_CRITICAL(&s_mux); ln = s_pad_last_n; memcpy(lr, s_pad_last, ln); portEXIT_CRITICAL(&s_mux);
    String hx;
    for (uint8_t k = 0; k < ln; k++) { char b3[4]; snprintf(b3, sizeof b3, " %02X", lr[k]); hx += b3; }
    r += String("  ultimo informe en bruto (") + ln + " B):" + hx + "\n";
    PadRep reps[6]; uint8_t nrep;
    portENTER_CRITICAL(&s_mux); nrep = s_pad_rep_n; memcpy(reps, s_pad_rep, sizeof reps); portEXIT_CRITICAL(&s_mux);
    for (uint8_t i = 0; i < nrep; i++) {
      String h2;
      for (uint8_t k = 0; k < reps[i].n; k++) { char b3[4]; snprintf(b3, sizeof b3, " %02X", reps[i].d[k]); h2 += b3; }
      r += String("  informe id ") + reps[i].rid + " (" + reps[i].count + " veces):" + h2 + "\n";
    }
  }
  if (L.saved_addr[0]) r += String("  guardado: ") + L.saved_addr + (L.saved_name[0] ? String(" (") + L.saved_name + ")" : String("")) + "\n";
  else r += "  guardado: ninguno\n";
  if (L.last_pair[0]) r += String("  ultimo emparejado: ") + L.last_pair + "\n";
  if (detail) {                                         // los ultimos estados de este dispositivo, del mas viejo al mas nuevo (tambien de antes de reiniciar)
    String h;
    const uint32_t total = s_ph.n, n = total < BLE_PH_N ? total : BLE_PH_N;
    for (uint32_t i = 0; i < n; i++) {
      BlePhEntry e;
      portENTER_CRITICAL(&s_hist_mux);                  // se copia con la seccion critica y se formatea fuera
      e = s_ph.e[(total - n + i) % BLE_PH_N];
      portEXIT_CRITICAL(&s_hist_mux);
      if (e.dev != s && e.dev != 3) continue;
      e.txt[sizeof e.txt - 1] = 0;
      char line[112];
      const uint32_t ago = s_ph.boot - e.boot;
      if (ago) snprintf(line, sizeof line, "    %6lu  [%3u/%3u KB]  %s   (%lu arranque%s antes)\n", (unsigned long)(e.ms / 1000), (unsigned)e.heap_kb,
                        (unsigned)e.min_kb, e.txt, (unsigned long)ago, ago == 1 ? "" : "s");
      else snprintf(line, sizeof line, "    %6lu  [%3u/%3u KB]  %s\n", (unsigned long)(e.ms / 1000), (unsigned)e.heap_kb, (unsigned)e.min_kb, e.txt);
      h += line;
    }
    if (h.length()) { r += "  historial (segundos desde su arranque, [memoria libre/minima]):\n"; r += h; }
    r += String("  ESP32: arrancado hace ") + (millis() / 1000) + " s; motivo del ultimo reinicio: " + reset_text(esp_reset_reason()) + "\n";
  }
  if (pairing(s)) r += "  EMPAREJANDO: la ventana se cierra en " + String(pair_left_s(s)) + " s\n";
  return r;
}

// El ultimo fallo del programa guardado en la flash (particion coredump): tarea, PC, RA, causa y las palabras de su pila que parecen direcciones
// de codigo. Con el .elf de ESE firmware, riscv32-esp-elf-addr2line da la linea. full = false: solo una linea para "esp ble".
static String crash_text(bool full) {
#ifdef HAVE_CRASH_SUMMARY
  static esp_core_dump_summary_t sum;                  // ~1,1 KB: estatico para no cargar la pila de quien llame
  if (esp_core_dump_image_check() != ESP_OK || esp_core_dump_get_summary(&sum) != ESP_OK) return full ? String("no hay ningun fallo guardado en la flash\n") : String("");
  sum.exc_task[sizeof sum.exc_task - 1] = 0;
  char t[200];
  snprintf(t, sizeof t, "fallo guardado: tarea \"%s\", PC 0x%08lX, RA 0x%08lX, SP 0x%08lX, mcause 0x%lX, mtval 0x%08lX, elf %.16s\n", sum.exc_task,
           (unsigned long)sum.exc_pc, (unsigned long)sum.ex_info.ra, (unsigned long)sum.ex_info.sp, (unsigned long)sum.ex_info.mcause,
           (unsigned long)sum.ex_info.mtval, (const char*)sum.app_elf_sha256);
  String r = String("  ") + t;
  if (!full) return r;
  snprintf(t, sizeof t, "  a0-a7: %08lX %08lX %08lX %08lX %08lX %08lX %08lX %08lX\n", (unsigned long)sum.ex_info.exc_a[0], (unsigned long)sum.ex_info.exc_a[1],
           (unsigned long)sum.ex_info.exc_a[2], (unsigned long)sum.ex_info.exc_a[3], (unsigned long)sum.ex_info.exc_a[4], (unsigned long)sum.ex_info.exc_a[5],
           (unsigned long)sum.ex_info.exc_a[6], (unsigned long)sum.ex_info.exc_a[7]);
  r += t;
  r += "  pila (palabras que parecen direcciones de codigo, de la cima hacia abajo):\n   ";
  uint32_t n = sum.exc_bt_info.dump_size / 4, shown = 0;
  if (n > sizeof sum.exc_bt_info.stackdump / 4) n = sizeof sum.exc_bt_info.stackdump / 4;
  for (uint32_t i = 0; i < n && shown < 40; i++) {
    uint32_t w; memcpy(&w, sum.exc_bt_info.stackdump + 4 * i, 4);
    const bool code = (w >= 0x42000000u && w < 0x42800000u) || (w >= 0x40380000u && w < 0x403E0000u);   // flash (IROM) o IRAM del C3
    if (!code) continue;
    snprintf(t, sizeof t, " %08lX", (unsigned long)w);
    r += t;
    if (++shown % 8 == 0) r += "\n   ";
  }
  r += "\n  (para borrarlo: esp ble crash clear)\n";
  return r;
#else
  (void)full;
  return full ? String("este firmware no guarda los fallos en la flash\n") : String("");
#endif
}

static String sniff_text() {
  if (s_sniff_state == 1) return "escucha pedida: empezara con la proxima busqueda (solo se busca si algun dispositivo activado no esta conectado)\n";
  if (s_sniff_state == 2) return "escuchando: la busqueda dura " + String(MOUSE_BLE_SCAN_S) + " s; vuelve a pedir esp ble sniff show\n";
  if (s_sniff_state != 3) return "esp ble sniff: escucha la proxima busqueda y apunta todos los anuncios (mueve el raton, pulsa teclas...); luego esp ble sniff show\n";
  static const char* ev[5] = { "anuncio", "DIRIGIDO", "escaneable", "no conectable", "respuesta" };
  String r = "anuncios oidos en la ultima busqueda (tipo de direccion: 0 publica, 1 aleatoria, 2/3 resuelta):\n";
  for (uint8_t i = 0; i < s_sniff_n; i++) {
    const SniffEnt& e = s_sniff[i];
    String evs;
    for (uint8_t k = 0; k < 5; k++) if (e.evmask & (1u << k)) { if (evs.length()) evs += "+"; evs += ev[k]; }
    const bool rpa = e.addr.type == BLE_ADDR_RANDOM && (e.addr.val[5] & 0xC0) == 0x40;
    char t[160];
    snprintf(t, sizeof t, "  %s tipo %u%s %4d dBm x%u [%s] banderas %02X%s%s%s%s \"%s\"\n", BLEAddress(e.addr).toString().c_str(), (unsigned)e.addr.type,
             rpa ? " (PRIVADA)" : "", (int)e.rssi, (unsigned)e.count, evs.c_str(), (unsigned)e.flags, e.hid ? " HID" : "",
             e.app ? " apariencia " : "", e.app ? String(e.app, HEX).c_str() : "", "", e.name);
    r += t;
  }
  if (s_sniff_lost) r += "  (y " + String(s_sniff_lost) + " mas que no cabian)\n";
  return r;
}

static String status_text() {
  String r;
  r += String("BLE: direcciones privadas oidas en la ultima busqueda: ") + s_rpa_seen + ", resueltas con una IRK guardada: " + s_rpa_resolved +
       " (IRK guardadas: " + s_irk_n + ")\n";
  r += String("ESP32: arrancado hace ") + (millis() / 1000) + " s; motivo del ultimo reinicio: " + reset_text(esp_reset_reason()) + "\n";
  r += String("  memoria libre: ") + (unsigned long)heap_caps_get_free_size(MALLOC_CAP_8BIT) + " B (minima desde el arranque " +
       (unsigned long)heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT) + ", bloque mayor " + (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) +
       "; para conectar otro dispositivo hacen falta " + (unsigned long)BLE_MIN_FREE_HEAP + ")\n";
  if (esp_reset_reason() == ESP_RST_PANIC) r += crash_text(false);
  bool searching = false;
  for (uint8_t s = 0; s < DEV_N; s++) {
    r += slot_status(s);
    if (SL[s].enabled && !SL[s].connected) searching = true;
  }
  if (searching) {                                      // diagnostico: lo que anuncia lo que hay cerca (si el guardado no sale aqui, no lo oye)
    for (uint8_t i = 0; i < s_seen_n; i++) {
      const SeenDev& v = s_seen[i];
      int8_t k = app_to_dev(v.app);
      r += String("  visto: ") + v.addr + " tipo " + v.type + " " + v.rssi + " dBm \"" + v.name + "\"" + (v.hid ? " HID" : "") +
           (k == DEV_MOUSE ? " raton" : (k == DEV_KBD ? " teclado" : (k == DEV_PAD ? " mando" : ""))) +
           ((v.flags & (BLE_HS_ADV_F_DISC_LTD | BLE_HS_ADV_F_DISC_GEN)) ? " EMPAREJABLE" : (v.flags ? " (no emparejable)" : "")) +
           (v.dir ? " DIRIGIDO a nosotros" : "") + "\n";
    }
  }
#ifdef HAVE_BLE_STORE
  int nb = s_inited ? bonds_count() : -1;
#ifndef CONFIG_BT_NIMBLE_MAX_BONDS
#define CONFIG_BT_NIMBLE_MAX_BONDS 3
#endif
  if (nb >= 0) r += "  vinculos guardados en la pila BLE: " + String(nb) + " de " + String(CONFIG_BT_NIMBLE_MAX_BONDS) + "\n";
#endif
  return r;
}

static void slot_disconnect(uint8_t s) {
  if (SL[s].client && SL[s].client->isConnected()) SL[s].client->disconnect();
  SL[s].connected = false;
}

static String slot_console(uint8_t s, const String& a) {
  Slot& L = SL[s];
  if (a == "" || a == "status") return slot_status(s, true);
  if (a == "on") {
    L.enabled = true; nvs_save_enabled(s); L.next_try = 0;
    if (!L.saved_addr[0]) return String("BLE ") + L.tag + " activado (se guarda), pero no hay ningun " + L.tag + " guardado: esp ble " + L.word + " pair para emparejar uno\n";
    return String("BLE ") + L.tag + " activado (se guarda): se conecta al " + L.tag + " guardado " + L.saved_addr + "\n";
  }
  if (a == "off") { L.enabled = false; nvs_save_enabled(s); return String("BLE ") + L.tag + " parado (se guarda). El " + L.tag + " guardado sigue guardado\n"; }
  if (a == "connect") {
    if (!L.saved_addr[0]) return String("no hay ningun ") + L.tag + " guardado: esp ble " + L.word + " pair para emparejar uno\n";
    L.enabled = true; nvs_save_enabled(s); L.next_try = 0;
    if (L.connected) return "ya esta conectado\n";
    return String("buscando el ") + L.tag + " guardado " + L.saved_addr + " (enciendelo; mira esp ble)\n";
  }
  if (a == "pair" || a.startsWith("pair ")) {
    int secs = 60;
    if (a.length() > 5) secs = a.substring(5).toInt();
    if (secs < 10) secs = 10;
    if (secs > 300) secs = 300;
    // Uno nuevo sustituye al guardado: se desconecta y se olvida el anterior (y su vinculo, y los huerfanos; NO los de los otros huecos)
    slot_disconnect(s);
    bonds_forget_slot(s, 1);
    nvs_forget_all(s);
    L.devname[0] = 0; L.devaddr[0] = 0;
    L.enabled = true; nvs_save_enabled(s);
    s_rej_n = 0;
    L.next_try = 0;
    L.last_pair[0] = 0;
    if (s == DEV_KBD) BLESecurity::setPassKey(false);   // clave nueva para esta ventana (la misma en todos sus intentos)
    L.pair_until = millis() + (uint32_t)secs * 1000UL;
    if (!L.pair_until) L.pair_until = 1;
    String hint;
    if (s == DEV_MOUSE) hint = "pon el raton en modo emparejado (suele ser un boton debajo)";
    else if (s == DEV_KBD) hint = "pon el teclado en modo emparejado (Logitech: manten pulsada 3 s la tecla de un canal Easy-Switch); si pide clave, sale aqui (esp ble kbd)";
    else hint = "pon el mando en modo emparejado";
    return String("emparejando durante ") + secs + " s: " + hint + " y mira esp ble. Sustituye al anterior\n";
  }
  if (a == "forget") {
    slot_disconnect(s);
    L.pair_until = 0;
    const int del = bonds_forget_slot(s, 0);            // solo el vinculo de este hueco: los otros dos siguen emparejados
    nvs_forget_all(s);
    L.devname[0] = 0; L.devaddr[0] = 0;
#ifdef HAVE_BLE_STORE
    if (del < 0) return String("olvidado el ") + L.tag + " guardado; su vinculo se borrara al arrancar el BLE. esp ble " + L.word + " pair para emparejar otro\n";
    return String("olvidado el ") + L.tag + " guardado y su vinculo (" + del + " borrado). esp ble " + L.word + " pair para emparejar otro\n";
#else
    (void)del;
    return "olvidada la direccion guardada (los emparejados de la pila BLE no se pueden borrar desde aqui)\n";
#endif
  }
  if (s == DEV_KBD) {
    if (a == "text" || a == "texto") { s_kbd_req_mode = KM_MODE_TEXT; if (s_hid_task) xTaskNotifyGive(s_hid_task); return "teclado en modo texto (una tecla cada vez, en cola)\n"; }
    if (a == "game" || a == "juego") { s_kbd_req_mode = KM_MODE_GAME; if (s_hid_task) xTaskNotifyGive(s_hid_task); return "teclado en modo juego (la matriz tal cual)\n"; }
    if (a == "layout es" || a == "layout us" || a == "es" || a == "us") {
      s_kbd_layout = a.endsWith("us") ? KM_LAYOUT_US : KM_LAYOUT_ES;
      nvs_save_layout();
      s_kbd_req_layout = (int8_t)s_kbd_layout;
      if (s_hid_task) xTaskNotifyGive(s_hid_task);
      return String("distribucion del teclado: ") + (s_kbd_layout == KM_LAYOUT_US ? "US" : "espanola") + " (se guarda)\n";
    }
  }
  String help = String("esp ble ") + L.word + ": pair [s], connect, forget, on, off o nada (estado)";
  if (s == DEV_KBD) help += "; text|game (modo del ZX81), layout es|us";
  return help + "\n";
}

// CMD_BLE: el ESP32 cuenta el estado de los tres al STM32 (para que el Z80/BASIC lo pueda leer) y recoge UNA orden pendiente (comandos 77
// y 78 del MCU). El STM32 no puede iniciar tramas, por eso se pregunta desde aqui. Ver WIFI_PROTOCOL.h.
static bool ble_stm_sync_once() {
  static uint32_t last_ms = 0;
  static uint32_t last_sig = 0xFFFFFFFFu;
  static uint32_t last_key = 0;
  uint8_t req[9];
  uint32_t sig = 0;
  for (uint8_t s = 0; s < DEV_N; s++) {
    uint8_t st = 0;
    if (SL[s].connected)     st |= 0x01;
    if (pairing(s))         st |= 0x02;
    if (SL[s].enabled)       st |= 0x04;
    if (SL[s].saved_addr[0]) st |= 0x08;
    uint32_t left = pair_left_s(s);
    req[2 * s] = st;
    req[2 * s + 1] = (uint8_t)(left > 255 ? 255 : left);
    sig |= (uint32_t)st << (8 * s);
  }
  const uint32_t key = s_passkey & 0xFFFFFF;
  req[6] = (uint8_t)key; req[7] = (uint8_t)(key >> 8); req[8] = (uint8_t)(key >> 16);
  const uint32_t now = millis();
  if (sig == last_sig && key == last_key && now - last_ms < 500) return false;
  last_ms = now;
  last_sig = sig;
  last_key = key;
  WifiProtoResp r = wifi_client_request(CMD_BLE, req, sizeof req);
  if (!r.ok || r.len < 3 || r.payload[0] != ST_OK) { last_sig = 0xFFFFFFFFu; return false; }   // no contesto (version vieja del STM32?): reintenta en la proxima
  const uint8_t order = r.payload[1], arg = r.payload[2];
  const uint8_t dev = r.len >= 4 ? r.payload[3] : DEV_MOUSE;                            // un STM32 viejo contesta 3 bytes: raton
  if (dev >= DEV_N) return false;
  String cmd;
  switch (order) {
    case 1: cmd = arg ? String("pair ") + arg : String("pair"); break;
    case 2: cmd = "connect"; break;
    case 3: cmd = "forget"; break;
    case 4: cmd = "on"; break;
    case 5: cmd = "off"; break;
    case 6: if (dev == DEV_KBD) cmd = "text"; break;
    case 7: if (dev == DEV_KBD) cmd = "game"; break;
    case 8: if (dev == DEV_KBD) cmd = "layout es"; break;     // distribucion del teclado (LOAD *KEYB ES / US, op 8 del 78)
    case 9: if (dev == DEV_KBD) cmd = "layout us"; break;
    default: break;
  }
  if (!cmd.length()) return false;
  Serial.printf("BLE %s: orden del Z80: esp ble %s %s\n", SL[dev].tag, SL[dev].word, cmd.c_str());
  slot_console(dev, cmd);
  last_sig = 0xFFFFFFFFu;                               // el estado habra cambiado: se cuenta enseguida
  return true;
}
// El STM32 entrega UNA orden por trama (por turno entre dispositivos): si ha llegado una, se pregunta otra vez enseguida
static void ble_stm_sync() {
  for (uint8_t i = 0; i < 4 && ble_stm_sync_once(); i++) {}
}

String mouse_ble_console(const String& args) {
  String a = args; a.trim(); a.toLowerCase();
  if (a == "" || a == "status") return status_text();
  if (a == "crash") return crash_text(true);
  if (a == "sniff") { s_sniff_state = 1; return sniff_text(); }
  if (a == "sniff show") return sniff_text();
#ifdef HAVE_CRASH_SUMMARY
  if (a == "crash clear") { esp_core_dump_image_erase(); return "volcado del fallo borrado de la flash\n"; }
#endif
  if (a == "reset") {
    portENTER_CRITICAL(&s_mux); s_lat_n = 0; s_lat_sum = 0; s_lat_max = 0; portEXIT_CRITICAL(&s_mux);
    s_notif = 0; s_tx_ok = 0; s_tx_fail = 0;
    for (uint8_t s = 0; s < DEV_N; s++) { SL[s].notif = 0; SL[s].tx_ok = 0; SL[s].tx_fail = 0; }
    s_kbd_phantom = 0; s_kbd_qfull = 0; s_kbd_reports = 0;
    return "estadisticas a cero\n";
  }
  // Primera palabra: el dispositivo. Sin ella, el raton (como antes: "esp ble pair" sigue siendo el raton)
  int sp = a.indexOf(' ');
  String w = sp < 0 ? a : a.substring(0, sp);
  String rest = sp < 0 ? String("") : a.substring(sp + 1);
  rest.trim();
  int8_t dev = -1;
  if (w == "mouse" || w == "raton") dev = DEV_MOUSE;
  else if (w == "kbd" || w == "keyb" || w == "teclado") dev = DEV_KBD;
  else if (w == "pad" || w == "mando" || w == "gamepad") dev = DEV_PAD;
  if (dev >= 0) return slot_console((uint8_t)dev, rest);
  if (a == "on" || a == "off" || a == "connect" || a == "forget" || a == "pair" || a.startsWith("pair ")) return slot_console(DEV_MOUSE, a);
  return "esp ble: [mouse|kbd|pad] pair [s], connect, forget, on, off; reset; o nada (estado de los tres). Sin dispositivo = raton\n";
}

#endif
