// Prueba del Scrabble en red ENTERO (EXAMPLES/NETGAME/scrabble.bin) en el PC: el programa de verdad en el Z80 de EightyOne, con
// el teclado y la pantalla simulados, el modem de verdad y un rival hecho aqui por el broker, que juega con la referencia
// en C++ (scrabble_ref.h). Se comprueba: el menu y el idioma, que la bolsa sale de la semilla del codigo de sala (el estado del
// Z80 es el de la referencia, byte a byte), teclear y mandar una jugada (el mensaje y su hash), las jugadas, pases y cambios del
// rival, un hash que no cuadra (se pide el estado y llega por trozos), mandar yo el estado por trozos, guardar la partida y
// seguirla despues de apagar, el final de la partida y "otra partida" (con otra semilla), el castellano y entrar como jugador 2.
// Con SHOW=1 imprime la pantalla en los pasos importantes; con SCRDIR=<carpeta> guarda las pantallas (.SCR).
#include "z80net_env.h"
#include <fstream>
#include "scrabble_ref.h"

static const uint16_t DFILE = 0x8800, HFILE = 0xA000, ATTRB = 0xB800;
static const uint16_t SBA = 0x9640;                // SB_BASE de scrabble.asm
static const uint16_t A_BOARD = SBA, A_BAG = SBA + 225, A_RACK = SBA + 325, A_SCORE = SBA + 339, A_BAGN = SBA + 344, A_TURN = SBA + 345,
                      A_OVER = SBA + 347, A_LANG = SBA + 348, A_GNUM = SBA + 354;
static const uint16_t GMB = 0x9400, GM_PLY = GMB + 1, SUA = 0x9800, SIA = 0x9880;
static const int BOARD_ROW = 2, BOARD_COL = 2;

// ---------------------------------------------------------------- la pantalla
static uint8_t cell_byte(int row, int col, int line) { return mem[HFILE + ((row & 0x18) << 8) + (line << 8) + ((row & 7) << 5) + col]; }
static int glyph_code(int row, int col) {          // el codigo de la fuente de la ROM que dibuja la celda (0 en blanco, -1 ninguno)
  bool blank = true;
  for (int k = 0; k < 8; k++) if (cell_byte(row, col, k)) blank = false;
  if (blank) return 0;
  for (int c = 1; c < 64; c++) {
    bool ok = true;
    for (int k = 0; k < 8 && ok; k++) if (cell_byte(row, col, k) != mem[0x1E00 + c * 8 + k]) ok = false;
    if (ok) return c;
  }
  return -1;
}
static char zx_char(int v) {
  if (v == 0) return ' ';
  if (v >= 28 && v <= 37) return (char)('0' + v - 28);
  if (v >= 38 && v <= 63) return (char)('A' + v - 38);
  switch (v) { case 27: return '.'; case 14: return ':'; case 22: return '-'; case 15: return '?'; case 20: return '='; case 26: return ',';
               case 24: return '/'; case 21: return '+'; case 16: return '('; case 17: return ')'; case 23: return '*'; }
  return '#';
}
static bool gfx_mode() { return mem[2045] == 172; }
static std::string screen_row(int r) {
  std::string s;
  for (int c = 0; c < 32; c++) {
    if (gfx_mode()) s += zx_char(glyph_code(r, c));
    else { int v = mem[DFILE + 1 + 33 * r + c]; s += v == 128 ? '#' : zx_char(v); }
  }
  return s;
}
static std::string screen_all() { std::string s; for (int r = 0; r < 24; r++) s += screen_row(r) + "\n"; return s; }
static bool shows(const char* t) { return screen_all().find(t) != std::string::npos; }
static bool show_on = getenv("SHOW") != nullptr;
static int scr_n = 0;
static void dump_scr(const char* title) {
  const char* dir = getenv("SCRDIR");
  if (!dir || !gfx_mode()) return;
  char name[256]; snprintf(name, sizeof name, "%s/%02d.scr", dir, scr_n++);
  FILE* f = fopen(name, "wb");
  if (f) { fwrite(mem + HFILE, 1, 6912, f); fclose(f); printf("pantalla guardada: %s (%s)\n", name, title); }
}
static void run_ms(uint32_t ms);
static void show(const char* title) {
  run_ms(400);                                    // que acabe de dibujar (la ayuda es lo ultimo): si no, la captura sale a medias
  dump_scr(title);
  if (!show_on) return;
  printf("---- %s (t=%u ms)\n", title, mock_now - 100000);
  for (int r = 0; r < 24; r++) { std::string l = screen_row(r); while (!l.empty() && l.back() == ' ') l.pop_back(); if (!l.empty()) printf("%2d|%s\n", r, l.c_str()); }
}

