// La referencia del Scrabble en C++ (escrita aparte del Z80): el tablero oficial, la bolsa barajada con la misma semilla, la
// puntuacion, los errores de cada jugada en el mismo orden que scrabble.inc y el fin de la partida. La usan test_z80scrabble.cpp
// (el motor) y test_z80scrabblegame.cpp (el juego entero, con un rival simulado).
#pragma once
#include <random>
#include <set>
#include <algorithm>
#include <string>
#include <vector>
#include <cstring>
#include <cstdint>

// ---- la referencia ----------------------------------------------------------------------------
static const char* LETTERS = "ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^";   // los 30 codigos de ficha: A..Z, N con tilde, CH, LL, RR
static const int CNT[2][30] = {
  {9,2,2,4,12,2,3,2,9,1,1,4,2,6,8,2,1,6,4,6,4,2,2,1,2,1,0,0,0,0},
  {12,2,4,5,12,1,2,2,6,1,0,4,2,5,9,2,1,5,6,4,5,1,0,1,1,1,1,1,1,1}};
static const int VAL[2][30] = {
  {1,3,3,2,1,4,2,4,1,8,5,1,3,1,1,3,10,1,1,1,1,4,4,8,4,10,0,0,0,0},
  {1,3,3,2,1,4,2,4,1,8,0,1,3,1,1,3,5,1,1,1,1,4,0,8,4,10,8,5,8,8}};

static int premium(int x, int y) {            // 0 nada, 1 DL, 2 TL, 3 DW, 4 TW (las coordenadas del tablero oficial)
  static const int tw[][2] = {{0,0},{0,7},{0,14},{7,0},{7,14},{14,0},{14,7},{14,14}};
  static const int dw[][2] = {{1,1},{2,2},{3,3},{4,4},{1,13},{2,12},{3,11},{4,10},{13,1},{12,2},{11,3},{10,4},{13,13},{12,12},{11,11},{10,10},{7,7}};
  static const int tl[][2] = {{1,5},{1,9},{5,1},{5,5},{5,9},{5,13},{9,1},{9,5},{9,9},{9,13},{13,5},{13,9}};
  static const int dl[][2] = {{0,3},{0,11},{2,6},{2,8},{3,0},{3,7},{3,14},{6,2},{6,6},{6,8},{6,12},{7,3},{7,11},{8,2},{8,6},{8,8},{8,12},{11,0},{11,7},{11,14},{12,6},{12,8},{14,3},{14,11}};
  for (auto& p : tw) if (p[0] == x && p[1] == y) return 4;
  for (auto& p : dw) if (p[0] == x && p[1] == y) return 3;
  for (auto& p : tl) if (p[0] == x && p[1] == y) return 2;
  for (auto& p : dl) if (p[0] == x && p[1] == y) return 1;
  return 0;
}

struct Ref {
  int lang = 0;
  uint8_t board[225];
  uint8_t bag[100];
  int bagh = 0, bagn = 100;
  uint8_t rack[2][7];
  int score[2] = {0, 0};
  int turn = 0, pass = 0, over = 0, bcnt = 0;
  uint32_t seed = 1;
  int gnum = 0;                        // el numero de partida de esta sala (scrabble_game.inc)
};
static uint32_t xs(uint32_t& x) { x ^= x << 13; x ^= x >> 17; x ^= x << 5; return x; }
static int tileval(const Ref& R, int code) {
  if (code & 0x80 || code == '?') return 0;
  return VAL[R.lang][code - 'A'];
}
static int draw(Ref& R) { int t = R.bag[R.bagh]; R.bagh = (R.bagh + 1) % 100; R.bagn--; return t; }
static void bagput(Ref& R, int t) { R.bag[(R.bagh + R.bagn) % 100] = (uint8_t)t; R.bagn++; }
static void refill(Ref& R, int p) { for (int i = 0; i < 7; i++) if (!R.rack[p][i] && R.bagn > 0) R.rack[p][i] = (uint8_t)draw(R); }
static bool take(uint8_t* rack, int code) { for (int i = 0; i < 7; i++) if (rack[i] == code) { rack[i] = 0; return true; } return false; }
static int rackval(const Ref& R, int p) { int s = 0; for (int i = 0; i < 7; i++) if (R.rack[p][i]) s += tileval(R, R.rack[p][i]); return s; }

