// Estado del MCU en los snapshots .Z81 (ver MCUSTATE.h). Las claves y su
// formato son los de EightyOne (zx81/snap.cpp, save_snap_sd81booster):
//   AY2_REGS r0..r15, AY2_REG_SEL n      el AY del MCU (PLAY, VGM, PEG). No
//                                        tiene registro elegido: siempre 00
//   VGM_PATH ruta|-, VGM_PLAYING 0/1/2 (parado, sonando, en pausa),
//   VGM_LOOP modo, VGM_POS posicion (8 cifras)
//   PEG_MEM (256 palabras), PEG_PC/PEG_ADDR/PEG_RUNNING/PEG_CARRY (3 hilos),
//   PEG_VARS (48 palabras: variable*3 + hilo)
//   FILE_HANDLE n ruta posicion          uno por fichero abierto
// Las rutas son absolutas en la SD. Una ruta con espacios no se guarda (el
// formato separa por espacios).
#include <Arduino.h>
#include <stdarg.h>
#include <SdFat.h>
#include "MCUSTATE.h"
#include "GLOBALS.h"
#include "VGM.h"
#include "PEG.h"
#include "ay_emulator.h"

extern uint16_t V[16][NUM_THREADS];          // PEG.cpp
extern uint8_t carry[NUM_THREADS];

// Lo leido del .Z81, hasta que se aplica
static struct {
  bool ay2; uint8_t ay2_regs[16];
  bool vgm; char vgm_path[MAX_FILENAME_LEN]; uint8_t vgm_state, vgm_loop; uint32_t vgm_pos;
  bool peg; uint16_t mem[256]; uint8_t pc[NUM_THREADS], addr[NUM_THREADS], run[NUM_THREADS], carry[NUM_THREADS];
  uint16_t vars[16 * NUM_THREADS];
  bool fh[4]; char fh_path[4][MAX_FILENAME_LEN]; uint32_t fh_pos[4];
} in;

static void wf(void (*w)(const char*), const char* fmt, ...){
  char b[MAX_FILENAME_LEN + 40];
  va_list a; va_start(a, fmt); vsnprintf(b, sizeof(b), fmt, a); va_end(a);
  w(b);
}

static void words(void (*w)(const char*), const char* key, const uint16_t* v, int n){
  wf(w, "%s\n", key);
  for (int i = 0; i < n; i++) wf(w, (i & 15) == 15 ? "%04X\n" : "%04X ", v[i]);
}

void mcustate_save(void (*w)(const char* s)){
  w("AY2_REGS");
  for (int r = 0; r < 16; r++) wf(w, " %02X", ay_registers[r]);
  w("\nAY2_REG_SEL 00\n");

  bool vgm = VGMFile.isOpen() && vgm_path[0] && !strchr(vgm_path, ' ');
  wf(w, "VGM_PATH %s\n", vgm ? vgm_path : "-");
  wf(w, "VGM_PLAYING %02X\n", !vgm ? 0 : playing_VGM ? 1 : 2);
  wf(w, "VGM_LOOP %02X\n", VGM_mode);
  wf(w, "VGM_POS %08lX\n", vgm ? (unsigned long)VGMFile.curPosition() : 0UL);

  words(w, "PEG_MEM", PEG_mem, 256);
  w("PEG_PC");      for (int i = 0; i < NUM_THREADS; i++) wf(w, " %02X", PEG_pc[i]);
  w("\nPEG_ADDR");  for (int i = 0; i < NUM_THREADS; i++) wf(w, " %02X", PEG_addr[i]);
  w("\nPEG_RUNNING"); for (int i = 0; i < NUM_THREADS; i++) wf(w, " %02X", playing_PEG[i] ? 1 : 0);
  w("\nPEG_CARRY"); for (int i = 0; i < NUM_THREADS; i++) wf(w, " %02X", carry[i]);
  w("\n");
  words(w, "PEG_VARS", &V[0][0], 16 * NUM_THREADS);

  for (int i = 0; i < 4; i++)
    if (f_opened[i] && f_path[i][0] && !strchr(f_path[i], ' '))
      wf(w, "FILE_HANDLE %02X %s %08lX\n", i, f_path[i], (unsigned long)f_handle[i].curPosition());
}

void mcustate_clear(void){
  memset(&in, 0, sizeof(in));
}

