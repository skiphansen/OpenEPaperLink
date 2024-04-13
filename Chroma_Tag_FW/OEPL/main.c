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
#include "timer.h"
#include "userinterface.h"
#include "wdt.h"
#include "logging.h"
#include "drawing.h"
#include "../../oepl-definitions.h"
#include "../../oepl-proto.h"
#include "syncedproto.h"
#include "adc.h"

// #define DEBUG_MODE

static const uint64_t __code __at(0x008b) firmwaremagic = (0xdeadd0d0beefcafeull) + HW_TYPE;

#define TAG_MODE_CHANSEARCH 0
#define TAG_MODE_ASSOCIATED 1

uint8_t currentTagMode = TAG_MODE_CHANSEARCH;

uint8_t __xdata slideShowCurrentImg;
uint8_t __xdata slideShowRefreshCount = 1;

extern uint8_t *__idata blockp;

__bit secondLongCheckIn;  // send another full request if the previous was a special reason

const uint8_t __code channelList[] = {
   200,100,201,101,202,102,203,103,204,104,205,105
};

uint8_t *rebootP;

const uint16_t __code fwVersion = FW_VERSION;
const char * __code gBoardName = BOARD_NAME;
uint16_t __xdata gUpdateFwVer;
uint8_t __xdata gUpdateErr;

__bit gLowBatteryShown;
__bit noAPShown;

// returns 0 if no accesspoints were found
uint8_t channelSelect(uint8_t rounds) 
{
   uint8_t __xdata result[sizeof(channelList)];

   memset(result, 0, sizeof(result));

   for(uint8_t i = 0; i < rounds; i++) {
      for(uint8_t c = 0; c < sizeof(channelList); c++) {
         if(detectAP(channelList[c])) {
            AP_SEARCH_LOG("Chan %d LQI %d RSSI %d\n",
                          channelList[c],mLastLqi,mLastRSSI);
            if(mLastLqi > result[c]) {
               result[c] = mLastLqi;
            }
         }
      }
   }
   uint8_t __xdata highestLqi = 0;
   uint8_t __xdata highestSlot = 0;
   for(uint8_t c = 0; c < sizeof(result); c++) {
      if(result[c] > highestLqi) {
         highestSlot = channelList[c];
         highestLqi = result[c];
      }
   }

   AP_SEARCH_LOG("Using ch %d\n",highestSlot);
   return highestSlot;
}

