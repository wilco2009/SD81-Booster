#include "LOGCFG.h"
#include "WIFI_CLIENT.h"
#include "DIAG.h"
#include "MOUSE_TEST.h"
#include "MOUSE_BLE.h"

LogCfg g_logcfg = { true, false, DIAG_DUMP_S, false };

static bool parse_bool(String v) {
  v.trim(); v.toUpperCase();
  return !(v == "" || v == "0" || v == "OFF" || v == "NO" || v == "FALSE");
}

void logcfg_load() {
  String content;
  if (!wifi_client_read_text_file("/SYS/LOG.CFG", &content)) return;   // fallo de transporte: se quedan los valores por defecto
  if (content.length() == 0) return;                                    // sin fichero

  int pos = 0;
  int len = content.length();
  while (pos <= len) {
    int nl = content.indexOf('\n', pos);
    String line = (nl == -1) ? content.substring(pos) : content.substring(pos, nl);
    while (line.endsWith("\r")) line.remove(line.length() - 1);
    line.trim();

    if (line.length() && line[0] != '#' && line[0] != ';') {
      int eq = line.indexOf('=');
      if (eq > 0) {
        String key = line.substring(0, eq);
        String val = line.substring(eq + 1);
        key.trim(); val.trim();
        key.toUpperCase();
        if (key == "SDLOG") {
          g_logcfg.sdlog = parse_bool(val);
        } else if (key == "DIAG") {
          g_logcfg.diag = parse_bool(val);
        } else if (key == "MOUSE") {
          g_logcfg.mouse = parse_bool(val);
        } else if (key == "DIAG_DUMP_S") {
          long v = val.toInt();
          if (v < 0) v = 0;
          if (v > 65535) v = 65535;
          g_logcfg.diag_dump_s = (uint16_t)v;
        }
        // STM32LOG y cualquier otra: es del STM32 (o desconocida), aqui se ignora
      }
    }
    if (nl == -1) break;
    pos = nl + 1;
  }
}

// --- Orden "esp" de la consola web -------------------------------------------------------------------------------------------------

static String status_text() {
  String s = "ESP32: SDLOG=" + String(g_logcfg.sdlog ? 1 : 0);
#if ESP_DIAG
  s += "  DIAG=" + String(g_logcfg.diag ? 1 : 0) + "  DIAG_DUMP_S=" + String(g_logcfg.diag_dump_s);
#else
  s += "  DIAG=(no compilado: ESP_DIAG=0)";
#endif
#if ESP_MOUSE_TEST
  s += "  MOUSE=" + String(g_logcfg.mouse ? 1 : 0);
#endif
  s += "\n";
  return s;
}

static String help_text() {
  String s =
    "esp                  esta ayuda y el estado\n"
    "esp log on|off       copiar el Serial a /SYS/ESP32LOG.TXT (hasta el proximo arranque; fijo: SDLOG en /SYS/LOG.CFG)\n"
    "esp diag [n]         las ultimas n lineas del diagnostico (20 por defecto)\n"
    "esp diag on|off      activar o parar el diagnostico (hasta el proximo arranque; fijo: DIAG en /SYS/LOG.CFG)\n"
    "esp diag dump        volcar ya el diagnostico a la SD\n"
    "esp ble              estado del raton Bluetooth (conexion, latencias)\n"
    "esp ble pair [s]     emparejar un raton nuevo (ventana de 60 s, de 10 a 300): sustituye al guardado\n"
    "esp ble connect      conectar ya con el raton guardado (y activarlo)\n"
    "esp ble forget       olvidar el raton guardado y sus claves\n"
    "esp ble on|off       activar o parar el raton Bluetooth (se guarda; con on se conecta solo al guardado)\n"
    "esp ble reset        poner a cero las estadisticas\n"
    "esp mouse [on|off]   el ultimo resumen de la prueba sintetica del raton; on/off la arranca o la para (hasta el proximo arranque; fijo: MOUSE en /SYS/LOG.CFG)\n";
  return s + status_text();
}

#if ESP_DIAG
// Las ultimas `n` lineas del historial del diagnostico
static String diag_tail(int n) {
  char* tmp = (char*)malloc(DIAG_RING + 1);
  if (!tmp) return "sin memoria\n";
  size_t len = diag_read(tmp, DIAG_RING);
  tmp[len] = 0;
  size_t end = len;
  if (end && tmp[end - 1] == '\n') end--;                      // el salto de linea final no abre una linea nueva
  size_t start = end;
  int lines = 1;
  while (start > 0) {
    if (tmp[start - 1] == '\n') { if (lines >= n) break; lines++; }
    start--;
  }
  String s(tmp + start);
  free(tmp);
  if (s.length() > 4000) s = s.substring(s.length() - 4000);
  if (s.length() == 0) return "(el diagnostico no tiene nada apuntado)\n";
  return s;
}
#endif

String esp_console_cmd(const String& args) {
  String a = args;
  a.trim();
  String word = a, rest = "";
  int sp = a.indexOf(' ');
  if (sp >= 0) { word = a.substring(0, sp); rest = a.substring(sp + 1); rest.trim(); }
  word.toLowerCase();
  String restl = rest; restl.toLowerCase();

  if (word == "" || word == "help" || word == "?") return help_text();

  if (word == "log") {
    if (restl == "on" || restl == "off") g_logcfg.sdlog = (restl == "on");
    else if (restl != "") return "esp log: on u off\n";
    return status_text();
  }

  if (word == "diag") {
#if ESP_DIAG
    if (restl == "on" || restl == "off") {
      g_logcfg.diag = (restl == "on");
      return status_text();
    }
    if (restl == "dump") {
      if (!g_logcfg.diag) return "esp diag dump: el diagnostico esta parado (esp diag on)\n";
      diag_dump_now();
      return "diagnostico volcado a /SYS/ESP32LOG.TXT\n";
    }
    int n = 20;
    if (restl != "") { n = restl.toInt(); if (n < 1) n = 20; if (n > 200) n = 200; }
    return status_text() + diag_tail(n);
#else
    return "esp diag: este firmware no lleva el diagnostico (ESP_DIAG = 0)\n";
#endif
  }

  if (word == "ble") {
#if ESP_MOUSE_BLE
    return mouse_ble_console(rest);
#else
    return "esp ble: este firmware no lleva el raton Bluetooth (ESP_MOUSE_BLE = 0)\n";
#endif
  }

  if (word == "mouse") {
#if ESP_MOUSE_TEST
    if (restl == "on" || restl == "off") { g_logcfg.mouse = (restl == "on"); return status_text(); }
    if (restl != "") return "esp mouse: on, off o nada\n";
    String r = mouse_test_last_report();
    if (!g_logcfg.mouse) return "la prueba del raton esta parada (esp mouse on, o MOUSE=1 en /SYS/LOG.CFG)\n" + r;
    return r.length() ? r : "(aun no hay resumen: salen cada " + String(MOUSE_TEST_REPORT_S) + " s)\n";
#else
    return "esp mouse: este firmware no lleva la prueba del raton (ESP_MOUSE_TEST = 0)\n";
#endif
  }

  return "esp: orden desconocida (esp help)\n";
}
