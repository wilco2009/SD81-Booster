#include <setjmp.h>
#include <EEPROM.h>
#include <SPI.h>
#include <Wire.h>
#include <SdFat.h>
#include <STM32RTC.h>

#include "PINS.h"
#include "GLOBALS.h"
#include "dac_control.h"
#include "voice_synthesizer.h"
#include "play_command_parser.h"
#include "mixer.h"
#include "ay_emulator.h"
#include "WILDCARD.h"
#include "MEM.h"
#include "SD_handle.h"
#include "VGM.h"
#include "PEG.h"
#include "WAV.h"
#include "COMMS.h"
#include "T81.h"
#include "transfer.h"
#include "COMMANDS.h"
#include "FLASH_SPI.h"
#include "FLASH_UPDATE.h"
#include "VBAT.h"
#include "RTC.h"
#include "z80-disassembler.h"
#include "TEMP.h"
#include "WIFI_HANDLER.h"

extern "C" uint32_t SystemCoreClock;

jmp_buf jbuf;

void jmpfar()
{
  //interrupts();
  longjmp(jbuf, 1);
}

const uint64_t QS_debounce_time = 1000;
uint64_t QS_pressed_time = 0;
bool QS_pressed = false;
void get_QS_button(void){
  // if(!QS_pressed) {
  //   nQS_en = !nQS_en;
  // }
  //  send_bit_config(cfgcmd_QSEN,nQS_en?1:0);
  QS_pressed = true;
  QS_pressed_time = millis();
//   Serial.print("PRESSED: ");
//   Serial.println(QS_pressed);
// //  set_status_led_ok();
}

#define NUM_OPCODES 100
#define OPCODE_WIDTH 4

uint8_t debug_info[NUM_OPCODES*OPCODE_WIDTH];
uint16_t debug_index = 0;

void get_debug(void){
  if (debug_index<NUM_OPCODES*OPCODE_WIDTH){
    debug_info[debug_index] = read_LATCH_B();
    debug_index++;
  }
  debug_clock = !debug_clock;
  digitalWrite(DEBUG_CLOCK, debug_clock);
}


void configure_interrupts(){
  attachInterrupt(digitalPinToInterrupt(GET_CTRL_REG),get_ctrl_reg,RISING);
//  attachInterrupt(digitalPinToInterrupt(QSPIN),get_QS_button,FALLING);
  attachInterrupt(digitalPinToInterrupt(DEBUG_RDY),get_debug,RISING);
//
  interrupts();
  log_1("ℹ️ Comm clock: %d", comm_clk);
  reseted = false;
  set_SDLed(LED_OFF);
  

  setup_dac(); 
  mixer_init(TIM1); 
  log_1("✅ Interrupts enabled");
}

bool read_wait_serial_jumper() {
  pinMode(WAIT_SERIAL, INPUT_PULLUP);
  delayMicroseconds(10);                    // asentamiento
  return (digitalRead(WAIT_SERIAL) == LOW); // true = jumper puenteado a GND = saltar la espera
}

