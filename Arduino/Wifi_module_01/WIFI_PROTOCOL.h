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
#define WIFI_PROTO_MAX_SSID     32    // limite estandar 802.11 (uso del lado ESP32 al parsear WIFI.CFG)
#define WIFI_PROTO_MAX_PASS     64    // limite estandar WPA2-PSK ASCII (uso del lado ESP32 al parsear WIFI.CFG)
#define WIFI_PROTO_MAX_NETWORKS 4     // maximo de redes que el ESP32 recuerda al parsear WIFI.CFG (ver mas abajo)

// Puente de red (CMD_NET_POLL). 128 y no 256: la trama lleva los DOS sentidos
// a la vez, y 128+128 mas cabeceras entra en WIFI_PROTO_MAX_FRAME_PAYLOAD sin
// tener que tocarlo (ese define dimensiona buffers fijos en ambos lados).
// A 10 ms de sondeo son 12,8 KB/s por sentido; un BBS a 2400 baudios son 240 B/s.
#define WIFI_PROTO_NET_CHUNK    128

// Tamano maximo de PAYLOAD de una trama (CMD+LEN no cuentan) - dimensiona los buffers
// fijos en ambos lados, deben usar la MISMA constante para no desbordar el lado contrario.
// Margen sobre WIFI_PROTO_CHUNK_SIZE para cabeceras de comando (handle, len, offset, etc.)
#define WIFI_PROTO_MAX_FRAME_PAYLOAD  (WIFI_PROTO_CHUNK_SIZE + 8)

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
  CMD_DELETE       = 0x0A,  // fichero O directorio (vacio) - el STM32 decide segun el tipo
  CMD_MKDIR        = 0x0C,
  CMD_SET_TIME     = 0x0D,
  CMD_WRITE_SYNC   = 0x0E,  // fuerza el tamano en disco de un handle abierto SIN cerrarlo
  CMD_NET_POLL     = 0x0F,  // puente de datos de red (BBS/telnet) - ver mas abajo
  // 0x0B (antiguo CMD_GET_WIFI_CFG) retirado - ver nota mas abajo sobre WIFI.CFG
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

// --- Puente de red: estado de la conexion (byte `status` de NET_POLL) -------
enum WifiProtoNetStatus : uint8_t {
  NET_ST_IDLE       = 0x00,   // sin conexion
  NET_ST_CONNECTING = 0x01,
  NET_ST_CONNECTED  = 0x02,
  NET_ST_ERROR      = 0x03,   // el ultimo intento fallo / la conexion se cayo
};

