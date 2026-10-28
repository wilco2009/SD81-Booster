#include "BMP.h"
#include <string.h>

// Ver BMP.h. Referencia en Python con el mismo algoritmo entero: EXAMPLES/PHOTO/bmp2i16.py

#define BMP_MAX_DIM      16384
#define BMP_MAX_PIXELS   16000000u     // del area de origen usada: con 255 * pixeles el acumulador cabe en 32 bits
#define BMP_PIX_OFF      516           // principio de los pixeles en el .I16
#define BMP_PIX_COUNT    (BMP_OUT_W * BMP_OUT_H)

// Constantes de la red de la SCART (pal160.py: 255/15 de la tension relativa de un paso del canal y de BRIGHT, por 65536)
static const int64_t BMP_STEP  = 707612;
static const int64_t BMP_LIFTU = 406500;

struct BmpInfo {
  uint32_t dataOff;
  uint32_t W, H;          // dimensiones (H en valor absoluto)
  bool     bottomUp;
  uint8_t  bpp;
  uint32_t stride;
  uint32_t fileSize;
};

struct BmpGeo {
  uint32_t sx0, sy0, sw, sh;      // rectangulo de origen que se usa
  uint32_t dx0, dy0, dw, dh;      // rectangulo de destino que se rellena
};

struct BmpCtx;
typedef void (*BmpSink)(BmpCtx&, uint32_t dy);

struct BmpCtx {
  BmpReader* in;
  BmpWork*   w;
  BmpInfo    bi;
  BmpGeo     g;
  uint16_t*  hist;        // histograma de 4+4+4 bits (pasada 1)
  uint8_t*   out;         // el .I16
  int        npal;        // colores de la paleta elegida
  int        lastr, lastg, lastb, lastidx;   // atajo del color mas cercano
  const char* errmsg;
};

static uint32_t le16(const uint8_t* p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8); }
static uint32_t le32(const uint8_t* p) { return le16(p) | (le16(p + 2) << 16); }

static bool fail(BmpError& err, const char* msg) {
  strncpy(err.msg, msg, sizeof(err.msg) - 1);
  err.msg[sizeof(err.msg) - 1] = 0;
  return false;
}

// ---------------------------------------------------------------------------
// Cabecera
// ---------------------------------------------------------------------------
static bool parse_header(BmpReader& in, BmpWork& w, BmpInfo& bi, BmpError& err) {
  uint8_t* h = w.chunk;
  bi.fileSize = in.size();
  if (!in.seek(0)) return fail(err, "CANNOT READ FILE");
  int n = in.read(h, 70);
  if (n < 26 || h[0] != 'B' || h[1] != 'M') return fail(err, "NOT A BMP FILE");

  bi.dataOff = le32(h + 10);
  uint32_t dib = le32(h + 14);
  int32_t  W, H;
  uint32_t planes, bpp, comp = 0, clrUsed = 0;
  int entsz;
  if (dib == 12) {
    W = (int32_t)le16(h + 18);
    H = (int32_t)le16(h + 20);
    planes = le16(h + 22);
    bpp = le16(h + 24);
    entsz = 3;
  } else if (dib >= 40 && dib <= 124 && n >= 54) {
    W = (int32_t)le32(h + 18);
    H = (int32_t)le32(h + 22);
    planes = le16(h + 26);
    bpp = le16(h + 28);
    comp = le32(h + 30);
    clrUsed = le32(h + 46);
    entsz = 4;
  } else {
    return fail(err, "BAD BMP HEADER");
  }
  if (planes != 1 || W <= 0 || H == 0) return fail(err, "BAD BMP HEADER");
  bi.bottomUp = H > 0;
  bi.W = (uint32_t)W;
  bi.H = (uint32_t)(H > 0 ? H : -H);
  if (bi.W > BMP_MAX_DIM || bi.H > BMP_MAX_DIM) return fail(err, "BMP TOO LARGE");
  if (bpp != 1 && bpp != 4 && bpp != 8 && bpp != 24 && bpp != 32) return fail(err, "UNSUPPORTED BMP DEPTH");
  bi.bpp = (uint8_t)bpp;
  if (comp != 0) {
    // solo BI_BITFIELDS de 32 bits con las mascaras normales (BGRA)
    if (!(comp == 3 && bpp == 32 && n >= 66 &&
          le32(h + 54) == 0x00FF0000u && le32(h + 58) == 0x0000FF00u && le32(h + 62) == 0x000000FFu))
      return fail(err, "UNSUPPORTED BMP (COMPRESSED)");
  }
  bi.stride = (((uint32_t)bi.W * bpp + 31) / 32) * 4;
  // la ultima fila puede venir sin el relleno
  uint32_t lastrow = (bi.W * bpp + 7) / 8;
  if ((uint64_t)bi.dataOff + (uint64_t)bi.stride * (bi.H - 1) + lastrow > bi.fileSize) return fail(err, "BMP TRUNCATED");

  memset(w.spal, 0, sizeof(w.spal));
  if (bpp <= 8) {
    uint32_t ncol = clrUsed ? clrUsed : (1u << bpp);
    if (ncol > (1u << bpp)) ncol = 1u << bpp;
    if (!in.seek(14 + dib)) return fail(err, "BAD BMP HEADER");
    for (uint32_t i = 0; i < ncol; i++) {
      uint8_t e[4];
      if (in.read(e, entsz) != entsz) return fail(err, "BMP TRUNCATED");
      w.spal[i][0] = e[2];      // R (el BMP guarda B, G, R)
      w.spal[i][1] = e[1];
      w.spal[i][2] = e[0];
    }
  }
  return true;
}

