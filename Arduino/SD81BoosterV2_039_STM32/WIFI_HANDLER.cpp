#include "WIFI_HANDLER.h"
#include "WIFI_PROTOCOL.h"
#include "SD_handle.h"
#include "GLOBALS.h"
#include "RTC.h"

// Todas las peticiones las inicia el ESP32; el STM32 solo responde. Handles
// propios (independientes de f_handle[]/f_opened[], que usa el interprete de
// comandos BASIC existente - no deben competir por los mismos slots).

#define WIFI_SERIAL             Serial1
#define WIFI_BAUD                921600
#define WIFI_FRAME_TIMEOUT_MS    200    // margen maximo entre bytes de una misma trama
#define WIFI_NUM_HANDLES         2

static FsFile wifi_handle[WIFI_NUM_HANDLES];
static bool   wifi_handle_used[WIFI_NUM_HANDLES];

static uint8_t rx_payload[WIFI_PROTO_MAX_FRAME_PAYLOAD];
static uint8_t tx_payload[WIFI_PROTO_MAX_FRAME_PAYLOAD];

void wifi_handler_init() {
  WIFI_SERIAL.setRx(UART_RX);
  WIFI_SERIAL.setTx(UART_TX);
  WIFI_SERIAL.begin(WIFI_BAUD);
  for (int i = 0; i < WIFI_NUM_HANDLES; i++) wifi_handle_used[i] = false;
  log_1("WiFi handler listo en UART1 (%d baud)", WIFI_BAUD);
}

static int wifi_read_byte(uint32_t timeout_ms) {
  uint32_t start = millis();
  while (!WIFI_SERIAL.available()) {
    if (millis() - start > timeout_ms) return -1;
  }
  return WIFI_SERIAL.read();
}

static void wifi_send_frame(uint8_t cmd, const uint8_t* payload, uint16_t len) {
  static uint8_t buf[3 + WIFI_PROTO_MAX_FRAME_PAYLOAD];
  buf[0] = cmd;
  buf[1] = (uint8_t)(len & 0xFF);
  buf[2] = (uint8_t)(len >> 8);
  if (len > 0) memcpy(&buf[3], payload, len);
  uint8_t crc = wifi_proto_crc8(buf, 3 + len);
  WIFI_SERIAL.write(WIFI_PROTO_SOF);
  WIFI_SERIAL.write(buf, 3 + len);
  WIFI_SERIAL.write(crc);
}

static void send_status_only(uint8_t cmd, uint8_t status) {
  tx_payload[0] = status;
  wifi_send_frame(cmd, tx_payload, 1);
}

// Lee una trama completa ya con el SOF consumido por el llamador. Devuelve
// false si hay timeout o CRC invalido (en CRC invalido ya responde
// CMD_FRAME_ERROR ella misma, para que el ESP32 reenvie).
static bool wifi_read_frame(uint8_t* cmd_out, uint8_t* payload_out, uint16_t* len_out) {
  int cmd    = wifi_read_byte(WIFI_FRAME_TIMEOUT_MS);
  int len_lo = wifi_read_byte(WIFI_FRAME_TIMEOUT_MS);
  int len_hi = wifi_read_byte(WIFI_FRAME_TIMEOUT_MS);
  if (cmd < 0 || len_lo < 0 || len_hi < 0) return false;

  uint16_t len = (uint16_t)len_lo | ((uint16_t)len_hi << 8);
  if (len > WIFI_PROTO_MAX_FRAME_PAYLOAD) return false; // trama absurda, se descarta

  static uint8_t crcbuf[3 + WIFI_PROTO_MAX_FRAME_PAYLOAD];
  crcbuf[0] = (uint8_t)cmd;
  crcbuf[1] = (uint8_t)len_lo;
  crcbuf[2] = (uint8_t)len_hi;
  for (uint16_t i = 0; i < len; i++) {
    int b = wifi_read_byte(WIFI_FRAME_TIMEOUT_MS);
    if (b < 0) return false;
    crcbuf[3 + i] = (uint8_t)b;
  }

  int crc_byte = wifi_read_byte(WIFI_FRAME_TIMEOUT_MS);
  if (crc_byte < 0) return false;

  if ((uint8_t)crc_byte != wifi_proto_crc8(crcbuf, 3 + len)) {
    wifi_send_frame(CMD_FRAME_ERROR, NULL, 0);
    return false;
  }

  *cmd_out = (uint8_t)cmd;
  memcpy(payload_out, &crcbuf[3], len);
  *len_out = len;
  return true;
}

