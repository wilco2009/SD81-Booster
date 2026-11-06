#include "JPG.h"
#include <string.h>

// Ver JPG.h. Escrito a partir de la norma (ITU-T T.81), sin codigo de otras bibliotecas.
// Referencia en Python con el mismo algoritmo entero: EXAMPLES/PHOTO/jpg2i16.py

#define JPG_MAX_PIXELS   128000000u    // de la imagen entera (el tiempo de decodificacion crece con ellos)
#define JPG_MAX_SAMPLES  16000000u     // de la parte que se usa, ya reducida: con 255 * muestras el acumulador cabe en 32 bits
#define JPG_LUT_BITS     7

// Orden en zigzag -> posicion natural (fila * 8 + columna) dentro del bloque
static const uint8_t ZZ[64] = {
   0,  1,  8, 16,  9,  2,  3, 10, 17, 24, 32, 25, 18, 11,  4,  5,
  12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13,  6,  7, 14, 21, 28,
  35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
  58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63
};

// IDCT de N puntos ortonormal (N = 2, 4, 8; con N = 1 es un solo coeficiente), por 4096: T[x][u] = 4096 * sqrt(2/N) * c(u) * cos((2x+1) u pi / 2N), c(0) = 1/sqrt(2)
static const int16_t T2[4]  = { 2896, 2896,
                                2896, -2896 };
static const int16_t T4[16] = { 2048,  2676,  2048,  1108,
                                2048,  1108, -2048, -2676,
                                2048, -1108, -2048,  2676,
                                2048, -2676,  2048, -1108 };
static const int16_t T8[64] = { 1448,  2009,  1892,  1703,  1448,  1138,   784,   400,
                                1448,  1703,   784,  -400, -1448, -2009, -1892, -1138,
                                1448,  1138,  -784, -2009, -1448,   400,  1892,  1703,
                                1448,   400, -1892, -1138,  1448,  1703,  -784, -2009,
                                1448,  -400, -1892,  1138,  1448, -1703,  -784,  2009,
                                1448, -1138,  -784,  2009, -1448,  -400,  1892, -1703,
                                1448, -1703,   784,   400, -1448,  2009, -1892,  1138,
                                1448, -2009,  1892, -1703,  1448, -1138,   784,  -400 };

struct JpgComp {
  uint8_t  id, h, v, tq;          // de la cabecera SOF
  uint8_t  td, ta;                // tablas Huffman (SOS)
  uint8_t  n;                     // lado de sus bloques reducidos (N, o 2N si esta submuestreado a la mitad en los dos ejes: la croma de 4:2:0)
  uint8_t  hr, vr;                // cuantas muestras de la rejilla de la imagen reducida caen en una suya
  uint32_t off, pw;               // principio de su plano en rowbuf y ancho del plano
};

struct JpgCtx {
  I16Reader* in;
  JpgWork*   w;
  // entrada
  uint32_t inbase;                // posicion en el fichero de inbuf[0]
  int      inlen, inpos;
  uint32_t rem;                   // bytes que quedan del segmento que se esta leyendo
  // cabecera
  uint32_t W, H;
  int      ncomp;
  JpgComp  comp[3];
  uint8_t  scanord[3];
  uint8_t  hmax, vmax;
  uint16_t ri;                    // intervalo de reinicio (en MCU)
  uint32_t scan_off;              // principio de los datos
  uint8_t  orient;                // etiqueta EXIF de orientacion (1..8)
  bool     sof, jfif, adobe, rgb;
  uint8_t  adobe_tr;
  bool     have_q[4], have_dc[2], have_ac[2];
  // geometria
  uint8_t  N;                     // escala: N/8 (1, 2, 4, 8)
  uint32_t mcuCols, mcuh;         // MCU por fila, filas de muestras por fila de MCU (vmax * N)
  uint32_t Ws, Hs;                // tamano reducido
  bool     swap;                  // la orientacion intercambia ejes
  I16Geo   g;                     // en la imagen orientada y reducida
  uint32_t Sx0, Sy0;              // origen del recorte en la imagen reducida sin orientar
  I16Scale sc;
  // decodificador de bits
  uint32_t bb;
  int      bc, pad;
  int      marker;                // marcador encontrado en los datos (-1: ninguno todavia)
  bool     overrun, bad;
  int32_t  pred[3];
  uint32_t mcu_count, first_mrow;
  int32_t  cur_mrow;
  I16HAcc  acc;
  // salida
  uint16_t* hist;
  uint8_t*  out;
  I16Map    map;
  const char* errmsg;
};

