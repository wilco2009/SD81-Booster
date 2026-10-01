#pragma once
#include <stdint.h>
#define clMAGENTA 0xff00ff
#define clYELLOW 0xffff00
extern uint8_t nQS_en;
void set_status_LED(uint32_t RGB);
void set_status_led_ok();
