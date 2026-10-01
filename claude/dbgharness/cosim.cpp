// Cosimulacion del depurador: el monitor de verdad (debugmon.bin) en el
// nucleo Z80 de EightyOne, el DEBUGGER.cpp de verdad en otro hilo con el
// protocolo de los puertos $A7/$AF byte a byte, y la FPGA a nivel de
// instruccion (ruptura = RST + CALL $2000 con la ventana; salida = epilogo).
#include "Arduino.h"
#include "DEBUGGER.h"
#include "COMMS.h"
#include <atomic>
#include <thread>
#include <mutex>
#include <deque>
#include <string>
#include <vector>
#include <cstdarg>
#include <cstdio>

extern "C" {
#include "z80.h"
int tstates = 0;
int StackChange = 0, StepOutRequested = 0, RetExecuted = 0;
int z80_fetch_is_m1 = 0;
void z80_m1_backtrack(void){}
void debug(int){}
}

// --------------------------- salida de consola ---------------------------
static std::mutex out_mx;
static std::string out_log;
SerialC Serial;
static void logs(const char* s){ std::lock_guard<std::mutex> l(out_mx); out_log += s; fputs(s, stdout); }
void SerialC::println(const char* s){ logs(s); logs("\n"); }
void SerialC::print(const char* s){ logs(s); }
int SerialC::printf(const char* f, ...){ char b[512]; va_list a; va_start(a, f); int r = vsnprintf(b, sizeof b, f, a); va_end(a); logs(b); return r; }

unsigned long millis(){ return 0; }
int digitalRead(int){ return 1; }
uint8_t nQS_en = 1;
bool debug_monitor_loaded = true;
void set_status_LED(uint32_t){}
void set_status_led_ok(){}
void send_bit_config(uint8_t, uint8_t){}

// ------------------------- protocolo $A7 / $AF ---------------------------
static std::atomic<int> clk{0};
static std::atomic<int> latch{0};
static std::atomic<int> zdata{0};
static std::atomic<bool> dreg{false};
static std::atomic<bool> quit{false};
uint8_t GetByteFromZ80_IT(){ while (!dreg) std::this_thread::yield(); dreg = false; return zdata; }
void SendByteToZ80(uint8_t b){ latch = b; clk ^= 1; while (!dreg) std::this_thread::yield(); dreg = false; }
void ToggleClock(){ clk ^= 1; }
static std::atomic<int> cmd_active{-1};
void reset_commands(){ cmd_active = -1; }

static std::mutex con_mx;
static std::deque<std::string> con_lines;
static std::atomic<int> posted{0}, processed{0};

static void mcu_thread(){
  while (!quit) {
    if (cmd_active == -1 && dreg) { cmd_active = zdata.load(); dreg = false; }
    int c = cmd_active;
    if (c == CMD_DBG_BREAK) cmd_dbg_break();
    else if (c == CMD_DBG_POLL) cmd_dbg_poll();
    else if (c != -1) { printf("MCU: comando desconocido %d\n", c); cmd_active = -1; }
    std::string line;
    { std::lock_guard<std::mutex> l(con_mx); if (!con_lines.empty()) { line = con_lines.front(); con_lines.pop_front(); } }
    if (!line.empty()) { logs("> "); logs(line.c_str()); logs("\n"); dbg_console(line.c_str()); processed++; }
    std::this_thread::yield();
  }
}

// ------------------------------ memoria ----------------------------------
static uint8_t ram[64][8192];
static uint8_t blk[8] = {0, 1, 2, 3, 4, 5, 6, 7};
static bool mon = false;            // fase MON: ventana, bloque 0 escribible
static uint8_t* ptr(int a){ a &= 0xFFFF; int b = a >> 13; int pg = (mon && b == 1) ? 63 : blk[b]; return &ram[pg][a & 0x1FFF]; }
static int cur_page(int a){ int b = (a & 0xFFFF) >> 13; return (mon && b == 1) ? 63 : blk[b]; }

