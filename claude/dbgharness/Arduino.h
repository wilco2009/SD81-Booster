#pragma once
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
typedef uint8_t byte;
struct SerialC { void println(const char* s); void print(const char* s); int printf(const char* f, ...); };
extern SerialC Serial;
unsigned long millis();
int digitalRead(int);
#define PD0 0
#define PD1 1
#define PD2 2
#define PD3 3
#define PD4 4
#define PD5 5
#define PD6 6
#define PD7 7
#define PD8 8
#define PD9 9
#define PD10 10
#define PD11 11
#define PD12 12
#define PD13 13
#define PD14 14
#define PD15 15
#define PROGMEM
#define pgm_read_byte_near(p) (*(const uint8_t*)(p))
#define pgm_read_word_near(p) (*(const uint16_t*)(p))
#define pgm_read_ptr(p) (*(void* const*)(p))
#include <algorithm>
using std::min;
