#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <ESPmDNS.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

#include "WIFI_CLIENT.h"
#include "LOGO.h"
#include "VERSION_CHECK.h"
#include "NET_BRIDGE.h"

// SD_LOG.h ultimo a proposito: redefine "Serial" como macro para el resto
// de este fichero (espeja toda la salida a /SYS/ESP32_LOG.TXT ademas del
// USB) - no debe afectar a las cabeceras de las librerias de arriba.
#include "SD_LOG.h"

// Fixed path on the SD card for a pending ESP32 firmware image. If present at
// boot, it gets flashed via the Update library and deleted on success - same
// pattern as the STM32's own SD81.MCS FPGA self-update.
// Raiz de la SD, no /SYS - misma convencion que firmware.bin (STM32) y
// SD81.MCS (FPGA), los otros dos ficheros de auto-actualizacion.
#define FW_UPDATE_PATH        "/ESP32_FW.BIN"
#define FW_UPDATE_FAILED_PATH "/ESP32_FW_FAILED.TXT"

#define MDNS_HOSTNAME "sd81booster"   // reachable at http://sd81booster.local

// WiFi module (ESP32-C3) own firmware version, shown on the web UI and in
// /MAN/IP.TXT. Bump this before building a new ESP32_FW.BIN release.
#define WIFI_FW_VERSION "1.2"

WebServer server(80);

bool     g_upload_active = false;
uint8_t  g_upload_handle = 0;
bool     g_upload_ok = false;

// true = no hace falta reintentar la sincronizacion NTP mas (exito, o
// MODE=LOCAL a proposito). false = el primer intento fallo (servidor
// inalcanzable, transporte...) y check_ntp_retry() lo repetira solo, cada
// minuto, mientras haya WiFi -- ver sync_time_from_ntp() mas abajo.
bool     ntp_stop_retrying = false;

String   g_list_html;

String join_path(const String& dir, const String& name) {
  if (dir == "/") return "/" + name;
  return dir + "/" + name;
}

String parent_path(const String& dir) {
  if (dir == "/") return "/";
  int slash = dir.lastIndexOf('/');
  if (slash <= 0) return "/";
  return dir.substring(0, slash);
}

// Percent-encoding for values placed inside a query string (distinct from
// html_escape, which is for values placed inside HTML markup).
String url_encode(const String& s) {
  String out;
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') {
      out += c;
    } else {
      char buf[4];
      snprintf(buf, sizeof(buf), "%%%02X", (unsigned char)c);
      out += buf;
    }
  }
  return out;
}

String html_escape(const String& s) {
  String out = s;
  out.replace("&", "&amp;");
  out.replace("<", "&lt;");
  out.replace(">", "&gt;");
  return out;
}

// current_dir is kept separately because the wifi_client_list_dir callback
// doesn't support its own context (only the entry)
String g_current_dir;

String delete_form(const String& fullPath) {
  return "<form method=\"POST\" action=\"/delete\" style=\"display:inline\" "
         "onsubmit=\"return confirm('Delete " + html_escape(fullPath) + "?');\">"
         "<input type=\"hidden\" name=\"path\" value=\"" + html_escape(fullPath) + "\">"
         "<input type=\"hidden\" name=\"dir\" value=\"" + html_escape(g_current_dir) + "\">"
         "<input type=\"submit\" value=\"Delete\">"
         "</form>";
}

void append_entry_to_list_html(const WifiDirEntry& e) {
  String name = html_escape(String(e.name));
  String target = join_path(g_current_dir, e.name);
  if (e.is_dir) {
    g_list_html += "<li><a href=\"/list?path=" + target + "\">[" + name + "]</a> " +
                   delete_form(target) + "</li>";
  } else {
    g_list_html += "<li>" + name + " (" + String(e.size) + " bytes) " +
                   "<a href=\"/download?path=" + target + "\">Download</a> " +
                   delete_form(target) + "</li>";
  }
}

void handleList() {
  String dir = server.hasArg("path") ? server.arg("path") : "/";
  if (dir.length() == 0) dir = "/";
  g_current_dir = dir;

  g_list_html = "<style>"
                "body{font-family:sans-serif;max-width:600px;margin:0 auto;padding:0 10px}"
                "h1{text-align:center;margin-top:0}"
                ".logo{display:block;margin:10px auto}"
                "</style>";
  g_list_html += "<img class=\"logo\" src=\"/logo.png\" alt=\"SD81 Booster\">";
  g_list_html += "<h1>File server</h1>";
  g_list_html += "<p style=\"text-align:center;color:#888\"><small>WiFi module firmware v" WIFI_FW_VERSION "</small></p>";
  g_list_html += "<p style=\"text-align:center\"><a href=\"/ntp\">NTP time settings</a> | <a href=\"/update\">Firmware update</a></p>";
  g_list_html += "<p>Directory: <b>" + html_escape(dir) + "</b></p>";
  if (dir != "/") {
    g_list_html += "<p><a href=\"/list?path=" + parent_path(dir) + "\">.. (up one level)</a></p>";
  }

  g_list_html += "<form method=\"POST\" action=\"/upload\" enctype=\"multipart/form-data\">"
                 "<input type=\"hidden\" name=\"dir\" value=\"" + html_escape(dir) + "\">"
                 "<input type=\"file\" name=\"file\" multiple>"
                 "<input type=\"submit\" value=\"Upload files\">"
                 "</form>";

  g_list_html += "<form method=\"POST\" action=\"/upload\" enctype=\"multipart/form-data\">"
                 "<input type=\"hidden\" name=\"dir\" value=\"" + html_escape(dir) + "\">"
                 "<input type=\"file\" name=\"file\" webkitdirectory multiple>"
                 "<input type=\"submit\" value=\"Upload whole folder\">"
                 "</form>"
                 "<p><small>Folder upload only works in desktop browsers "
                 "(Chrome/Edge/Firefox) - mobile browsers usually don't allow picking folders.</small></p>";

  g_list_html += "<form method=\"POST\" action=\"/mkdir\">"
                 "<input type=\"hidden\" name=\"dir\" value=\"" + html_escape(dir) + "\">"
                 "<input type=\"text\" name=\"name\" placeholder=\"folder name\">"
                 "<input type=\"submit\" value=\"Create folder here\">"
                 "</form><hr><ul>";

  if (!wifi_client_list_dir(dir.c_str(), append_entry_to_list_html)) {
    server.send(502, "text/html", "<h1>Error</h1><p>Could not list the SD card (no response from the STM32).</p>");
    return;
  }

  g_list_html += "</ul>";
  server.send(200, "text/html", g_list_html);
}

