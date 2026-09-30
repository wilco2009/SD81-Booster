// Conversion de listados BASIC en texto (.B81) a imagen .P.
//
// Reproduce el cargador de listados de EightyOne (IBasicLoader +
// zx81BasicLoader) con sus opciones por defecto, para que un .B81 cargado
// desde la SD quede igual que cargado en el emulador:
//   - no tokeniza dentro de REM ni de cadenas, y conserva los espacios;
//   - solo reconoce las palabras clave tal como las escribe el listador
//     (" PRINT ", "CODE ", "<>"...), no las variantes (GO TO, CODE(...);
//   - numera de 10 en 10 las lineas sin numero;
//   - el programa solo arranca al cargarlo si hay una linea
//     "#!basic-start=N": desde la linea N (o la siguiente que exista).
//     Sin ella se queda parado, como un programa tecleado.
// El formato es el del listador: %X caracter inverso, \XX codigo en
// hexadecimal, \:: \.' y demas graficos, \" o "" comillas dentro de una
// cadena, \ al final de linea para continuarla, # comentario, "@etiqueta:"
// al principio de linea y GOTO/GOSUB/RUN/LIST/LLIST @etiqueta.
//
// Diferencias a proposito con EightyOne:
//   - si una palabra clave aparece primero dentro de otra palabra (STAT y
//     AT, SPIN y PI), EightyOne deja de buscarla en esa linea; aqui se sigue;
//   - un "." suelto no es un numero (en EightyOne se queda en un bucle);
//   - el exponente de los numeros se calcula con frexp (exacto) y no con
//     logaritmos;
//   - admite "£" en UTF-8 ademas de en Latin-1, y minusculas en %x.

#include "B81.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

namespace {

const uint8_t BLANK = 0x01;       // posicion ya tratada, no se emite como caracter

const uint8_t ZX_QUOTE   = 0x0B;
const uint8_t ZX_NEWLINE = 0x76;
const uint8_t ZX_NUMBER  = 0x7E;
const uint8_t ZX_DQUOTE  = 0xC0;
const uint8_t ZX_REM     = 0xEA;
const uint8_t ZX_LOAD    = 0xEF;

const int LINE_INC   = 10;        // incremento de las lineas sin numero
const int HEAD       = 256;       // principio de linea que se guarda siempre
const int SLACK      = 64;        // margen para cambiar etiquetas por numeros
const int MAX_LABELS = 64;
const int LABEL_LEN  = 24;
const int RAM_START  = 16393;     // direccion de VERSN
const int SYSVARS    = 116;       // de VERSN al principio del programa

struct Token { uint8_t code; const char* text; };

// En el orden en que las busca EightyOne, de mayor a menor codigo: asi las
// largas se quitan antes que las que contienen (" PRINT " antes que "INT ").
const Token kTokens[] = {
  {255," COPY "},  {254," RETURN "}, {253," CLEAR "}, {252," UNPLOT "},
  {251," CLS "},   {250," IF "},     {249," RAND "},  {248," SAVE "},
  {247," RUN "},   {246," PLOT "},   {245," PRINT "}, {244," POKE "},
  {243," NEXT "},  {242," PAUSE "},  {241," LET "},   {240," LIST "},
  {239," LOAD "},  {238," INPUT "},  {237," GOSUB "}, {236," GOTO "},
  {235," FOR "},   {234," REM "},    {233," DIM "},   {232," CONT "},
  {231," SCROLL "},{230," NEW "},    {229," FAST "},  {228," SLOW "},
  {227," STOP "},  {226," LLIST "},  {225," LPRINT "},{224," STEP "},
  {223," TO "},    {222," THEN "},   {221,"<>"},      {220,">="},
  {219,"<="},      {218," AND "},    {217," OR "},    {216,"**"},
  {215,"NOT "},    {214,"CHR$ "},    {213,"STR$ "},   {212,"USR "},
  {211,"PEEK "},   {210,"ABS "},     {209,"SGN "},    {208,"SQR "},
  {207,"INT "},    {206,"EXP "},     {205,"LN "},     {204,"ATN "},
  {203,"ACS "},    {202,"ASN "},     {201,"TAN "},    {200,"COS "},
  {199,"SIN "},    {198,"LEN "},     {197,"VAL "},    {196,"CODE "},
  {194,"TAB "},    {193,"AT "},      {66,"PI"},       {65,"INKEY$"},
  {64,"RND"}
};

// Graficos: \ y dos caracteres
struct Graphic { char a, b; uint8_t code; };
const Graphic kGraphics[] = {
  {' ',' ',0},   {'\'',' ',1},  {' ','\'',2},  {'\'','\'',3}, {'.',' ',4},
  {':',' ',5},   {'.','\'',6},  {':','\'',7},  {'#','#',8},   {',',',',9},
  {'~','~',10},  {':',':',128}, {'.',':',129}, {':','.',130}, {'.','.',131},
  {'\'',':',132},{' ',':',133}, {'\'','.',134},{' ','.',135}, {'@','@',136},
  {';',';',137}, {'!','!',138}
};

// Palabras clave que pueden ir seguidas de un numero de linea (o etiqueta)
const char* const kLineKeywords[] = { "GOTO", "GOSUB", "RUN", "LIST", "LLIST" };

struct Label { char name[LABEL_LEN]; int line; };

struct Conv {
  B81Reader* in;
  B81Error*  err;
  bool       failed;
  bool       firstPass;