static void ref_init(Ref& R, uint32_t seed, int lang) {
  memset(R.board, 0, sizeof R.board); memset(R.rack, 0, sizeof R.rack);
  R.lang = lang; R.seed = seed ? seed : 1; R.score[0] = R.score[1] = 0; R.turn = R.pass = R.over = R.bcnt = 0; R.bagh = 0; R.bagn = 100;
  int n = 0;
  for (int c = 0; c < 30; c++) for (int k = 0; k < CNT[lang][c]; k++) R.bag[n++] = (uint8_t)LETTERS[c];
  R.bag[n++] = '?'; R.bag[n++] = '?';
  for (int i = 99; i >= 1; i--) { xs(R.seed); int r = (R.seed >> 8) & 255; int j = (r * (i + 1)) >> 8; std::swap(R.bag[i], R.bag[j]); }
  R.turn = 0; refill(R, 0); R.turn = 1; refill(R, 1); R.turn = 0;
}

struct Move { int x, y, dir, n; uint8_t c[16]; };
using Dict = std::set<std::string>;
static bool inboard(int x, int y) { return x >= 0 && x < 15 && y >= 0 && y < 15; }

// la palabra que pasa por (x, y) en el sentido (dx, dy), sobre el tablero b; los puntos con las casillas especiales de las fichas nuevas
static std::string run(const Ref& R, const uint8_t* b, const uint8_t* isnew, int x, int y, int dx, int dy, int& pts) {
  while (inboard(x - dx, y - dy) && b[(y - dy) * 15 + (x - dx)]) { x -= dx; y -= dy; }
  std::string w; int sum = 0, wm = 1;
  while (inboard(x, y) && b[y * 15 + x]) {
    int code = b[y * 15 + x]; w += (char)(code & 0x7F);
    int v = tileval(R, code);
    if (isnew[y * 15 + x]) { int p = premium(x, y); if (p == 1) v *= 2; else if (p == 2) v *= 3; else if (p == 3) wm *= 2; else if (p == 4) wm *= 3; }
    sum += v; x += dx; y += dy;
  }
  pts = sum * wm; return w;
}

