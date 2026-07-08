#include "dac_control.h"

// Función para configurar el DAC
void setup_dac() {
    pinMode(DAC_PIN, OUTPUT);
    analogWriteResolution(12); // Establece la resolución del DAC a 12 bits
    analogWrite(DAC_PIN, 0);   // Inicializa la salida del DAC a 0
}

// Función para establecer el valor de salida del DAC
void set_dac_output(uint16_t value) {
    DAC_out = value;
}