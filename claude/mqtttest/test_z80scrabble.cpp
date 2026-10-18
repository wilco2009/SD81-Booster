// Prueba del motor del Scrabble (EXAMPLES/NETGAME/scrabble.inc + dict.inc), el Z80 de verdad contra una
// implementacion de referencia en C++ escrita aparte: la bolsa barajada con una semilla, las casillas
// especiales, la puntuacion, los errores de cada jugada y el fin de la partida. Se juegan partidas enteras al
// azar (palabras reales de enable1, con el diccionario en la "SD" de mentira) y despues de cada jugada se
// compara TODO el estado. Los dos idiomas: el castellano usa el mismo diccionario con CH, LL y RR cambiados
// por los bytes de las fichas dobles y NY por la N con tilde.
#include "z80net_env.h"
#include <random>
#include <fstream>
#include <set>
#include <algorithm>
#include <cstdint>

static const uint16_t E_INIT = 24576, E_CHECK = 24579, E_APPLY = 24582, E_PASS = 24585, E_EXCH = 24588, E_DOPEN = 24591;
static const uint16_t SB = 0x9600;
static const uint16_t A_BOARD = SB, A_BAG = SB + 225, A_RACK = SB + 325, A_SCORE = SB + 339, A_BAGH = SB + 343, A_BAGN = SB + 344,
                      A_TURN = SB + 345, A_PASS = SB + 346, A_OVER = SB + 347, A_LANG = SB + 348, A_SEED = SB + 349, A_BCNT = SB + 353,
                      A_WB = SB + 364, A_NF = SB + 384, A_TOT = SB + 409, A_DCW = 0x9400 + 17;

#include "scrabble_ref.h"

// ---- el Z80 -------------------------------------------------------------------------------------
static void z_setmove(const Move& m) {
  mem[A_WB] = (uint8_t)m.x; mem[A_WB + 1] = (uint8_t)m.y; mem[A_WB + 2] = (uint8_t)m.dir; mem[A_WB + 3] = (uint8_t)m.n;
  for (int i = 0; i < 15; i++) mem[A_WB + 4 + i] = i < m.n ? m.c[i] : 0;
}
static int z_i16(uint16_t a) { return (int16_t)(mem[a] | (mem[a + 1] << 8)); }
static bool same_state(const Ref& R, const char* what) {
  bool ok = true;
  auto bad = [&](const char* s) { printf("  %s: difiere %s\n", what, s); ok = false; };
  if (memcmp(mem + A_BOARD, R.board, 225)) bad("el tablero");
  if (memcmp(mem + A_BAG, R.bag, 100)) bad("la bolsa");
  if (memcmp(mem + A_RACK, R.rack, 14)) bad("los atriles");
  if (z_i16(A_SCORE) != R.score[0] || z_i16(A_SCORE + 2) != R.score[1]) { printf("  %s: puntos Z80 %d-%d, ref %d-%d\n", what, z_i16(A_SCORE), z_i16(A_SCORE + 2), R.score[0], R.score[1]); ok = false; }
  if (mem[A_BAGH] != R.bagh || mem[A_BAGN] != R.bagn) bad("la cabeza o el numero de la bolsa");
  if (mem[A_TURN] != R.turn || mem[A_PASS] != R.pass || mem[A_OVER] != R.over || mem[A_BCNT] != R.bcnt) bad("turno, pases, fin o fichas");
  return ok;
}

static std::vector<uint8_t> make_dict(const Dict& d) {
  std::vector<uint8_t> v(16, 0);
  memcpy(v.data(), "SDICT01", 7);
  uint32_t n = (uint32_t)d.size();
  v[8] = n & 255; v[9] = (n >> 8) & 255; v[10] = (n >> 16) & 255; v[11] = (n >> 24) & 255;
  for (auto& w : d) { uint8_t r[16] = {0}; memcpy(r, w.data(), w.size()); v.insert(v.end(), r, r + 16); }
  return v;
}

