// logic.cpp -- banco de pruebas en el PC de DEBUGGER.cpp: un Z80 de mentira que habla el
// protocolo DBG_BREAK / DBG_POLL byte a byte y ejecuta un programa de NOPs
// (instrucciones de un byte), con breakpoints FF y pasos como la FPGA.
#include "Arduino.h"
#include "DEBUGGER.h"
#include "z80-disassembler.h"
#include "COMMS.h"
#include <deque>
#include <vector>
#include <string>
#include <cstdarg>
#include <cassert>

SerialC Serial;
void SerialC::println(const char* s){ puts(s); }
void SerialC::print(const char* s){ fputs(s, stdout); }
int SerialC::printf(const char* f, ...){ va_list a; va_start(a, f); int r = vprintf(f, a); va_end(a); return r; }
static std::string out_log;
unsigned long millis(){ return 0; }
int digitalRead(int){ return 1; }
uint8_t nQS_en = 1;
bool debug_monitor_loaded = true;
void set_status_LED(uint32_t){}
void set_status_led_ok(){}
void send_bit_config(uint8_t, uint8_t){}
uint8_t cfg_value(uint8_t){ return 0; }
void set_blinking(uint32_t, float){}
void set_blinking_off(){}
char current_dir[100] = "/";
bool z81in_open(const char*){ return false; }
int z81in_read(void){ return -1; }
bool z81in_seek(uint32_t){ return false; }
uint32_t z81in_pos(void){ return 0; }
void z81in_close(void){}
uint8_t opendir_list(const char*){ return 0; }
void mcustate_save(void (*)(const char*)){}
void mcustate_clear(void){}
bool mcustate_key(const char*, uint8_t (*)(char*, uint8_t)){ return false; }
void mcustate_apply(void){}
void mcustate_quiet(void){}
bool snapfile_open(const char*){ return false; }
bool snapfile_write(const void*, uint16_t){ return false; }
void snapfile_close(void){}
bool snapfile_exists(const char*){ return false; }
int32_t rom_file_read(uint32_t, uint8_t*, uint16_t){ return -1; }
uint8_t Z80Disassembler::disassemble(char* buf, byte* op, int){ sprintf(buf, "db %02X", op[0]); return 1; }

static std::deque<uint8_t> inbox;     // lo que manda el Z80
static std::vector<uint8_t> outbox;   // lo que manda el MCU
uint8_t GetByteFromZ80_IT(){ assert(!inbox.empty()); uint8_t b = inbox.front(); inbox.pop_front(); return b; }
void SendByteToZ80(uint8_t b){ outbox.push_back(b); }
void ToggleClock(){}
void reset_commands(){}

// --- el Z80 y el programa ---
static uint8_t mem[65536];
static uint8_t regs[30];
static uint16_t step_n = 0; static bool step_on = false;
static uint8_t wreg = 0xFF;
static int errors = 0;
#define CHECK(c, msg) do { if (!(c)) { printf("ERROR %s\n", msg); errors++; } else printf("ok    %s\n", msg); } while (0)

static void setpc(uint16_t v){ regs[22] = v; regs[23] = v >> 8; }
static uint16_t pc(){ return regs[22] | regs[23] << 8; }
static uint16_t sp(){ return regs[20] | regs[21] << 8; }

static void do_break(uint8_t reason){
  regs[27] = 0x80 | reason;
  inbox.clear();
  for (int i = 0; i < 30; i++) inbox.push_back(regs[i]);
  cmd_dbg_break();
}

// Un ciclo de POLL: manda el resultado y recoge la peticion. Devuelve el op
// (0 = el MCU no tiene peticion todavia).
static std::vector<uint8_t> result;
static bool waiting = false;
static int poll_once(){
  if (!waiting) {
    inbox.clear();
    inbox.push_back(result.size() & 0xFF);
    inbox.push_back(result.size() >> 8);
    for (auto b : result) inbox.push_back(b);
    result.clear();
    waiting = true;
  }
  outbox.clear();
  cmd_dbg_poll();
  if (outbox.empty()) return 0;
  waiting = false;
  uint8_t op = outbox[0];
  uint16_t addr = outbox[1] | outbox[2] << 8, n = outbox[3] | outbox[4] << 8;
  switch (op) {
    case 1: for (int i = 0; i < n; i++) result.push_back(mem[(uint16_t)(addr + i)]); break;
    case 2: assert(outbox.size() == 5u + n); for (int i = 0; i < n; i++) mem[(uint16_t)(addr + i)] = outbox[5 + i]; break;
    case 3: assert(n == 30 && outbox.size() == 35); memcpy(regs, &outbox[5], 30); break;
    case 4:
      if (addr == 0x3FEF) {
        uint8_t v = n & 0xFF;
        if (wreg != 0xFF) {
          if (wreg == 3) step_n = (step_n & 0xFF00) | v;
          if (wreg == 4) { step_n = (step_n & 0xFF) | (v << 8); step_on = step_n != 0; }
          wreg = 0xFF;
        } else if (v & 0x80) wreg = v & 7;
      }
      break;
    case 5: result.push_back(0x5A); break;
  }
  return op;
}

