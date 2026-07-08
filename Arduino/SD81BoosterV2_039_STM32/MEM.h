
#ifndef EXTMEM_H
#define EXTMEM_H
//static volatile uint8_t *extmem = reinterpret_cast<volatile uint8_t*>(0x8000);
#include <stdint.h>

extern uint8_t* extmem;

void mem_write_lapse(void);
void mem_read_lapse(void);
static void mem_testOk();
static void mem_testFail();
void enableMem();
void disableMem();
void write_sram(uint16_t addr, uint8_t d);
uint8_t read_sram(uint16_t addr);
bool mem_test();
void do_mem_Test ();

extern const int ADDRESS_PINS[];
extern const int DATA_PINS_OUT[];
extern const int DATA_PINS_IN[];

#endif