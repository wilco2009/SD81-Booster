#ifndef VGM_H
#define VGM_H
#include <stdint.h>

extern SdFile VGMFile;
extern bool playing_VGM;
extern uint8_t VGM_mode;
extern uint32_t ti_VGM;

uint8_t openVGM(char* s);
void stopVGM();
void read_next_VGM_command();

#endif