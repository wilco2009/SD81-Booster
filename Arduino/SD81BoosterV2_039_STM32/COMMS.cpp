#include <Arduino.h>
#include "COMMS.h"
#include "PINS.h"
#include "GLOBALS.h"
#include "WAV.h"
#include "PEG.h"
#include "VGM.h"

////#define  TRACE_CLOCK(s) Serial.print(s); Serial.println(clock);
//#define  TRACE_CLOCK(s)
//

uint8_t joy_cfg[5][2] = {{4,3},{4,4},{3,4},{4,2},{7,0}};

void sendBit(uint8_t bit){
  digitalWrite(CFG_CLK,LOW);
  if (bit==0)
      digitalWrite(CFG_DATA,LOW);
  else
      digitalWrite(CFG_DATA,HIGH);
  digitalWrite(CFG_CLK,HIGH);
  delayMicroseconds(10);
}

void send_cmd_code(uint8_t value){
  for (int j=0;j<CMD_BITS;j++)
    sendBit(bitRead(value,j));
}

void rst_config(){
  digitalWrite(CFG_RESET,HIGH);
  delayMicroseconds(10);
  digitalWrite(CFG_RESET,LOW);
  delayMicroseconds(10);
  digitalWrite(CFG_RESET,HIGH);
}

void send_joycfg(){
  send_cmd_code(cfgcmd_JOYSTICK);
  //send joystick config
  for (int i=0;i<5;i++){
    for (int j=0;j<3;j++)
      sendBit(bitRead(joy_cfg[i][0],j));
    for (int j=0;j<3;j++)
      sendBit(bitRead(joy_cfg[i][1],j));
  }
  rst_config();
}
void send_bit_config(uint8_t command, uint8_t value){
  send_cmd_code(command);
  sendBit(value);
  rst_config();
}

uint8_t keyb_coords[64][2] = {
	/* */{7,0},/*-*/{8,8},/*-*/{8,8},/*-*/{8,8},/*-*/{8,8},/*-*/{8,8},/*-*/{8,8},/*-*/{8,8},  //0..7
	/*-*/{8,8},/*-*/{8,8},/*-*/{8,8},/*-*/{8,8},/*-*/{8,8},/*-*/{8,8},/*-*/{8,8},/*-*/{8,8},  //8..15
	/*-*/{8,8},/*-*/{8,8},/*-*/{8,8},/*<*/{6,0},/*-*/{8,8},/*-*/{8,8},/*-*/{8,8},/***/{0,0},  //16..23
	/*-*/{8,8},/*-*/{8,8},/*-*/{8,8},/*-*/{8,8},/*0*/{4,0},/*1*/{3,0},/*2*/{3,1},/*3*/{3,2},  //24..31
	/*4*/{3,3},/*5*/{3,4},/*6*/{4,4},/*7*/{4,3},/*8*/{4,2},/*9*/{4,1},/*A*/{1,0},/*B*/{7,4},  //32..39
	/*C*/{0,3},/*D*/{1,2},/*E*/{2,2},/*F*/{1,3},/*G*/{1,4},/*H*/{6,4},/*I*/{5,2},/*J*/{6,3},  //40..47
	/*K*/{6,2},/*L*/{6,1},/*M*/{7,2},/*N*/{7,3},/*O*/{5,1},/*P*/{5,0},/*Q*/{2,0},/*R*/{2,3},  //48..55
	/*S*/{1,1},/*T*/{2,4},/*U*/{5,3},/*V*/{0,4},/*W*/{2,1},/*X*/{0,2},/*Y*/{5,4},/*Z*/{0,1}}; //56..63

bool check_joycfg(char* config){
    for (int i=0; i<5; i++){
      if ((config[i]>=64) || (keyb_coords[config[i]][0]>=8))
        return false;
    }
    return true;
}
  
  // UP DOWN LEFT RIGHT FIRE
bool send_joycfg(char* config){
  if (check_joycfg(config)){
    for (int i=0; i<5; i++){
      joy_cfg[i][0]=keyb_coords[config[i]][0];
      joy_cfg[i][1]=keyb_coords[config[i]][1];
    }
    send_joycfg();
    return true;
  } else return false;
}