void setup() {
  // SCB->VTOR = 0x0800C000;
  // pinMode(SD_LED, OUTPUT);
  // set_SDLed(LED_ON);
  // while(1);
  // __enable_irq();

  pinMode(ST_LED_R, OUTPUT);
  pinMode(ST_LED_G, OUTPUT);
  pinMode(ST_LED_B, OUTPUT);

  set_blinking(clRED, 4);
  //asm(".global _printf_float"); // Link FP-enabled printf library
  Serial.begin(SERIAL_SPEED);

  bool skip_wait = read_wait_serial_jumper();
  while (!Serial && !skip_wait);     // Espera a que el puerto serial esté listo o el jumper de deshabilitacion de la espera puesto
  log_0("ℹ️ INIT");
  start_reset_Z80();
  set_blinking(clORANGE, 4);

  pinMode(A18b, INPUT);
  pinMode(A17b, INPUT);
  pinMode(A16b, INPUT);
  pinMode(A15b, INPUT);
  pinMode(A14b, INPUT);
  pinMode(A13b, INPUT);
  pinMode(A12b, INPUT);
  pinMode(A11b, INPUT);
  pinMode(A10b, INPUT);
  pinMode(A9b, INPUT);
  pinMode(A8b, INPUT);
  pinMode(FLASH_MOSI, INPUT);
  pinMode(FLASH_MISO, INPUT);
  pinMode(FLASH_SCLK, INPUT);
  pinMode(FLASH_CS, INPUT);

  pinMode(FPGA_DONE, INPUT);
  pinMode(FPGAPROG, OUTPUT);
  digitalWrite(FPGAPROG, LOW);
  delay(100);
  digitalWrite(FPGAPROG, HIGH);

  pinMode(Z80_RESET, OUTPUT);
  digitalWrite(Z80_RESET, LOW);
  pinMode(FPGA_RESET, OUTPUT);
  set_blinking(clPINK, 4);

 
  pinMode(LED_BUILTIN, OUTPUT);

  pinMode(nWAIT, INPUT);
  pinMode(CTRL_CLK, OUTPUT);
  pinMode(nOE_OL, OUTPUT);
  pinMode(DEBUG_RDY, INPUT);

  pinMode(RST_CTRL_REG, OUTPUT);
  pinMode(RST_DATA_REG, OUTPUT);
  pinMode(GET_CTRL_REG, INPUT);
  pinMode(GET_DATA_REG, INPUT);

  pinMode(CFG_DATA, OUTPUT);
  pinMode(CFG_RESET, OUTPUT);
  pinMode(CFG_CLK, OUTPUT);

  pinMode(nWRb, INPUT);

  pinMode(DEBUG_CLOCK, OUTPUT);
  pinMode(QSPIN, INPUT_PULLUP);

  pinMode(SD_LED, OUTPUT);
  debug_clock = 0;
  digitalWrite(DEBUG_CLOCK, debug_clock);
  
  ioLatch->MODER = 0b0000000011111111; // input-output port
  for (int i = 0; i < sizeof(OUTPUT_LATCH) / sizeof(OUTPUT_LATCH[0]); i++) {
    pinMode(OUTPUT_LATCH[i], OUTPUT);
  }
  
  for (int i = 0; i < sizeof(INPUT_LATCH) / sizeof(INPUT_LATCH[0]); i++) {
    pinMode(INPUT_LATCH[i], INPUT);
  }
  
  digitalWrite(nOE_OL, LOW);



  Serial.print("ℹ️ System core clock frequency (HCLK): ");
  Serial.print(SystemCoreClock / 1000000.0, 2); // Divide por 1,000,000 para MHz, con 2 decimales
  Serial.println(" MHz");

  Serial.print("ℹ️ High speed external clock (HSE): ");
  Serial.print(HSE_VALUE / 1000000.0, 2); 
  Serial.println(" MHz (crystal value)");
  
  Serial.print("ℹ️ Low speed external clock (LSE): ");
  Serial.print(LSE_VALUE / 1000.0, 3); // En kHz con 3 decimales
  Serial.println(" kHz (crystal value)");

  showTemp();
  
  if (!SD_Init()) LED_error(clBLUE);

  digitalWrite(FPGAPROG, LOW);
  flash_begin();
  Serial.print("Flash Jedec ID: ");
  Serial.println(flash_read_jedec_id(), HEX);  // debe imprimir EF4018
  flash_end();
  char s[] = "/SD81.MCS";
  FlashUpdateStatus st = flash_update_from_mcs(s);
  if (st == FLASHUPD_NO_FILE) {
    log_1("ℹ️ No %s on SD, nothing to update.", s);
  } else {
    Serial.print("flash_update status: "); Serial.println(st);
    if (st == FLASHUPD_OK) {
      if (sd.remove(s)) Serial.println("file erased.");
      else Serial.println("❌ Error deleting file.");

      // Forzar un ciclo COMPLETO de reconfiguracion de la FPGA aqui mismo:
      // la FPGA sigue corriendo con el bitstream viejo en este punto. NO
      // hace falta resetear el STM32 despues: en este punto del setup()
      // aun no se ha tocado nada persistente de la MCU (RAM, interrupciones,
      // DAC/mixer, RTC, VBAT vienen todos despues), asi que basta con que
      // la FPGA quede reconfigurada y el propio setup() continua normal.
   }
    // On any error status: leave the file in place (retry next boot) and
    // continue the normal boot with whatever bitstream is already loaded.
  }

  Serial.println("ℹ️ Reconfiguring FPGA...");
  delay(100);
  digitalWrite(FPGAPROG, HIGH);
  while (!digitalRead(FPGA_DONE));
  Serial.println("✅ FPGA reconfigured.");
 
  set_SDLed(LED_OFF);

  enableMem();
  int status = load_ROM("/SYS/SDBOOST.ROM");
  disableMem();
  if (status == -1) LED_error(clORANGE);      // memory error
  else if (status == -2) LED_error(clBLUE);   // error openning ROM file
  set_blinking(clBLUE, 4);
  _rst_ctrl_reg(HIGH);
  _rst_data_reg(HIGH);
  // define comms functions

  wifi_handler_init();
  
  send_config();  
  // UP=Q DOWN=A LEFT=P RIGHT=O FIRE=SPACE
  send_joycfg("\x36\x26\x34\x35\x00");                

  configure_interrupts();
//  playing_wav = false;
  set_blinking(clYELLOW, 4);
  rtc_init();
  Serial_print_time();

  vbat_init();
 float bat_level = get_battery_level();
// uint8_t bat_byte = get_battery_byte();
if (bat_level > GOOD_BAT_LEVEL) Serial.print("✅ "); else if (bat_level > MIN_BAT_LEVEL) Serial.print("⚠️ "); else Serial.print("❌ "); 
 Serial.print("Battery level: "); Serial.print(bat_level);Serial.println("V");
// Serial.print("Battery level: "); Serial.println(bat_byte);
//  log_0("Battery level: %.2f \n\r",bat_level);

  set_blinking(clPINK, 4);
  log_0("ℹ️ reseting Z80");
  digitalWrite(FPGA_RESET, HIGH);
  delay(100);
  digitalWrite(Z80_RESET, HIGH);

  // Invalidar la IP publicada en el arranque anterior: si el ESP32 no llega
  // a conectarse esta vez (o no esta presente), no debe quedar una IP vieja
  // que LOAD THEN PRINT "*IP" siga mostrando como si fuera valida. Se
  // sobreescribe con un texto de aviso en vez de borrar el fichero.
  // Colocado DESPUES de liberar el reset del Z80 a proposito: si esta
  // escritura a SD fallase o tardase (p.ej. "/MAN" no existe todavia), no
  // debe poder bloquear nunca el arranque del Z80 (causo justo eso antes:
  // "/MAN" no se creaba en ningun sitio, sin video hasta comentarlo).
  if (!sd.exists("/MAN")) sd.mkdir("/MAN");
  {
    FsFile ipf;
    if (ipf.open("/MAN/IP.TXT", O_WRONLY | O_CREAT | O_TRUNC)) {
      ipf.print("NO CONNEXION");
      ipf.close();
    }
  }

  set_blinking_off();
  set_status_led_ok();
}

