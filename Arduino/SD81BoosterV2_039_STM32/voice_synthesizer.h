#ifndef VOICE_SYNTHESIZER_H
#define VOICE_SYNTHESIZER_H

#include <stdint.h>
#include <stdbool.h> // Para bool
#include "ay_emulator.h" // Para SAMPLE_RATE

// Factor de escala para normalizar la salida de voz (ajustar según sea necesario)
// Queremos que el rango de 8 bits (-128 a 127) se escale a un rango similar al de AY
// Max de AY es 1600 (para 12 bits), así que un factor de 12 (12*127 = 1524) podría ser un buen punto de partida.
#define VOICE_NORMALIZATION_FACTOR 12 // Escala de 8-bit a un rango compatible con 12-bit DAC y AY mix

// Frecuencia de muestreo del SP0256
#define SP0256_SAMPLE_RATE 9000

// convierte una secuencia de alofonos en modo texto a un array binario 
void text_to_speech(char* text, uint8_t* out);

// Declaración de la función principal para iniciar la reproducción de voz
void Speech(const uint8_t* alophone_sequence);
void Speech(const char* alophone_sequence_str);

// Función para obtener la muestra de audio actual del sintetizador de voz
// Esta función será llamada por la ISR de audio.
int16_t voice_synthesizer_get_sample();

// Función para actualizar el estado del sintetizador de voz (avanzar en la reproducción)
void voice_synthesizer_update();

// Función para detener la reproducción de voz
void voice_synthesizer_stop();

// Variable global que indica si el sintetizador de voz está activo
extern volatile bool voice_is_playing;

// esta funcion es llamada por la rutina de interrupcion del mixer devolviendo el valor de salida del sintetizador de voz
uint16_t voice_synthesizer_isr();


#endif // VOICE_SYNTHESIZER_H