#ifndef COMMS_H
#define COMMS_H
#include <stdint.h>

#define CMD_BITS 4

void write_LATCH_O(uint8_t d);
uint8_t read_LATCH_B(void);
void idle_tasks();
void set_clock(bool c);
void reset_commands();
void ToggleClock();
uint8_t GetByteFromZ80();
void SendByteToZ80(uint8_t byte);
uint8_t GetByteFromZ80_IT();
void SendByteToZ80_IT(uint8_t byte);
void _rst_ctrl_reg(uint8_t status);
void _rst_data_reg(uint8_t status);
void get_ctrl_reg(void);
void get_data_reg(void);
void read_data();
void jmpfar();

#define cfgcmd_JOYSTICK   0
#define cfgcmd_MC45       1
#define cfgcmd_MODE48K    2
#define cfgcmd_QSEN       3
#define cfgcmd_FULLPAG    4
#define cfgcmd_128CHARS   5
#define cfgcmd_256CHARS   6
#define cfgcmd_ROMLOCK    7
#define cfgcmd_DBGPAUSE   8   // depurador: pausa (la FPGA reacciona al cambio)
#define cfgcmd_DBGLOADED  9   // depurador: monitor cargado en la pagina 63 (y armado)
#define cfgcmd_DBGPOKE    10  // depurador: el monitor escribe como el programa (POKEs y BRAM)
#define cfgcmd_DBGDIRTY   11  // depurador: borrar las paginas escritas (1 y despues 0)


void send_debug_params(uint16_t pixel_cnt, uint16_t line_cnt);
void send_config(void);
bool send_joycfg(char* config);// UP DOWN LEFT RIGHT FIRE
void send_bit_config(uint8_t command, uint8_t value);
void rst_config(void);
uint8_t cfg_value(uint8_t command);   // ultimo valor mandado con send_bit_config

const int OUTPUT_LATCH[] = {PD0, PD1, PD2, PD3, PD4, PD5, PD6, PD7}; 
const int INPUT_LATCH[] = {PD8, PD9, PD10, PD11, PD12, PD13, PD14, PD15}; 

extern uint8_t joy_cfg[5][2];

#endif