// ---------------------------------------------------------------------------
// Geometria: que parte del BMP va a que parte de los 160x100 (la pantalla es 4:3)
// ---------------------------------------------------------------------------
static void geometry(const BmpInfo& bi, uint8_t fit, BmpGeo& g) {
  uint32_t W = bi.W, H = bi.H;
  g.sx0 = g.sy0 = 0;
  g.sw = W;
  g.sh = H;
  g.dx0 = g.dy0 = 0;
  g.dw = BMP_OUT_W;
  g.dh = BMP_OUT_H;
  bool wide = (uint64_t)W * 3 >= (uint64_t)H * 4;
  if (fit == BMP_FIT_FIT) {
    if (wide) {
      g.dh = (800 * H + 3 * W) / (6 * W);
      if (g.dh < 1) g.dh = 1;
      if (g.dh > BMP_OUT_H) g.dh = BMP_OUT_H;
    } else {
      g.dw = (240 * W + H) / (2 * H);
      if (g.dw < 1) g.dw = 1;
      if (g.dw > BMP_OUT_W) g.dw = BMP_OUT_W;
    }
    g.dx0 = (BMP_OUT_W - g.dw) / 2;
    g.dy0 = (BMP_OUT_H - g.dh) / 2;
  } else if (fit == BMP_FIT_CROP) {
    if (wide) {
      g.sw = (8 * H + 3) / 6;
      if (g.sw > W) g.sw = W;
      if (g.sw < 1) g.sw = 1;
      g.sx0 = (W - g.sw) / 2;
    } else {
      g.sh = (3 * W + 2) / 4;
      if (g.sh > H) g.sh = H;
      if (g.sh < 1) g.sh = 1;
      g.sy0 = (H - g.sh) / 2;
    }
  }
}

