// Prueba del ajedrez en red entero (EXAMPLES/NETGAME/chess.bin) en el PC: el programa de verdad en el
// Z80 de EightyOne, con el teclado y la pantalla simulados, el modem de verdad y un rival hecho aqui por
// el broker. Se juegan jugadas sencillas (sin enroque ni al paso: eso ya lo cubre test_z80chess.cpp con
// perft), un mate del pastor, un jaque, una jugada ilegal, y como jugador 2 con el tablero girado.
// Con SHOW=1 imprime la pantalla en cada paso importante.
#include "z80net_env.h"

static const uint16_t DFILE = 0x8800;
static const int ROW_BOARD = 3, BOARD_COL = 4;

static std::string screen_row(int r) {
  std::string s;
  for (int c = 0; c < 32; c++) {
    int v = mem[DFILE + 1 + 33 * r + c];
    char ch;
    if (v == 0) ch = ' ';
    else if (v >= 28 && v <= 37) ch = (char)('0' + v - 28);
    else if (v >= 38 && v <= 63) ch = (char)('A' + v - 38);
    else if (v >= 166 && v <= 191) ch = (char)('a' + v - 166);            // las negras: letras en inverso
    else if (v == 27) ch = '.'; else if (v == 14) ch = ':'; else if (v == 22) ch = '-';
    else if (v == 15) ch = '?'; else if (v == 20) ch = '='; else if (v == 26) ch = ',';
    else if (v == 128) ch = '#';
    else ch = '~';
    s += ch;
  }
  return s;
}
static std::string screen_all() { std::string s; for (int r = 0; r < 24; r++) s += screen_row(r) + "\n"; return s; }
static bool shows(const char* t) { return screen_all().find(t) != std::string::npos; }
static bool show_on = getenv("SHOW") != nullptr;
static void show(const char* title) {
  if (!show_on) return;
  printf("---- %s (t=%u ms)\n", title, mock_now - 100000);
  for (int r = 0; r < 24; r++) { std::string l = screen_row(r); while (!l.empty() && l.back() == ' ') l.pop_back(); if (!l.empty()) printf("%2d|%s\n", r, l.c_str()); }
}
// la pieza que se ve en la casilla (rank 0-7, file 0-7), con las blancas abajo
static char cell(int rank, int file, bool black_bottom = false) {
  int row = black_bottom ? ROW_BOARD + rank : ROW_BOARD + (7 - rank);
  int f = black_bottom ? 7 - file : file;
  return screen_row(row)[BOARD_COL + 3 * f + 1];
}

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
static void press(char c, uint32_t hold_ms = 120) {
  KeyPos k;
  if (c == 8) { keyrows[0] |= 1; keyrows[4] |= 1; run_ms(hold_ms); keyrows[0] = keyrows[4] = 0; run_ms(120); return; }
  if (!key_pos(c, &k)) { printf("ERROR tecla desconocida %d\n", c); errors++; return; }
  keyrows[k.row] |= (uint8_t)(1 << k.bit);
  run_ms(hold_ms);
  keyrows[k.row] &= (uint8_t)~(1 << k.bit);
  run_ms(120);
}
static void type(const char* s) { for (; *s; s++) press(*s); }
static const char ENTER = 13;

// ---------------------------------------------------------------- el rival
static std::string code_on_screen() {
  std::string c;
  for (int i = 0; i < 10; i++) { int v = mem[DFILE + 1 + 33 * 6 + 11 + i]; c += (char)(v >= 38 ? v + 27 : v + 20); }
  return c;
}
typedef std::vector<uint8_t> St;
static uint16_t hash16(const uint8_t* s, int n) {
  uint16_t h = 0x5A5A;
  for (int i = 0; i < n; i++) {
    h = (uint16_t)((h & 0xFF00) | ((h & 0xFF) ^ s[i]));
    for (int r = 0; r < 3; r++) h = (uint16_t)((h << 1) | (h >> 15));
  }
  return h;
}
static int sq(const char* n) { return (n[1] - '1') * 8 + (n[0] - 'a'); }
static St start_state() {
  St s(67, 0);
  const uint8_t back[8] = {4, 2, 3, 5, 6, 3, 2, 4};
  for (int f = 0; f < 8; f++) { s[f] = back[f]; s[8 + f] = 1; s[48 + f] = 9; s[56 + f] = (uint8_t)(back[f] + 8); }
  s[64] = 0x0F; s[65] = 0xFF; s[66] = 0;
  return s;
}
// una jugada sencilla (sin enroque, al paso ni coronacion): mueve y actualiza al paso y el reloj
static void apply_simple(St& s, const char* from, const char* to) {
  int f = sq(from), t = sq(to);
  uint8_t p = s[f], cap = s[t];
  s[t] = p; s[f] = 0;
  bool pawn = (p & 7) == 1;
  s[66] = (uint8_t)((pawn || cap) ? 0 : s[66] + 1);
  s[65] = (pawn && (t - f == 16 || f - t == 16)) ? (uint8_t)(f & 7) : 0xFF;
}
static std::string move_payload(const char* from, const char* to, const St& after) {
  std::string s; s += (char)1; s += (char)sq(from); s += (char)sq(to);
  uint16_t h = hash16(after.data(), 67); s += (char)(h & 255); s += (char)(h >> 8);
  return s;
}
static std::string body_of(const Seen* s) { return s ? s->data.substr(5, s->data.size() - 13) : std::string(); }