#define MAX_COMMAND_LENGTH 256 // Aumentar tamaño para múltiples cadenas
char serial_command_buffer[MAX_COMMAND_LENGTH + 1]; 
char speech_command_buffer[MAX_COMMAND_LENGTH + 1]; 
char wav_command_buffer[MAX_COMMAND_LENGTH + 1]; 
char play_command_buffer[MAX_COMMAND_LENGTH + 1]; 
char vgm_command_buffer[MAX_COMMAND_LENGTH + 1]; 
int serial_buffer_idx = 0;
uint8_t debug_cmd = 0;

// Función auxiliar para extraer una subcadena entre comillas o hasta una coma
// Retorna un puntero al inicio de la subcadena, y termina la subcadena con '\0'
// Actualiza el 'current_pos' al final de la subcadena + coma/comillas
char* extract_quoted_string(char* input_string, int* current_pos) {
    char* start_ptr = input_string + *current_pos;

    // Saltar espacios y comas iniciales
    while (*start_ptr == ' ' || *start_ptr == '\t' || *start_ptr == ',') {
        start_ptr++;
    }

    if (*start_ptr == '"') { // Si empieza con comilla
        start_ptr++; // Mover el puntero después de la comilla de apertura
        char* end_ptr = start_ptr;
        while (*end_ptr != '"' && *end_ptr != '\0') {
            end_ptr++;
        }
        if (*end_ptr == '"') {
            *end_ptr = '\0'; // Terminar la subcadena
            *current_pos = (end_ptr - input_string) + 1; // Mover pos al final de la comilla de cierre
            return start_ptr;
        }
    } else { // Si no hay comillas, leer hasta la coma o fin de cadena
        char* end_ptr = start_ptr;
        while (*end_ptr != ',' && *end_ptr != '\0') {
            end_ptr++;
        }
        char temp_char = *end_ptr; // Guardar el carácter original
        *end_ptr = '\0'; // Terminar la subcadena
        *current_pos = (end_ptr - input_string); // Mover pos al final de la subcadena
        if (temp_char == ',') { // Si se detuvo por una coma, avanza una posición más
             (*current_pos)++;
        }
        return start_ptr;
    }
    return nullptr; // No se encontró una cadena válida
}

