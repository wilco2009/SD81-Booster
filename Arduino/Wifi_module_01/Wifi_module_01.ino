#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>

#include "wifi_credentials.h"   // no trackeado en git — define ssid/password ahí

WebServer server(80);
File uploadFile;

const char* uploadForm = R"rawliteral(
<!DOCTYPE html><html><body>
<h1>SD81 Booster - prueba de subida (LittleFS)</h1>
<form method="POST" action="/upload" enctype="multipart/form-data">
  <input type="file" name="file">
  <input type="submit" value="Subir">
</form>
<hr>
<a href="/list">Listar ficheros</a>
</body></html>
)rawliteral";

void handleRoot() {
  server.send(200, "text/html", uploadForm);
}

void handleList() {
  String out = "<h1>Ficheros en LittleFS</h1><ul>";
  File root = LittleFS.open("/");
  File file = root.openNextFile();
  while (file) {
    out += "<li>" + String(file.name()) + " (" + String(file.size()) + " bytes)</li>";
    file = root.openNextFile();
  }
  out += "</ul><a href=\"/\">Volver</a>";
  server.send(200, "text/html", out);
}

void handleUpload() {
  HTTPUpload& upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    if (!filename.startsWith("/")) filename = "/" + filename;
    Serial.print("Subiendo: "); Serial.println(filename);
    uploadFile = LittleFS.open(filename, "w");
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadFile) uploadFile.write(upload.buf, upload.currentSize);
  } else if (upload.status == UPLOAD_FILE_END) {
    if (uploadFile) {
      uploadFile.close();
      Serial.print("Subida completa: "); Serial.print(upload.totalSize); Serial.println(" bytes");
    }
  }
}

void handleUploadDone() {
  server.sendHeader("Location", "/list");
  server.send(303);
}

void setup() {
  Serial.begin(115200);
  delay(1500);

  if (!LittleFS.begin(true)) {
    Serial.println("Error montando LittleFS");
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Conectando a WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("IP: "); Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/list", handleList);
  server.on("/upload", HTTP_POST, handleUploadDone, handleUpload);
  server.begin();
  Serial.println("Servidor listo.");
}

void loop() {
  server.handleClient();
}