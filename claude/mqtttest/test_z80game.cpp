// Pruebas de la capa C (EXAMPLES/NETGAME: game.inc y connect4.inc) en el PC, sin hardware:
//  1. las reglas del 4 en raya en el Z80, contra una implementacion de referencia en C++, con
//     miles de jugadas al azar (legales e ilegales), ganadores, tablas, guardar y cargar el estado;
//  2. el controlador de partida contra un rival hecho aqui (el mismo entorno que test_z80net.cpp:
//     el Z80 de verdad, el modem de verdad y un broker de mentira): jugadas, turnos, jugadas viejas,
//     jugadas que no cuadran, estado completo y fin de partida.
#include "z80net_env.h"
#include <random>

// ------------------------------------------------------------ referencia en C++
struct C4 {
  uint8_t b[42]; uint8_t h[7];
  void reset() { memset(b, 0, sizeof(b)); memset(h, 0, sizeof(h)); }
  bool valid(int c) const { return c >= 0 && c < 7 && h[c] < 6; }
  void apply(int c, int p) { b[c + 7 * h[c]] = (uint8_t)p; h[c]++; }
  // los cuatro de la primera linea ganadora (en el mismo orden de busqueda que el Z80); false si no hay
  bool winline(int out[4][2]) const {
    static const int dx[4] = {1, 0, 1, 1}, dy[4] = {0, 1, 1, -1};
    for (int y = 0; y < 6; y++) for (int x = 0; x < 7; x++) {
      int p = cell(x, y);
      if (!p) continue;
      for (int d = 0; d < 4; d++) {
        bool four = true;
        for (int k = 1; k < 4; k++) if (cell(x + k * dx[d], y + k * dy[d]) != p) { four = false; break; }
        if (four) { for (int k = 0; k < 4; k++) { out[k][0] = x + k * dx[d]; out[k][1] = y + k * dy[d]; } return true; }
      }
    }
    return false;
  }
  int cell(int x, int y) const { return (x < 0 || x > 6 || y < 0 || y > 5) ? 0xFF : b[x + 7 * y]; }
  int winner() const {
    static const int dx[4] = {1, 0, 1, 1}, dy[4] = {0, 1, 1, -1};
    for (int y = 0; y < 6; y++) for (int x = 0; x < 7; x++) {
      int p = cell(x, y);
      if (!p) continue;
      for (int d = 0; d < 4; d++) {
        bool four = true;
        for (int k = 1; k < 4; k++) if (cell(x + k * dx[d], y + k * dy[d]) != p) { four = false; break; }
        if (four) return p;
      }
    }
    for (int i = 0; i < 42; i++) if (!b[i]) return 0;
    return 3;
  }
};
static uint16_t hash16(const uint8_t* s, int n) {
  uint16_t h = 0x5A5A;
  for (int i = 0; i < n; i++) {
    h = (uint16_t)((h & 0xFF00) | ((h & 0xFF) ^ s[i]));
    for (int r = 0; r < 3; r++) h = (uint16_t)((h << 1) | (h >> 15));
  }
  return h;
}

// ------------------------------------------------------------ llamadas al Z80
static const uint16_t E_RESET = 24600, E_VALID = 24603, E_APPLY = 24606, E_WINNER = 24609, E_SAVE = 24612,
                      E_LOAD = 24615, E_HASH = 24618, E_GSTART = 24621, E_GMOVE = 24624, E_GPOLL = 24627,
                      E_GNEW = 24630, E_GRESEND = 24633, E_GREMATCH = 24636, E_INWIN = 24639;
