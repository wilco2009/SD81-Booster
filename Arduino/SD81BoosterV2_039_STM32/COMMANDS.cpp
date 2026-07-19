#include "COMMANDS.h"
#include <stdint.h>
#include <SdFat.h>
#include <EEPROM.h>
#include "GLOBALS.h"
#include "SD_handle.h"
#include "T81.h"
#include "COMMS.h"
#include "ay_emulator.h"
#include "play_command_parser.h"
#include "voice_synthesizer.h"
#include "transfer.h"
#include "MEM.h"
#include "WILDCARD.h"
#include "VGM.h"
#include "PEG.h"
#include "WAV.h"
#include "VBAT.h"
#include "RTC.h"

#define GET_NEXT_CHAR 13

char param2[MAX_PARAM_LEN];
char param3[MAX_PARAM_LEN];

// COMMAND = 0 OK
void cmd_nop(void){
  ToggleClock();
  command_active = CMD_IDLE;
  reset_commands();
}

// COMMAND = 1 OK
void cmd_ver(void){
//  ToggleClock();
  SendByteToZ80(VERSION); 
  ToggleClock();
  reset_commands();
}

// COMMAND = 2 OK
void cmd_pwd(void){
uint8_t c;
  check_SD();
  set_SDLed(LED_ON);
  ToggleClock();
  
  for (int i=0; current_dir[i]!=0;i++){
    c = GetByteFromZ80_IT();    // GET_CHAR
    if (c!=GET_NEXT_CHAR){
      command_active = c;
      return;
    }
    SendByteToZ80(asc_to_asc81[current_dir[i]]); 
    ToggleClock();       // finish command
  }
  c = GetByteFromZ80_IT();    // GET_CHAR
  if (c!=GET_NEXT_CHAR){
    command_active = c;
    return;
  }
  SendByteToZ80(EOT); // EOT...
  SendByteToZ80(0x00); // ... and Status
  ToggleClock();       // Final clock toggle
  reset_commands();
}

// COMMAND = 3 OK
void cmd_cd(void){
char ss[MAX_FILENAME_LEN];
uint8_t error_code;
char file_name[MAX_FILENAME_LEN];
  T81_dir = false;
  error_code = 0;

  check_SD();
  
  set_SDLed(LED_ON);
  ToggleClock();
  //GET PARAMS
  uint8_t param_len = GetByteFromZ80_IT();
  char ch;
  for (uint8_t i=0; i<param_len; i++){
    ToggleClock();
    ch = GetByteFromZ80_IT();
    params[i] = (char) asc81_to_ascii[ch];
  }
  params[param_len] = 0;

  if (params[0]!='/'){
    complete_dir(tmp,current_dir);
    strcpy(tmp,current_dir);
    strcat(tmp,params);
  } else {
    strcpy(tmp,params);
  }
  complete_dir(ss,tmp);
  if (dir.isOpen()) dir.close();
  for (int i = 0; i<10; i++){
    if (!dir.open(ss)) log_0("error openning %s",ss);
    else break;
    delay(10);
  }
  if (!dir.isOpen()){
    log_0("dir \"%s\" does not exists\r",ss);
    error_code = 1;
    strcpy(ss,current_dir);
  } else if (!dir.isDir()){
    if (ss[strlen(ss)-1]=='/') ss[strlen(ss)-1] = 0;
    char* ext = get_filename_ext(ss);
    upStr(ext);
    if (strcmp(ext,"T81")==0){
      strcat(ss,"/");
      T81_dir = true;
      error_code = 0;
      strcpy(current_dir,ss);
    } else {
      log_0("\"%s\" is not a dir\r",ss);
      error_code = 2;
    }
  } else {
    sprintf_P(tmp,PSTR("current dir = \"%s\"\r"), ss);
    strcpy(current_dir,ss);
  }
    if (dir.isOpen()) dir.close();
  SendByteToZ80(error_code);  // ... and Status
  reset_commands();           // antes del ToggleClock final: ver comentario
                               // en cmd_opendir2 sobre la ventana de carrera
  ToggleClock();               // Final clock toggle
}

// COMMAND = 4 OK
void cmd_del(){
uint8_t error_code;
char s[MAX_FILENAME_LEN];
char file_name[MAX_FILENAME_LEN];
  set_SDLed(LED_ON);
  ToggleClock();
  //GET PARAMS
  check_SD();
  uint8_t param_len = GetByteFromZ80_IT();
  char ch;
  for (uint8_t i=0; i<param_len; i++){
    ToggleClock();
    ch = GetByteFromZ80_IT();
    params[i] = (char) asc81_to_ascii[ch];
  }
  params[param_len] = 0;

  if (T81_dir){
    error_code = 13; // forbiden operation
  } else {
    complete_dir(s,current_dir);
    strcat(s,params);
    error_code = 0;
  
    if (!sd.exists(s))
      error_code = 1;
    else if (!sd.remove(s)) 
      error_code = 3;
  }
    
  SendByteToZ80(error_code);  // ... and Status
  ToggleClock();              // Final clock toggle
  reset_commands();
}

// COMMAND = 5 OK
void cmd_mkdir(){
uint8_t error_code;
char s[MAX_FILENAME_LEN];
char file_name[MAX_FILENAME_LEN];
  set_SDLed(LED_ON);
  check_SD();
  error_code = 0;
  ToggleClock();

  //GET PARAMS
  uint8_t param_len = GetByteFromZ80_IT();
  char ch;
  for (uint8_t i=0; i<param_len; i++){
    ToggleClock();
    ch = GetByteFromZ80_IT();
    params[i] = (char) asc81_to_ascii[ch];
  }
  params[param_len] = 0;

  if (T81_dir){
    error_code = 13; // forbiden operation
  } else {
    if (params[0]!='/'){
      complete_dir(tmp,current_dir);
      strcpy(tmp,current_dir);
      strcat(tmp,params);
    } else {
      strcpy(tmp,params);
    }
    complete_dir(s,tmp);
    
    if (sd.exists(s))
      error_code = 4;
    else if (!sd.mkdir(s)){
      log_0("can not create dir \"%s\"\r",s);
      error_code = 3;
    }
  }
  SendByteToZ80(error_code);  // ... and Status
  ToggleClock();              // Final clock toggle
  reset_commands();
}

// COMMAND = 6 OK
void cmd_rmdir(){
uint8_t error_code;
char s[MAX_FILENAME_LEN];
char file_name[MAX_FILENAME_LEN];
  check_SD();
  set_SDLed(LED_ON);
  error_code = 0;
  ToggleClock();

  //GET PARAMS
  uint8_t param_len = GetByteFromZ80_IT();
  char ch;
  for (uint8_t i=0; i<param_len; i++){
    ToggleClock();
    ch = GetByteFromZ80_IT();
    params[i] = (char) asc81_to_ascii[ch];
  }
  params[param_len] = 0;

  if (T81_dir){
    error_code = 13; // forbiden operation
  } else {
    if (params[0]!='/'){
      complete_dir(tmp,current_dir);
      strcpy(tmp,current_dir);
      strcat(tmp,params);
    } else {
      strcpy(tmp,params);
    }
    complete_dir(s,tmp);
    if (!sd.exists(s))
      error_code = 1;
    else {
      if (!sd.rmdir(s)){
        sprintf_P(tmp,PSTR("can not remove dir\r"), s);
        error_code = 3;
      }
      if (Sfile.open(s)){
        if (!Sfile.isDir()) error_code = 2;
        Sfile.close();    
      }
    } 
  }
  SendByteToZ80(error_code);  // ... and Status
  ToggleClock();              // Final clock toggle
  reset_commands();
}

// COMMAND = 7 OK
void cmd_move(){
uint8_t error_code;
char s[MAX_FILENAME_LEN];
char param2[MAX_PARAM_LEN];
char file_name[MAX_FILENAME_LEN];
  check_SD();
  set_SDLed(LED_ON);
  error_code = 0;
  ToggleClock();

  //GET PARAMS (first param)
  uint8_t param_len = GetByteFromZ80_IT();
  byte ch;
  for (uint8_t i=0; i<param_len; i++){
    ToggleClock();
    ch = GetByteFromZ80_IT();
    params[i] = (char) asc81_to_ascii[ch];
  }
  params[param_len] = 0;
  // Serial.print("PARAM1=");Serial.println(params);


  ToggleClock();
  //GET PARAMS (second param)
  param_len = GetByteFromZ80_IT();
  for (uint8_t i=0; i<param_len; i++){
    ToggleClock();
    ch = GetByteFromZ80_IT();
    param2[i] = (char) asc81_to_ascii[ch];
  }
  param2[param_len] = 0;
  if (T81_dir){
    error_code = 13; // forbiden operation
  } else {
    if (params[0]!='/'){
      complete_dir(tmp,current_dir);
      strcpy(tmp,current_dir);
      strcat(tmp,params);
    } else {
      strcpy(tmp,params);
    }
    complete_dir(params,tmp);
    
    if (param2[0]!='/'){
      complete_dir(tmp,current_dir);
      strcpy(tmp,current_dir);
      strcat(tmp,param2);
    } else {
      strcpy(tmp,param2);
    }
    complete_dir(param2,tmp);
    if (!sd.exists(params))
      error_code = 1;
    else if (sd.exists(param2))
      error_code = 4;
    else if (!sd.rename(params,param2)){
      sprintf_P(tmp,PSTR("can not rename file %s to %s"),params,param2);
      log_0("can not rename file %s to %s",params,param2);
      error_code = 3;
    }
  }
  SendByteToZ80(error_code);  // ... and Status
  ToggleClock();              // Final clock toggle
  reset_commands();
}