static bool fail(I16Error& err, const char* msg) {
  strncpy(err.msg, msg, sizeof(err.msg) - 1);
  err.msg[sizeof(err.msg) - 1] = 0;
  return false;
}

// ---------------------------------------------------------------------------
// Entrada con buffer
// ---------------------------------------------------------------------------
static bool in_seek(JpgCtx& c, uint32_t pos) {
  if (!c.in->seek(pos)) return false;
  c.inbase = pos;
  c.inlen = c.inpos = 0;
  return true;
}

static int in_byte(JpgCtx& c) {
  if (c.inpos >= c.inlen) {
    c.inbase += (uint32_t)c.inlen;
    c.inpos = 0;
    c.inlen = c.in->read(c.w->inbuf, (int)sizeof(c.w->inbuf));
    if (c.inlen <= 0) { c.inlen = 0; return -1; }
  }
  return c.w->inbuf[c.inpos++];
}

static bool in_skip(JpgCtx& c, uint32_t n) {
  uint32_t avail = (uint32_t)(c.inlen - c.inpos);
  if (n <= avail) { c.inpos += (int)n; return true; }
  uint32_t pos = c.inbase + (uint32_t)c.inpos + n;
  if (pos > c.in->size()) return false;
  return in_seek(c, pos);
}

// Bytes de un segmento (con la cuenta de lo que queda); -1 si se acaba
static int sb(JpgCtx& c) {
  if (!c.rem) return -1;
  int b = in_byte(c);
  if (b < 0) { c.rem = 0; return -1; }
  c.rem--;
  return b;
}
static int sb16(JpgCtx& c) {
  int a = sb(c), b = sb(c);
  return (a < 0 || b < 0) ? -1 : (a << 8) | b;
}
static bool seg_skip(JpgCtx& c, uint32_t n) {
  if (n > c.rem) n = c.rem;
  c.rem -= n;
  return in_skip(c, n);
}

// ---------------------------------------------------------------------------
// Tablas Huffman
// ---------------------------------------------------------------------------
static bool build_huff(const uint8_t* counts, const uint8_t* vals, int32_t* maxcode, uint16_t* valoff, uint16_t* lut) {
  int32_t code = 0;
  int k = 0;
  if (lut) memset(lut, 0, 128 * sizeof(uint16_t));
  for (int l = 1; l <= 16; l++) {
    valoff[l] = (uint16_t)(k - code);
    if (counts[l]) {
      for (int i = 0; i < counts[l]; i++, k++, code++) {
        if (code >= (1 << l)) return false;
        if (lut && l <= JPG_LUT_BITS) {
          int base = code << (JPG_LUT_BITS - l), n = 1 << (JPG_LUT_BITS - l);
          for (int j = 0; j < n; j++) lut[base + j] = (uint16_t)((l << 8) | vals[k]);
        }
      }
      maxcode[l] = code - 1;
    } else {
      maxcode[l] = -1;
    }
    code <<= 1;
  }
  maxcode[17] = 0x7FFFFFFF;
  return true;
}

// ---------------------------------------------------------------------------
// Cabecera
// ---------------------------------------------------------------------------
static bool parse_dht(JpgCtx& c, I16Error& err) {
  JpgWork& w = *c.w;
  while (c.rem) {
    int tt = sb(c);
    int tc = tt >> 4, th = tt & 15;
    if (tt < 0 || tc > 1 || th > 1) return fail(err, "UNSUPPORTED JPG HUFFMAN TABLE");
    uint8_t counts[17];
    int total = 0;
    for (int l = 1; l <= 16; l++) {
      int v = sb(c);
      if (v < 0) return fail(err, "BAD JPG HEADER");
      counts[l] = (uint8_t)v;
      total += v;
    }
    if (total > (tc ? 256 : 16)) return fail(err, "BAD JPG HUFFMAN TABLE");
    uint8_t* vals = tc ? w.ac[th].vals : w.dc[th].vals;
    for (int i = 0; i < total; i++) {
      int v = sb(c);
      if (v < 0) return fail(err, "BAD JPG HEADER");
      vals[i] = (uint8_t)v;
    }
    bool ok = tc ? build_huff(counts, vals, w.ac[th].maxcode, w.ac[th].valoff, w.ac[th].lut)
                 : build_huff(counts, vals, w.dc[th].maxcode, w.dc[th].valoff, nullptr);
    if (!ok) return fail(err, "BAD JPG HUFFMAN TABLE");
    if (tc) c.have_ac[th] = true; else c.have_dc[th] = true;
  }
  return true;
}

