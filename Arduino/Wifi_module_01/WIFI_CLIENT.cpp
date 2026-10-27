#include "WIFI_CLIENT.h"
#include "DIAG.h"

#define STM32_SERIAL           Serial1
#define STM32_RX_PIN           4
#define STM32_TX_PIN           5
#define STM32_BAUD             921600
#define WIFI_CLIENT_BYTE_TIMEOUT_MS      300
#define WIFI_CLIENT_SOF_SEARCH_TIMEOUT_MS 500   // limite TOTAL buscando el SOF, no por-byte
#define WIFI_CLIENT_MAX_RETRIES          3
// Esperas de NET_POLL (de momento iguales a las de las demas ordenes). NO BAJARLAS SOLAS: probado el 20/10/2026 con 60/150 ms y empeora. El STM32,
// ante una trama truncada, sigue esperando los bytes que faltan hasta WIFI_FRAME_TIMEOUT_MS (200 ms); si el ESP32 reintenta antes, la peticion nueva
// se la traga como si fuera la continuacion de la rota, sale con el CRC malo y contesta CMD_FRAME_ERROR, y asi en cadena (rafagas de
// intentos=3 ok=0 frame_error=1). Ademas el STM32 a veces tarda mas de 50 ms en contestar de verdad (se vio una respuesta a los 59 ms).
// Para acortar la perdida de una trama hay que bajar a la vez WIFI_FRAME_TIMEOUT_MS del STM32 (a ~20 ms; los bytes de una trama llegan
// seguidos) y entonces poner aqui ~100 ms.
#define WIFI_CLIENT_POLL_BYTE_TIMEOUT_MS  WIFI_CLIENT_BYTE_TIMEOUT_MS
#define WIFI_CLIENT_POLL_SOF_TIMEOUT_MS   WIFI_CLIENT_SOF_SEARCH_TIMEOUT_MS

void wifi_client_init() {
  STM32_SERIAL.begin(STM32_BAUD, SERIAL_8N1, STM32_RX_PIN, STM32_TX_PIN);
}

static int wifi_read_byte(uint32_t timeout_ms) {
  uint32_t start = millis();
  while (!STM32_SERIAL.available()) {
    if (millis() - start > timeout_ms) return -1;
  }
  return STM32_SERIAL.read();
}

static bool wifi_client_read_frame_raw(WifiProtoResp* out, uint32_t byte_ms, uint32_t sof_total_ms) {
  // Busca el SOF con un limite de tiempo TOTAL (no solo por-byte): si llegara
  // ruido continuo que nunca coincide con WIFI_PROTO_SOF, un timeout por-byte
  // solo no bastaria para salir de este bucle.
  uint32_t sof_search_start = millis();
  int b;
  do {
    if (millis() - sof_search_start > sof_total_ms) { DIAG_FAIL(1); return false; }   // 1: ni un SOF en 500 ms
    b = wifi_read_byte(byte_ms);
    if (b < 0) { DIAG_FAIL(2); return false; }                                                            // 2: silencio
  } while (b != WIFI_PROTO_SOF);

  int cmd    = wifi_read_byte(byte_ms);
  int len_lo = wifi_read_byte(byte_ms);
  int len_hi = wifi_read_byte(byte_ms);
  if (cmd < 0 || len_lo < 0 || len_hi < 0) { DIAG_FAIL(3); return false; }                               // 3: cabecera cortada

  uint16_t len = (uint16_t)len_lo | ((uint16_t)len_hi << 8);
  if (len > WIFI_PROTO_MAX_FRAME_PAYLOAD) { DIAG_FAIL(4); return false; }                                // 4: longitud absurda

  static uint8_t crcbuf[3 + WIFI_PROTO_MAX_FRAME_PAYLOAD];
  crcbuf[0] = (uint8_t)cmd;
  crcbuf[1] = (uint8_t)len_lo;
  crcbuf[2] = (uint8_t)len_hi;
  for (uint16_t i = 0; i < len; i++) {
    int bb = wifi_read_byte(byte_ms);
    if (bb < 0) { DIAG_FAIL(5); return false; }                                                          // 5: falta un byte de los datos
    crcbuf[3 + i] = (uint8_t)bb;
  }

  int crc_byte = wifi_read_byte(byte_ms);
  if (crc_byte < 0) { DIAG_FAIL(6); return false; }                                                      // 6: falta el CRC
  if ((uint8_t)crc_byte != wifi_proto_crc8(crcbuf, 3 + len)) { DIAG_FAIL(7); return false; }             // 7: CRC malo

  out->raw_cmd = (uint8_t)cmd;
  memcpy(out->payload, &crcbuf[3], len);
  out->len = len;
  return true;
}

