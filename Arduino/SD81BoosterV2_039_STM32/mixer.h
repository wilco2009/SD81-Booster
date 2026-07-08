#ifndef MIXER_H
#define MIXER_H

#include <Arduino.h>     // Necesario para tipos como uint8_t, etc.
#include <HardwareTimer.h> // Para la ISR
void mixer_init(TIM_TypeDef *timer_instance); 

#endif