uint8_t opcode_buffer[NUM_OPCODES];
uint16_t addr_buffer[NUM_OPCODES];
uint8_t signals_buffer[NUM_OPCODES];

void process_serial_commands(void){
  while (Serial.available()) {
      char inChar = Serial.read();

      if (inChar == '\n' || inChar == '\r') {
          if (serial_buffer_idx > 0) {
              serial_command_buffer[serial_buffer_idx] = '\0'; 

              //   if (strncmp(serial_command_buffer, "DEBUG", 5) == 0){
              //     debug_cmd = 3;
              //     send_debug_params(0, 0);
              //     Serial.println("DEBUG");
              //  } else 
               if (strncmp(serial_command_buffer, "DBG_PURGE", 9) == 0) {
                  debug_index = 0;
               } else
               if (strncmp(serial_command_buffer, "DBG_DUMP", 8) == 0) {
                  Serial.println("DEBUG DUMP");
                  Serial.print("DATA LENGTH="); Serial.println(debug_index);
                  for (int i=0; i<debug_index;i+=OPCODE_WIDTH){
                    Serial.print(i/OPCODE_WIDTH);
                    Serial.print(" Addr($");
                    Serial.print(debug_info[i]*256+debug_info[i+1],HEX);
                    Serial.print(")=$");
                    Serial.print(debug_info[i+2],HEX);
                    Serial.print(" Signals - $");
                    Serial.print(debug_info[i+3],HEX);
                    if ((debug_info[i+3] & 128)==0) Serial.print(" /M1");
                    if ((debug_info[i+3] & 64)==0) Serial.print(" /RD");
                    if ((debug_info[i+3] & 32)==0) Serial.print(" /WR");
                    if ((debug_info[i+3] & 16)==0) Serial.print(" /RFSH");
                    if ((debug_info[i+3] & 8)==0) Serial.print(" /IORQ");
                    Serial.print(" State="); Serial.println(debug_info[i+3] & 7);
                  }
                  //debug_index = 0;
               } else
               if (strncmp(serial_command_buffer, "DBG_DIS", 7) == 0) {
                  Serial.println("DEBUG DISASSEMBLE");
                  Serial.print("DATA LENGTH="); Serial.println(debug_index);
                  int i,j;
                  for (i=j=0; i<debug_index;i+=OPCODE_WIDTH){
                    if ((debug_info[i+3]&16)!=0) {      // discard refresh access
                      addr_buffer[j] = debug_info[i]*256+debug_info[i+1];
                      opcode_buffer[j] = debug_info[i+2];
                      signals_buffer[j] = debug_info[i+3];
                      j++;
                    }
                  }
                  int num_opcodes = j;
                  char text[100];
                  char bb[100];
                  int bytesUsed = 0;
                  for (int i=0; i<num_opcodes;i+=bytesUsed){
                    if ((signals_buffer[i]&128)==0){    // search opcode (/M1 signal present)
                      sprintf(bb, "(%02d) ", i);
                      Serial.print(bb);
                      bytesUsed = Z80Disassembler::disassemble(text, opcode_buffer+i,num_opcodes-i);
                      sprintf(bb, "%04x ", addr_buffer[i]);
                      Serial.print(bb);
                      for (int j=0; j<bytesUsed;j++){
                        sprintf(bb, "%02x", opcode_buffer[j+i]);
                        Serial.print(bb);
                      }
                      for (int j=0; j<7-bytesUsed; j++) Serial.print("  ");
                      Serial.print(" ");
                      Serial.println(text);
                    } else bytesUsed = 1;
                  }
               } else
               if (strncmp(serial_command_buffer, "CLK", 3) == 0) {
                  Serial.println(debug_clock);
               } else
               if (strncmp(serial_command_buffer, "ICLK", 3) == 0) {
                  debug_clock = !debug_clock;  
                  digitalWrite(DEBUG_CLOCK, debug_clock);
                  Serial.println(debug_clock);
               } else

               if (strncmp(serial_command_buffer, "PLAY ", 5) == 0) {
                  strcpy(play_command_buffer,serial_command_buffer);
                  char* command_str_raw = &play_command_buffer[5];
                  int pos = 0;
                  
                  char* cmd_A = extract_quoted_string(command_str_raw, &pos);
                  char* cmd_B = extract_quoted_string(command_str_raw, &pos);
                  char* cmd_C = extract_quoted_string(command_str_raw, &pos);

                  // Asegurarse de que las cadenas no sean nulas si alguna falta
                  if (!cmd_A) cmd_A = "";
                  if (!cmd_B) cmd_B = "";
                  if (!cmd_C) cmd_C = "";

                  log_2("A=%s B=%s C=%s",cmd_A,cmd_B,cmd_C);

                  play_parser_start(cmd_A, cmd_B, cmd_C);

              } else if (strncmp(serial_command_buffer, "SPEECH ", 7) == 0){
                  strcpy(speech_command_buffer,serial_command_buffer);
                  int pos = 0;
                  char* command_str_raw = &speech_command_buffer[7];
                  char* cmd_speech = extract_quoted_string(command_str_raw, &pos);
                  if (!cmd_speech) cmd_speech = "";
                  Speech(cmd_speech); // ¡Esto iniciará la voz!
              } else if (strncmp(serial_command_buffer, "PLAYWAV ", 8) == 0){
                  strcpy(wav_command_buffer,serial_command_buffer);
                  int pos = 0;
                  char* command_str_raw = &wav_command_buffer[8];
                  char* cmd_wav = extract_quoted_string(command_str_raw, &pos);
                  if (!cmd_wav) cmd_wav = "";
                  playWavFile(cmd_wav);
              } else if (strncmp(serial_command_buffer, "PLAYVGM ", 8) == 0){
                  strcpy(vgm_command_buffer,serial_command_buffer);
                  int pos = 0;
                  char* command_str_raw = &serial_command_buffer[8];
                  char* cmd_vgm = extract_quoted_string(command_str_raw, &pos);
                  if (!cmd_vgm) cmd_vgm = "";
                  int error_code = openVGM(cmd_vgm);
                  playing_VGM = true;
                  if (error_code==0){
                    log_0("VGM \"%s\" started \n\r",cmd_vgm);
                  } else {
                    log_0("error opening VGM \"%s\" \n\r",cmd_vgm);
                  }
              } else if (strncmp(serial_command_buffer, "DATETIME", 8) == 0){
                  if (serial_command_buffer[8]==' '){
                    char* command_str_raw = &serial_command_buffer[9];
                        // Conversión a std::string
                    String dateTimeString = command_str_raw;
                    int firstColon = dateTimeString.indexOf(':');
                    int secondColon = dateTimeString.indexOf(':', firstColon + 1);
                    int spaceIndex = dateTimeString.indexOf(' ');
                    int firstSlash = dateTimeString.indexOf('/');
                    int secondSlash = dateTimeString.indexOf('/', firstSlash + 1);
                    // Si se encuentran todos los separadores, intentar parsear
                    if (firstColon != -1 && secondColon != -1 && spaceIndex != -1 &&
                        firstSlash != -1 && secondSlash != -1) {

                      int hours = dateTimeString.substring(0, firstColon).toInt();
                      int minutes = dateTimeString.substring(firstColon + 1, secondColon).toInt();
                      int seconds = dateTimeString.substring(secondColon + 1, spaceIndex).toInt();

                      int day = dateTimeString.substring(spaceIndex + 1, firstSlash).toInt();
                      int month = dateTimeString.substring(firstSlash + 1, secondSlash).toInt();
                      int year = dateTimeString.substring(secondSlash + 1).toInt(); // El año lo lee como dos dígitos (ej: 24 para 2024)

                      // Validar rangos básicos 
                      if (hours >= 0 && hours < 24 &&
                          minutes >= 0 && minutes < 60 &&
                          seconds >= 0 && seconds < 60 &&
                          day >= 1 && day <= 31 &&
                          month >= 1 && month <= 12 &&
                          year >= 0 && year <= 99) { // Asume año de dos dígitos

                        // Establecer la nueva hora y fecha
                        rtc.setHours(hours);
                        rtc.setMinutes(minutes);
                        rtc.setSeconds(seconds);
                        rtc.setTime(hours, minutes, seconds); // Actualiza la hora

                        rtc.setDay(day);
                        rtc.setMonth(month);
                        rtc.setYear(year);
                        rtc.setDate(day, month, year); // Actualiza la fecha

                        Serial.println("✅ Reloj actualizado con exito desde el terminal serie.");

                        // Opcional: Volver a escribir la bandera de inicialización para asegurar
                        // RTC_BACKUP_REGISTER0 = RTC_INITIALIZED_FLAG_VALUE;
                      } else {
                        Serial.println("❌ Error: Valores de fecha/hora fuera de rango.");
                      }
                    }
                  }
                  Serial_print_time();
              } else if (strncmp(serial_command_buffer, "IP", 2) == 0){
                SdFile f;
                char ipfile[]="/MAN/IP.TXT";
                if (sd.exists(ipfile)) {
                  if (!f.open(ipfile, O_READ)) {
                    Serial.print("Error abriendo"); Serial.println(ipfile);
                    return;
                  }
                  char linea[100];
                  int n;
                  while ((n = f.fgets(linea, sizeof(linea))) > 0) {
                    Serial.print(linea);
                  }
                  f.close();
                }
              } 
              else {
                  Serial.print("Unknown command: ");
                  Serial.println(serial_command_buffer);
              }
              serial_buffer_idx = 0; 
          }
      } else {
          if (serial_buffer_idx < MAX_COMMAND_LENGTH) {
              serial_command_buffer[serial_buffer_idx++] = inChar;
          } else {
              Serial.println("Command buffer full!");
              serial_buffer_idx = 0; 
          }
      }
  }
}

