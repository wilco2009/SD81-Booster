#include <stdint.h>
#include "GLOBALS.h"
#include "PEG.h"
#include "ay_emulator.h"

bool playing_PEG[NUM_THREADS] = {false,false,false};

uint8_t PEG_addr[NUM_THREADS] = {0,0,0};

uint16_t PEG_mem[256];

uint8_t PEG_pc[NUM_THREADS];

uint32_t ti_PEG[NUM_THREADS];

uint16_t cmd;

uint16_t V[16][NUM_THREADS];
uint8_t carry[NUM_THREADS] = {0,0,0};

inline void exec_first_group(uint8_t i){
  uint8_t value = cmd & 0xff;
  uint8_t n1 =  (cmd >> 8) & 0x0f;
  uint32_t r;
  uint16_t rl;
  switch (cmd & 0b0011000000000000){
    case 0b0000000000000000:          // LD REG,VAL
//      AY_SetReg(n1, value);
        ay_registers[n1] = value;
        ay_emulator_apply_register_changes();
      log_3("PEG: Addr: %hu THREAD: %hu LD R%hu,%hu (%hu)", PEG_pc[i], i, n1, value, ay_registers[n1]);
      break;
    case 0b0001000000000000:          // ADD REG,VAL
//      rl = (AY_reg[n1]+value) & 0xff;
      rl = (ay_registers[n1]+value) & 0xff;    
      carry[i] = (rl >> 16) & 0x00000001;
      log_3("PEG: Addr: %hu THREAD: %hu ADD R%hu,%hu (%hu->%hu) carry=%hu", PEG_pc[i], i, n1, value, ay_registers[n1], rl, carry[i]);
//      AY_SetReg(n1, rl);
        ay_registers[n1] = rl;
        ay_emulator_apply_register_changes();
      break;
    case 0b0010000000000000:          // LD VAR,VAL
      V[n1][i] = value;
      log_3("PEG: Addr: %hu THREAD: %hu LD V%hu,%hu (%u)", PEG_pc[i], i, n1, value, V[n1][i]);
      break;
    case 0b0011000000000000:          // ADD VAR,VAL
      r = (uint32_t) V[n1][i]+value;
      rl = r & 0xffff;
      carry[i] = (r >> 16) & 0x00000001;
      log_3("PEG: Addr: %hu THREAD: %hu ADD V%u,%hu (%u->%u) carry=%hu", PEG_pc[i], i, n1, value, V[n1][i], rl, carry[i]);
      V[n1][i] = rl;
      break;
    default:
      playing_PEG[i] = false;
      log_0("PEG (%04X) group 1 instruction not supported", cmd);
  }
}

