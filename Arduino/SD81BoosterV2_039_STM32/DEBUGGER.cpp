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
//
// Snapshots (fase 2b): con el programa parado, el MCU lee todo por el monitor
// y escribe un .Z81 como los de EightyOne ([CPU], [ZX81], [MEMORY],
// [COLOUR], [SD81BOOSTER] con el mapper, el estado de la FPGA y las paginas,
// [EOF]). Los POKEs de control salen de la BRAM de sombra ($3FEF, indice 2).
#include <Arduino.h>
#include <stdarg.h>
#include <stdlib.h>
#include <ctype.h>
#include "DEBUGGER.h"
#include "COMMS.h"
#include "COMMANDS.h"
#include "GLOBALS.h"
#include "PINS.h"
#include "SD_handle.h"
#include "z80-disassembler.h"
#include "MCUSTATE.h"

// Lo que el depurador escribe en la consola va tambien a un anillo de 4 KB,
// con un contador que solo crece: la interfaz web (ESP32, CMD_DBG) lo lee
// desde donde se quedo. Desde aqui, Serial es dbg_out
#define OUT_RING 4096
static char out_ring[OUT_RING];
static uint32_t out_head = 0;                 // bytes escritos desde el arranque
static void out_put(const char* s){ while (*s) out_ring[out_head++ % OUT_RING] = *s++; }
struct DbgOut {
  void println(const char* s){ Serial.println(s); out_put(s); out_put("\r\n"); }
  int printf(const char* f, ...){
    char b[256];
    va_list a;
    va_start(a, f);
    int n = vsnprintf(b, sizeof(b), f, a);
    va_end(a);
    Serial.print(b);
    out_put(b);
    return n;
  }
};
static DbgOut dbg_out;
#define Serial dbg_out

#define DBG_PORT 0x3FEF

// Peticiones al monitor
#define OP_READ    1
#define OP_WRITE   2
#define OP_SETREGS 3
#define OP_OUT     4
#define OP_IN      5
#define OP_CONT    6
#define OP_INSEQ   7      // n IN seguidos del mismo puerto
#define OP_READP   8      // n bytes de una pagina (mapeada un momento en el bloque 7)
#define OP_WRITEP  9      // n bytes en una pagina (igual que READP)
#define OP_BRAMW   10     // n bytes a la BRAM de sombra, en su puntero
#define OP_AYREAD  11     // n registros de un AY (puerto de seleccion)
#define OP_AYWRITE 12     // n registros de un AY
#define OP_SETI    13     // I mientras el monitor espera (0 = la del programa); version 4

#define AY_PORT_A  0x00CF // seleccion / lectura del AY A (ZonX) y del B
#define AY_PORT_B  0x00C7
#define SPR_MIRROR 0x0C00 // copia de los 32 sprites en la BRAM de sombra (32 x 32)

// Que hacer con el resultado
enum {
  TAG_NONE,
  TAG_SHOW,         // desensamblar en PC y ensenar los registros
  TAG_DUMP,         // volcado de memoria
  TAG_DISASM,       // desensamblado (arg = instrucciones)
  TAG_BP_ORIG,      // byte original de un breakpoint (arg = indice)
  TAG_BP_CHECK,     // tras un reset: hay un FF puesto? (arg = indice)
  TAG_IN,           // lectura de un puerto
  TAG_SNAP_MAP,     // snapshot: pagina de un bloque (arg = bloque)
  TAG_SNAP_CHROMA,  // snapshot: registro de Chroma81
  TAG_SNAP_POKES,   // snapshot: los POKEs de control (BRAM)
  TAG_SNAP_MEM,     // snapshot: [MEMORY]
  TAG_SNAP_COLOUR,  // snapshot: [COLOUR]
  TAG_SNAP_PAGE,    // snapshot: un trozo de pagina
  TAG_SNAP_AYSEL,   // snapshot: registro elegido de un AY (arg = 0 A, 1 B)
  TAG_SNAP_AYREGS,  // snapshot: los 16 registros de un AY (arg = 0 A, 1 B)
  TAG_SNAP_SPR,     // snapshot: un trozo de la copia de los sprites (arg = trozo)
  TAG_SNAP_SPRSEL,  // snapshot: el sprite elegido (2100)
  TAG_SNAP_SHCMP,   // snapshot: la sombra de un trozo de [MEMORY], para comparar
  TAG_SNAP_SHADOW,  // snapshot: un trozo de un bloque de la sombra
  TAG_LD_MAP,       // carga: pagina de un bloque (sin MAPPER; arg = bloque)
  TAG_LD_SYNC,      // carga: ya se han escrito los POKEs
  TAG_SNAP_DIRTY,   // snapshot: las paginas escritas (arg = byte 0-7)
  TAG_UNWIND,       // SLOW: parado en la entrada de la NMI, su vuelta ([SP])
  TAG_STEPOVER,     // o: la instruccion en el PC
  TAG_STEPOUT,      // u: la direccion de vuelta en [SP]
  TAG_VIEW,         // v: los POKEs de control en la sombra (2038-2098)
  TAG_VIEW_SYNC,    // v: ya se ha escrito el 2045 (orden 10 fuera)
  TAG_VIEW_SAVE,    // v dir: un trozo de la sombra que se va a pisar (arg = trozo)
  TAG_VIEW_COPY,    // v dir: un trozo del mapa de bits (arg = trozo)
  TAG_UI_PK,        // pantalla: los POKEs de control en la sombra
  TAG_UI_CHROMA,    // pantalla: el registro de Chroma81
  TAG_UI_SAVE,      // pantalla: un trozo de la sombra que se va a pisar (arg = trozo)
  TAG_UI_DIS,       // pantalla: bytes para el desensamblado
  TAG_UI_STK,       // pantalla: la pila
  TAG_UI_MEM,       // pantalla: el volcado de memoria
  TAG_UI_KBD,       // pantalla: una fila del teclado (arg = fila 0-7)
  TAG_UI_FIX,       // pantalla: tras un reset, los POKEs en la sombra
  TAG_TRACE,        // traza: la instruccion en el PC (antes de ejecutarla)
  TAG_TRACE_KEY,    // traza: la fila de ESPACIO, para pararla
  TAG_FT_PTR,       // historial (FPGA): el puntero (arg = 0 bajo, 1 alto)
  TAG_FT_RING,      // historial: un trozo del anillo
  TAG_FT_OP,        // historial: los bytes de una instruccion (arg = cual)
  TAG_WEB_DIS,      // web: los bytes del desensamblado
  TAG_WEB_STK,      // web: la pila
  TAG_WEB_MEM       // web: el volcado
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
  uint8_t page;       // OP_READP
  uint8_t data[32];
};

#define QSIZE 96
static DbgReq q[QSIZE];
static uint8_t q_head = 0, q_count = 0;

#define NBP 16
struct Bp {
  bool used;
  uint16_t addr;
  uint8_t orig;
  uint8_t state;      // 0 quitado, 1 puesto (FF en memoria), 2 desconocido (tras un reset)
  bool temp;          // de g/o/u: se quita al parar
};
static Bp bps[NBP];

// El comparador de la FPGA (uno solo): un punto de vigilancia (w) o, si esta
// libre y no es SLOW, el destino de g/o/u (ejecucion, temporal)
static uint8_t watch_mode = 0;      // 0 nada, 2 lectura, 3 escritura, 4 E/S
static uint16_t watch_addr = 0;
static bool cmp_tmp = false;        // el comparador lleva un g/o/u

static uint8_t regs[REGS_LEN];
static bool stopped = false;
static uint8_t poll_state = 0;     // 0: recibir el resultado; 1: esperar peticion
static DbgReq last;                // la peticion cuyo resultado llega en el siguiente POLL
static int8_t internal_step = -1;  // breakpoint saltado con un paso (indice)
static bool stop_foreign = false;  // parado en un RST 38h del programa (FF que no es nuestro)
static bool stop_nmi = false;      // parado en SLOW: la NMI estaba encendida (el monitor la apaga)
static bool prog_slow = false;     // el programa es SLOW: su NMI esta apagada hasta que se siga (c, g...)
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
  r->op = op; r->addr = addr; r->n = n; r->tag = tag; r->arg = arg; r->page = 0;
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

// El monitor escribe POKEs como el programa (orden 10). Con LOAD *ROMLOCK
// la FPGA no los haria: se quita un momento. poke_end(), cuando llegue la
// barrera (TAG_VIEW_SYNC)
static bool poke_relock = false;
static void poke_begin(){
  send_bit_config(cfgcmd_DBGPOKE, 1);
  if (cfg_value(cfgcmd_ROMLOCK)) { send_bit_config(cfgcmd_ROMLOCK, 0); poke_relock = true; }
}
static void poke_end(){
  send_bit_config(cfgcmd_DBGPOKE, 0);
  if (poke_relock) { poke_relock = false; send_bit_config(cfgcmd_ROMLOCK, 1); }
}

static uint16_t reg16(int i){ return regs[i] | (regs[i+1] << 8); }
static void set_reg16(int i, uint16_t v){ regs[i] = v & 0xFF; regs[i+1] = v >> 8; }

static int bp_find(uint16_t addr){
  for (int i = 0; i < NBP; i++) if (bps[i].used && bps[i].addr == addr) return i;
  return -1;
}

// ---------------------------------------------------------------------
// Simbolos (fase 5). El fichero de simbolos de pasmo (pasmo prog.asm
// prog.bin prog.sym: "NOMBRE<tab>EQU 0ABCDH"); tambien vale "NOMBRE: EQU
// $ABCD" o "NOMBRE = 0x1234". Al cargar un programa (LOAD) se apunta su
// nombre con .SYM y, en la parada siguiente, se lee si existe (si no, fuera
// los que hubiera). O a mano: sym fichero. Van en la CCM RAM (64 KB que el
// enlazador no usa), ordenados por valor; con valores repetidos, el primero
// del fichero
// ---------------------------------------------------------------------
#define SYM_MAX   2048
#define SYM_NAME  18                          // con el 0 final; los mas largos se cortan
struct Sym { uint16_t val; char name[SYM_NAME]; };

// La traza (mas abajo) va en la CCM detras de los simbolos: el estado antes
// de cada instruccion y sus bytes
#define TRACE_MAX 1024
struct TraceE { uint16_t pc, af, bc, de, hl, ix, iy, sp; uint8_t op[4]; };
static_assert(sizeof(Sym) * SYM_MAX + sizeof(TraceE) * TRACE_MAX <= 0x10000, "los simbolos y la traza no caben en la CCM RAM");
#ifdef ARDUINO_ARCH_STM32
static Sym* const syms = (Sym*)0x10000000;
static TraceE* const trace_buf = (TraceE*)(0x10000000 + sizeof(Sym) * SYM_MAX);
#else
static Sym syms_ram[SYM_MAX];                 // (en el PC, el arnes)
static Sym* const syms = syms_ram;
static TraceE trace_ram[TRACE_MAX];
static TraceE* const trace_buf = trace_ram;
#endif
static uint16_t sym_n = 0;
static char sym_src[96] = "";                 // de que fichero son
static char sym_auto[96] = "";                // el .SYM del ultimo programa cargado
static bool sym_auto_pend = false;            // mirarlo en la parada siguiente

static bool ci_eq(const char* a, const char* b){
  while (*a && toupper((uint8_t)*a) == toupper((uint8_t)*b)) { a++; b++; }
  return !*a && !*b;
}

static bool sym_num(const char* t, uint32_t* v){
  char b[16];
  size_t L = strlen(t);
  int base = 10;
  if (t[0] == '$' || t[0] == '#') { t++; L--; base = 16; }
  else if (t[0] == '0' && (t[1] == 'x' || t[1] == 'X')) { t += 2; L -= 2; base = 16; }
  else if (L && (t[L - 1] == 'h' || t[L - 1] == 'H')) { L--; base = 16; }
  if (!L || L >= sizeof(b)) return false;
  memcpy(b, t, L);
  b[L] = 0;
  char* e;
  *v = strtoul(b, &e, base);
  return *e == 0;
}

static void sym_line(char* l){
  char* c = strchr(l, ';');
  if (c) *c = 0;
  char* t[3];
  int n = 0;
  for (char* q = strtok(l, " \t"); q && n < 3; q = strtok(nullptr, " \t")) t[n++] = q;
  if (n < 3 || !(ci_eq(t[1], "EQU") || !strcmp(t[1], "="))) return;
  size_t L = strlen(t[0]);
  if (L && t[0][L - 1] == ':') t[0][--L] = 0;
  if (!L || !(isalpha((uint8_t)t[0][0]) || t[0][0] == '_' || t[0][0] == '.' || t[0][0] == '@')) return;
  uint32_t v;
  if (!sym_num(t[2], &v) || v > 0xFFFF || sym_n >= SYM_MAX) return;
  syms[sym_n].val = v;
  snprintf(syms[sym_n].name, SYM_NAME, "%s", t[0]);
  sym_n++;
}

// Simbolos leidos; -1 si no se puede abrir (entonces no se toca lo que hay)
static int sym_load(const char* path){
  if (!z81in_open(path)) return -1;
  sym_n = 0;
  char line[128];
  int len = 0, c;
  do {
    c = z81in_read();
    if (c >= 0 && c != '\n' && c != '\r') { if (len < (int)sizeof(line) - 1) line[len++] = c; continue; }
    line[len] = 0;
    if (len) sym_line(line);
    len = 0;
  } while (c >= 0);
  z81in_close();
  for (int i = 1; i < sym_n; i++) {           // por valor (insercion: estable)
    Sym t = syms[i];
    int j = i;
    while (j > 0 && syms[j - 1].val > t.val) { syms[j] = syms[j - 1]; j--; }
    syms[j] = t;
  }
  snprintf(sym_src, sizeof(sym_src), "%s", path);
  return sym_n;
}

static int sym_lower(uint16_t v){
  int lo = 0, hi = sym_n;
  while (lo < hi) { int m = (lo + hi) / 2; if (syms[m].val < v) lo = m + 1; else hi = m; }
  return lo;
}

static const char* sym_at(uint16_t v){
  int i = sym_lower(v);
  return (i < sym_n && syms[i].val == v) ? syms[i].name : nullptr;
}

// "NOMBRE" o "NOMBRE+n": el simbolo anterior, a menos de 256 bytes (y desde
// $0100: los valores pequenos suelen ser constantes, no direcciones)
static bool sym_near(uint16_t v, char* out, size_t sz){
  int i = sym_lower(v);
  if (i < sym_n && syms[i].val == v) { snprintf(out, sz, "%s", syms[i].name); return true; }
  if (i == 0) return false;
  int k = i - 1;
  while (k > 0 && syms[k - 1].val == syms[k].val) k--;   // el primero de los que valen eso
  if (syms[k].val < 0x100 || v - syms[k].val >= 0x100) return false;
  snprintf(out, sz, "%s+%X", syms[k].name, v - syms[k].val);
  return true;
}

static bool sym_find(const char* name, uint16_t* v){
  for (int i = 0; i < sym_n; i++) if (ci_eq(syms[i].name, name)) { *v = syms[i].val; return true; }
  return false;
}

// En un desensamblado, "1234h" (4 cifras) por su simbolo, si lo hay
static void sym_subst(char* t, size_t sz){
  if (!sym_n) return;
  char out[96];
  size_t o = 0;
  for (size_t i = 0; t[i] && o < sizeof(out) - 1; ) {
    if ((i == 0 || !isalnum((uint8_t)t[i - 1])) && isxdigit((uint8_t)t[i]) && isxdigit((uint8_t)t[i + 1]) &&
        isxdigit((uint8_t)t[i + 2]) && isxdigit((uint8_t)t[i + 3]) && (t[i + 4] == 'h' || t[i + 4] == 'H') &&
        !isalnum((uint8_t)t[i + 5])) {
      char h[5] = {t[i], t[i + 1], t[i + 2], t[i + 3], 0};
      const char* name = sym_at(strtoul(h, nullptr, 16));
      if (name) {
        int w = snprintf(out + o, sizeof(out) - o, "%s", name);
        o = (w < 0 || o + w >= sizeof(out)) ? sizeof(out) - 1 : o + w;
        i += 5;
        continue;
      }
    }
    out[o++] = t[i++];
  }
  out[o] = 0;
  snprintf(t, sz, "%s", out);
}

// Al parar: el .SYM del programa cargado, si se ha cargado uno desde la
// ultima vez
static void trace_start(uint32_t n);
static void trace_stop_req();
static void trace_print(uint32_t n);

