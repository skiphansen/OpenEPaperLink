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

__bit gTagAssociated;

uint8_t __xdata slideShowCurrentImg;
uint8_t __xdata slideShowRefreshCount = 1;

extern uint8_t *__idata blockp;

__bit secondLongCheckIn;  // send another full request if the previous was a special reason

const uint8_t __code gChannelList[] = {
   200,100,201,101,202,102,203,103,204,104,205,105
};


uint8_t *rebootP;

const uint16_t __code fwVersion = FW_VERSION;
const char * __code gBoardName = BOARD_NAME;
uint16_t __xdata gUpdateFwVer;
uint8_t __xdata gUpdateErr;

__bit gLowBatteryShown;
__bit noAPShown;

// Return 0 if no APs are found
// 
// Initally we search for APs on both bands once we have found
// one we know what band to search and only search that band to
// avoid (further) out of band operation.
uint8_t channelSelect(uint8_t rounds) 
{
   uint8_t __xdata BestLqi = 0;
   uint8_t __xdata BestChannel = 0;
   uint8_t __xdata i;

   while(rounds-- > 0) {
      for(i = 0; i < sizeof(gChannelList); i++) {
         if(gSubGhzBand == BAND_915 && gChannelList[i] < 200) {
         // Using 915 band and this channel is in 868, skip it
         }
         else if(gSubGhzBand == BAND_868 && gChannelList[i] >= 200) {
         // Using 868 band and this channel is in 915, skip it
         }
         else if(detectAP(gChannelList[i])) {
            AP_SEARCH_LOG("Chan %d LQI %d RSSI %d\n",
                          gChannelList[i],mLastLqi,mLastRSSI);
            if(BestLqi < mLastLqi) {
               BestLqi = mLastLqi;
               BestChannel = gChannelList[i];
            }
         }
      }
   }

   AP_SEARCH_LOG("Using ch %d\n",BestChannel);
   if(BestChannel) {
      if(BestChannel >= 200) {
         gSubGhzBand = BAND_915;
      }
      else {
         gSubGhzBand = BAND_868;
      }
   }
   return BestChannel;
}
#undef result

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
         gTagAssociated = false;
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
   if(tagSettings.fixedChannel) {
      gCurrentChannel = detectAP(tagSettings.fixedChannel);
   }
   else {
      gCurrentChannel = channelSelect(2);
   }