// ---------------------------------------------------------------------------
// Escalado: lee una fila del fichero y la deja sumada en H (peso horizontal)
// ---------------------------------------------------------------------------
static bool read_row(BmpCtx& c, uint32_t t) {
  BmpWork& w = *c.w;
  const BmpInfo& bi = c.bi;
  const BmpGeo& g = c.g;
  uint32_t dw = g.dw, sw = g.sw;
  uint32_t bpp = bi.bpp;
  uint32_t* H = w.u.s.H;
  memset(H, 0, dw * 3 * sizeof(uint32_t));

  uint32_t rowoff = bi.dataOff + t * bi.stride;
  uint32_t firstbyte = (g.sx0 * bpp) >> 3;
  uint32_t endbyte = ((g.sx0 + sw) * bpp + 7) >> 3;
  if (!c.in->seek(rowoff + firstbyte)) { c.errmsg = "BMP TRUNCATED"; return false; }

  uint32_t cb = firstbyte, have = 0;     // la ventana [cb, cb + have) de la fila esta en w.chunk
  uint32_t dx = 0, rem_d = sw;
  for (uint32_t i = g.sx0; i < g.sx0 + sw; i++) {
    uint32_t pb = (i * bpp) >> 3;
    if (pb >= cb + have) {
      uint32_t n = endbyte - pb;
      if (n > sizeof(w.chunk)) n = sizeof(w.chunk);
      if (c.in->read(w.chunk, (int)n) != (int)n) { c.errmsg = "BMP TRUNCATED"; return false; }
      cb = pb;
      have = n;
    }
    const uint8_t* p = w.chunk + (pb - cb);
    uint32_t r, gg, b;
    if (bpp >= 24) {
      b = p[0]; gg = p[1]; r = p[2];
    } else {
      uint32_t idx;
      if (bpp == 8) idx = p[0];
      else if (bpp == 4) idx = (i & 1) ? (p[0] & 15) : (p[0] >> 4);
      else idx = (p[0] >> (7 - (i & 7))) & 1;
      r = w.spal[idx][0]; gg = w.spal[idx][1]; b = w.spal[idx][2];
    }
    uint32_t rem_s = dw;
    while (rem_s) {
      uint32_t m = rem_s < rem_d ? rem_s : rem_d;
      H[dx * 3 + 0] += m * r;
      H[dx * 3 + 1] += m * gg;
      H[dx * 3 + 2] += m * b;
      rem_s -= m;
      rem_d -= m;
      if (!rem_d) { dx++; rem_d = sw; }
    }
  }
  return true;
}

// Una pasada completa por el fichero: llama a sink con cada fila del destino terminada (en w.rowrgb)
static bool scale_pass(BmpCtx& c, BmpSink sink) {
  BmpWork& w = *c.w;
  const BmpGeo& g = c.g;
  uint32_t sh = g.sh, sw = g.sw, dw = g.dw, dh = g.dh;
  uint32_t area = sw * sh, half = area / 2;
  uint32_t* H = w.u.s.H;
  uint32_t* A = w.u.s.A;
  uint32_t n3 = dw * 3;
  memset(A, 0, n3 * sizeof(uint32_t));
  uint32_t done = 0;
  bool bu = c.bi.bottomUp;
  uint32_t t0 = bu ? (c.bi.H - g.sy0 - sh) : g.sy0;      // primera fila del fichero que se usa

  for (uint32_t k = 0; k < sh; k++) {
    uint32_t ys = bu ? (sh - 1 - k) : k;               // fila dentro del recorte
    if (!read_row(c, t0 + k)) return false;
    uint32_t s0 = dh * ys, s1 = dh * (ys + 1);          // lo que ocupa en unidades del destino
    uint32_t first = s0 / sh, last = (s1 - 1) / sh;
    for (uint32_t q = 0; q <= last - first; q++) {
      uint32_t dy = bu ? (last - q) : (first + q);
      uint32_t d0 = dy * sh, d1 = (dy + 1) * sh;
      uint32_t hi = s1 < d1 ? s1 : d1;
      uint32_t lo = s0 > d0 ? s0 : d0;
      uint32_t m = hi - lo;
      for (uint32_t i = 0; i < n3; i++) A[i] += m * H[i];
      bool complete = bu ? (s0 <= d0) : (s1 >= d1);
      if (complete) {
        for (uint32_t i = 0; i < n3; i++) w.rowrgb[i] = (uint8_t)((A[i] + half) / area);
        sink(c, dy);
        memset(A, 0, n3 * sizeof(uint32_t));
        done++;
      }
    }
  }
  if (done != dh) { c.errmsg = "INTERNAL ERROR"; return false; }
  return true;
}

// ---------------------------------------------------------------------------
// Pasada 1: histograma
// ---------------------------------------------------------------------------
static void sink_hist(BmpCtx& c, uint32_t) {
  const uint8_t* p = c.w->rowrgb;
  for (uint32_t dx = 0; dx < c.g.dw; dx++, p += 3)
    c.hist[((p[0] >> 4) << 8) | ((p[1] >> 4) << 4) | (p[2] >> 4)]++;
}