WifiProtoResp wifi_client_request(uint8_t cmd, const uint8_t* payload, uint16_t len) {
  WifiProtoResp resp;
  resp.ok = false;
  resp.len = 0;

  static uint8_t buf[3 + WIFI_PROTO_MAX_FRAME_PAYLOAD];
  buf[0] = cmd;
  buf[1] = (uint8_t)(len & 0xFF);
  buf[2] = (uint8_t)(len >> 8);
  if (len > 0) memcpy(&buf[3], payload, len);
  uint8_t crc = wifi_proto_crc8(buf, 3 + len);

  const bool quick = (cmd == CMD_NET_POLL);
  const uint32_t byte_ms = quick ? WIFI_CLIENT_POLL_BYTE_TIMEOUT_MS : WIFI_CLIENT_BYTE_TIMEOUT_MS;
  const uint32_t sof_ms  = quick ? WIFI_CLIENT_POLL_SOF_TIMEOUT_MS : WIFI_CLIENT_SOF_SEARCH_TIMEOUT_MS;
  DIAG_FAIL(0);           // el motivo de la primera perdida se queda aunque el reintento salga bien

#if ESP_DIAG
  uint32_t diag_t0 = millis(); uint8_t diag_ferr = 0, diag_ajenas = 0, diag_tries = 0;
#endif
  for (int attempt = 0; attempt < WIFI_CLIENT_MAX_RETRIES; attempt++) {
#if ESP_DIAG
    diag_tries++;
#endif
    while (STM32_SERIAL.available()) STM32_SERIAL.read();   // limpia basura antes de mandar

    STM32_SERIAL.write(WIFI_PROTO_SOF);
    STM32_SERIAL.write(buf, 3 + len);
    STM32_SERIAL.write(crc);

    // El STM32 responde siempre con el mismo CMD que recibio, y las tramas no
    // llevan numero de peticion: si tardo mas que el timeout y se reintento, la
    // respuesta tardia de la peticion anterior (otro CMD, CRC correcto) llegaria
    // aqui como si fuera la de esta. Las de otro CMD se descartan.
    bool got = false;
    for (int stale = 0; stale < 4; stale++) {
      if (!wifi_client_read_frame_raw(&resp, byte_ms, sof_ms)) break;          // timeout -> reintenta
#if ESP_DIAG
      if (resp.raw_cmd == CMD_FRAME_ERROR) diag_ferr++; else if (resp.raw_cmd != cmd) diag_ajenas++;
#endif
      if (resp.raw_cmd == CMD_FRAME_ERROR) break;             // CRC mal en el STM32 -> reintenta
      if (resp.raw_cmd != cmd) continue;                      // respuesta tardia de otra peticion
      got = true;
      break;
    }
    if (!got) continue;
    resp.ok = true;
#if ESP_DIAG
    diag_req(cmd, diag_tries, millis() - diag_t0, true, diag_ferr, diag_ajenas);
#endif
    return resp;
  }
#if ESP_DIAG
  diag_req(cmd, diag_tries, millis() - diag_t0, false, diag_ferr, diag_ajenas);
#endif
  return resp;   // ok=false, se agotaron los reintentos
}

static WifiProtoResp wifi_client_request_path(uint8_t cmd, const char* path, const uint8_t* extra, uint16_t extra_len) {
  uint8_t buf[WIFI_PROTO_MAX_FRAME_PAYLOAD];
  uint8_t path_len = (uint8_t)strnlen(path, WIFI_PROTO_MAX_PATH - 1);
  buf[0] = path_len;
  memcpy(&buf[1], path, path_len);
  uint16_t pos = 1 + path_len;
  if (extra_len > 0) { memcpy(&buf[pos], extra, extra_len); pos += extra_len; }
  return wifi_client_request(cmd, buf, pos);
}