// Check if we should redraw the screen with icons, info screen or screensaver
   if((!gCurrentChannel && !noAPShown && tagSettings.enableNoRFSymbol) 
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
   if(gCurrentChannel) {
   // now associated! set up and bail out of this loop.
      scanAttempts = 0;
      if(tagSettings.fixedChannel == 0) {
      // Save the AP channel
         powerUp(INIT_EEPROM);
         writeSettings();
         powerDown(INIT_EEPROM);
      }
      wakeUpReason = WAKEUP_REASON_NETWORK_SCAN;
      initPowerSaving(INTERVAL_BASE);
      doSleep(getNextSleep() * 1000UL);
      gTagAssociated = true;
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

      case CMD_DO_SCAN:
         LOGA("CMD_DO_SCAN\n");
         gTagAssociated = false;
         gCurrentChannel = 0;
         break;

      case CMD_DO_RESET_SETTINGS: {
      // save gCurrentChannel before loadDefaultSettings() clears it
         uint8_t ChannelSave = gCurrentChannel;
         LOGA("Reset settings\n");
         powerUp(INIT_EEPROM);
         loadDefaultSettings();
         writeSettings();
         powerDown(INIT_EEPROM);
      // retore gCurrentChannel
         gCurrentChannel = ChannelSave;
         break;
      }

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

#ifdef DEBUGGUI
void displayLoop() 
{
   powerUp(INIT_BASE);

   gLowBattery = true;

   LOG("AP NOT Found\n");
   showNoAP();
   timerDelay(TIMER_TICKS_PER_SECOND * 4);

   LOG("afterFlashScreenSaver\n");
   afterFlashScreenSaver();
   timerDelay(TIMER_TICKS_PER_SECOND * 4);

   LOG("Splash screen\n");
   showSplashScreen();
   timerDelay(TIMER_TICKS_PER_SECOND * 4);

   LOG("Update screen\n");
   showApplyUpdate();
   timerDelay(TIMER_TICKS_PER_SECOND * 4);

   LOG("Failed update screen\n");
   showFailedUpdate();
   timerDelay(TIMER_TICKS_PER_SECOND * 4);

   LOG("AP Found\n");
   showAPFound();
   timerDelay(TIMER_TICKS_PER_SECOND * 4);

   LOG("Longterm sleep screen\n");
   showLongTermSleep();
   timerDelay(TIMER_TICKS_PER_SECOND * 4);

   LOG("NO EEPROM\n");
   showNoEEPROM();
   timerDelay(TIMER_TICKS_PER_SECOND * 4);

   MAIN_LOG("done!\n");
   while(true);
}
#endif


void main() 
{  
   COPY_CFG();
   powerUp(INIT_BASE);
   PRINT_CONFIG("After reset");
   LOG_CONFIG("After INIT_BASE");

// Save initial battery voltage
   ADCRead(ADC_CHAN_VDD_3);
   gBootBattV = ADCScaleVDD(gRawA2DValue);

   LOGA("\n%s OEPL v%04x"
#ifdef FW_VERSION_SUFFIX
        FW_VERSION_SUFFIX
#endif
        ", compiled " __DATE__" " __TIME__ "\n",gBoardName,fwVersion);

   boardInitStage2();

   UpdateVBatt();
// Log initial battery voltage and temperature
   LogSummary();

#ifdef DEBUGGUI
   displayLoop(); // never returns
#else

// Find the reason why we're booting; is this a WDT?
   wakeUpReason = WAKEUP_REASON_FIRSTBOOT;
   SLEEP_LOG("SLEEP reg %02x\n",SLEEP);
   if((SLEEP & SLEEP_RST) == SLEEP_RST_WDT) {
      wakeUpReason = WAKEUP_REASON_WDT_RESET;
   }

   if(gEEpromFailure) {
      showNoEEPROM();
      doSleep(0); // forever, never returns
   }
   MAIN_LOG("MAC %s\n",gMacString);
   InitBcastFrame();

// do a little sleep, this prevents a partial boot during battery insertion
   doSleep(400UL);
   powerUp(INIT_EEPROM);
   loadSettings();

// get the highest slot number, number of slots
   initializeProto();

   if(tagSettings.enableFastBoot) {
   // Fastboot
      MAIN_LOG("Doing fast boot\n");
   }
   else {
   // Normal boot/startup
      MAIN_LOG("Normal boot\n");

   // show the splashscreen
      showSplashScreen();
      doSleep(10000UL);   // Give the user a chance to read it
   }

   wdt10s();

// Try the saved channel before scanning for an AP to avoid
// out of band transmissions as much as possible
   if(gCurrentChannel) {
      if(!detectAP(gCurrentChannel)) {
         MAIN_LOG("No AP found on saved channel\n");
         gCurrentChannel = 0;
      }
   }
   else {
      LOGA("No saved channel\n");
   }

   if(!gCurrentChannel) {
      TagChanSearch();
   }

   if(gCurrentChannel) {
      showAPFound();
      gTagAssociated = true;
   }
   else {
      showNoAP();
      gTagAssociated = false;
   }

#ifndef ISDEBUGBUILD
// write the settings to the eeprom nothing has changed, but we
// invalidated the settings in EEPROM when we loaded them into RAM
// to give the user the ability to clear flash by removing the batteries 
// during  the first screen refresh.

   powerUp(INIT_EEPROM);
   writeSettings();
#endif
   initPowerSaving(gTagAssociated ? INTERVAL_BASE : INTERVAL_AT_MAX_ATTEMPTS);
   doSleep(gTagAssociated ? 5000UL : 120000UL);

// this is the loop we'll stay in forever
   while(1) {
      wdt10s();
      if(gTagAssociated) {
         TagAssociated();
      }
      else {
         TagChanSearch();
      }
   }
#endif
}


