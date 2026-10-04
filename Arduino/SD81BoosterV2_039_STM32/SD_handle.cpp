#include "SD_handle.h"
#include "MEM.h"
#include "COMMS.h"
#include "RTC.h"

#define SD_CONFIG SdSpiConfig(SD_CS_PIN, DEDICATED_SPI, SD_SCK_MHZ(8))
bool SDOK = false;
SdFs sd;

void dateTime(uint16_t* date, uint16_t* time, uint8_t* ms10) {
  int year_4digits = rtc.getYear()+2000;
  int month = rtc.getMonth();
  int day = rtc.getDay();
  int hours = rtc.getHours();
  int minutes = rtc.getMinutes();
  int seconds = rtc.getSeconds();
  int hundredths = rtc.getSubSeconds()/10;

  // Return date using FS_DATE macro to format fields.
  *date = FS_DATE(year_4digits, month, day);

  // Return time using FS_TIME macro to format fields.
  *time = FS_TIME(hours, minutes, seconds);

  // Return low time bits in units of 10 ms, 0 <= ms10 <= 199.
  *ms10 = seconds & 1 ? 100 : 0;
}

boolean SD_Init(void){
  if (!SDOK) {
      // Set callback
    FsDateTime::setCallback(dateTime);
    Serial.print("ℹ️ SdFat version: ");
    Serial.println(SD_FAT_VERSION_STR);
    pinMode(SS_SD, OUTPUT);
    SPI.setMOSI(MOSI);
    SPI.setMISO(MISO);
    SPI.setSCLK(SCLK);
    if (!sd.begin(SD_CONFIG)) {
    //if (!sd.cardBegin(SD_CONFIG)) {
      SDOK = false;
      Serial.println(
             "❌ \nSD initialization failed.\n"
             "Do not reformat the card!\n"
             "Is the card correctly inserted?\n"
             "Is there a wiring/soldering problem?\n");
      if (isSpi(SD_CONFIG)) {
        Serial.println(
             "❌ Is SD_CS_PIN set to the correct value?\n"
             "Does another SPI device need to be disabled?\n"
             );
      }
      log_0("❌ SD initialization failed!");
      return false;
    } else {
      log_0("✅ SD initialization done.");
      SDOK = true;
      return true;
    }
//    SDOK = sd.begin(SD_CONFIG);
    // if (!SDOK) {
    //   log_0("SD initialization failed!");
    //   return false;
    // } else
    //   log_0("SD initialization done.");
  } else log_0("⚠️ SD already initialized.");
  return true;
}

void check_SD(){
    if (!SDOK) {
      strcpy(current_dir, "/");
    }
    SD_Init();
//SdFile dir;
//  if (!dir.open("/")) {
//    log_0("error openning SD card");
//    log_0("Trying to open a new SD card");
//    SDOK = SD_Init();
//    strcpy(current_dir, "/");
//  } else dir.close();
//  
}
boolean absolute_dir(char* fname){
  return fname[0]=='/';
}
boolean ext_present(char* fname){
  int i;
  for (i=strlen(fname)-1; i>=0; i--)
    if (fname[i]=='/') return false;
    else if (fname[i]=='.') return true;
  return false;
}

