// Prueba de SPORTS (EXAMPLES/NETGAME/sports.bin: Tennis, Hockey, Squash y Practice, la bola es de quien la recibe) en el PC: el programa de verdad en el Z80 de EightyOne, con el teclado del ZX81 de mentira
// (pulsaciones con tiempo), los sprites por hardware simulados (z80net_env.h), la pantalla leida de la memoria, el modem de verdad (NET_BRIDGE y NET_MQTT
// del ESP32, con su sondeo de 15 ms) y el rival hecho aqui por el broker de mentira. Dos partes:
//   A. el Z80 es el CREADOR (izquierda, el que simula): sprites, saque, fisica de la bola (limites, paredes, palas), tantos, fin de partida,
//      ENTER para otra partida, teclas, ping y salir;
//   B. el Z80 es el INVITADO (derecha): dibuja lo que manda el creador (que hace esta prueba), manda su pala con los sellos de fotograma, adelanta la
//      bola, avisa de la partida acabada y de que el rival no contesta.
// Los simbolos (P_BX...) se leen de pong.sym. Con SHOW=1 imprime la pantalla en los pasos importantes.
#include "z80net_env.h"
#include <functional>
#include <fstream>
#include <random>
#include <deque>
#include <sstream>

static const uint16_t DFILE = 0x8800;           // la pantalla de texto del lobby
static const uint16_t HFILE = 0xA000;           // la pantalla del juego: HiRes Spectrum

static std::map<std::string, uint16_t> syms;
static void load_syms(const char* path) {
  std::ifstream f(path); std::string line;
  while (std::getline(f, line)) {
    std::istringstream is(line); std::string name, equ, val;
    if (is >> name >> equ >> val && equ == "EQU") { if (val.back() == 'H' || val.back() == 'h') val.pop_back(); syms[name] = (uint16_t)strtoul(val.c_str(), nullptr, 16); }
  }
}
static uint16_t sym(const char* n) { auto it = syms.find(n); if (it == syms.end()) { printf("ERROR falta el simbolo %s\n", n); errors++; return 0; } return it->second; }
static uint8_t V(const char* n) { return mem[sym(n)]; }
static int8_t SV_(const char* n) { return (int8_t)mem[sym(n)]; }

static void install_font() { for (int c = 1; c < 64; c++) for (int k = 0; k < 8; k++) mem[0x1E00 + c * 8 + k] = (uint8_t)(c * 3 + 1 + k * 17); }
static uint8_t cell_byte(int row, int col, int line) { return mem[HFILE + ((row & 0x18) << 8) + (line << 8) + ((row & 7) << 5) + col]; }
static int glyph_code(int row, int col) {
  bool blank = true;
  for (int k = 0; k < 8; k++) if (cell_byte(row, col, k)) blank = false;
  if (blank) return 0;
  for (int c = 1; c < 64; c++) {
    bool ok = true;
    for (int k = 0; k < 8 && ok; k++) if (cell_byte(row, col, k) != (uint8_t)(c * 3 + 1 + k * 17)) ok = false;
    if (ok) return c;
  }
  return -1;
}
static char zx_char(int v) {
  if (v == 0) return ' ';
  if (v >= 28 && v <= 37) return (char)('0' + v - 28);
  if (v >= 38 && v <= 63) return (char)('A' + v - 38);
  if (v == 27) return '.'; if (v == 14) return ':'; if (v == 22) return '-'; if (v == 15) return '?'; if (v == 20) return '='; if (v == 24) return '/';
  if (v == 21) return '+'; if (v == 26) return ','; if (v == 16) return '('; if (v == 17) return ')';
  return '~';
}
static bool gfx_mode() { return mem[2045] == 172; }
static std::string screen_row(int r) {
  std::string s;
  for (int c = 0; c < 32; c++) {
    if (gfx_mode()) s += zx_char(glyph_code(r, c));
    else { int v = mem[DFILE + 1 + 33 * r + c]; s += zx_char(v); }
  }
  return s;
}
static std::string screen_all() { std::string s; for (int r = 0; r < 24; r++) s += screen_row(r) + "\n"; return s; }
static bool shows_now(const char* t) { return screen_all().find(t) != std::string::npos; }
static void run_ms(uint32_t ms);
// (el Z80 repinta una fila a trozos: una foto a mitad de pintar puede salir sin acabar, asi que se mira unas veces antes de decir que no)
static bool ping_shown() { for (int k = 0; k < 4; k++) { if (screen_row(23).compare(0, 4, "PING") == 0) return true; run_ms(10); } return false; }   // "PING nnnn MS" en las 12 celdas de la izquierda de la ultima fila
static bool shows(const char* t) { for (int k = 0; k < 4; k++) { if (shows_now(t)) return true; run_ms(10); } return false; }
static bool show_on = getenv("SHOW") != nullptr;
static void show(const char* title) {
  if (!show_on) return;
  printf("---- %s (t=%u ms)\n", title, mock_now - 100000);
  for (int r = 0; r < 24; r++) { std::string l = screen_row(r); while (!l.empty() && l.back() == ' ') l.pop_back(); if (!l.empty()) printf("%2d|%s\n", r, l.c_str()); }
}