// ---------------------------------------------------------------------------
// Paleta: cortes de mediana sobre el histograma
// ---------------------------------------------------------------------------
static void box_scan(const uint16_t* hist, BmpBox& b) {
  uint8_t mn[3] = {15, 15, 15}, mx[3] = {0, 0, 0};
  uint32_t cnt = 0, nb = 0;
  for (uint8_t r = b.lo[0]; r <= b.hi[0]; r++)
    for (uint8_t g = b.lo[1]; g <= b.hi[1]; g++)
      for (uint8_t bl = b.lo[2]; bl <= b.hi[2]; bl++) {
        uint16_t n = hist[(r << 8) | (g << 4) | bl];
        if (!n) continue;
        cnt += n;
        nb++;
        if (r < mn[0]) mn[0] = r;
        if (r > mx[0]) mx[0] = r;
        if (g < mn[1]) mn[1] = g;
        if (g > mx[1]) mx[1] = g;
        if (bl < mn[2]) mn[2] = bl;
        if (bl > mx[2]) mx[2] = bl;
      }
  if (nb) {
    for (int a = 0; a < 3; a++) { b.lo[a] = mn[a]; b.hi[a] = mx[a]; }
  }
  b.cnt = (uint16_t)cnt;
  b.nbins = (uint16_t)nb;
}

static int build_palette(BmpCtx& c) {
  BmpBox* boxes = c.w->u.boxes;
  const uint16_t* hist = c.hist;
  int nb = 1;
  for (int a = 0; a < 3; a++) { boxes[0].lo[a] = 0; boxes[0].hi[a] = 15; }
  box_scan(hist, boxes[0]);

  static const uint8_t order[3] = {1, 0, 2};        // a igualdad, G, luego R, luego B
  static const uint8_t wt[3]    = {3, 4, 2};        // peso de cada canal (R, G, B)
  while (nb < 256) {
    int pick = -1;
    for (int i = 0; i < nb; i++)
      if (boxes[i].nbins >= 2 && (pick < 0 || boxes[i].cnt > boxes[pick].cnt)) pick = i;
    if (pick < 0) break;
    BmpBox& b = boxes[pick];
    int axis = 0, bests = -1;
    for (int k = 0; k < 3; k++) {
      int a = order[k];
      int s = (b.hi[a] - b.lo[a]) * wt[a];
      if (s > bests) { bests = s; axis = a; }
    }
    // histograma marginal de la caja por el eje elegido
    uint32_t m[16];
    memset(m, 0, sizeof(m));
    for (uint8_t r = b.lo[0]; r <= b.hi[0]; r++)
      for (uint8_t g = b.lo[1]; g <= b.hi[1]; g++)
        for (uint8_t bl = b.lo[2]; bl <= b.hi[2]; bl++)
          m[axis == 0 ? r : (axis == 1 ? g : bl)] += hist[(r << 8) | (g << 4) | bl];
    uint32_t half = ((uint32_t)b.cnt + 1) / 2, cum = 0;
    uint8_t cut = b.hi[axis];
    for (uint8_t v = b.lo[axis]; v <= b.hi[axis]; v++) {
      cum += m[v];
      if (cum >= half) { cut = v; break; }
    }
    if (cut == b.hi[axis]) cut = (uint8_t)(b.hi[axis] - 1);
    BmpBox nbx = b;
    b.hi[axis] = cut;
    nbx.lo[axis] = (uint8_t)(cut + 1);
    box_scan(hist, b);
    box_scan(hist, nbx);
    boxes[nb++] = nbx;
  }

  // el color de cada caja: la media de sus bins (centro del bin = valor * 17)
  for (int j = 0; j < nb; j++) {
    const BmpBox& b = boxes[j];
    uint32_t sum[3] = {0, 0, 0};
    for (uint8_t r = b.lo[0]; r <= b.hi[0]; r++)
      for (uint8_t g = b.lo[1]; g <= b.hi[1]; g++)
        for (uint8_t bl = b.lo[2]; bl <= b.hi[2]; bl++) {
          uint32_t n = hist[(r << 8) | (g << 4) | bl];
          sum[0] += n * (r * 17u);
          sum[1] += n * (g * 17u);
          sum[2] += n * (bl * 17u);
        }
    uint32_t cnt = b.cnt ? b.cnt : 1;
    for (int a = 0; a < 3; a++) c.w->pal[j][a] = (uint8_t)((sum[a] + cnt / 2) / cnt);
  }
  for (int j = nb; j < 256; j++) c.w->pal[j][0] = c.w->pal[j][1] = c.w->pal[j][2] = 0;
  return nb;
}