static bool parse_dqt(JpgCtx& c, I16Error& err) {
  while (c.rem) {
    int pt = sb(c);
    if (pt < 0 || (pt & 15) > 3) return fail(err, "BAD JPG HEADER");
    if (pt >> 4) return fail(err, "UNSUPPORTED JPG (16 BIT TABLE)");
    for (int i = 0; i < 64; i++) {
      int v = sb(c);
      if (v < 0) return fail(err, "BAD JPG HEADER");
      c.w->q[pt & 15][i] = (uint8_t)v;
    }
    c.have_q[pt & 15] = true;
  }
  return true;
}

static bool parse_sof(JpgCtx& c, I16Error& err) {
  if (c.sof) return fail(err, "BAD JPG HEADER");
  int prec = sb(c);
  int h = sb16(c), wd = sb16(c);
  int nf = sb(c);
  if (prec != 8) return fail(err, "UNSUPPORTED JPG (NOT 8 BIT)");
  if (h <= 0 || wd <= 0) return fail(err, "BAD JPG SIZE");
  if (nf == 4) return fail(err, "CMYK JPG NOT SUPPORTED");
  if (nf != 1 && nf != 3) return fail(err, "UNSUPPORTED JPG COMPONENTS");
  c.W = (uint32_t)wd;
  c.H = (uint32_t)h;
  c.ncomp = nf;
  c.hmax = c.vmax = 1;
  for (int i = 0; i < nf; i++) {
    int id = sb(c), hv = sb(c), tq = sb(c);
    if (id < 0 || hv < 0 || tq < 0) return fail(err, "BAD JPG HEADER");
    JpgComp& cp = c.comp[i];
    cp.id = (uint8_t)id;
    cp.h = (uint8_t)(hv >> 4);
    cp.v = (uint8_t)(hv & 15);
    cp.tq = (uint8_t)tq;
    if (nf == 1) cp.h = cp.v = 1;                 // un solo componente: la MCU es un bloque
    if (cp.h < 1 || cp.h > 4 || cp.v < 1 || cp.v > 4 || cp.tq > 3) return fail(err, "BAD JPG HEADER");
    if (cp.h > c.hmax) c.hmax = cp.h;
    if (cp.v > c.vmax) c.vmax = cp.v;
  }
  if (c.hmax * c.vmax > 4) return fail(err, "UNSUPPORTED JPG SAMPLING");
  for (int i = 0; i < nf; i++) {
    JpgComp& cp = c.comp[i];
    if (c.hmax % cp.h || c.vmax % cp.v) return fail(err, "UNSUPPORTED JPG SAMPLING");
  }
  c.sof = true;
  return true;
}

static uint32_t rd16(const uint8_t* p, bool le) { return le ? (uint32_t)p[0] | ((uint32_t)p[1] << 8) : ((uint32_t)p[0] << 8) | p[1]; }
static uint32_t rd32(const uint8_t* p, bool le) { return le ? rd16(p, true) | (rd16(p + 2, true) << 16) : (rd16(p, false) << 16) | rd16(p + 2, false); }

// APP1 con EXIF: busca la orientacion en el IFD0, sin salir nunca hacia atras
static void parse_exif(JpgCtx& c) {
  uint8_t h[14];
  if (c.rem < 14) return;
  for (int i = 0; i < 14; i++) h[i] = (uint8_t)sb(c);
  if (memcmp(h, "Exif\0\0", 6) != 0) return;
  bool le;
  if (h[6] == 'I' && h[7] == 'I') le = true;
  else if (h[6] == 'M' && h[7] == 'M') le = false;
  else return;
  uint32_t ifd = rd32(h + 10, le);
  if (ifd < 8 || ifd - 8 > c.rem) return;
  if (!seg_skip(c, ifd - 8) || c.rem < 2) return;
  uint8_t e[12];
  e[0] = (uint8_t)sb(c);
  e[1] = (uint8_t)sb(c);
  uint32_t cnt = rd16(e, le);
  for (uint32_t i = 0; i < cnt && c.rem >= 12; i++) {
    for (int j = 0; j < 12; j++) e[j] = (uint8_t)sb(c);
    if (rd16(e, le) == 0x0112 && rd16(e + 2, le) == 3) {
      uint32_t o = rd16(e + 8, le);
      if (o >= 1 && o <= 8) c.orient = (uint8_t)o;
      return;
    }
  }
}

