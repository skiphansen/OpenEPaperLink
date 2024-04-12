#include "settings.h"

#ifndef DISABLE_UI
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>

#include "userinterface.h"
#include "asmUtil.h"

#include "board.h"
#include "comms.h"
#include "cpu.h"
#include "lut.h"
#include "powermgt.h"
#include "printf.h"
#include "../oepl-definitions.h"
#include "../oepl-proto.h"
#include "screen.h"
#include "bitmaps.h"
#include "sleep.h"
#include "syncedproto.h"  // for APmac / Channel
#include "timer.h"
#include "drawing.h"
#include "logging.h"

void addOverlay() 
{
#ifdef DEBUG_FORCE_OVERLAY
// force icons to be display for testing
   gBattV = 0;
   currentChannel = 0;
#endif

   if(currentChannel == 0 && tagSettings.enableNoRFSymbol) {
      loadRawBitmap(ant,SCREEN_WIDTH - 24,6,EPD_COLOR_BLACK);
      loadRawBitmap(cross,SCREEN_WIDTH - 16,13,EPD_COLOR_RED);
      noAPShown = true;
   }
   else {
      noAPShown = false;
   }
   if(gLowBattery && tagSettings.enableLowBatSymbol) {
      loadRawBitmap(battery,SCREEN_WIDTH - 24,SCREEN_HEIGHT - 16,EPD_COLOR_BLACK);
      gLowBatteryShown = true;
   }
   else {
      gLowBatteryShown = false;
   }
#ifdef ISDEBUGBUILD
   epdPrintBegin(0,SCREEN_HEIGHT - FONT_HEIGHT - 2,
                 EPD_DIRECTION_X,
                 EPD_SIZE_SINGLE,
#ifdef BWY
// Yellow is very low constrast, make it black for BWY displays
                 EPD_COLOR_BLACK);
#else
                 EPD_COLOR_RED);
#endif
   epdpr("DEBUG");
#endif
}

void DrawAfterFlashScreenSaver() 
{
   epdPrintBegin(3,3,EPD_DIRECTION_X,EPD_SIZE_DOUBLE,EPD_COLOR_BLACK);
   epdpr("OpenEPaperLink");

   epdPrintBegin(360,8,EPD_DIRECTION_X,EPD_SIZE_SINGLE,EPD_COLOR_BLACK);
   epdpr("v%04X",fwVersion);

   epdPrintBegin(10,48,EPD_DIRECTION_X,EPD_SIZE_SINGLE,EPD_COLOR_BLACK);
   epdpr("I'm fast asleep... UwU   To wake me:");

   epdPrintBegin(20,70,EPD_DIRECTION_X,EPD_SIZE_SINGLE,EPD_COLOR_BLACK);
   epdpr("- Remove all batteries");

   epdPrintBegin(20,86,EPD_DIRECTION_X,EPD_SIZE_SINGLE,EPD_COLOR_BLACK);
   epdpr("- Short battery contacts");

   epdPrintBegin(20,102,EPD_DIRECTION_X,EPD_SIZE_SINGLE,EPD_COLOR_BLACK);
   epdpr("- Reinsert batteries");

   epdPrintBegin(3,283,EPD_DIRECTION_X,EPD_SIZE_SINGLE,EPD_COLOR_BLACK);
   epdpr("openepaperlink.de");

   epdPrintBegin(255, 283, EPD_DIRECTION_X, EPD_SIZE_SINGLE, EPD_COLOR_RED);
   DrawTagMac();
}

void afterFlashScreenSaver()
{
   if(displayCustomImage(CUSTOM_IMAGE_LONGTERMSLEEP)) {
      return;
   }
   LOGA("Deep sleep\n");
   DrawScreen(DrawAfterFlashScreenSaver);
}


