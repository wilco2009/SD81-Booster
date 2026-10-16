// Pruebas del motor de ajedrez (EXAMPLES/NETGAME/chess.inc) en el PC: el Z80 de verdad, y el
// metodo de siempre para validar un generador de jugadas, "perft" (contar las posiciones a
// profundidad N desde posiciones conocidas, con los numeros publicados), mas jaque mate, ahogado,
// tablas, los efectos de enroque, al paso y coronacion, y la validacion de jugadas.
#include "z80net_env.h"

static const uint16_t E_RESET = 24576, E_VALID = 24579, E_APPLY = 24582, E_WINNER = 24585, E_SAVE = 24588,
                      E_LOAD = 24591, E_GEN = 24594;
static const uint16_t MBA = 0x8F00, CHB = 0x8500, GMB = 0x8400;
static const uint16_t CH_LIST = CHB + 512, CH_N = CHB + 272 + 1;

struct Mv { uint8_t b0, b1; };
typedef std::vector<uint8_t> St;               // el estado externo: 64 casillas, enroques, al paso, reloj

static St z_save() { call_ep(E_SAVE); return St(mem + MBA + 16, mem + MBA + 16 + 67); }
static void z_load(const St& s) { memcpy(mem + MBA + 16, s.data(), 67); call_ep(E_LOAD); }
static std::vector<Mv> z_gen(int side) {
  mem[MBA] = (uint8_t)(side * 8); call_ep(E_GEN);
  int n = mem[MBA + 4];
  std::vector<Mv> v;
  for (int i = 0; i < n; i++) {
    int f = mem[CH_LIST + 3 * i], t = mem[CH_LIST + 3 * i + 1], pr = mem[CH_LIST + 3 * i + 2];
    Mv m; m.b0 = (uint8_t)(((f >> 4) * 8 + (f & 7)) | (pr << 6)); m.b1 = (uint8_t)((t >> 4) * 8 + (t & 7));
    v.push_back(m);
  }
  return v;
}
static void z_apply(Mv m, int player) { mem[MBA] = m.b0; mem[MBA + 1] = m.b1; mem[MBA + 2] = (uint8_t)player; call_ep(E_APPLY); }
static bool z_valid(Mv m, int player) { mem[MBA] = m.b0; mem[MBA + 1] = m.b1; mem[MBA + 2] = (uint8_t)player; call_ep(E_VALID); return mem[MBA + 3] == 0; }
static int z_winner(int ply) { mem[GMB + 1] = (uint8_t)(ply & 255); mem[GMB + 2] = (uint8_t)(ply >> 8); call_ep(E_WINNER); return mem[MBA + 3]; }

static uint64_t perft(int d, int side) {
  std::vector<Mv> mv = z_gen(side);
  if (d == 1) return mv.size();
  St saved = z_save();
  uint64_t tot = 0;
  for (Mv m : mv) { z_apply(m, side + 1); tot += perft(d - 1, 1 - side); z_load(saved); }
  return tot;
}

// FEN -> estado externo; devuelve el color que mueve (0 blancas, 1 negras)
static int fen(const char* f, St* out) {
  St s(67, 0);
  int r = 7, c = 0; const char* p = f;
  for (; *p && *p != ' '; p++) {
    if (*p == '/') { r--; c = 0; }
    else if (*p >= '1' && *p <= '8') c += *p - '0';
    else {
      static const char* ps = "pnbrqk";
      const char* q = strchr(ps, *p >= 'a' ? *p : *p + 32);
      int t = (int)(q - ps) + 1;
      s[r * 8 + c] = (uint8_t)(t + (*p >= 'a' ? 8 : 0)); c++;
    }
  }
  p++; int side = (*p == 'b') ? 1 : 0; p += 2;
  int cs = 0;
  for (; *p && *p != ' '; p++) { if (*p == 'K') cs |= 1; if (*p == 'Q') cs |= 2; if (*p == 'k') cs |= 4; if (*p == 'q') cs |= 8; }
  p++;
  s[64] = (uint8_t)cs; s[65] = (*p >= 'a' && *p <= 'h') ? (uint8_t)(*p - 'a') : 0xFF; s[66] = 0;
  *out = s; return side;
}
static int sq(const char* n) { return (n[1] - '1') * 8 + (n[0] - 'a'); }
static Mv mv(const char* from, const char* to, int promo = 0) { Mv m; m.b0 = (uint8_t)(sq(from) | (promo << 6)); m.b1 = (uint8_t)sq(to); return m; }