// los errores en el mismo orden que el Z80; devuelve 0 si es legal (y sus puntos en score, las fichas puestas en nf)
static int ref_check(const Ref& R, const Dict& dict, const Move& m, int& score, uint8_t* nf, std::string* bad = nullptr) {
  if (R.over) return 11;
  if (m.n < 2 || m.n > 15) return 10;
  if (m.dir > 1 || m.x > 14 || m.y > 14 || m.x < 0 || m.y < 0) return 1;
  int dx = m.dir ? 0 : 1, dy = m.dir ? 1 : 0;
  if (m.x + dx * (m.n - 1) > 14 || m.y + dy * (m.n - 1) > 14) return 1;
  uint8_t tr[7]; memcpy(tr, R.rack[R.turn], 7);
  memset(nf, 0, 16);
  int nnew = 0;
  for (int i = 0; i < m.n; i++) {
    int x = m.x + dx * i, y = m.y + dy * i, cell = R.board[y * 15 + x], code = m.c[i], l = code & 0x7F;
    if (l < 'A' || l >= 0x5F) return 1;
    if (cell) { if ((cell & 0x7F) != l) return 3; continue; }
    if (code & 0x80) { if (!take(tr, '?')) return 2; nf[i] = (uint8_t)(l | 0x80); }
    else if (take(tr, code)) nf[i] = (uint8_t)code;
    else if (take(tr, '?')) nf[i] = (uint8_t)(code | 0x80);
    else return 2;
    nnew++;
  }
  if (!nnew) return 4;
  int bx = m.x - dx, by = m.y - dy, ax = m.x + dx * m.n, ay = m.y + dy * m.n;
  if (inboard(bx, by) && R.board[by * 15 + bx]) return 5;
  if (inboard(ax, ay) && R.board[ay * 15 + ax]) return 5;
  if (!R.bcnt) {
    bool cov = false;
    for (int i = 0; i < m.n; i++) if (m.x + dx * i == 7 && m.y + dy * i == 7) cov = true;
    if (!cov) return 6;
  }
  uint8_t b[225], isnew[225]; memcpy(b, R.board, 225); memset(isnew, 0, 225);
  for (int i = 0; i < m.n; i++) if (nf[i]) { b[(m.y + dy * i) * 15 + m.x + dx * i] = nf[i]; isnew[(m.y + dy * i) * 15 + m.x + dx * i] = 1; }
  int pts; std::string w = run(R, b, isnew, m.x, m.y, dx, dy, pts);
  if (!dict.count(w)) { if (bad) *bad = w; return 8; }
  int total = pts; bool conn = nnew < m.n;
  for (int i = 0; i < m.n; i++) if (nf[i]) {
    int p2; std::string cw = run(R, b, isnew, m.x + dx * i, m.y + dy * i, dy, dx, p2);
    if (cw.size() < 2) continue;
    conn = true;
    if (!dict.count(cw)) { if (bad) *bad = cw; return 9; }
    total += p2;
  }
  if (nnew == 7) total += 50;
  if (R.bcnt && !conn) return 7;
  score = total; return 0;
}

static void ref_apply(Ref& R, const Move& m, const uint8_t* nf, int score) {
  int dx = m.dir ? 0 : 1, dy = m.dir ? 1 : 0, nnew = 0;
  for (int i = 0; i < m.n; i++) if (nf[i]) { R.board[(m.y + dy * i) * 15 + m.x + dx * i] = nf[i]; nnew++; }
  R.bcnt += nnew;
  for (int i = 0; i < m.n; i++) if (nf[i]) take(R.rack[R.turn], (nf[i] & 0x80) ? '?' : nf[i]);
  refill(R, R.turn);
  R.score[R.turn] += score; R.pass = 0;
  if (R.bagn == 0) {
    bool empty = true; for (int i = 0; i < 7; i++) if (R.rack[R.turn][i]) empty = false;
    if (empty) { R.over = 1; int o = R.turn ^ 1, v = rackval(R, o); R.score[R.turn] += v; R.score[o] -= v; }
  }
  R.turn ^= 1;
}
static void ref_pass(Ref& R) {
  if (R.over) return;
  if (++R.pass >= 6) { R.over = 1; R.score[0] -= rackval(R, 0); R.score[1] -= rackval(R, 1); }
  R.turn ^= 1;
}
static int ref_exchange(Ref& R, const std::vector<uint8_t>& t) {
  if (R.over) return 11;
  if (t.empty() || t.size() > 7) return 10;
  if (R.bagn < 7) return 4;
  uint8_t tr[7]; memcpy(tr, R.rack[R.turn], 7);
  for (uint8_t c : t) if (!take(tr, c)) return 2;
  for (uint8_t c : t) take(R.rack[R.turn], c);
  refill(R, R.turn);
  for (uint8_t c : t) bagput(R, c);
  R.pass++; R.turn ^= 1; return 0;
}


