#ifndef PEG_H
#define PEG_H

#define NUM_THREADS 3

extern uint8_t PEG_addr[NUM_THREADS];
extern uint16_t PEG_mem[256];
extern uint8_t PEG_pc[NUM_THREADS];
extern bool playing_PEG[NUM_THREADS];


void exec_PEG();

#endif