// COMMAND = 8  OK
void cmd_copy(){
uint8_t error_code;
char s[MAX_FILENAME_LEN];
char param2[MAX_FILENAME_LEN];
char file_name[MAX_FILENAME_LEN];
uint32_t result;
  set_SDLed(LED_ON);
  error_code = 0;

  check_SD();
  ToggleClock();
  sd.remove("SERIAL.P");

  //GET PARAMS (first param)
  uint8_t param_len = GetByteFromZ80_IT();
  byte ch;
  for (uint8_t i=0; i<param_len; i++){
    ToggleClock();
    ch = GetByteFromZ80_IT();
    params[i] = (char) asc81_to_ascii[ch];
  }
  params[param_len] = 0;
  
  check_serial();
  
  ToggleClock();
  //GET PARAMS (second param)
  param_len = GetByteFromZ80_IT();
  for (uint8_t i=0; i<param_len; i++){
    ToggleClock();
    ch = GetByteFromZ80_IT();
    param2[i] = (char) asc81_to_ascii[ch];
  }
  param2[param_len] = 0;
  if (T81_dir){
    error_code = 13; // forbiden operation
  } else {
    if (params[0]!='/'){   // bug: comparaba param2[0] (el destino) al
                            // completar la ruta del ORIGEN (params);
                            // ver cmd_move, que si compara el campo que
                            // le corresponde a cada mitad
      complete_dir(tmp,current_dir);
      strcpy(tmp,current_dir);
      strcat(tmp,params);
    } else {
      strcpy(tmp,params);
    }
    complete_dir(params,tmp);

    if (param2[0]!='/'){
      complete_dir(tmp,current_dir);
      strcpy(tmp,current_dir);
      strcat(tmp,param2);
    } else {
      strcpy(tmp,param2);
    }
    complete_dir(param2,tmp);
    uint32_t i;
    boolean opened = Sfile.open(params,O_READ);
    i = Sfile.fileSize();
    if (i>(2000000)) {
      log_2("file too large: %s (%lu)",params,i);
      error_code = 5;
    } else if (!opened) {
      log_2("can not open %s",params);
      error_code = 1;
    } else if (!Dfile.open(param2,O_CREAT|O_WRITE|O_TRUNC)) {
      log_2("can not create %s", param2);
      error_code = 6;
    } else {
      i = 0;
      do {
        set_SDLed((millis() % 64)>32);
        result = Sfile.read(copy_buffer,BUFFSIZE);
        int error = Dfile.write(copy_buffer,result);
        if (error < 0) error_code = 7;
        i+=BUFFSIZE;
      } while (result==BUFFSIZE);
    }
    Sfile.close();
    Dfile.close();
  }
  SendByteToZ80(error_code);  // ... and Status
  ToggleClock();              // Final clock toggle
  reset_commands();
}

// COMMAND = 9
void cmd_load(){
char s[MAX_FILENAME_LEN];
char file_name[MAX_FILENAME_LEN];
uint32_t result;
uint8_t error_code;
uint32_t i;
uint32_t fsize;
uint32_t fsize2;

  uint32_t st = millis();
  set_SDLed(LED_ON);

  check_SD();
  error_code = 0;
//  log_0("1");
  ToggleClock();
//  log_0("2");

  //GET PARAMS
  param_len = GetByteFromZ80();
//  log_0("len=%hu",param_len);
  char ch;
  for (uint8_t i=0; i<param_len; i++){
    ToggleClock();
    ch = GetByteFromZ80() & 0b01111111;
    params[i] = (char) asc81_to_ascii[ch];
    delay(10);
//    Serial.print(params[i]); Serial.println((byte) ch);
  }
  params[param_len] = 0;
//  log_0("4");

  // char* ext = get_filename_ext(params);
  // upStr(ext);
  // if (strcmp(ext,"WAV")==0) {        // WAV FILE
  //   // send file len = 0
  //   Sfile.close();
  //   if (params[0]==0x7c){
  //     log_0("stop wav player", tmp);
  //     playing_wav = false;
  //   } else if (params[0]==0x7e){
  //     log_0("continue wav player", tmp);
  //     playing_wav = true;
  //   } else {
  //     log_0("playing %s", tmp);
  //     playWavFile(tmp);
  //   }
  //   SendByteToZ80(0);
  //   SendByteToZ80(0);
  //   SendByteToZ80(error_code);  // ... and Status
  //   ToggleClock();              // Final clock toggle
  //   reset_commands();
  //   return;
  // }
  if (sd.exists("SERIAL.P")) sd.remove("SERIAL.P");

  check_serial();  
  
  if (param_len==0) {
    sprintf_P(params,PSTR("%s%03d"),params,file_counter);
    file_counter++;
  } else {
    file_counter = 0;
  }

  if (T81_dir){   // T81 dir
    uint32_t fpos;
    uint16_t fsize;
    
    getT81File(params, &fpos, &fsize);
    // log_1("1");
    // log_1("1. NAME= %s, POS=%lu, SIZE=%u",params,fpos,fsize);
    if (fsize == 0){
      sprintf_P(tmp,PSTR("%s.P"),params);
      getT81File(params, &fpos, &fsize);
//      log_1("2. NAME= %s, POS=%lu, SIZE=%u",params,fpos,fsize);
    }
    if (fsize == 0){
        error_code = 1;
      log_0("can't open load file");
      // send file length = 0
      SendByteToZ80(0);
      SendByteToZ80(0);
    } else {
//      Sfile.open(current_dir);
      for (int i = 0; i<10; i++){
        if (!Sfile.open(current_dir)) log_0("error openning %s",current_dir);
        else break;
        delay(10);
      }
      i = 0;
      if (Sfile.isOpen()){
        Sfile.seekSet(fpos);
        log_1("sending file");
        // send file length
        SendByteToZ80(fsize & 0xff);
        SendByteToZ80((fsize >> 8) & 0xff);
        do {
          set_SDLed((millis() % 64)>32);
          result = Sfile.read(copy_buffer,BUFFSIZE);
          for (uint16_t j=0; j<result; j++){
            SendByteToZ80(copy_buffer[j]); 
            i++;
            if (i>=fsize) break;
          }
        } while ((i < fsize) && (result==BUFFSIZE));
      }
      if (i<fsize) {
        log_1("sending file");
        error_code = 8;
        for (; i<fsize;i++){
          SendByteToZ80(0);
        }
      }
      log_0("CLOSE");
      Sfile.close();
    }
  } else {      // normal dir
  
    if (params[0]!='/'){
      complete_dir(tmp,current_dir);
      strcpy(tmp,current_dir);
      strcat(tmp,params);
    } else {
      strcpy(tmp,params);
    }
    if (!sd.exists(tmp)) {
      log_0("file %s does not exist.\n\r",tmp);
      strcat(tmp,".P");
      log_0("trying with %s. \n\r",tmp);
    }
  
    //Sfile.open(tmp,O_READ);
    for (int i = 0; i<10; i++){
      if (!Sfile.open(tmp,O_READ)) log_0("error openning %s",tmp);
      else break;
      delay(10);
    }
    boolean opened = Sfile.isOpen();
    fsize = Sfile.fileSize();
    Sfile.seekSet(0);
    if (!opened || (fsize == 0)) {
      log_0("can't open load file");
      // send file length = 0
      SendByteToZ80(0);
      SendByteToZ80(0);
      error_code = 1;
      if (fsize>65535) error_code = 5;
    } else {
      char* ext = get_filename_ext(tmp);
      upStr(ext);
      log_2("sending file %s", tmp);
  
      if (strcmp(ext,"WAV")==0) {        // WAV FILE
        // send file len = 0
        Sfile.close();
        log_0("playing %s", tmp);
        playWavFile(tmp);
        SendByteToZ80(0);
        SendByteToZ80(0);
        SendByteToZ80(error_code);  // ... and Status
        ToggleClock();              // Final clock toggle
        reset_commands();
        return;
      }
      result = Sfile.read(copy_buffer,BUFFSIZE);
      
      if ((strcmp(ext,"P")==0) || (ext[0]==0)){
        fsize2=copy_buffer[11]+256*copy_buffer[12]-16393;
        Sfile.seekSet(0);
      } else if (strcmp(ext,"81")==0){
        fsize2=copy_buffer[11]+256*copy_buffer[12]-16393;
        Sfile.seekSet(0);
      } else if (strcmp(ext,"P81")==0){
        int i;      
        for (i=0; copy_buffer[i]<80;i++); // skip fname
        i++;                              // skip last fname char
        fsize2=copy_buffer[i+11]+256*copy_buffer[i+12]-16393;
        log_1("i=%d size=%d",i,fsize2);
        Sfile.seekSet(i);
      } else if (strcmp(ext,"ROM")==0){
          digitalWrite(Z80_RESET, LOW);
          digitalWrite(FPGA_RESET, LOW);
          delay(1);
          Sfile.close();
          enableMem();
          log_1("1. NAME= %s",params);
          int status = load_ROM(tmp);
          if (status == -1) LED_error(clORANGE);
          else if (status == -2) LED_error(clBLUE);
          disableMem();
          _rst_ctrl_reg(HIGH);
          _rst_data_reg(HIGH);
          reseted = true;
          delay(1);
          digitalWrite(FPGA_RESET, HIGH);
          delay(10);
          digitalWrite(Z80_RESET, HIGH);
          reset_commands();
          return;
      } else {
        fsize2 = fsize;
        Sfile.seekSet(0);
      }
      log_0("fsize2=%u",fsize2);
      // send file length
      SendByteToZ80(fsize2 & 0xff);
      SendByteToZ80((fsize2 >> 8) & 0xff);
      i = 0;
      do {
        set_SDLed((millis() % 64)>32);
        result = Sfile.read(copy_buffer,BUFFSIZE);
        for (uint16_t j=0; j<result; j++){
          SendByteToZ80(copy_buffer[j]); 
          i++;
          if (i>=fsize2) break;
        }
      } while ((i < fsize2) && (result==BUFFSIZE));
    }
    if (i<fsize2) {
      log_0("i=%lu",i);
      error_code = 8;
      for (; i<fsize2;i++){
        SendByteToZ80(0);
      }
    }
    Sfile.close();
  }
  log_0("status=%hu",error_code);
  SendByteToZ80(error_code);  // ... and Status
  ToggleClock();              // Final clock toggle
  reset_commands();
}

