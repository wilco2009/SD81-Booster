#ifndef MCUSTATE_H
#define MCUSTATE_H

#include <stdint.h>

// Estado del MCU que va en los snapshots .Z81 ([SD81BOOSTER], con las mismas
// claves que EightyOne): el AY del MCU (AY2_*), el VGM (VGM_*), el PEG
// (PEG_*) y los ficheros abiertos por el programa (FILE_HANDLE). Lo usa
// DEBUGGER.cpp al grabar (snap) y al cargar (LOAD *Z81 con el monitor).

// Grabar: escribe las claves con w (una cadena cada vez)
void mcustate_save(void (*w)(const char* s));

// Cargar: mcustate_clear antes de leer el fichero; mcustate_key con cada
// clave de [SD81BOOSTER] (lee sus valores con tok y devuelve true si era
// suya); mcustate_apply al cargar, cuando el programa ya esta parado
void mcustate_clear(void);
bool mcustate_key(const char* key, uint8_t (*tok)(char* b, uint8_t max));
void mcustate_apply(void);

#endif
