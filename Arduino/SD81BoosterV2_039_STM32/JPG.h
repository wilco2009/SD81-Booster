#ifndef JPG_H
#define JPG_H

// Conversion de imagenes JPG (baseline) a una imagen .I16 de 160x100 con 256 colores, con
// el mismo resultado y las mismas colocaciones (fit) que el BMP (BMP.h): el comando 76
// (CMD_f_open_img, COMMANDS.cpp) elige BMP o JPG por los dos primeros bytes del fichero.
//
// Un JPG de foto pesa 10-20 veces menos que el BMP y el resultado es de 160x100, asi que no
// se decodifica entero: cada bloque de 8x8 se reduce a la escala 1/8 (solo el coeficiente
// DC; los AC se recorren solo para avanzar en el flujo de bits), y si el JPG es pequeno, a
// 1/4 (2x2), 1/2 (4x4) o 1/1 (8x8) con una IDCT reducida. Se elige la mayor reduccion que
// aun deja al menos tantas muestras como puntos de destino (160x100, o menos con "fit") y cuya
// fila de MCU cabe en rowbuf. Como libjpeg, la croma submuestreada a la mitad en los dos ejes
// (4:2:0) se decodifica al doble de escala (a 1/8, 2x2 por bloque) para no repetir su DC.
// La imagen reducida no se guarda: se acumula en streaming en el mismo escalado por media de
// area del BMP (I16Common.h), una fila de MCU cada vez.
//
// JPG admitidos: secuencial baseline o extendido (SOF0 / SOF1) de 8 bits con Huffman, de 1
// componente (gris) o 3 (YCbCr; tambien RGB si lo dice Adobe o los identificadores), muestreo
// de croma 4:4:4, 4:2:2, 4:4:0, 4:2:0, 4:1:1 (la MCU puede ser de hasta 4 bloques), con o sin
// marcadores de reinicio (RSTn), una sola pasada (scan) con todos los componentes. Se aplica
// la orientacion EXIF (etiqueta 0x0112) de las fotos de movil. Se rechazan, con el motivo en
// err: aritmetico, 12 bits, CMYK, varios scans (en un baseline), sin tablas.
//
// Progresivos (SOF2): solo se usa el DC, es decir, siempre la escala 1/8, sin guardar nada: se
// salta (buscando marcadores) hasta el primer scan de DC con todos los componentes entrelazados
// (Ss = Se = 0, Ah = 0), que es el primero del fichero en lo habitual, se decodifica en
// streaming como el DC de un baseline y se deja de leer: los scans de AC y de refinamiento ni
// se tocan, asi que un progresivo grande se lee mas deprisa que un baseline. Si el DC trae
// transformada de punto (Al > 0, lo normal en libjpeg) le faltan sus bits bajos; se centra el
// error sumando la mitad. La croma de 4:2:0 se repite (no hay AC para decodificarla al doble de
// escala), y una imagen de menos de 1280x800 queda por debajo de 160x100 muestras y se amplia
// (borrosa). Sin ese scan de DC (solo AC, o DC componente a componente) se rechaza.
//
// Se lee el fichero dos veces (la primera pasada elige la paleta), siempre hacia delante
// (salvo el salto de vuelta al principio de los datos).
//
// El codigo no depende de Arduino: se compila tambien en el PC y se compara, byte a byte,
// con EXAMPLES/PHOTO/jpg2i16.py (claude/mqtttest/build_jpg.sh).

#include "BMP.h"

#define JPG_ROWBUF  4096     // muestras de una fila de MCU (12 bytes por MCU en 4:2:0 a escala 1/8: la croma va a 2x2 por bloque)

// Tabla Huffman de DC (hasta 16 simbolos) y de AC (hasta 256, con busqueda rapida de 7 bits)
struct JpgHuffDC {
  int32_t  maxcode[18];     // mayor codigo de cada longitud (-1: ninguno); [17] = infinito
  uint16_t valoff[17];      // indice del valor = (codigo + valoff[longitud]) modulo 65536
  uint8_t  vals[16];
};
struct JpgHuffAC {
  int32_t  maxcode[18];
  uint16_t valoff[17];
  uint8_t  vals[256];
  uint16_t lut[128];        // (longitud << 8) | simbolo para los codigos de hasta 7 bits; 0 si es mas largo
};

// Memoria de trabajo (la reserva quien llama; ~12 KB)
struct JpgWork {
  I16Work   c;              // escalado y paleta (compartido con BMP.cpp)
  JpgHuffDC dc[2];
  JpgHuffAC ac[2];
  uint8_t   q[4][64];       // tablas de cuantificacion (en zigzag)
  int16_t   coef[64];       // coeficientes del bloque que se esta decodificando (N x N)
  int32_t   tmp[64];        // IDCT
  uint8_t   inbuf[512];     // lectura de la SD
  uint8_t   rowbuf[JPG_ROWBUF];
};

// Convierte el JPG en out (I16_SIZE bytes, alineado a 2). Devuelve false si no se puede, con
// el motivo en err.
bool jpg_to_i16(I16Reader& in, uint8_t fit, uint8_t* out, JpgWork& w, I16Error& err);

#endif
