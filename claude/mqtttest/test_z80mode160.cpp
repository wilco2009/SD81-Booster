// Prueba de EXAMPLES/MODE160/mode160.bin en el PC: el Z80 de verdad (nucleo de EightyOne) ejecuta el programa con la memoria, los
// POKEs de configuracion, el teclado y el VSYNC simulados, y un modelo de la paleta de la FPGA (POKE 2063 puntero, 2064 byte bajo,
// 2065 byte alto: escribe la entrada y avanza). Comprueba que carga las 256 entradas de pal160.bin por ese protocolo, que la imagen
// ($8000-$BE7F) es la esperada (filas de la paleta de 4 en 4 filas y barras de 9), que el modo (POKE 2045,175) se activa DESPUES, y
// que P / O mueven la posicion (POKE 2099) de uno en uno entre 0 y 15. NO prueba la FPGA (tb_mode160.v) ni la placa.
//
//   ./build_mode160.sh        (necesita g++/gcc, pasmo, python y el nucleo Z80 de EightyOne)
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <map>
extern "C" {
#include "z80.h"
int tstates = 0;
int StackChange = 0, StepOutRequested = 0, RetExecuted = 0;
int z80_fetch_is_m1 = 0;
void z80_m1_backtrack(void) {}
void debug(int) {}
}

static uint8_t mem[65536];
static uint8_t keyrows[8];
static long instr_total = 0;
static uint64_t sim_us() { return (uint64_t)instr_total * 5000ULL / 3250ULL; }
static long frame() { return (long)(sim_us() / 20000ULL); }
static int errors = 0;
#define CHECK(c, msg) do { if (!(c)) { printf("  ERROR: %s\n", msg); errors++; } else printf("  ok    %s\n", msg); } while (0)

struct Poke { uint16_t a; uint8_t v; long f; };
static std::vector<Poke> pokes;
static uint16_t pal[256]; static bool pal_set[256];
static int pal_idx = 0, pal_hi_writes = 0; static uint8_t pal_lo = 0;
static long frame_mode = -1, frame_last_pal = -1;
static std::vector<int> xadj_seq;
static bool bad_write = false; static uint16_t bad_addr = 0;
static uint16_t code_hi = 30000;
static int img_writes_after_mode = 0;

extern "C" {
int  z80_contend_wrapper(int, int, int) { return 0; }
int  z80_contend_io_wrapper(int, int, int) { return 0; }
BYTE z80_readbyte_wrapper(int a) { return mem[a & 0xFFFF]; }
BYTE z80_readoperandbyte_wrapper(int a) { return mem[a & 0xFFFF]; }
BYTE z80_opcode_fetch_wrapper(int a) { z80_fetch_is_m1 = 0; return mem[a & 0xFFFF]; }
void z80_writebyte_wrapper(int a, int d) {
  a &= 0xFFFF;
  if (a < 0x2000) {
    pokes.push_back({(uint16_t)a, (uint8_t)d, frame()});
    if (a == 2063) pal_idx = d & 255;
    if (a == 2064) pal_lo = (uint8_t)d;
    if (a == 2065) { pal[pal_idx] = (uint16_t)((d << 8) | pal_lo); pal_set[pal_idx] = true; pal_idx = (pal_idx + 1) & 255; pal_hi_writes++; frame_last_pal = frame(); }
    if (a == 2045 && d == 175) frame_mode = frame();
    if (a == 2099) xadj_seq.push_back(d);
    return;
  }
  bool ok = (a >= 0x8000 && a < 0xBE80) || (a >= 30000 && a < code_hi) || a >= 0xFF00;
  if (!ok && !bad_write) { bad_write = true; bad_addr = (uint16_t)a; }
  if (a >= 0x8000 && a < 0xBE80 && frame_mode >= 0) img_writes_after_mode++;
  mem[a] = (uint8_t)d;
}
void z80_writeport_wrapper(int, int, int*) {}
BYTE z80_readport_wrapper(int port, int*) {
  int lo = port & 0xFF;
  if (lo == 0xAF) return (sim_us() % 20000ULL) < 512 ? 1 : 0;
  if (lo == 0xFE) {
    uint8_t v = 0x1F;
    for (int r = 0; r < 8; r++) if (!(((port >> 8) >> r) & 1)) v &= (uint8_t)~keyrows[r];
    return (uint8_t)(v | 0xE0);
  }
  return 0xFF;
}
}

