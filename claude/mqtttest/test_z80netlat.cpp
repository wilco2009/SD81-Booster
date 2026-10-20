// Prueba de EXAMPLES/NETLAT/netlat.bin en el PC: el programa de verdad en el Z80 de EightyOne, con el modem de verdad (NET_BRIDGE y
// NET_MQTT del ESP32) y un broker falso que devuelve los mensajes (el eco de la prueba 4). Se comprueba que las ocho pruebas
// acumulan las muestras que tienen que acumular, sin fallos, y que los numeros tienen sentido. Los tiempos son de este banco
// (el reloj del MCU simulado sigue las instrucciones del Z80): sirven para ver que la aritmetica va bien, no para medir la red.
#include "z80net_env.h"

static const uint16_t RESL = 0x7C00;
struct Rec { int n, mn, mx, sum, fail; };
static Rec rec(int t) {
  const uint8_t* p = mem + RESL + 8 * t;
  return Rec{p[0], p[1] | (p[2] << 8), p[3] | (p[4] << 8), p[5] | (p[6] << 8), p[7]};
}

int main(int argc, char** argv) {
  if (argc < 2) { printf("uso: test_z80netlat netlat.bin\n"); return 2; }
  FILE* f = fopen(argv[1], "rb");
  if (!f) { printf("no encuentro %s\n", argv[1]); return 2; }
  size_t sz = fread(mem + 24576, 1, 0x2000, f); fclose(f);
  printf("netlat.bin: %zu bytes en $6000\n", sz);
  if (24576 + sz > NB) { printf("ERROR el programa pisa el buzon de net.inc ($7000)\n"); errors++; }
  z80_init(); z80_reset(); reset_world();
  CHECK(call_ep(24576) == 0, "net_init");
  CHECK(call_ep(24579) == 0, "room_new: conecta y crea la sala");
  call_ep(24582);
  const char* names[8] = {"RTC", "NET_READ x100", "AT sin pausas", "AT con at_cmd", "MQTT eco", "publicar (espera >)", "publicar encadenado", "MQTT eco encadenado"};
  const int want_n[8] = {40, 5, 40, 40, 30, 30, 30, 30};
  for (int t = 0; t < 8; t++) {
    Rec r = rec(t);
    printf("  %-24s n=%2d fallos=%d  min=%4d ms  media=%6.1f ms  max=%4d ms\n", names[t], r.n, r.fail, r.mn * 10, r.n ? r.sum * 10.0 / r.n : 0.0, r.mx * 10);
    CHECK(r.n == want_n[t] && r.fail == 0, "la prueba hace todas sus muestras sin fallos");
    CHECK(r.n == 0 || (r.mn <= r.mx && r.sum >= r.mn * r.n && r.sum <= r.mx * r.n), "minimo <= media <= maximo");
  }
  // las pruebas con el modem tardan mas que leer el reloj, y la que pausa 20 ms tarda mas que la que no
  Rec r0 = rec(0), r2 = rec(2), r3 = rec(3), r4 = rec(4), r1 = rec(1);
  CHECK(r2.sum > r0.sum && r3.sum > r0.sum, "las dos pruebas AT > leer el reloj");
  CHECK(r4.sum >= r3.sum, "el eco del servidor cuesta al menos lo que un AT");
  CHECK(r1.sum > 0, "100 comandos NET_READ tardan algo");
  CHECK(call_ep(24585) == 0, "room_leave");
  if (errors) printf("%d ERRORES\n", errors); else printf("TODO OK\n");
  return errors ? 1 : 0;
}
