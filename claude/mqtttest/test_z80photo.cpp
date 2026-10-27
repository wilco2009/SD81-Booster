// Prueba de EXAMPLES/PHOTO/photo.bin en el PC: el Z80 de verdad (nucleo de EightyOne) ejecuta el visor con la memoria, los POKEs de
// configuracion, el teclado y un MCU simulado (fopen / fread / fclose sobre una carpeta, como en el arnes de SCUMM), y un modelo de
// la paleta de la FPGA (POKE 2063 puntero, 2064 byte bajo, 2065 byte alto: escribe la entrada y avanza).
//
// Cada vez que se completan 256 escrituras de paleta se toma una instantanea (paleta + $8000-$BE7F + ultimo POKE 2045). El visor
// escribe una paleta negra al arrancar y dos por imagen: la negra (antes de leer) y la de verdad (despues de leer la imagen entera). Se comprueba:
//   - la paleta negra es toda ceros y la de verdad es la del fichero IMGnnn.I16 (bytes 4-515)
//   - en esa instantanea la imagen en $8000 ya es la del fichero (bytes 516-16515)
//   - el modo (POKE 2045,175) se enciende despues de la primera paleta negra y antes de la primera de verdad
//   - N / B recorren los numeros que existen (saltan huecos y ficheros que no son .I16, y dan la vuelta), y ESPACIO sale con BC = 0
//   - sin imagenes: sale solo, con BC = 1 y el modo apagado (POKE 2045,85)
//   - no escribe fuera del programa, la imagen, la pila y los POKEs de configuracion
// NO prueba la FPGA (tb_mode160.v) ni la placa.
//
//   ./build_photo.sh        (necesita g++/gcc, pasmo, python y el nucleo Z80 de EightyOne)
//   test_z80photo photo.bin carpeta_SD secuencia teclas   (secuencia: numeros esperados de IMGnnn, por ejemplo 1,2,4,5,1,5, o "" = sin imagenes;
//                                                          teclas: una por imagen tras la primera, N / B / S = ESPACIO, por ejemplo NNNNBS)
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
static long instr_total = 0;
static int errors = 0;
#define CHECK(c, msg) do { if (!(c)) { printf("  ERROR: %s\n", msg); errors++; } else printf("  ok    %s\n", msg); } while (0)

// ---- paleta de la FPGA y POKEs
static uint16_t pal[256];
static int pal_idx = 0; static uint8_t pal_lo = 0;
static long pal_writes = 0;
static int last2045 = -1, first_175_at_snap = -1;
struct Snap { uint16_t pal[256]; uint8_t img[16000]; int mode; };
static std::vector<Snap*> snaps;
static bool bad_write = false; static uint16_t bad_addr = 0;
static uint16_t code_hi = 30000;
static int poke_other = 0; static uint16_t poke_other_addr = 0;

