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
// Depurador por hardware: escribe /SYS/DEBUG.BIN en la pagina 63. Hay que
// llamarla con el Z80 y la FPGA en reset y enableMem(). 1 = cargado,
// 0 = no hay fichero (depurador desarmado), -1 = demasiado grande.
#define DEBUG_MONITOR_FILE "/SYS/DEBUG.BIN"
int load_debug_monitor(void);
extern bool debug_monitor_loaded;
// Snapshots del depurador (DEBUGGER.cpp): el fichero que se esta escribiendo
// y la ROM en la SD, para saber si las paginas 0-1 siguen siendo la ROM
bool snapfile_open(const char* path);
bool snapfile_write(const void* data, uint16_t n);
void snapfile_close(void);
bool snapfile_exists(const char* path);
int32_t rom_file_read(uint32_t offset, uint8_t* buf, uint16_t n);  // bytes leidos; -1 sin fichero; con n = 0, el tamano
// El .Z81 que carga el depurador (LOAD *Z81 con el monitor)
bool z81in_open(const char* path);
int z81in_read(void);                       // un byte; -1 al final
bool z81in_seek(uint32_t pos);
uint32_t z81in_pos(void);
void z81in_close(void);

#endif