void handleRoot() {
  server.sendHeader("Location", "/list?path=/");
  server.send(303);
}

void handleLogo() {
  server.send_P(200, "image/png", (const char*)LOGO_PNG, LOGO_PNG_LEN);
}

void handleUpload() {
  HTTPUpload& upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    g_upload_ok = false;
    String dir = server.hasArg("dir") ? server.arg("dir") : "/";
    if (dir.length() == 0) dir = "/";
    // upload.filename may include subfolders (e.g. "MyFolder/sub/game.p") if
    // the upload came from a whole-folder picker (webkitdirectory) - they
    // need to be created before opening the file; sd.mkdir creates any
    // missing parent directories.
    String filename = join_path(dir, upload.filename);
    int lastSlash = filename.lastIndexOf('/');
    String fileDir = lastSlash > 0 ? filename.substring(0, lastSlash) : "/";
    if (fileDir != dir) {
      wifi_client_mkdir(fileDir.c_str());   // no-op if it already exists
    }
    Serial.print("Uploading: "); Serial.println(filename);
    g_upload_active = wifi_client_write_open(filename.c_str(), &g_upload_handle);
    if (!g_upload_active) Serial.println("Error: WRITE_OPEN failed");

  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (!g_upload_active) return;
    const uint8_t* p = upload.buf;
    size_t remaining = upload.currentSize;
    while (remaining > 0) {
      uint16_t n = remaining > WIFI_PROTO_CHUNK_SIZE ? WIFI_PROTO_CHUNK_SIZE : (uint16_t)remaining;
      if (!wifi_client_write_chunk(g_upload_handle, p, n)) {
        Serial.println("Error: WRITE_CHUNK failed");
        g_upload_active = false;
        return;
      }
      p += n;
      remaining -= n;
    }

  } else if (upload.status == UPLOAD_FILE_END) {
    if (!g_upload_active) return;
    uint32_t total = 0;
    if (wifi_client_write_close(g_upload_handle, &total)) {
      Serial.print("Upload complete: "); Serial.print(total); Serial.println(" bytes");
      g_upload_ok = true;
    } else {
      Serial.println("Error: WRITE_CLOSE failed");
    }
    g_upload_active = false;
  }
}

void handleUploadDone() {
  String dir = server.hasArg("dir") ? server.arg("dir") : "/";
  if (dir.length() == 0) dir = "/";
  // La subida manual desde el explorador de ficheros (formulario HTML normal)
  // espera la redireccion de siempre a /list. El JS de /update en cambio
  // manda un campo extra "ajax=1" y necesita el resultado real: /upload
  // devolvia SIEMPRE 303 pasara lo que pasara (exito o fallo silencioso en
  // el STM32), asi que el instalador nunca se enteraba de que no se habia
  // escrito nada en la SD.
  if (server.hasArg("ajax")) {
    server.send(g_upload_ok ? 200 : 500, "text/plain", g_upload_ok ? "ok" : "upload failed");
    return;
  }
  server.sendHeader("Location", "/list?path=" + dir);
  server.send(303);
}

void handleMkdir() {
  String dir = server.hasArg("dir") ? server.arg("dir") : "/";
  if (dir.length() == 0) dir = "/";
  String name = server.hasArg("name") ? server.arg("name") : "";

  if (name.length() > 0) {
    String path = join_path(dir, name);
    if (!wifi_client_mkdir(path.c_str())) {
      Serial.println("Error: MKDIR failed");
    }
  }

  server.sendHeader("Location", "/list?path=" + dir);
  server.send(303);
}

void handleDownload() {
  String path = server.hasArg("path") ? server.arg("path") : "";
  if (path.length() == 0) {
    server.send(400, "text/plain", "Missing path parameter");
    return;
  }

  uint8_t handle;
  uint32_t size;
  if (!wifi_client_read_open(path.c_str(), &handle, &size)) {
    server.send(502, "text/html", "<h1>Error</h1><p>Could not open the file (no response from the STM32).</p>");
    return;
  }

  String filename = path.substring(path.lastIndexOf('/') + 1);
  server.sendHeader("Content-Disposition", "attachment; filename=\"" + filename + "\"");
  server.setContentLength(size);
  server.send(200, "application/octet-stream", "");

  uint8_t buf[WIFI_PROTO_CHUNK_SIZE];
  uint32_t offset = 0;
  bool eof = false;
  while (!eof && offset < size) {
    uint16_t n = 0;
    if (!wifi_client_read_chunk(handle, offset, buf, &n, &eof)) break;
    if (n == 0) break;
    server.sendContent((const char*)buf, n);
    offset += n;
  }

  wifi_client_read_close(handle);
}

// El navegador no puede hacer fetch() directo a los assets de una release
// de GitHub: la URL de descarga redirige a
// release-assets.githubusercontent.com (blob de Azure), que no manda
// Access-Control-Allow-Origin - un fetch() cruzado ahi falla con "Failed to
// fetch" aunque el fichero se descargue bien por cualquier otra via
// (curl, navegacion normal...). Este endpoint hace de proxy: el ESP32 SI
// puede pedirlo (no esta sujeto a CORS, no es un navegador) y retransmite
// los bytes tal cual a quien pidio /proxy, que al ser mismo origen no
// tropieza con CORS. No decodifica ni escribe nada a la SD - el trabajo
// pesado (ZIP+inflate+subida) lo sigue haciendo el JS del navegador.
void handleProxyDownload() {
  if (!server.hasArg("url")) {
    server.send(400, "text/plain", "Missing url parameter");
    return;
  }
  String url = server.arg("url");
  // Solo se permite reenviar hacia GitHub - evita que este endpoint se
  // pueda usar como proxy abierto hacia cualquier URL.
  if (!url.startsWith("https://github.com/")) {
    server.send(403, "text/plain", "URL not allowed");
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  if (!http.begin(client, url)) {
    server.send(502, "text/plain", "Could not connect to GitHub");
    return;
  }
  http.addHeader("User-Agent", "SD81Booster-ESP32");
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.print("Proxy: upstream HTTP "); Serial.println(code);
    http.end();
    server.send(502, "text/plain", "Upstream HTTP error " + String(code));
    return;
  }

  int remaining = http.getSize(); // -1 si desconocido (chunked)
  if (remaining >= 0) server.setContentLength(remaining);
  server.send(200, "application/octet-stream", "");

  WiFiClient* stream = http.getStreamPtr();
  uint8_t buf[1024];
  while (http.connected() && remaining != 0) {
    size_t avail = stream->available();
    if (avail == 0) { delay(1); continue; }
    size_t want = avail > sizeof(buf) ? sizeof(buf) : avail;
    int n = stream->readBytes(buf, want);
    if (n <= 0) break;
    server.sendContent((const char*)buf, n);
    if (remaining > 0) remaining -= n;
  }
  http.end();
}