static bool parse_sos(JpgCtx& c, I16Error& err) {
  if (!c.sof) return fail(err, "BAD JPG HEADER");
  int ns = sb(c);
  if (ns != c.ncomp) return fail(err, ns < 1 ? "BAD JPG HEADER" : "MULTI-SCAN JPG NOT SUPPORTED");
  bool used[3] = {false, false, false};
  for (int i = 0; i < ns; i++) {
    int cs = sb(c), t = sb(c);
    if (cs < 0 || t < 0) return fail(err, "BAD JPG HEADER");
    int ci = -1;
    for (int j = 0; j < c.ncomp; j++) if (c.comp[j].id == cs && !used[j]) { ci = j; break; }
    if (ci < 0) return fail(err, "BAD JPG HEADER");
    used[ci] = true;
    c.scanord[i] = (uint8_t)ci;
    c.comp[ci].td = (uint8_t)(t >> 4);
    c.comp[ci].ta = (uint8_t)(t & 15);
  }
  int ss = sb(c), se = sb(c), a = sb(c);
  if (ss != 0 || se != 63 || a != 0) return fail(err, "UNSUPPORTED JPG (NOT BASELINE)");
  for (int i = 0; i < c.ncomp; i++) {
    const JpgComp& cp = c.comp[i];
    if (cp.td > 1 || cp.ta > 1 || !c.have_dc[cp.td] || !c.have_ac[cp.ta]) return fail(err, "JPG HUFFMAN TABLE MISSING");
    if (!c.have_q[cp.tq]) return fail(err, "JPG QUANT TABLE MISSING");
  }
  return true;
}

static bool parse_headers(JpgCtx& c, I16Error& err) {
  c.orient = 1;
  if (!in_seek(c, 0)) return fail(err, "CANNOT READ FILE");
  if (in_byte(c) != 0xFF || in_byte(c) != 0xD8) return fail(err, "NOT A JPG FILE");
  for (;;) {
    int b = in_byte(c);
    if (b < 0) return fail(err, "JPG TRUNCATED");
    if (b != 0xFF) continue;                      // basura entre segmentos
    do { b = in_byte(c); } while (b == 0xFF);
    if (b < 0) return fail(err, "JPG TRUNCATED");
    if (b == 0 || b == 0x01 || b == 0xD8 || (b >= 0xD0 && b <= 0xD7)) continue;   // sin longitud
    if (b == 0xD9) return fail(err, "JPG HAS NO IMAGE DATA");
    int len = in_byte(c) << 8;
    len |= in_byte(c);
    if (len < 2) return fail(err, "BAD JPG HEADER");
    c.rem = (uint32_t)(len - 2);
    switch (b) {
      case 0xC0: case 0xC1:
        if (!parse_sof(c, err)) return false;
        break;
      case 0xC2:
        return fail(err, "PROGRESSIVE JPG NOT SUPPORTED");
      case 0xC9: case 0xCA: case 0xCB: case 0xCC: case 0xCD: case 0xCE: case 0xCF:
        return fail(err, "ARITHMETIC JPG NOT SUPPORTED");
      case 0xC3: case 0xC5: case 0xC6: case 0xC7:
        return fail(err, "UNSUPPORTED JPG TYPE");
      case 0xC4:
        if (!parse_dht(c, err)) return false;
        break;
      case 0xDB:
        if (!parse_dqt(c, err)) return false;
        break;
      case 0xDD:
        c.ri = (uint16_t)(sb16(c) & 0xFFFF);
        break;
      case 0xE0: {
        uint8_t h[5];
        if (c.rem >= 5) {
          for (int i = 0; i < 5; i++) h[i] = (uint8_t)sb(c);
          if (memcmp(h, "JFIF\0", 5) == 0) c.jfif = true;
        }
        break;
      }
      case 0xE1:
        parse_exif(c);
        break;
      case 0xEE: {
        uint8_t h[12];
        if (c.rem >= 12) {
          for (int i = 0; i < 12; i++) h[i] = (uint8_t)sb(c);
          if (memcmp(h, "Adobe", 5) == 0) { c.adobe = true; c.adobe_tr = h[11]; }
        }
        break;
      }
      case 0xDA:
        if (!parse_sos(c, err)) return false;
        if (!seg_skip(c, c.rem)) return fail(err, "JPG TRUNCATED");
        c.scan_off = c.inbase + (uint32_t)c.inpos;
        return true;
      default:
        break;
    }
    if (!seg_skip(c, c.rem)) return fail(err, "JPG TRUNCATED");
  }
}

// ---------------------------------------------------------------------------
// Decodificador de bits
// ---------------------------------------------------------------------------
static void bits_reset(JpgCtx& c) {
  c.bb = 0;
  c.bc = c.pad = 0;
  c.marker = -1;
  c.overrun = c.bad = false;
}

