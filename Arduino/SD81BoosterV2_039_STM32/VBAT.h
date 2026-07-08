#ifndef VBAT_H
#define VBAT_H
#include <stdint.h>

 #define GOOD_BAT_LEVEL 3.0
 #define MIN_BAT_LEVEL 2.5


bool vbat_init(void);
float get_battery_level(void);
uint8_t get_battery_byte(void);

#endif