void loop() { // Loop for test PLAY command
    // Actualiza el parser de música y el emulador AY en cada ciclo
  setjmp(jbuf);
  if (reseted){
//    configure_interrupts();
    reseted = false;
    //AY_stop();
    Sfile.close();
    Dfile.close();
    dir.close();
    //VGMFile.close();
    TmpFile.close();
    // Cierre de los handles de acceso aleatorio (CP/M), diferido aqui desde
    // get_ctrl_reg() (COMMS.cpp) para no tocar la SD/SPI dentro de una ISR.
    for (int i = 0; i < 4; i++) {
      if (f_opened[i]) { f_handle[i].close(); f_opened[i] = false; }
    }
    delay(100);
  }
  QS_pressed = !digitalRead(QSPIN);
  if (QS_pressed) {// && ((millis()-QS_pressed_time) > QS_debounce_time) ) {
    nQS_en = !nQS_en;
    send_bit_config(cfgcmd_QSEN,nQS_en?1:0);
    set_status_led_ok();
    while (!digitalRead(QSPIN)) delay(100); // wait no button
  }
  // if (debug_data_rdy){
  //   Serial.print("DEBUG DATA: "); Serial.println(debug_data);
  //   if (debug_cmd < 14) {
  //     debug_info[debug_cmd-4] = debug_data;
  //     debug_cmd++;
  //     send_bit_config(debug_cmd, 0);
  //    } else {  
  //     debug_cmd = 0;
  //    }
  //   debug_data_rdy = false;
  // }
  set_SDLed(LED_OFF);
  data_ready = false;
  read_data();
  // call to active command handler
  if (command_active<=LAST_COMMAND) {
    log_1("ℹ️ COMMAND=%d",command_active);
    (*commands[command_active])();
    log_1("ℹ️ AFTER COMMAND=%d",command_active);
  } else {
    if (command_active != CMD_IDLE){
      log_1("⚠️ Unknown command: %d",command_active);
      command_active = CMD_IDLE;
    }
  }
  process_serial_commands();
  wifi_handler_poll();
  play_parser_update();
  idle_tasks(); 
}

