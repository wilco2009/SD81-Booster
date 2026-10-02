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
#include <chrono>
#include <functional>

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

static long now_ms(){
  using namespace std::chrono;
  return (long)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}
unsigned long millis(){ return (unsigned long)now_ms(); }
static std::atomic<int> qs_level{1};          // el boton QuickSilva (0 = pulsado)
int digitalRead(int){ return qs_level; }
uint8_t nQS_en = 1;
bool debug_monitor_loaded = true;
void set_status_LED(uint32_t){}
void set_status_led_ok(){}
static std::atomic<bool> pause_pend{false};
static bool skip = false;
static uint16_t stop_pc = 0;
static uint8_t cfgs[16];
static std::atomic<uint64_t> dirty{0};          // las paginas escritas (FPGA)
void send_bit_config(uint8_t c, uint8_t v){
  if ((c & 15) == 8 && v != cfgs[8]) pause_pend = true;   // orden 8: pausa (al conmutar)
  if ((c & 15) == 11 && v) dirty = 0;                      // orden 11: borrarlas
  cfgs[c & 15] = v;
}
uint8_t cfg_value(uint8_t c){ return cfgs[c & 15]; }
void set_blinking(uint32_t, float){}
void set_blinking_off(){}
char current_dir[100] = "/";
// la "SD": los ficheros van a esta carpeta (el "/" de la ruta se quita)
static FILE* snapf = nullptr;
static std::string sdpath(const char* p){ while (*p == '/') p++; return std::string("sd_") + p; }
bool snapfile_open(const char* p){ snapf = fopen(sdpath(p).c_str(), "wb"); return snapf != nullptr; }
bool snapfile_write(const void* d, uint16_t n){ return fwrite(d, 1, n, snapf) == n; }
void snapfile_close(void){ fclose(snapf); snapf = nullptr; }
bool snapfile_exists(const char* p){ FILE* f = fopen(sdpath(p).c_str(), "rb"); if (f) fclose(f); return f != nullptr; }
// el .Z81 que se carga (LOAD *Z81 con el monitor)
static FILE* z81f = nullptr;
bool z81in_open(const char* p){ if (z81f) fclose(z81f); z81f = fopen(sdpath(p).c_str(), "rb"); return z81f != nullptr; }
int z81in_read(void){ int c = fgetc(z81f); return c == EOF ? -1 : c; }
bool z81in_seek(uint32_t pos){ return fseek(z81f, pos, SEEK_SET) == 0; }
uint32_t z81in_pos(void){ return ftell(z81f); }
void z81in_close(void){ if (z81f) fclose(z81f); z81f = nullptr; }
static std::string opened_dir;                 // el ultimo OPENDIR
uint8_t opendir_list(const char* a){ opened_dir = a; return 0; }
// El estado del MCU (MCUSTATE.cpp): un fichero abierto de mentira, para
// probar que las claves pasan por el lector del .Z81 y se aplican
static std::string fh_saved = "/datos.bin", fh_loaded, fh_applied;
static uint32_t fh_pos_loaded = 0, fh_pos_applied = 0;
void mcustate_save(void (*w)(const char*)){ w("FILE_HANDLE 01 "); w(fh_saved.c_str()); w(" 00000123\n"); }
void mcustate_clear(void){ fh_loaded = ""; fh_pos_loaded = 0; }
bool mcustate_key(const char* k, uint8_t (*tok)(char*, uint8_t)){
  if (strcmp(k, "FILE_HANDLE")) return false;
  char t[100];
  tok(t, sizeof t); tok(t, sizeof t); fh_loaded = t;
  tok(t, sizeof t); fh_pos_loaded = strtoul(t, nullptr, 16);
  return true;
}
void mcustate_apply(void){ fh_applied = fh_loaded; fh_pos_applied = fh_pos_loaded; }
static uint8_t romfile[16384];                 // "SDBOOST.ROM": la imagen de las paginas 0-1
int32_t rom_file_read(uint32_t off, uint8_t* buf, uint16_t n){
  if (n == 0) return sizeof(romfile);
  if (off >= sizeof(romfile)) return 0;
  if (off + n > sizeof(romfile)) n = sizeof(romfile) - off;
  memcpy(buf, romfile + off, n); return n;
}

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
    dbg_qs_button();
    std::this_thread::yield();
  }
}