// ------------------------------------------------------------ el tiempo y el teclado
static std::function<void()> on_tick;           // lo que hace el rival cada 10 ms
static void run_ms(uint32_t ms) {
  for (uint32_t t = 0; t < ms; t += 10) {
    long target = instr_total + (long)(10.0 * 3250.0 / 5.0 / slowdown);
    while (instr_total < target) {
      z80_do_opcode();
      instr_total++;
      if ((instr_total & 255) == 0) { mock_now = 100000 + time_extra + (uint32_t)(instr_total * 5 * slowdown / 3250); net_bridge_loop(); }
    }
    if (on_tick) on_tick();
  }
}
static bool wait_text(const char* t, uint32_t max_ms) { for (uint32_t w = 0; w < max_ms; w += 50) { if (shows(t)) return true; run_ms(50); } return shows(t); }
static bool wait_gfx(uint32_t max_ms) { for (uint32_t w = 0; w < max_ms; w += 50) { if (gfx_mode()) return true; run_ms(50); } return gfx_mode(); }
struct KeyPos { int row, bit; };
static bool key_pos(char c, KeyPos* k) {
  static const char* rows[8] = {"\0ZXCV", "ASDFG", "QWERT", "12345", "09876", "POIUY", "\rLKJH", " .MNB"};
  for (int r = 0; r < 8; r++) for (int b = 0; b < 5; b++) if (rows[r][b] == c && c != 0) { k->row = r; k->bit = b; return true; }
  return false;
}
static void hold(char c, uint32_t ms) {
  KeyPos k; if (!key_pos(c, &k)) { printf("ERROR tecla desconocida %d\n", c); errors++; return; }
  keyrows[k.row] |= (uint8_t)(1 << k.bit);
  run_ms(ms);
  keyrows[k.row] &= (uint8_t)~(1 << k.bit);
  run_ms(60);
}
static void press(char c) { run_ms(60); if (c == 8) { keyrows[0] |= 1; keyrows[4] |= 1; run_ms(120); keyrows[0] = keyrows[4] = 0; run_ms(120); return; } hold(c, 120); }
static void type(const char* s) { for (; *s; s++) press(*s); }
static std::string code_on_screen() {
  std::string c;
  for (int i = 0; i < 10; i++) { int v = mem[DFILE + 1 + 33 * 6 + 11 + i]; c += (char)(v >= 38 ? v + 27 : v + 20); }
  return c;
}

static std::vector<uint8_t> bin;
static void reboot() {
  memset(keyrows, 0, sizeof keyrows);
  memset(mem, 0, sizeof(mem));
  memcpy(mem + 0x4400, bin.data(), bin.size());
  install_font();
  mem[0x02E7] = 0xC9; mem[0x0207] = 0xC9;
  z80_init(); z80_reset();
  reset_world();
  on_tick = nullptr;
  z80.sp.w = 0xFFF0; mem[0xFFF0] = 0x00; mem[0xFFF1] = 0x01; mem[0x0100] = 0x76;
  z80.pc.w = 17408; z80.iff1 = z80.iff2 = 0;
}

