#include "ay_emulator.h"
#include "dac_control.h" // Se sigue necesitando para set_dac_output
#include <math.h>        // Para pow si se usa en get_period_from_freq, aunque ya no se usa directamente en esta parte
#include "GLOBALS.h"

// --- Variables para los Registros del Emulador AY (definiciones) ---
uint8_t ay_registers[16] = {0}; // Definición de los registros AY

// Definición de la tabla de volumen
const uint16_t AY_VOLUME_TABLE[16] = {
    0, 80, 100, 130, 160, 200, 250, 300, 370, 450, 550, 680, 850, 1050, 1300, 1600
};

// Internal (static) variables for Tone Channels
static float toneCounterA = 0, toneCounterB = 0, toneCounterC = 0;
static uint8_t toneOutputA = 1, toneOutputB = 1, toneOutputC = 1;
static float tonePeriodA = 1, tonePeriodB = 1, tonePeriodC = 1;
static uint8_t channelAToneEnabled = 0, channelBToneEnabled = 0, channelCToneEnabled = 0;

// Internal (static) variables for Noise Generator
static uint32_t noise_lfsr = 1;
static uint32_t noiseCounter = 0;
static uint16_t noisePeriod = 1;
static uint8_t noiseOutput = 1;
static uint8_t channelANoiseEnabled = 0, channelBNoiseEnabled = 0, channelCNoiseEnabled = 0;

// Internal (static) variables for Envelope Generator
static uint32_t envelopeCounter = 0;
static uint16_t envelopePeriod = 1;
static uint8_t envelopeShape = 0;
static uint8_t envelopePhase = 0;
static bool envelopeHold = false;
static bool currentEnvelopeDirectionIsAttack = false;

static float master_volume_factor = 0.8f;

// Resetea la fase y estado de la envolvente según la forma actual
static void resetEnvelope() {
    envelopeCounter = 0;
    envelopeHold = false;

    if (envelopeShape & ENVELOPE_R13_ATTACK) {
        envelopePhase = 0;
        currentEnvelopeDirectionIsAttack = true;
    } else {
        envelopePhase = 15;
        currentEnvelopeDirectionIsAttack = false;
    }
}

// --- Funciones Públicas del Emulador AY ---

// Función para obtener el período AY a partir de la frecuencia
uint16_t ay_emulator_get_period_from_freq(float freq_hz) {
    if (freq_hz <= 0) return 0xFFFF; // Período máximo para 0Hz (silencio)
//    return (uint16_t)(AY_CLOCK_DIV_16 / freq_hz/2);
    return (uint16_t)(AY_CLOCK_DIV_16 / freq_hz);
}

// Aplica los valores de los registros AY a las variables internas del emulador
void ay_emulator_apply_register_changes() {
    // TONE PERIODS (R0-R5)
    float ayPeriodA = (((ay_registers[1] & 0x0F) << 8) | ay_registers[0]);
    tonePeriodA = (float)(ayPeriodA / period_divider_tone_noise);
    if (tonePeriodA == 0) tonePeriodA = 1;

    float ayPeriodB = (((ay_registers[3] & 0x0F) << 8) | ay_registers[2]);
    tonePeriodB = (float)(ayPeriodB / period_divider_tone_noise);
    if (tonePeriodB == 0) tonePeriodB = 1;

    float ayPeriodC = (((ay_registers[5] & 0x0F) << 8) | ay_registers[4]);
    tonePeriodC = (float)(ayPeriodC / period_divider_tone_noise);
    if (tonePeriodC == 0) tonePeriodC = 1;
    
    // NOISE PERIOD (R6)
    uint8_t ayNoisePeriod = ay_registers[6] & 0x1F;
    noisePeriod = (uint16_t)(ayNoisePeriod / period_divider_tone_noise);
    if (noisePeriod == 0) noisePeriod = 1;

    // ENVELOPE PERIOD (R11-R12) and SHAPE (R13)
    uint16_t newAyEnvelopePeriod = (((uint16_t)ay_registers[12] << 8) | ay_registers[11]);
    uint8_t newEnvelopeShape = ay_registers[13] & 0x0F;

    static uint16_t oldAyEnvelopePeriod = 0;
    static uint8_t oldEnvelopeShape = 0xFF;

    if (newAyEnvelopePeriod != oldAyEnvelopePeriod || newEnvelopeShape != oldEnvelopeShape) {
        envelopePeriod = (uint16_t)(newAyEnvelopePeriod / period_divider_envelope_actual);
        if (envelopePeriod == 0) envelopePeriod = 1;

        envelopeShape = newEnvelopeShape;
        resetEnvelope(); // Reset envelope state if period or shape changed

        oldAyEnvelopePeriod = newAyEnvelopePeriod;
        oldEnvelopeShape = newEnvelopeShape;
    } else {
        envelopePeriod = (uint16_t)(newAyEnvelopePeriod / period_divider_envelope_actual);
        if (envelopePeriod == 0) envelopePeriod = 1;
    }

    // MIXER (R7) - 0 = enabled, 1 = disabled
    channelAToneEnabled = !((ay_registers[7] >> 0) & 0b1);
    channelBToneEnabled = !((ay_registers[7] >> 1) & 0b1);
    channelCToneEnabled = !((ay_registers[7] >> 2) & 0b1);
    
    channelANoiseEnabled = !((ay_registers[7] >> 3) & 0b1);
    channelBNoiseEnabled = !((ay_registers[7] >> 4) & 0b1);
    channelCNoiseEnabled = !((ay_registers[7] >> 5) & 0b1);
}

void AY_SetReg(uint8_t reg, uint8_t value){
    ay_registers[reg] = value;
    ay_emulator_apply_register_changes();    
}