// ------------------------------ memoria ----------------------------------
static uint8_t ram[64][8192];
static uint8_t blk[8] = {0, 1, 2, 3, 4, 5, 6, 7};
static bool mon = false;            // fase MON: ventana, bloque 0 escribible
static uint8_t bram[65536];         // la BRAM de sombra, por direccion
static uint8_t pokereg[65536];      // los registros de los POKEs de control (2038-2098)
static std::vector<int> poke_log;   // en que orden se han escrito
static bool nmi_on = false;
static uint8_t ay_reg[2][16], ay_sel[2];  // los AY de la FPGA: [0] el A (A3=1), [1] el B
static uint8_t sprreg[32][28], spr_sel = 0;   // los sprites (2100-2128)
static int chroma_reg = -1;
static uint8_t* ptr(int a){ a &= 0xFFFF; int b = a >> 13; int pg = (mon && b == 1) ? 63 : blk[b]; return &ram[pg][a & 0x1FFF]; }
static uint8_t* pptr(int a){ a &= 0xFFFF; return &ram[blk[a >> 13]][a & 0x1FFF]; }   // como lo ve el programa
static int cur_page(int a){ int b = (a & 0xFFFF) >> 13; return (mon && b == 1) ? 63 : blk[b]; }

extern "C" {
int  z80_contend_wrapper(int, int, int){ return 0; }
int  z80_contend_io_wrapper(int, int, int){ return 0; }
BYTE z80_readbyte_wrapper(int a){ return *ptr(a); }
BYTE z80_readoperandbyte_wrapper(int a){ return *ptr(a); }
BYTE z80_opcode_fetch_wrapper(int a){ z80_fetch_is_m1 = 0; return *ptr(a); }
void z80_writebyte_wrapper(int a, int d){
  bool mon_ram = mon && !cfgs[cfgcmd_DBGPOKE];  // el monitor, sin la orden 10
  if (!mon_ram) bram[a & 0xFFFF] = d;           // la sombra copia lo que escribe la CPU
  int b = (a & 0xFFFF) >> 13;
  if (b == 0 && !mon_ram) {                     // ROM protegida: los POKEs de control
    if (a >= 2038 && a <= 2098) { pokereg[a] = d; poke_log.push_back(a); }
    if (a == 2100) spr_sel = d;
    if (a >= 2101 && a <= 2128) {               // un campo del sprite elegido; su sombra, en la copia
      sprreg[spr_sel & 31][a - 2101] = d;
      bram[a] = romfile[a];                     // (la FPGA no escribe ahi: deshacer lo de arriba)
      bram[0x0C00 + (spr_sel & 31) * 32 + (a - 2101)] = d;
    }
    return;
  }
  if (cur_page(a) == 63 && !mon) return;        // pagina 63 protegida
  *ptr(a) = d;
  dirty = dirty | (1ull << cur_page(a));        // la FPGA apunta la pagina escrita
}
// el teclado del ZX81: filas (A8-A15 a 0 la elegida), bits 0-4 a 0 pulsada
static std::atomic<uint8_t> keyrow[8];
static void key(int row, int bit, bool down){
  uint8_t v = keyrow[row];
  keyrow[row] = down ? (v | (1 << bit)) : (v & ~(1 << bit));
}
// FPGA: puerto del depurador
static uint8_t ridx = 0, wexp = 0, wreg = 0, step_lo = 0, reason = 0, lvl = 0, bptr_lo = 0;
static uint16_t bptr = 0;
static uint8_t dptr = 0;                        // indice 6: la pagina escrita que se lee
static bool step_on = false; static uint16_t step_cnt = 0;
void z80_writeport_wrapper(int port, int d, int*){
  int lo = port & 0xFF;
  if (lo == 0xA7) { zdata = d; dreg = true; return; }
  if (lo == 0xE7) { blk[d & 7] = cfgs[cfgcmd_FULLPAG] ? (port >> 8) & 63 : d >> 3; return; }   // como la FPGA
  if ((lo & 0x26) == 0x06) {                    // los AY: A3 el chip, A7 eleccion / dato
    int c = (lo & 8) ? 0 : 1;
    if (lo & 0x80) ay_sel[c] = d; else if (ay_sel[c] < 16) ay_reg[c][ay_sel[c]] = d;
    return;
  }
  if (lo == 0xFE) { nmi_on = true; return; }
  if (lo == 0xFD) { nmi_on = false; return; }
  if ((port & 0xFFFF) == 0x7FEF) { chroma_reg = d; return; }
  if ((port & 0xFFFF) == 0x3FEF) {
    if (wexp) { wexp = 0;
      if (wreg == 3) step_lo = d;
      if (wreg == 4) { step_cnt = step_lo | (d << 8); step_on = step_cnt != 0; }
      if (wreg == 5) bptr_lo = d;
      if (wreg == 6) bptr = bptr_lo | (d << 8);
      if (wreg == 7) bram[bptr++] = d;
    } else if (d & 0x80) { wexp = 1; wreg = d & 7; }
    else if (d < 16) { ridx = d; if (d == 6) dptr = 0; }
  }
}
BYTE z80_readport_wrapper(int port, int*){
  int lo = port & 0xFF;
  if (lo == 0xAF) return clk ? 0x80 : 0x00;
  if (lo == 0xA7) { int v = latch; dreg = true; return v; }
  if (lo == 0xE7) return blk[(port >> 8) & 7];
  if (lo == 0xFE) {                             // teclado
    uint8_t v = 0x1F;
    for (int r = 0; r < 8; r++) if (!(port & (0x100 << r))) v &= ~keyrow[r];
    return v | 0xE0;
  }
  if ((lo & 0x26) == 0x06 && (lo & 0x80)) { int c = (lo & 8) ? 0 : 1; return ay_sel[c] < 16 ? ay_reg[c][ay_sel[c]] : 0xFF; }
  if ((port & 0xFFFF) == 0x3FEF) {
    if (ridx == 2) return bram[bptr++];
    if (ridx == 3) return 0x2C;                 // registro de Chroma
    if (ridx == 4) return ay_sel[0];
    if (ridx == 5) return ay_sel[1];
    if (ridx == 6) return (uint8_t)((dirty >> (dptr++ & 63)) & 1);
    return ridx == 0 ? (0x80 | (lvl << 5) | reason) : ridx == 15 ? 0x52 : 0;
  }
  return 0xFF;
}
}

