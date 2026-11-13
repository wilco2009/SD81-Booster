// Prueba de EXAMPLES/SOLITARIO/solitario.bin (fases 1, 2 y 3) en el PC: el Z80 de verdad (nucleo de EightyOne) ejecuta el arranque
// hasta ml_init y despues las rutinas de dibujo, modelo y arrastre llamadas una a una, con la memoria y los puertos simulados y
// la ROM real (zx81v2.rom, para la fuente de la barra). Vuelca la pantalla ($A000 bitmap + $C000 atributos) a screen.bin y varias
// cartas dibujadas con paso 3 (bufer de sprites, con y sin giro de nibbles) a buf.bin; cmp_solitario.py los compara con
// preview_mesa.png y con el modelo de gen_cards.py. La fase 3 (pickup/drop) se prueba llamando a esas rutinas directamente, sin
// pasar por drag_loop (que necesita el protocolo del raton con el MCU): un movimiento legal, uno ilegal (se cancela) y un clic
// sin arrastre, comprobando el modelo (tab_n/tab) y que no hay escrituras fuera de sitio. NO prueba el raton real ni la FPGA.
//
//   ./build_solitario.sh
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
static long instr_total = 0;
static int errors = 0;
#define CHECK(c, msg) do { if (!(c)) { printf("  ERROR: %s\n", msg); errors++; } else printf("  ok    %s\n", msg); } while (0)

struct Out { uint16_t port; uint8_t v; };
static std::vector<Out> outs;
static uint8_t last_poke[65536];
static bool bad_write = false; static uint16_t bad_addr = 0;
static uint16_t code_lo = 20480, code_hi = 20480;

extern "C" {
int  z80_contend_wrapper(int, int, int) { return 0; }
int  z80_contend_io_wrapper(int, int, int) { return 0; }
BYTE z80_readbyte_wrapper(int a) { return mem[a & 0xFFFF]; }
BYTE z80_readoperandbyte_wrapper(int a) { return mem[a & 0xFFFF]; }
BYTE z80_opcode_fetch_wrapper(int a) { z80_fetch_is_m1 = 0; return mem[a & 0xFFFF]; }
void z80_writebyte_wrapper(int a, int d) {
  a &= 0xFFFF;
  if (a < 0x2000) { last_poke[a] = (uint8_t)d; return; }                       // POKEs de configuracion: la ROM no se escribe
  bool ok = (a >= 0x8000 && a < 0x9000) || (a >= 0x9000 && a < 0x9800) || (a >= 0xA000 && a < 0xB800) || (a >= 0xC000 && a < 0xD800) || a >= 0xFF00;
  if (!ok && !bad_write) { bad_write = true; bad_addr = (uint16_t)a; }
  mem[a] = (uint8_t)d;
}
void z80_writeport_wrapper(int port, int d, int*) { outs.push_back({(uint16_t)port, (uint8_t)d}); }
BYTE z80_readport_wrapper(int port, int*) {
  int lo = port & 0xFF;
  if (lo == 0xFE) return 0xFF;                                                // ninguna tecla
  if (lo == 0xAF) { static uint8_t af_bit = 0; af_bit ^= 1; return af_bit; }   // VSYNC: alterna (si no, waitvs se cuelga)
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
    std::string n = name; for (auto& c : n) c = (char)tolower(c);
    syms[n] = (uint16_t)strtoul(v.c_str(), nullptr, 16);
  }
  fclose(f);
}
static uint16_t S(const char* n) { return syms.at(n); }

// ejecuta hasta que el PC llega a "stop" (o a la trampa $0100 si stop = 0)
static bool run_until(uint16_t stop) {
  long guard = 0;
  while (z80.pc.w != stop) {
    z80_do_opcode(); instr_total++;
    if (++guard > 200000000L) { printf("ERROR: el Z80 no llega a $%04X (PC=%04X)\n", stop, z80.pc.w); return false; }
  }
  return true;
}
static void call_at(uint16_t addr, int a, int b, int c, int d, int e, int h, int l) {
  z80.sp.w = 0xFFF0; mem[0xFFF0] = 0x00; mem[0xFFF1] = 0x01; mem[0x0100] = 0x76;
  z80.af.b.h = (BYTE)a; z80.bc.b.h = (BYTE)b; z80.bc.b.l = (BYTE)c; z80.de.b.h = (BYTE)d; z80.de.b.l = (BYTE)e;
  z80.hl.b.h = (BYTE)h; z80.hl.b.l = (BYTE)l;
  z80.pc.w = addr; z80.halted = 0;
  if (!run_until(0x0100)) { errors++; }
}

