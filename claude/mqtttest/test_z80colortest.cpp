// Prueba de EXAMPLES/COLORTEST/colortest.bin (la parte de la paleta de 16 colores) en el PC: el Z80 de EightyOne ejecuta el programa con la
// memoria, los POKEs de configuracion y el teclado simulados, y un modelo de la paleta de 16 de la FPGA (POKE 2063 puntero, 2064 byte bajo,
// 2066 byte alto: escribe pal16[puntero & 15] y avanza; 2067 bit 0 = activa). Comprueba que carga las 16 entradas de pal16.inc por ese
// protocolo, que NO toca la de 256 (2065), que arranca desactivada, que cada P pasa a la siguiente paleta de pal16.inc (la carga entera, con la
// paleta desactivada mientras se escribe, y la deja activa) y, tras la ultima, a desactivada (y vuelta a la primera), que otra tecla sale,
// y que al salir la deja desactivada aunque estuviera activa (colores por defecto). El teclado simula el REBOTE de la P (8 ms alternando abierta y
// cerrada cada 150 us al pulsar y al soltar, con el tiempo de los T-states del Z80) y una pulsacion de 100 ms: el programa no debe salir a BASIC por una
// P que rebota ni cambiar dos veces de paleta con una sola pulsacion. NO prueba la FPGA (tb_mode160.v) ni la placa.
//
//   ./build_colortest.sh        (necesita g++/gcc, pasmo, python y el nucleo Z80 de EightyOne)
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
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
static uint64_t total_t = 0;                                                    // T-states ejecutados (el nucleo pone tstates a 0 en cada instruccion)
static uint64_t sim_us() { return total_t * 1000ULL / 3250ULL; }              // Z80 a 3,25 MHz
static bool p_down = false; static uint64_t p_t = 0;                           // la P: estado real y cuando cambio
static const uint64_t BOUNCE_US = 8000, BOUNCE_STEP_US = 150;
static bool p_read() {                                                          // lo que ve una lectura del puerto ahora mismo
  uint64_t d = sim_us() - p_t;
  if (d < BOUNCE_US) { bool odd = ((d / BOUNCE_STEP_US) & 1) != 0; return p_down ? odd : !odd; }   // al pulsar empieza abierta; al soltar, cerrada
  return p_down;
}
static int errors = 0;
#define CHECK(c, msg) do { if (!(c)) { printf("  ERROR: %s\n", msg); errors++; } else printf("  ok    %s\n", msg); } while (0)

static std::vector<int> en_seq;                       // valores escritos en 2067, en orden
static uint16_t pal16[16]; static bool pal16_set[16];
static int pidx = 0, hi16_writes = 0, hi256_writes = 0; static uint8_t plo = 0;
static bool en_state = false;
static std::vector<int> en_at_write;                  // por cada escritura en 2066: estaba activa?
static bool bad_write = false; static uint16_t bad_addr = 0;
static uint16_t code_hi = 24576;

extern "C" {
int  z80_contend_wrapper(int, int, int time) { return time; }
int  z80_contend_io_wrapper(int, int, int time) { return time; }
BYTE z80_readbyte_wrapper(int a) { return mem[a & 0xFFFF]; }
BYTE z80_readoperandbyte_wrapper(int a) { return mem[a & 0xFFFF]; }
BYTE z80_opcode_fetch_wrapper(int a) { z80_fetch_is_m1 = 0; return mem[a & 0xFFFF]; }
void z80_writebyte_wrapper(int a, int d) {
  a &= 0xFFFF;
  if (a < 0x2000) {
    if (a == 2063) pidx = d & 255;
    if (a == 2064) plo = (uint8_t)d;
    if (a == 2065) hi256_writes++;
    if (a == 2066) { pal16[pidx & 15] = (uint16_t)((d << 8) | plo); pal16_set[pidx & 15] = true; pidx = (pidx + 1) & 255; hi16_writes++; en_at_write.push_back(en_state); }
    if (a == 2067) { en_state = d & 1; en_seq.push_back(d & 1); }
    return;
  }
  bool ok = (a >= 24576 && a < code_hi) || (a >= 0xA000 && a < 0xBB00) || (a >= 0x7F00 && a < 0x7F10) || a >= 0xFF00;
  if (!ok && !bad_write) { bad_write = true; bad_addr = (uint16_t)a; }
  mem[a] = (uint8_t)d;
}
void z80_writeport_wrapper(int, int, int*) {}
BYTE z80_readport_wrapper(int port, int*) {
  int lo = port & 0xFF;
  if (lo == 0xFE) {
    uint8_t v = 0x1F;
    uint8_t rows[8]; memcpy(rows, keyrows, sizeof rows);
    if (p_read()) rows[5] |= 1;
    for (int r = 0; r < 8; r++) if (!(((port >> 8) >> r) & 1)) v &= (uint8_t)~rows[r];
    return (uint8_t)(v | 0xE0);
  }
  return 0xFF;
}
}

static unsigned sym(const char* symf, const char* name) {
  FILE* f = fopen(symf, "r"); if (!f) return 0;
  char line[256]; unsigned v = 0;
  while (fgets(line, sizeof line, f)) {
    char n[128], e[16], h[32];
    if (sscanf(line, "%127s %15s %31s", n, e, h) == 3 && !strcmp(n, name)) v = (unsigned)strtoul(h, nullptr, 16);
  }
  fclose(f); return v;
}