void handleDelete() {
  String path = server.hasArg("path") ? server.arg("path") : "";
  String dir = server.hasArg("dir") ? server.arg("dir") : "/";
  if (dir.length() == 0) dir = "/";

  if (path.length() > 0) {
    if (!wifi_client_delete(path.c_str())) {
      Serial.println("Error: DELETE failed");
    }
  }

  server.sendHeader("Location", "/list?path=" + dir);
  server.send(303);
}

// --- Puente de red (BBS/telnet) --------------------------------------------
// De momento esta es la unica via para abrir la conexion. A partir de la fase
// 2.5 se podra tambien con ATDT desde el propio CP/M, y las dos manipulan el
// mismo estado del ESP32.
static const char* net_state_text(uint8_t st) {
  switch (st) {
    case NET_ST_CONNECTING: return "connecting";
    case NET_ST_CONNECTED:  return "connected";
    case NET_ST_ERROR:      return "error";
    default:                return "not connected";
  }
}

void handleTelnetPage() {
  String html = "<style>"
                "body{font-family:sans-serif;max-width:600px;margin:0 auto;padding:0 10px}"
                "h1{text-align:center;margin-top:0}"
                "label{display:block;margin-top:12px}"
                "</style>";
  html += "<h1>BBS / telnet</h1>";
  html += "<p><a href=\"/list?path=/\">&laquo; Back to file server</a></p>";

  uint8_t st = net_bridge_state();
  html += "<p>Status: <b>" + String(net_state_text(st)) + "</b>";
  if (st == NET_ST_CONNECTED)
    html += " &mdash; " + html_escape(String(net_bridge_host())) + ":" + String(net_bridge_port());
  html += "</p>";

  html += "<form method=\"POST\" action=\"/telnet/connect\">";
  html += "<label>Host<br><input type=\"text\" name=\"host\" value=\"" +
          html_escape(String(net_bridge_host())) + "\" placeholder=\"bbs.example.com\"></label>";
  html += "<label>Port<br><input type=\"number\" name=\"port\" value=\"" +
          String(net_bridge_port() ? net_bridge_port() : 23) + "\"></label>";
  html += "<p><input type=\"submit\" value=\"Connect\"></p>";
  html += "</form>";
  html += "<form method=\"POST\" action=\"/telnet/disconnect\">";
  html += "<p><input type=\"submit\" value=\"Disconnect\"></p>";
  html += "</form>";
  server.send(200, "text/html", html);
}

void handleTelnetConnect() {
  String host = server.hasArg("host") ? server.arg("host") : "";
  host.trim();
  uint16_t port = server.hasArg("port") ? (uint16_t)server.arg("port").toInt() : 23;
  if (host.length() > 0 && port > 0) net_bridge_connect(host.c_str(), port);
  server.sendHeader("Location", "/telnet");
  server.send(303);
}

void handleTelnetDisconnect() {
  net_bridge_disconnect();
  server.sendHeader("Location", "/telnet");
  server.send(303);
}

void handleNtpPage() {
  NtpConfig cfg;
  wifi_client_read_ntp_config(&cfg); // on transport error just shows a blank/default form

  String html = "<style>"
                "body{font-family:sans-serif;max-width:600px;margin:0 auto;padding:0 10px}"
                "h1{text-align:center;margin-top:0}"
                "label{display:block;margin-top:12px}"
                "</style>";
  html += "<h1>NTP time settings</h1>";
  html += "<p><a href=\"/list?path=/\">&laquo; Back to file server</a></p>";

  if (server.hasArg("synced")) {
    html += server.arg("synced") == "1"
      ? "<p style=\"color:green\">Time synced successfully.</p>"
      : "<p style=\"color:red\">Sync failed - check the server address and that the network can reach it.</p>";
  }

  html += "<form method=\"POST\" action=\"/ntp/save\">";
  html += "<label>NTP server<br><input type=\"text\" name=\"server\" value=\"" +
          html_escape(String(cfg.server)) + "\" placeholder=\"pool.ntp.org\"></label>";
  html += String("<label><input type=\"checkbox\" name=\"enabled\" value=\"1\"") +
          (cfg.sync_enabled ? " checked" : "") + "> Sync automatically at boot</label>";
  html += "<label>UTC offset (hours)<br><input type=\"number\" name=\"utcoffset\" value=\"" +
          String(cfg.utc_offset_hours) + "\"></label>";
  html += String("<label><input type=\"checkbox\" name=\"dst\" value=\"1\"") +
          (cfg.dst ? " checked" : "") + "> Summer time (DST, +1h)</label>";
  html += "<p><input type=\"submit\" value=\"Save\"></p>";
  html += "</form>";

  html += "<form method=\"POST\" action=\"/ntp/sync\">"
          "<input type=\"submit\" value=\"Sync time now\"></form>";

  time_t now = time(nullptr);
  if (now > 24L * 3600L) { // sanity check: skip if the ESP32 clock still looks unset (epoch 1970)
    struct tm t;
    localtime_r(&now, &t);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &t);
    html += "<p><small>ESP32 system time (last successful sync): " + String(buf) + "</small></p>";
  }

  server.send(200, "text/html", html);
}

