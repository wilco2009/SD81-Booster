#include "transfer.h"
#include "GLOBALS.h"
#include "SD_handle.h"

bool check_serial(){
  char buff[64];
  // log_0("%s",params);
  if ((params[0]=='S') && (params[1]==':')) {   // from serial
    Serial.begin(9600);
    // log_0("ok");
    strcpy(params,params+2); // skip S:
    if (params[0] == 0) strcpy(params,"SERIAL.P");
    param_len = strlen(params);
    boolean opened = Sfile.open(params,O_CREAT|O_WRITE|O_TRUNC);
    uint32_t time_out = INITIAL_TIMEOUT; // maximum 20 seconds to start
    serial_ini = millis();
    serial_end = false;
    uint8_t pos = 0;
    uint16_t bytes = 0;
    while (millis()-serial_ini < time_out){

      if (Serial.available()){
        serial_ini = millis();
        uint8_t b = Serial.read();
        buff[pos] = b;
        if (pos <63){
          pos++;
        } else {
          pos = 0;
          Sfile.write(buff, 64);            
        }
        bytes++;
        time_out = FINAL_TIMEOUT; // one second without tranfer means file end
      }
    }
    Sfile.write(buff, pos);            
    Serial.print(bytes);
    Serial.println(" bytes");
    Sfile.close();
    if (time_out == INITIAL_TIMEOUT) sd.remove("SERIAL.P");
    Serial.begin(SERIAL_SPEED);
    return true;
  } else return false;
}
