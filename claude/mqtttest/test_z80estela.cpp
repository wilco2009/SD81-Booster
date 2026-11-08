// ESTELA (EXAMPLES/ESTELA/estela.asm) en el PC: el Z80 de verdad (nucleo de EightyOne, T-estados exactos) ejecuta el juego con un modelo
// de la memoria paginada (32 paginas de 8 KB, mapeador E7h, bloque 0 = ventana de POKEs), el VSYNC (64.584 T por imagen, 8 lineas a 1,
// como lo midio VSYNC.BIN en la placa), el teclado, el borde, la paleta de 256 (2063-2065), un MCU con las ordenes 53/55/57 (ficheros de
// la SD, servidos desde EXAMPLES/ESTELA/out) y un modelo del scroll en anillo del modo 175 (mode160_emulador.md §6):
//   pixel x de la fila r de la pantalla = byte $8000 + ((r + vofs) mod hc) * 160 + x   (hofs = 0, wc = 160)
//
// FASE 4 (sobre la 3): oleadas de enemigos, disparos enemigos, colisiones, explosiones y marcador. El modelo Game lleva tambien
//   todo eso (las oleadas de NIVEL1.MAP y las trayectorias leidas del texto de LEVEL.INC) y se compara con los 64 sprites en cada
//   imagen; ademas cuenta los sprites por linea (el limite del hardware es 12). Una ejecucion del nivel entero.
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
static int draw_lo = 0, draw_hi = 0; static long spr_maxlat = 0; static bool in_draw = false;   // in_draw: desde que entra en drawspr hasta cycle

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
      if (in_draw) { long lat = (long)(total_t % FRAME); if (lat > spr_maxlat) spr_maxlat = lat; }
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

struct Result { int bc; long frames; int vis_ok, vis_bad; long maxlat, maxloop; std::vector<int> pal_bad; std::vector<int> spr_bad; int fired, expired, spr_checked, maxb, minx, maxx, miny, maxy; int blk_bad, p16_bad;
                int spawned, kills, deaths, drope, dropw, ebfired, maxene, maxline, over12, score, waves, score_shown;
                // reparto por linea como el motor: lineas de sprite perdidas, por clase (0 enemigos, 1 disparos, 2 disparos enemigos,
                // 3 nave y llama, 4 marcador); imagenes con alguna perdida; racha mas larga de un enemigo con lineas perdidas
                long lost_lines, lost_cls[5]; int lost_frames, first_lost, consec[8], max_consec, ene_lost_frames; };