inline void exec_second_group(uint8_t i){
  uint32_t r;
  uint8_t n1;
  uint8_t n2;
  uint8_t n3;
  uint16_t rl;
  uint16_t rh;
  switch (cmd & 0b0111000000000000) {
    case 0b0101000000000000:                         // MUL VAR,VAL
      n1 = (cmd & 0x0f00)>>8;
      n2 = cmd & 0xff;
      r = (uint32_t) V[n1][i]*n2;
      n3 = (n1+1)&0x0f;
      rl = r & 0xffff;
      rh = r >> 16;
      log_3("PEG: Addr: %hu THREAD: %hu MUL V%u,%hu (%u->%u) V%hu=%u", PEG_pc[i], i, n1, n2, V[n1][i], rl, n3, rh);
      V[n1][i] = rl;                  // low byte
      V[n3][i] = rh;  // high byte
      break;
    case 0b0110000000000000:                         // DIV VAR,VAL
      n1 = (cmd & 0x0f00)>>8;
      n2 = cmd & 0xff;
      n3  =(n1+1)&0x0f;
      rl = V[n1][i] / n2;
      rh = V[n1][i] % n2;
      log_3("PEG: Addr: %hu THREAD: %hu DIV V%u,%hu (%u->%u) V%hu=%u", PEG_pc[i], i, n1, n2, V[n1][i], rl, n3, rh);
      V[n1][i] = rl;           // result 
      V[n3][i] = rh;  // remainder
      break;
    case 0b0111000000000000:                         // SUB VAR,VAL
      n1 = (cmd & 0x0f00)>>8;
      n2 = cmd & 0xff;
      r = V[n1][i] - n2;
      rl = r & 0xffff;
      carry[i] = (r >> 16) & 0x00000001;
      log_3("PEG: Addr: %hu THREAD: %hu SUB V%u,%hu (%u->%u) carry=%hu", PEG_pc[i], i, n1, n2, V[n1][i], r, carry[i]);
      V[n1][i] = rl;
      break;
    default:
      n2 = cmd & 0x000f;
      n1 = (cmd & 0x00f0) >> 4;
      switch (cmd & 0b1111111100000000){
        case 0b0100000000000000:          // LD REG,REG
//          AY_SetReg(n1,AY_reg[n2]);
          ay_registers[n1] = ay_registers[n2];
          ay_emulator_apply_register_changes();
          log_3("PEG: Addr: %hu THREAD: %hu LD R%hu,R%hu (%hu)", PEG_pc[i], i, n1, n2, ay_registers[n1]);
          break;
        case 0b0100000100000000:          // LD REG,VAR
//          AY_SetReg(n1,(uint8_t) V[n2][i]);
        ay_registers[n1] = (uint8_t) V[n2][i];
        ay_emulator_apply_register_changes();
          log_3("PEG: Addr: %hu THREAD: %hu LD R%hu,V%hu (%hu)", PEG_pc[i], i, n1, n2, ay_registers[n1]);
          break;
        case 0b0100001000000000:          // LD VAR,REG
//          V[n1][i] = AY_reg[n2];
          V[n1][i] = ay_registers[n2];
          log_3("PEG: Addr: %hu THREAD: %hu LD V%hu,R%hu (%u)", PEG_pc[i], i, n1, n2, V[n1][i]);
          break;
        case 0b0100001100000000:          // LD VAR,VAR
          V[n1][i] = V[n2][i];
          log_3("PEG: Addr: %hu THREAD: %hu LD V%hu,V%hu (%u)", PEG_pc[i], i, n1, n2, V[n1][i]);
          break;
        case 0b0100010000000000:          // ADD VAR,VAR
          r = (uint32_t) V[n1][i] + V[n2][i];
          rl = r & 0xffff;
          carry[i] = (r >> 16) & 0x00000001;
          log_3("PEG: Addr: %hu THREAD: %hu ADD V%u,V%hu (%u->%u) carry=%hu", PEG_pc[i], i, n1, n2, V[n1][i], rl, carry[i]);
          V[n1][i] = rl;
          break;
        case 0b0100010100000000:          // SUB VAR,VAR
          r = V[n1][i] - V[n2][i];
          rl = r & 0xffff;
          carry[i] = (r >> 16) & 0x00000001;
          log_3("PEG: Addr: %hu THREAD: %hu SUB V%u,V%hu (%u->%u) carry=%hu", PEG_pc[i], i, n1, n2, V[n1][i], rl, carry[i]);
          V[n1][i] = rl;
        case 0b0100011000000000:          // ADC VAR,VAR
          r = (uint32_t) V[n1][i] + V[n2][i] + carry[i];
          rl = r & 0xffff;
          carry[i] = (r >> 16) & 0x01;
          log_3("PEG: Addr: %hu THREAD: %hu ADC V%u,V%hu (%u->%u) carry=%hu", PEG_pc[i], i, n1, n2, V[n1][i], rl, carry[i]);
          V[n1][i] = rl;
          break;
        case 0b0100011100000000:          // SBC VAR,VAR
          r = V[n1][i] - V[n2][i] - carry[i];
          rl = r & 0xffff;
          carry[i] = (r >> 16) & 0x00000001;
          log_3("PEG: Addr: %hu THREAD: %hu SBC V%u,V%hu (%u->%u) carry=%hu", PEG_pc[i], i, n1, n2, V[n1][i], rl, carry[i]);
          V[n1][i] = rl;
          break;
        case 0b0100100000000000:          //  NOT VAR,VAR
          rl = ~V[n2][i];
          log_3("PEG: Addr: %hu THREAD: %hu NOT V%u,V%hu (%u->%u)", PEG_pc[i], i, n1, n2, V[n1][i], rl);
          V[n1][i] = rl;
          break;
        case 0b0100100100000000:          //  AND VAR,VAR
          rl = V[n1][i] & V[n2][i];
          log_3("PEG: Addr: %hu THREAD: %hu AND V%u,V%hu (%u->%u)", PEG_pc[i], i, n1, n2, V[n1][i], rl);
          V[n1][i] = rl;
          break;
        case 0b0100101000000000:          //  OR VAR,VAR
          rl = V[n1][i] | V[n2][i];
          log_3("PEG: Addr: %hu THREAD: %hu OR V%u,V%hu (%u->%u)", PEG_pc[i], i, n1, n2, V[n1][i], rl);
          V[n1][i] = V[n2][i];
          break;
        case 0b0100101100000000:          //  XOR VAR,VAR
          rl = V[n1][i] ^ V[n2][i];
          log_3("PEG: Addr: %hu THREAD: %hu XOR V%u,V%hu (%u->%u)", PEG_pc[i], i, n1, n2, V[n1][i], rl);
          V[n1][i] = rl;
          break;
        case 0b0100110000000000:          //  MUL VAR,VAR
          r = (uint32_t) V[n1][i] * (uint32_t) V[n2][i];
          rl = r & 0xffff;
          rh = r >>16;
          n3 = (n1+1)&0x0f;
          log_3("PEG: Addr: %hu THREAD: %hu MUL V%u,V%hu (%u->%u) V%hu=%u", PEG_pc[i], i, n1, n2, V[n1][i], rl, n3, rh);
          V[n1][i] = rl;                  // low byte
          V[n3][i] = rh;  // high byte
          break;
        case 0b0100110100000000:          // DIV VAR,VAR
          n3 = (n1+1)&0x0f;
          rl = V[n1][i]/V[n2][i];
          rh = V[n1][i]%V[n2][i];
          log_3("PEG: Addr: %hu THREAD: %hu DIV V%u,V%hu (%u->%u) V%hu=%u", PEG_pc[i], i, n1, n2, V[n1][i], rl, n3, rh);
          V[n1][i] = rl;           // result 
          V[n3][i] = rh;  // remainder
          break;
        case 0b0100111000000000:          // SHL VAR,VAL
          rl = V[n1][i] << n2;
          log_3("PEG: Addr: %hu THREAD: %hu SHL V%u,%hu (%u->%u)", PEG_pc[i], i, n1, n2, V[n1][i], rl);
          V[n1][i] = rl;
          break;
        case 0b0100111100000000:          // SHR VAR,VAL
          rl = V[n1][i] >> n2;
          log_3("PEG: Addr: %hu THREAD: %hu SHL V%u,%hu (%u->%u)", PEG_pc[i], i, n1, n2, V[n1][i], rl);
          V[n1][i] = rl;
          break;
        default:
          playing_PEG[i] = false;
          log_3("PEG (%04X) group 2 instruction not supported", cmd);
      }
  }
    
}