// un estado del creador, tal como sale al broker: [1][fr][bx][by][vx][vy][pala][tantos izq][tantos der][flags][sello del invitado][edad]
// ------------------------------------------------------------ el rival: una copia en C++ de las reglas (la bola es de quien la recibe)
static const int WIN = 15;
static const bool SERVE_TO_LOSER = true;
static const char* ALPHA = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
struct Peer {
  int role = 1, game = 0, bath = 32, spd0 = 3, ash = 3, pmax = 144, turn = 0;
  int fr = 0, my = 80, oy = 80, oyt = 80, sl = 0, sr = 0, flags = 0, serve = 50, ping = 2, last = 0, hits = 0, hc = 0, phc = 0, gen = 0, srv = 0, pend = 0, sim = 0, rxf = 0;
  int bx = 124, by = 92, vx = 0, vy = 0;
  bool rxok = false;
  int n_hit = 0, n_missed = 0, n_launch = 0, n_wall = 0;
  std::string topic;
  std::deque<std::pair<uint32_t, std::string>> dq; int jit_max = 0; std::mt19937 jr{5};

  void config(int cfg) {
    game = cfg & 3; bath = (cfg & 4) ? 16 : 32; pmax = 176 - bath; spd0 = (cfg & 8) ? 4 : 3;
    int c = (cfg & 16) ? 2 : 3; if (bath != 32) c--; ash = c;
    my = oy = oyt = 96 - bath / 2;
  }
  int bat_x(int r, int j) const { static const int t[16] = {12, 0, 236, 0, 52, 12, 196, 236, 12, 0, 28, 0, 12, 0, 12, 0}; return t[game * 4 + r * 2 + j]; }
  int nbats() const { return game == 1 ? 2 : 1; }
  bool is_owner() const {
    if (!vx) return false;
    int neg = vx < 0;
    if (game < 2) return neg == (role ^ 1);
    if (!neg) return false;
    return turn == role;
  }
  void chk_over() { if (sl >= WIN || sr >= WIN) flags |= 2; else flags &= ~2; }
  void center() { bx = 124; by = 92; vx = 0; vy = 0; }
  void begin_serve(int launcher) { srv = launcher; flags |= 1; center(); hits = 0; pend = 0; serve = 50; }
  void launch() {
    static const int vyt[4] = {-2, -1, 1, 2};
    flags &= ~1;
    if (game < 2) vx = role ? spd0 : -spd0;
    else { turn = (game == 3) ? role : role ^ 1; vx = spd0; }
    vy = vyt[fr & 3]; hits = 0; hc++; n_launch++;
  }
  void restart() { sl = sr = 0; gen = (gen + 1) & 255; flags &= ~2; begin_serve(0); }
  void missed() {
    n_missed++;
    if (game == 3) { if (sl >= sr) sr = sl; sl = 0; begin_serve(0); return; }
    if (role == 0) sr++; else sl++;
    chk_over(); begin_serve(SERVE_TO_LOSER ? role : role ^ 1);
  }
  bool hit_test(int py) const { return by + 8 > py && by < py + bath; }
  int hit_speed() { hits++; int h = hits >> 1; if (h >= 4) h = 3; return spd0 + h; }
  void hit_vy(int c) { int d = bath / 2 - 4; int a = (int8_t)(by - c - d); for (int i = 0; i < ash; i++) a >>= 1; if (a < 0) { if (a < -4) a = -4; } else if (a >= 5) a = 4; vy = a; }
  void set_pend() { if (!sim) pend = 12; }
  void own_ev() { hc++; pend = 12; }
  void own_hit() {
    hit_vy(my); n_hit++;
    if (game >= 2) { if (game == 2) turn = role ^ 1; else if (sl < 99) sl++; }
    own_ev();
  }
  void own_left() {
    for (int j = 0; j < nbats(); j++) {
      int x = bat_x(role, j);
      if (x + 8 < bx) return;
      if (bx < x - 7) continue;
      if (hit_test(my)) { bx = x + 9; vx = hit_speed(); own_hit(); return; }
    }
    if (game == 1) {
      if (bx >= 8) return;
      int c = ((by + 4) - 48) & 255;
      if (c >= 96) { bx = 8; vx = -vx; n_wall++; own_ev(); return; }
      if (bx < 2) missed();
      return;
    }
    if (bx < 5) missed();
  }
  void own_right() {
    for (int j = 0; j < nbats(); j++) {
      int x = bat_x(role, j);
      if (bx < x - 8) return;
      if (x + 7 < bx) continue;
      if (hit_test(my)) { bx = x - 9; vx = -hit_speed(); own_hit(); return; }
    }
    if (game == 1) {
      if (bx < 241) return;
      int c = ((by + 4) - 48) & 255;
      if (c >= 96) { bx = 240; vx = -vx; n_wall++; own_ev(); return; }
      if (bx >= 246) missed();
      return;
    }
    if (bx >= 244) missed();
  }
  void ball_step() {
    int x = bx + vx; if (x < 0) x = 0; if (x > 255) x = 255; bx = x;
    int y = by + vy;
    if (y < 16) { by = 16; vy = -vy; set_pend(); } else if (y >= 169) { by = 168; vy = -vy; set_pend(); } else by = y;
    if (game >= 2 && bx >= 241) { bx = 240; vx = -vx; set_pend(); }
    if (sim || !is_owner()) { if (bx < 4) bx = 4; if (bx > 244) bx = 244; return; }
    if (game >= 2 || role == 0) own_left(); else own_right();
  }
  void phys() {
    if (pend) pend--;
    if (flags & 2) return;
    if (flags & 1) { if (srv == role && --serve == 0) launch(); return; }
    ball_step();
  }
  void lead() { int l = ping >> 1; if (l > 6) l = 6; sim = 1; for (int i = 0; i < l; i++) ball_step(); sim = 0; }
  static bool dis(int nw, int old) { return nw && old && ((nw < 0) != (old < 0)); }
  static void half(int old, int& nw) {
    if (nw >= old) { int d = nw - old; if (d >= 24) return; nw = old + (d >> 1); }
    else { int d = old - nw; if (d >= 24) return; nw = old - (d >> 1); }
  }
  void recv(const std::string& m) {
    if (m.size() < 15 || m[0] != 1) return;
    uint8_t mf = (uint8_t)m[1];
    if ((uint8_t)(mf - last) & 0x80) return;
    if ((uint8_t)((uint8_t)m[8] - gen) & 0x80) return;
    last = mf; oyt = (uint8_t)m[2];
    int p = (int)(uint8_t)(fr - (uint8_t)m[3]) - (uint8_t)m[4]; ping = p < 0 ? 0 : p; rxf = fr & 255; rxok = true;
    if ((uint8_t)m[8] != gen) { gen = (uint8_t)m[8]; sl = (uint8_t)m[6]; sr = (uint8_t)m[7]; flags &= ~2; chk_over(); begin_serve(0); }
    else {
      int c = 0;
      if ((uint8_t)m[6] > sl) { sl = (uint8_t)m[6]; c |= 1; }
      if ((uint8_t)m[7] > sr) { sr = (uint8_t)m[7]; c |= 2; }
      if (c) { chk_over(); int loser = (c & 1) ? 1 : 0; if (!SERVE_TO_LOSER) loser ^= 1; begin_serve(loser); }
    }
    if (flags & 2) return;
    int bx_m = (uint8_t)m[11], by_m = (uint8_t)m[12], vx_m = (int8_t)m[13], vy_m = (int8_t)m[14];
    if ((uint8_t)m[9] != phc) { phc = (uint8_t)m[9]; hits = (uint8_t)m[10]; bx = bx_m; by = by_m; vx = vx_m; vy = vy_m; flags &= ~1; pend = 0; if (game == 2) turn = role; lead(); return; }
    if (((uint8_t)m[5] & 1) || (flags & 1)) return;
    if (is_owner()) return;
    int lbx = bx, lby = by, lvx = vx, lvy = vy;
    bx = bx_m; by = by_m; vx = vx_m; vy = vy_m; lead();
    if (dis(vx, lvx) || dis(vy, lvy)) {
      if (pend) { bx = lbx; by = lby; vx = lvx; vy = lvy; }
      else pend = 0;
    } else { pend = 0; half(lbx, bx); half(lby, by); }
  }
  void pad() {
    int t = std::max(16, std::min(pmax, by + 4 - bath / 2));
    if (my < t) my = std::min(t, my + 4); else if (my > t) my = std::max(t, my - 4);
  }
  std::string make() const {
    std::string s;
    int age = (uint8_t)(fr - rxf);
    int v[15] = {1, fr & 255, my, last & 255, age, flags, sl, sr, gen, hc & 255, hits, bx, by, vx & 255, vy & 255};
    for (int i = 0; i < 15; i++) s += (char)v[i];
    return s;
  }
  void tick(uint32_t now) {
    fr = (fr + 1) & 255;
    if (oy < oyt) oy = std::min(oyt, oy + 6); else if (oy > oyt) oy = std::max(oyt, oy - 6);
    pad(); phys();
    dq.push_back({now + (jit_max ? (uint32_t)(jr() % (jit_max + 1)) : 0u), make()});
  }
  void flush(uint32_t now) { while (!dq.empty() && dq.front().first <= now) { peer_publish(topic, dq.front().second, false); dq.pop_front(); } }
};