// Deja al menos 25 bits en bb. Al llegar a un marcador (o al final del fichero) entrega ceros y los cuenta como relleno
static void fill_bits(JpgCtx& c) {
  while (c.bc <= 24) {
    uint32_t b = 0;
    if (c.marker >= 0) {
      c.pad += 8;
    } else {
      int x = in_byte(c);
      if (x == 0xFF) {
        int y = in_byte(c);
        while (y == 0xFF) y = in_byte(c);
        if (y == 0) b = 0xFF;
        else { c.marker = y < 0 ? 0xD9 : y; c.pad += 8; }
      } else if (x < 0) {
        c.marker = 0xD9;
        c.pad += 8;
      } else {
        b = (uint32_t)x;
      }
    }
    c.bb = (c.bb << 8) | b;
    c.bc += 8;
  }
}

static inline void consume(JpgCtx& c, int n) {
  c.bc -= n;
  if (c.bc < c.pad) { c.overrun = true; c.pad = c.bc; }
}

static inline uint32_t peek(const JpgCtx& c, int n) { return (c.bb >> (c.bc - n)) & ((1u << n) - 1); }

static int dc_sym(JpgCtx& c, const JpgHuffDC& t) {
  fill_bits(c);
  int l = 1;
  int32_t code = (int32_t)peek(c, 1);
  while (code > t.maxcode[l]) {
    if (++l > 16) { c.bad = true; return 0; }
    code = (int32_t)peek(c, l);
  }
  consume(c, l);
  return t.vals[(uint16_t)(code + t.valoff[l]) & 15];
}

static int ac_sym(JpgCtx& c, const JpgHuffAC& t) {
  fill_bits(c);
  uint16_t e = t.lut[peek(c, JPG_LUT_BITS)];
  if (e) {
    consume(c, e >> 8);
    return e & 255;
  }
  int l = JPG_LUT_BITS + 1;
  int32_t code = (int32_t)peek(c, l);
  while (code > t.maxcode[l]) {
    if (++l > 16) { c.bad = true; return 0; }
    code = (int32_t)peek(c, l);
  }
  consume(c, l);
  return t.vals[(uint16_t)(code + t.valoff[l]) & 255];
}

// s bits como numero con signo (F.2.2.1)
static inline int receive_extend(JpgCtx& c, int s) {
  if (!s) return 0;
  if (c.bc < 16) fill_bits(c);
  int v = (int)peek(c, s);
  consume(c, s);
  return v < (1 << (s - 1)) ? v - (1 << s) + 1 : v;
}

// Reinicio: descarta los bits que quedan, busca el marcador RSTn y pone a cero los predictores
static bool restart(JpgCtx& c) {
  c.bb = 0;
  c.bc = c.pad = 0;
  int m = c.marker;
  c.marker = -1;
  while (m < 0) {                                  // basura hasta el marcador
    int x = in_byte(c);
    if (x < 0) { c.errmsg = "JPG TRUNCATED"; return false; }
    if (x != 0xFF) continue;
    int y = in_byte(c);
    while (y == 0xFF) y = in_byte(c);
    if (y < 0) { c.errmsg = "JPG TRUNCATED"; return false; }
    if (y != 0) m = y;
  }
  if (m < 0xD0 || m > 0xD7) { c.errmsg = "JPG TRUNCATED"; return false; }
  c.pred[0] = c.pred[1] = c.pred[2] = 0;
  return true;
}

// ---------------------------------------------------------------------------
// Bloques
// ---------------------------------------------------------------------------
static inline int16_t sat16(int32_t v) { return (int16_t)(v < -32768 ? -32768 : (v > 32767 ? 32767 : v)); }
static inline uint8_t clamp8(int32_t v) { return (uint8_t)(v < 0 ? 0 : (v > 255 ? 255 : v)); }

// Recorre los AC de un bloque sin guardarlos
static void skip_ac(JpgCtx& c, const JpgHuffAC& t) {
  for (int k = 1; k < 64 && !c.bad; ) {
    int sym = ac_sym(c, t);
    int r = sym >> 4, s = sym & 15;
    if (!s) {
      if (r != 15) break;
      k += 16;
      continue;
    }
    k += r + 1;
    if (c.bc < 16) fill_bits(c);
    consume(c, s);
  }
}

// DC de un bloque (con el predictor del componente)
static int32_t block_dc(JpgCtx& c, int ci, const JpgHuffDC& t) {
  int s = dc_sym(c, t);
  if (s > 15) { c.bad = true; s = 0; }
  int32_t p = c.pred[ci] + receive_extend(c, s);
  c.pred[ci] = p < -32768 ? -32768 : (p > 32767 ? 32767 : p);
  return c.pred[ci];
}