static void check_perft(const char* name, const char* f, int d, uint64_t want) {
  St s; int side = fen(f, &s);
  z_load(s);
  uint64_t got = perft(d, side);
  char msg[160]; snprintf(msg, sizeof(msg), "perft(%d) %s = %llu (esperado %llu)", d, name, (unsigned long long)got, (unsigned long long)want);
  CHECK(got == want, msg);
  if (got == want) printf("  ok perft(%d) %-12s %llu\n", d, name, (unsigned long long)got);
}

int main(int argc, char** argv) {
  if (argc < 2) { printf("uso: test_z80chess chtest.bin\n"); return 2; }
  FILE* f = fopen(argv[1], "rb");
  if (!f) { printf("no encuentro %s\n", argv[1]); return 2; }
  size_t sz = fread(mem + 0x6000, 1, 0x2000, f); fclose(f);
  printf("chtest.bin: %zu bytes en $6000\n", sz);
  z80_init(); z80_reset();
  reset_world();

  const char* START = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
  call_ep(E_RESET);
  { St s = z_save(), t; fen(START, &t);
    CHECK(s == t, "g_reset: la posicion inicial (como el FEN)"); }

  // ---- perft: las posiciones de siempre
  check_perft("inicial", START, 1, 20);
  check_perft("inicial", START, 2, 400);
  check_perft("inicial", START, 3, 8902);
  const char* KIWI = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1";
  check_perft("kiwipete", KIWI, 1, 48);
  check_perft("kiwipete", KIWI, 2, 2039);
  const char* P3 = "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1";
  check_perft("posicion 3", P3, 1, 14);
  check_perft("posicion 3", P3, 2, 191);
  check_perft("posicion 3", P3, 3, 2812);
  const char* P4 = "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1";
  check_perft("posicion 4", P4, 1, 6);
  check_perft("posicion 4", P4, 2, 264);
  const char* P5 = "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8";
  check_perft("posicion 5", P5, 1, 44);
  check_perft("posicion 5", P5, 2, 1486);
  check_perft("enroques", "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1", 1, 26);
  check_perft("enroques", "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1", 2, 568);
  check_perft("coronar", "n1n5/PPPk4/8/8/8/8/4Kppp/5N1N b - - 0 1", 1, 24);
  check_perft("coronar", "n1n5/PPPk4/8/8/8/8/4Kppp/5N1N b - - 0 1", 2, 496);
  if (getenv("PERFT_BIG")) {
    check_perft("inicial", START, 4, 197281);
    check_perft("kiwipete", KIWI, 3, 97862);
    check_perft("posicion 3", P3, 4, 43238);
    check_perft("posicion 4", P4, 3, 9467);
  }

  // ---- los efectos de las jugadas
  {
    St s; fen("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1", &s); z_load(s);
    z_apply(mv("e1", "g1"), 1);                                  // enroque corto blanco
    St t = z_save();
    CHECK(t[sq("g1")] == 6 && t[sq("f1")] == 4 && t[sq("h1")] == 0 && t[sq("e1")] == 0, "enroque corto: el rey en g1 y la torre en f1");
    CHECK(t[64] == 0x0C, "y las blancas pierden sus dos enroques");
    z_apply(mv("e8", "c8"), 2);                                  // enroque largo negro
    t = z_save();
    CHECK(t[sq("c8")] == 14 && t[sq("d8")] == 12 && t[sq("a8")] == 0 && t[64] == 0, "enroque largo: rey en c8, torre en d8, sin enroques");
  }
  {
    St s; fen("8/8/8/3pP3/8/8/8/k6K w - d6 0 1", &s); z_load(s);
    z_apply(mv("e5", "d6"), 1);                                  // al paso
    St t = z_save();
    CHECK(t[sq("d6")] == 1 && t[sq("d5")] == 0 && t[sq("e5")] == 0, "al paso: el peon negro de d5 desaparece");
  }
  {
    St s; fen("8/P7/8/8/8/8/8/k6K w - - 0 1", &s); z_load(s);
    CHECK(z_valid(mv("a7", "a8", 3), 1) && z_valid(mv("a7", "a8", 0), 1), "coronar: caballo y dama son legales");
    z_apply(mv("a7", "a8", 3), 1);
    St t = z_save();
    CHECK(t[sq("a8")] == 2 && t[sq("a7")] == 0, "coronar con caballo: queda un caballo blanco en a8");
  }
  {
    St s; fen(START, &s); z_load(s);
    z_apply(mv("e2", "e4"), 1);
    St t = z_save();
    CHECK(t[sq("e4")] == 1 && t[sq("e2")] == 0 && t[65] == 4 && t[66] == 0, "e2e4: el peon en e4, al paso en la columna e y el reloj a cero");
    z_apply(mv("g8", "f6"), 2);
    t = z_save();
    CHECK(t[65] == 0xFF && t[66] == 1, "Cf6: sin al paso y el reloj a 1");
  }

  // ---- validar jugadas
  {
    St s; fen(START, &s); z_load(s);
    CHECK(z_valid(mv("e2", "e4"), 1) && z_valid(mv("g1", "f3"), 1), "e2e4 y Cf3 son legales");
    CHECK(!z_valid(mv("e2", "e5"), 1), "e2e5 no");
    CHECK(!z_valid(mv("e7", "e5"), 1), "mover una pieza del rival, no");
    CHECK(!z_valid(mv("e1", "e2"), 1) && !z_valid(mv("a1", "a3"), 1), "el rey sobre su peon y la torre encerrada, no");
    CHECK(!z_valid(mv("e2", "e4", 1), 1), "una jugada normal con bits de corona, no");
  }
  {
    St s; fen("4k3/8/8/8/8/8/4r3/R3K2R w KQ - 0 1", &s); z_load(s);       // el rey blanco esta en jaque por la torre de e2? no: la torre negra en e2 ataca e1
    CHECK(!z_valid(mv("e1", "g1"), 1), "no se puede enrocar en jaque");
    fen("4k3/8/8/8/8/5r2/8/R3K2R w KQ - 0 1", &s); z_load(s);            // la torre negra en f3 ataca f1
    CHECK(!z_valid(mv("e1", "g1"), 1) && z_valid(mv("e1", "c1"), 1), "ni cruzar una casilla atacada, pero el largo si");
  }

  // ---- final de partida
  {
    St s; fen("rnb1kbnr/pppp1ppp/8/4p3/6Pq/5P2/PPPPP2P/RNBQKBNR w KQkq - 1 3", &s); z_load(s);
    CHECK(z_winner(4) == 2, "mate del loco: ganan las negras (2)");
    fen("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1", &s); z_load(s);
    CHECK(z_winner(1) == 3, "ahogado: tablas (3)");
    fen("8/8/8/4k3/8/8/3K4/8 w - - 0 1", &s); z_load(s);
    CHECK(z_winner(0) == 3, "rey contra rey: tablas");
    fen("8/8/8/4k3/8/8/3KN3/8 w - - 0 1", &s); z_load(s);
    CHECK(z_winner(0) == 3, "rey y caballo contra rey: tablas");
    fen("8/8/8/4k3/8/8/3KR3/8 w - - 0 1", &s); z_load(s);
    CHECK(z_winner(0) == 0, "rey y torre contra rey: sigue");
    fen(START, &s); s[66] = 100; z_load(s);
    CHECK(z_winner(0) == 3, "cien medias jugadas sin captura ni peon: tablas");
    fen(START, &s); z_load(s);
    CHECK(z_winner(0) == 0, "la posicion inicial: sigue");
    fen("6k1/5ppp/8/8/8/8/8/R3K3 w - - 0 1", &s); z_load(s);
    z_apply(mv("a1", "a8"), 1);
    CHECK(z_winner(1) == 1, "mate de pasillo: ganan las blancas (1)");
  }

  // ---- guardar y cargar: lo mismo
  {
    St s; fen(KIWI, &s); z_load(s);
    CHECK(z_save() == s, "g_save(g_load(x)) = x");
  }

  printf("  instrucciones del Z80: %ld\n", instr_total);
  if (errors) printf("%d ERRORES\n", errors); else printf("TODO OK\n");
  return errors ? 1 : 0;
}
