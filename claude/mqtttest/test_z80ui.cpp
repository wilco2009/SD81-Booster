// Prueba del juego entero (EXAMPLES/NETGAME/connect4.bin) en el PC: el programa de verdad en el Z80
// de EightyOne, con el teclado del ZX81 de mentira (pulsaciones con tiempo), la pantalla leida de la
// memoria (el lobby en texto, en $8800; la partida en HiRes Spectrum: bitmap $A000 y atributos $B800,
// como la leeria la FPGA), el modem de verdad y un rival hecho aqui por el broker.
// Con SHOW=1 imprime la pantalla en cada paso importante; con SCRDIR=<carpeta>, guarda la de la partida (.SCR).
#include "z80net_env.h"

static const uint16_t DFILE = 0x8800;           // la pantalla de texto del lobby
static const uint16_t HFILE = 0xA000, ATTRB = 0xB800;   // la pantalla del juego: HiRes Spectrum
static const int BOARD_ROW = 2, BOARD_COL = 1;  // la celda de arriba a la izquierda del tablero (7 x 6 casillas de 3 x 3 celdas)

// ------------------------------------------------------------ la pantalla
// la fuente de la ROM ($1E00) en el PC: sintetica, un dibujo distinto por codigo (el 0, el espacio, en blanco)
static void install_font() {
  for (int c = 1; c < 64; c++) for (int k = 0; k < 8; k++) mem[0x1E00 + c * 8 + k] = (uint8_t)(c * 3 + 1 + k * 17);
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
static char zx_char(int v) {
  if (v == 0) return ' ';
  if (v >= 28 && v <= 37) return (char)('0' + v - 28);
  if (v >= 38 && v <= 63) return (char)('A' + v - 38);
  if (v == 27) return '.'; if (v == 14) return ':'; if (v == 22) return '-';
  if (v == 15) return '?'; if (v == 18) return '>'; if (v == 19) return '<';
  if (v == 20) return '='; if (v == 21) return '+'; if (v == 23) return '*'; if (v == 24) return '/';
  if (v == 25) return ';'; if (v == 26) return ','; if (v == 16) return '('; if (v == 17) return ')';
  if (v == 11) return '"'; if (v == 13) return '$';
  return '~';
}
static bool gfx_mode() { return mem[2045] == 172; }
static std::string screen_row(int r) {
  std::string s;
  for (int c = 0; c < 32; c++) {
    if (gfx_mode()) {
      bool in_board = r >= BOARD_ROW && r < BOARD_ROW + 18 && c >= BOARD_COL && c < BOARD_COL + 21;
      s += in_board ? ' ' : zx_char(glyph_code(r, c));
    } else {
      int v = mem[DFILE + 1 + 33 * r + c];
      s += v == 128 ? '#' : v == 8 ? '%' : zx_char(v);
    }
  }
  return s;
}
static std::string screen_all() { std::string s; for (int r = 0; r < 24; r++) s += screen_row(r) + "\n"; return s; }
static bool shows(const char* t) { return screen_all().find(t) != std::string::npos; }
static bool show_on = getenv("SHOW") != nullptr;

// El tablero: la casilla de la columna x (0-6) y la fila de juego y (0 = abajo) es un disco de 3 x 3 celdas.
// Se mira la celda del medio: la tinta dice la ficha (0 hueco, 2 roja = jugador 1, 6 amarilla = jugador 2) y el
// papel si esta resaltada (7 blanco; el azul del tablero es 1). 'R' / 'Y' / '.' ; '?' si no se ve un disco entero
static int disc_row(int y) { return BOARD_ROW + 3 * (5 - y) + 1; }
static int disc_col(int x) { return BOARD_COL + 3 * x + 1; }
static uint8_t disc_attr(int x, int y) { return mem[ATTRB + disc_row(y) * 32 + disc_col(x)]; }
static char disc(int x, int y) {
  for (int k = 0; k < 8; k++) if (cell_byte(disc_row(y), disc_col(x), k) != 0xFF) return '?';   // el centro del disco, entero
  int ink = disc_attr(x, y) & 7;
  return ink == 2 ? 'R' : ink == 6 ? 'Y' : ink == 0 ? '.' : '?';
}
static bool hilite(int x, int y) { return ((disc_attr(x, y) >> 3) & 7) == 7; }
static std::string board_row(int y) { std::string s; for (int x = 0; x < 7; x++) s += disc(x, y); return s; }
static std::string board_all() { std::string s; for (int y = 5; y >= 0; y--) s += board_row(y) + (y ? "/" : ""); return s; }
// el tablero de una Board (abajo) con el mismo formato que board_all(): filas de arriba a abajo separadas por '/'
static std::string board_text(const uint8_t* b) {
  std::string s;
  for (int y = 5; y >= 0; y--) { for (int x = 0; x < 7; x++) s += b[x + 7 * y] == 1 ? 'R' : b[x + 7 * y] == 2 ? 'Y' : '.'; if (y) s += '/'; }
  return s;
}
static const char* EMPTY_BOARD = "......./......./......./......./......./.......";
static bool board_is(const char* rows_from_top) {         // "fila de arriba/.../fila de abajo", con '/' entre filas
  return board_all() == rows_from_top;
}
static int scr_n = 0;
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
  for (int r = 0; r < 24; r++) {
    std::string l = screen_row(r);
    if (gfx_mode() && r >= BOARD_ROW && r < BOARD_ROW + 18 && (r - BOARD_ROW) % 3 == 1) {     // el tablero, una linea por fila
      int y = 5 - (r - BOARD_ROW) / 3;
      std::string b;
      for (int x = 0; x < 7; x++) { b += disc(x, y); b += hilite(x, y) ? '*' : ' '; }
      l.replace(BOARD_COL, 21, std::string(b) + "      ");
    }
    while (!l.empty() && l.back() == ' ') l.pop_back();
    if (!l.empty()) printf("%2d|%s\n", r, l.c_str());
  }
}