// Un bloque de N x N muestras en dst (paso pw). store = false: solo avanza en el flujo de bits
static void decode_block(JpgCtx& c, int ci, uint8_t* dst, uint32_t pw, bool store) {
  JpgWork& w = *c.w;
  const JpgComp& cp = c.comp[ci];
  const JpgHuffAC& at = w.ac[cp.ta];
  const uint8_t* q = w.q[cp.tq];
  int N = cp.n;
  int32_t dc = block_dc(c, ci, w.dc[cp.td]);
  if (!store) { skip_ac(c, at); return; }

  if (N == 1) {
    skip_ac(c, at);
    int32_t F = sat16(dc * q[0]);
    *dst = clamp8(((F + 4) >> 3) + 128);
    return;
  }

  int16_t* coef = w.coef;
  memset(coef, 0, sizeof(int16_t) * (size_t)(N * N));
  coef[0] = sat16(dc * q[0]);
  for (int k = 1; k < 64 && !c.bad; ) {
    int sym = ac_sym(c, at);
    int r = sym >> 4, s = sym & 15;
    if (!s) {
      if (r != 15) break;
      k += 16;
      continue;
    }
    k += r;
    int v = receive_extend(c, s);
    if (k < 64) {
      int nat = ZZ[k];
      int row = nat >> 3, col = nat & 7;
      if (row < N && col < N) coef[row * N + col] = sat16(v * q[k]);
    }
    k++;
  }

  const int16_t* T = N == 2 ? T2 : (N == 4 ? T4 : T8);
  int shiftN = N == 2 ? 2 : (N == 4 ? 1 : 0);
  int32_t* tmp = w.tmp;
  // por filas de frecuencia: horizontal
  for (int v = 0; v < N; v++)
    for (int x = 0; x < N; x++) {
      int32_t s = 0;
      for (int u = 0; u < N; u++) s += (int32_t)T[x * N + u] * coef[v * N + u];
      tmp[v * N + x] = (s + 32) >> 6;
    }
  // vertical
  int64_t rnd = (int64_t)1 << (17 + shiftN);
  for (int x = 0; x < N; x++)
    for (int y = 0; y < N; y++) {
      int64_t s = 0;
      for (int v = 0; v < N; v++) s += (int64_t)T[y * N + v] * tmp[v * N + x];
      dst[y * pw + x] = clamp8((int32_t)((s + rnd) >> (18 + shiftN)) + 128);
    }
}

// Una fila de MCU: skip = true si queda fuera de lo que se usa (solo se avanza en el flujo)
static bool decode_mcu_row(JpgCtx& c, bool skip) {
  JpgWork& w = *c.w;
  for (uint32_t mx = 0; mx < c.mcuCols; mx++) {
    if (c.ri && c.mcu_count && c.mcu_count % c.ri == 0) {
      if (!restart(c)) return false;
    }
    c.mcu_count++;
    for (int si = 0; si < c.ncomp; si++) {
      int ci = c.scanord[si];
      const JpgComp& cp = c.comp[ci];
      uint32_t N = cp.n;
      for (uint32_t by = 0; by < cp.v; by++)
        for (uint32_t bx = 0; bx < cp.h; bx++) {
          uint8_t* dst = skip ? nullptr : w.rowbuf + cp.off + (by * N) * cp.pw + (mx * cp.h + bx) * N;
          decode_block(c, ci, dst, cp.pw, !skip);
        }
    }
    if (c.bad) { c.errmsg = "BAD JPG DATA"; return false; }
  }
  if (c.overrun) { c.errmsg = "JPG TRUNCATED"; return false; }
  return true;
}