// COMMAND = 10
void cmd_save(){
char s[MAX_FILENAME_LEN];
char file_name[MAX_FILENAME_LEN];
uint32_t result;
uint16_t block_size;
uint8_t error_code;

  set_SDLed(LED_ON);
  error_code = 0;
  check_SD();
  ToggleClock();

  //GET PARAMS
  uint8_t param_len = GetByteFromZ80();
  char ch;
  for (uint8_t i=0; i<param_len; i++){
    ToggleClock();
    ch = GetByteFromZ80();
    params[i] = (char) asc81_to_ascii[ch];
  }
  params[param_len] = 0;
  if (T81_dir){
    error_code = 13; // forbiden operation
  } else {
  
    if (params[0]!='/'){
      complete_dir(tmp,current_dir);
      strcpy(tmp,current_dir);
      strcat(tmp,params);
    } else {
      strcpy(tmp,params);
    }
    
    boolean opened = Sfile.open(tmp,O_CREAT|O_WRITE|O_TRUNC);
  
    // read file length
    ToggleClock();
    uint8_t l = GetByteFromZ80();
    ToggleClock();
    uint8_t h = GetByteFromZ80();
    uint16_t proglen = h*256+l;
  
    Sfile.seekSet(0);
    if (!opened || (proglen == 0)) {
      log_0("can't save file");
      error_code = 1;
    };
    log_2("saving file");
    uint32_t i = 0;
    do {
      if (proglen-i > BUFFSIZE) block_size = BUFFSIZE;
      else block_size = proglen-i;
      set_SDLed((millis() % 64)>32);
      for (uint16_t j=0; j<block_size; j++){  
        ToggleClock();
        copy_buffer[j] = GetByteFromZ80();
     }
      if (error_code == 0){
        result = Sfile.write(copy_buffer,block_size);
        if (result < block_size) error_code = 7;
      }
      i+=BUFFSIZE;
    } while (i<proglen);
    Sfile.close();
  }
  SendByteToZ80(error_code);  // ... and Status
  ToggleClock();              // Final clock toggle
  reset_commands();
}

void cmd_type(){
  uint8_t error_code = 0;

  char s[MAX_FILENAME_LEN];
  char file_name[MAX_FILENAME_LEN];
  uint32_t result;
  uint32_t i;
  uint32_t fsize;
  char* spar;

  uint32_t st = millis();
  set_SDLed(LED_ON);

  check_SD();
  error_code = 0;
  ToggleClock();

  //GET PARAMS
  param_len = GetByteFromZ80();
  char ch;
  ToggleClock();
  for (uint8_t i=0; i<param_len; i++){
    ch = GetByteFromZ80_IT();
    params[i] = (char) asc81_to_ascii[ch];
    ToggleClock();
    delay(1);
  }
  params[param_len] = 0;
  spar = &params[0];
  Serial.println(spar);

  int filetype = 0;       //ASCII file by default
  if (spar[0]=='*'){
    Serial.println("reading from MAN dir");
    spar++;
    if (spar[0]=='?') {
      Serial.println("native text");
      filetype = 1;   // zx81 native text file
      spar++;
    }
    char* fname = getfname(spar);
    strcpy(tmp,"/MAN/");
    strcat(tmp,fname);
//    sscanf(tmp,"/MAN/%s",fname);
    Serial.println(tmp);
  } else {
    if (spar[0]=='?') {
      Serial.println("native text");
      filetype = 1;   // zx81 native text file
      spar++;
    }
    if (spar[0]!='/'){
      complete_dir(tmp,current_dir);
      strcpy(tmp,current_dir);
      strcat(tmp,spar);
    } else {
      strcpy(tmp,spar);
    }
    Serial.println(tmp);
  }
  if (!sd.exists(tmp)) {
    log_0("file %s does not exist.\n\r",tmp);
    strcat(tmp,".TXT");
    log_0("trying with %s. \n\r",tmp);
  }

  // for (int i = 0; i<10; i++){
     if (!Sfile.open(tmp,O_READ)) log_0("error openning %s",tmp);
  //   else break;
  //   delay(10);
  // }
  boolean opened = Sfile.isOpen();
  fsize = Sfile.fileSize();
  Sfile.seekSet(0);
  if (!opened || (fsize == 0)) {
    log_0("can't open load file");
    // send file length = 0
    error_code = 1;
    if (fsize>65535) error_code = 5;
  } else {
    Serial.print("Sending text ("); Serial.print(fsize); Serial.println(") Bytes");
    // SEND STRING
    for (int i=0; (i < fsize);i++){
      set_SDLed((millis() % 64)>32);
      ch = GetByteFromZ80_IT();    // GET_CHAR
      Serial.println(ch);
      if (ch!=GET_NEXT_CHAR){
        command_active = ch;
        Serial.println("salimos");
        Sfile.close();
        return;
      }
      ch = Sfile.read();
      Serial.print("r=");Serial.println(ch);
      // if (result <0) {
      //   result = 0;
      //   error_code = 1;
      // }      
      if (filetype==0)
        SendByteToZ80_IT(asc_to_asc81[ch]); 
      else { 
       if (ch == EOT)  ch = 0; //avoid early EOF
        SendByteToZ80_IT(ch); 
      }
      ToggleClock();       // finish command
    }
  }

  Serial.println("FIN");
  ch = GetByteFromZ80_IT();    // GET_CHAR
  if (ch!=GET_NEXT_CHAR){
    command_active = ch;
    Sfile.close();
    return;
  }
  Sfile.close();
  SendByteToZ80(EOT);        // EOT...
  SendByteToZ80(error_code);  // ... and Status
  ToggleClock();              // Final clock toggle
  reset_commands();
}