void handleNtpSave() {
  String server_addr = server.hasArg("server") ? server.arg("server") : "";
  server_addr.replace("\n", ""); server_addr.replace("\r", ""); server_addr.trim();
  bool enabled = server.hasArg("enabled");
  int utcoffset = server.hasArg("utcoffset") ? server.arg("utcoffset").toInt() : 0;
  bool dst = server.hasArg("dst");

  String content = "SERVER=" + server_addr + "\n";
  content += "MODE="; content += (enabled ? "SERVER" : "LOCAL"); content += "\n";
  content += "UTCOFFSET=" + String(utcoffset) + "\n";
  content += "DST="; content += (dst ? "1" : "0"); content += "\n";

  uint8_t handle;
  if (wifi_client_write_open("/SYS/NTP.CFG", &handle)) {
    wifi_client_write_chunk(handle, (const uint8_t*)content.c_str(), content.length());
    uint32_t total;
    wifi_client_write_close(handle, &total);
  } else {
    Serial.println("Error: could not write /SYS/NTP.CFG");
  }

  server.sendHeader("Location", "/ntp");
  server.send(303);
}

void handleNtpSyncNow() {
  bool ok = sync_time_from_ntp(nullptr);
  if (ok) ntp_stop_retrying = true;   // exito manual: el reintento automatico ya no hace falta
  server.sendHeader("Location", String("/ntp?synced=") + (ok ? "1" : "0"));
  server.send(303);
}

// Resultado de la ultima comprobacion de actualizaciones, valido hasta el
// siguiente arranque - "/update/check" lo rellena, "/update" lo lee para
// generar el JS de instalacion. No hace falta persistirlo en la SD: el
// usuario comprueba y aplica en la misma sesion de navegador.
//
// La consulta y descarga van contra un MIRROR en GitHub del repo (ver
// VERSION_CHECK.h), no contra Codeberg directamente: Codeberg protege todo
// su dominio (API, descargas, vistas raw, hasta peticiones fetch() desde un
// navegador real) con un sistema anti-bot que bloquea cualquier cliente que
// no sea una navegacion de pagina completa - probado a mano contra curl,
// PowerShell y Chrome via fetch(), los tres bloqueados por igual (ver
// memoria del proyecto). GitHub no tiene ese problema.
bool         g_update_checked = false;
ReleaseInfo  g_latest_release;

// Un JS helper para meter un String de C++ como literal de cadena JS -
// distinto de html_escape (para marcado HTML): aqui hace falta escapar
// comillas y backslashes, no & < >.
String js_string_literal(const String& s) {
  String out = "\"";
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"' || c == '\\') out += '\\';
    out += c;
  }
  out += "\"";
  return out;
}