static bool extract_path(const uint8_t* payload, uint16_t len, char* out_path) {
  if (len < 1) return false;
  uint8_t path_len = payload[0];
  if (len < (uint16_t)(1 + path_len)) return false;
  uint8_t n = path_len < (WIFI_PROTO_MAX_PATH - 1) ? path_len : (WIFI_PROTO_MAX_PATH - 1);
  memcpy(out_path, &payload[1], n);
  out_path[n] = 0;
  return true;
}

static int alloc_handle() {
  for (int i = 0; i < WIFI_NUM_HANDLES; i++) if (!wifi_handle_used[i]) return i;
  return -1;
}

static void handle_list_dir(const uint8_t* payload, uint16_t len) {
  log_1("WIFI LIST_DIR: enter, len=%d", len);
  char path[WIFI_PROTO_MAX_PATH];
  if (len < 2 || !extract_path(payload, len - 1, path)) {
    log_1("WIFI LIST_DIR: extract_path failed");
    send_status_only(CMD_LIST_DIR, ST_IO_ERROR);
    return;
  }
  uint8_t start_index = payload[len - 1];
  log_1("WIFI LIST_DIR: path=%s start_index=%d", path, start_index);

  FsFile listDir = sd.open(path);
  log_1("WIFI LIST_DIR: sd.open returned");
  if (!listDir || !listDir.isDir()) {
    log_1("WIFI LIST_DIR: not a directory or doesn't exist");
    if (listDir) listDir.close();
    send_status_only(CMD_LIST_DIR, ST_NOT_FOUND);
    return;
  }

  FsFile entry;
  uint8_t skipped = 0;
  log_1("WIFI LIST_DIR: before skip loop");
  while (skipped < start_index && entry.openNext(&listDir, O_RDONLY)) {
    entry.close();
    skipped++;
  }
  log_1("WIFI LIST_DIR: skip done (%d)", skipped);

  uint16_t pos = 3;   // 0=status, 1=entry_count, 2=has_more, filled in at the end
  uint8_t count = 0;
  bool has_more = false;
  char name[64];

  while (count < 4) {   // max 4 entries per page, plenty within one frame
    log_1("WIFI LIST_DIR: before openNext count=%d", count);
    if (!entry.openNext(&listDir, O_RDONLY)) break;
    log_1("WIFI LIST_DIR: openNext returned, reading name");
    entry.getName(name, sizeof(name));
    log_1("WIFI LIST_DIR: entry=%s", name);
    uint8_t name_len = (uint8_t)strlen(name);
    uint32_t size = entry.size();
    uint8_t flags = entry.isDir() ? WIFI_PROTO_FLAG_DIR : 0;
    entry.close();

    if (pos + 1 + name_len + 4 + 1 > WIFI_PROTO_MAX_FRAME_PAYLOAD) { has_more = true; break; }

    tx_payload[pos++] = name_len;
    memcpy(&tx_payload[pos], name, name_len); pos += name_len;
    memcpy(&tx_payload[pos], &size, 4); pos += 4;
    tx_payload[pos++] = flags;
    count++;
  }
  log_1("WIFI LIST_DIR: main loop finished, count=%d", count);

  if (!has_more) {
    FsFile probe;
    if (probe.openNext(&listDir, O_RDONLY)) { has_more = true; probe.close(); }
  }
  log_1("WIFI LIST_DIR: has_more=%d, closing listDir", has_more);

  tx_payload[0] = ST_OK;
  tx_payload[1] = count;
  tx_payload[2] = has_more ? 1 : 0;
  listDir.close();
  log_1("WIFI LIST_DIR: sending response, pos=%d", pos);
  wifi_send_frame(CMD_LIST_DIR, tx_payload, pos);
  log_1("WIFI LIST_DIR: response sent");
}

static void handle_stat(const uint8_t* payload, uint16_t len) {
  char path[WIFI_PROTO_MAX_PATH];
  if (!extract_path(payload, len, path)) { send_status_only(CMD_STAT, ST_IO_ERROR); return; }

  FsFile f = sd.open(path, O_RDONLY);
  if (!f) { send_status_only(CMD_STAT, ST_NOT_FOUND); return; }

  uint32_t size = f.size();
  uint8_t flags = f.isDir() ? WIFI_PROTO_FLAG_DIR : 0;
  f.close();

  tx_payload[0] = ST_OK;
  memcpy(&tx_payload[1], &size, 4);
  tx_payload[5] = flags;
  wifi_send_frame(CMD_STAT, tx_payload, 6);
}

