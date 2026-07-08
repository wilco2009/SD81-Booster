#include "RTC.h"

STM32RTC& rtc = STM32RTC::getInstance(); // Obtiene la instancia del objeto RTC

// Definir un valor "mágico" para nuestra bandera
#define RTC_INITIALIZED_FLAG_VALUE 0x32F1 // Un número aleatorio para identificarlo

// --- DEFINICIONES PARA ACCESO DIRECTO A REGISTROS DE RESPALDO (ULTIMO RECURSO) ---
// Dirección de memoria del primer registro de datos de respaldo (BKP_DR0) para STM32F4xx
#define RTC_BKP_DR0_ADDRESS ((uint32_t)0x40002850)
// Macro para acceder al valor en esa dirección de memoria (se asegura que sea volátil)
#define RTC_BACKUP_REGISTER0 (*((volatile uint32_t *)RTC_BKP_DR0_ADDRESS))

bool isTime(int hours, int minutes, int seconds, int hundredths){
  return (hours >=0) && (hours <= 23) && (minutes >=0) && (minutes <= 59) && (seconds >=0) && (seconds<=59)&&(hundredths>=0)&&(hundredths<=99);
}

bool leapYear(int year){
  // Un año es bisiesto si es divisible por 400
  // O si es divisible por 4 pero NO por 100.
  return ((year % 400 == 0) || ((year % 4 == 0) && (year % 100 != 0)));
}

int maxday(int year, int month){
  int md;
  switch (month){
    case 1:
    case 3:
    case 5:
    case 7:
    case 8:
    case 10:
    case 12: return 31;
    case 4:
    case 6:
    case 9:
    case 11: return 30;
    case 2:  return leapYear(year)?29:28;
    default: 0;
  }
}

bool isDate(int year, int month, int day){
  int md = maxday(year,month);
  return (year > 0)&&(year <100)
    &&  (month > 0)&&(month<13)
    &&  (day > 0)&&(day<=md);
}

// returns a datetime YYYY-MM-DD hh:mm:ss.cc
void rtc_get_time(char* s){
    int year_4digits = rtc.getYear()+2000;
    int month = rtc.getMonth();
    int day = rtc.getDay();
    int hours = rtc.getHours();
    int minutes = rtc.getMinutes();
    int seconds = rtc.getSeconds();
    int hundredths = rtc.getSubSeconds()/10;
    // int sub = rtc.getSubSeconds(); // 0..pre (cuenta hacia 0)
    // int hundredths = (( (pre - sub) * 100u ) + ((pre + 1u)/2u)) / (pre + 1u); // 0..99

    sprintf(s,"%04d-%02d-%02d %02d:%02d:%02d.%02d",year_4digits,month,day,hours,minutes,seconds,hundredths);
}

void Serial_print_time(void){
    int year_2digits = rtc.getYear();
    int month = rtc.getMonth();
    int day = rtc.getDay();
    int hours = rtc.getHours();
    int minutes = rtc.getMinutes();
    int seconds = rtc.getSeconds();

    int full_year = year_2digits + 2000;

    Serial.print("ℹ️ Date: ");
    Serial.print(day);
    Serial.print("/");
    Serial.print(month);
    Serial.print("/");
    Serial.print(full_year);
    Serial.print("  Time: ");
    Serial.print(hours);
    Serial.print(":");
    Serial.print(minutes);
    Serial.print(":");
    Serial.println(seconds);
  }

