#define __packed
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "asmUtil.h"
#include "comms.h"  // for mLastLqi and mLastRSSI
#include "eeprom.h"
#include "powermgt.h"
#include "printf.h"

#include "radio.h"
#include "screen.h"
#include "settings.h"
#include "syncedproto.h"
#include "timer.h"
#include "userinterface.h"
#include "wdt.h"
#include "logging.h"

// #include "flash.h"

// #include "uart.h"

#include "../../oepl-definitions.h"
#include "../../oepl-proto.h"

// #define DEBUG_MODE

static const uint64_t __code __at(0x008b) firmwaremagic = (0xdeadd0d0beefcafeull) + HW_TYPE;

#define TAG_MODE_CHANSEARCH 0
#define TAG_MODE_ASSOCIATED 1

uint8_t currentTagMode = TAG_MODE_CHANSEARCH;

uint8_t __xdata slideShowCurrentImg = 0;
uint8_t __xdata slideShowRefreshCount = 1;

extern uint8_t *__idata blockp;
extern uint8_t __xdata blockbuffer[];

static bool __xdata secondLongCheckIn = false;  // send another full request if the previous was a special reason

const uint8_t __code channelList[6] = {11, 15, 20, 25, 26, 27};

uint8_t *rebootP;

#ifdef DEBUGGUI
void displayLoop() 
{
   powerUp(INIT_BASE | INIT_UART);

   pr("Splash screen\n");
   powerUp(INIT_EPD);
   showSplashScreen();
   timerDelay(TIMER_TICKS_PER_SECOND * 4);

   pr("Update screen\n");
   powerUp(INIT_EPD);
   showApplyUpdate();
   timerDelay(TIMER_TICKS_PER_SECOND * 4);

   wdt30s();

   pr("Failed update screen\n");
   powerUp(INIT_EPD);
   showFailedUpdate();
   timerDelay(TIMER_TICKS_PER_SECOND * 4);
   wdt30s();

   pr("AP Found\n");
   powerUp(INIT_EPD);
   showAPFound();
   timerDelay(TIMER_TICKS_PER_SECOND * 4);

   wdt30s();

   pr("AP NOT Found\n");
   powerUp(INIT_EPD);
   showNoAP();
   timerDelay(TIMER_TICKS_PER_SECOND * 4);

   wdt30s();

   pr("Longterm sleep screen\n");
   powerUp(INIT_EPD);
   showLongTermSleep();
   timerDelay(TIMER_TICKS_PER_SECOND * 4);

   wdt30s();

   pr("NO EEPROM\n");
   powerUp(INIT_EPD);
   showNoEEPROM();
   timerDelay(TIMER_TICKS_PER_SECOND * 4);

   wdt30s();

   pr("NO MAC\n");
   powerUp(INIT_EPD);
   showNoMAC();
   timerDelay(TIMER_TICKS_PER_SECOND * 4);
   wdtDeviceReset();
}
#endif

#ifdef WRITE_MAC_FROM_FLASH
void writeInfoPageWithMac() 
{
   uint8_t *settemp = blockbuffer + 2048;
   flashRead(FLASH_INFOPAGE_ADDR, (void *)blockbuffer, 1024);
   flashRead(0xFC06, (void *)(settemp + 8), 4);
   settemp[7] = 0x00;
   settemp[6] = 0x00;
   settemp[5] = settemp[8];
   settemp[4] = settemp[9];
   settemp[3] = settemp[10];
   settemp[2] = settemp[11];
#if (HW_TYPE == SOLUM_29_SSD1619)
   settemp[1] = 0x3B;
   settemp[0] = 0x10;
#endif
#if (HW_TYPE == SOLUM_M2_BWR_29_UC8151)
   settemp[1] = 0x3B;
   settemp[0] = 0x30;
#endif
#if (HW_TYPE == SOLUM_154_SSD1619)
   settemp[1] = 0x34;
   settemp[0] = 0x10;
#endif
#if (HW_TYPE == SOLUM_42_SSD1619)
   settemp[1] = 0x48;
   settemp[0] = 0x10;
#endif
#if (HW_TYPE == SOLUM_M2_BW_29_LOWTEMP)
   settemp[1] = 0x2D;
   settemp[0] = 0x10;
#endif
   uint8_t cksum = 0;
   for(uint8_t c = 0; c < 8; c++) {
      cksum ^= settemp[c];
      cksum ^= settemp[c] >> 4;
   }
   settemp[0] += cksum & 0x0F;

   xMemCopyShort((void *)(blockbuffer + 0x0010), (void *)settemp, 8);

   flashErase(FLASH_INFOPAGE_ADDR + 1);
   flashWrite(FLASH_INFOPAGE_ADDR, (void *)blockbuffer, 1024, false);
}
#endif

