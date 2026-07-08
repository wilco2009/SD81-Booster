class PlayCPLD: public PlayXSVFJTAGArduino{
public:
  PlayCPLD(
    File &s,
    int stream_buffer_size,
   
    uint8_t tms,
    uint8_t tdi,
    uint8_t tdo,
    uint8_t tck,
    uint8_t vref);
  ~PlayCPLD() {}
  void show_progress(int progress);
};

PlayCPLD::PlayCPLD(    File &s,
    int stream_buffer_size,
   
    uint8_t tms,
    uint8_t tdi,
    uint8_t tdo,
    uint8_t tck,
    uint8_t vref): PlayXSVFJTAGArduino(s, stream_buffer_size, tms, tdi, tdo, tck, vref, false)
{
}
void PlayCPLD::show_progress(int progress){
  int status;
  status = progress * 255 / 100;
  if (millis() % 1000 >  500) 
    set_status_LED(255-status,status,0);
  else 
    set_status_LED(0);
  //Serial.println(progress/10);
};

void check_CPLD_update(){
  File fcpld;
  char fname[MAX_FILENAME_LEN];
  sprintf_P(fname, PSTR("%s.XSVF"),CPLD_NAME);
  // sprintf_P(tmp, PSTR("checking if CPLD file %s is present."),fname);
  log_2("checking if CPLD file %s is present.",fname);
  if (fcpld.open(fname)){
    fcpld.close();
    log_0("Programming CPLD...");
    for (int i=0; i<40; i++){
      set_status_LED(clYELLOW);
      delay(250);
      set_status_LED(clPINK);
      delay(250);
    }
    sprintf_P(tmp, PSTR("%s.DONE"),CPLD_NAME);
    sd.remove(tmp);
    sprintf_P(tmp, PSTR("%s.ERROR"),CPLD_NAME);
    sd.remove(tmp);
    sprintf_P(tmp, PSTR("%s.PROCESS"),CPLD_NAME);
    sd.remove(tmp);
    sd.rename(fname, tmp);
    if (fcpld.open(tmp)){
      PlayCPLD p(fcpld, SERIAL_RX_BUFFER_SIZE, TMS, TDI, TDO, TCK, VREF);
      if (p.play()){
        log_0("Done");
        sprintf_P(fname, PSTR("%s.DONE"),CPLD_NAME);
        fcpld.close();
        sd.rename(tmp,fname);
        set_status_LED(clGREEN);
      } else {
        log_0("Error");
        sprintf_P(fname, PSTR("%s.ERROR"),CPLD_NAME);
        fcpld.close();
        sd.rename(tmp,fname);
        set_status_LED(clRED);
      }
    }
    
  } else set_status_LED(clGREEN);
}