void handleUpdatePage() {
  String installed = version_check_get_installed();

  String html = "<style>"
                "body{font-family:sans-serif;max-width:600px;margin:0 auto;padding:0 10px}"
                "h1{text-align:center;margin-top:0}"
                "#log{background:#1e1e1e;color:#a8d8a8;font-family:monospace;font-size:12px;"
                "padding:8px;height:200px;overflow-y:auto;white-space:pre-wrap;display:none}"
                "</style>";
  html += "<h1>Firmware update</h1>";
  html += "<p><a href=\"/list?path=/\">&laquo; Back to file server</a></p>";
  html += "<p>Installed version: <b>" + (installed.length() ? html_escape(installed) : String("unknown (pre-1.1.0)")) + "</b></p>";

  if (server.hasArg("err")) {
    html += "<p style=\"color:red\">" + html_escape(server.arg("err")) + "</p>";
  }

  if (g_update_checked) {
    if (g_latest_release.tag == installed) {
      html += "<p style=\"color:green\">Already up to date.</p>";
      html += "<form method=\"POST\" action=\"/update/check\"><input type=\"submit\" value=\"Check again\"></form>";
    } else {
      html += "<p>Update available: <b>" + html_escape(g_latest_release.tag) + "</b></p>";
      // La descarga, descompresion (DecompressionStream nativo del
      // navegador) y subida (reutilizando /upload, ya probado) se hacen
      // aqui en JS, en el propio PC del usuario - el ESP32 solo recibe los
      // ficheros ya descomprimidos uno a uno, igual que en una subida
      // manual. Evita que el ESP32 tenga que descargar+descomprimir+
      // escribir un ZIP entero de forma sincrona (con hardware real esto
      // superaba el timeout del watchdog y reiniciaba el ESP32 a medias -
      // ver memoria del proyecto).
      html += "<p><button id=\"installBtn\" onclick=\"installUpdate()\">Install update</button></p>";
      html += "<pre id=\"log\"></pre>";
      html += "<script>\n"
              "const FW_URL = " + js_string_literal(g_latest_release.firmware_zip_url) + ";\n"
              "const SD_URL = " + js_string_literal(g_latest_release.sdcontent_zip_url) + ";\n"
              "const TAG = " + js_string_literal(g_latest_release.tag) + ";\n"
              "\n"
              "function rd16(dv,o){return dv.getUint16(o,true);}\n"
              "function rd32(dv,o){return dv.getUint32(o,true);}\n"
              "\n"
              "function parseZipEntries(bytes){\n"
              "  const dv = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);\n"
              "  const EOCD = 0x06054b50;\n"
              "  let eocd = -1;\n"
              "  const minPos = Math.max(0, bytes.length - 65557);\n"
              "  for (let p = bytes.length - 22; p >= minPos; p--) {\n"
              "    if (rd32(dv,p) === EOCD) { eocd = p; break; }\n"
              "  }\n"
              "  if (eocd < 0) throw new Error('EOCD not found');\n"
              "  const total = rd16(dv, eocd+10);\n"
              "  const cdOff = rd32(dv, eocd+16);\n"
              "  const entries = [];\n"
              "  let p = cdOff;\n"
              "  for (let i = 0; i < total; i++) {\n"
              "    if (rd32(dv,p) !== 0x02014b50) throw new Error('bad central dir entry');\n"
              "    const method = rd16(dv, p+10);\n"
              "    const csize = rd32(dv, p+20);\n"
              "    const nlen = rd16(dv, p+28);\n"
              "    const elen = rd16(dv, p+30);\n"
              "    const clen = rd16(dv, p+32);\n"
              "    const localOffset = rd32(dv, p+42);\n"
              "    const name = new TextDecoder().decode(bytes.subarray(p+46, p+46+nlen));\n"
              "    entries.push({name, method, csize, localOffset});\n"
              "    p += 46 + nlen + elen + clen;\n"
              "  }\n"
              "  return entries;\n"
              "}\n"
              "\n"
              "async function extractEntry(bytes, entry){\n"
              "  const dv = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);\n"
              "  const lp = entry.localOffset;\n"
              "  if (rd32(dv,lp) !== 0x04034b50) throw new Error('bad local header for ' + entry.name);\n"
              "  const nlen = rd16(dv, lp+26);\n"
              "  const elen = rd16(dv, lp+28);\n"
              "  const start = lp + 30 + nlen + elen;\n"
              "  const compressed = bytes.subarray(start, start + entry.csize);\n"
              "  if (entry.method === 0) return compressed;\n"
              "  if (entry.method !== 8) throw new Error('unsupported method ' + entry.method + ' for ' + entry.name);\n"
              "  const ds = new DecompressionStream('deflate-raw');\n"
              "  const writer = ds.writable.getWriter();\n"
              "  writer.write(compressed); writer.close();\n"
              "  const chunks = []; const reader = ds.readable.getReader();\n"
              "  for (;;) { const {done, value} = await reader.read(); if (done) break; chunks.push(value); }\n"
              "  let total = 0; for (const c of chunks) total += c.length;\n"
              "  const out = new Uint8Array(total); let off = 0;\n"
              "  for (const c of chunks) { out.set(c, off); off += c.length; }\n"
              "  return out;\n"
              "}\n"
              "\n"
              "// Si todas las entradas (no-carpeta) comparten el mismo primer\n"
              "// segmento de ruta (p.ej. 'FIRMWARE/'), se quita - para que\n"
              "// firmware.bin etc. acaben en la raiz de la SD en vez de en una\n"
              "// subcarpeta que imite la del ZIP. Si no hay un prefijo comun\n"
              "// (como en el ZIP de contenido de SD, con SYS/, MAN/, AUTOEXEC.P\n"
              "// suelto...) no se toca nada.\n"
              "function detectCommonPrefix(entries){\n"
              "  const files = entries.filter(e => !e.name.endsWith('/'));\n"
              "  if (files.length === 0) return '';\n"
              "  const slash = files[0].name.indexOf('/');\n"
              "  if (slash < 0) return '';\n"
              "  const prefix = files[0].name.substring(0, slash+1);\n"
              "  for (const f of files) if (!f.name.startsWith(prefix)) return '';\n"
              "  return prefix;\n"
              "}\n"
              "\n"
              "async function uploadFile(path, data){\n"
              "  const form = new FormData();\n"
              "  form.append('dir', '/');\n"
              "  form.append('file', new Blob([data]), path);\n"
              "  const r = await fetch('/upload?ajax=1', {method:'POST', body: form});\n"
              "  if (!r.ok) throw new Error('upload failed for ' + path + ' (HTTP ' + r.status + ')');\n"
              "}\n"
              "\n"
              "async function processZip(url, log){\n"
              "  log('Downloading ' + url + ' ...');\n"
              // Descarga via /proxy (mismo origen) en vez de fetch() directo a
              // GitHub: el blob real del asset no manda cabeceras CORS y un
              // fetch() cruzado fallaria con 'Failed to fetch'.
              "  const resp = await fetch('/proxy?url=' + encodeURIComponent(url));\n"
              "  if (!resp.ok) throw new Error('HTTP ' + resp.status + ' fetching ' + url);\n"
              "  const bytes = new Uint8Array(await resp.arrayBuffer());\n"
              "  log('Downloaded (' + bytes.length + ' bytes), parsing ZIP...');\n"
              "  const entries = parseZipEntries(bytes);\n"
              "  const prefix = detectCommonPrefix(entries);\n"
              "  const files = entries.filter(e => !e.name.endsWith('/'));\n"
              "  let n = 0;\n"
              "  for (const entry of files) {\n"
              "    n++;\n"
              "    const target = entry.name.startsWith(prefix) ? entry.name.substring(prefix.length) : entry.name;\n"
              "    log('[' + n + '/' + files.length + '] ' + target + ' ...');\n"
              "    const data = await extractEntry(bytes, entry);\n"
              "    await uploadFile(target, data);\n"
              "  }\n"
              "  log('Done: ' + url);\n"
              "}\n"
              "\n"
              "async function installUpdate(){\n"
              "  const logEl = document.getElementById('log');\n"
              "  logEl.style.display = 'block';\n"
              "  const log = (m) => { logEl.textContent += m + \"\\n\"; logEl.scrollTop = logEl.scrollHeight; };\n"
              "  document.getElementById('installBtn').disabled = true;\n"
              "  try {\n"
              "    await processZip(FW_URL, log);\n"
              "    await processZip(SD_URL, log);\n"
              "    log('Marking installed version...');\n"
              "    await fetch('/update/finish', {method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'}, body:'tag=' + encodeURIComponent(TAG)});\n"
              // El ESP32 solo puede reiniciarse a si mismo (necesario para
              // que aplique su propio ESP32_FW.BIN, si venia en el ZIP) -
              // no puede resetear el STM32/interface completo por software
              // (no hay comando de reset remoto en el protocolo), y ese
              // reset es imprescindible para que el STM32 detecte
              // firmware.bin/SD81.MCS en la raiz de la SD y los aplique.
              "    log('Files uploaded. Power-cycle the SD81 Booster now (unplug and plug it back in) to apply the update.');\n"
              "  } catch (e) {\n"
              "    log('ERROR: ' + e.message);\n"
              "    document.getElementById('installBtn').disabled = false;\n"
              "  }\n"
              "}\n"
              "</script>\n";
    }
  } else {
    html += "<form method=\"POST\" action=\"/update/check\"><input type=\"submit\" value=\"Check for updates\"></form>";
  }

  server.send(200, "text/html", html);
}

void handleUpdateCheck() {
  g_update_checked = version_check_fetch_latest(&g_latest_release);
  if (!g_update_checked) {
    server.sendHeader("Location", "/update?err=" + url_encode("Could not reach the GitHub mirror - check the network and try again."));
  } else {
    server.sendHeader("Location", "/update");
  }
  server.send(303);
}