  int  fileLine;      // lineas fisicas leidas
  int  lineStart;     // linea fisica donde empieza la linea actual
  int  pend;          // byte leido de mas al mirar el UTF-8 (-2: ninguno)

  // Linea actual: " " + partes (sin las \ de continuacion). El principio
  // va siempre a head; en la segunda pasada, la linea entera va a lb.
  char  head[HEAD + 1];
  int   len;
  int   maxLen;

  int  current;       // ultimo numero de linea asignado
  int  lineNumber;
  int  lnLen;         // caracteres del numero, con los espacios de delante
  int  labelLen;      // longitud de la etiqueta con la @, 0 si no hay
  int  colonPos;

  Label labels[MAX_LABELS];
  int   nLabels;
  int   startLine;    // #!basic-start=N (-2: no hay)

  // segunda pasada
  int      size;      // tamano de lb, tk, out y pop
  char*    lb;        // la linea; lo ya tratado queda como BLANK
  char*    tk;        // copia para buscar palabras clave
  uint8_t* out;       // codigo ZX81 de cada posicion...
  uint8_t* pop;       // ... si esta puesto
  uint8_t* p;
  int      pos;
};

inline bool isDigit(int ch)  { return ch >= '0' && ch <= '9'; }
inline bool isAlpha(int ch)  { return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z'); }
inline bool isAlnum(int ch)  { return isDigit(ch) || isAlpha(ch); }
inline bool isXDigit(int ch) { return isDigit(ch) || ((ch|0x20) >= 'a' && (ch|0x20) <= 'f'); }
inline bool isSpace(int ch)  { return ch == ' ' || (ch >= '\t' && ch <= '\r'); }
inline int  toUpper(int ch)  { return (ch >= 'a' && ch <= 'z') ? ch - 32 : ch; }
inline int  hexVal(int ch)   { return isDigit(ch) ? ch - '0' : (ch|0x20) - 'a' + 10; }

// ---------------------------------------------------------------- errores

bool fail(Conv& c, const char* msg, int ch = -1)
{
  if (c.failed) return false;
  c.failed = true;
  B81Error& e = *c.err;
  e.fileLine  = c.lineStart;
  e.basicLine = c.lineNumber;
  strncpy(e.msg, msg, sizeof(e.msg) - 1);
  if (ch >= 0) {                      // anade el caracter que ha fallado
    int n = strlen(e.msg);
    if (ch >= 0x20 && ch < 0x7F) {
      if (n < (int)sizeof(e.msg) - 3) { e.msg[n++] = ' '; e.msg[n++] = ch; }
    } else if (n < (int)sizeof(e.msg) - 5) {
      const char* hex = "0123456789ABCDEF";
      e.msg[n++] = ' '; e.msg[n++] = '\\';
      e.msg[n++] = hex[(ch >> 4) & 15]; e.msg[n++] = hex[ch & 15];
    }
    e.msg[n] = 0;
  }
  int n = c.len - 1;                  // la linea, sin el espacio del principio
  if (n > HEAD - 1) n = HEAD - 1;
  if (n > (int)sizeof(e.text) - 1) n = sizeof(e.text) - 1;
  for (int i = 0; i < n; i++) {
    int ch2 = (uint8_t)c.head[i + 1];
    e.text[i] = (ch2 >= 0x20 && ch2 < 0x7F) ? ch2 : '?';
  }
  e.text[n > 0 ? n : 0] = 0;
  return false;
}

void emit(Conv& c, uint8_t b)
{
  if (c.pos >= B81_MAX_P) { fail(c, "PROGRAM TOO LARGE"); return; }
  c.p[c.pos++] = b;
}

void putWord(Conv& c, int at, int w)
{
  c.p[at] = w & 0xFF;
  c.p[at + 1] = (w >> 8) & 0xFF;
}

// ---------------------------------------------------------------- lectura

int nextByte(Conv& c)
{
  if (c.pend != -2) { int b = c.pend; c.pend = -2; return b; }
  int b = c.in->getByte();
  if (b == 0xC2) {                    // "£" en UTF-8 (C2 A3): como en Latin-1
    int b2 = c.in->getByte();
    if (b2 == 0xA3) return 0xA3;
    c.pend = b2;
  }
  return b;
}

void putChar(Conv& c, char ch)
{
  if (c.len < HEAD) c.head[c.len] = ch;
  if (c.lb && c.len < c.size - SLACK - 2) c.lb[c.len] = ch;
  c.len++;
}

// Lee una linea fisica y la anade a la actual, sin el \r del final (los
// \0 se descartan). false al final del fichero si no queda nada, como
// getline. blank: solo espacios o tabuladores; cont: termina en \.
bool readPhys(Conv& c, bool& blank, bool& cont, int& firstNB)
{
  int b = nextByte(c);
  if (b < 0) return false;
  c.fileLine++;
  int st = c.len, nbPos = -1, last = -1, prev = -1;
  firstNB = 0;
  while (b >= 0 && b != '\n') {
    if (b != 0) {
      putChar(c, (char)b);
      if (nbPos < 0 && b != ' ' && b != '\t') { nbPos = c.len - 1; firstNB = b; }
      prev = last; last = b;
    }
    b = nextByte(c);
  }
  if (c.len > st && last == '\r') { c.len--; last = prev; }
  blank = (nbPos < 0 || nbPos >= c.len);
  cont = !blank && c.len > st && last == '\\';
  if (cont) c.len--;
  return true;
}

// Siguiente linea del listado, como IBasicLoader::ReadLine: se salta las
// lineas en blanco y los comentarios (#), y une las que acaban en \.
bool readLine(Conv& c)
{
  for (;;) {
    c.len = 0;
    c.lineNumber = -1;
    putChar(c, ' ');
    bool first = true, cont;
    int lineFirstNB = 0;
    do {
      bool blank;
      int fnb, st;
      do {
        st = c.len;
        if (!readPhys(c, blank, cont, fnb)) return false;
        if (blank) c.len = st;
      } while (blank);
      if (first) { lineFirstNB = fnb; c.lineStart = c.fileLine; first = false; }
    } while (cont);

    if (lineFirstNB != '#') return true;

    // "#!basic-start=N" en la columna 0: linea de arranque
    if (c.startLine == -2 && c.len > 15 && !memcmp(c.head + 1, "#!basic-start=", 14)) {
      int i = 15, neg = 0;
      c.head[c.len < HEAD ? c.len : HEAD] = 0;
      while (isSpace(c.head[i])) i++;
      if (c.head[i] == '-' || c.head[i] == '+') neg = (c.head[i++] == '-');
      if (isDigit(c.head[i])) {
        long v = 0;
        while (isDigit(c.head[i]) && v < 100000) v = v * 10 + (c.head[i++] - '0');
        c.startLine = neg ? -v : v;
      }
    }
  }
}

char charAt(Conv& c, int i)
{
  if (i >= c.len) return 0;
  return c.lb ? c.lb[i] : (i < HEAD ? c.head[i] : 0);
}

int findLabel(Conv& c, const char* name, int n)
{
  for (int i = 0; i < c.nLabels; i++)
    if ((int)strlen(c.labels[i].name) == n && !memcmp(c.labels[i].name, name, n)) return i;
  return -1;
}

// Numero de linea: el del principio, el siguiente a la etiqueta o el
// anterior mas 10.
bool numberLine(Conv& c)
{
  c.lnLen = 0;
  c.labelLen = 0;

  // numero al principio, como strtol
  int i = 0;
  while (isSpace(charAt(c, i))) i++;
  int j = i, neg = 0;
  if (charAt(c, j) == '+' || charAt(c, j) == '-') neg = (charAt(c, j++) == '-');
  int k = j;
  long v = 0;
  while (isDigit(charAt(c, k))) {
    if (v < 100000000) v = v * 10 + (charAt(c, k) - '0');
    k++;
  }

  if (k > j) {
    c.lnLen = k;
    c.lineNumber = neg ? -v : v;
    if (c.lineNumber <= c.current) return fail(c, "LINE NUMBER NOT INCREASING");
    c.current = c.lineNumber;
  } else {
    // "@etiqueta:" en la columna 0 (el ':' se busca en el principio)
    int colon = -1;
    if (charAt(c, 1) == '@') {
      int lim = c.len < HEAD ? c.len : HEAD;
      for (int n = 0; n < lim; n++)
        if (charAt(c, n) == ':') { colon = n; break; }
    }
    if (c.current == -1) c.current = 0;
    c.current += LINE_INC;
    c.lineNumber = c.current;
    if (colon >= 0) {
      if (colon == 2) return fail(c, "INVALID LABEL");
      c.labelLen = colon - 1;
      c.colonPos = colon;
      if (c.firstPass) {
        char name[LABEL_LEN];
        if (c.labelLen >= LABEL_LEN) return fail(c, "LABEL TOO LONG");
        for (int n = 0; n < c.labelLen; n++) name[n] = charAt(c, n + 1);
        if (findLabel(c, name, c.labelLen) < 0) {
          if (c.nLabels == MAX_LABELS) return fail(c, "TOO MANY LABELS");
          memcpy(c.labels[c.nLabels].name, name, c.labelLen);
          c.labels[c.nLabels].name[c.labelLen] = 0;
          c.labels[c.nLabels].line = c.lineNumber;
          c.nLabels++;
        }
      }
    }
  }
  if (c.lineNumber < 0 || c.lineNumber > 16383) return fail(c, "LINE NUMBER OUT OF RANGE");
  return true;
}

// ---------------------------------------------------------------- tokenizado

int asciiToZx(Conv& c, int ch)
{
  ch = toUpper(ch);
  if (ch >= 'A' && ch <= 'Z') return ch - 'A' + 38;
  if (isDigit(ch)) return ch - '0' + 28;
  switch (ch) {
    case ' ':  return 0;
    case '"':  return 11;
    case 0xA3:
    case '#':  return 12;
    case '$':  return 13;
    case ':':  return 14;
    case '?':  return 15;
    case '(':  return 16;
    case ')':  return 17;
    case '>':  return 18;
    case '<':  return 19;
    case '=':  return 20;
    case '+':  return 21;
    case '-':  return 22;
    case '*':  return 23;
    case '/':  return 24;
    case ';':  return 25;
    case ',':  return 26;
    case '.':  return 27;
  }
  fail(c, "INVALID CHARACTER", ch);
  return -1;
}

// GOTO @etiqueta -> GOTO numero, en el texto (EightyOne lo hace al
// tokenizar la palabra clave; el resultado es el mismo).
bool replaceLabels(Conv& c, int& n)
{
  char* lb = c.lb;
  bool inq = false;
  for (int i = 0; i < n; i++) {
    char ch = lb[i];
    if (ch == '\\') { i++; continue; }
    if (ch == '"') { inq = !inq; continue; }
    if (inq) continue;
    if (!memcmp(lb + i, " REM ", 5)) return true;
    if (ch != '@' || i < 2 || lb[i - 1] != ' ') continue;

    // palabra anterior: una de las que admiten numero de linea
    int e = i - 1;
    while (e > 0 && lb[e - 1] == ' ') e--;
    int s = e;
    while (s > 0 && isAlpha(lb[s - 1])) s--;
    if (s == 0 || lb[s - 1] != ' ') continue;
    bool kw = false;
    for (unsigned k = 0; k < sizeof(kLineKeywords) / sizeof(kLineKeywords[0]); k++) {
      int kl = strlen(kLineKeywords[k]);
      if (e - s == kl && !memcmp(lb + s, kLineKeywords[k], kl)) kw = true;
    }
    if (!kw) continue;

    int le = i + 1;
    while (le < n && lb[le] != ' ') le++;
    if (le - i < 2) return fail(c, "INVALID LABEL");
    int idx = findLabel(c, lb + i, le - i);
    if (idx < 0) return fail(c, "UNKNOWN LABEL");

    char num[8];
    int nl = 0, v = c.labels[idx].line;
    char tmp[8];
    do { tmp[nl++] = '0' + v % 10; v /= 10; } while (v);
    for (int d = 0; d < nl; d++) num[d] = tmp[nl - 1 - d];

    int delta = nl - (le - i);
    if (n + delta > c.size - 2) return fail(c, "LINE TOO LONG");
    memmove(lb + le + delta, lb + le, n - le);
    memcpy(lb + i, num, nl);
    n += delta;
    lb[n] = 0;
    i += nl - 1;
  }
  return true;
}

void maskStrings(char* buf)
{
  char* q = strstr(buf, "\"");
  if (!q) return;
  char* r = strstr(buf, " REM ");
  if (r && r < q) return;
  bool inq = false;
  for (buf = q; *buf; buf++) {
    if (*buf == '"') {
      inq = !inq;
    } else if (inq) {
      *buf = BLANK;
    } else {
      q = strstr(buf + 1, "\"");
      if (!q) return;
      r = strstr(buf + 1, " REM ");
      if (r && r < q) return;
    }
  }
}

void maskRem(char* buf)
{
  char* pos = buf;
  char* r;
  for (;;) {
    r = strstr(pos, " REM ");
    if (!r) return;
    char* q = strstr(pos, "\"");
    if (q && q < r) {
      q = strstr(q + 1, "\"");
      if (!q) return;
      pos = q + 1;
    } else {
      break;
    }
  }
  char* p = r + 5;
  memset(p, BLANK, strlen(p));
}

void tokenise(Conv& c)
{
  for (unsigned t = 0; t < sizeof(kTokens) / sizeof(kTokens[0]); t++) {
    const char* pat = kTokens[t].text;
    int L = strlen(pat);
    bool bs = pat[0] == ' ', ba = isAlpha(pat[0]);
    bool es = pat[L - 1] == ' ', ea = isAlpha(pat[L - 1]);
    char* from = c.tk;
    for (;;) {
      char* m = strstr(from, pat);
      if (!m) break;
      bool startOk = bs || !ba || m == c.tk || !isAlnum((uint8_t)m[-1]);
      bool endOk = es || !ea || !isAlnum((uint8_t)m[L]);
      if (!startOk || !endOk) { from = m + 1; continue; }
      int so = bs ? 1 : 0, eo = es ? L - 1 : L;
      for (int b = so; b < eo; b++) m[b] = BLANK;
      int off = m - c.tk;
      for (int b = 0; b < L; b++) c.lb[off + b] = BLANK;
      c.out[off + so] = kTokens[t].code;
      c.pop[off + so] = 1;
      from = m;
    }
  }
}

// El '*' que va detras de LOAD (LOAD *128C) introduce una orden del SD81
bool starAfterLoad(Conv& c, int star)
{
  for (int i = star - 1; i >= 0; i--) {
    if (c.pop[i] && c.out[i] == ZX_LOAD) return true;
    if ((uint8_t)c.lb[i] != BLANK && c.lb[i] != ' ') return false;
  }
  return false;
}

bool startOfNumber(Conv& c, int index)
{
  const char* lb = c.lb;
  if (lb[index] != '.' && !isDigit(lb[index])) return false;
  bool skipped = false;
  while (index > 0 && c.tk[index - 1] == ' ') { index--; skipped = true; }
  if (index == 0) return true;

  // Un digito justo detras del '*' de una orden del SD81 es parte del
  // nombre ("LOAD *128C", "LOAD *64C"): la ROM compara el nombre caracter a
  // caracter y un numero oculto lo romperia.
  if (lb[index - 1] == '*' && starAfterLoad(c, index - 1)) return false;
  int prev = (uint8_t)lb[index - 1];
  if (!isAlpha(prev) && !isDigit(prev)) return true;

  // "LOAD *MAP 7,63": el nombre de la orden no es una variable que siga
  // tras el espacio, asi que el 7 es un numero
  if (skipped) {
    int ws = index - 1;
    while (ws > 0 && isAlpha(lb[ws - 1])) ws--;
    if (ws > 0 && lb[ws - 1] == '*') return true;
  }
  return false;
}

// Coma flotante del ZX81: exponente + 128 y mantisa sin el 1 de delante
// (el bit de signo queda a 0: el signo de un literal es un operador).
void emitFloat(Conv& c, double v)
{
  uint8_t b[5] = { 0, 0, 0, 0, 0 };
  if (v != 0) {
    int e;
    double m = frexp(v, &e);          // v = m * 2^e, 0.5 <= m < 1
    int ex = e - 1;
    if (ex < -129 || ex > 126) { fail(c, "NUMBER OUT OF RANGE"); return; }
    uint32_t f = (uint32_t)floor((2 * m - 1) * 2147483648.0);
    b[0] = ex + 129;
    b[1] = f >> 24; b[2] = f >> 16; b[3] = f >> 8; b[4] = f;
  }
  for (int i = 0; i < 5; i++) emit(c, b[i]);
}

// Literal numerico: sus cifras, los espacios de detras y el numero oculto
// (7E + 5 bytes). false si no es un numero (un "." suelto).
bool embeddedNumber(Conv& c, int& index)
{
  const char* lb = c.lb;
  char w[64];
  int wl = 0, total = strlen(lb + index);
  for (int j = 0; j < total && wl < 63; j++)
    if (c.tk[index + j] != ' ') w[wl++] = lb[index + j];
  w[wl] = 0;

  int k = 0;
  while (isDigit(w[k])) k++;
  if (w[k] == '.') { k++; while (isDigit(w[k])) k++; }
  if (k == 1 && w[0] == '.') return false;
  if (w[k] == 'E' || w[k] == 'e') {
    int e = k + 1;
    if (w[e] == '+' || w[e] == '-') e++;
    if (isDigit(w[e])) { while (isDigit(w[e])) e++; k = e; }
  }
  w[k] = 0;
  double value = strtod(w, 0);

  int ws = 0;
  for (int n = 0; n < k; n++) {
    while (lb[index + ws] == ' ' || (uint8_t)lb[index + ws] == BLANK) ws++;
    ws++;
  }
  int end = index + ws;
  for (; index < end; index++)
    if (c.out[index] != BLANK) emit(c, c.out[index]);
  while (lb[index] == ' ') { emit(c, 0); index++; }
  index--;
  emit(c, ZX_NUMBER);
  emitFloat(c, value);
  return true;
}

// Como IBasicLoader::ProcessLine + zx81BasicLoader::OutputLine
bool processLine(Conv& c)
{
  char* lb = c.lb;
  int offset = c.labelLen ? c.labelLen + 2 : 0;
  bool exists = c.labelLen ? (c.len > c.colonPos + 1) : (c.len > c.lnLen);
  int n = c.len - offset;
  memmove(lb, lb + offset, n);
  memset(lb + n, 0, c.size - n);
  if (!replaceLabels(c, n)) return false;

  memset(c.out, BLANK, n);
  memset(c.out + n, 0, c.size - n);
  memset(c.pop, 0, c.size);

  // el numero de linea no se emite; sin numero, se deja un espacio delante
  if (c.lnLen > 0) {
    memset(lb, BLANK, c.lnLen);
  } else {
    int i = 0;
    while (lb[i] == ' ' || lb[i] == '\t') lb[i++] = BLANK;
    if (i > 0) lb[i - 1] = ' ';
  }

  // %X: caracter inverso
  for (int p = 0; lb[p]; p++) {
    if (lb[p] == '%') {
      lb[p++] = BLANK;
      int z = asciiToZx(c, (uint8_t)lb[p]);
      if (z < 0) return false;
      c.out[p] = 0x80 | z;
      c.pop[p] = 1;
      lb[p] = BLANK;
    }
  }

  // \XX, \" y graficos
  for (int p = 0; lb[p]; p++) {
    if (lb[p] != '\\') continue;
    lb[p++] = BLANK;
    int c1 = (uint8_t)lb[p], z = -1;
    if (!c1) return fail(c, "INCOMPLETE ESCAPE SEQUENCE");
    lb[p] = BLANK;
    if (c1 == '"') {
      z = ZX_DQUOTE;
    } else {
      int c2 = (uint8_t)lb[++p];
      if (!c2) return fail(c, "INCOMPLETE ESCAPE SEQUENCE");
      lb[p] = BLANK;
      if (isXDigit(c1)) {
        if (!isXDigit(c2)) return fail(c, "INVALID CHARACTER CODE", c2);
        z = hexVal(c1) * 16 + hexVal(c2);
      } else {
        for (unsigned g = 0; g < sizeof(kGraphics) / sizeof(kGraphics[0]); g++)
          if (kGraphics[g].a == c1 && kGraphics[g].b == c2) z = kGraphics[g].code;
        if (z < 0) return fail(c, "INVALID GRAPHIC", c1);
      }
    }
    c.out[p] = z;
    c.pop[p] = 1;
  }

  // "" dentro de una cadena: comillas (salvo en las lineas REM)
  int i = 0;
  while ((uint8_t)lb[i] == BLANK || lb[i] == ' ') i++;
  if (strncmp(lb + i, "REM ", 4)) {
    bool inq = false;
    for (i = 0; lb[i]; i++) {
      if (!inq) {
        if (lb[i] == '"') inq = true;
      } else if (lb[i] == '"') {
        if (lb[i + 1] == '"') {
          lb[i] = BLANK;
          c.out[i] = ZX_DQUOTE;
          c.pop[i] = 1;
          lb[++i] = BLANK;
        } else {
          inq = false;
        }
      }
    }
  }

  // palabras clave, fuera de las cadenas y del texto de los REM
  int m = strlen(lb);
  memset(c.tk, 0, c.size);
  memcpy(c.tk, lb, m);
  c.tk[m] = ' ';
  maskStrings(c.tk);
  maskRem(c.tk);
  tokenise(c);

  // el resto, caracter a caracter
  for (i = 1; lb[i]; i++) {
    if ((uint8_t)lb[i] == BLANK) continue;
    int z = asciiToZx(c, (uint8_t)lb[i]);
    if (z < 0) return false;
    c.out[i] = z;
    c.pop[i] = 1;
  }

  if (!exists) return true;

  emit(c, c.lineNumber >> 8);
  emit(c, c.lineNumber & 0xFF);
  int lenPos = c.pos;
  emit(c, 0);
  emit(c, 0);
  int start = c.pos;
  bool inq = false, inrem = false;
  for (i = 0; lb[i] && !c.failed; i++) {
    if (!c.pop[i]) continue;
    uint8_t ch = c.out[i];
    if (!inrem && ch == ZX_QUOTE) inq = !inq;
    else if (!inq && ch == ZX_REM) inrem = true;
    if (!inq && !inrem && startOfNumber(c, i) && embeddedNumber(c, i)) continue;
    emit(c, ch);
  }
  emit(c, ZX_NEWLINE);
  if (c.failed) return false;
  putWord(c, lenPos, c.pos - start);
  return true;
}

// Variables del sistema, como zx81BasicLoader::OutputStartOfProgramData
void header(Conv& c)
{
  memset(c.p, 0, SYSVARS);
  putWord(c, 22, 0x405D);             // MEM
  c.p[25] = 0x02;                     // DF_SZ
  putWord(c, 28, 0xFFFF);             // LAST_K
  c.p[31] = 0x37;                     // MARGIN
  putWord(c, 39, 0x0C8D);             // T_ADDR
  putWord(c, 41, 0x4321);             // SEED
  putWord(c, 43, 0xE6E0);             // FRAMES
  c.p[47] = 0xBC;                     // PR_CC
  putWord(c, 48, 0x1821);             // S_POSN
  c.p[50] = 0x40;                     // CDFLAG
  c.p[83] = 0x76;                     // final de PRBUFF
  c.pos = SYSVARS;
}

// DFILE colapsado, zona de variables vacia y punteros. NXTLIN apunta a la
// zona de variables (el programa no arranca) salvo que "#!basic-start=N"
// pida arrancar: entonces a la linea N o la siguiente que exista
void trailer(Conv& c)
{
  int dfile = RAM_START + c.pos;
  for (int d = 0; d < 25; d++) emit(c, ZX_NEWLINE);
  int vars = RAM_START + c.pos;
  emit(c, 0x80);
  if (c.failed) return;
  int eline = RAM_START + c.pos;
  putWord(c, 3, dfile);               // D_FILE
  putWord(c, 5, dfile + 1);           // DF_CC
  putWord(c, 7, vars);                // VARS
  putWord(c, 11, vars + 1);           // E_LINE
  putWord(c, 13, vars + 5);           // CH_ADD
  putWord(c, 17, eline + 5);          // STKBOT
  putWord(c, 19, eline + 5);          // STKEND
  putWord(c, 32, vars);               // NXTLIN

  if (c.startLine >= 0) {
    for (int off = SYSVARS; off < dfile - RAM_START; ) {
      int ln = (c.p[off] << 8) | c.p[off + 1];
      if (ln >= c.startLine) { putWord(c, 32, RAM_START + off); break; }
      off += 4 + (c.p[off + 2] | (c.p[off + 3] << 8));
    }
  }
}

} // namespace

uint16_t b81_to_p(B81Reader& in, uint8_t* out, B81Error& err)
{
  memset(&err, 0, sizeof(err));
  err.basicLine = -1;

  Conv* cp = (Conv*)calloc(1, sizeof(Conv));
  if (!cp) {
    strcpy(err.msg, "OUT OF MEMORY");
    return 0;
  }
  Conv& c = *cp;
  c.in = &in;
  c.err = &err;
  c.p = out;
  c.startLine = -2;
  c.lineNumber = -1;

  // primera pasada: numeracion, etiquetas y linea mas larga
  c.firstPass = true;
  c.pend = -2;
  c.current = -1;
  if (!in.rewind()) fail(c, "CAN NOT READ FILE");
  while (!c.failed && readLine(c)) {
    numberLine(c);
    if (c.len > c.maxLen) c.maxLen = c.len;
  }

  // segunda pasada: tokenizado
  if (!c.failed) {
    c.size = c.maxLen + SLACK + 2;
    c.lb  = (char*)malloc(c.size);
    c.tk  = (char*)malloc(c.size);
    c.out = (uint8_t*)malloc(c.size);
    c.pop = (uint8_t*)malloc(c.size);
    if (!c.lb || !c.tk || !c.out || !c.pop) {
      c.lineStart = 0;
      c.lineNumber = -1;
      c.len = 0;
      fail(c, "OUT OF MEMORY");
    } else if (!in.rewind()) {
      fail(c, "CAN NOT READ FILE");
    } else {
      c.firstPass = false;
      c.fileLine = 0;
      c.pend = -2;
      c.current = -1;
      header(c);
      while (!c.failed && readLine(c)) {
        if (numberLine(c)) processLine(c);
      }
      if (!c.failed) trailer(c);
    }
    free(c.lb);
    free(c.tk);
    free(c.out);
    free(c.pop);
  }

  uint16_t result = c.failed ? 0 : c.pos;
  free(cp);
  return result;
}