extern "C" {
int  z80_contend_wrapper(int, int, int){ return 0; }
int  z80_contend_io_wrapper(int, int, int){ return 0; }
BYTE z80_readbyte_wrapper(int a){ return *ptr(a); }
BYTE z80_readoperandbyte_wrapper(int a){ return *ptr(a); }
BYTE z80_opcode_fetch_wrapper(int a){ z80_fetch_is_m1 = 0; return *ptr(a); }
void z80_writebyte_wrapper(int a, int d){
  int b = (a & 0xFFFF) >> 13;
  if (b == 0 && !mon) return;                   // ROM protegida
  if (cur_page(a) == 63 && !mon) return;        // pagina 63 protegida
  *ptr(a) = d;
}
// FPGA: puerto del depurador
static uint8_t ridx = 0, wexp = 0, wreg = 0, step_lo = 0, reason = 0, lvl = 0;
static bool step_on = false; static uint16_t step_cnt = 0;
void z80_writeport_wrapper(int port, int d, int*){
  int lo = port & 0xFF;
  if (lo == 0xA7) { zdata = d; dreg = true; return; }
  if (lo == 0xE7) { blk[d & 7] = d >> 3; return; }             // paginacion simple
  if ((port & 0xFFFF) == 0x3FEF) {
    if (wexp) { wexp = 0;
      if (wreg == 3) step_lo = d;
      if (wreg == 4) { step_cnt = step_lo | (d << 8); step_on = step_cnt != 0; }
    } else if (d & 0x80) { wexp = 1; wreg = d & 7; }
    else if (d < 16) ridx = d;
  }
}
BYTE z80_readport_wrapper(int port, int*){
  int lo = port & 0xFF;
  if (lo == 0xAF) return clk ? 0x80 : 0x00;
  if (lo == 0xA7) { int v = latch; dreg = true; return v; }
  if (lo == 0xE7) return blk[(port >> 8) & 7];
  if ((port & 0xFFFF) == 0x3FEF) return ridx == 0 ? (0x80 | (lvl << 5) | reason) : ridx == 15 ? 0x52 : 0;
  return 0xFF;
}
}

static uint8_t rd(int a){ return *ptr(a); }
static uint16_t rd16(int a){ return rd(a) | rd(a + 1) << 8; }
static void wr(int a, uint8_t v){ *ptr(a) = v; }
static int R(){ return (z80.r & 0x7F) | (z80.r7 & 0x80); }

// ------------------------- la FPGA, por instruccion -----------------------
static bool pause_pend = false, skip = false;
static void do_entry(uint8_t why){
  uint16_t x = z80.pc.w, sp = z80.sp.w;
  sp -= 2; wr(sp, (x + 1) & 0xFF); wr(sp + 1, (x + 1) >> 8);     // RST
  sp -= 2; wr(sp, 0x3B); wr(sp + 1, 0x00);                          // CALL
  z80.sp.w = sp; z80.pc.w = 0x2000; z80.r += 2;
  reason = why; mon = true; step_on = false; pause_pend = false; skip = false;
}
static void do_exit(){
  mon = false;
  uint16_t sp = z80.sp.w;
  z80.pc.w = rd16(sp) - 1; z80.sp.w = sp + 2; z80.r += 4;
  skip = true;
}

static int errors = 0;
#define CHECK(c, msg) do { if (!(c)) { printf("ERROR %s\n", msg); errors++; } else printf("ok    %s\n", msg); } while (0)

static void con(const char* l){ std::lock_guard<std::mutex> lk(con_mx); con_lines.push_back(l); posted++; }

// Ejecuta hasta que el MCU espera una orden (parado) o se pasan n instrucciones
static long executed = 0;
static void step_cpu(){
  if (mon) {
    z80_do_opcode();
    if (z80.pc.w == 0x003B) do_exit();
    return;
  }
  uint16_t pc = z80.pc.w;
  if (rd(pc) == 0xFF) { do_entry(7); return; }
  if (step_on && step_cnt == 0) { do_entry(4); return; }
  if (pause_pend && !skip) { do_entry(1); return; }
  if (step_on) step_cnt--;
  skip = false;
  z80_do_opcode();
  executed++;
}
static bool run_until_waiting(long max){
  for (long k = 0; k < max; k++) {
    step_cpu();
    if ((k & 255) == 0 && mon && processed == posted && dbg_waiting()) return true;
  }
  return false;
}
// Deja que el MCU procese las ordenes pendientes y despues ejecuta el
// programa n instrucciones (o hasta que vuelva a parar)
static void run_program(long n){
  while (processed != posted) step_cpu();
  for (long k = 0; k < 2000000 && mon; k++) step_cpu();     // hasta que salga del monitor
  for (long k = 0; k < n && !mon; k++) step_cpu();
}

