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
#include "adc.h"
#include "logging.h"

// Holds the amount of attempts required per data_req/check-in
uint16_t __xdata dataReqAttemptArr[POWER_SAVING_SMOOTHING];  
uint8_t __xdata dataReqAttemptArrayIndex;
uint8_t __xdata dataReqLastAttempt;
uint16_t __xdata nextCheckInFromAP;
uint8_t __xdata wakeUpReason;
uint8_t __xdata scanAttempts;

// Battery voltage immediately after last boot
uint16_t __xdata gBootBattV;

// Minimum battery voltage seen while transmitting
uint16_t __xdata gTxBattV;

// Minimum battery voltage seen while updating display
uint16_t __xdata gRefreshBattV;

//in mV
uint16_t __xdata gBattV;

__bit gLowBattery;
uint16_t __xdata longDataReqCounter;

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
      radioInit();
      radioSetTxPower(10);
      radioSetChannel(gCurrentChannel);
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
}

void powerDown(const uint8_t parts) 
{
   if((parts & INIT_EEPROM) && gEEPROM_PoweredUp) {
      EEPROM_LOG("Power down EEPROM\n");
      eepromDeepPowerDown();
   }
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
#elif !defined(DEBUG_SLEEP)
   MAIN_LOG("Sleeping for %ld ms\n",t);
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
   SLEEP_LOG("...\n");
#endif
   if(gEEPROM_PoweredUp) {
      powerDown(INIT_EEPROM);
   }
   screenShutdown();
   powerPortsDownForSleep();

// sleepy time
   sleepForMsec(t);
   powerUp(INIT_BASE);
#if defined(DEBUG_MAX_SLEEP) | defined(DEBUG_SLEEP)
   LOG("Awake\n");
#endif
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

void LogSummary(void)
{
   LOGA("Temp %d C\n",gTemperature);
   LOGA("RSSI %d, LQI %d\n",mLastRSSI,mLastLqi);
}

// refresh gBattV, gTemperaturecheck for low battery
// if low battery is detected set gBattV to the triggering value
void UpdateVBatt()
{
   ADCRead(ADC_CHAN_TEMP);
   ADCScaleTemperature();
   ADCRead(ADC_CHAN_VDD_3);
   gBattV = ADCScaleVDD(gRawA2DValue);
   gLowBattery = false;  // assume the best

   if(gBattV < tagSettings.batLowVoltage) {
      gLowBattery = true;
   }
   else if(gTxBattV != 0 && gTxBattV < tagSettings.batLowVoltage) {
      gBattV = gTxBattV;
      gLowBattery = true;
   }
   else if(gRefreshBattV != 0 && gRefreshBattV < tagSettings.batLowVoltage) {
      gBattV = gRefreshBattV;
      gLowBattery = true;
   }
   LOGA("BattV Boot %d, now %d, Tx %d, update %d\n",
        gBootBattV,gBattV,gTxBattV,gRefreshBattV);
}
