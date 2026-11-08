// ESTELA (EXAMPLES/ESTELA/estela.asm) en el PC: el Z80 de verdad (nucleo de EightyOne, T-estados exactos) ejecuta el juego con un modelo
// de la memoria paginada (32 paginas de 8 KB, mapeador E7h, bloque 0 = ventana de POKEs), el VSYNC (64.584 T por imagen, 8 lineas a 1,
// como lo midio VSYNC.BIN en la placa), el teclado, el borde, la paleta de 256 (2063-2065), un MCU con las ordenes 53/55/57 (ficheros de
// la SD, servidos desde EXAMPLES/ESTELA/out) y un modelo del scroll en anillo del modo 175 (mode160_emulador.md §6):
//   pixel x de la fila r de la pantalla = byte $8000 + ((r + vofs) mod hc) * 160 + x   (hofs = 0, wc = 160)
//
// FASE 3 (sobre la 2): la nave y sus disparos en sprites de formato B. Un modelo de aqui (Game, escrito con la especificacion de
//   estela.asm, no con su codigo) mueve la nave con las teclas de cada imagen y se compara en CADA imagen con los registros de los
//   sprites que ha dejado el Z80 en la ventana de POKEs (2100-2131): posicion, pagina (inclinacion, llama, disparos), mascara,
//   transparente y encendido; la hoja en $7000 = HOJA.BIN, PAL16 = HOJA.PAL y activa, los sprites escritos en el borrado.
//
// FASE 2. El fondo es el del nivel: el modelo de aqui lee NIVEL1.I16 y NIVEL1.MAP por su cuenta (formatos de juego160.md §8) y
//   la fila k de la pantalla con s pixeles de scroll = la linea 99+s-k del nivel (fila del mapa L/16, linea del tile 15 - L mod 16),
//   con los paneles de la fila 96 del tileset. Comprueba:
//   - el tileset de $C000 (paginas 6 y 7) es el del .I16, y el nivel esta en LVLBUF tal cual
//   - lo visible es EXACTAMENTE el nivel desde la s que habia cuando se escribio POKE 2072, en 500 instantes al azar
//   - nunca se escribe una fila del anillo que se este viendo (salvo al volver al principio con R, con la paleta en negro)
//   - POKE 2072 pocos T despues del flanco, una vez por imagen; pasos segun la velocidad (la del nivel al empezar y luego S)
//   - la paleta: al principio la del .I16; en cada imagen, el agua, la lava y las luces giradas lo que toca y el resto intacto; los
//     giros se escriben en el borrado
//   - un nivel corto: el scroll se para en LVL_END y R vuelve al principio
//   - errores de carga (falta un fichero, magia mala, no cabe, cabecera incoherente): vuelve con el codigo en BC y todo restaurado
//   - al salir: modo 85, anillo a 0, color apagado, paginas de los bloques 6 y 7 como estaban, IX; no escribe fuera de su sitio
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
static std::vector<uint8_t> readfile(const std::string& fn) {
  std::vector<uint8_t> v; FILE* f = fopen(fn.c_str(), "rb"); if (!f) return v;
  int c; while ((c = fgetc(f)) != EOF) v.push_back((uint8_t)c); fclose(f); return v;
}
static int u16(const std::vector<uint8_t>& v, int o) { return v[o] | (v[o + 1] << 8); }

// ---- el nivel, leido aqui aparte (tiene que coincidir con lo que pinta el Z80)
struct Level {
  std::vector<uint8_t> i16, map;
  int rows = 0, mapoff = 0, end = 0;
  uint8_t pix(int a) const { return i16[4 + 512 + a]; }
  uint8_t at(int L, int x) const {                  // pixel x (0-159) de la linea L del nivel
    if (x < 16 || x >= 144) return pix(96 * 160 + x);
    int r = L >> 4, j = 15 - (L & 15), tx = x - 16;
    int t = map[mapoff + r * 8 + (tx >> 4)];
    return pix((t / 10) * 2560 + (t % 10) * 16 + j * 160 + (tx & 15));
  }
  uint16_t pal(int i) const { return (uint16_t)(i16[4 + 2 * i] | (i16[5 + 2 * i] << 8)); }
};

