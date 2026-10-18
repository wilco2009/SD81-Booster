// Prueba de la pantalla del Scrabble (EXAMPLES/NETGAME/scrabble_ui.inc) en el PC: el Z80 de verdad juega unas jugadas
// con el motor (el diccionario de enable1 en la "SD" de mentira) y dibuja el tablero, el atril y el panel. Se comprueba
// lo que cada celda dibuja (letra, color del premio, ficha del atril, puntos) y con SCRDIR=<carpeta> se guardan las
// pantallas (6912 bytes, formato .SCR del Spectrum) para verlas con scr2png.py. La fuente de la ROM de verdad
// ($1E00) se lee de la ROM que se pasa como segundo argumento.
#include "z80net_env.h"
#include <fstream>
#include <set>
#include <algorithm>

static const uint16_t E_DRAW = 24576, E_INIT = 24579, E_CHECK = 24582, E_APPLY = 24585, E_NOTE = 24588, E_BOARD = 24591, E_RACK = 24594,
                      E_PANEL = 24597, E_MSG = 24600, E_STATUS = 24603, E_DOPEN = 24606, E_HELP = 24609, E_INPUT = 24612, E_KEY = 24615, E_ICLEAR = 24618, E_IBUILD = 24621, E_IDRAW = 24624, E_ERRMSG = 24627, E_SPRINIT = 24630, E_SPROFF = 24633, E_BLINK = 24636;
static const uint16_t SI = 0x9B00;   // SI_BASE: las variables de scrabble_in.inc
static const uint16_t SB = 0x9600, SU = 0x9900;
static const uint16_t A_BOARD = SB, A_RACK = SB + 325, A_SCORE = SB + 339, A_BAGN = SB + 344, A_TURN = SB + 345, A_LANG = SB + 348,
                      A_SEED = SB + 349, A_BCNT = SB + 353, A_WB = SB + 364, A_TOT = SB + 409, A_ME = SU;
static const uint16_t HFILE = 0xA000, ATTRB = 0xB800;
static const int BOARD_ROW = 2, BOARD_COL = 2, RACK_ROW = 17, RACK_COL = 2;