void TagAssociated() 
{
   // associated
   struct AvailDataInfo *__xdata avail;
   // Is there any reason why we should do a long (full) get data request (including reason, status)?
   if(longDataReqCounter > LONG_DATAREQ_INTERVAL 
      || wakeUpReason != WAKEUP_REASON_TIMED 
      || secondLongCheckIn) 
   {
// check if the battery level is below minimum, and force a redraw of the screen
      if((gLowBattery && !gLowBatteryShown && tagSettings.enableLowBatSymbol)
          || (noAPShown && tagSettings.enableNoRFSymbol)) 
      {
         // Check if we were already displaying an image
         if(curImgSlot != 0xFF) {
            wdt60s();
            drawImageFromEeprom(curImgSlot,0);
         }
         else {
            showAPFound();
         }
      }

      avail = getAvailDataInfo();
      if(avail != NULL) {
      // we got some data!
         longDataReqCounter = 0;

         if(secondLongCheckIn) {
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
      avail = getShortAvailDataInfo();
   }

   addAverageValue();

   if(avail == NULL) {
   // no data :( this means no reply from AP
      nextCheckInFromAP = 0;  // let the power-saving algorithm determine the next sleep period
   }
   else {
   // got some data from the AP!
      nextCheckInFromAP = avail->nextCheckIn;
      if(avail->dataType != DATATYPE_NOUPDATE) {
      // data transfer
         BLOCK_LOG("Update available\n");
         if(processAvailDataInfo(avail)) {
         // succesful transfer, next wake time is determined by the NextCheckin;
         }
         else {
         // failed transfer, let the algorithm determine next sleep interval (not the AP)
            nextCheckInFromAP = 0;
            BLOCK_LOG("processAvailDataInfo failed\n");
         }
      } 
      else {
      // no data transfer, just sleep.
         LOGA("No update\n");
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
}

void TagChanSearch() 
{
// not associated

// try to find a working channel
   currentChannel = channelSelect(2);

// Check if we should redraw the screen with icons, info screen or screensaver
   if((!currentChannel && !noAPShown && tagSettings.enableNoRFSymbol) 
      || (gLowBattery && !gLowBatteryShown && tagSettings.enableLowBatSymbol) 
      || (scanAttempts == (INTERVAL_1_ATTEMPTS + INTERVAL_2_ATTEMPTS - 1))) 
   {
      wdt60s();
      if(curImgSlot != 0xFF) {
         if(!displayCustomImage(CUSTOM_IMAGE_LOST_CONNECTION)) {
            drawImageFromEeprom(curImgSlot,0);
         }
      }
      else if(scanAttempts >= (INTERVAL_1_ATTEMPTS + INTERVAL_2_ATTEMPTS - 1)) {
         showLongTermSleep();
      }
      else {
         showNoAP();
      }
   }

// did we find a working channel?
   if(currentChannel) {
   // now associated! set up and bail out of this loop.
      scanAttempts = 0;
      powerUp(INIT_EEPROM);
      writeSettings();
      powerDown(INIT_EEPROM);
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
         LOGA("Reset settings\n");
         powerUp(INIT_EEPROM);
         loadDefaultSettings();
         writeSettings();
         powerDown(INIT_EEPROM);
         break;

      case CMD_DO_DEEPSLEEP:
         afterFlashScreenSaver();
         doSleep(0);
         break;

      case CMD_ERASE_EEPROM_IMAGES:
         LOGA("Erasing images\n");
         powerUp(INIT_EEPROM);
         eraseImageBlocks();
         powerDown(INIT_EEPROM);
         break;

      default:
         LOGA("Cmd 0x%x ignored\n",cmd);
         break;
   }
}

void main() 
{  
   powerUp(INIT_BASE);

// Save initial battery voltage
   ADCRead(ADC_CHAN_VDD_3);
   gBootBattV = ADCScaleVDD(gRawA2DValue);

   LOGA("\n%s OEPL v%04x, compiled " __DATE__" " __TIME__ "\n",
        gBoardName,fwVersion);
   boardInitStage2();

   ADCRead(ADC_CHAN_TEMP);
   ADCScaleTemperature();
// Log initial battery voltage and temperature
   LogSummary();

// Find the reason why we're booting; is this a WDT?
   wakeUpReason = WAKEUP_REASON_FIRSTBOOT;
   SLEEP_LOG("SLEEP reg %02x\n",SLEEP);
   if((SLEEP & SLEEP_RST) == SLEEP_RST_WDT) {
      wakeUpReason = WAKEUP_REASON_WDT_RESET;
   }

   InitBcastFrame();

   MAIN_LOG("MAC>%02X%02X", mSelfMac[0], mSelfMac[1]);
   MAIN_LOG("%02X%02X", mSelfMac[2], mSelfMac[3]);
   MAIN_LOG("%02X%02X", mSelfMac[4], mSelfMac[5]);
   MAIN_LOG("%02X%02X\n", mSelfMac[6], mSelfMac[7]);

// do a little sleep, this prevents a partial boot during battery insertion
   doSleep(400UL);
   powerUp(INIT_EEPROM);

   loadSettings();

// get the highest slot number, number of slots
   initializeProto();

#if 0
   drawImageAtAddress(EEPROM_IMG_START + EEPROM_IMG_EACH);
   showSplashScreen();
   showAPFound();
   showNoAP();
   showLongTermSleep();
   afterFlashScreenSaver();
   MAIN_LOG("drawing image\n");
   powerDown(INIT_EEPROM);
   showSplashScreen();
   MAIN_LOG("image drawn\n");
   powerUp(INIT_EEPROM);
   doSleep(20000L);
   powerUp(INIT_EEPROM);
   MAIN_LOG("drawing slot 1\n");
   drawImageAtAddress(EEPROM_IMG_START + EEPROM_IMG_EACH);
   MAIN_LOG("image drawn\n");
   powerUp(INIT_EEPROM);
   doSleep(20000L);
   MAIN_LOG("done!\n");
   while(true);
#endif

   if(tagSettings.enableFastBoot) {
   // Fastboot
      MAIN_LOG("Doing fast boot\n");
   }
   else {
   // Normal boot/startup
      MAIN_LOG("Normal boot\n");

   // show the splashscreen
      showSplashScreen();
   }

   wdt10s();

// Try the saved channel before scanning for an AP to avoid
// out of band transmissions as much as possible
   if(currentChannel) {
      LOGA("Check last channel\n");
      if(!detectAP(currentChannel)) {
         currentChannel = 0;
      }
   }

   if(currentChannel) {
      showAPFound();
#if 0
// Why ??? 
   // write the settings to the eeprom
      powerUp(INIT_EEPROM);
      writeSettings();
      powerDown(INIT_EEPROM);
#endif
      initPowerSaving(INTERVAL_BASE);
      currentTagMode = TAG_MODE_ASSOCIATED;
      doSleep(5000UL);
   }
   else {
      MAIN_LOG("No AP found\n");
      showNoAP();
#if 0
// Why ??? 
      // write the settings to the eeprom
      powerUp(INIT_EEPROM);
      writeSettings();
      powerDown(INIT_EEPROM);
#endif
      initPowerSaving(INTERVAL_AT_MAX_ATTEMPTS);
      currentTagMode = TAG_MODE_CHANSEARCH;
      doSleep(120000UL);
   }

// this is the loop we'll stay in forever, basically.
   while(1) {
      wdt10s();
      if(currentTagMode == TAG_MODE_ASSOCIATED) {
         TagAssociated();
      }
      else {
         TagChanSearch();
      }
   }
}