// las teclas del Z80: 1 sigue la bola con la pala, 2 se aleja de ella (nunca la alcanza), 0 quieta
static void z80_keys(int track) {
  keyrows[2] &= ~1; keyrows[1] &= ~1;             // Q, A
  if (track == 2) { if (V("P_BY") < 92) keyrows[1] |= 1; else keyrows[2] |= 1; return; }
  if (!track) return;
  int by = V("P_BY"), my = V("P_MY"), c = by + 4 - (my + V("P_BATH") / 2);
  if (c < -3) keyrows[2] |= 1; else if (c > 3) keyrows[1] |= 1;
}

static const char* GAMES[4] = {"TENNIS", "HOCKEY", "SQUASH", "PRACTICE"};

// el menu del juego y de las opciones, tal como lo usa una persona: 1 = partida nueva; la cifra del juego; las opciones (1 palas, 2 bola, 3 angulos); ENTER
static void menu_new(int cfg) {
  press('1');
  CHECK(wait_text("CHOOSE THE GAME", 3000), "menu de juego");
  press((char)('1' + (cfg & 3)));
  CHECK(wait_text("OPTIONS", 3000), "menu de opciones");
  if (cfg & 4) press('1');
  if (cfg & 8) press('2');
  if (cfg & 16) press('3');
  press('\r');
}