void rtc_init(void){
  Serial.println("ℹ️ Starting configuration and querying RTC date and time...\n\r");
  // Habilitar acceso al dominio de respaldo (NECESARIO para acceder a registros del RTC/reloj)
  HAL_PWR_EnableBkUpAccess();

  // --- OPCIONAL/AVANZADO: Forzar un reset del dominio de respaldo ---
  // Esto borrará todos los datos de respaldo y la configuración del RTC.
  // ÚSALO SOLO SI LOS PROBLEMAS PERSISTEN Y NECESITAS UN REINICIO COMPLETO.
  // Una vez que funcione, deberías comentar estas líneas.
  // Serial.println("Forzando reset del dominio de respaldo...");
  // __HAL_RCC_BACKUPRESET_FORCE(); // Aplica el reset
  // delay(10);
  // __HAL_RCC_BACKUPRESET_RELEASE(); // Libera el reset
  // Serial.println("Reset del dominio de respaldo completado.");

  // Forzar el uso del LSE como fuente de reloj del RTC
  rtc.setClockSource(STM32RTC::LSE_CLOCK);

  // Intentar iniciar el RTC. Esto debería habilitar el reloj y establecer la fuente.
  rtc.begin(); // Esto llamará a HAL_RTC_Init() internamente, que debería habilitar RTCEN.

  // --- Sección para verificar los relojes y el RTCEN usando librerías HAL ---

  uint32_t rtc_clock_source = __HAL_RCC_GET_RTC_SOURCE();

  if (rtc_clock_source == RCC_RTCCLKSOURCE_LSI) {
//    Serial.println("Fuente de reloj del RTC configurada como LSI.");
    Serial.print("⚠️ Clock source configured as LSI.\n\r");
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_LSIRDY) != RESET) {
//      Serial.println("⚠️ El oscilador LSI esta activo y estable.");
      Serial.print("⚠️ LSI oscillator is active and stable.\n\r");
    } else {
//      Serial.println("❗ ADVERTENCIA: LSI seleccionado, pero NO ESTA ESTABLE.");
      Serial.print("❗ WARNING: LSI selected, but not stable.\n\r");
    }
  } else if (rtc_clock_source == RCC_RTCCLKSOURCE_LSE) {
//    Serial.println("Fuente de reloj del RTC configurada como LSE.");
      Serial.print("✅ Clock source configured as LSE.\n\r");
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_LSERDY) != RESET) {
//      Serial.println("✅ El oscilador LSE esta activo y estable.");
      Serial.print("✅ LSE oscillator is active and stable.\n\r");
    } else {
      Serial.print("❗ WARNING: LSE selected, but not stable.\n\r");
//      Serial.println("❗ ADVERTENCIA: LSE seleccionado, pero NO ESTA ESTABLE. Revisa hardware (cristal/condensadores).");
    }
  } else {
    Serial.println("❌ Fuente de reloj del RTC desconocida o no configurada.");
  }

  // Verificar si el reloj del RTC está habilitado en el dominio de respaldo (RTCEN)
  if ((RCC->BDCR & RCC_BDCR_RTCEN) != RESET) {
      //Serial.println("✅ El reloj del RTC (RTCEN) esta habilitado en BDCR.");
      Serial.print("✅ RTC clock is enabled in BDCR.\n\r");
  } else {
      Serial.println("⚠️ ADVERTENCIA: El reloj del RTC (RTCEN) NO esta habilitado. Intentando habilitacion...");
      __HAL_RCC_RTC_ENABLE();
      delay(10);
      if ((RCC->BDCR & RCC_BDCR_RTCEN) != RESET) {
         //Serial.println("RTCEN habilitado con exito.");
         Serial.print("✅ RTCEN successfully enabled.\n\r");
      } else {
//         Serial.println("Fallo al habilitar RTCEN. Problema critico.");
         Serial.print("❌ Failed to enable RTCEN. Critical problem.\n\r");
      }
  }

  // --- USAR EL REGISTRO DE RESPALDO COMO BANDERA (ACCESO DIRECTO A LA DIRECCIÓN) ---
  uint32_t backup_flag_read = RTC_BACKUP_REGISTER0; // Acceso directo a la dirección de memoria

  if (backup_flag_read != RTC_INITIALIZED_FLAG_VALUE) {
    //Serial.println("RTC parece estar desconfigurado (bandera no encontrada). Configurando fecha y hora...");
    Serial.print("⚠️ RTC appears to be misconfigured (flag not found). Setting date and time...\n\r");
    
    uint8_t currentHours = 13;
    uint8_t currentMinutes = 43; 
    uint8_t currentSeconds = 0;
    uint8_t currentDay = 20;
    uint8_t currentMonth = 7;
    uint8_t currentYear = 25; // Año 2025 -> 25 (dos dígitos)

    rtc.setHours(currentHours);
    rtc.setMinutes(currentMinutes);
    rtc.setSeconds(currentSeconds);
    rtc.setTime(currentHours, currentMinutes, currentSeconds); 

    rtc.setDay(currentDay);
    rtc.setMonth(currentMonth);
    rtc.setYear(currentYear);
    rtc.setDate(currentDay, currentMonth, currentYear); 

    // Serial.print("RTC configurado a ");
    // Serial.print(currentDay); Serial.print("/"); Serial.print(currentMonth); Serial.print("/");
    // Serial.print(currentYear + 2000); // Mostrar el año completo en el mensaje
    // Serial.print(" "); Serial.print(currentHours); Serial.print(":"); Serial.print(currentMinutes); Serial.println(":00.");
    Serial.printf("✅ RTC configured to %d/%d/%d %d:%d:%d \n\r",currentDay,currentMonth,currentYear + 2000,currentHours,currentMinutes,currentSeconds); // Mostrar el año completo en el mensaje

    RTC_BACKUP_REGISTER0 = RTC_INITIALIZED_FLAG_VALUE; // Escribir directamente a la dirección de memoria
  //  Serial.println("Bandera de inicializacion del RTC establecida.");
    Serial.print("✅ RTC initialization flag set. \n\r");
  } else {
    //Serial.println("RTC ya fue inicializado (bandera encontrada). No se reconfigura.");
  }
  Serial.print("✅ RTC initiated and verified.\n\r");
}


