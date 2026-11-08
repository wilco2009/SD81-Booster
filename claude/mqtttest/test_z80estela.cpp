// ESTELA (EXAMPLES/ESTELA/estela.asm) en el PC: el Z80 de verdad (nucleo de EightyOne, T-estados exactos) ejecuta el juego con un modelo
// de la memoria paginada (32 paginas de 8 KB, mapeador E7h, bloque 0 = ventana de POKEs), el VSYNC (64.584 T por imagen, 8 lineas a 1,
// como lo midio VSYNC.BIN en la placa), el teclado y el borde, y un modelo del scroll en anillo del modo 175 (mode160_emulador.md §6):
//   pixel x de la fila r de la pantalla = byte $8000 + ((r + vofs) mod hc) * 160 + x   (hofs = 0, wc = 160)
//
// FASE 1. Comprueba:
//   - el tileset generado en $C000 (paginas 6 y 7) y el modelo del mundo de aqui (tiles + mapa + paneles), escritos aparte del Z80
//   - lo visible es EXACTAMENTE el mundo desde la fila cy que habia cuando se escribio POKE 2072, en 500 instantes al azar (tambien
//     mientras se pinta la fila oculta)
//   - nunca se escribe una fila del anillo que se este viendo (las vofs..vofs+99 del ultimo POKE 2072)
//   - POKE 2072 sale pocos T-estados despues del flanco del VSYNC, una vez por imagen, y cada imagen sube 1, 0/1 o 2 filas segun la
//     velocidad (S), con las vueltas al anillo
//   - al salir: modo 85, anillo a 0, color apagado, paginas de los bloques 6 y 7 como estaban, IX
//   - el codigo acaba antes de $7000 y no escribe fuera de su sitio
// NO prueba la FPGA ni la placa.
//
//   ./build_estela.sh
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <map>
#include <random>
#include <algorithm>
extern "C" {
#include "z80.h"
int tstates = 0;
int StackChange = 0, StepOutRequested = 0, RetExecuted = 0;
int z80_fetch_is_m1 = 0;
void z80_m1_backtrack(void) {}
void debug(int) {}
}

static const int FRAME = 64584, VSW = 1656;
static uint8_t sram[32][8192];
static int mapb[8];
static uint64_t total_t = 0;
static long frame() { return (long)(total_t / FRAME); }
static int errors = 0;
#define CHECK(c, msg) do { if (!(c)) { printf("  ERROR: %s\n", msg); errors++; } else printf("  ok    %s\n", msg); } while (0)

static std::map<std::string, int> sym;
static bool load_sym(const char* fn) {
  FILE* f = fopen(fn, "r"); if (!f) return false;
  char name[128], eq[16], val[32];
  while (fscanf(f, "%127s %15s %31s", name, eq, val) == 3) { std::string v = val; if (!v.empty() && (v.back() == 'H' || v.back() == 'h')) v.pop_back(); sym[name] = (int)strtol(v.c_str(), 0, 16); }
  fclose(f); return true;
}
static int S(const char* n) { auto it = sym.find(n); if (it == sym.end()) { printf("falta el simbolo %s\n", n); exit(2); } return it->second; }
static uint8_t rd(int a) { a &= 0xFFFF; return sram[mapb[a >> 13]][a & 8191]; }
static void wr_raw(int a, uint8_t d) { a &= 0xFFFF; sram[mapb[a >> 13]][a & 8191] = d; }
static uint16_t rd16(int a) { return (uint16_t)(rd(a) | (rd(a + 1) << 8)); }

// ---- el mundo, escrito aqui aparte (tiene que coincidir con el del Z80)
static const uint8_t PNL_L[16] = {242,244,246,248,250,252,254,255,253,251,249,247,245,243,241,240};
static const uint8_t PNL_R[16] = {240,241,243,245,247,249,251,253,255,254,252,250,248,246,244,242};
static uint8_t tilepix(int t, int x, int y) {
  if (x == 0 || y == 0) return 0;
  if (x == y) return 255;
  return ((x ^ y) & 8) ? (uint8_t)((t * 7) % 220 + 8) : (uint8_t)((t * 13) % 220 + 12);
}
static int maptile(uint8_t f, int c) { return ((uint8_t)(f * 5 + c * 3) ^ (uint8_t)(f >> 2)) % 60; }
static uint8_t world(uint16_t y, int x) {
  if (x < 16) return PNL_L[x];
  if (x >= 144) return PNL_R[x - 144];
  int tx = x - 16;
  uint8_t f = (uint8_t)(y >> 4);
  return tilepix(maptile(f, tx >> 4), tx & 15, y & 15);
}