// void loop_bak() {
//   setjmp(jbuf);
//   if (reseted){
//     reseted = false;
//     //AY_stop();
//     Sfile.close();
//     Dfile.close();
//     dir.close();
//     //VGMFile.close();
//     TmpFile.close();
//     // if (comm_clk) GPIOR0 |= 1;
//     // else GPIOR0 &= B11111110;
//     //noInterrupts();
//     delay(100);

// //    SdFile FPEG;
// //    FPEG.open("/EFECT.bin");
// //    FPEG.read(&PEG_mem[0],FPEG.size());
// //    FPEG.close();
   
  
//     //PEG_pc[0] = 0;
//     //playing_PEG[0] = false;
//     //__asm("jmp 0");
//   }
//   set_SDLed(LED_OFF);
//   data_ready = false;
//   read_data();
//   // call to active command handler
//   if (command_active<=LAST_COMMAND) {
//     log_1("COMMAND=%d",command_active);
//     (*commands[command_active])();
//     log_1("AFTER COMMAND=%d",command_active);
//   } else {
//     if (command_active != CMD_IDLE){
//       log_1("Unknown command: %d",command_active);
//       command_active = CMD_IDLE;
//     }
//   }

// //  xmodem.receive();
//   idle_tasks();
//   // handle serial commands
// //   if (Serial.available()) {
// //     char ch = Serial.read();
// //     //Serial.println(ch);
// //     if (ch=='R') { digitalWrite(Z80_RESET, LOW); delay(1); digitalWrite(Z80_RESET, HIGH); }
// //     if (ch=='S') { 
// //       ToggleClock();
// //     }
// // //    if (ch=='P') {  digitalWrite(ROM_WR_EN, HIGH);}
// // //    if (ch=='p') {  digitalWrite(ROM_WR_EN, LOW);}
// //     if (ch=='M') {  digitalWrite(EN_M1_NOT, HIGH);}
// //     if (ch=='m') {  digitalWrite(EN_M1_NOT, LOW);}
// //     if (ch=='G') {  showHex((unsigned char*) PEG_mem);}
// //     if (ch=='W') {  playWavFile("/coche/race.WAV");}

// //     if (ch=='V') { 
// //       Serial.print("millis="); Serial.print(millis()); Serial.print(" tiVGM="); Serial.println(ti_VGM); 
// //       if (openVGM("/music0002.vgm")==0) playing_VGM = true; }

// //     if (ch=='J') { 
// //       Serial.print("playing="); Serial.print(playing_VGM); Serial.print(" millis="); Serial.print(millis()); Serial.print(" tiVGM="); Serial.println(ti_VGM); }

// //   }
// }
