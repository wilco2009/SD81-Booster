// Prueba de EXAMPLES/IMGVIEW/imgview.bin en el PC: el Z80 de verdad (nucleo de EightyOne) ejecuta el visor del explorador con la memoria, los POKEs,
// el teclado, las variables BASIC (F$ en la zona de VARS) y un MCU simulado (fopen / fread / fclose de una carpeta y la orden 76, que ejecuta el
// MISMO BMP.cpp del firmware), mas un modelo de la paleta de la FPGA (POKE 2063 / 2064 / 2065).
//
// Se toma una instantanea cada 256 escrituras de paleta (paleta + $8000-$BE7F + ultimo POKE 2045). El visor escribe dos paletas por carga: la negra
// (antes de leer) y la de verdad (despues de leer la imagen entera). Se comprueba:
//   - el codigo que devuelve (BC): 0 bien, 1 no se puede abrir (sin F$, ni .BMP ni .I16, no existe, .I16 malo), 2 BMP que no se convierte
//   - en cada carga: la paleta negra y luego la del fichero (o la del BMP convertido con la colocacion que toca) con la imagen ya completa y el modo
//     160x100 encendido (POKE 2045,175) antes de la primera paleta de verdad
//   - F cambia la colocacion de un BMP (1 -> 2 -> 0 -> 1) y recarga; con un .I16 no hace nada. La F rebota (8 ms alternando al pulsar y al soltar):
//     no debe tomarse por otra tecla y salir; cualquier otra tecla sale
//   - al salir: POKE 2045,85, $7FEF a $0C, y no queda ninguna imagen virtual abierta ni se abre una con otra abierta
//   - los errores no tocan la pantalla (ni 2045 ni la paleta)
//   - no escribe fuera del programa, la imagen, la pila y los POKEs de configuracion
// NO prueba la FPGA (tb_mode160.v) ni la placa.
//
//   ./build_imgview.sh
//   test_z80imgview imgview.bin carpeta_SD NOMBRE npulsaciones_F codigo_esperado cargas   (cargas: la colocacion de cada carga, "-" si es un .I16: 1,2,0,1)
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include "BMP.h"
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
static uint64_t total_t = 0;
static uint64_t sim_us() { return total_t * 1000ULL / 3250ULL; }          // Z80 a 3,25 MHz
static bool f_down = false; static uint64_t f_t = 0;                      // la F: estado real y cuando cambio (rebota 8 ms)
static bool f_read() {
  uint64_t d = sim_us() - f_t;
  if (d < 8000) { bool odd = ((d / 150) & 1) != 0; return f_down ? odd : !odd; }
  return f_down;
}
static int errors = 0;
#define CHECK(c, msg) do { if (!(c)) { printf("  ERROR: %s\n", msg); errors++; } else printf("  ok    %s\n", msg); } while (0)

// ---- paleta de la FPGA y POKEs
static uint16_t pal[256];
static int pal_idx = 0; static uint8_t pal_lo = 0;
static long pal_writes = 0;
static int last2045 = -1, n2045_175 = 0, n2045_85 = 0, first_175_at_snap = -1, chroma_last = -1;
struct Snap { uint16_t pal[256]; uint8_t img[16000]; int mode; };
static std::vector<Snap*> snaps;
static bool bad_write = false; static uint16_t bad_addr = 0;
static uint16_t code_hi = 24576;
static int poke_other = 0; static uint16_t poke_other_addr = 0;