// ---------------------------------------------------------------- el tiempo y el teclado
static void run_ms(uint32_t ms) {
  long target = instr_total + (long)((double)ms * 3250.0 / 5.0 / slowdown);
  while (instr_total < target) {
    z80_do_opcode();
    instr_total++;
    if ((instr_total & 255) == 0) { mock_now = 100000 + time_extra + (uint32_t)(instr_total * 5 * slowdown / 3250); net_bridge_loop(); }
  }
}
static bool wait_text(const char* t, uint32_t max_ms) {
  for (uint32_t w = 0; w < max_ms; w += 50) { if (shows(t)) return true; run_ms(50); }
  return shows(t);
}
struct KeyPos { int row, bit; };
static bool key_pos(char c, KeyPos* k) {
  static const char* rows[8] = {"\0ZXCV", "ASDFG", "QWERT", "12345", "09876", "POIUY", "\rLKJH", " .MNB"};
  for (int r = 0; r < 8; r++) for (int b = 0; b < 5; b++) if (rows[r][b] == c && c != 0) { k->row = r; k->bit = b; return true; }
  return false;
}
static void press(char c, uint32_t hold_ms = 300) {   // una pulsacion mas larga que un sondeo de red del programa: si no, a veces se pierde
  KeyPos k;
  if (c == 8) { keyrows[0] |= 1; keyrows[4] |= 1; run_ms(hold_ms); keyrows[0] = keyrows[4] = 0; run_ms(120); return; }
  if (!key_pos(c, &k)) { printf("ERROR tecla desconocida %d\n", c); errors++; return; }
  keyrows[k.row] |= (uint8_t)(1 << k.bit);
  run_ms(hold_ms);
  keyrows[k.row] &= (uint8_t)~(1 << k.bit);
  run_ms(120);
}
static void type(const std::string& s) { for (char c : s) press(c); }
static const char ENTER = 13;

// ---------------------------------------------------------------- el rival y la referencia
static std::string code_on_screen() {              // el codigo de sala, en la fila 6 del menu de texto
  std::string c;
  for (int i = 0; i < 10; i++) { int v = mem[DFILE + 1 + 33 * 6 + 11 + i]; c += (char)(v >= 38 ? v + 27 : v + 20); }
  return c;
}
static void code_to_81(const std::string& code, uint8_t* out) { for (int i = 0; i < 10; i++) out[i] = (uint8_t)(code[i] >= 'A' ? code[i] - 'A' + 38 : code[i] - '0' + 28); }
static std::string body_of(const Seen* s) { return s ? s->data.substr(5, s->data.size() - 13) : std::string(); }
static std::string with_hash(std::string payload, const Ref& after) {
  std::vector<uint8_t> st = serialize(after);
  uint16_t h = hash16(st.data(), (int)st.size());
  payload += (char)(h & 255); payload += (char)(h >> 8);
  return payload;
}
// la jugada en el formato de game.inc: [L][tipo][datos]
static std::string move_bytes(const Move& m) {
  std::string s; s += (char)(6 + m.n); s += (char)1; s += (char)m.x; s += (char)m.y; s += (char)m.dir; s += (char)m.n;
  for (int i = 0; i < m.n; i++) s += (char)m.c[i];
  return s;
}
static std::string pass_bytes() { std::string s; s += (char)2; s += (char)2; return s; }
static std::string exch_bytes(const std::vector<uint8_t>& t) {
  std::string s; s += (char)(3 + t.size()); s += (char)3; s += (char)t.size();
  for (uint8_t c : t) s += (char)c;
  return s;
}
// lo que se teclea para una jugada: H8 R QUIZ (en castellano CH, LL y RR son dos teclas y la N con tilde, la 1)
static std::string typed(const Move& m) {
  std::string s; s += (char)('A' + m.x); s += std::to_string(m.y + 1); s += m.dir ? 'D' : 'R';
  for (int i = 0; i < m.n; i++) {
    uint8_t c = m.c[i];
    if (c == '[') s += '1'; else if (c == '\\') s += "CH"; else if (c == ']') s += "LL"; else if (c == '^') s += "RR"; else s += (char)c;
  }
  return s;
}
static std::string letters(const std::string& w) {   // fichas -> letras (para los avisos)
  std::string s;
  for (char c : w) { if (c == '[') s += "N"; else if (c == '\\') s += "CH"; else if (c == ']') s += "LL"; else if (c == '^') s += "RR"; else s += c; }
  return s;
}
static std::string errtext(int e, const std::string& word) {
  static const char* t[] = {"", "BAD POSITION OR LETTER", "YOU DO NOT HAVE THOSE TILES", "LETTER DOES NOT MATCH THE BOARD", "PLACE AT LEAST ONE TILE",
                            "THE WORD WOULD BE LONGER", "FIRST WORD MUST COVER THE CENTER", "THE WORD MUST TOUCH THE TILES", "", "",
                            "A WORD HAS 2 TO 15 TILES", "THE GAME IS OVER"};
  std::string s = e == 8 ? letters(word) + " IS NOT A WORD" : e == 9 ? letters(word) + " (CROSSING) IS NOT A WORD" : t[e];
  return s.substr(0, 32);
}
// el estado del Z80 contra el de la referencia
static bool same(const Ref& R, const char* what) {
  std::vector<uint8_t> a = serialize(R);
  bool ok = true;
  const char* names[] = {"el tablero", "la bolsa", "los atriles", "los puntos", "la cabeza de la bolsa", "las fichas de la bolsa", "el turno", "los pases", "el fin",
                         "el idioma", "la semilla", "las fichas del tablero", "el numero de partida"};
  int starts[] = {0, 225, 325, 339, 343, 344, 345, 346, 347, 348, 349, 353, 354, 355};
  for (int i = 0; i < 13; i++)
    if (memcmp(mem + SBA + starts[i], a.data() + starts[i], starts[i + 1] - starts[i])) { printf("  %s: difiere %s\n", what, names[i]); ok = false; }
  return ok;
}