static const uint16_t MB = 0x7F00, GMB = 0x7400;
static int mbr() { return mem[MB + 2]; }
static void z_reset() { call_ep(E_RESET); }
static bool z_valid(int c, int p) { mem[MB] = (uint8_t)c; mem[MB + 1] = (uint8_t)p; call_ep(E_VALID); return mbr() == 0; }
static void z_apply(int c, int p) { mem[MB] = (uint8_t)c; mem[MB + 1] = (uint8_t)p; call_ep(E_APPLY); }
static int z_winner() { call_ep(E_WINNER); return mbr(); }
static bool z_inwin(int x, int y) { mem[MB] = (uint8_t)x; mem[MB + 1] = (uint8_t)y; call_ep(E_INWIN); return mbr() != 0; }
// la linea ganadora que ve el Z80 (c4_inwin, para resaltarla) es la misma que la de la referencia: 42 casillas
static bool same_winline(const C4& r, const char* msg) {
  int w[4][2]; bool has = r.winline(w);
  for (int y = 0; y < 6; y++) for (int x = 0; x < 7; x++) {
    bool want = false;
    if (has) for (int k = 0; k < 4; k++) if (w[k][0] == x && w[k][1] == y) want = true;
    if (z_inwin(x, y) != want) { printf("ERROR %s: casilla (%d,%d) Z80=%d ref=%d\n", msg, x, y, !want, want); errors++; return false; }
  }
  return true;
}
static void z_save(uint8_t* out) { call_ep(E_SAVE); memcpy(out, mem + MB + 16, 42); }
static void z_load(const uint8_t* in) { memcpy(mem + MB + 16, in, 42); call_ep(E_LOAD); }
static int z_gmove(int c) { mem[MB] = (uint8_t)c; call_ep(E_GMOVE); return mbr(); }
static int z_gpoll() { call_ep(E_GPOLL); return mbr(); }
static uint16_t z_ply() { return (uint16_t)(mem[GMB + 1] | (mem[GMB + 2] << 8)); }
static int z_over() { return mem[GMB + 3]; }

static bool same_board(const C4& r, const char* msg) {
  uint8_t z[42]; z_save(z);
  if (memcmp(z, r.b, 42) == 0) return true;
  printf("ERROR %s: tablero distinto\n", msg); errors++;
  return false;
}