// void send_debug_params(uint16_t pixel_cnt, uint16_t line_cnt){
//   rst_config();
//   send_cmd_code(3);
//   for (int j=0;j<9;j++)
//     sendBit(bitRead(pixel_cnt,j));
//   for (int j=0;j<9;j++)
//     sendBit(bitRead(line_cnt,j));
//   rst_config();
// }


void send_config(void){
  // start with a reset config
  rst_config();

  send_joycfg();
}

void write_LATCH_O(uint8_t d){
  ioLatch->ODR = d;
  // digitalWrite(LE_IL,HIGH);  
}

uint8_t read_LATCH_B(void){
  digitalWrite(nOE_OL,LOW);
  return ioLatch ->IDR >>8;
}

void idle_tasks(){
  if (playing_wav){ 
    if (load_wav_buffer_pending >= 0) load_wav_buffer(load_wav_buffer_pending);
  }
  
  if (playing_VGM) {
    if ((millis() >= ti_VGM))  {
      troggle_SDLed();
      read_next_VGM_command();
    } // else waiting.......
  }

  if (playing_PEG) {
    exec_PEG();
  }
}


void set_clock(bool c){
  comm_clk = c;
  digitalWrite(CTRL_CLK, comm_clk);
}

void reset_commands(){
  command_active = CMD_IDLE;
  data_ready = false;
}

void ToggleClock()
{
  set_clock(!comm_clk);
}

uint8_t GetByteFromZ80()
{
  do /*nothing*/; while (!digitalRead(GET_DATA_REG));
  digitalWrite(RST_DATA_REG, LOW);
  lapse;
  digitalWrite(RST_DATA_REG, HIGH);
  return read_LATCH_B();
}

void SendByteToZ80(uint8_t byte)
{
  write_LATCH_O(byte);
  ToggleClock();
  do /*nothing*/; while (!digitalRead(GET_DATA_REG));
  digitalWrite(RST_DATA_REG, LOW);
  lapse;
  digitalWrite(RST_DATA_REG, HIGH);
  delayMicroseconds(10);
//  delay(1);
}

uint8_t GetByteFromZ80_IT()
{
  do /*nothing*/idle_tasks(); while (!digitalRead(GET_DATA_REG));
  digitalWrite(RST_DATA_REG, LOW);
  lapse;
  digitalWrite(RST_DATA_REG, HIGH);
  return read_LATCH_B();
}

void SendByteToZ80_IT(uint8_t byte)
{
  write_LATCH_O(byte);
  ToggleClock();
  do /*nothing*/idle_tasks(); while (!digitalRead(GET_DATA_REG));
  digitalWrite(RST_DATA_REG, LOW);
  lapse;
  digitalWrite(RST_DATA_REG, HIGH);
}


void _rst_ctrl_reg(uint8_t status) {
  rst_ctrl_reg=status;
  digitalWrite(RST_CTRL_REG, status);
}
  
void _rst_data_reg(uint8_t status) {
  rst_data_reg=status;
  digitalWrite(RST_DATA_REG, status);
}

void get_ctrl_reg(void){
   _rst_ctrl_reg(LOW);
  reset_commands();
  // cerrar los handles de acceso aleatorio (CP/M) abiertos
  for (int i=0; i<4; i++){
    if (f_opened[i]){ f_handle[i].close(); f_opened[i]=false; }
  }
  reseted = true;
  initialised = false;
  Serial.println("RESET");
  delay(10);
  _rst_ctrl_reg(HIGH);
  ToggleClock();
  // GPIOR0 |= 2; // software reset
  // noInterrupts();
  // jmpfar();
  //__asm("sei\n%~jmp %x0" :: "i" (jmpfar));
}

void get_data_reg(void){
  data_in = read_LATCH_B();
  _rst_data_reg(LOW);
  data_ready = true;
 _rst_data_reg(HIGH);
  if (command_active==CMD_IDLE){
    command_active = data_in;
  }
}

void read_data(){
//  if (digitalRead(GET_CTRL_REG)) {get_ctrl_reg();}
  if (digitalRead(GET_DATA_REG)) {get_data_reg();}
}