bool wifi_client_ping(uint8_t* out_fw_version) {
  WifiProtoResp r = wifi_client_request(CMD_PING, NULL, 0);
  if (!r.ok || r.len < 1) return false;
  *out_fw_version = r.payload[0];
  return true;
}

bool wifi_client_list_dir(const char* path, void (*on_entry)(const WifiDirEntry&)) {
  uint8_t start_index = 0;
  bool has_more = true;
  while (has_more) {
    uint8_t extra[1] = { start_index };
    WifiProtoResp r = wifi_client_request_path(CMD_LIST_DIR, path, extra, 1);
    if (!r.ok || r.len < 3 || r.payload[0] != ST_OK) return false;

    uint8_t count = r.payload[1];
    has_more = r.payload[2] != 0;
    uint16_t pos = 3;
    for (uint8_t i = 0; i < count; i++) {
      uint8_t name_len = r.payload[pos++];
      if ((uint32_t)pos + name_len + 5 > r.len) return false;   // entrada truncada o trama incoherente
      WifiDirEntry e;
      uint8_t n = name_len < sizeof(e.name) - 1 ? name_len : sizeof(e.name) - 1;
      memcpy(e.name, &r.payload[pos], n); e.name[n] = 0;
      pos += name_len;
      memcpy(&e.size, &r.payload[pos], 4); pos += 4;
      e.is_dir = (r.payload[pos] & WIFI_PROTO_FLAG_DIR) != 0; pos++;
      on_entry(e);
      start_index++;
    }
  }
  return true;
}

bool wifi_client_write_open(const char* path, uint8_t* out_handle) {
  WifiProtoResp r = wifi_client_request_path(CMD_WRITE_OPEN, path, NULL, 0);
  if (!r.ok || r.len < 2 || r.payload[0] != ST_OK) return false;
  *out_handle = r.payload[1];
  return true;
}

bool wifi_client_write_chunk(uint8_t handle, const uint8_t* data, uint16_t len) {
  uint8_t buf[3 + WIFI_PROTO_CHUNK_SIZE];
  buf[0] = handle;
  buf[1] = (uint8_t)(len & 0xFF);
  buf[2] = (uint8_t)(len >> 8);
  memcpy(&buf[3], data, len);
  WifiProtoResp r = wifi_client_request(CMD_WRITE_CHUNK, buf, 3 + len);
  return r.ok && r.len >= 1 && r.payload[0] == ST_OK;
}

bool wifi_client_write_close(uint8_t handle, uint32_t* out_total) {
  uint8_t buf[1] = { handle };
  WifiProtoResp r = wifi_client_request(CMD_WRITE_CLOSE, buf, 1);
  if (!r.ok || r.len < 5 || r.payload[0] != ST_OK) return false;
  memcpy(out_total, &r.payload[1], 4);
  return true;
}

bool wifi_client_write_sync(uint8_t handle) {
  uint8_t buf[1] = { handle };
  WifiProtoResp r = wifi_client_request(CMD_WRITE_SYNC, buf, 1);
  return r.ok && r.len >= 1 && r.payload[0] == ST_OK;
}

bool wifi_client_read_open(const char* path, uint8_t* out_handle, uint32_t* out_size) {
  WifiProtoResp r = wifi_client_request_path(CMD_READ_OPEN, path, NULL, 0);
  if (!r.ok || r.len < 6 || r.payload[0] != ST_OK) return false;
  *out_handle = r.payload[1];
  memcpy(out_size, &r.payload[2], 4);
  return true;
}