int main(int argc, char** argv) {
  if (argc < 2) { printf("uso: test_z80chessui chess.bin\n"); return 2; }
  FILE* f = fopen(argv[1], "rb");
  if (!f) { printf("no encuentro %s\n", argv[1]); return 2; }
  size_t sz = fread(mem + 0x6000, 1, 0x2800, f); fclose(f);
  printf("chess.bin: %zu bytes en $6000\n", sz);
  mem[0x02E7] = 0xC9; mem[0x0207] = 0xC9;              // SET_FAST y SLOW_FAST de la ROM: aqui, un RET
  z80_init(); z80_reset();
  reset_world();
  z80.sp.w = 0xFFF0; mem[0xFFF0] = 0x00; mem[0xFFF1] = 0x01; mem[0x0100] = 0x76;
  z80.pc.w = 24576; z80.iff1 = z80.iff2 = 0;

  CHECK(wait_text("1 = NEW GAME", 20000), "arranca y llega al menu");
  CHECK(shows("NETWORK CHESS"), "con el titulo del ajedrez");
  CHECK(mem[2045] == 170 && mem[2096] == 0x00 && mem[2097] == 0x88 && mem[2098] == 170, "Superfast texto con la pantalla alternativa en $8800");

  // --- partida nueva (soy las blancas)
  press('1');
  CHECK(wait_text("WAITING FOR THE RIVAL", 40000), "partida nueva: codigo y espera");
  std::string code = code_on_screen();
  std::string base = topic_base(code);
  printf("  codigo: %s\n", code.c_str());
  uint16_t pseq = 1000;
  auto peer_msg = [&](uint16_t ply, std::string payload) { peer_publish(base + "/1", make_msg(code, pseq++, ply, payload), false); };
  peer_publish(base + "/p1", "1", true);
  CHECK(wait_text("YOUR MOVE", 10000), "el rival entra: me toca");
  show("el tablero al empezar");
  CHECK(shows("YOU ARE WHITE") && shows("A  B  C  D  E  F  G  H"), "soy las blancas y los nombres de las columnas");
  { bool ok = true;
    const char* w = "RNBQKBNR"; const char* b = "rnbqkbnr";
    for (int fl = 0; fl < 8; fl++) { if (cell(0, fl) != w[fl] || cell(7, fl) != b[fl] || cell(1, fl) != 'P' || cell(6, fl) != 'p') ok = false; }
    CHECK(ok, "el tablero: blancas en mayusculas abajo, negras en inverso arriba");
    CHECK(cell(0, 0) == 'R' && screen_row(ROW_BOARD + 7)[2] == '1' && screen_row(ROW_BOARD)[2] == '8', "los numeros de fila: 1 abajo, 8 arriba");
    CHECK(cell(3, 0) == ' ' && cell(3, 1) == '.', "las casillas vacias: oscura con punto (a4), clara sin nada (b4)"); }

  St st = start_state();
  // 1. e4
  type("E2E4");
  CHECK(shows("MOVE: E2E4"), "se ve lo que tecleo");
  press(ENTER);
  CHECK(wait_text("THE RIVAL IS THINKING", 5000), "jugada hecha: le toca al rival");
  apply_simple(st, "e2", "e4");
  CHECK(cell(3, 4) == 'P' && cell(1, 4) == ' ' && shows("LAST: E2E4"), "el peon esta en e4 y se ve la ultima jugada");
  { std::string b = body_of(last_seen("PUBLISH", base + "/0"));
    CHECK(b.size() == 5 && b[0] == 1 && b[1] == sq("e2") && b[2] == sq("e4"), "al broker sale la jugada: tipo 1, e2, e4");
    uint16_t h = hash16(st.data(), 67);
    CHECK(b.size() == 5 && (uint8_t)b[3] == (h & 255) && (uint8_t)b[4] == (h >> 8), "con el hash del estado despues (el rival lo calcula igual)"); }
  // jugar cuando no toca
  type("A2A3"); press(ENTER);
  CHECK(wait_text("IT IS NOT YOUR TURN", 3000), "no me toca: aviso");
  // 1... e5
  apply_simple(st, "e7", "e5"); peer_msg(2, move_payload("e7", "e5", st));
  CHECK(wait_text("YOUR MOVE", 5000) && cell(4, 4) == 'p', "el rival juega e5 (un peon negro en inverso)");
  show("1.e4 e5");
  // jugada ilegal
  type("E2E5"); press(ENTER);
  CHECK(wait_text("THAT MOVE IS NOT LEGAL", 5000), "una jugada ilegal: aviso");
  CHECK(shows("MOVE: -") , "y se vacia lo tecleado");
  // el teclado: una tecla que no toca no entra, y SHIFT+0 borra la ultima
  type("DH");
  CHECK(shows("MOVE: D-"), "una letra donde va una cifra: no entra");
  type("1H6");
  CHECK(shows("MOVE: D1H6-"), "D1H6");
  press(8);
  CHECK(shows("MOVE: D1H-"), "SHIFT+0 borra la ultima tecla");
  type("5");
  press(ENTER);
  // 2. Dh5 (la dama a h5)
  CHECK(wait_text("THE RIVAL IS THINKING", 5000), "2. Dh5");
  // 2... Cc6
  apply_simple(st, "d1", "h5"); apply_simple(st, "b8", "c6"); peer_msg(4, move_payload("b8", "c6", st));
  CHECK(wait_text("YOUR MOVE", 5000), "2... Cc6");
  // 3. Ac4
  type("F1C4"); press(ENTER); apply_simple(st, "f1", "c4");
  CHECK(wait_text("THE RIVAL IS THINKING", 5000), "3. Ac4");
  apply_simple(st, "g8", "f6"); peer_msg(6, move_payload("g8", "f6", st));
  CHECK(wait_text("YOUR MOVE", 5000), "3... Cf6");
  show("antes del mate");
  // 4. Dxf7#
  type("H5F7"); press(ENTER);
  CHECK(wait_text("CHECKMATE - YOU WIN", 8000), "4. Dxf7++: mate del pastor");
  show("mate");
  CHECK(shows("ENTER = AGAIN"), "y la ayuda cambia");
  CHECK(cell(6, 5) == 'Q', "la dama blanca esta en f7");
  type("A2A3"); press(ENTER);
  CHECK(wait_text("THE GAME IS OVER", 3000), "con la partida acabada, una jugada da un aviso");
  press(ENTER);
  CHECK(wait_text("YOUR MOVE", 5000) && !shows("CHECKMATE") && cell(1, 4) == 'P' && cell(3, 4) == ' ', "ENTER: otra partida, el tablero inicial");
  { std::string b = body_of(last_seen("PUBLISH", base + "/0")); CHECK(b.size() == 1 && b[0] == 4, "y se lo digo al rival (tipo 4)"); }

  // --- un jaque: 1.e4 f5 2.Dh5+
  st = start_state();
  type("E2E4"); press(ENTER); apply_simple(st, "e2", "e4");
  CHECK(wait_text("THE RIVAL IS THINKING", 5000), "otra partida: 1.e4");
  apply_simple(st, "f7", "f5"); peer_msg(2, move_payload("f7", "f5", st));
  CHECK(wait_text("YOUR MOVE", 5000), "1... f5");
  type("D1H5"); press(ENTER); apply_simple(st, "d1", "h5");
  CHECK(wait_text("THE RIVAL IS IN CHECK", 5000), "2. Dh5+: se ve que el rival esta en jaque");
  show("jaque");

  // --- el rival se va y salgo
  peer_publish(base + "/p1", "0", true);
  CHECK(wait_text("THE RIVAL HAS LEFT", 5000), "el rival se va: aviso");
  press('0');
  CHECK(wait_text("1 = NEW GAME", 30000), "0: salgo de la sala y vuelvo al menu");

  // --- como jugador 2: el tablero girado
  seen.clear(); retained.clear(); filters.clear(); has_will = false;
  {
    std::string code2 = "MNPQRSTUVW"; std::string base2 = topic_base(code2);
    peer_publish(base2 + "/p0", "1", true);
    St s2 = start_state(); apply_simple(s2, "e2", "e4");
    peer_publish(base2 + "/0", make_msg(code2, 500, 1, move_payload("e2", "e4", s2)), true);
    press('2');
    CHECK(wait_text("TYPE THE ROOM CODE", 3000), "unirse: pide el codigo");
    type(code2.c_str());
    press(ENTER);
    CHECK(wait_text("YOU ARE BLACK", 40000), "unido: soy las negras");
    CHECK(wait_text("YOUR MOVE", 10000), "ya tengo su jugada retenida (1.e4) y me toca");
    show("como las negras");
    CHECK(shows("H  G  F  E  D  C  B  A"), "las columnas, al reves");
    CHECK(screen_row(ROW_BOARD)[2] == '1' && screen_row(ROW_BOARD + 7)[2] == '8', "las filas, al reves: la 1 arriba");
    CHECK(cell(7, 4, true) == 'k' && cell(0, 4, true) == 'K' && cell(3, 4, true) == 'P', "el rey negro abajo, el blanco arriba y el peon de e4");
    type("E7E5"); press(ENTER);
    CHECK(wait_text("THE RIVAL IS THINKING", 5000), "juego como las negras: e5");
    { std::string b = body_of(last_seen("PUBLISH", base2 + "/1"));
      CHECK(b.size() == 5 && b[1] == sq("e7") && b[2] == sq("e5"), "sale e7 a e5 (en casillas, no en pantalla)"); }
    CHECK(cell(4, 4, true) == 'p', "y el peon negro esta en e5");
  }

  printf("  instrucciones del Z80: %ld\n", instr_total);
  if (errors) printf("%d ERRORES\n", errors); else printf("TODO OK\n");
  return errors ? 1 : 0;
}
