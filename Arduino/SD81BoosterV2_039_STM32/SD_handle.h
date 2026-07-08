#ifndef SD_handle_H
#define SD_handle_H

#include <Arduino.h>
#include <stdint.h>
#include "PINS.h"
#include <SdFat.h>
#include "GLOBALS.h"

#define CHECK_ROM


#define _DIR_START    0
#define _DIR_NAME     1
#define _FIRST_DOT    2
#define _SECOND_DOT   3
#define _REMOVE_DIRS  4

#define _DIR_UNDER_ROOT   -1
#define _NOT_A_DIR        -2
#define _NOT_EXISTS       -3

const int8_t DISABLE_CS_PIN = -1;
const uint8_t SD_CS_PIN = SS_SD;

extern bool SDOK;
extern SdFs sd;

boolean SD_Init(void);
void check_SD();
boolean absolute_dir(char* fname);
boolean ext_present(char* fname);
int remove_dotdirs(char* dest);
int complete_dir(char* dest,char* source);
char* getfname(char* filename);
void split_fname(char* source, char* dir, char* fname);
char *get_filename_ext(const char *filename);
void complete_fname(char* dest,char* source);
int load_ROM(char* rom_file); // returns 0 ok, -1 mem error, -2 file error 

#endif