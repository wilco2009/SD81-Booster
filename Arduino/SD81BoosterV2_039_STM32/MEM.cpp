#include "MEM.h"
#include "GLOBALS.h"

uint8_t* extmem;

const int ADDRESS_PINS[] = {PE0, PE1, PE2, PE3, PE4, PE5, PE6, PE7, PE8, PE9, PE10, PE11, PE12, PE13, PE14,A16b,A17b,A18b}; 
const int DATA_PINS_OUT[] = {PD0, PD1, PD2, PD3, PD4, PD5, PD6, PD7}; 
const int DATA_PINS_IN[] = {PD8, PD9, PD10, PD11, PD12, PD13, PD14, PD15}; 

void mem_write_lapse(void){
  delayMicroseconds(3);
}

void mem_read_lapse(void){
  delayMicroseconds(3);
}

static void mem_testOk() {
//  digitalWrite(LED_BUILTIN, HIGH);
//  delay(500);
//  digitalWrite(LED_BUILTIN, LOW);
//  delay(500);
}

static void mem_testFail() {
  for (uint8_t i=0; i<3; ++i) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(50);
    digitalWrite(LED_BUILTIN, LOW);
    delay(50);
  }
}

void enableMem(){
  log_2("ℹ️ Enabling external memory interface");

  
//  bitSet(XMCRA, SRE);  // enable externalmemory
//  bitSet(XMCRB, XMM0); // release unused pin PC7
//
//  // SRW11=0 SW10=0 (Wait cycles: 0)
//  // SRW11=0 SW10=1 (Wait cycles: 1)
//  // SRW11=1 SW10=0 (Wait cycles: 2)
//  // SRW11=1 SW10=1 (Wait cycles: 2 + 1)
//
//  log_2("1. Wait cycles: 0");
//  bitClear(XMCRA, SRW11);
//  bitClear(XMCRA, SRW10);
//
  for (int i = 0; i < sizeof(ADDRESS_PINS) / sizeof(ADDRESS_PINS[0]); i++) {
      pinMode(ADDRESS_PINS[i], OUTPUT);
  }

  // Ya está configurado el puerto para el datalatch
  // for (int i = 0; i < sizeof(DATA_PINS_OUT) / sizeof(DATA_PINS_OUT[0]); i++) {
  //     pinMode(DATA_PINS_OUT[i], OUTPUT);
  // }
  // for (int i = 0; i < sizeof(DATA_PINS_IN) / sizeof(DATA_PINS_IN[0]); i++) {
  //     pinMode(DATA_PINS_IN[i], INPUT);
  // }
  pinMode(nMEM_OEb,OUTPUT); 
  pinMode(nWRb,OUTPUT); 

  digitalWrite(A16b,LOW);
  digitalWrite(A17b,LOW);
  digitalWrite(A18b,LOW);
  digitalWrite(nMEM_OEb,HIGH);
  digitalWrite(nWRb,HIGH);
}

void disableMem(){
  log_2("ℹ️ Disabling external memory interface");
  for (int i = 0; i < sizeof(ADDRESS_PINS) / sizeof(ADDRESS_PINS[0]); i++) {
      pinMode(ADDRESS_PINS[i], INPUT);
  }

  pinMode(nMEM_OEb,INPUT); 
  pinMode(nWRb,INPUT); 
}

void write_sram(uint16_t addr, uint8_t d){
  Addr->ODR = addr;  
  Data->ODR = d;  

  
  digitalWrite(nMEM_OEb,HIGH);
  digitalWrite(nWRb,LOW);
  mem_write_lapse();
  digitalWrite(nWRb,HIGH);
//  digitalWrite(nMEM_OE,HIGH);
  
}

uint8_t read_sram(uint16_t addr){
  Addr->ODR = addr;  
  digitalWrite(nWRb,HIGH);
  digitalWrite(nMEM_OEb,LOW);
  mem_read_lapse();
  uint8_t d = Data ->IDR >>8;
  digitalWrite(nMEM_OEb,HIGH);
//  digitalWrite(nWRb,HIGH);
  return d; 
}



bool mem_test() {

  char s[100];

  log_2("WRITTING");
  srand(1);
  for (uint16_t i = 0; i<32768; i++) {
    byte v = rand()&0xFF;
      write_sram(i,v);
  }

  log_2("READING");
  srand(1);
  for (uint16_t i = 0; i<32768; i++) {
    byte dummy = read_sram(i);
    byte v =rand()&0xFF;
    if (dummy !=v)
    {
      log_0("BAD! ADDR=%d READ=%d WRITE=%d",0, i, dummy, v);
      return false;
    } 
  }
  log_2("DONE");
  return true;
}


void do_mem_Test () {
  long startTime, stopTime;
  bool res;

  startTime = millis();
  res = mem_test();
  stopTime = millis();
  if (res) {
    //log_1("MEM TEST PASSED, time (ms): %d",stopTime-startTime);
    mem_testOk();
  } else {
    //log_0("MEM TEST FAILED");
    mem_testFail();
  }
};