// Nombre fijo usado por el ESP32 para auto-actualizar su propio firmware
// desde la SD (ver Wifi_module_01.ino, FW_UPDATE_PATH). READ_OPEN/DELETE
// sobre este path no llevan ninguna marca especial en el protocolo - se
// reconoce aqui solo para informar al usuario por la consola y el LED del
// STM32 (los que de verdad ve un usuario normal, no el Monitor Serie del
// ESP32, que no va a tener conectado).
// En la raiz de la SD, no en /SYS - misma convencion que firmware.bin (STM32)
// y SD81.MCS (FPGA), los otros dos ficheros de auto-actualizacion.
#define ESP32_FW_PATH "/ESP32_FW.BIN"

// Si el ESP32 falla a mitad de la actualizacion (imagen corrupta, error de
// escritura, etc.) nunca llega a pedir DELETE - el fichero se queda en la
// SD para reintentar en el siguiente arranque, pero el STM32 no tiene
// ninguna otra señal de que ha terminado (con error). Sin esto, el LED se
// quedaria parpadeando rosa para siempre. Timeout: si no llega la
// confirmacion de exito (DELETE) en este plazo, se asume fallo.
#define ESP32_UPDATE_TIMEOUT_MS  60000
static bool esp32_update_in_progress = false;
static uint32_t esp32_update_start_ms = 0;

static void handle_read_open(const uint8_t* payload, uint16_t len) {
  char path[WIFI_PROTO_MAX_PATH];
  if (!extract_path(payload, len, path)) { send_status_only(CMD_READ_OPEN, ST_IO_ERROR); return; }

  int h = alloc_handle();
  if (h < 0) { send_status_only(CMD_READ_OPEN, ST_NO_HANDLE_FREE); return; }

  if (!wifi_handle[h].open(path, O_RDONLY)) {
    send_status_only(CMD_READ_OPEN, ST_NOT_FOUND);
    return;
  }
  wifi_handle_used[h] = true;

  // Solo se dispara aqui, DESPUES de confirmar que el fichero existe de
  // verdad - el ESP32 pide READ_OPEN de este path en TODOS los arranques
  // (exista o no la actualizacion), asi que disparar esto antes de saber
  // si realmente se abrio encenderia el LED en cada arranque normal.
  if (strcmp(path, ESP32_FW_PATH) == 0) {
    Serial.println("Updating ESP32 firmware from the SD card...");
    set_blinking(clPINK, 4);
    esp32_update_in_progress = true;
    esp32_update_start_ms = millis();
  }

  uint32_t size = wifi_handle[h].size();
  tx_payload[0] = ST_OK;
  tx_payload[1] = (uint8_t)h;
  memcpy(&tx_payload[2], &size, 4);
  wifi_send_frame(CMD_READ_OPEN, tx_payload, 6);
}

static void handle_read_chunk(const uint8_t* payload, uint16_t len) {
  if (len < 5) { send_status_only(CMD_READ_CHUNK, ST_IO_ERROR); return; }
  uint8_t h = payload[0];
  uint32_t offset; memcpy(&offset, &payload[1], 4);

  if (h >= WIFI_NUM_HANDLES || !wifi_handle_used[h]) {
    send_status_only(CMD_READ_CHUNK, ST_BAD_HANDLE);
    return;
  }

  wifi_handle[h].seekSet(offset);
  int n = wifi_handle[h].read(&tx_payload[3], WIFI_PROTO_CHUNK_SIZE);
  if (n < 0) n = 0;
  uint16_t n16 = (uint16_t)n;
  bool eof = (offset + n16) >= wifi_handle[h].size();

  tx_payload[0] = ST_OK;
  memcpy(&tx_payload[1], &n16, 2);
  tx_payload[3 + n16] = eof ? 1 : 0;
  wifi_send_frame(CMD_READ_CHUNK, tx_payload, 3 + n16 + 1);
}