void cmd_dir_open(){
char s[MAX_FILENAME_LEN];
char file_name[MAX_FILENAME_LEN];
char dirname[MAX_FILENAME_LEN];
char wildcards[MAX_FILENAME_LEN];
int pos;
uint8_t error_code;
uint8_t c;
uint16_t nfiles;
uint16_t ndirs;
bool isdir = false;
char ascSize[0x10];
  check_SD();
  nfiles = 0;
  ndirs=0;
  error_code = 0;
  ToggleClock();
  
  //GET PARAMS
  uint8_t param_len = GetByteFromZ80_IT();
  ToggleClock();
  byte ch;
  for (uint8_t i=0; i<param_len; i++){
    ch = GetByteFromZ80_IT();
    params[i] = (char) asc81_to_ascii[ch];
    ToggleClock();
  }
  params[param_len] = 0;

  if (T81_dir){
    if (param_len==0) strcpy(wildcards,"*");
    else strcpy(wildcards,params);
    strcpy(dirname, current_dir); 
    sprintf_P(s,PSTR("dir of \"%s%s\" is \r"), dirname,wildcards);
    
    // SEND STRING
    for (int i=0; s[i]!=0;i++){
      c = GetByteFromZ80_IT();    // GET_CHAR
      if (c!=GET_NEXT_CHAR){
        command_active = c;
        return;
      }
      SendByteToZ80_IT(inverse_video(asc_to_asc81[toupper(s[i])])); 
      ToggleClock();       // finish command
    }
    nfiles = 0;
    ndirs=0;
    bool fin = false;
    for (int i = 0; i<10; i++){
      if (!Sfile.open(dirname)) log_0("error openning %s",dirname);
      else break;
      delay(10);
    }
    Sfile.seekSet(4); // skip header
    while (!fin) {
      set_SDLed((millis() % 64)>32);
      if (Sfile.read(file_name,0x20)!= 0x20) break;
      if (Sfile.read(ascSize,0x10)!= 0x10) break; 
      uint32_t fsize = atoi(ascSize);
      if (matches(file_name,wildcards)){
       nfiles++;
       sprintf_P(s,PSTR("%-24s %7lu\r"),file_name, fsize);
        // SEND STRING
        for (int i=0; s[i]!=0;i++){
          c = GetByteFromZ80_IT();    // GET_CHAR
          if (c!=GET_NEXT_CHAR){
            command_active = c;
            Sfile.close(); 
            return;
          }
          SendByteToZ80_IT(asc_to_asc81[toupper(s[i])]); 
          ToggleClock();       // finish command
        }
      }
      fin = !Sfile.seekCur(fsize);
    }  
    Sfile.close(); 
  } else {
  
    complete_dir(s, params);
    for (int i = 0; i<10; i++){
      if (!dir.open(s)) log_0("error openning %s",s);
      else break;
      delay(10);
    }
    if (dir.isOpen())
      isdir = dir.isDir();
    else isdir = false;
    dir.close();
    if (isdir) {
       strcpy(dirname, s);
       strcpy(wildcards,"*");
    }
    else {
       sprintf_P(s, PSTR("%s%s"), current_dir, params);
       int len = strlen(s);
       // SPLIT INTO DIR AND FILENAME
      pos = 0;
      for (int i = len - 1; i >= 0; i--) {
          if (s[i] == '/') {
              pos = i + 1;
              break;
          }
      }
      memcpy(wildcards, s + pos, len - pos);
      wildcards[len - pos] = 0;
      if (pos > 0) s[pos] = 0;
      complete_dir(dirname, s);
    }
  
    set_SDLed(LED_ON);
  
    if (wildcards[0]==0) strcpy(wildcards,"*");
    for (int i = 0; i<10; i++){
      if (!dir.open(dirname)) log_0("error openning %s",dirname);
      else break;
      delay(10);
    }
    if (!dir.isOpen()){
      // Serial.println(dirname);
      log_0("dir open failed");
      error_code = 1;
    } else {
      sprintf_P(s,PSTR("dir of \"%s%s\" is \r"), dirname,wildcards);
      
      // SEND STRING
      for (int i=0; s[i]!=0;i++){
        c = GetByteFromZ80_IT();    // GET_CHAR
        if (c!=GET_NEXT_CHAR){
          command_active = c;
          return;
        }
        SendByteToZ80_IT(inverse_video(asc_to_asc81[toupper(s[i])])); 
        ToggleClock();       // finish command
      }
      
      while (Sfile.openNext(&dir, O_RDONLY)) {
        set_SDLed((millis() % 64)>32);
        Sfile.getName(file_name, MAX_FILENAME_LEN);
        if (Sfile.isDir() || matches(file_name,wildcards) || (wildcards[0]==0)){
          if (Sfile.isDir()) {
            strcat(file_name, "/");
            ndirs++;
            sprintf_P(s,PSTR("%s\r"),file_name);
          } else {
            nfiles++;
            uint32_t fsize = Sfile.fileSize();
            sprintf_P(s,PSTR("%-24s %7lu\r"),file_name, fsize);
          }
          // SEND STRING
          for (int i=0; s[i]!=0;i++){
            c = GetByteFromZ80_IT();    // GET_CHAR
            if (c!=GET_NEXT_CHAR){
              command_active = c;
              return;
            }
            SendByteToZ80_IT(asc_to_asc81[toupper(s[i])]); 
            ToggleClock();       // finish command
          }
          
        }
        Sfile.close();
      }
    }  
    dir.close();
  }
  sprintf_P(s,PSTR("Total %d dir(s), %d file(s) \r"), ndirs, nfiles);
  // SEND STRING
  for (int i=0; s[i]!=0;i++){
    c = GetByteFromZ80_IT();    // GET_CHAR
    if (c!=GET_NEXT_CHAR){
      command_active = c;
      return;
    }
    SendByteToZ80_IT(inverse_video(asc_to_asc81[toupper(s[i])])); 
    ToggleClock();       // finish command
  }

  c = GetByteFromZ80_IT();    // GET_CHAR
  if (c!=GET_NEXT_CHAR){
    command_active = c;
    return;
  }
  SendByteToZ80(EOT);        // EOT...
  SendByteToZ80(error_code);  // ... and Status
  ToggleClock();              // Final clock toggle
  reset_commands();
}

// COMMAND = GET_NEXT_CHAR
void cmd_get_next_char(){
  SendByteToZ80(EOT);        // EOT...
  SendByteToZ80(0);  // ... and Status
  ToggleClock();              // Final clock toggle
  reset_commands();
}

// COMMAND = 14
void cmd_free_txt(){
char s[MAX_FILENAME_LEN];
uint32_t sdfree;
  check_SD();
  set_SDLed(LED_ON);
  if (sdtotal == 0)
    sdtotal = sd.card()->sectorCount()>>1;
  sdfree = (sd.vol()->freeClusterCount() * sd.vol()->sectorsPerCluster())>>1;
  sprintf_P(s, PSTR("SD size %" PRIu32 " KB \rFREE %" PRIu32 " KB"), sdtotal, sdfree);
  ToggleClock();
  // SEND STRING
  for (int i=0; s[i]!=0;i++){
    char c = GetByteFromZ80_IT();    // GET_CHAR
    if (c!=GET_NEXT_CHAR){
      command_active = c;
      return;
    }
    SendByteToZ80(asc_to_asc81[s[i]]); 
    ToggleClock();       // finish command
  }
  char c = GetByteFromZ80_IT();    // GET_CHAR
  if (c!=GET_NEXT_CHAR){
    command_active = c;
    return;
  }
  SendByteToZ80(EOT);        // EOT...
  SendByteToZ80(0);  // ... and Status
  ToggleClock();              // Final clock toggle
  reset_commands();
}

// COMMAND =15
void cmd_free(){
uint32_t sdtotal;
uint32_t sdfree;
  check_SD();
  set_SDLed(LED_ON);
  sdtotal = 0.000512*sd.card()->sectorCount();
  sdfree = sd.vol()->freeClusterCount() * sd.vol()->sectorsPerCluster() * 0.000512;
  SendByteToZ80(sdtotal & 0xff);
  SendByteToZ80(sdtotal >> 8 & 0xff);
  SendByteToZ80(sdtotal >> 16 & 0xff);
  SendByteToZ80(sdtotal >> 24 & 0xff);
  SendByteToZ80(sdfree & 0xff);
  SendByteToZ80(sdfree >> 8 & 0xff);
  SendByteToZ80(sdfree >> 16 & 0xff);
  SendByteToZ80(sdfree >> 24 & 0xff);
  SendByteToZ80(0);  // ... and Status
  ToggleClock();              // Final clock toggle
  reset_commands();
}