int main(int argc, char** argv) {
  const char* bin = argc > 1 ? argv[1] : "mode160.bin";
  const char* palf = argc > 2 ? argv[2] : "pal160.bin";
  FILE* f = fopen(bin, "rb");
  if (!f) { printf("no encuentro %s\n", bin); return 2; }
  size_t sz = fread(mem + 30000, 1, 0x4000, f); fclose(f);
  uint8_t palbin[512];
  f = fopen(palf, "rb");
  if (!f || fread(palbin, 1, 512, f) != 512) { printf("no encuentro %s\n", palf); return 2; }
  fclose(f);
  code_hi = (uint16_t)(30000 + sz);
  printf("mode160.bin: %zu bytes en 30000-%u\n", sz, 30000 + (unsigned)sz - 1);
  CHECK(30000 + sz < 0x8000, "el programa queda por debajo de $8000");
  memset(mem + 0x8000, 0xEE, 0x4000);                                              // un valor distinto para ver bytes sin escribir

  z80_init(); z80_reset();
  z80.sp.w = 0xFFF0; mem[0xFFF0] = 0x00; mem[0xFFF1] = 0x01; mem[0x0100] = 0x76;
  z80.pc.w = 30000; z80.halted = 0; z80.iff1 = z80.iff2 = 0;
  z80.ix.w = 0x1234; z80.iy.w = 0x5678;

  struct K { long from, to; int row, bit; };
  const K keys[] = { {30, 90, 5, 0},       // P: 60 cuadros, sube hasta 15 y se queda
                     {110, 260, 5, 1},     // O: 150 cuadros, baja hasta 0 y se queda
                     {290, 292, 0, 2} };   // X: salir
  long guard = 0;
  while (z80.pc.w != 0x0100) {
    long fr = frame();
    memset(keyrows, 0, sizeof keyrows);
    for (const K& k : keys) if (fr >= k.from && fr < k.to) keyrows[k.row] |= (uint8_t)(1 << k.bit);
    z80_do_opcode(); instr_total++;
    if (++guard > 400000000L) { printf("ERROR: el Z80 no termina (PC=%04X)\n", z80.pc.w); return 1; }
    if (fr > 400) { printf("ERROR: no ha salido con X (cuadro %ld, PC=%04X)\n", fr, z80.pc.w); errors++; break; }
  }
  bool done = (z80.pc.w == 0x0100);

  CHECK(done, "el programa vuelve (RET)");
  CHECK(!bad_write, "no escribe fuera del programa, la imagen y la pila");
  if (bad_write) printf("        primera escritura rara en $%04X\n", bad_addr);
  CHECK(pal_hi_writes == 256, "256 entradas de paleta escritas (2065)");
  int pal_bad = 0;
  for (int i = 0; i < 256; i++) {
    uint16_t want = (uint16_t)((palbin[2 * i + 1] << 8) | palbin[2 * i]);
    if (!pal_set[i] || pal[i] != want) pal_bad++;
  }
  CHECK(pal_bad == 0, "la paleta cargada es la de pal160.bin, con el puntero que avanza solo");
  int img_bad = 0;
  for (int r = 0; r < 100; r++) {
    int prow = r < 64 ? r / 4 : (r - 64) / 9;
    for (int x = 0; x < 160; x++) if (mem[0x8000 + r * 160 + x] != prow * 16 + x / 10) img_bad++;
  }
  CHECK(img_bad == 0, "imagen: bloques de 10x4 (filas 0-63) y cuatro barras de 9 lineas (64-99)");
  CHECK(mem[0xBE80] == 0xEE && mem[0xBFFF] == 0xEE, "no pasa de $BE7F");
  CHECK(frame_mode >= 0, "POKE 2045,175");
  CHECK(frame_mode >= frame_last_pal, "el modo se activa despues de la paleta y la imagen");
  CHECK(img_writes_after_mode == 0, "no toca la imagen con el modo activo");
  int steps_ok = 1, mx = 0, mn = 99; int prev = 8;
  for (int v : xadj_seq) { if (v < 0 || v > 15 || (v != prev + 1 && v != prev - 1)) steps_ok = 0; prev = v; if (v > mx) mx = v; if (v < mn) mn = v; }
  CHECK(steps_ok, "POKE 2099 cambia de uno en uno y entre 0 y 15");
  CHECK(mx == 15 && mn == 0, "llega a 15 con P y a 0 con O (los topes aguantan con la tecla pulsada)");
  CHECK(xadj_seq.size() == 7 + 15, "7 pasos hacia arriba (8 -> 15) y 15 hacia abajo (15 -> 0)");
  CHECK(!pokes.empty() && pokes.back().a == 2045 && pokes.back().v == 85, "al salir, POKE 2045,85 (modo normal)");
  printf("%s (%d errores)\n", errors ? "FALLOS" : "TODO OK", errors);
  return errors ? 1 : 0;
}