bool wifi_client_read_chunk(uint8_t handle, uint32_t offset, uint8_t* buf, uint16_t* out_len, bool* out_eof) {
  uint8_t req[5];
  req[0] = handle;
  memcpy(&req[1], &offset, 4);
  WifiProtoResp r = wifi_client_request(CMD_READ_CHUNK, req, 5);
  if (!r.ok || r.len < 4 || r.payload[0] != ST_OK) return false;
  uint16_t dlen; memcpy(&dlen, &r.payload[1], 2);
  // dlen viene de la trama: se comprueba contra la trama (datos + byte de eof) y
  // contra el buffer del llamador (todos son de WIFI_PROTO_CHUNK_SIZE).
  if (dlen > WIFI_PROTO_CHUNK_SIZE || (uint32_t)dlen + 4 > r.len) return false;
  memcpy(buf, &r.payload[3], dlen);
  *out_len = dlen;
  *out_eof = r.payload[3 + dlen] != 0;
  return true;
}

bool wifi_client_read_close(uint8_t handle) {
  uint8_t buf[1] = { handle };
  WifiProtoResp r = wifi_client_request(CMD_READ_CLOSE, buf, 1);
  return r.ok && r.len >= 1 && r.payload[0] == ST_OK;
}

bool wifi_client_delete(const char* path) {
  WifiProtoResp r = wifi_client_request_path(CMD_DELETE, path, NULL, 0);
  return r.ok && r.len >= 1 && r.payload[0] == ST_OK;
}

bool wifi_client_mkdir(const char* path) {
  WifiProtoResp r = wifi_client_request_path(CMD_MKDIR, path, NULL, 0);
  return r.ok && r.len >= 1 && r.payload[0] == ST_OK;
}

// Descarga un fichero de texto pequeno entero a un String via READ_OPEN/
// READ_CHUNK/READ_CLOSE. *out_existed queda a false (return true) si el
// fichero simplemente no existe/no se pudo abrir - no es un error de
// transporte. Devuelve false solo ante un fallo de transporte real.
static bool download_text_file(const char* path, String* content, bool* out_existed) {
  *out_existed = false;

  uint8_t handle;
  uint32_t file_size;
  if (!wifi_client_read_open(path, &handle, &file_size)) return true;
  *out_existed = true;

  content->reserve(file_size + 1);
  uint32_t offset = 0;
  bool eof = false;
  bool transport_ok = true;

  while (!eof) {
    uint8_t buf[WIFI_PROTO_CHUNK_SIZE];
    uint16_t len = 0;
    if (!wifi_client_read_chunk(handle, offset, buf, &len, &eof)) {
      transport_ok = false;
      break;
    }
    for (uint16_t i = 0; i < len; i++) *content += (char)buf[i];
    offset += len;
    if (len == 0 && !eof) break; // evita bucle infinito si el STM32 no marca eof
  }

  wifi_client_read_close(handle);
  return transport_ok;
}

bool wifi_client_read_wifi_networks(WifiNetwork* networks, uint8_t max_networks, uint8_t* out_count) {
  *out_count = 0;

  String content;
  bool existed;
  if (!download_text_file("/SYS/WIFI.CFG", &content, &existed)) return false;
  if (!existed) return true;

  // Formato: pares de lineas SSID/password, uno tras otro. Lineas separadas
  // por \n (y \r opcional, por si el fichero se edito en Windows).
  uint8_t stored = 0;
  int pos = 0;
  int len = content.length();
  String lines[2];
  int line_idx = 0;

  while (pos <= len && stored < max_networks) {
    int nl = content.indexOf('\n', pos);
    String line = (nl == -1) ? content.substring(pos) : content.substring(pos, nl);
    while (line.endsWith("\r")) line.remove(line.length() - 1);

    if (line.length() > 0) {
      lines[line_idx++] = line;
      if (line_idx == 2) {
        lines[0].toCharArray(networks[stored].ssid, sizeof(networks[stored].ssid));
        lines[1].toCharArray(networks[stored].pass, sizeof(networks[stored].pass));
        stored++;
        line_idx = 0;
      }
    }

    if (nl == -1) break;
    pos = nl + 1;
  }

  *out_count = stored;
  return true;
}