// ---- BMP.cpp del firmware sobre un fichero
class FileRd : public BmpReader {
public:
  explicit FileRd(FILE* f) : f_(f) { fseek(f_, 0, SEEK_END); size_ = (uint32_t)ftell(f_); fseek(f_, 0, SEEK_SET); }
  uint32_t size() override { return size_; }
  bool seek(uint32_t pos) override { return pos <= size_ && fseek(f_, (long)pos, SEEK_SET) == 0; }
  int read(uint8_t* dst, int n) override { return (int)fread(dst, 1, (size_t)n, f_); }
private:
  FILE* f_; uint32_t size_;
};
static bool convert_bmp(const std::string& path, int fit, std::vector<uint8_t>& out) {
  FILE* f = fopen(path.c_str(), "rb");
  if (!f) return false;
  out.assign(BMP_I16_SIZE, 0);
  BmpWork* w = (BmpWork*)malloc(sizeof(BmpWork));
  BmpError err;
  FileRd rd(f);
  bool ok = bmp_to_i16(rd, (uint8_t)fit, out.data(), *w, err);
  free(w);
  fclose(f);
  return ok;
}

// ---- MCU simulado (ordenes 53 fopen, 55 fread, 57 fclose y 76 fopen de un BMP como .I16)
static std::string sd_dir;
static int mcu_clk = 0;
static std::vector<uint8_t> mcu_out;
static size_t mcu_outpos = 0;
static FILE* mcu_files[8];
static std::string mcu_name;
static int mcu_cmd = -1, mcu_cnt = 0, mcu_nlen = 0, mcu_h = 0, mcu_n = 0;
static std::vector<uint8_t> mcu_virt; static int mcu_virt_h = -1; static size_t mcu_virt_pos = 0;
static int bmp_ok = 0, bmp_fail = 0, bmp_busy = 0, bmp_missing = 0, i16_ok = 0, i16_missing = 0;
static std::vector<int> bmp_fits;                                    // la colocacion pedida en cada conversion que salio bien
static int mcu_free_slot() { for (int i = 0; i < 4; i++) if (!mcu_files[i] && mcu_virt_h != i) return i; return -1; }
static void mcu_byte(uint8_t b) {
  if (mcu_cmd < 0) { mcu_cmd = b; mcu_cnt = 0; mcu_name.clear(); mcu_nlen = 0; return; }
  if (mcu_cmd == 76) {
    if (mcu_cnt == 0) { mcu_nlen = b; mcu_cnt = mcu_nlen ? 1 : 2; }
    else if (mcu_cnt == 1) { mcu_name += (char)b; if ((int)mcu_name.size() == mcu_nlen) mcu_cnt = 2; }
    else {
      int fit = b, res;
      int slot = mcu_free_slot();
      if (mcu_virt_h >= 0) { res = 0xFC; bmp_busy++; }
      else if (slot < 0) res = 0xFC;
      else {
        std::string path = sd_dir + "/" + mcu_name;
        FILE* t = fopen(path.c_str(), "rb");
        if (!t) { res = 0xFF; bmp_missing++; }
        else {
          fclose(t);
          if (convert_bmp(path, fit, mcu_virt)) { mcu_virt_h = slot; mcu_virt_pos = 0; res = slot; bmp_ok++; bmp_fits.push_back(fit); }
          else { res = 0xFE; bmp_fail++; }
        }
      }
      mcu_out.push_back((uint8_t)res);
      mcu_cmd = -1;
    }
  } else if (mcu_cmd == 53) {
    if (mcu_cnt == 0) { mcu_nlen = b; mcu_cnt = 1; }
    else {
      mcu_name += (char)b;
      if ((int)mcu_name.size() == mcu_nlen) {
        FILE* f = fopen((sd_dir + "/" + mcu_name).c_str(), "rb");
        int h = 0xFF;
        if (f) { int sl = mcu_free_slot(); if (sl >= 0) { mcu_files[sl] = f; h = sl; } else fclose(f); }
        if (h == 0xFF) i16_missing++; else i16_ok++;
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
      if (mcu_h == mcu_virt_h) {
        for (int i = 0; i < mcu_n; i++) mcu_out.push_back(mcu_virt_pos < mcu_virt.size() ? mcu_virt[mcu_virt_pos++] : 0);
      } else
        for (int i = 0; i < mcu_n; i++) { int c = f ? fgetc(f) : 0; mcu_out.push_back(c < 0 ? 0 : c); }
      mcu_out.push_back(0);
      mcu_cmd = -1;
    }
  } else if (mcu_cmd == 57) {
    int h = b & 7;
    if (h == mcu_virt_h) { mcu_virt_h = -1; mcu_virt.clear(); }
    if (mcu_files[h]) { fclose(mcu_files[h]); mcu_files[h] = nullptr; }
    mcu_out.push_back(0);
    mcu_cmd = -1;
  } else { printf("orden del MCU desconocida: %d\n", mcu_cmd); exit(3); }
}

extern "C" {
int  z80_contend_wrapper(int, int, int time) { return time; }
int  z80_contend_io_wrapper(int, int, int time) { return time; }
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
    else if (a == 2045) { last2045 = d; if (d == 175) n2045_175++; if (d == 85) n2045_85++; }
    else if (a == 2043 || a == 2044) { }
    else if (!poke_other) { poke_other = 1; poke_other_addr = (uint16_t)a; }
    return;
  }
  bool ok = (a >= 0x8000 && a < 0xBE80) || (a >= 24576 && a < code_hi) || a >= 0xFF00;
  if (!ok && !bad_write) { bad_write = true; bad_addr = (uint16_t)a; }
  mem[a] = (uint8_t)d;
}
void z80_writeport_wrapper(int port, int d, int*) {
  int lo = port & 0xFF;
  if (lo == 0xA7) { mcu_byte(d & 255); mcu_clk ^= 1; }
  else if (lo == 0xEF && (port >> 8) == 0x7F) chroma_last = d & 255;
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
    uint8_t rows[8]; memcpy(rows, keyrows, sizeof rows);
    if (f_read()) rows[1] |= 8;                         // F: fila A-G ($FD), bit 3
    uint8_t v = 0x1F;
    for (int r = 0; r < 8; r++) if (!(((port >> 8) >> r) & 1)) v &= (uint8_t)~rows[r];
    return (uint8_t)(v | 0xE0);
  }
  return 0xFF;
}
}

