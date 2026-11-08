// Pruebas de la fase 0 de ESTELA (EXAMPLES/ESTELA/PRUEBAS) en el PC: el Z80 de verdad (nucleo de EightyOne, con los T-estados exactos
// de cada instruccion) ejecuta cada programa con un modelo de:
//   - la memoria paginada: 32 paginas de 8 KB, el mapeador (puerto E7h, OUT (C),A / IN (C)) y el bloque 0 de solo lectura (sus escrituras
//     son la ventana de POKEs de control: paleta 2063-2066, sprites 2100-2131, modo 2045...)
//   - el VSYNC (puerto $AF bit 0: a 1 los primeros VSW T-estados de cada imagen de FRAME T), el teclado y el borde ($7FEF)
//   - los dos AY de la FPGA ($CF/$0F y $C7/$07) y un MCU simulado (bit 7 de $AF y $A7) con los comandos 23, 34-38, 40 y 41
//
//   carga   : devuelve 0; y 1, 2, 4 y 8 con el patron roto, la rutina de $7FF0 rota, RAMTOP mal y la pila por encima de $4800
//   vsync   : D = 116 + 26n exacto en cada imagen (del final del IN que ve el flanco al final de la escritura del blanco); F y W medidos;
//             teclas Q A P O con sus topes (1 y 2300); M; X
//   sonido  : comandos al MCU, registros de los AY de la FPGA en cada imagen, borde, salida; y sin MUSICA.VGM
//   pagina  : las cuatro fases desde donde tocan (PC), la ventana de POKEs desde cada una, las paginas restauradas; y un cambio en la pagina 21
//
// NO prueba la FPGA ni la placa (el tiempo de la linea 58 es justo lo que mide VSYNC en la placa).
//
//   ./build_estela0.sh
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

static const int FRAME = 64584;                  // 312 lineas de 207 T
static const int VSW = 1656;                     // VSYNC a 1 durante 8 lineas (medido en la placa con VSYNC.BIN: W = 1.650)
static uint8_t sram[32][8192];
static int mapb[8];
static bool blk0_writable = false;
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

// ---- lo que se observa
struct PalW { uint64_t t; int idx; uint16_t v; };
static std::vector<PalW> palw;                   // escrituras de la paleta de 256 (2065)
static int pal_idx = 0; static uint8_t pal_lo = 0;
static bool wrote_2065 = false; static uint8_t wrote_2065_v = 0;
static int last2045 = -1;
static std::vector<int> borders;                 // cada OUT a $7FEF (4 bits bajos), sin repetir
static int sprsel = 0; static std::vector<int> spr0x; static int spr0en = -1, spr0y = -1, spr0page = -1;
static bool bad_write = false; static uint16_t bad_addr = 0;
static int lo_ok = 0x4800, hi_ok = 0x8000;       // donde puede escribir el programa (ademas de la imagen, la pila y la ventana)
static bool allow_e000 = false;
static int last_af = 0;
static uint8_t keyrows[8];
// AY de la FPGA
struct AyW { long f; int chip, reg, val; };
static std::vector<AyW> ayw;
static int aysel[2] = {0, 0};
static int ayreg[2][16];
// MCU
static int mcu_clk = 0;
static std::vector<uint8_t> mcu_out; static size_t mcu_outpos = 0;
static std::vector<std::vector<int>> mcu_log;    // un vector por comando, con sus parametros
static std::vector<long> mcu_log_f;
static int mcu_need = 0;                         // parametros que faltan del comando en curso (-1 = longitud y nombre)
static int vgm_status = 0;
static int map_bad = 0;

