// Prueba del Pong en red (EXAMPLES/NETGAME/pong.bin, la bola es de quien la recibe) en el PC: el programa de verdad en el Z80 de EightyOne, con el teclado del ZX81 de mentira
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
  memset(mem, 0, sizeof(mem));
  memcpy(mem + 0x6000, bin.data(), bin.size());
  install_font();
  mem[0x02E7] = 0xC9; mem[0x0207] = 0xC9;
  z80_init(); z80_reset();
  reset_world();
  on_tick = nullptr;
  z80.sp.w = 0xFFF0; mem[0xFFF0] = 0x00; mem[0xFFF1] = 0x01; mem[0x0100] = 0x76;
  z80.pc.w = 24576; z80.iff1 = z80.iff2 = 0;
}

// un estado del creador, tal como sale al broker: [1][fr][bx][by][vx][vy][pala][tantos izq][tantos der][flags][sello del invitado][edad]
// ------------------------------------------------------------ el rival: una copia en C++ de las reglas del Pong (la bola es de quien la recibe)
static const int WIN = 15;
static const bool SERVE_TO_LOSER = true;
struct Peer {
  int role = 1;
  int fr = 0, my = 80, oy = 80, oyt = 80, sl = 0, sr = 0, flags = 0, serve = 50, ping = 2, last = 0, hits = 0, hc = 0, phc = 0, gen = 0, srv = 0, pend = 0, sim = 0, rxf = 0;
  int bx = 124, by = 92, vx = 0, vy = 0;
  bool rxok = false;
  int mode = 0;                       // 0 sigue la bola con la pala, 1 la deja quieta (my fijo)
  int n_hit = 0, n_missed = 0, n_launch = 0;
  std::string topic;
  std::deque<std::pair<uint32_t, std::string>> dq; int jit_max = 0; std::mt19937 jr{5};