// Atiende peticiones hasta CONT (true) o hasta que el MCU espera (false)
static bool serve(){
  for (int k = 0; k < 200; k++) {
    int op = poll_once();
    if (op == 0) return false;
    if (op == 6) return true;
  }
  assert(0);
  return false;
}

// Ejecuta el programa desde PC como hace la FPGA: rompe en un FF de memoria
// (motivo 7) o al agotar los pasos (motivo 4). Devuelve el motivo.
static int run(int max = 100){
  bool first = true;          // "skip": la primera no rompe por pasos... (los pasos si cuentan)
  (void)first;
  for (int k = 0; k < max; k++) {
    uint16_t p = pc();
    if (mem[p] == 0xFF) { do_break(7); return 7; }
    if (step_on) {
      if (step_n == 0) { step_on = false; do_break(4); return 4; }
      step_n--;
    }
    setpc(p + 1);             // NOP
  }
  return 0;
}

static void console(const char* l){ printf("> %s\n", l); dbg_console(l); }

int main(){
  memset(mem, 0, sizeof(mem));
  setpc(0x6000); regs[20] = 0x00; regs[21] = 0x7F;   // SP = $7F00

  // 1. pausa en $6000
  do_break(1);
  CHECK(!serve(), "pausa: el MCU ensena y espera");
  CHECK(dbg_is_stopped(), "parado");

  // 2. breakpoint en $6005 y continuar
  console("b 6005");
  console("c");
  CHECK(serve(), "c: peticiones hasta CONT");
  CHECK(mem[0x6005] == 0xFF, "el FF esta puesto");
  CHECK(run() == 7 && pc() == 0x6005, "rompe en el breakpoint");
  CHECK(!serve(), "parado en el breakpoint");
  CHECK(mem[0x6005] == 0x00, "al parar, el byte original repuesto");

  // 3. continuar desde el breakpoint: paso sin el y vuelta a ponerlo
  console("b 6008");
  console("c");
  CHECK(serve(), "c desde el breakpoint");
  CHECK(mem[0x6005] == 0x00 && mem[0x6008] == 0xFF, "en el paso: el del PC quitado, el otro puesto");
  CHECK(run() == 4 && pc() == 0x6006, "el paso para en $6006");
  CHECK(serve(), "en silencio: pone el de $6005 y sigue");
  CHECK(mem[0x6005] == 0xFF, "el de $6005 vuelve a estar puesto");
  CHECK(run() == 7 && pc() == 0x6008, "rompe en el segundo breakpoint");
  CHECK(!serve(), "parado");
  CHECK(mem[0x6005] == 0x00 && mem[0x6008] == 0x00, "al parar, los dos quitados");

  // 4. pasos
  console("s 3");
  CHECK(serve(), "s 3");
  CHECK(mem[0x6005] == 0x00, "los pasos no ponen breakpoints");
  CHECK(run() == 4 && pc() == 0x600B, "3 pasos: $600B");
  CHECK(!serve(), "parado tras los pasos");

  // 5. RST 38h que no es un breakpoint
  console("bc");
  mem[0x6010] = 0xFF;
  console("c");
  CHECK(serve(), "c");
  CHECK(run() == 7 && pc() == 0x6010, "para en el FF ajeno");
  CHECK(!serve(), "parado");
  console("c");
  CHECK(serve(), "c: emula el RST");
  CHECK(pc() == 0x0038 && sp() == 0x7EFE, "PC = $0038, SP - 2");
  CHECK(mem[0x7EFE] == 0x11 && mem[0x7EFF] == 0x60, "en la pila, $6011");

  // 6. registros
  do_break(1);
  CHECK(!serve(), "pausa");
  console("x hl=1234");
  serve();
  CHECK(regs[12] == 0x34 && regs[13] == 0x12, "x hl=1234 llega al bloque");
  console("x pc=6020");
  serve();
  CHECK(pc() == 0x6020, "x pc=6020");

  // 7. reset con un breakpoint puesto en memoria
  console("b 6025");
  console("c");
  CHECK(serve(), "c");
  CHECK(mem[0x6025] == 0xFF, "puesto");
  dbg_reset();                       // el Z80 se resetea; el FF sigue en memoria
  setpc(0x6022);
  do_break(1);
  CHECK(!serve(), "tras el reset, pausa");
  CHECK(mem[0x6025] == 0x00, "comprobado y quitado");

  // 8. volcado, desensamblado y E/S
  console("m 6000 20");
  CHECK(!serve(), "m");
  console("d 6000 4");
  CHECK(!serve(), "d");
  console("e 6030 1 2 3");
  serve();
  CHECK(mem[0x6030] == 1 && mem[0x6031] == 2 && mem[0x6032] == 3, "e escribe");
  console("io fe");
  CHECK(!serve(), "io");

  printf(errors ? "%d ERRORES\n" : "TODO OK\n", errors);
  return errors;
}
