// Depurador por hardware, lado del MCU (fase 2).
//
// El monitor del Z80 (z80rom/debugmon.asm, en la pagina 63) es un servidor
// minimo: al parar manda los registros (DBG_BREAK) y despues pide
// peticiones una tras otra (DBG_POLL), devolviendo el resultado de la
// anterior. Aqui esta toda la logica: la consola, el desensamblado y los
// breakpoints por software.
//
// DBG_POLL no contesta hasta tener una peticion: el ultimo byte que manda el
// Z80 se queda sin confirmar (el Z80 espera en su mcu_send) y el comando se
// queda activo; el loop lo vuelve a llamar en cada vuelta y no se bloquea.
//
// Breakpoints por software: un FF (RST 38h) en memoria, que la FPGA ve y
// convierte en una ruptura. Solo estan puestos mientras el programa corre:
// al parar se quitan todos (asi la memoria y el desensamblado se ven
// limpios) y al continuar se vuelven a poner, leyendo cada vez el byte
// original. Si hay uno en el PC, se da antes un paso sin el.
#include <Arduino.h>
#include "DEBUGGER.h"
#include "COMMS.h"
#include "COMMANDS.h"
#include "GLOBALS.h"
#include "PINS.h"
#include "SD_handle.h"
#include "z80-disassembler.h"

#define DBG_PORT 0x3FEF

// Peticiones al monitor
#define OP_READ    1
#define OP_WRITE   2
#define OP_SETREGS 3
#define OP_OUT     4
#define OP_IN      5
#define OP_CONT    6

// Que hacer con el resultado
enum {
  TAG_NONE,
  TAG_SHOW,         // desensamblar en PC y ensenar los registros
  TAG_DUMP,         // volcado de memoria
  TAG_DISASM,       // desensamblado (arg = instrucciones)
  TAG_BP_ORIG,      // byte original de un breakpoint (arg = indice)
  TAG_BP_CHECK,     // tras un reset: hay un FF puesto? (arg = indice)
  TAG_IN            // lectura de un puerto
};

// Bloque de registros (ver debugmon.asm)
enum {
  R_HL_ = 0, R_DE_ = 2, R_BC_ = 4, R_AF_ = 6, R_IY = 8, R_IX = 10,
  R_HL = 12, R_DE = 14, R_BC = 16, R_AF = 18, R_SP = 20, R_PC = 22,
  R_I = 24, R_R = 25, R_IFF = 26, R_DBGST = 27, R_SIMST = 28, R_PAGE1 = 29,
  REGS_LEN = 30
};

struct DbgReq {
  uint8_t op;
  uint16_t addr;
  uint16_t n;
  uint8_t tag;
  uint8_t arg;
  uint8_t data[32];
};

#define QSIZE 48
static DbgReq q[QSIZE];
static uint8_t q_head = 0, q_count = 0;

#define NBP 16
struct Bp {
  bool used;
  uint16_t addr;
  uint8_t orig;
  uint8_t state;      // 0 quitado, 1 puesto (FF en memoria), 2 desconocido (tras un reset)
};
static Bp bps[NBP];

static uint8_t regs[REGS_LEN];
static bool stopped = false;
static uint8_t poll_state = 0;     // 0: recibir el resultado; 1: esperar peticion
static DbgReq last;                // la peticion cuyo resultado llega en el siguiente POLL
static int8_t internal_step = -1;  // breakpoint saltado con un paso (indice)
static bool stop_foreign = false;  // parado en un RST 38h del programa (FF que no es nuestro)
static uint8_t pause_tgl = 0;
static uint8_t res[256];

// ---------------------------------------------------------------------
// Cola de peticiones
// ---------------------------------------------------------------------
static DbgReq* q_push(uint8_t op, uint16_t addr, uint16_t n, uint8_t tag = TAG_NONE, uint8_t arg = 0, bool front = false){
  if (q_count >= QSIZE) return nullptr;
  uint8_t idx;
  if (front) {
    q_head = (q_head + QSIZE - 1) % QSIZE;
    idx = q_head;
  } else idx = (q_head + q_count) % QSIZE;
  q_count++;
  DbgReq* r = &q[idx];
  r->op = op; r->addr = addr; r->n = n; r->tag = tag; r->arg = arg;
  return r;
}