// ---- lo que se observa
static int vofs = 0, vring = 0, hofs = 0, hring = 0, mode = -1;
static uint16_t cy_shown = 0; static bool shown_valid = false;
static int cy_sym = 0;
struct VW { long f; uint64_t t; int v; uint16_t cy; };
static std::vector<VW> vwrites;
static std::vector<int> borders;
static bool bad_write = false; static uint16_t bad_addr = 0;
static int code_lo = 0x4800, code_hi = 0x4800;
static long ring_vis_writes = 0; static bool checking_vis = false;
static uint8_t keyrows[8];
static int map_bad = 0;
static int last2063 = 0;

extern "C" {
int  z80_contend_wrapper(int, int, int time) { return time; }
int  z80_contend_io_wrapper(int, int, int time) { return time; }
BYTE z80_readbyte_wrapper(int a) { return rd(a); }
BYTE z80_readoperandbyte_wrapper(int a) { return rd(a); }
BYTE z80_opcode_fetch_wrapper(int a) { z80_fetch_is_m1 = 0; return rd(a); }
void z80_writebyte_wrapper(int a, int d) {
  a &= 0xFFFF;
  if (a < 0x2000) {
    if (a == 2072) { vofs = d; vwrites.push_back({frame(), total_t, d, rd16(cy_sym)}); cy_shown = rd16(cy_sym); shown_valid = true; }
    else if (a == 2073) vring = d;
    else if (a == 2074) hofs = d;
    else if (a == 2075) hring = d;
    else if (a == 2045) mode = d;
    else if (a == 2063) last2063 = d;
    else if (a >= 2038 && a <= 2131) { }
    else if (!bad_write) { bad_write = true; bad_addr = (uint16_t)a; }
    return;
  }
  if (a >= 0x8000 && a < 0x8000 + 102 * 160 && checking_vis && vring == 102) {
    int row = (a - 0x8000) / 160, rel = (row - vofs + 102) % 102;
    if (rel < 100) { if (!ring_vis_writes) printf("        primera escritura en fila visible: $%04X (fila %d, vofs %d) imagen %ld PC=%04X\n", a, row, vofs, frame(), z80.pc.w); ring_vis_writes++; }
  }
  bool ok = (a >= 0x8000 && a < 0x8000 + 102 * 160) || (a >= code_lo && a < code_hi) || (a >= 0x4700 && a < 0x4800) || a >= 0xC000;
  if (!ok && !bad_write) { bad_write = true; bad_addr = (uint16_t)a; printf("        escritura rara en $%04X, imagen %ld, PC=%04X SP=%04X\n", a, frame(), z80.pc.w, z80.sp.w); }
  wr_raw(a, (uint8_t)d);
}
void z80_writeport_wrapper(int port, int d, int*) {
  int lo = port & 0xFF, hi = (port >> 8) & 0xFF;
  if (lo == 0xEF && hi == 0x7F) { int c = d & 0x2F; if (borders.empty() || borders.back() != c) borders.push_back(c); return; }
  if (lo == 0xE7) { int blk = d & 7, pg = (d >> 3) & 31; if (pg != (hi & 31)) map_bad++; mapb[blk] = pg; return; }
  printf("OUT inesperado al puerto %04X\n", port & 0xFFFF); errors++;
}
BYTE z80_readport_wrapper(int port, int*) {
  int lo = port & 0xFF;
  if (lo == 0xAF) return (BYTE)((total_t % FRAME) < (uint64_t)VSW ? 1 : 0);
  if (lo == 0xE7) return (BYTE)mapb[(port >> 8) & 7];
  if (lo == 0xFE) {
    uint8_t v = 0x1F;
    for (int r = 0; r < 8; r++) if (!(((port >> 8) >> r) & 1)) v &= (uint8_t)~keyrows[r];
    return (uint8_t)(v | 0xE0);
  }
  return 0xFF;
}
}

static const int RET_AT = 0x4700;
struct Key { long from, to; int row, bit; };

