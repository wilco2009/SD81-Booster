#pragma once
#include <stdint.h>

// SD81 Booster - Protocolo UART STM32 <-> modulo WiFi (ESP32-C3)
//
// Sincrono peticion/respuesta: el ESP32 pide, el STM32 responde, el ESP32
// no manda la siguiente trama hasta tener respuesta (o timeout + reintento).
//
// Framing:
//   [SOF 0xAA][CMD 1B][LEN 2B LE][PAYLOAD LEN bytes][CRC8 1B]
//
// El CRC8 se calcula sobre CMD+LEN+PAYLOAD (NO incluye el SOF ni el propio
// byte de CRC). Si el CRC no cuadra, el STM32 responde CMD_FRAME_ERROR
// (payload vacio) para que el ESP32 reenvie la misma peticion.
//
// IMPORTANTE: este fichero debe mantenerse IDENTICO en los dos proyectos
// (SD81BoosterV2_039_STM32 y Wifi_module_01) - copiar, no reinventar.

#define WIFI_PROTO_SOF          0xAA
#define WIFI_PROTO_CHUNK_SIZE   256   // tamano de trozo para READ_CHUNK/WRITE_CHUNK
#define WIFI_PROTO_MAX_PATH     128   // longitud maxima de path (incluye '\0' logico, ver LIST_DIR/STAT/etc.)

// Comandos (campo CMD de la trama)
enum WifiProtoCmd : uint8_t {
  CMD_FRAME_ERROR  = 0x00,  // reservado: respuesta ante CRC invalido, no es una peticion real
  CMD_PING         = 0x01,
  CMD_LIST_DIR     = 0x02,
  CMD_STAT         = 0x03,
  CMD_READ_OPEN    = 0x04,
  CMD_READ_CHUNK   = 0x05,
  CMD_READ_CLOSE   = 0x06,
  CMD_WRITE_OPEN   = 0x07,
  CMD_WRITE_CHUNK  = 0x08,
  CMD_WRITE_CLOSE  = 0x09,
  CMD_DELETE       = 0x0A,
};

// Status - primer byte de payload en casi todas las respuestas
enum WifiProtoStatus : uint8_t {
  ST_OK             = 0x00,
  ST_NOT_FOUND      = 0x01,
  ST_IO_ERROR       = 0x02,
  ST_NO_HANDLE_FREE = 0x03,
  ST_BAD_HANDLE     = 0x04,
};

// Flags de entrada (LIST_DIR) / STAT
#define WIFI_PROTO_FLAG_DIR   0x01

// --- Formato de payload por comando (referencia) ---------------------------
// PING          req: (vacio)                          resp: fw_version(1B)
// LIST_DIR      req: path(str), start_index(1B)        resp: status,
//                                                       entry_count(1B), has_more(1B),
//                                                       entradas x entry_count:
//                                                         name_len(1B), name(name_len),
//                                                         size(4B LE), flags(1B)
// STAT          req: path(str)                         resp: status, size(4B LE), flags(1B)
// READ_OPEN     req: path(str)                         resp: status, handle(1B), file_size(4B LE)
// READ_CHUNK    req: handle(1B), offset(4B LE)          resp: status, len(2B LE), data(len), eof(1B)
// READ_CLOSE    req: handle(1B)                         resp: status
// WRITE_OPEN    req: path(str)                          resp: status, handle(1B)
// WRITE_CHUNK   req: handle(1B), len(2B LE), data(len)  resp: status   (ack por trozo)
// WRITE_CLOSE   req: handle(1B)                         resp: status, total_bytes(4B LE)
// DELETE        req: path(str)                          resp: status
//
// "path(str)": length-prefixed, 1 byte de longitud + bytes UTF-8/ASCII (NO terminador nulo
// en el cable), maximo WIFI_PROTO_MAX_PATH-1 bytes de nombre.

// CRC8 (poli 0x07, sin reflejar, init 0x00) - identico en ambos lados
static inline uint8_t wifi_proto_crc8(const uint8_t* data, uint16_t len) {
  uint8_t crc = 0x00;
  for (uint16_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t b = 0; b < 8; b++) {
      crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
    }
  }
  return crc;
}
