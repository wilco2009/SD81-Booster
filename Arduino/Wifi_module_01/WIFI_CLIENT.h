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

struct NtpConfig {
  bool    sync_enabled;      // MODE=SERVER en /SYS/NTP.CFG (false = MODE=LOCAL, fichero ausente, o valor no reconocido)
  char    server[64];
  int     utc_offset_hours;  // UTCOFFSET= - horas enteras respecto a UTC, puede ser negativo
  bool    dst;               // DST=1 - suma +1h extra, interruptor manual del usuario
};

void wifi_client_init();

// Peticion generica (bajo nivel) - usar las de mas arriba cuando encajen.
WifiProtoResp wifi_client_request(uint8_t cmd, const uint8_t* payload, uint16_t len);

bool wifi_client_ping(uint8_t* out_fw_version);
bool wifi_client_list_dir(const char* path, void (*on_entry)(const WifiDirEntry&));
bool wifi_client_write_open(const char* path, uint8_t* out_handle);
bool wifi_client_write_chunk(uint8_t handle, const uint8_t* data, uint16_t len);
bool wifi_client_write_close(uint8_t handle, uint32_t* out_total);
// Fuerza el tamano/contenido en disco de un handle de escritura abierto,
// SIN cerrarlo - para escritores de larga duracion (ver SD_LOG.cpp) que
// quieren que el fichero sea legible por otros de forma periodica.
bool wifi_client_write_sync(uint8_t handle);
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

// Descarga /SYS/NTP.CFG (mismo mecanismo que WIFI.CFG) y lo interpreta aqui:
// lineas CLAVE=VALOR (SERVER, MODE, UTCOFFSET, DST - ver WIFI_PROTOCOL.h).
// Si el fichero no existe o no se puede leer, devuelve true con
// out->sync_enabled=false (no es un error, simplemente no hay NTP
// configurado) - solo devuelve false ante un fallo de transporte real.
bool wifi_client_read_ntp_config(NtpConfig* out);

// Envia la hora local ya calculada (UTC + UTCOFFSET + DST) para que el
// STM32 ajuste su RTC. year es de 2 digitos (25 = 2025), como ya usa
// LOAD *RTC= en el firmware del STM32.
bool wifi_client_set_time(uint8_t year, uint8_t month, uint8_t day,
                           uint8_t hour, uint8_t minute, uint8_t second);

// Descarga un fichero de texto pequeno cualquiera (mismo mecanismo que
// WIFI.CFG/NTP.CFG) sin interpretarlo. *out queda vacio si el fichero no
// existe (no es un error). Devuelve false solo ante un fallo de transporte.
bool wifi_client_read_text_file(const char* path, String* out);

// --- Puente de red (BBS/telnet) --------------------------------------------
// Una sola llamada mueve los DOS sentidos: entrega `tx` (lo que viene del
// socket, hacia el Z80) y recoge en `rx` lo que el Z80 haya escrito.
//
// Lleva por dentro el bit de secuencia, asi que el llamante NO debe descartar
// su buffer hasta que *out_tx_accepted sea true: si hubo timeout, los mismos
// bytes tienen que volver a ofrecerse tal cual en la siguiente llamada.
//
// `status` es el estado del socket TAL COMO LO VE EL ESP32 (WifiProtoNetStatus):
// viaja en la peticion porque el STM32 no sabe nada de la conexion, solo lo
// memoriza para poder contestarselo al Z80.
//
// *out_rx_free son los trozos de 16 bytes libres en el buffer de entrada del
// STM32: control de flujo por credito, no mandar mas de lo que quepa.
bool wifi_client_net_poll(uint8_t status,
                          const uint8_t* tx, uint16_t tx_len, bool* out_tx_accepted,
                          uint8_t* rx, uint16_t* out_rx_len,
                          uint8_t* out_rx_free);

#endif
