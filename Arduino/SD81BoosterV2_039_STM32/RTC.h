#ifndef RTC_H
#define RTC_H

#include "GLOBALS.h"
#include <STM32RTC.h>

extern STM32RTC& rtc;

void rtcInit(void);
void rtc_init(void);
void Serial_print_time(void);
// returns a datetime YYYY-MM-DD hh:mm:ss.cc
void rtc_get_time(char* s);
bool leapYear(int year);
int maxday(int year, int month);
bool isDate(int year, int month, int day);
bool isTime(int hours, int minutes, int seconds, int hundredths);

#endif