// Un juego entero con el Z80 de creador (z80_role = 0) o de invitado (1) y el rival escrito aqui. Devuelve cuando ha vuelto al menu.
static void scenario(int cfg, int z80_role) {
  int game = cfg & 3;
  char title[120]; snprintf(title, sizeof title, "%s%s%s%s, el Z80 es el %s", GAMES[game], (cfg & 4) ? " palas pequenas" : "", (cfg & 8) ? " bola rapida" : "", (cfg & 16) ? " angulos amplios" : "", z80_role ? "invitado" : "creador");
  printf("== %s\n", title);
  reboot();
  CHECK(wait_text("1 = NEW GAME", 20000), "arranca y llega al menu");
  std::string base;
  Peer pe; pe.role = z80_role ? 0 : 1; pe.config(cfg);
  if (z80_role == 0) {
    menu_new(cfg);
    CHECK(wait_text("ROOM CODE:", 30000), "partida nueva: sale el codigo de sala");
    std::string code = code_on_screen(); base = topic_base(code);
    CHECK(code[0] == ALPHA[cfg], "la primera letra del codigo es la configuracion (juego y opciones)");
    peer_publish(base + "/p1", "1", true);
    pe.topic = base + "/1";
  } else {
    std::string code = std::string(1, ALPHA[cfg]) + "NPQRSTUVW"; base = topic_base(code);
    peer_publish(base + "/p0", "1", true);
    press('2');
    CHECK(wait_text("TYPE THE ROOM CODE", 3000), "unirse: pide el codigo");
    type(code.c_str());
    press('\r');
    pe.topic = base + "/0";
  }
  CHECK(wait_gfx(40000), "empieza el juego (pantalla grafica)");
  run_ms(300);
  CHECK(V("P_ROLE") == z80_role, "mi papel");
  CHECK(V("P_GAME") == game && V("P_BATH") == ((cfg & 4) ? 16 : 32), "el Z80 lee el juego y las palas de la primera letra del codigo");
  CHECK(V("P_SPD0") == ((cfg & 8) ? 4 : 3), "y la velocidad de la bola");
  {
    int bats = (game == 1 ? 2 : 1) * 2, per = V("P_BATH") / 8, on = 0;
    for (int i = 0; i < 16; i++) if (sprs[i].en) on++;
    CHECK(on == bats * per, "las palas son las que toca (jugadores x palas x trozos de sprite)");
    CHECK(sprs[16].en && sprs[16].x - 32 + 4 == 128 && sprs[16].y - 32 + 4 == 96, "la bola quieta en el centro: su centro en (128, 96)");
  }
  Peer* P = &pe;
  pe.begin_serve(0);
  int tick_n = 0, z80_track = 1; bool peer_restart_pending = false; uint32_t over_since = 0;
  size_t si = 0;
  std::string topic_in = base + (z80_role ? "/1" : "/0");
  on_tick = [&]() {
    tick_n++;
    for (; si < seen.size(); si++) if (seen[si].kind == "PUBLISH" && seen[si].topic == topic_in) P->recv(seen[si].data);
    z80_keys(z80_track);
    if (tick_n % 2 == 0) {
      if (z80_role == 1) {                                              // el rival es el creador: el decide la nueva partida
        if ((P->flags & 2) && !peer_restart_pending) { peer_restart_pending = true; over_since = mock_now; }
        if (peer_restart_pending && mock_now > over_since + 1500) { peer_restart_pending = false; P->restart(); }
      }
      P->tick(mock_now);
    }
    P->flush(mock_now);
  };
  // 1. los dos siguen la bola: golpes de los dos y marcadores iguales
  if (getenv("SPWATCH")) {
    int pv = SV_("P_VX"), shown = 0;
    for (int k = 0; k < 80000 && shown < 14; k++) {
      long target = instr_total + 40;
      while (instr_total < target) { z80_do_opcode(); instr_total++; if ((instr_total & 255) == 0) { mock_now = 100000 + time_extra + (uint32_t)(instr_total * 5 / 3250); net_bridge_loop(); } int nv = SV_("P_VX"); if (nv != pv) { printf("  VX %d -> %d en PC=%04X (t=%u ms)\n", pv, nv, z80.pc.w, mock_now - 100000); pv = nv; shown++; } }
      if (on_tick && k % 250 == 0) on_tick();
    }
  }
  if (getenv("SPDBG")) for (int k = 0; k < 60; k++) {
    run_ms(100);
    printf("  t=%4d Z80 b=(%3d,%3d) v=(%2d,%2d) turn=%d fl=%d hc=%d phc=%d my=%d | rival b=(%3d,%3d) v=(%2d,%2d) turn=%d fl=%d hc=%d phc=%d my=%d own=%d\n", k * 100, V("P_BX"), V("P_BY"), SV_("P_VX"), SV_("P_VY"), V("P_TURN"), V("P_FLAGS"), V("P_HC"), V("P_PHC"), V("P_MY"),
           pe.bx, pe.by, pe.vx, pe.vy, pe.turn, pe.flags, pe.hc, pe.phc, pe.my, (int)pe.is_owner());
  }
  run_ms(1500);
  CHECK(V("P_RXOK") == 1 && !shows("WAITING FOR RIVAL"), "ya ha llegado algo del rival");
  uint32_t t_end = mock_now + 25000; int hz = 0, hp = 0; int lz = V("P_HC"), lp = pe.hc;
  while (mock_now < t_end && !(V("P_FLAGS") & 2)) {
    run_ms(20);
    if (V("P_HC") != lz) { lz = V("P_HC"); hz++; }
    if (pe.hc != lp) { lp = pe.hc; hp++; }
  }
  printf("  25 s con los dos siguiendo la bola: eventos del Z80 %d, del rival %d; marcador Z80 %d-%d, rival %d-%d; paredes de hockey del rival %d\n", hz, hp, V("P_SL"), V("P_SR"), pe.sl, pe.sr, pe.n_wall);
  CHECK(hz >= 3 && hp >= 3, "los dos devuelven la bola varias veces (cada uno decide la suya)");
  run_ms(300);
  CHECK(V("P_SL") == pe.sl && V("P_SR") == pe.sr, "los marcadores coinciden");
  // 2. el Z80 se aleja de la bola: el rival marca hasta 15 (o, en Squash, los dos se turnan; falla el Z80 en su turno)
  z80_track = 2;
  {
    bool over = false;
    int limit = game == 1 ? 400 : 9000;             // en Hockey los goles dependen de la punteria (el hueco de la porteria es estrecho): no se espera al 15
    for (int k = 0; k < limit && !over; k++) { run_ms(100); if (V("P_FLAGS") & 2) over = true; }
    printf("  con el Z80 apartado: marcador Z80 %d-%d, rival %d-%d (el rival: %d golpes, %d fallos, %d paredes)\n", V("P_SL"), V("P_SR"), pe.sl, pe.sr, pe.n_hit, pe.n_missed, pe.n_wall);
    if (!over && game == 1) {
      run_ms(300);
      CHECK(V("P_SL") == pe.sl && V("P_SR") == pe.sr, "Hockey: los marcadores coinciden");
      on_tick = [&]() { z80_keys(0); };
      run_ms(3500);
      CHECK(shows("RIVAL NOT ANSWERING"), "sin oir al rival, el aviso lo dice");
      hold('X', 200);
      CHECK(wait_text("NETWORK SPORTS", 30000) || wait_text("1 = NEW GAME", 30000), "X: vuelvo al menu");
      return;
    }
    CHECK(over, "la partida acaba cuando uno llega a 15");
    CHECK(V("P_SL") == pe.sl && V("P_SR") == pe.sr && (V("P_SL") == WIN || V("P_SR") == WIN), "marcadores iguales y uno a 15");
    run_ms(300);
    CHECK((pe.flags & 2) != 0, "el rival tambien lo sabe");
    if (z80_role == 0) {
      CHECK(shows("ENTER=AGAIN"), "el aviso dice ENTER = otra partida");
      z80_track = 1;
      hold('\r', 150);
      run_ms(300);
      CHECK(V("P_SL") == 0 && V("P_SR") == 0 && !(V("P_FLAGS") & 2) && (V("P_FLAGS") & 1), "ENTER: otra partida, a cero y sacando");
      CHECK(pe.sl == 0 && pe.sr == 0 && !(pe.flags & 2) && pe.gen == V("P_GEN"), "el rival la sigue (otro numero de partida)");
    } else {
      CHECK(shows("WAIT HOST"), "el invitado espera al creador");
      int gen0 = V("P_GEN");
      run_ms(3500);
      CHECK(V("P_GEN") != gen0 && V("P_SL") == 0 && V("P_SR") == 0 && !(V("P_FLAGS") & 2), "el creador empieza otra partida y el invitado la sigue");
      z80_track = 1;
    }
    run_ms(2500);
    CHECK(V("P_VX") != 0 || (V("P_FLAGS") & 1) == 0, "y tras el saque la bola se mueve");
  }
  // 3. el rival se calla
  on_tick = [&]() { z80_keys(0); };
  run_ms(3500);
  CHECK(shows("RIVAL NOT ANSWERING"), "sin oir al rival, el aviso lo dice");
  hold('X', 200);
  CHECK(wait_text("NETWORK SPORTS", 30000) || wait_text("1 = NEW GAME", 30000), "X: vuelvo al menu");
  { bool off = true; for (int i = 0; i < 17; i++) if (sprs[i].en) off = false; CHECK(off, "y los sprites se apagan"); }
}

