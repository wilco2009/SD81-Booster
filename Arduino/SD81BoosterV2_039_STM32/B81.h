#ifndef B81_H
#define B81_H

// Conversion de listados BASIC en texto (.B81, el formato de EightyOne) a
// imagen .P, para cargarlos con LOAD "PROG.B81" como un .P normal.
//
// El codigo no depende de Arduino: se compila tambien en el PC para
// comparar su salida con la del cargador de EightyOne.

#include <stdint.h>

// El programa y el DFILE tienen que quedar por debajo de 32K, asi que un .P
// nunca pasa de 32768-16393 bytes.
#define B81_MAX_P 16375

// Fuente del texto. Se lee dos veces: la primera pasada numera las lineas,
// recoge las etiquetas y mide la linea mas larga; la segunda tokeniza.
class B81Reader {
public:
  virtual int  getByte() = 0;   // 0..255, o -1 al final del fichero
  virtual bool rewind() = 0;
};

struct B81Error {
  int  fileLine;    // linea del fichero de texto (desde 1), 0 si no aplica
  int  basicLine;   // numero de linea BASIC, -1 si no aplica
  char msg[48];
  char text[48];    // principio de la linea que ha fallado
};

// Convierte el listado en out (B81_MAX_P bytes como minimo): variables del
// sistema, programa, DFILE colapsado y zona de variables vacia; es decir,
// lo mismo que se envia al Z80 al cargar un .P (de VERSN hasta E_LINE).
// Devuelve la longitud, o 0 si hay error (con el detalle en err).
uint16_t b81_to_p(B81Reader& in, uint8_t* out, B81Error& err);

#endif