// Rutina de Servicio de Interrupción (ISR) para la generación de audio
uint16_t ay_audio_isr() {
    // --- Generación de Ruido ---
    noiseCounter++;
    if (noiseCounter >= noisePeriod) {
        noiseCounter = 0;
        // LFSR (Linear Feedback Shift Register) de 17 bits para el ruido AY
        if (((noise_lfsr >> 0) & 1) ^ ((noise_lfsr >> 3) & 1)) {
            noise_lfsr = (noise_lfsr >> 1) | (1 << 16);
        } else {
            noise_lfsr = noise_lfsr >> 1;
        }
        noiseOutput = noise_lfsr & 1;
    }
    
    // --- Generación de Tonos ---
    toneCounterA++;
    if (toneCounterA >= tonePeriodA) { toneCounterA -= tonePeriodA; toneOutputA ^= 1; }
    toneCounterB++;
    if (toneCounterB >= tonePeriodB) { toneCounterB -= tonePeriodB; toneOutputB ^= 1; }
    toneCounterC++;
    if (toneCounterC >= tonePeriodC) { toneCounterC -= tonePeriodC; toneOutputC ^= 1; }

    // --- Generación de Envolvente ---
    if (!envelopeHold) {
        envelopeCounter++;
        if (envelopeCounter >= envelopePeriod) {
            envelopeCounter = 0;
            bool is_hold = (envelopeShape & ENVELOPE_R13_HOLD);
            bool is_alternate = (envelopeShape & ENVELOPE_R13_ALTERNATE);
            bool is_attack = (envelopeShape & ENVELOPE_R13_ATTACK);
            bool is_continue = (envelopeShape & ENVELOPE_R13_CONTINUE);
            
            if (currentEnvelopeDirectionIsAttack) { // Ataque (0 -> 15)
                envelopePhase++;
                if (envelopePhase > 15) {
                    if (is_continue) {
                        if (is_alternate) { currentEnvelopeDirectionIsAttack = false; envelopePhase = 14; }
                        else { envelopePhase = 0; }
                    } else { 
                        envelopePhase = 15;
                        if (is_hold) envelopeHold = true;
                    }
                }
            } else { // Decaimiento (15 -> 0)
                if (envelopePhase == 0) {
                    if (is_continue) {
                        if (is_alternate) { currentEnvelopeDirectionIsAttack = true; envelopePhase = 1; }
                        else { envelopePhase = 15; }
                    } else {
                        envelopePhase = 0;
                        if (is_hold) envelopeHold = true;
                    }
                } else {
                    envelopePhase--;
                }
            }
        }
    }
    
    // --- Mezcla de Canales ---
    uint8_t outA = (channelAToneEnabled ? toneOutputA : 1) & (channelANoiseEnabled ? noiseOutput : 1);
    uint8_t outB = (channelBToneEnabled ? toneOutputB : 1) & (channelBNoiseEnabled ? noiseOutput : 1);
    uint8_t outC = (channelCToneEnabled ? toneOutputC : 1) & (channelCNoiseEnabled ? noiseOutput : 1);

    // --- Cálculo de Volumen ---
    uint16_t amplitudeA_final;
    if (ay_registers[8] & 0x10) { amplitudeA_final = AY_VOLUME_TABLE[envelopePhase]; }
    else { amplitudeA_final = AY_VOLUME_TABLE[ay_registers[8] & 0x0F]; }

    uint16_t amplitudeB_final;
    if (ay_registers[9] & 0x10) { amplitudeB_final = AY_VOLUME_TABLE[envelopePhase]; }
    else { amplitudeB_final = AY_VOLUME_TABLE[ay_registers[9] & 0x0F]; }

    uint16_t amplitudeC_final;
    if (ay_registers[10] & 0x10) { amplitudeC_final = AY_VOLUME_TABLE[envelopePhase]; }
    else { amplitudeC_final = AY_VOLUME_TABLE[ay_registers[10] & 0x0F]; }

    // --- Mezcla Final ---
    long total_output_raw = 0;
    if (outA) total_output_raw += amplitudeA_final;
    if (outB) total_output_raw += amplitudeB_final;
    if (outC) total_output_raw += amplitudeC_final;

    uint16_t final_sample = (uint16_t)(total_output_raw * master_volume_factor);

    // --- Prevención de Recorte (Clipping) ---
    if (final_sample > 4095) { final_sample = 4095; }
    return final_sample;
}

// Inicialización del emulador AY
void ay_emulator_init(void) {
    log_2("ℹ️ Initializing AY emulator...");

    // Configuración inicial de los Registros AY a un estado silente
    ay_registers[7] = 0b00111111; // Todos los generadores de tono y ruido deshabilitados
    ay_registers[8] = 0;         // Volumen A = 0
    ay_registers[9] = 0;         // Volumen B = 0
    ay_registers[10] = 0;        // Volumen C = 0

    // Periodo de tono por defecto (A440), aunque estará silenciado
    uint16_t periodA_default = ay_emulator_get_period_from_freq(440);
    ay_registers[0] = periodA_default & 0xFF;
    ay_registers[1] = (periodA_default >> 8) & 0x0F;

    // Periodo y forma de envolvente por defecto
    uint16_t env_period_ay = 1000;
    ay_registers[11] = env_period_ay & 0xFF;
    ay_registers[12] = (env_period_ay >> 8) & 0xFF;
    ay_registers[13] = 0x00; // Forma de envolvente por defecto (sierra descendente)
    
    // Aplica los valores iniciales a las variables internas del emulador y resetea la envolvente
    ay_emulator_apply_register_changes(); 

    // Configuración del temporizador para la ISR de audio
    log_2("✅ AY emulator started.");
}