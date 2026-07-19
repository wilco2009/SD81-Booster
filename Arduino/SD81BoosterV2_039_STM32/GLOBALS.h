#ifndef GLOBALS_H
#define GLOBALS_H

#include <SdFat.h>
#include "PINS.h"

//#define reversed
 #define LOG0
 #define LOG1
 #define LOG2
 //#define LOG3

#ifndef SERIAL_RX_BUFFER_SIZE
#define SERIAL_RX_BUFFER_SIZE 512
#endif /* SERIAL_RX_BUFFER_SIZE */

// #define COMMON_ANODE

#define clRED     0xff0000
#define clGREEN   0x00ff00
#define clBLUE    0x0000ff
#define clCYAN    0x00ffff
#define clMAGENTA 0xff00ff
#define clYELLOW  0xffff00
#define clWHITE   0xffffff
#define clORANGE  0xffa500
#define clPINK    0xf08080
#define clBLACK   0x000000

#define CHECK_ROM

#define VERSION 0x20
#define CPLD_NAME "/SD81CPLD"
#define MAX_FILENAME_LEN 100
#define MAX_PARAM_LEN 100

#define BUFFSIZE 512
#define MAX_FILE_ARRAY BUFFSIZE/2

#define GET_NEXT_CHAR 13
#define EOT 0x6f

#define CMD_NOP       0
#define CMD_VER       1
#define CMD_DIR       2
#define CMD_CD        3
#define CMD_IDLE    255

#define END_BYTE    255

#define LED_OFF HIGH
#define LED_ON  LOW

#define SERIAL_SPEED 115200

#define lapse for(volatile uint32_t i=0;i<10;i++)
#define getlapse for(volatile uint32_t i=0;i<100;i++)
#define sendlapse for(volatile uint32_t i=0;i<100;i++)

#define REVERSE_BITS8(x) ( \
    (((x) & 0x80) >> 7) | \
    (((x) & 0x40) >> 5) | \
    (((x) & 0x20) >> 3) | \
    (((x) & 0x10) >> 1) | \
    (((x) & 0x08) << 1) | \
    (((x) & 0x04) << 3) | \
    (((x) & 0x02) << 5) | \
    (((x) & 0x01) << 7) )

extern uint32_t DAC_freq;

extern uint8_t GPIOR0; // Esta variable tiene que ser sustituida por alguna forma de preservar el valor después dee un reset

extern uint8_t copy_buffer[BUFFSIZE];
extern char array_dirname[MAX_FILENAME_LEN];
extern uint16_t* file_array; //[MAX_FILE_ARRAY];
extern uint16_t index_array;
extern uint8_t sys_vars[128];
extern uint16_t row_array;
extern uint32_t serial_ini;
extern boolean serial_end;
extern uint8_t nQS_en;

extern boolean T81_dir;

extern uint32_t sdtotal;
extern bool sdled;
extern uint32_t clk_count;
extern volatile bool comm_clk;
extern volatile uint8_t command_active;
extern uint8_t param_len;
extern char params[MAX_PARAM_LEN];
extern char tmp[MAX_FILENAME_LEN];

extern uint8_t file_counter;

extern volatile uint8_t data_in;
extern volatile bool data_ready;

extern volatile bool rst_ctrl_reg;
extern volatile bool rst_data_reg;
extern bool reseted;
extern bool initialised;

extern FsFile dir;
extern FsFile Sfile;
extern FsFile Dfile;
extern FsFile TmpFile;
extern FsFile f_handle[4];
extern bool f_opened[4];

extern const char asc_to_asc81 [];

extern const char asc81_to_ascii [];
extern char current_dir[MAX_FILENAME_LEN];
extern char S[100];
extern bool debug_clock;

uint8_t inverse_video(uint8_t ch);
uint8_t hexval(char x);
void set_SDLed(bool state);
void troggle_SDLed();
void set_status_LED(uint8_t R,uint8_t G, uint8_t B);
void set_status_LED(uint32_t RGB);
void set_status_led_ok();
void set_blinking(uint32_t RGB, float frequency);
void set_blinking_off();

void LED_error(uint32_t RGB1, uint32_t RGB2);
void LED_error(uint32_t RGB);

void split_two_params(char* params, char* param1, char* param2);
void hex_to_str(char* v, char* params);
void upStr(char* s);

#if defined(LOG0)||defined(LOG1)||defined(LOG2)||defined(LOG3)
void log_debug(const char* fmt /*PROGMEM*/, ...);
#endif

#ifdef LOG0
#define log_0(x, ...) log_debug(PSTR(x),##__VA_ARGS__)
#else
#define log_0(...)
#endif

#ifdef LOG1
#define log_1(x, ...) log_debug(PSTR(x),##__VA_ARGS__)
#else
#define log_1(...)
#endif

#ifdef LOG2
#define log_2(x, ...) log_debug(PSTR(x),##__VA_ARGS__)
#else
#define log_2(...)
#endif

#ifdef LOG3
#define log_3(x, ...) log_debug(PSTR(x), ##__VA_ARGS__)
#else
#define log_3(...)
#endif


void start_reset_Z80();

void end_reset_Z80();
#endif