inline void exec_third_group(uint8_t i){
  uint8_t cmd_1;    // cmd bytes 7,6,5,4
  uint16_t ti;
  uint8_t n1;
  uint8_t n2;
  uint8_t addr;
  switch (cmd & 0b1111000000000000){
    case 0b1000000000000000:          // DJNZ
      n1 = (cmd >> 8) & 0x0f;
      V[n1][i]--;
      addr = PEG_pc[i] + (cmd & 0xff);
      log_3("PEG: Addr: %hu THREAD: %hu DJNZ (V%hu=%u),%hu ", PEG_pc[i], i, n1,V[n1][i], addr);
      if (V[n1][i] != 0) PEG_pc[i] = addr;
      break;
    case 0b1001000000000000:      // WAIT VAL
      ti = cmd & 0b0000111111111111;
      ti_PEG[i] = millis()+ti;
      log_3("PEG: Addr: %hu THREAD: %hu WAIT %u", PEG_pc[i], i, ti);
      break;    
    default:
      if (cmd == 0b1010000000010000){                                // HALT
        log_0("PEG: HALT Addr: %hu THREAD: %hu ", PEG_pc[i], i);
        playing_PEG[i] = false;
      } else if ((cmd & 0b1111111111110000) == 0b1010000000000000) {  // WAIT VAR
        n1 = cmd & 0x0f;
        ti = V[n1][i];
        ti_PEG[i] = millis()+ti;
        log_3("PEG: Addr: %hu THREAD: %hu WAIT V%u (%u)", PEG_pc[i], i, n1, ti);
      } else if ((cmd & 0b1111111100000000) == 0b1010000100000000) {  // JR
        addr = PEG_pc[i] + (cmd & 0xff);
        log_3("PEG: Addr: %hu THREAD: %hu JR %hu", PEG_pc[i], i, addr);
        PEG_pc[i] = addr;
      } else {
        playing_PEG[i] = false;
        log_0("PEG (%04X) group 3 instruction not supported", cmd);
      }
  }
}

void exec_PEG(){
  uint8_t cmd_0;    
  
  for (int i=0; i<NUM_THREADS; i++){
    if (playing_PEG[i] && (millis()>=ti_PEG[i])){
      cmd = PEG_mem[PEG_pc[i]];
      switch (cmd & 0b1100000000000000) {
        // first group commands
        // LD REG,VAL; ADD REG,VAL; LD VAR,VAL; ADD VAR,VAL
        case 0b0000000000000000:
          exec_first_group(i); 
          break; 
        // second group commands
        // ADD VAR,VAR; SUB VAR,VAR; ADC VAR,VAR; SBC VAR,VAR; NOT VAR; AND VAR; 
        // OR VAR; XOR VAR; MUL VAR,VAR; DIV VAR,VAR; SHL VAR,VAL; SHR VAR,VAL; MUL VAR,VAL; DIV VAR,VAL
        case 0b0100000000000000:       
          exec_second_group(i); 
          break; 
        // third group commands
        // WAIT VAR; WAIT CONST; DJNZ; JR
        case 0b1000000000000000: 
        case 0b1100000000000000: 
          exec_third_group(i); 
          break;
      }
      PEG_pc[i] ++;
    }
  }
}