static void q_write1(uint16_t addr, uint8_t v, bool front = false){
  DbgReq* r = q_push(OP_WRITE, addr, 1, TAG_NONE, 0, front);
  if (r) r->data[0] = v;
}

static void q_out(uint16_t port, uint8_t v){
  q_push(OP_OUT, port, v);
}

static void q_setregs(){
  DbgReq* r = q_push(OP_SETREGS, 0, REGS_LEN);
  if (r) memcpy(r->data, regs, REGS_LEN);
}

static uint16_t reg16(int i){ return regs[i] | (regs[i+1] << 8); }
static void set_reg16(int i, uint16_t v){ regs[i] = v & 0xFF; regs[i+1] = v >> 8; }

static int bp_find(uint16_t addr){
  for (int i = 0; i < NBP; i++) if (bps[i].used && bps[i].addr == addr) return i;
  return -1;
}

// ---------------------------------------------------------------------
// Salida por la consola
// ---------------------------------------------------------------------
static const char* reason_name(uint8_t st){
  switch (st & 7) {
    case 1: return "pause (MCU)";
    case 2: return "pause (joystick)";
    case 3: return "trap (OUT $10)";
    case 4: return "step";
    case 5: return "comparator (exec)";
    case 6: return "watchpoint";
    case 7: return stop_foreign ? "RST 38h (not a breakpoint)" : "breakpoint";
  }
  return "?";
}

static void print_regs(){
  char b[160];
  uint8_t f = regs[R_AF];
  snprintf(b, sizeof(b), "PC=%04X SP=%04X AF=%04X BC=%04X DE=%04X HL=%04X IX=%04X IY=%04X",
    reg16(R_PC), reg16(R_SP), reg16(R_AF), reg16(R_BC), reg16(R_DE), reg16(R_HL), reg16(R_IX), reg16(R_IY));
  Serial.println(b);
  snprintf(b, sizeof(b), "AF'=%04X BC'=%04X DE'=%04X HL'=%04X I=%02X R=%02X IFF=%d  %c%c-%c-%c%c%c",
    reg16(R_AF_), reg16(R_BC_), reg16(R_DE_), reg16(R_HL_), regs[R_I], regs[R_R], regs[R_IFF],
    (f & 0x80) ? 'S' : 's', (f & 0x40) ? 'Z' : 'z', (f & 0x10) ? 'H' : 'h',
    (f & 0x04) ? 'P' : 'p', (f & 0x02) ? 'N' : 'n', (f & 0x01) ? 'C' : 'c');
  Serial.println(b);
}

// Desensambla count instrucciones de bytes (leidos desde addr). Devuelve los
// bytes usados.
static int print_disasm(uint16_t addr, uint8_t* bytes, int len, int count){
  char text[64], b[100];
  int pos = 0;
  for (int k = 0; k < count && pos < len; k++) {
    int used = Z80Disassembler::disassemble(text, bytes + pos, len - pos);
    if (used <= 0) used = 1;
    int p = snprintf(b, sizeof(b), "%c%04X  ", bp_find(addr + pos) >= 0 ? '*' : ' ', (uint16_t)(addr + pos));
    for (int j = 0; j < 4; j++)
      p += snprintf(b + p, sizeof(b) - p, j < used ? "%02X" : "  ", bytes[pos + j]);
    snprintf(b + p, sizeof(b) - p, "  %s", text);
    Serial.println(b);
    pos += used;
  }
  return pos;
}

static void print_dump(uint16_t addr, uint8_t* bytes, int len){
  char b[100];
  for (int i = 0; i < len; i += 16) {
    int p = snprintf(b, sizeof(b), "%04X ", (uint16_t)(addr + i));
    for (int j = 0; j < 16; j++)
      p += snprintf(b + p, sizeof(b) - p, (i + j < len) ? " %02X" : "   ", bytes[i + j]);
    p += snprintf(b + p, sizeof(b) - p, "  ");
    for (int j = 0; j < 16 && i + j < len; j++) {
      uint8_t c = bytes[i + j];
      b[p++] = (c >= 32 && c < 127) ? c : '.';
    }
    b[p] = 0;
    Serial.println(b);
  }
}