int remove_dotdirs(char* dest){
  int i,i2;
  byte c=0;
  char tmp[MAX_FILENAME_LEN];
  int len;
  int nd = 0; 

//  serial_printf("1. dest=%s \n\r", dest);
  len = strlen(dest);
  i2 = 0;
  for(i=len-1; i>=0;i--){
//    serial_printf("%d(%c) ",c,dest[i]);
    switch (c) {
      case _DIR_START:
        if (dest[i]=='/') {tmp[i2]=dest[i];i2++;}
        else if (dest[i]!='.') {tmp[i2]=dest[i];i2++; c = _DIR_NAME;}
        else c = _FIRST_DOT;
        break;
      case _DIR_NAME:
        tmp[i2]=dest[i];i2++; 
        if (dest[i]=='/') c = _DIR_START;
        break;
      case _FIRST_DOT: // first dot
        if (dest[i]=='/') {
          if (nd > 0) c = _REMOVE_DIRS;
          else c=_DIR_START;
        }
        else if (dest[i]!='.') {
          if (nd > 0) c = _REMOVE_DIRS;
          else {tmp[i2]='.';i2++;tmp[i2]=dest[i];i2++;c=_DIR_NAME;}
        } else c=_SECOND_DOT;
        break;
      case _SECOND_DOT: // second dot
        if (dest[i]!='/') {tmp[i2]='.';i2++;tmp[i2]='.';i2++;tmp[i2]=dest[i];i2++;c=_DIR_NAME;}
        else {c=_REMOVE_DIRS;nd++;}
        break;
      case _REMOVE_DIRS: // removing previous dirs (nd = number of folders to be removed)
        if (dest[i]=='/') {nd--; if (nd==0) c = _DIR_START;}
        else if (dest[i]=='.') {c = _FIRST_DOT;}
        break;
    }
  }
  if (nd>0) return _DIR_UNDER_ROOT;
  tmp[i2]='\0';
//  serial_printf("2. dest=%s tmp=%s \n\r", dest, tmp);

  len = strlen(tmp);
  for (i=0; i<len;i++){
    dest[i]=tmp[len-i-1];
  }
  dest[i]='\0';

//  serial_printf("3. dest=%s tmp=%s \n\r", dest, tmp);
  return 1;
  
}

int complete_dir(char* dest,char* source){
  int n;
    if (!absolute_dir(source)){
      sprintf_P(dest,PSTR("%s%s"),current_dir,source);
    } else sprintf_P(dest,PSTR("%s"),source);

    int err = remove_dotdirs(dest);
    if (err < 0) return err;

    n = strlen(dest)-1;
    if (dest[n]!='/') {sprintf_P(dest,PSTR("%s/"),dest);}
  
    if (TmpFile.open(dest)){
      if (TmpFile.isDir()){
        TmpFile.close();
        return 1;
      } else {
        TmpFile.close();
        return _NOT_A_DIR;
      } 
    } else return _NOT_EXISTS;
}

char* getfname(char* filename){
  char* str = strrchr(filename, '/');
  if (str==0) return filename;
  else return ++str;
}

void split_fname(char* source, char* dir, char* fname){
  int i;
  int len = strlen(source);
  for (i=len-1; (source[i]!='/')&&(i>=0); i--);
  memcpy(fname,source+i+1,len-i-1);
  fname[len-i-1]='\0';
  memcpy(dir,source,i+1);
  dir[i+1] = '\0';
}

char *get_filename_ext(const char *filename) {
    char *dot = strrchr(filename, '.');
    if(!dot || dot == filename) return "";
    return dot + 1;
}

void complete_fname(char* dest,char* source){
  char ext[]=".BAS";
  char dir[MAX_FILENAME_LEN];
  char fname[MAX_FILENAME_LEN];
  if (ext_present(source)) ext[0]='\0';
//  if (!absolute_dir(source)){
//    sprintf(dest,"%s%s%s",current_dir,source,ext);
//  } else 
  sprintf_P(dest,PSTR("%s%s"),source,ext);
  split_fname(dest,dir,fname);
  complete_dir(dest,dir);
  sprintf_P(dest,PSTR("%s%s"),dest,fname);
}

// Depurador por hardware (claude/planning/hw_debugger_plan.md). El monitor
// vive en la pagina 63 y el Z80 lo ve en $2000-$3FFF. El MCU solo tiene 16
// lineas de direccion: con la orden 9 ("monitor cargado") la FPGA pone a 1
// las lineas A16-A18 de la SRAM cuando el MCU escribe en $E000-$FFFF, asi
// que escribiendo ahi se llega a la pagina 63 (y no se copia a la BRAM). La
// misma orden arma el depurador; los registros de configuracion no se
// borran con el reset, asi que basta con mandarla una vez.
bool debug_monitor_loaded = false;