static uint8_t cell_byte(int row, int col, int line) { return mem[HFILE + ((row & 0x18) << 8) + (line << 8) + ((row & 7) << 5) + col]; }
static uint8_t attr(int row, int col) { return mem[ATTRB + row * 32 + col]; }
// el codigo de caracter del ZX81 que dibuja la celda (0 en blanco, -1 ninguno de la fuente)
static int glyph_code(int row, int col) {
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
static std::string text_row(int row, int c0 = 0, int c1 = 32) {      // lo escrito en una fila, en ASCII
  std::string s;
  for (int c = c0; c < c1; c++) {
    int g = glyph_code(row, c);
    if (g == 0) s += ' ';
    else if (g >= 38 && g <= 63) s += (char)('A' + g - 38);
    else if (g >= 28 && g <= 37) s += (char)('0' + g - 28);
    else if (g == 22) s += '-';
    else if (g == 20) s += '=';
    else if (g == 14) s += ':';
    else if (g == 24) s += '/';
    else if (g == 27) s += '.';
    else if (g == 26) s += ',';
    else if (g == 21) s += '+';
    else if (g == 16) s += '(';
    else if (g == 17) s += ')';
    else if (g == 15) s += '?';
    else s += '#';
  }
  return s;
}
static int scr_n = 0;
static void dump(const char* name) {
  const char* dir = getenv("SCRDIR");
  if (!dir) return;
  char path[256]; snprintf(path, sizeof path, "%s/%02d_%s.scr", dir, scr_n++, name);
  FILE* f = fopen(path, "wb");
  if (f) { fwrite(mem + HFILE, 1, 6912, f); fclose(f); printf("pantalla guardada: %s\n", path); }
}
static int z_i16_ui(uint16_t a) { return (int16_t)(mem[a] | (mem[a + 1] << 8)); }
static void puts_mem(uint16_t a, const char* s) { while (*s) mem[a++] = (uint8_t)*s++; mem[a] = 0; }
static std::vector<uint8_t> make_dict(const std::set<std::string>& d) {
  std::vector<uint8_t> v(16, 0);
  memcpy(v.data(), "SDICT01", 7);
  uint32_t n = (uint32_t)d.size();
  v[8] = n & 255; v[9] = (n >> 8) & 255; v[10] = (n >> 16) & 255; v[11] = (n >> 24) & 255;
  for (auto& w : d) { uint8_t r[16] = {0}; memcpy(r, w.data(), w.size()); v.insert(v.end(), r, r + 16); }
  return v;
}
static void setrack(int p, const char* s) { for (int i = 0; i < 7; i++) mem[A_RACK + 7 * p + i] = (uint8_t)s[i]; }
// juega una jugada con el motor: la comprueba y la hace; deja anotada la ultima jugada para el resaltado
static int play(int player, int x, int y, int dir, const char* word, const char* rack) {
  mem[A_TURN] = (uint8_t)player;
  setrack(player, rack);
  int n = (int)strlen(word);
  mem[A_WB] = (uint8_t)x; mem[A_WB + 1] = (uint8_t)y; mem[A_WB + 2] = (uint8_t)dir; mem[A_WB + 3] = (uint8_t)n;
  for (int i = 0; i < n; i++) mem[A_WB + 4 + i] = (uint8_t)word[i];
  int e = call_ep(E_CHECK);
  if (e) return e;
  call_ep(E_APPLY);
  mem[0x9FF0] = (uint8_t)player;
  call_ep(E_NOTE);
  return 0;
}

int main(int argc, char** argv) {
  if (argc < 4) { printf("uso: test_z80scrabbleui scrui.bin SDBOOST.ROM enable1.txt\n"); return 2; }
  FILE* f = fopen(argv[1], "rb");
  if (!f) { printf("no encuentro %s\n", argv[1]); return 2; }
  size_t sz = fread(mem + 24576, 1, 0x3000, f); fclose(f);
  printf("scrui.bin: %zu bytes\n", sz);
  FILE* rf = fopen(argv[2], "rb");
  if (!rf) { printf("no encuentro %s\n", argv[2]); return 2; }
  uint8_t rom[0x2000]; size_t rn = fread(rom, 1, sizeof rom, rf); fclose(rf);
  if (rn < 0x2000) { printf("la ROM es muy corta\n"); return 2; }
  memcpy(mem + 0x1E00, rom + 0x1E00, 0x200);              // la fuente de verdad
  std::ifstream wf(argv[3]);
  std::set<std::string> en;
  std::string line;
  while (std::getline(wf, line)) {
    while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
    if (line.size() < 2 || line.size() > 15) continue;
    bool ok = true; std::string u;
    for (char ch : line) { if (ch >= 'a' && ch <= 'z') ch -= 32; if (ch < 'A' || ch > 'Z') ok = false; u += ch; }
    if (ok) en.insert(u);
  }
  sdfiles["DICTEN.DIC"] = make_dict(en);
  z80_init(); z80_reset(); reset_world();

  // --- la partida: unas jugadas de verdad con el motor
  for (int i = 0; i < 4; i++) mem[A_SEED + i] = (0x1234567 >> (8 * i)) & 255;
  mem[A_LANG] = 0;
  call_ep(E_INIT);
  CHECK(call_ep(E_DOPEN) == 0, "abre el diccionario");
  mem[A_ME] = 0;
  call_ep(E_DRAW);
  dump("inicio");
  // el tablero vacio: las letras de arriba y los numeros de la izquierda
  CHECK(text_row(1, 2, 17) == "ABCDEFGHIJKLMNO", "las letras A-O sobre las columnas");
  CHECK(text_row(2, 0, 2) == " 1" && text_row(10, 0, 2) == " 9" && text_row(11, 0, 2) == "10" && text_row(16, 0, 2) == "15", "los numeros de las filas 1, 9, 10 y 15");
  CHECK(text_row(0, 0, 32).find("SCRABBLE") != std::string::npos, "el titulo");
  // los colores de los premios (casilla, atributo): TW en A1, DL en D1, TL en F2, DW en B2, el centro (H8) DW con estrella
  auto bat = [&](int x, int y) { return attr(BOARD_ROW + y, BOARD_COL + x); };
  CHECK(bat(0, 0) == 0x57 && bat(14, 14) == 0x57 && bat(7, 0) == 0x57, "las esquinas y el medio del borde son palabra triple");
  CHECK(bat(3, 0) == 0x68 && bat(0, 3) == 0x68, "D1 y A4 son letra doble");
  CHECK(bat(5, 1) == 0x4F && bat(1, 5) == 0x4F, "F2 y B6 son letra triple");
  CHECK(bat(1, 1) == 0x58 && bat(7, 7) == 0x58, "B2 y el centro son palabra doble");
  CHECK(bat(1, 0) == 0x20 && bat(7, 6) == 0x20, "las demas casillas, verdes");
  { bool any = false; for (int k = 0; k < 8; k++) if (cell_byte(BOARD_ROW + 7, BOARD_COL + 7, k)) any = true; CHECK(any, "el centro lleva dibujada la estrella"); }
  { bool any = false; for (int k = 0; k < 8; k++) if (cell_byte(BOARD_ROW + 7, BOARD_COL + 6, k)) any = true; CHECK(!any, "una casilla vacia normal no lleva nada"); }
  CHECK(text_row(2, 18, 32).substr(0, 3) == "YOU" && text_row(3, 18, 32).substr(0, 5) == "RIVAL", "el panel: YOU y RIVAL");
  CHECK(text_row(7, 18, 32).substr(0, 9) == "YOUR TURN", "empieza el turno del jugador 0");
  CHECK(text_row(5, 18, 32).find("86") != std::string::npos, "la bolsa tiene 86 fichas");
  CHECK(text_row(13, 18, 29) == "DL TL DW TW", "la leyenda de los premios");
  CHECK(attr(13, 18) == 0x68 && attr(13, 21) == 0x4F && attr(13, 24) == 0x58 && attr(13, 27) == 0x57, "los colores de la leyenda");

  // el atril del jugador 0 (7 fichas de 2 x 2 celdas con la letra grande) y su valor debajo
  setrack(0, "QUIZXE?");
  call_ep(E_RACK);
  dump("atril");
  for (int i = 0; i < 7; i++) {
    bool any = false;
    for (int r = 0; r < 2; r++) for (int c = 0; c < 2; c++) for (int k = 0; k < 8; k++) if (cell_byte(RACK_ROW + r, RACK_COL + 3 * i + c, k)) any = true;
    CHECK(any, "cada ficha del atril esta dibujada");
    { int want = (i == 6) ? 0x79 : 0x78;               // blancas, y el comodin con la tinta azul
      CHECK(attr(RACK_ROW, RACK_COL + 3 * i) == want && attr(RACK_ROW + 1, RACK_COL + 3 * i + 1) == want, "y es blanca (el comodin, con tinta azul)"); }
  }
  { std::string s = text_row(19, 2, 24);          // QUIZXE?: 10 1 1 10 8 1 0 (una ficha cada 3 celdas)
    CHECK(s.substr(0, 2) == "10" && s[3] == '1' && s[6] == '1' && s.substr(9, 2) == "10" && s[12] == '8' && s[15] == '1' && s[18] == '0', "los valores debajo de las fichas: Q 10, U 1, I 1, Z 10, X 8, E 1, comodin 0"); }
  // un hueco en el atril: sin ficha
  mem[A_RACK + 2] = 0; call_ep(E_RACK);
  { bool any = false; for (int r = 0; r < 2; r++) for (int c = 0; c < 2; c++) for (int k = 0; k < 8; k++) if (cell_byte(RACK_ROW + r, RACK_COL + 6 + c, k)) any = true;
    CHECK(!any && attr(RACK_ROW, RACK_COL + 6) == 0x20, "un hueco del atril: verde y sin dibujo"); }
  CHECK(text_row(19, 8, 10) == "  ", "y sin valor");

  // una partida: QUIZ (H8), ZAX hacia abajo, UNITS hacia abajo, EAS+Y con el comodin
  CHECK(play(0, 4, 7, 0, "QUIZ", "QUIZZES") == 0, "jugada 1: QUIZ");
  CHECK(play(1, 7, 7, 1, "ZAX", "AXEIOUY") == 0, "jugada 2: ZAX");
  CHECK(play(0, 5, 7, 1, "UNITS", "NITSEEE") == 0, "jugada 3: UNITS");
  { const char w[] = {'E', 'A', 'S', (char)('Y' | 0x80), 0}; CHECK(play(1, 3, 11, 0, w, "EAB????") == 0, "jugada 4: EASY con el comodin como Y"); }
  mem[A_ME] = 1;                                           // se ve desde el jugador 1 (RIVAL = el 0)
  setrack(1, "ZEBRAXX");
  call_ep(E_DRAW);
  dump("partida");
  auto tile = [&](int x, int y) { return glyph_code(BOARD_ROW + y, BOARD_COL + x); };
  CHECK(tile(4, 7) == 38 + 'Q' - 'A' && tile(5, 7) == 38 + 'U' - 'A' && tile(6, 7) == 38 + 'I' - 'A' && tile(7, 7) == 38 + 'Z' - 'A', "QUIZ en H8 (las letras en las celdas)");
  CHECK(tile(7, 8) == 38 && tile(7, 9) == 38 + 'X' - 'A', "ZAX hacia abajo");
  CHECK(tile(3, 11) == 38 + 'E' - 'A' && tile(6, 11) == 38 + 'Y' - 'A', "EASY, con la Y del comodin");
  CHECK(bat(4, 7) == 0x78 && bat(5, 7) == 0x78, "las fichas puestas son blancas");
  CHECK(bat(3, 11) == 0x70 && bat(4, 11) == 0x70 && bat(5, 11) == 0x78 && bat(6, 11) == 0x71, "las fichas de la ultima jugada, en amarillo (la S ya estaba; el comodin, en su propio color)");
  CHECK(text_row(2, 18, 32).find("YOU") != std::string::npos, "el panel");
  std::string r2 = text_row(2, 18, 32), r3 = text_row(3, 18, 32);
  // jugador 1: ZAX (19) + EASY...; jugador 0: QUIZ 44 + UNITS...
  int s0 = (int16_t)(mem[A_SCORE] | (mem[A_SCORE + 1] << 8)), s1 = (int16_t)(mem[A_SCORE + 2] | (mem[A_SCORE + 3] << 8));
  printf("puntos: jugador 0 = %d, jugador 1 = %d\n", s0, s1);
  CHECK(r2.find(std::to_string(s1)) != std::string::npos && r3.find(std::to_string(s0)) != std::string::npos, "el panel muestra mis puntos (jugador 1) y los del rival");
  CHECK(text_row(7, 18, 32).substr(0, 10) == "RIVAL TURN" || text_row(7, 18, 32).substr(0, 9) == "YOUR TURN", "el turno");
  CHECK(text_row(9, 18, 32).substr(0, 9) == "LAST MOVE" && text_row(10, 18, 32).substr(0, 4) == "EASY", "la ultima jugada: EASY");
  // numeros negativos y grandes en el panel
  mem[A_SCORE] = 0xF6; mem[A_SCORE + 1] = 0xFF; mem[A_SCORE + 2] = 0xE7; mem[A_SCORE + 3] = 0x03;   // -10 y 999
  call_ep(E_PANEL);
  CHECK(text_row(3, 18, 32).find("-10") != std::string::npos && text_row(2, 18, 32).find("999") != std::string::npos, "puntos negativos y de tres cifras");
  mem[A_SCORE] = 0; mem[A_SCORE + 1] = 0; mem[A_SCORE + 2] = 0; mem[A_SCORE + 3] = 0; call_ep(E_PANEL);
  CHECK(text_row(2, 18, 32).find("0") != std::string::npos && text_row(2, 18, 32).find("999") == std::string::npos, "y a cero");
  // las filas de texto
  puts_mem(0x9F00, "YOUR MOVE. TYPE E.G. H8 R QUIZ"); call_ep(E_STATUS);
  puts_mem(0x9F00, "NOT IN THE DICTIONARY"); call_ep(E_MSG);
  puts_mem(0x9F00, "ENTER=PLAY  P=PASS  X=EXCHANGE"); call_ep(E_HELP);
  puts_mem(0x9F00, "H8 R QUIZ"); call_ep(E_INPUT);
  CHECK(text_row(20).find("YOUR MOVE") == 0 && text_row(21).find("NOT IN THE DICTIONARY") == 0 && text_row(22).find("ENTER") == 0 && text_row(23).find("H8 R QUIZ") == 0, "las filas de estado, aviso, ayuda y entrada");
  CHECK(attr(20, 5) == 0x60 && attr(21, 5) == 0x42 && attr(22, 5) == 0x05, "con sus colores");
  dump("textos");

  // el castellano: las fichas N con tilde, CH, LL y RR en el atril y sobre el tablero
  mem[SU + 1] = 0;                                         // partida nueva: sbu_reset (la ultima jugada, fuera)
  mem[A_LANG] = 1;
  for (int i = 0; i < 4; i++) mem[A_SEED + i] = (777 >> (8 * i)) & 255;
  call_ep(E_INIT);
  mem[A_ME] = 0;
  setrack(0, "[\\]^A?E");                                   // N con tilde, CH, LL, RR, A, comodin, E
  mem[A_BOARD + 7 * 15 + 5] = '[';  mem[A_BOARD + 7 * 15 + 6] = '\\'; mem[A_BOARD + 7 * 15 + 7] = ']';
  mem[A_BOARD + 8 * 15 + 7] = '^'; mem[A_BOARD + 9 * 15 + 7] = (uint8_t)('A' | 0x80);
  call_ep(E_DRAW);
  dump("castellano");
  for (int i = 0; i < 4; i++) {
    bool any = false; for (int k = 0; k < 8; k++) if (cell_byte(BOARD_ROW + 7 + (i == 3), BOARD_COL + 5 + (i == 3 ? 2 : i), k)) any = true;
    CHECK(any, "las fichas especiales del castellano estan dibujadas en el tablero");
  }
  { std::string s = text_row(19, 2, 24);          // [ \ ] ^ A ? E: 8 5 8 8 1 0 1
    CHECK(s[0] == '8' && s[3] == '5' && s[6] == '8' && s[9] == '8' && s[12] == '1' && s[18] == '1', "valores en castellano: N con tilde 8, CH 5, LL 8, RR 8"); }

  // ===================================================================================================
  //  Teclear la jugada (scrabble_in.inc)
  // ===================================================================================================
  // teclas: '<' = borrar (SHIFT+0, 8), '\n' = ENTER (13); devuelve lo que contesto la ultima
  auto keys = [&](const char* s) {
    int r = 0;
    for (; *s; s++) { mem[0x9FF1] = (*s == '<') ? 8 : (*s == '\n') ? 13 : (uint8_t)*s; r = call_ep(E_KEY); }
    return r;
  };
  auto mode = [&]() { return (int)mem[SI + 41]; };
  auto sx = [&]() { return (int)mem[SI + 42]; };
  auto sy = [&]() { return (int)mem[SI + 43]; };
  auto sdir = [&]() { return (int)mem[SI + 44]; };
  auto sn = [&]() { return (int)mem[SI + 45]; };
  auto stage = [&]() { return (int)mem[SI + 62]; };
  auto tiles = [&]() { std::string s; for (int i = 0; i < sn(); i++) { uint8_t c = mem[SI + 46 + i]; if (c & 0x80) s += '.'; s += (char)(c & 0x7F); } return s; };
  auto kn = [&]() { return (int)mem[SI]; };
  auto fresh = [&](int lang) { mem[A_LANG] = (uint8_t)lang; call_ep(E_ICLEAR); };

  fresh(0);
  CHECK(keys("H8RQUIZ") == 0 && mode() == 1 && sx() == 7 && sy() == 7 && sdir() == 0 && sn() == 4 && tiles() == "QUIZ" && stage() == 5, "H8 R QUIZ: columna H (7), fila 8 (7), derecha, 4 fichas");
  CHECK(keys("\n") == 1, "ENTER con la jugada completa: 1");
  fresh(0); keys("H8 R QUIZ");
  CHECK(kn() == 7 && tiles() == "QUIZ", "los espacios no cuentan");
  fresh(0); keys("A10DWORD");
  CHECK(sx() == 0 && sy() == 9 && sdir() == 1 && tiles() == "WORD", "A10 D: la fila 10 (dos cifras)");
  fresh(0); keys("O15DAB");
  CHECK(sx() == 14 && sy() == 14 && sn() == 1, "O15 D: solo cabe una ficha en la ultima fila");
  CHECK(keys("\n") == 2, "con una sola ficha no se puede jugar: ENTER da 2");
  fresh(0); keys("A1R");
  CHECK(sy() == 0 && sdir() == 0 && stage() == 5, "A1 R: la fila 1 (un 1 y ya el sentido)");
  fresh(0); keys("A12R");
  CHECK(sy() == 11, "A12 R: la fila 12");
  fresh(0); keys("A15R"); CHECK(sy() == 14, "A15 R: la fila 15");
  fresh(0); keys("A16");
  CHECK(kn() == 3 && sdir() == 1 && sy() == 0, "A1 6: despues de un 1, el 6 es el sentido hacia abajo (las filas llegan a 15)");
  fresh(0); keys("A17");
  CHECK(kn() == 2, "A17: el 7 no vale despues de un 1");
  // teclas que no cuadran en esa posicion
  fresh(0); keys("Z");  CHECK(kn() == 0, "la columna Z no existe");
  keys("A0");           CHECK(kn() == 1, "la fila 0 no existe");
  keys("A");            CHECK(kn() == 1, "dos letras seguidas: la segunda se rechaza");
  keys("5");            CHECK(kn() == 2, "A5");
  keys("5");            CHECK(kn() == 2, "A55: no hay filas de dos cifras con un 5");
  keys("X");            CHECK(kn() == 2, "A5X: el sentido es R o D");
  keys("D");            CHECK(kn() == 3 && stage() == 5, "A5D");
  keys("1");            CHECK(kn() == 3, "en ingles, la tecla 1 no es una ficha");
  keys("0");            CHECK(kn() == 3, "ni el 0");
  keys("QU");           CHECK(kn() == 5, "QU");
  // borrar
  keys("<");            CHECK(kn() == 4 && tiles() == "Q", "borrar quita la ultima tecla");
  keys("<<<");          CHECK(kn() == 1 && stage() == 1, "se puede borrar hasta la columna");
  keys("<<");           CHECK(kn() == 0 && mode() == 0, "borrar sin nada no hace nada");
  fresh(0); keys("H1<"); CHECK(kn() == 1, "borrar la cifra");
  // limite del tablero: N8 R solo admite 2 fichas (columnas N y O)
  fresh(0); keys("N8RABC");
  CHECK(sn() == 2 && tiles() == "AB", "una palabra que se saldria del tablero: las demas fichas no entran");
  fresh(0); keys("H15DABC");
  CHECK(sn() == 1, "H15 D: solo una ficha cabe en la fila 15");
  // comodin
  fresh(0); keys("H8R.QUIZ");
  CHECK(tiles() == ".QUIZ" && sn() == 4, "un punto antes de una letra fuerza el comodin");
  fresh(0); keys("H8R.");
  CHECK(stage() == 6 && keys("\n") == 2, "un punto pendiente: no se puede jugar");
  keys("1"); CHECK(stage() == 6, "...y espera una letra (no vale el 1)");
  keys("."); CHECK(stage() == 6, "...ni otro punto");
  keys("<"); CHECK(stage() == 5 && kn() == 3, "borrar el punto");
  // pasar y cambiar
  fresh(0); keys("P");
  CHECK(mode() == 2 && keys("\n") == 1, "P ENTER: pasar");
  keys("A"); CHECK(kn() == 1, "despues de P no entra nada mas");
  keys("<"); CHECK(kn() == 0, "pero se puede borrar");
  fresh(0); keys("XQUI");
  CHECK(mode() == 3 && tiles() == "QUI" && keys("\n") == 1, "X QUI ENTER: cambiar tres fichas");
  call_ep(E_IBUILD);
  CHECK(mem[A_WB + 3] == 3 && mem[A_WB + 4] == 'Q' && mem[A_WB + 5] == 'U' && mem[A_WB + 6] == 'I', "la jugada de cambio va a WB+3 y WB+4");
  fresh(0); keys("XQ.A");
  call_ep(E_IBUILD);
  CHECK(mem[A_WB + 3] == 2 && mem[A_WB + 4] == 'Q' && mem[A_WB + 5] == '?', "un comodin que se cambia es '?' en el atril");
  fresh(0); keys("XABCDEFGH");
  CHECK(sn() == 7, "se cambian como mucho 7 fichas");
  fresh(0); keys("X1"); CHECK(sn() == 0, "(en ingles no hay N con tilde)");
  CHECK(keys("\n") == 2, "X sin fichas: ENTER da 2");
  // la jugada que llega al motor
  fresh(0); keys("H8RQUIZ");
  CHECK(call_ep(E_IBUILD) == 1, "build devuelve 1: una jugada");
  CHECK(mem[A_WB] == 7 && mem[A_WB + 1] == 7 && mem[A_WB + 2] == 0 && mem[A_WB + 3] == 4 && memcmp(mem + A_WB + 4, "QUIZ", 4) == 0, "WB = x, y, sentido, n, fichas");
  fresh(0); keys("D4D.AB");
  call_ep(E_IBUILD);
  CHECK(mem[A_WB] == 3 && mem[A_WB + 1] == 3 && mem[A_WB + 2] == 1 && mem[A_WB + 3] == 2 && mem[A_WB + 4] == ('A' | 0x80) && mem[A_WB + 5] == 'B', "D4 D .A B: el comodin forzado lleva el bit 7");
  fresh(0); keys("P");
  CHECK(call_ep(E_IBUILD) == 2, "build devuelve 2 con P");
  // castellano
  fresh(1);
  keys("H8RCHICO");
  CHECK(sn() == 4 && mem[SI + 46] == 0x5C && tiles() == "\\ICO", "castellano: C H se junta en la ficha CH");
  keys("<<<");
  CHECK(tiles() == "\\" && kn() == 4 + 1, "borrar hasta el CH...");
  keys("<"); CHECK(tiles() == "C" && sn() == 1, "...y borrar la H deja la C sola");
  fresh(1); keys("H8RCALLE");
  CHECK(tiles() == "CA]E", "CALLE: C A LL E (la LL es una ficha)");
  fresh(1); keys("H8RLLLL");
  CHECK(tiles() == "]]", "LLLL: dos fichas LL");
  fresh(1); keys("H8RLLL");
  CHECK(tiles() == "]L", "LLL: LL y una L");
  fresh(1); keys("H8RPERRO");
  CHECK(tiles() == "PE^O", "PERRO: P E RR O");
  fresh(1); keys("H8R1");
  CHECK(tiles() == "[", "la tecla 1 es la N con tilde");
  keys("1A"); CHECK(tiles() == "[[A", "dos N con tilde y una A");
  fresh(1); keys("H8RC.H");
  CHECK(sn() == 2 && tiles() == "C.H", "un comodin H detras de una C no se junta con ella");
  fresh(1); keys("H8R.CH");
  CHECK(sn() == 2 && tiles() == ".CH", "ni uno C: el comodin no se junta");
  fresh(1); keys("XCH");
  CHECK(tiles() == "\\", "en un cambio tambien: CH es una ficha");

  // ===================================================================================================
  //  El cursor y la flecha de sentido (sprites por hardware) y las teclas 5-8 y 0
  // ===================================================================================================
  auto cxv = [&]() { return (int)mem[SI + 124]; };
  auto cyv = [&]() { return (int)mem[SI + 125]; };
  // el sprite n esta encendido sobre la casilla (col, fila) del tablero? (las coordenadas de sprite llevan 32 de mas)
  auto spr_at = [&](int n, int col, int row) { const Spr& s = sprs[n]; return s.en && s.x == (BOARD_COL + col) * 8 + 32 && s.y == (BOARD_ROW + row) * 8 + 32; };
  auto any_spr_on = [&]() { return sprs[0].en || sprs[1].en || sprs[2].en; };

  fresh(0);
  call_ep(E_SPRINIT);
  CHECK(!any_spr_on(), "los sprites se definen apagados");
  { static const uint8_t ring[8] = {0xFF, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0xFF};
    CHECK(memcmp(sprs[0].pix, ring, 8) == 0 && memcmp(sprs[0].msk, ring, 8) == 0 && sprs[0].col[3] == 0x00, "el cursor es un marco de 8x8 de tinta negra y el centro transparente");
    CHECK(sprs[1].pix[3] == 0xFE && sprs[1].msk[3] == 0xFF && sprs[1].col[0] == 0x0F, "la flecha a la derecha: negra con el borde blanco");
    CHECK(sprs[2].pix[4] == 0x7E && sprs[2].msk[7] == 0x3C && sprs[2].col[7] == 0x0F, "y la de abajo"); }
  CHECK(cxv() == 7 && cyv() == 7, "el cursor empieza en el centro (H8)");
  call_ep(E_IDRAW);
  CHECK(spr_at(0, 7, 7) && !sprs[1].en && !sprs[2].en, "con nada tecleado hay cursor, sobre H8, y ninguna flecha");
  keys("8"); call_ep(E_IDRAW);
  CHECK(cxv() == 8 && cyv() == 7 && kn() == 0 && spr_at(0, 8, 7), "8 mueve el cursor a la derecha (y no se guarda como tecla)");
  keys("6"); call_ep(E_IDRAW);
  CHECK(cxv() == 8 && cyv() == 8 && spr_at(0, 8, 8), "6, hacia abajo");
  keys("7"); keys("7"); call_ep(E_IDRAW);
  CHECK(cyv() == 6 && spr_at(0, 8, 6), "7, hacia arriba");
  keys("5"); call_ep(E_IDRAW);
  CHECK(cxv() == 7 && spr_at(0, 7, 6), "5, hacia la izquierda");
  for (int i = 0; i < 20; i++) keys("5");
  for (int i = 0; i < 20; i++) keys("7");
  CHECK(cxv() == 0 && cyv() == 0, "el cursor no sale del tablero por la izquierda ni por arriba");
  for (int i = 0; i < 20; i++) keys("8");
  for (int i = 0; i < 20; i++) keys("6");
  CHECK(cxv() == 14 && cyv() == 14, "ni por la derecha ni por abajo");
  // elegir la casilla: 0 teclea la columna y la fila
  fresh(0); call_ep(E_SPRINIT);
  keys("0");
  CHECK(kn() == 2 && sx() == 7 && sy() == 7 && stage() == 3 && mode() == 1, "0 elige la casilla del cursor: H8 (columna y fila como si se hubieran tecleado)");
  call_ep(E_IDRAW);
  CHECK(spr_at(0, 7, 7) && !sprs[1].en && !sprs[2].en, "el cursor sigue sobre la casilla elegida y aun no hay flecha");
  CHECK(text_row(22).find("8=RIGHT 6=DOWN") == 0, "la ayuda pide el sentido: 8 derecha, 6 abajo");
  keys("8"); call_ep(E_IDRAW);
  CHECK(stage() == 5 && sdir() == 0 && spr_at(1, 8, 7) && !sprs[2].en && spr_at(0, 7, 7), "8: sentido a la derecha, con la flecha en la casilla de al lado");
  CHECK(text_row(23, 0, 5) == "H8 R ", "y se ve lo tecleado: H8 R");
  keys("<"); call_ep(E_IDRAW);
  CHECK(stage() == 3 && !sprs[1].en && !sprs[2].en, "borrar el sentido quita la flecha");
  keys("6"); call_ep(E_IDRAW);
  CHECK(stage() == 5 && sdir() == 1 && spr_at(2, 7, 8) && !sprs[1].en, "6: sentido hacia abajo, con la flecha en la casilla de debajo");
  keys("QU");
  CHECK(sn() == 2 && tiles() == "QU", "y se escribe la palabra");
  keys("<<<<"); call_ep(E_IDRAW);
  CHECK(kn() == 1 && stage() == 1, "se puede borrar hasta la columna");
  keys("<"); call_ep(E_IDRAW);
  CHECK(kn() == 0 && spr_at(0, 7, 7), "y de vuelta a mover el cursor, que se ha quedado donde estaba");
  // filas de dos cifras y la primera
  fresh(0); call_ep(E_SPRINIT);
  for (int i = 0; i < 2; i++) keys("6");                      // fila 10 (y = 9)
  keys("0");
  CHECK(sx() == 7 && sy() == 9 && stage() == 3 && kn() == 3, "0 en la fila 10 teclea H, 1 y 0");
  keys("6"); CHECK(stage() == 5 && sdir() == 1, "y 6 es abajo");
  fresh(0); call_ep(E_SPRINIT);
  for (int i = 0; i < 20; i++) keys("7");
  keys("0");
  CHECK(sy() == 0 && stage() == 2, "0 en la fila 1 teclea H y 1: aun puede ser 10-15 (etapa 2)");
  keys("8"); CHECK(stage() == 5 && sdir() == 0 && sy() == 0, "y 8 ya es el sentido: derecha");
  fresh(0); call_ep(E_SPRINIT);
  for (int i = 0; i < 20; i++) keys("6");
  keys("0");
  CHECK(sy() == 14 && stage() == 3 && kn() == 3, "0 en la fila 15 teclea H, 1 y 5");
  // teclear la casilla a mano mueve el cursor
  fresh(0); call_ep(E_SPRINIT);
  keys("C5R"); call_ep(E_IDRAW);
  CHECK(spr_at(0, 2, 4) && spr_at(1, 3, 4), "teclear C5 R pone el cursor en C5 y la flecha a su derecha");
  fresh(0); call_ep(E_SPRINIT);
  keys("O3D"); call_ep(E_IDRAW);
  CHECK(spr_at(0, 14, 2) && spr_at(2, 14, 3) && !sprs[1].en, "O3 D: abajo");
  fresh(0); call_ep(E_SPRINIT);
  keys("O3R"); call_ep(E_IDRAW);
  CHECK(spr_at(0, 14, 2) && !sprs[1].en && !sprs[2].en, "en la ultima columna no hay casilla a la derecha: sin flecha");
  // las teclas 5-8 y 0 dentro de la palabra no son flechas
  fresh(0); call_ep(E_SPRINIT);
  keys("H8RQ5"); CHECK(tiles() == "Q" && kn() == 4, "dentro de la palabra el 5 no entra");
  keys("0"); CHECK(kn() == 4, "ni el 0");
  keys("8"); CHECK(kn() == 4, "ni el 8");
  // sin cursor
  fresh(0); call_ep(E_SPRINIT); keys("P"); call_ep(E_IDRAW);
  CHECK(!any_spr_on(), "pasar: sin cursor");
  fresh(0); call_ep(E_SPRINIT); keys("XQU"); call_ep(E_IDRAW);
  CHECK(!any_spr_on(), "cambiar fichas: sin cursor");
  fresh(0); call_ep(E_SPRINIT); mem[SI + 128] = 1; call_ep(E_IDRAW);
  CHECK(!any_spr_on(), "con SI_HIDE (partida acabada): sin cursor");
  mem[SI + 128] = 0; call_ep(E_IDRAW);
  CHECK(sprs[0].en, "y con SI_HIDE a 0 vuelve");
  call_ep(E_SPROFF);
  CHECK(!any_spr_on(), "sbi_spr_off apaga los tres");
  // el parpadeo: cada 16 vueltas el cursor aparece o desaparece cambiando la MASCARA (el color y los pixeles no cambian)
  { static const uint8_t ring[8] = {0xFF, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0xFF};
    static const uint8_t zero[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    fresh(0); call_ep(E_SPRINIT); call_ep(E_IDRAW);
    uint8_t pix0[8], col0[8]; memcpy(pix0, sprs[0].pix, 8); memcpy(col0, sprs[0].col, 8);
    CHECK(memcmp(sprs[0].msk, ring, 8) == 0, "el cursor empieza visible: la mascara es el marco");
    for (int i = 0; i < 15; i++) call_ep(E_BLINK);
    CHECK(memcmp(sprs[0].msk, ring, 8) == 0, "15 vueltas despues sigue visible");
    call_ep(E_BLINK);
    CHECK(memcmp(sprs[0].msk, zero, 8) == 0, "a la 16 desaparece: mascara toda a 0 (no tapa ningun punto de las letras)");
    CHECK(memcmp(sprs[0].pix, pix0, 8) == 0 && memcmp(sprs[0].col, col0, 8) == 0, "sin tocar los pixeles ni el color");
    for (int i = 0; i < 16; i++) call_ep(E_BLINK);
    CHECK(memcmp(sprs[0].msk, ring, 8) == 0, "y 16 vueltas mas tarde vuelve");
    call_ep(E_IDRAW);
    CHECK(sprs[0].en, "(el sprite se queda encendido todo el rato: solo cambia la mascara)");
    for (int i = 0; i < 16; i++) call_ep(E_BLINK);
    keys("8"); call_ep(E_IDRAW);                      // mover el cursor con la mascara a 0 no la cambia (el parpadeo sigue su ritmo)
    CHECK(sprs[0].en && memcmp(sprs[0].msk, zero, 8) == 0, "moverlo mientras esta oculto no lo hace aparecer fuera de turno");
  }
  // los textos de ayuda
  fresh(0); call_ep(E_IDRAW);
  CHECK(text_row(22).find("5-8=MOVE 0=OK P=PASS X=SWAP") == 0, "la ayuda del principio: 5-8 mueven, 0 elige");
  fresh(0); keys("H1"); call_ep(E_IDRAW);
  CHECK(text_row(22).find("8=RIGHT 6=DOWN") == 0, "tecleando H1 (puede ser 10-15): 8 y 6 siguen siendo el sentido");

  // ---- lo que se ve en la fila de entrada y en la ayuda
  fresh(0);
  call_ep(E_IDRAW);
  CHECK(text_row(22).find("5-8=MOVE") == 0, "ayuda al empezar: la columna, P y X");
  keys("H8R"); call_ep(E_IDRAW);
  CHECK(text_row(23, 0, 5) == "H8 R ", "la fila de entrada: H8 R");
  keys("QU"); call_ep(E_IDRAW);
  CHECK(text_row(23, 0, 7) == "H8 R QU", "...y las fichas tecleadas");
  CHECK(attr(23, 5) == 0x78 && attr(23, 6) == 0x78, "las fichas, en blanco como las del tablero");
  { bool full = true; for (int k = 0; k < 8; k++) if (cell_byte(23, 7, k) != 0xFF) full = false; CHECK(full, "el cursor es un bloque justo despues"); }
  CHECK(text_row(22).find("LETTERS") == 0, "ayuda de la palabra");
  keys(".A"); call_ep(E_IDRAW);
  CHECK(attr(23, 7) == 0x79, "una ficha de comodin forzado, en el color del comodin");
  keys("."); call_ep(E_IDRAW);
  CHECK(text_row(22).find("WHICH LETTER") == 0, "con un punto pendiente, la ayuda pregunta la letra");
  fresh(0); keys("H"); call_ep(E_IDRAW);
  CHECK(text_row(23, 0, 1) == "H" && text_row(22).find("ROW NUMBER (1-15)") == 0, "tras la columna: la fila");
  keys("1"); call_ep(E_IDRAW);
  CHECK(text_row(22).find("8=RIGHT 6=DOWN") == 0, "tras un 1: 8, 6 (R o D) o 0-5");
  keys("0"); call_ep(E_IDRAW);
  CHECK(text_row(23, 0, 3) == "H10" && text_row(22).find("8=RIGHT 6=DOWN") == 0, "H10: la fila de dos cifras, y despues el sentido");
  fresh(0); keys("P"); call_ep(E_IDRAW);
  CHECK(text_row(23, 0, 4) == "PASS" && text_row(22).find("ENTER = PASS") == 0, "PASS en la fila de entrada y su ayuda");
  fresh(0); keys("XQU"); call_ep(E_IDRAW);
  CHECK(text_row(23, 0, 8) == "SWAP: QU" && text_row(22).find("TILES TO SWAP") == 0, "SWAP: QU");
  fresh(1); keys("H8R"); call_ep(E_IDRAW);
  CHECK(text_row(22).find("1=ENYE") == 0, "en castellano la ayuda dice que 1 es la N con tilde");
  keys("CH"); call_ep(E_IDRAW);
  CHECK(text_row(23, 0, 5) == "H8 R " && attr(23, 5) == 0x78, "la ficha CH ocupa una sola celda");
  { bool full = true; for (int k = 0; k < 8; k++) if (cell_byte(23, 6, k) != 0xFF) full = false; CHECK(full, "...y el cursor queda en la celda de al lado"); }
  dump("entrada");

  // ---- el juego: teclear, comprobar con el motor y ver el aviso
  mem[A_LANG] = 0;
  for (int i = 0; i < 4; i++) mem[A_SEED + i] = (99 >> (8 * i)) & 255;
  call_ep(E_INIT); call_ep(E_DOPEN);
  auto msg = [&](int code) {
    mem[0x9FF1] = (uint8_t)code; int p = call_ep(E_ERRMSG);
    std::string s; for (int i = 0; i < 40 && mem[p + i]; i++) s += (char)mem[p + i]; return s;
  };
  auto try_move = [&](const char* typed, const char* rack) {
    fresh(0);
    keys(typed); keys("\n");
    mem[A_TURN] = 0; setrack(0, rack);
    call_ep(E_IBUILD);
    return call_ep(E_CHECK);
  };
  CHECK(try_move("H8RQUIZ", "QUIZZES") == 0 && z_i16_ui(A_TOT) == 44, "H8 R QUIZ con las fichas: legal y vale 44");
  int e;
  e = try_move("H8RQUIZ", "ABCDEFG");     CHECK(e == 2 && msg(e) == "YOU DO NOT HAVE THOSE TILES", "sin las fichas: error 2");
  e = try_move("A1RQUIZ", "QUIZZES");     CHECK(e == 6 && msg(e) == "FIRST WORD MUST COVER THE CENTER", "la primera jugada lejos del centro: error 6");
  e = try_move("H8RQUZZ", "QUIZZES");     CHECK(e == 8 && msg(e) == "QUZZ IS NOT A WORD", "una palabra que no existe: el aviso lleva la palabra");
  CHECK(msg(1) == "BAD POSITION OR LETTER" && msg(4) == "PLACE AT LEAST ONE TILE" && msg(5) == "THE WORD WOULD BE LONGER" && msg(11) == "THE GAME IS OVER", "textos de los avisos");
  CHECK(msg(7) == "THE WORD MUST TOUCH THE TILES" && msg(3) == "LETTER DOES NOT MATCH THE BOARD", "mas textos");
  mem[SI + 41] = 3;   CHECK(msg(4) == "THE BAG HAS LESS THAN 7 TILES", "cambiar con la bolsa casi vacia: aviso propio");
  // castellano: la palabra que falla con las fichas dobles escritas en letras
  mem[0x9400 + 17] = 0x5C; mem[0x9400 + 18] = 'A'; mem[0x9400 + 19] = ']'; mem[0x9400 + 20] = 0; mem[SI + 41] = 1;
  CHECK(msg(8) == "CHALL IS NOT A WORD", "una palabra con CH y LL: sale en letras");
  CHECK(msg(9) == "CHALL (CROSSING) IS NOT A WORD", "y la cruzada");

  if (errors) printf("%d ERRORES\n", errors); else printf("TODO OK\n");
  return errors ? 1 : 0;
}