// ---------------------------------------------------------------------
// Parar y continuar
// ---------------------------------------------------------------------
static void queue_show(){
  q_push(OP_READ, reg16(R_PC), 4, TAG_SHOW);
}

// Quitar de la memoria los breakpoints puestos
static void queue_remove_bps(){
  for (int i = 0; i < NBP; i++) {
    if (!bps[i].used) continue;
    if (bps[i].state == 1) { q_write1(bps[i].addr, bps[i].orig); bps[i].state = 0; }
    else if (bps[i].state == 2) q_push(OP_READ, bps[i].addr, 1, TAG_BP_CHECK, i);
  }
}

// Poner los breakpoints (menos el de except): leer el original y escribir FF
static void queue_insert_bps(int except){
  for (int i = 0; i < NBP; i++) {
    if (!bps[i].used || i == except || bps[i].state == 1) continue;
    q_push(OP_READ, bps[i].addr, 1, TAG_BP_ORIG, i);
    q_write1(bps[i].addr, 0xFF);
  }
}

// Un RST 38h de verdad del programa (FF que no es un breakpoint): al seguir,
// se hace lo que haria la CPU: guardar PC+1 y saltar a $0038. Se decide al
// parar (stop_foreign): si despues se borra el breakpoint en el que se ha
// parado, su FF ya se quito de la memoria y no hay RST que emular.
static bool foreign_rst(){
  return stop_foreign;
}
static void queue_emulate_rst(){
  uint16_t sp = reg16(R_SP) - 2, ret = reg16(R_PC) + 1;
  DbgReq* r = q_push(OP_WRITE, sp, 2);
  if (r) { r->data[0] = ret & 0xFF; r->data[1] = ret >> 8; }
  set_reg16(R_SP, sp);
  set_reg16(R_PC, 0x0038);
  stop_foreign = false;                      // ya no es un RST pendiente
  q_setregs();
}

static void queue_steps(uint16_t n){
  q_out(DBG_PORT, 0x83); q_out(DBG_PORT, n & 0xFF);
  q_out(DBG_PORT, 0x84); q_out(DBG_PORT, n >> 8);
}

static void resumed(){
  stopped = false;
  set_status_led_ok();
}

static void dbg_continue(){
  if (foreign_rst()) queue_emulate_rst();
  int at = bp_find(reg16(R_PC));
  queue_insert_bps(at);
  if (at >= 0) {                              // primero un paso sin ese breakpoint
    queue_steps(1);
    internal_step = at;
  }
  q_push(OP_CONT, 0, 0);
  resumed();
}

static void dbg_step(uint16_t n){
  if (foreign_rst()) queue_emulate_rst();
  queue_steps(n);
  q_push(OP_CONT, 0, 0);
  resumed();
}

// Ha llegado DBG_BREAK con los registros
static void on_break(){
  q_count = 0;
  poll_state = 0;
  stopped = true;
  if (internal_step >= 0) {
    int i = internal_step;
    internal_step = -1;
    if ((regs[R_DBGST] & 7) == 4) {           // el paso para saltar el breakpoint:
      q_push(OP_READ, bps[i].addr, 1, TAG_BP_ORIG, i);   // ponerlo y seguir
      q_write1(bps[i].addr, 0xFF);
      q_push(OP_CONT, 0, 0);
      resumed();
      return;
    }
  }
  stop_foreign = (regs[R_DBGST] & 7) == 7 && bp_find(reg16(R_PC)) < 0;
  set_status_LED(clMAGENTA);
  char b[80];
  snprintf(b, sizeof(b), "\r\n*** STOP at %04X: %s%s", reg16(R_PC), reason_name(regs[R_DBGST]),
    (regs[R_DBGST] & 0x20) ? " (inside the simulated interrupt)" : "");
  Serial.println(b);
  queue_remove_bps();
  queue_show();
}