#if 0
// returns 0 if no accesspoints were found
uint8_t channelSelect(uint8_t rounds) 
{
   uint8_t __xdata result[16];

   memset(result, 0, sizeof(result));
   powerUp(INIT_RADIO);

   for(uint8_t i = 0; i < rounds; i++) {
      for(uint8_t c = 0; c < sizeof(channelList); c++) {
         if(detectAP(channelList[c])) {
            if(mLastLqi > result[c]) result[c] = mLastLqi;
#ifdef DEBUGMAIN
            if(rounds > 2) pr("MAIN: Channel: %d - LQI: %d RSSI %d\n", channelList[c], mLastLqi, mLastRSSI);
#endif
         }
      }
   }
   powerDown(INIT_RADIO);
   uint8_t __xdata highestLqi = 0;
   uint8_t __xdata highestSlot = 0;
   for(uint8_t c = 0; c < sizeof(result); c++) {
      if(result[c] > highestLqi) {
         highestSlot = channelList[c];
         highestLqi = result[c];
      }
   }

   mLastLqi = highestLqi;
   return highestSlot;
}
#endif

// FIX me !!
uint8_t getFirstWakeUpReason() 
{
#if 0
   if(RESET & 0x01) {
      MAIN_LOG("WDT reset!\n");
      return WAKEUP_REASON_WDT_RESET;
   }
#endif
   return WAKEUP_REASON_FIRSTBOOT;
}



void TagAssociated() 
{
   // associated
   struct AvailDataInfo *__xdata avail;
   // Is there any reason why we should do a long (full) get data request (including reason, status)?
   if((longDataReqCounter > LONG_DATAREQ_INTERVAL) 
      || wakeUpReason != WAKEUP_REASON_TIMED 
      || secondLongCheckIn) 
   { // check if we should do a voltage measurement (those are pretty expensive)
      if(voltageCheckCounter == VOLTAGE_CHECK_INTERVAL) {
         doVoltageReading();
         voltageCheckCounter = 0;
      }
      else {
         powerUp(INIT_TEMPREADING);
      }
      voltageCheckCounter++;

   // check if the battery level is below minimum, and force a redraw of the screen
      if((lowBattery && !lowBatteryShown && tagSettings.enableLowBatSymbol)
          || (noAPShown && tagSettings.enableNoRFSymbol)) 
      {
         // Check if we were already displaying an image
         if(curImgSlot != 0xFF) {
            powerUp(INIT_EEPROM | INIT_EPD);
            wdt60s();
            uint8_t lut = getEepromImageDataArgument(curImgSlot) & 0x03;
            drawImageFromEeprom(curImgSlot, lut);
            powerDown(INIT_EEPROM | INIT_EPD);
         } else {
            powerUp(INIT_EPD);
            showAPFound();
            powerDown(INIT_EPD);
         }
      }

      powerUp(INIT_RADIO);
      avail = getAvailDataInfo();
      pr("avail %x\n",avail);
      powerDown(INIT_RADIO);

      if(avail != NULL) {
      // we got some data!
         longDataReqCounter = 0;

         if(secondLongCheckIn == true) {
            secondLongCheckIn = false;
         }

       // since we've had succesful contact, and communicated the wakeup 
       // reason succesfully, we can now reset to the 'normal' status
         if(wakeUpReason != WAKEUP_REASON_TIMED) {
            secondLongCheckIn = true;
         }
         wakeUpReason = WAKEUP_REASON_TIMED;
      }
   }
   else {
      powerUp(INIT_RADIO);
      avail = getShortAvailDataInfo();
      powerDown(INIT_RADIO);
   }

   addAverageValue();

   if(avail == NULL) {
   // no data :( this means no reply from AP
      nextCheckInFromAP = 0;  // let the power-saving algorithm determine the next sleep period
   }
   else {
      nextCheckInFromAP = avail->nextCheckIn;
   // got some data from the AP!
      if(avail->dataType != DATATYPE_NOUPDATE) {
   // data transfer
         if(processAvailDataInfo(avail)) {
         // succesful transfer, next wake time is determined by the NextCheckin;
         }
         else {
         // failed transfer, let the algorithm determine next sleep interval (not the AP)
            nextCheckInFromAP = 0;
         }
      } 
      else {
         // no data transfer, just sleep.
      }
   }

   uint16_t nextCheckin = getNextSleep();
   longDataReqCounter += nextCheckin;

   if(nextCheckin == INTERVAL_AT_MAX_ATTEMPTS) {
   // We've averaged up to the maximum interval, this means the tag 
   // hasn't been in contact with an AP for some time.
      if(tagSettings.enableScanForAPAfterTimeout) {
         currentTagMode = TAG_MODE_CHANSEARCH;
         return;
      }
   }

   if(fastNextCheckin) {
   // do a fast check-in next
      fastNextCheckin = false;
      doSleep(100UL);
   }
   else {
      if(nextCheckInFromAP) {
      // if the AP told us to sleep for a specific period, do so.
         if(nextCheckInFromAP & 0x8000) {
            doSleep((nextCheckInFromAP & 0x7FFF) * 1000UL);
         }
         else {
            doSleep(nextCheckInFromAP * 60000UL);
         }
      }
      else {
      // sleep determined by algorithm
         doSleep(getNextSleep() * 1000UL);
      }
   }
   powerUp(INIT_UART);
}

