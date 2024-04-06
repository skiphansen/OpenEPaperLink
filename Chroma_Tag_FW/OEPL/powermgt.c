#include "powermgt.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "asmUtil.h"
#include "board.h"
#include "comms.h"
#include "cpu.h"
#include "drawing.h"
#include "eeprom.h"
#include "printf.h"

#define __packed
#include "../oepl-definitions.h"
#include "../oepl-proto.h"

#include "radio.h"
#include "screen.h"
#include "settings.h"
#include "sleep.h"
#include "syncedproto.h"
#include "timer.h"
#include "userinterface.h"
#include "wdt.h"
#include "logging.h"

// Holds the amount of attempts required per data_req/check-in
uint16_t __xdata dataReqAttemptArr[POWER_SAVING_SMOOTHING];  
uint8_t __xdata dataReqAttemptArrayIndex;
uint8_t __xdata dataReqLastAttempt;
uint16_t __xdata nextCheckInFromAP;
uint8_t __xdata wakeUpReason;
uint8_t __xdata scanAttempts;

int8_t __xdata temperature;
uint16_t __xdata batteryVoltage = 2600;   //in mV
__bit lowBattery;
uint16_t __xdata longDataReqCounter;
uint16_t __xdata voltageCheckCounter;

// True if SPI port configured for UART, False when configured for EEPROM
__bit gUartSelected;
// True if EEPROM has been put into the deep power down mode
__bit gEEPROM_PoweredUp;
extern int8_t adcSampleTemperature(void);  // in degrees C


void initPowerSaving(const uint16_t initialValue) 
{
   for(uint8_t c = 0; c < POWER_SAVING_SMOOTHING; c++) {
      dataReqAttemptArr[c] = initialValue;
   }
}


void powerUp(const uint8_t parts) 
{
   if(parts & INIT_BASE) {
      clockingAndIntsInit();
      timerInit();
      irqsOn();
      wdtOn();
      wdt10s();
      PortInit();
      u1init();
   }

#if 0
   if(parts & INIT_EPD) {
      configSPI(true);
      epdConfigGPIO(true);
      epdSetup();
   }
#endif
   if(parts & INIT_EPD_VOLTREADING) {
      batteryVoltage = adcSampleBattery();
   }

// The debug UART and the EEPROM both use USART1 on Chroma devices
// so we can't have both.  The EEPROM has priority
   if(parts & INIT_EEPROM) {
      u1setEepromMode();
      if(!gEEPROM_PoweredUp) {
         EEPROM_LOG("Power up EEPROM\n");
         eepromWakeFromPowerdown();
      }
      else {
         EEPROM_LOG("EEPROM selected\n");
      }
   } 
#if 0
// never set Uart mode here...  maybe for now anyway
   else if((parts & INIT_UART) && !gUartActive) {
      u1setUartMode();
   }
#endif

   if(parts & INIT_RADIO) {
      radioInit();
      radioSetTxPower(10);
      radioSetChannel(currentChannel);
   }
}

void powerDown(const uint8_t parts) 
{
#if 0
   if(parts & INIT_UART) {
      configUART(false);
   }
   if(parts & INIT_RADIO) {  // warning; this also touches some stuff about the EEPROM, apparently. Re-init EEPROM afterwards
      radioRxEnable(false, true);
      RADIO_IRQ4_pending = 0;
      UNK_C1 &= ~0x81;
      TCON &= ~0x20;
      uint8_t __xdata cfgPg = CFGPAGE;
      CFGPAGE = 4;
      RADIO_command = 0xCA;
      RADIO_command = 0xC5;
      CFGPAGE = cfgPg;
   }
#endif
   if((parts & INIT_EEPROM) && gEEPROM_PoweredUp) {
      EEPROM_LOG("Power down EEPROM\n");
      eepromDeepPowerDown();
   }
#if 0
   if(!gEepromActive && !epdGPIOActive) {
      configSPI(false);
   }
#endif
}

// t = sleep time in milliseconds
void doSleep(uint32_t __xdata t) 
{
#ifdef DEBUG_MAX_SLEEP
   if(t > DEBUG_MAX_SLEEP) {
      LOG("Sleep time reduced from %ld",t);
      t = DEBUG_MAX_SLEEP;
      LOG(" to %ld ms\n",t);
   }
#endif

#ifdef DEBUG_SLEEP
   uint32_t hrs = t;
   uint32_t Ms = mathPrvMod32x16(hrs,1000);
   hrs = mathPrvDiv32x16(hrs,1000);
   uint32_t Sec = mathPrvMod32x16(hrs,60);
   hrs = mathPrvDiv32x16(hrs,60);
   uint32_t Mins = mathPrvMod32x16(hrs,60);
   hrs = mathPrvDiv32x16(hrs,60);
   SLEEP_LOG("Sleep for %ld (%ld:%02ld:%02ld.%03ld)",t,hrs,Mins,Sec,Ms);
   SLEEP_LOG("...");
#endif
   if(gEEPROM_PoweredUp) {
      powerDown(INIT_EEPROM);
   }
   screenShutdown();
   powerPortsDownForSleep();

// sleepy time
   sleepForMsec(t);
   powerUp(INIT_BASE);
   SLEEP_LOG("\nAwake\n");
}

void doVoltageReading() 
{
   powerUp(INIT_RADIO);  // load down the battery using the radio to get a good voltage reading
   temperature = adcSampleTemperature();
   powerDown(INIT_RADIO);
}

uint32_t getNextScanSleep(const bool increment) 
{
   if(increment) {
      if(scanAttempts < 255) {
         scanAttempts++;
      }
   }

   if(scanAttempts < INTERVAL_1_ATTEMPTS) {
      return INTERVAL_1_TIME;
   } else if(scanAttempts < (INTERVAL_1_ATTEMPTS + INTERVAL_2_ATTEMPTS)) {
      return INTERVAL_2_TIME;
   }
   return INTERVAL_3_TIME;
}

void addAverageValue() 
{
   uint16_t __xdata curval = INTERVAL_AT_MAX_ATTEMPTS - INTERVAL_BASE;

   curval *= dataReqLastAttempt;
   curval /= DATA_REQ_MAX_ATTEMPTS;
   curval += INTERVAL_BASE;
   dataReqAttemptArr[dataReqAttemptArrayIndex % POWER_SAVING_SMOOTHING] = curval;
   dataReqAttemptArrayIndex++;
}

uint16_t getNextSleep() 
{
   uint16_t avg = 0;
   for(uint8_t c = 0; c < POWER_SAVING_SMOOTHING; c++) {
      avg += dataReqAttemptArr[c];
   }
   avg /= POWER_SAVING_SMOOTHING;

// check if we should sleep longer due to an override in the config
   if(avg < tagSettings.minimumCheckInTime) {
      return tagSettings.minimumCheckInTime;
   }
   return avg;
}