// el expected de una carga: el .I16 del fichero o el .BMP convertido con esa colocacion
static bool expected_image(const std::string& name, int fit, uint8_t* pal512, uint8_t* pix) {
  std::string path = sd_dir + "/" + name;
  size_t dot = name.rfind('.');
  std::string ext = dot == std::string::npos ? "" : name.substr(dot);
  if (ext == ".BMP") {
    std::vector<uint8_t> v;
    if (!convert_bmp(path, fit, v)) return false;
    memcpy(pal512, v.data() + 4, 512); memcpy(pix, v.data() + 516, 16000);
    return true;
  }
  FILE* f = fopen(path.c_str(), "rb"); if (!f) return false;
  uint8_t hdr[4];
  bool ok = fread(hdr, 1, 4, f) == 4 && !memcmp(hdr, "I160", 4) && fread(pal512, 1, 512, f) == 512 && fread(pix, 1, 16000, f) == 16000;
  fclose(f); return ok;
}

static uint8_t zx_code(char c) {                        // ASCII -> codigo ZX81 (letras, cifras y algunos signos)
  if (c >= '0' && c <= '9') return (uint8_t)(28 + c - '0');
  if (c >= 'A' && c <= 'Z') return (uint8_t)(38 + c - 'A');
  if (c == '.') return 27;
  if (c == '-') return 22;
  return 0;
}