void TagChanSearch() 
{
// not associated
   if((scanAttempts != 0 && (scanAttempts % VOLTAGEREADING_DURING_SCAN_INTERVAL == 0))
      || scanAttempts > (INTERVAL_1_ATTEMPTS + INTERVAL_2_ATTEMPTS))
   {
      doVoltageReading();
   }

// Is channel working?
   currentChannel = detectAP(tagSettings.fixedChannel);

// Check if we should redraw the screen with icons, info screen or screensaver
   if((!currentChannel && !noAPShown && tagSettings.enableNoRFSymbol) 
      || (lowBattery && !lowBatteryShown && tagSettings.enableLowBatSymbol) 
      || (scanAttempts == (INTERVAL_1_ATTEMPTS + INTERVAL_2_ATTEMPTS - 1))) 
   {
      powerUp(INIT_EPD);
      wdt60s();
      if(curImgSlot != 0xFF) {
         if(!displayCustomImage(CUSTOM_IMAGE_LOST_CONNECTION)) {
            powerUp(INIT_EEPROM);
            uint8_t lut = getEepromImageDataArgument(curImgSlot) & 0x03;
            drawImageFromEeprom(curImgSlot, lut);
            powerDown(INIT_EEPROM);
         }
      }
      else if(scanAttempts >= (INTERVAL_1_ATTEMPTS + INTERVAL_2_ATTEMPTS - 1)) {
         showLongTermSleep();
      }
      else {
         showNoAP();
      }
      powerDown(INIT_EPD);
   }

// did we find a working channel?
   if(currentChannel) {
   // now associated! set up and bail out of this loop.
      scanAttempts = 0;
      wakeUpReason = WAKEUP_REASON_NETWORK_SCAN;
      initPowerSaving(INTERVAL_BASE);
      doSleep(getNextSleep() * 1000UL);
      currentTagMode = TAG_MODE_ASSOCIATED;
   }
   else {
   // still not associated
      doSleep(getNextScanSleep(true) * 1000UL);
   }
}

void executeCommand(uint8_t cmd) 
{
   switch(cmd) {
      case CMD_DO_REBOOT:
         wdtDeviceReset();
         break;

      case CMD_DO_RESET_SETTINGS:
         powerUp(INIT_EEPROM);
         loadDefaultSettings();
         writeSettings();
         powerDown(INIT_EEPROM);
         break;

      case CMD_DO_DEEPSLEEP:
         powerUp(INIT_EPD);
         afterFlashScreenSaver();
         powerDown(INIT_EPD | INIT_UART);
         while(1) {
            doSleep(-1);
         }
         break;

      case CMD_ERASE_EEPROM_IMAGES:
         powerUp(INIT_EEPROM);
         eraseImageBlocks();
         powerDown(INIT_EEPROM);
         break;
   }
}