// ---- MCU: ordenes 53 (fopen), 55 (fread), 57 (fclose)
static std::map<std::string, std::vector<uint8_t>> sdfiles;
static int mcu_clk = 0;
static std::vector<uint8_t> mcu_in, mcu_out; static size_t mcu_outpos = 0;
static const std::vector<uint8_t>* openf = nullptr; static size_t fpos = 0;
static std::vector<std::string> opened;
static int fopen_open = 0;
static void mcu_byte(uint8_t b) {
  mcu_in.push_back(b);
  int c = mcu_in[0];
  if (c == 53) {
    if (mcu_in.size() >= 2 && (int)mcu_in.size() == 2 + mcu_in[1]) {
      std::string nm(mcu_in.begin() + 2, mcu_in.end()); opened.push_back(nm);
      auto it = sdfiles.find(nm);
      if (it == sdfiles.end() || openf) mcu_out.push_back(0xFF); else { openf = &it->second; fpos = 0; fopen_open++; mcu_out.push_back(0); }
      mcu_in.clear();
    }
  } else if (c == 55) {
    if (mcu_in.size() == 4) {
      int n = mcu_in[2] | (mcu_in[3] << 8);
      if (!openf || mcu_in[1] != 0) { printf("        fread sin fichero abierto\n"); errors++; }
      for (int i = 0; i < n; i++) mcu_out.push_back(openf && fpos < openf->size() ? (*openf)[fpos++] : 0);
      mcu_out.push_back(0); mcu_in.clear();
    }
  } else if (c == 57) {
    if (mcu_in.size() == 2) { if (openf) fopen_open--; openf = nullptr; mcu_out.push_back(0); mcu_in.clear(); }
  } else { printf("orden del MCU inesperada: %d\n", c); errors++; mcu_in.clear(); }
}

// ---- lo que se observa
static int vofs = 0, vring = 0, hofs = 0, hring = 0, mode = -1;
static int s_shown = 0; static bool shown_valid = false;
static int scr_sym = 0;
struct VW { long f; uint64_t t; int v; int s; bool restart; };
static std::vector<VW> vwrites;
static std::vector<int> borders;
static bool bad_write = false; static uint16_t bad_addr = 0;
static int code_hi = 0x4800, buf_lo = 0, buf_hi = 0;
static long ring_vis_writes = 0; static bool checking_vis = false, in_restart = false, restart_seen = false;
static uint8_t keyrows[8];
static int map_bad = 0;
static uint16_t pal[256]; static int palptr = 0, pallo = 0;
static int cyc_lo = 0, cyc_hi = 0; static long cyc_maxlat = 0, cyc_writes = 0;
// sprites: los registros de la ventana de POKEs
struct Spr { int en, xl, xh, y, page; uint8_t mask[8]; };
static Spr spr[64]; static int spr_sel = 0, spr_blk = 2, spr_xtr = 0, pal16_on = 0;
static uint16_t pal16[16];
static int draw_lo = 0, draw_hi = 0; static long spr_maxlat = 0;