// ---------------------------------------------------------------------------
// Lineas de la imagen reducida -> escalado
// ---------------------------------------------------------------------------
static bool jpg_line(void* vc, uint32_t k, uint32_t* H) {
  JpgCtx& c = *(JpgCtx*)vc;
  JpgWork& w = *c.w;
  uint32_t ys = c.Sy0 + k;
  int32_t mrow = (int32_t)(ys / c.mcuh);
  while (c.cur_mrow < mrow) {
    c.cur_mrow++;
    if (!decode_mcu_row(c, (uint32_t)c.cur_mrow < c.first_mrow)) return false;
  }
  uint32_t r = ys - (uint32_t)mrow * c.mcuh;
  c.acc.init(H, c.sc);
  const uint8_t* rp[3] = {nullptr, nullptr, nullptr};
  for (int i = 0; i < c.ncomp; i++) rp[i] = w.rowbuf + c.comp[i].off + (r / c.comp[i].vr) * c.comp[i].pw;

  uint32_t x1 = c.Sx0 + c.sc.Sw;
  if (c.ncomp == 1) {
    for (uint32_t x = c.Sx0; x < x1; x++) {
      uint32_t y = rp[0][x];
      c.acc.add(y, y, y);
    }
  } else if (c.rgb) {
    for (uint32_t x = c.Sx0; x < x1; x++)
      c.acc.add(rp[0][x / c.comp[0].hr], rp[1][x / c.comp[1].hr], rp[2][x / c.comp[2].hr]);
  } else {
    for (uint32_t x = c.Sx0; x < x1; x++) {
      int32_t y = rp[0][x / c.comp[0].hr];
      int32_t cb = (int32_t)rp[1][x / c.comp[1].hr] - 128;
      int32_t cr = (int32_t)rp[2][x / c.comp[2].hr] - 128;
      c.acc.add(clamp8(y + ((91881 * cr + 32768) >> 16)),
                clamp8(y + ((-22554 * cb - 46802 * cr + 32768) >> 16)),
                clamp8(y + ((116130 * cb + 32768) >> 16)));
    }
  }
  return true;
}

// ---------------------------------------------------------------------------
// Pasadas
// ---------------------------------------------------------------------------
static bool run_pass(JpgCtx& c, I16SinkFn sink) {
  if (!in_seek(c, c.scan_off)) { c.errmsg = "CANNOT READ FILE"; return false; }
  bits_reset(c);
  c.pred[0] = c.pred[1] = c.pred[2] = 0;
  c.mcu_count = 0;
  c.cur_mrow = -1;
  c.first_mrow = c.Sy0 / c.mcuh;
  c.errmsg = nullptr;
  return i16_scale_pass(c.sc, jpg_line, sink, &c, c.w->c);
}

static void sink_hist(void* vc, uint32_t, const uint8_t* rgb) {
  JpgCtx& c = *(JpgCtx*)vc;
  i16_hist_add(c.hist, rgb, c.sc.D1);
}

// La linea de destino dline es una fila de la imagen orientada, o una columna si la orientacion intercambia los ejes
static void sink_map(void* vc, uint32_t dline, const uint8_t* rgb) {
  JpgCtx& c = *(JpgCtx*)vc;
  uint8_t* o = c.out + I16_PIX_OFF;
  if (!c.swap) i16_map_row(c.map, rgb, c.sc.D1, o + (c.g.dy0 + dline) * I16_W + c.g.dx0, 1);
  else         i16_map_row(c.map, rgb, c.sc.D1, o + c.g.dy0 * I16_W + c.g.dx0 + dline, I16_W);
}

// Lado de los bloques reducidos del componente a la escala N, y cuantas muestras de la rejilla de la imagen reducida (hmax * N por MCU) caen en una suya.
// Como hace libjpeg: un componente submuestreado a la mitad en los dos ejes (la croma de 4:2:0) se decodifica al doble de escala, para no repetirla.
static void comp_scale(const JpgCtx& c, const JpgComp& cp, uint32_t N, uint8_t& n, uint8_t& hr, uint8_t& vr) {
  uint32_t rh = c.hmax / cp.h, rv = c.vmax / cp.v, k = N;
  while (k < 8 && !(rh & 1) && !(rv & 1)) { k <<= 1; rh >>= 1; rv >>= 1; }
  n = (uint8_t)k;
  hr = (uint8_t)rh;
  vr = (uint8_t)rv;
}

// Bytes de una fila de MCU a la escala N
static uint32_t rowbytes(const JpgCtx& c, uint32_t N) {
  uint32_t total = 0;
  for (int i = 0; i < c.ncomp; i++) {
    uint8_t n, hr, vr;
    comp_scale(c, c.comp[i], N, n, hr, vr);
    total += (uint32_t)c.comp[i].h * c.comp[i].v * n * n;
  }
  return c.mcuCols * total;
}

// Orientacion EXIF: (cambia ejes, el eje x de la imagen sin orientar va al reves, el eje y va al reves)
static void orient_flags(uint8_t o, bool& swap, bool& fx_rev, bool& fy_rev) {
  swap = o >= 5;
  switch (o) {
    case 2:  fx_rev = true;  fy_rev = false; break;
    case 3:  fx_rev = true;  fy_rev = true;  break;
    case 4:  fx_rev = false; fy_rev = true;  break;
    case 5:  fx_rev = false; fy_rev = false; break;
    case 6:  fx_rev = false; fy_rev = true;  break;
    case 7:  fx_rev = true;  fy_rev = true;  break;
    case 8:  fx_rev = true;  fy_rev = false; break;
    default: fx_rev = false; fy_rev = false; break;
  }
}

