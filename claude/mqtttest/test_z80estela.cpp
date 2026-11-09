// ESTELA (EXAMPLES/ESTELA/estela.asm) en el PC: el Z80 de verdad (nucleo de EightyOne, T-estados exactos) ejecuta el juego con un modelo
// de la memoria paginada (32 paginas de 8 KB, mapeador E7h, bloque 0 = ventana de POKEs), el VSYNC (64.584 T por imagen, 8 lineas a 1,
// como lo midio VSYNC.BIN en la placa), el teclado, el borde, la paleta de 256 (2063-2065), un MCU con las ordenes 53/55/57 (ficheros de
// la SD, servidos desde EXAMPLES/ESTELA/out) y un modelo del scroll en anillo del modo 175 (mode160_emulador.md §6):
//   pixel x de la fila r de la pantalla = byte $8000 + ((r + vofs) mod hc) * 160 + x   (hofs = 0, wc = 160)
//
// FASE 8a: varios niveles y dificultad (seccion 1h): NIVEL1..5 de prueba (cortos, jefe + mejora en s = 0; los de out/ si existen), paso de nivel
//   tras el jefe, vuelta al 1 con +1 de dificultad, fallos de carga del siguiente, el 1 que desaparece (el Z80 sale con BC = 1), cifra del nivel
//   en el sprite 54 (la mejora espera), musica de respaldo, R en el nivel actual y fin de partida -> nivel 1. El modelo Game lleva nivel, vueltas
//   y dificultad (g_curlv/g_lvstore/model_load) y se compara con la memoria del Z80 en cada imagen; cuatro pruebas en negativo.
// FASE 7c: mejoras (tipos 8-10 de las oleadas: arma, escudo, bomba), el disparo doble, el escudo de 250 ticks, las bombas (B) y sus
//   iconos. El modelo Game lleva tambien todo eso. Dos partidas con niveles hechos aqui (ver 1f): una con las tres mejoras y una
//   bomba contra enemigos, y otra con el jefe y bombas; y tres pruebas en negativo con el binario cambiado a proposito.
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

static Level mklevel(const std::vector<uint8_t>& i16, const std::vector<uint8_t>& map) {
  Level L; L.i16 = i16; L.map = map; L.rows = u16(map, 6); L.mapoff = u16(map, 10); L.end = u16(map, 14); return L;
}