int main(int argc, char** argv){
  FILE* f = fopen(argv[1], "rb"); size_t n = fread(&ram[63][0], 1, 8192, f); fclose(f);    // DEBUG.BIN
  f = fopen(argv[2], "rb"); n = fread(&ram[3][0], 1, 8192, f); fclose(f);                 // programa en $6000
  (void)n;
  for (int i = 0; i < 8192; i++) ram[1][i] = 0xA5;                                          // "ROM" del bloque 1
  z80_init(); z80_reset();
  z80.pc.w = 0x6000;
  std::thread mcu(mcu_thread);

  // 1. el programa corre un rato y se pausa unas cuantas veces en medio del bucle
  int stops = 0;
  for (int k = 0; k < 6; k++) {
    run_program(37 + k * 13);
    pause_pend = true;
    CHECK(run_until_waiting(5000000), "pausa: el MCU espera ordenes");
    stops++;
    if (k == 2) { con("s 5"); CHECK(run_until_waiting(5000000), "s 5"); }
    con("c");
    run_program(1);
  }
  // 2. la ventana: lo que hay en $2000 es la pagina del programa
  run_program(300);
  pause_pend = true;
  CHECK(run_until_waiting(5000000), "pausa");
  { std::lock_guard<std::mutex> l(out_mx); out_log.clear(); }
  con("m 2000 8");
  CHECK(run_until_waiting(5000000), "m 2000");
  CHECK(out_log.find("2000  A5 A5 A5 A5 A5 A5 A5 A5") != std::string::npos, "m 2000 ensena la pagina del programa");
  con("e 2000 11 22");
  CHECK(run_until_waiting(5000000), "e 2000");
  CHECK(ram[1][0] == 0x11 && ram[1][1] == 0x22 && ram[63][0] == 0xC3, "e 2000 escribe en la pagina del programa");
  // 3. breakpoint en el bucle
  con("b 6029");
  con("c");
  CHECK(run_until_waiting(5000000), "rompe en el breakpoint");
  CHECK(z80.pc.w >= 0x2000 && reason == 7, "motivo 7");
  con("c");
  CHECK(run_until_waiting(5000000), "otra vuelta, otra vez en el breakpoint");
  con("bc");
  // 4. el programa acaba el bucle y se queda en wait; se le saca con x pc
  con("c");
  run_program(200000);
  CHECK(rd(0x6103) == 0, "R avanza siempre 11 entre los dos LD A,R");
  CHECK(rd(0x6100) == 200 && rd16(0x6101) == 200, "el bucle ha dado sus 200 vueltas");
  CHECK(z80.pc.w == 0x604F || z80.pc.w == 0x6050, "esperando en wait");
  pause_pend = true;
  CHECK(run_until_waiting(5000000), "pausa en wait");
  con("x pc=6051");
  con("x hl=abcd");
  con("c");
  run_program(100);
  CHECK(rd(0x6104) == 1, "ha llegado al final");
  CHECK(rd16(0x6105) == 0xABCD, "HL cambiado con x");
  CHECK(rd16(0x6107) == 0x2222, "DE intacto");
  CHECK(rd16(0x6109) == 0xA5A5 && rd16(0x610B) == 0x5A5A, "IX, IY intactos");
  CHECK(rd16(0x610D) == 0x7F00, "SP intacto");
  CHECK(rd16(0x610F) == 0x6666 && rd16(0x6111) == 0x5555 && rd16(0x6113) == 0x4444, "HL' DE' BC' intactos");
  CHECK(rd(0x6115) == 0x88, "A' intacto");
  CHECK(stops == 6, "paradas");

  quit = true; mcu.join();
  printf(errors ? "%d ERRORES\n" : "TODO OK\n", errors);
  return errors;
}