// Ha llegado el resultado de la peticion last
static void on_result(uint8_t* data, uint16_t n){
  char b[80];
  switch (last.tag) {
    case TAG_SHOW:
      print_regs();
      print_disasm(last.addr, data, n, 1);
      snprintf(b, sizeof(b), "block 1 page %d, sim_int %02X. Type h for help.", regs[R_PAGE1], regs[R_SIMST]);
      Serial.println(b);
      break;
    case TAG_DUMP:
      print_dump(last.addr, data, n);
      break;
    case TAG_DISASM:
      print_disasm(last.addr, data, n, last.arg);
      break;
    case TAG_BP_ORIG:
      if (n) { bps[last.arg].orig = data[0]; bps[last.arg].state = 1; }
      break;
    case TAG_BP_CHECK:                        // tras un reset: si sigue el FF, quitarlo
      if (n && data[0] == 0xFF) q_write1(bps[last.arg].addr, bps[last.arg].orig, true);
      bps[last.arg].state = 0;
      break;
    case TAG_IN:
      snprintf(b, sizeof(b), "IN (%04X) = %02X", last.addr, n ? data[0] : 0);
      Serial.println(b);
      break;
  }
}

// ---------------------------------------------------------------------
// Comandos del monitor
// ---------------------------------------------------------------------
// COMMAND = 73: cmd + 30 bytes de registros
void cmd_dbg_break(void){
  ToggleClock();                              // ACK
  for (int i = 0; i < REGS_LEN; i++) {
    if (i) ToggleClock();
    regs[i] = GetByteFromZ80_IT();
  }
  reset_commands();
  ToggleClock();                              // toggle final
  on_break();
}

// COMMAND = 74: cmd + n(2) + n bytes (resultado) -> op, dir(2), n(2) [+ datos]
void cmd_dbg_poll(void){
  if (poll_state == 0) {
    ToggleClock();                            // ACK
    uint8_t lo = GetByteFromZ80_IT();
    ToggleClock();
    uint8_t hi = GetByteFromZ80_IT();
    uint16_t n = lo | (hi << 8);
    for (uint16_t i = 0; i < n; i++) {
      ToggleClock();
      uint8_t v = GetByteFromZ80_IT();
      if (i < sizeof(res)) res[i] = v;
    }
    // el ultimo byte queda sin confirmar: el Z80 espera hasta la peticion
    if (n > sizeof(res)) n = sizeof(res);
    on_result(res, n);
    last.tag = TAG_NONE;
    poll_state = 1;
  }
  if (q_count == 0) return;                   // sin peticion: otra vuelta del loop
  poll_state = 0;                             // (antes de sacarla: ver dbg_waiting)
  DbgReq r = q[q_head];
  q_head = (q_head + 1) % QSIZE;
  q_count--;
  SendByteToZ80(r.op);                        // confirma el ultimo byte + peticion
  SendByteToZ80(r.addr & 0xFF);
  SendByteToZ80(r.addr >> 8);
  SendByteToZ80(r.n & 0xFF);
  SendByteToZ80(r.n >> 8);
  if (r.op == OP_WRITE || r.op == OP_SETREGS)
    for (uint16_t i = 0; i < r.n; i++) SendByteToZ80(r.data[i]);
  last = r;
  reset_commands();
  ToggleClock();                              // toggle final
}

// ---------------------------------------------------------------------
// Consola
// ---------------------------------------------------------------------
static bool parse_hex(const char*& p, uint32_t& v){
  while (*p == ' ') p++;
  if (*p == '$') p++;
  const char* s = p;
  v = 0;
  while (isxdigit(*p)) { v = v * 16 + (isdigit(*p) ? *p - '0' : (tolower(*p) - 'a' + 10)); p++; }
  return p != s;
}