int main(int argc, char** argv) {
  if (argc < 3) { printf("uso: test_z80estela estela.bin estela.sym\n"); return 2; }
  if (!load_sym(argv[2])) { printf("no encuentro %s\n", argv[2]); return 2; }
  cy_sym = S("cy");
  int MAIN = S("main"), CODEEND = S("codeend");
  memset(sram, 0, sizeof sram);
  int init[8] = {0, 1, 2, 3, 4, 5, 2, 3};
  memcpy(mapb, init, sizeof mapb);
  sram[0][0] = 0xD3;
  memset(sram[4], 0xEE, 8192); memset(sram[5], 0xEE, 8192); memset(sram[6], 0xEE, 8192); memset(sram[7], 0xEE, 8192);
  FILE* f = fopen(argv[1], "rb"); if (!f) { printf("no encuentro %s\n", argv[1]); return 2; }
  std::vector<uint8_t> b(0x8000); size_t n = fread(b.data(), 1, b.size(), f); fclose(f);
  for (size_t i = 0; i < n; i++) wr_raw((int)(0x4800 + i), b[i]);
  code_hi = 0x4800 + (int)n;
  printf("estela.bin: %zu bytes, $4800-$%04X (codigo y tablas hasta $%04X)\n", n, code_hi - 1, CODEEND - 1);
  CHECK(CODEEND <= 0x7000, "el codigo y sus tablas acaban antes de $7000 (la hoja de sprites)");
  uint8_t snap2[8192], snap3[8192]; memcpy(snap2, sram[2], 8192); memcpy(snap3, sram[3], 8192);
  wr_raw(RET_AT, 0x76);
  z80_init(); z80_reset();
  z80.sp.w = 0x47F0; wr_raw(0x47F0, RET_AT & 255); wr_raw(0x47F1, RET_AT >> 8);
  z80.pc.w = 0x4800; z80.halted = 0; z80.iff1 = z80.iff2 = 0;
  z80.ix.w = 0x0281; z80.iy.w = 0x4000;

  // filas del teclado: 1 = A S D F G (S bit 1), 0 = SHIFT Z X C V (X bit 2)
  std::vector<Key> script = { {300, 304, 1, 1},    // S: 1 cada 2 imagenes
                              {500, 504, 1, 1},    // S: 2 por imagen
                              {700, 704, 1, 1},    // S: parado
                              {760, 764, 1, 1},    // S: 1 por imagen otra vez
                              {900, 904, 0, 2} };  // X
  std::mt19937 rng(12345);
  std::vector<uint64_t> checks;
  for (int i = 0; i < 500; i++) checks.push_back((uint64_t)(rng() % (uint64_t)(880LL * FRAME)) + 20ULL * FRAME);
  std::sort(checks.begin(), checks.end());
  size_t ci = 0; int vis_ok = 0, vis_bad = 0, vis_skip = 0;
  bool in_main = false; long first_main_frame = -1;
  long guard = 0;
  while (z80.pc.w != RET_AT) {
    long fr = frame();
    memset(keyrows, 0, sizeof keyrows);
    for (const Key& k : script) if (fr >= k.from && fr < k.to) keyrows[k.row] |= (uint8_t)(1 << k.bit);
    if (z80.pc.w == MAIN && !in_main) { in_main = true; first_main_frame = fr; checking_vis = true; shown_valid = false; }
    z80_do_opcode(); total_t += (uint64_t)tstates;
    if (++guard > 400000000L) { printf("ERROR: no termina (PC=%04X)\n", z80.pc.w); return 1; }
    while (ci < checks.size() && total_t >= checks[ci]) {
      if (!checking_vis || !shown_valid || mode != 175 || vring != 102 || hofs != 0 || hring != 0) { vis_skip++; ci++; continue; }
      int bad = 0;
      for (int r = 0; r < 100 && bad < 3; r++)
        for (int x = 0; x < 160; x++) {
          uint8_t got = rd(0x8000 + ((r + vofs) % 102) * 160 + x), want = world((uint16_t)(cy_shown + r), x);
          if (got != want) { if (bad < 3) printf("        instante %zu (imagen %ld): fila %d x %d = %d, mundo %d (vofs %d, cy %u)\n", ci, frame(), r, x, got, want, vofs, cy_shown); bad++; break; }
        }
      if (bad) vis_bad++; else vis_ok++;
      ci++;
    }
    if (fr > 1000) { printf("  ERROR: no sale con X (PC=%04X)\n", z80.pc.w); errors++; break; }
  }
  printf("        bucle principal desde la imagen %ld; %d instantes comprobados, %d antes de empezar\n", first_main_frame, vis_ok + vis_bad, vis_skip);
  CHECK(z80.pc.w == RET_AT, "vuelve con X");
  // el tileset de $C000 con el modelo (paginas 6 y 7)
  int tbad = 0;
  for (int y = 0; y < 96; y++) for (int x = 0; x < 160; x++) {
    int a = y * 160 + x; uint8_t got = a < 8192 ? sram[6][a] : sram[7][a - 8192];
    if (got != tilepix((y / 16) * 10 + x / 16, x & 15, y & 15)) tbad++;
  }
  CHECK(tbad == 0, "el tileset de $C000 (paginas 6 y 7) es el del modelo");
  CHECK(vis_ok >= 450 && vis_bad == 0, "lo visible es exactamente el mundo desde la fila cy del POKE 2072, en todos los instantes");
  CHECK(ring_vis_writes == 0, "nunca escribe en una fila del anillo que se esta viendo");
  // POKE 2072: una vez por imagen, poco despues del flanco; pasos segun la velocidad
  long maxlat = 0; int dup = 0, badstep = 0; long prevf = -1;
  std::map<int, int> steps_by_phase[5];
  if (!vwrites.empty() && vwrites.back().v == 0 && mode == 85) vwrites.pop_back();      // el POKE 2072,0 de la salida
  for (size_t i = 0; i < vwrites.size(); i++) {
    const VW& w = vwrites[i];
    if (w.f < first_main_frame) continue;
    long lat = (long)(w.t % FRAME); if (lat > maxlat) maxlat = lat;
    if (w.f == prevf) dup++;
    prevf = w.f;
    if (i > 0 && vwrites[i - 1].f >= first_main_frame) {
      int dv = (vwrites[i - 1].v - w.v + 102) % 102, dc = (uint16_t)(vwrites[i - 1].cy - w.cy);
      if (dv != dc || w.v < 0 || w.v >= 102) badstep++;
      int ph = w.f < 302 ? 0 : w.f < 502 ? 1 : w.f < 702 ? 2 : w.f < 762 ? 3 : 4;
      steps_by_phase[ph][dc]++;
    }
  }
  printf("        POKE 2072 como mucho %ld T despues del flanco (D0 medido en la placa: ~12.600)\n", maxlat);
  CHECK(maxlat < 2000 && dup == 0, "POKE 2072 una vez por imagen, en los primeros 2.000 T tras el flanco");
  CHECK(badstep == 0, "vofs y cy bajan a la vez, con la vuelta del anillo (mod 102)");
  auto only = [&](int ph, std::vector<int> ks) { for (auto& kv : steps_by_phase[ph]) { bool in = false; for (int k : ks) if (kv.first == k) in = true; if (!in) return false; } return true; };
  printf("        pasos por imagen: v1 {1:%d}  v1/2 {0:%d 1:%d}  v2 {2:%d}  parado {0:%d}  v1 {1:%d}\n", steps_by_phase[0][1], steps_by_phase[1][0], steps_by_phase[1][1],
         steps_by_phase[2][2], steps_by_phase[3][0], steps_by_phase[4][1]);
  CHECK(only(0, {1}) && only(1, {0, 1}) && only(2, {2}) && only(3, {0}) && only(4, {1}) && steps_by_phase[1][0] > 80 && steps_by_phase[1][1] > 80,
        "S: 1 fila por imagen, 1 cada 2, 2 por imagen, parado y otra vez 1");
  long total_up = (uint16_t)(0 - vwrites.back().cy);
  printf("        filas subidas en total: %ld (%ld vueltas al anillo)\n", total_up, total_up / 102);
  CHECK(borders.size() >= 6 && borders[1] == 0x24 && borders[2] == 0x25 && borders[3] == 0x26 && borders[4] == 0x22 && borders[5] == 0x24,
        "borde: verde, cyan, amarillo, rojo y verde segun la velocidad");
  CHECK(mode == 85 && vofs == 0 && vring == 0 && hofs == 0 && hring == 0, "al salir: modo 85 y el anillo a 0");
  CHECK(!borders.empty() && borders.back() == 0x0C, "al salir: $7FEF = $0C (color apagado)");
  CHECK(!memcmp(mapb, init, sizeof mapb) && map_bad == 0, "al salir: las paginas de los bloques 6 y 7 como estaban");
  CHECK(!memcmp(sram[3], snap3, 8192), "las paginas 2 y 3 (el BASIC, $6000-$7FFF) no se han tocado al copiar a $C000");
  CHECK(z80.ix.w == 0x0281, "IX como estaba (la ROM lo usa en SLOW)");
  CHECK(!bad_write, "no escribe fuera del codigo, la pila, el anillo, $C000-$FFFF y la ventana de POKEs");
  if (bad_write) printf("        primera escritura rara en $%04X\n", bad_addr);
  printf("%s (%d errores)\n", errors ? "FALLOS" : "TODO OK", errors);
  return errors ? 1 : 0;
}