// ---- lo que hace falta para el juego entero ------------------------------------------------------------------------
// el estado serializado, igual que la memoria del Z80 desde SB_BOARD (SB_STATE_LEN = 355 bytes)
static std::vector<uint8_t> serialize(const Ref& R) {
  std::vector<uint8_t> s;
  s.insert(s.end(), R.board, R.board + 225);
  s.insert(s.end(), R.bag, R.bag + 100);
  s.insert(s.end(), &R.rack[0][0], &R.rack[0][0] + 14);
  for (int p = 0; p < 2; p++) { s.push_back((uint8_t)(R.score[p] & 255)); s.push_back((uint8_t)((R.score[p] >> 8) & 255)); }
  s.push_back((uint8_t)R.bagh); s.push_back((uint8_t)R.bagn); s.push_back((uint8_t)R.turn); s.push_back((uint8_t)R.pass);
  s.push_back((uint8_t)R.over); s.push_back((uint8_t)R.lang);
  for (int i = 0; i < 4; i++) s.push_back((uint8_t)((R.seed >> (8 * i)) & 255));
  s.push_back((uint8_t)R.bcnt); s.push_back((uint8_t)R.gnum);
  return s;
}
static void unserialize(Ref& R, const std::vector<uint8_t>& s) {
  memcpy(R.board, s.data(), 225); memcpy(R.bag, s.data() + 225, 100); memcpy(&R.rack[0][0], s.data() + 325, 14);
  for (int p = 0; p < 2; p++) R.score[p] = (int16_t)(s[339 + 2 * p] | (s[340 + 2 * p] << 8));
  R.bagh = s[343]; R.bagn = s[344]; R.turn = s[345]; R.pass = s[346]; R.over = s[347]; R.lang = s[348];
  R.seed = s[349] | (s[350] << 8) | (s[351] << 16) | ((uint32_t)s[352] << 24); R.bcnt = s[353]; R.gnum = s[354];
}
// el resumen de 16 bits de game.inc / scrabble_game.inc (g_hash)
static uint16_t hash16(const uint8_t* s, int n) {
  uint16_t h = 0x5A5A;
  for (int i = 0; i < n; i++) {
    h = (uint16_t)((h & 0xFF00) | ((h & 0xFF) ^ s[i]));
    for (int r = 0; r < 3; r++) h = (uint16_t)((h << 1) | (h >> 15));
  }
  return h;
}
// la semilla de una partida: sale del codigo de sala (10 codigos de caracter del ZX81) y del numero de partida (g_reset)
static uint32_t seed_from_code(const uint8_t* code81, int gnum) {
  uint8_t b[4] = {(uint8_t)gnum, 0x5A, 0xA5, 0x3C};
  uint32_t x = b[0] | (b[1] << 8) | (b[2] << 16) | ((uint32_t)b[3] << 24);
  for (int i = 0; i < 10; i++) {
    x ^= (uint32_t)code81[i] << (8 * (i & 3));
    xs(x);
  }
  return x;
}
static void ref_start(Ref& R, const uint8_t* code81, int gnum, int lang) { ref_init(R, seed_from_code(code81, gnum), lang); R.gnum = gnum; }

// una jugada legal para el jugador de turno, o found = false. Prueba posiciones al azar cerca de lo ya puesto con las palabras
// del diccionario que caben (letras ya puestas + atril)
struct Found { bool found = false; Move m{}; uint8_t nf[16]; int score = 0; };
static Found find_move(const Ref& R, const Dict& dict, const std::vector<std::string>* byLen, std::mt19937& rng, int tries = 600) {
  Found f;
  std::vector<int> tiles; for (int i = 0; i < 225; i++) if (R.board[i]) tiles.push_back(i);
  for (int attempt = 0; attempt < tries && !f.found; attempt++) {
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
          if (cell) { if ((cell & 0x7F) != (uint8_t)w[i]) fit = false; } else need[(uint8_t)w[i]]++;
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
      int sc = 0; uint8_t nf[16];
      if (ref_check(R, dict, m, sc, nf) == 0) { f.found = true; f.m = m; memcpy(f.nf, nf, 16); f.score = sc; break; }
    }
  }
  return f;
}