int main(int argc, char** argv) {
  const char* bin = argc > 1 ? argv[1] : "solitario.bin";
  const char* sym = argc > 2 ? argv[2] : "solitario.sym";
  const char* rom = argc > 3 ? argv[3] : "zx81v2.rom";
  FILE* f = fopen(bin, "rb");
  if (!f) { printf("no encuentro %s\n", bin); return 2; }
  size_t sz = fread(mem + 20480, 1, 0x3000 + 1, f); fclose(f);
  code_hi = (uint16_t)(20480 + sz);
  load_syms(sym);
  f = fopen(rom, "rb");
  if (!f) { printf("no encuentro %s\n", rom); return 2; }
  fread(mem, 1, 0x2000, f); fclose(f);
  printf("solitario.bin: %zu bytes en 20480-%u\n", sz, 20480 + (unsigned)sz - 1);
  CHECK(sz <= 12288, "el programa cabe en $5000-$7FFF (12288 bytes)");

  z80_init(); z80_reset();
  z80.iff1 = z80.iff2 = 0;

  // --- arranque hasta ml_init ---
  printf("-- arranque\n");
  z80.sp.w = 0xFFF0; mem[0xFFF0] = 0x00; mem[0xFFF1] = 0x01; mem[0x0100] = 0x76;
  z80.pc.w = 20480; z80.halted = 0;
  CHECK(run_until(S("ml_init")), "el arranque llega a ml_init");
  CHECK(last_poke[2043] == 0 && last_poke[2044] == 0xA0, "HFILE = $A000 (POKE 2043,0 / 2044,$A0)");
  CHECK(last_poke[2045] == 171, "modo Superfast HiRes nativo (POKE 2045,171)");
  int chroma = -1; for (auto& o : outs) if (o.port == 0x7FEF) chroma = o.v;
  CHECK(chroma == 0x20, "Chroma activo, modo 0 ($7FEF = $20)");
  CHECK(z80.bc.b.h == 191 && z80.bc.b.l == 255, "ml_init con Y maximo 191 y X maximo 255");
  CHECK(!z80.iff1, "interrupciones desactivadas (DI)");
  FILE* o = fopen("screen.bin", "wb");
  fwrite(mem + 0xA000, 1, 6144, o); fwrite(mem + 0xC000, 1, 6144, o); fwrite(mem + 0x8200, 1, 512, o); fclose(o);

  // --- draw_card con paso 3: cartas en un bufer lineal de 96 bytes cada una, con atributos aparte ---
  printf("-- draw_card con paso 3\n");
  memset(mem + 0x9000, 0xEE, 0x400);      // bitmap de las cartas de prueba: $8400 + i*96
  memset(mem + 0x9400, 0xEE, 0x400);      // atributos: $8800 + i*96
  struct T { int code, lines, swap; };
  const T tests[] = {
    {0x80 + 0 * 16 + 0, 32, 0},           // A de picas
    {0x80 + 1 * 16 + 9, 32, 0},           // 10 de corazones
    {0x80 + 2 * 16 + 10, 32, 1},          // J de diamantes, atributo girado (sprite)
    {0x80 + 3 * 16 + 11, 32, 0},          // Q de treboles
    {0x80 + 0 * 16 + 12, 32, 1},          // K de picas, girado
    {0x00, 32, 0},                        // dorso
    {0x00, 4, 0},                         // dorso, banda de 4 lineas
    {0x80 + 1 * 16 + 4, 10, 0},           // 5 de corazones, banda de 10 lineas
    {0xFF, 32, 0},                        // hueco
  };
  for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
    mem[S("dc_pitch")] = 3; mem[S("dc_swap")] = (uint8_t)tests[i].swap;
    uint16_t bm = (uint16_t)(0x9000 + i * 96), at = (uint16_t)(0x9400 + i * 96);
    call_at(S("draw_card"), tests[i].code, tests[i].lines, 0, at >> 8, at & 255, bm >> 8, bm & 255);
  }
  o = fopen("buf.bin", "wb"); fwrite(mem + 0x9000, 1, 0x400, o); fwrite(mem + 0x9400, 1, 0x400, o); fclose(o);
  CHECK(!bad_write, "solo escribe en la pantalla, sus atributos, el buzon/variables ($8000-$83FF), los bufers de prueba y la pila");
  if (bad_write) printf("        primera escritura fuera de sitio: $%04X\n", bad_addr);
  // --- fase 2: modelo, mesa y prueba de impacto ---
  auto dump = [&](const char* name, bool hits) {
    std::string fn = std::string("sc_") + name + ".bin";
    FILE* fo = fopen(fn.c_str(), "wb");
    fwrite(mem + 0xA000, 1, 6144, fo); fwrite(mem + 0xC000, 1, 6144, fo); fwrite(mem + 0x8200, 1, 512, fo); fclose(fo);
    if (hits) {
      fn = std::string("hits_") + name + ".bin"; fo = fopen(fn.c_str(), "wb");
      for (int y = 0; y < 192; y += 2) for (int x = 0; x < 256; x += 2) {
        mem[S("ms_base") + 0] = (uint8_t)x; mem[S("ms_base") + 1] = (uint8_t)y;
        call_at(S("hit_test"), 0, 0, 0, 0, 0, 0, 0);
        fputc(mem[S("ht_area")], fo); fputc(mem[S("ht_idx")], fo); fputc(mem[S("ht_card")], fo);
      }
      fclose(fo);
    }
  };
  auto scen = [&](const char* name, int seed, int mode) {
    printf("-- %s (semilla %04X, robo de %d)\n", name, seed, mode);
    mem[S("seed")] = (uint8_t)seed; mem[S("seed") + 1] = (uint8_t)(seed >> 8);
    mem[S("draw_mode")] = (uint8_t)mode;
    call_at(S("shuffle_deal"), 0, 0, 0, 0, 0, 0, 0);
    call_at(S("draw_board"), 0, 0, 0, 0, 0, 0, 0);
    dump((std::string(name) + "0").c_str(), true);
    for (int i = 1; i <= 10; i++) {                         // clics en el mazo: roba hasta vaciarlo y recicla
      mem[S("ms_base") + 0] = 20; mem[S("ms_base") + 1] = 10;
      call_at(S("hit_test"), 0, 0, 0, 0, 0, 0, 0);
      CHECK(mem[S("ht_area")] == 1, "hit_test: el mazo esta en (20,10)");
      call_at(S("stock_click"), 0, 0, 0, 0, 0, 0, 0);
      char nm[32]; snprintf(nm, sizeof nm, "%s%d", name, i);
      dump(nm, i == 3 || i == 10);
    }
  };
  scen("a", 0x1234, 3);
  scen("b", 0xBEEF, 1);

  printf("-- tablero a tope (peor caso de solape) y varias areas\n");
  {
    const int n[7]  = {0, 4, 6, 19, 1, 14, 10};
    const int nd[7] = {0, 1, 2, 6, 0, 5, 3};
    for (int k = 0; k < 7; k++) {
      mem[S("tab_n") + k] = (uint8_t)n[k];
      for (int i = 0; i < n[k]; i++) {
        int v = (k * 7 + i * 5) % 52;
        mem[S("tab") + 20 * k + i] = (uint8_t)(((v / 13) << 4) | (v % 13) | (i >= nd[k] ? 0x80 : 0));
      }
    }
    mem[S("stock_n")] = 5; for (int i = 0; i < 5; i++) mem[S("stock") + i] = (uint8_t)(0x10 + i);
    mem[S("waste_n")] = 7; mem[S("waste_vis")] = 3;
    for (int i = 0; i < 7; i++) mem[S("waste") + i] = (uint8_t)(0x80 | (0x20 + i));
    const int fn[4] = {0, 5, 13, 1}; for (int i = 0; i < 4; i++) { mem[S("found_n") + i] = (uint8_t)fn[i]; mem[S("found_s") + i] = (uint8_t)i; }
    mem[S("draw_mode")] = 3;
    call_at(S("draw_board"), 0, 0, 0, 0, 0, 0, 0);
    dump("full", true);
  }
  CHECK(!bad_write, "la fase 2 tampoco escribe fuera de sitio");
  if (bad_write) printf("        primera escritura fuera de sitio: $%04X\n", bad_addr);

  // --- fase 3: arrastre (pickup / drop), llamando a las rutinas directamente ---
  auto clear_model = [&]() {
    for (int k = 0; k < 7; k++) mem[S("tab_n") + k] = 0;
    mem[S("stock_n")] = 0; mem[S("waste_n")] = 0; mem[S("waste_vis")] = 0;
    for (int i = 0; i < 4; i++) { mem[S("found_n") + i] = 0; mem[S("found_s") + i] = 0; }
  };
  auto setup_col = [&](int k, uint8_t card) { mem[S("tab_n") + k] = 1; mem[S("tab") + 20 * k] = card; };
  auto point_at_col0 = [&]() {
    mem[S("ms_base") + 0] = 26; mem[S("ms_base") + 1] = 45;   // sobre la carta de la columna 0 (x=16, y=40)
    call_at(S("hit_test"), 0, 0, 0, 0, 0, 0, 0);
  };
  const uint8_t C6H = 0x80 | (1 << 4) | 5;   // 6 de corazones (roja), boca arriba
  const uint8_t C7S = 0x80 | (0 << 4) | 6;   // 7 de picas (negra)
  const uint8_t C7H = 0x80 | (1 << 4) | 6;   // 7 de corazones (roja)

  printf("-- fase 3, escenario A: 6H sobre 7S (legal)\n");
  clear_model(); setup_col(0, C6H); setup_col(1, C7S);
  call_at(S("draw_board"), 0, 0, 0, 0, 0, 0, 0);
  point_at_col0();
  CHECK(mem[S("ht_area")] == 4 && mem[S("ht_idx")] == 0 && mem[S("ht_card")] == 0, "hit_test encuentra la carta de la columna 0");
  call_at(S("pickup"), 0, 0, 0, 0, 0, 0, 0);
  CHECK(z80.af.b.l & 1, "pickup agarra la carta (CARRY)");
  CHECK(mem[S("tab_n") + 0] == 0, "pickup quita la carta de su columna de origen");
  mem[S("dr_moved")] = 1; mem[S("dr_px")] = 50; mem[S("dr_py")] = 45;   // se suelta sobre la columna 1
  call_at(S("drop"), 0, 0, 0, 0, 0, 0, 0);
  CHECK(mem[S("tab_n") + 1] == 2, "drop: movimiento legal -> se anade a la columna destino");
  CHECK(mem[S("tab") + 20 * 1 + 1] == C6H, "drop: la carta movida llega boca arriba y sin cambiar");
  CHECK(!bad_write, "escenario A: sin escrituras fuera de sitio");
  if (bad_write) printf("        primera escritura fuera de sitio: $%04X\n", bad_addr);

  printf("-- fase 3, escenario B: 6H sobre 7H (mismo color, ilegal, se cancela)\n");
  bad_write = false;
  clear_model(); setup_col(0, C6H); setup_col(1, C7H);
  call_at(S("draw_board"), 0, 0, 0, 0, 0, 0, 0);
  point_at_col0();
  call_at(S("pickup"), 0, 0, 0, 0, 0, 0, 0);
  CHECK(z80.af.b.l & 1, "pickup agarra la carta (CARRY)");
  mem[S("dr_moved")] = 1; mem[S("dr_px")] = 50; mem[S("dr_py")] = 45;
  call_at(S("drop"), 0, 0, 0, 0, 0, 0, 0);
  CHECK(mem[S("tab_n") + 0] == 1, "drop: movimiento ilegal -> la carta vuelve a su columna");
  CHECK(mem[S("tab_n") + 1] == 1, "drop: la columna de destino no cambia");
  CHECK(!bad_write, "escenario B: sin escrituras fuera de sitio (incluye el deslizamiento de cancel_slide)");
  if (bad_write) printf("        primera escritura fuera de sitio: $%04X\n", bad_addr);

  printf("-- fase 3, escenario G: As a la fundacion (como en la foto del usuario) -> la tapada de debajo deberia destaparse\n");
  bad_write = false;
  clear_model();
  const uint8_t CAH = 0x80 | (1 << 4) | 0;                  // As de corazones (rango 0), boca arriba
  mem[S("tab_n") + 1] = 2;
  mem[S("tab") + 20 * 1 + 0] = (uint8_t)((2 << 4) | 2);     // tapada (no importa cual)
  mem[S("tab") + 20 * 1 + 1] = CAH;                         // As de corazones, la de arriba
  call_at(S("draw_board"), 0, 0, 0, 0, 0, 0, 0);
  int yg = -1;
  for (int y = 40; y < 100; y++) {
    mem[S("ms_base") + 0] = 58; mem[S("ms_base") + 1] = (uint8_t)y;   // columna 1 (x = 48)
    call_at(S("hit_test"), 0, 0, 0, 0, 0, 0, 0);
    if (mem[S("ht_area")] == 4 && mem[S("ht_idx")] == 1 && mem[S("ht_card")] == 1) { yg = y; break; }
  }
  CHECK(yg >= 0, "hit_test encuentra el As en la columna 1");
  call_at(S("pickup"), 0, 0, 0, 0, 0, 0, 0);
  CHECK(z80.af.b.l & 1, "pickup agarra el As");
  CHECK(mem[S("tab_n") + 1] == 1, "pickup: la columna 1 se queda con la tapada");
  mem[S("dr_moved")] = 1; mem[S("dr_px")] = 90; mem[S("dr_py")] = 10;   // zona de las fundaciones (arriba)
  call_at(S("drop"), 0, 0, 0, 0, 0, 0, 0);
  CHECK(mem[S("found_n") + 0] == 1 && mem[S("found_s") + 0] == 1, "drop: el As llega a la primera fundacion libre (corazones)");
  CHECK(mem[S("tab") + 20 * 1 + 0] & 0x80, "drop: la tapada de la columna 1 se destapa sola tras ir a la fundacion");
  CHECK(!bad_write, "escenario G: sin escrituras fuera de sitio");
  if (bad_write) printf("        primera escritura fuera de sitio: $%04X\n", bad_addr);

  printf("-- fase 3, escenario D: debajo de la carta movida hay una tapada -> se destapa sola\n");
  bad_write = false;
  clear_model();
  mem[S("tab_n") + 0] = 2;
  mem[S("tab") + 20 * 0 + 0] = (uint8_t)((2 << 4) | 2);   // tapada (bit 7 = 0): 3 de diamantes
  mem[S("tab") + 20 * 0 + 1] = C6H;                       // destapada (la de arriba)
  setup_col(1, C7S);
  call_at(S("draw_board"), 0, 0, 0, 0, 0, 0, 0);
  // busca donde esta la carta de arriba (indice 1) en la columna 0
  int y_found = -1;
  for (int y = 40; y < 100; y++) {
    mem[S("ms_base") + 0] = 26; mem[S("ms_base") + 1] = (uint8_t)y;
    call_at(S("hit_test"), 0, 0, 0, 0, 0, 0, 0);
    if (mem[S("ht_area")] == 4 && mem[S("ht_idx")] == 0 && mem[S("ht_card")] == 1) { y_found = y; break; }
  }
  CHECK(y_found >= 0, "hit_test encuentra la carta de arriba (indice 1) de la columna con una tapada debajo");
  call_at(S("pickup"), 0, 0, 0, 0, 0, 0, 0);
  CHECK(z80.af.b.l & 1, "pickup agarra la carta de arriba");
  CHECK(mem[S("tab_n") + 0] == 1, "pickup: la columna de origen se queda con la tapada (1 carta)");
  CHECK(!(mem[S("tab") + 20 * 0 + 0] & 0x80), "pickup: la tapada sigue boca abajo justo al agarrar (se destapa al soltar)");
  mem[S("dr_moved")] = 1; mem[S("dr_px")] = 50; mem[S("dr_py")] = 45;
  call_at(S("drop"), 0, 0, 0, 0, 0, 0, 0);
  CHECK(mem[S("tab") + 20 * 0 + 0] & 0x80, "drop: la tapada que queda en la columna de origen se destapa sola");
  CHECK(mem[S("tab_n") + 1] == 2, "drop: la carta movida se anade a la columna destino");
  CHECK(!bad_write, "escenario D: sin escrituras fuera de sitio");
  if (bad_write) printf("        primera escritura fuera de sitio: $%04X\n", bad_addr);

  printf("-- fase 3, escenario E: dos arrastres seguidos en la misma columna (como en una partida real)\n");
  bad_write = false;
  clear_model();
  const uint8_t CTAPADA = (uint8_t)((2 << 4) | 2);          // tapada, no importa cual
  const uint8_t C7C = 0x80 | (3 << 4) | 6;                  // 7 de treboles (negra)
  const uint8_t C8H = 0x80 | (1 << 4) | 7;                  // 8 de corazones (roja)
  mem[S("tab_n") + 0] = 3;
  mem[S("tab") + 20 * 0 + 0] = CTAPADA;                     // tapada
  mem[S("tab") + 20 * 0 + 1] = C7S;                         // destapada
  mem[S("tab") + 20 * 0 + 2] = C6H;                         // destapada (la de arriba)
  setup_col(1, C7C);
  setup_col(2, C8H);
  call_at(S("draw_board"), 0, 0, 0, 0, 0, 0, 0);
  auto find_top_y = [&](int col, int card_idx) {
    for (int y = 40; y < 150; y++) {
      mem[S("ms_base") + 0] = (uint8_t)(16 + col * 32 + 10); mem[S("ms_base") + 1] = (uint8_t)y;
      call_at(S("hit_test"), 0, 0, 0, 0, 0, 0, 0);
      if (mem[S("ht_area")] == 4 && mem[S("ht_idx")] == col && mem[S("ht_card")] == card_idx) return y;
    }
    return -1;
  };
  int y1 = find_top_y(0, 2);
  CHECK(y1 >= 0, "1er arrastre: hit_test encuentra la carta de arriba (indice 2)");
  call_at(S("pickup"), 0, 0, 0, 0, 0, 0, 0);
  CHECK(z80.af.b.l & 1, "1er arrastre: pickup agarra la carta");
  mem[S("dr_moved")] = 1; mem[S("dr_px")] = (uint8_t)(16 + 32 + 10); mem[S("dr_py")] = 45;   // columna 1
  call_at(S("drop"), 0, 0, 0, 0, 0, 0, 0);
  CHECK(mem[S("tab_n") + 0] == 2, "1er arrastre: la columna de origen se queda con 2 cartas");
  CHECK(mem[S("tab_n") + 1] == 2, "1er arrastre: se anade a la columna 1");
  int y2 = find_top_y(0, 1);
  CHECK(y2 >= 0, "2o arrastre: hit_test encuentra la nueva carta de arriba (indice 1)");
  call_at(S("pickup"), 0, 0, 0, 0, 0, 0, 0);
  CHECK(z80.af.b.l & 1, "2o arrastre: pickup agarra la carta");
  CHECK(mem[S("tab_n") + 0] == 1, "2o arrastre: la columna de origen se queda con la tapada (1 carta)");
  mem[S("dr_moved")] = 1; mem[S("dr_px")] = (uint8_t)(16 + 64 + 10); mem[S("dr_py")] = 45;   // columna 2
  call_at(S("drop"), 0, 0, 0, 0, 0, 0, 0);
  CHECK(mem[S("tab_n") + 2] == 2, "2o arrastre: se anade a la columna 2");
  CHECK(mem[S("tab") + 20 * 0 + 0] & 0x80, "2o arrastre: la tapada que queda se destapa sola (tras un arrastre previo)");
  CHECK(!bad_write, "escenario E: sin escrituras fuera de sitio");
  if (bad_write) printf("        primera escritura fuera de sitio: $%04X\n", bad_addr);

  printf("-- fase 3, escenario F: arrastre de dos cartas juntas (un grupo), debajo queda una tapada\n");
  bad_write = false;
  clear_model();
  const uint8_t C8S = 0x80 | (0 << 4) | 7;                  // 8 de picas (negra)
  mem[S("tab_n") + 0] = 3;
  mem[S("tab") + 20 * 0 + 0] = CTAPADA;                     // tapada
  mem[S("tab") + 20 * 0 + 1] = C8S;                         // destapada (se arrastra junto con la siguiente)
  mem[S("tab") + 20 * 0 + 2] = C7H;                         // destapada (encima de C8S: 7H sobre 8S es legal)
  setup_col(1, 0x80 | (1 << 4) | 8);                        // destino: 9 de corazones (roja), acepta la de abajo del grupo (8S, negra)
  call_at(S("draw_board"), 0, 0, 0, 0, 0, 0, 0);
  int yf = find_top_y(0, 1);   // el indice 1 (C8S), no el de arriba: se arrastran las dos
  CHECK(yf >= 0, "hit_test encuentra la carta de en medio (indice 1)");
  call_at(S("pickup"), 0, 0, 0, 0, 0, 0, 0);
  CHECK(z80.af.b.l & 1, "pickup agarra el grupo de dos cartas");
  CHECK(mem[S("dr_m")] == 2, "pickup: se arrastran las dos cartas (C8S y C7H)");
  CHECK(mem[S("tab_n") + 0] == 1, "pickup: la columna de origen se queda con la tapada");
  mem[S("dr_moved")] = 1; mem[S("dr_px")] = (uint8_t)(16 + 32 + 10); mem[S("dr_py")] = 45;
  call_at(S("drop"), 0, 0, 0, 0, 0, 0, 0);
  CHECK(mem[S("tab_n") + 1] == 3, "drop: se anaden las dos cartas a la columna destino");
  CHECK(mem[S("tab") + 20 * 1 + 1] == C8S && mem[S("tab") + 20 * 1 + 2] == C7H, "drop: llegan en el mismo orden");
  CHECK(mem[S("tab") + 20 * 0 + 0] & 0x80, "drop: la tapada que queda en el origen se destapa sola");
  CHECK(!bad_write, "escenario F: sin escrituras fuera de sitio");
  if (bad_write) printf("        primera escritura fuera de sitio: $%04X\n", bad_addr);

  printf("-- fase 3, escenario C: clic sin arrastrar (dr_moved = 0)\n");
  bad_write = false;
  clear_model(); setup_col(0, C6H); setup_col(1, C7S);
  call_at(S("draw_board"), 0, 0, 0, 0, 0, 0, 0);
  point_at_col0();
  call_at(S("pickup"), 0, 0, 0, 0, 0, 0, 0);
  CHECK(z80.af.b.l & 1, "pickup agarra la carta (CARRY)");
  CHECK(mem[S("dr_moved")] == 0, "pickup deja dr_moved a 0 (sin mover todavia)");
  call_at(S("drop"), 0, 0, 0, 0, 0, 0, 0);
  CHECK(mem[S("tab_n") + 0] == 1, "drop: clic sin arrastrar -> la carta vuelve a su sitio sin deslizarse");
  CHECK(!bad_write, "escenario C: sin escrituras fuera de sitio");
  if (bad_write) printf("        primera escritura fuera de sitio: $%04X\n", bad_addr);

  printf(errors ? "%d ERRORES\n" : "ejecucion OK (falta cmp_solitario.py)\n", errors);
  return errors ? 1 : 0;
}