// COMMAND =16  
void cmd_opendir2(){
char s[MAX_FILENAME_LEN];
char file_name[MAX_FILENAME_LEN];
char wildcards[MAX_FILENAME_LEN];
int pos;
uint8_t error_code;
uint8_t c;
bool isdir = false;
  check_SD();
  error_code = 0;
  ToggleClock();
  
  //GET PARAMS
  uint8_t param_len = GetByteFromZ80_IT();
  byte ch;
  for (uint8_t i=0; i<param_len; i++){
    ToggleClock();
    ch = GetByteFromZ80_IT();
    params[i] = (char) asc81_to_ascii[ch];
  }
  params[param_len] = 0;

  complete_dir(s, params);
  if (dir.open(s))
    isdir = dir.isDir();
  else isdir = false;
    if (dir.isOpen()) dir.close(); 
  if (isdir) {
     strcpy(array_dirname, s);
     strcpy(wildcards,"*");
  }
  else {
     sprintf_P(s, PSTR("%s%s"), current_dir, params);
     int len = strlen(s);
     // SPLIT INTO DIR AND FILENAME
    pos = 0;
    for (int i = len - 1; i >= 0; i--) {
        if (s[i] == '/') {
            pos = i + 1;
            break;
        }
    }
    memcpy(wildcards, s + pos, len - pos);
    wildcards[len - pos] = 0;
    if (pos > 0) s[pos] = 0;
    complete_dir(array_dirname, s);
  }

  set_SDLed(LED_ON);

  if (wildcards[0]==0) strcpy(wildcards,"*");
  if (!dir.open(array_dirname)){
    log_0("dir open failed");
    error_code = 1;
  } else {
    sprintf_P(s,PSTR("dir of \"%s%s\" is \r"), array_dirname,wildcards);

    // Sfile es un objeto GLOBAL compartido con otros comandos (p.ej.
    // cmd_copy lo usa via Sfile.open(nombre,...) para el origen). Si un
    // comando previo lo dejo sin cerrar del todo, openNext() puede
    // arrastrar estado y duplicar la entrada que ese comando toco --
    // cierre defensivo antes de enumerar, por si acaso.
    if (Sfile.isOpen()) Sfile.close();
    index_array = 0;
    while (Sfile.openNext(&dir, O_RDONLY)) {
      set_SDLed((millis() % 64)>32);
      Sfile.getName(file_name, MAX_FILENAME_LEN);
      if ((Sfile.isDir() || matches(file_name,wildcards) || (wildcards[0]==0)) && (index_array < MAX_FILE_ARRAY)){
        file_array[index_array] = Sfile.dirIndex();
        index_array++;
      }
      Sfile.close();
    }
  }
    if (dir.isOpen()) dir.close();
  SendByteToZ80(error_code);                // ... and Status
  // reset_commands() ANTES del ToggleClock() final (no despues, como en
  // el resto de comandos): reset_commands() es lo que pone
  // command_active=CMD_IDLE, que es la condicion que get_data_reg()
  // (COMMS.cpp) exige para aceptar un byte de comando nuevo. Con el
  // orden antiguo hay una ventana de carrera: el Z80 ve el ToggleClock
  // y puede mandar ya el siguiente comando antes de que el firmware
  // ejecute reset_commands(), y ese byte se pierde en silencio. Rara
  // vez se nota (el codigo Z80 normal deja hueco de sobra entre
  // llamadas MCU), pero con dos llamadas seguidas casi sin nada en
  // medio (p.ej. OPENDIR2 justo tras un CD) se pierde el comando.
  reset_commands();
  ToggleClock();                            // Final clock toggle
}

// COMMAND =17
void cmd_getrowlen(){
  
  uint8_t error_code;
  uint8_t h,l;
  uint8_t len;
  char file_name[MAX_FILENAME_LEN];

  error_code = 0;
  ToggleClock();
  l = GetByteFromZ80_IT();
  ToggleClock();
  h = GetByteFromZ80_IT();
  row_array = h*256+l;
  if (!dir.open(array_dirname)) {
    file_name[0]=0;
    error_code = 1;
    len = 0;
  } else {
    Sfile.open(dir,row_array);
    Sfile.getName(file_name, MAX_FILENAME_LEN);
    len = strlen(file_name);
    Sfile.close();
  }
    if (dir.isOpen()) dir.close(); 
  SendByteToZ80(len);
  SendByteToZ80(error_code);  // ... and Status
  ToggleClock();              // Final clock toggle
  reset_commands();
}


// COMMAND =18
void cmd_getrow(){
  uint8_t h,l;
  
  uint8_t error_code;
  char file_name[MAX_FILENAME_LEN];
  uint8_t len;

  error_code = 0;
  ToggleClock();
  l = GetByteFromZ80_IT();
  ToggleClock();
  h = GetByteFromZ80_IT();
  row_array = h*256+l;
  if (row_array==0){
    strcpy(file_name,current_dir);
    error_code = 0;
    len = strlen(file_name);  
    SendByteToZ80(len);  // string len
    for (int i=0; i<len;i++){
      SendByteToZ80(asc_to_asc81[file_name[i]]);
    }
  } else if(!dir.open(array_dirname, O_RDONLY)) {
    file_name[0]=0;
    error_code = 1;
    len = 0;
  } else if (row_array > index_array) {
    // Fuera de rango: OPENDIR2 solo encontro index_array entradas para
    // ESTE directorio (indices validos de file_array: 0..index_array-1).
    // Sin este chequeo, file_array[row_array-1] leia memoria residual
    // (de una enumeracion anterior, posiblemente de otra carpeta) en
    // vez de devolver "no existe", duplicando la ultima entrada real.
    SendByteToZ80(0);  // string len
    dir.getName(file_name, MAX_FILENAME_LEN);
    error_code = 0;
  } else {
    if (Sfile.open(&dir, file_array[row_array-1], O_RDONLY)){
      Sfile.getName(file_name, MAX_FILENAME_LEN);
      if (Sfile.isDir()){
        strcpy(tmp,file_name);
        sprintf(file_name,"<%s>",tmp);
      }
      len = strlen(file_name);
      SendByteToZ80(len);  // string len
      for (int i=0; i<len;i++){
        SendByteToZ80(asc_to_asc81[file_name[i]]);
      }
      Sfile.close();
    } else {
        SendByteToZ80(0);  // string len
        dir.getName(file_name, MAX_FILENAME_LEN);
        // Serial.print("DIR: "); Serial.println(file_name);
        // Serial.print("COULD NOT OPEN FILENO "); Serial.println(file_array[row_array]);
        error_code = 0;
    }
  }
    if (dir.isOpen()) dir.close(); 
  SendByteToZ80(error_code);  // ... and Status
  ToggleClock();              // Final clock toggle
  reset_commands();
}

// COMMAND = 19
void cmd_enable_mc45() {
  log_0("MC45 ENABLED");
  send_bit_config(cfgcmd_MC45,1);
//  digitalWrite(EN_M1_NOT,HIGH);
  ToggleClock();
  
  reset_commands();
}

// COMMAND = 20
void cmd_disable_mc45() {
  log_0("MC45 DISABLED");
  send_bit_config(cfgcmd_MC45,0);
//  digitalWrite(EN_M1_NOT,LOW);
  ToggleClock();
  
  reset_commands();
}


// COMMAND = 21
void cmd_joy() {
uint8_t error_code;
  ToggleClock();
  //GET PARAMS
  uint8_t param_len = GetByteFromZ80_IT();
  char ch;
  for (uint8_t i=0; i<param_len; i++){
    ToggleClock();
    params[i] = GetByteFromZ80_IT();
    //params[i] = (char) asc81_to_ascii[ch];
  }
  params[param_len] = 0;

  error_code = 0;

  if (param_len!=5) error_code = 14;
  else if (!send_joycfg(params)) error_code = 14;
  SendByteToZ80(error_code);  // ... and Status
  ToggleClock();              // Final clock toggle
  reset_commands();
}

// COMMAND = 22
void cmd_binary_say(){
  uint8_t error_code;
  uint8_t i;
  uint16_t value;
  uint8_t len;
  uint8_t v[256];
  
  check_SD();
  error_code = 0;
  ToggleClock();
  //GET PARAMS
  uint8_t param_len = GetByteFromZ80_IT();
  byte ch;
  Serial.println("Alophone string:");
  for (uint8_t i=0; i<param_len; i++){
    ToggleClock();
    ch = GetByteFromZ80_IT();
    Serial.println(ch);
    v[i] = ch;
  }
  v[param_len] = 0;
  v[param_len+1] = 0;

  Speech(v);
  while (voice_is_playing)idle_tasks();
  SendByteToZ80(error_code);  // ... and Status
  ToggleClock();              // Final clock toggle
  reset_commands();
}

