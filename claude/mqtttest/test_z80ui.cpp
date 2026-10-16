// Prueba del juego entero (EXAMPLES/NETGAME/connect4.bin) en el PC: el programa de verdad en el Z80
// de EightyOne, con el teclado del ZX81 de mentira (pulsaciones con tiempo), la pantalla leida de la
// memoria ($8000, como la leeria la FPGA), el modem de verdad y un rival hecho aqui por el broker.
// Con SHOW=1 imprime la pantalla en cada paso importante.
#include "z80net_env.h"

static const uint16_t DFILE = 0x8000;

// ------------------------------------------------------------ la pantalla
static std::string screen_row(int r) {
  std::string s;
  for (int c = 0; c < 32; c++) {
    int v = mem[DFILE + 1 + 33 * r + c];
    char ch;
    if (v == 0) ch = ' ';
    else if (v >= 28 && v <= 37) ch = (char)('0' + v - 28);
    else if (v >= 38 && v <= 63) ch = (char)('A' + v - 38);
    else if (v == 27) ch = '.'; else if (v == 14) ch = ':'; else if (v == 22) ch = '-';
    else if (v == 15) ch = '?'; else if (v == 18) ch = '>'; else if (v == 19) ch = '<';
    else if (v == 20) ch = '='; else if (v == 21) ch = '+'; else if (v == 23) ch = '*'; else if (v == 24) ch = '/';
    else if (v == 25) ch = ';'; else if (v == 26) ch = ','; else if (v == 16) ch = '('; else if (v == 17) ch = ')';
    else if (v == 11) ch = '"'; else if (v == 13) ch = '$';
    else if (v == 128) ch = '#'; else if (v == 8) ch = '%';
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
  printf("connect4.bin: %zu bytes en $6000\n", sz);
  mem[0x02E7] = 0xC9; mem[0x0207] = 0xC9;              // SET_FAST y SLOW_FAST de la ROM: aqui, un RET
  z80_init(); z80_reset();
  reset_world();
  z80.sp.w = 0xFFF0; mem[0xFFF0] = 0x00; mem[0xFFF1] = 0x01; mem[0x0100] = 0x76;   // si vuelve, a la trampa
  z80.pc.w = 24576; z80.iff1 = z80.iff2 = 0;

  // --- arranque y menu
  CHECK(wait_text("1 = NEW GAME", 20000), "arranca y llega al menu (tras iniciar el modem)");
  show("menu");
  CHECK(shows("2 = JOIN A GAME") && shows("Q = QUIT"), "el menu tiene las tres opciones");
  CHECK(mem[2045] == 170 && mem[2096] == 0x00 && mem[2097] == 0x80 && mem[2098] == 170, "Superfast texto con la pantalla alternativa en $8000");

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
  CHECK(shows("RIVAL: HERE"), "se ve que el rival esta");
  CHECK(screen_row(11).substr(4, 23) == ":.. . .. . .. . .. . .. . .. . .:" || screen_row(11).find(':') == 4, "el tablero esta dibujado (bordes)");

  Board g; g.reset();
  // 1. mi jugada en la columna 1 (tecla 1): cae abajo a la izquierda
  press('1');
  CHECK(wait_text("THE RIVAL IS THINKING", 5000), "juego: ahora le toca al rival");
  g.apply(0, 1);
  CHECK(screen_row(11).substr(5, 3) == "###", "mi ficha (bloque entero) abajo a la izquierda");
  { const Seen* s = last_seen("PUBLISH", base + "/0"); std::string b = body_of(s);
    CHECK(b.size() == 4 && b[0] == 1 && b[1] == 0, "la jugada sale al broker: tipo 1, columna 0"); }
  // 2. no me toca: la tecla 3 da un aviso
  press('3');
  CHECK(wait_text("IT IS NOT YOUR TURN", 3000), "no me toca: aviso");
  // 3. el rival juega en la columna 2
  g.apply(1, 2); peer_msg(2, move_payload(1, g));
  CHECK(wait_text("YOUR TURN:", 5000), "el rival juega y me vuelve a tocar");
  CHECK(screen_row(11).substr(8, 3) == "%%%", "su ficha (gris) a mi lado");
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
  CHECK(shows("ENTER = AGAIN"), "y la ayuda cambia a ENTER = otra partida");
  press('2');
  CHECK(wait_text("THE GAME IS OVER", 3000), "con la partida acabada, una jugada da un aviso");

  // 5. otra partida
  press('\r');
  CHECK(wait_text("YOUR TURN: PRESS 1 TO 7", 5000) && !shows("YOU WIN"), "ENTER: otra partida, a cero");
  { const Seen* s = last_seen("PUBLISH", base + "/0"); std::string b = body_of(s);
    CHECK(b.size() == 1 && b[0] == 4, "y se lo digo al rival (tipo 4)"); }
  CHECK(screen_row(11).substr(5, 3) == " . ", "el tablero esta vacio");
  show("otra partida");
  // 6. el rival pide otra partida cuando ya hay una a medias
  press('4'); g.reset(); g.apply(3, 1);
  CHECK(wait_text("THE RIVAL IS THINKING", 5000), "juego en la columna 4");
  peer_msg(1, std::string("\x04", 1));
  CHECK(wait_text("YOUR TURN: PRESS 1 TO 7", 5000) && screen_row(11).substr(14, 3) == " . ", "el rival pide otra partida: a cero");
  // 7. el rival sale y yo salgo
  peer_publish(base + "/p1", "0", true);
  CHECK(wait_text("THE RIVAL HAS LEFT", 5000) && shows("RIVAL: AWAY"), "el rival se va: aviso");
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
    CHECK(screen_row(11).substr(11, 3) == "###" , "se ve su primera jugada (columna 3, la suya es el bloque entero)");
    CHECK(wait_text("RIVAL: HERE", 3000), "y, al siguiente sondeo, que esta (su presencia retenida)");
    press('1');
    CHECK(wait_text("THE RIVAL IS THINKING", 5000), "juego como el 2");
    CHECK(screen_row(11).substr(5, 3) == "%%%", "mi ficha es la gris");
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

  if (errors) printf("%d ERRORES\n", errors); else printf("TODO OK\n");
  return errors ? 1 : 0;
}
