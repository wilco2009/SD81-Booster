// BMP.cpp del STM32 en el PC: convierte un .BMP en un .I16 con el mismo codigo que usa el comando 76 del firmware.
//   test_bmp.exe entrada.bmp salida.i16 [fit]       fit: 0 estira (por omision), 1 conserva la proporcion, 2 recorta
// Escribe "OK <bytes leidos> <seeks hacia atras>" o "ERR <motivo>". Comprueba tambien la memoria de trabajo y que la lectura
// del fichero es hacia delante (salvo el principio de cada pasada). build_bmp.sh lo compara con bmp2i16.py, byte a byte.
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include "BMP.h"

class FileReader : public BmpReader {
public:
  explicit FileReader(FILE* f) : f_(f) {
    fseek(f_, 0, SEEK_END);
    size_ = (uint32_t)ftell(f_);
    fseek(f_, 0, SEEK_SET);
  }
  uint32_t size() override { return size_; }
  bool seek(uint32_t pos) override {
    if (pos > size_) return false;
    if (pos < pos_) back++;
    pos_ = pos;
    return fseek(f_, (long)pos, SEEK_SET) == 0;
  }
  int read(uint8_t* dst, int n) override {
    int r = (int)fread(dst, 1, (size_t)n, f_);
    pos_ += (uint32_t)r;
    total += (uint32_t)r;
    return r;
  }
  uint32_t total = 0;
  int back = 0;
private:
  FILE* f_;
  uint32_t size_, pos_ = 0;
};

int main(int argc, char** argv) {
  if (argc < 3) { printf("uso: test_bmp entrada.bmp salida.i16 [fit]\n"); return 2; }
  FILE* f = fopen(argv[1], "rb");
  if (!f) { printf("ERR no puedo abrir %s\n", argv[1]); return 2; }
  uint8_t fit = argc > 3 ? (uint8_t)atoi(argv[3]) : 0;

  // la misma reserva que hace el firmware: un .I16 (alineado) y la memoria de trabajo
  uint8_t* out = (uint8_t*)malloc(BMP_I16_SIZE);
  BmpWork* w = (BmpWork*)malloc(sizeof(BmpWork));
  if (!out || !w) { printf("ERR sin memoria\n"); return 2; }
  memset(out, 0xEE, BMP_I16_SIZE);       // para ver bytes sin escribir

  FileReader rd(f);
  BmpError err;
  memset(&err, 0, sizeof(err));
  bool ok = bmp_to_i16(rd, fit, out, *w, err);
  fclose(f);
  if (!ok) { printf("ERR %s\n", err.msg); return 1; }
  for (int i = 0; i < BMP_I16_SIZE; i++)
    if (out[i] == 0xEE && i >= 516) {              // un indice 0xEE legitimo es posible: solo avisa si hay muchos
      int n = 0;
      for (int j = 516; j < BMP_I16_SIZE; j++) n += out[j] == 0xEE;
      if (n == BMP_I16_SIZE - 516) { printf("ERR no se escribieron los pixeles\n"); return 1; }
      break;
    }
  FILE* o = fopen(argv[2], "wb");
  if (!o) { printf("ERR no puedo escribir %s\n", argv[2]); return 2; }
  fwrite(out, 1, BMP_I16_SIZE, o);
  fclose(o);
  printf("OK %u %d %zu\n", rd.total, rd.back, sizeof(BmpWork));
  free(out);
  free(w);
  return 0;
}