  bool is_owner() const { if (!vx) return false; int neg = vx < 0; return neg == (role ^ 1); }
  void chk_over() { if (sl >= WIN || sr >= WIN) flags |= 2; else flags &= ~2; }
  void center() { bx = 124; by = 92; vx = 0; vy = 0; }
  void begin_serve(int launcher) { srv = launcher; flags |= 1; center(); hits = 0; pend = 0; serve = 50; }
  void launch() { static const int vyt[4] = {-2, -1, 1, 2}; flags &= ~1; vx = role ? 3 : -3; vy = vyt[fr & 3]; hits = 0; hc++; n_launch++; }
  void restart() { sl = sr = 0; gen = (gen + 1) & 255; flags &= ~2; begin_serve(0); }
  void missed() { if (role == 0) sr++; else sl++; chk_over(); begin_serve(SERVE_TO_LOSER ? role : role ^ 1); n_missed++; }
  bool hit_test(int py) const { return by + 8 > py && by < py + 32; }
  int hit_speed() { hits++; int s = (hits >> 1) + 3; return s >= 7 ? 6 : s; }
  void hit_vy(int c) { int a = (int8_t)(by - c - 12); a >>= 2; if (a < -4) a = -4; vy = a; }
  void set_pend() { if (!sim) pend = 12; }
  void ball_step() {
    int x = bx + vx; if (x < 0) x = 0; if (x > 255) x = 255; bx = x;
    int y = by + vy;
    if (y < 16) { by = 16; vy = -vy; set_pend(); } else if (y >= 169) { by = 168; vy = -vy; set_pend(); } else by = y;
    if (!sim && is_owner()) {
      if (role == 0) {
        if (bx < 5) { missed(); return; }
        if (bx >= 21) return;
        if (!hit_test(my)) return;
        bx = 21; vx = hit_speed();
      } else {
        if (bx >= 244) { missed(); return; }
        if (bx < 228) return;
        if (!hit_test(my)) return;
        bx = 227; vx = -hit_speed();
      }
      hit_vy(my); hc++; pend = 12; n_hit++;
      return;
    }
    if (bx < 4) bx = 4; if (bx > 244) bx = 244;
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
    if ((uint8_t)((uint8_t)m[8] - gen) & 0x80) return;                // de una partida anterior: se ignora
    last = mf; oyt = (uint8_t)m[2];
    int p = (int)(uint8_t)(fr - (uint8_t)m[3]) - (uint8_t)m[4]; ping = p < 0 ? 0 : p; rxf = fr & 255; rxok = true;
    // marcadores
    if ((uint8_t)m[8] != gen) { gen = (uint8_t)m[8]; sl = (uint8_t)m[6]; sr = (uint8_t)m[7]; flags &= ~2; chk_over(); begin_serve(0); }
    else {
      int c = 0;
      if ((uint8_t)m[6] > sl) { sl = (uint8_t)m[6]; c |= 1; }
      if ((uint8_t)m[7] > sr) { sr = (uint8_t)m[7]; c |= 2; }
      if (c) { chk_over(); int loser = (c & 1) ? 1 : 0; if (!SERVE_TO_LOSER) loser ^= 1; begin_serve(loser); }
    }
    // bola
    if (flags & 2) return;
    int bx_m = (uint8_t)m[11], by_m = (uint8_t)m[12], vx_m = (int8_t)m[13], vy_m = (int8_t)m[14];
    if ((uint8_t)m[9] != phc) { phc = (uint8_t)m[9]; hits = (uint8_t)m[10]; bx = bx_m; by = by_m; vx = vx_m; vy = vy_m; flags &= ~1; pend = 0; lead(); return; }
    if (flags & 1) return;
    if (is_owner()) return;
    int lbx = bx, lby = by, lvx = vx, lvy = vy;
    bx = bx_m; by = by_m; vx = vx_m; vy = vy_m; lead();
    if (dis(vx, lvx) || dis(vy, lvy)) {
      if (pend) { bx = lbx; by = lby; vx = lvx; vy = lvy; }
      else pend = 0;
    } else { pend = 0; half(lbx, bx); half(lby, by); }
  }
  void pad() {
    if (mode == 1) return;
    int t = std::max(16, std::min(144, by - 12));
    if (my < t) my = std::min(t, my + 4); else if (my > t) my = std::max(t, my - 4);
  }
  std::string make() const {
    std::string s;
    int age = (uint8_t)(fr - rxf);
    int v[15] = {1, fr & 255, my, last & 255, age, flags, sl, sr, gen, hc & 255, hits, bx, by, vx & 255, vy & 255};
    for (int i = 0; i < 15; i++) s += (char)v[i];
    return s;
  }
  // un fotograma de 20 ms
  void tick(uint32_t now) {
    fr = (fr + 1) & 255;
    // el rival suaviza la pala del otro como el Z80 (no se usa para nada mas)
    if (oy < oyt) oy = std::min(oyt, oy + 6); else if (oy > oyt) oy = std::max(oyt, oy - 6);
    pad(); phys();
    dq.push_back({now + (jit_max ? (uint32_t)(jr() % (jit_max + 1)) : 0u), make()});
  }
  void flush(uint32_t now) { while (!dq.empty() && dq.front().first <= now) { peer_publish(topic, dq.front().second, false); dq.pop_front(); } }
};

// las teclas del Z80 que sigue la bola (autojuego) o no
static void z80_keys(int track) {
  keyrows[2] &= ~1; keyrows[1] &= ~1;             // Q, A
  if (track == 2) { if (V("P_BY") < 92) keyrows[1] |= 1; else keyrows[2] |= 1; return; }      // 2: se aleja de la bola, nunca la alcanza
  if (!track) return;
  int by = V("P_BY"), my = V("P_MY"), c = by + 4 - (my + 16);
  if (c < -3) keyrows[2] |= 1; else if (c > 3) keyrows[1] |= 1;
}