// Llamado por el JS de installUpdate() SOLO tras haber subido ya todos los
// ficheros de los dos ZIP via /upload - aqui no se descarga ni descomprime
// nada, solo se deja constancia de la version instalada.
//
// Deliberadamente NO se llama a ESP.restart() aqui. El ESP32 solo puede
// reiniciarse a si mismo, no al STM32/interface completo (no existe
// comando de reset remoto en WIFI_PROTOCOL.h) - la parte STM32/FPGA/ROM de
// la actualizacion (firmware.bin/SD81.MCS/SDBOOST.ROM) solo se aplica en el
// arranque fisico del STM32. Si el ESP32 se reiniciara YA para aplicar su
// propio ESP32_FW.BIN (ver FW_UPDATE_PATH mas arriba) y el usuario le
// hiciera el power-cycle a la interfaz completa mientras el ESP32 todavia
// esta escribiendo su propia flash, se arriesgaria a corromper ese
// autoflasheo a medias. Dejando el ESP32_FW.BIN pendiente en la SD sin
// tocarlo, el UNICO power-cycle que el usuario tiene que hacer de todos
// modos (para el STM32) tambien dispara, de paso, el autoflasheo del ESP32
// - todo en un solo ciclo de encendido, sin ventana de riesgo.
void handleUpdateFinish() {
  String tag = server.hasArg("tag") ? server.arg("tag") : "";
  if (tag.length() > 0) {
    uint8_t handle;
    if (wifi_client_write_open("/SYS/VERSION.TXT", &handle)) {
      wifi_client_write_chunk(handle, (const uint8_t*)tag.c_str(), tag.length());
      uint32_t total;
      wifi_client_write_close(handle, &total);
    }
  }
  sdlog_close(); // vuelca el log a la SD antes de que el usuario corte la alimentacion
  server.send(200, "text/plain", "ok");
}

// Writes a short text note explaining why the update was rejected, so the
// SD card carries visible evidence instead of the update silently vanishing.
void mark_firmware_update_failed(const char* reason) {
  uint8_t handle;
  if (!wifi_client_write_open(FW_UPDATE_FAILED_PATH, &handle)) return;
  String content = "SD81 Booster ESP32 firmware update REJECTED.\n";
  content += "Reason: "; content += reason; content += "\n";
  content += "The bad file was deleted - copy a known-good ESP32_FW.BIN to the SD card root and retry.\n";
  wifi_client_write_chunk(handle, (const uint8_t*)content.c_str(), content.length());
  uint32_t total;
  wifi_client_write_close(handle, &total);
}

// Checks for a pending firmware image on the SD card and applies it. Reuses
// the existing READ_OPEN/READ_CHUNK/READ_CLOSE + DELETE protocol commands -
// no new protocol needed. On success, deletes the file and reboots into the
// new firmware.
//
// On failure there are two different cases:
// - Transport error (READ_CHUNK failed, i.e. the STM32<->ESP32 UART link
//   hiccuped) - not the file's fault, leave it in place so next boot retries.
// - Content error (bad magic byte, size mismatch, SHA-256 mismatch inside
//   Update.end()) - the file itself is bad, so retrying forever is pointless
//   and would just re-trigger the update-in-progress LED sequence on every
//   boot. Delete it and leave a short explanatory note instead (see
//   FW_UPDATE_FAILED_PATH above).
void check_and_apply_firmware_update() {
  uint8_t handle;
  uint32_t size;
  if (!wifi_client_read_open(FW_UPDATE_PATH, &handle, &size)) {
    Serial.println("No pending ESP32 firmware update on the SD card.");
    return;
  }

  Serial.print("Firmware update file found, size="); Serial.println(size);

  if (!Update.begin(size)) {
    // Not a transport error - Update itself rejected the requested size
    // (e.g. too big for the inactive OTA partition) before reading anything.
    Serial.print("Update.begin failed: "); Serial.println(Update.errorString());
    wifi_client_read_close(handle);
    wifi_client_delete(FW_UPDATE_PATH);
    mark_firmware_update_failed(Update.errorString());
    return;
  }

  uint8_t buf[WIFI_PROTO_CHUNK_SIZE];
  uint32_t offset = 0;
  bool eof = false;
  bool ok = true;
  bool transport_error = false;

  while (!eof && offset < size) {
    uint16_t n = 0;
    if (!wifi_client_read_chunk(handle, offset, buf, &n, &eof)) {
      Serial.println("Error: READ_CHUNK failed during firmware update");
      ok = false;
      transport_error = true;
      break;
    }
    if (n == 0) break;
    if (Update.write(buf, n) != n) {
      Serial.print("Update.write failed: "); Serial.println(Update.errorString());
      ok = false;
      break;
    }
    offset += n;
  }

  wifi_client_read_close(handle);

  if (ok && offset == size && Update.end(true)) {
    Serial.println("Firmware update applied successfully - deleting file and rebooting...");
    wifi_client_delete(FW_UPDATE_PATH);
    wifi_client_delete(FW_UPDATE_FAILED_PATH); // clear any stale failure note from a previous attempt
    delay(200);
    sdlog_close();
    ESP.restart();
    return;
  }

  const char* reason = Update.errorString();
  Update.abort();
  Serial.print("Firmware update FAILED: "); Serial.println(reason);

  if (transport_error) {
    Serial.println("Transport error - file left on the SD card for retry on next boot.");
  } else {
    Serial.println("Content error - deleting bad file so it isn't retried forever.");
    wifi_client_delete(FW_UPDATE_PATH);
    mark_firmware_update_failed(reason);
  }
}

#define NTP_SYNC_TIMEOUT_MS 5000   // patience waiting for the NTP reply itself

