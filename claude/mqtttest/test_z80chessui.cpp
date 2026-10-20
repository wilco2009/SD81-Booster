// Prueba del ajedrez en red entero (EXAMPLES/NETGAME/chess.bin) en el PC: el programa de verdad en el
// Z80 de EightyOne, con el teclado y la pantalla simulados, el modem de verdad y un rival hecho aqui por
// el broker. Se juegan jugadas sencillas (sin enroque ni al paso: eso ya lo cubre test_z80chess.cpp con
// perft), un mate del pastor, un jaque, una jugada ilegal, y como jugador 2 con el tablero girado.
// Con SHOW=1 imprime la pantalla en cada paso importante.
#include "z80net_env.h"

static const uint16_t DFILE = 0x8800;           // la pantalla de texto del lobby
static const uint16_t HFILE = 0xA000, ATTRB = 0xB800;   // la pantalla del juego: HiRes Spectrum
static const int BOARD_ROW = 1, BOARD_COL = 3;

// la fuente de la ROM ($1E00) en el PC: sintetica, un dibujo distinto por codigo (el 0, el espacio, en blanco)
static void install_font() {
  for (int c = 1; c < 64; c++) for (int k = 0; k < 8; k++) mem[0x1E00 + c * 8 + k] = (uint8_t)(c * 3 + 1 + k * 17);
}
static std::vector<uint8_t> pc_tab;             // pieces.inc: las 6 piezas (P N B R Q K), 32 bytes cada una
static void load_pieces(const char* path) {
  FILE* f = fopen(path, "r");
  if (!f) { printf("no encuentro %s\n", path); exit(2); }
  char line[256];
  while (fgets(line, sizeof line, f)) {
    char* p = strstr(line, "defb");
    if (!p) continue;
    for (p += 4; *p; p++) {
      if (*p == '0' && p[1] != 0 && isxdigit((unsigned char)p[1])) { pc_tab.push_back((uint8_t)strtoul(p, &p, 16)); p--; }
    }
  }
  fclose(f);
  if (pc_tab.size() != 192) { printf("pieces.inc: %zu bytes, esperaba 192\n", pc_tab.size()); exit(2); }
}
static uint8_t cell_byte(int row, int col, int line) {
  return mem[HFILE + ((row & 0x18) << 8) + (line << 8) + ((row & 7) << 5) + col];
}
// el codigo de caracter del ZX81 que dibuja la celda, 0 si esta en blanco, -1 si no es ninguno de la fuente
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
static uint8_t font_byte(int code, int k) { return (uint8_t)(code * 3 + 1 + k * 17); }
// las coordenadas van centradas en las dos celdas de la casilla: una letra desplazada 4 puntos en horizontal
// (las dos celdas de una fila) y una cifra desplazada 4 puntos en vertical (las dos celdas de una columna)
static int hlabel_code(int row, int col) {
  bool blank = true;
  for (int k = 0; k < 8; k++) if (cell_byte(row, col, k) || cell_byte(row, col + 1, k)) blank = false;
  if (blank) return 0;
  for (int c = 1; c < 64; c++) {
    bool ok = true;
    for (int k = 0; k < 8 && ok; k++)
      if (cell_byte(row, col, k) != (font_byte(c, k) >> 4) || cell_byte(row, col + 1, k) != (uint8_t)(font_byte(c, k) << 4)) ok = false;
    if (ok) return c;
  }
  return -1;
}
static int vlabel_code(int row, int col) {
  bool blank = true;
  for (int k = 0; k < 8; k++) if (cell_byte(row, col, k) || cell_byte(row + 1, col, k)) blank = false;
  if (blank) return 0;
  for (int c = 1; c < 64; c++) {
    bool ok = true;
    for (int k = 0; k < 4 && ok; k++)
      if (cell_byte(row, col, 4 + k) != font_byte(c, k) || cell_byte(row + 1, col, k) != font_byte(c, 4 + k)) ok = false;
    for (int k = 0; k < 4 && ok; k++) if (cell_byte(row, col, k) || cell_byte(row + 1, col, 4 + k)) ok = false;
    if (ok) return c;
  }
  return -1;
}
static char zx_char(int v) {
  if (v == 0) return ' ';
  if (v >= 28 && v <= 37) return (char)('0' + v - 28);
  if (v >= 38 && v <= 63) return (char)('A' + v - 38);
  if (v == 27) return '.'; if (v == 14) return ':'; if (v == 22) return '-';
  if (v == 15) return '?'; if (v == 20) return '='; if (v == 26) return ',';
  return '~';
}
static bool gfx_mode() { return mem[2045] == 172; }
static std::string screen_row(int r) {
  std::string s;
  for (int c = 0; c < 32; c++) {
    if (gfx_mode() && r == 17 && c >= BOARD_COL && c < BOARD_COL + 16) {          // A-H, centradas en cada casilla
      s += ((c - BOARD_COL) % 2 == 0) ? zx_char(hlabel_code(r, c)) : ' ';
      continue;
    }
    if (gfx_mode() && c == BOARD_COL - 1 && r >= BOARD_ROW && r < BOARD_ROW + 16) {  // 1-8, centradas en cada casilla
      s += ((r - BOARD_ROW) % 2 == 0) ? zx_char(vlabel_code(r, c)) : ' ';
      continue;
    }
    if (gfx_mode()) {
      bool in_board = r >= BOARD_ROW && r < BOARD_ROW + 16 && c >= BOARD_COL && c < BOARD_COL + 16;
      s += in_board ? ' ' : zx_char(glyph_code(r, c));
    } else {
      int v = mem[DFILE + 1 + 33 * r + c];
      s += v == 128 ? '#' : (v >= 166 && v <= 191) ? (char)('a' + v - 166) : zx_char(v);
    }
  }
  return s;
}
static std::string screen_all() { std::string s; for (int r = 0; r < 24; r++) s += screen_row(r) + "\n"; return s; }
static bool shows(const char* t) { return screen_all().find(t) != std::string::npos; }
static bool show_on = getenv("SHOW") != nullptr;
// la pieza que se ve en la casilla (rank 0-7, file 0-7): letra mayuscula = blanca (tinta blanca), minuscula =
// negra (tinta negra), ' ' vacia, '?' un dibujo desconocido
static char cell(int rank, int file, bool black_bottom = false) {
  int y = black_bottom ? rank : 7 - rank;
  int x = black_bottom ? 7 - file : file;
  int row = BOARD_ROW + 2 * y, col = BOARD_COL + 2 * x;
  uint8_t b[32];
  bool blank = true;
  for (int i = 0; i < 4; i++) for (int k = 0; k < 8; k++) { b[i * 8 + k] = cell_byte(row + (i >> 1), col + (i & 1), k); if (b[i * 8 + k]) blank = false; }
  if (blank) return ' ';
  const char* names = "PNBRQK";
  bool white = (mem[ATTRB + row * 32 + col] & 7) == 7;
  for (int p = 0; p < 6; p++)
    if (!memcmp(b, &pc_tab[p * 32], 32)) return white ? names[p] : (char)(names[p] + 32);
  return '?';
}
// el papel (color de fondo) de la casilla: 4 verde (clara), 2 rojo (oscura), 3 magenta (ultima jugada), 0 negro (rey blanco
// en jaque), 7 blanco (rey negro en jaque)
static int square_paper(int rank, int file, bool black_bottom = false) {
  int y = black_bottom ? rank : 7 - rank;
  int x = black_bottom ? 7 - file : file;
  return (mem[ATTRB + (BOARD_ROW + 2 * y) * 32 + BOARD_COL + 2 * x] >> 3) & 7;
}
static int scr_n = 0;
// con SCRDIR=<carpeta>, guarda la pantalla del juego (6912 bytes, formato .SCR del Spectrum) en cada show()
static void dump_scr(const char* title) {
  const char* dir = getenv("SCRDIR");
  if (!dir || !gfx_mode()) return;
  char name[256];
  snprintf(name, sizeof name, "%s/%02d.scr", dir, scr_n++);
  FILE* f = fopen(name, "wb");
  if (!f) return;
  fwrite(mem + HFILE, 1, 6912, f);
  fclose(f);
  printf("pantalla guardada: %s (%s)\n", name, title);
}
static void show(const char* title) {
  dump_scr(title);
  if (!show_on) return;
  printf("---- %s (t=%u ms)\n", title, mock_now - 100000);
  bool black_bottom = false;
  { std::string l = screen_row(5); if (l.find("BLACK") != std::string::npos) black_bottom = true; }
  for (int r = 0; r < 24; r++) {
    std::string l = screen_row(r);
    if (gfx_mode() && r >= BOARD_ROW && r < BOARD_ROW + 16 && (r - BOARD_ROW) % 2 == 0) {   // el tablero, una linea por casilla
      int y = (r - BOARD_ROW) / 2;
      std::string b;
      for (int x = 0; x < 8; x++) {
        int rank = black_bottom ? y : 7 - y, file = black_bottom ? 7 - x : x;
        char ch = cell(rank, file, black_bottom);
        int paper = square_paper(rank, file, black_bottom);
        b += (ch == ' ' ? '.' : ch);
        b += (paper == 3 ? '*' : (paper == 0 || paper == 7) ? '!' : paper == 2 ? ':' : ' ');
      }
      for (int x = 0; x < 16; x++) l[BOARD_COL + x] = b[x];
    }
    while (!l.empty() && l.back() == ' ') l.pop_back();
    if (!l.empty()) printf("%2d|%s\n", r, l.c_str());
  }
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
  run_ms(60);                                          // una persona no pulsa en el mismo instante en que sale el texto
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
  for (int i = 0; i < 10; i++) { int v = gfx_mode() ? glyph_code(2, 21 + i) : mem[DFILE + 1 + 33 * 6 + 11 + i]; c += (char)(v >= 38 ? v + 27 : v + 20); }
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
static std::string move_payload(const char* from, const char* to, const St& after, uint16_t rem = 0) {
  std::string s; s += (char)1; s += (char)sq(from); s += (char)sq(to);
  uint16_t h = hash16(after.data(), 67); s += (char)(h & 255); s += (char)(h >> 8);
  s += (char)(rem & 255); s += (char)(rem >> 8);          // los segundos que le quedan al que juega
  return s;
}
// "WHITE 09:58" o "BLACK 09:58" en la fila r del panel -> segundos, o -1 si no hay reloj
static int clock_secs(int r) {
  std::string l = screen_row(r);
  size_t p = l.find("WHITE ");
  if (p == std::string::npos) p = l.find("BLACK ");
  if (p == std::string::npos) return -1;
  return atoi(l.substr(p + 6, 2).c_str()) * 60 + atoi(l.substr(p + 9, 2).c_str());
}
static std::string body_of(const Seen* s) { return s ? s->data.substr(5, s->data.size() - 13) : std::string(); }

int main(int argc, char** argv) {
  if (argc < 2) { printf("uso: test_z80chessui chess.bin\n"); return 2; }
  FILE* f = fopen(argv[1], "rb");
  if (!f) { printf("no encuentro %s\n", argv[1]); return 2; }
  size_t sz = fread(mem + 0x5000, 1, 0x3000, f); fclose(f);
  if (0x5000 + sz > 0x8000) { printf("ERROR el programa llega a $%04zX: pasa de $8000 y alli el ZX81 no ejecuta codigo\n", 0x5000 + sz); errors++; }
  printf("chess.bin: %zu bytes en $5000\n", sz);
  std::vector<uint8_t> bin(mem + 0x5000, mem + 0x5000 + sz);
  // apagar y encender el ZX81: se pierde la RAM; la SD (sdfiles) y el broker (los retenidos) no
  auto reboot = [&]() {
    auto keep = retained;
    memset(mem, 0, sizeof(mem));
    memcpy(mem + 0x5000, bin.data(), bin.size());
    install_font();
    mem[0x02E7] = 0xC9; mem[0x0207] = 0xC9;
    z80_init(); z80_reset();
    reset_world();
    retained = keep;
    z80.sp.w = 0xFFF0; mem[0xFFF0] = 0x00; mem[0xFFF1] = 0x01; mem[0x0100] = 0x76;
    z80.pc.w = 20480; z80.iff1 = z80.iff2 = 0;
  };
  install_font(); load_pieces("pieces.inc");
  mem[0x02E7] = 0xC9; mem[0x0207] = 0xC9;              // SET_FAST y SLOW_FAST de la ROM: aqui, un RET
  z80_init(); z80_reset();
  reset_world();
  z80.sp.w = 0xFFF0; mem[0xFFF0] = 0x00; mem[0xFFF1] = 0x01; mem[0x0100] = 0x76;
  z80.pc.w = 20480; z80.iff1 = z80.iff2 = 0;

  CHECK(wait_text("1 = NEW GAME", 20000), "arranca y llega al menu");
  CHECK(shows("NETWORK CHESS"), "con el titulo del ajedrez");
  CHECK(mem[2045] == 170 && mem[2096] == 0x00 && mem[2097] == 0x88 && mem[2098] == 170, "Superfast texto con la pantalla alternativa en $8800");

  // --- partida nueva (soy las blancas), sin reloj
  press('1');
  CHECK(wait_text("TIME FOR EACH PLAYER", 3000), "partida nueva: pide el control de tiempo");
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
  CHECK(shows("YOU ARE") && shows("WHITE") && screen_row(17).find("A B C D E F G H") != std::string::npos, "soy las blancas y los nombres de las columnas");
  { bool ok = true;
    const char* w = "RNBQKBNR"; const char* b = "rnbqkbnr";
    for (int fl = 0; fl < 8; fl++) { if (cell(0, fl) != w[fl] || cell(7, fl) != b[fl] || cell(1, fl) != 'P' || cell(6, fl) != 'p') ok = false; }
    CHECK(ok, "el tablero: blancas en mayusculas abajo, negras en inverso arriba");
    CHECK(cell(0, 0) == 'R' && screen_row(BOARD_ROW + 14)[2] == '1' && screen_row(BOARD_ROW)[2] == '8', "los numeros de fila: 1 abajo, 8 arriba");
    CHECK(cell(3, 0) == ' ' && cell(3, 1) == ' ' && square_paper(3, 0) == 4 && square_paper(3, 1) == 2 && square_paper(0, 0) == 2,
          "los colores: a1 es oscura (roja), a4 clara (verde), b4 oscura"); }

  St st = start_state();
  // 1. e4
  type("E2E4");
  CHECK(shows("MOVE: E2E4"), "se ve lo que tecleo");
  press(ENTER);
  CHECK(wait_text("THE RIVAL IS THINKING", 5000), "jugada hecha: le toca al rival");
  apply_simple(st, "e2", "e4");
  CHECK(cell(3, 4) == 'P' && cell(1, 4) == ' ' && screen_row(9).find("E2-E4") != std::string::npos && square_paper(1, 4) == 3 && square_paper(3, 4) == 3, "el peon esta en e4, se ve la ultima jugada y sus dos casillas resaltadas");
  { std::string b = body_of(last_seen("PUBLISH", base + "/0"));
    CHECK(b.size() == 7 && b[0] == 1 && b[1] == sq("e2") && b[2] == sq("e4"), "al broker sale la jugada: tipo 1, e2, e4 (y los segundos del reloj)");
    uint16_t h = hash16(st.data(), 67);
    CHECK(b.size() == 7 && (uint8_t)b[3] == (h & 255) && (uint8_t)b[4] == (h >> 8), "con el hash del estado despues (el rival lo calcula igual)"); }
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
  CHECK(wait_text("ENTER = AGAIN", 2000), "y la ayuda cambia");
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
  CHECK(square_paper(7, 4) == 7 && cell(7, 4) == 'k', "y su rey (negro) sale con el papel blanco");
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
    CHECK(wait_text("BLACK", 40000), "unido: soy las negras");
    CHECK(wait_text("YOUR MOVE", 10000), "ya tengo su jugada retenida (1.e4) y me toca");
    show("como las negras");
    CHECK(shows("H G F E D C B A"), "las columnas, al reves");
    CHECK(screen_row(BOARD_ROW)[2] == '1' && screen_row(BOARD_ROW + 14)[2] == '8', "las filas, al reves: la 1 arriba");
    CHECK(cell(7, 4, true) == 'k' && cell(0, 4, true) == 'K' && cell(3, 4, true) == 'P', "el rey negro abajo, el blanco arriba y el peon de e4");
    type("E7E5"); press(ENTER);
    CHECK(wait_text("THE RIVAL IS THINKING", 5000), "juego como las negras: e5");
    { std::string b = body_of(last_seen("PUBLISH", base2 + "/1"));
      CHECK(b.size() == 7 && b[1] == sq("e7") && b[2] == sq("e5"), "sale e7 a e5 (en casillas, no en pantalla)"); }
    CHECK(cell(4, 4, true) == 'p', "y el peon negro esta en e5");
  }

  // --- se cayeron los dos (presencias a '0'): la sala existe pero esta vacia; se elige con que ranura entrar
  press('0');
  CHECK(wait_text("1 = NEW GAME", 30000), "0: vuelvo al menu");
  seen.clear(); retained.clear(); filters.clear(); has_will = false;
  {
    std::string code9 = "BCDEFGHJKM"; std::string b9 = topic_base(code9);
    peer_publish(b9 + "/p0", "0", true);
    peer_publish(b9 + "/p1", "0", true);
    press('2');
    CHECK(wait_text("TYPE THE ROOM CODE", 3000), "unirse: pide el codigo");
    type(code9.c_str());
    press(ENTER);
    CHECK(wait_text("THE ROOM IS EMPTY", 40000) && shows("1 = PLAYER 1") && shows("PLAYER 1 = WHITE, 2 = BLACK"), "sala vacia: se ofrece elegir ranura");
    bool pub = false; for (auto& sn : seen) if (sn.kind == "PUBLISH") pub = true;
    CHECK(!pub, "sin publicar nada mientras se elige");
    press('1');
    CHECK(wait_text("YOUR MOVE", 40000) && shows("WHITE"), "elijo ser el jugador 1: blancas, me toca");
    const Seen* pp = last_seen("PUBLISH", b9 + "/p0");
    CHECK(pp && pp->data == "1" && pp->retain, "con mi presencia en la ranura 0");
  }


  // --- los relojes: 10 minutos por jugador (el control de tiempo va en la primera letra del codigo: C)
  press('0');
  CHECK(wait_text("1 = NEW GAME", 30000), "0: vuelvo al menu");
  seen.clear(); retained.clear(); filters.clear(); has_will = false;
  {
    press('1');
    CHECK(wait_text("TIME FOR EACH PLAYER", 3000), "partida nueva: pide el control de tiempo");
    press('3');                                                  // 3 = 10 minutos
    CHECK(wait_text("WAITING FOR THE RIVAL", 40000), "y crea la sala");
    std::string codeT = code_on_screen();
    CHECK(codeT.size() == 10 && codeT[0] == 'C', "el control de tiempo viaja en la primera letra del codigo (C = 10 min)");
    std::string baseT = topic_base(codeT);
    uint16_t pseqT = 3000;
    auto peerT = [&](uint16_t ply, std::string payload) { peer_publish(baseT + "/1", make_msg(codeT, pseqT++, ply, payload), false); };
    peer_publish(baseT + "/p1", "1", true);
    CHECK(wait_text("YOUR MOVE", 10000), "el rival entra: me toca");
    show("relojes: 10 minutos");
    { int w = clock_secs(12), b = clock_secs(13);
      CHECK(w >= 595 && w <= 600 && b == 600, "los dos relojes, a 10:00"); }
    time_extra += 25000; run_ms(2500);                            // pienso 27 s
    { int w = clock_secs(12);
      CHECK(w >= 565 && w <= 580, "mi reloj (el de las blancas) baja con el tiempo que pienso"); }
    type("E2E4"); press(ENTER);
    CHECK(wait_text("THE RIVAL IS THINKING", 5000), "juego e4");
    int mine = 0;
    { std::string b = body_of(last_seen("PUBLISH", baseT + "/0"));
      mine = b.size() == 7 ? ((uint8_t)b[5] | ((uint8_t)b[6] << 8)) : -1;
      CHECK(mine >= 560 && mine <= 580, "la jugada lleva los segundos que me quedan"); }
    time_extra += 12000; run_ms(2500);                            // el rival piensa unos 14 s
    { int w = clock_secs(12), b = clock_secs(13);
      CHECK(w >= mine - 2 && w <= mine + 1, "mi reloj se para mientras piensa el rival");
      CHECK(b >= 575 && b <= 592, "y el suyo (una estimacion mia) baja"); }
    St stT = start_state();
    apply_simple(stT, "e2", "e4"); apply_simple(stT, "e7", "e5");
    peerT(2, move_payload("e7", "e5", stT, 590));                 // el rival dice que le quedan 590 s
    CHECK(wait_text("YOUR MOVE", 5000), "el rival juega e5");
    { int b = clock_secs(13);
      CHECK(b >= 586 && b <= 590, "su reloj pasa a lo que dice su mensaje (590 s)"); }
    show("relojes: tras 1.e4 e5");
    // se me acaba el tiempo: pierdo, y el rival lo sabe
    time_extra += 700000; run_ms(3000);
    CHECK(wait_text("TIME OUT - THE RIVAL WINS", 5000), "se me acaba el tiempo: pierdo");
    { std::string b = body_of(last_seen("PUBLISH", baseT + "/0"));
      CHECK(b.size() == 1 && b[0] == 5, "y se lo digo al rival (tipo 5)"); }
    show("relojes: me quedo sin tiempo");
    // otra partida: los relojes vuelven a 10:00
    press(ENTER);
    CHECK(wait_text("YOUR MOVE", 8000) && !shows("TIME OUT"), "ENTER: otra partida");
    { int w = clock_secs(12), b = clock_secs(13);
      CHECK(w >= 595 && w <= 600 && b >= 595 && b <= 600, "con los relojes otra vez a 10:00"); }
    // al rival se le acaba el tiempo: me lo dice y gano
    peerT(0, std::string(1, (char)5));
    CHECK(wait_text("TIME OUT - YOU WIN", 5000), "el rival dice que se le ha acabado el tiempo: gano");
    show("relojes: gano por tiempo");
  }


  // --- guardar la partida y seguirla otro dia (savegame.inc), con relojes de 10 minutos
  press('0');
  CHECK(wait_text("1 = NEW GAME", 30000), "0: vuelvo al menu");
  CHECK(shows("3 = CONTINUE SAVED GAME"), "la partida anterior esta guardada: el menu ofrece seguirla");
  seen.clear(); retained.clear(); filters.clear(); has_will = false;
  sdfiles.clear();
  {
    press('1');
    CHECK(wait_text("TIME FOR EACH PLAYER", 3000), "partida nueva");
    press('3');                                                  // 10 minutos
    CHECK(wait_text("WAITING FOR THE RIVAL", 40000), "y crea la sala");
    std::string codeS = code_on_screen(), baseS = topic_base(codeS);
    uint16_t sseq = 5000;
    auto peerS = [&](uint16_t ply, std::string payload) { peer_publish(baseS + "/1", make_msg(codeS, sseq++, ply, payload), true); };
    peer_publish(baseS + "/p1", "1", true);
    CHECK(wait_text("YOUR MOVE", 10000), "el rival entra: me toca");
    type("E2E4"); press(ENTER);
    CHECK(wait_text("THE RIVAL IS THINKING", 5000), "juego e4");
    St stS = start_state();
    apply_simple(stS, "e2", "e4"); apply_simple(stS, "e7", "e5");
    peerS(2, move_payload("e7", "e5", stS, 590));
    CHECK(wait_text("YOUR MOVE", 5000), "el rival juega e5");
    type("G1F3"); press(ENTER);
    CHECK(wait_text("THE RIVAL IS THINKING", 5000), "juego Cf3");
    apply_simple(stS, "g1", "f3");
    run_ms(500);
    int mine = 0;
    { std::string b = body_of(last_seen("PUBLISH", baseS + "/0"));
      mine = b.size() == 7 ? ((uint8_t)b[5] | ((uint8_t)b[6] << 8)) : -1; }
    { auto it = sdfiles.find("CHESS.SAV");
      CHECK(it != sdfiles.end() && it->second.size() == 91, "se ha guardado CHESS.SAV (24 + 67 bytes)");
      if (it != sdfiles.end() && it->second.size() == 91) {
        const auto& v = it->second;
        CHECK(v[0] == 2 && v[1] == 1 && v[2] == 0 && v[3] == 2, "el fichero: ajedrez, version 1, ranura 0 y control de tiempo 2 (10 minutos: la letra C)");
        CHECK(v[14] == 3 && v[15] == 0 && v[16] == 0, "tres jugadas hechas y la partida sigue");
        CHECK(memcmp(v.data() + 24, stS.data(), 67) == 0, "y el tablero (las 67 bytes del estado)");
        int w = v[19] | (v[20] << 8), b = v[21] | (v[22] << 8);
        CHECK(w == mine && b == 590, "y los relojes: los mios (los de mi ultima jugada) y los 590 s del rival"); } }
    press('0');
    CHECK(wait_text("1 = NEW GAME", 30000), "salgo al menu");

    reboot();                                                    // apagar y encender: la SD y el broker se quedan como estan
    CHECK(wait_text("1 = NEW GAME", 20000), "el ZX81 arranca otra vez");
    CHECK(shows("3 = CONTINUE SAVED GAME"), "y el menu ofrece seguir la partida guardada");
    press('3');
    CHECK(wait_text("THE RIVAL IS THINKING", 40000), "sigo la partida: la ultima jugada fue mia y espero al rival");
    show("ajedrez seguido");
    CHECK(cell(3, 4) == 'P' && cell(4, 4) == 'p' && cell(2, 5) == 'N' && cell(1, 4) == ' ' && cell(6, 4) == ' ', "el tablero vuelve como estaba (e4, e5 y Cf3)");
    CHECK(shows("WHITE") && shows(codeS.c_str()), "soy las blancas, en la misma sala");
    { int w = clock_secs(12), b = clock_secs(13);
      CHECK(w >= mine - 2 && w <= mine + 3 && b >= 586 && b <= 592, "los relojes vuelven como estaban (el tiempo que estuve fuera no cuenta)"); }
    { const Seen* c = last_seen("CONNECT");
      CHECK(c && c->topic == baseS + "/p0", "he entrado con el testamento en MI ranura (la 0)"); }
    { std::string b = body_of(last_seen("PUBLISH", baseS + "/0"));
      CHECK(b.size() == 7 && b[0] == 1 && b[1] == sq("g1") && b[2] == sq("f3"), "he vuelto a mandar mi ultima jugada (Cf3)"); }
    apply_simple(stS, "b8", "c6");
    peerS(4, move_payload("b8", "c6", stS, 580));
    CHECK(wait_text("YOUR MOVE", 10000), "el rival juega Cc6: me toca");
    CHECK(cell(5, 2) == 'n', "y su jugada encaja en el estado guardado (el hash cuadra)");
    { int b = clock_secs(13);
      CHECK(b >= 576 && b <= 580, "y su reloj pasa a lo que dice su mensaje (580 s)"); }
    show("ajedrez seguido: tras Cc6");
  }

  printf("  instrucciones del Z80: %ld\n", instr_total);
  if (errors) printf("%d ERRORES\n", errors); else printf("TODO OK\n");
  return errors ? 1 : 0;
}