// ---- el juego, escrito aqui con la especificacion (cabecera de estela.asm y juego160.md §8.3), no con el codigo del Z80.
// Nave: x 16-128, y 0-76, 1 px por imagen con O P Q A; inclinacion = dx + 1. Disparos: 8 huecos, suben 3 px por tick, se borran
// con y < -7; con ESPACIO y la espera a 0 sale uno en (x + 4, y - 6) en el primer hueco y la espera pasa a 6; la espera baja 1 por
// tick y se puede disparar en el tick en que llega a 0. Nada de eso mientras la nave explota (salvo que los disparos suben).
// Enemigos, disparos enemigos, colisiones, oleadas y explosiones: ver la cabecera de estela.asm.
static std::vector<uint8_t> g_map;                       // NIVEL1.MAP
static std::vector<std::vector<int>> g_paths;            // PATHTAB de LEVEL.INC: pares dx, dy
static bool g_immortal = false;                          // caso peor del reparto: sin colisiones ni disparos enemigos
static const int EFT[6] = {96, 0, 80, 72, 36, 60}, PTS[6] = {10, 20, 30, 40, 60, 50};
struct Ene { int st, typ, x, y, path, k, flg, tmr, ft, w; };      // w: la oleada (para el caso peor del reparto)
struct EB { int a, x, y, dx; };
struct Spn { int rem, w, t; };
struct Game { int x, y, tilt, cd, tick; int ba[8], bx[8], by[8]; int pstate, ptimer, score; Ene e[8]; EB b[7]; Spn sp[4]; int w; };
static int woff() { return u16(g_map, 8); }
static void ginit(Game& g) {
  g.x = 72; g.y = 76; g.tilt = 1; g.cd = 0; g.tick = 0; g.pstate = 0; g.ptimer = 0; g.score = 0;
  for (int i = 0; i < 8; i++) { g.ba[i] = 0; g.e[i] = Ene{}; }
  for (int i = 0; i < 7; i++) g.b[i] = EB{};
  for (int i = 0; i < 4; i++) g.sp[i] = Spn{};
  g.w = woff();
}
static bool keydown(const std::vector<Key>& sc, long f, int row, int bit) { for (auto& k : sc) if (f >= k.from && f < k.to && k.row == row && k.bit == bit) return true; return false; }
static bool ovl(int b, int c, int d, int e) { return !((e & 255) < (b & 255)) && !((c & 255) < (d & 255)); }
static void kill(Game& g, Ene& e, Result& R) { e.st = 2; e.tmr = 16; g.score = (g.score + PTS[e.typ]) % 1000000; R.kills++; }
static void pdie(Game& g, Result& R) { g.pstate = 1; g.ptimer = 16; R.deaths++; }
static void spawn(Game& g, int w, int x, Result& R) {
  const uint8_t* r = &g_map[w];
  for (int i = 0; i < 8; i++) if (g.e[i].st == 0) {
    Ene& e = g.e[i]; e.st = 1; e.typ = r[2]; e.x = (x + 32) & 255; e.y = 16; e.tmr = 0; e.flg = r[7]; e.ft = EFT[e.typ]; e.path = r[4]; e.k = 0; e.w = w;
    R.spawned++; return;
  }
  R.drope++;
}
static void gupdate(Game& g, const std::vector<Key>& sc, long f, int scr, int nstp, Result& R) {
  g.tick = (g.tick + 1) & 255;
  if (g.pstate != 1) {
    int dx = (keydown(sc, f, 5, 0) ? 1 : 0) - (keydown(sc, f, 5, 1) ? 1 : 0);
    g.tilt = dx + 1; g.x = std::min(128, std::max(16, g.x + dx));
    int dy = (keydown(sc, f, 1, 0) ? 1 : 0) - (keydown(sc, f, 2, 0) ? 1 : 0);
    g.y = std::min(76, std::max(0, g.y + dy));
  }
  for (int i = 0; i < 8; i++) if (g.ba[i]) { g.by[i] -= 3; if (g.by[i] < -7) { g.ba[i] = 0; R.expired++; } }
  if (g.pstate != 1) {
    bool fire = true;
    if (g.cd > 0) { g.cd--; if (g.cd > 0) fire = false; }
    if (fire && keydown(sc, f, 7, 0))
      for (int i = 0; i < 8; i++) if (!g.ba[i]) { g.ba[i] = 1; g.bx[i] = g.x + 4; g.by[i] = g.y - 6; g.cd = 6; R.fired++; break; }
  }
  // enemigos
  for (auto& e : g.e) {
    if (e.st == 1) {
      const std::vector<int>& p = g_paths[e.path];
      int n = (int)p.size() / 2, dx = p[2 * e.k], dy = p[2 * e.k + 1];
      if (e.flg & 1) dx = -dx;
      e.x = (e.x + dx) & 255; e.y = (e.y + dy + ((e.flg & 4) ? nstp : 0)) & 255;
      if (e.k < n - 1) e.k++;
      if (e.x < 33 || e.x >= 176 || e.y < 16 || e.y >= 132) e.st = 0;
    } else if (e.st == 2) { if (--e.tmr == 0) e.st = 0; }
  }
  // disparos enemigos
  for (auto& b : g.b) if (b.a) {
    b.x = (b.x + b.dx) & 255;
    if (b.x < 24 || b.x >= 176) { b.a = 0; continue; }
    b.y = (b.y + 2) & 255;
    if (b.y >= 132) b.a = 0;
  }
  // colisiones
  if (!g_immortal)
  for (int i = 0; i < 8; i++) if (g.ba[i]) {
    int cbx = (g.bx[i] + 35) & 255, cby = (g.by[i] + 32) & 255;
    for (auto& e : g.e) if (e.st == 1 && ovl(cbx, cbx + 1, e.x + 1, e.x + 14) && ovl(cby, cby + 7, e.y + 1, e.y + 14)) { kill(g, e, R); g.ba[i] = 0; break; }
  }
  if (g.pstate == 0 && !g_immortal) {
    int cpx = g.x + 36, cpy = g.y + 35; bool hit = false;
    for (auto& b : g.b) if (b.a && ovl(b.x + 3, b.x + 4, cpx, cpx + 7) && ovl(b.y + 3, b.y + 4, cpy, cpy + 10)) { b.a = 0; pdie(g, R); hit = true; break; }
    if (!hit) for (auto& e : g.e) if (e.st == 1 && !(e.flg & 4) && ovl(e.x + 2, e.x + 13, cpx, cpx + 7) && ovl(e.y + 2, e.y + 13, cpy, cpy + 10)) { kill(g, e, R); pdie(g, R); break; }
  }
  // disparos de los enemigos
  if (!g_immortal)
  for (auto& e : g.e) if (e.st == 1 && e.ft != 0) {
    if (--e.ft != 0) continue;
    e.ft = EFT[e.typ];
    if (e.y >= 32 && e.y < 93 && g.pstate != 1)
      for (auto& b : g.b) if (!b.a) {
        b.a = 1; b.x = (e.x + 4) & 255; b.y = (e.y + 12) & 255;
        int ec = e.x + 8, pc = g.x + 40;
        b.dx = pc >= ec + 16 ? 1 : (pc + 16 <= ec ? -1 : 0);
        b.dx &= 255; R.ebfired++; break;
      }
  }
  for (auto& b : g.b) if (b.a && b.dx == 255) b.dx = -1;
  // oleadas
  while (scr >= u16(g_map, g.w)) {
    int w = g.w; g.w += 8; R.waves++;
    const uint8_t* r = &g_map[w];
    if (r[2] >= 6) continue;
    if (r[7] & 2) { for (int i = 0; i < r[5]; i++) spawn(g, w, r[3] + i * r[6], R); }
    else {
      bool ok = false;
      for (auto& s : g.sp) if (s.rem == 0) { s.rem = r[5]; s.w = w; s.t = 0; ok = true; break; }
      if (!ok) R.dropw++;
    }
  }
  for (auto& s : g.sp) if (s.rem) {
    if (s.t) { if (--s.t != 0) continue; }
    spawn(g, s.w, g_map[s.w + 3], R); s.rem--; s.t = g_map[s.w + 6];
  }
  // la nave
  if (g.pstate && --g.ptimer == 0) {
    if (g.pstate == 1) { g.x = 72; g.y = 76; g.tilt = 1; g.pstate = 2; g.ptimer = 100; } else g.pstate = 0;
  }
}
static long line_loss(Result& R, int& mx, bool elost[8]);
// los registros de los 64 sprites tienen que ser los de este estado; cuenta tambien los sprites por linea
static int spr_check(const Game& g, int scr, Result& R) {
  int bad = 0, NAVE = S("NAVE"), LLAMA = S("LLAMA"), BALA = S("BALAJ_0"), BALAE = S("BALAE_0"), EXPL = S("EXPL"), CIFRA = S("CIFRA");
  int ETAB[6] = {S("ENEMY1"), S("ENEMY2"), S("ENEMY3"), S("ENEMY4"), S("ENEMY5"), S("ENEMY6")};
  bool used[64] = {};
  auto chk = [&](int n, int x, int y, int pg) {
    used[n] = true; const Spr& q = spr[n]; int X = q.xl | ((q.xh & 1) << 8); bool m = true;
    for (int r = 0; r < 8; r++) if (q.mask[r] != 0xFF) m = false;
    if (!(q.en == 1 && X == (x & 255) && q.y == (y & 255) && q.page == pg && m && (q.xh >> 4) == 0)) bad++;
  };
  auto chk4 = [&](int n, int x, int y, int pg) { chk(n, x, y, pg); chk(n + 1, x + 8, y, pg + 1); chk(n + 2, x, y + 8, pg + 2); chk(n + 3, x + 8, y + 8, pg + 3); };
  bool blink = g.pstate == 2 && (g.ptimer & 4);
  if (!blink) {
    chk4(50, g.x + 32, g.y + 32, g.pstate == 1 ? EXPL + ((16 - g.ptimer) & 12) : NAVE + g.tilt * 4);
    if (g.pstate != 1) chk(49, g.x + 36, g.y + 48, LLAMA + ((g.tick >> 2) & 1));
  }
  for (int i = 0; i < 8; i++) if (g.ba[i]) chk(34 + i, g.bx[i] + 32, g.by[i] + 32, BALA);
  int ne = 0;
  for (int i = 0; i < 8; i++) {
    const Ene& e = g.e[i];
    if (e.st == 1) { chk4(4 * i, e.x, e.y, ETAB[e.typ] + ((g.tick & 8) >> 1)); ne++; }
    else if (e.st == 2) { chk4(4 * i, e.x, e.y, EXPL + ((16 - e.tmr) & 12)); ne++; }
  }
  R.maxene = std::max(R.maxene, ne);
  for (int i = 0; i < 7; i++) if (g.b[i].a) chk(42 + i, g.b[i].x, g.b[i].y, BALAE);
  // marcador: una fila de 6 cifras en x 56-103, y 2, solo con la nave explotando o parpadeando o al final del nivel
  if (g.pstate != 0 || scr >= u16(g_map, 14)) {
    int dig[6] = { g.score / 100000, g.score / 10000 % 10, g.score / 1000 % 10, g.score / 100 % 10, g.score / 10 % 10, g.score % 10 };
    for (int i = 0; i < 6; i++) chk(58 + i, 32 + 56 + 8 * i, 32 + 2, CIFRA + dig[i]);
    R.score_shown++;
  }
  for (int n = 0; n < 64; n++) if (!used[n] && spr[n].en) bad++;
  int mx = 0; bool elost[8] = {};
  long lost = line_loss(R, mx, elost);
  R.maxline = std::max(R.maxline, mx);
  if (mx > 12) R.over12++;
  if (lost) { if (!R.lost_frames) R.first_lost = (int)g.tick; R.lost_frames++; R.lost_lines += lost; }
  bool any = false;
  for (int i = 0; i < 8; i++) { if (elost[i]) { any = true; R.consec[i]++; R.max_consec = std::max(R.max_consec, R.consec[i]); } else R.consec[i] = 0; }
  if (any) R.ene_lost_frames++;
  R.score = g.score;
  return bad;
}

