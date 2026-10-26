// Prueba de EXAMPLES/SCROLLM/scrollm.bin en el PC: el Z80 de verdad (nucleo de EightyOne) ejecuta el programa de
// prueba del MOTOR de scroll por hardware, con la memoria, los POKEs, los puertos $3FEF (orden y estado del motor),
// $7FEF (Chroma), el teclado y el VSYNC simulados, y un MODELO EN SOFTWARE del motor (escrito aparte de scroll_motor.v:
// cadena L m0..m31 R por linea y por fila de atributos, con la mascara de filas; y el vertical: pares de filas de la
// region con los bufers T y B, la esquina y las columnas L y R). En cada cuadro estable (el programa de
// vuelta en `main` y sin orden pendiente) comprueba que lo que hay en memoria es el mundo esperado: las 32 columnas
// visibles, los dos bufers de entrada (derecha e izquierda, y abajo, arriba y la esquina cuando estan alineados) y el
// juego de atributos que este activo, en HiRes (modos 0 y 1) y en Spectrum, y con el HUD las filas 0 y 23 intactas. Comprueba tambien que la posicion (8 * wc0 + fx) avanza un pixel por cuadro, que nunca
// se manda una orden con el motor ocupado, el doble buffer, los sprites y el ruido (escrituras del Z80 mientras corre el
// motor). NO prueba la FPGA: eso es tb_scroll_motor.v, tb_dbg.v y la placa.
//
//   ./build_scrollm.sh        (necesita g++/gcc, pasmo y el nucleo Z80 de EightyOne)
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
static uint8_t keyrows[8];
static long instr_total = 0;
static uint64_t sim_us() { return (uint64_t)instr_total * 5000ULL / 3250ULL; }
static long frame() { return (long)(sim_us() / 20000ULL); }
static int errors = 0;
#define CHECK(c, msg) do { if (!(c)) { printf("  ERROR: %s\n", msg); errors++; } else printf("  ok    %s\n", msg); } while (0)

struct Poke { uint16_t a; uint8_t v; long f; };
struct Out { uint16_t port; uint8_t v; long f; };
static std::vector<Poke> pokes;
static std::vector<Out> outs;
static uint8_t last_poke[65536];
static bool bad_write = false; static uint16_t bad_addr = 0;
static uint16_t code_lo = 30000, code_hi = 30000;
static long noise_writes = 0;                                            // escrituras en $9E80-$9EFF (la zona del ruido)
static uint8_t mask_l = 0xFF, mask_m = 0xFF, mask_h = 0xFF;             // mascara de filas (POKE 2091-2093)
static int spr_sel = 0; static uint8_t spr_en[64];

// ---- el puerto $3FEF y el motor ----
static int ridx = 0; static bool wexp2 = false; static int wreg2 = 0;
static bool cmd_pend = false; static int cmd_val = 0; static uint64_t cmd_exec = 0, run_until = 0;
static int moves = 0, ignored_cmds = 0, cmds = 0, noops = 0;
static int chroma = 0;
static int reg_top = 0, reg_bot = 23, fine_y = 0, vmoves = 0;      // region vertical (registros 10 y 11) y fino Y (registro 8)
static bool hr_mode() { return last_poke[2045] == 171; }
static bool sp_mode() { return last_poke[2045] == 172; }
static bool busy() { return cmd_pend || sim_us() < run_until; }
static bool row_moves(int r) { uint8_t m = r < 8 ? mask_l : r < 16 ? mask_m : mask_h; return (m >> (r & 7)) & 1; }
static uint64_t next_blank_start(uint64_t t) {                           // la linea 254 de un cuadro de 312 lineas
  uint64_t f = t / 20000ULL, t254 = f * 20000ULL + 20000ULL * 254 / 312;
  return t <= t254 ? t254 : t254 + 20000ULL;
}