// Practice: solo, sin red
static void practice(int cfg) {
  printf("== PRACTICE%s%s%s\n", (cfg & 4) ? " palas pequenas" : "", (cfg & 8) ? " bola rapida" : "", (cfg & 16) ? " angulos amplios" : "");
  reboot();
  CHECK(wait_text("1 = NEW GAME", 20000), "arranca y llega al menu");
  menu_new(3 | (cfg & 28));
  CHECK(wait_gfx(10000), "Practice empieza sin sala (pantalla grafica)");
  run_ms(300);
  CHECK(V("P_GAME") == 3 && V("P_LOCAL") == 1 && V("P_ROLE") == 0, "Practice: solo, sin red");
  CHECK(shows("HITS") && shows("BEST"), "las etiquetas son HITS y BEST");
  int on = 0; for (int i = 0; i < 16; i++) if (sprs[i].en) on++;
  CHECK(on == V("P_BATH") / 8, "una sola pala");
  int tr = 1;
  on_tick = [&]() { z80_keys(tr); };
  { int hc0 = V("P_HC"); for (int k = 0; k < 100 && V("P_HC") == hc0; k++) run_ms(50);
    CHECK(V("P_HC") != hc0 && !(V("P_FLAGS") & 1) && SV_("P_VX") > 0, "tras el saque la bola va hacia la pared de la derecha"); }
  // la pared la devuelve y la pala la recoge: sube la racha
  run_ms(20000);
  int streak = V("P_SL"), best = V("P_SR");
  printf("  20 s siguiendo la bola: racha %d, mejor %d\n", streak, best);
  CHECK(streak >= 5 || best >= 5, "la pala devuelve la bola contra la pared varias veces seguidas");
  CHECK(!shows("WAITING"), "sin avisos de red");
  tr = 2;                                          // se aparta: falla, la racha acaba y queda como mejor
  int b0 = V("P_SR"); bool failed = false;
  for (int k = 0; k < 400 && !failed; k++) { run_ms(100); if (V("P_SL") == 0 && V("P_SR") >= std::max(streak, b0) && V("P_SR") > 0) failed = true; }
  CHECK(failed, "al fallar, la racha vuelve a 0 y el mejor la recoge");
  printf("  tras fallar: racha %d, mejor %d\n", V("P_SL"), V("P_SR"));
  hold('X', 200);
  CHECK(wait_text("1 = NEW GAME", 30000), "X: vuelvo al menu");
}