// Reads /SYS/NTP.CFG and, if MODE=SERVER, fetches the current time from the
// configured NTP server and pushes it to the STM32's RTC via CMD_SET_TIME.
// Called once per boot, right after connecting to WiFi - the STM32's own
// battery-backed RTC keeps good enough time between boots, no need to
// resync periodically once it has worked - and also on demand from the
// "/ntp/sync" web button and from check_ntp_retry() below.
// If NTP is disabled, unreachable, or the config file is missing, this
// simply does nothing and the RTC keeps whatever it had. Returns true only
// if the STM32's RTC was actually updated.
//
// out_disabled (optional): set to true only when sync is OFF ON PURPOSE
// (MODE=LOCAL or no config file) -- the caller uses this to tell "nothing
// to retry, the user doesn't want this" apart from "failed, try again
// later" (transport error, server unreachable, STM32 didn't ack).
bool sync_time_from_ntp(bool* out_disabled) {
  if (out_disabled) *out_disabled = false;
  NtpConfig cfg;
  if (!wifi_client_read_ntp_config(&cfg)) {
    Serial.println("NTP: transport error reading /SYS/NTP.CFG, skipping.");
    return false;
  }
  if (!cfg.sync_enabled) {
    Serial.println("NTP: disabled (MODE=LOCAL or no /SYS/NTP.CFG) - RTC left untouched.");
    if (out_disabled) *out_disabled = true;
    return false;
  }

  Serial.print("NTP: syncing from "); Serial.print(cfg.server);
  Serial.print(" (UTC"); Serial.print(cfg.utc_offset_hours >= 0 ? "+" : ""); Serial.print(cfg.utc_offset_hours);
  Serial.print(cfg.dst ? ", DST on)" : ", DST off)"); Serial.println("...");

  long gmt_offset_sec = (long)cfg.utc_offset_hours * 3600L;
  int  dst_offset_sec = cfg.dst ? 3600 : 0;
  configTime(gmt_offset_sec, dst_offset_sec, cfg.server);

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, NTP_SYNC_TIMEOUT_MS)) {
    Serial.println("NTP: no reply from server - RTC left untouched.");
    return false;
  }

  uint8_t year = (uint8_t)((timeinfo.tm_year + 1900) % 100);
  uint8_t month = (uint8_t)(timeinfo.tm_mon + 1);
  uint8_t day = (uint8_t)timeinfo.tm_mday;
  uint8_t hour = (uint8_t)timeinfo.tm_hour;
  uint8_t minute = (uint8_t)timeinfo.tm_min;
  uint8_t second = (uint8_t)timeinfo.tm_sec;

  if (wifi_client_set_time(year, month, day, hour, minute, second)) {
    Serial.printf("NTP: STM32 RTC updated to %02d/%02d/%02d %02d:%02d:%02d\n",
                  day, month, year, hour, minute, second);
    return true;
  }
  Serial.println("NTP: STM32 rejected or did not acknowledge the new time.");
  return false;
}

// Both MCUs boot at roughly the same time, but the STM32 typically takes
// longer (FPGA reconfig, RTC init, etc.). A single request's normal retry
// budget (~1.5s) isn't enough to cover that race - wait here, with more
// patience, until the STM32 actually answers a PING before doing anything
// that depends on it (firmware update check, WiFi config fetch).
bool wait_for_stm32(uint32_t timeout_ms) {
  uint32_t start = millis();
  uint8_t fw_version;
  while (millis() - start < timeout_ms) {
    if (wifi_client_ping(&fw_version)) return true;
    delay(200);
  }
  return false;
}

// Writes /MAN/IP.TXT with the current IP address, reusing the existing
// "LOAD THEN PRINT" help-file convention (looks up /MAN/<name>.TXT) - lets
// the user check the module's address right from BASIC on the ZX81 itself,
// with LOAD THEN PRINT "*IP", no phone/PC/mDNS support needed.
//
// The ZX81 charset has no real lowercase glyphs - any lowercase ASCII byte
// displays as inverse video of an unrelated character. Force everything to
// uppercase before writing (digits/./:/ are unaffected and already display
// correctly).
void write_ip_help_file() {
  String ip = WiFi.localIP().toString();
  String content = "SD81 Booster WiFi module\n";
  content += "Firmware version: " WIFI_FW_VERSION "\n";
  content += "IP address: " + ip + "\n";
  content += "http://" + ip + "/\n";
  content += "Also try: http://" + String(MDNS_HOSTNAME) + ".local/\n";
  content.toUpperCase();

  uint8_t handle;
  if (!wifi_client_write_open("/MAN/IP.TXT", &handle)) {
    Serial.println("Error: could not write /MAN/IP.TXT");
    return;
  }
  wifi_client_write_chunk(handle, (const uint8_t*)content.c_str(), content.length());
  uint32_t total;
  wifi_client_write_close(handle, &total);
}

#define WIFI_CONNECT_TIMEOUT_MS 10000   // per-network timeout while trying candidates in turn

// Tries WiFi.begin(ssid, pass), waiting up to WIFI_CONNECT_TIMEOUT_MS. Returns
// true if connected. On failure, disconnects cleanly before returning so the
// next candidate starts from a clean state.
bool try_connect(const char* ssid_try, const char* pass_try) {
  Serial.print("Trying network: "); Serial.println(ssid_try);
  WiFi.begin(ssid_try, pass_try);
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
    delay(300);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) return true;
  WiFi.disconnect(true);
  return false;
}

