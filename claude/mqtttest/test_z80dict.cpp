// Prueba de la busqueda binaria en el diccionario de la SD hecha por el Z80 (EXAMPLES/DICTTEST/dicttest.bin):
// el programa de verdad en el Z80 de EightyOne, con los comandos de fichero (fopen, fseek, fread) y la hora del MCU
// simulados (la SD de mentira esta en z80net_env.h). Se genera un diccionario con el formato de TOOLS/make_dict.py y
// se compara cada busqueda con una referencia en C++: palabras que estan, que no, los extremos, prefijos y la
// prueba de tiempos (run_test). De paso cuenta cuanto Z80 cuesta cada busqueda: eso es solo la parte del Z80;
// el tiempo del MCU y de la SD solo se mide en la placa (DICTTEST.B81).
#include "z80net_env.h"
#include <random>
#include <cmath>
#include <set>

static const uint16_t RUN = 24576, OPEN = 24579, LOOKUP = 24582, RES = 0x7C00, WBUF = 0x7C40;

static std::vector<uint8_t> make_dict(const std::set<std::string>& words) {
  std::vector<uint8_t> d(16, 0);
  memcpy(d.data(), "SDICT01", 7);
  uint32_t n = (uint32_t)words.size();
  d[8] = n & 255; d[9] = (n >> 8) & 255; d[10] = (n >> 16) & 255; d[11] = (n >> 24) & 255;
  for (auto& w : words) { std::vector<uint8_t> r(16, 0); memcpy(r.data(), w.data(), w.size()); d.insert(d.end(), r.begin(), r.end()); }
  return d;
}
static bool z_lookup(const std::string& w) {
  for (int i = 0; i < 16; i++) mem[WBUF + i] = i < (int)w.size() ? (uint8_t)w[i] : 0;
  return call_ep(LOOKUP) == 1;
}