int main(int argc, char** argv) {
  if (argc < 2) { printf("uso: test_z80game c4test.bin\n"); return 2; }
  FILE* f = fopen(argv[1], "rb");
  if (!f) { printf("no encuentro %s\n", argv[1]); return 2; }
  size_t sz = fread(mem + 0x6000, 1, 0x1800, f); fclose(f);
  printf("c4test.bin: %zu bytes en $6000\n", sz);
  z80_init(); z80_reset();
  reset_world();

  // ================= 1. las reglas =================
  std::mt19937 rng(12345);
  C4 r; int draws = 0, wins1 = 0, wins2 = 0, steps = 0, bad = 0;
  for (int game = 0; game < 400 && bad == 0; game++) {
    r.reset(); z_reset();
    int p = 1, res = 0;
    bool full_random = game % 3 == 0;                 // un tercio: solo jugadas legales, hasta llenar
    for (int n = 0; n < 60 && res == 0; n++) {
      int c = (int)(rng() % 9);                       // 7 y 8 no existen
      if (full_random) {                              // legal y sin hacer cuatro seguidos si se puede: llega a tablas
        std::vector<int> ok;
        for (int cc = 0; cc < 7; cc++) if (r.valid(cc)) {
          C4 t = r; t.apply(cc, p);
          if (t.winner() != p) ok.push_back(cc);
        }
        if (ok.empty()) { for (int cc = 0; cc < 7; cc++) if (r.valid(cc)) ok.push_back(cc); }
        c = ok[rng() % ok.size()];
      }
      bool zv = z_valid(c, p);
      if (zv != r.valid(c)) { printf("ERROR partida %d: legal(%d) Z80=%d ref=%d\n", game, c, zv, r.valid(c)); errors++; bad++; break; }
      if (!zv) continue;
      z_apply(c, p); r.apply(c, p); steps++;
      res = r.winner();
      int zw = z_winner();
      if (zw != res) { printf("ERROR partida %d: ganador Z80=%d ref=%d\n", game, zw, res); errors++; bad++; break; }
      if (!same_board(r, "tras apply")) { bad++; break; }
      if ((res == 1 || res == 2) && !same_winline(r, "linea ganadora en una partida al azar")) { bad++; break; }
      if (n % 7 == 0) {                               // guardar, borrar y cargar: lo mismo, y las alturas valen
        uint8_t s[42]; z_save(s); z_reset(); z_load(s);
        if (!same_board(r, "tras guardar y cargar")) { bad++; break; }
        for (int cc = 0; cc < 7; cc++) if (z_valid(cc, 1) != r.valid(cc)) { printf("ERROR alturas tras cargar (columna %d)\n", cc); errors++; bad++; break; }
      }
      p = 3 - p;
    }
    if (res == 3) draws++; else if (res == 1) wins1++; else if (res == 2) wins2++;
  }
  printf("  reglas: %d jugadas, ganan 1:%d 2:%d, tablas:%d\n", steps, wins1, wins2, draws);
  CHECK(bad == 0, "el 4 en raya del Z80 coincide con la referencia");
  CHECK(wins1 > 10 && wins2 > 10 && draws > 0, "se han visto victorias de los dos y tablas");
  {                                                   // cuatro casos concretos
    struct T { const char* name; int moves[16]; int n; int win; } T[] = {
      {"vertical",    {0,1,0,1,0,1,0}, 7, 1},
      {"horizontal",  {0,0,1,1,2,2,3}, 7, 1},
      {"diagonal /",  {0,1,1,2,2,3,2,3,3,6,3}, 11, 1},
      {"diagonal \\", {3,2,2,1,1,0,1,0,0,6,0}, 11, 1},
    };
    for (auto& t : T) {
      z_reset(); r.reset();
      int p = 1, last = 0;
      for (int i = 0; i < t.n; i++) { z_apply(t.moves[i], p); r.apply(t.moves[i], p); last = z_winner(); p = 3 - p; }
      char msg[64]; snprintf(msg, sizeof(msg), "ganador en %s", t.name);
      CHECK(last == t.win && r.winner() == t.win, msg);
      snprintf(msg, sizeof(msg), "los cuatro resaltados en %s", t.name);
      CHECK(same_winline(r, msg), msg);
    }
  }

  // ================= 2. el controlador, contra un rival hecho aqui =================
  write_limit = 255;
  reset_world();
  CHECK(call_ep(24576) == 0, "net_init");
  CHECK(call_ep(24579) == 0, "room_new (soy el jugador 1)");
  std::string code = room_code_ascii();
  std::string base = topic_base(code);
  call_ep(E_GSTART);
  CHECK(mem[GMB] == 1 && z_ply() == 0 && z_over() == 0, "game_start: soy el 1 y la partida esta a cero");
  C4 pe; pe.reset();                                // la posicion del rival
  uint16_t pseq = 100;
  auto peer_msg = [&](uint16_t ply, std::string payload, bool retain = false) {
    peer_publish(base + "/1", make_msg(code, pseq++, ply, payload), retain);
  };
  auto move_payload = [&](int col, const C4& after) {
    std::string s; s += (char)1; s += (char)col;
    uint16_t h = hash16(after.b, 42); s += (char)(h & 255); s += (char)(h >> 8);
    return s;
  };
  auto last_pub = [&]() { return last_seen("PUBLISH", base + "/0"); };
  auto body_of = [&](const Seen* s) { return s ? s->data.substr(5, s->data.size() - 13) : std::string(); };

  // el rival entra
  peer_publish(base + "/p1", "1", true);
  CHECK(z_gpoll() == 2, "game_poll: el rival ha entrado");

  // 2.1 mi jugada: legal, mandada con el hash del estado resultante
  CHECK(z_gmove(3) == 0, "game_move(3) bien");
  C4 me; me.reset(); me.apply(3, 1); pe.apply(3, 1);
  { const Seen* s = last_pub();
    CHECK(s != nullptr && s->data.size() == 5 + 4 + 8, "mi jugada sale: 17 bytes");
    std::string b = body_of(s);
    CHECK(b.size() == 4 && b[0] == 1 && b[1] == 3 && (uint8_t)b[2] == (hash16(me.b, 42) & 255) && (uint8_t)b[3] == (hash16(me.b, 42) >> 8),
          "payload: tipo 1, columna 3 y el hash del estado despues");
    CHECK(s && (uint8_t)s->data[3] == 1 && (uint8_t)s->data[4] == 0, "el turno de la jugada es 1"); }
  CHECK(z_ply() == 1 && z_gmove(2) == 2, "no me toca: game_move devuelve 2");
  CHECK(z_gmove(9) == 2, "y fuera del tablero tambien: primero se mira el turno");

  // 2.2 el rival juega (columna 3, encima de la mia)
  pe.apply(3, 2);
  peer_msg(2, move_payload(3, pe));
  CHECK(z_gpoll() == 1 && mem[GMB + 4] == 3 && z_ply() == 2, "game_poll: el rival ha jugado la columna 3");
  { uint8_t zb[42]; z_save(zb); CHECK(memcmp(zb, pe.b, 42) == 0, "mi tablero coincide con el del rival"); }
  CHECK(z_gmove(9) == 1, "no es legal (columna 9): game_move devuelve 1");
  CHECK(z_ply() == 2, "y no cambia nada");

  // 2.3 una jugada vieja (la misma que ya tengo) se ignora
  peer_msg(2, move_payload(3, pe));
  CHECK(z_gpoll() == 0 && z_ply() == 2, "una jugada repetida / vieja se ignora");

  // 2.4 juego con otra jugada mia y del rival
  CHECK(z_gmove(4) == 0, "game_move(4) bien"); me.apply(3, 1); me.apply(3, 2); me.apply(4, 1);
  pe.apply(4, 1);
  pe.apply(4, 2);
  peer_msg(4, move_payload(4, pe));
  CHECK(z_gpoll() == 1 && z_ply() == 4, "el rival responde en la columna 4");

  // 2.5 hash que no cuadra: pide el estado
  { size_t n0 = seen.size();
    CHECK(z_gmove(2) == 0, "game_move(2) bien"); pe.apply(2, 1);
    peer_msg(6, move_payload(2, pe).substr(0, 2) + std::string("\x12\x34", 2));   // hash mal
    CHECK(z_gpoll() == 5, "hash distinto: desincronizado, pide el estado");
    const Seen* s = last_pub();
    CHECK(seen.size() > n0 && s && body_of(s).size() == 1 && body_of(s)[0] == 2, "y sale un mensaje de tipo 2 (pido estado)"); }

  // 2.6 el rival me manda el estado completo
  { C4 st; st.reset(); st.apply(0, 1); st.apply(1, 2); st.apply(0, 1);
    std::string pl; pl += (char)3; pl += std::string((const char*)st.b, 42);
    peer_msg(3, pl);
    CHECK(z_gpoll() == 4 && z_ply() == 3, "estado completo recibido: me sincronizo con la jugada 3");
    CHECK(same_board(st, "tras recibir el estado"), "mi tablero es el recibido");
    pe = st; }

  // 2.7 el rival me pide el estado
  peer_msg(3, std::string("\x02", 1));
  CHECK(z_gpoll() == 0, "me piden el estado: no es un suceso para el juego");
  { const Seen* s = last_pub(); std::string b = body_of(s);
    CHECK(b.size() == 43 && b[0] == 3 && memcmp(b.data() + 1, pe.b, 42) == 0, "y mando el estado completo (tipo 3, 42 casillas)"); }

  // 2.8 faltan jugadas (el rival va tres por delante): pide el estado
  { C4 x = pe; x.apply(2, 2);
    peer_msg(7, move_payload(2, x));                // yo estoy en la jugada 3: la 7 no es la 4
    CHECK(z_gpoll() == 5, "faltan jugadas: pide el estado"); }

  // 2.9 fin de partida: gano con cuatro en vertical (mi 4 en raya, columna 6)
  { z_reset(); call_ep(E_GNEW);
    CHECK(z_ply() == 0 && z_over() == 0, "game_new vuelve a cero");
    C4 g; g.reset();
    int seq[] = {6, 5, 6, 5, 6, 5, 6};              // yo (1): 6,6,6,6; el rival (2): 5,5,5
    for (int i = 0; i < 7; i++) {
      if (i % 2 == 0) { CHECK(z_gmove(seq[i]) == 0, "mi jugada de la partida corta"); g.apply(seq[i], 1); }
      else { g.apply(seq[i], 2); peer_msg((uint16_t)(i + 1), move_payload(seq[i], g)); CHECK(z_gpoll() == 1, "la jugada del rival"); }
    }
    CHECK(z_over() == 1 && z_ply() == 7, "gano yo: GM_OVER = 1");
    CHECK(z_gmove(0) == 3, "con la partida acabada, game_move devuelve 3"); }

  // 2.10 otra partida: se lo digo y vuelvo a cero; y el que me lo pide, me pone a cero
  call_ep(E_GREMATCH);
  { const Seen* s = last_pub(); std::string b = body_of(s);
    CHECK(z_ply() == 0 && z_over() == 0 && b.size() == 1 && b[0] == 4, "game_rematch: a cero y manda el tipo 4"); }
  CHECK(z_gmove(0) == 0, "y puedo jugar otra vez"); me.reset(); me.apply(0, 1);
  peer_msg(1, std::string("\x04", 1));
  CHECK(z_gpoll() == 8 && z_ply() == 0, "el rival pide otra partida: a cero (tipo 4)");

  // 2.11 reenviar mi ultima jugada
  CHECK(z_gmove(1) == 0, "otra jugada");
  { size_t n0 = seen.size(); call_ep(E_GRESEND);
    CHECK(mbr() == 0 && seen.size() > n0, "game_resend vuelve a mandarla"); }

  // 2.12 yo de jugador 2 (me uno a una sala): el rival abre, yo respondo
  reset_world(); write_limit = 255;
  CHECK(call_ep(24576) == 0, "net_init (segunda sala)");
  { std::string code2 = "MNPQRSTUVW"; std::string base2 = topic_base(code2);
    peer_publish(base2 + "/p0", "1", true);
    { C4 a; a.reset(); a.apply(2, 1);
      std::string s; s += (char)1; s += (char)2; uint16_t h = hash16(a.b, 42); s += (char)(h & 255); s += (char)(h >> 8);
      peer_publish(base2 + "/0", make_msg(code2, 500, 1, s), true);    // su jugada, retenida (llegare tarde)
      for (int i = 0; i < 10; i++) mem[NB + 8 + i] = (uint8_t)(code2[i] >= 'A' ? code2[i] - 27 : code2[i] - 20);
      CHECK(call_ep(24582) == 0, "room_join (soy el 2)");
      call_ep(E_GSTART);
      CHECK(mem[GMB] == 2, "game_start: soy el jugador 2");
      CHECK(z_gmove(0) == 2, "no me toca todavia (empieza el 1)");
      int a1 = z_gpoll(), a2 = z_gpoll();
      CHECK(a1 == 1 && z_ply() == 1, "al unirme llega su jugada retenida (la 1) y la acepto");
      CHECK(a2 == 2, "y su presencia");
      CHECK(z_gmove(3) == 0 && z_ply() == 2, "ahora me toca a mi (jugador 2) y juego"); }
  }

  printf("  instrucciones del Z80: %ld\n", instr_total);
  if (errors) printf("%d ERRORES\n", errors); else printf("TODO OK\n");
  return errors ? 1 : 0;
}
