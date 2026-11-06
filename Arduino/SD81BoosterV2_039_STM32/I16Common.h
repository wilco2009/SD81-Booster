#ifndef I16COMMON_H
#define I16COMMON_H

// Parte comun de los conversores de imagen a .I16 (BMP.cpp y JPG.cpp): el formato, la
// colocacion en los 160x100, el escalado por media de area, la paleta de 256 colores
// (histograma 4+4+4 y cortes de mediana), el color mas cercano y las densidades de la red
// de la SCART. Todo en enteros, para que el PC y el STM32 den los mismos bytes.
//
// No depende de Arduino: se compila tambien en el PC (claude/mqtttest/build_bmp.sh y
// build_jpg.sh). Referencia en Python: EXAMPLES/PHOTO/bmp2i16.py (BMP) y jpg2i16.py (JPG).
//
// Formato .I16 (I16_SIZE bytes): "I160", 512 bytes de paleta (por entrada, byte bajo
// {G[3:0],B[3:0]} y byte alto {BRIGHT[3:0],R[3:0]}: lo que se escribe en POKE 2064 y 2065) y
// 100 filas de 160 pixeles (un byte = indice).

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define I16_SIZE      16516
#define I16_W         160
#define I16_H         100
#define I16_PIX_OFF   516
#define I16_PIX_COUNT (I16_W * I16_H)

// Como colocar la imagen en los 160x100 (la pantalla es 4:3):
#define I16_FIT_STRETCH 0     // estira para llenar todo (distorsiona si no es 4:3)
#define I16_FIT_FIT     1     // conserva la proporcion y rellena de negro
#define I16_FIT_CROP    2     // conserva la proporcion y recorta lo que sobra

// Lector de ficheros (lo implementa quien llama: la SD en el firmware, un FILE* en el PC)
class I16Reader {
public:
  virtual uint32_t size() = 0;
  virtual bool     seek(uint32_t pos) = 0;
  virtual int      read(uint8_t* dst, int n) = 0;   // bytes leidos; 0 al final o si falla
};

struct I16Error {
  char msg[48];
};

// Caja del corte de mediana (coordenadas 0..15 de cada canal, extremos incluidos)
struct I16Box {
  uint8_t  lo[3], hi[3];
  uint16_t cnt, nbins;
};

// Memoria de trabajo comun (~5 KB)
struct I16Work {
  union {
    struct {
      uint32_t H[I16_W * 3];         // suma horizontal de la linea de origen
      uint32_t A[I16_W * 3];         // acumulador de la linea de destino
    } s;
    I16Box boxes[256];               // solo entre las dos pasadas
  } u;
  uint8_t pal[256][3];               // paleta elegida (RGB)
  uint8_t rowrgb[I16_W * 3];         // una linea de destino terminada
};

// Que parte del origen va a que parte de los 160x100
struct I16Geo {
  uint32_t sx0, sy0, sw, sh;         // rectangulo de origen que se usa
  uint32_t dx0, dy0, dw, dh;         // rectangulo de destino que se rellena
};

void i16_geometry(uint32_t W, uint32_t H, uint8_t fit, I16Geo& g);

// ---------------------------------------------------------------------------
// Paleta
// ---------------------------------------------------------------------------
// Histograma de 4+4+4 bits: n pixeles RGB seguidos
void i16_hist_add(uint16_t* hist, const uint8_t* rgb, uint32_t n);
// Paleta de hasta 256 colores por cortes de mediana sobre el histograma; devuelve cuantos
int  i16_build_palette(const uint16_t* hist, I16Box* boxes, uint8_t pal[256][3]);
// "I160" y las 256 entradas de la paleta (cada color pasa a las densidades R, G, B y BRIGHT)
void i16_write_header(uint8_t* out, const uint8_t pal[256][3]);

// Color mas cercano de la paleta (con un atajo para el color repetido)
struct I16Map {
  const uint8_t (*pal)[3];
  int npal;
  int lastr, lastg, lastb, lastidx;
};
void i16_map_init(I16Map& m, const uint8_t (*pal)[3], int npal);
void i16_map_reset(I16Map& m);
int  i16_nearest(I16Map& m, int r, int g, int b);
// n pixeles RGB -> indices, escritos en dst con el paso stride (1 en una fila, 160 en una columna)
void i16_map_row(I16Map& m, const uint8_t* rgb, uint32_t n, uint8_t* dst, uint32_t stride);

// ---------------------------------------------------------------------------
// Escalado por media de area
// ---------------------------------------------------------------------------
// El origen son Sh lineas de Sw pixeles; el destino D2 lineas de D1 pixeles. Cada linea de
// origen la entrega getline como suma horizontal ya repartida entre los D1 pixeles de destino
// (I16HAcc), y la linea de destino terminada llega a sink como D1 pixeles RGB.
// rev_x: la linea de origen va en sentido contrario al de destino (espejo);
// rev_y: las lineas de origen llegan en orden contrario al de destino.
struct I16Scale {
  uint32_t Sw, Sh;
  uint32_t D1, D2;
  bool rev_x, rev_y;
};

// Reparto horizontal: add() recibe los pixeles de una linea de origen, uno tras otro
struct I16HAcc {
  uint32_t* H;
  uint32_t D1, Sw, dx, rem_d;
  bool rev;
  void init(uint32_t* h, const I16Scale& s) {
    H = h; D1 = s.D1; Sw = s.Sw; dx = 0; rem_d = s.Sw; rev = s.rev_x;
    memset(H, 0, D1 * 3 * sizeof(uint32_t));
  }
  inline void add(uint32_t r, uint32_t g, uint32_t b) {
    uint32_t rem_s = D1;
    while (rem_s) {
      uint32_t m = rem_s < rem_d ? rem_s : rem_d;
      uint32_t* p = H + (rev ? (D1 - 1 - dx) : dx) * 3;
      p[0] += m * r;
      p[1] += m * g;
      p[2] += m * b;
      rem_s -= m;
      rem_d -= m;
      if (!rem_d) { dx++; rem_d = Sw; }
    }
  }
};

typedef bool (*I16LineFn)(void* ctx, uint32_t k, uint32_t* H);                 // linea k (0..Sh-1) del origen, en orden
typedef void (*I16SinkFn)(void* ctx, uint32_t dline, const uint8_t* rgb);      // linea dline de destino terminada

// Devuelve false si getline falla (o por un error interno)
bool i16_scale_pass(const I16Scale& s, I16LineFn getline, I16SinkFn sink, void* ctx, I16Work& w);

#endif