int main(int argc, char** argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);
  if (argc < 3) { printf("uso: test_z80sports sports.bin sports.sym [juego]\n"); return 2; }
  FILE* f = fopen(argv[1], "rb");
  if (!f) { printf("no encuentro %s\n", argv[1]); return 2; }
  memset(mem, 0, sizeof(mem));
  size_t sz = fread(mem + 0x4400, 1, 0x3C00, f); fclose(f);
  if (0x4400 + sz > 0x8000) { printf("ERROR el programa llega a $%04zX: pasa de $8000 y alli el ZX81 no ejecuta codigo\n", 0x4400 + sz); errors++; }
  printf("sports.bin: %zu bytes en $4400\n", sz);
  bin.assign(mem + 0x4400, mem + 0x4400 + sz);
  load_syms(argv[2]);
  int only = argc > 3 ? atoi(argv[3]) : -1;
  // (juego | palas pequenas 4 | bola rapida 8 | angulos amplios 16, y quien es el Z80)
  struct Sc { int cfg, role; };
  static const Sc list[] = {{0, 0}, {1, 0}, {2, 0}, {1, 1}, {2, 1}, {1 | 4 | 8 | 16, 0}, {0 | 4 | 8, 1}};
  for (size_t i = 0; i < sizeof(list) / sizeof(list[0]); i++) if (only < 0 || only == (int)i) scenario(list[i].cfg, list[i].role);
  if (only < 0 || only == 100) { practice(0); practice(4 | 8 | 16); }
  printf("  instrucciones del Z80: %ld\n", instr_total);
  if (errors) printf("%d ERRORES\n", errors); else printf("TODO OK\n");
  return errors ? 1 : 0;
}