static bool vals(uint8_t (*tok)(char*, uint8_t), uint32_t* v, int n){
  char t[12];
  for (int i = 0; i < n; i++) {
    if (!tok(t, sizeof(t))) return false;
    v[i] = strtoul(t, nullptr, 16);
  }
  return true;
}

bool mcustate_key(const char* k, uint8_t (*tok)(char* b, uint8_t max)){
  uint32_t v[16];
  char t[MAX_FILENAME_LEN];
  if (!strcmp(k, "AY2_REGS")) {
    if (vals(tok, v, 16)) { for (int i = 0; i < 16; i++) in.ay2_regs[i] = v[i]; in.ay2 = true; }
  } else if (!strcmp(k, "AY2_REG_SEL")) {
    vals(tok, v, 1);
  } else if (!strcmp(k, "VGM_PATH")) {
    if (tok(t, sizeof(t))) { strcpy(in.vgm_path, strcmp(t, "-") ? t : ""); in.vgm = true; }
  } else if (!strcmp(k, "VGM_PLAYING")) {
    if (vals(tok, v, 1)) { in.vgm_state = v[0]; in.vgm = true; }
  } else if (!strcmp(k, "VGM_LOOP")) {
    if (vals(tok, v, 1)) in.vgm_loop = v[0];
  } else if (!strcmp(k, "VGM_POS")) {
    if (vals(tok, v, 1)) in.vgm_pos = v[0];
  } else if (!strcmp(k, "PEG_MEM") || !strcmp(k, "PEG_VARS")) {
    bool mem = !strcmp(k, "PEG_MEM");
    uint16_t* d = mem ? in.mem : in.vars;
    int n = mem ? 256 : 16 * NUM_THREADS;
    for (int i = 0; i < n; i++) { if (!vals(tok, v, 1)) break; d[i] = v[0]; }
    in.peg = true;
  } else if (!strcmp(k, "PEG_PC") || !strcmp(k, "PEG_ADDR") || !strcmp(k, "PEG_RUNNING") || !strcmp(k, "PEG_CARRY")) {
    uint8_t* d = !strcmp(k, "PEG_PC") ? in.pc : !strcmp(k, "PEG_ADDR") ? in.addr : !strcmp(k, "PEG_RUNNING") ? in.run : in.carry;
    if (vals(tok, v, NUM_THREADS)) for (int i = 0; i < NUM_THREADS; i++) d[i] = v[i];
    in.peg = true;
  } else if (!strcmp(k, "FILE_HANDLE")) {
    if (!vals(tok, v, 1) || !tok(t, sizeof(t))) return true;
    uint8_t h = v[0];
    if (!vals(tok, v, 1) || h >= 4) return true;
    strcpy(in.fh_path[h], t);
    in.fh_pos[h] = v[0];
    in.fh[h] = true;
  } else return false;
  return true;
}

void mcustate_apply(void){
  // VGM: el del snapshot o ninguno (que no siga sonando el de antes)
  playing_VGM = false;
  if (in.vgm && in.vgm_state && in.vgm_path[0] && openVGM(in.vgm_path) == 0) {
    if (in.vgm_pos) VGMFile.seekSet(in.vgm_pos);
    VGM_mode = in.vgm_loop;
    ti_VGM = 0;
    playing_VGM = (in.vgm_state == 1);
  }
  // el AY del MCU
  if (in.ay2) {
    memcpy(ay_registers, in.ay2_regs, 16);
    ay_emulator_apply_register_changes();
  }
  // PEG
  if (in.peg) {
    memcpy(PEG_mem, in.mem, sizeof(PEG_mem));
    memcpy(&V[0][0], in.vars, sizeof(in.vars));
    for (int i = 0; i < NUM_THREADS; i++) {
      PEG_pc[i] = in.pc[i]; PEG_addr[i] = in.addr[i]; carry[i] = in.carry[i];
      playing_PEG[i] = in.run[i] != 0;
    }
  } else
    for (int i = 0; i < NUM_THREADS; i++) playing_PEG[i] = false;
  // ficheros: se cierran los de antes y se abren los del snapshot
  for (int i = 0; i < 4; i++) {
    if (f_opened[i]) { f_handle[i].close(); f_opened[i] = false; }
    if (in.fh[i] && f_handle[i].open(in.fh_path[i], O_RDWR)) {
      f_handle[i].seekSet(in.fh_pos[i]);
      f_opened[i] = true;
      strcpy(f_path[i], in.fh_path[i]);
    }
  }
}
