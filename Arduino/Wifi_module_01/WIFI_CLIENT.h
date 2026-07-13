#ifndef WIFI_CLIENT_H
#define WIFI_CLIENT_H

#include <Arduino.h>
#include "WIFI_PROTOCOL.h"

// Cliente del protocolo UART hacia el STM32 (SD81 Booster). El ESP32 es
// SIEMPRE el iniciador; el STM32 solo responde. UART1 dedicado, GPIO4(RX)/
// GPIO5(TX) por defecto - NO usa el UART0 (compartido con el USB de
// programacion/depuracion).

struct WifiProtoResp {
  bool     ok;       // false = fallo de transporte (timeout tras reintentos)
  uint8_t  raw_cmd;  // CMD devuelto (para detectar CMD_FRAME_ERROR)
  uint8_t  payload[WIFI_PROTO_MAX_FRAME_PAYLOAD];
  uint16_t len;
};

struct WifiDirEntry {
  char     name[64];
  uint32_t size;
  bool     is_dir;
};

struct WifiNetwork {
  char ssid[WIFI_PROTO_MAX_SSID + 1];
  char pass[WIFI_PROTO_MAX_PASS + 1];
};

void wifi_client_init();

// Peticion generica (bajo nivel) - usar las de mas arriba cuando encajen.
WifiProtoResp wifi_client_request(uint8_t cmd, const uint8_t* payload, uint16_t len);

bool wifi_client_ping(uint8_t* out_fw_version);
bool wifi_client_list_dir(const char* path, void (*on_entry)(const WifiDirEntry&));
bool wifi_client_write_open(const char* path, uint8_t* out_handle);
bool wifi_client_write_chunk(uint8_t handle, const uint8_t* data, uint16_t len);
bool wifi_client_write_close(uint8_t handle, uint32_t* out_total);
bool wifi_client_read_open(const char* path, uint8_t* out_handle, uint32_t* out_size);
bool wifi_client_read_chunk(uint8_t handle, uint32_t offset, uint8_t* buf, uint16_t* out_len, bool* out_eof);
bool wifi_client_read_close(uint8_t handle);
bool wifi_client_delete(const char* path);
bool wifi_client_mkdir(const char* path);

// Descarga /SYS/WIFI.CFG (via READ_OPEN/CHUNK/CLOSE, como cualquier otro
// fichero - el STM32 no sabe nada de "redes WiFi") y lo interpreta aqui
// mismo: pares de lineas SSID/password, repetidos uno tras otro. Rellena
// `networks` (hasta max_networks entradas) en el mismo orden del fichero.
// *out_count es cuantas se guardaron. Devuelve false solo si fallo el
// transporte al pedir el fichero - que no exista o este vacio da
// *out_count=0 con return true, no es un error.
bool wifi_client_read_wifi_networks(WifiNetwork* networks, uint8_t max_networks, uint8_t* out_count);

#endif
