// Prueba de EXAMPLES/SCROLLT/scrollt.bin en el PC: el Z80 de verdad (nucleo de EightyOne) ejecuta el programa de
// prueba del scroll fino por hardware, con la memoria (bitmap, atributos y bufers de entrada), los POKEs de
// configuracion ($2043-$2098), el puerto $3FEF del depurador, el teclado y el VSYNC simulados. Comprueba el dibujo
// inicial, que las teclas mueven FX / FY / HUD / automatico, lo que se escribe en los registros de la FPGA en cada
// momento y la salida. NO prueba la FPGA: eso es tb_spec_scroll_addr.v y la placa.
//
//   ./build_scrollt.sh        (necesita g++/gcc, pasmo y el nucleo Z80 de EightyOne)
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <map>
#include <functional>
extern "C" {
#include "z80.h"
int tstates = 0;
int StackChange = 0, StepOutRequested = 0, RetExecuted = 0;
int z80_fetch_is_m1 = 0;
void z80_m1_backtrack(void) {}
void debug(int) {}
}

static uint8_t mem[65536];
static uint8_t keyrows[8];                              // teclas pulsadas por fila (bit 0 = la primera de la fila)
static long instr_total = 0;
static uint64_t sim_us() { return (uint64_t)instr_total * 5000ULL / 3250ULL; }
static long frame() { return (long)(sim_us() / 20000ULL); }
static int errors = 0;
#define CHECK(c, msg) do { if (!(c)) { printf("  ERROR: %s\n", msg); errors++; } else printf("  ok    %s\n", msg); } while (0)

struct Poke { uint16_t a; uint8_t v; long f; };         // escrituras en la zona de configuracion de la ROM (POKEs)
struct Out { uint16_t port; uint8_t v; long f; };
static std::vector<Poke> pokes;
static std::vector<Out> outs;
static uint8_t last_poke[65536];
static bool bad_write = false; static uint16_t bad_addr = 0;
static uint16_t code_lo = 30000, code_hi = 30000;

extern "C" {
int  z80_contend_wrapper(int, int, int) { return 0; }
int  z80_contend_io_wrapper(int, int, int) { return 0; }
BYTE z80_readbyte_wrapper(int a) { return mem[a & 0xFFFF]; }
BYTE z80_readoperandbyte_wrapper(int a) { return mem[a & 0xFFFF]; }
BYTE z80_opcode_fetch_wrapper(int a) { z80_fetch_is_m1 = 0; return mem[a & 0xFFFF]; }
void z80_writebyte_wrapper(int a, int d) {
  a &= 0xFFFF;
  if (a < 0x2000) { pokes.push_back({(uint16_t)a, (uint8_t)d, frame()}); last_poke[a] = (uint8_t)d; return; }   // la ROM no se escribe: son puertos
  bool ok = (a >= 0x8000 && a < 0xA000) || (a >= code_lo && a < code_hi) || a >= 0xFF00;                     // pantalla, variables del programa, pila
  if (!ok && !bad_write) { bad_write = true; bad_addr = (uint16_t)a; }
  mem[a] = (uint8_t)d;
}
void z80_writeport_wrapper(int port, int d, int*) { outs.push_back({(uint16_t)port, (uint8_t)d, frame()}); }
BYTE z80_readport_wrapper(int port, int*) {
  int lo = port & 0xFF;
  if (lo == 0xAF) return (sim_us() % 20000ULL) < 512 ? 1 : 0;            // bit 0 = VSYNC, 50 Hz, alto 0,5 ms
  if (lo == 0xFE) {                                                       // teclado: B/A = la fila (0 = elegida), bits 0-4 a 0 = pulsada
    uint8_t v = 0x1F;
    for (int r = 0; r < 8; r++) if (!(((port >> 8) >> r) & 1)) v &= (uint8_t)~keyrows[r];
    return (uint8_t)(v | 0xE0);
  }
  return 0xFF;
}
}