int main(int argc, char** argv) {
  if (argc < 7) { printf("uso: test_z80imgview imgview.bin carpeta_SD NOMBRE|- npulsaciones_F codigo_esperado cargas\n"); return 2; }
  sd_dir = argv[2];
  std::string name = argv[3];
  bool no_fvar = name == "-";
  int npress = atoi(argv[4]);
  int want_rc = atoi(argv[5]);
  std::vector<int> loads;                                // la colocacion de cada carga (-1 = .I16)
  for (char* s = argv[6]; *s;) {
    if (*s == '-') { loads.push_back(-1); s++; }
    else if (*s >= '0' && *s <= '9') { loads.push_back(atoi(s)); while (*s >= '0' && *s <= '9') s++; }
    else s++;
    if (*s == ',') s++;
  }
  FILE* f = fopen(argv[1], "rb");
  if (!f) { printf("no encuentro %s\n", argv[1]); return 2; }
  size_t sz = fread(mem + 24576, 1, 0x4000, f); fclose(f);
  code_hi = (uint16_t)(24576 + sz);
  CHECK(24576 + sz < 0x8000, "el programa queda por debajo de $8000");
  memset(mem + 0x8000, 0xEE, 0x4000);
  mem[0x02E7] = 0xC9; mem[0x0207] = 0xC9;                // SET_FAST y SLOW_FAST de la ROM: un RET

  // variables BASIC: algo antes (A = 5, una cadena X$ y un bucle FOR), luego F$ y el fin ($80); VARS (16400) apunta a ellas
  uint16_t v = 0x4800;
  mem[16400] = v & 255; mem[16401] = v >> 8;
  mem[v++] = 0x60 | 1; for (int i = 0; i < 5; i++) mem[v++] = 0;             // A = numero de una letra (5 bytes)
  mem[v++] = 0x40 | 24; mem[v++] = 3; mem[v++] = 0; mem[v++] = 38; mem[v++] = 39; mem[v++] = 40;   // X$ = "ABC"
  if (!no_fvar) {
    mem[v++] = 0x4B; mem[v++] = (uint8_t)name.size(); mem[v++] = 0;
    for (char c : name) mem[v++] = zx_code(c);
  }
  mem[v++] = 0x80;

  z80_init(); z80_reset();
  z80.sp.w = 0xFFF0; mem[0xFFF0] = 0x00; mem[0xFFF1] = 0x01; mem[0x0100] = 0x76;
  z80.pc.w = 24576; z80.halted = 0; z80.iff1 = z80.iff2 = 0;
  z80.ix.w = 0x1234; z80.iy.w = 0x4000;

  // teclas: F (hasta npress, una por carga completa) y despues ESPACIO (fila 7 bit 0). Cada pulsacion dura 100 ms
  size_t expect_snaps = 0; int pressed = 0; bool space_sent = false;
  uint64_t t_press = 0; bool f_active = false; uint64_t space_from = 0, space_to = 0; uint64_t ready_t = 0;
  long guard = 0;
  while (z80.pc.w != 0x0100) {
    z80_do_opcode(); total_t += (uint64_t)tstates;
    if (snaps.size() != expect_snaps && snaps.size() % 2 == 0) {                 // una carga completa: se pulsa lo siguiente 80 ms despues
      expect_snaps = snaps.size();
      ready_t = sim_us() + 80000;
    }
    if (snaps.size() % 2 == 0 && !snaps.empty() && ready_t && sim_us() >= ready_t && !f_active && !space_sent) {
      ready_t = 0;
      if (pressed < npress) { f_down = true; f_t = sim_us(); t_press = f_t; f_active = true; pressed++; }
      else { space_from = sim_us(); space_to = space_from + 100000; space_sent = true; }
    }
    if (f_active && sim_us() >= t_press + 100000) {
      f_down = false; f_t = sim_us(); f_active = false;
      if ((size_t)pressed >= loads.size()) ready_t = sim_us() + 300000;       // esta F no recarga nada (un .I16, o las de sobra): la siguiente tecla, sin esperar a una carga
    }
    memset(keyrows, 0, sizeof keyrows);
    if (space_sent && sim_us() >= space_from && sim_us() < space_to) keyrows[7] = 1;
    // un error sale sin cargar nada: no hay instantaneas; el programa termina solo
    if (++guard > 2000000000L) { printf("ERROR: el Z80 no termina (PC=%04X)\n", z80.pc.w); return 1; }
    if (sim_us() > 60ULL * 1000000ULL) { printf("ERROR: demasiado tiempo simulado (PC=%04X, %zu instantaneas)\n", z80.pc.w, snaps.size()); return 1; }
  }
  CHECK(z80.pc.w == 0x0100, "el programa vuelve (RET)");
  CHECK((int)z80.bc.w == want_rc, "devuelve el codigo esperado en BC");
  if ((int)z80.bc.w != want_rc) printf("        BC = %u, esperado %d\n", z80.bc.w, want_rc);
  CHECK(!bad_write, "no escribe fuera del programa, la imagen y la pila");
  if (bad_write) printf("        primera escritura rara en $%04X\n", bad_addr);
  CHECK(!poke_other, "solo los POKEs 2043-2045 y 2063-2065");
  CHECK(mcu_virt_h < 0, "no queda ninguna imagen virtual abierta");
  CHECK(bmp_busy == 0, "nunca se abre un BMP con otra imagen abierta");
  CHECK(z80.iy.w == 0x4000, "conserva IY (la ROM del ZX81 lo necesita)");

  if (want_rc != 0) {
    CHECK(snaps.empty(), "con un error no se toca la paleta");
    CHECK(n2045_175 == 0 && last2045 == -1, "con un error no se enciende el modo (ni se apaga)");
    printf("%s (%d errores)\n", errors ? "FALLOS" : "TODO OK", errors);
    return errors ? 1 : 0;
  }

  CHECK(snaps.size() == 2 * loads.size(), "dos paletas por carga (la negra y la de verdad), ni una mas");
  CHECK(n2045_175 == 1, "el modo 160x100 se enciende una sola vez");
  CHECK(first_175_at_snap == 1, "y despues de la primera paleta negra y antes de la primera de verdad");
  CHECK(last2045 == 85 && n2045_85 == 1, "al salir, POKE 2045,85 (modo normal)");
  CHECK(chroma_last == 0x0C, "al salir, $7FEF a $0C (color apagado)");
  int blk_bad = 0, pal_bad = 0, img_bad = 0, mode_bad = 0;
  for (size_t i = 0; i < snaps.size() && i / 2 < loads.size(); i++) {
    Snap* s = snaps[i];
    if (i % 2 == 0) {
      for (int k = 0; k < 256; k++) if (s->pal[k]) { blk_bad++; break; }
    } else {
      uint8_t p512[512], pix[16000];
      if (!expected_image(name, loads[i / 2] < 0 ? 0 : loads[i / 2], p512, pix)) { printf("  ERROR: no puedo calcular lo esperado para %s\n", name.c_str()); errors++; continue; }
      for (int k = 0; k < 256; k++) if (s->pal[k] != (uint16_t)((p512[2 * k + 1] << 8) | p512[2 * k])) { pal_bad++; break; }
      if (memcmp(s->img, pix, 16000)) img_bad++;
      if (s->mode != 175) mode_bad++;
    }
  }
  CHECK(blk_bad == 0, "las paletas previas a cada carga son negras");
  CHECK(pal_bad == 0, "cada paleta de verdad es la del fichero o la del BMP convertido con la colocacion que toca");
  CHECK(img_bad == 0, "y en ese momento la imagen de $8000 ya esta completa");
  CHECK(mode_bad == 0, "con el modo 175 activo en todas las cargas");
  std::vector<int> want_fits;
  for (int l : loads) if (l >= 0) want_fits.push_back(l);
  CHECK(bmp_fits == want_fits, "las colocaciones pedidas al MCU son las esperadas (1, 2, 0, 1 con cada F)");
  if (bmp_fits != want_fits) { printf("        pedidas:"); for (int x : bmp_fits) printf(" %d", x); printf("\n"); }
  printf("MCU: %d .I16 abiertos, %d BMP convertidos, %d no existen como BMP\n", i16_ok, bmp_ok, bmp_missing);
  printf("%s (%d errores)\n", errors ? "FALLOS" : "TODO OK", errors);
  return errors ? 1 : 0;
}