bool wifi_client_read_ntp_config(NtpConfig* out) {
  out->sync_enabled = false;
  out->server[0] = 0;
  out->utc_offset_hours = 0;
  out->dst = false;

  String content;
  bool existed;
  if (!download_text_file("/SYS/NTP.CFG", &content, &existed)) return false;
  if (!existed) return true; // sin fichero = NTP desactivado, no es un error

  // Formato: lineas CLAVE=VALOR, una por linea (ver WIFI_PROTOCOL.h).
  int pos = 0;
  int len = content.length();
  while (pos <= len) {
    int nl = content.indexOf('\n', pos);
    String line = (nl == -1) ? content.substring(pos) : content.substring(pos, nl);
    while (line.endsWith("\r")) line.remove(line.length() - 1);

    int eq = line.indexOf('=');
    if (eq > 0) {
      String key = line.substring(0, eq);
      String val = line.substring(eq + 1);
      key.trim(); val.trim();
      key.toUpperCase();
      if (key == "SERVER") {
        val.toCharArray(out->server, sizeof(out->server));
      } else if (key == "MODE") {
        val.toUpperCase();
        out->sync_enabled = (val == "SERVER");
      } else if (key == "UTCOFFSET") {
        out->utc_offset_hours = val.toInt();
      } else if (key == "DST") {
        out->dst = (val.toInt() != 0);
      }
    }

    if (nl == -1) break;
    pos = nl + 1;
  }

  return true;
}

bool wifi_client_set_time(uint8_t year, uint8_t month, uint8_t day,
                           uint8_t hour, uint8_t minute, uint8_t second) {
  uint8_t buf[6] = { year, month, day, hour, minute, second };
  WifiProtoResp r = wifi_client_request(CMD_SET_TIME, buf, 6);
  return r.ok && r.len >= 1 && r.payload[0] == ST_OK;
}

bool wifi_client_read_text_file(const char* path, String* out) {
  out->remove(0, out->length());
  bool existed;
  return download_text_file(path, out, &existed);
}

// --- Puente de red ----------------------------------------------------------
// seq_out: el bit que ponemos en los datos que mandamos, hasta que el STM32
// nos lo confirma. seq_in_last: la ultima secuencia que le hemos ACEPTADO.
//
// seq_in_last arranca en true al CONTRARIO del primer valor que mandara el
// STM32 (false); si empezara igual, su primera trama con datos se tomaria por
// un reintento y se tiraria. Mismo cuidado que net_seq_in en el otro extremo.
static bool net_seq_out     = false;
static bool net_seq_in_last = true;

bool wifi_client_net_poll(uint8_t status,
                          const uint8_t* tx, uint16_t tx_len, bool* out_tx_accepted,
                          uint8_t* rx, uint16_t* out_rx_len,
                          uint8_t* out_rx_free) {
  *out_tx_accepted = false;
  *out_rx_len      = 0;
  *out_rx_free     = 0;

  if (tx_len > WIFI_PROTO_NET_CHUNK) tx_len = WIFI_PROTO_NET_CHUNK;

  static uint8_t req[3 + WIFI_PROTO_NET_CHUNK];
  req[0] = (uint8_t)((net_seq_out     ? WIFI_PROTO_NET_SEQ : 0) |
                     (net_seq_in_last ? WIFI_PROTO_NET_ACK : 0));
  req[1] = status;
  req[2] = (uint8_t)tx_len;
  if (tx_len > 0) memcpy(&req[3], tx, tx_len);

  // Los reintentos internos de wifi_client_request reenvian este MISMO buffer,
  // con la misma secuencia, que es justo lo que el otro lado espera para no
  // duplicar nada.
  WifiProtoResp r = wifi_client_request(CMD_NET_POLL, req, (uint16_t)(3 + tx_len));
  if (!r.ok || r.len < 3) return false;

  uint8_t  resp_flags = r.payload[0];
  *out_rx_free        = r.payload[1];
  uint16_t rx_len     = r.payload[2];
  if ((uint16_t)(3 + rx_len) > r.len) return false;

  bool resp_seq = (resp_flags & WIFI_PROTO_NET_SEQ) != 0;
  bool resp_ack = (resp_flags & WIFI_PROTO_NET_ACK) != 0;

  // Nos confirma lo que mandamos: ya podemos soltarlo y alternar el bit.
  if (tx_len > 0 && resp_ack == net_seq_out) {
    *out_tx_accepted = true;
    net_seq_out = !net_seq_out;
  } else if (tx_len == 0) {
    *out_tx_accepted = true;   // nada que confirmar
  }

  // Datos del Z80: solo son nuevos si la secuencia cambio. OJO: cuando
  // rx_len==0 NO se toca net_seq_in_last (antes se sincronizaba igual, y
  // eso bloqueaba el primer mensaje real -- ver el comentario gemelo en
  // handle_net_poll/WIFI_HANDLER.cpp del STM32, misma causa).
  if (rx_len > 0 && resp_seq != net_seq_in_last) {
    memcpy(rx, &r.payload[3], rx_len);
    *out_rx_len     = rx_len;
    net_seq_in_last = resp_seq;
  }
  return true;
}

