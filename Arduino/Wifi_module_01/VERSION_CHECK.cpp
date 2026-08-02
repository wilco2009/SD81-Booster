#include "VERSION_CHECK.h"
#include "WIFI_CLIENT.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "SD_LOG.h" // ultimo a proposito, ver Wifi_module_01.ino

#define GITHUB_API_URL "https://api.github.com/repos/" GITHUB_OWNER "/" GITHUB_REPO "/releases/latest"

String version_check_get_installed() {
  String v;
  wifi_client_read_text_file("/SYS/VERSION.TXT", &v);
  v.trim();
  return v;
}

bool version_check_fetch_latest(ReleaseInfo* out) {
  WiFiClientSecure client;
  client.setInsecure(); // solo se lee informacion publica de la release, sin secretos que proteger
  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  Serial.print("Update check: GET "); Serial.println(GITHUB_API_URL);
  if (!http.begin(client, GITHUB_API_URL)) {
    Serial.println("Update check: http.begin() failed (URL/parse error?)");
    return false;
  }
  // La API de GitHub exige un User-Agent valido en toda peticion (403 si
  // falta) - no es proteccion anti-bot, es un requisito documentado de su API.
  http.addHeader("User-Agent", "SD81Booster-ESP32");
  http.addHeader("Accept", "application/vnd.github+json");

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.print("Update check: GET failed, code="); Serial.print(code);
    Serial.print(" ("); Serial.print(HTTPClient::errorToString(code)); Serial.println(")");
    http.end();
    return false;
  }

  // Parsear directamente desde http.getStream() es poco fiable con TLS: con
  // ArduinoJson a veces available()==0 momentaneamente aunque queden mas
  // bytes por llegar, y lo interpreta como fin de la respuesta demasiado
  // pronto (error "IncompleteInput" con JSON valido pero truncado en el
  // punto equivocado). Mas robusto: recibir el cuerpo completo primero.
  String body = http.getString();
  http.end();

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    Serial.print("Update check: JSON parse failed: "); Serial.print(err.c_str());
    Serial.print(" (body length="); Serial.print(body.length()); Serial.println(")");
    return false;
  }

  out->tag = doc["tag_name"].as<String>();
  out->firmware_zip_url = "";
  out->sdcontent_zip_url = "";

  for (JsonObject asset : doc["assets"].as<JsonArray>()) {
    String name = asset["name"].as<String>();
    String url = asset["browser_download_url"].as<String>();
    // GitHub sanea los nombres al subir el asset (espacios/parentesis se
    // convierten en puntos, p.ej. "SD81Booster 1.0 (SD Content).zip" ->
    // "SD81Booster.1.0.SD.Content.zip") - buscar solo "Content" evita
    // depender del separador exacto; ningun otro asset lo contiene.
    if (name.indexOf("Firmware") >= 0) out->firmware_zip_url = url;
    else if (name.indexOf("Content") >= 0) out->sdcontent_zip_url = url;
  }

  bool ok = out->tag.length() > 0 && out->firmware_zip_url.length() > 0 && out->sdcontent_zip_url.length() > 0;
  if (!ok) {
    Serial.print("Update check: tag="); Serial.print(out->tag);
    Serial.print(" firmware_url="); Serial.print(out->firmware_zip_url.length() ? "ok" : "MISSING");
    Serial.print(" sdcontent_url="); Serial.println(out->sdcontent_zip_url.length() ? "ok" : "MISSING");
  } else {
    Serial.print("Update check: latest release is "); Serial.println(out->tag);
  }
  return ok;
}