static void sym_auto_load(){
  if (!sym_auto_pend) return;
  sym_auto_pend = false;
  int n = sym_load(sym_auto);
  if (n >= 0) Serial.printf("Symbols: %d from %s\r\n", n, sym_auto);
  else if (sym_n) { sym_n = 0; sym_src[0] = 0; Serial.printf("Symbols cleared (no %s)\r\n", sym_auto); }
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
  char text[96], b[140];
  int pos = 0;
  for (int k = 0; k < count && pos < len; k++) {
    int used = Z80Disassembler::disassemble(text, bytes + pos, len - pos);
    if (used <= 0) used = 1;
    sym_subst(text, sizeof(text));
    const char* lab = sym_at(addr + pos);
    if (lab) Serial.printf("%s:\r\n", lab);
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

static bool kbd_arm = false;        // la proxima parada es la del boton QS (teclado: ui_pause_qs)

static bool trace_on = false;                 // traza lenta en curso (mas abajo)
static bool trace_slow_valid = false;         // lo ultimo es una traza lenta (t) y no se ha seguido desde entonces
static void web_refresh();                    // la vista de la web (mas abajo)

static void resumed(){
  stopped = false;
  if (!trace_on) web_refresh();               // la web: "en marcha"
  if (!trace_on) trace_slow_valid = false;    // th y H: ahora, el historial de la FPGA
  set_status_led_ok();
}

// Volver al programa. Si es SLOW, el monitor apago la NMI al entrar. Al
// seguir de verdad (run: c, g, o sobre un CALL, u) se vuelve por OUT ($FE),A
// / RET escritos debajo de su pila (8 bytes), como en la carga de snapshots
// con NMI 01. Los pasos se dan con la NMI apagada: son exactos (sin NMI no
// hay pantalla ni rutina de video que se cuele) y, tras 20 ms parado, la
// FPGA ya ve FAST y para en cualquier instruccion
static void q_cont(bool run){
  if (run && prog_slow) {
    uint16_t s = reg16(R_SP), pc = reg16(R_PC);
    DbgReq* r = q_push(OP_WRITE, s - 8, 8);
    if (r) {
      static const uint8_t stub[8] = {0xD3, 0xFE, 0xC9, 0, 0, 0, 0, 0};
      memcpy(r->data, stub, 8);
      r->data[6] = pc & 0xFF; r->data[7] = pc >> 8;
    }
    set_reg16(R_SP, s - 2);
    set_reg16(R_PC, s - 8);
    regs[R_R] = (regs[R_R] & 0x80) | ((regs[R_R] - 2) & 0x7F);   // las dos M1 del OUT y el RET
    q_setregs();
    prog_slow = false;
  }
  q_push(OP_CONT, 0, 0);
}

static void view_off();
static void ui_run();

static void dbg_continue(){
  view_off();                                 // v: el video del programa, antes de nada
  ui_run();                                   // y la pantalla del depurador (vuelve al parar)
  if (foreign_rst()) queue_emulate_rst();
  int at = bp_find(reg16(R_PC));
  queue_insert_bps(at);
  if (at >= 0) {                              // primero un paso sin ese breakpoint
    queue_steps(1);                           // (en SLOW, con la NMI apagada)
    internal_step = at;
  }
  q_cont(at < 0);
  resumed();
}

static void dbg_step(uint16_t n){
  if (foreign_rst()) queue_emulate_rst();
  queue_steps(n);
  q_cont(false);                              // con la NMI apagada, si es SLOW
  resumed();
}

// ---------------------------------------------------------------------
// Fase 3: ejecutar hasta (g), paso por encima (o), salir de la rutina (u)
// y puntos de vigilancia (w). g/o/u usan el comparador de ejecucion de la
// FPGA si esta libre (no hay punto de vigilancia) y no es SLOW: no toca la
// memoria (vale con codigo automodificable o que aun no se ha cargado). Si
// no, un FF temporal en la tabla de breakpoints (en SLOW el comparador solo
// mira la M1 exacta y la FPGA solo para en la NMI: el FF espera en JR $ y
// si). Al parar, por lo que sea, se quita
// ---------------------------------------------------------------------
static void queue_cmp(uint16_t addr, uint8_t mode){
  q_out(DBG_PORT, 0x80); q_out(DBG_PORT, addr & 0xFF);
  q_out(DBG_PORT, 0x81); q_out(DBG_PORT, addr >> 8);
  q_out(DBG_PORT, 0x82); q_out(DBG_PORT, mode);
}

static const char* watch_name(){
  return watch_mode == 2 ? "read" : watch_mode == 3 ? "write" : "I/O";
}

static void run_to(uint16_t addr, const char* what){
  if (!prog_slow && !watch_mode) {
    queue_cmp(addr, 1);                       // comparador de ejecucion
    cmp_tmp = true;
  } else if (bp_find(addr) < 0) {             // FF temporal
    int i = 0;
    while (i < NBP && bps[i].used) i++;
    if (i == NBP) { Serial.println("No room for a temporary breakpoint"); return; }
    bps[i].used = true; bps[i].addr = addr; bps[i].state = 0; bps[i].temp = true;
  }
  Serial.printf("%s %04X\r\n", what, (unsigned)addr);
  dbg_continue();
}

// o: CALL, RST, DJNZ, HALT y los repetidos (LDIR...) se pasan enteros; lo
// demas, un paso
static void step_over(uint8_t* b, uint16_t n){
  if (n < 4) { dbg_step(1); return; }
  char text[64];
  int len = Z80Disassembler::disassemble(text, b, 4);
  if (len <= 0) len = 1;
  uint8_t op = b[0];
  bool over = op == 0xCD || (op & 0xC7) == 0xC4 || (op & 0xC7) == 0xC7 ||   // CALL, CALL cc, RST
              op == 0x10 || op == 0x76 ||                                     // DJNZ, HALT
              (op == 0xED && (b[1] & 0xF4) == 0xB0);                          // LDIR, CPIR, INIR, OTIR...
  if (over) run_to(reg16(R_PC) + len, "Step over to");
  else dbg_step(1);
}

// Al parar: el comparador vuelve al punto de vigilancia (o apagado)
static void tmp_stop(){
  if (!cmp_tmp) return;
  cmp_tmp = false;
  queue_cmp(watch_addr, watch_mode);
}

// Los FF temporales fuera de la tabla (queue_remove_bps ya los quito de la
// memoria)
static void free_temp_bps(){
  for (int i = 0; i < NBP; i++) if (bps[i].temp) { bps[i].used = false; bps[i].temp = false; }
}

// Snapshots (mas abajo)
static char last_snap[96] = "";     // el ultimo snapshot grabado (L en la pausa del boton QS)
static void dirty_clear();
static uint8_t ld_prepare(const char* path, uint8_t* im);
static bool snap_pending = false;
static bool snap_busy();
static void snap_begin();
static void snap_result(uint8_t tag, uint8_t* data, uint16_t n);
static void snap_feed();
// Carga de snapshots (mas abajo)
static bool ld_busy();
static bool ld_pending();
static void ld_begin();
static void ld_result(uint8_t tag, uint8_t* data, uint16_t n);
static void ld_feed();
static uint8_t* ld_data(uint8_t k);

static void tmp_stop();
static void free_temp_bps();
static void view_result(uint8_t* d, uint16_t n);
static void view_part(uint8_t tag, uint8_t k, uint8_t* d, uint16_t n);
static bool view_is_on();
static bool view_busy();
static void view_drop();
static void view_request(bool hr, uint16_t dir);
static uint8_t* view_data(uint8_t which, uint16_t off);
static void view_times();
// Pantalla del depurador (fase 3b, mas abajo)
static bool ui_want = false;        // pedida: se ensena en cada parada
static void ui_result(uint8_t tag, uint8_t k, uint8_t* d, uint16_t n);
static void ui_show();
static void ui_hide();
static void ui_run();
static void ui_drop();
static void ui_on_break();
static void ui_on_stop();
static void ui_refresh();
static bool ui_busy();
static bool ui_entering_now();
static bool ui_is_shown();
static void ui_kbd_feed();
static void ui_pause_qs();
static void run_to(uint16_t addr, const char* what);
static void step_over(uint8_t* b, uint16_t n);

// Ha llegado DBG_BREAK con los registros
static void on_break2();
static void on_stopped(bool reached);
// Traza (mas abajo)
static uint32_t trace_n = 0;                  // apuntadas (en el anillo, como mucho TRACE_MAX)
static uint32_t trace_total = 0;              // en esta traza
static const TraceE& trace_at(uint32_t i);
static bool trace_break();
static void trace_result(uint8_t tag, uint8_t* d, uint16_t n);
// Historial: la traza de la FPGA (mas abajo)
#define FT_SHOW 60                            // como mucho, de una vez
static uint8_t ft_stage = 0;                  // 0 nada, 1 puntero, 2 anillo, 3 bytes
static uint8_t ft_purpose = 0;                // 1 consola (th), 2 pantalla (H)
static uint16_t ft_n = 0;                     // entradas pedidas / leidas
static uint16_t ft_pc[FT_SHOW];
static uint8_t ft_op[FT_SHOW][4];
static void ft_result(uint8_t tag, uint8_t k, uint8_t* d, uint16_t n);
static void ft_fetch(uint16_t n, uint8_t purpose);
// La vista estructurada de la web (mas abajo)
static void web_refresh();
static void web_result(uint8_t tag, uint8_t* d, uint16_t n);
static uint8_t web_pend = 0;
static bool web_wake = false;                 // la web empieza a mirar: componer la vista
static bool web_again = false;                // pedida otra vez mientras se leia
static bool ft_on = true;                     // tron / troff (la orden 12 va puesta al cargar el monitor)

// En SLOW la FPGA para en la entrada de la NMI ($0066, sim_int 0.12): antes
// de nada se deshace (PC = [SP], SP + 2, R - 1 por la M1 del reconocimiento),
// como si no hubiera llegado. La ROM pierde una linea de su cuenta: como
// mucho, un cuadro movido al seguir
static void on_break(){
  q_count = 0;
  poll_state = 0;
  stopped = true;
  ft_stage = 0;                               // (la cola se ha vaciado)
  web_pend = 0;
  web_again = false;
  ui_on_break();                              // la cola se ha vaciado
  stop_nmi = (regs[R_DBGST] & 0x40) != 0;
  if (stop_nmi) prog_slow = true;
  if (stop_nmi && reg16(R_PC) == 0x0066) {
    q_push(OP_READ, reg16(R_SP), 2, TAG_UNWIND);
    return;
  }
  on_break2();
}

static void on_break2(){
  if (internal_step >= 0) {
    int i = internal_step;
    internal_step = -1;
    if ((regs[R_DBGST] & 7) == 4) {           // el paso para saltar el breakpoint:
      q_push(OP_READ, bps[i].addr, 1, TAG_BP_ORIG, i);   // ponerlo y seguir
      q_write1(bps[i].addr, 0xFF);
      q_cont(true);
      resumed();
      return;
    }
  }
  stop_foreign = (regs[R_DBGST] & 7) == 7 && bp_find(reg16(R_PC)) < 0;
  int at_bp = bp_find(reg16(R_PC));
  bool reached = (cmp_tmp && (regs[R_DBGST] & 7) == 5) ||               // g/o/u: llegado al destino
                 ((regs[R_DBGST] & 7) == 7 && at_bp >= 0 && bps[at_bp].temp);
  tmp_stop();                                 // g/o/u: el comparador, como estaba
  if (ld_pending()) {                         // la trampa de LOAD *Z81: cargar
    stop_foreign = false;
    queue_remove_bps();
    free_temp_bps();
    ld_begin();
    return;
  }
  if (trace_on && trace_break()) return;      // la traza: otro paso
  on_stopped(reached);
}

// Parado de verdad: los simbolos, el LED, la linea de STOP, los registros y
// la pantalla del depurador
static void on_stopped(bool reached){
  sym_auto_load();
  set_status_LED(clMAGENTA);
  if (kbd_arm) { kbd_arm = false; ui_pause_qs(); }   // pausa del boton QS: el teclado, y V la pantalla
  char b[140], where[40] = "";
  if (sym_near(reg16(R_PC), where + 2, sizeof(where) - 3)) { where[0] = ' '; where[1] = '('; strcat(where, ")"); }
  snprintf(b, sizeof(b), "\r\n*** STOP at %04X%s: %s%s%s", reg16(R_PC), where, reached ? "reached" : reason_name(regs[R_DBGST]),
    (regs[R_DBGST] & 0x20) ? " (inside the simulated interrupt)" : "", prog_slow ? " (SLOW)" : "");
  Serial.println(b);
  queue_remove_bps();
  free_temp_bps();
  queue_show();
  if (snap_pending) { snap_pending = false; snap_begin(); }
  else ui_on_stop();                          // la pantalla del depurador, si se pidio
  web_refresh();                              // y la vista de la web, si hay alguien mirando
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
    case TAG_SNAP_MAP: case TAG_SNAP_CHROMA: case TAG_SNAP_POKES:
    case TAG_SNAP_MEM: case TAG_SNAP_COLOUR: case TAG_SNAP_PAGE:
    case TAG_SNAP_AYSEL: case TAG_SNAP_AYREGS: case TAG_SNAP_SPR: case TAG_SNAP_SPRSEL:
    case TAG_SNAP_SHCMP: case TAG_SNAP_SHADOW:
      snap_result(last.tag, data, n);
      break;
    case TAG_LD_MAP: case TAG_LD_SYNC:
      ld_result(last.tag, data, n);
      break;
    case TAG_SNAP_DIRTY:
      snap_result(last.tag, data, n);
      break;
    case TAG_STEPOVER: step_over(data, n); break;
    case TAG_STEPOUT:  if (n == 2) run_to(data[0] | (data[1] << 8), "Step out"); break;
    case TAG_VIEW:      view_result(data, n); break;
    case TAG_VIEW_SAVE: case TAG_VIEW_COPY: view_part(last.tag, last.arg, data, n); break;
    case TAG_VIEW_SYNC: poke_end(); view_times(); break;
    case TAG_TRACE: case TAG_TRACE_KEY: trace_result(last.tag, data, n); break;
    case TAG_FT_PTR: case TAG_FT_RING: case TAG_FT_OP: ft_result(last.tag, last.arg, data, n); break;
    case TAG_WEB_DIS: case TAG_WEB_STK: case TAG_WEB_MEM: web_result(last.tag, data, n); break;
    case TAG_UI_PK: case TAG_UI_CHROMA: case TAG_UI_SAVE: case TAG_UI_DIS:
    case TAG_UI_STK: case TAG_UI_MEM: case TAG_UI_KBD: case TAG_UI_FIX:
      ui_result(last.tag, last.arg, data, n);
      break;
    case TAG_UNWIND:                          // SLOW: deshacer la NMI
      if (n == 2) {
        set_reg16(R_PC, data[0] | (data[1] << 8));
        set_reg16(R_SP, reg16(R_SP) + 2);
        regs[R_R] = (regs[R_R] & 0x80) | ((regs[R_R] - 1) & 0x7F);
        q_setregs();                          // tambien en el monitor: si no, al dar
      }                                       // un paso volveria a $0066
      on_break2();
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
  snap_feed();                                // un snapshot en curso pide lo siguiente
  ui_kbd_feed();                              // el teclado: pausa del boton QS y pantalla del depurador
  if (web_wake && poll_state == 1) { web_wake = false; web_refresh(); }   // la web acaba de abrirse
  ld_feed();                                  // y una carga
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
  if (r.op == OP_READP || r.op == OP_WRITEP) SendByteToZ80(r.page);
  if (r.op == OP_WRITE || r.op == OP_SETREGS || r.op == OP_AYWRITE)
    for (uint16_t i = 0; i < r.n; i++) SendByteToZ80(r.data[i]);
  if (r.op == OP_WRITEP || r.op == OP_BRAMW) {  // los datos de la carga, en su buffer
    uint8_t* d = (r.arg == 0xFF) ? r.data :                   // (0xFF: en la propia peticion;
                 (r.arg == 0xFE) ? view_data(r.page, r.addr) :    //  0xFE: los buffers de v dir
                 ld_data(r.arg);                                  //  y de la pantalla)
    for (uint16_t i = 0; i < r.n; i++) SendByteToZ80(d[i]);
  }
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

// Una direccion: un simbolo (con +desplazamiento en hex), o hex. El
// simbolo gana: para un hex que tambien sea simbolo (BEEF), $BEEF
static bool parse_addr(const char*& p, uint32_t& v){
  while (*p == ' ') p++;
  char tok[SYM_NAME + 8];
  int n = 0;
  const char* q = p;
  while (*q && *q != ' ' && *q != '+' && n < (int)sizeof(tok) - 1) tok[n++] = *q++;
  tok[n] = 0;
  uint16_t sv;
  if (n && *p != '$' && sym_find(tok, &sv)) {
    v = sv;
    p = q;
    uint32_t off;
    if (*p == '+') { p++; if (parse_hex(p, off)) v = (v + off) & 0xFFFF; }
    return true;
  }
  return parse_hex(p, v);
}

static void help(){
  Serial.println("Debugger (hex numbers; an addr can also be a symbol, or symbol+n):");
  Serial.println("  p              pause the program");
  Serial.println("  r              registers and instruction at PC");
  Serial.println("  s [n]          step n instructions (1)");
  Serial.println("  c              continue");
  Serial.println("  o              step over (CALL, RST, DJNZ, LDIR... run to the next instruction)");
  Serial.println("  u              step out (run to the return address at [SP])");
  Serial.println("  g addr         run to addr");
  Serial.println("  b addr         set a breakpoint      bc [addr]  clear one / all      bl  list");
  Serial.println("  w r|w|io addr  watchpoint (read, write, I/O port)    w  clear it");
  Serial.println("  t [n]          trace: n instructions, one by one (none: until a breakpoint or SPACE / any key here)");
  Serial.println("  th [n]         the last n instructions (14): those of t, with the registers, or, if the");
  Serial.println("                 program has run since, the FPGA's history (PC only; tron / troff)");
  Serial.println("  v              video while stopped: Superfast text on / off (SLOW and FAST programs)");
  Serial.println("  v addr         video while stopped: Superfast HiRes, bitmap (32 x 192) at addr (WRX)");
  Serial.println("  ui             debugger screen on the ZX81 on / off (running: at the next stop). QS pause: V");
  Serial.println("  d [addr] [n]   disassemble n instructions (10) from addr (PC)");
  Serial.println("  m addr [n]     dump n bytes (64)");
  Serial.println("  e addr b1 b2.. write bytes");
  Serial.println("  x reg=val      set a register (af bc de hl ix iy af' bc' de' hl' sp pc i r)");
  Serial.println("  io port [val]  read (or write) an I/O port");
  Serial.println("  snap [-a] [f]  snapshot (.Z81): mapped pages, -a all pages; name: last LOADed file");
  Serial.println("  sym [f | -]    symbols (pasmo .sym): load f, - clears, none: show; LOAD reads <name>.SYM");
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
      if (!parse_addr(v, val)) return false;
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
  char cmd[6] = {0};
  for (int i = 0; i < 5 && *p && *p != ' '; i++) cmd[i] = *p++;
  if (*p && *p != ' ') { Serial.println("? (h for help)"); return true; }
  uint32_t a, n;

  if (!strcmp(cmd, "h") || !strcmp(cmd, "?")) { help(); return true; }
  if (trace_on) { trace_stop_req(); return true; }   // mientras traza, cualquier orden la para
  if (snap_busy()) { Serial.println("Snapshot in progress"); return true; }
  if (ld_busy()) { Serial.println("Loading a snapshot"); return true; }
  if (view_busy()) { Serial.println("Video: copying the bitmap"); return true; }
  if (ui_entering_now()) { Serial.println("Debugger screen: starting"); return true; }
  if (!strcmp(cmd, "p")) { if (stopped) Serial.println("Already stopped"); else dbg_pause(); return true; }
  if (!strcmp(cmd, "snap")) {
    while (*p == ' ') p++;
    bool all = false;
    if (p[0] == '-' && p[1] == 'a') { all = true; p += 2; while (*p == ' ') p++; }
    dbg_snapshot(all, p, false);              // desde la consola se queda parado
    return true;
  }
  if (!strcmp(cmd, "sym")) {
    while (*p == ' ') p++;
    if (!*p) {
      if (sym_n) Serial.printf("%d symbols from %s\r\n", sym_n, sym_src);
      else Serial.println("No symbols");
    } else if (!strcmp(p, "-")) { sym_n = 0; sym_src[0] = 0; Serial.println("Symbols cleared"); }
    else {
      char path[96];
      snprintf(path, sizeof(path), "%s%s", p[0] == '/' ? "" : current_dir, p);
      int n = sym_load(path);
      if (n < 0) Serial.printf("Can't open %s\r\n", path);
      else Serial.printf("%d symbols from %s\r\n", n, path);
    }
    if (stopped) ui_refresh();
    return true;
  }
  if (!stopped && !strcmp(cmd, "ui")) {       // en marcha: si sale o no en la parada siguiente
    ui_want = !ui_want;
    Serial.println(ui_want ? "Debugger screen: on at the next stop" : "Debugger screen: off");
    return true;
  }
  if (!stopped) { Serial.println("Running: p to pause"); return true; }

  if (!strcmp(cmd, "r")) queue_show();
  else if (!strcmp(cmd, "c")) dbg_continue();
  else if (!strcmp(cmd, "s")) dbg_step(parse_hex(p, n) && n ? n : 1);
  else if (!strcmp(cmd, "b")) {
    if (!parse_addr(p, a)) { Serial.println("b addr"); return true; }
    if (bp_find(a) >= 0) { Serial.println("Already set"); return true; }
    int i = 0;
    while (i < NBP && bps[i].used) i++;
    if (i == NBP) { Serial.println("No room for more breakpoints"); return true; }
    bps[i].used = true; bps[i].addr = a; bps[i].state = 0;
    Serial.printf("Breakpoint %d at %04X\r\n", i, (unsigned)a);
  }
  else if (!strcmp(cmd, "bc")) {
    if (parse_addr(p, a)) {
      int i = bp_find(a);
      if (i < 0) Serial.println("No breakpoint there"); else bps[i].used = false;
    } else for (int i = 0; i < NBP; i++) bps[i].used = false;
  }
  else if (!strcmp(cmd, "bl")) {
    bool any = false;
    for (int i = 0; i < NBP; i++) if (bps[i].used) { Serial.printf("%d: %04X\r\n", i, bps[i].addr); any = true; }
    if (!any) Serial.println("No breakpoints");
    if (watch_mode) Serial.printf("Watchpoint: %s %04X\r\n", watch_name(), watch_addr);
  }
  else if (!strcmp(cmd, "t")) {
    if (!parse_hex(p, n)) n = 0;
    trace_start(n);
    return true;
  }
  else if (!strcmp(cmd, "th")) {
    if (!parse_hex(p, n) || !n) n = 0x14;
    if (trace_slow_valid) trace_print(n);     // la de t, con los registros
    else if (ft_stage) Serial.println("History: busy");
    else ft_fetch(n > FT_SHOW ? FT_SHOW : n, 1);
    return true;
  }
  else if (!strcmp(cmd, "tron") || !strcmp(cmd, "troff")) {
    ft_on = cmd[3] == 'n';
    send_bit_config(cfgcmd_DBGTRACE, ft_on ? 1 : 0);
    Serial.println(ft_on ? "History on (the FPGA notes every instruction)" : "History off");
    return true;
  }
  else if (!strcmp(cmd, "ui")) {
    if (ui_is_shown()) { ui_want = false; ui_run(); Serial.println("Debugger screen off"); }
    else { ui_want = true; ui_show(); Serial.println("Debugger screen on (V on the ZX81: program screen; ui: off)"); }
    return true;
  }
  else if (!strcmp(cmd, "v")) {
    if (ui_is_shown()) { Serial.println("Close the debugger screen first (ui)"); return true; }
    if (view_is_on()) { view_off(); Serial.println("Video: back to the program's own"); }
    else if (parse_addr(p, a)) {
      if (a + 6144 > 0x10000) { Serial.println("v addr: the bitmap (6144 bytes) does not fit"); return true; }
      view_request(true, a);
    } else view_request(false, 0);
  }
  else if (!strcmp(cmd, "g")) {
    if (!parse_addr(p, a)) { Serial.println("g addr"); return true; }
    run_to(a, "Run to");
  }
  else if (!strcmp(cmd, "o")) q_push(OP_READ, reg16(R_PC), 4, TAG_STEPOVER);
  else if (!strcmp(cmd, "u")) q_push(OP_READ, reg16(R_SP), 2, TAG_STEPOUT);
  else if (!strcmp(cmd, "w")) {
    while (*p == ' ') p++;
    uint8_t m = 0;
    if (p[0] == 'r' && p[1] == ' ') { m = 2; p += 1; }
    else if (p[0] == 'w' && p[1] == ' ') { m = 3; p += 1; }
    else if (p[0] == 'i' && p[1] == 'o' && p[2] == ' ') { m = 4; p += 2; }
    if (!m) {
      if (*p) { Serial.println("w r|w|io addr   or   w (clear)"); return true; }
      watch_mode = 0;
      queue_cmp(0, 0);
      Serial.println("Watchpoint cleared");
      return true;
    }
    if (!parse_addr(p, a)) { Serial.println("w r|w|io addr"); return true; }
    watch_mode = m; watch_addr = a;
    queue_cmp(a, m);
    Serial.printf("Watchpoint: %s %04X\r\n", watch_name(), watch_addr);
  }
  else if (!strcmp(cmd, "d")) {
    if (!parse_addr(p, a)) a = reg16(R_PC);
    if (!parse_hex(p, n) || !n) n = 10;
    if (n > 60) n = 60;
    q_push(OP_READ, a, n * 4, TAG_DISASM, n);
  }
  else if (!strcmp(cmd, "m")) {
    if (!parse_addr(p, a)) { Serial.println("m addr [n]"); return true; }
    if (!parse_hex(p, n) || !n) n = 64;
    if (n > 256) n = 256;
    q_push(OP_READ, a, n, TAG_DUMP);
  }
  else if (!strcmp(cmd, "e")) {
    uint8_t bytes[32];
    uint16_t cnt = 0;
    uint32_t v;
    if (parse_addr(p, a))
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
  else { Serial.println("? (h for help)"); return true; }
  if (stopped) ui_refresh();                  // lo que haya cambiado, tambien en la pantalla
  web_refresh();
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
bool dbg_waiting(void){ return stopped && poll_state == 1 && q_count == 0 && !snap_busy() && !ld_busy() && !view_busy() && !ui_busy(); }

static void ld_cancel();

void dbg_reset(void){
  ld_cancel();                                // una carga a medias no sigue
  kbd_arm = false;
  dirty_clear();                              // las paginas escritas, desde aqui
  watch_mode = 0;                             // el reset de la FPGA borra el comparador
  prog_slow = stop_nmi = false;
  view_drop();
  ui_drop();
  cmp_tmp = false;
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
static uint8_t qs_stage = 0;       // 0 menos de 1 s, 1 de 1 a 3 s, 2 mas de 3 s

static void led_state(){
  if (snap_busy() || ld_busy()) return;       // el snapshot y la carga llevan su propio aviso
  if (stopped) set_status_LED(clMAGENTA); else set_status_led_ok();
}

static void toggle_qs(){
  nQS_en = !nQS_en;
  send_bit_config(cfgcmd_QSEN, nQS_en ? 1 : 0);
  led_state();
}

// Todo se decide al soltar; mientras se mantiene, el LED avisa de lo que va a
// pasar: magenta al llegar a 1 s (pausa o continuar), amarillo a los 3 s
// (snapshot)
void dbg_qs_button(void){
  bool down = !digitalRead(QSPIN);
  uint32_t now = millis();
  if (down && !qs_down) { qs_down = true; qs_t0 = now; qs_stage = 0; return; }
  if (down) {
    if (!debug_monitor_loaded || snap_busy() || ld_busy() || view_busy() || ui_entering_now()) return;
    if (qs_stage == 0 && now - qs_t0 >= 1000) { qs_stage = 1; set_status_LED(clMAGENTA); }
    if (qs_stage == 1 && now - qs_t0 >= 3000) { qs_stage = 2; set_status_LED(clYELLOW); }
    return;
  }
  if (!qs_down) return;
  qs_down = false;                            // al soltar
  if (now - qs_t0 < 30) return;               // rebote
  if (!debug_monitor_loaded || qs_stage == 0) { toggle_qs(); return; }
  if (snap_busy() || ld_busy() || view_busy() || ui_entering_now()) return;
  if (qs_stage == 1) {
    if (trace_on) { Serial.println("QS button: stop the trace"); trace_stop_req(); }
    else if (stopped) { Serial.println("QS button: continue"); dbg_continue(); }
    else { kbd_arm = true; dbg_pause(); led_state(); }   // magenta cuando llegue; despues, el teclado
  } else {
    Serial.println("QS button: snapshot");
    dbg_snapshot(false, "", !stopped);        // en marcha: para, graba y sigue
  }
}

// ---------------------------------------------------------------------
// Snapshots (fase 2b)
// ---------------------------------------------------------------------
#define POKE_FIRST 2038                       // POKEs de control que se guardan
#define POKE_LAST  2098                       // (2038-2040 interrupciones, 2041-2062,
#define POKE_N     (POKE_LAST - POKE_FIRST + 1)   // 2090-2098)
#define SNAP_INFLIGHT 12                      // peticiones en la cola a la vez

enum { SN_OFF, SN_INFO, SN_MEM, SN_COLOUR, SN_PAGES, SN_SHADOW };

static struct {
  uint8_t stage;
  bool all, resume;
  char user_name[40];
  char path[96];
  uint8_t mapper[8], chroma, pokes[POKE_N], rom[POKE_N];
  bool rom_ok;
  uint32_t req, got, end;                     // [MEMORY] / [COLOUR]
  uint8_t pages[64], npages, pidx;
  uint16_t preq, pgot;
  uint8_t page[8192];
  uint8_t rle_val; uint32_t rle_cnt; uint8_t toks;
  uint8_t ay_sel[2], ay[2][16];               // los AY de la FPGA (A, B)
  uint8_t spr[1024], spr_sel;                 // la copia de los sprites
  uint8_t cmp[256]; uint16_t cmp_n; uint8_t cmp_blk;   // trozo de [MEMORY] para comparar con la sombra
  uint8_t shdiff;                             // bloques cuya sombra no es [MEMORY]
  uint8_t shblk;                              // SN_SHADOW: bloque en curso
  uint8_t dirty[8];                           // paginas escritas por la CPU ($3FEF, indice 6)
  char wbuf[512]; uint16_t wlen;
  uint32_t bytes;
  bool err;
} sn;

static char last_name[24] = "";
static char opendir_arg[48] = "";
static bool opendir_ok = false;

void dbg_note_opendir(const char* arg, bool ok){
  strncpy(opendir_arg, arg, sizeof(opendir_arg) - 1);
  opendir_arg[sizeof(opendir_arg) - 1] = 0;
  opendir_ok = ok;
}

void dbg_note_loaded(const char* name){
  dirty_clear();                              // programa nuevo: sus paginas, desde cero
  const char* dot = strrchr(name, '.');       // sus simbolos: el mismo nombre con .SYM
  const char* sl = strrchr(name, '/');
  int base = (dot && (!sl || dot > sl)) ? dot - name : strlen(name);
  snprintf(sym_auto, sizeof(sym_auto), "%s%.*s.SYM", name[0] == '/' ? "" : current_dir, base, name);
  sym_auto_pend = true;
  const char* b = strrchr(name, '/');
  b = b ? b + 1 : name;
  int i = 0;
  while (b[i] && b[i] != '.' && i < (int)sizeof(last_name) - 1) { last_name[i] = b[i]; i++; }
  last_name[i] = 0;
}

static bool snap_busy(){ return sn.stage != SN_OFF; }

// --- escritura con buffer ---
static void sflush(){
  if (sn.wlen) {
    if (!snapfile_write(sn.wbuf, sn.wlen)) sn.err = true;
    sn.bytes += sn.wlen;
    sn.wlen = 0;
  }
}
static void sw(const char* t){
  while (*t) {
    sn.wbuf[sn.wlen++] = *t++;
    if (sn.wlen == sizeof(sn.wbuf)) sflush();
  }
}
static void swf(const char* fmt, ...){
  char b[160];
  va_list a; va_start(a, fmt); vsnprintf(b, sizeof(b), fmt, a); va_end(a);
  sw(b);
}

// --- RLE como el de EightyOne: "VV " o "*NNNN VV " ---
static void rle_tok(){
  if (!sn.rle_cnt) return;
  if (sn.rle_cnt > 1) swf("*%04X %02X ", (unsigned)sn.rle_cnt, sn.rle_val);
  else swf("%02X ", sn.rle_val);
  sn.rle_cnt = 0;
  if (++sn.toks == 16) { sw("\n"); sn.toks = 0; }
}
static void rle_byte(uint8_t v){
  if (sn.rle_cnt && v == sn.rle_val && sn.rle_cnt < 0xFFFF) { sn.rle_cnt++; return; }
  rle_tok();
  sn.rle_val = v; sn.rle_cnt = 1;
}
static void rle_end(){ rle_tok(); sw("\n"); sn.toks = 0; }

// El valor del POKE a (o -1 si no se ha escrito nunca: en la BRAM sigue el
// byte de la ROM, que es lo que se cargo ahi al arrancar)
static int pk(int a){
  int i = a - POKE_FIRST;
  if (sn.rom_ok && sn.pokes[i] == sn.rom[i]) return -1;
  return sn.pokes[i];
}
static int pkd(int a, int def){ int v = pk(a); return v < 0 ? def : v; }

void dbg_snapshot(bool all, const char* name, bool resume){
  if (!debug_monitor_loaded) { Serial.println("No debug monitor loaded"); return; }
  if (snap_busy()) { Serial.println("Snapshot in progress"); return; }
  sn.all = all;
  sn.resume = resume;
  strncpy(sn.user_name, name ? name : "", sizeof(sn.user_name) - 1);
  sn.user_name[sizeof(sn.user_name) - 1] = 0;
  view_off();                                 // el .Z81 guarda el video del programa
  ui_hide();
  if (stopped) snap_begin();
  else {                                      // al parar empieza (on_break)
    snap_pending = true;
    set_blinking(clYELLOW, 4);                // ya se ve que va a grabar
    dbg_pause();
  }
}

static void snap_make_path(){
  if (sn.user_name[0]) {
    bool ext = strchr(sn.user_name, '.') != nullptr;
    snprintf(sn.path, sizeof(sn.path), "%s%s%s", current_dir, sn.user_name, ext ? "" : ".Z81");
    return;
  }
  const char* base = last_name[0] ? last_name : "NONAME";
  for (int k = 1; k < 1000; k++) {
    // sin separador: el ZX81 no tiene "_" (EXPLORER001.Z81, NONAME001.Z81)
    snprintf(sn.path, sizeof(sn.path), "%s%s%03d.Z81", current_dir, base, k);
    if (!snapfile_exists(sn.path)) return;
  }
}

static void snap_begin(){
  snap_make_path();
  if (!snapfile_open(sn.path)) {
    Serial.printf("Snapshot: can't create %s\r\n", sn.path);
    if (sn.resume) dbg_continue();
    return;
  }
  sn.stage = SN_INFO;
  sn.err = false; sn.bytes = 0; sn.wlen = 0; sn.rle_cnt = 0; sn.toks = 0;
  set_blinking(clYELLOW, 4);
  Serial.printf("Snapshot: writing %s ...\r\n", sn.path);
  sn.rom_ok = rom_file_read(POKE_FIRST, sn.rom, POKE_N) == POKE_N;

  // [CPU] y [ZX81], como los escribe EightyOne
  sw("[MACHINE]\nMODEL ZX81\n\n[CPU]\n");
  swf("PC %04X    SP  %04X\n", reg16(R_PC), reg16(R_SP));
  swf("HL %04X    HL_ %04X\n", reg16(R_HL), reg16(R_HL_));
  swf("DE %04X    DE_ %04X\n", reg16(R_DE), reg16(R_DE_));
  swf("BC %04X    BC_ %04X\n", reg16(R_BC), reg16(R_BC_));
  swf("AF %04X    AF_ %04X\n", reg16(R_AF), reg16(R_AF_));
  swf("IX %04X    IY  %04X\n", reg16(R_IX), reg16(R_IY));
  swf("IR %04X\n", (regs[R_I] << 8) | regs[R_R]);
  swf("IM 01      IF1 %02X\n", regs[R_IFF]);   // IM no se puede leer: la ROM pone IM 1
  swf("HT 00      IF2 %02X\n", regs[R_IFF]);
  swf("\n[ZX81]\nNMI %02X     SYNC 00\nLINE 000\n", prog_slow ? 1 : 0);   // SLOW: NMI encendida

  // el mapper, el registro de Chroma y los POKEs de control (BRAM de sombra)
  for (int b = 0; b < 8; b++) q_push(OP_IN, (b << 8) | 0xE7, 0, TAG_SNAP_MAP, b);
  q_out(DBG_PORT, 0x03);
  q_push(OP_IN, DBG_PORT, 0, TAG_SNAP_CHROMA);
  // los AY de la FPGA: el registro elegido (indices 4 y 5) y los 16
  q_out(DBG_PORT, 0x04); q_push(OP_IN, DBG_PORT, 0, TAG_SNAP_AYSEL, 0);
  q_out(DBG_PORT, 0x05); q_push(OP_IN, DBG_PORT, 0, TAG_SNAP_AYSEL, 1);
  q_push(OP_AYREAD, AY_PORT_A, 16, TAG_SNAP_AYREGS, 0);
  q_push(OP_AYREAD, AY_PORT_B, 16, TAG_SNAP_AYREGS, 1);
  // las paginas que ha escrito el programa (una FPGA anterior devuelve 0)
  // (indice 6: un bit por IN, de la pagina 0 a la 63)
  q_out(DBG_PORT, 6);
  q_push(OP_INSEQ, DBG_PORT, 64, TAG_SNAP_DIRTY);
  // la BRAM de sombra (indice 2): los POKEs, los sprites y el elegido
  q_out(DBG_PORT, 0x85); q_out(DBG_PORT, POKE_FIRST & 0xFF);
  q_out(DBG_PORT, 0x86); q_out(DBG_PORT, POKE_FIRST >> 8);
  q_out(DBG_PORT, 0x02);
  q_push(OP_INSEQ, DBG_PORT, POKE_N, TAG_SNAP_POKES);
  q_out(DBG_PORT, 0x85); q_out(DBG_PORT, SPR_MIRROR & 0xFF);
  q_out(DBG_PORT, 0x86); q_out(DBG_PORT, SPR_MIRROR >> 8);
  for (int k = 0; k < 4; k++) q_push(OP_INSEQ, DBG_PORT, 256, TAG_SNAP_SPR, k);
  q_out(DBG_PORT, 0x85); q_out(DBG_PORT, 2100 & 0xFF);
  q_out(DBG_PORT, 0x86); q_out(DBG_PORT, 2100 >> 8);
  q_push(OP_INSEQ, DBG_PORT, 1, TAG_SNAP_SPRSEL);
}

static void snap_feed(){
  if (sn.stage == SN_MEM || sn.stage == SN_COLOUR) {
    while (q_count < SNAP_INFLIGHT && sn.req < sn.end) {
      uint16_t n = (sn.end - sn.req > 256) ? 256 : (uint16_t)(sn.end - sn.req);
      if (sn.stage == SN_MEM) {
        q_push(OP_READ, (uint16_t)sn.req, n, TAG_SNAP_MEM);
        // la sombra del mismo trozo ($2000-$BFFF, seguida en el puntero),
        // para saber que bloques hay que guardar aparte (SHADOW)
        if (sn.req < 0xC000) q_push(OP_INSEQ, DBG_PORT, n, TAG_SNAP_SHCMP);
      } else
        q_push(OP_INSEQ, DBG_PORT, n, TAG_SNAP_COLOUR);   // [COLOUR] es la sombra de $C000-$FFFF
      sn.req += n;
    }
  } else if (sn.stage == SN_SHADOW) {
    while (q_count < SNAP_INFLIGHT && sn.preq < 8192) {
      q_push(OP_INSEQ, DBG_PORT, 256, TAG_SNAP_SHADOW);
      sn.preq += 256;
    }
  } else if (sn.stage == SN_PAGES) {
    while (q_count < SNAP_INFLIGHT && sn.preq < 8192) {
      DbgReq* r = q_push(OP_READP, sn.preq, 256, TAG_SNAP_PAGE);
      if (!r) break;
      r->page = sn.pages[sn.pidx];
      sn.preq += 256;
    }
  }
}

// La pagina 0 o 1 sigue siendo la ROM tal como la cargo el MCU (load_ROM
// escribe el fichero en 0 y otra vez en 8192: la pagina 1 es el final del
// fichero y, detras, el principio otra vez)
static bool page_is_rom(uint8_t p){
  int32_t size = rom_file_read(0, nullptr, 0);
  if (size <= 0 || size > 16384) return false;
  uint8_t* rom = (uint8_t*)malloc(size);       // ~13 KB, solo un momento
  if (!rom) return false;
  bool same = rom_file_read(0, rom, size) == size;
  for (uint32_t k = 0; k < 8192 && same; k++) {
    uint32_t off = (p == 0) ? k : ((8192 + k < (uint32_t)size) ? 8192 + k : k);
    if (off < (uint32_t)size && rom[off] != sn.page[k]) same = false;   // donde la ROM no llega, no se compara
  }
  free(rom);
  return same;
}

static void snap_write_sd81(){
  sw("\n[SD81BOOSTER]\n");
  swf("CUR_DIR %s\n", current_dir);
  sw("MAPPER");
  for (int b = 0; b < 8; b++) swf(" %02X", sn.mapper[b]);
  sw("\n");
  int m = pk(2045), dm = 0;
  if (m == 170 || m == 173 || m == 174) dm = 1;
  else if (m == 171) dm = 3;
  else if (m == 172) dm = 5;
  swf("DISPLAY_MODE %02X\n", dm);
  if (m == 173) sw("WIDE_COLS 46\n");        // texto ancho: 70 columnas
  else if (m == 174) sw("WIDE_COLS 50\n");   // 80 columnas
  swf("BORDER_INK %02X\n", pkd(2046, 0x0F));
  swf("BORDER_PATTERN %02X\n", pk(2047) == 170 ? 1 : 0);
  sw("BORDER_CHARS");
  for (int i = 0; i < 8; i++) swf(" %02X", pkd(2048 + i, 0));
  sw("\n");
  swf("HFILE %04X\n", (pkd(2044, 0) << 8) | pkd(2043, 0));
  swf("CHROMA_MODE %02X\n", sn.chroma);
  int d = pk(2057), dbuf = 0;
  if (d >= 0 && (d & 0xF8) == 0xA8) dbuf = 0xC0 | (d & 7);       // AUTO
  else if (d >= 0 && (d & 0xF8) == 0xC8) dbuf = 0x80 | (d & 7);  // MANUAL
  swf("DBUF %02X\n", dbuf);
  swf("SEL128 %02X\nSEL256 %02X\n", cfg_value(cfgcmd_128CHARS), cfg_value(cfgcmd_256CHARS));
  swf("WRX %02X\n", pk(2058) == 170 ? 1 : 0);
  swf("ROMLOCK %02X\n", cfg_value(cfgcmd_ROMLOCK));
  swf("SCROLL %02X %02X %02X %02X\n", pkd(2090, 0), pkd(2091, 0xFF), pkd(2092, 0xFF), pkd(2093, 0xFF));
  // El listado que abrio el ultimo OPENDIR (el argumento en hexadecimal,
  // "-" si estaba vacio): sin el, el explorador no puede pedir sus filas
  if (opendir_ok) {
    sw("DIR_OPEN ");
    if (!opendir_arg[0]) sw("-");
    for (int i = 0; opendir_arg[i]; i++) swf("%02X", (uint8_t)opendir_arg[i]);
    sw("\n");
  }
  // D_FILE y base de atributos alternativos (2096-2098, 2059-2061): activos
  // si lo ultimo en 2098 / 2061 fue 170; POKE 2045,85 los apaga
  if (m != 85) {
    if (pk(2098) == 170) swf("DISP_ADDR %04X 01\n", (pkd(2097, 0) << 8) | pkd(2096, 0));
    if (pk(2061) == 170) swf("ATTR_ADDR %04X 01\n", (pkd(2060, 0) << 8) | pkd(2059, 0));
  }
  // Propias del hardware (EightyOne las ignora): los POKEs de control tal
  // cual ("--" = nunca escrito) y la configuracion del MCU, para LOAD *Z81
  sw("HW_POKES");
  for (int i = 0; i < POKE_N; i++) {
    int v = pk(POKE_FIRST + i);
    if (v < 0) sw(" --"); else swf(" %02X", v);
  }
  sw("\n");
  swf("HW_CFG %02X %02X %02X %02X %02X %02X %02X\n",
    cfg_value(cfgcmd_MC45), cfg_value(cfgcmd_MODE48K), nQS_en, cfg_value(cfgcmd_FULLPAG),
    cfg_value(cfgcmd_128CHARS), cfg_value(cfgcmd_256CHARS), cfg_value(cfgcmd_ROMLOCK));
  // Los AY de la FPGA: AY1 el A (ZonX, como en EightyOne), AY3 el B. El
  // AY2 es el del MCU (lo escribe mcustate_save, con el VGM, el PEG y los
  // ficheros abiertos)
  static const char* const ayk[2] = {"AY1", "AY3"};
  for (int c = 0; c < 2; c++) {
    swf("%s_REGS", ayk[c]);
    for (int r = 0; r < 16; r++) swf(" %02X", sn.ay[c][r]);
    swf("\n%s_REG_SEL %02X\n", ayk[c], sn.ay_sel[c]);
  }
  mcustate_save(sw);
  // Sprites (de la copia en la sombra: 32 bytes por sprite, los 28 campos
  // de los POKEs 2101-2128); solo los que no estan a cero. La ROM los pone
  // a cero en cada reset; si un sprite sigue teniendo los bytes de la ROM
  // (una ROM sin esa limpieza), no se ha escrito nunca: no se guarda
  uint8_t* rom = sn.page;                     // (libre hasta las paginas)
  bool rom_ok = rom_file_read(SPR_MIRROR, rom, 0x400) == 0x400;
  uint8_t sel_rom;
  bool sel_never = rom_file_read(2100, &sel_rom, 1) == 1 && sel_rom == sn.spr_sel;
  swf("SPRITE_SEL %02X\n", sel_never ? 0 : sn.spr_sel);
  for (int i = 0; i < 32; i++) {
    const uint8_t* f = sn.spr + i * 32;
    bool zero = true, never = rom_ok;
    for (int j = 0; j < 28; j++) {
      if (f[j]) zero = false;
      if (rom_ok && f[j] != rom[i * 32 + j]) never = false;
    }
    if (zero || never) continue;
    swf("SPRITE %02X %02X %04X %02X", i, f[0] & 1, f[1] | ((f[2] & 1) << 8), f[3]);
    for (int j = 4; j < 28; j++) swf(" %02X", f[j]);
    sw("\n");
  }
}

static void snap_next_page();

static void snap_finish(){
  sw("\n[EOF]\n");
  sflush();
  snapfile_close();
  set_blinking_off();
  sn.stage = SN_OFF;
  if (sn.err) Serial.printf("Snapshot: SD write error in %s\r\n", sn.path);
  else {
    Serial.printf("Snapshot saved: %s (%lu bytes)\r\n", sn.path, (unsigned long)sn.bytes);
    strcpy(last_snap, sn.path);               // para la L en la pausa del boton QS
  }
  if (sn.resume) dbg_continue();
  else { led_state(); Serial.println("Stopped. Type h for help."); ui_on_stop(); }
}

// La sombra de los bloques que no son [MEMORY] (SHADOW, como EightyOne): el
// 0 si no es la ROM (sin contar los POKEs y los sprites, que van en sus
// claves) y los que se vio al leer [MEMORY] ($2000-$BFFF; $C000-$FFFF ya
// es [COLOUR])
static void snap_shadow_next(){
  while (sn.shblk < 6 && sn.shblk && !(sn.shdiff & (1 << sn.shblk))) sn.shblk++;
  if (sn.shblk >= 6) { snap_finish(); return; }
  sn.stage = SN_SHADOW;
  sn.preq = sn.pgot = 0;
  uint16_t a = sn.shblk * 0x2000;
  q_out(DBG_PORT, 0x85); q_out(DBG_PORT, a & 0xFF);
  q_out(DBG_PORT, 0x86); q_out(DBG_PORT, a >> 8);
}

static bool shadow0_is_rom(){
  uint8_t r[256];
  for (uint16_t a = 0; a < 0x2000; a += 256) {
    if (rom_file_read(a, r, 256) != 256) return false;
    for (int i = 0; i < 256; i++) {
      uint16_t x = a + i;
      if ((x >= POKE_FIRST && x <= 2128) || (x >= SPR_MIRROR && x < SPR_MIRROR + 0x400)) continue;
      if (r[i] != sn.page[x]) return false;
    }
  }
  return true;
}

static void snap_next_page(){
  sn.preq = sn.pgot = 0;
  if (sn.pidx >= sn.npages) { sn.shblk = 0; snap_shadow_next(); }
}

static void snap_result(uint8_t tag, uint8_t* data, uint16_t n){
  switch (tag) {
    case TAG_SNAP_MAP:    sn.mapper[last.arg] = n ? data[0] & 0x3F : 0; break;
    case TAG_SNAP_AYSEL:  sn.ay_sel[last.arg & 1] = n ? data[0] : 0; break;
    case TAG_SNAP_AYREGS:
      memcpy(sn.ay[last.arg & 1], data, n < 16 ? n : 16);
      q_out(last.arg ? AY_PORT_B : AY_PORT_A, sn.ay_sel[last.arg & 1]);   // el elegido, como estaba
      break;
    case TAG_SNAP_SPR:    memcpy(sn.spr + (last.arg & 3) * 256, data, n < 256 ? n : 256); break;
    case TAG_SNAP_SPRSEL: sn.spr_sel = n ? data[0] : 0; break;
    case TAG_SNAP_DIRTY:
      memset(sn.dirty, 0, sizeof(sn.dirty));
      for (uint16_t p = 0; p < n && p < 64; p++) if (data[p] & 1) sn.dirty[p >> 3] |= 1 << (p & 7);
      break;
    case TAG_SNAP_SHCMP:
      if (n != sn.cmp_n || memcmp(data, sn.cmp, n)) sn.shdiff |= 1 << sn.cmp_blk;
      break;
    case TAG_SNAP_SHADOW:
      if (sn.pgot + n <= 8192) memcpy(sn.page + sn.pgot, data, n);
      sn.pgot += n;
      if (sn.pgot < 8192) break;
      if (!sn.shblk) rom_file_read(0x1000, sn.page + 0x1000, 0x800);   // ahi va la traza de la FPGA: como la ROM
      if (sn.shblk || !shadow0_is_rom()) {
        swf("SHADOW %02X\n", sn.shblk);
        sn.toks = 0;
        for (int i = 0; i < 8192; i++) rle_byte(sn.page[i]);
        rle_tok();
        sw("\nSHADOW_END\n");
        sn.toks = 0;
      }
      sn.shblk++;
      snap_shadow_next();
      break;
    case TAG_SNAP_CHROMA: sn.chroma = n ? data[0] : 0; break;
    case TAG_SNAP_POKES:
      memcpy(sn.pokes, data, n < POKE_N ? n : POKE_N);
      sw("\n[MEMORY]\nRAM_PACK 48K\n8K_RAM_ENABLED 01\nROM_PROTECTED 00\nMEMRANGE 2000 FFFF\n");
      sn.stage = SN_MEM; sn.req = sn.got = 0x2000; sn.end = 0x10000;
      sn.shdiff = 0;
      q_out(DBG_PORT, 0x85); q_out(DBG_PORT, 0x00);   // la sombra desde $2000, para comparar
      q_out(DBG_PORT, 0x86); q_out(DBG_PORT, 0x20);
      break;
    case TAG_SNAP_MEM:
    case TAG_SNAP_COLOUR:
      if (tag == TAG_SNAP_MEM) {                // para compararlo con su sombra
        memcpy(sn.cmp, data, n);
        sn.cmp_n = n;
        sn.cmp_blk = sn.got >> 13;
      }
      for (uint16_t i = 0; i < n; i++) rle_byte(data[i]);
      sn.got += n;
      if (sn.got < sn.end) break;
      rle_end();
      if (tag == TAG_SNAP_MEM) {
        sw("\n[COLOUR]\nTYPE Chroma\n");
        sn.stage = SN_COLOUR; sn.req = sn.got = 0xC000; sn.end = 0x10000;
        q_out(DBG_PORT, 0x85); q_out(DBG_PORT, 0x00);   // el color es la sombra de $C000
        q_out(DBG_PORT, 0x86); q_out(DBG_PORT, 0xC0);
      } else {
        swf("CHROMA_MODE %02X\nCOLOUR_ENABLED 01\n", sn.chroma);
        snap_write_sd81();
        sn.npages = 0;                         // paginas: las mapeadas y las escritas, o todas
        int last_p = cfg_value(cfgcmd_FULLPAG) ? 63 : 32;   // sin FULL_PAGING el mapper solo llega a la 31
        for (int p = 0; p < 63; p++) {
          bool want = (sn.all && p < last_p) || (sn.dirty[p >> 3] & (1 << (p & 7)));
          for (int b = 0; b < 8 && !want; b++) want = (sn.mapper[b] == p);
          if (want) sn.pages[sn.npages++] = p;
        }
        sn.pidx = 0;
        sn.stage = SN_PAGES;
        snap_next_page();
      }
      break;
    case TAG_SNAP_PAGE:
      if (sn.pgot + n <= 8192) memcpy(sn.page + sn.pgot, data, n);
      sn.pgot += n;
      if (sn.pgot < 8192) break;
      {
        uint8_t pg = sn.pages[sn.pidx];
        bool skip;
        if (pg < 2) skip = page_is_rom(pg);  // la ROM sin tocar no se guarda
        else if (sn.all) {                    // con -a, las paginas a FF tampoco
          skip = true;
          for (int i = 0; i < 8192 && skip; i++) skip = (sn.page[i] == 0xFF);
        } else skip = false;
        if (!skip) {
          swf("RAM_PAGE %02X\n", pg);
          sn.toks = 0;
          for (int i = 0; i < 8192; i++) rle_byte(sn.page[i]);
          rle_tok();
          sw("\nRAM_PAGE_END\n");
          sn.toks = 0;
        }
        sn.pidx++;
        snap_next_page();
      }
      break;
  }
}

// ---------------------------------------------------------------------
// Carga de snapshots con el monitor (LOAD *Z81, comando 75)
//
// La ROM manda el nombre; dbg_z81_prepare lee el .Z81 entero una vez (lo
// valida y apunta donde empieza cada seccion) y, si esta bien, la ROM apaga
// la NMI y lanza la trampa. Al parar (on_break) el MCU lo carga todo por el
// monitor, que corre en la pagina 63 y puede escribir cualquier pagina:
//   1. estado del MCU: directorio, listado abierto, bits de configuracion;
//   2. los POKEs de control, con la orden 10 (el monitor escribe como el
//      programa): primero el modo de video, el 2040 y el 2056 al final;
//   3. las paginas (RAM_PAGE) con WRITEP;
//   4. [MEMORY]: en la pagina de cada bloque que no venga en RAM_PAGE, y en
//      la BRAM de sombra (BRAMW), que tiene que ser la vista del programa;
//      [COLOUR] en la sombra de $C000-$FFFF, la pagina del bloque 0 en la de
//      $0000-$1FFF y en 2038-2098 los POKEs (o el byte de la ROM si nunca se
//      escribieron, como al arrancar);
//   5. el mapper, Chroma81, los AY, ROMLOCK, los registros y CONT. Si el snapshot
//      lleva la NMI encendida, se vuelve por OUT ($FE),A / RET puestos
//      debajo de la pila (sin la NMI no se puede parar en SLOW).
// ---------------------------------------------------------------------
enum { LD_OFF, LD_POKES, LD_SPRITES, LD_POKES_LATE, LD_POKES_WAIT, LD_PAGES, LD_MEM, LD_COLOUR, LD_BLOCK0,
       LD_SHADOW, LD_FINISH };

#define LD_NBUF 4
static uint8_t ld_buf[LD_NBUF][256];

static struct {
  bool pending;                 // la ROM va a lanzar la trampa
  uint8_t stage;
  uint8_t cpu[REGS_LEN];        // registros, en el orden del monitor
  uint8_t im, nmi;              // nmi: 0, 1 o 0xFF (no viene)
  uint16_t mem_start;
  uint32_t mem_len, mem_pos;
  bool has_col; uint32_t col_pos;
  bool has_chroma; uint8_t chroma;
  bool has_mapper; uint8_t mapper[8];
  uint32_t page_pos[64];        // donde empieza cada RAM_PAGE (0 = no viene)
  uint8_t pk[POKE_N], pk_mode[POKE_N];   // 0 no tocar, 1 valor del snapshot, 2 valor de reset
  int16_t cfg[8];               // ordenes de configuracion (-1 = no tocar)
  uint8_t romlock_old;
  bool has_dir; char dir[MAX_FILENAME_LEN];
  bool dir_open; char dir_arg[48];
  bool set_im;                  // cargado desde la parada (L): el IM va con SETREGS
  bool has_ay[2]; uint8_t ay[2][16], ay_sel[2];   // los AY de la FPGA (A, B)
  uint8_t spr[32][28], spr_sel;                   // los sprites (los 28 campos de 2101-2128)
  uint32_t shadow_pos[8];                         // SHADOW de cada bloque (0 = no viene)
  uint8_t page;                 // LD_PAGES: pagina en curso; LD_SPRITES y LD_SHADOW: el siguiente
  uint32_t done;                // bytes ya pedidos de la fuente en curso
  uint32_t rle_cnt; uint8_t rle_val;
} ld;

static bool ld_busy(){ return ld.stage != LD_OFF; }

// --- lectura del fichero: tokens y el RLE de EightyOne ---
static bool ld_space(int c){ return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
static uint8_t ld_tok(char* b, uint8_t max){
  int c;
  do c = z81in_read(); while (c >= 0 && ld_space(c));
  uint8_t n = 0;
  while (c >= 0 && !ld_space(c)) {
    if (n < max - 1) b[n++] = (char)c;
    c = z81in_read();
  }
  b[n] = 0;
  return n;
}
static uint32_t ld_hex(const char* t){ return strtoul(t, nullptr, 16); }
static bool ld_is_hex(const char* t){
  if (!*t) return false;
  for (; *t; t++) if (!isxdigit((unsigned char)*t)) return false;
  return true;
}
static void dec_reset(){ ld.rle_cnt = 0; }
static int dec_next(){                        // siguiente byte, -1 si no hay
  if (ld.rle_cnt) { ld.rle_cnt--; return ld.rle_val; }
  char t[16];
  if (!ld_tok(t, sizeof(t))) return -1;
  if (t[0] == '*') {
    char v[8];
    uint32_t c = ld_hex(t + 1);
    if (!c || !ld_tok(v, sizeof(v)) || !ld_is_hex(v)) return -1;
    ld.rle_val = ld_hex(v);
    ld.rle_cnt = c - 1;
    return ld.rle_val;
  }
  if (!ld_is_hex(t)) return -1;
  return ld_hex(t) & 0xFF;
}
static bool dec_skip(uint32_t n){             // n bytes, que acaben justo ahi
  while (n) {
    if (ld.rle_cnt) {
      uint32_t k = n < ld.rle_cnt ? n : ld.rle_cnt;
      ld.rle_cnt -= k; n -= k;
      continue;
    }
    if (dec_next() < 0) return false;
    n--;
  }
  return ld.rle_cnt == 0;
}
static bool dec_read(uint8_t* b, uint16_t n){
  for (uint16_t i = 0; i < n; i++) {
    int v = dec_next();
    if (v < 0) return false;
    b[i] = v;
  }
  return true;
}

// Valor de reset de cada POKE de control (-1: no es un registro que se
// pueda devolver a su valor de reset con un POKE)
static int16_t pk_default(uint16_t a){
  static const uint8_t bchr[8] = {0x00, 0x3C, 0x42, 0x42, 0x7E, 0x42, 0x42, 0x00};
  if (a >= 2048 && a <= 2055) return bchr[a - 2048];
  switch (a) {
    case 2038: case 2039: case 2040: case 2043: case 2044: return 0;
    case 2045: case 2047: case 2057: case 2058: case 2061: case 2062: case 2098: return 85;
    case 2046: return 0x0F;
    case 2059: case 2060: case 2090: case 2094: case 2095: case 2096: case 2097: return 0;
    case 2091: case 2092: case 2093: return 0xFF;
  }
  return -1;
}
static void pk_set(uint16_t a, uint8_t v){    // valor sacado de las claves de EightyOne
  int i = a - POKE_FIRST;
  ld.pk[i] = v;
  ld.pk_mode[i] = (pk_default(a) == v) ? 2 : 1;
}

static void ld_cpu_key(const char* k, uint32_t v){
  static const struct { const char* k; uint8_t r; } map[] = {
    {"PC", R_PC}, {"SP", R_SP}, {"HL", R_HL}, {"DE", R_DE}, {"BC", R_BC}, {"AF", R_AF},
    {"HL_", R_HL_}, {"DE_", R_DE_}, {"BC_", R_BC_}, {"AF_", R_AF_}, {"IX", R_IX}, {"IY", R_IY}
  };
  for (auto& m : map)
    if (!strcmp(k, m.k)) { ld.cpu[m.r] = v & 0xFF; ld.cpu[m.r + 1] = (v >> 8) & 0xFF; return; }
  if (!strcmp(k, "IR")) { ld.cpu[R_I] = v >> 8; ld.cpu[R_R] = v & 0xFF; }
  else if (!strcmp(k, "IM")) ld.im = v;
  else if (!strcmp(k, "IF1")) ld.cpu[R_IFF] = v ? 1 : 0;
}

// Primera pasada (comando 75, con el Z80 esperando). Devuelve 0 si se puede
// cargar (la ROM lanza la trampa), 0xFF si no hay monitor (la ROM usa el
// cargador de siempre), 1 sin fichero, 2 sin [MEMORY], 3 fichero mal hecho.
uint8_t dbg_z81_prepare(const char* path, uint8_t* im){
  if (stopped) { *im = 1; return 0xFF; }
  return ld_prepare(path, im);
}

static uint8_t ld_prepare(const char* path, uint8_t* im){
  *im = 1;
  if (!debug_monitor_loaded || snap_busy() || ld_busy()) return 0xFF;
  if (!z81in_open(path)) return 1;
  memset(&ld, 0, sizeof(ld));
  mcustate_clear();
  ld.nmi = 0xFF; ld.im = 1;
  for (int i = 0; i < 8; i++) ld.cfg[i] = -1;
  for (int i = 0; i < POKE_N; i++) {
    int16_t d = pk_default(POKE_FIRST + i);
    ld.pk[i] = d < 0 ? 0 : d;
    ld.pk_mode[i] = d < 0 ? 0 : 2;
  }

  enum { S_NONE, S_CPU, S_ZX81, S_MEM, S_COL, S_SD81, S_HIRES, S_CHRGEN } sec = S_NONE;
  char t[MAX_FILENAME_LEN], k[24];
  bool bad = false, have_mem = false, hw_pokes = false, hw_cfg = false, chroma_sd81 = false;
  int dm = -1, wide = 0;
  while (!bad && ld_tok(t, sizeof(t))) {
    if (t[0] == '[') {
      if (!strcmp(t, "[EOF]")) break;
      sec = !strcmp(t, "[CPU]") ? S_CPU : !strcmp(t, "[ZX81]") ? S_ZX81 : !strcmp(t, "[MEMORY]") ? S_MEM :
            !strcmp(t, "[COLOUR]") ? S_COL : !strcmp(t, "[SD81BOOSTER]") ? S_SD81 :
            !strcmp(t, "[HIGH_RESOLUTION]") ? S_HIRES : !strcmp(t, "[CHR$_GENERATOR]") ? S_CHRGEN : S_NONE;
      continue;
    }
    strncpy(k, t, sizeof(k) - 1); k[sizeof(k) - 1] = 0;
    switch (sec) {
      case S_CPU:
        if (!ld_tok(t, sizeof(t))) bad = true; else ld_cpu_key(k, ld_hex(t));
        break;
      case S_ZX81:
        if (!strcmp(k, "NMI") && ld_tok(t, sizeof(t))) ld.nmi = ld_hex(t) ? 1 : 0;
        break;
      case S_MEM:
        if (!strcmp(k, "MEMRANGE")) {
          uint32_t a = 0, b = 0;
          if (ld_tok(t, sizeof(t))) a = ld_hex(t); else bad = true;
          if (ld_tok(t, sizeof(t))) b = ld_hex(t); else bad = true;
          if (bad || b < a || a < 0x2000 || b > 0xFFFF) { bad = true; break; }
          ld.mem_start = a; ld.mem_len = b - a + 1;
          ld.mem_pos = z81in_pos();
          dec_reset();
          if (!dec_skip(ld.mem_len)) bad = true;
          have_mem = true;
        } else if (!strcmp(k, "RAM_PACK") || !strcmp(k, "8K_RAM_ENABLED") || !strcmp(k, "ROM_PROTECTED") ||
                   !strcmp(k, "8K_RAM_PROTECTED"))
          ld_tok(t, sizeof(t));
        break;
      case S_COL:
        if (!strcmp(k, "TYPE")) {
          if (ld_tok(t, sizeof(t)) && !strcmp(t, "Chroma")) {
            ld.col_pos = z81in_pos();
            dec_reset();
            if (!dec_skip(16384)) bad = true;
            ld.has_col = true;
          }
        } else if (!strcmp(k, "CHROMA_MODE")) {
          if (ld_tok(t, sizeof(t)) && !chroma_sd81) { ld.has_chroma = true; ld.chroma = ld_hex(t); }
        }
        break;
      case S_HIRES:                           // .Z81 sin [SD81BOOSTER]
        if (!strcmp(k, "TYPE") && ld_tok(t, sizeof(t)) && strcmp(t, "None") && !hw_pokes)
          pk_set(2058, !strcmp(t, "WRX") ? 170 : 85);
        break;
      case S_CHRGEN:
        if (!strcmp(k, "TYPE") && ld_tok(t, sizeof(t)) && strcmp(t, "None") && !hw_cfg) {
          ld.cfg[cfgcmd_128CHARS] = !strcmp(t, "CHR$128");
          ld.cfg[cfgcmd_256CHARS] = !strcmp(t, "CHR$256");
        }
        break;
      case S_SD81: {
        uint32_t v[8];
        auto vals = [&](int n){ for (int i = 0; i < n; i++) { if (!ld_tok(t, sizeof(t))) { bad = true; return; } v[i] = ld_hex(t); } };
        if (!strcmp(k, "CUR_DIR")) {
          if (ld_tok(t, sizeof(t))) { ld.has_dir = true; strncpy(ld.dir, t, sizeof(ld.dir) - 1); }
        } else if (!strcmp(k, "MAPPER")) {
          vals(8);
          for (int i = 0; i < 8; i++) ld.mapper[i] = v[i] & 0x3F;
          ld.has_mapper = !bad;
        } else if (!strcmp(k, "DISPLAY_MODE")) { vals(1); dm = v[0]; }
        else if (!strcmp(k, "WIDE_COLS")) { vals(1); wide = v[0]; }
        else if (!strcmp(k, "BORDER_INK")) { vals(1); if (!hw_pokes) pk_set(2046, v[0]); }
        else if (!strcmp(k, "BORDER_PATTERN")) { vals(1); if (!hw_pokes) pk_set(2047, v[0] ? 170 : 85); }
        else if (!strcmp(k, "BORDER_CHARS")) { vals(8); if (!hw_pokes) for (int i = 0; i < 8; i++) pk_set(2048 + i, v[i]); }
        else if (!strcmp(k, "HFILE")) { vals(1); if (!hw_pokes) { pk_set(2043, v[0] & 0xFF); pk_set(2044, v[0] >> 8); } }
        else if (!strcmp(k, "CHROMA_MODE")) { vals(1); ld.has_chroma = true; ld.chroma = v[0]; chroma_sd81 = true; }
        else if (!strcmp(k, "DBUF")) {
          vals(1);
          if (!hw_pokes) pk_set(2057, (v[0] & 0x80) ? (((v[0] & 0x40) ? 168 : 200) + (v[0] & 7)) : 85);
        }
        else if (!strcmp(k, "SEL128")) { vals(1); if (!hw_cfg) ld.cfg[cfgcmd_128CHARS] = v[0] ? 1 : 0; }
        else if (!strcmp(k, "SEL256")) { vals(1); if (!hw_cfg) ld.cfg[cfgcmd_256CHARS] = v[0] ? 1 : 0; }
        else if (!strcmp(k, "WRX")) { vals(1); if (!hw_pokes) pk_set(2058, v[0] ? 170 : 85); }
        else if (!strcmp(k, "ROMLOCK")) { vals(1); if (!hw_cfg) ld.cfg[cfgcmd_ROMLOCK] = v[0] ? 1 : 0; }
        else if (!strcmp(k, "SCROLL")) { vals(4); if (!hw_pokes) for (int i = 0; i < 4; i++) pk_set(2090 + i, v[i]); }
        else if (!strcmp(k, "DISP_ADDR")) {
          vals(2);
          if (!hw_pokes && v[1]) { pk_set(2096, v[0] & 0xFF); pk_set(2097, v[0] >> 8); pk_set(2098, 170); }
        }
        else if (!strcmp(k, "ATTR_ADDR")) {
          vals(2);
          if (!hw_pokes && v[1]) { pk_set(2059, v[0] & 0xFF); pk_set(2060, v[0] >> 8); pk_set(2061, 170); }
        }
        else if (!strcmp(k, "DIR_OPEN")) {
          if (ld_tok(t, sizeof(t))) {
            ld.dir_open = true;
            int n = 0;
            if (strcmp(t, "-"))
              for (int i = 0; t[i] && t[i + 1] && n < (int)sizeof(ld.dir_arg) - 1; i += 2) {
                char h[3] = {t[i], t[i + 1], 0};
                ld.dir_arg[n++] = ld_hex(h);
              }
            ld.dir_arg[n] = 0;
          }
        }
        else if (!strcmp(k, "HW_POKES")) {   // los POKEs tal cual: mandan sobre lo de arriba
          hw_pokes = true;
          for (int i = 0; i < POKE_N && !bad; i++) {
            if (!ld_tok(t, sizeof(t))) { bad = true; break; }
            int16_t d = pk_default(POKE_FIRST + i);
            if (strcmp(t, "--")) { ld.pk[i] = ld_hex(t); ld.pk_mode[i] = 1; }
            else { ld.pk[i] = d < 0 ? 0 : d; ld.pk_mode[i] = d < 0 ? 0 : 2; }
          }
        }
        else if (!strcmp(k, "HW_CFG")) {
          vals(7);
          static const uint8_t order[7] = {cfgcmd_MC45, cfgcmd_MODE48K, cfgcmd_QSEN, cfgcmd_FULLPAG,
                                           cfgcmd_128CHARS, cfgcmd_256CHARS, cfgcmd_ROMLOCK};
          for (int i = 0; i < 7; i++) ld.cfg[order[i]] = v[i] ? 1 : 0;
          hw_cfg = true;
        }
        else if (!strcmp(k, "RAM_PAGE") || !strcmp(k, "SHADOW")) {
          bool shadow = k[0] == 'S';
          vals(1);
          if (bad) break;
          if (shadow) ld.shadow_pos[v[0] & 7] = z81in_pos();
          else ld.page_pos[v[0] & 63] = z81in_pos();
          dec_reset();
          if (!dec_skip(8192) || !ld_tok(t, sizeof(t)) || strcmp(t, shadow ? "SHADOW_END" : "RAM_PAGE_END")) bad = true;
        }
        else if (!strcmp(k, "AY1_REGS") || !strcmp(k, "AY3_REGS")) {
          int c = k[2] == '3';
          for (int i = 0; i < 16 && !bad; i++) {
            if (!ld_tok(t, sizeof(t))) bad = true; else ld.ay[c][i] = ld_hex(t);
          }
          ld.has_ay[c] = !bad;
        }
        else if (!strcmp(k, "AY1_REG_SEL") || !strcmp(k, "AY3_REG_SEL")) { vals(1); ld.ay_sel[k[2] == '3'] = v[0]; }
        else if (!strcmp(k, "SPRITE_SEL")) { vals(1); ld.spr_sel = v[0]; }
        else if (!strcmp(k, "SPRITE")) {      // n en x y, 8 colores, 8 filas, 8 mascaras
          uint32_t h[4];
          for (int i = 0; i < 4 && !bad; i++) { if (!ld_tok(t, sizeof(t))) bad = true; else h[i] = ld_hex(t); }
          if (bad) break;
          uint8_t* f = ld.spr[h[0] & 31];
          f[0] = h[1]; f[1] = h[2] & 0xFF; f[2] = (h[2] >> 8) & 1; f[3] = h[3];
          for (int i = 4; i < 28 && !bad; i++) { if (!ld_tok(t, sizeof(t))) bad = true; else f[i] = ld_hex(t); }
        }
        else mcustate_key(k, ld_tok);         // AY2, VGM, PEG, FILE_HANDLE
        break;
      }
      default: break;
    }
  }
  if (!hw_pokes && dm >= 0) {                 // el modo de video de EightyOne
    uint8_t m = 85;
    if (wide == 0x46) m = 173;
    else if (wide == 0x50) m = 174;
    else if (dm & 1) m = (dm & 2) ? 171 : (dm & 4) ? 172 : 170;
    pk_set(2045, m);
  }
  if (bad || !have_mem) {
    z81in_close();
    return bad ? 3 : 2;
  }
  ld.pending = true;
  *im = ld.im;
  return 0;
}

// Pide un trozo de la fuente en curso (ya colocada) en un buffer libre
static int ld_chunk(uint16_t n){
  static uint8_t next = 0;
  uint8_t k = next;
  next = (next + 1) % LD_NBUF;
  if (!dec_read(ld_buf[k], n)) return -1;
  return k;
}

static void ld_abort(const char* why){
  z81in_close();
  ld.stage = LD_OFF;
  set_blinking_off();
  send_bit_config(cfgcmd_DBGPOKE, 0);
  Serial.printf("Snapshot load failed: %s. The program is half loaded: reset.\r\n", why);
  set_status_LED(clMAGENTA);
}

// La BRAM de sombra: el puntero
static void ld_bram_ptr(uint16_t a){
  q_out(DBG_PORT, 0x85); q_out(DBG_PORT, a & 0xFF);
  q_out(DBG_PORT, 0x86); q_out(DBG_PORT, a >> 8);
}

// Empieza al parar en la trampa de la ROM
static void ld_begin(){
  ld.pending = false;
  view_drop();                                // la carga repone los POKEs
  ui_want = false;                            // y la pantalla, si estaba, se quita bien antes
  ui_run();
  dirty_clear();                              // las paginas escritas seran las de la carga
  set_blinking(clCYAN, 4);
  Serial.println("\r\nLoading snapshot ...");
  if (ld.has_dir) { strncpy(current_dir, ld.dir, MAX_FILENAME_LEN - 1); current_dir[MAX_FILENAME_LEN - 1] = 0; }
  static const uint8_t first[] = {cfgcmd_FULLPAG, cfgcmd_MC45, cfgcmd_MODE48K, cfgcmd_128CHARS, cfgcmd_256CHARS};
  for (uint8_t c : first) if (ld.cfg[c] >= 0) send_bit_config(c, ld.cfg[c]);
  if (ld.cfg[cfgcmd_QSEN] >= 0) { nQS_en = ld.cfg[cfgcmd_QSEN]; send_bit_config(cfgcmd_QSEN, nQS_en ? 1 : 0); }
  ld.romlock_old = cfg_value(cfgcmd_ROMLOCK);
  if (ld.romlock_old) send_bit_config(cfgcmd_ROMLOCK, 0);   // si no, los POKEs no hacen nada
  if (ld.dir_open) opendir_list(ld.dir_arg);
  // Mientras se carga, nada suena: el MCU esta ocupado mandando la memoria
  // y un VGM se arrastraria. Su estado (y el de los AY de la FPGA) se pone
  // al final, justo antes de seguir (ld_finish)
  mcustate_quiet();
  for (int c = 0; c < 2; c++) {               // los AY de la FPGA, sin volumen
    DbgReq* r = q_push(OP_AYWRITE, c ? AY_PORT_B : AY_PORT_A, 16);
    if (r) memset(r->data, 0, 16);
  }
  if (!ld.has_mapper)                         // sin MAPPER: las paginas de ahora
    for (int b = 0; b < 8; b++) q_push(OP_IN, (b << 8) | 0xE7, 0, TAG_LD_MAP, b);
  ld.stage = LD_POKES;
}

// Los POKEs de control: el modo de video primero (85 apaga los D_FILE y
// atributos alternativos), despues el resto en orden, los 32 sprites (los
// que no vienen, a cero), y al final las interrupciones simuladas
// (2038-2040) y el 2056 (bloque 0 escribible). En tres tandas: la cola no
// da para todo de una vez
static void ld_wr(uint16_t a, uint8_t n){
  DbgReq* r = q_push(OP_WRITE, a, n);
  if (r) for (int i = 0; i < n; i++) r->data[i] = ld.pk[a - POKE_FIRST + i];
}
static void ld_pokes(){
  send_bit_config(cfgcmd_DBGPOKE, 1);         // el monitor escribe como el programa
  auto wr = ld_wr;
  auto run = [&](uint16_t from, uint16_t to){   // los tramos seguidos que hay que escribir
    for (uint16_t a = from; a <= to; ) {
      if (!ld.pk_mode[a - POKE_FIRST] || a == 2045 || a == 2056 || (a >= 2038 && a <= 2040)) { a++; continue; }
      uint16_t b = a;
      while (b < to && b - a < 31 && ld.pk_mode[b + 1 - POKE_FIRST] && b + 1 != 2045 && b + 1 != 2056) b++;
      wr(a, b - a + 1);
      a = b + 1;
    }
  };
  if (ld.pk_mode[2045 - POKE_FIRST]) wr(2045, 1);
  run(2041, POKE_LAST);
}
static void ld_sprites(){                     // 8 sprites cada vez
  for (int k = 0; k < 8 && ld.page < 32; k++, ld.page++) {
    DbgReq* r = q_push(OP_WRITE, 2100, 1);
    if (r) r->data[0] = ld.page;
    r = q_push(OP_WRITE, 2101, 28);
    if (r) memcpy(r->data, ld.spr[ld.page], 28);
  }
}
static void ld_pokes_late(){
  DbgReq* r = q_push(OP_WRITE, 2100, 1);
  if (r) r->data[0] = ld.spr_sel;
  if (ld.pk_mode[2038 - POKE_FIRST]) ld_wr(2038, 2);
  if (ld.pk_mode[2040 - POKE_FIRST]) ld_wr(2040, 1);
  if (ld.pk_mode[2056 - POKE_FIRST] == 1) ld_wr(2056, 1);
  q_push(OP_IN, 0x00E7, 0, TAG_LD_SYNC);      // cuando llegue, ya estan todos (IN inocuo)
}

static void ld_finish(){
  // el mapper (WRITEP ya no usa el bloque 7) y Chroma81
  if (ld.has_mapper) {
    bool full = cfg_value(cfgcmd_FULLPAG);
    for (int b = 0; b < 8; b++) {
      uint8_t p = ld.mapper[b];
      q_out((p << 8) | 0xE7, full ? b : (((p & 31) << 3) | b));
    }
  }
  if (ld.has_chroma) q_out(0x7FEF, ld.chroma);
  for (int c = 0; c < 2; c++) {               // los AY de la FPGA y su registro elegido
    if (!ld.has_ay[c]) continue;
    DbgReq* r = q_push(OP_AYWRITE, c ? AY_PORT_B : AY_PORT_A, 16);
    if (r) memcpy(r->data, ld.ay[c], 16);
    q_out(c ? AY_PORT_B : AY_PORT_A, ld.ay_sel[c]);
  }
  send_bit_config(cfgcmd_ROMLOCK, ld.cfg[cfgcmd_ROMLOCK] >= 0 ? ld.cfg[cfgcmd_ROMLOCK] : ld.romlock_old);

  memcpy(regs, ld.cpu, REGS_LEN);
  regs[R_PAGE1] = ld.mapper[1];
  if (ld.nmi == 1) {                          // vuelta por OUT ($FE),A / RET debajo de la pila
    uint16_t s = reg16(R_SP), pc = reg16(R_PC);
    DbgReq* r = q_push(OP_WRITE, s - 8, 8);
    if (r) {
      static const uint8_t stub[8] = {0xD3, 0xFE, 0xC9, 0, 0, 0, 0, 0};
      memcpy(r->data, stub, 8);
      r->data[6] = pc & 0xFF; r->data[7] = pc >> 8;
    }
    set_reg16(R_SP, s - 2);
    set_reg16(R_PC, s - 8);
    regs[R_R] = (regs[R_R] & 0x80) | ((regs[R_R] - 2) & 0x7F);   // las dos M1 del OUT y el RET
  }
  z81in_close();
  ld.stage = LD_OFF;
  set_blinking_off();
  stop_foreign = false;
  stop_nmi = false;                           // la NMI la pone la carga (ld.nmi)
  prog_slow = false;
  Serial.printf("Snapshot loaded: PC=%04X\r\n", reg16(R_PC));
  mcustate_apply();                           // AY del MCU, VGM, PEG, ficheros: ahora, al seguir
  DbgReq* r = q_push(OP_SETREGS, 0, ld.set_im ? REGS_LEN + 1 : REGS_LEN);
  if (r) {                                    // sin la ROM (L), el IM va detras de los registros
    memcpy(r->data, regs, REGS_LEN);
    r->data[REGS_LEN] = ld.im;
  }
  dbg_continue();
}

// Desde cmd_dbg_poll: pide lo siguiente cuando la cola esta vacia (la
// fuente del RLE se coloca al empezar cada tramo, sin peticiones a medias)
static void ld_feed(){
  if (!ld_busy() || q_count) return;
  switch (ld.stage) {
    case LD_POKES:
      ld_pokes();
      ld.stage = LD_SPRITES; ld.page = 0;
      return;
    case LD_SPRITES:
      ld_sprites();
      if (ld.page >= 32) ld.stage = LD_POKES_LATE;
      return;
    case LD_POKES_LATE:
      ld_pokes_late();
      ld.stage = LD_POKES_WAIT;
      return;
    case LD_POKES_WAIT:                       // sigue en ld_result (TAG_LD_SYNC)
      return;
    case LD_PAGES:
      while (ld.page < 63 && !ld.page_pos[ld.page]) ld.page++;
      if (ld.page >= 63) {                    // la 63 es la del monitor
        ld.stage = LD_MEM; ld.done = 0;
        z81in_seek(ld.mem_pos); dec_reset();
        ld_bram_ptr(ld.mem_start);
        return;
      }
      if (ld.done == 0) { z81in_seek(ld.page_pos[ld.page]); dec_reset(); }
      for (int i = 0; i < LD_NBUF && ld.done < 8192; i++) {
        int k = ld_chunk(256);
        if (k < 0) { ld_abort("bad RAM_PAGE"); return; }
        DbgReq* r = q_push(OP_WRITEP, ld.done, 256, TAG_NONE, k);
        r->page = ld.page;
        ld.done += 256;
      }
      if (ld.done >= 8192) { ld.page++; ld.done = 0; }
      return;
    case LD_MEM:
      for (int i = 0; i < LD_NBUF && ld.done < ld.mem_len; i++) {
        uint32_t a = ld.mem_start + ld.done;
        uint16_t n = 256 - (a & 0xFF);
        if (n > ld.mem_len - ld.done) n = ld.mem_len - ld.done;
        int k = ld_chunk(n);
        if (k < 0) { ld_abort("bad [MEMORY]"); return; }
        uint8_t p = ld.mapper[a >> 13];
        if ((a >> 13) && p != 63 && !ld.page_pos[p]) {   // pagina sin RAM_PAGE: de [MEMORY]
          DbgReq* r = q_push(OP_WRITEP, a & 0x1FFF, n, TAG_NONE, k);
          r->page = p;
        }
        if (!(ld.has_col && a >= 0xC000)) q_push(OP_BRAMW, 0, n, TAG_NONE, k);
        ld.done += n;
      }
      if (ld.done >= ld.mem_len) {
        ld.done = 0;
        ld.stage = LD_COLOUR;
        if (ld.has_col) { z81in_seek(ld.col_pos); dec_reset(); ld_bram_ptr(0xC000); }
      }
      return;
    case LD_COLOUR:
      if (ld.has_col)
        for (int i = 0; i < LD_NBUF && ld.done < 16384; i++) {
          int k = ld_chunk(256);
          if (k < 0) { ld_abort("bad [COLOUR]"); return; }
          q_push(OP_BRAMW, 0, 256, TAG_NONE, k);
          ld.done += 256;
        }
      if (!ld.has_col || ld.done >= 16384) {
        ld.done = 0;
        ld.stage = LD_BLOCK0;
        uint32_t p0 = ld.page_pos[ld.mapper[0]];
        if (p0) { z81in_seek(p0); dec_reset(); ld_bram_ptr(0x0000); }
      }
      return;
    case LD_BLOCK0:
      if (ld.page_pos[ld.mapper[0]])        // el bloque 0 es RAM (CP/M): su sombra,
        for (int i = 0; i < LD_NBUF && ld.done < 8192; i++) {   // menos la copia de los sprites
          int k = ld_chunk(256);
          if (k < 0) { ld_abort("bad RAM_PAGE"); return; }
          if (ld.done < SPR_MIRROR || ld.done >= SPR_MIRROR + 0x400) q_push(OP_BRAMW, 0, 256, TAG_NONE, k);
          ld.done += 256;
          if (ld.done == SPR_MIRROR) { ld_bram_ptr(SPR_MIRROR + 0x400); break; }
        }
      if (!ld.page_pos[ld.mapper[0]] || ld.done >= 8192) {
        // 2038-2098 en la sombra: el valor de los POKEs escritos y el byte de
        // la ROM en los demas (como al arrancar: asi los ve el proximo snap).
        // Con SHADOW 00 no hace falta: trae el bloque 0 tal cual
        uint8_t* b = ld_buf[0];
        if (!ld.shadow_pos[0] && rom_file_read(POKE_FIRST, b, POKE_N) == POKE_N) {
          for (int i = 0; i < POKE_N; i++) if (ld.pk_mode[i] == 1) b[i] = ld.pk[i];
          ld_bram_ptr(POKE_FIRST);
          q_push(OP_BRAMW, 0, POKE_N, TAG_NONE, 0);
        }
        ld.stage = LD_SHADOW; ld.page = 0; ld.done = 0;
      }
      return;
    case LD_SHADOW:                           // los bloques de la sombra que trae el fichero
      while (ld.page < 8 && !ld.shadow_pos[ld.page]) ld.page++;
      if (ld.page >= 8) { ld.stage = LD_FINISH; return; }
      if (ld.done == 0) {
        z81in_seek(ld.shadow_pos[ld.page]); dec_reset();
        ld_bram_ptr(ld.page * 0x2000);
      }
      for (int i = 0; i < LD_NBUF && ld.done < 8192; i++) {
        int k = ld_chunk(256);
        if (k < 0) { ld_abort("bad SHADOW"); return; }
        q_push(OP_BRAMW, 0, 256, TAG_NONE, k);
        ld.done += 256;
      }
      if (ld.done >= 8192) { ld.page++; ld.done = 0; }
      return;
    case LD_FINISH:
      ld_finish();
      return;
  }
}

static void ld_result(uint8_t tag, uint8_t* data, uint16_t n){
  if (tag == TAG_LD_MAP) ld.mapper[last.arg] = n ? data[0] & 0x3F : 0;
  else if (tag == TAG_LD_SYNC) {
    send_bit_config(cfgcmd_DBGPOKE, 0);
    ld.stage = LD_PAGES; ld.page = 0; ld.done = 0;
  }
}

static bool ld_pending(){ return ld.pending; }
static uint8_t* ld_data(uint8_t k){ return ld_buf[k % LD_NBUF]; }
static void ld_cancel(){
  if (ld.pending || ld_busy()) z81in_close();
  ld.pending = false;
  ld.stage = LD_OFF;
}

// Las paginas escritas por la CPU (FPGA, orden 11): se borran al cargar un
// programa o un snapshot y en el reset; el snapshot guarda las mapeadas y
// las escritas desde entonces
static void dirty_clear(){
  send_bit_config(cfgcmd_DBGDIRTY, 1);
  send_bit_config(cfgcmd_DBGDIRTY, 0);
}

// ---------------------------------------------------------------------
// v: video mientras esta parado. Un programa en SLOW o en FAST no tiene
// imagen parado (la NMI esta apagada); la FPGA si puede pintar desde la BRAM
// de sombra, donde esta todo lo que ha escrito la CPU:
//   v       Superfast texto (POKE 2045,170): su D_FILE
//   v dir   Superfast HiRes (POKE 2045,171) con el mapa de bits en dir: 32
//           bytes x 192 lineas seguidas, como WRX. La FPGA lo pinta desde el
//           principio de un bloque de 8K (solo usa HFILE[15:13]): si dir no
//           lo es, el MCU guarda 6K de la sombra del bloque de dir, copia ahi
//           el mapa de bits (leido de la memoria) y al quitarla repone la
//           sombra como estaba
// Se pone con la orden 10, desde el monitor. Se quita al seguir de verdad
// (c, g, o sobre un CALL, u, S), antes de un snapshot o con otra v: 2045 y
// HFILE vuelven a su valor (85 / 0 si nunca se escribieron) y sus bytes de
// la sombra a los de antes. 85 apaga el D_FILE y los atributos
// alternativos: si estaban, se vuelven a poner. Los pasos la mantienen
// ---------------------------------------------------------------------
#define VIEW_HR_LEN 6144
#define VIEW_CHUNKS (VIEW_HR_LEN / 256)
static bool view_on = false;
static bool view_hr = false;                // HiRes (v dir)
static bool view_copied = false;            // mapa de bits copiado en la sombra (dir no alineada)
static bool view_copying = false;           // copiandolo: la consola y el boton QS esperan
static uint16_t view_dir = 0, view_blk = 0;  // donde esta y desde donde lo pinta la FPGA
static uint8_t view_pk[POKE_N];             // 2038-2098 en la sombra al activarla
static uint8_t view_buf[2][VIEW_HR_LEN];    // [0] el mapa de bits, [1] la sombra que se pisa
static uint32_t view_t[3];                  // la copia: inicio (0 = sin medir), sombra leida, memoria leida

static bool view_is_on(){ return view_on; }
static bool view_busy(){ return view_copying; }
static void view_drop(){ view_on = false; view_copying = false; }
static uint8_t* ui_buf(uint8_t which);
static uint8_t* view_data(uint8_t which, uint16_t off){
  return (which >= 2 ? ui_buf(which) : view_buf[which & 1]) + off;
}

// Lo que tarda la copia, por partes (6144 bytes cada una): la velocidad del
// enlace con el monitor en cada sentido
static void view_times(){
  if (!view_t[0]) return;
  uint32_t now = millis();
  Serial.printf("Video: copy timing - shadow read %lu ms, memory read %lu ms, shadow write %lu ms\r\n",
                view_t[1] - view_t[0], view_t[2] - view_t[1], now - view_t[2]);
  view_t[0] = 0;
}

static void view_bram_ptr(uint16_t a){
  q_out(DBG_PORT, 0x85); q_out(DBG_PORT, a & 0xFF);
  q_out(DBG_PORT, 0x86); q_out(DBG_PORT, a >> 8);
}

// Primero, los POKEs de control en la sombra (indice 2)
static void view_request(bool hr, uint16_t dir){
  view_hr = hr;
  view_dir = dir;
  view_bram_ptr(POKE_FIRST);
  q_out(DBG_PORT, 0x02);
  q_push(OP_INSEQ, DBG_PORT, POKE_N, TAG_VIEW);
}

static void view_apply(){
  poke_begin();                               // el monitor escribe como el programa
  if (view_hr) {
    DbgReq* r = q_push(OP_WRITE, 2043, 3);    // HFILE y el modo
    if (r) { r->data[0] = view_blk & 0xFF; r->data[1] = view_blk >> 8; r->data[2] = 171; }
  } else q_write1(2045, 170);
  q_push(OP_IN, 0x00E7, 0, TAG_VIEW_SYNC);    // cuando llegue, la orden 10 fuera
  view_on = true;
  if (view_hr) Serial.printf("Video: Superfast HiRes, bitmap at %04X%s (v again, or continue, to go back)\r\n",
                             view_dir, view_copied ? " (copied to the shadow)" : "");
  else Serial.println("Video: Superfast text while stopped (v again, or continue, to go back)");
}

static void view_result(uint8_t* d, uint16_t n){
  if (n < POKE_N) return;
  uint8_t m = d[2045 - POKE_FIRST];
  if (!view_hr && m >= 170 && m <= 174) { Serial.printf("Video: the program is already Superfast (POKE 2045,%d)\r\n", m); return; }
  memcpy(view_pk, d, POKE_N);
  view_copied = view_hr && (view_dir & 0x1FFF);
  view_blk = view_dir & 0xE000;
  if (!view_copied) { view_apply(); return; }
  // no alineado: guardar la sombra que se va a pisar y leer el mapa de bits
  if (!view_blk) { Serial.println("v addr: below 2000 only 0000 (the copy would hide the ROM)"); return; }
  Serial.println("Video: copying the bitmap ...");
  view_copying = true;
  view_t[0] = millis();
  view_bram_ptr(view_blk);
  q_out(DBG_PORT, 0x02);
  for (int k = 0; k < VIEW_CHUNKS; k++) q_push(OP_INSEQ, DBG_PORT, 256, TAG_VIEW_SAVE, k);
  for (int k = 0; k < VIEW_CHUNKS; k++) q_push(OP_READ, view_dir + k * 256, 256, TAG_VIEW_COPY, k);
}

static void view_part(uint8_t tag, uint8_t k, uint8_t* d, uint16_t n){
  if (k >= VIEW_CHUNKS) return;
  memcpy(view_buf[tag == TAG_VIEW_SAVE ? 1 : 0] + k * 256, d, n < 256 ? n : 256);
  if (k == VIEW_CHUNKS - 1) view_t[tag == TAG_VIEW_SAVE ? 1 : 2] = millis();
  if (tag != TAG_VIEW_COPY || k != VIEW_CHUNKS - 1) return;
  view_copying = false;                       // lo que queda va en la cola, antes que lo siguiente
  view_bram_ptr(view_blk);                    // el mapa de bits, al principio del bloque
  for (int j = 0; j < VIEW_CHUNKS; j++) {
    DbgReq* r = q_push(OP_BRAMW, j * 256, 256, TAG_NONE, 0xFE);
    if (r) r->page = 0;
  }
  view_apply();
}

static void view_off(){
  if (!view_on) return;
  view_on = false;
  uint8_t rom[3] = {0, 0, 0};
  bool rom_ok = rom_file_read(2043, rom, 3) == 3;
  const uint8_t* sh = view_pk + (2043 - POKE_FIRST);       // 2043, 2044, 2045 en la sombra
  auto never = [&](int i){ return rom_ok && rom[i] == sh[i]; };
  uint8_t m = never(2) ? 85 : sh[2];
  poke_begin();
  if (view_hr) {                              // HFILE como estaba (0 si nunca se escribio)
    DbgReq* r = q_push(OP_WRITE, 2043, 2);
    if (r) { r->data[0] = never(0) ? 0 : sh[0]; r->data[1] = never(1) ? 0 : sh[1]; }
  }
  q_write1(2045, m);
  if (m == 85) {                              // 85 apaga el D_FILE y los atributos alternativos
    if (view_pk[2098 - POKE_FIRST] == 170) q_write1(2098, 170);
    if (view_pk[2061 - POKE_FIRST] == 170) q_write1(2061, 170);
  }
  q_push(OP_IN, 0x00E7, 0, TAG_VIEW_SYNC);
  view_bram_ptr(2043);                        // la sombra de 2043-2045, como estaba
  DbgReq* r = q_push(OP_BRAMW, 0, 3, TAG_NONE, 0xFF);
  if (r) memcpy(r->data, sh, 3);
  if (view_copied) {                          // y la del bloque donde se copio el mapa de bits
    view_bram_ptr(view_blk);
    for (int j = 0; j < VIEW_CHUNKS; j++) {
      DbgReq* q = q_push(OP_BRAMW, j * 256, 256, TAG_NONE, 0xFE);
      if (q) q->page = 1;
    }
    view_copied = false;
  }
}

// ---------------------------------------------------------------------
// Pantalla del depurador en el ZX81 (fase 3b), sin FPGA. Mientras esta
// parado, la FPGA pinta en Superfast de 80 columnas (POKE 2045,174) un
// D_FILE que el MCU compone en la sombra del bloque de la ROM ($1000, con
// el override 2096/2097 y 2098,170), escribiendolo con BRAMW: la SRAM del
// programa no se toca. El monitor pone I = $1E mientras espera (SETI, version
// 4): la fuente sale de I. Sin color: el Chroma se apaga mientras se ve, y
// los 128/256 caracteres tambien (la fuente seria otra).
//   Entrar: los POKEs y el Chroma de ahora, y guardar la sombra de $1000.
//           Despues el primer dibujo y, detras, el video.
//   Cada parada: se leen el desensamblado, la pila y el volcado y solo se
//           mandan los trozos que cambian (BRAMW va a ~84 us/byte).
//   Salir (Q, ui, seguir de verdad, snapshot, carga): los POKEs como
//           estaban (85 / 0 si nunca se escribieron), su sombra, la de
//           $1000, el Chroma, los 128/256 caracteres y SETI 0.
// Teclado (al soltar), lo de la consola sin consola:
//   S paso  O por encima  U salir de la rutina  C o ESPACIO seguir
//   G ir a  B breakpoint (pone o quita; ENTER: en el PC)  W vigilancia (R, W
//   o I y la direccion; ENTER la quita)  R registro (P S A B D H X Y I y el
//   valor)  E poke (direccion y bytes; ENTER acaba)  D desensamblar desde
//   (ENTER: el PC)  M volcado desde (ENTER: HL)  5-8 moverlo
//   V la pantalla del programa (Superfast texto si no lo es)  Z snapshot
//   L cargar el ultimo  Q la pantalla del programa como estaba
// La pausa del boton QS empieza con ella. Con la pantalla del programa (V o
// Q), el teclado sigue: S snapshot y sigue, Z snapshot, L cargar el ultimo,
// C o ESPACIO seguir, V la del depurador. Una vez abierta (pausa QS, V o
// ui), sale en cada parada (breakpoint, paso...) hasta ui en la consola,
// que la ensena o la quita del todo
// Las direcciones: hasta 4 cifras hex y ENTER (con 4, sola); otra tecla
// cancela.
// ---------------------------------------------------------------------
#define UI_DF      0x0000                     // D_FILE en la sombra (libre: la FPGA no lee ahi; $1000, la traza)
#define UI_COLS    80
#define UI_ROWS    24
#define UI_STRIDE  81                         // 80 + 1 de relleno por fila (como el NEWLINE)
#define UI_DF_LEN  (1 + UI_ROWS * UI_STRIDE)  // 1945
#define UI_CHUNKS  ((UI_DF_LEN + 255) / 256)
#define UI_DIS_N   12                         // lineas de desensamblado
#define UI_STK_N   12                         // palabras de la pila
#define UI_MEM_N   3                          // lineas de volcado (16 bytes)
#define UI_FONT_I  0x1E                       // la fuente de la ROM

static bool ui_shown = false;       // en la FPGA (o a punto: primer dibujo en camino)
static bool ui_entering = false;    // guardando la sombra: la consola y el boton QS esperan
static bool ui_abort = false;       // quitada mientras entraba
static bool ui_applied = false;     // POKEs, Chroma y caracteres puestos
static bool ui_apply_pend = false;  // ponerlos detras del primer dibujo
static bool ui_seti = false;        // el monitor tiene SETI puesto (se queda en la pagina 63)
static bool ui_fix = false;         // un reset con la pantalla puesta: arreglar la sombra
static uint8_t ui_df[UI_DF_LEN];    // lo que hay (o va a haber) en la sombra
static uint8_t ui_nd[UI_DF_LEN];    // la pantalla nueva
static uint8_t ui_save[UI_DF_LEN];  // la sombra de antes
static uint8_t ui_pk[POKE_N];       // los POKEs de control en la sombra, al entrar
static uint8_t ui_chroma = 0, ui_c128 = 0, ui_c256 = 0;
static uint16_t ui_dis = 0, ui_mem = 0;
static bool ui_mem_set = false;
static uint8_t ui_dis_b[64], ui_stk_b[2 * UI_STK_N], ui_mem_b[16 * UI_MEM_N];
static uint8_t ui_pend = 0;         // lecturas de la pantalla en camino
// Lo que se esta tecleando (fila 1)
enum { UP_NONE, UP_GO, UP_BP, UP_DIS, UP_MEM, UP_POKEADDR, UP_POKE, UP_WMODE, UP_WADDR, UP_REG, UP_REGVAL, UP_TRACE };
static uint8_t ui_pr = UP_NONE, ui_pr_n = 0;  // pregunta y cifras tecleadas
static uint16_t ui_pr_v = 0, ui_poke_a = 0;
static uint8_t ui_wmode = 0, ui_reg = 0;
static char ui_msg[UI_COLS + 1] = "";         // lo que ha pasado (fila 1, hasta la tecla siguiente)
static bool ui_dis_follow = true;           // el desensamblado sigue al PC (D lo suelta hasta la parada siguiente)
static bool ui_prog_view = false;           // la pantalla del programa, con el teclado leyendose (pausa QS, V, Q)
static bool ui_prog_v = false;              // y con Superfast texto (V)
static bool ui_hist = false;                // H: la traza en lugar del desensamblado
static uint8_t ui_krow[8], ui_kprev[8], ui_karmed[8];
static uint32_t ui_kt = 0;

static bool ui_busy(){ return ui_entering || ui_pend; }
static bool ui_entering_now(){ return ui_entering; }
static bool ui_is_shown(){ return ui_shown || ui_entering; }
static uint8_t* ui_buf(uint8_t which){ return which == 2 ? ui_df : ui_save; }

static void ui_bram_ptr(uint16_t a){
  q_out(DBG_PORT, 0x85); q_out(DBG_PORT, a & 0xFF);
  q_out(DBG_PORT, 0x86); q_out(DBG_PORT, a >> 8);
}

// n bytes de un buffer de la pantalla (2 ui_df, 3 ui_save) desde off, a la
// sombra en base + off
static void ui_bramw(uint8_t which, uint16_t base, uint16_t off, uint16_t n){
  ui_bram_ptr(base + off);
  for (uint16_t o = 0; o < n; o += 256) {
    DbgReq* r = q_push(OP_BRAMW, off + o, (n - o) < 256 ? (n - o) : 256, TAG_NONE, 0xFE);
    if (r) r->page = which;
  }
}

// --- texto en el juego de caracteres del ZX81 ---
static uint8_t zx_chr(char c){
  if (c >= 'a' && c <= 'z') c -= 32;
  if (c >= 'A' && c <= 'Z') return 38 + (c - 'A');
  if (c >= '0' && c <= '9') return 28 + (c - '0');
  switch (c) {
    case '"': case '\'': return 11;
    case '$': return 13; case ':': return 14; case '?': return 15;
    case '(': return 16; case ')': return 17; case '>': return 18; case '<': return 19;
    case '=': return 20; case '+': return 21; case '-': return 22; case '*': return 23;
    case '/': return 24; case ';': return 25; case ',': return 26; case '.': return 27;
  }
  return 0;                                   // el espacio y lo que no hay
}

static uint8_t* ui_row(int row){ return ui_nd + 1 + row * UI_STRIDE; }

static void ui_put(int row, int col, const char* t, bool inv = false){
  uint8_t* d = ui_row(row);
  for (; *t && col < UI_COLS; t++, col++) d[col] = zx_chr(*t) | (inv ? 0x80 : 0);
}

static void ui_putf(int row, int col, bool inv, const char* fmt, ...){
  char b[UI_COLS + 1];
  va_list a;
  va_start(a, fmt);
  vsnprintf(b, sizeof(b), fmt, a);
  va_end(a);
  ui_put(row, col, b, inv);
}

static void ui_invert(int row, int c0, int c1){       // c0..c1-1 en inverso
  uint8_t* d = ui_row(row);
  for (int c = c0; c < c1 && c < UI_COLS; c++) d[c] |= 0x80;
}

// Una instruccion; los saltos relativos con su destino. Devuelve los bytes
static int ui_disasm(uint16_t addr, uint8_t* b, int len, char* out, size_t sz){
  uint8_t op = b[0];
  if (len >= 2 && (op == 0x10 || op == 0x18 || (op & 0xE7) == 0x20)) {
    static const char* const jr[] = {"DJNZ ", "JR ", "JR NZ,", "JR Z,", "JR NC,", "JR C,"};
    int i = op == 0x10 ? 0 : op == 0x18 ? 1 : 2 + ((op >> 3) & 3);
    snprintf(out, sz, "%s%04XH", jr[i], (uint16_t)(addr + 2 + (int8_t)b[1]));
    return 2;
  }
  int used = Z80Disassembler::disassemble(out, b, len);
  return used <= 0 ? 1 : used;
}

static void ui_compose(){
  memset(ui_nd, 0, UI_DF_LEN);
  uint16_t pc = reg16(R_PC), sp = reg16(R_SP);
  // 0: titulo y por que ha parado
  ui_invert(0, 0, UI_COLS);
  ui_put(0, 1, "SD81 BOOSTER DEBUGGER", true);
  char b[UI_COLS + 1];
  char where[40] = "";
  if (sym_near(pc, where + 2, sizeof(where) - 3)) { where[0] = ' '; where[1] = '('; strcat(where, ")"); }
  snprintf(b, sizeof(b), "%s AT %04X%s%s", reason_name(regs[R_DBGST]), pc, where, prog_slow ? " (SLOW)" : "");
  ui_put(0, UI_COLS - 1 - (int)strlen(b), b, true);
  // 1: lo que se esta tecleando, o lo que ha pasado
  if (ui_pr) {
    static const char* const q[] = {"", "GO TO", "BREAKPOINT (ENTER: AT PC)", "DISASSEMBLE FROM (ENTER: PC)",
      "MEMORY FROM (ENTER: HL)", "POKE AT", "", "WATCH: R READ  W WRITE  I I/O  (ENTER CLEARS)", "WATCH", "REGISTER: P PC  S SP  A AF  B BC  D DE  H HL  X IX  Y IY  I I", "", "TRACE, INSTRUCTIONS (ENTER: UNTIL A BREAKPOINT; SPACE STOPS IT)"};
    int w = (ui_pr == UP_REGVAL && ui_reg == R_I) ? 2 : 4;
    char h[5] = "----";                       // (el ZX81 no tiene "_")
    h[w] = 0;
    for (int i = 0; i < ui_pr_n; i++) h[i] = "0123456789ABCDEF"[(ui_pr_v >> (4 * (ui_pr_n - 1 - i))) & 15];
    if (ui_pr == UP_POKE) {
      h[2] = 0;
      ui_putf(1, 1, false, "POKE %04X: %s  (2 DIGITS A BYTE, ENTER ENDS)", ui_poke_a, h);
    } else if (ui_pr == UP_REGVAL) {
      static const struct { uint8_t idx; const char* n; } rn[] = {{R_PC, "PC"}, {R_SP, "SP"}, {R_AF, "AF"}, {R_BC, "BC"},
        {R_DE, "DE"}, {R_HL, "HL"}, {R_IX, "IX"}, {R_IY, "IY"}, {R_I, "I"}};
      const char* name = "";
      for (auto& r : rn) if (r.idx == ui_reg) name = r.n;
      ui_putf(1, 1, false, "%s = %s", name, h);
    } else if (ui_pr == UP_WADDR) {
      ui_putf(1, 1, false, "WATCH %s: %s", ui_wmode == 2 ? "READ" : ui_wmode == 3 ? "WRITE" : "I/O", h);
    } else if (ui_pr == UP_WMODE || ui_pr == UP_REG) ui_put(1, 1, q[ui_pr]);
    else ui_putf(1, 1, false, "%s: %s", q[ui_pr], h);
  } else if (ui_msg[0]) ui_put(1, 1, ui_msg);
  // 2-3: registros
  ui_putf(2, 1, false, "PC %04X  SP %04X  AF %04X  BC %04X  DE %04X  HL %04X  IX %04X  IY %04X",
          pc, sp, reg16(R_AF), reg16(R_BC), reg16(R_DE), reg16(R_HL), reg16(R_IX), reg16(R_IY));
  char fl[9];
  for (int i = 0; i < 8; i++) fl[i] = (regs[R_AF] & (0x80 >> i)) ? "SZ5H3PNC"[i] : '-';
  fl[8] = 0;
  ui_putf(3, 1, false, "AF'%04X  BC'%04X  DE'%04X  HL'%04X  I %02X  R %02X  IFF %d  F %s  PAGE %02X",
          reg16(R_AF_), reg16(R_BC_), reg16(R_DE_), reg16(R_HL_), regs[R_I], regs[R_R], regs[R_IFF], fl, regs[R_PAGE1]);
  // 4: breakpoints y punto de vigilancia
  {
    char t[UI_COLS + 1];
    int p = snprintf(t, sizeof(t), "BREAKPOINTS");
    int nb = 0;
    for (int i = 0; i < NBP && p < 60; i++)
      if (bps[i].used && !bps[i].temp) { p += snprintf(t + p, sizeof(t) - p, " %04X", bps[i].addr); nb++; }
    if (!nb) p += snprintf(t + p, sizeof(t) - p, " NONE");
    if (watch_mode) snprintf(t + p, sizeof(t) - p, "   WATCH %s %04X", watch_name(), watch_addr);
    ui_put(4, 1, t);
  }
  // 5-17: desensamblado (con el PC en inverso) y la pila
  ui_invert(5, 0, 57);
  bool hist_ft = ui_hist && !trace_slow_valid;   // H: el historial de la FPGA, o la traza lenta
  if (hist_ft) ui_putf(5, 1, true, ft_on ? "HISTORY (THE LAST %d)" : "HISTORY (OFF: TRON IN THE CONSOLE)", ft_n);
  else if (ui_hist) ui_putf(5, 1, true, "TRACE (THE LAST %d OF %lu)", trace_n < UI_DIS_N ? (int)trace_n : UI_DIS_N, (unsigned long)trace_total);
  else ui_put(5, 1, "DISASSEMBLY", true);
  ui_invert(5, 58, UI_COLS);
  ui_putf(5, 59, true, "STACK %04X", sp);
  int pos = 0;
  if (hist_ft) {                              // el historial: las ultimas, la mas reciente abajo
    for (int k = 0; k < ft_n && k < UI_DIS_N; k++) {
      char t[96];
      ui_disasm(ft_pc[k], ft_op[k], 4, t, sizeof(t));
      sym_subst(t, sizeof(t));
      const char* lab = sym_at(ft_pc[k]);
      ui_putf(6 + k, 1, false, "%04X %-10.10s %.40s", ft_pc[k], lab ? lab : "", t);
    }
    pos = 60;                                 // (sin desensamblado)
  } else if (ui_hist) {                       // la traza lenta: las ultimas, la mas reciente abajo
    int m = trace_n < UI_DIS_N ? trace_n : UI_DIS_N;
    for (int k = 0; k < m; k++) {
      const TraceE& e = trace_at(trace_n - m + k);
      char t[96];
      ui_disasm(e.pc, (uint8_t*)e.op, 4, t, sizeof(t));
      sym_subst(t, sizeof(t));
      ui_putf(6 + k, 1, false, "%04X  %-24.24s AF %04X HL %04X SP %04X", e.pc, t, e.af, e.hl, e.sp);
    }
    pos = 60;                                 // (sin desensamblado)
  }
  for (int k = 0; k < UI_DIS_N && pos < 60; k++) {
    uint16_t a = ui_dis + pos;
    char t[96];
    int used = ui_disasm(a, ui_dis_b + pos, 64 - pos, t, sizeof(t));
    sym_subst(t, sizeof(t));
    char hx[9] = "";
    for (int j = 0; j < used && j < 4; j++) snprintf(hx + 2 * j, 3, "%02X", ui_dis_b[pos + j]);
    const char* lab = sym_at(a);
    ui_putf(6 + k, 1, false, "%c%04X %-10.10s %-8s %.30s", bp_find(a) >= 0 ? '*' : ' ', a, lab ? lab : "", hx, t);
    if (a == pc) ui_invert(6 + k, 0, 57);
    pos += used;
  }
  for (int k = 0; k < UI_STK_N; k++)
    ui_putf(6 + k, 59, false, "%04X  %04X", (uint16_t)(sp + 2 * k), ui_stk_b[2 * k] | (ui_stk_b[2 * k + 1] << 8));
  // 18-21: volcado (los caracteres, en los del ZX81; los codigos 64-127, un punto)
  ui_invert(18, 0, UI_COLS);
  ui_putf(18, 1, true, "MEMORY %04X", ui_mem);
  for (int k = 0; k < UI_MEM_N; k++) {
    uint16_t a = ui_mem + 16 * k;
    char t[UI_COLS + 1];
    int p = snprintf(t, sizeof(t), "%04X ", a);
    for (int j = 0; j < 16; j++) p += snprintf(t + p, sizeof(t) - p, " %02X", ui_mem_b[16 * k + j]);
    ui_put(19 + k, 1, t);
    uint8_t* d = ui_row(19 + k) + 1 + p + 2;
    for (int j = 0; j < 16; j++) { uint8_t c = ui_mem_b[16 * k + j]; d[j] = (c & 0x40) ? 27 : c; }
  }
  // 22-23: las teclas
  ui_invert(22, 0, UI_COLS);
  ui_put(22, 1, "S STEP O OVER U OUT C/SPACE CONT G GO TO B BREAKPOINT W WATCH R REG T TRACE", true);
  ui_invert(23, 0, UI_COLS);
  ui_put(23, 1, "H HISTORY D DISASM M MEMORY 5-8 SCROLL E POKE V PROGRAM Z SNAP L LOAD Q HIDE", true);
}

// Manda lo que ha cambiado. Trozos seguidos (huecos de menos de 24 bytes se
// mandan tambien); con muchos, uno solo del primero al ultimo: la cola
static void ui_flush(){
  uint16_t st[16], en[16];
  int ns = 0, i = 0;
  bool many = false;
  while (i < UI_DF_LEN) {
    if (ui_nd[i] == ui_df[i]) { i++; continue; }
    int e = i, gap = 0;
    for (int j = i + 1; j < UI_DF_LEN && gap < 24; j++) {
      if (ui_nd[j] != ui_df[j]) { e = j; gap = 0; } else gap++;
    }
    if (ns == 16) { many = true; en[ns - 1] = e; }
    else { st[ns] = i; en[ns] = e; ns++; }
    i = e + 1;
  }
  if (!ns) return;
  if (many || ns > 8) { en[0] = en[ns - 1]; ns = 1; }
  for (int k = 0; k < ns; k++) {
    uint16_t n = en[k] - st[k] + 1;
    memcpy(ui_df + st[k], ui_nd + st[k], n);
    ui_bramw(2, UI_DF, st[k], n);
  }
}

// El video de la pantalla, detras del primer dibujo
static void ui_apply_video(){
  ui_applied = true;
  if (ui_c128) send_bit_config(cfgcmd_128CHARS, 0);
  if (ui_c256) send_bit_config(cfgcmd_256CHARS, 0);
  if (ui_chroma & 0x20) q_out(0x7FEF, ui_chroma & ~0x20);   // sin color
  q_push(OP_SETI, 0, UI_FONT_I);
  ui_seti = true;
  poke_begin();
  q_write1(2045, 174);                        // 80 columnas
  DbgReq* r = q_push(OP_WRITE, 2096, 3);      // y el D_FILE alternativo
  if (r) { r->data[0] = UI_DF & 0xFF; r->data[1] = UI_DF >> 8; r->data[2] = 170; }
  q_push(OP_IN, 0x00E7, 0, TAG_VIEW_SYNC);    // barrera: la orden 10 fuera
}

static void ui_draw(){
  if (!ui_shown || !stopped) return;          // seguir ya en la cola: al parar, otra vez
  ui_compose();
  ui_flush();
  if (ui_apply_pend) { ui_apply_pend = false; ui_apply_video(); }
}

static void ui_refresh(){
  if (!ui_shown || ui_entering || QSIZE - q_count < 8) return;
  q_push(OP_READ, ui_dis, 64, TAG_UI_DIS);
  q_push(OP_READ, reg16(R_SP), 2 * UI_STK_N, TAG_UI_STK);
  q_push(OP_READ, ui_mem, 16 * UI_MEM_N, TAG_UI_MEM);
  ui_pend += 3;
  if (ui_hist && !trace_slow_valid && ft_on && !ft_stage && QSIZE - q_count > UI_DIS_N + 12) {
    ft_fetch(UI_DIS_N, 2);                    // H: el historial de la FPGA
    ui_pend++;
  }
}

// Entrar: los POKEs y el Chroma de ahora, y la sombra que se va a pisar
static void ui_show(){
  ui_prog_view = false;
  if (ui_shown || ui_entering || !stopped) return;
  view_off();
  ui_entering = true;
  ui_abort = false;
  ui_c128 = cfg_value(cfgcmd_128CHARS);
  ui_c256 = cfg_value(cfgcmd_256CHARS);
  ui_bram_ptr(POKE_FIRST);
  q_out(DBG_PORT, 0x02);
  q_push(OP_INSEQ, DBG_PORT, POKE_N, TAG_UI_PK);
  q_out(DBG_PORT, 0x03);
  q_push(OP_IN, DBG_PORT, 0, TAG_UI_CHROMA);
  ui_bram_ptr(UI_DF);
  q_out(DBG_PORT, 0x02);
  for (int k = 0; k < UI_CHUNKS; k++) {
    uint16_t n = UI_DF_LEN - k * 256;
    q_push(OP_INSEQ, DBG_PORT, n < 256 ? n : 256, TAG_UI_SAVE, k);
  }
}

// La sombra como estaba: 2045, 2096-2098 y la de $1000
static void ui_restore_shadow(){
  ui_bram_ptr(2045);
  DbgReq* r = q_push(OP_BRAMW, 0, 1, TAG_NONE, 0xFF);
  if (r) r->data[0] = ui_pk[2045 - POKE_FIRST];
  ui_bram_ptr(2096);
  r = q_push(OP_BRAMW, 0, 3, TAG_NONE, 0xFF);
  if (r) memcpy(r->data, ui_pk + (2096 - POKE_FIRST), 3);
  ui_bramw(3, UI_DF, 0, UI_DF_LEN);
}

// Salir: el video del programa (como view_off) y la sombra
static void ui_hide(){
  if (ui_entering) { ui_abort = true; return; }
  if (!ui_shown) return;
  ui_shown = false;
  ui_pr = UP_NONE;
  ui_msg[0] = 0;
  if (ui_applied) {
    ui_applied = false;
    uint8_t rom[POKE_N];
    bool rom_ok = rom_file_read(POKE_FIRST, rom, POKE_N) == POKE_N;
    auto pk = [&](int a){ return ui_pk[a - POKE_FIRST]; };
    auto never = [&](int a){ return rom_ok && rom[a - POKE_FIRST] == pk(a); };
    poke_begin();
    DbgReq* r = q_push(OP_WRITE, 2096, 3);
    if (r) {
      r->data[0] = never(2096) ? 0 : pk(2096);
      r->data[1] = never(2097) ? 0 : pk(2097);
      r->data[2] = pk(2098) == 170 ? 170 : 85;
    }
    uint8_t m = never(2045) ? 85 : pk(2045);
    q_write1(2045, m);
    if (m == 85) {                            // 85 apaga el D_FILE y los atributos alternativos
      if (pk(2098) == 170) q_write1(2098, 170);
      if (pk(2061) == 170) q_write1(2061, 170);
    }
    q_push(OP_IN, 0x00E7, 0, TAG_VIEW_SYNC);
    if (ui_chroma & 0x20) q_out(0x7FEF, ui_chroma);
    if (ui_c128) send_bit_config(cfgcmd_128CHARS, 1);
    if (ui_c256) send_bit_config(cfgcmd_256CHARS, 1);
  }
  if (ui_seti) { q_push(OP_SETI, 0, 0); ui_seti = false; }
  ui_restore_shadow();
}

// Reset del Z80: la FPGA ya no tiene los POKEs, pero la sombra si. Se
// arregla en la siguiente parada, si sigue como la dejo la pantalla
static void ui_drop(){
  if (ui_shown && ui_applied) ui_fix = true;
  ui_shown = ui_entering = ui_applied = ui_apply_pend = ui_abort = false;
  ui_pend = 0;
  ui_pr = UP_NONE;
  ui_prog_view = false;
  ui_want = false;
}

// Seguir de verdad o cargar: fuera la pantalla y la del programa (V)
static void ui_run(){
  ui_prog_view = false;
  ui_hide();
}

static void ui_on_break(){
  ui_pend = 0;                                // la cola se ha vaciado
  ui_dis_follow = true;
  if (ui_fix) {
    ui_fix = false;
    ui_bram_ptr(POKE_FIRST);
    q_out(DBG_PORT, 0x02);
    q_push(OP_INSEQ, DBG_PORT, POKE_N, TAG_UI_FIX);
  }
}

// Al quedarse parado (despues de lo del snapshot): ensenarla o refrescarla.
// Sin pedirla, el SETI fuera (se queda en el monitor)
static void ui_on_stop(){
  if (ui_prog_view) { if (ui_prog_v && !view_is_on()) view_request(false, 0); }   // (tras un snapshot)
  else if (ui_want) { if (ui_shown) ui_refresh(); else ui_show(); }
  else if (ui_seti) { q_push(OP_SETI, 0, 0); ui_seti = false; }
}

// --- teclado: las 8 filas cada 40 ms; cuenta al soltar ---
static void ui_kbd_feed(){
  if ((!ui_shown && !ui_prog_view) || !stopped || trace_on || ui_busy() || view_busy() || snap_busy() || ld_busy() || q_count) return;
  if (millis() - ui_kt < 40) return;
  ui_kt = millis();
  for (int r = 0; r < 8; r++) q_push(OP_IN, ((0xFF ^ (1 << r)) << 8) | 0xFE, 0, TAG_UI_KBD, r);
}

// Una respuesta completa (ENTER, o todas las cifras)
static void ui_accept(){
  uint8_t pr = ui_pr;
  uint16_t v = ui_pr_v;
  bool got = ui_pr_n > 0;
  ui_pr = UP_NONE;
  switch (pr) {
    case UP_GO:
      if (got) run_to(v, "Run to");
      break;
    case UP_BP: {
      if (!got) v = reg16(R_PC);
      int i = bp_find(v);
      if (i >= 0) { bps[i].used = false; snprintf(ui_msg, sizeof(ui_msg), "BREAKPOINT CLEARED AT %04X", v); break; }
      i = 0;
      while (i < NBP && bps[i].used) i++;
      if (i == NBP) { snprintf(ui_msg, sizeof(ui_msg), "NO ROOM FOR MORE BREAKPOINTS"); break; }
      bps[i].used = true; bps[i].addr = v; bps[i].state = 0; bps[i].temp = false;
      snprintf(ui_msg, sizeof(ui_msg), "BREAKPOINT SET AT %04X", v);
      break;
    }
    case UP_DIS:
      ui_dis = got ? v : reg16(R_PC);
      ui_dis_follow = !got;
      break;
    case UP_MEM:
      ui_mem = got ? v : reg16(R_HL);
      break;
    case UP_POKEADDR:
      if (got) { ui_poke_a = v; ui_pr = UP_POKE; ui_pr_n = 0; ui_pr_v = 0; }
      break;
    case UP_WADDR:
      if (!got) break;
      watch_mode = ui_wmode; watch_addr = v;
      queue_cmp(v, ui_wmode);
      snprintf(ui_msg, sizeof(ui_msg), "WATCHPOINT: %s %04X", watch_name(), v);
      break;
    case UP_TRACE:
      ui_hist = true;
      trace_start(got ? v : 0);
      break;
    case UP_REGVAL:
      if (!got) break;
      if (ui_reg == R_I) regs[R_I] = v; else set_reg16(ui_reg, v);
      if (ui_reg == R_PC) { stop_foreign = false; ui_dis_follow = true; }
      q_setregs();
      break;
  }
}

static void ui_ask(uint8_t pr){ ui_pr = pr; ui_pr_n = 0; ui_pr_v = 0; }

static void ui_prompt_key(char c){
  int h = (c >= '0' && c <= '9') ? c - '0' : (c >= 'A' && c <= 'F') ? c - 'A' + 10 : -1;
  switch (ui_pr) {
    case UP_WMODE:
      if (c == 'R' || c == 'W' || c == 'I') { ui_wmode = c == 'R' ? 2 : c == 'W' ? 3 : 4; ui_ask(UP_WADDR); }
      else if (c == '\n') {
        ui_pr = UP_NONE;
        watch_mode = 0;
        queue_cmp(0, 0);
        snprintf(ui_msg, sizeof(ui_msg), "WATCHPOINT CLEARED");
      } else ui_pr = UP_NONE;
      return;
    case UP_REG: {
      static const struct { char k; uint8_t idx; } rk[] = {{'P', R_PC}, {'S', R_SP}, {'A', R_AF}, {'B', R_BC},
        {'D', R_DE}, {'H', R_HL}, {'X', R_IX}, {'Y', R_IY}, {'I', R_I}};
      ui_pr = UP_NONE;
      for (auto& r : rk) if (r.k == c) { ui_reg = r.idx; ui_ask(UP_REGVAL); }
      return;
    }
    case UP_POKE:
      if (h >= 0) {
        ui_pr_v = (ui_pr_v << 4) | h;
        if (++ui_pr_n == 2) { q_write1(ui_poke_a, ui_pr_v); ui_poke_a++; ui_pr_n = 0; ui_pr_v = 0; }
      } else ui_pr = UP_NONE;                 // ENTER u otra tecla: se acabo
      return;
    default: {
      int w = (ui_pr == UP_REGVAL && ui_reg == R_I) ? 2 : 4;
      if (h >= 0) {
        ui_pr_v = (ui_pr_v << 4) | h;
        if (++ui_pr_n == w) ui_accept();
      } else if (c == '\n') ui_accept();
      else ui_pr = UP_NONE;
      return;
    }
  }
}

// L: el ultimo snapshot grabado en la sesion. Si no se puede, el porque en
// ui_msg (y en la consola)
static bool ui_load_last(){
  if (!last_snap[0]) snprintf(ui_msg, sizeof(ui_msg), "NO SNAPSHOT SAVED YET");
  else {
    uint8_t im;
    uint8_t st = ld_prepare(last_snap, &im);
    if (!st) {
      Serial.printf("L: %s\r\n", last_snap);
      ld.set_im = true;                       // sin la ROM: el IM va con SETREGS
      ld_begin();
      return true;
    }
    snprintf(ui_msg, sizeof(ui_msg), "CAN'T LOAD %.50s (ERROR %d)", last_snap, st);
  }
  Serial.printf("L: %s\r\n", ui_msg);
  return false;
}

// V: la pantalla del programa mientras esta parado (la suya si es
// Superfast; si no, Superfast texto de su D_FILE, como la orden v)
static void ui_prog_screen(bool sf){
  ui_hide();
  ui_prog_view = true;
  ui_prog_v = sf;
  if (sf) view_request(false, 0);
  Serial.println("Debugger screen: program screen (V to come back)");
}

// La pausa del boton QS: la pantalla del depurador (ui_on_stop la pone, y
// desde ahi sale en cada parada), asi se ve que esta parado. Las teclas que
// ya estaban pulsadas al parar no cuentan hasta soltarlas
static void ui_pause_qs(){
  ui_want = true;
  ui_prog_view = false;
  memset(ui_kprev, 0x1F, sizeof(ui_kprev));
  memset(ui_karmed, 0, sizeof(ui_karmed));
  Serial.println("QS pause: debugger screen on the ZX81 (C or SPACE continue, Z snapshot, V program screen)");
}

static void ui_key(char c){
  if (ui_prog_view) {                         // con la pantalla del programa
    if (c == 'V') { view_off(); ui_want = true; ui_show(); }   // desde aqui, en cada parada
    else if (c == 'C' || c == ' ') { Serial.println("Paused: continue"); dbg_continue(); }
    else if (c == 'S') { Serial.println("Paused: S, snapshot and continue"); dbg_snapshot(false, "", true); }
    else if (c == 'Z') { Serial.println("Paused: Z, snapshot"); dbg_snapshot(false, "", false); }
    else if (c == 'L') ui_load_last();
    return;
  }
  ui_msg[0] = 0;
  if (ui_pr) { ui_prompt_key(c); ui_refresh(); return; }
  switch (c) {
    case 'S': dbg_step(1); return;
    case 'O': q_push(OP_READ, reg16(R_PC), 4, TAG_STEPOVER); return;
    case 'U': q_push(OP_READ, reg16(R_SP), 2, TAG_STEPOUT); return;
    case 'C': case ' ': Serial.println("Debugger screen: continue"); dbg_continue(); return;
    case 'Z': Serial.println("Debugger screen: snapshot"); dbg_snapshot(false, "", false); return;
    case 'Q': ui_prog_screen(false); return;
    case 'V': ui_prog_screen(true); return;
    case 'L': if (ui_load_last()) return; break;
    case 'G': ui_ask(UP_GO); break;
    case 'B': ui_ask(UP_BP); break;
    case 'D': ui_ask(UP_DIS); break;
    case 'M': ui_ask(UP_MEM); break;
    case 'E': ui_ask(UP_POKEADDR); break;
    case 'W': ui_ask(UP_WMODE); break;
    case 'R': ui_ask(UP_REG); break;
    case 'T': ui_ask(UP_TRACE); break;
    case 'H': ui_hist = !ui_hist; break;
    case '5': ui_mem -= 48; break;
    case '6': ui_mem += 16; break;
    case '7': ui_mem -= 16; break;
    case '8': ui_mem += 48; break;
    default: return;
  }
  ui_refresh();
}

static void ui_kbd_result(uint8_t r, uint8_t v){
  static const char keys[8][5] = {
    {0, 'Z', 'X', 'C', 'V'}, {'A', 'S', 'D', 'F', 'G'}, {'Q', 'W', 'E', 'R', 'T'}, {'1', '2', '3', '4', '5'},
    {'0', '9', '8', '7', '6'}, {'P', 'O', 'I', 'U', 'Y'}, {'\n', 'L', 'K', 'J', 'H'}, {' ', '.', 'M', 'N', 'B'}
  };
  if (r > 7) return;
  ui_krow[r] = ~v & 0x1F;
  if (r != 7) return;
  char go = 0;
  for (int k = 0; k < 8; k++) {
    uint8_t press = ui_krow[k] & ~ui_kprev[k], release = ui_kprev[k] & ~ui_krow[k];
    ui_kprev[k] = ui_krow[k];
    ui_karmed[k] |= press;                    // pulsada con la pantalla puesta
    uint8_t g = release & ui_karmed[k];
    ui_karmed[k] &= ~release;
    for (int b = 0; b < 5 && !go; b++) if ((g & (1 << b)) && keys[k][b]) go = keys[k][b];
  }
  if (go && (ui_shown || ui_prog_view) && stopped) ui_key(go);
}

static void ui_result(uint8_t tag, uint8_t k, uint8_t* d, uint16_t n){
  switch (tag) {
    case TAG_UI_PK: if (n >= POKE_N) memcpy(ui_pk, d, POKE_N); break;
    case TAG_UI_CHROMA: ui_chroma = n ? d[0] : 0; break;
    case TAG_UI_SAVE:
      if (k >= UI_CHUNKS) break;
      memcpy(ui_save + k * 256, d, n < 256 ? n : 256);
      if (k != UI_CHUNKS - 1) break;
      ui_entering = false;
      if (ui_abort) { ui_abort = false; break; }
      ui_shown = true;
      memset(ui_df, 0xFF, UI_DF_LEN);         // nada mandado: el primer dibujo, entero
      memset(ui_karmed, 0, sizeof(ui_karmed));
      memset(ui_kprev, 0x1F, sizeof(ui_kprev));   // las ya pulsadas no cuentan hasta soltarlas
      if (!ui_mem_set) { ui_mem = reg16(R_HL) & 0xFFF0; ui_mem_set = true; }
      ui_dis = reg16(R_PC);
      ui_apply_pend = true;
      ui_refresh();
      break;
    case TAG_UI_DIS: case TAG_UI_STK: case TAG_UI_MEM:
      if (tag == TAG_UI_DIS) {
        memset(ui_dis_b, 0, sizeof(ui_dis_b));
        memcpy(ui_dis_b, d, n < 64 ? n : 64);
        // el PC tiene que verse, con un par de lineas detras: si no, desde el PC
        uint16_t pc = reg16(R_PC);
        int pos = 0;
        bool seen = false;
        char t[48];
        for (int j = 0; j < UI_DIS_N - 3 && pos < 60; j++) {
          if ((uint16_t)(ui_dis + pos) == pc) { seen = true; break; }
          pos += ui_disasm(ui_dis + pos, ui_dis_b + pos, 64 - pos, t, sizeof(t));
        }
        if (!seen && ui_dis != pc && ui_dis_follow) {
          ui_dis = pc;
          if (q_push(OP_READ, ui_dis, 64, TAG_UI_DIS)) ui_pend++;
        }
      }
      else if (tag == TAG_UI_STK) memcpy(ui_stk_b, d, n < sizeof(ui_stk_b) ? n : sizeof(ui_stk_b));
      else memcpy(ui_mem_b, d, n < sizeof(ui_mem_b) ? n : sizeof(ui_mem_b));
      if (ui_pend && --ui_pend == 0) ui_draw();
      break;
    case TAG_UI_KBD: ui_kbd_result(k, n ? d[0] : 0xFF); break;
    case TAG_UI_FIX:                          // sigue como la dejo la pantalla? entonces arreglarla
      if (n >= POKE_N && d[2045 - POKE_FIRST] == 174 && d[2098 - POKE_FIRST] == 170 &&
          d[2096 - POKE_FIRST] == (UI_DF & 0xFF) && d[2097 - POKE_FIRST] == (UI_DF >> 8))
        ui_restore_shadow();
      break;
  }
}

// ---------------------------------------------------------------------
// Traza lenta (sin FPGA). El MCU da los pasos de uno en uno (la FPGA para
// tras cada instruccion) y apunta el estado antes de cada una: PC, los
// registros y sus 4 bytes (leidos en ese momento). Se para al llegar a n
// instrucciones, en un breakpoint (el PC en uno, sin ejecutarlo: durante la
// traza los FF no estan en memoria), con un punto de vigilancia u otra
// ruptura que no sea un paso, con ESPACIO en el ZX81 (se mira cada 100 ms),
// con cualquier orden en la consola o con el boton QS. Un RST 38h del
// programa se ejecuta como siempre. Las ultimas TRACE_MAX quedan en un
// anillo: th en la consola, H en la pantalla. Cada t empieza una nueva.
// Velocidad: la de un paso y una lectura por instruccion (unos cientos por
// segundo)
// ---------------------------------------------------------------------
static uint32_t trace_left = 0;               // por ejecutar (0xFFFFFFFF: sin limite)
static bool trace_abort = false;
static uint32_t trace_kt = 0, trace_t0 = 0;
static const char* trace_why = "";

static const TraceE& trace_at(uint32_t i){ return trace_buf[i % TRACE_MAX]; }

static void trace_read(){
  q_push(OP_READ, reg16(R_PC), 4, TAG_TRACE);
  if (millis() - trace_kt >= 100) {           // ESPACIO para pararla
    trace_kt = millis();
    q_push(OP_IN, 0x7FFE, 0, TAG_TRACE_KEY);
  }
}

static void trace_start(uint32_t n){
  if (!stopped) return;
  trace_on = true;
  trace_abort = false;
  trace_n = trace_total = 0;
  trace_left = n ? n : 0xFFFFFFFF;
  trace_t0 = trace_kt = millis();
  if (n) Serial.printf("Trace: %lu instructions (SPACE on the ZX81, or any command here, stops it)\r\n", (unsigned long)n);
  else Serial.println("Trace: until a breakpoint (SPACE on the ZX81, or any command here, stops it)");
  ui_refresh();
  trace_read();                               // la primera, la del PC de ahora
}

static void trace_stop_req(){ trace_abort = true; }

// Acabada: lo normal de una parada
static void trace_end(const char* why){
  trace_on = false;
  trace_slow_valid = true;
  uint32_t ms = millis() - trace_t0;
  Serial.printf("Trace: %lu instructions in %lu ms (%s); th to see them\r\n", (unsigned long)trace_total,
                (unsigned long)ms, why);
  on_stopped(false);
}

// En cada parada durante la traza. true: sigue (ya pedida la instruccion)
static bool trace_break(){
  uint8_t why = regs[R_DBGST] & 7;
  if (why != 4 && !stop_foreign) trace_why = reason_name(regs[R_DBGST]);   // vigilancia, pausa, trampa...
  else if (trace_abort) trace_why = "stopped";
  else if (!trace_left) trace_why = "count reached";
  else if (bp_find(reg16(R_PC)) >= 0) trace_why = "breakpoint";
  else { trace_read(); return true; }
  trace_end(trace_why);
  return true;                                // (trace_end ya ha hecho lo de parar)
}

static void trace_result(uint8_t tag, uint8_t* d, uint16_t n){
  if (!trace_on) return;
  if (tag == TAG_TRACE_KEY) { if (n && !(d[0] & 1)) trace_abort = true; return; }
  if (trace_abort) { trace_end("stopped"); return; }
  TraceE& e = trace_buf[trace_n % TRACE_MAX];
  e.pc = reg16(R_PC); e.af = reg16(R_AF); e.bc = reg16(R_BC); e.de = reg16(R_DE);
  e.hl = reg16(R_HL); e.ix = reg16(R_IX); e.iy = reg16(R_IY); e.sp = reg16(R_SP);
  memset(e.op, 0, 4);
  memcpy(e.op, d, n < 4 ? n : 4);
  trace_n++;
  trace_total++;
  if (trace_left != 0xFFFFFFFF) trace_left--;
  dbg_step(1);
}

static void trace_print(uint32_t n){
  if (!trace_n) { Serial.println("No trace (t [n])"); return; }
  uint32_t have = trace_n < TRACE_MAX ? trace_n : TRACE_MAX;
  if (n > have) n = have;
  for (uint32_t k = trace_n - n; k < trace_n; k++) {
    const TraceE& e = trace_at(k);
    char t[96];
    ui_disasm(e.pc, (uint8_t*)e.op, 4, t, sizeof(t));
    sym_subst(t, sizeof(t));
    const char* lab = sym_at(e.pc);
    if (lab) Serial.printf("%s:\r\n", lab);
    Serial.printf("%04X  %-22s AF=%04X BC=%04X DE=%04X HL=%04X IX=%04X IY=%04X SP=%04X\r\n",
                  e.pc, t, e.af, e.bc, e.de, e.hl, e.ix, e.iy, e.sp);
  }
  Serial.printf("(%lu of %lu traced; the last one ran just before the stop)\r\n", (unsigned long)n, (unsigned long)trace_total);
}

// ---------------------------------------------------------------------
// Historial: la traza de la FPGA (sim_int 0.13, orden 12). Con ella puesta
// (tron, por defecto), cada instruccion del programa apunta su PC en la
// sombra, en un anillo de 1024 entradas de 2 bytes en $1000-$17FF; el
// puntero (la entrada siguiente) se lee con los indices 7/8 de $3FEF. Al
// pedirlo (th, H) se leen solo las ultimas n entradas y, de la memoria, los
// bytes de cada instruccion para desensamblarla (los de ahora: con codigo
// que se reescribe pueden no ser los que se ejecutaron). No hay registros:
// para eso, la traza lenta (t)
// ---------------------------------------------------------------------
#define FT_BASE 0x1000
#define FT_RING 1024
static uint8_t ft_lo = 0;
static uint8_t ft_ring[2 * FT_SHOW];
static uint16_t ft_got = 0, ft_pend = 0;

static void ft_fetch(uint16_t n, uint8_t purpose){
  if (!stopped || ft_stage || !n) return;
  ft_stage = 1;
  ft_purpose = purpose;
  ft_n = n;
  q_out(DBG_PORT, 0x07); q_push(OP_IN, DBG_PORT, 0, TAG_FT_PTR, 0);
  q_out(DBG_PORT, 0x08); q_push(OP_IN, DBG_PORT, 0, TAG_FT_PTR, 1);
}

static void ft_print(){
  for (uint16_t k = 0; k < ft_n; k++) {
    char t[96];
    ui_disasm(ft_pc[k], ft_op[k], 4, t, sizeof(t));
    sym_subst(t, sizeof(t));
    const char* lab = sym_at(ft_pc[k]);
    if (lab) Serial.printf("%s:\r\n", lab);
    Serial.printf("%04X  %s\r\n", ft_pc[k], t);
  }
  Serial.printf("(the last %d instructions the program ran, from the FPGA's history; the last one, just before the stop)\r\n", ft_n);
}

static void ft_done(){
  ft_stage = 0;
  if (ft_purpose == 1) ft_print();
  else if (ui_pend && --ui_pend == 0) ui_draw();
}

static void ft_result(uint8_t tag, uint8_t k, uint8_t* d, uint16_t n){
  if (!ft_stage) return;
  if (tag == TAG_FT_PTR) {
    if (k == 0) { ft_lo = n ? d[0] : 0; return; }
    uint16_t ptr = (((n ? d[0] : 0) & 3) << 8) | ft_lo;
    uint16_t first = (ptr + FT_RING - ft_n) % FT_RING;   // la mas antigua de las que se piden
    uint16_t n1 = (first + ft_n <= FT_RING) ? ft_n : FT_RING - first;
    ft_stage = 2;
    ft_got = 0;
    q_out(DBG_PORT, 0x85); q_out(DBG_PORT, (FT_BASE + 2 * first) & 0xFF);
    q_out(DBG_PORT, 0x86); q_out(DBG_PORT, (FT_BASE + 2 * first) >> 8);
    q_out(DBG_PORT, 0x02);
    q_push(OP_INSEQ, DBG_PORT, 2 * n1, TAG_FT_RING);
    if (n1 < ft_n) {                          // da la vuelta
      q_out(DBG_PORT, 0x85); q_out(DBG_PORT, FT_BASE & 0xFF);
      q_out(DBG_PORT, 0x86); q_out(DBG_PORT, FT_BASE >> 8);
      q_push(OP_INSEQ, DBG_PORT, 2 * (ft_n - n1), TAG_FT_RING);
    }
    return;
  }
  if (tag == TAG_FT_RING) {
    for (uint16_t i = 0; i < n && ft_got < 2 * ft_n; i++) ft_ring[ft_got++] = d[i];
    if (ft_got < 2 * ft_n) return;
    ft_stage = 3;
    ft_pend = ft_n;
    for (uint16_t i = 0; i < ft_n; i++) {
      ft_pc[i] = ft_ring[2 * i] | (ft_ring[2 * i + 1] << 8);
      q_push(OP_READ, ft_pc[i], 4, TAG_FT_OP, i);
    }
    return;
  }
  if (k < FT_SHOW) { memset(ft_op[k], 0, 4); memcpy(ft_op[k], d, n < 4 ? n : 4); }
  if (ft_pend && --ft_pend == 0) ft_done();
}

// ---------------------------------------------------------------------
// La vista estructurada de la web: un documento de texto, de lineas, que
// el MCU compone en cada parada (y al seguir) mientras alguien la mira
// (un CMD_DBG en los ultimos 3 s), y que la pagina pide (CMD_DBG_VIEW)
// cuando cambia su version:
//   S estado(hex) parado(0/1) motivo|donde
//   R PC SP AF BC DE HL IX IY AF' BC' DE' HL' I R IFF PAGINA   (hex)
//   D marcas direccion|etiqueta|bytes|instruccion   (marcas: > PC, * breakpoint)
//   K direccion valor                               (la pila)
//   M direccion 16 bytes                            (el volcado)
//   B direcciones de los breakpoints
//   W modo direccion                                (punto de vigilancia)
//   F seguir(0/1) direccion-del-desensamblado direccion-del-volcado
// Ordenes de la pagina para la vista (sin eco): @d dir (desensamblar desde
// ahi), @d (seguir al PC), @m dir (volcado)
// ---------------------------------------------------------------------
#define WEB_DIS_N   20
#define WEB_MEM_N   8
static uint32_t web_seen = 0;
static uint16_t web_ver = 1;
static char web_doc[2400];
static uint16_t web_doc_len = 0;
static bool web_follow = true;
static uint16_t web_dis = 0, web_mem = 0x4000;   // lo que se pide
static uint16_t web_dis_rd = 0, web_mem_rd = 0x4000;   // lo que se ha leido (con eso se compone)
static uint8_t web_dis_b[96], web_stk_b[24], web_mem_b[16 * WEB_MEM_N];

static bool web_active(){ return web_seen && millis() - web_seen < 3000; }

static void web_add(const char* f, ...){
  if (web_doc_len >= sizeof(web_doc) - 1) return;
  va_list a;
  va_start(a, f);
  int n = vsnprintf(web_doc + web_doc_len, sizeof(web_doc) - web_doc_len, f, a);
  va_end(a);
  if (n > 0) web_doc_len = ((size_t)web_doc_len + n < sizeof(web_doc)) ? web_doc_len + n : sizeof(web_doc) - 1;
}

static void web_build(){
  web_doc_len = 0;
  web_doc[0] = 0;
  char where[40] = "";
  if (stopped) sym_near(reg16(R_PC), where, sizeof(where));
  web_add("S %02X %d %s|%s\n", dbg_web_state(), stopped ? 1 : 0, stopped ? reason_name(regs[R_DBGST]) : "running", where);
  web_add("R %04X %04X %04X %04X %04X %04X %04X %04X %04X %04X %04X %04X %02X %02X %d %02X%s\n",
          reg16(R_PC), reg16(R_SP), reg16(R_AF), reg16(R_BC), reg16(R_DE), reg16(R_HL), reg16(R_IX), reg16(R_IY),
          reg16(R_AF_), reg16(R_BC_), reg16(R_DE_), reg16(R_HL_), regs[R_I], regs[R_R], regs[R_IFF], regs[R_PAGE1],
          prog_slow ? " SLOW" : "");
  web_add("F %d %04X %04X\n", web_follow ? 1 : 0, web_dis_rd, web_mem_rd);
  web_add("B");
  for (int i = 0; i < NBP; i++) if (bps[i].used && !bps[i].temp) web_add(" %04X", bps[i].addr);
  web_add("\n");
  if (watch_mode) web_add("W %s %04X\n", watch_name(), watch_addr);
  if (stopped) {
    int pos = 0;
    for (int k = 0; k < WEB_DIS_N && pos < 88; k++) {
      uint16_t a = web_dis_rd + pos;
      char t[96];
      int used = ui_disasm(a, web_dis_b + pos, 96 - pos, t, sizeof(t));
      sym_subst(t, sizeof(t));
      char hx[9] = "";
      for (int j = 0; j < used && j < 4; j++) snprintf(hx + 2 * j, 3, "%02X", web_dis_b[pos + j]);
      const char* lab = sym_at(a);
      web_add("D %c%c%04X|%s|%s|%s\n", a == reg16(R_PC) ? '>' : ' ', bp_find(a) >= 0 ? '*' : ' ', a, lab ? lab : "", hx, t);
      pos += used;
    }
    for (int k = 0; k < 12; k++)
      web_add("K %04X %04X\n", (uint16_t)(reg16(R_SP) + 2 * k), web_stk_b[2 * k] | (web_stk_b[2 * k + 1] << 8));
    for (int k = 0; k < WEB_MEM_N; k++) {
      web_add("M %04X", (uint16_t)(web_mem_rd + 16 * k));
      for (int j = 0; j < 16; j++) web_add(" %02X", web_mem_b[16 * k + j]);
      web_add("\n");
    }
  }
  if (++web_ver == 0) web_ver = 1;
}

// Lo que haga falta para la vista: parado, leer la memoria (al llegar, se
// compone); en marcha, solo el estado
static void web_refresh(){
  if (!web_active()) return;
  if (!stopped) { web_build(); return; }
  if (web_pend) { web_again = true; return; }   // al acabar la de ahora
  if (QSIZE - q_count < 8) return;
  uint16_t pc = reg16(R_PC);
  if (web_follow && (pc < web_dis || pc >= web_dis + 48)) web_dis = pc;   // el PC, a la vista
  web_dis_rd = web_dis;
  web_mem_rd = web_mem;
  q_push(OP_READ, web_dis_rd, 96, TAG_WEB_DIS);
  q_push(OP_READ, reg16(R_SP), 24, TAG_WEB_STK);
  q_push(OP_READ, web_mem_rd, 16 * WEB_MEM_N, TAG_WEB_MEM);
  web_pend = 3;
}

static void web_result(uint8_t tag, uint8_t* d, uint16_t n){
  uint8_t* b = tag == TAG_WEB_DIS ? web_dis_b : tag == TAG_WEB_STK ? web_stk_b : web_mem_b;
  size_t sz = tag == TAG_WEB_DIS ? sizeof(web_dis_b) : tag == TAG_WEB_STK ? sizeof(web_stk_b) : sizeof(web_mem_b);
  memset(b, 0, sz);
  memcpy(b, d, n < sz ? n : sz);
  if (web_pend && --web_pend == 0) {
    web_build();
    if (web_again) { web_again = false; web_refresh(); }
  }
}

static void web_view_cmd(const char* l){
  uint32_t a;
  const char* p = l + 1;
  if (l[0] == 'd') {
    if (parse_addr(p, a)) { web_dis = a; web_follow = false; }
    else web_follow = true;
  } else if (l[0] == 'm' && parse_addr(p, a)) web_mem = a;
  web_refresh();
}

// Un trozo de la vista: part * 240 (CMD_DBG_VIEW). *ver, su version; *total,
// su largo
uint16_t dbg_view_read(uint8_t part, uint8_t* buf, uint16_t max, uint16_t* ver, uint16_t* total){
  web_seen = millis();
  *ver = web_ver;
  *total = web_doc_len;
  uint32_t off = (uint32_t)part * max;
  if (off >= web_doc_len) return 0;
  uint16_t n = web_doc_len - off < max ? web_doc_len - off : max;
  memcpy(buf, web_doc + off, n);
  return n;
}

uint16_t dbg_view_ver(void){ return web_ver; }

// ---------------------------------------------------------------------
// Interfaz web (ESP32): CMD_DBG trae una orden de la consola (con un numero:
// un reintento del ESP32 no la ejecuta dos veces) y se lleva lo que se ha
// escrito desde donde iba y el estado
// ---------------------------------------------------------------------
static uint8_t web_last_id = 0;
static void web_view_cmd(const char* l);

void dbg_web_exec(uint8_t id, const char* line){
  if (!id || id == web_last_id || !line[0]) return;
  web_last_id = id;
  char l[128];
  size_t n = 0;
  while (line[n] && n < sizeof(l) - 1) { l[n] = line[n]; n++; }
  l[n] = 0;
  if (l[0] == '@') { web_view_cmd(l + 1); return; }   // la vista de la web, sin eco
  for (size_t i = 0; i < n && l[i] != ' '; i++) l[i] = tolower((uint8_t)l[i]);   // la orden, en minusculas
  out_put("> "); out_put(l); out_put("\r\n");
  if (!dbg_console(l)) Serial.println("? (h for help)");
}

// Hasta max bytes desde seq (si ya no estan, desde el mas antiguo que haya);
// *from, donde empiezan
uint16_t dbg_out_read(uint32_t seq, uint8_t* buf, uint16_t max, uint32_t* from){
  if (!web_active()) web_wake = true;         // empieza a mirar: la vista, en cuanto se pueda
  web_seen = millis();                        // hay alguien mirando la web
  uint32_t head = out_head;
  if (seq > head) seq = head;                 // (el STM32 se ha reiniciado)
  if (head - seq > OUT_RING) seq = head - OUT_RING;
  uint16_t n = 0;
  while (seq + n < head && n < max) { buf[n] = out_ring[(seq + n) % OUT_RING]; n++; }
  *from = seq;
  return n;
}

// bit 0 monitor cargado, 1 parado, 2 pantalla del depurador, 3 historial
// (tron), 4 traza lenta en curso, 5 snapshot o carga en curso
uint8_t dbg_web_state(void){
  return (debug_monitor_loaded ? 1 : 0) | (stopped ? 2 : 0) | (ui_is_shown() ? 4 : 0) | (ft_on ? 8 : 0) |
         (trace_on ? 16 : 0) | ((snap_busy() || ld_busy()) ? 32 : 0);
}