void main() 
{
   setupPortsInitial();
   powerUp(INIT_BASE | INIT_UART);
   pr("Chroma OEPL v%04x, compiled " __DATE__" " __TIME__ "\n",fwVersion);

#ifdef DEBUGGUI
   displayLoop();  // remove me
#endif

// Find the reason why we're booting; is this a WDT?
   wakeUpReason = getFirstWakeUpReason();

// dump(blockbuffer, 1024);
//  get our own mac address. this is stored in Infopage at offset 0x10-onwards
   boardGetOwnMac(mSelfMac);
   InitBcastFrame();

   MAIN_LOG("MAC>%02X%02X", mSelfMac[0], mSelfMac[1]);
   MAIN_LOG("%02X%02X", mSelfMac[2], mSelfMac[3]);
   MAIN_LOG("%02X%02X", mSelfMac[4], mSelfMac[5]);
   MAIN_LOG("%02X%02X\n", mSelfMac[6], mSelfMac[7]);
// do a little sleep, this prevents a partial boot during battery insertion
   doSleep(400UL);
   powerUp(INIT_EEPROM | INIT_UART);

// load settings from infopage
   loadSettings();
// invalidate the settings, and write them back in a later state
// invalidateSettingsEEPROM();

// get the highest slot number, number of slots
   initializeProto();
   powerDown(INIT_EEPROM);
   if(tagSettings.enableFastBoot) {
   // Fastboot
      MAIN_LOG("Doing fast boot\n");
   }
   else {
   // Normal boot/startup
      MAIN_LOG("Normal boot\n");

   // Get a voltage reading on the tag, loading down the battery with the radio
      doVoltageReading();

   // show the splashscreen
      showSplashScreen();
   }

   wdt10s();

   if(currentChannel) {
      MAIN_LOG("MAIN: Ap Found!\n");
      //showNoAP();

      showAPFound();
   // write the settings to the eeprom
      powerUp(INIT_EEPROM);
      writeSettings();
      powerDown(INIT_EEPROM);

      initPowerSaving(INTERVAL_BASE);
      currentTagMode = TAG_MODE_ASSOCIATED;
      doSleep(5000UL);
   }
   else {
      MAIN_LOG("MAIN: No AP found...\n");
      //showAPFound();
      showNoAP();
      // write the settings to the eeprom
      powerUp(INIT_EEPROM);
      writeSettings();
      powerDown(INIT_EEPROM);

      initPowerSaving(INTERVAL_AT_MAX_ATTEMPTS);
      currentTagMode = TAG_MODE_CHANSEARCH;
      doSleep(120000UL);
   }

   // this is the loop we'll stay in forever, basically.
   while(1) {
      powerUp(INIT_UART);
      wdt10s();

      switch(currentTagMode) {
         case TAG_MODE_ASSOCIATED:
            TagAssociated();
            break;

         case TAG_MODE_CHANSEARCH:
            TagChanSearch();
            break;
      }
   }
}

const char __xdata* fwVerString(void)
{
   static char __xdata fwVer[32];

   if(!fwVer[0]) {

      spr(fwVer, "FW v%u.%u.%*u",
          fwVersion / 100,(fwVersion % 100) / 10,fwVersion % 10);
   }

   return fwVer;
}

const char __xdata* macString(void)
{
   static char __xdata macStr[28];

   if(!macStr[0])
      spr(macStr, "%M", (uintptr_near_t)mSelfMac);

   return macStr;
}

uint16_t __xdata MallocCaller;
void free(void *p)
{
   p;
   if(MallocCaller == 0) {
   // Get the caller address
      __asm__(
           "mov   dptr,#_MallocCaller\n"
           "pop   b\n"
           "pop   acc\n"
           "movx  @dptr,a\n"
           "inc   dptr\n"
           "mov   a,b\n"
           "movx  @dptr,a\n"
           "mov   a,sp\n"
           "inc   a\n"
           "inc   a\n"
           "mov   sp,a\n"
      );
      pr("\n0x%x: free err\n",MallocCaller);
   }
   else {
      MallocCaller = 0;
   }
}

void __xdata *malloc (size_t size)
{
   uint16_t LastCaller = MallocCaller;

// Get the caller address for debugging
   __asm__(
        "mov   dptr,#_MallocCaller\n"
        "pop   b\n"
        "pop   acc\n"
        "movx  @dptr,a\n"
        "inc   dptr\n"
        "mov   a,b\n"
        "movx  @dptr,a\n"
        "mov   a,sp\n"
        "inc   a\n"
        "inc   a\n"
        "mov   sp,a\n"
   );

   if(LastCaller != 0) {
      pr("\n0x%x Malloc err 0x%x\n",
         MallocCaller,LastCaller);
      while(true);
   }
   else if(size > sizeof(mScreenRow)) {
      pr("\nMalloc err %d 0x%x\n",size,MallocCaller);
      while(true);
   }
   return &mScreenRow;
}