// cadena de 34 posiciones (L, m0..m31, R); -1 = no existe. Se aplica el movimiento como en la FPGA
static void move_chain(const int ch[34], int dir) {
  uint8_t tmp[34];
  for (int q = 0; q < 34; q++) tmp[q] = ch[q] >= 0 ? mem[ch[q]] : 0;
  if (dir == 1) {
    for (int q = 1; q <= 32; q++) mem[ch[q]] = tmp[q + 1];
    if (ch[0] >= 0) mem[ch[0]] = tmp[1];
  } else {
    for (int q = 2; q <= 33; q++) mem[ch[q]] = tmp[q - 1];
    if (ch[0] >= 0) mem[ch[1]] = tmp[0];
    mem[ch[33]] = tmp[32];
  }
}
static int sp_addr(int y, int c) { return 0x8000 + (y >> 6) * 2048 + (y & 7) * 256 + ((y >> 3) & 7) * 32 + c; }
// vertical: direccion del byte de la columna q (0 = L, 1..32 = m0..m31, 33 = R) y la linea k en el hueco `slot`
// (0 = bufer de arriba T, 1 = la fila rr, 2 = bufer de abajo B); -1 = no existe. att = 1: el area de atributos
static int vaddr(int att, int slot, int rr, int k, int q) {
  int yy = rr * 8 + k, cc = q - 1, bse = 0x8000;
  if (sp_mode()) {
    if (!att) {
      if (slot == 1) return q == 0 ? bse + 0x1C00 + yy : q == 33 ? bse + 0x1B00 + yy : sp_addr(yy, cc);
      if (slot == 0) return (q >= 1 && q <= 32) ? bse + 0x1F00 + k * 32 + cc : -1;
      if (q >= 1 && q <= 32) return bse + 0x1D00 + k * 32 + cc;
      return q == 33 ? bse + 0x1E40 + k : -1;
    }
    if (slot == 1) return q == 0 ? bse + 0x1CC0 + rr : q == 33 ? bse + 0x1BC0 + rr : bse + 0x1800 + rr * 32 + cc;
    if (slot == 0) return (q >= 1 && q <= 32) ? bse + 0x1E20 + cc : -1;
    if (q >= 1 && q <= 32) return bse + 0x1E00 + cc;
    return q == 33 ? bse + 0x1E48 : -1;
  }
  bool m1 = chroma & 0x10;
  if (att && m1) {                                                       // rejilla de modo 1: no hay L ni T; abajo, la fila 24
    if (q == 0 || slot == 0) return -1;
    return (0xA000 + (slot == 2 ? 24 : rr) * 33 + cc) & 65535;
  }
  if (att) bse = 0xC000;                                                 // HiRes modo 0: la misma geometria en $C000
  if (slot == 1) return q == 0 ? bse + 0x1C00 + yy : q == 33 ? bse + 0x1800 + yy : bse + yy * 32 + cc;
  if (slot == 0) return (q >= 1 && q <= 32) ? bse + 0x1D00 + k * 32 + cc : -1;
  if (q >= 1 && q <= 32) return bse + 0x1900 + k * 32 + cc;
  return q == 33 ? bse + 0x1A00 + k : -1;
}
// cadena T fila_top .. fila_bot B: arriba (3) cada hueco toma el de debajo, abajo (4) el de arriba. false = region vacia
static bool move_vert(int dir) {
  int top = reg_top, bot = reg_bot > 23 ? 23 : reg_bot;
  if (top > bot) return false;
  int nr = bot - top + 1;
  bool color = chroma & 0x20, m1 = chroma & 0x10;
  for (int att = 0; att < 2; att++) {
    if (att && !sp_mode() && !color) continue;
    int nl = att ? ((sp_mode() || m1) ? 1 : 8) : 8;
    for (int k = 0; k < nl; k++)
      for (int q = 0; q < 34; q++) {
        int vs[27]; uint8_t vt[27];
        for (int s = 0; s <= nr + 1; s++) {
          vs[s] = s == 0 ? vaddr(att, 0, 0, k, q) : s == nr + 1 ? vaddr(att, 2, 0, k, q) : vaddr(att, 1, top + s - 1, k, q);
          vt[s] = vs[s] >= 0 ? mem[vs[s]] : 0;
        }
        if (dir == 3) { for (int s = 0; s <= nr; s++) if (vs[s] >= 0 && vs[s + 1] >= 0) mem[vs[s]] = vt[s + 1]; }
        else          { for (int s = 1; s <= nr + 1; s++) if (vs[s] >= 0 && vs[s - 1] >= 0) mem[vs[s]] = vt[s - 1]; }
      }
  }
  return true;
}
static void do_move(int dir) {
  if ((!hr_mode() && !sp_mode()) || dir < 1 || dir > 4) { noops++; return; }
  if (dir >= 3) { if (move_vert(dir)) { moves++; vmoves++; } else noops++; return; }
  int ch[34];
  for (int y = 0; y < 192; y++) {                                        // bitmap
    if (!row_moves(y >> 3)) continue;
    if (sp_mode()) {
      ch[0] = 0x9C00 + y; ch[33] = 0x9B00 + y;
      for (int c = 0; c < 32; c++) ch[c + 1] = sp_addr(y, c);
    } else {
      ch[0] = 0x9C00 + y; ch[33] = 0x9800 + y;
      for (int c = 0; c < 32; c++) ch[c + 1] = 0x8000 + y * 32 + c;
    }
    move_chain(ch, dir);
  }
  if (sp_mode()) {                                                       // atributos del Spectrum (siempre, haya o no color)
    for (int r = 0; r < 24; r++) {
      if (!row_moves(r)) continue;
      ch[0] = 0x9CC0 + r; ch[33] = 0x9BC0 + r;
      for (int c = 0; c < 32; c++) ch[c + 1] = 0x9800 + r * 32 + c;
      move_chain(ch, dir);
    }
  } else {
    bool color = chroma & 0x20, m1 = chroma & 0x10;
    if (color && !m1)                                                    // HiRes modo 0 (8x1)
      for (int y = 0; y < 192; y++) {
        if (!row_moves(y >> 3)) continue;
        ch[0] = 0xDC00 + y; ch[33] = 0xD800 + y;
        for (int c = 0; c < 32; c++) ch[c + 1] = 0xC000 + y * 32 + c;
        move_chain(ch, dir);
      }
    if (color && m1)                                                     // HiRes modo 1 (8x8, paso 33, sin L)
      for (int r = 0; r < 24; r++) {
        if (!row_moves(r)) continue;
        ch[0] = -1; ch[33] = 0xA000 + r * 33 + 32;
        for (int c = 0; c < 32; c++) ch[c + 1] = 0xA000 + r * 33 + c;
        move_chain(ch, dir);
      }
  }
  moves++;
}
static void motor_tick() {
  if (cmd_pend && sim_us() >= cmd_exec) { do_move(cmd_val); cmd_pend = false; run_until = cmd_exec + 1500; }
}