static void handle_read_close(const uint8_t* payload, uint16_t len) {
  if (len < 1) { send_status_only(CMD_READ_CLOSE, ST_IO_ERROR); return; }
  uint8_t h = payload[0];
  if (h >= WIFI_NUM_HANDLES || !wifi_handle_used[h]) {
    send_status_only(CMD_READ_CLOSE, ST_BAD_HANDLE);
    return;
  }
  wifi_handle[h].close();
  wifi_handle_used[h] = false;
  send_status_only(CMD_READ_CLOSE, ST_OK);
}

static void handle_write_open(const uint8_t* payload, uint16_t len) {
  char path[WIFI_PROTO_MAX_PATH];
  if (!extract_path(payload, len, path)) { send_status_only(CMD_WRITE_OPEN, ST_IO_ERROR); return; }

  int h = alloc_handle();
  if (h < 0) { send_status_only(CMD_WRITE_OPEN, ST_NO_HANDLE_FREE); return; }

  if (!wifi_handle[h].open(path, O_WRONLY | O_CREAT | O_TRUNC)) {
    send_status_only(CMD_WRITE_OPEN, ST_IO_ERROR);
    return;
  }
  wifi_handle_used[h] = true;

  tx_payload[0] = ST_OK;
  tx_payload[1] = (uint8_t)h;
  wifi_send_frame(CMD_WRITE_OPEN, tx_payload, 2);
}

static void handle_write_chunk(const uint8_t* payload, uint16_t len) {
  if (len < 3) { send_status_only(CMD_WRITE_CHUNK, ST_IO_ERROR); return; }
  uint8_t h = payload[0];
  uint16_t dlen; memcpy(&dlen, &payload[1], 2);

  if (h >= WIFI_NUM_HANDLES || !wifi_handle_used[h]) {
    send_status_only(CMD_WRITE_CHUNK, ST_BAD_HANDLE);
    return;
  }
  if ((uint16_t)(3 + dlen) > len) { send_status_only(CMD_WRITE_CHUNK, ST_IO_ERROR); return; }

  size_t written = wifi_handle[h].write(&payload[3], dlen);
  send_status_only(CMD_WRITE_CHUNK, (written == dlen) ? ST_OK : ST_IO_ERROR);
}

static void handle_write_close(const uint8_t* payload, uint16_t len) {
  if (len < 1) { send_status_only(CMD_WRITE_CLOSE, ST_IO_ERROR); return; }
  uint8_t h = payload[0];
  if (h >= WIFI_NUM_HANDLES || !wifi_handle_used[h]) {
    send_status_only(CMD_WRITE_CLOSE, ST_BAD_HANDLE);
    return;
  }

  wifi_handle[h].sync();
  uint32_t total = wifi_handle[h].size();
  wifi_handle[h].close();
  wifi_handle_used[h] = false;

  tx_payload[0] = ST_OK;
  memcpy(&tx_payload[1], &total, 4);
  wifi_send_frame(CMD_WRITE_CLOSE, tx_payload, 5);
}

// Igual que el sync() de dentro de handle_write_close, pero sin cerrar el
// handle - para escritores de larga duracion (p.ej. el log del ESP32, ver
// SD_LOG.cpp) que quieren que el fichero sea legible por otros de forma
// periodica sin perder el progreso reabriendo (WRITE_OPEN siempre trunca).
static void handle_write_sync(const uint8_t* payload, uint16_t len) {
  if (len < 1) { send_status_only(CMD_WRITE_SYNC, ST_IO_ERROR); return; }
  uint8_t h = payload[0];
  if (h >= WIFI_NUM_HANDLES || !wifi_handle_used[h]) {
    send_status_only(CMD_WRITE_SYNC, ST_BAD_HANDLE);
    return;
  }
  wifi_handle[h].sync();
  send_status_only(CMD_WRITE_SYNC, ST_OK);
}

static void handle_delete(const uint8_t* payload, uint16_t len) {
  char path[WIFI_PROTO_MAX_PATH];
  if (!extract_path(payload, len, path)) { send_status_only(CMD_DELETE, ST_IO_ERROR); return; }

  FsFile f = sd.open(path, O_RDONLY);
  bool exists = (bool)f;
  bool is_dir = exists && f.isDir();
  if (exists) f.close();

  if (!exists) { send_status_only(CMD_DELETE, ST_NOT_FOUND); return; }

  bool ok = is_dir ? sd.rmdir(path) : sd.remove(path);
  if (ok && strcmp(path, ESP32_FW_PATH) == 0) {
    Serial.println("ESP32 firmware update completed successfully.");
    set_blinking_off();
    set_status_led_ok();
    esp32_update_in_progress = false;
  }
  send_status_only(CMD_DELETE, ok ? ST_OK : ST_IO_ERROR);
}

