#include "I16Common.h"

// Ver I16Common.h. Referencia en Python con el mismo algoritmo entero: EXAMPLES/PHOTO/bmp2i16.py

// Constantes de la red de la SCART (pal160.py: 255/15 de la tension relativa de un paso del canal y de BRIGHT, por 65536)
static const int64_t I16_STEP  = 707612;
static const int64_t I16_LIFTU = 406500;

// ---------------------------------------------------------------------------
// Geometria: que parte del origen va a que parte de los 160x100 (la pantalla es 4:3)
// ---------------------------------------------------------------------------
void i16_geometry(uint32_t W, uint32_t H, uint8_t fit, I16Geo& g) {
  g.sx0 = g.sy0 = 0;
  g.sw = W;
  g.sh = H;
  g.dx0 = g.dy0 = 0;
  g.dw = I16_W;
  g.dh = I16_H;
  bool wide = (uint64_t)W * 3 >= (uint64_t)H * 4;
  if (fit == I16_FIT_FIT) {
    if (wide) {
      g.dh = (800 * H + 3 * W) / (6 * W);
      if (g.dh < 1) g.dh = 1;
      if (g.dh > I16_H) g.dh = I16_H;
    } else {
      g.dw = (240 * W + H) / (2 * H);
      if (g.dw < 1) g.dw = 1;
      if (g.dw > I16_W) g.dw = I16_W;
    }
    g.dx0 = (I16_W - g.dw) / 2;
    g.dy0 = (I16_H - g.dh) / 2;
  } else if (fit == I16_FIT_CROP) {
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
// Escalado
// ---------------------------------------------------------------------------
bool i16_scale_pass(const I16Scale& s, I16LineFn getline, I16SinkFn sink, void* ctx, I16Work& w) {
  uint32_t Sh = s.Sh, D2 = s.D2;
  uint32_t area = s.Sw * Sh, half = area / 2;
  uint32_t* H = w.u.s.H;
  uint32_t* A = w.u.s.A;
  uint32_t n3 = s.D1 * 3;
  memset(A, 0, n3 * sizeof(uint32_t));
  uint32_t done = 0;
  bool bu = s.rev_y;

  for (uint32_t k = 0; k < Sh; k++) {
    uint32_t ys = bu ? (Sh - 1 - k) : k;               // linea dentro del recorte, en el orden del destino
    if (!getline(ctx, k, H)) return false;
    uint32_t s0 = D2 * ys, s1 = D2 * (ys + 1);          // lo que ocupa en unidades del destino
    uint32_t first = s0 / Sh, last = (s1 - 1) / Sh;
    for (uint32_t q = 0; q <= last - first; q++) {
      uint32_t dy = bu ? (last - q) : (first + q);
      uint32_t d0 = dy * Sh, d1 = (dy + 1) * Sh;
      uint32_t hi = s1 < d1 ? s1 : d1;
      uint32_t lo = s0 > d0 ? s0 : d0;
      uint32_t m = hi - lo;
      for (uint32_t i = 0; i < n3; i++) A[i] += m * H[i];
      bool complete = bu ? (s0 <= d0) : (s1 >= d1);
      if (complete) {
        for (uint32_t i = 0; i < n3; i++) w.rowrgb[i] = (uint8_t)((A[i] + half) / area);
        sink(ctx, dy, w.rowrgb);
        memset(A, 0, n3 * sizeof(uint32_t));
        done++;
      }
    }
  }
  return done == D2;
}

// ---------------------------------------------------------------------------
// Histograma y paleta: cortes de mediana
// ---------------------------------------------------------------------------
void i16_hist_add(uint16_t* hist, const uint8_t* p, uint32_t n) {
  for (uint32_t i = 0; i < n; i++, p += 3)
    hist[((p[0] >> 4) << 8) | ((p[1] >> 4) << 4) | (p[2] >> 4)]++;
}

static void box_scan(const uint16_t* hist, I16Box& b) {
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

int i16_build_palette(const uint16_t* hist, I16Box* boxes, uint8_t pal[256][3]) {
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
    I16Box& b = boxes[pick];
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
    I16Box nbx = b;
    b.hi[axis] = cut;
    nbx.lo[axis] = (uint8_t)(cut + 1);
    box_scan(hist, b);
    box_scan(hist, nbx);
    boxes[nb++] = nbx;
  }

  // el color de cada caja: la media de sus bins (centro del bin = valor * 17)
  for (int j = 0; j < nb; j++) {
    const I16Box& b = boxes[j];
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
    for (int a = 0; a < 3; a++) pal[j][a] = (uint8_t)((sum[a] + cnt / 2) / cnt);
  }
  for (int j = nb; j < 256; j++) pal[j][0] = pal[j][1] = pal[j][2] = 0;
  return nb;
}

// Un color (R, G, B de 0 a 255) -> las cuatro densidades 0..15 de la red de la SCART (pal160.py, en enteros)
static void solve_int(int r, int g, int b, uint8_t& nr, uint8_t& ng, uint8_t& nb, uint8_t& nw) {
  int tgt[3] = {r, g, b};
  int64_t best = -1;
  nr = ng = nb = nw = 0;
  for (int w = 0; w < 16; w++) {
    int64_t lift = I16_LIFTU * w;
    int64_t e = 0;
    int n3[3];
    for (int ch = 0; ch < 3; ch++) {
      int64_t t = (int64_t)tgt[ch] * 65536;
      int64_t num = t - lift;
      int64_t n = num <= 0 ? 0 : (num + I16_STEP / 2) / I16_STEP;
      if (n > 15) n = 15;
      int64_t err = n * I16_STEP + lift - t;
      e += err * err;
      n3[ch] = (int)n;
    }
    if (best < 0 || e < best) {
      best = e;
      nr = (uint8_t)n3[0]; ng = (uint8_t)n3[1]; nb = (uint8_t)n3[2]; nw = (uint8_t)w;
    }
  }
}

void i16_write_header(uint8_t* out, const uint8_t pal[256][3]) {
  memcpy(out, "I160", 4);
  for (int j = 0; j < 256; j++) {
    uint8_t nr, ng, nb, nw;
    solve_int(pal[j][0], pal[j][1], pal[j][2], nr, ng, nb, nw);
    out[4 + 2 * j] = (uint8_t)((ng << 4) | nb);
    out[5 + 2 * j] = (uint8_t)((nw << 4) | nr);
  }
}

// ---------------------------------------------------------------------------
// Color mas cercano
// ---------------------------------------------------------------------------
void i16_map_init(I16Map& m, const uint8_t (*pal)[3], int npal) {
  m.pal = pal;
  m.npal = npal;
  i16_map_reset(m);
}

void i16_map_reset(I16Map& m) {
  m.lastr = m.lastg = m.lastb = -1;
  m.lastidx = 0;
}

int i16_nearest(I16Map& m, int r, int g, int b) {
  if (r == m.lastr && g == m.lastg && b == m.lastb) return m.lastidx;
  int best = 0;
  int32_t bd = 0x7FFFFFFF;
  for (int j = 0; j < m.npal; j++) {
    int dr = r - m.pal[j][0], dg = g - m.pal[j][1], db = b - m.pal[j][2];
    int32_t d = 3 * dr * dr + 4 * dg * dg + 2 * db * db;
    if (d < bd) { bd = d; best = j; }
  }
  m.lastr = r; m.lastg = g; m.lastb = b; m.lastidx = best;
  return best;
}

void i16_map_row(I16Map& m, const uint8_t* p, uint32_t n, uint8_t* dst, uint32_t stride) {
  for (uint32_t i = 0; i < n; i++, p += 3, dst += stride)
    *dst = (uint8_t)i16_nearest(m, p[0], p[1], p[2]);
}