extern "C" {
int  z80_contend_wrapper(int, int, int) { return 0; }
int  z80_contend_io_wrapper(int, int, int) { return 0; }
BYTE z80_readbyte_wrapper(int a) { return mem[a & 0xFFFF]; }
BYTE z80_readoperandbyte_wrapper(int a) { return mem[a & 0xFFFF]; }
BYTE z80_opcode_fetch_wrapper(int a) { z80_fetch_is_m1 = 0; return mem[a & 0xFFFF]; }
void z80_writebyte_wrapper(int a, int d) {
  a &= 0xFFFF;
  if (a < 0x2000) {
    pokes.push_back({(uint16_t)a, (uint8_t)d, frame()}); last_poke[a] = (uint8_t)d;
    if (a == 2091) mask_l = (uint8_t)d; if (a == 2092) mask_m = (uint8_t)d; if (a == 2093) mask_h = (uint8_t)d;
    if (a == 2100) spr_sel = d & 63;
    if (a == 2101) spr_en[spr_sel] = (uint8_t)d;
    return;
  }
  bool ok = (a >= 0x8000 && a < 0xAF00) || (a >= 0xC000 && a < 0xDE00) || (a >= code_lo && a < code_hi) || a >= 0xFF00;
  if (!ok && !bad_write) { bad_write = true; bad_addr = (uint16_t)a; }
  if (a >= 0x9E80 && a < 0x9F00) noise_writes++;
  mem[a] = (uint8_t)d;
}
void z80_writeport_wrapper(int port, int d, int*) {
  outs.push_back({(uint16_t)port, (uint8_t)d, frame()});
  if (port == 0x7FEF) chroma = d;
  if (port == 0x3FEF) {
    if (wexp2) {
      wexp2 = false;
      if (wreg2 == 1 && (d & 7) != 0) {                                 // registro 9: orden del motor
        cmds++;
        if (busy()) ignored_cmds++;
        else { cmd_pend = true; cmd_val = d & 7; cmd_exec = next_blank_start(sim_us()); }
      } else if (wreg2 == 0) fine_y = d & 7;                            // registro 8: fino Y
      else if (wreg2 == 2) reg_top = d & 31;                            // registro 10: primera fila de la region
      else if (wreg2 == 3) reg_bot = d & 31;                            // registro 11: ultima
    } else if (d >= 0x20 && d <= 0x2F) ridx = 16 + (d & 15);
    else if (d >= 0x40 && d <= 0x4F) { wexp2 = true; wreg2 = d & 15; }
    else if (d <= 0x0F) ridx = d;
  }
}
BYTE z80_readport_wrapper(int port, int*) {
  int lo = port & 0xFF;
  if (lo == 0xAF) return (sim_us() % 20000ULL) < 512 ? 1 : 0;
  if (lo == 0xFE) {
    uint8_t v = 0x1F;
    for (int r = 0; r < 8; r++) if (!(((port >> 8) >> r) & 1)) v &= (uint8_t)~keyrows[r];
    return (uint8_t)(v | 0xE0);
  }
  if (port == 0x3FEF) {
    if (ridx == 16) return (uint8_t)((busy() ? 0x80 : 0) | (cmd_pend ? 0x40 : 0) | ((!cmd_pend && sim_us() < run_until) ? 0x20 : 0));
    return 0;
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

// ---- el mundo ----
static const uint8_t ATT[8]  = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x90};     // Chroma: papel en el nibble alto
static const uint8_t SATT[8] = {0x08, 0x10, 0x18, 0x20, 0x28, 0x30, 0x38, 0x48};     // Spectrum
static uint8_t w_bm(int wc, int y) {
  int p = wc & 1, yy = y & 15; uint8_t b = 0;
  for (int k = 0; k < 8; k++) b = (uint8_t)((b << 1) | ((((8 * p + k + yy) & 15) < 2) ? 1 : 0));
  return b;
}
static uint8_t w_a0(int wc, int y) { return ATT[(wc + y) & 7]; }
static uint8_t w_a1(int wc, int r) { return ATT[(wc + r) & 7]; }
static uint8_t w_as(int wc, int r) { return SATT[(wc + r) & 7]; }
static uint8_t hud_pb(int c) { return (c & 1) ? 0xF0 : 0x0F; }                        // las rayas del HUD

// compara la memoria con el mundo; devuelve el numero de bytes distintos (y el primero en `first`).
// El mundo de la pantalla empieza en la fila wr0; los bufers de abajo y de arriba (y la esquina) son las filas
// wr0 + rbot + 1 y wr0 + rtop - 1, y solo se comprueban cuando estan alineados con wc0 (btd = 0)
static int check_world(char* first, size_t fsz) {
  int wc0 = var("wc0"), wr0 = var("wr0"), amode = var("amode"), vmode = var("vmode"), hud = var("hud"), btd = var("btd"), bad = 0;
  int rtop = hud ? 1 : 0, rbot = hud ? 22 : 23;
  first[0] = 0;
  auto note = [&](const char* what, int a, int got, int want) {
    if (!bad) snprintf(first, fsz, "%s en $%04X: hay %02X, esperado %02X (wc0=%d, wr0=%d, vmode=%d, amode=%d, hud=%d)", what, a, got, want, wc0, wr0, vmode, amode, hud);
    bad++;
  };
  auto hudy = [&](int y) { return hud && (y < 8 || y >= 184); };                   // linea de una fila fija
  auto hudr = [&](int r) { return hud && (r == 0 || r == 23); };
  for (int c = 0; c < 32; c++) {
    int wc = (wc0 + c) & 255;
    for (int y = 0; y < 192; y++) {
      int a = vmode ? sp_addr(y, c) : 0x8000 + y * 32 + c;
      int want = hudy(y) ? hud_pb(c) : w_bm(wc, y + 8 * wr0);
      if (mem[a] != want) note(hudy(y) ? "HUD bitmap" : "bitmap", a, mem[a], want);
      if (!vmode && amode == 0) {
        int b = 0xC000 + y * 32 + c, w = hudy(y) ? 0xF0 : w_a0(wc, y);
        if (mem[b] != w) note(hudy(y) ? "HUD atributo modo 0" : "atributo modo 0", b, mem[b], w);
      }
    }
    for (int r = 0; r < 24; r++) {
      if (!vmode && amode == 1) { int b = 0xA000 + r * 33 + c, w = hudr(r) ? 0xF0 : w_a1(wc, r + wr0); if (mem[b] != w) note(hudr(r) ? "HUD atributo modo 1" : "atributo modo 1", b, mem[b], w); }
      if (vmode)                { int b = 0x9800 + r * 32 + c, w = hudr(r) ? 0x78 : w_as(wc, r + wr0); if (mem[b] != w) note(hudr(r) ? "HUD atributo Spectrum" : "atributo Spectrum", b, mem[b], w); }
    }
  }
  int wcR = (wc0 + 32) & 255, wcL = (wc0 + 255) & 255;
  int rb = vmode ? 0x9B00 : 0x9800, lb = 0x9C00;
  for (int y = 0; y < 192; y++) {
    if (hudy(y)) continue;                                                 // el motor no toca las filas fijas: sus bufers no se mantienen
    if (mem[rb + y] != w_bm(wcR, y + 8 * wr0)) note("bufer R bitmap", rb + y, mem[rb + y], w_bm(wcR, y + 8 * wr0));
    if (mem[lb + y] != w_bm(wcL, y + 8 * wr0)) note("bufer L bitmap", lb + y, mem[lb + y], w_bm(wcL, y + 8 * wr0));
    if (!vmode && amode == 0) {
      if (mem[0xD800 + y] != w_a0(wcR, y)) note("bufer R modo 0", 0xD800 + y, mem[0xD800 + y], w_a0(wcR, y));
      if (mem[0xDC00 + y] != w_a0(wcL, y)) note("bufer L modo 0", 0xDC00 + y, mem[0xDC00 + y], w_a0(wcL, y));
    }
  }
  for (int r = 0; r < 24; r++) {
    if (hudr(r)) continue;
    if (!vmode && amode == 1) { int b = 0xA000 + r * 33 + 32; if (mem[b] != w_a1(wcR, r + wr0)) note("columna 32 modo 1", b, mem[b], w_a1(wcR, r + wr0)); }
    if (vmode) {
      if (mem[0x9BC0 + r] != w_as(wcR, r + wr0)) note("bufer R atributo Spectrum", 0x9BC0 + r, mem[0x9BC0 + r], w_as(wcR, r + wr0));
      if (mem[0x9CC0 + r] != w_as(wcL, r + wr0)) note("bufer L atributo Spectrum", 0x9CC0 + r, mem[0x9CC0 + r], w_as(wcL, r + wr0));
    }
  }
  if (!btd) {                                                              // bufers de abajo (B), de arriba (T) y la esquina
    int WB = (wr0 + rbot + 1) & 255, WT = (wr0 + rtop - 1) & 255;
    int bB = vmode ? 0x9D00 : 0x9900, bT = vmode ? 0x9F00 : 0x9D00, bC = vmode ? 0x9E40 : 0x9A00;
    for (int k = 0; k < 8; k++) {
      for (int c = 0; c < 32; c++) {
        int wc = (wc0 + c) & 255, a = bB + k * 32 + c, w = w_bm(wc, 8 * WB + k);
        if (mem[a] != w) note("bufer de abajo bitmap", a, mem[a], w);
        a = bT + k * 32 + c; w = w_bm(wc, 8 * WT + k);
        if (mem[a] != w) note("bufer de arriba bitmap", a, mem[a], w);
        if (!vmode && amode == 0) {
          a = 0xD900 + k * 32 + c; w = w_a0(wc, k);
          if (mem[a] != w) note("bufer de abajo modo 0", a, mem[a], w);
          a = 0xDD00 + k * 32 + c;
          if (mem[a] != w) note("bufer de arriba modo 0", a, mem[a], w);
        }
      }
      int a = bC + k, w = w_bm(wcR, 8 * WB + k);
      if (mem[a] != w) note("esquina bitmap", a, mem[a], w);
      if (!vmode && amode == 0) { a = 0xDA00 + k; w = w_a0(wcR, k); if (mem[a] != w) note("esquina modo 0", a, mem[a], w); }
    }
    if (vmode) {
      for (int c = 0; c < 32; c++) {
        int wc = (wc0 + c) & 255, a = 0x9E00 + c, w = w_as(wc, WB);
        if (mem[a] != w) note("bufer de abajo atributo Spectrum", a, mem[a], w);
        a = 0x9E20 + c; w = w_as(wc, WT);
        if (mem[a] != w) note("bufer de arriba atributo Spectrum", a, mem[a], w);
      }
      if (mem[0x9E48] != w_as(wcR, WB)) note("esquina atributo Spectrum", 0x9E48, mem[0x9E48], w_as(wcR, WB));
    }
    if (!vmode && amode == 1)
      for (int c = 0; c <= 32; c++) {
        int a = 0xA000 + 24 * 33 + c, w = w_a1((wc0 + c) & 255, WB);
        if (mem[a] != w) note("fila 24 de la rejilla", a, mem[a], w);
      }
  }
  return bad;
}

int main(int argc, char** argv) {
  const char* bin = argc > 1 ? argv[1] : "scrollm.bin";
  const char* sym = argc > 2 ? argv[2] : "scrollm.sym";
  FILE* f = fopen(bin, "rb");
  if (!f) { printf("no encuentro %s\n", bin); return 2; }
  size_t sz = fread(mem + 30000, 1, 0x4000, f); fclose(f);
  load_syms(sym);
  code_hi = (uint16_t)(30000 + sz);
  printf("scrollm.bin: %zu bytes en 30000-%u\n", sz, 30000 + (unsigned)sz - 1);
  CHECK(30000 + sz < 0x8000, "el programa queda por debajo de $8000");

  z80_init(); z80_reset();
  z80.sp.w = 0xFFF0; mem[0xFFF0] = 0x00; mem[0xFFF1] = 0x01; mem[0x0100] = 0x76;
  z80.pc.w = 30000; z80.halted = 0; z80.iff1 = z80.iff2 = 0;
  z80.ix.w = 0x1234; z80.iy.w = 0x5678;                                   // la ROM del ZX81 los espera intactos

  struct K { long from, to; int row, bit; };
  const K keys[] = {
    {70, 169, 5, 0},      // P: izquierda, 100 cuadros (HiRes, atributos de modo 0)
    {180, 309, 5, 1},     // O: derecha, 130 cuadros (pasa por detras del origen)
    {330, 332, 7, 2},     // M: atributos de modo 1
    {370, 439, 5, 0},     // P
    {450, 509, 5, 1},     // O
    {520, 522, 6, 4},     // H: HUD (filas 0 y 23 fijas)
    {540, 619, 5, 0},     // P con HUD
    {630, 689, 5, 1},     // O con HUD
    {700, 702, 6, 4},     // H: sin HUD
    {710, 712, 1, 2},     // D: doble buffer AUTO
    {730, 809, 5, 0},     // P con doble buffer
    {820, 879, 5, 1},     // O con doble buffer
    {890, 892, 7, 3},     // N: ruido (el Z80 escribe mientras corre el motor)
    {900, 979, 5, 0},     // P con ruido
    {990, 1049, 5, 1},    // O con ruido
    {1060, 1062, 7, 3},   // N: sin ruido
    {1070, 1072, 1, 1},   // S: sprites
    {1080, 1082, 2, 4},   // T: Spectrum
    {1130, 1209, 5, 0},   // P en Spectrum
    {1220, 1279, 5, 1},   // O en Spectrum
    {1290, 1292, 6, 4},   // H: HUD en Spectrum
    {1310, 1389, 5, 0},   // P
    {1400, 1459, 5, 1},   // O
    {1470, 1472, 7, 3},   // N: ruido en Spectrum
    {1480, 1539, 5, 0},   // P
    {1560, 1562, 7, 3},   // N: sin ruido
    {1570, 1572, 2, 4},   // T: otra vez HiRes (modo 1, con HUD)
    {1600, 1602, 7, 0},   // SPACE: automatico (160 izquierda, 160 derecha, ...)
    {2300, 2302, 7, 0},   // SPACE: manual
    {2320, 2322, 6, 4},   // H: sin HUD
    {2340, 2342, 1, 2},   // D: sin doble buffer
    {2350, 2352, 1, 1},   // S: sin sprites
    // ---- vertical: Q arriba (fila 2, bit 0), A abajo (fila 1, bit 0), P / O con ellas = diagonales, V automatico vertical ----
    {2400, 2479, 2, 0},   // Q: arriba, 80 cuadros (HiRes modo 1)
    {2490, 2569, 1, 0},   // A: abajo
    {2580, 2659, 1, 0},   // A: abajo otra vez (pasa por detras del origen)
    {2670, 2749, 2, 0},   // Q
    {2760, 2839, 2, 0}, {2760, 2839, 5, 0},   // Q + P: diagonal
    {2850, 2929, 1, 0}, {2850, 2929, 5, 1},   // A + O
    {2940, 2942, 7, 2},   // M: atributos de modo 0
    {2960, 3039, 2, 0},   // Q (modo 0)
    {3050, 3129, 1, 0},   // A
    {3140, 3219, 2, 0}, {3140, 3219, 5, 1},   // Q + O
    {3230, 3232, 6, 4},   // H: HUD (region 1-22)
    {3250, 3329, 2, 0},   // Q con HUD
    {3340, 3419, 1, 0},   // A con HUD
    {3430, 3509, 1, 0}, {3430, 3509, 5, 0},   // A + P con HUD
    {3520, 3522, 6, 4},   // H: sin HUD
    {3540, 3542, 2, 4},   // T: Spectrum
    {3590, 3669, 2, 0},   // Q en Spectrum
    {3680, 3759, 1, 0},   // A
    {3770, 3849, 2, 0}, {3770, 3849, 5, 0},   // Q + P
    {3860, 3862, 6, 4},   // H: HUD en Spectrum
    {3880, 3959, 2, 0},   // Q
    {3970, 4049, 1, 0},   // A
    {4060, 4062, 1, 2},   // D: doble buffer
    {4080, 4159, 2, 0},   // Q con doble buffer
    {4170, 4249, 1, 0},   // A
    {4260, 4262, 7, 3},   // N: ruido
    {4280, 4359, 2, 0},   // Q con ruido
    {4370, 4449, 1, 0},   // A
    {4460, 4462, 7, 3},   // N: sin ruido
    {4470, 4472, 1, 1},   // S: sprites
    {4490, 4549, 2, 0},   // Q con sprites
    {4560, 4562, 1, 1},   // S: sin sprites
    {4570, 4572, 1, 2},   // D: sin doble buffer
    {4600, 4602, 0, 4},   // V: el automatico va en vertical
    {4610, 4612, 7, 0},   // SPACE: automatico (160 arriba, 160 abajo, ...)
    {4960, 4962, 7, 0},   // SPACE: manual
    {4970, 4972, 0, 4},   // V: otra vez horizontal
    {4980, 4982, 6, 4},   // H: sin HUD
    {5100, 5110, 0, 2},   // X: salir
  };

  std::map<long, std::function<void()>> at;
  long noise_a = 0, noise_b = 0, noise_c = 0;
  at[600] = [&]() {
    printf("-- HUD puesto en HiRes modo 1 (cuadro %ld)\n", frame());
    CHECK(var("hud") == 1 && var("amode") == 1 && var("vmode") == 0, "HUD = 1, HiRes, atributos de modo 1");
    CHECK(mask_l == 0xFE && mask_m == 0xFF && mask_h == 0x7F, "mascara de filas con el HUD: filas 0 y 23 fijas (FE, FF, 7F)");
  };
  at[720] = [&]() {
    printf("-- sin HUD y con el doble buffer (cuadro %ld)\n", frame());
    CHECK(var("hud") == 0 && mask_l == 0xFF && mask_m == 0xFF && mask_h == 0xFF, "HUD = 0 y la mascara de filas vuelve a FF, FF, FF");
    CHECK(last_poke[2057] == 175, "doble buffer AUTO con el bloque 7 de delante (POKE 2057,175)");
  };
  at[885] = [&]() { noise_a = noise_writes; CHECK(noise_writes == 0, "sin ruido: nadie escribe en $9E80-$9EFF"); };
  at[975] = [&]() { noise_b = noise_writes; CHECK(noise_b - noise_a > 2000, "con el ruido el Z80 escribe sin parar (mas de 2000 escrituras en 80 cuadros)"); };
  at[1065] = [&]() { noise_c = noise_writes; };
  at[1075] = [&]() {
    CHECK(noise_writes == noise_c, "al quitar el ruido deja de escribir");
    CHECK(spr_en[0] == 1 && spr_en[1] == 1, "dos sprites activados (POKE 2101)");
  };
  at[1110] = [&]() {
    printf("-- Spectrum (cuadro %ld)\n", frame());
    CHECK(var("vmode") == 1 && last_poke[2045] == 172 && chroma == 0x20, "Spectrum: POKE 2045,172 y Chroma en modo 0");
  };
  at[1300] = [&]() { CHECK(var("hud") == 1 && var("vmode") == 1, "HUD en Spectrum"); };
  at[1590] = [&]() {
    printf("-- HiRes otra vez (cuadro %ld)\n", frame());
    CHECK(var("vmode") == 0 && last_poke[2045] == 171 && var("hud") == 1 && var("amode") == 1, "HiRes (171), atributos de modo 1 y HUD");
  };
  at[2330] = [&]() { CHECK(var("hud") == 0 && mask_l == 0xFF && mask_m == 0xFF && mask_h == 0xFF, "HUD fuera: mascara por defecto"); };
  at[2345] = [&]() { CHECK(last_poke[2057] == 85, "doble buffer apagado (POKE 2057,85)"); };
  at[2355] = [&]() { CHECK(spr_en[0] == 0 && spr_en[1] == 0, "sprites apagados"); };
  at[2395] = [&]() {
    printf("-- vertical (cuadro %ld)\n", frame());
    CHECK(var("vmode") == 0 && var("amode") == 1 && var("hud") == 0 && reg_top == 0 && reg_bot == 23, "HiRes, atributos de modo 1, sin HUD y la region entera (0-23)");
  };
  at[3260] = [&]() { CHECK(var("hud") == 1 && var("amode") == 0 && reg_top == 1 && reg_bot == 22, "HUD en HiRes modo 0: region 1-22 (registros 10 y 11)"); };
  at[3530] = [&]() { CHECK(var("hud") == 0 && reg_top == 0 && reg_bot == 23, "sin HUD: la region vuelve a 0-23"); };
  at[3550] = [&]() { CHECK(var("vmode") == 1 && last_poke[2045] == 172, "Spectrum"); };
  at[3890] = [&]() { CHECK(var("hud") == 1 && var("vmode") == 1 && reg_top == 1 && reg_bot == 22, "HUD en Spectrum: region 1-22"); };
  at[4075] = [&]() { CHECK(last_poke[2057] == 175, "doble buffer AUTO con el bloque 7 de delante"); };
  at[4620] = [&]() { CHECK(var("avert") == 1 && var("auto") == 1, "automatico en vertical"); };
  at[4990] = [&]() { CHECK(var("avert") == 0 && var("auto") == 0 && var("hud") == 0 && reg_top == 0 && reg_bot == 23, "manual, horizontal, sin HUD y la region entera"); };

  struct Sample { long f; int pos; int pend; int posv; };
  std::vector<Sample> samples;
  long checks = 0, bad_frames = 0; char first[240] = {0}; std::string first_msg; long first_frame = -1;
  long last_check_frame = -1;
  long chk_hr0 = 0, chk_hr1 = 0, chk_sp = 0, chk_hud = 0;
  long guard = 0;
  while (z80.pc.w != 0x0100) {
    long fr = frame();
    for (int r = 0; r < 8; r++) keyrows[r] = 0;
    for (auto& k : keys) if (fr >= k.from && fr <= k.to) keyrows[k.row] |= (uint8_t)(1 << k.bit);
    motor_tick();
    while (!at.empty() && at.begin()->first <= fr && z80.pc.w == syms.at("main")) {   // las comprobaciones de un cuadro, la primera vez que el programa vuelve a `main` desde entonces
      auto fn = at.begin()->second; at.erase(at.begin()); fn();
    }
    if (z80.pc.w == syms.at("main") && fr != last_check_frame) {          // el programa en el bucle principal
      last_check_frame = fr;
      int pos = (int)var("wc0") * 8 + var("fx");
      samples.push_back({fr, pos, var("pend"), (int)var("wr0") * 8 + var("fy")});
      if (var("pend") == 0 && !busy()) {
        int bad = check_world(first, sizeof first);
        checks++;
        if (var("vmode")) chk_sp++; else if (var("amode")) chk_hr1++; else chk_hr0++;
        if (var("hud")) chk_hud++;
        if (bad) { bad_frames++; if (first_frame < 0) { first_frame = fr; first_msg = first; } }
      }
    }
    z80_do_opcode(); instr_total++;
    if (++guard > 1500000000L) { printf("ERROR: el Z80 no termina (PC=%04X)\n", z80.pc.w); return 1; }
    if (fr > 5300) { printf("ERROR: no ha salido con X (cuadro %ld, PC=%04X)\n", fr, z80.pc.w); errors++; break; }
  }
  printf("-- recorrido (%zu cuadros muestreados, %ld comprobados, %d movimientos del motor)\n", samples.size(), checks, moves);
  printf("   comprobados: HiRes modo 0 %ld, HiRes modo 1 %ld, Spectrum %ld, con HUD %ld\n", chk_hr0, chk_hr1, chk_sp, chk_hud);
  CHECK(bad_frames == 0, "en cada cuadro estable la memoria es exactamente el mundo (32 columnas, bufers, atributos activos, HUD intacto)");
  if (bad_frames) printf("        primer fallo en el cuadro %ld: %s (%ld cuadros mal)\n", first_frame, first_msg.c_str(), bad_frames);
  CHECK(chk_hr0 > 250 && chk_hr1 > 200 && chk_sp > 200 && chk_hud > 300, "se han comprobado muchos cuadros de cada modo: HiRes 0 y 1, Spectrum y HUD");
  CHECK(ignored_cmds == 0, "nunca se manda una orden con el motor ocupado");
  CHECK(noops == 0, "todas las ordenes eran movimientos de verdad (HiRes o Spectrum, sentido 1 a 4)");

  // la posicion en pixeles (8 * wc0 + fx) avanza un pixel por cuadro mientras se mantiene P, y retrocede con O
  auto pos_at = [&](long f) { int p = 0; for (auto& s : samples) if (s.f <= f) p = s.pos; return p; };
  auto span_ok = [&](long f0, long f1, int sign, int& moved, int& stalls, int& wrong) {
    moved = stalls = wrong = 0; bool first_s = true; int prev = 0;
    for (auto& s : samples) if (s.f >= f0 && s.f <= f1) {
      if (!first_s) {
        int d = ((s.pos - prev + 1024) & 2047) - 1024;                          // la posicion envuelve cada 256 columnas = 2048 pixeles
        if (d == sign) moved++; else if (d == 0) stalls++; else wrong++;
      }
      prev = s.pos; first_s = false;
    }
  };
  struct Span { long a, b; int sign; const char* what; };
  const Span spans[] = {
    {75, 168, +1, "HiRes modo 0"}, {185, 308, -1, "HiRes modo 0, hacia atras"},
    {375, 438, +1, "HiRes modo 1"}, {455, 508, -1, "HiRes modo 1, hacia atras"},
    {545, 618, +1, "con HUD"}, {635, 688, -1, "con HUD, hacia atras"},
    {735, 808, +1, "con doble buffer"}, {825, 878, -1, "con doble buffer, hacia atras"},
    {905, 978, +1, "con ruido"}, {995, 1048, -1, "con ruido, hacia atras"},
    {1135, 1208, +1, "Spectrum"}, {1225, 1278, -1, "Spectrum, hacia atras"},
    {1315, 1388, +1, "Spectrum con HUD"}, {1405, 1458, -1, "Spectrum con HUD, hacia atras"},
    {1485, 1538, +1, "Spectrum con ruido"},
  };
  for (auto& s : spans) {
    int mv, st, wr; span_ok(s.a, s.b, s.sign, mv, st, wr);
    char msg[160]; snprintf(msg, sizeof msg, "%s: cada cuadro %s exactamente un pixel (%d pasos, %d parados, %d saltos)", s.what, s.sign > 0 ? "avanza" : "retrocede", mv, st, wr);
    CHECK(wr == 0 && st == 0 && mv >= (s.b - s.a) - 2, msg);
  }
  // la posicion vertical (8 * wr0 + fy) avanza un pixel por cuadro con Q, retrocede con A, y la horizontal no se mueve
  auto vspan_ok = [&](long f0, long f1, int sign, int& moved, int& stalls, int& wrong, int& hmoved) {
    moved = stalls = wrong = hmoved = 0; bool first_s = true; int prev = 0, prevh = 0;
    for (auto& s : samples) if (s.f >= f0 && s.f <= f1) {
      if (!first_s) {
        int d = ((s.posv - prev + 1024) & 2047) - 1024;
        if (d == sign) moved++; else if (d == 0) stalls++; else wrong++;
        if (s.pos != prevh) hmoved++;
      }
      prev = s.posv; prevh = s.pos; first_s = false;
    }
  };
  const Span vspans[] = {
    {2405, 2478, +1, "arriba, HiRes modo 1"}, {2495, 2568, -1, "abajo, HiRes modo 1"}, {2585, 2658, -1, "abajo, por detras del origen"}, {2675, 2748, +1, "arriba otra vez"},
    {2965, 3038, +1, "arriba, HiRes modo 0"}, {3055, 3128, -1, "abajo, HiRes modo 0"},
    {3255, 3328, +1, "arriba con HUD"}, {3345, 3418, -1, "abajo con HUD"},
    {3595, 3668, +1, "arriba, Spectrum"}, {3685, 3758, -1, "abajo, Spectrum"},
    {3885, 3958, +1, "arriba, Spectrum con HUD"}, {3975, 4048, -1, "abajo, Spectrum con HUD"},
    {4085, 4158, +1, "arriba con doble buffer"}, {4175, 4248, -1, "abajo con doble buffer"},
    {4285, 4358, +1, "arriba con ruido"}, {4375, 4448, -1, "abajo con ruido"},
    {4495, 4548, +1, "arriba con sprites"},
  };
  for (auto& s : vspans) {
    int mv, st, wr, hm; vspan_ok(s.a, s.b, s.sign, mv, st, wr, hm);
    char msg[170]; snprintf(msg, sizeof msg, "%s: cada cuadro %s exactamente un pixel en vertical (%d pasos, %d parados, %d saltos, %d movimientos horizontales)", s.what, s.sign > 0 ? "avanza" : "retrocede", mv, st, wr, hm);
    CHECK(wr == 0 && st == 0 && hm == 0 && mv >= (s.b - s.a) - 2, msg);
  }
  CHECK(vmoves >= 100, "el motor ha hecho al menos 100 movimientos verticales");
  {   // diagonales: los dos fino avanzan (el vertical y el horizontal se esperan a veces uno al otro, pero ninguno retrocede)
    const Span diag[] = {{2765, 2838, +1, "Q + P"}, {2855, 2928, -1, "A + O"}, {3145, 3218, +1, "Q + O"}, {3435, 3508, -1, "A + P"}, {3775, 3848, +1, "Q + P en Spectrum"}};
    const int hs[] = {+1, -1, -1, +1, +1};
    int i = 0;
    for (auto& s : diag) {
      int vup = 0, vback = 0, hup = 0, hback = 0, prev = 0, prevh = 0; bool first_s = true;
      for (auto& x : samples) if (x.f >= s.a && x.f <= s.b) {
        if (!first_s) {
          int dv = ((x.posv - prev + 1024) & 2047) - 1024, dh = ((x.pos - prevh + 1024) & 2047) - 1024;
          if (dv * s.sign > 0) vup++; else if (dv * s.sign < 0) vback++;
          if (dh * hs[i] > 0) hup++; else if (dh * hs[i] < 0) hback++;
        }
        prev = x.posv; prevh = x.pos; first_s = false;
      }
      char msg[170]; snprintf(msg, sizeof msg, "diagonal %s: avanza en los dos ejes y no retrocede (vertical %d / %d, horizontal %d / %d)", s.what, vup, vback, hup, hback);
      CHECK(vup > 50 && hup > 50 && vback == 0 && hback == 0, msg);
      i++;
    }
  }
  {   // automatico vertical: sube y baja
    bool up_seen = false, down_seen = false;
    for (size_t i = 1; i < samples.size(); i++) if (samples[i].f >= 4620 && samples[i].f <= 4950) {
      int d = ((samples[i].posv - samples[i - 1].posv + 1024) & 2047) - 1024;
      if (d > 0) up_seen = true;
      if (d < 0) down_seen = true;
    }
    CHECK(up_seen && down_seen, "el automatico vertical va arriba y abajo");
  }
  int back = ((pos_at(310) - pos_at(170) + 1024) & 2047) - 1024;
  CHECK(back <= -120 && back >= -135, "O retrocede unos 130 pixeles y pasa por detras del origen (wc0 da la vuelta)");

  CHECK(moves >= 150, "el motor ha hecho al menos 150 movimientos");
  bool saw_auto_right = false, saw_auto_left = false;
  for (size_t i = 1; i < samples.size(); i++) if (samples[i].f >= 1620 && samples[i].f <= 2290) {
    int d = ((samples[i].pos - samples[i - 1].pos + 1024) & 2047) - 1024;
    if (d > 0) saw_auto_left = true;
    if (d < 0) saw_auto_right = true;
  }
  CHECK(saw_auto_left && saw_auto_right, "el automatico va en los dos sentidos");

  printf("-- salida con X (cuadro %ld)\n", frame());
  CHECK(z80.pc.w == 0x0100, "el programa vuelve (RET)");
  CHECK(z80.ix.w == 0x1234 && z80.iy.w == 0x5678, "IX e IY intactos al volver");
  CHECK(last_poke[2045] == 85 && last_poke[2061] == 85 && last_poke[2090] == 0, "modo ZX81 normal, sin base propia de atributos y fino X a 0");
  CHECK(last_poke[2057] == 85 && mask_l == 0xFF && mask_m == 0xFF && mask_h == 0xFF, "doble buffer apagado y mascara de filas por defecto");
  CHECK(spr_en[0] == 0 && spr_en[1] == 0, "sprites apagados");
  CHECK(chroma == 0x20, "Chroma en modo 0 al salir");
  CHECK(fine_y == 0 && reg_top == 0 && reg_bot == 23, "fino Y a 0 y region entera (0-23) al salir");
  CHECK(!bad_write, "solo escribe en la pantalla, los atributos, las tablas ($A400-$AEFF), sus variables y la pila");
  if (bad_write) printf("        primera escritura fuera de sitio: $%04X\n", bad_addr);
  bool unexpected = false;
  for (auto& o : outs) if (o.port != 0x3FEF && (o.port & 0xFF) != 0xEF && (o.port & 0xFF) != 0xFB) unexpected = true;
  CHECK(!unexpected, "solo usa los puertos $3FEF, $7FEF (color) y $FB (borde)");
  printf(errors ? "%d ERRORES\n" : "TODO OK\n", errors);
  return errors ? 1 : 0;
}