static uint8_t rd(int a){ return *ptr(a); }
static uint16_t rd16(int a){ return rd(a) | rd(a + 1) << 8; }
static void wr(int a, uint8_t v){ *ptr(a) = v; }
static int R(){ return (z80.r & 0x7F) | (z80.r7 & 0x80); }

// ------------------------- la FPGA, por instruccion -----------------------
static processor z_entry;            // el programa al parar (para comparar)
static int r_entry;
static void do_entry(uint8_t why){
  uint16_t x = z80.pc.w, sp = z80.sp.w;
  stop_pc = x;
  z_entry = z80; r_entry = (z80.r & 0x7F) | (z80.r7 & 0x80);
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

// Lee un .Z81 y lo compara con la memoria emulada
static std::vector<std::string> toks;
static size_t tp;
static void load_tokens(const char* fn){
  toks.clear(); tp = 0;
  FILE* f = fopen(fn, "rb"); if (!f) return;
  char w[128]; while (fscanf(f, "%127s", w) == 1) toks.push_back(w);
  fclose(f);
}
static bool seek_tok(const char* t){ while (tp < toks.size() && toks[tp] != t) tp++; if (tp < toks.size()) { tp++; return true; } return false; }
static int hexv(const std::string& s){ return (int)strtol(s.c_str(), nullptr, 16); }
static bool decode(std::vector<uint8_t>& out, size_t n){
  out.clear();
  while (out.size() < n && tp < toks.size()) {
    const std::string& t = toks[tp++];
    if (t[0] == '*') { int c = hexv(t.substr(1)); int v = hexv(toks[tp++]); while (c--) out.push_back(v); }
    else out.push_back(hexv(t));
  }
  return out.size() == n;
}
static void check_snapshot(const char* fn, bool all){
  load_tokens(fn);
  CHECK(!toks.empty(), "el fichero existe");
  tp = 0; CHECK(seek_tok("[CPU]") && seek_tok("PC") && hexv(toks[tp]) == stop_pc, "[CPU] PC");
  tp = 0;
  std::vector<uint8_t> m;
  CHECK(seek_tok("[MEMORY]") && seek_tok("MEMRANGE") && toks[tp] == "2000" && toks[tp+1] == "FFFF", "[MEMORY] MEMRANGE 2000 FFFF");
  tp += 2;
  bool ok = decode(m, 0xE000);
  for (int i = 0; i < 0xE000 && ok; i++) if (m[i] != *pptr(0x2000 + i)) { ok = false; printf("  [MEMORY] difiere en %04X: %02X / %02X\n", 0x2000 + i, m[i], *pptr(0x2000 + i)); }
  CHECK(ok, "[MEMORY] = la memoria que ve el programa");
  tp = 0;
  CHECK(seek_tok("MAPPER") && toks[tp] == "00" && toks[tp+7] == "07", "MAPPER");
  tp = 0;
  CHECK(seek_tok("HW_POKES") && toks[tp + (2045 - 2038)] == "AD" && toks[tp + (2090 - 2038)] == "55", "HW_POKES: 2045 y 2090");
  CHECK(toks[tp] == "--", "HW_POKES: 2038 nunca escrito");
  tp = 0;
  CHECK(seek_tok("DISPLAY_MODE") && toks[tp] == "01", "DISPLAY_MODE (2045 = 173)");
  CHECK(toks[tp + 1] == "WIDE_COLS" && toks[tp + 2] == "46", "WIDE_COLS 46 (70 columnas)");
  tp = 0;
  CHECK(seek_tok("SCROLL") && toks[tp] == "55" && toks[tp + 1] == "FF" && toks[tp + 3] == "FF", "SCROLL 55 FF FF FF");
  tp = 0;
  CHECK(seek_tok("DIR_OPEN") && toks[tp] == "2A2E5A3831", "DIR_OPEN *.Z81");
  tp = 0;
  CHECK(seek_tok("DISP_ADDR") && toks[tp] == "E000" && toks[tp + 1] == "01", "DISP_ADDR E000 01");
  tp = 0;
  CHECK(seek_tok("ATTR_ADDR") && toks[tp] == "E800" && toks[tp + 1] == "01", "ATTR_ADDR E800 01");
  tp = 0;
  CHECK(seek_tok("ROM_PROTECTED") && toks[tp] == "00", "ROM_PROTECTED 00");
  // paginas
  int pages = 0; bool pages_ok = true; bool p0 = false, p1 = false, p63 = false, p20 = false;
  tp = 0;
  while (seek_tok("RAM_PAGE")) {
    int pg = hexv(toks[tp++]);
    std::vector<uint8_t> d;
    if (!decode(d, 8192)) { pages_ok = false; break; }
    for (int i = 0; i < 8192; i++) if (d[i] != ram[pg][i]) { pages_ok = false; printf("  pagina %d difiere en %04X\n", pg, i); break; }
    if (toks[tp] != "RAM_PAGE_END") pages_ok = false;
    pages++; if (pg == 0) p0 = true; if (pg == 1) p1 = true; if (pg == 63) p63 = true; if (pg == 20) p20 = true;
  }
  CHECK(pages_ok, "RAM_PAGE = las paginas");
  CHECK(!p0 && !p63, "sin la pagina 0 (ROM intacta) ni la 63 (monitor)");
  CHECK(p1, "la pagina 1 si (se escribio en $2000)");
  CHECK(all ? pages == 62 : (pages == 8 && p20), all ? "con -a: 62 paginas" : "mapeadas y escritas: 8 paginas (1-7 y la 20)");
  tp = 0; CHECK(seek_tok("[EOF]"), "[EOF] al final");
}

int main(int argc, char** argv){
  remove("sd_prueba1.Z81"); remove("sd_prueba2.Z81");
  remove("sd_NONAME001.Z81"); remove("sd_NONAME002.Z81"); remove("sd_NONAME003.Z81"); remove("sd_NONAME004.Z81");
  FILE* f = fopen(argv[1], "rb"); size_t n = fread(&ram[63][0], 1, 8192, f); fclose(f);    // DEBUG.BIN
  f = fopen(argv[2], "rb"); n = fread(&ram[3][0], 1, 8192, f); fclose(f);                 // programa en $6000
  (void)n;
  for (int i = 0; i < 8192; i++) ram[1][i] = 0xA5;                                          // "ROM" del bloque 1
  memcpy(romfile, ram[0], 8192); memcpy(romfile + 8192, ram[1], 8192);                       // y su fichero
  memcpy(bram, ram[0], 8192); memcpy(bram + 8192, ram[1], 8192);                             // la BRAM al arrancar
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

  // 5. snapshot con el programa parado: solo las paginas mapeadas
  bram[2045] = 0xAD; bram[2090] = 0x55;                      // POKEs de control: 70 columnas,
  bram[2096] = 0x00; bram[2097] = 0xE0; bram[2098] = 0xAA;    // scroll, D_FILE alternativo
  bram[2059] = 0x00; bram[2060] = 0xE8; bram[2061] = 0xAA;    // y atributos alternativos
  dbg_note_opendir("*.Z81", true);                            // como el explorador
  for (int r = 0; r < 16; r++) { ay_reg[0][r] = 0x10 + r; ay_reg[1][r] = 0x80 + r; }   // los AY
  ay_sel[0] = 7; ay_sel[1] = 13;
  for (int j = 0; j < 28; j++) { sprreg[5][j] = j + 1; bram[0x0C00 + 5 * 32 + j] = j + 1; }   // el sprite 5
  sprreg[5][0] = 1; bram[0x0C00 + 5 * 32] = 1;               // enable y el bit alto de X: 0/1
  sprreg[5][2] = 1; bram[0x0C00 + 5 * 32 + 2] = 1;
  spr_sel = 5; bram[2100] = 5;
  bram[0x4100] ^= 0xFF;                                       // la sombra del bloque 2 no es la memoria
  { uint8_t b7 = blk[7]; blk[7] = 20; z80_writebyte_wrapper(0xE123, 0x5A); blk[7] = b7; }   // el programa escribe en la pagina 20
  pause_pend = true;
  CHECK(run_until_waiting(5000000), "pausa para el snapshot");
  con("snap prueba1");
  CHECK(run_until_waiting(50000000), "snap prueba1 acaba");
  check_snapshot("sd_prueba1.Z81", false);
  load_tokens("sd_prueba1.Z81");
  tp = 0; CHECK(seek_tok("AY1_REGS") && toks[tp] == "10" && toks[tp + 15] == "1F" && toks[tp + 16] == "AY1_REG_SEL" && toks[tp + 17] == "07",
                "AY1 (el A) y su registro elegido");
  tp = 0; CHECK(seek_tok("AY3_REGS") && toks[tp] == "80" && toks[tp + 17] == "0D", "AY3 (el B)");
  CHECK(ay_sel[0] == 7 && ay_sel[1] == 13, "el registro elegido de los AY, como estaba");
  tp = 0; CHECK(seek_tok("SPRITE_SEL") && toks[tp] == "05", "SPRITE_SEL");
  tp = 0; CHECK(seek_tok("SPRITE") && toks[tp] == "05" && toks[tp + 1] == "01" && toks[tp + 2] == "0102" && toks[tp + 3] == "04" &&
                toks[tp + 4] == "05" && toks[tp + 27] == "1C", "SPRITE 05 (n en x y colores filas mascaras)");
  CHECK(!seek_tok("SPRITE"), "solo el sprite 5");
  {
    // el 1 y el 3 tambien salen: el monitor (e 2000) y las pilas de la parada
    // escriben en la memoria sin pasar por la sombra
    bool b0 = false, b2 = false;
    tp = 0;
    while (seek_tok("SHADOW")) { if (toks[tp] == "00") b0 = true; if (toks[tp] == "02") b2 = true; }
    CHECK(b2, "SHADOW 02 (la sombra del bloque 2 no es la memoria)");
    CHECK(!b0, "sin SHADOW 00 (el bloque 0 es la ROM, salvo POKEs y sprites)");
  }
  tp = 0; CHECK(seek_tok("FILE_HANDLE") && toks[tp + 1] == "/datos.bin", "el estado del MCU (mcustate_save)");
  // lo que habia al hacer prueba1, para comparar despues de cargarlo
  static uint8_t ram_s[64][8192], bram_s[65536];
  uint8_t blk_s[8];
  memcpy(ram_s, ram, sizeof(ram)); memcpy(bram_s, bram, sizeof(bram)); memcpy(blk_s, blk, 8);
  processor z_s = z_entry;
  int r_s = r_entry;
  // 6. con -a: todas las paginas (con FULL_PAGING; sin el, solo hasta la 31)
  cfgs[cfgcmd_FULLPAG] = 1;
  con("snap -a prueba2");
  CHECK(run_until_waiting(80000000), "snap -a prueba2 acaba");
  check_snapshot("sd_prueba2.Z81", true);
  // 7. con el programa en marcha y nombre automatico: para, graba y se queda parado
  con("c");
  run_program(1000);
  con("snap");                                // desde la consola: para, graba y se queda parado
  CHECK(run_until_waiting(80000000), "snap en marcha: para y graba");
  CHECK(mon && dbg_is_stopped(), "snap desde la consola: se queda parado");
  con("c");
  run_program(1000);
  CHECK(!mon && !dbg_is_stopped(), "c: el programa sigue");
  FILE* f3 = fopen("sd_NONAME001.Z81", "rb");
  CHECK(f3 != nullptr, "nombre automatico NONAME001.Z81");
  if (f3) fclose(f3);

  // 8. LOAD *Z81 con el monitor: se desordena todo y se carga prueba1. La
  //    ROM llamaria al comando 75 y lanzaria la trampa; aqui, directamente.
  auto scramble = [&](){
    run_program(300);
    for (int p = 1; p < 63; p++) memset(ram[p], 0x77, 8192);
    for (int b = 1; b < 8; b++) blk[b] = 8 + b;
    memset(bram + 0x2000, 0x99, 0xE000);
    for (int a = 2038; a <= 2098; a++) { bram[a] = 0x99; pokereg[a] = 0x99; }
    poke_log.clear(); chroma_reg = -1; nmi_on = false; opened_dir = "";
    memset(ay_reg, 0x33, sizeof(ay_reg)); ay_sel[0] = ay_sel[1] = 2;
    memset(sprreg, 0x44, sizeof(sprreg)); spr_sel = 9; bram[2100] = 9;
    for (int i = 0; i < 32; i++) memset(bram + 0x0C00 + i * 32, 0x44, 28);   // (los bytes 28-31 nadie los escribe)
    fh_applied = ""; fh_pos_applied = 0;
  };
  auto load = [&](const char* name, uint8_t want_im){
    uint8_t im = 9;
    uint8_t st = dbg_z81_prepare(name, &im);
    CHECK(st == 0 && im == want_im, "LOAD *Z81: el MCU acepta el fichero (estado 0, IM)");
    do_entry(3);                                // la trampa
    for (long k = 0; k < 400000000 && mon; k++) step_cpu();
    CHECK(!mon, "carga: vuelve al programa");
  };
  auto same_regs = [&](const processor& z, int r){
    CHECK(z80.pc.w == z.pc.w && z80.sp.w == z.sp.w, "carga: PC y SP");
    CHECK(z80.af.w == z.af.w && z80.bc.w == z.bc.w && z80.de.w == z.de.w && z80.hl.w == z.hl.w, "carga: AF BC DE HL");
    CHECK(z80.af_.w == z.af_.w && z80.bc_.w == z.bc_.w && z80.de_.w == z.de_.w && z80.hl_.w == z.hl_.w, "carga: los alternativos");
    CHECK(z80.ix.w == z.ix.w && z80.iy.w == z.iy.w, "carga: IX IY");
    CHECK(z80.i == z.i && R() == r, "carga: I y R");
    CHECK(z80.iff1 == z.iff1, "carga: IFF");
  };
  scramble();
  load("/prueba1.Z81", 1);
  same_regs(z_s, r_s);
  CHECK(memcmp(blk, blk_s, 8) == 0, "carga: el mapper");
  {
    bool ok = true;
    for (int p = 0; p < 21 && ok; p++)
      for (int i = 0; i < 8192 && (p < 8 || p == 20); i++)
        if (ram[p][i] != ram_s[p][i]) { printf("  pagina %d difiere en %04X: %02X / %02X\n", p, i, ram[p][i], ram_s[p][i]); ok = false; break; }
    CHECK(ok, "carga: paginas 0-7 y 20 como al hacer prueba1");
    CHECK(dirty & (1ull << 20), "carga: la pagina 20 queda como escrita");
    ok = true;
    for (int a = 0; a < 0x10000 && ok; a++)
      if (bram[a] != bram_s[a]) { printf("  sombra difiere en %04X: %02X / %02X\n", a, bram[a], bram_s[a]); ok = false; }
    CHECK(ok, "carga: la BRAM de sombra entera como al hacer prueba1 (con SHADOW 02 y los sprites)");
    ok = true;
    for (int a = 2038; a <= 2098 && ok; a++)
      if (bram[a] != bram_s[a]) { printf("  sombra de los POKEs difiere en %d: %02X / %02X\n", a, bram[a], bram_s[a]); ok = false; }
    CHECK(ok, "carga: la sombra de 2038-2098 como al hacer prueba1");
  }
  CHECK(pokereg[2045] == 0xAD && pokereg[2090] == 0x55, "carga: POKEs del snapshot (2045, 2090)");
  CHECK(pokereg[2096] == 0x00 && pokereg[2097] == 0xE0 && pokereg[2098] == 0xAA, "carga: D_FILE alternativo");
  CHECK(pokereg[2059] == 0x00 && pokereg[2060] == 0xE8 && pokereg[2061] == 0xAA, "carga: atributos alternativos");
  CHECK(pokereg[2046] == 0x0F && pokereg[2047] == 85 && pokereg[2049] == 0x3C && pokereg[2040] == 0 && pokereg[2091] == 0xFF,
        "carga: los nunca escritos, a su valor de reset");
  CHECK(pokereg[2041] == 0x99 && pokereg[2056] == 0x99, "carga: 2041 y 2056 sin tocar");
  {
    auto at = [&](int a){ for (size_t i = 0; i < poke_log.size(); i++) if (poke_log[i] == a) return (int)i; return -1; };
    CHECK(at(2045) == 0, "carga: el modo de video el primero");
    CHECK(at(2040) > at(2098) && at(2040) > at(2061) && at(2038) > at(2098), "carga: las interrupciones simuladas al final");
  }
  CHECK(chroma_reg == 0x2C, "carga: Chroma81");
  {
    bool ok = ay_sel[0] == 7 && ay_sel[1] == 13;
    for (int r = 0; r < 16; r++) ok = ok && ay_reg[0][r] == 0x10 + r && ay_reg[1][r] == 0x80 + r;
    CHECK(ok, "carga: los dos AY y su registro elegido");
    ok = spr_sel == 5;
    for (int i = 0; i < 32; i++)
      for (int j = 0; j < 28; j++) ok = ok && sprreg[i][j] == (i == 5 ? (j == 0 || j == 2 ? 1 : j + 1) : 0);
    CHECK(ok, "carga: los sprites (el 5 y los demas a cero) y el elegido");
  }
  CHECK(fh_applied == "/datos.bin" && fh_pos_applied == 0x123, "carga: el estado del MCU (mcustate_key / apply)");
  CHECK(opened_dir == "*.Z81", "carga: DIR_OPEN -> OPENDIR");
  CHECK(!nmi_on, "carga: NMI apagada (NMI 00)");
  CHECK(cfgs[cfgcmd_DBGPOKE] == 0, "carga: la orden 10 queda apagada");

  // 9. con NMI 01: la vuelta pasa por OUT ($FE),A / RET debajo de la pila
  {
    FILE* fi = fopen("sd_prueba1.Z81", "rb"); FILE* fo = fopen("sd_prueba1n.Z81", "wb");
    char line[512];
    while (fgets(line, sizeof line, fi)) {
      if (!strncmp(line, "NMI 00", 6)) memcpy(line, "NMI 01", 6);
      fputs(line, fo);
    }
    fclose(fi); fclose(fo);
  }
  scramble();
  load("/prueba1n.Z81", 1);
  CHECK(z80.pc.w == (uint16_t)(z_s.sp.w - 8) && !nmi_on, "NMI: sale por el OUT puesto debajo de la pila");
  step_cpu(); step_cpu();
  CHECK(nmi_on, "NMI: encendida");
  same_regs(z_s, r_s);

  // 11. sin consola: el boton QS (1-3 s, pausa) y el teclado del ZX81
  {
    auto run_ms = [&](long ms){ long t = now_ms(); while (now_ms() - t < ms) step_cpu(); };
    auto run_until = [&](std::function<bool()> c, long ms){ long t = now_ms(); while (!c() && now_ms() - t < ms) step_cpu(); return c(); };
    auto qs_pause = [&](){
      qs_level = 0; run_ms(1500); qs_level = 1;
      bool ok = run_until([]{ return mon && dbg_is_stopped(); }, 3000);
      run_ms(150);                              // que el MCU lea el teclado sin nada pulsado
      return ok;
    };
    size_t from = 0;                            // solo lo escrito desde la ultima accion
    auto mark = [&](){ std::lock_guard<std::mutex> l(out_mx); from = out_log.size(); };
    auto logged = [&](const char* t){ std::lock_guard<std::mutex> l(out_mx); return out_log.find(t, from) != std::string::npos; };
    CHECK(qs_pause(), "QS 1,5 s: pausa");
    processor zq = z_entry; int rq = r_entry;
    uint8_t m6100 = *pptr(0x6100);
    auto tap = [&](int row, int bit){ mark(); key(row, bit, true); run_ms(150); key(row, bit, false); };
    tap(0, 1);                                  // Z: snapshot y se queda parado
    CHECK(run_until([&]{ return logged("Snapshot saved: /NONAME002.Z81"); }, 20000), "Z: snapshot NONAME002");
    run_ms(200);
    CHECK(mon && dbg_is_stopped(), "Z: sigue parado");
    con("x hl=1234"); con("e 6100 99");         // se cambia algo
    run_until([]{ return processed == posted; }, 2000);
    run_ms(100);
    CHECK(*pptr(0x6100) == 0x99 && zq.hl.w != 0x1234, "cambiados HL y $6100");
    tap(6, 1);                                  // L: carga el ultimo grabado
    CHECK(run_until([&]{ return logged("Snapshot loaded"); }, 20000), "L: carga NONAME002");
    CHECK(run_until([]{ return !mon; }, 5000), "L: el programa sigue");
    CHECK(z80.hl.w == zq.hl.w && z80.pc.w == zq.pc.w && R() == rq && *pptr(0x6100) == m6100, "L: como al hacer el snapshot");
    run_ms(100);
    CHECK(qs_pause(), "QS: pausa otra vez");
    tap(1, 1);                                  // S: snapshot y sigue
    CHECK(run_until([&]{ return logged("Snapshot saved: /NONAME003.Z81"); }, 20000), "S: snapshot NONAME003");
    CHECK(run_until([]{ return !mon && !dbg_is_stopped(); }, 5000), "S: el programa sigue");
    run_ms(100);
    CHECK(qs_pause(), "QS: pausa otra vez");
    key(7, 0, true);                            // espacio: sigue, pero al soltarlo
    run_ms(400);
    CHECK(mon && dbg_is_stopped(), "espacio pulsado: todavia parado");
    key(7, 0, false);
    CHECK(run_until([]{ return !mon && !dbg_is_stopped(); }, 5000), "espacio soltado: el programa sigue");
    // el boton 3,5 s con el programa en marcha: snapshot y sigue
    run_ms(100);
    mark(); qs_level = 0; run_ms(3500); qs_level = 1;
    CHECK(run_until([&]{ return logged("Snapshot saved: /NONAME004.Z81"); }, 20000), "QS 3,5 s: snapshot NONAME004");
    CHECK(run_until([]{ return !mon && !dbg_is_stopped(); }, 5000), "QS 3,5 s: el programa sigue");
  }

  // 10. un .Z81 de EightyOne sin MAPPER ni HW_POKES (claves de EightyOne)
  {
    FILE* fo = fopen("sd_emu.Z81", "wb");
    fputs("[MACHINE]\nMODEL ZX81\n\n[CPU]\nPC 6000    SP  7E00\nHL 1111    HL_ 2222\nDE 3333    DE_ 4444\n"
          "BC 5555    BC_ 6666\nAF 7700    AF_ 8800\nIX 9999    IY  AAAA\nIR 3F42\nIM 02      IF1 01\nHT 00      IF2 01\n\n"
          "[ZX81]\nNMI 00     SYNC 01\nLINE 002\n\n[MEMORY]\nRAM_PACK 48K\n8K_RAM_ENABLED 01\nROM_PROTECTED 00\nMEMRANGE 2000 FFFF\n", fo);
    for (int a = 0x2000; a < 0x10000; a++) {
      if ((a & 0xFFF) == 0x800) { fprintf(fo, "*0100 %02X ", a >> 8); a += 0xFF; continue; }   // algun tramo con RLE
      fprintf(fo, "%02X ", (a * 7 + (a >> 8)) & 0xFF);
      if ((a & 15) == 15) fputs("\n", fo);
    }
    fputs("\n[INTERFACES]\nZX_PRINTER 00\n[COLOUR]\nTYPE Chroma\n", fo);
    for (int i = 0; i < 16384; i++) { fprintf(fo, "%02X ", (0x5A ^ i ^ (i >> 7)) & 0xFF); if ((i & 15) == 15) fputs("\n", fo); }
    fputs("CHROMA_MODE 31\nCOLOUR_ENABLED 01\n\n[CHR$_GENERATOR]\nTYPE CHR$128\n\n[SD81BOOSTER]\nSD_PATH C:\\x\nCUR_DIR /JUEGOS/\n"
          "DISPLAY_MODE 01\nWIDE_COLS 50\nBORDER_INK 03\nBORDER_PATTERN 00\nBORDER_CHARS 00 00 00 00 00 00 00 00\nHFILE 1234\n"
          "CHROMA_MODE 31\nDBUF 45\nSEL128 00\nSEL256 01\nWRX 00\nROMLOCK 00\nSCROLL 00 FF FF FF\nDIR_OPEN 2A\n"
          "DISP_ADDR E000 01\nAY1_REGS 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00\nVGM_PATH -\nPEG_MEM\n"
          "0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000\nPEG_PC 00 00 00\n\n[EOF]\n", fo);
    fclose(fo);
  }
  scramble();
  uint8_t blk_e[8]; memcpy(blk_e, blk, 8);
  load("/emu.Z81", 2);
  CHECK(z80.pc.w == 0x6000 && z80.sp.w == 0x7E00 && z80.hl.w == 0x1111 && z80.hl_.w == 0x2222 && z80.ix.w == 0x9999 && z80.iy.w == 0xAAAA,
        "EightyOne: registros");
  CHECK(z80.i == 0x3F && R() == 0x42 && z80.iff1 == 1, "EightyOne: I, R, IFF");
  CHECK(memcmp(blk, blk_e, 8) == 0, "EightyOne: sin MAPPER, el mapper no cambia");
  {
    bool ok = true;
    for (int a = 0x2000; a < 0x10000 && ok; a++) {
      if (a == 0x7DFE || a == 0x7DFF) continue;   // SP-2: la vuelta del monitor (PC+1), como en cada parada
      uint8_t want = ((a & 0xFFF) >= 0x800 && (a & 0xFFF) < 0x900) ? (a >> 8) : ((a * 7 + (a >> 8)) & 0xFF);
      if (*pptr(a) != want) { printf("  memoria difiere en %04X: %02X / %02X\n", a, *pptr(a), want); ok = false; }
      uint8_t sh = a >= 0xC000 ? ((0x5A ^ (a - 0xC000) ^ ((a - 0xC000) >> 7)) & 0xFF) : want;
      if (ok && bram[a] != sh) { printf("  sombra difiere en %04X: %02X / %02X\n", a, bram[a], sh); ok = false; }
    }
    CHECK(ok, "EightyOne: [MEMORY] en las paginas de ahora, la sombra con [COLOUR] encima");
  }
  CHECK(pokereg[2045] == 174 && pokereg[2046] == 3 && pokereg[2043] == 0x34 && pokereg[2044] == 0x12, "EightyOne: 80 columnas, borde, HFILE");
  CHECK(pokereg[2096] == 0 && pokereg[2097] == 0xE0 && pokereg[2098] == 170 && pokereg[2061] == 85 && pokereg[2057] == 85,
        "EightyOne: DISP_ADDR, sin ATTR_ADDR ni doble buffer");
  CHECK(cfgs[cfgcmd_256CHARS] == 1 && cfgs[cfgcmd_128CHARS] == 0, "EightyOne: SEL256 manda sobre [CHR$_GENERATOR]");
  CHECK(chroma_reg == 0x31 && opened_dir == "*" && !strcmp(current_dir, "/JUEGOS/"), "EightyOne: Chroma, DIR_OPEN, CUR_DIR");
  strcpy(current_dir, "/");
  { uint8_t im2; CHECK(dbg_z81_prepare("/no_existe.Z81", &im2) == 1, "LOAD *Z81: sin fichero, error 1"); }

  quit = true; mcu.join();
  printf(errors ? "%d ERRORES\n" : "TODO OK\n", errors);
  return errors;
}