// ------------------------------------------------------------ el tiempo y el teclado
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
  if (c == 8) {                                        // borrar = SHIFT + 0
    keyrows[0] |= 1; keyrows[4] |= 1;
    run_ms(hold_ms); keyrows[0] = keyrows[4] = 0; run_ms(120); return;
  }
  if (!key_pos(c, &k)) { printf("ERROR tecla desconocida %d\n", c); errors++; return; }
  keyrows[k.row] |= (uint8_t)(1 << k.bit);
  run_ms(hold_ms);
  keyrows[k.row] &= (uint8_t)~(1 << k.bit);
  run_ms(120);
}
static void type(const char* s) { for (; *s; s++) press(*s); }

// ------------------------------------------------------------ el rival
static std::string code_on_screen() {
  std::string c;
  for (int i = 0; i < 10; i++) { int v = mem[DFILE + 1 + 33 * 6 + 11 + i]; c += (char)(v >= 38 ? v + 27 : v + 20); }
  return c;
}
struct Board {
  uint8_t b[42]; uint8_t h[7];
  void reset() { memset(b, 0, sizeof(b)); memset(h, 0, sizeof(h)); }
  void apply(int c, int p) { b[c + 7 * h[c]] = (uint8_t)p; h[c]++; }
};
static uint16_t hash16(const uint8_t* s, int n) {
  uint16_t h = 0x5A5A;
  for (int i = 0; i < n; i++) {
    h = (uint16_t)((h & 0xFF00) | ((h & 0xFF) ^ s[i]));
    for (int r = 0; r < 3; r++) h = (uint16_t)((h << 1) | (h >> 15));
  }
  return h;
}
static std::string move_payload(int col, const Board& after) {
  std::string s; s += (char)1; s += (char)col;
  uint16_t h = hash16(after.b, 42); s += (char)(h & 255); s += (char)(h >> 8);
  return s;
}
static std::string body_of(const Seen* s) { return s ? s->data.substr(5, s->data.size() - 13) : std::string(); }