static std::map<std::string, uint16_t> syms;
static void load_syms(const char* fn) {
  FILE* f = fopen(fn, "r"); if (!f) return;
  char name[128], eq[16], val[32];
  while (fscanf(f, "%127s %15s %31s", name, eq, val) == 3) {
    if (strcmp(eq, "EQU")) continue;
    std::string v = val; if (!v.empty() && (v.back() == 'H' || v.back() == 'h')) v.pop_back();
    syms[name] = (uint16_t)strtoul(v.c_str(), nullptr, 16);
  }
  fclose(f);
}
static uint8_t var(const char* n) { return mem[syms.at(n)]; }

// la ultima vez que se escribio el registro r del puerto $3FEF (OUT $40+r, dato): devuelve -1 si nunca
static int dbg_reg(int r, size_t upto = (size_t)-1) {
  int v = -1; size_t n = std::min(upto, outs.size());
  for (size_t i = 0; i + 1 < n; i++) if (outs[i].port == 0x3FEF && outs[i].v == 0x40 + r) { v = outs[i + 1].v; i++; }
  return v;
}
static int poke_last(int a) { return last_poke[a]; }

int main(int argc, char** argv) {
  const char* bin = argc > 1 ? argv[1] : "scrollt.bin";
  const char* sym = argc > 2 ? argv[2] : "scrollt.sym";
  FILE* f = fopen(bin, "rb");
  if (!f) { printf("no encuentro %s\n", bin); return 2; }
  size_t sz = fread(mem + 30000, 1, 0x4000, f); fclose(f);
  load_syms(sym);
  code_hi = (uint16_t)(30000 + sz);
  printf("scrollt.bin: %zu bytes en 30000-%u\n", sz, 30000 + (unsigned)sz - 1);
  CHECK(30000 + sz < 0x8000, "el programa queda por debajo de $8000");
  for (int i = 0; i < 0x200; i++) mem[0x1E00 + i] = (uint8_t)(i * 37 + 11);       // la "fuente" de la ROM: un patron conocido

  z80_init(); z80_reset();
  z80.sp.w = 0xFFF0; mem[0xFFF0] = 0x00; mem[0xFFF1] = 0x01; mem[0x0100] = 0x76;   // retorno a una trampa HALT en $0100
  z80.pc.w = 30000; z80.halted = 0; z80.iff1 = z80.iff2 = 0;

  // guion de teclas por cuadro: {desde, hasta, fila, bit}
  struct K { long from, to; int row, bit; };
  const K keys[] = {
    {10, 60, 5, 0},      // P: FX +
    {65, 120, 5, 1},     // O: FX -
    {130, 185, 2, 0},    // Q: FY +
    {195, 250, 1, 0},    // A: FY -
    {260, 262, 6, 4},    // H: HUD no
    {275, 277, 6, 4},    // H: HUD si
    {290, 292, 7, 0},    // SPACE: automatico
    {620, 622, 7, 0},    // SPACE: manual
    {640, 650, 0, 2},    // X: salir
  };
  // comprobaciones en un cuadro dado
  std::map<long, std::function<void()>> at;
  size_t o_mark = 0;

  at[6] = [&]() {
    printf("-- tras el dibujo inicial (cuadro %ld)\n", frame());
    CHECK(var("fx") == 0 && var("fy") == 0 && var("hud") == 1 && var("auto") == 0, "variables iniciales: FX 0, FY 0, HUD 1, automatico 0");
    CHECK(last_poke[2043] == 0 && last_poke[2044] == 0x80, "HFILE = $8000");
    CHECK(last_poke[2045] == 172, "modo Superfast HiRes Spectrum (POKE 2045,172)");
    CHECK(mem[0x8000 + (3 << 5) + 5] == 0xFF && mem[0x8000 + (3 << 5) + 5 + 3 * 256] == 0x90 && mem[0x8000 + 0x800 + 5 + 7 * 256 + 32 * 1] == 0x81,
          "cuadricula: arriba $FF, diagonal $90 en la linea 3, $81 en la 7 (en cada tercio)");
    bool ok = true;
    for (int r = 0; r < 24; r++) for (int c = 0; c < 32; c++)
      if (mem[0x9800 + r * 32 + c] != (uint8_t)(((r + c) % 6 + 1) << 3)) ok = false;
    CHECK(ok, "los 768 atributos: papel = ((fila + columna) mod 6) + 1, tinta negra");
    ok = true; for (int i = 0; i < 192; i++) if (mem[0x9B00 + i] != 0xAA) ok = false;
    for (int i = 0; i < 24; i++) if (mem[0x9BC0 + i] != 0x46) ok = false;
    CHECK(ok, "bufer de la derecha: 192 bytes $AA y 24 atributos $46");
    ok = true; for (int i = 0; i < 256; i++) if (mem[0x9D00 + i] != 0xFF) ok = false;
    for (int i = 0; i < 32; i++) if (mem[0x9E00 + i] != 0x44) ok = false;
    CHECK(ok, "bufer de abajo: 256 bytes $FF y 32 atributos $44");
    ok = true; for (int i = 0; i < 8; i++) if (mem[0x9E40 + i] != 0xFF) ok = false;
    CHECK(ok && mem[0x9E48] == 0x47, "esquina: 8 bytes $FF y el atributo $47");
    CHECK(mem[0x9BD8] == 0 && mem[0x9C00] == 0 && mem[0x9E49] == 0 && mem[0x9F00] == 0, "los bufers de la izquierda y de arriba no se tocan");
    // la fila 0 lleva el texto: los glifos de "FX=0" salen de la fuente (F = 43, X = 61, = = 20, 0 = 28)
    int codes[] = {43, 61, 20, 28};
    ok = true; for (int c = 0; c < 4; c++) for (int l = 0; l < 8; l++) if (mem[0x8000 + c + l * 256] != mem[0x1E00 + codes[c] * 8 + l]) ok = false;
    CHECK(ok, "el texto \"FX=0\" en la fila 0, con la fuente de la ROM");
    CHECK(dbg_reg(0) == 0 && dbg_reg(2) == 1 && dbg_reg(3) == 22, "puerto $3FEF: fino Y 0, region 1-22 (HUD)");
    CHECK(poke_last(2091) == 0xFE && poke_last(2092) == 0xFF && poke_last(2093) == 0x7F, "mascara de filas con el HUD: filas 0 y 23 fijas (FE, FF, 7F)");
  };
  at[62] = [&]() {
    printf("-- tras mantener P (cuadro %ld)\n", frame());
    CHECK(var("fx") == 7 && poke_last(2090) == 7, "FX llega a 7 y se escribe en POKE 2090");
    CHECK(var("fy") == 0 && dbg_reg(0) == 0, "FY sigue en 0");
    int d = 28 + 7; bool ok = true;
    for (int l = 0; l < 8; l++) if (mem[0x8000 + 3 + l * 256] != mem[0x1E00 + d * 8 + l]) ok = false;
    CHECK(ok, "el texto se redibuja: \"FX=7\"");
  };
  at[125] = [&]() {
    printf("-- tras mantener O (cuadro %ld)\n", frame());
    CHECK(var("fx") == 0 && poke_last(2090) == 0, "FX vuelve a 0");
  };
  at[190] = [&]() {
    printf("-- tras mantener Q (cuadro %ld)\n", frame());
    CHECK(var("fy") == 7 && dbg_reg(0) == 7, "FY llega a 7 y se escribe en el registro 8 del puerto $3FEF");
    CHECK(dbg_reg(2) == 1 && dbg_reg(3) == 22, "la region sigue siendo 1-22");
  };
  at[255] = [&]() {
    printf("-- tras mantener A (cuadro %ld)\n", frame());
    CHECK(var("fy") == 0 && dbg_reg(0) == 0, "FY vuelve a 0");
  };
  at[270] = [&]() {
    printf("-- tras la primera H (cuadro %ld)\n", frame());
    CHECK(var("hud") == 0, "HUD = 0");
    CHECK(dbg_reg(2) == 0 && dbg_reg(3) == 23, "la region pasa a 0-23");
    CHECK(poke_last(2091) == 0xFF && poke_last(2092) == 0xFF && poke_last(2093) == 0xFF, "y todas las filas se desplazan en horizontal (FF, FF, FF)");
  };
  at[285] = [&]() {
    printf("-- tras la segunda H (cuadro %ld)\n", frame());
    CHECK(var("hud") == 1, "HUD = 1 otra vez (una sola vez por pulsacion, aunque se mantenga 3 cuadros)");
    CHECK(dbg_reg(2) == 1 && dbg_reg(3) == 22 && poke_last(2091) == 0xFE && poke_last(2093) == 0x7F, "region 1-22 y filas 0 y 23 fijas otra vez");
    o_mark = outs.size();
  };
  at[300] = [&]() { CHECK(var("auto") == 1, "SPACE activa el modo automatico"); pokes.clear(); o_mark = outs.size(); };
  at[615] = [&]() {
    printf("-- modo automatico, 315 cuadros\n");
    int fxmin = 99, fxmax = -1, fymin = 99, fymax = -1, fxjump = 0, fyjump = 0, last = -1;
    for (auto& p : pokes) if (p.a == 2090) { fxmin = std::min(fxmin, (int)p.v); fxmax = std::max(fxmax, (int)p.v); }
    // FX cambia como mucho en 1 de un cuadro al siguiente (ida y vuelta); se mira la serie de valores distintos
    for (auto& p : pokes) if (p.a == 2090) { if (last >= 0 && p.v != last && abs((int)p.v - last) != 1) fxjump++; last = p.v; }
    last = -1;
    for (size_t i = o_mark; i + 1 < outs.size(); i++) if (outs[i].port == 0x3FEF && outs[i].v == 0x40) {
      int v = outs[i + 1].v; fymin = std::min(fymin, v); fymax = std::max(fymax, v);
      if (last >= 0 && v != last && abs(v - last) != 1) fyjump++; last = v; }
    CHECK(fxmin == 0 && fxmax == 7, "automatico: FX recorre de 0 a 7");
    CHECK(fymin == 0 && fymax == 7, "automatico: FY recorre de 0 a 7");
    CHECK(fxjump == 0 && fyjump == 0, "automatico: nunca salta mas de un paso de un cuadro al siguiente (ida y vuelta)");
  };
  at[630] = [&]() { CHECK(var("auto") == 0, "SPACE vuelve al modo manual"); };

  long guard = 0;
  while (z80.pc.w != 0x0100) {
    long fr = frame();
    for (int r = 0; r < 8; r++) keyrows[r] = 0;
    for (auto& k : keys) if (fr >= k.from && fr <= k.to) keyrows[k.row] |= (uint8_t)(1 << k.bit);
    auto it = at.find(fr);
    if (it != at.end()) { auto fn = it->second; at.erase(it); fn(); }
    z80_do_opcode(); instr_total++;
    if (++guard > 400000000L) { printf("ERROR: el Z80 no termina (PC=%04X)\n", z80.pc.w); return 1; }
    if (fr > 700) { printf("ERROR: no ha salido con X (cuadro %ld, PC=%04X)\n", fr, z80.pc.w); errors++; break; }
  }
  printf("-- salida con X (cuadro %ld)\n", frame());
  CHECK(z80.pc.w == 0x0100, "el programa vuelve (RET)");
  CHECK(poke_last(2045) == 85, "modo ZX81 normal (POKE 2045,85)");
  CHECK(poke_last(2090) == 0 && dbg_reg(0) == 0, "fino X e Y a 0");
  CHECK(dbg_reg(2) == 0 && dbg_reg(3) == 23 && poke_last(2091) == 0xFF && poke_last(2092) == 0xFF && poke_last(2093) == 0xFF,
        "region 0-23 y mascara de filas por defecto");
  CHECK(!bad_write, "solo escribe en la pantalla ($8000-$9FFF), en sus variables y en la pila");
  if (bad_write) printf("        primera escritura fuera de sitio: $%04X\n", bad_addr);
  bool unexpected = false;
  for (auto& o : outs) {
    if (o.port == 0x3FEF) { if (o.v != 0x40 && o.v != 0x42 && o.v != 0x43 && !(o.v <= 23 || o.v == 0xFF)) unexpected = true; }
    else if ((o.port & 0xFF) != 0xEF && (o.port & 0xFF) != 0xFB) unexpected = true;
  }
  CHECK(!unexpected, "solo usa los puertos $3FEF, $7FEF (color) y $FB (borde)");
  printf(errors ? "%d ERRORES\n" : "TODO OK\n", errors);
  return errors ? 1 : 0;
}
