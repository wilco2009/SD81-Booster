// Sustitutos minimos de Arduino / ESP32 / mbedTLS / WIFI_CLIENT / NET_BRIDGE para compilar
// NET_MQTT.cpp en el PC y probarlo contra un broker falso (test_net_mqtt.cpp).
// build.sh copia NET_MQTT.cpp, NET_MQTT.h y mqtt_codec.h junto a este fichero y a los
// "cabeceras" falsas, para que los #include "..." se resuelvan aqui.
#ifndef MOCK_H
#define MOCK_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <ctype.h>
#include <string>
#include <vector>
#include <deque>

#define __has_include_mock 1

// --- Arduino ---
extern uint32_t mock_now;
static inline uint32_t millis() { return mock_now; }
static inline void delay(uint32_t ms) { mock_now += ms; }

class String {
public:
  String() {}
  String(const char* s) : s_(s) {}
  size_t length() const { return s_.size(); }
  int indexOf(char c, int from = 0) const { size_t p = s_.find(c, from); return p == std::string::npos ? -1 : (int)p; }
  String substring(int a, int b) const { return String(s_.substr(a, b - a).c_str()); }
  void trim() {
    size_t a = s_.find_first_not_of(" \t\r\n");
    size_t b = s_.find_last_not_of(" \t\r\n");
    s_ = (a == std::string::npos) ? "" : s_.substr(a, b - a + 1);
  }
  char operator[](int i) const { return s_[i]; }
  const char* c_str() const { return s_.c_str(); }
  std::string s_;
};

struct SerialMock {
  template <typename... A> void printf(const char* f, A... a) { ::printf(f, a...); }
  void println(const char* s) { ::printf("%s\n", s); }
};
extern SerialMock Serial;

class Client {
public:
  virtual ~Client() {}
  virtual int available() = 0;
  virtual int read(uint8_t* b, size_t n) = 0;
  virtual size_t write(const uint8_t* b, size_t n) = 0;
  virtual void stop() = 0;
  virtual uint8_t connected() = 0;
};

enum { WL_CONNECTED = 3, WL_DISCONNECTED = 6 };
struct WiFiMock { int status() { return mock_wifi_ok ? WL_CONNECTED : WL_DISCONNECTED; } static bool mock_wifi_ok; };
extern WiFiMock WiFi;

// Broker falso: lo que escribe el cliente va a mock_broker_rx() (lo define la prueba). Cada
// objeto sabe si el abrio la conexion: el WiFiClient del puente (ATDT) nunca esta conectado.
extern void mock_broker_rx(const uint8_t* b, size_t n);
extern std::deque<uint8_t> mock_from_broker;
extern bool mock_connected, mock_tcp_fail;
extern bool mock_quiet;
class WiFiClient : public Client {
public:
  bool conn = false;
  int available() override { return conn ? (int)mock_from_broker.size() : 0; }
  int read(uint8_t* b, size_t n) override {
    size_t k = 0;
    while (conn && k < n && !mock_from_broker.empty()) { b[k++] = mock_from_broker.front(); mock_from_broker.pop_front(); }
    return (int)k;
  }
  size_t write(const uint8_t* b, size_t n) override {
    if (!conn || !mock_connected) return 0;
    mock_broker_rx(b, n);
    return n;
  }
  void stop() override { if (conn) { conn = false; mock_connected = false; } }
  uint8_t connected() override { return conn && mock_connected; }
  int connect(const char* host, uint16_t port, int32_t timeout = 0) {
    (void)timeout;
    if (mock_tcp_fail) return 0;
    conn = true; mock_connected = true; mock_from_broker.clear();
    if (!mock_quiet) printf("    (tcp a %s:%u)\n", host, port);
    return 1;
  }
  void setNoDelay(bool) {}
};
class WiFiClientSecure : public WiFiClient {
public:
  void setInsecure() {}
};

// --- esp ---
static inline uint32_t esp_random() { static uint32_t x = 0x1234ABCD; x = x * 1664525u + 1013904223u; return x; }
static inline void esp_fill_random(void* b, size_t n) { for (size_t i = 0; i < n; i++) ((uint8_t*)b)[i] = (uint8_t)(esp_random() >> 24); }

// --- mbedtls: por defecto solo comprueba que se llama con los argumentos buenos; con
// MOCK_REAL_MD la prueba pone SHA-256 y HMAC de verdad ---
typedef int mbedtls_md_type_t;
typedef struct { int dummy; } mbedtls_md_info_t;
#define MBEDTLS_MD_SHA256 6
static inline const mbedtls_md_info_t* mbedtls_md_info_from_type(int) { static mbedtls_md_info_t i; return &i; }
#ifdef MOCK_REAL_MD
int mbedtls_md(const mbedtls_md_info_t*, const unsigned char* in, size_t n, unsigned char* out);
int mbedtls_md_hmac(const mbedtls_md_info_t*, const unsigned char* key, size_t kl,
                    const unsigned char* in, size_t n, unsigned char* out);
#else
extern std::vector<uint8_t> mock_md_in, mock_md_key;
static inline int mbedtls_md(const mbedtls_md_info_t*, const unsigned char* in, size_t n, unsigned char* out) {
  mock_md_in.assign(in, in + n); mock_md_key.clear();
  for (int i = 0; i < 32; i++) out[i] = (uint8_t)(0xA0 + i);
  return 0;
}
static inline int mbedtls_md_hmac(const mbedtls_md_info_t*, const unsigned char* key, size_t kl,
                                  const unsigned char* in, size_t n, unsigned char* out) {
  mock_md_key.assign(key, key + kl); mock_md_in.assign(in, in + n);
  for (int i = 0; i < 32; i++) out[i] = (uint8_t)(0x10 + i);
  return 0;
}
#endif

// --- WIFI_CLIENT ---
extern bool mock_cfg_present; extern std::string mock_cfg;
static inline bool wifi_client_read_text_file(const char*, String* out) {
  if (!mock_cfg_present) return false;
  *out = String(mock_cfg.c_str());
  return true;
}

// --- NET_BRIDGE: cola de salida y datos crudos ---
extern std::string mock_txq;
extern uint16_t mock_txq_room;
extern uint16_t mock_raw_len;
extern bool mock_raw_pending;
typedef void (*net_raw_done_t)(const uint8_t* data, uint16_t len, bool ok);
extern net_raw_done_t mock_raw_cb;


#endif
