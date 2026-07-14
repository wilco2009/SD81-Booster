#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <ESPmDNS.h>

#include "WIFI_CLIENT.h"
#include "LOGO.h"

// Fixed path on the SD card for a pending ESP32 firmware image. If present at
// boot, it gets flashed via the Update library and deleted on success - same
// pattern as the STM32's own SD81.MCS FPGA self-update.
// Raiz de la SD, no /SYS - misma convencion que firmware.bin (STM32) y
// SD81.MCS (FPGA), los otros dos ficheros de auto-actualizacion.
#define FW_UPDATE_PATH        "/ESP32_FW.BIN"
#define FW_UPDATE_FAILED_PATH "/ESP32_FW_FAILED.TXT"

#define MDNS_HOSTNAME "sd81booster"   // reachable at http://sd81booster.local

WebServer server(80);

bool     g_upload_active = false;
uint8_t  g_upload_handle = 0;

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
    } else {
      Serial.println("Error: WRITE_CLOSE failed");
    }
    g_upload_active = false;
  }
}

void handleUploadDone() {
  String dir = server.hasArg("dir") ? server.arg("dir") : "/";
  if (dir.length() == 0) dir = "/";
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
    WifiNetwork networks[WIFI_PROTO_MAX_NETWORKS];
    uint8_t network_count = 0;
    bool got_cfg = wifi_client_read_wifi_networks(networks, WIFI_PROTO_MAX_NETWORKS, &network_count);
    Serial.print("get_wifi_cfg: got_cfg="); Serial.print(got_cfg);
    Serial.print(" network_count="); Serial.println(network_count);

    if (got_cfg && network_count > 0) {
      for (uint8_t i = 0; i < network_count && !connected; i++) {
        connected = try_connect(networks[i].ssid, networks[i].pass);
      }
    }

    if (!connected) {
      Serial.println("No usable network yet - check /SYS/WIFI.CFG on the SD card. Retrying in 5s...");
      delay(5000);
    }
  }

  Serial.print("IP: "); Serial.println(WiFi.localIP());
  write_ip_help_file();

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
  server.on("/logo.png", HTTP_GET, handleLogo);
  server.begin();
  Serial.println("Server ready.");
}

void loop() {
  server.handleClient();
}