static void help(){
  Serial.println("Debugger (hex numbers):");
  Serial.println("  p              pause the program");
  Serial.println("  r              registers and instruction at PC");
  Serial.println("  s [n]          step n instructions (1)");
  Serial.println("  c              continue");
  Serial.println("  b addr         set a breakpoint      bc [addr]  clear one / all      bl  list");
  Serial.println("  d [addr] [n]   disassemble n instructions (10) from addr (PC)");
  Serial.println("  m addr [n]     dump n bytes (64)");
  Serial.println("  e addr b1 b2.. write bytes");
  Serial.println("  x reg=val      set a register (af bc de hl ix iy af' bc' de' hl' sp pc i r)");
  Serial.println("  io port [val]  read (or write) an I/O port");
  Serial.println("  DBG_RELOAD     reload /SYS/DEBUG.BIN (like a reset)");
}

static bool set_reg(const char* p){
  static const struct { const char* name; int idx; bool wide; } names[] = {
    {"af'", R_AF_, true}, {"bc'", R_BC_, true}, {"de'", R_DE_, true}, {"hl'", R_HL_, true},
    {"af", R_AF, true}, {"bc", R_BC, true}, {"de", R_DE, true}, {"hl", R_HL, true},
    {"ix", R_IX, true}, {"iy", R_IY, true}, {"sp", R_SP, true}, {"pc", R_PC, true},
    {"i", R_I, false}, {"r", R_R, false}
  };
  while (*p == ' ') p++;
  for (auto& n : names) {
    size_t l = strlen(n.name);
    if (strncmp(p, n.name, l) == 0 && p[l] == '=') {
      const char* v = p + l + 1;
      uint32_t val;
      if (!parse_hex(v, val)) return false;
      if (n.wide) set_reg16(n.idx, val); else regs[n.idx] = val;
      if (n.idx == R_PC) stop_foreign = false;   // ya no se esta sobre el RST que paro
      return true;
    }
  }
  return false;
}

bool dbg_console(const char* line){
  if (!(line[0] >= 'a' && line[0] <= 'z') && line[0] != '?') return false;
  if (!debug_monitor_loaded) { Serial.println("No debug monitor loaded (/SYS/DEBUG.BIN)"); return true; }
  const char* p = line;
  char cmd[4] = {0};
  for (int i = 0; i < 3 && *p && *p != ' '; i++) cmd[i] = *p++;
  if (*p && *p != ' ') { Serial.println("? (h for help)"); return true; }
  uint32_t a, n;

  if (!strcmp(cmd, "h") || !strcmp(cmd, "?")) { help(); return true; }
  if (!strcmp(cmd, "p")) { if (stopped) Serial.println("Already stopped"); else dbg_pause(); return true; }
  if (!stopped) { Serial.println("Running: p to pause"); return true; }

  if (!strcmp(cmd, "r")) queue_show();
  else if (!strcmp(cmd, "c")) dbg_continue();
  else if (!strcmp(cmd, "s")) dbg_step(parse_hex(p, n) && n ? n : 1);
  else if (!strcmp(cmd, "b")) {
    if (!parse_hex(p, a)) { Serial.println("b addr"); return true; }
    if (bp_find(a) >= 0) { Serial.println("Already set"); return true; }
    int i = 0;
    while (i < NBP && bps[i].used) i++;
    if (i == NBP) { Serial.println("No room for more breakpoints"); return true; }
    bps[i].used = true; bps[i].addr = a; bps[i].state = 0;
    Serial.printf("Breakpoint %d at %04X\r\n", i, (unsigned)a);
  }
  else if (!strcmp(cmd, "bc")) {
    if (parse_hex(p, a)) {
      int i = bp_find(a);
      if (i < 0) Serial.println("No breakpoint there"); else bps[i].used = false;
    } else for (int i = 0; i < NBP; i++) bps[i].used = false;
  }
  else if (!strcmp(cmd, "bl")) {
    bool any = false;
    for (int i = 0; i < NBP; i++) if (bps[i].used) { Serial.printf("%d: %04X\r\n", i, bps[i].addr); any = true; }
    if (!any) Serial.println("No breakpoints");
  }
  else if (!strcmp(cmd, "d")) {
    if (!parse_hex(p, a)) a = reg16(R_PC);
    if (!parse_hex(p, n) || !n) n = 10;
    if (n > 60) n = 60;
    q_push(OP_READ, a, n * 4, TAG_DISASM, n);
  }
  else if (!strcmp(cmd, "m")) {
    if (!parse_hex(p, a)) { Serial.println("m addr [n]"); return true; }
    if (!parse_hex(p, n) || !n) n = 64;
    if (n > 256) n = 256;
    q_push(OP_READ, a, n, TAG_DUMP);
  }
  else if (!strcmp(cmd, "e")) {
    uint8_t bytes[32];
    uint16_t cnt = 0;
    uint32_t v;
    if (parse_hex(p, a))
      while (cnt < sizeof(bytes) && parse_hex(p, v)) bytes[cnt++] = v;
    if (!cnt) { Serial.println("e addr b1 b2 ..."); return true; }
    DbgReq* r = q_push(OP_WRITE, a, cnt);
    if (r) memcpy(r->data, bytes, cnt);
  }
  else if (!strcmp(cmd, "x")) {
    if (set_reg(p)) { q_setregs(); print_regs(); }
    else Serial.println("x reg=val");
  }
  else if (!strcmp(cmd, "io")) {
    if (!parse_hex(p, a)) { Serial.println("io port [val]"); return true; }
    if (parse_hex(p, n)) q_out(a, n);
    else q_push(OP_IN, a, 0, TAG_IN);
  }
  else Serial.println("? (h for help)");
  return true;
}

