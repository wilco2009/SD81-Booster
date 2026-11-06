#include "BMP.h"
#include <string.h>

// Ver BMP.h. Referencia en Python con el mismo algoritmo entero: EXAMPLES/PHOTO/bmp2i16.py

#define BMP_MAX_DIM      16384
#define BMP_MAX_PIXELS   16000000u     // del area de origen usada: con 255 * pixeles el acumulador cabe en 32 bits
#define BMP_PIX_OFF      I16_PIX_OFF   // principio de los pixeles en el .I16
#define BMP_PIX_COUNT    I16_PIX_COUNT

struct BmpInfo {
  uint32_t dataOff;
  uint32_t W, H;          // dimensiones (H en valor absoluto)
  bool     bottomUp;
  uint8_t  bpp;
  uint32_t stride;
  uint32_t fileSize;
};

struct BmpCtx {
  BmpReader* in;
  BmpWork*   w;
  BmpInfo    bi;
  I16Geo     g;
  I16Scale   sc;
  uint16_t*  hist;        // histograma de 4+4+4 bits (pasada 1)
  uint8_t*   out;         // el .I16
  I16Map     map;         // color mas cercano (pasada 2)
  const char* errmsg;     // motivo del fallo de la lectura (null: error interno)
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
// Escalado: lee una fila del fichero y la deja sumada en H (peso horizontal)
// ---------------------------------------------------------------------------
static bool read_row(BmpCtx& c, uint32_t t, uint32_t* H) {
  BmpWork& w = *c.w;
  const BmpInfo& bi = c.bi;
  const I16Geo& g = c.g;
  uint32_t sw = g.sw;
  uint32_t bpp = bi.bpp;
  I16HAcc acc;
  acc.init(H, c.sc);

  uint32_t rowoff = bi.dataOff + t * bi.stride;
  uint32_t firstbyte = (g.sx0 * bpp) >> 3;
  uint32_t endbyte = ((g.sx0 + sw) * bpp + 7) >> 3;
  if (!c.in->seek(rowoff + firstbyte)) { c.errmsg = "BMP TRUNCATED"; return false; }

  uint32_t cb = firstbyte, have = 0;     // la ventana [cb, cb + have) de la fila esta en w.chunk
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
    acc.add(r, gg, b);
  }
  return true;
}

// La k-esima linea que se lee del recorte (la primera del fichero que se usa es t0)
static bool bmp_line(void* vc, uint32_t k, uint32_t* H) {
  BmpCtx& c = *(BmpCtx*)vc;
  uint32_t t0 = c.bi.bottomUp ? (c.bi.H - c.g.sy0 - c.g.sh) : c.g.sy0;
  return read_row(c, t0 + k, H);
}

// Una pasada completa por el fichero: llama a sink con cada fila del destino terminada (en w.c.rowrgb)
static bool scale_pass(BmpCtx& c, I16SinkFn sink) {
  c.sc.Sw = c.g.sw;
  c.sc.Sh = c.g.sh;
  c.sc.D1 = c.g.dw;
  c.sc.D2 = c.g.dh;
  c.sc.rev_x = false;
  c.sc.rev_y = c.bi.bottomUp;
  c.errmsg = nullptr;
  return i16_scale_pass(c.sc, bmp_line, sink, &c, c.w->c);
}

// ---------------------------------------------------------------------------
// Pasada 1: histograma
// ---------------------------------------------------------------------------
static void sink_hist(void* vc, uint32_t, const uint8_t* rgb) {
  BmpCtx& c = *(BmpCtx*)vc;
  i16_hist_add(c.hist, rgb, c.g.dw);
}

// ---------------------------------------------------------------------------
// Pasada 2: color mas cercano
// ---------------------------------------------------------------------------
static void sink_map(void* vc, uint32_t dy, const uint8_t* rgb) {
  BmpCtx& c = *(BmpCtx*)vc;
  i16_map_row(c.map, rgb, c.g.dw, c.out + BMP_PIX_OFF + (c.g.dy0 + dy) * BMP_OUT_W + c.g.dx0, 1);
}

// ---------------------------------------------------------------------------
bool bmp_to_i16(BmpReader& in, uint8_t fit, uint8_t* out, BmpWork& w, BmpError& err) {
  BmpCtx c;
  memset(&c, 0, sizeof(c));
  c.in = &in;
  c.w = &w;
  c.out = out;
  if (!parse_header(in, w, c.bi, err)) return false;
  if ((uint64_t)c.bi.W * c.bi.H > BMP_MAX_PIXELS) return fail(err, "BMP TOO LARGE");
  i16_geometry(c.bi.W, c.bi.H, fit, c.g);
  if ((uint64_t)c.g.sw * c.g.sh > BMP_MAX_PIXELS) return fail(err, "BMP TOO LARGE");

  // pasada 1: el histograma vive donde luego iran los pixeles
  c.hist = (uint16_t*)(out + BMP_PIX_OFF);
  memset(out + BMP_PIX_OFF, 0, 4096 * sizeof(uint16_t));
  if (!scale_pass(c, sink_hist)) return fail(err, c.errmsg ? c.errmsg : "INTERNAL ERROR");
  uint32_t outside = BMP_PIX_COUNT - c.g.dw * c.g.dh;       // las bandas negras del "fit"
  c.hist[0] += (uint16_t)outside;

  int npal = i16_build_palette(c.hist, w.c.u.boxes, w.c.pal);
  i16_write_header(out, w.c.pal);

  // pasada 2: el fondo y la imagen
  i16_map_init(c.map, w.c.pal, npal);
  int black = i16_nearest(c.map, 0, 0, 0);
  memset(out + BMP_PIX_OFF, black, BMP_PIX_COUNT);
  i16_map_reset(c.map);
  if (!scale_pass(c, sink_map)) return fail(err, c.errmsg ? c.errmsg : "INTERNAL ERROR");
  return true;
}