// COMMAND = 23
void cmd_say(){
  uint8_t error_code;
  uint8_t i;
  uint8_t v[256];
  uint16_t value;
  uint8_t len;
  
  check_SD();
  error_code = 0;
  ToggleClock();
  //GET PARAMS
  uint8_t param_len = GetByteFromZ80_IT();
  byte ch;
  for (uint8_t i=0; i<param_len; i++){
    ToggleClock();
    ch = GetByteFromZ80_IT();
    Serial.println(ch);
    params[i] = (char) asc81_to_ascii[ch];
  }
  params[param_len] = 0;
  Serial.println(params);

  // // convert hex string to binary
  // len = param_len/2;
  // for (i=0; i<len; i++){
  //   v[i]=hexval(params[i*2])*16+hexval(params[i*2+1]);
  // }


  v[i] = 0;
  v[i+1]=0;
  bool background = params[0]=='*';
  char* param1 = background? params+1:params;
  Speech(param1);
  while (voice_is_playing && !background)idle_tasks();
  SendByteToZ80(error_code);  // ... and Status
  ToggleClock();              // Final clock toggle
  reset_commands();
}

// COMMAND = 24
void cmd_AY_set_reg(){

  uint8_t reg;
  uint16_t value;

  ToggleClock();
  reg = GetByteFromZ80_IT();  
  ToggleClock();
  value = GetByteFromZ80_IT();  

  AY_SetReg(reg, value);

  ToggleClock();              // Final clock toggle
  reset_commands();
}

// COMMAND = 25
void cmd_AY_get_reg(){
  uint8_t reg;
  uint8_t value;

  ToggleClock();
  reg = GetByteFromZ80_IT();  

  if (reg <= 015) 
    value = ay_registers[reg];
  else 
    value = 0;

  SendByteToZ80(value);  
  ToggleClock();
  reset_commands();
}

// COMMAND = 26
void cmd_AY_play(){
  uint8_t error_code;
  uint8_t param_len;
  bool background = false;
  char* param1;
  param2[0]='\0';
  param3[0]='\0';

  error_code = 0;
  ToggleClock();
  //GET PARAM1
  param_len = GetByteFromZ80_IT();
  byte ch;
  for (uint8_t i=0; i<param_len; i++){
    ToggleClock();
    ch = GetByteFromZ80_IT();
    params[i] = (unsigned char) asc81_to_ascii[ch];
    if ((ch >= 0x26) && (ch <= 0x3f)) params[i] = tolower(params[i]);
  }
  params[param_len] = 0;
  ToggleClock();
  //GET PARAM2
  param_len = GetByteFromZ80_IT();
  for (uint8_t i=0; i<param_len; i++){
    ToggleClock();
    ch = GetByteFromZ80_IT();
    param2[i] = (unsigned char) asc81_to_ascii[ch];
    if ((ch >= 0x26) && (ch <= 0x3f)) param2[i] = tolower(param2[i]);
  }
  param2[param_len] = 0;
  ToggleClock();
  //GET PARAM3
  param_len = GetByteFromZ80_IT();
  for (uint8_t i=0; i<param_len; i++){
    ToggleClock();
    ch = GetByteFromZ80_IT();
    param3[i] = (unsigned char) asc81_to_ascii[ch];
    if ((ch >= 0x26) && (ch <= 0x3f)) param3[i] = tolower(param3[i]);
  }
  if (params[0]=='*') {
    background = true;
    param1 = &params[1];
  } else {
    background = false;
    param1 = params;
  }
  param3[param_len] = 0;
  Serial.printf("Playing AY strings %s,%s,%s.\n\r",params,param2,param3);
  error_code = play_parser_start(param1,param2,param3);
  Serial.printf("error code:%d \n\r",error_code);
  while (play_parser_update()&&!background){
    if (reseted) play_parser_stop();
  }; 
  if(!reseted){ 
    SendByteToZ80(error_code);  // ... and Status
    ToggleClock();              // Final clock toggle
  }
  reset_commands();
}

// COMMAND = 27
void cmd_sel_128_chars(){
  log_2("128 chars mode selected");
//  digitalWrite(SEL_128CHARS,HIGH);
  send_bit_config(cfgcmd_128CHARS,1);
  ToggleClock();
  
  reset_commands();
}

// COMMAND = 28
void cmd_sel_64_chars(){
  log_2("64 chars mode selected");
//  digitalWrite(SEL_128CHARS,LOW);
  send_bit_config(cfgcmd_128CHARS,0);
  ToggleClock();
  reset_commands();
}

// COMMAND = 29
void cmd_fullpaging(){
  log_2("full paging mode selected");
  // digitalWrite(FULLPAGING,HIGH);
  send_bit_config(cfgcmd_FULLPAG,1);
  ToggleClock();
  reset_commands();
}

// COMMAND = 30
void cmd_halfpaging(){
  log_2("half paging mode selected");
//  digitalWrite(FULLPAGING,LOW);
  send_bit_config(cfgcmd_FULLPAG,0);
  ToggleClock();
  reset_commands();
}

// COMMAND = 32
void cmd_getbyte(){
  uint8_t value;
  ToggleClock();
  uint8_t reg = GetByteFromZ80_IT();  
  
  if (reg <128) value = sys_vars[reg];
  else value = EEPROM.read(reg-128);
  
  SendByteToZ80(value);  
  ToggleClock();
  reset_commands();
}

// COMMAND = 33
void cmd_setbyte(){
  ToggleClock();
  uint8_t reg = GetByteFromZ80_IT();  
  ToggleClock();
  uint8_t value = GetByteFromZ80_IT();  
  ToggleClock();
  if (reg <128) sys_vars[reg]=value;  
  else EEPROM.write(reg-128,value);

  reset_commands();
}

// COMMAND = 34
void cmd_play_VGM(){
char file_name[MAX_FILENAME_LEN];
uint32_t result;
uint8_t error_code;
uint32_t i;


  check_SD();
  error_code = 0;
  ToggleClock();

  //GET PARAMS
  uint8_t param_len = GetByteFromZ80_IT();
  char ch;
  for (uint8_t i=0; i<param_len; i++){
    ToggleClock();
    ch = GetByteFromZ80_IT();
    params[i] = (char) asc81_to_ascii[ch];
  }
  params[param_len] = 0;

  if (params[0]!='/'){
    complete_dir(tmp,current_dir);
    strcpy(tmp,current_dir);
    strcat(tmp,params);
  } else {
    strcpy(tmp,params);
  }
  if (!sd.exists(tmp)) strcat(tmp,".vgm");

  error_code = openVGM(tmp);

  if (error_code == 0) {
    set_SDLed(LED_ON);
    playing_VGM = true;
    log_3("VGM started");
  }
  
  SendByteToZ80(error_code);  // ... and Status
  ToggleClock();              // Final clock toggle
  reset_commands();
}

// COMMAND = 35
void cmd_stop_VGM() {
uint8_t error_code;
  log_2("Stop VGM");
  if (VGMFile.isOpen()) error_code = 0;
  else error_code = 12;
  if (error_code == 0) {
    playing_VGM = false;
    // AY_stop();
    ay_emulator_init();
    log_3("VGM stopped");
    stopVGM();
    // VGMFile.close();
  }
//  SendByteToZ80(error_code);  // ... and Status
  set_SDLed(LED_OFF);
  ToggleClock();
  
  reset_commands();
}

// COMMAND = 36
void cmd_pause_VGM() {
uint8_t error_code;
  log_2("Pause VGM");
  if (VGMFile.isOpen()) error_code = 0;
  else error_code = 12;
  if (error_code == 0) {
    playing_VGM = false;
    // AY_stop();
    ay_emulator_init();
    log_3("VGM paused");
  }
//  SendByteToZ80(error_code);  // ... and Status
  ToggleClock();
  
  set_SDLed(LED_OFF);
  reset_commands();
}

// COMMAND = 37
void cmd_cont_VGM() {
uint8_t error_code;
  log_2("Continue VGM");
  if (VGMFile.isOpen()) error_code = 0;
  else error_code = 12;
  if (error_code == 0) {
    set_SDLed(LED_ON);
    playing_VGM = true;
    log_3("VGM restarted");
  }
//  SendByteToZ80(error_code);  // ... and Status
  ToggleClock();
  
  reset_commands();
}

// COMMAND = 38
void cmd_loop_VGM() {
  log_2("LOOP VGM");
  ToggleClock();
  VGM_mode = GetByteFromZ80_IT();
  log_2("VGM MODE = %hu",VGM_mode);
  ToggleClock();
  reset_commands();
}

