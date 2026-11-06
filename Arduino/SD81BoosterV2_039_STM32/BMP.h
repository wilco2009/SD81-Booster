#ifndef BMP_H
#define BMP_H

// Conversion de imagenes BMP a una imagen .I16 de 160x100 con 256 colores (el
// formato del visor EXAMPLES/PHOTO y del modo 160x100 de la FPGA): escala el
// BMP y reduce sus colores a una paleta de 256 propia de la imagen.
//
// (El JPG lo convierte JPG.cpp con la misma salida; la parte comun esta en I16Common.cpp.)
//
// Se usa con el comando 76 (CMD_f_open_img, COMMANDS.cpp): el Z80 abre un .BMP
// y lo lee como si fuera un fichero .I16 con los comandos de acceso aleatorio.
//
// El codigo no depende de Arduino: se compila tambien en el PC para comparar
// su salida, byte a byte, con la de EXAMPLES/PHOTO/bmp2i16.py
// (claude/mqtttest/build_bmp.sh).
//
// Formato .I16 (BMP_I16_SIZE bytes): "I160", 512 bytes de paleta (por entrada,
// byte bajo {G[3:0],B[3:0]} y byte alto {BRIGHT[3:0],R[3:0]}: lo que se escribe
// en POKE 2064 y 2065) y 100 filas de 160 pixeles (un byte = indice).
//
// BMP admitidos: sin compresion (BI_RGB, o BI_BITFIELDS de 32 bits con las
// mascaras normales BGRA) de 1, 4, 8, 24 y 32 bits, de abajo arriba o de arriba
// abajo, con cabecera CORE, INFO, V4 o V5. Hasta 16384 x 16384 y 16 millones de
// pixeles. Se leen dos veces (la primera pasada elige la paleta) y siempre hacia
// delante.
//
// Algoritmo (todo en enteros, para que el PC y el STM32 den lo mismo):
//   1. Escalado con la media por area (cada pixel del destino es el promedio
//      exacto, con pesos enteros, de los pixeles de origen que cubre).
//   2. Histograma de 4+4+4 bits de la imagen escalada y paleta de 256 colores
//      por cortes de mediana (median cut). Cada pixel va al color mas cercano.
//   3. Cada color de la paleta pasa a las cuatro densidades (R, G, B, BRIGHT)
//      que mejor lo dan con la red de la SCART (como pal160.py).

#include "I16Common.h"

#define BMP_I16_SIZE   I16_SIZE
#define BMP_OUT_W      I16_W
#define BMP_OUT_H      I16_H

// Como colocar la imagen en los 160x100 (la pantalla es 4:3):
#define BMP_FIT_STRETCH I16_FIT_STRETCH   // estira para llenar todo (distorsiona si no es 4:3)
#define BMP_FIT_FIT     I16_FIT_FIT       // conserva la proporcion y rellena de negro
#define BMP_FIT_CROP    I16_FIT_CROP      // conserva la proporcion y recorta lo que sobra

typedef I16Reader BmpReader;      // lector de ficheros: size(), seek(pos), read(dst, n)
typedef I16Error  BmpError;       // motivo del fallo (48 bytes)
typedef I16Box    BmpBox;

// Memoria de trabajo (la reserva quien llama; ~6 KB)
struct BmpWork {
  I16Work c;                         // escalado y paleta (compartido con JPG.cpp)
  uint8_t spal[256][3];              // paleta del BMP (RGB)
  uint8_t chunk[504];                // lectura de la SD (multiplo de 3 y de 4 bytes)
};

// Convierte el BMP en out (BMP_I16_SIZE bytes, alineado a 2). Devuelve false si
// no se puede, con el motivo en err.
bool bmp_to_i16(BmpReader& in, uint8_t fit, uint8_t* out, BmpWork& w, BmpError& err);

#endif