// ---- MCU simulado (ordenes 53 fopen, 55 fread, 57 fclose)
static std::string sd_dir;
static int mcu_clk = 0;
static std::vector<uint8_t> mcu_out;
static size_t mcu_outpos = 0;
static FILE* mcu_files[8];
static std::string mcu_name;
static int mcu_cmd = -1, mcu_cnt = 0, mcu_nlen = 0, mcu_h = 0, mcu_n = 0;
static int opens_ok = 0, opens_fail = 0;
static void mcu_byte(uint8_t b) {
  if (mcu_cmd < 0) { mcu_cmd = b; mcu_cnt = 0; mcu_name.clear(); mcu_nlen = 0; return; }
  if (mcu_cmd == 53) {
    if (mcu_cnt == 0) { mcu_nlen = b; mcu_cnt = 1; }
    else {
      mcu_name += (char)b;
      if ((int)mcu_name.size() == mcu_nlen) {
        FILE* f = fopen((sd_dir + "/" + mcu_name).c_str(), "rb");
        int h = 0xFF;
        if (f) for (int i = 0; i < 8; i++) if (!mcu_files[i]) { mcu_files[i] = f; h = i; break; }
        if (h == 0xFF) opens_fail++; else opens_ok++;
        mcu_out.push_back((uint8_t)h);
        mcu_cmd = -1;
      }
    }
  } else if (mcu_cmd == 55) {
    if (mcu_cnt == 0) { mcu_h = b & 7; mcu_cnt = 1; }
    else if (mcu_cnt == 1) { mcu_n = b; mcu_cnt = 2; }
    else {
      mcu_n |= b << 8;
      FILE* f = mcu_files[mcu_h];
      for (int i = 0; i < mcu_n; i++) { int c = f ? fgetc(f) : 0; mcu_out.push_back(c < 0 ? 0 : c); }
      mcu_out.push_back(0);
      mcu_cmd = -1;
    }
  } else if (mcu_cmd == 57) {
    int h = b & 7;
    if (mcu_files[h]) { fclose(mcu_files[h]); mcu_files[h] = nullptr; }
    mcu_out.push_back(0);
    mcu_cmd = -1;
  } else { printf("orden del MCU desconocida: %d\n", mcu_cmd); exit(3); }
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
    if (a == 2063) pal_idx = d & 255;
    else if (a == 2064) pal_lo = (uint8_t)d;
    else if (a == 2065) {
      pal[pal_idx] = (uint16_t)((d << 8) | pal_lo); pal_idx = (pal_idx + 1) & 255;
      if (++pal_writes % 256 == 0) {
        Snap* s = new Snap; memcpy(s->pal, pal, sizeof pal); memcpy(s->img, mem + 0x8000, 16000); s->mode = last2045;
        if (last2045 == 175 && first_175_at_snap < 0) first_175_at_snap = (int)snaps.size();
        snaps.push_back(s);
      }
    }
    else if (a == 2045) last2045 = d;
    else if (a == 2043 || a == 2044) { }
    else if (!poke_other) { poke_other = 1; poke_other_addr = (uint16_t)a; }
    return;
  }
  bool ok = (a >= 0x8000 && a < 0xBE80) || (a >= 30000 && a < code_hi) || a >= 0xFF00;
  if (!ok && !bad_write) { bad_write = true; bad_addr = (uint16_t)a; }
  mem[a] = (uint8_t)d;
}
void z80_writeport_wrapper(int port, int d, int*) {
  int lo = port & 0xFF;
  if (lo == 0xA7) { mcu_byte(d & 255); mcu_clk ^= 1; }
}
BYTE z80_readport_wrapper(int port, int*) {
  int lo = port & 0xFF;
  if (lo == 0xAF) return (mcu_clk << 7) | 0x02;
  if (lo == 0xA7) {
    uint8_t v = mcu_outpos < mcu_out.size() ? mcu_out[mcu_outpos++] : 0;
    if (mcu_outpos >= mcu_out.size()) { mcu_out.clear(); mcu_outpos = 0; }
    mcu_clk ^= 1;
    return v;
  }
  if (lo == 0xFE) {
    uint8_t v = 0x1F;
    for (int r = 0; r < 8; r++) if (!(((port >> 8) >> r) & 1)) v &= (uint8_t)~keyrows[r];
    return (uint8_t)(v | 0xE0);
  }
  return 0xFF;
}
}

static bool load_i16(int n, uint8_t* pal512, uint8_t* pix) {
  char name[64]; snprintf(name, sizeof name, "%s/IMG%03d.I16", sd_dir.c_str(), n);
  FILE* f = fopen(name, "rb"); if (!f) return false;
  uint8_t hdr[4];
  bool ok = fread(hdr, 1, 4, f) == 4 && !memcmp(hdr, "I160", 4) && fread(pal512, 1, 512, f) == 512 && fread(pix, 1, 16000, f) == 16000;
  fclose(f); return ok;
}