bool wifi_client_dbg(uint8_t id, uint32_t seq, const char* cmd,
                     uint8_t* out_state, uint32_t* out_from, uint16_t* out_view_ver,
                     uint8_t* out_data, uint16_t* out_len) {
  *out_len = 0;
  uint8_t req[6 + WIFI_PROTO_DBG_CMD];
  size_t cl = (id && cmd) ? strnlen(cmd, WIFI_PROTO_DBG_CMD) : 0;
  req[0] = id;
  memcpy(&req[1], &seq, 4);
  req[5] = (uint8_t)cl;
  if (cl) memcpy(&req[6], cmd, cl);
  WifiProtoResp r = wifi_client_request(CMD_DBG, req, (uint16_t)(6 + cl));
  if (!r.ok || r.len < 10 || r.payload[0] != ST_OK) return false;
  *out_state = r.payload[1];
  memcpy(out_from, &r.payload[2], 4);
  memcpy(out_view_ver, &r.payload[6], 2);
  uint16_t n = r.payload[8] | (r.payload[9] << 8);
  if (n > WIFI_PROTO_DBG_OUT || (uint16_t)(10 + n) > r.len) return false;
  memcpy(out_data, &r.payload[10], n);
  *out_len = n;
  return true;
}

bool wifi_client_dbg_keys(const uint8_t* rows) {
  WifiProtoResp r = wifi_client_request(CMD_DBG_KEYS, rows, 8);
  return r.ok && r.len >= 1 && r.payload[0] == ST_OK;
}

bool wifi_client_dbg_cap(uint8_t op, uint16_t arg, uint8_t* out_state, uint16_t* out_total,
                         uint16_t* out_off, uint8_t* out_data, uint16_t* out_len) {
  *out_len = 0;
  uint8_t req[3] = { op, (uint8_t)(arg & 0xFF), (uint8_t)(arg >> 8) };
  WifiProtoResp r = wifi_client_request(CMD_DBG_CAP, req, 3);
  if (!r.ok || r.len < 7 || r.payload[0] != ST_OK) return false;
  *out_state = r.payload[1];
  memcpy(out_total, &r.payload[2], 2);
  memcpy(out_off, &r.payload[4], 2);
  uint16_t n = r.payload[6];
  if (n > WIFI_PROTO_CAP_CHUNK || (uint16_t)(7 + n) > r.len) return false;
  memcpy(out_data, &r.payload[7], n);
  *out_len = n;
  return true;
}

bool wifi_client_dbg_view(uint8_t part, uint16_t* out_ver, uint16_t* out_total,
                          uint8_t* out_data, uint16_t* out_len) {
  *out_len = 0;
  uint8_t req[1] = { part };
  WifiProtoResp r = wifi_client_request(CMD_DBG_VIEW, req, 1);
  if (!r.ok || r.len < 6 || r.payload[0] != ST_OK) return false;
  memcpy(out_ver, &r.payload[1], 2);
  memcpy(out_total, &r.payload[3], 2);
  uint16_t n = r.payload[5];
  if (n > WIFI_PROTO_DBG_OUT || (uint16_t)(6 + n) > r.len) return false;
  memcpy(out_data, &r.payload[6], n);
  *out_len = n;
  return true;
}