// ---- MCU: ordenes 53 (fopen), 55 (fread), 56 (fwrite), 57 (fclose), 71 (fcreate: crea o vacia y abre); 38 (LOOP_VGM), 34 (PLAY_VGM: longitud y nombre en ZX81, responde 0 si
// suena) y 35 (STOP_VGM), que se apuntan en mlog con sus parametros (y la respuesta de la 34 al final)
static std::map<std::string, std::vector<uint8_t>> sdfiles;
static int mcu_clk = 0;
static std::vector<uint8_t> mcu_in, mcu_out; static size_t mcu_outpos = 0;
static const std::vector<uint8_t>* openf = nullptr; static size_t fpos = 0;
static std::vector<std::string> opened, created;         // ficheros abiertos con la 53 y creados con la 71
static std::string g_vanish;                              // prueba del fallo fatal: este fichero desaparece de la SD tras las 5 primeras aperturas
static int written_n = 0;                                 // ordenes 56 recibidas
static int fopen_open = 0;
static std::vector<std::vector<int>> mlog, says;
// musica esperada (cabecera de estela.asm): mexp = las ordenes de despues de la de empezar; g_mok = la ultima PLAY_VGM fue bien
// (entonces STOP_VGM antes de la siguiente y al salir); g_musno = 0 la del nivel, 1 la del fin de partida
// record (fase 7d): ESTELA.REC = "EREC" + 3 bytes BCD. El modelo lo lee de sdfiles al empezar cada ejecucion (sin fichero o con
// otra firma, 0; lo que falta de un fichero corto cuenta como 0, como el relleno del MCU) y lo sube con cada fin de partida que
// lo supera (g_saves). g_rec0 = el fichero tal como estaba antes de la ejecucion.
static int g_rec = 0, g_saves = 0; static bool g_rec0_has = false; static std::vector<uint8_t> g_rec0;
static int bcd3(const uint8_t* b) { int v = 0; for (int i = 0; i < 3; i++) v = v * 100 + (b[i] >> 4) * 10 + (b[i] & 15); return v; }
static std::vector<uint8_t> rec_bytes(int v) { std::vector<uint8_t> r = {'E', 'R', 'E', 'C'}; int d[6]; for (int i = 5; i >= 0; i--) { d[i] = v % 10; v /= 10; } for (int i = 0; i < 3; i++) r.push_back((uint8_t)(d[2 * i] << 4 | d[2 * i + 1])); return r; }
static void rec_init() {
  g_rec = 0; g_saves = 0; g_rec0.clear(); auto it = sdfiles.find("ESTELA.REC"); g_rec0_has = it != sdfiles.end();
  if (g_rec0_has) {
    g_rec0 = it->second; uint8_t b[7] = {};
    for (size_t i = 0; i < 7 && i < g_rec0.size(); i++) b[i] = g_rec0[i];
    if (!memcmp(b, "EREC", 4)) g_rec = bcd3(b + 4);
  }
}
static void rec_gameover(int score) { if (score > g_rec) { g_rec = score; g_saves++; } }
// el veredicto sobre ESTELA.REC despues de una ejecucion: tantas ordenes 71 como subidas del modelo y el contenido exacto
static bool rec_verdict(std::string& why) {
  int ncre = 0; for (auto& n : created) if (n == "ESTELA.REC") ncre++;
  char b[160];
  if (ncre != g_saves || (int)created.size() != ncre || written_n != ncre) { snprintf(b, sizeof b, "%d creaciones y %d escrituras de ESTELA.REC, el modelo %d", ncre, written_n, g_saves); why = b; return false; }
  auto it = sdfiles.find("ESTELA.REC");
  if (g_saves > 0) {
    if (it == sdfiles.end() || it->second != rec_bytes(g_rec)) { why = "el contenido de ESTELA.REC no es EREC + el record del modelo"; return false; }
  } else if ((it != sdfiles.end()) != g_rec0_has || (g_rec0_has && it->second != g_rec0)) { why = "ESTELA.REC ha cambiado sin superar el record"; return false; }
  return true;
}
static std::vector<std::vector<int>> mexp; static bool g_mok = false; static int g_musno = 0;
static std::vector<int> playcmd(const char* nm, bool ok) {
  std::vector<int> v = {34, (int)strlen(nm)};
  for (const char* c = nm; *c; c++) v.push_back(*c >= 'A' && *c <= 'Z' ? 38 + *c - 'A' : *c >= '0' && *c <= '9' ? 28 + *c - '0' : 27);
  v.push_back(ok ? 0 : 1); return v;
}
// which 0: la del nivel lvl (NIVELn.VGM; si falta y lvl no es el 1, se prueba la del 1); 1: la del fin de partida
static void mus_go(int which, int lvl = 1) {
  std::string nm = which ? "GAMEOVER.VGM" : "NIVEL" + std::to_string(lvl) + ".VGM";
  if (g_mok) mexp.push_back({35});
  mexp.push_back({38, which ? 0 : 1});
  bool ok = sdfiles.count(nm) > 0;
  mexp.push_back(playcmd(nm.c_str(), ok)); g_mok = ok; g_musno = which;
  if (!ok && !which && lvl != 1) { ok = sdfiles.count("NIVEL1.VGM") > 0; mexp.push_back(playcmd("NIVEL1.VGM", ok)); g_mok = ok; }
}       // says: ordenes 23 (voz): longitud y texto en ZX81
static void mcu_byte(uint8_t b) {
  mcu_in.push_back(b);
  int c = mcu_in[0];
  if (c == 53) {
    if (mcu_in.size() >= 2 && (int)mcu_in.size() == 2 + mcu_in[1]) {
      std::string nm(mcu_in.begin() + 2, mcu_in.end()); opened.push_back(nm);
      if (!g_vanish.empty() && opened.size() == 5) sdfiles.erase(g_vanish);
      auto it = sdfiles.find(nm);
      if (it == sdfiles.end() || openf) mcu_out.push_back(0xFF); else { openf = &it->second; fpos = 0; fopen_open++; mcu_out.push_back(0); }
      mcu_in.clear();
    }
  } else if (c == 71) {                                   // fcreate (ASCII): crea o vacia el fichero y lo abre; responde con el manejador
    if (mcu_in.size() >= 2 && (int)mcu_in.size() == 2 + mcu_in[1]) {
      std::string nm(mcu_in.begin() + 2, mcu_in.end()); created.push_back(nm);
      if (openf) mcu_out.push_back(0xFF);
      else { sdfiles[nm].clear(); openf = &sdfiles[nm]; fpos = 0; fopen_open++; mcu_out.push_back(0); }
      mcu_in.clear();
    }
  } else if (c == 56) {                                   // fwrite: manejador, cuantos (2) y los bytes -> estado
    if (mcu_in.size() >= 4 && (int)mcu_in.size() == 4 + (mcu_in[2] | (mcu_in[3] << 8))) {
      if (!openf || mcu_in[1] != 0) { printf("        fwrite sin fichero abierto\n"); errors++; mcu_out.push_back(0xFF); }
      else {
        std::vector<uint8_t>* w = const_cast<std::vector<uint8_t>*>(openf);
        for (size_t i = 4; i < mcu_in.size(); i++) { if (fpos < w->size()) (*w)[fpos] = mcu_in[i]; else w->push_back(mcu_in[i]); fpos++; }
        written_n++; mcu_out.push_back(0);
      }
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
  } else if (c == 38) {
    if (mcu_in.size() == 2) { mlog.push_back(std::vector<int>(mcu_in.begin(), mcu_in.end())); mcu_in.clear(); }
  } else if (c == 35) {
    mlog.push_back({35}); mcu_in.clear();
  } else if (c == 23) {
    if (mcu_in.size() >= 2 && (int)mcu_in.size() == 2 + mcu_in[1]) { says.push_back(std::vector<int>(mcu_in.begin(), mcu_in.end())); mcu_out.push_back(0); mcu_in.clear(); }
  } else if (c == 34) {
    if (mcu_in.size() >= 2 && (int)mcu_in.size() == 2 + mcu_in[1]) {
      std::vector<int> v(mcu_in.begin(), mcu_in.end());
      std::string nm;
      for (size_t i = 2; i < mcu_in.size(); i++) { int z = mcu_in[i]; nm += z >= 38 ? (char)('A' + z - 38) : z >= 28 ? (char)('0' + z - 28) : '.'; }
      int st = sdfiles.count(nm) ? 0 : 1;
      v.push_back(st); mlog.push_back(v); mcu_out.push_back((uint8_t)st); mcu_in.clear();
    }
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
static int code_hi = 0x4800, buf_lo = 0, buf_hi = 0, lvlpg = -1;
static long ring_vis_writes = 0; static bool checking_vis = false, in_restart = false, restart_seen = false;
static uint8_t keyrows[8];
static int map_bad = 0;
static int aysel[2], ayreg[2][16];                       // los dos AY de la FPGA: A ($CF/$0F) y B ($C7/$07)
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
  bool ok = (a >= 0x8000 && a < 0x8000 + 102 * 160) || (a >= 0x4800 && a < code_hi) || (a >= buf_lo && a < buf_hi) || (a >= 0x4700 && a < 0x4800) || a >= 0xC000 ||
            (a >= 0x2000 && a < 0x4000 && (mapb[1] == lvlpg || rd(a) == (uint8_t)d));   // el nivel; con la ROM, los mismos bytes
  if (!ok && !bad_write) { bad_write = true; bad_addr = (uint16_t)a; printf("        escritura rara en $%04X, imagen %ld, PC=%04X SP=%04X\n", a, frame(), z80.pc.w, z80.sp.w); }
  wr_raw(a, (uint8_t)d);
}
void z80_writeport_wrapper(int port, int d, int*) {
  int lo = port & 0xFF, hi = (port >> 8) & 0xFF;
  if (lo == 0xEF && hi == 0x7F) { int c = d & 0x2F; if (borders.empty() || borders.back() != c) borders.push_back(c); return; }
  if (lo == 0xE7) { int blk = d & 7, pg = (d >> 3) & 31; if (pg != (hi & 31)) map_bad++; mapb[blk] = pg; return; }
  if (lo == 0xA7) { mcu_byte((uint8_t)d); mcu_clk ^= 1; return; }
  if (lo == 0xCF || lo == 0xC7) { aysel[lo == 0xC7] = d & 15; return; }
  if (lo == 0x0F || lo == 0x07) { ayreg[lo == 0x07][aysel[lo == 0x07]] = d; return; }
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
// titulo (fase 7d): el sdfile TITULO.I16 de la prueba; title_seen = pasado por la espera de teclas del titulo (tlkey) y title_bad = lo que
// no cuadraba entonces (-1 = no ha salido); rec_start_bad = el record de la memoria del Z80 al llegar a main
static const std::vector<uint8_t>* g_title = nullptr;     // TITULO.I16 de sdfiles si es un .I16 (si no, no hay titulo)
static int title_check();
static bool title_seen = false; static int title_bad = -1, rec_start_bad = 0, title_frame = 10;
static int init[8] = {0, 1, 2, 3, 4, 5, 2, 3};
static uint8_t basic_snap[0x8000];

struct Result { int bc; long frames; int vis_ok, vis_bad; long maxlat, maxloop; std::vector<int> pal_bad; std::vector<int> spr_bad; int fired, expired, spr_checked, maxb, minx, maxx, miny, maxy; int blk_bad, p16_bad;
                int spawned, kills, deaths, drope, dropw, ebfired, maxene, maxline, over12, score, waves, score_shown;
                // reparto por linea como el motor: lineas de sprite perdidas, por clase (0 enemigos, 1 disparos, 2 disparos enemigos,
                // 3 nave y llama, 4 marcador); imagenes con alguna perdida; racha mas larga de un enemigo con lineas perdidas
                long lost_lines, lost_cls[5]; int lost_frames, first_lost, consec[8], max_consec, ene_lost_frames;
                int ay_bad, gameovers, goresets, cartel;
                int bwarn, benter, bkills, bhits, bshots, laps, bdeaths, bmaxhp, inftoggles;
                int items[3], itemmiss, dblvolleys, bombuse, bombkills, bombboss, bclamp, weaplost, shielded; int hofs_bad, hofs_nz, bhp_bad, bhp_min, vis_shk; std::string lostinfo;
                // fase 8a: cambios de nivel (imagen, nivel al que se pasa), partidas nuevas con ESPACIO, salidas por fallo fatal y imagenes en que
                // la memoria del Z80 (nivel, vueltas, dificultad, velocidad) no es la del modelo
                std::vector<std::pair<long, int>> lvchg; int newgames, fatals, dif_bad; };

// ---- el juego, escrito aqui con la especificacion (cabecera de estela.asm y juego160.md §8.3), no con el codigo del Z80.
// Nave: x 16-128, y 0-76, 1 px por imagen con O P Q A; inclinacion = dx + 1. Disparos: 8 huecos, suben 3 px por tick, se borran
// con y < -7; con ESPACIO y la espera a 0 sale uno en (x + 4, y - 6) en el primer hueco y la espera pasa a 6; la espera baja 1 por
// tick y se puede disparar en el tick en que llega a 0. Nada de eso mientras la nave explota (salvo que los disparos suben).
// Enemigos, disparos enemigos, colisiones, oleadas y explosiones: ver la cabecera de estela.asm.
static std::vector<uint8_t> g_map;                       // NIVEL1.MAP
static std::vector<std::vector<int>> g_paths;            // PATHTAB de LEVEL.INC: pares dx, dy
static bool g_immortal = false;                          // caso peor del reparto: sin colisiones ni disparos enemigos
static const int EFT0[6] = {96, 0, 80, 72, 36, 60}, PTS[6] = {10, 20, 30, 40, 60, 50};
// Varios niveles (fase 8a). g_curlv = el nivel que se juega (lo visible y la paleta se comparan con el); g_lvstore[n] = el nivel n tal como
// lo ha cargado el modelo de la SD; g_exp_open = los ficheros que el modelo espera que el Z80 abra al cambiar de nivel (en orden);
// g_difs = (nivel, vuelta, vidas del jefe, ticks entre salvas, cadencia del tipo 0) en cada comienzo de nivel; g_fatal = el modelo
// espera que el Z80 salga (ni el siguiente nivel ni el 1 se cargan)
static Level g_lvstore[6];
static const Level* g_curlv = nullptr;
static std::vector<std::string> g_exp_open;
static std::vector<std::vector<int>> g_difs;
static bool g_fatal = false, g_chk_speed = false;
struct Ene { int st, typ, x, y, path, k, flg, tmr, ft, w; };      // w: la oleada (para el caso peor del reparto)
struct EB { int a, x, y, dx; };
struct Spn { int rem, w, t; };
struct Game { int x, y, tilt, cd, tick; int ba[8], bx[8], by[8]; int pstate, ptimer, score; Ene e[8]; EB b[7]; Spn sp[4]; int w;
             int fxp[4], fxpr[4]; int lives;
             int bpend, bst, btmr, jx, jy, bk, bhp, bdir, bft, bflash, bwx, bpath;
             int ityp, itx, ity, ik, ipath, weap, bombs, prevb, stimer, shk, hofv;
             int lvl, laps, lvt, bhp0, bft0, eft[6]; };   // fase 8a: nivel (1-5), vueltas, ticks de la cifra del nivel, dificultad (vidas del jefe, ticks entre salvas, cadencias)   // mejoras (7c): ityp 0 ninguna, 1 arma, 2 escudo, 3 bomba; prevb = B en bruto (0 = pulsada)   // el jefe (cabecera de estela.asm)      // efectos: siguiente par de cada voz (0 = libre) y su prioridad
// Efectos (cabecera de estela.asm): 4 voces; cada una escribe en cada imagen un par (valor, volumen) de su tabla en sus
// registros: 0 disparo (AY A, R4 y R10), 1 disparo enemigo por la izquierda (AY A, R0 y R8), 2 por la derecha (AY A, R2 y
// R9), 3 explosiones (AY B, ruido R6 y R10). Volumen $FF = fin: volumen 0 y libre. Un efecto entra si la voz esta libre o el
// suyo tiene tanta o mas prioridad (primer byte de la tabla). Las tablas se leen de la memoria del Z80.
static const int FXD[4][3] = { {0, 4, 10}, {0, 0, 8}, {0, 2, 9}, {1, 6, 10} };
static int ayexp[2][16];
static long fx_started[8];                                // disparo, enemigo izquierda, derecha, explosion, nave, impacto en el jefe
static void fxgo(Game& g, int v, const char* tab, int k) {
  int t = S(tab), pr = rd(t);
  if (g.fxp[v] == 0 || pr >= g.fxpr[v]) { g.fxp[v] = t + 1; g.fxpr[v] = pr; }
  fx_started[k]++;
}
static void fxstep(Game& g) {
  for (int v = 0; v < 4; v++) if (g.fxp[v]) {
    int val = rd(g.fxp[v]), vol = rd(g.fxp[v] + 1); g.fxp[v] += 2;
    if (vol == 0xFF) { ayexp[FXD[v][0]][FXD[v][2]] = 0; g.fxp[v] = 0; }
    else { ayexp[FXD[v][0]][FXD[v][1]] = val; ayexp[FXD[v][0]][FXD[v][2]] = vol; }
  }
}
static void ay_model_setup() {                            // aysetup: tonos de A, ruido C de B, volumenes 0 (los periodos quedan)
  ayexp[0][7] = 0x38; ayexp[0][1] = ayexp[0][3] = ayexp[0][5] = 0; ayexp[1][7] = 0x1F;
  for (int c = 0; c < 2; c++) ayexp[c][8] = ayexp[c][9] = ayexp[c][10] = 0;
}
static bool ay_match() {
  static const int ra[] = {0, 1, 2, 3, 4, 5, 7, 8, 9, 10}, rb[] = {6, 7, 8, 9, 10};
  for (int r : ra) if (ayreg[0][r] != ayexp[0][r]) return false;
  for (int r : rb) if (ayreg[1][r] != ayexp[1][r]) return false;
  return true;
}
static int woff() { return u16(g_map, 8); }
static void ginit(Game& g) {
  g.x = 72; g.y = 76; g.tilt = 1; g.cd = 0; g.tick = 0; g.pstate = 0; g.ptimer = 0; g.score = 0;
  for (int i = 0; i < 8; i++) { g.ba[i] = 0; g.e[i] = Ene{}; }
  for (int i = 0; i < 7; i++) g.b[i] = EB{};
  for (int i = 0; i < 4; i++) { g.sp[i] = Spn{}; g.fxp[i] = g.fxpr[i] = 0; }
  g.w = woff();
  g.lives = S("LIVES0");
  g.bpend = g.bst = g.btmr = g.jx = g.jy = g.bk = g.bhp = g.bdir = g.bft = g.bflash = g.bwx = g.bpath = 0;
  g.ityp = g.itx = g.ity = g.ik = g.ipath = g.weap = g.bombs = g.prevb = g.stimer = g.shk = g.hofv = 0;
  // el principio de un nivel (lvl y laps ya puestos): la cifra del nivel 100 ticks y la dificultad d = (nivel - 1) + 5 * vueltas:
  // jefe con 40 + 10 d vidas y una salva cada max(15, 30 - 3 d) ticks; cadencia de los enemigos: la base con v = v - v / 8, d veces, y como poco la mitad
  g.lvt = S("LVLT0");
  int d = (g.lvl - 1) + 5 * g.laps;
  g.bhp0 = S("BHP0") + 10 * d; g.bft0 = std::max(15, S("BFT0") - 3 * d);
  for (int i = 0; i < 6; i++) { int v = EFT0[i]; for (int k = 0; k < d; k++) v = std::max(v - v / 8, EFT0[i] / 2); g.eft[i] = v; }
  g_difs.push_back({g.lvl, g.laps, g.bhp0, g.bft0, g.eft[0]});
}
// cargar el nivel n como loadlvl (errores 1-6): apunta en g_exp_open los ficheros que el Z80 abre y, si vale, lo deja en g_lvstore[n]
static bool model_load(int n) {
  std::string a = "NIVEL" + std::to_string(n) + ".I16", b = "NIVEL" + std::to_string(n) + ".MAP";
  g_exp_open.push_back(a);
  auto ia = sdfiles.find(a); if (ia == sdfiles.end()) return false;
  if (ia->second.size() < 4 || memcmp(&ia->second[0], "I160", 4)) return false;
  g_exp_open.push_back(b);
  auto ib = sdfiles.find(b); if (ib == sdfiles.end()) return false;
  const std::vector<uint8_t>& m = ib->second;
  if (m.size() < 16 || memcmp(&m[0], "ELV1", 4)) return false;
  int len = u16(m, 12), rows = u16(m, 6), mo = u16(m, 10);
  if (len > S("MAXLVL") || len < 16 || rows < 7 || mo + 8 * rows > len) return false;
  Level& L = g_lvstore[n]; L.i16 = ia->second; L.map = m; L.rows = rows; L.mapoff = mo; L.end = u16(m, 14);
  return true;
}
static bool keydown(const std::vector<Key>& sc, long f, int row, int bit) { for (auto& k : sc) if (f >= k.from && f < k.to && k.row == row && k.bit == bit) return true; return false; }
static bool ovl(int b, int c, int d, int e) { return !((e & 255) < (b & 255)) && !((c & 255) < (d & 255)); }
static void kill(Game& g, Ene& e, Result& R) { e.st = 2; e.tmr = 16; g.score = (g.score + PTS[e.typ]) % 1000000; R.kills++; fxgo(g, 3, "fxboom", 3); }
static bool g_inf = false;                                // vidas infinitas (tecla I)
static void pdie(Game& g, Result& R) { if (g.weap) R.weaplost++; g.weap = 0; g.pstate = 1; g.ptimer = 16; R.deaths++; if (!g_inf) g.lives--; fxgo(g, 3, "fxdie", 4); }
static void spawn(Game& g, int w, int x, Result& R) {
  const uint8_t* r = &g_map[w];
  for (int i = 0; i < (g.bpend ? 5 : 8); i++) if (g.e[i].st == 0) {
    Ene& e = g.e[i]; e.st = 1; e.typ = r[2]; e.x = (x + 32) & 255; e.y = 16; e.tmr = 0; e.flg = r[7]; e.ft = g.eft[e.typ]; e.path = r[4]; e.k = 0; e.w = w;
    R.spawned++; return;
  }
  R.drope++;
}
// un disparo enemigo desde (X, Y) de hardware: sale en (X+4, Y+12), en el primer hueco; dx hacia la nave si esta a 16 o mas
static void ebfire_m(Game& g, int ex, int ey, Result& R) {
  for (auto& b : g.b) if (!b.a) {
    b.a = 1; b.x = (ex + 4) & 255; b.y = (ey + 12) & 255;
    int ec = (ex + 8) & 255, pc = g.x + 40;
    b.dx = pc >= ec + 16 ? 1 : (pc + 16 <= ec ? -1 : 0);
    R.ebfired++;
    if (ec < 112) fxgo(g, 1, "fxebs", 1); else fxgo(g, 2, "fxebs", 2);
    return;
  }
}
// el jefe recibe un disparo
static void bdmg(Game& g, Result& R) {
  g.bflash = 6; R.bhits++;
  if (--g.bhp == 0) { g.bst = 4; g.btmr = S("BEXT"); g.score = (g.score + 1000) % 1000000; R.bkills++; fxgo(g, 3, "fxdie", 4); }
  else fxgo(g, 3, "fxhit", 5);
}
// los disparos de la nave y la nave contra el jefe ([X+2,X+29] x [Y+2,Y+21])
static void bosshit(Game& g, Result& R) {
  for (int i = 0; i < 8; i++) if (g.ba[i]) {
    int cbx = (g.bx[i] + 35) & 255, cby = (g.by[i] + 32) & 255;
    if (ovl(cbx, cbx + 1, g.jx + 2, g.jx + 29) && ovl(cby, cby + 7, g.jy + 2, g.jy + 21)) { g.ba[i] = 0; bdmg(g, R); }
    if (g.bst == 4) return;
  }
  if (g.pstate == 0) {
    int cpx = g.x + 36, cpy = g.y + 35;
    if (ovl(cpx, cpx + 7, g.jx + 2, g.jx + 29) && ovl(cpy, cpy + 10, g.jy + 2, g.jy + 21)) { pdie(g, R); R.bdeaths++; }
  }
}
static void gupdate(Game& g, const std::vector<Key>& sc, long f, int scr, int nstp, Result& R) {
  g.tick = (g.tick + 1) & 255;
  if (g.stimer) g.stimer--;                              // el marcador de la mejora recogida (SCT ticks)
  if (g.lvt) g.lvt--;                                    // la cifra del nivel
  if (g.pstate != 1 && g.pstate != 3) {                  // explotando o fin de partida: ni se mueve ni dispara
    int dx = (keydown(sc, f, 5, 0) ? 1 : 0) - (keydown(sc, f, 5, 1) ? 1 : 0);
    g.tilt = dx + 1; g.x = std::min(128, std::max(16, g.x + dx));
    int dy = (keydown(sc, f, 1, 0) ? 1 : 0) - (keydown(sc, f, 2, 0) ? 1 : 0);
    g.y = std::min(76, std::max(0, g.y + dy));
  }
  for (int i = 0; i < 8; i++) if (g.ba[i]) { g.by[i] -= 3; if (g.by[i] < -7) { g.ba[i] = 0; R.expired++; } }
  if (g.pstate != 1 && g.pstate != 3) {
    bool fire = true;
    if (g.cd > 0) { g.cd--; if (g.cd > 0) fire = false; }
    if (fire && keydown(sc, f, 7, 0)) {                 // con el arma, dos disparos: x + 0 en el primer hueco y x + 8 en el siguiente libre
      int first = -1, second = -1;
      for (int i = 0; i < 8; i++) if (!g.ba[i]) { first = i; break; }
      if (first >= 0) {
        if (g.weap) for (int i = first + 1; i < 8; i++) if (!g.ba[i]) { second = i; break; }
        g.ba[first] = 1; g.bx[first] = g.x + (g.weap ? 0 : 4); g.by[first] = g.y - 6; R.fired++;
        if (second >= 0) { g.ba[second] = 1; g.bx[second] = g.x + 8; g.by[second] = g.y - 6; R.fired++; R.dblvolleys++; }
        g.cd = 6; fxgo(g, 0, "fxshot", 0);
      }
    }
    // bomba: B (fila 7, bit 4), pulsacion nueva y con alguna bomba
    int raw = keydown(sc, f, 7, 4) ? 0 : 16;
    bool newp = raw == 0 && g.prevb != 0; g.prevb = raw;
    if (newp && g.bombs > 0) {
      g.bombs--; R.bombuse++;
      for (auto& e : g.e) if (e.st == 1) { kill(g, e, R); R.bombkills++; }
      if (g.bst == 2 || g.bst == 3) {                    // el jefe pierde 10 vidas sin bajar de 1
        int nh = g.bhp - 10; if (nh <= 0) { nh = 1; R.bclamp++; }
        g.bhp = nh; g.bflash = 6; R.bombboss++;
      }
      g.shk = 24;                                        // la pantalla tiembla 24 imagenes
      fxgo(g, 3, "fxbomb", 7);                           // ruido grave largo, prioridad 3
    }
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
  // la mejora: un paso de su trayectoria (el ultimo se repite); sale por abajo (Y >= 132); con la nave viva o parpadeando, si toca
  // su caja [X+1,X+6] x [Y+1,Y+6] con la de la nave, se recoge
  if (g.ityp && !g.lvt) {                                // (con la cifra del nivel a la vista, la mejora espera)
    const std::vector<int>& p = g_paths[g.ipath]; int n = (int)p.size() / 2;
    g.itx = (g.itx + p[2 * g.ik]) & 255; g.ity = (g.ity + p[2 * g.ik + 1]) & 255;
    if (g.ik < n - 1) g.ik++;
    if (g.ity >= 132) { g.ityp = 0; R.itemmiss++; }
    else if (!(g.pstate & 1)) {
      int cpx = g.x + 36, cpy = g.y + 35;
      if (ovl(cpx, cpx + 7, g.itx + 1, g.itx + 6) && ovl(cpy, cpy + 10, g.ity + 1, g.ity + 6)) {
        int t = g.ityp; g.ityp = 0; g.stimer = S("SCT"); R.items[t - 1]++; fxgo(g, 0, "fxitem", 6);
        if (t == 1) g.weap = 1;
        else if (t == 2) { g.pstate = 2; g.ptimer = S("SHLDT"); R.shielded++; }
        else if (g.bombs < 2) g.bombs++;
      }
    }
  }
  // disparos de los enemigos
  if (!g_immortal)
  for (auto& e : g.e) if (e.st == 1 && e.ft != 0) {
    if (--e.ft != 0) continue;
    e.ft = g.eft[e.typ];
    if (e.y >= 32 && e.y < 93 && g.pstate != 1 && g.pstate != 3) ebfire_m(g, e.x, e.y, R);
  }
  for (auto& b : g.b) if (b.a && b.dx == 255) b.dx = -1;
  // oleadas
  while (scr >= u16(g_map, g.w)) {
    int w = g.w; g.w += 8; R.waves++;
    const uint8_t* r = &g_map[w];
    if (r[2] == 6) { g.bwx = r[3]; g.bpath = r[4]; g.bpend = 1; continue; }   // el jefe
    if (r[2] > 6) {                                                            // mejoras (tipos 8-10): una a la vez, en (WAVE_X, -8)
      if (r[2] >= 8 && !g.ityp) { g.ityp = r[2] - 7; g.itx = (r[3] + 32) & 255; g.ity = 24; g.ik = 0; g.ipath = r[4]; }
      continue;
    }
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
  // el jefe
  if (!g_immortal) {
    if (g.bflash) g.bflash--;
    switch (g.bst) {
      case 0: if (g.bpend && !g.e[5].st && !g.e[6].st && !g.e[7].st) { g.bst = 1; g.btmr = S("WARNT"); R.bwarn++; } break;
      case 1: if (--g.btmr == 0) { g.bst = 2; g.jx = (g.bwx + 32) & 255; g.jy = 8; g.bhp = g.bhp0; g.bk = 0; R.benter++; } break;
      case 2: {
        const std::vector<int>& p = g_paths[g.bpath]; int n = (int)p.size() / 2;
        g.jx = (g.jx + p[2 * g.bk]) & 255; g.jy = (g.jy + p[2 * g.bk + 1]) & 255;
        if (g.bk < n - 1) g.bk++; else { g.bst = 3; g.bdir = 1; g.bft = g.bft0; }
        bosshit(g, R); break;
      }
      case 3:
        g.jx = (g.jx + g.bdir) & 255;
        if (g.jx == S("BXMIN") || g.jx == S("BXMAX")) g.bdir = -g.bdir;
        if (--g.bft == 0) {
          g.bft = g.bft0;
          if (g.pstate != 1 && g.pstate != 3) { ebfire_m(g, g.jx, g.jy + 8, R); ebfire_m(g, g.jx + 16, g.jy + 8, R); R.bshots++; }
        }
        bosshit(g, R); break;
      case 4: if (--g.btmr == 0) { g.bst = 5; g.btmr = S("CLEART"); } break;
      case 5:
        if (--g.btmr == 0) {                            // lap: restart con el mismo marcador y las mismas vidas
          // tras el jefe: el nivel siguiente (si existe y vale); tras el 5, o si no, el 1 con una vuelta mas (hasta LAPMAX); si ni el 1 se carga, el Z80 sale
          int sc = g.score, lv = g.lives, n = g.lvl + 1; bool ok = false;
          if (n <= 5) ok = model_load(n);
          if (!ok) { if (g.laps < S("LAPMAX")) g.laps++; n = 1; ok = model_load(1); }
          if (!ok) { g_fatal = true; R.fatals++; break; }
          g.lvl = n; g_curlv = &g_lvstore[n]; g_map = g_lvstore[n].map; R.lvchg.push_back({frame(), n});
          mus_go(0, n);
          ginit(g); ay_model_setup(); g.score = sc; g.lives = lv; R.laps++;
        }
        break;
    }
  }
  // la nave
  if (g.pstate == 3) { if (g.ptimer) g.ptimer--; }      // fin de partida: la espera baja hasta 0
  else if (g.pstate && --g.ptimer == 0) {
    if (g.pstate == 1) {
      if (g.lives == 0) { g.pstate = 3; g.ptimer = S("GOT"); g.shk = 0; R.gameovers++; mus_go(1); rec_gameover(g.score); }
      else { g.x = 72; g.y = 76; g.tilt = 1; g.pstate = 2; g.ptimer = 100; }
    } else g.pstate = 0;
  }
  // el desplazamiento horizontal de la imagen siguiente (se escribe en el VSYNC junto a POKE 2072)
  static const int SHK[24] = {3, -3, 3, -3, 3, -3, 2, -2, 2, -2, 2, -2, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, 0};
  if (g.shk > 0) { g.hofv = (SHK[24 - g.shk] + 160) % 160; g.shk--; } else g.hofv = 0;
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
  if (g.pstate == 3) {                                  // cartel GAME OVER: GAME en y 40, OVER en y 50, x 64-95
    int L0 = S("LETRAS");                               // G A M E O V R
    static const int idx[8] = {50, 51, 52, 53, 49, 54, 32, 33}, let[8] = {0, 1, 2, 3, 4, 5, 3, 6};
    for (int i = 0; i < 8; i++) chk(idx[i], 32 + 64 + 8 * (i & 3), 32 + 40 + 10 * (i >> 2), L0 + let[i]);
    R.cartel++;
  }
  if (!blink && g.pstate != 3) {
    chk4(50, g.x + 32, g.y + 32, g.pstate == 1 ? EXPL + ((16 - g.ptimer) & 12) : NAVE + g.tilt * 4);
    if (g.pstate != 1) chk(49, g.x + 36, g.y + 48, LLAMA + ((g.tick >> 2) & 1));
  }
  for (int i = 0; i < 8; i++) if (g.ba[i]) chk(34 + i, g.bx[i] + 32, g.by[i] + 32, BALA);
  if (g.pstate != 3) {                                  // sprite 54 (en el fin de partida, la V del cartel): la cifra del nivel (x 76, y 46) o la mejora
    if (g.lvt) chk(54, 32 + 76, 32 + 46, S("CIFRA") + g.lvl);
    else if (g.ityp) chk(54, g.itx, g.ity, S("ITEM1") + g.ityp - 1);
  }
  int ne = 0;
  for (int i = 0; i < (g.bst ? 5 : 8); i++) {            // con jefe, sus sprites son los de los huecos 5-7
    const Ene& e = g.e[i];
    if (e.st == 1) { chk4(4 * i, e.x, e.y, ETAB[e.typ] + ((g.tick & 8) >> 1)); ne++; }
    else if (e.st == 2) { chk4(4 * i, e.x, e.y, EXPL + ((16 - e.tmr) & 12)); ne++; }
  }
  R.maxene = std::max(R.maxene, ne);
  for (int i = 0; i < 7; i++) if (g.b[i].a) chk(42 + i, g.b[i].x, g.b[i].y, BALAE);
  if (g.bst) {                                            // el jefe (sprites 20-31)
    int L0 = S("LETRAS"), L2 = S("LETRAS2"), JEFE = S("JEFE");
    if (g.bst == 1) {                                     // WARNING parpadeando (bit 4 de btmr): x 52-107, y 40
      int pg[7] = {L2, L0 + 1, L0 + 6, L2 + 1, L2 + 2, L2 + 1, L0};
      if (g.btmr & 16) for (int i = 0; i < 7; i++) chk(24 + i, 32 + 52 + 8 * i, 32 + 40, pg[i]);
    } else if (g.bst <= 3) {                              // 12 sprites, 2 fases cada 8 ticks; apagado con el bit 1 de bflash
      int ph = (g.tick & 8) ? 12 : 0;
      if (!(g.bflash & 2)) for (int r = 0; r < 3; r++) for (int c = 0; c < 4; c++) chk(20 + r * 4 + c, g.jx + 8 * c, g.jy + 8 * r, JEFE + ph + r * 4 + c);
    } else if (g.bst == 4) {                              // dos explosiones
      int p1 = ((48 - g.btmr) >> 1) & 12, p2 = (p1 + 8) & 12;
      chk4(20, g.jx, g.jy, EXPL + p1); chk4(24, g.jx + 16, g.jy + 8, EXPL + p2);
    } else {                                              // WELL DONE, como GAME OVER
      int pg[8] = {L2, L0 + 3, L2 + 3, L2 + 3, L2 + 4, L0 + 4, L2 + 1, L0 + 3};
      for (int i = 0; i < 8; i++) chk(20 + i, 32 + 64 + 8 * (i & 3), 32 + 40 + 10 * (i >> 2), pg[i]);
    }
  }
  // marcador: una fila de 6 cifras en x 56-103, y 2, solo con la nave explotando o parpadeando o al final del nivel
  if (g.pstate != 0 || g.stimer || scr >= u16(g_map, 14)) {
    int dig[6] = { g.score / 100000, g.score / 10000 % 10, g.score / 1000 % 10, g.score / 100 % 10, g.score / 10 % 10, g.score % 10 };
    for (int i = 0; i < 6; i++) chk(58 + i, 32 + 56 + 8 * i, 32 + 2, CIFRA + dig[i]);
    for (int i = 0; i < 3 && i < g.lives; i++) chk(55 + i, 32 + 68 + 8 * i, 32 + 12, S("VIDA"));   // vidas: x 68-91, y 12
    if (g.pstate != 3) for (int i = 0; i < 2 && i < g.bombs; i++) chk(32 + i, 32 + 100 + 8 * i, 32 + 12, S("BOMBA"));  // bombas: x 100-115, y 12 (32-33: en el fin de partida, el cartel)
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
        if (R.lostinfo.empty()) {                         // la primera perdida: fila y sprites que la cruzan
          char b[64]; snprintf(b, sizeof b, "fila %d, pierde el %d, cruzan:", row, n); R.lostinfo = b;
          for (int q = 63; q >= 0; q--) if (spr[q].en && (((row + 32) - spr[q].y) & 511) < 8) { snprintf(b, sizeof b, " %d", q); R.lostinfo += b; }
        }
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

// El titulo, visto en su primera espera de teclas (tlkey): modo 175 con el anillo de 102 sin desplazar, la paleta de 256 y los
// pixeles de TITULO.I16, y el record en las 6 cifras de los sprites 58-63 (x 56-103, y RECY) y nada mas encendido. Devuelve cuantas
// cosas no cuadran (0 = bien); sin TITULO.I16 valido no tendria que llegar aqui.
static int title_check() {
  if (!g_title) return 1000;
  const std::vector<uint8_t>& t = *g_title; int bad = 0;
  if (mode != 175 || vring != 102 || vofs != 0 || hofs != 0) bad++;
  for (int i = 0; i < 256; i++) if (pal[i] != (uint16_t)(t[4 + 2 * i] | (t[5 + 2 * i] << 8))) bad++;
  int pb = 0;
  for (int r = 0; r < 100; r++) for (int x = 0; x < 160; x++) if (rd(0x8000 + r * 160 + x) != t[4 + 512 + r * 160 + x]) pb++;
  if (pb) printf("        titulo: %d pixeles distintos\n", pb);
  bad += pb ? 1 : 0;
  std::vector<uint8_t> rb = rec_bytes(g_rec); int dig[6];
  for (int i = 0; i < 3; i++) { dig[2 * i] = rb[4 + i] >> 4; dig[2 * i + 1] = rb[4 + i] & 15; }
  for (int n = 0; n < 64; n++) {
    const Spr& q = spr[n];
    if (n >= 58) {
      bool m = true; for (int r = 0; r < 8; r++) if (q.mask[r] != 0xFF) m = false;
      if (!(q.en == 1 && (q.xl | ((q.xh & 1) << 8)) == 32 + 56 + 8 * (n - 58) && q.y == 32 + S("RECY") && q.page == S("CIFRA") + dig[n - 58] && m)) bad++;
    } else if (q.en) bad++;
  }
  return bad;
}

// una ejecucion: hasta que vuelve o hasta maxf imagenes
static Result run(const Level& lv, const std::vector<Key>& script, long maxf, int nchecks, bool check_pal) {
  Result R{}; R.bc = -1; R.bhp_min = 255;
  memset(sram, 0, sizeof sram); memcpy(mapb, init, sizeof mapb);
  for (int i = 0; i < 8192; i++) sram[1][i] = (uint8_t)(i * 13 + 5 + (i >> 9));       // "ROM" del bloque 1, para ver si se toca
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
  rec_init(); title_seen = false; title_bad = -1; rec_start_bad = 0;
  Game g{}; g.lvl = 1; g.laps = 0; g_difs.clear(); g_exp_open.clear(); g_fatal = false; g_curlv = &lv; ginit(g); size_t nspr = 0; bool prev_r = false, prev_sp = false, prev_i = false; g_inf = false;
  R.minx = R.miny = 999; R.maxx = R.maxy = -999;
  mcu_in.clear(); mcu_out.clear(); mcu_outpos = 0; mcu_clk = 0; openf = nullptr; opened.clear(); created.clear(); written_n = 0; fopen_open = 0; mlog.clear(); says.clear(); mexp.clear(); g_musno = 0; g_mok = sdfiles.count("NIVEL1.VGM") > 0;
  for (auto& c : ayreg) for (int& r : c) r = 0xAA; memset(ayexp, 0, sizeof ayexp); ay_model_setup(); memset(fx_started, 0, sizeof fx_started);
  for (int c = 0; c < 2; c++) for (int r = 0; r < 16; r++) if (r == 0 || r == 2 || r == 4 || r == 6) ayexp[c][r] = 0xAA;   // periodos: los de antes
  z80_init(); z80_reset();
  z80.sp.w = 0x47F0; wr_raw(0x47F0, RET_AT & 255); wr_raw(0x47F1, RET_AT >> 8);
  z80.pc.w = 0x4800; z80.halted = 0; z80.iff1 = z80.iff2 = 0;
  z80.ix.w = 0x0281; z80.iy.w = 0x4000;
  int TLKEY = S("tlkey");
  g_title = nullptr;
  { auto it = sdfiles.find("TITULO.I16"); if (it != sdfiles.end() && it->second.size() == 16516 && !memcmp(&it->second[0], "I160", 4)) g_title = &it->second; }
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
    if (z80.pc.w == TLKEY && !title_seen) { title_seen = true; title_bad = title_check(); }
    if (z80.pc.w == MAIN && !in_main) {
      in_main = true; checking_vis = true; shown_valid = false; calls = 0;
      std::vector<uint8_t> rb = rec_bytes(g_rec); for (int i = 0; i < 3; i++) if (rd(S("rec") + i) != rb[4 + i]) rec_start_bad++;   // el record leido de ESTELA.REC
    }
    if (z80.pc.w == RESTART) { in_restart = true; restart_seen = true; calls = -1; }
    if (z80.pc.w == draw_lo) in_draw = true;
    if (z80.pc.w == CYCLE) in_draw = false;
    if (in_main && z80.pc.w == WAITVS && frame_t0 && !restart_seen) { long lp = (long)(total_t - frame_t0); if (lp > R.maxloop) R.maxloop = lp; }
    if (in_main && z80.pc.w == WAITVS && vwrites.size() > nspr) {
      // acaba una vuelta: lo que se ha visto en esta imagen es el estado de antes de update; luego update con las teclas de la imagen
      nspr = vwrites.size(); long f = vwrites.back().f;
      if (hofs != g.hofv) R.hofs_bad++;                  // POKE 2074: el temblor de la bomba, en cada imagen
      if (hofs) R.hofs_nz++;
      if (spr_check(g, vwrites.back().s, R)) R.spr_bad.push_back((int)f);     // drawspr va antes de los pasos: el scroll del POKE 2072
      if (spr_blk != S("SHEET_BLOCK") || spr_xtr != 0) R.blk_bad++;
      if (pal16_on != 1) R.p16_bad++;
      R.spr_checked++;
      int nb = 0; for (int i = 0; i < 8; i++) nb += g.ba[i]; R.maxb = std::max(R.maxb, nb);
      R.minx = std::min(R.minx, g.x); R.maxx = std::max(R.maxx, g.x); R.miny = std::min(R.miny, g.y); R.maxy = std::max(R.maxy, g.y);
      int scr_now = rd16(scr_sym);                    // el scroll despues de los pasos de esta imagen (con R se descarta)
      gupdate(g, script, f, scr_now, scr_now - vwrites.back().s, R);
      bool r = keydown(script, f, 2, 3);
      fxstep(g);                                      // sfx va justo despues de update
      if (r && !prev_r) { if (g_musno) mus_go(0, g.lvl); ginit(g); ay_model_setup(); }   // R: aysetup (voces libres, volumenes 0); la musica del nivel
      prev_r = r;
      bool sp = keydown(script, f, 7, 0);               // fin de partida: pasada la espera, una pulsacion nueva de ESPACIO
      if (sp && !prev_sp && g.pstate == 3 && g.ptimer == 0) {   // newgame: el nivel 1 con la dificultad inicial (si estaba en otro, se carga)
        g.laps = 0;
        if (g.lvl != 1) {
          if (model_load(1)) { g.lvl = 1; g_curlv = &g_lvstore[1]; g_map = g_lvstore[1].map; R.lvchg.push_back({frame(), 1}); }
          else { g_fatal = true; R.fatals++; }
        }
        if (!g_fatal) { if (g_musno) mus_go(0, g.lvl); ginit(g); ay_model_setup(); R.goresets++; R.newgames++; }
      }
      prev_sp = sp;
      if (g.bst == 2 || g.bst == 3) {                    // vidas del jefe: las de la memoria del Z80 (ya con esta vuelta hecha) son las del modelo, y nunca menos de 1
        int h = rd(S("bhp")); if (h != g.bhp) R.bhp_bad++;
        R.bhp_min = std::min(R.bhp_min, h);
      }
      bool ik = keydown(script, f, 5, 2);               // I: vidas infinitas si / no
      if (ik && !prev_i) { g_inf = !g_inf; R.inftoggles++; }
      prev_i = ik;
      if (!g_fatal) {                                    // nivel, vueltas, dificultad y velocidad: la memoria del Z80 es la del modelo
        bool bad = rd(S("lvlno")) != g.lvl || rd(S("laps")) != g.laps || rd(S("dif")) != (g.lvl - 1) + 5 * g.laps || rd(S("bhpv")) != g.bhp0 || rd(S("bftv")) != g.bft0;
        for (int i = 0; i < 6; i++) if (rd(S("eftab") + i) != g.eft[i]) bad = true;
        if (g_chk_speed && rd(S("speed")) != (g_curlv->map[4] >= 2 ? 1 : 0)) bad = true;
        if (bad) R.dif_bad++;
      }
      if (!ay_match() && R.ay_bad++ == 0) {
        printf("        AY distinto del modelo en la imagen %ld:", f);
        for (int c = 0; c < 2; c++) for (int q = 0; q < 11; q++) if (ayreg[c][q] != ayexp[c][q]) printf(" %c R%d = %d (modelo %d)", c ? 'B' : 'A', q, ayreg[c][q], ayexp[c][q]);
        printf("\n");
      }
    }
    z80_do_opcode(); total_t += (uint64_t)tstates;
    if (vwrites.size() > nvw) {                       // acaba de escribir POKE 2072: la paleta tiene que estar girada lo que toca
      nvw = vwrites.size(); frame_t0 = total_t - total_t % FRAME;
      if (in_main && check_pal) {
        if (calls < 0) calls = 0;
        int bad = 0;
        for (int i = 0; i < 256; i++) {
          uint16_t want = g_curlv->pal(i);
          for (auto& g : rngs) if (i >= g.base && i < g.base + g.n) {
            int o = calls >= g.c0 ? (int)((1 + (calls - g.c0) / g.t) % g.n) : 0;
            want = g_curlv->pal(g.base + ((i - g.base) + o) % g.n);
          }
          if (pal[i] != want) bad++;
        }
        if (bad) R.pal_bad.push_back((int)fr);
        calls++;
      }
    }
    if (++guard > 600000000L) { printf("ERROR: no termina (PC=%04X)\n", z80.pc.w); errors++; break; }
    while (ci < checks.size() && total_t >= checks[ci]) {
      if (!checking_vis || in_restart || !shown_valid || mode != 175 || vring != 102 || hring != 0) { ci++; continue; }
      int bad = 0;
      for (int r = 0; r < 100 && bad < 3; r++)
        for (int x = 0; x < 160; x++) {
          int xs = (x + hofs) % 160;                      // el pixel x de la pantalla es el del anillo (x + hofs) mod 160
          uint8_t got = rd(0x8000 + ((r + vofs) % 102) * 160 + xs), want = g_curlv->at(99 + s_shown - r, xs);
          if (got != want) { printf("        instante %zu (imagen %ld): fila %d x %d = %d, nivel %d (vofs %d, s %d)\n", ci, frame(), r, x, got, want, vofs, s_shown); bad++; break; }
        }
      if (bad) R.vis_bad++; else R.vis_ok++;
      if (hofs) R.vis_shk++;
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

static void fx_report(const Result& L, bool all) {
  printf("        efectos: %ld disparos, %ld disparos enemigos por la izquierda y %ld por la derecha, %ld explosiones, %ld de la nave;"
         " AY distinto del modelo en %d imagenes\n", fx_started[0], fx_started[1], fx_started[2], fx_started[3], fx_started[4], L.ay_bad);
  CHECK(L.ay_bad == 0, "en cada imagen, los registros de los dos AY de la FPGA son los del modelo de los efectos (periodos, ruido, volumenes, mezcladores)");
  if (all) CHECK(fx_started[0] > 0 && fx_started[1] > 0 && fx_started[2] > 0 && fx_started[3] > 0 && fx_started[4] > 0,
                 "suenan los cinco efectos (disparo, disparo enemigo por cada lado, explosion, explosion de la nave)");
}

static void go_report(const Result& L) {
  std::vector<int> want = {23, 10};
  for (const char* c = "*GAME OVER"; *c; c++) want.push_back(*c == '*' ? 0x17 : *c == ' ' ? 0 : 38 + *c - 'A');
  int bad = 0; for (auto& v : says) if (v != want) bad++;
  printf("        %d veces tocada; %d fines de partida, %d veces vuelta a empezar con ESPACIO; imagenes con el cartel: %d\n", L.deaths, L.gameovers, L.goresets, L.cartel);
  (void)bad;
  CHECK(says.empty(), "ninguna orden de voz (el fin de partida es un cartel)");
}

static void boss_report(const Result& L) {
  printf("        jefe: %d avisos, %d entradas, %d impactos, %d salvas, %d derribados, %d vueltas al nivel, la nave tocada por el %d veces; %ld chasquidos\n",
         L.bwarn, L.benter, L.bhits, L.bshots, L.bkills, L.laps, L.bdeaths, fx_started[5]);
}

static void lost_report(const Result& L) {
  printf("        reparto por linea (motor: 63..0, 12 huecos, 171 pasos): %d imagenes con perdidas, %ld lineas de sprite perdidas"
         " (enemigos %ld, disparos %ld, disparos enemigos %ld, nave %ld, marcador %ld)\n",
         L.lost_frames, L.lost_lines, L.lost_cls[0], L.lost_cls[1], L.lost_cls[2], L.lost_cls[3], L.lost_cls[4]);
  printf("        enemigos con lineas perdidas en %d imagenes; racha mas larga de un mismo enemigo: %d imagenes%s\n",
         L.ene_lost_frames, L.max_consec, L.lost_frames ? "" : " (ninguna)");
  if (L.lost_frames) printf("        primera perdida en el tick %d (%s)\n", L.first_lost, L.lostinfo.c_str());
}

// caso peor del reparto, solo con el modelo: el nivel entero a su velocidad con los enemigos inmortales (nadie los derriba ni
// chocan); cuenta los enemigos que cruzan cada linea. Lo que queda para ellos en una linea: 12 - nave (2) - llama (1) - un
// disparo de la nave (salen cada 6 ticks a 3 px: 18 px entre uno y otro) - 2 disparos enemigos = 6 sprites, 3 enemigos; en las
// lineas del marcador (y 2-9) quitar ademas sus 6, pero solo sale tras una muerte (unos 2,3 s).
struct Worst { int maxe, maxe_top, over3, over3_top, frames; long at; std::vector<int> waves; };
static Worst worst_case() {
  Worst W{}; Result R{}; Game g{}; g.lvl = 1; ginit(g); g_immortal = true;
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

// las ordenes de musica que tiene que haber hecho el Z80: NIVEL1.VGM en bucle al empezar, las del modelo (mexp) y STOP_VGM al salir si sonaba
static bool mlog_matches() {
  std::vector<std::vector<int>> want = { {38, 1}, playcmd("NIVEL1.VGM", sdfiles.count("NIVEL1.VGM") > 0) };
  for (auto& v : mexp) want.push_back(v);
  if (g_mok) want.push_back({35});
  return mlog == want;
}
// los ficheros que ha abierto el Z80: los 5 del principio (nivel 1, hoja, record y titulo) y los de los cambios de nivel que espera el modelo
static bool opened_matches() {
  std::vector<std::string> want = {"NIVEL1.I16", "NIVEL1.MAP", "HOJA.BIN", "ESTELA.REC", "TITULO.I16"};
  for (auto& n : g_exp_open) want.push_back(n);
  return opened == want;
}
static void exit_checks(const char* what, int want_bc, int played = -1) {
  bool pl = played < 0 ? want_bc == 0 : played != 0;
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
  {
    bool quiet = true;
    for (int c = 0; c < 2; c++) { if (ayreg[c][7] != 0x3F) quiet = false; for (int r = 8; r <= 10; r++) if (ayreg[c][r] != 0) quiet = false; }
    snprintf(m, sizeof m, "%s: los dos AY de la FPGA callados (mezclador $3F, volumenes 0)", what); CHECK(quiet, m);
    bool ok;
    if (!pl) { ok = mlog.empty(); snprintf(m, sizeof m, "%s: ninguna orden de musica (no se ha llegado a jugar)", what); }
    else {
      ok = mlog_matches();
      int ngo = 0; for (auto& v : mexp) if (v.size() > 1 && v[0] == 38 && v[1] == 0) ngo++;
      snprintf(m, sizeof m, "%s: musica: NIVEL1.VGM en bucle al empezar; %d veces GAMEOVER.VGM sin bucle y de vuelta la del nivel; STOP_VGM antes de cambiar y al salir%s",
               what, ngo, sdfiles.count("NIVEL1.VGM") ? "" : " (sin NIVEL1.VGM)");
    }
    CHECK(ok, m);
    if (!ok) { printf("        mlog:"); for (auto& v : mlog) { printf(" ["); for (int x : v) printf(" %d", x); printf(" ]"); } printf("\n"); }
  }
  {
    int rb = 0; for (int i = 0; i < 8192; i++) if (sram[1][i] != (uint8_t)(i * 13 + 5 + (i >> 9))) rb++;
    snprintf(m, sizeof m, "%s: el bloque 1 vuelve a tener su pagina (la ROM), sin un byte cambiado", what); CHECK(rb == 0 && mapb[1] == 1, m);
  }
  snprintf(m, sizeof m, "%s: ningun fichero abierto y nada escrito fuera de su sitio", what);
  CHECK(fopen_open == 0 && !bad_write, m);
  {
    std::string why; bool ok = rec_verdict(why);
    snprintf(m, sizeof m, "%s: ESTELA.REC: %d fines de partida superan el record -> otras tantas escrituras (71, 56, 57) con el contenido exacto; si no, intacto", what, g_saves);
    CHECK(ok, m);
    if (!ok) printf("        %s\n", why.c_str());
  }
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
  int CODEEND = S("codeend"), PALBUF = S("PALBUF"), LVLBUF = S("LVLBUF"), PALEND = S("PALEND"); lvlpg = S("LVLPG");
  code_hi = 0x4800 + (int)bin.size(); buf_lo = 0x6F60; buf_hi = S("SHEET_END") + 32 * 7;     // las letras W N I L D ($6F60), la hoja y las 7 letras
  cyc_lo = S("cycle"); cyc_hi = S("keys");
  printf("estela.bin: %zu bytes, $4800-$%04X (libres hasta $6F60: %d bytes); nivel en $%04X, pagina %d (8 KB), con TPALBUF $%04X y PALBUF $%04X-$%04X al final\n",
         bin.size(), code_hi - 1, 0x6F60 - CODEEND, LVLBUF, lvlpg, S("TPALBUF"), PALBUF, PALEND - 1);
  printf("NIVEL1.MAP: %zu bytes, %d filas, mapa en +%d, LVL_END %d, LVL_SCROLL %d\n", lv.map.size(), lv.rows, lv.mapoff, lv.end, lv.map[4]);
  CHECK(CODEEND == code_hi && CODEEND <= 0x6F60 && S("LET2ADR") == 0x6F60 && S("LETRAS2") == 123 && LVLBUF == 0x2000 &&
        S("TPALBUF") == LVLBUF + S("MAXLVL") && PALBUF == S("TPALBUF") + 512 && PALEND == 0x4000 && (int)lv.map.size() <= S("MAXLVL"),
        "el codigo acaba antes de $6F60 (letras W N I L D y la hoja de sprites); el nivel (hasta MAXLVL) y las paletas TPALBUF y PALBUF caben en los 8 KB del bloque 1");

  // ===== 1. el nivel 1, con las cuatro velocidades y R
  printf("-- nivel 1 --\n");
  sdfiles.clear(); sdfiles["NIVEL1.I16"] = lv.i16; sdfiles["NIVEL1.MAP"] = lv.map; sdfiles["HOJA.BIN"] = hoja; sdfiles["NIVEL1.VGM"] = {'V', 'g', 'm', ' '};
  sdfiles["GAMEOVER.VGM"] = {'V', 'g', 'm', ' '};
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
  CHECK(opened.size() == 5 && opened[0] == "NIVEL1.I16" && opened[1] == "NIVEL1.MAP" && opened[2] == "HOJA.BIN" && opened[3] == "ESTELA.REC" && opened[4] == "TITULO.I16" &&
        !title_seen && created.empty(), "abre NIVEL1.I16, NIVEL1.MAP, HOJA.BIN, ESTELA.REC y TITULO.I16 (y los cierra); sin TITULO.I16 se salta el titulo; sin superar el record no crea nada");
  int tbad = 0;
  for (int a = 0; a < 16000; a++) { uint8_t got = a < 8192 ? sram[6][a] : sram[7][a - 8192]; if (got != lv.pix(a)) tbad++; }
  CHECK(tbad == 0, "el tileset de $C000 (paginas 6 y 7) es el de NIVEL1.I16");
  int lbad = 0;
  for (size_t i = 0; i < lv.map.size(); i++) if (sram[lvlpg][i] != lv.map[i]) lbad++;
  CHECK(lbad == 0, "NIVEL1.MAP esta entero en la pagina LVLPG (el bloque 1 mientras se juega)");
  CHECK(R.vis_ok >= 400 && R.vis_bad == 0, "lo visible es exactamente el nivel desde la s del POKE 2072, en todos los instantes");
  {
    int hbad = 0; for (size_t i = 0; i < hoja.size(); i++) if (rd(S("SHEET_ADDR") + (int)i) != hoja[i]) hbad++;
    CHECK(hbad == 0 && S("SHEET_ADDR") + (int)hoja.size() <= 0x8000, "la hoja de $7000 es HOJA.BIN");
    int lb = 0; for (int i = 0; i < 32 * 7; i++) if (rd(S("SHEET_END") + i) != rd(S("letras") + i)) lb++;
    for (int i = 0; i < 32 * 5; i++) if (rd(0x6F60 + i) != rd(S("letras2") + i)) lb++;
    CHECK(lb == 0 && S("LETRAS") == S("SHEET_PAGE0") + S("SHEET_CELLS") && S("SHEET_END") + 32 * 7 <= 0x8000,
          "las letras de los carteles: G A M E O V R detras de HOJA.BIN (paginas LETRAS..+6) y W N I L D en las paginas 123-127 ($6F60)");
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
    int touched = 0;
    for (int a = CODEEND; a < 0x8000; a++) if ((a < 0x6F60 || a >= S("SHEET_END") + 32 * 7) && rd(a) != basic_snap[a - 0x4000]) touched++;
    for (int a = 0x4000; a < 0x4700; a++) if (rd(a) != basic_snap[a - 0x4000]) touched++;
    CHECK(touched == 0, "no toca $4000-$46FF ni lo que hay detras del codigo hasta $7FFF (salvo la hoja) (al copiar a $C000 se reescriben los mismos bytes)");
  }
  printf("        juego: %d oleadas, %d enemigos, %d derribados, %d disparos enemigos, %d veces tocada, marcador %d, hasta %d enemigos a la vez\n",
         R.waves, R.spawned, R.kills, R.ebfired, R.deaths, R.score, R.maxene);
  fx_report(R, false);
  CHECK(R.blk_bad == 0 && R.p16_bad == 0, "en cada imagen del juego: hoja en el bloque SHEET_BLOCK (POKE 2129), sin extra, paleta de 16 activa (POKE 2067,1)");
  exit_checks("nivel 1", 0);

  // ===== 1b. el nivel entero, disparando y moviendose sin parar
  printf("-- nivel 1 entero (4.500 imagenes) --\n");
  {
    std::vector<Key> sc;
    for (long f = 30; f < 4440; f += 200) sc.push_back({f, f + 190, 7, 0});   // ESPACIO casi todo el rato (se suelta 10 imagenes cada 4 s)
    for (long f = 40; f < 4400; f += 100) { sc.push_back({f, f + 45, 5, 1}); sc.push_back({f + 50, f + 95, 5, 0}); }   // O y P
    for (long f = 300; f < 4400; f += 700) { sc.push_back({f, f + 60, 2, 0}); sc.push_back({f + 200, f + 260, 1, 0}); }  // Q y A
    sc.push_back({4450, 4454, 0, 2});                                // X
    sdfiles.clear(); sdfiles["NIVEL1.I16"] = lv.i16; sdfiles["NIVEL1.MAP"] = lv.map; sdfiles["HOJA.BIN"] = hoja; sdfiles["NIVEL1.VGM"] = {'V', 'g', 'm', ' '};
    sdfiles["GAMEOVER.VGM"] = {'V', 'g', 'm', ' '};
    Result L = run(lv, sc, 4500, 300, true);
    printf("        %d imagenes; %d oleadas, %d enemigos, %d derribados, %d disparos enemigos, %d veces tocada, marcador %d\n",
           L.spr_checked, L.waves, L.spawned, L.kills, L.ebfired, L.deaths, L.score);
    printf("        el marcador se ve en %d imagenes (tras cada muerte)\n", L.score_shown);
    printf("        hasta %d enemigos a la vez; sin hueco: %d enemigos, %d oleadas; hasta %d sprites en una linea (%d imagenes con mas de 12)\n",
           L.maxene, L.drope, L.dropw, L.maxline, L.over12);
    printf("        vuelta mas larga del bucle %ld T; sprites escritos hasta %ld T tras el flanco\n", L.maxloop, spr_maxlat);
    lost_report(L);
    CHECK(L.lost_frames == 0 && L.maxline <= 12, "con el reparto del motor no se pierde ninguna linea de sprite en todo el nivel");
    fx_report(L, true);
    go_report(L);
    boss_report(L);
    CHECK(L.gameovers > 0 && L.goresets > 0, "con 3 vidas llega el fin de partida, y ESPACIO vuelve a empezar");
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
    struct Sc { const char* what; bool weave; } scs[] = { {"casi sin disparar (un toque de ESPACIO cada 8 s), quieta abajo", false},
                                                          {"sin disparar, moviendose", true} };
    for (auto& c : scs) {
      printf("-- nivel 1 entero, %s --\n", c.what);
      std::vector<Key> sc;
      if (c.weave) {
        for (long f = 40; f < 4400; f += 100) { sc.push_back({f, f + 45, 5, 1}); sc.push_back({f + 50, f + 95, 5, 0}); }
        for (long f = 300; f < 4400; f += 700) { sc.push_back({f, f + 60, 2, 0}); sc.push_back({f + 200, f + 260, 1, 0}); }
      }
      if (!c.weave) for (long f = 200; f < 4400; f += 400) sc.push_back({f, f + 3, 7, 0});
      sc.push_back({4450, 4454, 0, 2});
      sdfiles.clear(); sdfiles["NIVEL1.I16"] = lv.i16; sdfiles["NIVEL1.MAP"] = lv.map; sdfiles["HOJA.BIN"] = hoja; sdfiles["NIVEL1.VGM"] = {'V', 'g', 'm', ' '};
      Result L = run(lv, sc, 4500, 50, true);
      printf("        %d imagenes; %d oleadas, %d enemigos, %d derribados, %d disparos enemigos, %d veces tocada; marcador visible en %d\n",
             L.spr_checked, L.waves, L.spawned, L.kills, L.ebfired, L.deaths, L.score_shown);
      printf("        hasta %d enemigos a la vez; hasta %d sprites en una linea (%d imagenes con mas de 12)\n", L.maxene, L.maxline, L.over12);
      lost_report(L);
      CHECK(L.lost_frames == 0 && L.maxline <= 12, "con el reparto del motor no se pierde ninguna linea de sprite (y el marcador sale mas a menudo)");
      fx_report(L, false);
      go_report(L);
      CHECK(L.gameovers > 0, "sin disparar, la partida se acaba");
      if (!L.spr_bad.empty()) printf("        sprites mal en %zu imagenes, la primera %d\n", L.spr_bad.size(), L.spr_bad[0]);
      CHECK(L.spr_checked > 4300 && L.spr_bad.empty() && L.vis_bad == 0 && L.pal_bad.empty(), "los 64 sprites, lo visible y la paleta, iguales al modelo");
      exit_checks(c.what, 0);
    }
  }

  // ===== 1e. el jefe: un nivel de 10 filas con solo su oleada (en s = 0) y vidas infinitas (tecla I)
  {
    printf("-- jefe (nivel corto con solo el jefe, vidas infinitas con I) --\n");
    std::vector<uint8_t> bm(lv.map.begin(), lv.map.begin() + 16);
    uint8_t bw[16] = {0, 0, 6, 64, (uint8_t)S("PATH_ENTER"), 1, 0, 0, 0xFF, 0xFF, 0, 0, 0, 0, 0, 0};
    bm.insert(bm.end(), bw, bw + 16);
    for (int i = 0; i < 80; i++) bm.push_back(lv.map[lv.mapoff + i]);
    bm[5] = 1; bm[6] = 10; bm[7] = 0; bm[10] = 32; bm[11] = 0;
    bm[12] = (uint8_t)(bm.size() & 255); bm[13] = (uint8_t)(bm.size() >> 8); bm[14] = 60; bm[15] = 0;
    Level bl = lv; bl.map = bm; bl.rows = 10; bl.mapoff = 32; bl.end = 60;
    sdfiles.clear(); sdfiles["NIVEL1.I16"] = lv.i16; sdfiles["NIVEL1.MAP"] = bm; sdfiles["HOJA.BIN"] = hoja; sdfiles["NIVEL1.VGM"] = {'V', 'g', 'm', ' '};
    sdfiles["GAMEOVER.VGM"] = {'V', 'g', 'm', ' '};
    std::vector<Key> sc;
    sc.push_back({30, 3940, 7, 0});                                  // ESPACIO todo el rato
    sc.push_back({60, 64, 5, 2});                                    // I: vidas infinitas
    for (long f = 300; f < 3900; f += 400) { sc.push_back({f, f + 60, 5, 1}); sc.push_back({f + 200, f + 260, 5, 0}); }   // O y P de vez en cuando
    sc.push_back({3950, 3954, 0, 2});
    g_map = bm;
    Result L = run(bl, sc, 4000, 100, true);
    g_map = lv.map;
    boss_report(L);
    printf("        hasta %d sprites en una linea; %d imagenes con perdidas (%ld lineas); sprites escritos hasta %ld T; vuelta mas larga %ld T\n",
           L.maxline, L.lost_frames, L.lost_lines, spr_maxlat, L.maxloop);
    if (!L.spr_bad.empty()) printf("        sprites mal en %zu imagenes, la primera %d\n", L.spr_bad.size(), L.spr_bad[0]);
    CHECK(L.spr_checked > 3800 && L.spr_bad.empty() && L.ay_bad == 0 && L.vis_bad == 0 && L.pal_bad.empty(),
          "los 64 sprites (WARNING, el jefe, su parpadeo y su explosion, WELL DONE), los AY, lo visible y la paleta, iguales al modelo");
    CHECK(L.bwarn >= 2 && L.benter >= 2 && L.bshots > 10 && L.bkills >= 1 && L.laps >= 1 && L.bhits >= 40,
          "el jefe avisa, entra, dispara, aguanta sus disparos, explota, sale WELL DONE y el nivel vuelve a empezar (y vuelve a salir)");
    CHECK(L.lost_frames == 0, "con el jefe no se pierde ninguna linea de sprite");
    {
      bool bright = false; for (int b : borders) if (b == 0x2C) bright = true;
      printf("        vidas infinitas: %d pulsaciones de I, %d veces tocada, %d fines de partida\n", L.inftoggles, L.deaths, L.gameovers);
      CHECK(L.inftoggles == 1 && L.deaths > 3 && L.gameovers == 0 && bright, "con I, vidas infinitas: tocada mas de 3 veces sin fin de partida, y el borde verde brillante");
    }
    CHECK(spr_maxlat < 12000, "los sprites se escriben en el borrado (antes de los ~12.600 T de la primera linea)");
    exit_checks("jefe", 0);
  }

  // ===== 1f. mejoras (fase 7c): niveles cortos hechos aqui con sus oleadas de tipo 8-10 (arma, escudo, bomba)
  {
    struct W { int pos, typ, x, path, count, spc, flg; };
    auto mk = [&](const std::vector<W>& ws, int rows, Level& bl) {
      std::vector<uint8_t> m(lv.map.begin(), lv.map.begin() + 16);
      for (auto& w : ws) { uint8_t r[8] = {(uint8_t)(w.pos & 255), (uint8_t)(w.pos >> 8), (uint8_t)w.typ, (uint8_t)w.x, (uint8_t)w.path, (uint8_t)w.count, (uint8_t)w.spc, (uint8_t)w.flg}; m.insert(m.end(), r, r + 8); }
      uint8_t t[8] = {0xFF, 0xFF, 0, 0, 0, 0, 0, 0}; m.insert(m.end(), t, t + 8);
      int mo = (int)m.size();
      for (int i = 0; i < 8 * rows; i++) m.push_back(lv.map[lv.mapoff + i]);
      int end = 16 * rows - 100;
      m[5] = (uint8_t)ws.size(); m[6] = (uint8_t)rows; m[7] = (uint8_t)(rows >> 8); m[10] = (uint8_t)mo; m[11] = (uint8_t)(mo >> 8);
      m[12] = (uint8_t)m.size(); m[13] = (uint8_t)(m.size() >> 8); m[14] = (uint8_t)end; m[15] = (uint8_t)(end >> 8);
      bl = lv; bl.map = m; bl.rows = rows; bl.mapoff = mo; bl.end = end;
      return m;
    };
    auto item_report = [&](const Result& L) {
      printf("        mejoras recogidas: %d armas, %d escudos, %d bombas; %d se van por abajo; %d salvas dobles; %d bombas usadas (%d enemigos, jefe %d veces, %d sin bajar de 1);"
             " arma perdida %d veces; %ld sonidos de recogida\n", L.items[0], L.items[1], L.items[2], L.itemmiss, L.dblvolleys, L.bombuse, L.bombkills, L.bombboss, L.bclamp, L.weaplost, fx_started[6]);
    };
    auto basefiles = [&](const std::vector<uint8_t>& bm) {
      sdfiles.clear(); sdfiles["NIVEL1.I16"] = lv.i16; sdfiles["NIVEL1.MAP"] = bm; sdfiles["HOJA.BIN"] = hoja; sdfiles["NIVEL1.VGM"] = {'V', 'g', 'm', ' '};
      sdfiles["GAMEOVER.VGM"] = {'V', 'g', 'm', ' '};
    };
    const int SLOW = S("PATH_SLOW"), SINE = S("PATH_SINE"), DIVE = S("PATH_DIVE"), DOWN = S("PATH_DOWN");
    // --- A: arma, bombas (con el tope de 2), escudo y enemigos: la nave quieta en x = 72 (la mejora cae en su X) y luego movida a la derecha
    printf("-- mejoras (nivel de 150 filas con oleadas de tipo 8-10 y el jefe al final; la nave quieta en x = 72, luego en el borde derecho) --\n");
    Level bl; std::vector<uint8_t> bm = mk({ {0, 8, 72, SLOW, 1, 0, 0}, {250, 10, 72, SLOW, 1, 0, 0}, {360, 10, 72, SLOW, 1, 0, 0}, {480, 10, 72, SLOW, 1, 0, 0},
                                             {600, 9, 72, SLOW, 1, 0, 0}, {700, 0, 48, SINE, 4, 16, 0}, {740, 1, 24, DIVE, 3, 18, 0}, {1000, 1, 72, DOWN, 1, 0, 0},
                                             {1200, 8, 72, SLOW, 1, 0, 0}, {1300, 9, 20, SLOW, 1, 0, 0}, {1450, 9, 130, SLOW, 1, 0, 0},
                                             {1480, 10, 130, SLOW, 1, 0, 0}, {1580, 10, 130, SLOW, 1, 0, 0}, {1620, 6, 64, S("PATH_ENTER"), 1, 0, 0} }, 156, bl);
    // (con la cifra del nivel a la vista, 100 ticks, la mejora de s = 0 espera: las demas empiezan 100 mas tarde que en la fase 7c y las teclas tambien)
    std::vector<Key> sc = { {30, 980, 7, 0}, {1110, 2340, 7, 0},                    // ESPACIO; se suelta para que el enemigo que baja derecho toque la nave
                            {220, 224, 7, 4}, {800, 804, 7, 4}, {860, 864, 7, 4},   // B: sin bombas, y dos veces con enemigos delante
                            {1420, 1520, 5, 0},                                      // P: la nave al borde derecho
                            {1400, 1404, 5, 2},                                      // I: vidas infinitas (el jefe llega al final)
                            {1950, 1954, 7, 4}, {2050, 2054, 7, 4}, {2150, 2154, 7, 4},   // B contra el jefe (las dos bombas recogidas a 130)
                            {2360, 2364, 0, 2} };                                    // X
    auto runA = [&]() { basefiles(bm); g_map = bm; Result L = run(bl, sc, 2420, 500, true); g_map = lv.map; return L; };
    Result L = runA();
    item_report(L); lost_report(L);
    printf("        hasta %d sprites en una linea; %d imagenes con perdidas; sprites escritos hasta %ld T; vuelta mas larga %ld T; la nave tocada %d veces\n",
           L.maxline, L.lost_frames, spr_maxlat, L.maxloop, L.deaths);
    if (!L.spr_bad.empty()) printf("        sprites mal en %zu imagenes, la primera %d\n", L.spr_bad.size(), L.spr_bad[0]);
    CHECK(L.spr_checked > 2300 && L.spr_bad.empty() && L.ay_bad == 0 && L.vis_bad == 0 && L.pal_bad.empty(),
          "los 64 sprites (mejora en el 54, iconos de las bombas en el 32-33, disparo doble), los AY, lo visible y la paleta, iguales al modelo en cada imagen");
    CHECK(L.items[0] >= 2 && L.items[1] >= 2 && L.items[2] >= 3 && L.itemmiss >= 1 && fx_started[6] == L.items[0] + L.items[1] + L.items[2],
          "se recogen las tres mejoras (arma, escudo, bomba; una bomba de mas con el tope), una se va por abajo sin recogerla y cada recogida suena");
    CHECK(L.dblvolleys > 30 && L.weaplost >= 1, "con el arma salen dos disparos por salva (x + 0 y x + 8) y se pierde al perder la nave");
    CHECK(L.bombuse >= 3 && L.bombkills > 0, "B sin bombas no hace nada; con ellas, todos los enemigos vivos explotan (con sus puntos)");
    {
      printf("        temblor: %d imagenes con POKE 2074 distinto de 0; %d instantes de lo visible comprobados con el desplazamiento; jefe: %d bombas, mas bajo que llega: %d vidas\n",
             L.hofs_nz, L.vis_shk, L.bombboss, L.bhp_min);
      CHECK(L.hofs_bad == 0 && L.hofs_nz >= 23 * L.bombuse / 2, "POKE 2074 en cada imagen es el del modelo (tabla de 24 desplazamientos que se apagan, 0 al acabar, junto al POKE 2072)");
      CHECK(L.vis_shk > 0, "durante el temblor, lo visible es el anillo rotado por hofs (el pixel x sale de la columna (x + hofs) mod 160)");
      CHECK(L.bombboss >= 1 && L.bhp_bad == 0 && L.bhp_min >= 1, "bomba contra el jefe: sus vidas (bhp, en la memoria del Z80) son las del modelo en cada imagen y nunca baja de 1");
      CHECK(fx_started[7] == L.bombuse, "cada bomba suena con fxbomb");
      int t = S("fxbomb"), tdie = S("fxdie"), bad = rd(t) <= rd(tdie) ? 1 : 0, n = 0;
      for (int a = t + 1; rd(a + 1) != 0xFF; a += 2, n++) { int per = rd(a), vol = rd(a + 1); if (per < 24 || per > 31 || vol > 15 || vol == 0) bad++; if (n && vol > rd(a - 1)) bad++; }
      printf("        fxbomb: prioridad %d (fxdie %d), %d pares, volumen inicial %d\n", rd(t), rd(tdie), n, rd(t + 2));
      CHECK(bad == 0 && n >= 60 && n <= 68 && rd(t + 2) == 15, "fxbomb: prioridad 3 (mayor que fxdie), unas 64 imagenes de ruido grave (periodo 24-31), volumen 15 que baja despacio");
    }
    CHECK(L.shielded >= 2 && spr_maxlat < 12000, "dos escudos (250 ticks, parpadea la nave) y los sprites escritos antes de 12.000 T");
    // El escudo deja el marcador (6 cifras, y 2-9) a la vista 250 ticks; con 3 enemigos entrando a la vez justo debajo, 2 disparos de la
    // salva doble y el marcador pasan de 12 en esas filas: se pierden unas lineas de arriba de un enemigo en una imagen (limite conocido
    // desde la fase 5; en el nivel real, con las mejoras donde estan, no ocurre: ver el nivel entero)
    CHECK(L.lost_frames <= 2 && L.lost_lines == L.lost_cls[0] && L.maxline <= 14,
          "con esta oleada de 3 enemigos que entra bajo el marcador del escudo, solo se pierden unas lineas de arriba de un enemigo (como mucho en 2 imagenes)");
    exit_checks("mejoras", 0);

    // --- pruebas en negativo: seis cambios a proposito en el codigo (el escudo, el tope de bombas, la separacion del disparo doble, la duracion y la tabla del temblor, el dano de la bomba al jefe) y el arnes tiene que verlos
    {
      std::vector<uint8_t> good = bin;
      auto find1 = [&](const std::vector<uint8_t>& pat) { int at = -1, n = 0; for (size_t i = 0; i + pat.size() <= good.size(); i++) if (!memcmp(&good[i], pat.data(), pat.size())) { at = (int)i; n++; } return n == 1 ? at : -1; };
      int pt = S("ptimer");
      struct Neg { const char* what; std::vector<uint8_t> pat; int off; uint8_t val; } negs[] = {
        { "escudo de 200 ticks en lugar de 250", {0x3E, 0xFA, 0x32, (uint8_t)(pt & 255), (uint8_t)(pt >> 8)}, 1, 200 },
        { "tope de 3 bombas en lugar de 2", {0x7E, 0xFE, 0x02, 0xD0, 0x34, 0xC9}, 2, 3 },
        { "el segundo disparo en x + 9 en lugar de x + 8", {0x1E, 0x08, 0xDD, 0x36, 0x00, 0x01}, 1, 9 },
        { "el temblor de 23 imagenes en lugar de 24", {0x3E, 0x18, 0x32, (uint8_t)(S("shk") & 255), (uint8_t)(S("shk") >> 8)}, 1, 23 },
        { "el primer desplazamiento del temblor en 4 en lugar de 3", {3, 157, 3, 157, 3, 157, 2, 158}, 0, 4 },
        { "la bomba quita 9 vidas al jefe en lugar de 10", {0x3A, (uint8_t)(S("bhp") & 255), (uint8_t)(S("bhp") >> 8), 0xD6, 0x0A}, 4, 9 } };
      for (auto& n : negs) {
        int at = find1(n.pat);
        CHECK(at >= 0, "(prueba en negativo) el trozo de codigo a cambiar esta una sola vez");
        if (at < 0) continue;
        bin = good; bin[at + n.off] = n.val;
        int e0 = errors;
        Result N = runA();
        errors = e0;                                   // los fallos de dentro de run no cuentan: se buscan
        char m[240]; snprintf(m, sizeof m, "(prueba en negativo) con %s, el arnes lo ve (%zu imagenes de sprites mal, %d de POKE 2074, %d de vidas del jefe)", n.what, N.spr_bad.size(), N.hofs_bad, N.bhp_bad);
        CHECK(!N.spr_bad.empty() || N.hofs_bad > 0 || N.bhp_bad > 0, m);
      }
      bin = good;
    }

    // --- B: el jefe y las bombas: vidas infinitas; la bomba le quita 10 vidas (sin bajar de 1)
    printf("-- mejoras con el jefe (nivel de 70 filas: el jefe y una bomba cada 100 en s = 0-800; B cada 30 imagenes; vidas infinitas) --\n");
    std::vector<uint8_t> bm2 = mk({ {0, 6, 64, S("PATH_ENTER"), 1, 0, 0}, {0, 10, 72, SLOW, 1, 0, 0}, {100, 10, 72, SLOW, 1, 0, 0}, {200, 10, 72, SLOW, 1, 0, 0}, {300, 10, 72, SLOW, 1, 0, 0}, {400, 10, 72, SLOW, 1, 0, 0}, {500, 10, 72, SLOW, 1, 0, 0}, {600, 10, 72, SLOW, 1, 0, 0}, {700, 10, 72, SLOW, 1, 0, 0}, {800, 10, 72, SLOW, 1, 0, 0} }, 70, bl);
    std::vector<Key> sc2 = { {30, 3940, 7, 0}, {60, 64, 5, 2}, {3950, 3954, 0, 2} };
    for (long t = 120; t < 3900; t += 30) sc2.push_back({t, t + 4, 7, 4});
    basefiles(bm2); g_map = bm2;
    Result J = run(bl, sc2, 4000, 100, true);
    g_map = lv.map;
    boss_report(J); item_report(J);
    printf("        sprites escritos hasta %ld T tras el flanco; hasta %d sprites en una linea\n", spr_maxlat, J.maxline);
    if (!J.spr_bad.empty()) printf("        sprites mal en %zu imagenes, la primera %d\n", J.spr_bad.size(), J.spr_bad[0]);
    printf("        spr_bad %zu, ay_bad %d, vis_bad %d, pal_bad %zu\n", J.spr_bad.size(), J.ay_bad, J.vis_bad, J.pal_bad.size());
    CHECK(J.spr_checked > 3500 && J.spr_bad.empty() && J.ay_bad == 0 && J.vis_bad == 0 && J.pal_bad.empty(),
          "con el jefe, los 64 sprites, los AY, lo visible y la paleta, iguales al modelo");
    CHECK(J.hofs_bad == 0 && J.bhp_bad == 0 && J.bhp_min >= 1, "con el jefe, el temblor y las vidas del jefe (que nunca bajan de 1) son los del modelo en cada imagen");
    CHECK(J.bombboss >= 2 && J.bclamp >= 1 && J.bkills >= 1, "las bombas le quitan 10 vidas al jefe (y una, con menos de 11, lo deja en 1) y aun asi se le puede derribar");
    CHECK(J.lost_frames == 0 && spr_maxlat < 12000, "sin lineas perdidas y los sprites escritos antes de 12.000 T");
    exit_checks("mejoras con el jefe", 0);
  }

  // ===== 1g. titulo y record (fase 7d)
  {
    auto basef = [&]() { sdfiles.clear(); sdfiles["NIVEL1.I16"] = lv.i16; sdfiles["NIVEL1.MAP"] = lv.map; sdfiles["HOJA.BIN"] = hoja; sdfiles["NIVEL1.VGM"] = {'V', 'g', 'm', ' '};
                         sdfiles["GAMEOVER.VGM"] = {'V', 'g', 'm', ' '}; };
    // un TITULO.I16 de mentira: paleta y pixeles que no se parecen a los del nivel
    auto mk_title = [&](int seed) {
      std::vector<uint8_t> t = {'I', '1', '6', '0'};
      for (int i = 0; i < 256; i++) { t.push_back((uint8_t)(i * 5 + seed)); t.push_back((uint8_t)(i * 3 + seed * 7 + 1)); }
      for (int y = 0; y < 100; y++) for (int x = 0; x < 160; x++) t.push_back((uint8_t)(x * 3 + y * 5 + seed));
      return t;
    };
    auto mk_rec = [&](const char* sig, int v) { std::vector<uint8_t> r(sig, sig + 4); std::vector<uint8_t> b = rec_bytes(v); r.insert(r.end(), b.begin() + 4, b.end()); return r; };

    printf("-- titulo: con TITULO.I16 y ESTELA.REC = 001234; ESPACIO empieza --\n");
    basef(); sdfiles["TITULO.I16"] = mk_title(3); sdfiles["ESTELA.REC"] = mk_rec("EREC", 1234);
    Result T = run(lv, { {260, 264, 7, 0}, {700, 704, 0, 2} }, 760, 300, true);
    printf("        titulo visto: %s (%d cosas distintas); record de la memoria distinto del fichero: %d; imagenes de juego comparadas: %d\n",
           title_seen ? "si" : "no", title_bad, rec_start_bad, T.spr_checked);
    CHECK(title_seen && title_bad == 0, "el titulo: modo 175, anillo sin desplazar, la paleta y los 16.000 pixeles de TITULO.I16, y 001234 en los sprites 58-63 (x 56-103, y RECY); los demas apagados");
    CHECK(opened.size() == 5 && opened[3] == "ESTELA.REC" && opened[4] == "TITULO.I16" && created.empty(), "lee ESTELA.REC (53, 55, 57) y TITULO.I16; no escribe nada");
    CHECK(rec_start_bad == 0, "el record leido (la memoria del Z80) es 001234");
    CHECK(T.spr_checked > 300 && T.spr_bad.empty() && T.vis_bad == 0 && T.pal_bad.empty() && T.ay_bad == 0,
          "tras ESPACIO el juego va como siempre: los 64 sprites, lo visible y la paleta iguales al modelo (el anillo y los paneles repintados)");
    CHECK(T.vis_ok >= 20, "el nivel se ve de verdad despues del titulo (20 instantes o mas)");
    exit_checks("titulo y juego", 0);

    printf("-- titulo: X en el titulo sale al BASIC sin jugar --\n");
    basef(); sdfiles["TITULO.I16"] = mk_title(9);
    T = run(lv, { {260, 264, 0, 2} }, 400, 0, false);
    CHECK(title_seen && title_bad == 0 && T.spr_checked == 0 && T.frames > 200, "con X en el titulo sale (BC = 0) sin llegar a jugar: el titulo se ve entero (paleta, pixeles, 000000)");
    exit_checks("titulo, X", 0, 0);

    printf("-- titulo: ESPACIO sigue pulsado desde antes (empieza al primer vistazo) --\n");
    basef(); sdfiles["TITULO.I16"] = mk_title(5);
    T = run(lv, { {0, 700, 7, 0}, {700, 704, 0, 2} }, 760, 50, true);
    CHECK(title_seen && title_bad == 0 && T.spr_checked > 300 && T.spr_bad.empty(), "con ESPACIO pulsado desde el principio el titulo sale una imagen y empieza la partida");
    exit_checks("titulo, ESPACIO pulsado", 0);

    printf("-- titulo: TITULO.I16 que no es un .I16 --\n");
    basef(); sdfiles["TITULO.I16"] = mk_title(2); sdfiles["TITULO.I16"][3] = '1';
    T = run(lv, { {400, 404, 0, 2} }, 460, 20, true);
    CHECK(!title_seen && T.spr_checked > 100 && T.spr_bad.empty() && opened.back() == "TITULO.I16", "sin firma I160 se salta el titulo: la partida empieza sola");
    exit_checks("titulo que no es un .I16", 0);

    // el nivel entero como en 1b (disparando y moviendose; 3 vidas: llega el fin de partida), con distintos ESTELA.REC
    auto script1b = [&]() {
      std::vector<Key> sc;
      for (long f = 30; f < 4440; f += 200) sc.push_back({f, f + 190, 7, 0});
      for (long f = 40; f < 4400; f += 100) { sc.push_back({f, f + 45, 5, 1}); sc.push_back({f + 50, f + 95, 5, 0}); }
      for (long f = 300; f < 4400; f += 700) { sc.push_back({f, f + 60, 2, 0}); sc.push_back({f + 200, f + 260, 1, 0}); }
      sc.push_back({4450, 4454, 0, 2});
      return sc;
    };
    struct RC { const char* what; int kind; int v; } rcs[] = {
      { "sin ESTELA.REC (record 0): se guarda el record", 0, 0 },
      { "ESTELA.REC = 999999: no se supera, no se escribe nada", 1, 999999 },
      { "ESTELA.REC con otra firma (XREC y 999999): vale 0 y se sobrescribe", 2, 999999 },
      { "ESTELA.REC = 000100: se sube al marcador mas alto", 1, 100 } };
    for (auto& c : rcs) {
      printf("-- record: %s --\n", c.what);
      basef();
      if (c.kind == 1) sdfiles["ESTELA.REC"] = mk_rec("EREC", c.v);
      if (c.kind == 2) sdfiles["ESTELA.REC"] = mk_rec("XREC", c.v);
      Result L = run(lv, script1b(), 4500, 20, true);
      printf("        %d fines de partida, marcador mas alto superado %d veces: %d ordenes fcreate, %d fwrite; record final del modelo %06d\n",
             L.gameovers, g_saves, (int)created.size(), written_n, g_rec);
      CHECK(L.gameovers > 0 && rec_start_bad == 0 && !title_seen, "hay fin de partida, el record leido al empezar es el del modelo y sin TITULO.I16 no hay titulo");
      if (c.kind == 1 && c.v == 999999) CHECK(g_saves == 0 && created.empty() && written_n == 0, "no se supera el record: ni fcreate ni fwrite");
      else CHECK(g_saves >= 1 && (int)created.size() == g_saves, "se supera el record: hay escritura(s) (solo las que lo superan)");
      CHECK(L.spr_bad.empty() && L.vis_bad == 0 && L.pal_bad.empty(), "y los 64 sprites, lo visible y la paleta, iguales al modelo (el guardado no estropea el juego)");
      exit_checks(c.what, 0);
    }

    // pruebas en negativo: cambios a proposito en el codigo del record; el veredicto de ESTELA.REC tiene que fallar
    {
      std::vector<uint8_t> good = bin;
      auto find1 = [&](const std::vector<uint8_t>& pat) { int at = -1, n = 0; for (size_t i = 0; i + pat.size() <= good.size(); i++) if (!memcmp(&good[i], pat.data(), pat.size())) { at = (int)i; n++; } return n == 1 ? at : -1; };
      struct Neg { const char* what; std::vector<uint8_t> pat; int off; uint8_t val; int rec; } negs[] = {
        { "la firma FREC en lugar de EREC", {'E', 'R', 'E', 'C'}, 0, 'F', -1 },
        { "guardar siempre, aunque no se supere (JR C pasa a JR)", {0x38, 0x06, 0xC0, 0x23, 0x13, 0x10, 0xF7, 0xC9}, 0, 0x18, 999999 } };
      for (auto& n : negs) {
        int at = find1(n.pat);
        CHECK(at >= 0, "(prueba en negativo) el trozo de codigo a cambiar esta una sola vez");
        if (at < 0) continue;
        bin = good; bin[at + n.off] = n.val;
        basef(); if (n.rec >= 0) sdfiles["ESTELA.REC"] = mk_rec("EREC", n.rec);
        int e0 = errors;
        run(lv, script1b(), 4500, 0, false);
        std::string why; bool ok = rec_verdict(why);
        errors = e0;                                   // los fallos de dentro de run no cuentan: se buscan
        char m[240]; snprintf(m, sizeof m, "(prueba en negativo) con %s, el arnes lo ve (%s)", n.what, ok ? "NO" : why.c_str());
        CHECK(!ok, m);
      }
      bin = good;
    }
  }

  // ===== 1h. varios niveles y dificultad (fase 8a): NIVEL1..NIVEL5, cifra del nivel, paso de nivel tras el jefe, vuelta al 1, fallos de carga
  // Niveles de prueba: cada uno es corto (12 filas: solo el jefe y una mejora en s = 0) y distinto (el tileset de out/NIVELn.I16 si existe,
  // si no el del 1 con los pixeles desplazados; velocidad 1 en los impares y 2 en los pares). Se juega con vidas infinitas disparando.
  {
    struct Real { bool has; std::vector<uint8_t> i16, map; } real[6] = {};
    int nreal = 0;
    for (int n = 2; n <= 5; n++) {
      real[n].i16 = readfile(out + "/NIVEL" + std::to_string(n) + ".I16"); real[n].map = readfile(out + "/NIVEL" + std::to_string(n) + ".MAP");
      real[n].has = real[n].i16.size() == 16516 && !memcmp(&real[n].i16[0], "I160", 4) && real[n].map.size() >= 16 && !memcmp(&real[n].map[0], "ELV1", 4) &&
                    u16(real[n].map, 6) >= 12 && u16(real[n].map, 10) + 8 * 12 <= (int)real[n].map.size();
      if (real[n].has) nreal++;
    }
    printf("-- varios niveles: NIVEL2..5 de out/: %d de 4 (los demas, copias del 1 con los pixeles desplazados) --\n", nreal);
    const int ENTER = S("PATH_ENTER"), SLOW = S("PATH_SLOW");
    struct W { int pos, typ, x, path, count, spc, flg; };
    auto mkmap = [&](const std::vector<uint8_t>& base, const std::vector<W>& ws, int rows, int scroll) {
      int bmo = u16(base, 10);
      std::vector<uint8_t> m(base.begin(), base.begin() + 16);
      for (auto& w : ws) { uint8_t r[8] = {(uint8_t)(w.pos & 255), (uint8_t)(w.pos >> 8), (uint8_t)w.typ, (uint8_t)w.x, (uint8_t)w.path, (uint8_t)w.count, (uint8_t)w.spc, (uint8_t)w.flg}; m.insert(m.end(), r, r + 8); }
      uint8_t t[8] = {0xFF, 0xFF, 0, 0, 0, 0, 0, 0}; m.insert(m.end(), t, t + 8);
      int mo = (int)m.size();
      for (int i = 0; i < 8 * rows; i++) m.push_back(base[bmo + i]);
      int end = 16 * rows - 100;
      m[4] = (uint8_t)scroll; m[5] = (uint8_t)ws.size(); m[6] = (uint8_t)rows; m[7] = (uint8_t)(rows >> 8); m[8] = 16; m[9] = 0; m[10] = (uint8_t)mo; m[11] = (uint8_t)(mo >> 8);
      m[12] = (uint8_t)m.size(); m[13] = (uint8_t)(m.size() >> 8); m[14] = (uint8_t)end; m[15] = (uint8_t)(end >> 8);
      return m;
    };
    // los ficheros del nivel n: tileset (el real o el del 1 desplazado 17 (n - 1)) y mapa corto
    auto lvfiles = [&](int n, std::vector<uint8_t>& i16, std::vector<uint8_t>& map) {
      i16 = real[n].has ? real[n].i16 : lv.i16;
      if (!real[n].has && n > 1) for (int a = 0; a < 16000; a++) i16[4 + 512 + a] = (uint8_t)(i16[4 + 512 + a] + 17 * (n - 1));
      map = mkmap(real[n].has ? real[n].map : lv.map, { {0, 6, 64, ENTER, 1, 0, 0}, {0, 8 + n % 3, 72, SLOW, 1, 0, 0} }, 12, n % 2 ? 1 : 2);
    };
    // la SD con los niveles de la lista (y su musica, salvo la de los de skipvgm)
    auto build = [&](const std::vector<int>& lvls, const std::vector<int>& skipvgm) {
      sdfiles.clear(); sdfiles["HOJA.BIN"] = hoja; sdfiles["GAMEOVER.VGM"] = {'V', 'g', 'm', ' '};
      for (int n : lvls) {
        std::vector<uint8_t> i16, map; lvfiles(n, i16, map);
        sdfiles["NIVEL" + std::to_string(n) + ".I16"] = i16; sdfiles["NIVEL" + std::to_string(n) + ".MAP"] = map;
        if (std::find(skipvgm.begin(), skipvgm.end(), n) == skipvgm.end()) sdfiles["NIVEL" + std::to_string(n) + ".VGM"] = {'V', 'g', 'm', ' '};
      }
    };
    // las teclas: ESPACIO todo el rato, I (vidas infinitas) y O / P cada 100 imagenes, hasta la imagen upto
    auto gen = [&](long upto) {
      std::vector<Key> sc;
      auto add = [&](long a, long b, int row, int bit) { if (a < upto) sc.push_back({a, std::min(b, upto), row, bit}); };
      add(30, 1000000, 7, 0); add(60, 64, 5, 2);
      for (long f = 100; f < 30000; f += 100) { add(f, f + 40, 5, 1); add(f + 50, f + 90, 5, 0); }
      return sc;
    };
    // (nivel, vuelta, vidas del jefe, ticks entre salvas, cadencia del tipo 0) al empezar cada nivel: las cifras de la especificacion, no del modelo
    // d = 0..10: 40 + 10 d, max(15, 30 - 3 d), 96 con v - v / 8 repetido y como poco 48
    static const int EXPB[11] = {40, 50, 60, 70, 80, 90, 100, 110, 120, 130, 140}, EXPF[11] = {30, 27, 24, 21, 18, 15, 15, 15, 15, 15, 15},
                     EXPE[11] = {96, 84, 74, 65, 57, 50, 48, 48, 48, 48, 48};
    auto chk_difs = [&](const char* what, size_t n) {
      bool ok = g_difs.size() >= n; size_t at = 0;
      for (size_t i = 0; ok && i < n; i++) {
        int d = (g_difs[i][0] - 1) + 5 * g_difs[i][1];
        if (d > 10 || g_difs[i][2] != EXPB[d] || g_difs[i][3] != EXPF[d] || g_difs[i][4] != EXPE[d]) { ok = false; at = i; }
      }
      char m[240]; snprintf(m, sizeof m, "%s: la dificultad de cada nivel es la de la tabla (jefe 40 + 10 d vidas, salva cada max(15, 30 - 3 d), enemigos v - v / 8 d veces, como poco la mitad)", what);
      CHECK(ok, m);
      if (!ok) printf("        nivel %d, vuelta %d: jefe %d, salvas %d, cadencia %d (n = %zu, hay %zu)\n", g_difs.size() > at ? g_difs[at][0] : -1, g_difs.size() > at ? g_difs[at][1] : -1,
                      g_difs.size() > at ? g_difs[at][2] : -1, g_difs.size() > at ? g_difs[at][3] : -1, g_difs.size() > at ? g_difs[at][4] : -1, n, g_difs.size());
    };
    auto lvseq = [&](const Result& L) { std::string s; for (auto& c : L.lvchg) s += " " + std::to_string(c.second); return s; };
    // una cadena de niveles: want = los niveles a los que tiene que ir pasando; fin = el nivel en que acaba el modelo
    auto chain = [&](const char* title, long maxf, const std::vector<int>& want, bool fatal) {
      printf("-- %s --\n", title);
      std::vector<uint8_t> i1 = sdfiles["NIVEL1.I16"], m1 = sdfiles["NIVEL1.MAP"];
      Level L1 = mklevel(i1, m1); g_map = L1.map;
      std::vector<Key> sc = gen(maxf - 60); sc.push_back({maxf - 50, maxf - 46, 0, 2});
      g_chk_speed = true;
      Result L = run(L1, sc, maxf, 150, true);
      g_chk_speed = false; g_map = lv.map;
      if (fatal) { g_exp_open.push_back("NIVEL3.I16"); g_exp_open.push_back("NIVEL1.I16"); }   // el Z80 sale en mitad de un update: el modelo no llega a verlo
      std::string got = lvseq(L), w;
      for (int x : want) w += " " + std::to_string(x);
      printf("        %ld imagenes; niveles a los que pasa:%s; jefes derribados %d; sprites escritos hasta %ld T; hasta %d sprites en una linea; vuelta mas larga %ld T\n",
             L.frames, got.c_str(), L.bkills, spr_maxlat, L.maxline, L.maxloop);
      bool seq = L.lvchg.size() >= want.size();
      for (size_t i = 0; seq && i < want.size(); i++) if (L.lvchg[i].second != want[i]) seq = false;
      if (!seq) printf("        esperaba:%s\n", w.c_str());
      char m[300];
      snprintf(m, sizeof m, "%s: tras cada jefe pasa a los niveles%s (el siguiente; tras el 5 o si falta o no vale, el 1)", title, w.c_str());
      CHECK(seq, m);
      if (!L.spr_bad.empty()) printf("        sprites mal en %zu imagenes, la primera %d\n", L.spr_bad.size(), L.spr_bad[0]);
      printf("        imagenes de cada cambio:"); for (auto& c : L.lvchg) printf(" %ld", c.first); printf("; spr_bad %zu, ay_bad %d, vis_bad %d (ok %d), pal_bad %zu\n", L.spr_bad.size(), L.ay_bad, L.vis_bad, L.vis_ok, L.pal_bad.size());
      snprintf(m, sizeof m, "%s: en cada imagen los 64 sprites (con la cifra del nivel en el 54 y la mejora esperando), los AY, lo visible y la paleta son los del modelo", title);
      CHECK(L.spr_bad.empty() && L.ay_bad == 0 && L.vis_bad == 0 && L.pal_bad.empty() && L.vis_ok >= 20 && L.spr_checked > (fatal ? 500 : maxf - 150 - 100 * (int)want.size()), m);   // (cada carga de un nivel son unas 40 imagenes sin escribir nada; si falla el siguiente, se cargan dos)
      snprintf(m, sizeof m, "%s: en cada imagen, nivel, vueltas, dificultad (vidas y salvas del jefe, cadencias) y velocidad del nivel de la memoria del Z80 son los del modelo", title);
      CHECK(L.dif_bad == 0, m);
      snprintf(m, sizeof m, "%s: abre los ficheros que espera el modelo, en orden (NIVELn.I16, NIVELn.MAP del siguiente; si falla, los del 1)", title);
      CHECK(opened_matches() && (fatal || !g_fatal), m);
      if (!opened_matches()) { printf("        abiertos:"); for (auto& n : opened) printf(" %s", n.c_str()); printf("\n        esperados:"); for (auto& n : g_exp_open) printf(" %s", n.c_str()); printf("\n"); }
      CHECK(spr_maxlat < 12000 && L.lost_frames <= 1, "los sprites se escriben antes de 12.000 T y no se pierden lineas (como mucho una imagen)");
      return L;
    };
    // --- todos los niveles, con musica salvo la del 3: 1 2 3 4 5 y vuelta al 1 (+1 de dificultad), 2
    build({1, 2, 3, 4, 5}, {3});
    Result L = chain("niveles 1 a 5 y vuelta", 9500, {2, 3, 4, 5, 1, 2}, false);
    chk_difs("niveles 1 a 5 y vuelta", 7);
    CHECK(L.laps >= 6 && L.bkills >= 6, "seis jefes derribados y seis cambios de nivel");
    exit_checks("niveles 1 a 5 y vuelta", 0);
    long f2 = L.lvchg.empty() ? 1000 : L.lvchg[0].first;
    // --- solo el 1 (una instalacion antigua): tras el jefe, otra vez el 1 con una vuelta mas, y otra; luego se queda en LAPMAX
    build({1}, {});
    L = chain("solo el nivel 1", 7000, {1, 1, 1}, false);
    chk_difs("solo el nivel 1", 3);   // (1,0), (1,1) con d = 5 y (1,2) con d = 10
    exit_checks("solo el nivel 1", 0);
    // --- NIVEL2.MAP que no cabe (longitud 8 KB + 1: error 5) -> al 1 con una vuelta mas
    build({1, 2}, {});
    sdfiles["NIVEL2.MAP"][12] = 0x01; sdfiles["NIVEL2.MAP"][13] = 0x20;
    L = chain("NIVEL2.MAP que no cabe", 4500, {1, 1}, false);
    exit_checks("NIVEL2.MAP que no cabe", 0);
    // --- NIVEL3.I16 que no es un .I16: 1 2 (3 mal) 1 2
    build({1, 2, 3}, {});
    sdfiles["NIVEL3.I16"][3] = '1';
    L = chain("NIVEL3.I16 sin firma", 6500, {2, 1, 2}, false);
    chk_difs("NIVEL3.I16 sin firma", 4);
    exit_checks("NIVEL3.I16 sin firma", 0);
    // --- el 1 desaparece de la SD tras empezar: 1 2 (3 falta) (el 1 falta): el Z80 sale con el error 1
    build({1, 2}, {});
    g_vanish = "NIVEL1.I16";
    L = chain("el nivel 1 desaparece de la SD", 4500, {2}, true);
    g_vanish.clear();
    CHECK(L.bc == 1, "ni el 3 ni el 1 se cargan (aperturas comprobadas arriba): el Z80 sale en mitad de la partida con BC = 1 (no cuelga ni sigue)");
    exit_checks("el nivel 1 desaparece", 1, 1);

    // --- fin de partida y R en el nivel 2: R vuelve al principio del 2 (misma dificultad) y tras el fin de partida ESPACIO vuelve al 1 con la dificultad inicial
    {
      build({1, 2, 3}, {});
      printf("-- R en el nivel 2 y fin de partida en el nivel 2: ESPACIO vuelve al nivel 1 --\n");
      long upto = f2 + 25, maxf = f2 + 2600;
      std::vector<Key> sc = gen(upto);
      sc.push_back({f2 + 10, f2 + 13, 2, 3});                   // R: el nivel 2 desde el principio
      sc.push_back({upto, upto + 4, 5, 2});                      // I: otra vez vidas normales
      for (long t = upto + 5; t < maxf - 60; t += 12) sc.push_back({t, t + 3, 7, 0});    // ESPACIO a golpes (no se queda disparando)
      sc.push_back({maxf - 50, maxf - 46, 0, 2});
      Level L1 = mklevel(sdfiles["NIVEL1.I16"], sdfiles["NIVEL1.MAP"]); g_map = L1.map; g_chk_speed = true;
      L = run(L1, sc, maxf, 150, true);
      g_chk_speed = false; g_map = lv.map;
      printf("        niveles:%s; %d fines de partida, %d partidas nuevas; sprites escritos hasta %ld T\n", lvseq(L).c_str(), L.gameovers, L.newgames, spr_maxlat);
      if (!L.spr_bad.empty()) printf("        sprites mal en %zu imagenes, la primera %d\n", L.spr_bad.size(), L.spr_bad[0]);
      bool seq = L.lvchg.size() >= 2 && L.lvchg[0].second == 2 && L.lvchg[1].second == 1;
      CHECK(seq && L.gameovers >= 1 && L.newgames >= 1, "con el nivel 2 en juego: fin de partida y, con ESPACIO, vuelve al nivel 1");
      CHECK(g_difs.size() >= 4 && g_difs[1][0] == 2 && g_difs[2][0] == 2 && g_difs[2][1] == 0 && g_difs[3][0] == 1 && g_difs[3][1] == 0 && g_difs[3][2] == EXPB[0] && g_difs[3][3] == EXPF[0] && g_difs[3][4] == EXPE[0],
            "R se queda en el nivel 2 (dificultad 1) y el fin de partida vuelve al 1 con la dificultad inicial (40 vidas, salvas cada 30, cadencia 96)");
      CHECK(L.spr_bad.empty() && L.ay_bad == 0 && L.vis_bad == 0 && L.pal_bad.empty() && L.dif_bad == 0 && opened_matches(),
            "los 64 sprites, los AY, lo visible, la paleta, la dificultad y los ficheros abiertos, iguales al modelo");
      exit_checks("R y fin de partida en el nivel 2", 0);
    }

    // --- pruebas en negativo: cambios a proposito en el codigo y el arnes tiene que verlos
    {
      std::vector<uint8_t> good = bin;
      auto find_in = [&](int lo, int hi, const std::vector<uint8_t>& pat) {
        int at = -1, n = 0; int top = std::min(hi - 0x4800, (int)good.size()); for (int i = lo - 0x4800; i + (int)pat.size() <= top; i++) if (!memcmp(&good[i], pat.data(), pat.size())) { at = i; n++; } return n == 1 ? at : -1; };
      struct Neg { const char* what; int lo, hi; std::vector<uint8_t> pat; int off; uint8_t val; int kind; } negs[] = {
        { "la cifra del nivel un pixel mas abajo", 0x4800, 0x7000, {0x1E, (uint8_t)(32 + 76), 0x2E, (uint8_t)(32 + 46), 0xCD}, 3, (uint8_t)(32 + 47), 0 },
        { "9 vidas del jefe por nivel en lugar de 10", S("setdif"), S("drawspr"), {0x7D, 0xC6, 0x0A, 0x6F}, 2, 9, 1 },
        { "la cadencia sin el minimo de la mitad (srl a -> nop nop)", S("setdif"), S("drawspr"), {0xCB, 0x3F, 0xB9, 0x38}, 0, 0, 2 },
        { "sin musica de respaldo: el 1 se vuelve a probar con la del 1", S("music"), S("sfxgo"), {0x3A, (uint8_t)(S("lvlno") & 255), (uint8_t)(S("lvlno") >> 8), 0xFE, 0x01}, 4, 9, 3 } };
      for (auto& n : negs) {
        int at = find_in(n.lo, n.hi, n.pat);
        CHECK(at >= 0, "(prueba en negativo) el trozo de codigo a cambiar esta una sola vez");
        if (at < 0) continue;
        bin = good; bin[at + n.off] = n.val; if (n.kind == 2) bin[at + 1] = 0;
        int e0 = errors;
        bool seen;
        if (n.kind == 3) {                                  // sin NIVEL1.VGM el Z80 solo debe probar una vez; con el cambio, dos
          build({1}, {1}); sdfiles.erase("NIVEL1.VGM");
          Level L1 = mklevel(sdfiles["NIVEL1.I16"], sdfiles["NIVEL1.MAP"]); g_map = L1.map;
          run(L1, { {60, 64, 0, 2} }, 120, 0, false); g_map = lv.map;
          seen = !mlog_matches();
        } else {
          build(n.kind == 0 ? std::vector<int>{1} : std::vector<int>{1, 2}, {});
          Level L1 = mklevel(sdfiles["NIVEL1.I16"], sdfiles["NIVEL1.MAP"]); g_map = L1.map; g_chk_speed = true;
          long mf = n.kind == 0 ? 150 : f2 + 60;
          std::vector<Key> sc = gen(mf - 20); sc.push_back({mf - 15, mf - 11, 0, 2});
          Result N = run(L1, sc, mf, 0, false);
          g_chk_speed = false; g_map = lv.map;
          seen = n.kind == 0 ? !N.spr_bad.empty() : N.dif_bad > 0;
          if (n.kind != 0) printf("        (%d imagenes con la memoria distinta del modelo)\n", N.dif_bad);
        }
        errors = e0;                                       // los fallos de dentro de run no cuentan: se buscan
        char m[240]; snprintf(m, sizeof m, "(prueba en negativo) con %s, el arnes lo ve", n.what);
        CHECK(seen, m);
      }
      bin = good;
    }
    sdfiles.clear();
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
    CHECK(R.ay_bad == 0, "sin NIVEL1.VGM el juego sigue igual, con los efectos");
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
    if (e.patch == 3) { int len = 0x2001; mp[12] = (uint8_t)len; mp[13] = (uint8_t)(len >> 8); }
    if (e.patch == 4) { mp[6] = 5; mp[7] = 0; }
    if (e.patch == 5) { mp[6] = (uint8_t)(lv.rows + 1); mp[7] = (uint8_t)((lv.rows + 1) >> 8); }
    sdfiles.clear(); if (e.i16) sdfiles["NIVEL1.I16"] = i16; if (e.map) sdfiles["NIVEL1.MAP"] = mp; if (e.patch != 6) sdfiles["HOJA.BIN"] = hoja;
    run(lv, {}, 60, 0, false);
    exit_checks(e.what, e.bc);
  }

  printf("%s (%d errores)\n", errors ? "FALLOS" : "TODO OK", errors);
  return errors ? 1 : 0;
}