// Flags de NET_POLL. El bit de secuencia (alternating bit) hace el sondeo
// IDEMPOTENTE, que no es un lujo: wifi_handler_poll() se llama desde loop(),
// asi que mientras el Z80 esta dentro de un comando MCU largo el loop() del
// STM32 no corre, el ESP32 hace timeout y reintenta. Con sondeo cada 10-20 ms
// eso pasa constantemente. El receptor solo consume los datos si el bit
// CAMBIO respecto a la trama anterior, y el emisor no descarta lo suyo hasta
// ver el ack: un reintento repite la trama tal cual, sin duplicar ni perder.
#define WIFI_PROTO_NET_SEQ    0x01   // secuencia de los datos que van EN esta trama
#define WIFI_PROTO_NET_ACK    0x02   // eco de la secuencia recibida en el otro sentido

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
//                                                        (si path es un directorio, debe
//                                                        estar VACIO - equivale a rmdir, no
//                                                        borrado recursivo)
// MKDIR         req: path(str)                          resp: status
// SET_TIME      req: year(1B, 2 digitos, 0-99),          resp: status
//               month(1B, 1-12), day(1B, 1-31),
//               hour(1B, 0-23), minute(1B, 0-59),
//               second(1B, 0-59)
//               (hora local ya calculada por el ESP32 - offset UTC + DST
//               aplicados alli, el STM32 solo ajusta su RTC tal cual)
// WRITE_SYNC    req: handle(1B)                         resp: status
//               (fuerza el tamano/contenido en disco de un handle que se
//               mantiene abierto mucho tiempo sin cerrarlo - WRITE_CHUNK
//               por si solo NO actualiza el tamano visible del fichero
//               hasta el WRITE_CLOSE; util para logs u otros escritores de
//               larga duracion que quieren que el fichero sea legible por
//               otros antes de terminar)
//
// NET_POLL      req: flags(1B), len(1B), data(len)      resp: status(1B), flags(1B),
//                    (lo que va del socket al Z80)             rx_free(1B), len(1B), data(len)
//                                                              (lo que va del Z80 al socket)
//               flags: bit0 = secuencia de los datos de ESTA trama,
//                      bit1 = ack de la secuencia recibida del otro lado.
//               rx_free: hueco libre en el buffer de entrada del STM32, en
//                      trozos de 16 bytes (control de flujo por credito: el
//                      ESP32 nunca manda mas de lo que cabe; sin esto, un Z80
//                      que no lee a tiempo pierde datos en silencio).
//               len <= WIFI_PROTO_NET_CHUNK en los dos sentidos.
//
//               Este comando es el UNICO que el ESP32 manda de forma
//               PERIODICA aunque no tenga nada que decir (~250 ms en reposo,
//               ~10-20 ms con conexion abierta). Hace falta porque el STM32
//               no puede iniciar tramas: es la unica via por la que sale lo
//               que el Z80 escribe, incluido el propio "ATDT host:puerto" del
//               intérprete de modem. La cadencia en reposo se mantiene baja a
//               proposito: el loop() del STM32 lo comparte con el AY, la voz y
//               el WAV.
//
//               El STM32 NO interpreta nada de lo que pasa por aqui: solo son
//               dos buffers circulares. Socket, destino y comandos AT viven en
//               el ESP32.
//
// "path(str)": length-prefixed, 1 byte de longitud + bytes UTF-8/ASCII (NO terminador nulo
// en el cable), maximo WIFI_PROTO_MAX_PATH-1 bytes de nombre.
//
// Todas las peticiones las inicia SIEMPRE el ESP32 - el STM32 nunca manda una trama sin
// que se la hayan pedido, para no necesitar arbitraje en el UART.
//
// --- Configuracion WiFi (/SYS/WIFI.CFG) -------------------------------------
// NO hay comando dedicado para esto (hubo uno, CMD_GET_WIFI_CFG, retirado por un bug de
// concurrencia SD dificil de depurar - ver memoria del proyecto). El STM32 no sabe nada
// de "redes WiFi": /SYS/WIFI.CFG es un fichero de texto mas, servido por los comandos
// genericos READ_OPEN/READ_CHUNK/READ_CLOSE ya existentes (los mismos que usan la
// descarga y la actualizacion de firmware). El ESP32 lo descarga entero al arrancar y lo
// interpreta el mismo: pares de lineas de texto SSID/password, repetidos uno tras otro
// para soportar varias redes (prueba cada una en orden hasta conectar), ej:
//   CasaWifi
//   passwordcasa
//   TrabajoWifi
//   passwordtrabajo
// WIFI_PROTO_MAX_NETWORKS/MAX_SSID/MAX_PASS son limites de parseo del lado ESP32
// unicamente - el STM32 ni los conoce ni los necesita.
//
// --- Sincronizacion horaria NTP (/SYS/NTP.CFG) ------------------------------
// Mismo patron que WIFI.CFG: no hay comando de lectura dedicado, el ESP32
// descarga /SYS/NTP.CFG via READ_OPEN/READ_CHUNK/READ_CLOSE y lo interpreta
// el mismo. Formato clave=valor, una por linea:
//   SERVER=pool.ntp.org
//   MODE=SERVER
//   UTCOFFSET=1
//   DST=1
// SERVER: host del servidor NTP. MODE=LOCAL desactiva la sincronizacion por
// completo (o si el fichero no existe/no se puede leer - comportamiento por
// defecto, no rompe nada para quien no lo configure). UTCOFFSET: horas
// enteras respecto a UTC (puede ser negativo). DST: 0/1, suma +1h extra en
// horario de verano - interruptor manual, el usuario lo cambia el mismo dos
// veces al ano, no hay deteccion automatica de DST/zona horaria.
//
// Cuando MODE=SERVER y consigue conectar al servidor NTP, el ESP32 calcula
// la hora local final (UTC + UTCOFFSET + DST) y se la envia al STM32 con
// CMD_SET_TIME - UNA sola vez, justo despues de conectar al WiFi (el RTC con
// bateria del STM32 ya mantiene bien la hora entre medias, no hace falta
// resincronizar periodicamente). Si no hay Internet o el fichero dice
// MODE=LOCAL, simplemente no se manda CMD_SET_TIME y el RTC sigue con lo que
// ya tenia.

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
