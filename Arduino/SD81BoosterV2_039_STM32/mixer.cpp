#include "mixer.h"
#include "dac_control.h" // Se sigue necesitando para set_dac_output
#include "ay_emulator.h"
#include "voice_synthesizer.h"
#include "WAV.h"

static HardwareTimer *MyTimLocal = nullptr;

// Rutina de Servicio de Interrupción (ISR) para la generación de audio
void mixer_isr() {
  uint16_t final_sample;
  uint16_t ay_sample = ay_audio_isr();
  uint16_t wav_sample = get_wav_sample();
  uint16_t voice_sample = voice_synthesizer_isr();
  if (!playing_wav)
    final_sample = ay_sample*3/4 + voice_sample/4; 
  else
    final_sample = ay_sample*3/4 + wav_sample*8; 
  set_dac_output(final_sample);
}

void mixer_init(TIM_TypeDef *timer_instance){
    ay_emulator_init(); 
    MyTimLocal = new HardwareTimer(timer_instance);
    MyTimLocal->setOverflow(SAMPLE_RATE, HERTZ_FORMAT);
    MyTimLocal->attachInterrupt(mixer_isr);
    MyTimLocal->resume();
    Serial.println("✅ Mixer started.");

} 