// COMMAND = 40
void cmd_load_PEG(){
  ToggleClock();
  check_SD();
  uint8_t addr = GetByteFromZ80_IT();  
  ToggleClock();
  //GET PARAM1
  uint8_t param_len = GetByteFromZ80_IT()/2;
  log_1("PEG: addr=%hu,len=%hu",addr,param_len);
  for (uint8_t i=addr; i<addr+param_len; i++){
    ToggleClock();
    uint16_t vl = GetByteFromZ80_IT();
    ToggleClock();
    uint16_t vh = GetByteFromZ80_IT();
    PEG_mem[i] = vh*256+vl;
//    sprintf(tmp,"%04x ", PEG_mem[i]);
//    Serial.print(tmp);
//    if ((i % 8) == 7) Serial.println();
  }
  ToggleClock();
  reset_commands();
}

// COMMAND = 41
void cmd_play_PEG(){
uint8_t error_code;
  ToggleClock();
  uint8_t thread = GetByteFromZ80_IT() % NUM_THREADS;  
  ToggleClock();
  uint8_t addr = GetByteFromZ80_IT();  
  ToggleClock();
  PEG_addr[thread] = addr;
  PEG_pc[thread] = addr;
  playing_PEG[thread] = true;
  reset_commands();
}

// COMMAND = 42
void cmd_stop_PEG(){
  ToggleClock();
  uint8_t thread = GetByteFromZ80_IT() % NUM_THREADS; 
  PEG_pc[thread] = PEG_addr[thread];
  ToggleClock();
  playing_PEG[thread] = false;
  reset_commands();
}

// COMMAND = 43
void cmd_pause_PEG(){
  ToggleClock();
  uint8_t thread = GetByteFromZ80_IT() % NUM_THREADS;  
  ToggleClock();
  playing_PEG[thread] = false;
  reset_commands();
}

// COMMAND = 44
void cmd_cont_PEG(){
  ToggleClock();
  uint8_t thread = GetByteFromZ80_IT() % NUM_THREADS;  
  ToggleClock();
  playing_PEG[thread] = true;
  reset_commands();
}

// COMMAND = 45
void cmd_sdload_PEG(){
char s[MAX_FILENAME_LEN];
char file_name[MAX_FILENAME_LEN];
uint32_t result;
uint8_t error_code;
uint32_t i;
uint32_t fsize;

  uint32_t st = millis();
  set_SDLed(LED_ON);
  check_SD();

  error_code = 0;
  ToggleClock();

  //GET PARAMS
  uint8_t param_len = GetByteFromZ80();
  char ch;
  for (uint8_t i=0; i<param_len; i++){
    ToggleClock();
    ch = GetByteFromZ80();
    params[i] = (char) asc81_to_ascii[ch];
  }
  params[param_len] = 0;
  if (param_len==0) {
    sprintf_P(params,PSTR("%s%03d"),params,file_counter);
    file_counter++;
  } else {
    file_counter = 0;
  }

  ToggleClock();
  uint8_t addr = GetByteFromZ80();

  if (params[0]!='/'){
    complete_dir(tmp,current_dir);
    strcpy(tmp,current_dir);
    strcat(tmp,params);
  } else {
    strcpy(tmp,params);
  }
  if (!sd.exists(tmp)) strcat(tmp,".PEB");
  
  boolean opened = Sfile.open(tmp,O_READ);
  fsize = Sfile.fileSize();
  Sfile.seekSet(0);
  if (!opened || (fsize>512) || (fsize == 0)) {
    log_0("can't open load file");
    error_code = 1;
    if (fsize>512) error_code = 5;
  } else {
    char* ext = get_filename_ext(tmp);
    upStr(ext);
    log_2("sending file");
    result = Sfile.read((PEG_mem+addr),fsize);
  }
  if (result<fsize) {
    error_code = 8;
  }
  Sfile.close();
//  for (uint8_t i=addr; i<((fsize/2)+addr); i++){
//    log_0("DIR:%02X VAL:%02X",i,PEG_mem[i]);
//  }
  SendByteToZ80(error_code);  // ... and Status
  ToggleClock();              // Final clock toggle
  reset_commands();
}

void cmd_enable_ich_table(){
//  log_0("ICH_TABLE_ENABLED");
//  digitalWrite(INT_CHR_TABLE,HIGH);
//  ToggleClock();
 
//  reset_commands();
}

void cmd_disable_ich_table(){
//  log_0("ICH_TABLE_DISABLED");
//  digitalWrite(INT_CHR_TABLE,LOW);
//  ToggleClock();
 
//  reset_commands();
}

void cmd_enable_48K(){
  log_2("STD_48K_ENABLED");
//  digitalWrite(STD_48K,LOW); // active low
  send_bit_config(cfgcmd_MODE48K,0);
  ToggleClock();
 
  reset_commands();
}

void cmd_disable_48K(){
  log_2("STD_48K_DISABLED");
//  digitalWrite(STD_48K,HIGH); // active low
  send_bit_config(cfgcmd_MODE48K,1);
  ToggleClock();
 
  reset_commands();
}

void cmd_rtc(){
  char date_time [23];
  uint8_t error = 0;

  ToggleClock();
  //GET PARAMS
  uint8_t param_len = GetByteFromZ80_IT();
  Serial.print("LEN="); Serial.println(param_len);
  if (param_len==0) { // returns date time
    rtc_get_time(date_time);
    Serial.print("DATE/TIME: "); Serial.println(date_time);
    for (int i=0; i<strlen(date_time);i++){
      uint8_t v=(asc_to_asc81[date_time[i]]);
      SendByteToZ80(v); 
      delay(1);
//      Serial.println(v);
    }
  } else {
    char ch;
    for (uint8_t i=0; i<param_len; i++){
      ToggleClock();
      ch = GetByteFromZ80_IT();
      params[i] = (char) asc81_to_ascii[ch];
    }

    params[param_len]=0;
    // "yyyy-mm-dd hh:mm:ss.cc"
    Serial.print("PARAMS: "); Serial.println(params);

    bool done = false;

  //  if (param_len == 22) { // set date time
    int year,month,day,hours,minutes,seconds,hundredths;
    int n = sscanf(params,"%04d-%02d-%02d %02d:%02d:%02d.%02d",&year,&month,&day,&hours,&minutes,&seconds,&hundredths);
    Serial.println(n);

    year %=100; // only two digits

    if ((n==7)&&(isDate(year,month,day))&&(isTime(hours,minutes,seconds,hundredths))){
      rtc.setHours(hours);
      rtc.setMinutes(minutes);
      rtc.setSeconds(seconds);
      rtc.setTime(hours, minutes, seconds); // Actualiza la hora

      rtc.setDay(day);
      rtc.setMonth(month);
      rtc.setYear(year);
      rtc.setDate(day, month, year); // Actualiza la fecha

      Serial.print("✅ Reloj actualizado a "); Serial.println(params);
      done = true;
    } 
    if ((n==3)&&(isDate(year,month,day))){
      rtc.setDay(day);
      rtc.setMonth(month);
      rtc.setYear(year);
      rtc.setDate(day, month, year); // Actualiza la fecha
      done = true;
    }
    if (!done){
      int n = sscanf(params,"%02d:%02d:%02d.%02d",&hours,&minutes,&seconds,&hundredths);
      if ((n==4)&&(isTime(hours,minutes,seconds,hundredths))){
        rtc.setHours(hours);
        rtc.setMinutes(minutes);
        rtc.setSeconds(seconds);
        rtc.setTime(hours, minutes, seconds); // Actualiza la hora
        done = true;
      } else if ((n==3)&&(isTime(hours,minutes,seconds,0))){
        rtc.setHours(hours);
        rtc.setMinutes(minutes);
        rtc.setSeconds(seconds);
        rtc.setTime(hours, minutes, seconds); // Actualiza la hora
        done = true;
      } else if ((n==2)&&(isTime(hours,minutes,0,0))){
        rtc.setHours(hours);
        rtc.setMinutes(minutes);
        rtc.setSeconds(0);
        rtc.setTime(hours, minutes, 0); // Actualiza la hora
        done = true;
      }
    }
  if (!done) error = 3;
 }

  Serial.println("fin");
  SendByteToZ80(error); // ... and Status
  ToggleClock();       // Final clock toggle
  reset_commands();
}

void cmd_bat(){
  float bat = get_battery_level();
  int parte_entera = bat;
  int parte_decimal = ((bat-parte_entera)*1000.0);
  char sbat[6];
  sprintf(sbat,"%d.%d",parte_entera,parte_decimal);
  for (int i=0; i<5;i++){
    Serial.println(i);
    SendByteToZ80(asc_to_asc81[sbat[i]]); 
  }
  Serial.println("fin");
  SendByteToZ80(0x00); // ... and Status
  ToggleClock();       // Final clock toggle
  reset_commands();
}