static void mcu_end(std::vector<int>& c) {
  if (c[0] == 34) mcu_out.push_back((uint8_t)vgm_status);
  if (c[0] == 23) mcu_out.push_back(0);
}
static void mcu_byte(uint8_t b) {
  if (mcu_need == 0) {
    mcu_log.push_back({b}); mcu_log_f.push_back(frame());
    switch (b) {
      case 23: case 34: mcu_need = -1; break;        // longitud y nombre, respuesta
      case 35: case 36: case 37: break;
      case 38: mcu_need = 1; break;
      case 40: mcu_need = -2; break;                 // direccion, longitud y bytes
      case 41: mcu_need = 2; break;
      default: printf("orden del MCU inesperada: %d\n", b); errors++; break;
    }
    return;
  }
  std::vector<int>& c = mcu_log.back();
  c.push_back(b);
  if (mcu_need > 0) { if (--mcu_need == 0) mcu_end(c); return; }
  if (mcu_need == -1) { if ((int)c.size() == 2 + c[1]) { mcu_need = 0; mcu_end(c); } return; }
  if (mcu_need == -2) { if (c.size() >= 3 && (int)c.size() == 3 + c[2]) mcu_need = 0; return; }
}

extern "C" {
int  z80_contend_wrapper(int, int, int time) { return time; }
int  z80_contend_io_wrapper(int, int, int time) { return time; }
BYTE z80_readbyte_wrapper(int a) { return rd(a); }
BYTE z80_readoperandbyte_wrapper(int a) { return rd(a); }
BYTE z80_opcode_fetch_wrapper(int a) { z80_fetch_is_m1 = 0; return rd(a); }
void z80_writebyte_wrapper(int a, int d) {
  a &= 0xFFFF;
  if (a < 0x2000) {                              // bloque 0: la ventana de POKEs (la sombra), nunca la pagina salvo si es escribible
    if (blk0_writable) wr_raw(a, (uint8_t)d);
    if (a == 2063) pal_idx = d & 255;
    else if (a == 2064) pal_lo = (uint8_t)d;
    else if (a == 2065) { palw.push_back({0, pal_idx, (uint16_t)((d << 8) | pal_lo)}); pal_idx = (pal_idx + 1) & 255; wrote_2065 = true; wrote_2065_v = (uint8_t)d; }
    else if (a == 2066) pal_idx = (pal_idx + 1) & 255;
    else if (a == 2045) last2045 = d;
    else if (a == 2100) sprsel = d;
    else if (a == 2101 && sprsel == 0) spr0en = d;
    else if (a == 2102 && sprsel == 0) spr0x.push_back(d);
    else if (a == 2104 && sprsel == 0) spr0y = d;
    else if (a == 2130 && sprsel == 0) spr0page = d;
    else if ((a >= 2038 && a <= 2131)) { }
    else if (!bad_write) { bad_write = true; bad_addr = (uint16_t)a; }
    return;
  }
  bool ok = (a >= 0x8000 && a < 0xBE80) || (a >= lo_ok && a < hi_ok) || (a >= 0x4700 && a < 0x4800) || (allow_e000 && a >= 0xE000);
  if (!ok && !bad_write) { bad_write = true; bad_addr = (uint16_t)a; }
  wr_raw(a, (uint8_t)d);
}
void z80_writeport_wrapper(int port, int d, int*) {
  int lo = port & 0xFF, hi = (port >> 8) & 0xFF;
  if (lo == 0xA7) { mcu_byte((uint8_t)d); mcu_clk ^= 1; return; }
  if (lo == 0xEF && hi == 0x7F) { int c = d & 15; if (borders.empty() || borders.back() != c) borders.push_back(c); return; }
  if (lo == 0xE7) {
    int blk = d & 7, pg = (d >> 3) & 31;
    if (pg != (hi & 31)) map_bad++;              // la pagina va igual en el dato (simple) y en B (completa)
    mapb[blk] = pg; return;
  }
  if (lo == 0xCF || lo == 0xC7) { aysel[lo == 0xC7] = d & 15; return; }
  if (lo == 0x0F || lo == 0x07) { int ch = lo == 0x07; ayreg[ch][aysel[ch]] = d; ayw.push_back({frame(), ch, aysel[ch], d}); return; }
  printf("OUT inesperado al puerto %04X\n", port & 0xFFFF); errors++;
}
BYTE z80_readport_wrapper(int port, int*) {
  int lo = port & 0xFF;
  if (lo == 0xAF) { last_af = ((total_t % FRAME) < (uint64_t)VSW ? 1 : 0) | (mcu_clk << 7); return (BYTE)last_af; }
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

// ---- comun
static const int RET_AT = 0x4700;                // direccion de vuelta (HALT) y pila por debajo de $4800
static void reset_all(const char* bin, int sp = 0x47F0) {
  memset(sram, 0, sizeof sram);
  int init[8] = {0, 1, 2, 3, 4, 5, 2, 3};
  memcpy(mapb, init, sizeof mapb);
  sram[0][0] = 0xD3; sram[0][1] = 0xFD;          // el principio de la ROM
  for (int i = 2; i < 8192; i++) sram[0][i] = (uint8_t)(i * 7);
  for (int i = 0; i < 8192; i++) sram[1][i] = (uint8_t)(i * 13 + 5);
  memset(sram[4], 0xEE, 8192); memset(sram[5], 0xEE, 8192);
  FILE* f = fopen(bin, "rb"); if (!f) { printf("no encuentro %s\n", bin); exit(2); }
  std::vector<uint8_t> b(0x4000); size_t n = fread(b.data(), 1, b.size(), f); fclose(f);
  for (size_t i = 0; i < n; i++) wr_raw((int)(0x4800 + i), b[i]);
  wr_raw(RET_AT, 0x76);
  wr_raw(0x4004, 0x00); wr_raw(0x4005, 0x48);    // RAMTOP = 18432 (CLEAR 18431)
  palw.clear(); borders.clear(); spr0x.clear(); ayw.clear(); mcu_log.clear(); mcu_log_f.clear();
  mcu_out.clear(); mcu_outpos = 0; mcu_need = 0; mcu_clk = 0; map_bad = 0;
  bad_write = false; last2045 = -1; spr0en = spr0y = spr0page = -1; total_t = 0; blk0_writable = false; allow_e000 = false;
  memset(keyrows, 0, sizeof keyrows); memset(ayreg, 0, sizeof ayreg);
  lo_ok = 0x4800; hi_ok = 0x4800 + (int)n;
  z80_init(); z80_reset();
  z80.sp.w = (WORD)sp;
  wr_raw(sp, RET_AT & 255); wr_raw(sp + 1, RET_AT >> 8);
  z80.pc.w = 0x4800; z80.halted = 0; z80.iff1 = z80.iff2 = 0;
  z80.ix.w = 0x0281; z80.iy.w = 0x4000;
}

struct Key { long from, to; int row, bit; };
static std::vector<Key> script;
static void set_keys() {
  long fr = frame();
  memset(keyrows, 0, sizeof keyrows);
  for (const Key& k : script) if (fr >= k.from && fr < k.to) keyrows[k.row] |= (uint8_t)(1 << k.bit);
}

// ejecuta hasta volver; hook se llama tras cada instruccion con el PC de antes
template <class H> static bool run(long max_frames, H hook) {
  while (z80.pc.w != RET_AT) {
    set_keys();
    int pc = z80.pc.w;
    wrote_2065 = false;
    z80_do_opcode(); total_t += (uint64_t)tstates;
    if (wrote_2065) palw.back().t = total_t;
    hook(pc);
    if (frame() > max_frames) { printf("  ERROR: no vuelve (imagen %ld, PC=%04X)\n", frame(), z80.pc.w); errors++; return false; }
  }
  return true;
}
static void nohook(int) {}

// ---- CARGA
static int run_carga(const char* bin, int variant) {
  reset_all(bin);
  if (variant == 1) wr_raw(0x6000, rd(0x6000) ^ 1);
  if (variant == 2) wr_raw(0x7FF1, 0x5B);        // XOR $5A -> XOR $5B: devuelve otro A
  if (variant == 3) wr_raw(0x4004, 0x01);
  if (variant == 4) { reset_all(bin, 0x9000); }
  if (!run(100, nohook)) return -1;
  return z80.bc.w;
}
static void test_carga(const char* bin) {
  printf("CARGA\n");
  int rc = run_carga(bin, 0);
  CHECK(rc == 0, "devuelve 0 con todo bien");
  CHECK(rd16(0x4802) == 0, "0 errores del patron (PEEK 18434)");
  CHECK(!bad_write, "solo escribe su contador y la pila");
  CHECK(hi_ok == 0x8000, "el binario ocupa exactamente $4800-$7FFF");
  rc = run_carga(bin, 1);
  CHECK(rc == 1 && rd16(0x4802) == 1, "un byte del patron cambiado: 1, con 1 error");
  rc = run_carga(bin, 2);
  CHECK(rc == 2, "la rutina de $7FF0 cambiada: 2");
  rc = run_carga(bin, 3);
  CHECK(rc == 4, "RAMTOP 18433: 4");
  rc = run_carga(bin, 4);
  CHECK(rc == 8, "la pila por encima de $4800: 8");
}

// ---- VSYNC
static void test_vsync(const char* bin, const char* symf) {
  printf("VSYNC\n");
  if (!load_sym(symf)) { printf("no encuentro %s\n", symf); exit(2); }
  int W2 = S("w2"), NV = S("nval"), DV = S("dval"), FV = S("fval"), WV = S("wval"), MAIN = S("main");
  uint64_t t_white = 0; long long post_max = 0, post_still = 0; int prevn_post = -1;
  reset_all(bin);
  // filas: 0 SHIFT Z X C V, 1 A S D F G, 2 Q W E R T, 5 P O I U Y, 7 SPACE . M N B
  script = { {20, 50, 2, 0},            // Q: +1 por vuelta
             {60, 80, 5, 0},            // P: +8
             {90, 300, 5, 1},           // O: -8 hasta el tope de 1
             {310, 320, 1, 0},          // A: sigue en 1
             {330, 400, 7, 2},          // M: volver a medir (una vez, espera a soltar)
             {410, 1100, 5, 0},         // P: hasta el tope de 2300 (con D grande, una vuelta cada 2 imagenes al escribir D)
             {1120, 1140, 2, 0},        // Q: sigue en 2300
             {1150, 1160, 0, 2} };      // X
  uint64_t t_edge = 0; bool have_edge = false;
  int nD = 0, badD = 0, nblack_late = 0, nmin = 1 << 30, nmax = -1, badstep = 0, prevn = rd16(NV);
  long long minD = 1LL << 40, maxD = -1;
  bool ok = run(1250, [&](int pc) {
    if (pc == W2 && (last_af & 1)) { t_edge = total_t; have_edge = true; }
    if (wrote_2065 && have_edge) {
      const PalW& p = palw.back();
      if (p.idx == 0 && p.v == 0x0000 && p.t - t_edge > 60) nblack_late++;
      if (p.idx == 0 && p.v == 0xFFFF) {
        long D = (long)(p.t - t_edge), n = rd16(NV);
        nD++; if (D != 116 + 26 * n) { if (badD < 3) printf("        D = %ld con n = %ld (esperado %ld)\n", D, n, 116 + 26 * n); badD++; }
        if (D < minD) minD = D; if (D > maxD) maxD = D;
        have_edge = false; t_white = p.t; prevn_post = rd16(NV);
      }
    }
    if (pc == MAIN && t_white) {               // lo que tarda la vuelta despues del corte (teclas y, si cambia D, escribirlo)
      long long d = (long long)(total_t - t_white);
      if (rd16(NV) != prevn_post) { if (d > post_max) post_max = d; } else if (d > post_still) post_still = d;
      t_white = 0;
    }
    int n = rd16(NV);
    if (n != prevn) {
      int d = n - prevn;
      if (!(d == 1 || d == -1 || d == 8 || d == -8 || n == 1 || n == 2300)) badstep++;
      if (n < 1 || n > 2300) badstep++;
      prevn = n;
    }
    if (n < nmin) nmin = n; if (n > nmax) nmax = n;
  });
  CHECK(ok, "vuelve con X");
  printf("        despues del corte: %lld T sin cambiar D, hasta %lld T cuando se escribe D\n", post_still, post_max);
  printf("        %d imagenes con el corte; D de %lld a %lld T\n", nD, minD, maxD);
  CHECK(nD > 700 && badD == 0, "D = 116 + 26n exacto en cada imagen (del final del IN del flanco al final de la escritura del blanco)");
  CHECK(nblack_late == 0, "el negro se escribe en los 60 T siguientes al flanco");
  CHECK(nmin == 1 && nmax == 2300 && badstep == 0, "Q A P O cambian n de 1 y de 8, con los topes 1 y 2300");
  long n = rd16(NV);
  CHECK(rd16(DV) == 116 + 26 * n, "el D que se ve es el de n");
  int F = rd16(FV), Wv = rd16(WV);
  printf("        F medido = %d (modelo %d), W medido = %d (modelo %d)\n", F, FRAME, Wv, VSW);
  CHECK(abs(F - FRAME) <= 40 && abs(Wv - VSW) <= 40, "F y W medidos con menos de 40 T de error");
  CHECK(last2045 == 85, "al salir, POKE 2045,85");
  CHECK(!borders.empty() && borders.back() == 12, "al salir, $7FEF = $0C (color de Chroma apagado: el BASIC se lee)");
  CHECK(z80.ix.w == 0x0281, "IX como estaba (la ROM lo usa en SLOW)");
  CHECK(!bad_write, "no escribe fuera del programa, la imagen, la pila y la ventana");
  if (bad_write) printf("        primera escritura rara en $%04X\n", bad_addr);
  // la imagen: cifras de F en su sitio (la del 6 de 64584) y la regla
  CHECK(rd(0x8000 + 0 * 160 + 7) == 2 && rd(0x8000 + 10 * 160 + 3) == 2 && rd(0x8000 + 10 * 160 + 4) == 0 && rd(0x8000 + 50 * 160 + 159) == 2,
        "la regla: larga en la fila 0 y la 50, corta en la 10");
}

// ---- SONIDO
static std::vector<int> zx(const char* s) { std::vector<int> v; for (; *s; s++) { char c = *s;
  v.push_back(c >= 'A' && c <= 'Z' ? 38 + c - 'A' : c >= '0' && c <= '9' ? 28 + c - '0' : c == '.' ? 27 : c == '*' ? 23 : -1); } return v; }
static bool has_cmd(const std::vector<int>& want, long f0, long f1) {
  for (size_t i = 0; i < mcu_log.size(); i++) if (mcu_log[i] == want && mcu_log_f[i] >= f0 && mcu_log_f[i] < f1) return true;
  return false;
}
static int count_cmd(int c) { int n = 0; for (auto& v : mcu_log) if (v[0] == c) n++; return n; }
static void test_sonido(const char* bin, int status) {
  printf("SONIDO (PLAY_VGM devuelve %d)\n", status);
  reset_all(bin);
  vgm_status = status;
  // filas: 3 = 1 2 3 4 5, 7 = SPACE . M N B, 0 = SHIFT Z X C V
  script = { {30, 34, 3, 0},   // 1 laser
             {80, 84, 3, 1},   // 2 explosion
             {130, 134, 3, 2}, // 3 PEG
             {170, 174, 3, 3}, // 4 voz
             {240, 244, 3, 4}, // 5 todos
             {280, 284, 4, 4}, // 6 acorde
             {300, 304, 7, 2}, // M parar
             {320, 324, 7, 2}, // M seguir
             {360, 362, 0, 2} };
  bool ok = run(400, nohook);
  CHECK(ok, "vuelve con X");
  std::vector<int> peg = {40, 0, 24, 0x3E,0x07,0x00,0x01,0x0C,0x08,0x96,0x00,0x3C,0x90,0x64,0x00,0x50,0x90,0x4B,0x00,0x64,0x90,0x00,0x08,0x3F,0x07,0x10,0xA0};
  std::vector<int> play = {34, 10}; for (int c : zx("MUSICA.VGM")) play.push_back(c);
  std::vector<int> say = {23, 8}; for (int c : zx("*WARNING")) say.push_back(c);
  CHECK(mcu_log.size() >= 3 && mcu_log[0] == peg && mcu_log[1] == std::vector<int>({38, 1}) && mcu_log[2] == play,
        "al empezar: LOAD_PEG coin en la direccion 0, LOOP_VGM 1 y PLAY_VGM \"MUSICA.VGM\" en codigo ZX81");
  CHECK(has_cmd({41, 0, 0}, 130, 140) && has_cmd(say, 170, 180) && has_cmd({41, 0, 0}, 240, 250) && has_cmd(say, 240, 250),
        "3 y 5: PLAY_PEG hilo 0 direccion 0; 4 y 5: SAY \"*WARNING\" (en segundo plano)");
  CHECK(count_cmd(41) == 2 && count_cmd(23) == 2, "cada tecla una sola vez aunque se mantenga pulsada");
  if (status == 0) {
    CHECK(has_cmd({36}, 300, 310) && has_cmd({37}, 320, 330) && count_cmd(36) == 1 && count_cmd(37) == 1, "M: PAUSE_VGM y luego CONT_VGM");
    CHECK(!mcu_log.empty() && mcu_log.back() == std::vector<int>({35}), "al salir: STOP_VGM");
    CHECK(borders == std::vector<int>({0, 4, 6, 4, 12}), "borde: verde con musica, amarillo parada, verde otra vez");
  } else {
    CHECK(count_cmd(35) == 0 && count_cmd(36) == 0 && count_cmd(37) == 0, "sin musica, ni STOP, ni PAUSE, ni CONT");
    CHECK(borders == std::vector<int>({0, 2, 12}), "borde rojo sin musica");
  }
  // laser: AY A canal C, R7 = $3B al pulsar y luego 15 imagenes con R10 = 14..0 y el periodo subiendo de 12 en 12
  std::vector<int> vols; std::vector<int> per; bool mix = false; long fA = -1;
  for (auto& w : ayw) if (w.chip == 0 && w.f >= 30 && w.f < 70) {
    if (w.reg == 7 && w.val == 0x3B) mix = true;
    if (w.reg == 10) { vols.push_back(w.val); if (fA < 0) fA = w.f; }
    if (w.reg == 4) per.push_back(w.val);
  }
  bool volok = vols.size() == 15; for (size_t i = 0; volok && i < 15; i++) if (vols[i] != 14 - (int)i) volok = false;
  bool perok = per.size() == 15; for (size_t i = 0; perok && i < 15; i++) if (per[i] != 52 + 12 * (int)i) perok = false;
  CHECK(mix && volok && perok, "laser (AY A, canal C): mezclador $3B, 15 imagenes con volumen 14..0 y periodo 52, 64, ...");
  std::vector<int> vb; bool mixb = false; int noise_last = -1;
  for (auto& w : ayw) if (w.chip == 1 && w.f >= 80 && w.f < 130) {
    if (w.reg == 7 && w.val == 0x1F) mixb = true;
    if (w.reg == 10) vb.push_back(w.val);
    if (w.reg == 6) noise_last = w.val;
  }
  bool vbok = vb.size() == 30; for (size_t i = 0; vbok && i < 30; i++) if (vb[i] != (29 - (int)i) / 2) vbok = false;
  CHECK(mixb && vbok && noise_last == 31, "explosion (AY B, canal C): mezclador $1F, 30 imagenes con volumen 14..0 cada 2 y ruido hasta 31");
  bool a5 = false, b5 = false;
  for (auto& w : ayw) if (w.f >= 240 && w.f < 246 && w.reg == 10 && w.val == 14) { if (w.chip == 0) a5 = true; else b5 = true; }
  CHECK(a5 && b5, "5: el laser y la explosion a la vez");
  // 6: acorde en los dos chips (R0-R5 con los periodos, R7 = $38, volumenes 15) y 50 imagenes despues, callados
  { int regs[2][16]; memset(regs, -1, sizeof regs); long fq[2] = {-1, -1};
    for (auto& w : ayw) if (w.f >= 280 && w.f < 286) regs[w.chip][w.reg] = w.val;
    for (auto& w : ayw) if (w.f >= 286 && w.f < 355 && w.reg == 7 && w.val == 0x3F && fq[w.chip] < 0) fq[w.chip] = w.f;
    int pa[6] = {400 & 255, 400 >> 8, 330 & 255, 330 >> 8, 270 & 255, 270 >> 8}, pb[6] = {220, 0, 180, 0, 145, 0};
    bool ch = regs[0][7] == 0x38 && regs[1][7] == 0x38;
    for (int r = 0; r < 6; r++) ch = ch && regs[0][r] == pa[r] && regs[1][r] == pb[r];
    for (int r = 8; r < 11; r++) ch = ch && regs[0][r] == 15 && regs[1][r] == 15;
    printf("        acorde callado en la imagen %ld / %ld\n", fq[0], fq[1]);
    CHECK(ch && fq[0] >= 280 + 49 && fq[0] <= 280 + 54 && fq[1] == fq[0], "6: acorde en los tres canales de los dos AY de la FPGA, 1 s, y luego callados"); }
  CHECK(ayreg[0][7] == 0x3F && ayreg[1][7] == 0x3F && ayreg[0][8] == 0 && ayreg[1][8] == 0 && ayreg[0][9] == 0 && ayreg[1][10] == 0,
        "al salir, los dos AY de la FPGA callados");
  CHECK(last2045 == 85, "al salir, POKE 2045,85");
  CHECK(!bad_write, "no escribe fuera del programa, la imagen, la pila y la ventana");
  if (bad_write) printf("        primera escritura rara en $%04X\n", bad_addr);
}

// ---- PAGINA
static void test_pagina(const char* bin, const char* symf, bool inject) {
  printf("PAGINA%s\n", inject ? " (con un byte de la pagina 21 cambiado)" : "");
  if (!load_sym(symf)) { printf("no encuentro %s\n", symf); exit(2); }
  int BLOB = S("blob"), BLOBEND = S("blobend"), BLEN = BLOBEND - BLOB;
  reset_all(bin);
  allow_e000 = true;
  script = {};
  // fase de cada instruccion segun el borde; PC visto en cada fase dentro de cada zona
  struct Ph { int pcs_blob = 0, pcs_2000 = 0, pcs_1000 = 0, pcs_07f0 = 0, pal = 0; std::vector<int> xs; };
  std::map<int, Ph> ph;
  size_t palseen = 0, xseen = 0;
  bool injected = false;
  bool ok = run(1200, [&](int pc) {
    int b = borders.empty() ? -1 : borders.back();
    Ph& p = ph[b];
    if (pc >= BLOB && pc < BLOBEND) p.pcs_blob++;
    if (pc >= 0x2000 && pc < 0x2000 + BLEN) p.pcs_2000++;
    if (pc >= 0x1000 && pc < 0x1000 + BLEN) p.pcs_1000++;
    if (pc >= 0x07F0 && pc < 0x07F0 + BLEN) p.pcs_07f0++;
    while (palseen < palw.size()) { p.pal++; palseen++; }
    while (xseen < spr0x.size()) { p.xs.push_back(spr0x[xseen++]); }
    if (inject && !injected && b == 5 && pc >= 0x07F0 && pc < 0x0800) { sram[21][0x1500] = 0x55; injected = true; }
  });
  CHECK(ok, "vuelve");
  for (int b : {7, 1, 3, 5}) printf("        borde %d: PC en el blob %d, en $2000 %d, en $1000 %d, en $07F0 %d\n", b, ph[b].pcs_blob, ph[b].pcs_2000, ph[b].pcs_1000, ph[b].pcs_07f0);
  CHECK(borders == std::vector<int>({0, 7, 1, 3, 5, inject ? 2 : 4, 12}), inject ? "bordes: blanco, azul, magenta, cyan y rojo" : "bordes: blanco, azul, magenta, cyan y verde");
  CHECK(ph[7].pcs_blob > 0 && ph[7].pcs_2000 == 0 && ph[7].pcs_1000 == 0, "fase 0: el blob en su sitio");
  CHECK(ph[1].pcs_2000 > 100000 && ph[1].pcs_blob == 0, "fase 1: el blob en $2000 (pagina 20 en el bloque 1)");
  CHECK(ph[3].pcs_1000 > 100000 && ph[3].pcs_blob == 0, "fase 2: el blob en $1000 (pagina 21 en el bloque 0)");
  CHECK(ph[5].pcs_07f0 > 100000 && ph[5].pcs_blob == 0 && ph[5].pcs_1000 == 0, "fase 3: el blob en $07F0, dentro de la ventana de POKEs");
  bool xs_ok = true;
  for (int b : {7, 1, 3, 5}) {
    std::vector<int>& xs = ph[b].xs;
    if (xs.size() != 100) xs_ok = false;
    for (size_t i = 0; xs_ok && i < xs.size(); i++) if (xs[i] != 32 + (int)i) xs_ok = false;
    if (ph[b].pal != 400) xs_ok = false;
  }
  CHECK(xs_ok, "en cada fase: 100 imagenes con el sprite 0 en X = 32..131 y 4 colores de la paleta por imagen");
  int want = inject ? 1 : 0;
  CHECK((int)z80.bc.w == want, inject ? "devuelve 1 (la pagina 21 ha cambiado)" : "devuelve 0");
  CHECK(rd16(0x4803) == (inject ? 1 : 0), inject ? "1 byte cambiado (PEEK 18435)" : "0 bytes cambiados (PEEK 18435)");
  int init[8] = {0, 1, 2, 3, 4, 5, 2, 3};
  CHECK(!memcmp(mapb, init, sizeof mapb), "las paginas como estaban");
  CHECK(map_bad == 0, "la pagina va igual en el dato y en B (vale en paginacion simple y completa)");
  // las copias del blob en las paginas y el resto a 0
  bool pg_ok = !memcmp(sram[20], sram[2] + (BLOB - 0x4000), BLEN) && !memcmp(sram[21] + 0x1000, sram[2] + (BLOB - 0x4000), BLEN)
               && !memcmp(sram[21] + 0x07F0, sram[2] + (BLOB - 0x4000), BLEN) && sram[20][BLEN] == 0 && sram[21][0] == 0;
  CHECK(pg_ok, "las paginas 20 y 21 tienen el blob donde toca");
  CHECK(spr0en == 0 && last2045 == 85, "al salir, sprite 0 apagado y POKE 2045,85");
  CHECK(z80.ix.w == 0x0281, "IX como estaba");
  CHECK(!bad_write, "no escribe fuera del programa, la imagen, la pila, la ventana y $E000");
  if (bad_write) printf("        primera escritura rara en $%04X\n", bad_addr);
}

int main(int argc, char** argv) {
  if (argc < 3) { printf("uso: test_z80estela0 carga|vsync|sonido|pagina fichero.bin [fichero.sym]\n"); return 2; }
  std::string t = argv[1];
  if (t == "carga") test_carga(argv[2]);
  else if (t == "vsync") test_vsync(argv[2], argv[3]);
  else if (t == "sonido") { test_sonido(argv[2], 0); test_sonido(argv[2], 1); }
  else if (t == "pagina") { test_pagina(argv[2], argv[3], false); test_pagina(argv[2], argv[3], true); }
  printf("%s (%d errores)\n", errors ? "FALLOS" : "TODO OK", errors);
  return errors ? 1 : 0;
}
