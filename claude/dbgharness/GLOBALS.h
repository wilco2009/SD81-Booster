#pragma once
#include <stdint.h>
#define clMAGENTA 0xff00ff
#define clYELLOW 0xffff00
#define clCYAN 0x00ffff
#define MAX_FILENAME_LEN 100
extern uint8_t nQS_en;
void set_status_LED(uint32_t RGB);
void set_status_led_ok();
void set_blinking(uint32_t RGB, float frequency);
void set_blinking_off();
extern char current_dir[];