int load_debug_monitor(void){
  FsFile f;
  // En el arranque esta es la primera orden del canal de configuracion, y
  // CFG_RESET sigue bajo desde el pinMode: con el reset sujeto la FPGA no
  // desplaza los bits y la orden se perderia. rst_config() lo deja alto
  // (como hace send_config() al empezar).
  rst_config();
  if (!f.open(DEBUG_MONITOR_FILE)) {
    send_bit_config(cfgcmd_DBGLOADED, 0);
    debug_monitor_loaded = false;
    log_2("ℹ️ No debug monitor (%s)", DEBUG_MONITOR_FILE);
    return 0;
  }
  uint32_t size = f.fileSize();
  if (size > 8192) {
    f.close();
    send_bit_config(cfgcmd_DBGLOADED, 0);
    debug_monitor_loaded = false;
    log_0("❌ %s is larger than 8K", DEBUG_MONITOR_FILE);
    return -1;
  }
  send_bit_config(cfgcmd_DBGLOADED, 1);
  send_bit_config(cfgcmd_DBGTRACE, 1);   // el historial (traza de la FPGA), siempre puesto
  for (uint32_t i = 0; i < size; i++)
    write_sram(0xE000 + i, f.read());
  f.close();
  debug_monitor_loaded = true;
  log_1("✅ Debug monitor loaded (%d bytes, page 63)", (int)size);
  return 1;
}

// --- Snapshots del depurador (DEBUGGER.cpp) ---
static FsFile snapf;
bool snapfile_open(const char* path){ return snapf.open(path, O_WRONLY | O_CREAT | O_TRUNC); }
bool snapfile_write(const void* data, uint16_t n){ return snapf.write(data, n) == n; }
void snapfile_close(void){ snapf.close(); }
bool snapfile_exists(const char* path){ return sd.exists(path); }
int32_t rom_file_read(uint32_t offset, uint8_t* buf, uint16_t n){
  FsFile f;
  if (!f.open("/SYS/SDBOOST.ROM")) return -1;
  int32_t r;
  if (n == 0) r = f.fileSize();
  else { f.seekSet(offset); r = f.read(buf, n); }
  f.close();
  return r;
}

// --- El .Z81 que carga el depurador (se lee byte a byte: con buffer) ---
static FsFile z81in;
static uint8_t z81buf[512];
static uint16_t z81len = 0, z81idx = 0;
static uint32_t z81base = 0;                  // posicion en el fichero de z81buf[0]
bool z81in_open(const char* path){
  if (z81in.isOpen()) z81in.close();
  z81len = z81idx = 0; z81base = 0;
  return z81in.open(path, O_RDONLY);
}
int z81in_read(void){
  if (z81idx >= z81len) {
    z81base += z81len;
    z81len = z81idx = 0;
    int n = z81in.read(z81buf, sizeof(z81buf));
    if (n <= 0) return -1;
    z81len = n;
  }
  return z81buf[z81idx++];
}
bool z81in_seek(uint32_t pos){ z81len = z81idx = 0; z81base = pos; return z81in.seekSet(pos); }
uint32_t z81in_pos(void){ return z81base + z81idx; }
void z81in_close(void){ if (z81in.isOpen()) z81in.close(); }

int load_ROM(char* rom_file){
//  SdFile f;
FsFile f;
  long startTime, stopTime;
  log_2("ℹ️ Reading ROM file \"%s\"",rom_file);
  startTime = millis();
  if (f.open(rom_file)){
    extmem = (uint8_t*) malloc(f.fileSize());
    f.read(extmem,f.fileSize());
//    f.seekSet(0);
//    f.read(extmem+8192,f.fileSize());
  for (int i=0;i<f.fileSize();i++){
    write_sram(i,extmem[i]);
    write_sram(i+8192,extmem[i]);
  }
#ifdef CHECK_ROM
    f.seekSet(0);
    for (int i=0; i< f.fileSize(); i++){
      uint8_t b = f.read();
      uint8_t a = read_sram(i);
      if (a != b) {
        log_0("❌ Error in mem pos=%d write=%d read=%d ",i, b, a);
        return -1;
      }
    }
#endif
    stopTime = millis();
    log_2("✅ ROM file \"%s\" read to memory in %d ms",rom_file,stopTime-startTime);
    f.close();
    free(extmem);
  } else {
    log_0("❌ Error openning ROM file");
    return -2;
  }
  return 0;
}