// reparto por linea como el motor (sprites_scroll160_emulador.md 1.6): normales del 63 al 0 (sin extra), como mucho 12
// cargados y LIMITE de 171 pasos en la temporizacion de 80 columnas (B cuesta 8, mirar cada sprite 1). Devuelve las lineas
// de sprite que no salen; mx = mas sprites en una linea; elost[i] = el enemigo i (sprites 4i..4i+3) pierde alguna
static long line_loss(Result& R, int& mx, bool elost[8]) {
  long lost = 0; mx = 0;
  for (int row = 0; row < 100; row++) {
    int steps = 2, loaded = 0, cnt = 0; bool stop = false;
    for (int n = 63; n >= 0; n--) {
      steps++;
      if (!spr[n].en) continue;
      if ((((row + 32) - spr[n].y) & 511) >= 8) continue;
      cnt++;
      if (!stop && (loaded == 12 || steps + 8 > 171)) stop = true;
      if (stop) {
        lost++;
        int c = n < 32 ? 0 : n < 42 ? 1 : n < 49 ? 2 : n < 58 ? 3 : 4;
        R.lost_cls[c]++;
        if (n < 32) elost[n >> 2] = true;
        continue;
      }
      steps += 8; loaded++;
    }
    mx = std::max(mx, cnt);
  }
  return lost;
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
  int MAIN = S("main"), RESTART = S("restart"), WAITVS = S("waitvs"), CYCLE = S("cycle"); in_draw = false;
  int TW = S("TCYW"), TL = S("TCYL"), TG = S("TCYG");
  // c0: la vuelta en la que gira por primera vez (luego cada t)
  struct Rng { int base, n, t, c0; } rngs[3] = { {S("PAL_WATER"), S("PAL_WATER_N"), TW, S("CYW0")}, {S("PAL_LAVA"), S("PAL_LAVA_N"), TL, S("CYL0")},
                                                 {S("PAL_LIGHTS"), S("PAL_LIGHTS_N"), TG, S("CYG0")} };
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
    if (z80.pc.w == draw_lo) in_draw = true;
    if (z80.pc.w == CYCLE) in_draw = false;
    if (in_main && z80.pc.w == WAITVS && frame_t0 && !restart_seen) { long lp = (long)(total_t - frame_t0); if (lp > R.maxloop) R.maxloop = lp; }
    if (in_main && z80.pc.w == WAITVS && vwrites.size() > nspr) {
      // acaba una vuelta: lo que se ha visto en esta imagen es el estado de antes de update; luego update con las teclas de la imagen
      nspr = vwrites.size(); long f = vwrites.back().f;
      if (spr_check(g, vwrites.back().s, R)) R.spr_bad.push_back((int)f);     // drawspr va antes de los pasos: el scroll del POKE 2072
      if (spr_blk != S("SHEET_BLOCK") || spr_xtr != 0) R.blk_bad++;
      if (pal16_on != 1) R.p16_bad++;
      R.spr_checked++;
      int nb = 0; for (int i = 0; i < 8; i++) nb += g.ba[i]; R.maxb = std::max(R.maxb, nb);
      R.minx = std::min(R.minx, g.x); R.maxx = std::max(R.maxx, g.x); R.miny = std::min(R.miny, g.y); R.maxy = std::max(R.maxy, g.y);
      int scr_now = rd16(scr_sym);                    // el scroll despues de los pasos de esta imagen (con R se descarta)
      gupdate(g, script, f, scr_now, scr_now - vwrites.back().s, R);
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
          for (auto& g : rngs) if (i >= g.base && i < g.base + g.n) {
            int o = calls >= g.c0 ? (int)((1 + (calls - g.c0) / g.t) % g.n) : 0;
            want = lv.pal(g.base + ((i - g.base) + o) % g.n);
          }
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

static void lost_report(const Result& L) {
  printf("        reparto por linea (motor: 63..0, 12 huecos, 171 pasos): %d imagenes con perdidas, %ld lineas de sprite perdidas"
         " (enemigos %ld, disparos %ld, disparos enemigos %ld, nave %ld, marcador %ld)\n",
         L.lost_frames, L.lost_lines, L.lost_cls[0], L.lost_cls[1], L.lost_cls[2], L.lost_cls[3], L.lost_cls[4]);
  printf("        enemigos con lineas perdidas en %d imagenes; racha mas larga de un mismo enemigo: %d imagenes%s\n",
         L.ene_lost_frames, L.max_consec, L.lost_frames ? "" : " (ninguna)");
  if (L.lost_frames) printf("        primera perdida en el tick %d\n", L.first_lost);
}

// caso peor del reparto, solo con el modelo: el nivel entero a su velocidad con los enemigos inmortales (nadie los derriba ni
// chocan); cuenta los enemigos que cruzan cada linea. Lo que queda para ellos en una linea: 12 - nave (2) - llama (1) - un
// disparo de la nave (salen cada 6 ticks a 3 px: 18 px entre uno y otro) - 2 disparos enemigos = 6 sprites, 3 enemigos; en las
// lineas del marcador (y 2-9) quitar ademas sus 6, pero solo sale tras una muerte (unos 2,3 s).
struct Worst { int maxe, maxe_top, over3, over3_top, frames; long at; std::vector<int> waves; };
static Worst worst_case() {
  Worst W{}; Result R{}; Game g; ginit(g); g_immortal = true;
  int spd = std::max(1, (int)g_map[4]), end = u16(g_map, 14), prev = 0;
  for (long f = 0; ; f++) {
    int scr = std::min(end, (int)(f / spd));
    if (scr == end && f > (long)end * spd + 300) break;
    gupdate(g, {}, f, scr, scr - prev, R); prev = scr; W.frames++;
    int cnt[100] = {}; std::vector<int> who[100];
    for (auto& e : g.e) if (e.st) for (int r = 0; r < 16; r++) { int row = e.y - 32 + r; if (row >= 0 && row < 100) { cnt[row]++; who[row].push_back((e.w - woff()) / 8); } }
    bool o = false, ot = false;
    for (int r = 0; r < 100; r++) {
      if (cnt[r] > W.maxe) { W.maxe = cnt[r]; W.at = f; W.waves = who[r]; }
      if (r >= 2 && r < 10) { W.maxe_top = std::max(W.maxe_top, cnt[r]); if (cnt[r] > 0) ot = true; }
      if (cnt[r] > 3) o = true;
    }
    W.over3 += o; W.over3_top += ot;
  }
  g_immortal = false;
  return W;
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
  {
    // las trayectorias, del texto de LEVEL.INC (PATHTAB: defw nombres; cada nombre: defb n y luego los pares)
    FILE* f = fopen((out + "/LEVEL.INC").c_str(), "r"); if (!f) { printf("falta LEVEL.INC\n"); return 2; }
    std::vector<std::string> lines; char buf[1024];
    while (fgets(buf, sizeof buf, f)) lines.push_back(buf);
    fclose(f);
    std::vector<std::string> names;
    for (size_t i = 0; i + 1 < lines.size(); i++) if (lines[i].rfind("PATHTAB:", 0) == 0) {
      std::string l = lines[i + 1]; size_t q = l.find("defw"); l = l.substr(q + 4);
      std::string cur; for (char c : l) { if (c == ',' || c == '\n' || c == '\r' || c == ' ' || c == '\t') { if (!cur.empty()) names.push_back(cur); cur.clear(); } else cur += c; }
      if (!cur.empty()) names.push_back(cur);
    }
    auto nums = [](const std::string& l, std::vector<int>& v) {
      size_t q = l.find("defb"); if (q == std::string::npos) return false;
      std::string t = l.substr(q + 4); size_t sc = t.find(';'); if (sc != std::string::npos) t = t.substr(0, sc);
      for (char& c : t) if (c == ',') c = ' ';
      char* e = &t[0]; while (true) { char* e2; long x = strtol(e, &e2, 10); if (e2 == e) break; v.push_back((int)x); e = e2; }
      return true;
    };
    for (auto& nm : names) {
      std::vector<int> v;
      for (size_t i = 0; i < lines.size(); i++) if (lines[i].rfind(nm + ":", 0) == 0) {
        nums(lines[i], v);
        for (size_t j = i + 1; j < lines.size() && (lines[j][0] == '\t' || lines[j][0] == ' ') && nums(lines[j], v); j++) {}
        break;
      }
      if (v.empty() || (int)v.size() != 1 + 2 * v[0]) { printf("trayectoria %s mal leida (%zu)\n", nm.c_str(), v.size()); return 2; }
      g_paths.push_back(std::vector<int>(v.begin() + 1, v.end()));
    }
    printf("LEVEL.INC: %zu trayectorias\n", g_paths.size());
  }
  g_map = lv.map;
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
  CHECK(cyc_writes > 0 && cyc_maxlat < 12000, "el ciclo de la paleta se escribe en el borrado (antes de los ~12.600 T de la primera linea)");
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
  CHECK(R.minx == 16 && R.maxx == 128 && R.miny < 30 && R.maxy == 76 && R.fired > 40 && R.expired > 30 && R.maxb >= 4,
        "la nave llega a los bordes y se para (arriba la derriban antes); los disparos salen, suben y se borran arriba");
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
  printf("        juego: %d oleadas, %d enemigos, %d derribados, %d disparos enemigos, %d veces tocada, marcador %d, hasta %d enemigos a la vez\n",
         R.waves, R.spawned, R.kills, R.ebfired, R.deaths, R.score, R.maxene);
  CHECK(R.blk_bad == 0 && R.p16_bad == 0, "en cada imagen del juego: hoja en el bloque SHEET_BLOCK (POKE 2129), sin extra, paleta de 16 activa (POKE 2067,1)");
  exit_checks("nivel 1", 0);

  // ===== 1b. el nivel entero, disparando y moviendose sin parar
  printf("-- nivel 1 entero (4.500 imagenes) --\n");
  {
    std::vector<Key> sc;
    sc.push_back({30, 4440, 7, 0});                                  // ESPACIO todo el rato
    for (long f = 40; f < 4400; f += 100) { sc.push_back({f, f + 45, 5, 1}); sc.push_back({f + 50, f + 95, 5, 0}); }   // O y P
    for (long f = 300; f < 4400; f += 700) { sc.push_back({f, f + 60, 2, 0}); sc.push_back({f + 200, f + 260, 1, 0}); }  // Q y A
    sc.push_back({4450, 4454, 0, 2});                                // X
    sdfiles.clear(); sdfiles["NIVEL1.I16"] = lv.i16; sdfiles["NIVEL1.MAP"] = lv.map; sdfiles["HOJA.BIN"] = hoja;
    Result L = run(lv, sc, 4500, 300, true);
    printf("        %d imagenes; %d oleadas, %d enemigos, %d derribados, %d disparos enemigos, %d veces tocada, marcador %d\n",
           L.spr_checked, L.waves, L.spawned, L.kills, L.ebfired, L.deaths, L.score);
    printf("        el marcador se ve en %d imagenes (tras cada muerte)\n", L.score_shown);
    printf("        hasta %d enemigos a la vez; sin hueco: %d enemigos, %d oleadas; hasta %d sprites en una linea (%d imagenes con mas de 12)\n",
           L.maxene, L.drope, L.dropw, L.maxline, L.over12);
    printf("        vuelta mas larga del bucle %ld T; sprites escritos hasta %ld T tras el flanco\n", L.maxloop, spr_maxlat);
    lost_report(L);
    CHECK(L.lost_frames == 0 && L.maxline <= 12, "con el reparto del motor no se pierde ninguna linea de sprite en todo el nivel");
    if (!L.spr_bad.empty()) printf("        sprites mal en %zu imagenes, la primera %d\n", L.spr_bad.size(), L.spr_bad[0]);
    CHECK(L.spr_checked > 4300 && L.spr_bad.empty(), "en cada imagen del nivel entero, los 64 sprites son los del modelo (nave, disparos, enemigos, explosiones, marcador)");
    CHECK(L.vis_ok >= 250 && L.vis_bad == 0 && L.pal_bad.empty(), "lo visible y la paleta, exactos en todo el nivel");
    CHECK(L.spawned > 60 && L.kills > 10 && L.ebfired > 10 && L.deaths > 0 && L.score > 0, "salen las oleadas, se derriban enemigos, disparan, dan a la nave y suma el marcador");
    CHECK(L.drope == 0 && L.dropw == 0, "ningun enemigo ni oleada se pierde por falta de hueco");
    CHECK(L.score_shown > 0 && L.score_shown < L.spr_checked / 2, "el marcador sale solo tras las muertes (en una fila arriba) y el resto del tiempo esta apagado");
    CHECK(spr_maxlat < 12000, "los sprites se escriben en el borrado (antes de los ~12.600 T de la primera linea)");
    CHECK(L.maxloop < FRAME - 2000, "la vuelta del bucle cabe en una imagen");
    exit_checks("nivel entero", 0);
  }

  // ===== 1c. caso peor del reparto por linea (solo el modelo)
  {
    printf("-- caso peor del reparto: el nivel entero con los enemigos inmortales (solo el modelo) --\n");
    Worst W = worst_case();
    std::sort(W.waves.begin(), W.waves.end()); W.waves.erase(std::unique(W.waves.begin(), W.waves.end()), W.waves.end());
    printf("        %d imagenes; hasta %d enemigos en una linea (imagen %ld, oleadas", W.frames, W.maxe, W.at);
    for (int w : W.waves) printf(" %d", w);
    printf("); imagenes con mas de 3 en una linea: %d\n", W.over3);
    printf("        en las lineas del marcador (y 2-9): hasta %d enemigos; imagenes con alguno alli: %d\n", W.maxe_top, W.over3_top);
    CHECK(W.maxe <= 3, "ninguna linea con mas de 3 enemigos aunque no se derribe ninguno: con la nave, la llama y los disparos caben en los 12");
    // en negativo: la oleada 7 (en fila de 3) con 5 enemigos tiene que dar 5 en una linea
    std::vector<uint8_t> keep = g_map;
    g_map[woff() + 7 * 8 + 5] = 5; g_map[woff() + 7 * 8 + 6] = 24;   // 5 enemigos cada 24 px: x 40-136, caben
    Worst W5 = worst_case();
    g_map = keep;
    CHECK(W5.maxe == 5, "(prueba del caso peor) con la oleada 7 en fila de 5 (cada 24 px), salen 5 en una linea");
    // en negativo: el reparto con 14 sprites en la misma linea (el 60..47 en y 40): no salen los dos de indice mas bajo
    Spr keepspr[64]; memcpy(keepspr, spr, sizeof spr);
    for (int n = 0; n < 64; n++) spr[n].en = 0;
    for (int n = 47; n <= 60; n++) { spr[n].en = 1; spr[n].y = 32 + 40; }
    spr[3].en = 1; spr[3].y = 32 + 80;                  // un enemigo solo, mas abajo: sale
    Result T{}; int mx; bool el[8] = {};
    long lost = line_loss(T, mx, el);
    memcpy(spr, keepspr, sizeof spr);
    CHECK(lost == 16 && mx == 14 && T.lost_cls[2] == 16 && !el[0], "(prueba del reparto) 14 sprites en 8 lineas: no salen los 2 de indice mas bajo (16 lineas)");
  }

  // ===== 1d. el nivel entero sin disparar: los enemigos no se derriban y se juntan mas en pantalla (el caso peor del reparto)
  {
    struct Sc { const char* what; bool weave; } scs[] = { {"sin disparar, quieta abajo", false}, {"sin disparar, moviendose", true} };
    for (auto& c : scs) {
      printf("-- nivel 1 entero, %s --\n", c.what);
      std::vector<Key> sc;
      if (c.weave) {
        for (long f = 40; f < 4400; f += 100) { sc.push_back({f, f + 45, 5, 1}); sc.push_back({f + 50, f + 95, 5, 0}); }
        for (long f = 300; f < 4400; f += 700) { sc.push_back({f, f + 60, 2, 0}); sc.push_back({f + 200, f + 260, 1, 0}); }
      }
      sc.push_back({4450, 4454, 0, 2});
      sdfiles.clear(); sdfiles["NIVEL1.I16"] = lv.i16; sdfiles["NIVEL1.MAP"] = lv.map; sdfiles["HOJA.BIN"] = hoja;
      Result L = run(lv, sc, 4500, 50, true);
      printf("        %d imagenes; %d oleadas, %d enemigos, %d derribados, %d disparos enemigos, %d veces tocada; marcador visible en %d\n",
             L.spr_checked, L.waves, L.spawned, L.kills, L.ebfired, L.deaths, L.score_shown);
      printf("        hasta %d enemigos a la vez; hasta %d sprites en una linea (%d imagenes con mas de 12)\n", L.maxene, L.maxline, L.over12);
      lost_report(L);
      CHECK(L.lost_frames == 0 && L.maxline <= 12, "con el reparto del motor no se pierde ninguna linea de sprite (y el marcador sale mas a menudo)");
      if (!L.spr_bad.empty()) printf("        sprites mal en %zu imagenes, la primera %d\n", L.spr_bad.size(), L.spr_bad[0]);
      CHECK(L.spr_checked > 4300 && L.spr_bad.empty() && L.vis_bad == 0 && L.pal_bad.empty(), "los 64 sprites, lo visible y la paleta, iguales al modelo");
      exit_checks(c.what, 0);
    }
  }

  // ===== 2. un nivel corto: el scroll se para en LVL_END; R vuelve al principio
  printf("-- nivel corto (10 filas) --\n");
  Level sh = lv; sh.rows = 10; sh.end = 16 * 10 - 100;
  sh.map.resize(lv.mapoff + 8 * 10);
  sh.map[6] = 10; sh.map[7] = 0; sh.map[12] = (uint8_t)(sh.map.size() & 255); sh.map[13] = (uint8_t)(sh.map.size() >> 8);
  sh.map[14] = (uint8_t)sh.end; sh.map[15] = 0;
  sdfiles.clear(); sdfiles["NIVEL1.I16"] = lv.i16; sdfiles["NIVEL1.MAP"] = sh.map; sdfiles["HOJA.BIN"] = hoja;
  g_map = sh.map;                                      // el modelo, con el nivel corto
  R = run(sh, { {200, 204, 2, 3}, {380, 384, 0, 2} }, 420, 200, true);
  g_map = lv.map;
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