// Un barrido de las redes de /SYS/WIFI.CFG, probando cada una UNA vez (a
// diferencia del bucle de setup(), que insiste sin parar hasta conectar).
// Reutilizada por setup() y por check_wifi_reconnect() -- esta ultima no
// puede permitirse bloquear para siempre si el WiFi se cae despues de
// arrancar, con el resto de la placa (STM32, telnet, web) ya funcionando.
bool connect_wifi_from_cfg_once() {
  WifiNetwork networks[WIFI_PROTO_MAX_NETWORKS];
  uint8_t network_count = 0;
  bool got_cfg = wifi_client_read_wifi_networks(networks, WIFI_PROTO_MAX_NETWORKS, &network_count);
  Serial.print("get_wifi_cfg: got_cfg="); Serial.print(got_cfg);
  Serial.print(" network_count="); Serial.println(network_count);
  if (!got_cfg || network_count == 0) return false;

  for (uint8_t i = 0; i < network_count; i++) {
    if (try_connect(networks[i].ssid, networks[i].pass)) return true;
  }
  return false;
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("STEP1: Serial up");

  wifi_client_init();
  Serial.println("STEP2: wifi_client_init done");

  Serial.println("Waiting for the STM32 to be ready...");
  if (wait_for_stm32(15000)) {
    Serial.println("STM32 responded.");
  } else {
    Serial.println("STM32 did not respond within the timeout - continuing anyway.");
  }
  sdlog_try_flush(); // primer intento de volcado, para no perder los mensajes de arranque

  check_and_apply_firmware_update();

  // No hardcoded fallback credentials on purpose - baking a real network's
  // password into compiled firmware would leak it to anyone who flashes or
  // receives that binary. /SYS/WIFI.CFG on the SD card is the only source
  // of truth; keep asking the STM32 for it until at least one configured
  // network actually connects (also covers "file added/edited after boot,
  // no reset yet" without extra logic).
  Serial.println("STEP3: connecting to WiFi");
  WiFi.mode(WIFI_STA);
  bool connected = false;

  while (!connected) {
    connected = connect_wifi_from_cfg_once();
    if (!connected) {
      Serial.println("No usable network yet - check /SYS/WIFI.CFG on the SD card. Retrying in 5s...");
      delay(5000);
    }
  }

  Serial.print("IP: "); Serial.println(WiFi.localIP());
  write_ip_help_file();
  {
    bool ntp_disabled = false;
    ntp_stop_retrying = sync_time_from_ntp(&ntp_disabled) || ntp_disabled;
  }

  if (MDNS.begin(MDNS_HOSTNAME)) {
    MDNS.addService("http", "tcp", 80);
    Serial.println("mDNS responder started: http://" MDNS_HOSTNAME ".local");
  } else {
    Serial.println("Error starting mDNS responder.");
  }

  server.on("/", handleRoot);
  server.on("/list", handleList);
  server.on("/upload", HTTP_POST, handleUploadDone, handleUpload);
  server.on("/mkdir", HTTP_POST, handleMkdir);
  server.on("/delete", HTTP_POST, handleDelete);
  server.on("/download", HTTP_GET, handleDownload);
  server.on("/proxy", HTTP_GET, handleProxyDownload);
  server.on("/logo.png", HTTP_GET, handleLogo);
  server.on("/ntp", HTTP_GET, handleNtpPage);
  server.on("/ntp/save", HTTP_POST, handleNtpSave);
  server.on("/ntp/sync", HTTP_POST, handleNtpSyncNow);
  server.on("/telnet", HTTP_GET, handleTelnetPage);
  server.on("/telnet/connect", HTTP_POST, handleTelnetConnect);
  server.on("/telnet/disconnect", HTTP_POST, handleTelnetDisconnect);
  server.on("/update", HTTP_GET, handleUpdatePage);
  server.on("/update/check", HTTP_POST, handleUpdateCheck);
  server.on("/update/finish", HTTP_POST, handleUpdateFinish);
  server.begin();
  Serial.println("Server ready.");
  net_bridge_init();
}

#define NTP_SYNC_FLAG_PATH      "/SYS/NTP_SYNC_NOW.FLAG"
#define NTP_SYNC_FLAG_POLL_MS   5000   // how often to check for it in loop()

// The STM32 can't initiate anything on this UART (it only ever responds -
// see WIFI_PROTOCOL.h), so LOAD *NTP on the Z80 side can't tell the ESP32
// directly to resync. Instead it just drops this empty flag file on the SD
// card (cmd_ntp_sync in COMMANDS.cpp); polling for it here, the same way the
// web UI's "Sync time now" button triggers sync_time_from_ntp() on demand,
// is what actually acts on it.
void check_ntp_sync_flag() {
  static uint32_t last_check = 0;
  if (millis() - last_check < NTP_SYNC_FLAG_POLL_MS) return;
  last_check = millis();

  uint8_t handle;
  uint32_t size;
  if (!wifi_client_read_open(NTP_SYNC_FLAG_PATH, &handle, &size)) return; // no flag pending
  wifi_client_read_close(handle);

  Serial.println("NTP: sync requested via LOAD *NTP - syncing now...");
  wifi_client_delete(NTP_SYNC_FLAG_PATH);
  if (sync_time_from_ntp(nullptr)) ntp_stop_retrying = true;
}

#define NTP_RETRY_INTERVAL_MS 60000   // cada minuto hasta que funcione una vez

// El primer intento (en setup(), justo tras conectar) puede fallar sin culpa
// de nadie -- el servidor NTP tarda en responder, o la conexion aun no tiene
// DNS resuelto del todo. Reintentar solo, en vez de dejar el RTC con lo que
// tuviera, hasta que funcione una vez; despues, silencio (el RTC con bateria
// ya se basta solo entre arranques).
void check_ntp_retry() {
  static uint32_t last_attempt = 0;
  if (ntp_stop_retrying) return;
  if (WiFi.status() != WL_CONNECTED) return;   // sin red no hay nada que intentar
  if (millis() - last_attempt < NTP_RETRY_INTERVAL_MS) return;
  last_attempt = millis();

  Serial.println("NTP: reintentando (el intento anterior fallo)...");
  bool disabled = false;
  bool ok = sync_time_from_ntp(&disabled);
  if (ok || disabled) ntp_stop_retrying = true;
}

#define WIFI_RECONNECT_INTERVAL_MS 30000   // no reintentar mas de una vez cada 30s

// El WiFi solo se conecta una vez, en setup(). Si el router se reinicia o la
// senal se pierde despues, WiFi.status() se queda en algo distinto de
// WL_CONNECTED para siempre sin esto -- se pierde la web, el explorador por
// red y el puente de telnet, todo a la vez, hasta un reset manual.
// connect_wifi_from_cfg_once() SI bloquea mientras dura el intento (hasta
// ~10s por red configurada), pero solo se llama cuando ya esta desconectado
// -- el resto de la placa ya esta parada igualmente en ese caso -- y como
// mucho una vez cada WIFI_RECONNECT_INTERVAL_MS.
void check_wifi_reconnect() {
  static uint32_t last_attempt = 0;
  if (WiFi.status() == WL_CONNECTED) return;
  if (millis() - last_attempt < WIFI_RECONNECT_INTERVAL_MS) return;
  last_attempt = millis();

  Serial.println("WiFi: reconectando...");
  if (connect_wifi_from_cfg_once()) {
    Serial.print("WiFi: reconectado, IP "); Serial.println(WiFi.localIP());
    write_ip_help_file();
    ntp_stop_retrying = false;   // la hora pudo quedarse atras durante el corte
  } else {
    Serial.println("WiFi: reconexion fallida, se probara de nuevo mas tarde.");
  }
}

void loop() {
  server.handleClient();
  check_ntp_sync_flag();
  check_ntp_retry();
  check_wifi_reconnect();
  net_bridge_loop();
  sdlog_try_flush();
}