int main(int argc, char** argv) {
  if (argc < 3) { printf("uso: test_z80photo photo.bin carpeta_SD [secuencia]\n"); return 2; }
  sd_dir = argv[2];
  std::vector<int> seq;
  if (argc > 3) { char* s = argv[3]; while (*s) { seq.push_back(atoi(s)); while (*s && *s != ',') s++; if (*s == ',') s++; } }
  FILE* f = fopen(argv[1], "rb");
  if (!f) { printf("no encuentro %s\n", argv[1]); return 2; }
  size_t sz = fread(mem + 30000, 1, 0x4000, f); fclose(f);
  code_hi = (uint16_t)(30000 + sz);
  printf("photo.bin: %zu bytes en 30000-%u\n", sz, 30000 + (unsigned)sz - 1);
  CHECK(30000 + sz < 0x8000, "el programa queda por debajo de $8000");
  memset(mem + 0x8000, 0xEE, 0x4000);

  z80_init(); z80_reset();
  z80.sp.w = 0xFFF0; mem[0xFFF0] = 0x00; mem[0xFFF1] = 0x01; mem[0x0100] = 0x76;
  z80.pc.w = 30000; z80.halted = 0; z80.iff1 = z80.iff2 = 0;
  z80.ix.w = 0x1234; z80.iy.w = 0x5678;

  // teclas: tras cada imagen cargada (paleta de verdad terminada) se pulsa la siguiente: N = fila 7 bit 3, B = fila 7 bit 4, ESPACIO = fila 7 bit 0
  std::vector<int> keyseq;
  const char* kdesc = argc > 4 ? argv[4] : "";                 // por ejemplo "NNNNBS": N, B o S (ESPACIO), una por imagen tras la primera
  for (size_t i = 0; kdesc[i]; i++) keyseq.push_back(kdesc[i] == 'N' ? 3 : kdesc[i] == 'B' ? 4 : 0);
  size_t next_key = 0; long press_from = -1, press_to = -1; int cur_bit = 0; size_t last_snaps = 0;
  long guard = 0;
  while (z80.pc.w != 0x0100) {
    z80_do_opcode(); instr_total++;
    if (snaps.size() != last_snaps) {
      last_snaps = snaps.size();
      if (last_snaps >= 3 && last_snaps % 2 == 1 && next_key < keyseq.size()) { press_from = instr_total + 3000; press_to = press_from + 4000; cur_bit = keyseq[next_key++]; }
    }
    memset(keyrows, 0, sizeof keyrows);
    if (instr_total >= press_from && instr_total < press_to) keyrows[7] = (uint8_t)(1 << cur_bit);
    if (++guard > 2000000000L) { printf("ERROR: el Z80 no termina (PC=%04X)\n", z80.pc.w); return 1; }
  }
  bool done = (z80.pc.w == 0x0100);

  CHECK(done, "el programa vuelve (RET)");
  CHECK(!bad_write, "no escribe fuera del programa, la imagen y la pila");
  if (bad_write) printf("        primera escritura rara en $%04X\n", bad_addr);
  CHECK(!poke_other, "solo los POKEs 2043-2045 y 2063-2065");
  if (poke_other) printf("        POKE raro en %u\n", poke_other_addr);
  CHECK(last2045 == 85, "al salir, POKE 2045,85 (modo normal)");

  if (seq.empty()) {
    CHECK(z80.bc.w == 1, "sin imagenes: devuelve BC = 1");
    CHECK(snaps.size() == 2 || snaps.size() == 1 || snaps.size() == 0, "sin imagenes: no carga ninguna paleta de verdad");
    int real = 0; for (size_t i = 0; i < snaps.size(); i++) { bool z = true; for (int k = 0; k < 256; k++) if (snaps[i]->pal[k]) z = false; if (!z) real++; }
    CHECK(real == 0, "sin imagenes: ninguna paleta distinta de negro");
    printf("%s (%d errores)\n", errors ? "FALLOS" : "TODO OK", errors);
    return errors ? 1 : 0;
  }

  CHECK(z80.bc.w == 0, "ESPACIO: devuelve BC = 0");
  CHECK(snaps.size() == 2 * seq.size() + 1, "una paleta negra al arrancar y dos por imagen (negra y de verdad), ni una mas");
  CHECK(first_175_at_snap == 1, "el modo 160x100 se enciende despues de la primera paleta negra y antes de la primera de verdad");
  int blk_bad = 0, pal_bad = 0, img_bad = 0, mode_bad = 0;
  for (size_t i = 0; i < snaps.size(); i++) {
    Snap* s = snaps[i];
    if (i == 0 || i % 2 == 1) {                              // negra (la primera, al arrancar, y la de antes de cada carga)
      for (int k = 0; k < 256; k++) if (s->pal[k]) { blk_bad++; break; }
    } else {                                                 // de verdad: la del fichero, con la imagen ya cargada
      uint8_t p512[512], pix[16000];
      int n = seq[i / 2 - 1];
      if (!load_i16(n, p512, pix)) { printf("  ERROR: no puedo leer IMG%03d.I16 para comparar\n", n); errors++; continue; }
      for (int k = 0; k < 256; k++) if (s->pal[k] != (uint16_t)((p512[2 * k + 1] << 8) | p512[2 * k])) { pal_bad++; break; }
      if (memcmp(s->img, pix, 16000)) img_bad++;
      if (s->mode != 175) mode_bad++;
    }
  }
  CHECK(blk_bad == 0, "las paletas previas a cada carga son negras");
  CHECK(pal_bad == 0, "cada paleta de verdad es la del fichero de la imagen que toca (N / B, huecos, fichero malo y vuelta)");
  CHECK(img_bad == 0, "en ese momento la imagen de $8000 ya es la del fichero (no se ve nunca a medias con la paleta buena)");
  CHECK(mode_bad == 0, "con el modo 175 activo en todas las cargas");
  CHECK(opens_ok >= (int)seq.size(), "se abrieron los ficheros");
  printf("aperturas: %d bien, %d fallidas\n", opens_ok, opens_fail);
  printf("%s (%d errores)\n", errors ? "FALLOS" : "TODO OK", errors);
  return errors ? 1 : 0;
}