static void handle_mkdir(const uint8_t* payload, uint16_t len) {
  char path[WIFI_PROTO_MAX_PATH];
  if (!extract_path(payload, len, path)) { send_status_only(CMD_MKDIR, ST_IO_ERROR); return; }
  bool ok = sd.mkdir(path);
  send_status_only(CMD_MKDIR, ok ? ST_OK : ST_IO_ERROR);
}

// El ESP32 ya calcula la hora local final (UTC + UTCOFFSET + DST, ver
// /SYS/NTP.CFG en WIFI_PROTOCOL.h) - aqui solo se ajusta el RTC, igual que
// hace el comando BASIC "LOAD *RTC=" en COMMANDS.cpp.
static void handle_set_time(const uint8_t* payload, uint16_t len) {
  if (len < 6) { send_status_only(CMD_SET_TIME, ST_IO_ERROR); return; }

  int year = payload[0], month = payload[1], day = payload[2];
  int hours = payload[3], minutes = payload[4], seconds = payload[5];

  if (!isDate(year, month, day) || !isTime(hours, minutes, seconds, 0)) {
    send_status_only(CMD_SET_TIME, ST_IO_ERROR);
    return;
  }

  rtc.setHours(hours);
  rtc.setMinutes(minutes);
  rtc.setSeconds(seconds);
  rtc.setTime(hours, minutes, seconds);

  rtc.setDay(day);
  rtc.setMonth(month);
  rtc.setYear(year);
  rtc.setDate(day, month, year);

  Serial.printf("Reloj actualizado via NTP (WiFi) a %02d/%02d/%02d %02d:%02d:%02d\n",
                day, month, year, hours, minutes, seconds);

  send_status_only(CMD_SET_TIME, ST_OK);
}

// /SYS/WIFI.CFG ya no tiene un comando dedicado (era CMD_GET_WIFI_CFG,
// retirado por una condicion de carrera SD dificil de depurar entre esta
// funcion y la ISR get_ctrl_reg - ver memoria del proyecto). Se sirve como
// un fichero de texto mas via los comandos genericos READ_OPEN/READ_CHUNK/
// READ_CLOSE, ya manejados por handle_read_open/handle_read_chunk mas
// arriba en este fichero - el ESP32 lo descarga y lo interpreta el mismo.

void wifi_handler_poll() {
  if (esp32_update_in_progress && (millis() - esp32_update_start_ms > ESP32_UPDATE_TIMEOUT_MS)) {
    Serial.println("ESP32 firmware update timed out - assuming it failed.");
    set_blinking_off();
    set_status_LED(clYELLOW);
    esp32_update_in_progress = false;
  }

  if (!WIFI_SERIAL.available()) return;
  if (WIFI_SERIAL.read() != WIFI_PROTO_SOF) return;   // resincroniza byte a byte si hay ruido

  uint8_t cmd;
  uint16_t len;
  if (!wifi_read_frame(&cmd, rx_payload, &len)) return;

  switch (cmd) {
    case CMD_PING:         tx_payload[0] = VERSION; wifi_send_frame(CMD_PING, tx_payload, 1); break;
    case CMD_LIST_DIR:     handle_list_dir(rx_payload, len); break;
    case CMD_STAT:         handle_stat(rx_payload, len); break;
    case CMD_READ_OPEN:    handle_read_open(rx_payload, len); break;
    case CMD_READ_CHUNK:   handle_read_chunk(rx_payload, len); break;
    case CMD_READ_CLOSE:   handle_read_close(rx_payload, len); break;
    case CMD_WRITE_OPEN:   handle_write_open(rx_payload, len); break;
    case CMD_WRITE_CHUNK:  handle_write_chunk(rx_payload, len); break;
    case CMD_WRITE_CLOSE:  handle_write_close(rx_payload, len); break;
    case CMD_DELETE:       handle_delete(rx_payload, len); break;
    case CMD_MKDIR:        handle_mkdir(rx_payload, len); break;
    case CMD_SET_TIME:     handle_set_time(rx_payload, len); break;
    case CMD_WRITE_SYNC:   handle_write_sync(rx_payload, len); break;
    default: break;   // comando desconocido: se ignora
  }
}
