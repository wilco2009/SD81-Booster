#ifndef DAC_CONTROL_H
#define DAC_CONTROL_H

#include <Arduino.h>
#include "PINS.h"

// Definiciones de hardware del DAC
#define DAC_out DAC1->DHR12R1 // Registro de datos DAC1 a 12 bits
// Frecuencia de muestreo 
#define SAMPLE_RATE 44100 // Frecuencia de muestreo del DAC (muestras por segundo)

// Funciones públicas del control DAC
void setup_dac();
void set_dac_output(uint16_t value);

#endif // DAC_CONTROL_H