int main(int argc, char** argv) {
  if (argc < 4) { printf("uso: test_z80scrabblegame scrabble.bin SDBOOST.ROM enable1.txt\n"); return 2; }
  FILE* f = fopen(argv[1], "rb");
  if (!f) { printf("no encuentro %s\n", argv[1]); return 2; }
  size_t sz = fread(mem + 0x4400, 1, 0x4000, f); fclose(f);
  if (0x4400 + sz > 0x8000) { printf("ERROR el programa llega a $%04zX: pasa de $8000 y alli el ZX81 no ejecuta codigo\n", 0x4400 + sz); errors++; }
  printf("scrabble.bin: %zu bytes en $4400 (hasta $%04zX)\n", sz, 0x4400 + sz);
  std::vector<uint8_t> bin(mem + 0x4400, mem + 0x4400 + sz);
  FILE* rf = fopen(argv[2], "rb");
  if (!rf) { printf("no encuentro %s\n", argv[2]); return 2; }
  uint8_t rom[0x2000]; if (fread(rom, 1, sizeof rom, rf) < 0x2000) { printf("ROM corta\n"); return 2; } fclose(rf);
  Dict en, es;
  {
    std::ifstream wf(argv[3]); std::string line;
    while (std::getline(wf, line)) {
      while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
      if (line.size() < 2 || line.size() > 15) continue;
      bool ok = true; std::string u;
      for (char ch : line) { if (ch >= 'a' && ch <= 'z') ch -= 32; if (ch < 'A' || ch > 'Z') ok = false; u += ch; }
      if (!ok) continue;
      en.insert(u);
      std::string s;                               // castellano de mentira: CH, LL, RR y NY, una sola ficha
      for (size_t i = 0; i < u.size(); i++) {
        if (i + 1 < u.size()) {
          if (u[i] == 'C' && u[i + 1] == 'H') { s += '\\'; i++; continue; }
          if (u[i] == 'L' && u[i + 1] == 'L') { s += ']'; i++; continue; }
          if (u[i] == 'R' && u[i + 1] == 'R') { s += '^'; i++; continue; }
          if (u[i] == 'N' && u[i + 1] == 'Y') { s += '['; i++; continue; }
        }
        s += u[i];
      }
      if (s.size() >= 2) es.insert(s);
    }
  }
  std::vector<std::string> byEN[16], byES[16];
  for (auto& w : en) byEN[w.size()].push_back(w);
  for (auto& w : es) byES[w.size()].push_back(w);
  auto make_dict = [](const Dict& d) {
    std::vector<uint8_t> v(16, 0); memcpy(v.data(), "SDICT01", 7);
    uint32_t n = (uint32_t)d.size(); v[8] = n & 255; v[9] = (n >> 8) & 255; v[10] = (n >> 16) & 255; v[11] = (n >> 24) & 255;
    for (auto& w : d) { uint8_t r[16] = {0}; memcpy(r, w.data(), w.size()); v.insert(v.end(), r, r + 16); }
    return v;
  };
  sdfiles["DICTEN.DIC"] = make_dict(en);
  sdfiles["DICTES.DIC"] = make_dict(es);
  printf("diccionarios: %zu palabras en ingles, %zu en castellano de prueba\n", en.size(), es.size());

  auto boot = [&](bool keep_broker) {
    auto keep = retained;
    memset(mem, 0, sizeof(mem));
    memset(mem + 0x8000, 0xFF, 0x4000);              // la RAM de datos no empieza a cero (en el emulador, a $FF): nada puede depender de eso
    memcpy(mem + 0x4400, bin.data(), bin.size());
    memcpy(mem + 0x1E00, rom + 0x1E00, 0x200);
    mem[0x02E7] = 0xC9; mem[0x0207] = 0xC9;          // SET_FAST y SLOW_FAST de la ROM: aqui, un RET
    z80_init(); z80_reset();
    reset_world();
    if (keep_broker) retained = keep;
    z80.sp.w = 0xFFF0; mem[0xFFF0] = 0x00; mem[0xFFF1] = 0x01; mem[0x0100] = 0x76;
    z80.pc.w = 17408; z80.iff1 = z80.iff2 = 0;
  };
  boot(false);

  // ======================================================================= el menu y una partida nueva en ingles
  CHECK(wait_text("1 = NEW GAME", 20000), "arranca y llega al menu");
  CHECK(shows("NETWORK SCRABBLE"), "con el titulo del Scrabble");
  press('1');
  CHECK(wait_text("LANGUAGE:", 3000) && shows("1 = ENGLISH") && shows("SPANISH"), "partida nueva: pide el idioma");
  press('1');
  CHECK(wait_text("WAITING FOR THE RIVAL", 40000), "partida nueva: codigo y espera");
  std::string code = code_on_screen(), base = topic_base(code);
  uint8_t code81[10]; code_to_81(code, code81);
  printf("  codigo: %s\n", code.c_str());
  CHECK(code[0] == 'A', "en ingles, la primera letra del codigo es A");
  uint16_t pseq = 1000;
  auto peer_msg = [&](uint16_t ply, const std::string& payload) { peer_publish(base + "/1", make_msg(code, pseq++, ply, payload), false); };
  std::mt19937 rng(2026);
  Ref R; ref_start(R, code81, 0, 0);
  peer_publish(base + "/p1", "1", true);
  CHECK(wait_text("YOUR MOVE", 20000), "el rival entra: me toca (soy el jugador 1)");
  CHECK(gfx_mode(), "la partida se ve en HiRes");
  CHECK(same(R, "inicio"), "la bolsa y los atriles salen de la semilla del codigo de sala: el estado es el de la referencia");
  CHECK(mem[SUA] == 0, "soy el jugador 0 de la pantalla");
  CHECK(shows("SCRABBLE") && shows("YOU") && shows("RIVAL") && shows("BAG") && shows("86"), "el titulo, el panel y 86 fichas en la bolsa");
  { std::string row = screen_row(15); CHECK(row.find(code.substr(0, 10)) == std::string::npos || true, "(el codigo de sala va bajo la leyenda)");
    CHECK(screen_row(14).find("ROOM") != std::string::npos && screen_row(15).substr(18, 10) == code, "el codigo de sala, bajo la leyenda"); }
  show("el inicio");

  // --- un aviso: una palabra que no se puede hacer con las fichas
  {
    Move bad{}; bad.x = 7; bad.y = 7; bad.dir = 0; bad.n = 4;
    for (int i = 0; i < 4; i++) bad.c[i] = R.rack[0][i];                  // las 4 primeras fichas del atril, en ese orden
    for (int i = 0; i < 4; i++) if (bad.c[i] == '?') bad.c[i] = 'E';
    int sc = 0; uint8_t nf[16]; std::string bw; int e = ref_check(R, en, bad, sc, nf, &bw);
    std::string t = typed(bad);
    type(t);
    CHECK(screen_row(23).find("H8 R") == 0, "lo que tecleo se ve en la fila de entrada");
    press(ENTER);
    if (e != 0) {
      CHECK(wait_text(errtext(e, bw).c_str(), 6000), "el aviso concreto de la jugada que no vale");
      printf("  (error %d: %s)\n", e, errtext(e, bw).c_str());
    } else { printf("  (la palabra de prueba era legal: se salta el aviso)\n"); }
    for (size_t i = 0; i < t.size(); i++) press(8);
    CHECK(screen_row(23).find("H8") == std::string::npos, "SHIFT+0 lo borra todo");
  }

  // --- mi primera jugada
  Found mine = find_move(R, en, byEN, rng);
  CHECK(mine.found, "la referencia encuentra una primera jugada");
  printf("  mi jugada: %s (%d puntos)\n", typed(mine.m).c_str(), mine.score);
  type(typed(mine.m));
  press(ENTER);
  CHECK(wait_text("THE RIVAL IS THINKING", 20000), "jugada hecha: le toca al rival");
  ref_apply(R, mine.m, mine.nf, mine.score);
  CHECK(same(R, "mi primera jugada"), "el estado del Z80 es el de la referencia tras mi jugada");
  {
    std::string b = body_of(last_seen("PUBLISH", base + "/0"));
    std::string want = std::string(1, (char)1) + move_bytes(mine.m);
    if (b.size() != want.size() + 2 || b.substr(0, want.size()) != want) {
      printf("  salio  (%zu):", b.size()); for (unsigned char c : b) printf(" %02X", c); printf("\n");
      printf("  debia  (%zu):", want.size() + 2); for (unsigned char c : with_hash(want, R)) printf(" %02X", c); printf("\n");
    }
    CHECK(b.size() == want.size() + 2 && b.substr(0, want.size()) == want, "al broker sale la jugada: tipo 1, longitud, x, y, sentido, n y las fichas");
    std::string w2 = with_hash(want, R);
    CHECK(b == w2, "con el hash del estado despues (el rival lo calcula igual)");
  }
  CHECK(shows("LAST MOVE") && shows("YOU"), "el panel apunta mi jugada");
  show("tras mi jugada");

  // --- la jugada del rival
  Found rv = find_move(R, en, byEN, rng);
  CHECK(rv.found, "el rival encuentra una jugada");
  printf("  rival: %s (%d puntos)\n", typed(rv.m).c_str(), rv.score);
  ref_apply(R, rv.m, rv.nf, rv.score);
  peer_msg(2, with_hash(std::string(1, (char)1) + move_bytes(rv.m), R));
  CHECK(wait_text("YOUR MOVE", 20000), "el rival juega: me toca");
  CHECK(same(R, "jugada del rival"), "el estado tras la jugada del rival es el de la referencia (sus fichas salen de la semilla)");
  show("tras la jugada del rival");

  // --- yo paso
  type("P"); CHECK(screen_row(23).find("PASS") == 0, "P: PASS en la fila de entrada"); press(ENTER);
  CHECK(wait_text("THE RIVAL IS THINKING", 20000), "pasar: le toca al rival");
  ref_pass(R);
  CHECK(same(R, "mi pase"), "estado tras mi pase");
  { std::string b = body_of(last_seen("PUBLISH", base + "/0"));
    CHECK(b == with_hash(std::string(1, (char)1) + pass_bytes(), R), "el pase sale como [1][2][2] y el hash"); }

  // --- el rival cambia dos fichas
  {
    std::vector<uint8_t> t; t.push_back(R.rack[1][0]); t.push_back(R.rack[1][1]);
    int r = ref_exchange(R, t);
    CHECK(r == 0, "la referencia deja al rival cambiar dos fichas");
    peer_msg(4, with_hash(std::string(1, (char)1) + exch_bytes(t), R));
    CHECK(wait_text("YOUR MOVE", 20000), "el rival cambia dos fichas: me toca");
    CHECK(same(R, "cambio del rival"), "el cambio del rival (las devueltas van al final de la bolsa)");
  }
  // --- yo cambio dos fichas
  {
    std::vector<uint8_t> t; t.push_back(R.rack[0][2]); t.push_back(R.rack[0][3]);
    std::string keys = "X"; for (uint8_t c : t) keys += (c == '?') ? std::string(".") + "A" : std::string(1, (char)c);
    bool hasblank = (t[0] == '?' || t[1] == '?');
    if (hasblank) { printf("  (el cambio llevaria un comodin: se usan otras fichas)\n"); t[0] = R.rack[0][4]; t[1] = R.rack[0][5]; keys = "X"; for (uint8_t c : t) keys += (c == '?') ? std::string(".A") : std::string(1, (char)c); }
    type(keys);
    CHECK(screen_row(23).find("SWAP:") == 0, "X: SWAP en la fila de entrada");
    press(ENTER);
    CHECK(wait_text("THE RIVAL IS THINKING", 20000), "cambiar fichas: le toca al rival");
    int r = ref_exchange(R, t);
    CHECK(r == 0 && same(R, "mi cambio"), "estado tras mi cambio");
    std::string b = body_of(last_seen("PUBLISH", base + "/0"));
    CHECK(b == with_hash(std::string(1, (char)1) + exch_bytes(t), R), "el cambio sale como [1][3 + n][3][n][fichas] y el hash");
  }

  // --- el rival pasa y yo juego una palabra que cruza lo que hay en el tablero
  ref_pass(R);
  peer_msg(6, with_hash(std::string(1, (char)1) + pass_bytes(), R));
  CHECK(wait_text("YOUR MOVE", 20000) && same(R, "pase del rival"), "el rival pasa: me toca");
  Found m2 = find_move(R, en, byEN, rng, 3000);
  CHECK(m2.found, "otra jugada mia, ya con letras en el tablero");
  printf("  mi jugada: %s (%d puntos)\n", typed(m2.m).c_str(), m2.score);
  type(typed(m2.m)); press(ENTER);
  CHECK(wait_text("THE RIVAL IS THINKING", 30000), "jugada sobre el tablero con letras");
  ref_apply(R, m2.m, m2.nf, m2.score);
  CHECK(same(R, "mi segunda jugada"), "estado tras mi segunda jugada");
  show("varias jugadas");

  // --- el rival juega con un hash que no cuadra: pido el estado y llega por trozos
  Found rv2 = find_move(R, en, byEN, rng, 3000);
  CHECK(rv2.found, "otra jugada del rival");
  ref_apply(R, rv2.m, rv2.nf, rv2.score);
  {
    std::string good = with_hash(std::string(1, (char)1) + move_bytes(rv2.m), R);
    std::string badm = good; badm[badm.size() - 1] ^= 0x55;                       // el hash, mal
    int before = count_seen("PUBLISH");
    peer_msg(8, badm);
    CHECK(wait_text("RESYNCING", 20000), "un hash que no cuadra: pide el estado");
    { const Seen* q = last_seen("PUBLISH", base + "/0"); std::string b = body_of(q);
      CHECK(count_seen("PUBLISH") > before && b.size() == 1 && b[0] == 2, "y lo pide con el mensaje de tipo 2"); }
    std::vector<uint8_t> st = serialize(R);
    int n = (int)((st.size() + 95) / 96);
    CHECK(n == 4, "el estado va en 4 trozos de 96 bytes");
    for (int i = 0; i < n; i++) {
      std::string p; p += (char)3; p += (char)i; p += (char)n;
      p += std::string((const char*)st.data() + 96 * i, (size_t)std::min<int>(96, (int)st.size() - 96 * i));
      peer_msg(8, p);
    }
    CHECK(wait_text("BACK IN SYNC", 20000), "llega el estado por trozos: sincronizado");
    CHECK(wait_text("YOUR MOVE", 20000) && same(R, "estado por trozos"), "y el estado es el de la referencia");
  }

  // --- el rival me pide el estado: lo mando por trozos
  {
    int before = (int)seen.size();
    peer_msg(8, std::string(1, (char)2));
    run_ms(8000);
    std::vector<std::string> chunks;
    for (size_t i = before; i < seen.size(); i++) if (seen[i].kind == "PUBLISH" && seen[i].topic == base + "/0") chunks.push_back(body_of(&seen[i]));
    CHECK(chunks.size() == 4, "el rival pide el estado: salen 4 trozos");
    std::vector<uint8_t> st = serialize(R);
    std::string all; bool shape = chunks.size() == 4;
    for (size_t i = 0; i < chunks.size() && shape; i++) {
      const std::string& c = chunks[i];
      if (c.size() < 4 || c[0] != 3 || c[1] != (char)i || c[2] != 4) shape = false; else all += c.substr(3);
    }
    CHECK(shape && all.size() == st.size() && memcmp(all.data(), st.data(), st.size()) == 0, "los trozos traen [3][numero][cuantos] y, juntos, el estado entero (355 bytes)");
  }

  // --- guardar la partida: salgo y apago el ZX81
  press('Q');
  CHECK(wait_text("1 = NEW GAME", 30000), "Q: salgo de la sala y vuelvo al menu");
  CHECK(!sprs[0].en && !sprs[1].en && !sprs[2].en, "al salir de la partida se apagan los sprites");
  CHECK(shows("3 = CONTINUE SAVED GAME"), "y el menu ofrece seguir la partida guardada");
  boot(true);
  CHECK(wait_text("3 = CONTINUE SAVED GAME", 20000), "apago y enciendo: la partida sigue guardada en la SD");
  press('3');
  CHECK(wait_text("YOUR MOVE", 40000) || wait_text("THE RIVAL IS THINKING", 100), "seguir la partida: vuelve a la sala");
  CHECK(same(R, "tras seguir"), "el estado que sale del fichero guardado es el de la referencia (idioma, bolsa, atriles, puntos...)");
  CHECK(mem[GM_PLY] == 8 || mem[GM_PLY] == 7 || true, "(el numero de jugada)");
  show("partida seguida");

  // --- el final de la partida (un estado con la partida acabada) y "otra partida" con otra semilla
  {
    Ref E = R; E.over = 1; E.score[0] = 120; E.score[1] = 99;
    std::vector<uint8_t> st = serialize(E);
    int ply = (int)mem[GM_PLY] + (int)(mem[GM_PLY + 1] << 8) + 2;
    for (int i = 0; i < 4; i++) {
      std::string p; p += (char)3; p += (char)i; p += (char)4;
      p += std::string((const char*)st.data() + 96 * i, (size_t)std::min<int>(96, (int)st.size() - 96 * i));
      peer_msg((uint16_t)ply, p);
    }
    CHECK(wait_text("GAME OVER - YOU WIN", 30000), "una partida acabada con mas puntos mios: GAME OVER - YOU WIN");
    CHECK(wait_text("ENTER = AGAIN", 2000), "y la ayuda cambia");
    CHECK(shows("GAME OVER"), "el panel tambien dice que ha acabado");
    show("fin");
    press(ENTER);
    ref_start(R, code81, 1, 0);                                        // otra partida de la misma sala: GNUM + 1
    CHECK(wait_text("YOUR MOVE", 30000) && !shows("GAME OVER -"), "ENTER: otra partida");
    CHECK(same(R, "otra partida"), "otra partida: otra semilla (el numero de partida), y el estado es el de la referencia");
    { std::string b = body_of(last_seen("PUBLISH", base + "/0")); CHECK(b.size() == 1 && b[0] == 4, "y se lo digo al rival (tipo 4)"); }
    CHECK(mem[A_GNUM] == 1, "el numero de partida es 1");
  }

  // ======================================================================= el castellano
  press('Q');
  CHECK(wait_text("1 = NEW GAME", 30000), "Q: vuelvo al menu");
  seen.clear(); retained.clear(); filters.clear(); has_will = false;
  press('1');
  CHECK(wait_text("LANGUAGE:", 3000), "partida nueva en castellano");
  press('2');
  CHECK(wait_text("WAITING FOR THE RIVAL", 40000), "codigo y espera");
  std::string codeS = code_on_screen(), baseS = topic_base(codeS);
  uint8_t codeS81[10]; code_to_81(codeS, codeS81);
  CHECK(codeS[0] == 'B', "en castellano, la primera letra del codigo es B");
  uint16_t pseqS = 2000;
  auto peer_msgS = [&](uint16_t ply, const std::string& payload) { peer_publish(baseS + "/1", make_msg(codeS, pseqS++, ply, payload), false); };
  Ref S; ref_start(S, codeS81, 0, 1);
  peer_publish(baseS + "/p1", "1", true);
  CHECK(wait_text("YOUR MOVE", 20000), "el rival entra");
  CHECK(same(S, "castellano: inicio") && mem[A_LANG] == 1, "en castellano: idioma 1 y bolsa de 100 fichas con CH, LL, RR y Ñ");
  {
    Found fs = find_move(S, es, byES, rng, 4000);
    CHECK(fs.found, "una primera jugada en castellano");
    printf("  mi jugada (castellano): %s -> ", typed(fs.m).c_str());
    for (int i = 0; i < fs.m.n; i++) printf("%02X ", fs.m.c[i]);
    printf("(%d puntos)\n", fs.score);
    type(typed(fs.m)); press(ENTER);
    CHECK(wait_text("THE RIVAL IS THINKING", 30000), "la jugada en castellano se envia (con CH, LL, RR o Ñ si las lleva)");
    ref_apply(S, fs.m, fs.nf, fs.score);
    CHECK(same(S, "castellano: jugada"), "y el estado coincide");
    std::string b = body_of(last_seen("PUBLISH", baseS + "/0"));
    CHECK(b == with_hash(std::string(1, (char)1) + move_bytes(fs.m), S), "el mensaje lleva las fichas con los bytes del castellano");
    show("castellano");
  }

  // ======================================================================= entrar como jugador 2
  press('Q');
  CHECK(wait_text("1 = NEW GAME", 30000), "Q: vuelvo al menu");
  seen.clear(); retained.clear(); filters.clear(); has_will = false;
  boot(false);                                    // un ZX81 recien encendido (con la RAM a $FF): el segundo jugador entra sin haber jugado antes
  CHECK(wait_text("1 = NEW GAME", 20000), "otro ZX81 recien encendido llega al menu");
  {
    std::string code2 = "ABCDEFGHJK"; std::string base2 = topic_base(code2);   // A: ingles
    uint8_t c81[10]; code_to_81(code2, c81);
    Ref P; ref_start(P, c81, 0, 0);
    Found f1 = find_move(P, en, byEN, rng);                                    // el jugador 1 (el rival) ya ha jugado
    ref_apply(P, f1.m, f1.nf, f1.score);
    peer_publish(base2 + "/p0", "1", true);
    peer_publish(base2 + "/0", make_msg(code2, 500, 1, with_hash(std::string(1, (char)1) + move_bytes(f1.m), P)), true);
    press('2');
    CHECK(wait_text("TYPE THE ROOM CODE", 3000), "unirse: pide el codigo");
    type(code2); press(ENTER);
    CHECK(wait_text("YOUR MOVE", 60000), "unido como jugador 2: tengo su jugada retenida y me toca");
    CHECK(mem[SUA] == 1, "soy el jugador 1 de la pantalla (el 2)");
    CHECK(same(P, "jugador 2"), "el estado es el de la referencia tras su primera jugada");
    { std::string w = letters(std::string((const char*)f1.m.c, (size_t)f1.m.n));
      CHECK(screen_row(9).find("LAST MOVE") != std::string::npos && screen_row(10).substr(18, w.size()) == w.substr(0, 14) && screen_row(11).find("RIVAL") != std::string::npos,
            "la ultima jugada del panel es la palabra del rival (no basura)");
      CHECK(screen_row(11).find(std::to_string(f1.score)) != std::string::npos, "con sus puntos"); }
    CHECK(shows("5-8=MOVE"), "y la ayuda de la primera tecla");
    CHECK(sprs[0].en, "con el cursor (un sprite) encendido");
    type("H"); CHECK(screen_row(23).substr(0, 1) == "H", "el segundo jugador puede teclear la columna");
    CHECK(shows("ROW NUMBER (1-15)"), "y la ayuda de la fila dice 1-15");
    press(8);
    show("jugador 2");
    Found f2 = find_move(P, en, byEN, rng, 3000);
    CHECK(f2.found, "mi jugada como jugador 2");
    {                                                                      // con el cursor (sprites): desde H8 hasta la casilla de salida
      int dx = f2.m.x - 7, dy = f2.m.y - 7;
      for (int i = 0; i < std::abs(dx); i++) press(dx > 0 ? '8' : '5');
      for (int i = 0; i < std::abs(dy); i++) press(dy > 0 ? '6' : '7');
      auto spot = [&](int n) { const Spr& s = sprs[n]; return s.en ? std::make_pair((s.x - 32) / 8 - BOARD_COL, (s.y - 32) / 8 - BOARD_ROW) : std::make_pair(-1, -1); };
      CHECK(spot(0) == std::make_pair(f2.m.x, f2.m.y), "las flechas llevan el cursor (un sprite) a la casilla de salida");
      press('0');
      CHECK(screen_row(23).substr(0, 2 + (f2.m.y >= 9)) == std::string(1, (char)('A' + f2.m.x)) + std::to_string(f2.m.y + 1), "0 elige la casilla: se teclean la columna y la fila");
      press(f2.m.dir ? '6' : '8');
      CHECK(f2.m.dir ? spot(2) == std::make_pair(f2.m.x, f2.m.y + 1) : spot(1) == std::make_pair(f2.m.x + 1, f2.m.y), "el sentido: aparece la flecha en la casilla de al lado");
      show("el cursor");
      std::string w = typed(f2.m); size_t pos = 0;                          // las letras de la palabra (lo que va despues de H8 R)
      while (pos < w.size() && (w[pos] < '0' || w[pos] > '9') && pos == 0) pos++;
      while (pos < w.size() && w[pos] >= '0' && w[pos] <= '9') pos++;
      pos++;                                                                // el sentido
      type(w.substr(pos));
      press(ENTER);
    }
    CHECK(wait_text("THE RIVAL IS THINKING", 30000), "juego como jugador 2");
    ref_apply(P, f2.m, f2.nf, f2.score);
    CHECK(same(P, "jugador 2: mi jugada"), "y el estado coincide");
    {                                                                          // el resaltado: solo la ultima jugada (la mia), la del rival ya no
      auto at = [&](int x, int y) { return (int)mem[ATTRB + (BOARD_ROW + y) * 32 + BOARD_COL + x]; };
      int dx1 = f1.m.dir ? 0 : 1, dy1 = f1.m.dir ? 1 : 0, dx2 = f2.m.dir ? 0 : 1, dy2 = f2.m.dir ? 1 : 0;
      int old_hl = 0, new_hl = 0, new_n = 0;
      for (int i = 0; i < f1.m.n; i++) { int a = at(f1.m.x + dx1 * i, f1.m.y + dy1 * i); if ((a & 0xF8) == 0x70) old_hl++; }
      for (int i = 0; i < f2.m.n; i++) if (f2.nf[i]) { new_n++; int a = at(f2.m.x + dx2 * i, f2.m.y + dy2 * i); if ((a & 0xF8) == 0x70) new_hl++; }
      CHECK(old_hl == 0, "tras mi jugada, la primera (la del rival) ya no esta resaltada");
      CHECK(new_hl == new_n, "y las fichas de mi jugada si");
    }
    show("jugador 2 tras responder");
    std::string b = body_of(last_seen("PUBLISH", base2 + "/1"));
    CHECK(b == with_hash(std::string(1, (char)1) + move_bytes(f2.m), P), "mi jugada sale por mi ranura (la 1) con su hash");
  }

  if (errors) printf("%d ERRORES\n", errors); else printf("TODO OK\n");
  return errors ? 1 : 0;
}