extern "C" {
int  z80_contend_wrapper(int, int, int time) { return time; }
int  z80_contend_io_wrapper(int, int, int time) { return time; }
BYTE z80_readbyte_wrapper(int a) { return rd(a); }
BYTE z80_readoperandbyte_wrapper(int a) { return rd(a); }
BYTE z80_opcode_fetch_wrapper(int a) { z80_fetch_is_m1 = 0; return rd(a); }
void z80_writebyte_wrapper(int a, int d) {
  a &= 0xFFFF;
  if (a < 0x2000) {
    if (a == 2072) {                                  // solo cuentan los del bucle principal (no el POKE 2072,0 del arranque)
      vofs = d; int s = rd16(scr_sym);
      if (checking_vis) { vwrites.push_back({frame(), total_t, d, s, restart_seen}); restart_seen = false; s_shown = s; shown_valid = true; in_restart = false; }
    }
    else if (a == 2073) vring = d;
    else if (a == 2074) hofs = d;
    else if (a == 2075) hring = d;
    else if (a == 2045) mode = d;
    else if (a == 2063) palptr = d;
    else if (a == 2064) pallo = d;
    else if (a == 2065) {
      pal[palptr] = (uint16_t)(pallo | (d << 8)); palptr = (palptr + 1) & 255;
      if (z80.pc.w >= cyc_lo && z80.pc.w < cyc_hi) { long lat = (long)(total_t % FRAME); if (lat > cyc_maxlat) cyc_maxlat = lat; cyc_writes++; }
    }
    else if (a == 2066) { pal16[palptr & 15] = (uint16_t)(pallo | (d << 8)); palptr = (palptr + 1) & 255; }
    else if (a == 2067) pal16_on = d;
    else if (a >= 2100 && a <= 2131) {
      if (z80.pc.w >= draw_lo && z80.pc.w < draw_hi) { long lat = (long)(total_t % FRAME); if (lat > spr_maxlat) spr_maxlat = lat; }
      Spr& q = spr[spr_sel & 63];
      if (a == 2100) spr_sel = d; else if (a == 2101) q.en = d; else if (a == 2102) q.xl = d; else if (a == 2103) q.xh = d;
      else if (a == 2104) q.y = d; else if (a >= 2121 && a <= 2128) q.mask[a - 2121] = (uint8_t)d; else if (a == 2129) spr_blk = d;
      else if (a == 2130) q.page = d; else if (a == 2131) spr_xtr = d;
    }
    else if (a >= 2038 && a <= 2131) { }
    else if (!bad_write) { bad_write = true; bad_addr = (uint16_t)a; }
    return;
  }
  if (a >= 0x8000 && a < 0x8000 + 102 * 160 && checking_vis && !in_restart && vring == 102) {
    int row = (a - 0x8000) / 160, rel = (row - vofs + 102) % 102;
    if (rel < 100) { if (!ring_vis_writes) printf("        primera escritura en fila visible: $%04X (fila %d, vofs %d) imagen %ld PC=%04X\n", a, row, vofs, frame(), z80.pc.w); ring_vis_writes++; }
  }
  bool ok = (a >= 0x8000 && a < 0x8000 + 102 * 160) || (a >= 0x4800 && a < code_hi) || (a >= buf_lo && a < buf_hi) || (a >= 0x4700 && a < 0x4800) || a >= 0xC000;
  if (!ok && !bad_write) { bad_write = true; bad_addr = (uint16_t)a; printf("        escritura rara en $%04X, imagen %ld, PC=%04X SP=%04X\n", a, frame(), z80.pc.w, z80.sp.w); }
  wr_raw(a, (uint8_t)d);
}
void z80_writeport_wrapper(int port, int d, int*) {
  int lo = port & 0xFF, hi = (port >> 8) & 0xFF;
  if (lo == 0xEF && hi == 0x7F) { int c = d & 0x2F; if (borders.empty() || borders.back() != c) borders.push_back(c); return; }
  if (lo == 0xE7) { int blk = d & 7, pg = (d >> 3) & 31; if (pg != (hi & 31)) map_bad++; mapb[blk] = pg; return; }
  if (lo == 0xA7) { mcu_byte((uint8_t)d); mcu_clk ^= 1; return; }
  printf("OUT inesperado al puerto %04X\n", port & 0xFFFF); errors++;
}
BYTE z80_readport_wrapper(int port, int*) {
  int lo = port & 0xFF;
  if (lo == 0xAF) return (BYTE)(((total_t % FRAME) < (uint64_t)VSW ? 1 : 0) | (mcu_clk << 7));
  if (lo == 0xA7) {
    uint8_t v = mcu_outpos < mcu_out.size() ? mcu_out[mcu_outpos++] : 0;
    if (mcu_outpos >= mcu_out.size()) { mcu_out.clear(); mcu_outpos = 0; }
    mcu_clk ^= 1; return v;
  }
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
static std::vector<uint8_t> bin;
static int init[8] = {0, 1, 2, 3, 4, 5, 2, 3};
static uint8_t basic_snap[0x8000];

struct Result { int bc; long frames; int vis_ok, vis_bad; long maxlat, maxloop; std::vector<int> pal_bad; std::vector<int> spr_bad; int fired, expired, spr_checked, maxb, minx, maxx, miny, maxy; int blk_bad, p16_bad; };

// ---- la nave, escrita aqui con la especificacion (cabecera de estela.asm y juego160.md): x 16-128, y 0-76, 1 px por imagen con
// O P Q A; inclinacion = dx + 1; disparos de 8 huecos, suben 3 px por imagen, se borran con y < -7; con ESPACIO y la espera a 0
// sale uno en (x + 4, y - 6) en el primer hueco y la espera pasa a 6; la espera baja 1 por imagen y se puede disparar en la misma
// imagen en que llega a 0. tick sube 1 por imagen; la llama cambia de fase cada 4.
struct Game { int x, y, tilt, cd, tick; int ba[8], bx[8], by[8]; };
static void ginit(Game& g) { g.x = 72; g.y = 76; g.tilt = 1; g.cd = 0; g.tick = 0; for (int i = 0; i < 8; i++) g.ba[i] = 0; }
static bool keydown(const std::vector<Key>& sc, long f, int row, int bit) { for (auto& k : sc) if (f >= k.from && f < k.to && k.row == row && k.bit == bit) return true; return false; }
static void gupdate(Game& g, const std::vector<Key>& sc, long f, Result& R) {
  g.tick = (g.tick + 1) & 255;
  int dx = (keydown(sc, f, 5, 0) ? 1 : 0) - (keydown(sc, f, 5, 1) ? 1 : 0);
  g.tilt = dx + 1; g.x = std::min(128, std::max(16, g.x + dx));
  int dy = (keydown(sc, f, 1, 0) ? 1 : 0) - (keydown(sc, f, 2, 0) ? 1 : 0);
  g.y = std::min(76, std::max(0, g.y + dy));
  for (int i = 0; i < 8; i++) if (g.ba[i]) { g.by[i] -= 3; if (g.by[i] < -7) { g.ba[i] = 0; R.expired++; } }
  if (g.cd > 0) { g.cd--; if (g.cd > 0) return; }
  if (!keydown(sc, f, 7, 0)) return;
  for (int i = 0; i < 8; i++) if (!g.ba[i]) { g.ba[i] = 1; g.bx[i] = g.x + 4; g.by[i] = g.y - 6; g.cd = 6; R.fired++; return; }
}
static int spr_check(const Game& g) {
  int bad = 0, NAVE = S("NAVE"), LLAMA = S("LLAMA"), BALA = S("BALAJ_0");
  bool used[64] = {};
  auto chk = [&](int n, int x, int y, int pg) {
    used[n] = true; const Spr& q = spr[n]; int X = q.xl | ((q.xh & 1) << 8); bool m = true;
    for (int r = 0; r < 8; r++) if (q.mask[r] != 0xFF) m = false;
    if (!(q.en == 1 && X == x && q.y == (y & 255) && q.page == pg && m && (q.xh >> 4) == 0)) bad++;
  };
  int pg = NAVE + g.tilt * 4;
  chk(50, g.x + 32, g.y + 32, pg); chk(51, g.x + 40, g.y + 32, pg + 1); chk(52, g.x + 32, g.y + 40, pg + 2); chk(53, g.x + 40, g.y + 40, pg + 3);
  chk(49, g.x + 36, g.y + 48, LLAMA + ((g.tick >> 2) & 1));
  for (int i = 0; i < 8; i++) if (g.ba[i]) chk(34 + i, g.bx[i] + 32, g.by[i] + 32, BALA);
  for (int n = 0; n < 64; n++) if (!used[n] && spr[n].en) bad++;
  return bad;
}

// una ejecucion: hasta que vuelve o hasta maxf imagenes
static Result run(const Level& lv, const std::vector<Key>& script, long maxf, int nchecks, bool check_pal) {
  Result R{}; R.bc = -1;
  memset(sram, 0, sizeof sram); memcpy(mapb, init, sizeof mapb);
  sram[0][0] = 0xD3;
  for (int p = 4; p < 8; p++) memset(sram[p], 0xEE, 8192);
  for (int a = 0x4000; a < 0x8000; a++) wr_raw(a, (uint8_t)(a * 7 + (a >> 8)));     // "BASIC" de relleno, para ver si se toca
  for (size_t i = 0; i < bin.size(); i++) wr_raw((int)(0x4800 + i), bin[i]);
  for (int a = 0x4000; a < 0x8000; a++) basic_snap[a - 0x4000] = rd(a);
  wr_raw(RET_AT, 0x76);
  total_t = 0; vofs = vring = hofs = hring = 0; mode = -1; shown_valid = false; s_shown = 0;
  vwrites.clear(); borders.clear(); bad_write = false; ring_vis_writes = 0; checking_vis = false; in_restart = false; restart_seen = false;
  map_bad = 0; memset(pal, 0, sizeof pal); palptr = pallo = 0; cyc_maxlat = 0; cyc_writes = 0;
  memset(spr, 0, sizeof spr); spr_sel = 0; spr_blk = 2; spr_xtr = 0; pal16_on = 0; memset(pal16, 0, sizeof pal16); spr_maxlat = 0;
  Game g; ginit(g); size_t nspr = 0; bool prev_r = false;
  R.minx = R.miny = 999; R.maxx = R.maxy = -999;
  mcu_in.clear(); mcu_out.clear(); mcu_outpos = 0; mcu_clk = 0; openf = nullptr; opened.clear(); fopen_open = 0;
  z80_init(); z80_reset();
  z80.sp.w = 0x47F0; wr_raw(0x47F0, RET_AT & 255); wr_raw(0x47F1, RET_AT >> 8);
  z80.pc.w = 0x4800; z80.halted = 0; z80.iff1 = z80.iff2 = 0;
  z80.ix.w = 0x0281; z80.iy.w = 0x4000;
  int MAIN = S("main"), RESTART = S("restart"), WAITVS = S("waitvs");
  int TW = S("TCYW"), TL = S("TCYL"), TG = S("TCYG");
  struct Rng { int base, n, t; } rngs[3] = { {S("PAL_WATER"), S("PAL_WATER_N"), TW}, {S("PAL_LAVA"), S("PAL_LAVA_N"), TL}, {S("PAL_LIGHTS"), S("PAL_LIGHTS_N"), TG} };
  std::mt19937 rng(12345);
  std::vector<uint64_t> checks;
  for (int i = 0; i < nchecks; i++) checks.push_back((uint64_t)(rng() % (uint64_t)((maxf - 30) * FRAME)) + 20ULL * FRAME);
  std::sort(checks.begin(), checks.end());
  size_t ci = 0; bool in_main = false; long guard = 0; long calls = 0; uint64_t frame_t0 = 0; size_t nvw = 0;
  while (z80.pc.w != RET_AT) {
    long fr = frame();
    memset(keyrows, 0, sizeof keyrows);
    for (const Key& k : script) if (fr >= k.from && fr < k.to) keyrows[k.row] |= (uint8_t)(1 << k.bit);
    if (z80.pc.w == MAIN && !in_main) { in_main = true; checking_vis = true; shown_valid = false; calls = 0; }
    if (z80.pc.w == RESTART) { in_restart = true; restart_seen = true; calls = -1; }
    if (in_main && z80.pc.w == WAITVS && frame_t0 && !restart_seen) { long lp = (long)(total_t - frame_t0); if (lp > R.maxloop) R.maxloop = lp; }
    if (in_main && z80.pc.w == WAITVS && vwrites.size() > nspr) {
      // acaba una vuelta: lo que se ha visto en esta imagen es el estado de antes de update; luego update con las teclas de la imagen
      nspr = vwrites.size(); long f = vwrites.back().f;
      if (spr_check(g)) R.spr_bad.push_back((int)f);
      if (spr_blk != S("SHEET_BLOCK") || spr_xtr != 0) R.blk_bad++;
      if (pal16_on != 1) R.p16_bad++;
      R.spr_checked++;
      int nb = 0; for (int i = 0; i < 8; i++) nb += g.ba[i]; R.maxb = std::max(R.maxb, nb);
      R.minx = std::min(R.minx, g.x); R.maxx = std::max(R.maxx, g.x); R.miny = std::min(R.miny, g.y); R.maxy = std::max(R.maxy, g.y);
      gupdate(g, script, f, R);
      bool r = keydown(script, f, 2, 3);
      if (r && !prev_r) ginit(g);
      prev_r = r;
    }
    z80_do_opcode(); total_t += (uint64_t)tstates;
    if (vwrites.size() > nvw) {                       // acaba de escribir POKE 2072: la paleta tiene que estar girada lo que toca
      nvw = vwrites.size(); frame_t0 = total_t - total_t % FRAME;
      if (in_main && check_pal) {
        if (calls < 0) calls = 0;
        int bad = 0;
        for (int i = 0; i < 256; i++) {
          uint16_t want = lv.pal(i);
          for (auto& g : rngs) if (i >= g.base && i < g.base + g.n) { int o = (int)((calls / g.t) % g.n); want = lv.pal(g.base + ((i - g.base) + o) % g.n); }
          if (pal[i] != want) bad++;
        }
        if (bad) R.pal_bad.push_back((int)fr);
        calls++;
      }
    }
    if (++guard > 600000000L) { printf("ERROR: no termina (PC=%04X)\n", z80.pc.w); errors++; break; }
    while (ci < checks.size() && total_t >= checks[ci]) {
      if (!checking_vis || in_restart || !shown_valid || mode != 175 || vring != 102 || hofs != 0 || hring != 0) { ci++; continue; }
      int bad = 0;
      for (int r = 0; r < 100 && bad < 3; r++)
        for (int x = 0; x < 160; x++) {
          uint8_t got = rd(0x8000 + ((r + vofs) % 102) * 160 + x), want = lv.at(99 + s_shown - r, x);
          if (got != want) { printf("        instante %zu (imagen %ld): fila %d x %d = %d, nivel %d (vofs %d, s %d)\n", ci, frame(), r, x, got, want, vofs, s_shown); bad++; break; }
        }
      if (bad) R.vis_bad++; else R.vis_ok++;
      ci++;
    }
    if (fr > maxf) { printf("  ERROR: no sale (PC=%04X)\n", z80.pc.w); errors++; break; }
  }
  R.frames = frame();
  if (z80.pc.w == RET_AT) R.bc = z80.bc.w;
  if (!vwrites.empty() && vwrites.back().v == 0 && mode == 85) vwrites.pop_back();        // el POKE 2072,0 de la salida
  for (auto& w : vwrites) { long lat = (long)(w.t % FRAME); if (lat > R.maxlat) R.maxlat = lat; }
  return R;
}

static void exit_checks(const char* what, int want_bc) {
  {
    int on = 0; for (int n = 0; n < 64; n++) if (spr[n].en || spr[n].page) on++;
    char m0[200]; snprintf(m0, sizeof m0, "%s: los 64 sprites apagados y en formato A, sin extra, hoja en el bloque 2, paleta de 16 desactivada", what);
    CHECK(on == 0 && spr_xtr == 0 && spr_blk == 2 && pal16_on == 0, m0);
  }
  char m[200];
  snprintf(m, sizeof m, "%s: vuelve con BC = %d", what, want_bc); CHECK(z80.pc.w == RET_AT && z80.bc.w == want_bc, m);
  if (z80.pc.w == RET_AT && z80.bc.w != want_bc) printf("        BC = %d\n", z80.bc.w);
  snprintf(m, sizeof m, "%s: modo 85, anillo a 0, $7FEF = $0C, paginas de los bloques 6 y 7 e IX como estaban", what);
  CHECK(mode == 85 && vofs == 0 && vring == 0 && hofs == 0 && hring == 0 && !borders.empty() && borders.back() == 0x0C &&
        !memcmp(mapb, init, sizeof mapb) && map_bad == 0 && z80.ix.w == 0x0281, m);
  snprintf(m, sizeof m, "%s: ningun fichero abierto y nada escrito fuera de su sitio", what);
  CHECK(fopen_open == 0 && !bad_write, m);
  if (bad_write) printf("        primera escritura rara en $%04X\n", bad_addr);
}

int main(int argc, char** argv) {
  if (argc < 4) { printf("uso: test_z80estela estela.bin estela.sym carpeta_out\n"); return 2; }
  if (!load_sym(argv[2])) { printf("no encuentro %s\n", argv[2]); return 2; }
  bin = readfile(argv[1]); if (bin.empty()) { printf("no encuentro %s\n", argv[1]); return 2; }
  std::string out = argv[3];
  Level lv; lv.i16 = readfile(out + "/NIVEL1.I16"); lv.map = readfile(out + "/NIVEL1.MAP");
  std::vector<uint8_t> hoja = readfile(out + "/HOJA.BIN"), hpal = readfile(out + "/HOJA.PAL");
  if (hoja.empty() || hpal.size() != 32) { printf("faltan HOJA.BIN / HOJA.PAL en %s\n", out.c_str()); return 2; }
  draw_lo = S("drawspr"); draw_hi = S("update");
  if (lv.i16.size() != 16516 || lv.map.size() < 16) { printf("faltan NIVEL1.I16 / NIVEL1.MAP en %s\n", out.c_str()); return 2; }
  lv.rows = u16(lv.map, 6); lv.mapoff = u16(lv.map, 10); lv.end = u16(lv.map, 14);
  scr_sym = S("scr");
  int CODEEND = S("codeend"), PALBUF = S("PALBUF"), LVLBUF = S("LVLBUF");
  code_hi = 0x4800 + (int)bin.size(); buf_lo = PALBUF; buf_hi = S("SHEET_END");     // PALBUF, el nivel y la hoja (hasta $7000 y $7000-SHEET_END)
  cyc_lo = S("cycle"); cyc_hi = S("keys");
  printf("estela.bin: %zu bytes, $4800-$%04X; PALBUF $%04X, LVLBUF $%04X (hasta $6FFF: %d bytes de nivel)\n", bin.size(), code_hi - 1, PALBUF, LVLBUF, 0x7000 - LVLBUF);
  printf("NIVEL1.MAP: %zu bytes, %d filas, mapa en +%d, LVL_END %d, LVL_SCROLL %d\n", lv.map.size(), lv.rows, lv.mapoff, lv.end, lv.map[4]);
  CHECK(CODEEND <= 0x7000 && LVLBUF + (int)lv.map.size() <= 0x7000, "el codigo, PALBUF y el nivel acaban antes de $7000 (la hoja de sprites)");

  // ===== 1. el nivel 1, con las cuatro velocidades y R
  printf("-- nivel 1 --\n");
  sdfiles.clear(); sdfiles["NIVEL1.I16"] = lv.i16; sdfiles["NIVEL1.MAP"] = lv.map; sdfiles["HOJA.BIN"] = hoja;
  // filas del teclado: 0 SHIFT Z X C V, 1 A S D F G, 2 Q W E R T, 5 P O I U Y, 7 ESPACIO . M N B
  std::vector<Key> script = { {30, 60, 5, 0},      // P: derecha
                              {60, 170, 5, 1},     // O: izquierda hasta el borde
                              {100, 250, 7, 0},    // ESPACIO
                              {170, 260, 2, 0},    // Q: arriba hasta el borde
                              {270, 290, 1, 0},    // A: abajo
                              {320, 340, 5, 0}, {320, 340, 2, 0},     // P y Q a la vez
                              {400, 600, 7, 0},    // ESPACIO mientras cambia la velocidad
                              {450, 560, 5, 0},    // P hasta el borde
                              {610, 620, 5, 0}, {610, 620, 5, 1}, {610, 640, 7, 0},   // O y P a la vez: recta
                              {640, 760, 1, 0},    // A hasta el borde
                              {300, 304, 1, 1},    // S: 1 cada 2 imagenes
                              {500, 504, 1, 1},    // S: 2 por imagen
                              {700, 704, 1, 1},    // S: parado
                              {760, 764, 1, 1},    // S: 1 por imagen otra vez
                              {850, 854, 2, 3},    // R: al principio
                              {960, 964, 0, 2} };  // X
  Result R = run(lv, script, 1000, 500, true);
  printf("        %d instantes comprobados; la vuelta mas larga del bucle: %ld T (de %d)\n", R.vis_ok + R.vis_bad, R.maxloop, FRAME);
  CHECK(opened.size() == 3 && opened[0] == "NIVEL1.I16" && opened[1] == "NIVEL1.MAP" && opened[2] == "HOJA.BIN", "abre NIVEL1.I16, NIVEL1.MAP y HOJA.BIN (y los cierra)");
  int tbad = 0;
  for (int a = 0; a < 16000; a++) { uint8_t got = a < 8192 ? sram[6][a] : sram[7][a - 8192]; if (got != lv.pix(a)) tbad++; }
  CHECK(tbad == 0, "el tileset de $C000 (paginas 6 y 7) es el de NIVEL1.I16");
  int lbad = 0;
  for (size_t i = 0; i < lv.map.size(); i++) if (rd(LVLBUF + (int)i) != lv.map[i]) lbad++;
  CHECK(lbad == 0, "NIVEL1.MAP esta entero en LVLBUF");
  CHECK(R.vis_ok >= 400 && R.vis_bad == 0, "lo visible es exactamente el nivel desde la s del POKE 2072, en todos los instantes");
  {
    int hbad = 0; for (size_t i = 0; i < hoja.size(); i++) if (rd(S("SHEET_ADDR") + (int)i) != hoja[i]) hbad++;
    CHECK(hbad == 0 && S("SHEET_ADDR") + (int)hoja.size() <= 0x8000, "la hoja de $7000 es HOJA.BIN");
  }
  CHECK(ring_vis_writes == 0, "nunca escribe en una fila del anillo que se esta viendo (fuera de R)");
  printf("        POKE 2072 como mucho %ld T despues del flanco; los giros de la paleta, hasta %ld T (%ld entradas)\n", R.maxlat, cyc_maxlat, cyc_writes);
  CHECK(R.maxlat < 2000, "POKE 2072 en los primeros 2.000 T tras el flanco");
  CHECK(cyc_writes > 0 && cyc_maxlat < 10000, "el ciclo de la paleta se escribe en el borrado (antes de los ~12.600 T de la primera linea)");
  if (!R.pal_bad.empty()) printf("        paleta mal en %zu imagenes, la primera %d\n", R.pal_bad.size(), R.pal_bad[0]);
  CHECK(R.pal_bad.empty(), "la paleta en cada imagen: la del .I16 con el agua, la lava y las luces giradas lo que toca");
  {
    int dup = 0, badstep = 0, resets = 0; long prevf = -1;
    std::map<int, int> ph[6];
    for (size_t i = 0; i < vwrites.size(); i++) {
      const VW& w = vwrites[i];
      if (w.f == prevf) dup++;
      prevf = w.f;
      if (i == 0) continue;
      const VW& p = vwrites[i - 1];
      if (w.restart) { resets++; if (w.s != 0 || w.v != 0) badstep++; continue; }
      int dv = (p.v - w.v + 102) % 102, ds = w.s - p.s;
      if (dv != ds || w.v < 0 || w.v >= 102) badstep++;
      int k = w.f < 302 ? 0 : w.f < 502 ? 1 : w.f < 702 ? 2 : w.f < 762 ? 3 : w.f < 852 ? 4 : 5;
      ph[k][ds]++;
    }
    CHECK(dup == 0, "POKE 2072 una vez por imagen");
    CHECK(badstep == 0 && resets == 1, "vofs baja lo que sube s, con la vuelta del anillo; R vuelve a s = 0 y vofs = 0");
    auto only = [&](int k, std::vector<int> ks) { for (auto& kv : ph[k]) { bool in = false; for (int x : ks) if (kv.first == x) in = true; if (!in) return false; } return true; };
    printf("        pasos por imagen: nivel {1:%d}  1/2 {0:%d 1:%d}  2 {2:%d}  parado {0:%d}  1 {1:%d}  tras R {1:%d}\n", ph[0][1], ph[1][0], ph[1][1], ph[2][2], ph[3][0], ph[4][1], ph[5][1]);
    CHECK(only(0, {1}) && only(1, {0, 1}) && only(2, {2}) && only(3, {0}) && only(4, {1}) && only(5, {1}) && ph[1][0] > 80 && ph[1][1] > 80,
          "velocidad del nivel (1 px por imagen) y luego S: 1 cada 2, 2 por imagen, parado y otra vez 1");
    int maxs = 0; for (auto& w : vwrites) maxs = std::max(maxs, w.s);
    printf("        s mas alta: %d (LVL_END %d)\n", maxs, lv.end);
  }
  printf("        sprites: %d imagenes comparadas con el modelo; nave en x %d-%d, y %d-%d; %d disparos, %d salen por arriba, hasta %d a la vez\n",
         R.spr_checked, R.minx, R.maxx, R.miny, R.maxy, R.fired, R.expired, R.maxb);
  printf("        los sprites se escriben hasta %ld T despues del flanco\n", spr_maxlat);
  if (!R.spr_bad.empty()) printf("        sprites mal en %zu imagenes, la primera %d\n", R.spr_bad.size(), R.spr_bad[0]);
  CHECK(R.spr_checked > 800 && R.spr_bad.empty(), "en cada imagen, los sprites de la nave, la llama y los disparos son los del modelo (y ninguno mas)");
  CHECK(R.minx == 16 && R.maxx == 128 && R.miny == 0 && R.maxy == 76 && R.fired > 40 && R.expired > 30 && R.maxb >= 4,
        "la nave llega a los cuatro bordes y se para; los disparos salen, suben y se borran arriba");
  CHECK(spr_maxlat < 12000, "los sprites se escriben en el borrado (antes de los ~12.600 T de la primera linea)");
  {
    int pbad = 0; for (int i = 0; i < 16; i++) if (pal16[i] != (hpal[2 * i] | (hpal[2 * i + 1] << 8))) pbad++;
    CHECK(pbad == 0, "la paleta de 16 es la de HOJA.PAL (PAL16 de HOJA.INC)");
  }
  CHECK(borders.size() >= 6 && borders[1] == 0x24 && borders[2] == 0x25 && borders[3] == 0x26 && borders[4] == 0x22 && borders[5] == 0x24,
        "borde: verde, cyan, amarillo, rojo y verde segun la velocidad");
  {
    int lvlend = LVLBUF + (int)lv.map.size(), touched = 0;
    for (int a = lvlend; a < 0x8000; a++) if ((a < 0x7000 || a >= S("SHEET_END")) && rd(a) != basic_snap[a - 0x4000]) touched++;
    for (int a = 0x4000; a < 0x4700; a++) if (rd(a) != basic_snap[a - 0x4000]) touched++;
    CHECK(touched == 0, "no toca $4000-$46FF ni lo que hay detras del nivel hasta $7FFF (salvo la hoja) (al copiar a $C000 se reescriben los mismos bytes)");
  }
  CHECK(R.blk_bad == 0 && R.p16_bad == 0, "en cada imagen del juego: hoja en el bloque SHEET_BLOCK (POKE 2129), sin extra, paleta de 16 activa (POKE 2067,1)");
  exit_checks("nivel 1", 0);

  // ===== 2. un nivel corto: el scroll se para en LVL_END; R vuelve al principio
  printf("-- nivel corto (10 filas) --\n");
  Level sh = lv; sh.rows = 10; sh.end = 16 * 10 - 100;
  sh.map.resize(lv.mapoff + 8 * 10);
  sh.map[6] = 10; sh.map[7] = 0; sh.map[12] = (uint8_t)(sh.map.size() & 255); sh.map[13] = (uint8_t)(sh.map.size() >> 8);
  sh.map[14] = (uint8_t)sh.end; sh.map[15] = 0;
  sdfiles.clear(); sdfiles["NIVEL1.I16"] = lv.i16; sdfiles["NIVEL1.MAP"] = sh.map; sdfiles["HOJA.BIN"] = hoja;
  R = run(sh, { {200, 204, 2, 3}, {380, 384, 0, 2} }, 420, 200, true);
  {
    int maxs = 0, after = 0, reset_ok = 0, stuck = 0;
    for (auto& w : vwrites) { maxs = std::max(maxs, w.s); if (w.restart && w.s == 0 && w.v == 0) reset_ok++; }
    for (auto& w : vwrites) { if (w.f > 150 && w.f < 200) { after++; if (w.s == sh.end) stuck++; } }
    printf("        s mas alta %d (LVL_END %d); imagenes 150-199 en s = LVL_END: %d de %d\n", maxs, sh.end, stuck, after);
    CHECK(maxs == sh.end && stuck == after && after > 40, "el scroll se para en LVL_END y no pinta mas alla del mapa");
    CHECK(reset_ok == 1 && vwrites.back().s == sh.end, "R vuelve a s = 0 y el nivel se recorre otra vez hasta LVL_END");
    CHECK(R.vis_ok >= 150 && R.vis_bad == 0 && R.pal_bad.empty() && R.spr_bad.empty(), "lo visible, la paleta y los sprites, exactos tambien aqui");
  }
  exit_checks("nivel corto", 0);

  // ===== 3. errores de carga
  printf("-- errores de carga --\n");
  struct Err { const char* what; int bc; bool i16; bool map; int patch; };
  Err errs[] = { {"sin NIVEL1.I16", 1, false, true, 0}, {"NIVEL1.I16 sin I160", 2, true, true, 1}, {"sin NIVEL1.MAP", 3, true, false, 0},
                 {"NIVEL1.MAP sin ELV1", 4, true, true, 2}, {"nivel que no cabe", 5, true, true, 3}, {"nivel de 5 filas", 6, true, true, 4},
                 {"mapa mas largo que el fichero", 6, true, true, 5}, {"sin HOJA.BIN", 7, true, true, 6} };
  for (auto& e : errs) {
    std::vector<uint8_t> i16 = lv.i16, mp = lv.map;
    if (e.patch == 1) i16[3] = '1';
    if (e.patch == 2) mp[0] = 'X';
    if (e.patch == 3) { int len = 0x7000 - LVLBUF + 1; mp[12] = (uint8_t)len; mp[13] = (uint8_t)(len >> 8); }
    if (e.patch == 4) { mp[6] = 5; mp[7] = 0; }
    if (e.patch == 5) { mp[6] = (uint8_t)(lv.rows + 1); mp[7] = (uint8_t)((lv.rows + 1) >> 8); }
    sdfiles.clear(); if (e.i16) sdfiles["NIVEL1.I16"] = i16; if (e.map) sdfiles["NIVEL1.MAP"] = mp; if (e.patch != 6) sdfiles["HOJA.BIN"] = hoja;
    run(lv, {}, 60, 0, false);
    exit_checks(e.what, e.bc);
  }

  printf("%s (%d errores)\n", errors ? "FALLOS" : "TODO OK", errors);
  return errors ? 1 : 0;
}
