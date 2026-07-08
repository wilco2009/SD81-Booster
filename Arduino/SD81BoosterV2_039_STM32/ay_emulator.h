#ifndef AY_EMULATOR_H
#define AY_EMULATOR_H

#include <Arduino.h>     // Necesario para tipos como uint8_t, etc.
#include <HardwareTimer.h> // Para la ISR
#include "dac_control.h"

#define R_FINE_TUNE_A     000
#define R_COARSE_TUNE_A   001
#define R_FINE_TUNE_B     002
#define R_COARSE_TUNE_B   003
#define R_FINE_TUNE_C     004
#define R_COARSE_TUNE_C   005
#define R_NOISE_PERIOD    006
#define R_ENABLE          007
#define R_AMPLITUDE_A     010
#define R_AMPLITUDE_B     011
#define R_AMPLITUDE_C     012
#define R_FINE_EP         013
#define R_COARSE_EP       014
#define R_ENV_SHAPE       015

// --- Constantes de Frecuencia del AY ---
const float AY_CLOCK_FREQ = 1789772.8f; // Frecuencia del reloj maestro del chip AY-3-8912
const float AY_CLOCK_DIV_16 = AY_CLOCK_FREQ / 16.0f; // AY_CLOCK_FREQ / 16
const float freq_factor = 0.5f;
// --- Divisores para convertir periodos AY a periodos de la ISR (en muestras) ---
const float period_divider_tone_noise = AY_CLOCK_DIV_16 / SAMPLE_RATE / freq_factor;
const float period_divider_envelope_actual = AY_CLOCK_FREQ / (256.0f * SAMPLE_RATE * freq_factor);

// --- Tabla de Volumen no lineal del AY-3-8912 (escalada para DAC de 12 bits) ---
extern const uint16_t AY_VOLUME_TABLE[16]; // Declaración externa

// --- Variables para los Registros del Emulador AY (declaración externa) ---
extern uint8_t ay_registers[16]; // Los registros AY (R0-R15)

// Constantes para los bits de R13 (Envelope Shape)
#define ENVELOPE_R13_HOLD       0x01 // Bit 0
#define ENVELOPE_R13_ALTERNATE  0x02 // Bit 1
#define ENVELOPE_R13_ATTACK     0x04 // Bit 2
#define ENVELOPE_R13_CONTINUE   0x08 // Bit 3

// --- Funciones Públicas del Emulador AY ---
// Función de inicialización principal del emulador AY
void ay_emulator_init(void); 

// Rutina de servicio de interrupción (ISR) para el audio AY
uint16_t ay_audio_isr();

// asigna value al registro reg y aplica los cambios
void AY_SetReg(uint8_t reg, uint8_t value);

// Función para notificar al emulador que los registros han cambiado y que debe aplicar los nuevos valores
void ay_emulator_apply_register_changes();

// Función auxiliar para obtener el período AY a partir de la frecuencia (útil para que otros módulos lo usen)
uint16_t ay_emulator_get_period_from_freq(float freq_hz); 

#endif // AY_EMULATOR_H