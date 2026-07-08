#include "T81.h"
#include "GLOBALS.h"


void getT81File(char* fname, uint32_t *fpos, uint16_t *filesize){
  bool fin = false;
  uint32_t pos = 0;
  SdFile f;
  if (f.open(current_dir)) {
    log_1("T1");
    f.seekSet(4); // skip header
    pos = 4;
    while (!fin) {
      set_SDLed((millis() % 64)>32);
      if (f.read(tmp,0x20)!= 0x20) break;
      pos+=0x20;
      char ascSize[0x10];
      if (f.read(ascSize,0x10)!= 0x10) break; 
      pos+=0x10;
      uint32_t fsize = atoi(ascSize);
      log_1("T2 name=%s pos=%lu size=%u",tmp,pos,fsize);
      if (strcmp(tmp,fname)==0){
        (*fpos) = pos;
        (*filesize) = fsize;
        log_1("match");
        f.close();
        return;
      }
      fin = !f.seekCur(fsize);
    }
    f.close();
  }
}