void DrawTagMac() 
{
   epdpr("MAC: %02X:%02X", mSelfMac[7], mSelfMac[6]);
   epdpr(":%02X:%02X",mSelfMac[5],mSelfMac[4]);
   epdpr(":%02X:%02X",mSelfMac[3],mSelfMac[2]);
   epdpr(":%02X:%02X",mSelfMac[1],mSelfMac[0]);
}

void DrawFwVer()
{
#ifdef FW_VERSION_SUFFIX
   epdpr(BOARD_NAME " v%04X" FW_VERSION_SUFFIX,fwVersion);
#else
   epdpr(BOARD_NAME " v%04X",fwVersion);
#endif
}

void DrawSplashScreen()
{
   epdPrintBegin(48,80,EPD_DIRECTION_X,EPD_SIZE_DOUBLE,EPD_COLOR_BLACK);
   epdpr("Starting...");

   epdPrintBegin(3,268,EPD_DIRECTION_X,EPD_SIZE_SINGLE,EPD_COLOR_BLACK);
   DrawFwVer();

   epdPrintBegin(3,284,EPD_DIRECTION_X,EPD_SIZE_SINGLE,EPD_COLOR_RED);
   DrawTagMac();
#ifndef LEAN_VERSION
   loadRawBitmap(oepli,136,22,EPD_COLOR_BLACK);
   loadRawBitmap(cloud,136,10,EPD_COLOR_RED);
#endif

#if 0
   uint8_t __xdata buffer[17];
   spr(buffer, "%02X%02X", mSelfMac[7], mSelfMac[6]);
   spr(buffer + 4, "%02X%02X", mSelfMac[5], mSelfMac[4]);
   spr(buffer + 8, "%02X%02X", mSelfMac[3], mSelfMac[2]);
   spr(buffer + 12, "%02X%02X", mSelfMac[1], mSelfMac[0]);
   printBarcode(buffer, 392, 264);
   printBarcode(buffer, 384, 264);
#endif
}

void showSplashScreen()
{
   if(displayCustomImage(CUSTOM_IMAGE_SPLASHSCREEN)) {
      return;
   }
   DrawScreen(DrawSplashScreen);
}

void DrawApplyUpdate() 
{
    epdPrintBegin(136,134,EPD_DIRECTION_X,EPD_SIZE_DOUBLE,EPD_COLOR_BLACK);
    epdpr("Updating!");
}

void showApplyUpdate()
{
   DrawScreen(DrawApplyUpdate);
}


void DrawFailedUpdate() 
{
    epdPrintBegin(68, 134, EPD_DIRECTION_X, EPD_SIZE_DOUBLE, EPD_COLOR_BLACK);
    epdpr("Invalid OTA FW!");
}

void showFailedUpdate()
{
   DrawScreen(DrawFailedUpdate);
}

void DrawAPFound() 
{
   epdPrintBegin(10,10,EPD_DIRECTION_X,EPD_SIZE_DOUBLE,EPD_COLOR_BLACK);
   epdpr("Waiting for data...");
   epdPrintBegin(48,80,EPD_DIRECTION_X,EPD_SIZE_SINGLE,EPD_COLOR_BLACK);
   epdpr("Found the following AP:");
   epdPrintBegin(48,96,EPD_DIRECTION_X,EPD_SIZE_SINGLE,EPD_COLOR_BLACK);
   epdpr("AP MAC: %02X:%02X",APmac[7],APmac[6]);
   epdpr(":%02X:%02X",APmac[5],APmac[4]);
   epdpr(":%02X:%02X",APmac[3],APmac[2]);
   epdpr(":%02X:%02X",APmac[1],APmac[0]);
   epdPrintBegin(48,112,EPD_DIRECTION_X,EPD_SIZE_SINGLE,EPD_COLOR_BLACK);
   epdpr("Ch: %d RSSI: %d LQI: %d",currentChannel,mLastRSSI,mLastLqi);
   epdPrintBegin(48,128,EPD_DIRECTION_X,EPD_SIZE_SINGLE,EPD_COLOR_BLACK);
   epdpr("Tag ");
   DrawTagMac();
   epdPrintBegin(48,144,EPD_DIRECTION_X,EPD_SIZE_SINGLE,EPD_COLOR_BLACK);
   DrawFwVer();
#if 0
   uint8_t __xdata buffer[17];
   spr(buffer,"%02X%02X",mSelfMac[7],mSelfMac[6]);
   spr(buffer + 4,"%02X%02X",mSelfMac[5],mSelfMac[4]);
   spr(buffer + 8,"%02X%02X",mSelfMac[3],mSelfMac[2]);
   spr(buffer + 12,"%02X%02X",mSelfMac[1],mSelfMac[0]);
   printBarcode(buffer,392,253);
   printBarcode(buffer,384,253);
#endif

#ifndef LEAN_VERSION
   loadRawBitmap(receive,100,170,EPD_COLOR_BLACK);
#endif
   addOverlay();
}