// =====================================================================
//  Acceso aleatorio a fichero (CP/M-SD81).  Comandos 0x35-0x39 (53-57).
//  Handshake: igual que el resto de comandos. Un ToggleClock por byte,
//  desfasado: el ToggleClock inicial confirma el byte de comando; cada
//  ToggleClock posterior confirma el byte anterior; el ultimo byte
//  recibido lo confirma el SendByteToZ80/ToggleClock final.
//  El nombre llega en ASCII PURO (no codigo ZX81): NO usar asc81_to_ascii.
//  Estado: 0x00 OK, 0x01 lectura corta/EOF (solo fread), 0xFF error.
// =====================================================================

// fopen comun:  cmd + len(1) + nombre -> handle (0..3 / 0xFF)
//   convert=false -> nombre en ASCII puro (cliente CP/M)
//   convert=true  -> nombre en codigo ZX81 (modo nativo): se traduce a ASCII
static void do_f_open(bool convert){
  uint8_t handle = 0xFF;
  check_SD();
  set_SDLed(LED_ON);
  ToggleClock();                          // ACK del byte de comando

  uint8_t name_len = GetByteFromZ80_IT(); // longitud del nombre
  for (uint8_t i=0; i<name_len; i++){
    ToggleClock();                        // confirma byte anterior
    uint8_t b = GetByteFromZ80_IT();
    params[i] = convert ? (char)asc81_to_ascii[b] : (char)b;
  }
  params[name_len] = 0;

  // ruta relativa al directorio actual del SD (salvo ruta absoluta)
  if (params[0]!='/'){
    strcpy(tmp,current_dir);
    strcat(tmp,params);
  } else {
    strcpy(tmp,params);
  }

  int h;
  if (sd.exists(tmp)){                     // NO crear si no existe
    h = -1;
    for (int i=0; i<4; i++) if (!f_opened[i]){ h=i; break; }
    if (h>=0 && f_handle[h].open(tmp, O_RDWR)){   // equivale a "r+b"
      f_opened[h] = true;
      handle = (uint8_t) h;
    }
  } else {
    log_0("fopen: %s no existe",tmp);
  }

  SendByteToZ80(handle);                   // confirma ultimo byte + envia handle
  ToggleClock();                           // toggle final
  reset_commands();
}

// COMMAND = 53 (0x35) fopen ASCII (cliente CP/M)
void cmd_f_open(){ do_f_open(false); }

// COMMAND = 58 (0x3A) fopen ZX81 (modo nativo): nombre en codigo ZX81
void cmd_f_open_zx81(){ do_f_open(true); }

// COMMAND = 54 (0x36) fseek:  cmd + handle(1) + offset(4 LE) -> status
void cmd_f_seek(){
  uint8_t status = 0xFF;
  ToggleClock();                           // ACK
  uint8_t h = GetByteFromZ80_IT();         // handle
  uint32_t off = 0;
  for (uint8_t i=0; i<4; i++){
    ToggleClock();                         // confirma byte anterior
    uint8_t b = GetByteFromZ80_IT();
    off |= ((uint32_t)b) << (8*i);         // little-endian
  }
  if (h<4 && f_opened[h]){
    status = f_handle[h].seekSet(off) ? 0x00 : 0xFF;
  }
  SendByteToZ80(status);                    // confirma ultimo byte + status
  ToggleClock();                            // toggle final
  reset_commands();
}

// COMMAND = 55 (0x37) fread:  cmd + handle(1) + count(2 LE) -> count bytes + status
// IMPORTANTE: enviar SIEMPRE count bytes (rellenar con 0) seguidos del status.
void cmd_f_read(){
  uint8_t status = 0xFF;
  ToggleClock();                           // ACK
  uint8_t h  = GetByteFromZ80_IT();        // handle
  ToggleClock();
  uint8_t cl = GetByteFromZ80_IT();        // count lo
  ToggleClock();
  uint8_t ch = GetByteFromZ80_IT();        // count hi
  uint16_t count = ((uint16_t)ch<<8) | cl;

  set_SDLed(LED_ON);
  // lectura en bloque (evita el read() byte a byte que puede desalinear el buffer interno)
  int32_t got = 0;
  if (h<4 && f_opened[h] && count>0 && count<=BUFFSIZE){
    got = f_handle[h].read(copy_buffer, count);
    if (got < 0) got = 0;
  }
  // rellenar con ceros si lectura corta
  if ((uint16_t)got < count)
    memset(copy_buffer + got, 0, count - (uint16_t)got);

  for (uint16_t i=0; i<count; i++)
    SendByteToZ80(copy_buffer[i]);          // confirma anterior + envia dato

  if (h>=4 || !f_opened[h]) status = 0xFF;
  else status = ((uint16_t)got==count) ? 0x00 : 0x01;

  SendByteToZ80(status);                    // confirma ultimo byte + status
  ToggleClock();                            // toggle final
  reset_commands();
}

// COMMAND = 56 (0x38) fwrite:  cmd + handle(1) + count(2 LE) + count bytes -> status
void cmd_f_write(){
  uint8_t status = 0xFF;
  ToggleClock();                           // ACK
  uint8_t h  = GetByteFromZ80_IT();        // handle
  ToggleClock();
  uint8_t cl = GetByteFromZ80_IT();        // count lo
  ToggleClock();
  uint8_t ch = GetByteFromZ80_IT();        // count hi
  uint16_t count = ((uint16_t)ch<<8) | cl;

  // recibir todos los bytes en copy_buffer antes de escribir en bloque
  for (uint16_t i=0; i<count; i++){
    ToggleClock();                         // confirma byte anterior
    copy_buffer[i] = GetByteFromZ80_IT();
  }
  set_SDLed(LED_ON);
  if (h<4 && f_opened[h]){
    int32_t written = (count>0) ? f_handle[h].write(copy_buffer, count) : count;
    if (written == (int32_t)count){
      f_handle[h].sync();
      status = 0x00;
    }
  }

  SendByteToZ80(status);                    // confirma ultimo byte + status
  ToggleClock();                            // toggle final
  reset_commands();
}

// COMMAND = 57 (0x39) fclose:  cmd + handle(1) -> status
void cmd_f_close(){
  uint8_t status = 0xFF;
  ToggleClock();                           // ACK
  uint8_t h = GetByteFromZ80_IT();         // handle
  if (h<4 && f_opened[h]){
    f_handle[h].close();
    f_opened[h] = false;
    status = 0x00;
  }
  SendByteToZ80(status);                    // confirma handle + status
  ToggleClock();                            // toggle final
  reset_commands();
}

// reserved codes for future
void cmd_spare(){
  log_0("Command not recognized: %s",command_active);
  ToggleClock();
  SendByteToZ80(1);  // ... and Status
  reset_commands();
}

command_handler commands[] = {
  cmd_nop,              //0
  cmd_ver,              //1
  cmd_pwd,              //2
  cmd_cd,               //3
  cmd_del,              //4
  cmd_mkdir,            //5
  cmd_rmdir,            //6
  cmd_move,             //7
  cmd_copy,             //8
  cmd_load,             //9
  cmd_save,             //10
  cmd_type,            //11
  cmd_dir_open,         //12
  cmd_get_next_char,    //13
  cmd_free_txt,         //14
  cmd_free,             //15
  cmd_opendir2,         //16
  cmd_getrowlen,        //17
  cmd_getrow,           //18
  cmd_enable_mc45,      //19
  cmd_disable_mc45,     //20
  cmd_joy,              //21
  cmd_spare,            //22
  cmd_say,              //23
  cmd_AY_set_reg,       //24   
  cmd_AY_get_reg,       //25
  cmd_AY_play,          //26
  cmd_sel_128_chars,    //27
  cmd_sel_64_chars,     //28
  cmd_fullpaging,       //29
  cmd_halfpaging,       //30
  cmd_spare,            //31
  cmd_getbyte,          //32
  cmd_setbyte,          //33
  
  cmd_play_VGM,         //34
  cmd_stop_VGM,         //35
  cmd_pause_VGM,        //36
  cmd_cont_VGM,         //37
  cmd_loop_VGM,         //38
  
  cmd_spare,
  
  cmd_load_PEG,         //40
  cmd_play_PEG,         //41
  cmd_stop_PEG,         //42
  cmd_pause_PEG,        //43
  cmd_cont_PEG,         //44
  cmd_sdload_PEG,       //45
  
  cmd_enable_ich_table, //46
  cmd_disable_ich_table,//47
  cmd_enable_48K,   //48
  cmd_disable_48K,  //49
  cmd_rtc,              //50
  cmd_spare,            //51
  cmd_bat,              //52
  cmd_f_open,           //53
  cmd_f_seek,           //54
  cmd_f_read,           //55
  cmd_f_write,    //56
  cmd_f_close,            //57
  cmd_f_open_zx81,      //58 (0x3A) fopen con nombre en codigo ZX81
  cmd_spare
};