// Un color (R, G, B de 0 a 255) -> las cuatro densidades 0..15 de la red de la SCART (pal160.py, en enteros)
static void solve_int(int r, int g, int b, uint8_t& nr, uint8_t& ng, uint8_t& nb, uint8_t& nw) {
  int tgt[3] = {r, g, b};
  int64_t best = -1;
  nr = ng = nb = nw = 0;
  for (int w = 0; w < 16; w++) {
    int64_t lift = BMP_LIFTU * w;
    int64_t e = 0;
    int n3[3];
    for (int ch = 0; ch < 3; ch++) {
      int64_t t = (int64_t)tgt[ch] * 65536;
      int64_t num = t - lift;
      int64_t n = num <= 0 ? 0 : (num + BMP_STEP / 2) / BMP_STEP;
      if (n > 15) n = 15;
      int64_t err = n * BMP_STEP + lift - t;
      e += err * err;
      n3[ch] = (int)n;
    }
    if (best < 0 || e < best) {
      best = e;
      nr = (uint8_t)n3[0]; ng = (uint8_t)n3[1]; nb = (uint8_t)n3[2]; nw = (uint8_t)w;
    }
  }
}

// ---------------------------------------------------------------------------
// Pasada 2: color mas cercano
// ---------------------------------------------------------------------------
static int nearest(BmpCtx& c, int r, int g, int b) {
  if (r == c.lastr && g == c.lastg && b == c.lastb) return c.lastidx;
  int best = 0;
  int32_t bd = 0x7FFFFFFF;
  for (int j = 0; j < c.npal; j++) {
    int dr = r - c.w->pal[j][0], dg = g - c.w->pal[j][1], db = b - c.w->pal[j][2];
    int32_t d = 3 * dr * dr + 4 * dg * dg + 2 * db * db;
    if (d < bd) { bd = d; best = j; }
  }
  c.lastr = r; c.lastg = g; c.lastb = b; c.lastidx = best;
  return best;
}

static void sink_map(BmpCtx& c, uint32_t dy) {
  const uint8_t* p = c.w->rowrgb;
  uint8_t* o = c.out + BMP_PIX_OFF + (c.g.dy0 + dy) * BMP_OUT_W + c.g.dx0;
  for (uint32_t dx = 0; dx < c.g.dw; dx++, p += 3)
    o[dx] = (uint8_t)nearest(c, p[0], p[1], p[2]);
}

// ---------------------------------------------------------------------------
bool bmp_to_i16(BmpReader& in, uint8_t fit, uint8_t* out, BmpWork& w, BmpError& err) {
  BmpCtx c;
  memset(&c, 0, sizeof(c));
  c.in = &in;
  c.w = &w;
  c.out = out;
  c.errmsg = "BMP ERROR";
  if (!parse_header(in, w, c.bi, err)) return false;
  if ((uint64_t)c.bi.W * c.bi.H > BMP_MAX_PIXELS) return fail(err, "BMP TOO LARGE");
  geometry(c.bi, fit, c.g);
  if ((uint64_t)c.g.sw * c.g.sh > BMP_MAX_PIXELS) return fail(err, "BMP TOO LARGE");

  // pasada 1: el histograma vive donde luego iran los pixeles
  c.hist = (uint16_t*)(out + BMP_PIX_OFF);
  memset(out + BMP_PIX_OFF, 0, 4096 * sizeof(uint16_t));
  if (!scale_pass(c, sink_hist)) return fail(err, c.errmsg);
  uint32_t outside = BMP_PIX_COUNT - c.g.dw * c.g.dh;       // las bandas negras del "fit"
  c.hist[0] += (uint16_t)outside;

  c.npal = build_palette(c);

  // cabecera y paleta
  memcpy(out, "I160", 4);
  for (int j = 0; j < 256; j++) {
    uint8_t nr, ng, nb, nw;
    solve_int(w.pal[j][0], w.pal[j][1], w.pal[j][2], nr, ng, nb, nw);
    out[4 + 2 * j] = (uint8_t)((ng << 4) | nb);
    out[5 + 2 * j] = (uint8_t)((nw << 4) | nr);
  }

  // pasada 2: el fondo y la imagen
  c.lastr = c.lastg = c.lastb = -1;
  int black = nearest(c, 0, 0, 0);
  memset(out + BMP_PIX_OFF, black, BMP_PIX_COUNT);
  c.lastr = c.lastg = c.lastb = -1;
  if (!scale_pass(c, sink_map)) return fail(err, c.errmsg);
  return true;
}