// ---------------------------------------------------------------------
// Pausa, boton QuickSilva y reset
// ---------------------------------------------------------------------
void dbg_pause(void){
  if (!debug_monitor_loaded) { Serial.println("No debug monitor loaded"); return; }
  pause_tgl ^= 1;
  send_bit_config(cfgcmd_DBGPAUSE, pause_tgl);   // la FPGA reacciona al cambio
  Serial.println("Pause requested (waits for FAST if the program is in SLOW)");
}

bool dbg_is_stopped(void){ return stopped; }
bool dbg_waiting(void){ return stopped && poll_state == 1 && q_count == 0; }

void dbg_reset(void){
  stopped = false;
  poll_state = 0;
  q_count = 0;
  internal_step = -1;
  // los breakpoints puestos siguen en la tabla: los de la ROM siguen en
  // memoria y los de la RAM no se sabe; al parar se mira cada uno
  for (int i = 0; i < NBP; i++) if (bps[i].used && bps[i].state == 1) bps[i].state = 2;
  set_status_led_ok();
}

static bool qs_down = false;
static uint32_t qs_t0 = 0;
static uint8_t qs_stage = 0;       // 0 nada, 1 ya se hizo lo de 1 s, 2 ya se paso de 3 s

static void toggle_qs(){
  nQS_en = !nQS_en;
  send_bit_config(cfgcmd_QSEN, nQS_en ? 1 : 0);
  if (stopped) set_status_LED(clMAGENTA); else set_status_led_ok();
}

void dbg_qs_button(void){
  bool down = !digitalRead(QSPIN);
  uint32_t now = millis();
  if (down && !qs_down) { qs_down = true; qs_t0 = now; qs_stage = 0; return; }
  if (down) {
    if (!debug_monitor_loaded) return;
    if (qs_stage == 0 && now - qs_t0 >= 1000) {
      qs_stage = 1;
      if (stopped) { Serial.println("QS button: continue"); dbg_continue(); }
      else dbg_pause();
    }
    if (qs_stage == 1 && now - qs_t0 >= 3000) {
      qs_stage = 2;
      set_status_LED(clYELLOW);               // snapshot: fase 2b
    }
    return;
  }
  if (qs_down) {                               // al soltar
    qs_down = false;
    uint32_t t = now - qs_t0;
    if (t < 30) return;                        // rebote
    if (!debug_monitor_loaded || qs_stage == 0) toggle_qs();
    else if (qs_stage == 2) {
      Serial.println("Snapshot: not implemented yet (phase 2b)");
      if (stopped) set_status_LED(clMAGENTA); else set_status_led_ok();
    }
  }
}