int main(int argc, char** argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);
  if (argc < 3) { printf("uso: test_z80pong pong.bin pong.sym\n"); return 2; }
  FILE* f = fopen(argv[1], "rb");
  if (!f) { printf("no encuentro %s\n", argv[1]); return 2; }
  size_t sz = fread(mem + 0x6000, 1, 0x2000, f); fclose(f);
  if (0x6000 + sz > 0x8000) { printf("ERROR el programa llega a $%04zX: pasa de $8000 y alli el ZX81 no ejecuta codigo\n", 0x6000 + sz); errors++; }
  printf("pong.bin: %zu bytes en $6000\n", sz);
  bin.assign(mem + 0x6000, mem + 0x6000 + sz);
  load_syms(argv[2]);
  reboot();

  // =================================================================== A. el Z80 crea la sala (izquierda): el rival es el invitado (derecha)
  CHECK(wait_text("1 = NEW GAME", 20000), "arranca y llega al menu");
  press('1');
  CHECK(wait_text("ROOM CODE:", 30000), "partida nueva: sale el codigo de sala");
  std::string code = code_on_screen(), base = topic_base(code);
  printf("  sala %s\n", code.c_str());
  peer_publish(base + "/p1", "1", true);
  CHECK(wait_gfx(30000), "el rival entra: empieza el Pong (pantalla grafica)");
  run_ms(300);
  show("Pong: el creador");
  CHECK(shows("PONG") && shows("P1") && shows("P2"), "titulo y las dos etiquetas");
  CHECK(V("P_ROLE") == 0, "soy el creador (izquierda)");
  CHECK(last_seen("SUBSCRIBE", base + "/1") != nullptr && last_seen("UNSUBSCRIBE", base + "/+") != nullptr, "la sala pasa a escuchar solo al rival (nq_open)");
  {
    bool ok = true;
    for (int i = 0; i < 4; i++) { if (!sprs[i].en || sprs[i].x != 12 + 32 || sprs[i].y != V("P_MY") + 8 * i + 32) ok = false; }
    for (int i = 4; i < 8; i++) { if (!sprs[i].en || sprs[i].x != 236 + 32 || sprs[i].y != V("P_OY") + 8 * (i - 4) + 32) ok = false; }
    CHECK(ok, "las dos palas son 4 sprites apilados cada una, en x = 12 y 236");
    CHECK(sprs[8].en, "la bola es el sprite 8");
    CHECK(sprs[0].col[0] == 0xC0 && sprs[4].col[0] == 0xA0, "mi pala es verde y la del rival roja");
  }
  CHECK(V("P_FLAGS") & 1, "empieza sacando (bola quieta en el centro)");
  {
    // la bola quieta en el centro de la pista: su centro (x + 4, y + 4) = (128, 96), entre las palas y las paredes; la red esta en x = 127 y 128
    CHECK(sprs[8].x - 32 + 4 == 128 && sprs[8].y - 32 + 4 == 96, "la bola esta centrada: su centro en (128, 96)");
    bool net = true; for (int row = 2; row < 22; row++) for (int l = 0; l < 4; l++) { if (cell_byte(row, 15, l) != 0x01 || cell_byte(row, 16, l) != 0x80) net = false; }
    CHECK(net, "la red del centro es de 2 puntos de ancho, en x = 127 y 128");
  }
  Peer pe; pe.role = 1; pe.topic = base + "/1"; pe.begin_serve(0);
  int tick_n = 0; int z80_track = 1;
  on_tick = [&]() {
    tick_n++;
    // lo que el Z80 ha mandado, al rival (sin retraso: el broker de mentira ya pasa por el modem)
    static size_t si = 0;
    for (; si < seen.size(); si++) if (seen[si].kind == "PUBLISH" && seen[si].topic == base + "/0") pe.recv(seen[si].data);
    z80_keys(z80_track);
    if (tick_n % 2 == 0) { pe.tick(mock_now); }
    pe.flush(mock_now);
  };
  // medidas del ritmo
  {
    int f0 = V("P_FR"); uint32_t t0 = mock_now; run_ms(1000); int f1 = V("P_FR"); uint32_t t1 = mock_now;
    double ms_per_frame = (double)(t1 - t0) / (uint8_t)(f1 - f0);
    printf("  un fotograma dura %.1f ms en el banco (sin contar la espera del tubo de la placa real)\n", ms_per_frame);
    CHECK(ms_per_frame > 12 && ms_per_frame < 22, "el fotograma esta cerca de 20 ms");
  }
  // 1. el creador saca (a los 50 fotogramas): la bola va hacia el creador y lo recibe el rival
  run_ms(1200);
  CHECK(V("P_RXOK") == 1 && !shows("WAITING FOR RIVAL"), "ya ha llegado algo del rival");
  CHECK(V("P_HC") >= 1, "el creador ha sacado (cuenta como golpe)");
  CHECK(pe.hc == 0 || pe.vx != 0, "el rival ve la bola moverse");
  // 2. los dos siguen la bola con su pala: hay golpes de los dos y los marcadores coinciden
  uint32_t t_end = mock_now + 30000; int hits_z = 0, hits_p = 0; int last_hcz = V("P_HC"), last_hcp = pe.hc;
  while (mock_now < t_end && !(V("P_FLAGS") & 2)) {
    run_ms(20);
    if (V("P_HC") != last_hcz) { last_hcz = V("P_HC"); hits_z++; }
    if (pe.hc != last_hcp) { last_hcp = pe.hc; hits_p++; }
  }
  printf("  30 s con los dos siguiendo la bola: eventos del Z80 %d, del rival %d; marcador Z80 %d-%d, rival %d-%d\n", hits_z, hits_p, V("P_SL"), V("P_SR"), pe.sl, pe.sr);
  CHECK(hits_z >= 3 && hits_p >= 3, "los dos golpean la bola varias veces (cada uno decide el suyo)");
  run_ms(300);
  CHECK(V("P_SL") == pe.sl && V("P_SR") == pe.sr, "los marcadores coinciden");
  // 3. el Z80 se queda quieto: el rival marca hasta 15
  z80_track = 2;
  {
    int sr0 = pe.sr; bool over = false;
    for (int k = 0; k < 6000 && !over; k++) { run_ms(100); if (V("P_FLAGS") & 2) over = true; if (k % 50 == 49) {} }
    printf("  con el Z80 quieto: marcador Z80 %d-%d, rival %d-%d, el rival ha golpeado %d veces, fallado %d\n", V("P_SL"), V("P_SR"), pe.sl, pe.sr, pe.n_hit, pe.n_missed);
    CHECK(over, "la partida acaba cuando uno llega a 15");
    CHECK(V("P_SL") == pe.sl && V("P_SR") == pe.sr, "los marcadores coinciden al acabar");
    CHECK(V("P_SL") == WIN || V("P_SR") == WIN, "uno ha llegado exactamente a 15");
    (void)sr0;
    run_ms(300);
    show("fin de partida");
    CHECK(shows("ENTER=AGAIN"), "el aviso dice ENTER = otra partida");
    CHECK(shows(V("P_SL") == WIN ? "YOU WIN" : "YOU LOSE"), "y si he ganado o perdido");
    CHECK((pe.flags & 2) != 0, "el rival tambien lo sabe");
    int bx = V("P_BX"); run_ms(300); CHECK(V("P_BX") == bx, "la bola esta parada");
    // los marcadores se ven con dos cifras
    { int sc = V("P_SR") >= WIN ? V("P_SR") : V("P_SL"); int col = V("P_SR") >= WIN ? 22 : 7; int tens = glyph_code(0, col) - 28, unit = glyph_code(0, col + 1) - 28;
      CHECK(sc == 15 && tens == 1 && unit == 5, "el 15 se ve con dos cifras"); }
    z80_track = 1;
    hold('\r', 150);
    run_ms(300);
    CHECK(V("P_SL") == 0 && V("P_SR") == 0 && !(V("P_FLAGS") & 2) && (V("P_FLAGS") & 1), "ENTER: otra partida, a cero y sacando");
    CHECK(pe.sl == 0 && pe.sr == 0 && !(pe.flags & 2) && pe.gen == V("P_GEN"), "el rival la sigue (otro numero de partida)");
    { int hc0 = V("P_HC"); for (int k = 0; k < 60 && V("P_HC") == hc0; k++) run_ms(50);
      { char b[160]; snprintf(b, sizeof b, "  tras el ENTER: hc %d (antes %d) flags %d vx %d bx %d srv %d serve %d gen %d", V("P_HC"), hc0, V("P_FLAGS"), SV_("P_VX"), V("P_BX"), V("P_SRV"), V("P_SERVE"), V("P_GEN")); puts(b); }
      CHECK(V("P_HC") != hc0 && !(V("P_FLAGS") & 1) && SV_("P_VX") < 0, "tras 1 s saca el creador: la bola va hacia el"); }
  }
  // el rival deja de contestar
  on_tick = [&]() { z80_keys(false); };
  run_ms(3500);
  CHECK(shows("RIVAL NOT ANSWERING"), "sin oir al rival, el aviso lo dice");
  show("el rival no contesta");
  hold('X', 200);
  CHECK(wait_text("1 = NEW GAME", 30000), "X: vuelvo al menu");
  { bool off = true; for (int i = 0; i < 9; i++) if (sprs[i].en) off = false; CHECK(off, "y los sprites se apagan"); }
  CHECK(count_seen("DISCONNECT") >= 1, "y me despido del broker");

  // =================================================================== B. el Z80 se une (derecha): el rival es el creador (izquierda)
  reboot();
  CHECK(wait_text("1 = NEW GAME", 20000), "B: arranca y llega al menu");
  std::string code2 = "MNPQRSTUVW", base2 = topic_base(code2);
  peer_publish(base2 + "/p0", "1", true);
  press('2');
  CHECK(wait_text("TYPE THE ROOM CODE", 3000), "unirse: pide el codigo");
  type(code2.c_str());
  press('\r');
  CHECK(wait_gfx(40000), "unido: empieza el Pong");
  run_ms(300);
  CHECK(V("P_ROLE") == 1, "soy el invitado (derecha)");
  CHECK(sprs[0].col[0] == 0xA0 && sprs[4].col[0] == 0xC0, "mi pala (la derecha) es la verde y la del creador la roja");
  Peer pc; pc.role = 0; pc.topic = base2 + "/0"; pc.begin_serve(0);
  int tk = 0; int z80_tr = 1; bool peer_restart_pending = false; uint32_t over_since = 0;
  on_tick = [&]() {
    tk++;
    static size_t gi = 0;
    for (; gi < seen.size(); gi++) if (seen[gi].kind == "PUBLISH" && seen[gi].topic == base2 + "/1") pc.recv(seen[gi].data);
    z80_keys(z80_tr);
    if (tk % 2 == 0) {
      if ((pc.flags & 2) && !peer_restart_pending) { peer_restart_pending = true; over_since = mock_now; }
      if (peer_restart_pending && mock_now > over_since + 1500) { peer_restart_pending = false; pc.restart(); }
      pc.tick(mock_now);
    }
    pc.flush(mock_now);
  };
  run_ms(1500);
  CHECK(V("P_RXOK") == 1 && !shows("WAITING FOR RIVAL"), "con el primer mensaje, ya juego");
  CHECK(V("P_OY") == pc.my || std::abs(V("P_OY") - pc.my) <= 12, "la pala del creador esta donde dice");
  CHECK(std::abs(sprs[0].y - (V("P_OY") + 32)) <= 6, "y su sprite tambien");
  {
    int ping = V("P_PING"); printf("  ping medido por el invitado: %d fotogramas\n", ping);
    CHECK(ping >= 0 && ping <= 12, "ping razonable");
    CHECK(!ping_shown(), "el ping no se ve de entrada");
    CHECK(shows("P=PING"), "el aviso de siempre dice la tecla");
    hold('P', 150);
    run_ms(100);
    CHECK(ping_shown(), "con la tecla P se ve el ping");
    hold('P', 150);
    run_ms(100);
    CHECK(!ping_shown() && shows("P=PING"), "y con otra pulsacion se oculta, sin perder el aviso");
  }
  // el creador (rival) saca hacia el: la bola es de quien la recibe; los dos siguen la bola
  {
    uint32_t t_end2 = mock_now + 30000; int hz = 0, hp = 0; int lz = V("P_HC"), lp = pc.hc;
    while (mock_now < t_end2 && !(V("P_FLAGS") & 2)) {
      run_ms(20);
      if (V("P_HC") != lz) { lz = V("P_HC"); hz++; }
      if (pc.hc != lp) { lp = pc.hc; hp++; }
    }
    printf("  invitado, 30 s siguiendo la bola: eventos del Z80 %d, del creador %d; marcador Z80 %d-%d, creador %d-%d\n", hz, hp, V("P_SL"), V("P_SR"), pc.sl, pc.sr);
    CHECK(hz >= 3 && hp >= 3, "los dos golpean la bola varias veces");
    run_ms(300);
    CHECK(V("P_SL") == pc.sl && V("P_SR") == pc.sr, "los marcadores coinciden");
  }
  // con jitter en la red: cada mensaje del rival llega con 0-120 ms de retraso; la bola del invitado no debe retroceder cuando no es suya
  {
    pc.jit_max = 120;
    int viol = 0, samples = 0, last_fr = V("P_FR"), last_x = V("P_BX"), last_vx = SV_("P_VX"); int worst = 0;
    for (int k = 0; k < 3000; k++) {
      run_ms(10);
      int fr = V("P_FR");
      if (fr != last_fr) {
        int x = V("P_BX"), vx = SV_("P_VX");
        bool mine = vx != 0 && ((vx < 0) == (V("P_ROLE") == 0));
        if (!mine && vx == last_vx && vx != 0 && (uint8_t)(fr - last_fr) == 1 && !(V("P_FLAGS") & 3)) {
          int dx = (x - last_x) * (vx > 0 ? 1 : -1);
          samples++; if (dx < -2) { viol++; worst = std::min(worst, dx); }
        }
        last_fr = fr; last_x = x; last_vx = vx;
      }
    }
    printf("  con jitter de 0-120 ms: %d muestras con la bola ajena, %d retrocesos (peor %d px); marcador Z80 %d-%d, creador %d-%d\n", samples, viol, worst, V("P_SL"), V("P_SR"), pc.sl, pc.sr);
    CHECK(samples > 100 && viol <= samples / 20, "con jitter la bola ajena casi nunca retrocede (menos del 5 %)");
    pc.jit_max = 0; pc.dq.clear();
    run_ms(500);
    CHECK(V("P_SL") == pc.sl && V("P_SR") == pc.sr, "y tras el jitter los marcadores siguen coincidiendo");
  }
  // el Z80 se queda quieto: el creador (rival) llega a 15 y es el quien empieza otra partida
  z80_tr = 2;
  {
    bool over = false;
    for (int k = 0; k < 8000 && !over; k++) { run_ms(100); if (V("P_FLAGS") & 2) over = true; }
    CHECK(over, "la partida acaba cuando uno llega a 15");
    CHECK(V("P_SL") == pc.sl && V("P_SR") == pc.sr && (V("P_SL") == WIN || V("P_SR") == WIN), "marcadores iguales, uno a 15");
    run_ms(300);
    show("el invitado, fin");
    CHECK(shows(V("P_SR") == WIN ? "YOU WIN" : "YOU LOSE") && shows("WAIT HOST"), "el invitado sabe quien ha ganado y espera al creador");
    int gen0 = V("P_GEN");
    run_ms(3500);                                  // el creador (esta prueba) empieza otra partida a los 1,5 s
    { char b[200]; snprintf(b, sizeof b, "  tras el fin: Z80 gen %d marcador %d-%d flags %d srv %d; creador gen %d marcador %d-%d flags %d srv %d", V("P_GEN"), V("P_SL"), V("P_SR"), V("P_FLAGS"), V("P_SRV"), pc.gen, pc.sl, pc.sr, pc.flags, pc.srv); puts(b); }
    CHECK(V("P_GEN") != gen0 && V("P_SL") == 0 && V("P_SR") == 0 && !(V("P_FLAGS") & 2), "el creador empieza otra partida y el invitado la sigue");
    CHECK(V("P_SRV") == 0, "saca el creador");
    z80_tr = 1;
    run_ms(2500);
    CHECK(V("P_VX") != 0 && (V("P_FLAGS") & 1) == 0, "y tras su saque la bola se mueve");
  }
  // el creador se calla
  on_tick = [&]() { z80_keys(false); };
  run_ms(3500);
  CHECK(shows("RIVAL NOT ANSWERING"), "el invitado avisa de que el creador no contesta");
  hold('X', 200);
  CHECK(wait_text("1 = NEW GAME", 30000), "X: vuelvo al menu");

  printf("  instrucciones del Z80: %ld\n", instr_total);
  if (errors) printf("%d ERRORES\n", errors); else printf("TODO OK\n");
  return errors ? 1 : 0;
}