int main(int argc, char** argv) {
  if (argc < 2) { printf("uso: test_z80dict dicttest.bin\n"); return 2; }
  FILE* f = fopen(argv[1], "rb");
  if (!f) { printf("no encuentro %s\n", argv[1]); return 2; }
  size_t sz = fread(mem + 0x6000, 1, 0x1000, f); fclose(f);
  printf("dicttest.bin: %zu bytes en $6000\n", sz);
  if (0x6000 + sz > 0x7C00) { printf("ERROR el programa pisa los datos ($7C00)\n"); errors++; }
  z80_init(); z80_reset(); reset_world();

  std::mt19937 rng(777);
  auto rword = [&](int minl, int maxl) {
    int k = minl + (int)(rng() % (unsigned)(maxl - minl + 1)); std::string s;
    for (int i = 0; i < k; i++) s += (char)('A' + rng() % 26);
    return s;
  };

  // --- sin el fichero: error 1; con un fichero que no es un diccionario: error 2; vacio: 3
  CHECK(call_ep(OPEN) == 1, "sin DICT.DIC: dict_open da el error 1");
  sdfiles["DICT.DIC"] = std::vector<uint8_t>(40, 7);
  CHECK(call_ep(OPEN) == 2, "un fichero que no es un diccionario: error 2");
  { std::set<std::string> none; sdfiles["DICT.DIC"] = make_dict(none); }
  CHECK(call_ep(OPEN) == 3, "un diccionario vacio: error 3");

  // --- el diccionario de verdad: miles de palabras con prefijos comunes, A y ZZZ... en los extremos
  std::set<std::string> words = {"A", "AA", "AB", "CAT", "CATS", "CATSUP", "CATALOG", "DOG", "DOGS", "ZZZ", "ZZZZZZZZZZZZZZZ"};
  while (words.size() < 20000) words.insert(rword(1, 15));
  { std::string pre = "QUIZ"; for (int i = 0; i < 40; i++) words.insert(pre + rword(0, 11)); }   // muchas con el mismo prefijo
  sdfiles["DICT.DIC"] = make_dict(words);
  printf("diccionario de prueba: %zu palabras, %zu bytes\n", words.size(), sdfiles["DICT.DIC"].size());
  CHECK(call_ep(OPEN) == 0, "dict_open bien");

  // --- las palabras que estan, todas (se comprueba que no se salta ninguna)
  { int bad = 0; long i0 = instr_total; int n = 0;
    for (auto& w : words) { if (!z_lookup(w)) { if (bad < 5) printf("  no encuentra %s\n", w.c_str()); bad++; } n++; }
    long per = (instr_total - i0) / n;
    CHECK(bad == 0, "encuentra todas las palabras del diccionario");
    printf("  %d busquedas de palabras que estan: %ld instrucciones del Z80 por busqueda (%.1f ms a 3,25 MHz, sin contar el MCU)\n",
           n, per, per * 4.7 / 3250.0); }
  // --- las que no estan: al azar, con '~' detras (las demas letras ya estan) y los extremos
  { int bad = 0, tested = 0;
    for (int i = 0; i < 4000; i++) { std::string w = rword(1, 15); bool want = words.count(w) != 0; if (z_lookup(w) != want) { if (bad < 5) printf("  mal con %s (Z80=%d ref=%d)\n", w.c_str(), !want, want); bad++; } tested++; }
    CHECK(bad == 0, "4.000 palabras al azar: coincide con la referencia (estan o no)"); }
  { const char* absent[] = {"", "AAAAAAAAAAAAAAA", "CA", "CATSU", "CATSUPS", "DOGZ", "ZZ", "ZZZZZZZZZZZZZZ~", "~", "CAT~", "DO", "B"};
    int bad = 0;
    for (const char* w : absent) { bool want = words.count(w) != 0; if (z_lookup(w) != want) { printf("  mal con '%s'\n", w); bad++; } }
    CHECK(bad == 0, "extremos y prefijos: coincide con la referencia"); }
  { const char* present[] = {"A", "AA", "CAT", "CATS", "CATSUP", "ZZZ", "ZZZZZZZZZZZZZZZ"};
    int bad = 0; for (const char* w : present) if (!z_lookup(w)) { printf("  no encuentra '%s'\n", w); bad++; }
    CHECK(bad == 0, "las palabras de los extremos y los prefijos que si estan, las encuentra"); }

  // --- la prueba de tiempos, la de la placa (run_test)
  { long i0 = instr_total;
    int rc = call_ep(RUN);
    long used = instr_total - i0;
    CHECK(rc == 0 && mem[RES] == 0, "run_test termina bien");
    CHECK(mem[RES + 1] == 16, "run_test: encuentra las 16 palabras que estan");
    CHECK(mem[RES + 2] == 16, "run_test: no encuentra las 16 con '~'");
    int steps = mem[RES + 9] | (mem[RES + 10] << 8);
    uint32_t n = mem[RES + 11] | (mem[RES + 12] << 8) | (mem[RES + 13] << 16);
    CHECK(n == words.size(), "run_test lee bien el numero de palabras");
    int maxsteps = 0; { uint32_t k = (uint32_t)words.size(); while (k) { maxsteps++; k >>= 1; } }
    CHECK(steps >= 16 && steps <= 32 * (maxsteps + 1), "run_test: el numero de pasos es el de una busqueda binaria");
    printf("  run_test: %d pasos en 32 busquedas (%.1f por busqueda; con %zu palabras, log2 = %.1f); %ld instrucciones del Z80 en total\n",
           steps, steps / 32.0, words.size(), std::log2((double)words.size()), used);
    uint32_t t0 = ((mem[RES + 3] * 60 + mem[RES + 4]) * 100 + mem[RES + 5]), t1 = ((mem[RES + 6] * 60 + mem[RES + 7]) * 100 + mem[RES + 8]);
    printf("  la hora (simulada) al salir: %02d:%02d.%02d, al acabar: %02d:%02d.%02d\n", mem[RES + 3], mem[RES + 4], mem[RES + 5], mem[RES + 6], mem[RES + 7], mem[RES + 8]);
    (void)t0; (void)t1; }

  // --- un diccionario grande de verdad (el tamano del castellano): solo los pasos y la correccion
  { std::set<std::string> big;
    while (big.size() < 300000) big.insert(rword(2, 15));
    sdfiles["DICT.DIC"] = make_dict(big);
    CHECK(call_ep(OPEN) == 0, "dict_open con 300.000 palabras");
    int bad = 0; long i0 = instr_total; int n = 0;
    for (int i = 0; i < 300; i++) { auto it = big.begin(); std::advance(it, rng() % big.size()); if (!z_lookup(*it)) bad++; n++; }
    for (int i = 0; i < 300; i++) { std::string w = rword(2, 15); if (z_lookup(w) != (big.count(w) != 0)) bad++; n++; }
    CHECK(bad == 0, "300.000 palabras: 600 busquedas bien");
    printf("  300.000 palabras: %ld instrucciones del Z80 por busqueda\n", (instr_total - i0) / n); }

  if (errors) printf("%d ERRORES\n", errors); else printf("TODO OK\n");
  return errors ? 1 : 0;
}