int main(int argc, char** argv) {
  const char* bin = argc > 1 ? argv[1] : "colortest.bin";
  const char* symf = argc > 2 ? argv[2] : "colortest.sym";
  const char* incf = argc > 3 ? argv[3] : "pal16.inc";
  FILE* f = fopen(bin, "rb");
  if (!f) { printf("no encuentro %s\n", bin); return 2; }
  size_t sz = fread(mem + 24576, 1, 0x4000, f); fclose(f);
  code_hi = (uint16_t)(24576 + sz);
  unsigned a_wait = sym(symf, "wait_k");
  if (!a_wait) { printf("no encuentro wait_k en %s\n", symf); return 2; }
  // la tabla esperada, de pal16.inc (lineas "defb 0xxh,0yyh")
  static uint8_t want[1024]; int nw = 0;
  f = fopen(incf, "r"); if (!f) { printf("no encuentro %s\n", incf); return 2; }
  char line[256];
  while (fgets(line, sizeof line, f)) { unsigned lo, hi; if (sscanf(line, " defb 0%xh,0%xh", &lo, &hi) == 2 && nw < 256) { want[2 * nw] = (uint8_t)lo; want[2 * nw + 1] = (uint8_t)hi; nw++; } }
  fclose(f);
  printf("colortest.bin: %zu bytes en 24576-%u, wait_k = %04X, %d entradas en %s\n", sz, 24576 + (unsigned)sz - 1, a_wait, nw, incf);
  CHECK(nw > 0 && nw % 16 == 0, "pal16.inc son paletas enteras de 16 entradas");
  const int npal = nw / 16;
  printf("  %d paletas\n", npal);
  CHECK(24576 + sz < 0x7F00, "el programa queda por debajo de las variables ($7F00)");

  z80_init(); z80_reset();
  z80.sp.w = 0xFFF0; mem[0xFFF0] = 0x00; mem[0xFFF1] = 0x01; mem[0x0100] = 0x76;
  z80.pc.w = 24576; z80.halted = 0; z80.iff1 = z80.iff2 = 0;
  mem[0x02E7] = 0xC9; mem[0x0207] = 0xC9;                                          // SET_FAST y SLOW_FAST de la ROM: un RET

  // guion: P npal + 2 veces (todas las paletas, desactivada y la primera otra vez) y X (salir). Cada P se mantiene hasta que el programa escribe 2067 y
  // se suelta; al volver el programa a wait_k se comprueba el estado que ha dejado.
  enum { P, X };
  std::vector<int> script(npal + 2, P); script.push_back(X);
  size_t step = 0, checked = 0; int phase = 0; long guard = 0; int state_bad = 0;
  uint64_t press_t = 0, rel_t = 0;
  const uint64_t HOLD_US = 100000, SETTLE_US = 40000;
  memset(keyrows, 0, sizeof keyrows);
  while (z80.pc.w != 0x0100) {
    if (z80.pc.w == a_wait && phase == 0 && (rel_t == 0 || sim_us() >= rel_t + SETTLE_US)) {   // listo para recibir una tecla
      if (step > checked) {                                   // estado que dejo la ultima P: estado = step mod (npal + 1)
        int st = (int)(step % (npal + 1));
        if (st == 0) { if (en_state) state_bad++; }
        else {
          if (!en_state) state_bad++;
          for (int k = 0; k < 16; k++) {
            uint16_t w = (uint16_t)((want[2 * (16 * (st - 1) + k) + 1] << 8) | want[2 * (16 * (st - 1) + k)]);
            if (pal16[k] != w) state_bad++;
          }
        }
        checked = step;
      }
      if (step < script.size()) {
        memset(keyrows, 0, sizeof keyrows);
        if (script[step] == P) { p_down = true; p_t = sim_us(); press_t = p_t; }
        else keyrows[0] |= 4;
        phase = 1;
      }
    }
    if (phase == 1 && script[step] == P && sim_us() >= press_t + HOLD_US) {   // se suelta a los 100 ms
      p_down = false; p_t = sim_us(); rel_t = p_t;
      phase = 0; step++;
    }
    z80_do_opcode();
    total_t += (uint64_t)tstates;
    if (++guard > 400000000L) { printf("ERROR: el Z80 no termina (PC=%04X, paso %zu)\n", z80.pc.w, step); return 1; }
  }
  printf("  simulados %.2f s\n", sim_us() / 1e6);
  CHECK(z80.pc.w == 0x0100, "el programa vuelve (RET)");
  CHECK(!bad_write, "no escribe fuera del programa, la pantalla, las variables y la pila");
  if (bad_write) printf("        primera escritura rara en $%04X\n", bad_addr);
  CHECK(hi16_writes == 16 * (npal + 1), "una carga de 16 entradas (2066) por cada paleta mostrada");
  CHECK(hi256_writes == 0, "no toca la paleta de 256 (2065)");
  CHECK(state_bad == 0, "despues de cada P: la paleta que toca cargada entera y activa (o desactivada tras la ultima)");
  bool all_off = !en_at_write.empty();
  for (int e : en_at_write) if (e) all_off = false;
  CHECK(all_off, "cada carga se hace con la paleta desactivada");
  std::vector<int> exp = { 0 };                            // al arrancar
  for (int k = 1; k <= npal + 2; k++) { int st = k % (npal + 1); exp.push_back(0); exp.push_back(st ? 1 : 0); }
  exp.push_back(0);                                        // al salir
  if (en_seq != exp) { printf("  obtenido:"); for (int v : en_seq) printf(" %d", v); printf("\n  esperado:"); for (int v : exp) printf(" %d", v); printf("\n"); }
  CHECK(en_seq == exp, "POKE 2067: desactiva al cambiar, activa (o deja a 0) segun haya paleta, y 0 al salir");
  CHECK(!en_state, "al salir la paleta queda desactivada (colores por defecto)");
  printf("%s (%d errores)\n", errors ? "FALLOS" : "TODO OK", errors);
  return errors ? 1 : 0;
}