bool jpg_to_i16(I16Reader& in, uint8_t fit, uint8_t* out, JpgWork& w, I16Error& err) {
  JpgCtx c;
  memset(&c, 0, sizeof(c));
  c.in = &in;
  c.w = &w;
  c.out = out;
  if (!parse_headers(c, err)) return false;
  if ((uint64_t)c.W * c.H > JPG_MAX_PIXELS) return fail(err, "JPG TOO LARGE");

  if (c.ncomp == 3) {
    if (c.adobe) c.rgb = c.adobe_tr == 0;
    else if (c.jfif) c.rgb = false;
    else c.rgb = c.comp[0].id == 'R' && c.comp[1].id == 'G' && c.comp[2].id == 'B';
  }
  c.mcuCols = (c.W + 8u * c.hmax - 1) / (8u * c.hmax);

  // escala: la mayor reduccion que deja al menos tantas muestras como puntos de destino
  bool fx_rev, fy_rev;
  orient_flags(c.orient, c.swap, fx_rev, fy_rev);
  int best = 0;
  for (uint32_t N = 1; N <= 8; N <<= 1) {
    if (rowbytes(c, N) > JPG_ROWBUF) break;
    best = (int)N;
    uint32_t ws = (c.W * N + 7) / 8, hs = (c.H * N + 7) / 8;
    I16Geo g;
    if (c.swap) i16_geometry(hs, ws, fit, g); else i16_geometry(ws, hs, fit, g);
    if (g.sw >= g.dw && g.sh >= g.dh) break;
  }
  if (!best) return fail(err, "JPG TOO WIDE");
  c.N = (uint8_t)best;
  c.Ws = (c.W * c.N + 7) / 8;
  c.Hs = (c.H * c.N + 7) / 8;
  c.mcuh = (uint32_t)c.vmax * c.N;

  // planos de la fila de MCU
  uint32_t off = 0;
  for (int i = 0; i < c.ncomp; i++) {
    JpgComp& cp = c.comp[i];
    comp_scale(c, cp, c.N, cp.n, cp.hr, cp.vr);
    cp.pw = c.mcuCols * cp.h * cp.n;
    cp.off = off;
    off += cp.pw * cp.v * cp.n;
  }

  // colocacion en la imagen orientada y su recorte en la imagen reducida sin orientar
  if (c.swap) i16_geometry(c.Hs, c.Ws, fit, c.g); else i16_geometry(c.Ws, c.Hs, fit, c.g);
  const I16Geo& g = c.g;
  if ((uint64_t)g.sw * g.sh > JPG_MAX_SAMPLES) return fail(err, "JPG TOO LARGE");
  if (!c.swap) {
    c.sc.Sw = g.sw; c.sc.Sh = g.sh; c.sc.D1 = g.dw; c.sc.D2 = g.dh;
    c.Sx0 = fx_rev ? c.Ws - g.sx0 - g.sw : g.sx0;
    c.Sy0 = fy_rev ? c.Hs - g.sy0 - g.sh : g.sy0;
  } else {
    c.sc.Sw = g.sh; c.sc.Sh = g.sw; c.sc.D1 = g.dh; c.sc.D2 = g.dw;
    c.Sx0 = fx_rev ? c.Ws - g.sy0 - g.sh : g.sy0;
    c.Sy0 = fy_rev ? c.Hs - g.sx0 - g.sw : g.sx0;
  }
  c.sc.rev_x = fx_rev;
  c.sc.rev_y = fy_rev;

  // pasada 1: el histograma vive donde luego iran los pixeles
  c.hist = (uint16_t*)(out + I16_PIX_OFF);
  memset(out + I16_PIX_OFF, 0, 4096 * sizeof(uint16_t));
  if (!run_pass(c, sink_hist)) return fail(err, c.errmsg ? c.errmsg : "INTERNAL ERROR");
  c.hist[0] += (uint16_t)(I16_PIX_COUNT - g.dw * g.dh);        // las bandas negras del "fit"

  int npal = i16_build_palette(c.hist, w.c.u.boxes, w.c.pal);
  i16_write_header(out, w.c.pal);

  // pasada 2: el fondo y la imagen
  i16_map_init(c.map, w.c.pal, npal);
  int black = i16_nearest(c.map, 0, 0, 0);
  memset(out + I16_PIX_OFF, black, I16_PIX_COUNT);
  i16_map_reset(c.map);
  if (!run_pass(c, sink_map)) return fail(err, c.errmsg ? c.errmsg : "INTERNAL ERROR");
  return true;
}