int main(int argc, char** argv) {
  if (argc < 2) { printf("uso: test_z80ui connect4.bin\n"); return 2; }
  FILE* f = fopen(argv[1], "rb");
  if (!f) { printf("no encuentro %s\n", argv[1]); return 2; }
  size_t sz = fread(mem + 0x6000, 1, 0x2000, f); fclose(f);
  if (0x6000 + sz > 0x8000) { printf("ERROR el programa llega a $%04zX: pasa de $8000 y alli el ZX81 no ejecuta codigo\n", 0x6000 + sz); errors++; }
  printf("connect4.bin: %zu bytes en $6000\n", sz);
  std::vector<uint8_t> bin(mem + 0x6000, mem + 0x6000 + sz);
  // apagar y encender el ZX81: se pierde la RAM, la SD (sdfiles) y el broker (los retenidos) no
  auto reboot = [&]() {
    auto keep = retained;
    memset(mem, 0, sizeof(mem));
    memcpy(mem + 0x6000, bin.data(), bin.size());
    install_font();
    mem[0x02E7] = 0xC9; mem[0x0207] = 0xC9;
    z80_init(); z80_reset();
    reset_world();
    retained = keep;
    z80.sp.w = 0xFFF0; mem[0xFFF0] = 0x00; mem[0xFFF1] = 0x01; mem[0x0100] = 0x76;
    z80.pc.w = 24576; z80.iff1 = z80.iff2 = 0;
  };
  install_font();
  mem[0x02E7] = 0xC9; mem[0x0207] = 0xC9;              // SET_FAST y SLOW_FAST de la ROM: aqui, un RET
  z80_init(); z80_reset();
  reset_world();
  z80.sp.w = 0xFFF0; mem[0xFFF0] = 0x00; mem[0xFFF1] = 0x01; mem[0x0100] = 0x76;   // si vuelve, a la trampa
  z80.pc.w = 24576; z80.iff1 = z80.iff2 = 0;

  // --- arranque y menu
  CHECK(wait_text("1 = NEW GAME", 20000), "arranca y llega al menu (tras iniciar el modem)");
  show("menu");
  CHECK(shows("2 = JOIN A GAME") && shows("Q = QUIT"), "el menu tiene las tres opciones");
  CHECK(mem[2045] == 170 && mem[2096] == 0x00 && mem[2097] == 0x88 && mem[2098] == 170, "Superfast texto con la pantalla alternativa en $8800");

  // --- partida nueva
  press('1');
  CHECK(wait_text("ROOM CODE:", 30000), "partida nueva: sale el codigo de sala");
  CHECK(wait_text("WAITING FOR THE RIVAL", 10000), "y espera al rival");
  std::string code = code_on_screen();
  printf("  codigo en pantalla: %s\n", code.c_str());
  std::string base = topic_base(code);
  show("esperando al rival");
  const char* alpha = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
  bool ok = code.size() == 10; for (char ch : code) if (!strchr(alpha, ch)) ok = false;
  CHECK(ok, "el codigo en pantalla es valido");
  CHECK(last_seen("SUBSCRIBE") && last_seen("SUBSCRIBE")->topic == base + "/+", "la sala existe en el broker con el codigo que sale en pantalla");
  uint16_t pseq = 1000;
  auto peer_msg = [&](uint16_t ply, std::string payload) { peer_publish(base + "/1", make_msg(code, pseq++, ply, payload), false); };

  peer_publish(base + "/p1", "1", true);
  CHECK(wait_text("YOUR TURN: PRESS 1 TO 7", 10000), "el rival entra: empieza la partida y me toca");
  show("la partida");
  CHECK(gfx_mode(), "la partida se dibuja en HiRes Spectrum");
  CHECK(shows("NETWORK CONNECT 4") && shows("HERE") && shows("YELLOW") == false && shows("RED"), "titulo, el rival esta aqui y soy el rojo (jugador 1)");
  { std::string n = screen_row(1);
    CHECK(n[2] == '1' && n[5] == '2' && n[8] == '3' && n[11] == '4' && n[14] == '5' && n[17] == '6' && n[20] == '7', "los numeros 1-7 sobre las columnas"); }
  CHECK(board_all() == "......./......./......./......./......./.......", "el tablero esta dibujado y vacio");
  { bool allblue = true; for (int y = 0; y < 6; y++) for (int x = 0; x < 7; x++) if (hilite(x, y) || (disc_attr(x, y) & 0x78) != 0x48) allblue = false;
    CHECK(allblue, "el fondo del tablero es azul brillante"); }
  CHECK(shows("ROOM") && shows(code.c_str()), "el panel tiene la sala y su codigo");
  CHECK(shows("LAST MOVE") && shows("--------"), "y la ultima jugada, todavia ninguna");

  Board g; g.reset();
  // 1. mi jugada en la columna 1 (tecla 1): cae abajo a la izquierda
  press('1');
  CHECK(wait_text("THE RIVAL IS THINKING", 5000), "juego: ahora le toca al rival");
  g.apply(0, 1);
  CHECK(disc(0, 0) == 'R' && hilite(0, 0), "mi ficha (roja) abajo a la izquierda, resaltada como la ultima");
  CHECK(shows("COLUMN 1"), "y el panel dice la ultima jugada: columna 1");
  { const Seen* s = last_seen("PUBLISH", base + "/0"); std::string b = body_of(s);
    CHECK(b.size() == 4 && b[0] == 1 && b[1] == 0, "la jugada sale al broker: tipo 1, columna 0"); }
  // 2. no me toca: la tecla 3 da un aviso
  press('3');
  CHECK(wait_text("IT IS NOT YOUR TURN", 3000), "no me toca: aviso");
  // 3. el rival juega en la columna 2
  g.apply(1, 2); peer_msg(2, move_payload(1, g));
  CHECK(wait_text("YOUR TURN:", 5000), "el rival juega y me vuelve a tocar");
  CHECK(disc(1, 0) == 'Y' && hilite(1, 0) && !hilite(0, 0), "su ficha (amarilla) a mi lado, y el resaltado pasa a ella");
  CHECK(shows("COLUMN 2"), "ultima jugada: columna 2");
  CHECK(!shows("IT IS NOT YOUR TURN"), "y el aviso se va");
  show("tras la jugada del rival");
  // 4. gano en vertical: 1,2,1,2,1,2,1  (yo columna 0, el rival columna 1)
  press('1'); g.apply(0, 1);
  CHECK(wait_text("THE RIVAL IS THINKING", 5000), "mi segunda jugada"); g.apply(1, 2); peer_msg(4, move_payload(1, g));
  CHECK(wait_text("YOUR TURN:", 5000), "turno otra vez");
  press('1'); g.apply(0, 1);
  CHECK(wait_text("THE RIVAL IS THINKING", 5000), "mi tercera jugada"); g.apply(1, 2); peer_msg(6, move_payload(1, g));
  CHECK(wait_text("YOUR TURN:", 5000), "turno otra vez");
  press('1'); g.apply(0, 1);
  CHECK(wait_text("YOU WIN", 5000), "cuatro en vertical: YOU WIN");
  show("he ganado");
  CHECK(wait_text("ENTER = AGAIN", 2000), "y la ayuda cambia a ENTER = otra partida");
  { bool win4 = true; for (int y = 0; y < 4; y++) if (disc(0, y) != 'R' || !hilite(0, y)) win4 = false;
    bool others = true; for (int y = 0; y < 3; y++) if (disc(1, y) != 'Y' || hilite(1, y)) others = false;
    CHECK(win4, "los cuatro de la columna 1 ganadores, resaltados en blanco");
    CHECK(others, "y las fichas del rival sin resaltar"); }
  press('2');
  CHECK(wait_text("THE GAME IS OVER", 3000), "con la partida acabada, una jugada da un aviso");

  // 5. otra partida
  press('\r');
  CHECK(wait_text("YOUR TURN: PRESS 1 TO 7", 5000) && !shows("YOU WIN"), "ENTER: otra partida, a cero");
  { const Seen* s = last_seen("PUBLISH", base + "/0"); std::string b = body_of(s);
    CHECK(b.size() == 1 && b[0] == 4, "y se lo digo al rival (tipo 4)"); }
  CHECK(board_all() == "......./......./......./......./......./.......", "el tablero esta vacio");
  CHECK(shows("--------"), "y sin ultima jugada");
  show("otra partida");
  // 6. el rival pide otra partida cuando ya hay una a medias
  press('4'); g.reset(); g.apply(3, 1);
  CHECK(wait_text("THE RIVAL IS THINKING", 5000), "juego en la columna 4");
  peer_msg(1, std::string("\x04", 1));
  CHECK(wait_text("YOUR TURN: PRESS 1 TO 7", 5000) && board_all() == "......./......./......./......./......./.......", "el rival pide otra partida: a cero");
  // 7. el rival sale y yo salgo
  peer_publish(base + "/p1", "0", true);
  CHECK(wait_text("THE RIVAL HAS LEFT", 5000) && shows("AWAY") && !shows("HERE"), "el rival se va: aviso");
  show("el rival se fue");
  press('Q');
  CHECK(wait_text("1 = NEW GAME", 30000), "Q: salgo de la sala y vuelvo al menu");
  CHECK(count_seen("DISCONNECT") >= 1, "y me despido del broker");

  // --- unirse a una partida
  seen.clear(); retained.clear(); filters.clear(); has_will = false;     // (el modem sigue como esta: no se reinicia)
  {
    std::string code2 = "MNPQRSTUVW"; std::string base2 = topic_base(code2);
    peer_publish(base2 + "/p0", "1", true);
    Board a; a.reset(); a.apply(2, 1);
    peer_publish(base2 + "/0", make_msg(code2, 500, 1, move_payload(2, a)), true);
    press('2');
    CHECK(wait_text("TYPE THE ROOM CODE", 3000), "unirse: pide el codigo");
    type("MNPQRSTUVX");
    press(8);                                          // SHIFT+0: borra la X
    press('W');
    CHECK(shows("MNPQRSTUVW"), "se ve lo tecleado, con el borrado");
    show("el codigo tecleado");
    press('\r');
    CHECK(wait_text("YOUR TURN:", 40000), "unido: ya tengo su jugada retenida y me toca");
    show("unido a la partida");
    CHECK(disc(2, 0) == 'R' && hilite(2, 0), "se ve su primera jugada (columna 3 de 1 a 7, roja)");
    CHECK(shows("YELLOW") && !shows("RED"), "y soy el amarillo (jugador 2)");
    CHECK(wait_text("HERE", 3000), "y, al siguiente sondeo, que esta (su presencia retenida)");
    press('1');
    CHECK(wait_text("THE RIVAL IS THINKING", 5000), "juego como el 2");
    CHECK(disc(0, 0) == 'Y', "mi ficha es la amarilla");
  }

  printf("  instrucciones del Z80: %ld\n", instr_total);
  // --- una sala llena: no deja entrar a un tercero
  press('Q');
  CHECK(wait_text("1 = NEW GAME", 30000), "Q: vuelvo al menu");
  seen.clear(); retained.clear(); filters.clear(); has_will = false;
  {
    std::string code4 = "ABCDEFGHJK"; std::string b4 = topic_base(code4);
    peer_publish(b4 + "/p0", "1", true);
    peer_publish(b4 + "/p1", "1", true);            // ya estan los dos
    press('2');
    CHECK(wait_text("TYPE THE ROOM CODE", 3000), "unirse: pide el codigo");
    type(code4.c_str());
    press('\r');
    CHECK(wait_text("THE ROOM IS FULL", 40000), "sala llena: no me deja entrar");
    show("sala llena");
    CHECK(count_seen("DISCONNECT") >= 1, "y me despido del broker sin tocar su presencia");
    bool pub = false; for (auto& sn : seen) if (sn.kind == "PUBLISH") pub = true;
    CHECK(!pub, "no publica nada");
    press(' ');
    CHECK(wait_text("1 = NEW GAME", 5000), "una tecla: vuelvo al menu");
  }

  // --- una sala que nadie ha creado: tampoco deja entrar (antes se colaba como un jugador fantasma)
  seen.clear(); retained.clear(); filters.clear(); has_will = false;
  {
    std::string code5 = "PQRSTUVWXY"; std::string b5 = topic_base(code5);
    press('2');
    CHECK(wait_text("TYPE THE ROOM CODE", 3000), "unirse: pide el codigo");
    type(code5.c_str());
    press('\r');
    CHECK(wait_text("THERE IS NO SUCH ROOM", 40000), "sala inexistente: me lo dice");
    show("sala inexistente");
    bool pub = false; for (auto& sn : seen) if (sn.kind == "PUBLISH") pub = true;
    CHECK(!pub && retained.find(b5 + "/p1") == retained.end(), "y no deja una presencia fantasma en la ranura 1");
    press(' ');
    CHECK(wait_text("1 = NEW GAME", 5000), "una tecla: vuelvo al menu");
  }

  // --- el creador reentra en su sala: su presencia esta a '0' y el rival sigue en la ranura 1
  seen.clear(); retained.clear(); filters.clear(); has_will = false;
  {
    std::string code6 = "BCDEFGHJKM"; std::string b6 = topic_base(code6);
    peer_publish(b6 + "/p0", "0", true);
    peer_publish(b6 + "/p1", "1", true);
    press('2');
    CHECK(wait_text("TYPE THE ROOM CODE", 3000), "unirse: pide el codigo");
    type(code6.c_str());
    press('\r');
    CHECK(wait_text("YOUR TURN:", 40000), "el creador vuelve a su sala y juega como el 1");
    const Seen* pp = last_seen("PUBLISH", b6 + "/p0");
    CHECK(pp && pp->data == "1" && pp->retain, "con su presencia en la ranura 0");
    press('Q');
    CHECK(wait_text("1 = NEW GAME", 30000), "Q: vuelvo al menu");
  }


  // --- guardar la partida en la SD y seguirla otro dia (savegame.inc)
  CHECK(shows("3 = CONTINUE SAVED GAME"), "hay una partida guardada (la anterior): el menu ofrece seguirla");
  {
    seen.clear(); retained.clear(); filters.clear(); has_will = false;
    sdfiles.clear();
    press('2'); press('\r');                           // ENTER sin codigo: otra vez el menu, ahora sin fichero
    CHECK(wait_text("1 = NEW GAME", 5000) && !shows("CONTINUE"), "sin partida guardada, el menu no ofrece seguir");
    press('1');
    CHECK(wait_text("WAITING FOR THE RIVAL", 40000), "partida nueva para guardar");
    std::string codeS = code_on_screen(); std::string baseS = topic_base(codeS);
    uint16_t sseq = 3000;
    auto peerS = [&](uint16_t ply, std::string payload) { peer_publish(baseS + "/1", make_msg(codeS, sseq++, ply, payload), true); };
    peer_publish(baseS + "/p1", "1", true);
    CHECK(wait_text("YOUR TURN", 10000), "el rival entra");
    Board gs; gs.reset();
    press('1'); gs.apply(0, 1);
    CHECK(wait_text("THE RIVAL IS THINKING", 5000), "juego la columna 1");
    gs.apply(1, 2); peerS(2, move_payload(1, gs));
    CHECK(wait_text("YOUR TURN:", 5000), "el rival juega la columna 2");
    press('3'); gs.apply(2, 1);
    CHECK(wait_text("THE RIVAL IS THINKING", 5000), "juego la columna 3");
    run_ms(500);
    { auto it = sdfiles.find("CONNECT4.SAV");
      CHECK(it != sdfiles.end() && it->second.size() == 66, "se ha guardado CONNECT4.SAV (24 + 42 bytes)");
      if (it != sdfiles.end() && it->second.size() == 66) {
        const auto& v = it->second;
        CHECK(v[0] == 1 && v[1] == 1 && v[2] == 0, "el fichero: juego 1, version 1, mi ranura (la 0)");
        CHECK(v[14] == 3 && v[15] == 0 && v[16] == 0, "tres jugadas hechas y la partida sigue");
        bool codeok = true;
        for (int i = 0; i < 10; i++) { char ch = codeS[i]; uint8_t z = (uint8_t)(ch >= 'A' ? ch - 27 : ch - 20); if (v[4 + i] != z) codeok = false; }
        CHECK(codeok, "con el codigo de la sala");
        CHECK(memcmp(v.data() + 24, gs.b, 42) == 0, "y el tablero"); } }
    press('Q');
    CHECK(wait_text("1 = NEW GAME", 30000), "salgo al menu");

    // apagar y encender el ZX81: la RAM se pierde, la SD y los mensajes retenidos del broker no
    reboot();
    CHECK(wait_text("1 = NEW GAME", 20000), "el ZX81 arranca otra vez");
    CHECK(shows("3 = CONTINUE SAVED GAME"), "y el menu ofrece seguir la partida guardada");
    press('3');
    CHECK(wait_text("THE RIVAL IS THINKING", 40000), "sigo la partida: la ultima jugada fue mia y espero al rival");
    show("partida seguida");
    CHECK(board_all() == board_text(gs.b), "el tablero vuelve como estaba");
    CHECK(shows("RED") && shows(codeS.c_str()), "soy el rojo, en la misma sala");
    { const Seen* c = last_seen("CONNECT");
      CHECK(c && c->topic == baseS + "/p0", "he entrado con el testamento en MI ranura (la 0)"); }
    { const Seen* pp = last_seen("PUBLISH", baseS + "/p0");
      CHECK(pp && pp->data == "1" && pp->retain, "y con mi presencia retenida en la 0"); }
    { const Seen* s = last_seen("PUBLISH", baseS + "/0"); std::string b = body_of(s);
      CHECK(s && b.size() == 4 && b[0] == 1 && b[1] == 2 && (uint8_t)s->data[3] == 3,
            "he vuelto a mandar mi ultima jugada (columna 3, jugada 3): el broker pudo olvidarla"); }
    gs.apply(3, 2); peerS(4, move_payload(3, gs));
    CHECK(wait_text("YOUR TURN:", 10000), "el rival juega la columna 4: me toca");
    CHECK(board_all() == board_text(gs.b), "su jugada encaja en el estado que traia guardado (el hash cuadra)");
    show("tras la jugada del rival, ya seguida");
    press('Q');
    CHECK(wait_text("1 = NEW GAME", 30000), "salgo al menu");

    // un fichero que no es de este programa: se vuelve al menu, sin colgarse
    sdfiles["CONNECT4.SAV"] = std::vector<uint8_t>(5, 7);
    press('2'); press('\r');
    CHECK(wait_text("1 = NEW GAME", 5000) && shows("3 = CONTINUE SAVED GAME"), "con un fichero roto sigue saliendo la opcion");
    press('3');
    run_ms(3000);
    CHECK(shows("1 = NEW GAME") && !shows("JOINING"), "pero no lo carga: vuelve al menu");
  }

  if (errors) printf("%d ERRORES\n", errors); else printf("TODO OK\n");
  return errors ? 1 : 0;
}
