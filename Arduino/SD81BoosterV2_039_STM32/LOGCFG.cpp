// LOGCFG.cpp - lee /SYS/LOG.CFG de la SD al arrancar y fija el nivel de log del STM32 (log_level, ver GLOBALS.h).
//
// El mismo fichero lo lee tambien el ESP32 (claves SDLOG, DIAG y DIAG_DUMP_S, ver Wifi_module_01/LOGCFG.cpp): cada firmware ignora las
// claves del otro. Formato: lineas CLAVE=VALOR, `#` o `;` al principio = comentario, mayusculas o minusculas.
//
//   STM32LOG=0     nivel de log del STM32 por el USB: 0 = nada (por defecto), 1 = errores, 2 = errores+info (lo que salia hasta ahora),
//                  3 y 4 = mas detalle, pero solo si el firmware se compilo con LOG2/LOG3
//   STM32LAT=0     1 = sacar por USB las lineas [LAT] de LATSTATS cada 10 s (por defecto 0; hace falta LAT_STATS = 1 en LATSTATS.h)
//
// Sin fichero, o sin esas claves, el STM32 no saca nada por USB (aparte del banner de arranque y de lo que se pida desde la consola).
#include "GLOBALS.h"
#include "SD_handle.h"

void log_cfg_load() {
  SdFile f;
  if (!f.open("/SYS/LOG.CFG", O_RDONLY)) return;

  char buf[257];
  int n = f.read(buf, sizeof(buf) - 1);
  f.close();
  if (n <= 0) return;
  buf[n] = 0;

  char* line = buf;
  while (line && *line) {
    char* next = strpbrk(line, "\r\n");
    if (next) { *next = 0; next++; while (*next == '\r' || *next == '\n') next++; }

    while (*line == ' ' || *line == '\t') line++;
    if (*line != '#' && *line != ';') {
      char* eq = strchr(line, '=');
      if (eq) {
        *eq = 0;
        char* key = line;
        char* val = eq + 1;
        for (char* p = key; *p; p++) *p = toupper((unsigned char)*p);
        while (*val == ' ' || *val == '\t') val++;
        // quita espacios finales de la clave
        for (int i = (int)strlen(key) - 1; i >= 0 && (key[i] == ' ' || key[i] == '\t'); i--) key[i] = 0;
        if (strcmp(key, "STM32LAT") == 0) {
          log_lat = (atoi(val) != 0);
        } else if (strcmp(key, "STM32LOG") == 0) {
          int v = atoi(val);
          if (v < 0) v = 0;
          if (v > 4) v = 4;
          log_level = (uint8_t)v;
        }
      }
    }
    line = next;
  }
}