int main(int argc, char** argv) {
  if (argc < 3) { printf("uso: test_z80scrabble scrtest.bin enable1.txt\n"); return 2; }
  FILE* f = fopen(argv[1], "rb");
  if (!f) { printf("no encuentro %s\n", argv[1]); return 2; }
  size_t sz = fread(mem + 24576, 1, 0x3000, f); fclose(f);
  printf("scrtest.bin: %zu bytes\n", sz);
  std::ifstream wf(argv[2]);
  if (!wf) { printf("no encuentro %s\n", argv[2]); return 2; }
  Dict en, es;
  std::string line;
  while (std::getline(wf, line)) {
    while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
    if (line.size() < 2 || line.size() > 15) continue;
    bool ok = true; std::string u;
    for (char ch : line) { if (ch >= 'a' && ch <= 'z') ch -= 32; if (ch < 'A' || ch > 'Z') ok = false; u += ch; }
    if (!ok) continue;
    en.insert(u);
    std::string s;       // castellano de mentira: CH, LL, RR y NY pasan a ser una sola ficha
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
  if (argc > 3) {                                // la lista real en castellano (UTF-8, minusculas, con la n con tilde)
    std::ifstream sf(argv[3]);
    if (!sf) { printf("no encuentro %s\n", argv[3]); return 2; }
    es.clear();
    while (std::getline(sf, line)) {
      while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
      std::string s; bool ok = !line.empty();
      for (size_t i = 0; i < line.size() && ok; i++) {
        unsigned char ch = (unsigned char)line[i];
        if (ch >= 'a' && ch <= 'z') ch -= 32;
        if (ch == 0xC3 && i + 1 < line.size() && (unsigned char)line[i + 1] == 0xB1) { s += '['; i++; continue; }
        if (ch < 'A' || ch > 'Z') { ok = false; break; }
        if (i + 1 < line.size()) {
          unsigned char n = (unsigned char)line[i + 1]; if (n >= 'a' && n <= 'z') n -= 32;
          if (ch == 'C' && n == 'H') { s += '\\'; i++; continue; }
          if (ch == 'L' && n == 'L') { s += ']'; i++; continue; }
          if (ch == 'R' && n == 'R') { s += '^'; i++; continue; }
        }
        s += (char)ch;
      }
      if (ok && s.size() >= 2 && s.size() <= 15) es.insert(s);
    }
    printf("(castellano: la lista real de %s)\n", argv[3]);
  }
  printf("diccionarios: %zu palabras en ingles, %zu en castellano\n", en.size(), es.size());
  sdfiles["DICTEN.DIC"] = make_dict(en);
  sdfiles["DICTES.DIC"] = make_dict(es);
  z80_init(); z80_reset(); reset_world();

  // --- la bolsa: el reparto de fichas es el de las reglas y la barajada es la misma que la de la referencia
  for (int lang = 0; lang < 2; lang++) {
    int total = 0; for (int c = 0; c < 30; c++) total += CNT[lang][c];
    CHECK(total == 98, "el reparto de fichas suma 98 mas 2 comodines");
    std::mt19937 rng(11);
    for (int t = 0; t < 8; t++) {
      uint32_t seed = (t == 0) ? 0 : rng();
      for (int i = 0; i < 4; i++) mem[A_SEED + i] = (seed >> (8 * i)) & 255;
      mem[A_LANG] = (uint8_t)lang;
      call_ep(E_INIT);
      Ref R; ref_init(R, seed, lang);
      char nm[64]; snprintf(nm, sizeof nm, "init idioma %d semilla %08x", lang, seed);
      CHECK(same_state(R, nm), nm);
      int cnt[256] = {0}; for (int i = 0; i < 100; i++) cnt[mem[A_BAG + i]]++;
      bool okc = cnt['?'] == 2; for (int c = 0; c < 30; c++) okc = okc && cnt[(uint8_t)LETTERS[c]] == CNT[lang][c];
      CHECK(okc, "la bolsa tiene exactamente las fichas del reparto");
      CHECK(mem[A_BAGN] == 86 && mem[A_BAGH] == 14, "tras repartir quedan 86 fichas en la bolsa");
    }
  }

  // --- partidas enteras al azar
  std::map<int, int> hist;                      // errores vistos
  long checks = 0, applied = 0, exchanges = 0, passes = 0, games = 0;
  int bingos = 0, ended_out = 0, ended_pass = 0, bigscore = 0;
  for (int lang = 0; lang < 2; lang++) {
    const Dict& dict = lang ? es : en;
    std::vector<std::string> byLen[16];
    for (auto& w : dict) byLen[w.size()].push_back(w);
    std::mt19937 rng(1000 + lang);
    for (int g = 0; g < (getenv("GAMES") ? atoi(getenv("GAMES")) : 4); g++) {
      uint32_t seed = rng() | 1;
      for (int i = 0; i < 4; i++) mem[A_SEED + i] = (seed >> (8 * i)) & 255;
      mem[A_LANG] = (uint8_t)lang;
      call_ep(E_INIT);
      CHECK(call_ep(E_DOPEN) == 0, "abre el diccionario del idioma");
      Ref R; ref_init(R, seed, lang);
      if (!same_state(R, "inicio de partida")) { errors++; break; }
      games++;
      int stuck = 0;
      for (int turn = 0; turn < 400 && !R.over; turn++) {
        // candidatos: palabras que caben en una linea (empezando en una casilla al azar cerca de lo ya puesto)
        Move found{}; uint8_t fnf[16]; int fscore = 0; bool have = false;
        std::vector<int> tiles; for (int i = 0; i < 225; i++) if (R.board[i]) tiles.push_back(i);
        for (int attempt = 0; attempt < 400 && !have; attempt++) {
          int dir = rng() & 1, x0, y0;
          if (tiles.empty()) { int k = rng() % 7; x0 = dir ? 7 : 7 - k; y0 = dir ? 7 - k : 7; }
          else { int t = tiles[rng() % tiles.size()]; int tx = t % 15, ty = t / 15, back = rng() % 5; x0 = dir ? tx : tx - back; y0 = dir ? ty - back : ty; }
          if (x0 < 0 || y0 < 0 || x0 > 14 || y0 > 14) continue;
          int maxL = dir ? 15 - y0 : 15 - x0; if (maxL < 2) continue;
          int dx = dir ? 0 : 1, dy = dir ? 1 : 0;
          int rcount[256] = {0}; for (int i = 0; i < 7; i++) rcount[R.rack[R.turn][i]]++;
          std::vector<Move> cands;
          int L0 = 2 + rng() % std::min(maxL - 1, 7);
          for (int L = L0; L <= std::min(maxL, 15) && cands.size() < 30; L++) {
            const auto& lst = byLen[L]; if (lst.empty()) continue;
            size_t start = rng() % lst.size();
            for (size_t q = 0; q < lst.size() && cands.size() < 30; q++) {
              const std::string& w = lst[(start + q) % lst.size()];
              int need[256] = {0}; bool fit = true;
              for (int i = 0; i < L && fit; i++) {
                int cell = R.board[(y0 + dy * i) * 15 + x0 + dx * i];
                if (cell) { if ((cell & 0x7F) != (uint8_t)w[i]) fit = false; }
                else need[(uint8_t)w[i]]++;
              }
              if (!fit) continue;
              int blanks = rcount['?'];
              for (int c = 0; c < 256 && fit; c++) if (need[c] > rcount[c]) { blanks -= need[c] - rcount[c]; if (blanks < 0) fit = false; }
              if (!fit) continue;
              Move m{}; m.x = x0; m.y = y0; m.dir = dir; m.n = L; for (int i = 0; i < L; i++) m.c[i] = (uint8_t)w[i];
              cands.push_back(m);
            }
          }
          for (auto& m : cands) {
            int sc = 0; uint8_t nf[16]; std::string bad;
            int e = ref_check(R, dict, m, sc, nf, &bad);
            z_setmove(m); int ze = call_ep(E_CHECK); checks++;
            hist[e]++;
            if (ze != e || (e == 0 && z_i16(A_TOT) != sc)) {
              printf("  DIFIERE idioma %d turno %d: %c%d %s '%.*s': Z80 error %d puntos %d, ref error %d puntos %d\n", lang, turn, 'A' + m.x, m.y + 1, m.dir ? "abajo" : "derecha", m.n, (const char*)m.c, ze, z_i16(A_TOT), e, sc);
              errors++;
            }
            if (e == 0) { found = m; memcpy(fnf, nf, 16); fscore = sc; have = true; break; }
            if (e == 8 || e == 9) { char got[17] = {0}; memcpy(got, mem + A_DCW, 16); if (std::string(got) != bad) { printf("  DC_W '%s' y la ref dice '%s'\n", got, bad.c_str()); errors++; } }
          }
        }
        std::string who = "jugada " + std::to_string(turn);
        if (have) {
          z_setmove(found); int ze = call_ep(E_CHECK); (void)ze;
          call_ep(E_APPLY); ref_apply(R, found, fnf, fscore); applied++; stuck = 0;
          int nn = 0; for (int i = 0; i < found.n; i++) if (fnf[i]) nn++;
          if (nn == 7) bingos++;
          if (fscore > 60) bigscore++;
        } else if (R.bagn >= 7 && (rng() % 3) == 0) {
          std::vector<uint8_t> t; int k = 1 + rng() % 3; int idx[7] = {0,1,2,3,4,5,6}; std::shuffle(idx, idx + 7, rng);
          for (int i = 0; i < k; i++) if (R.rack[R.turn][idx[i]]) t.push_back(R.rack[R.turn][idx[i]]);
          if (t.empty()) t.push_back('E');
          mem[A_WB + 3] = (uint8_t)t.size(); for (size_t i = 0; i < t.size(); i++) mem[A_WB + 4 + i] = t[i];
          int ze = call_ep(E_EXCH), re = ref_exchange(R, t);
          CHECK(ze == re, "cambiar fichas: mismo resultado");
          hist[100 + re]++; exchanges++; stuck++;
        } else { call_ep(E_PASS); ref_pass(R); passes++; stuck++; }
        if (!same_state(R, who.c_str())) { printf("  (idioma %d, partida %d, %s)\n", lang, g, who.c_str()); errors++; break; }
      }
      if (R.over) { bool out = false; for (int p = 0; p < 2; p++) { bool e = true; for (int i = 0; i < 7; i++) if (R.rack[p][i]) e = false; if (e) out = true; } if (out) ended_out++; else ended_pass++; }
      printf("  idioma %d partida %d: %s, puntos %d-%d, %d fichas en el tablero\n", lang, g, R.over ? "acabada" : "sin acabar", R.score[0], R.score[1], R.bcnt);
      CHECK(R.over, "la partida acaba");
      CHECK(mem[A_OVER] == R.over, "el Z80 tambien la da por acabada");
    }
  }
  printf("jugadas comprobadas: %ld (aplicadas %ld, cambios %ld, pases %ld); partidas %ld; acabadas sin fichas %d, por pases %d; 7 fichas %d; mas de 60 puntos %d\n",
         checks, applied, exchanges, passes, games, ended_out, ended_pass, bingos, bigscore);
  printf("errores vistos en las comprobaciones:");
  for (auto& kv : hist) printf(" %d:%d", kv.first, kv.second);
  printf("\n");

  // --- jugadas mal formadas y casos fijos, para los errores que el azar no encuentra
  { Ref R; ref_init(R, 7, 0);
    for (int i = 0; i < 4; i++) mem[A_SEED + i] = (7 >> (8 * i)) & 255;
    mem[A_LANG] = 0; call_ep(E_INIT); call_ep(E_DOPEN);
    Dict& dict = en; std::mt19937 rng(5);
    int mism = 0;
    for (int t = 0; t < 3000; t++) {
      Move m{}; m.n = (int)(rng() % 20); if (rng() % 4) m.n = 2 + rng() % 5;
      m.x = (int)(rng() % 17) - 1; m.y = (int)(rng() % 17) - 1; m.dir = (int)(rng() % 3);
      for (int i = 0; i < 16; i++) { int r = rng() % 12; m.c[i] = r == 0 ? (uint8_t)(rng() % 256) : r == 1 ? (uint8_t)('A' + rng() % 26 + 128) : (uint8_t)('A' + rng() % 26); }
      if (m.x < 0) m.x = 255; if (m.y < 0) m.y = 255;
      int sc = 0; uint8_t nf[16];
      Move rm = m; if (rm.x == 255) rm.x = -1; if (rm.y == 255) rm.y = -1;
      int e = ref_check(R, dict, rm, sc, nf);
      z_setmove(m); int ze = call_ep(E_CHECK);
      if (ze != e) { if (mism++ < 5) printf("  mal formada %d,%d dir %d n %d: Z80 %d, ref %d\n", m.x, m.y, m.dir, m.n, ze, e); }
      hist[e]++;
    }
    CHECK(mism == 0, "3.000 jugadas mal formadas al azar: mismos errores que la referencia");
    printf("errores vistos con jugadas mal formadas:");
    for (auto& kv : hist) printf(" %d:%d", kv.first, kv.second);
    printf("\n");
    // casos fijos con atriles a medida y puntos calculados a mano (la referencia no puede equivocarse con ellos)
    auto setrack = [&](int p, const char* s) { for (int i = 0; i < 7; i++) { mem[A_RACK + 7 * p + i] = (uint8_t)s[i]; R.rack[p][i] = (uint8_t)s[i]; } };
    auto mk = [&](int x, int y, int dir, const char* w) { Move m{}; m.x = x; m.y = y; m.dir = dir; m.n = (int)strlen(w); memcpy(m.c, w, m.n); return m; };
    // la comprobacion, con el Z80 y con la referencia: devuelve el error del Z80 y deja sus puntos en pts
    auto both = [&](const Move& m, int& pts, const char* what) {
      int sc = 0; uint8_t nf[16];
      int e = ref_check(R, dict, m, sc, nf); z_setmove(m); int ze = call_ep(E_CHECK);
      pts = z_i16(A_TOT);
      char msg[96]; snprintf(msg, sizeof msg, "%s: el Z80 y la referencia dan el mismo error", what);
      CHECK(e == ze, msg);
      if (e == 0) { snprintf(msg, sizeof msg, "%s: los mismos puntos", what); CHECK(sc == pts, msg); }
      return ze;
    };
    int pts;
    setrack(0, "QUIZZES"); setrack(1, "ABCDEFG");
    CHECK(both(mk(4, 7, 0, "QUIZ"), pts, "QUIZ en H8") == 0 && pts == 44, "QUIZ cruzando la casilla central: (10+1+1+10) x 2 = 44");
    CHECK(both(mk(0, 7, 0, "QUIZ"), pts, "QUIZ lejos del centro") == 6, "la primera jugada que no cubre el centro: error 6");
    CHECK(both(mk(4, 7, 0, "ZEBRA"), pts, "ZEBRA sin fichas") == 2, "una ficha que no esta en el atril: error 2");
    CHECK(both(mk(4, 7, 0, "XXXX"), pts, "XXXX") == 2, "XXXX sin esas fichas: error 2");
    setrack(0, "XQZZZZZ");
    CHECK(both(mk(4, 7, 0, "ZZZZ"), pts, "ZZZZ") == 8, "ZZZZ no es una palabra: error 8");
    { char got[17] = {0}; memcpy(got, mem + A_DCW, 16); CHECK(std::string(got) == "ZZZZ", "con el error 8, DC_W tiene la palabra que falla"); }
    CHECK(both(mk(4, 7, 0, "Z"), pts, "una letra") == 10, "una sola ficha no es una palabra: error 10");
    setrack(0, "ZEBRAXX");
    CHECK(both(mk(3, 7, 0, "ZEBRA"), pts, "ZEBRA en D8") == 0 && pts == 52, "ZEBRA: Z con letra doble (20+1+3+1+1) x 2 = 52");
    CHECK(both(mk(3, 7, 1, "ZEBRA"), pts, "ZEBRA hacia abajo en D8") == 6, "vertical que no cubre el centro: error 6");
    CHECK(both(mk(7, 3, 1, "ZEBRA"), pts, "ZEBRA hacia abajo en H4") == 0 && pts == 52, "ZEBRA vertical por la columna central: Z en letra doble (20+1+3+1+1) x 2 = 52");
    setrack(0, "RETAINS");
    CHECK(both(mk(1, 7, 0, "RETAINS"), pts, "RETAINS") == 0 && pts == 66, "RETAINS con las 7 fichas: (1+1+2+1+1+1+1) x 2 + 50 = 66");
    setrack(0, "QUI?ABC");
    CHECK(both(mk(4, 7, 0, "QUIZ"), pts, "QUIZ con comodin") == 0 && pts == 24, "el comodin vale 0: (10+1+1+0) x 2 = 24");
    setrack(0, "QUI?ZES");
    CHECK(both(mk(4, 7, 0, "QUI\xDA"), pts, "QUIZ con comodin forzado") == 0 && pts == 24, "con el bit 7 se fuerza el comodin aunque haya una Z");
    // jugar de verdad en el Z80 (QUIZ) y seguir sobre el tablero: cruzadas, letras que no coinciden, pegadas
    ref_init(R, 3, 0);
    for (int i = 0; i < 4; i++) mem[A_SEED + i] = (3 >> (8 * i)) & 255;
    mem[A_LANG] = 0; call_ep(E_INIT); call_ep(E_DOPEN);
    setrack(0, "QUIZZES"); setrack(1, "AEIOUXY");
    { Move m = mk(4, 7, 0, "QUIZ"); int sc; uint8_t nf[16]; int e = ref_check(R, dict, m, sc, nf); z_setmove(m); call_ep(E_CHECK); call_ep(E_APPLY); ref_apply(R, m, nf, sc);
      CHECK(e == 0 && same_state(R, "tras QUIZ"), "QUIZ hecha en el Z80 y en la referencia"); }
    // el jugador 1 (AEIOUXY) : sobre la Z de (7,7) hacia abajo, "ZA"? no esta; probamos lo que dice el diccionario
    both(mk(4, 8, 0, "AX"), pts, "AX pegada debajo de QU");              // solo se compara con la referencia
    CHECK(both(mk(7, 7, 1, "ZAX"), pts, "ZAX") == 0 && pts == 19, "ZAX hacia abajo desde la Z de QUIZ: 10+1+8 = 19 (la Z ya estaba: sin casilla especial)");
    CHECK(both(mk(7, 7, 1, "ZOY"), pts, "ZOY") == 8, "ZOY no es una palabra: error 8");
    both(mk(4, 7, 1, "QI"), pts, "QI por la Q");
    both(mk(5, 7, 1, "UX"), pts, "UX");
    CHECK(both(mk(4, 7, 0, "QUIZ"), pts, "QUIZ encima de QUIZ") == 4, "no pone ninguna ficha: error 4");
    CHECK(both(mk(4, 7, 0, "QUIT"), pts, "QUIT sobre QUIZ") == 3, "una letra que no coincide con el tablero: error 3");
    CHECK(both(mk(5, 7, 0, "UIZ"), pts, "UIZ sin la Q") == 4, "todo son fichas ya puestas: error 4");
    CHECK(both(mk(8, 7, 0, "AX"), pts, "AX pegada a la Z") == 5, "la palabra seria mas larga (la Z esta pegada): error 5");
    printf("casos fijos hechos\n");
  }

  if (errors) printf("%d ERRORES\n", errors); else printf("TODO OK\n");
  return errors ? 1 : 0;
}