void showAPFound() 
{
   if(displayCustomImage(CUSTOM_IMAGE_APFOUND)) {
      return;
   }
   DrawScreen(DrawAPFound);
}

void DrawNoAP() 
{
   epdPrintBegin(10,10,EPD_DIRECTION_X,EPD_SIZE_DOUBLE,EPD_COLOR_BLACK);
   epdpr("No AP found :(");
   epdPrintBegin(10,274,EPD_DIRECTION_X,EPD_SIZE_SINGLE,EPD_COLOR_BLACK);
   epdpr("We'll try again in a little while...");
#ifndef LEAN_VERSION
   loadRawBitmap(receive, 76, 120, EPD_COLOR_BLACK);
   loadRawBitmap(failed, 82, 122, EPD_COLOR_RED);
#endif
   addOverlay();
}

void showNoAP()
{
   if(displayCustomImage(CUSTOM_IMAGE_NOAPFOUND)) {
      return;
   }
   DrawScreen(DrawNoAP);
}

void DrawLongTermSleep() 
{
   epdPrintBegin(SCREEN_WIDTH/2,SCREEN_HEIGHT/2,EPD_DIRECTION_X,
                 EPD_SIZE_SINGLE,EPD_COLOR_BLACK);
   epdpr("zzZZZ...");
   addOverlay();
}

void showLongTermSleep()
{
   if(displayCustomImage(CUSTOM_IMAGE_LONGTERMSLEEP)) {
      return;
   }
   DrawScreen(DrawLongTermSleep);
}


void DrawSleepForever() 
{
   epdPrintBegin(100,284,EPD_DIRECTION_X,EPD_SIZE_SINGLE,EPD_COLOR_BLACK);
   epdpr("Sleeping forever :'(");
}

void DrawNoEEPROM() 
{
    epdPrintBegin(50,3,EPD_DIRECTION_X,EPD_SIZE_DOUBLE,EPD_COLOR_BLACK);
    epdpr("EEPROM FAILED :(");
#ifndef LEAN_VERSION
    loadRawBitmap(failed,176,126,EPD_COLOR_RED);
#endif
}

void showNoEEPROM()
{
   DrawScreen(DrawNoEEPROM);
   DrawSleepForever();
}

void DrawNoMAC() 
{
    epdPrintBegin(100,3,EPD_DIRECTION_X,EPD_SIZE_DOUBLE,EPD_COLOR_BLACK);
    epdpr("NO MAC SET :(");
#ifndef LEAN_VERSION
    loadRawBitmap(failed,176,126,EPD_COLOR_RED);
#endif
}

void showNoMAC()
{
   DrawScreen(DrawNoMAC);
   DrawSleepForever();
}

bool displayCustomImage(uint8_t imagetype) 
{
    uint8_t slot = findSlotDataTypeArg(imagetype << 3);
    if(slot != 0xFF) {
    // found a slot for a custom image type
        drawImageFromEeprom(slot,0);
        return true;
    } 
    return false;
}
#endif
