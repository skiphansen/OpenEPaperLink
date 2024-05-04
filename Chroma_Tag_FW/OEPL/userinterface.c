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

void DrawTagMac(void);
void DrawFwVer(void);

void VertCenterLine()
{
   gCharY = (DISPLAY_HEIGHT - FONT_HEIGHT) / 2;
   if(gLargeFont) {
      gCharY -= (FONT_HEIGHT / 2);
   }
}

void HorzCenterLine(uint8_t Chars)
{
   if(gLargeFont) {
      gCharX = (DISPLAY_WIDTH/2) - (Chars * (FONT_WIDTH + 1));
   }
   else {
      gCharX = (DISPLAY_WIDTH/2) - ((Chars * (FONT_WIDTH + 1)/2));
   }
}

void CenterLine(uint8_t Chars)
{
   HorzCenterLine(Chars);
   VertCenterLine();
}

void addOverlay() 
{
#ifdef DEBUG_FORCE_OVERLAY
// force icons to be display for testing
   gLowBattery = true;
   gCurrentChannel = 0;
#endif

   if(gCurrentChannel == 0 && tagSettings.enableNoRFSymbol) {
      loadRawBitmap(ant,DISPLAY_WIDTH - 24,6,EPD_COLOR_BLACK);
      loadRawBitmap(cross,DISPLAY_WIDTH - 16,13,EPD_COLOR_RED);
      noAPShown = true;
   }
   else {
      noAPShown = false;
   }
   if(gLowBattery && tagSettings.enableLowBatSymbol) {
      loadRawBitmap(battery,DISPLAY_WIDTH - 24,DISPLAY_HEIGHT - 16,EPD_COLOR_BLACK);
      gLowBatteryShown = true;
   }
   else {
      gLowBatteryShown = false;
   }
#ifdef FW_VERSION_SUFFIX
   epdPrintBegin(0,DISPLAY_HEIGHT - FONT_HEIGHT - 2,
                 EPD_DIRECTION_X,
                 EPD_SIZE_SINGLE,
#ifdef BWY
// Yellow is very low constrast, make it black for BWY displays
                 EPD_COLOR_BLACK);
#else
                 EPD_COLOR_RED);
#endif
   epdpr("DEBUG - ");
   DrawFwVer();
#endif
}

void SetTextPos(int16_t x, int16_t y)
{
// Save gCharX, gCharY for possible use later
    gCharX = x;
    gCharY = y;
    gTempX = x;
    gTempY = y;

    if(gDirectionY) {
       gFontHeight = gLargeFont ? FONT_WIDTH * 2 : FONT_WIDTH;
       gFontWidth = gLargeFont ? FONT_HEIGHT * 2 : FONT_HEIGHT;
    }
    else {
       gFontHeight = gLargeFont ? FONT_HEIGHT * 2 : FONT_HEIGHT;
       gFontWidth = gLargeFont ? FONT_WIDTH * 2 : FONT_WIDTH;
    }
}

void NextLine(uint8_t Lines)
{
// Save gCharX, gCharY for possible use later
    gCharX = gTempX;
    gCharY = gTempY + (Lines * FONT_HEIGHT);
    gTempY = gCharY;
}

void DrawAfterFlashScreenSaver() 
{
   gLargeFont = EPD_SIZE_DOUBLE;
   gDirectionY = EPD_DIRECTION_X;
   gWinColor = EPD_COLOR_BLACK;

   SetTextPos(16,3);
   // epdPrintBegin(3,3,EPD_DIRECTION_X,EPD_SIZE_DOUBLE,EPD_COLOR_BLACK);
   epdpr("OpenEPaperLink");

   gLargeFont = EPD_SIZE_SINGLE;
   NextLine(3);

   //epdPrintBegin(10,48,EPD_DIRECTION_X,EPD_SIZE_SINGLE,EPD_COLOR_BLACK);
   epdpr("I'm fast asleep... UwU");
   NextLine(2);
   epdpr("wake me:");
   NextLine(1);

// Indent
   gCharX = gCharX + (2 * FONT_WIDTH);
   gTempX = gCharX;

   // epdPrintBegin(20,70,EPD_DIRECTION_X,EPD_SIZE_SINGLE,EPD_COLOR_BLACK);
   epdpr("- Remove all batteries");
   NextLine(1);

   //epdPrintBegin(20,86,EPD_DIRECTION_X,EPD_SIZE_SINGLE,EPD_COLOR_BLACK);
   epdpr("- Short battery contacts");
   NextLine(1);

   // epdPrintBegin(20,102,EPD_DIRECTION_X,EPD_SIZE_SINGLE,EPD_COLOR_BLACK);
   epdpr("- Reinsert batteries");
   NextLine(2);

// Drop indent
   gCharX = 16;
   gTempX = gCharX;

   // epdPrintBegin(3,283,EPD_DIRECTION_X,EPD_SIZE_SINGLE,EPD_COLOR_BLACK);
   epdpr("openepaperlink.de");
   NextLine(2);

   // epdPrintBegin(255, 283, EPD_DIRECTION_X, EPD_SIZE_SINGLE, EPD_COLOR_RED);
   DrawTagMac();
   NextLine(2);

   addOverlay();
//   epdPrintBegin(360,8,EPD_DIRECTION_X,EPD_SIZE_SINGLE,EPD_COLOR_BLACK);
//   DrawFwVer();
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
   epdpr("MAC: %s",gMacString);
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
   gLargeFont = EPD_SIZE_DOUBLE;
   gDirectionY = EPD_DIRECTION_X;
   gWinColor = EPD_COLOR_BLACK;

   SetTextPos(48,80);
//   epdPrintBegin(48,80,EPD_DIRECTION_X,EPD_SIZE_DOUBLE,EPD_COLOR_BLACK);
   epdpr("Starting...");

   gLargeFont = EPD_SIZE_SINGLE;

//   epdPrintBegin(48,144,EPD_DIRECTION_X,EPD_SIZE_SINGLE,EPD_COLOR_BLACK);
   NextLine(4);
   DrawFwVer();
#if 0
   epdPrintBegin(48,176,EPD_DIRECTION_X,EPD_SIZE_SINGLE,
#ifdef BWY
// Yellow is very low constrast, make it black for BWY displays
                 EPD_COLOR_BLACK);
#else
                 EPD_COLOR_RED);
#endif
#endif
   NextLine(2);
   DrawTagMac();

//   epdPrintBegin(48,192,EPD_DIRECTION_X,EPD_SIZE_SINGLE,EPD_COLOR_BLACK);
   NextLine(1);
   epdpr("VBat: %d mV",gBootBattV);
   NextLine(1);
//   epdPrintBegin(48,208,EPD_DIRECTION_X,EPD_SIZE_SINGLE,EPD_COLOR_BLACK);
   epdpr("Temperature %dC",gTemperature);

#ifndef LEAN_VERSION
   loadRawBitmap(oepli,136,22,EPD_COLOR_BLACK);
   loadRawBitmap(cloud,136,10,EPD_COLOR_RED);
#endif

}

void showSplashScreen()
{
   if(displayCustomImage(CUSTOM_IMAGE_SPLASHSCREEN)) {
      return;
   }
   UpdateVBatt();
   DrawScreen(DrawSplashScreen);
}

void DrawApplyUpdate() 
{
#if DISPLAY_WIDTH >= 640
   gLargeFont = EPD_SIZE_DOUBLE;
#else
// Large font won't fit on one line, two looks odd
   gLargeFont = EPD_SIZE_SINGLE;
#endif
   gDirectionY = EPD_DIRECTION_X;
   gWinColor = EPD_COLOR_BLACK;
//   CenterLine(18);
//   gCharX = 0;
//   gCharY = 0;
   CenterLine(1);
   SetTextPos(gCharX,gCharY);
//   epdpr("Flashing v%04x ...",gUpdateFwVer);
   epdpr("ABCDE");
}

void showApplyUpdate()
{
   DrawScreen(DrawApplyUpdate);
}


void DrawFailedUpdate() 
{
   gDirectionY = EPD_DIRECTION_X;
   gWinColor = EPD_COLOR_BLACK;
   gLargeFont = EPD_SIZE_DOUBLE;
   HorzCenterLine(13);
   gCharY = 0;
   epdpr("OTA FAILED :(");

   gWinColor = EPD_COLOR_RED;
   CenterLine(12);
   epdpr("Error Code %d",gUpdateErr);
}

void showFailedUpdate()
{
   DrawScreen(DrawFailedUpdate);
}

void DrawAPFound() 
{
   gLargeFont = EPD_SIZE_DOUBLE;
   gDirectionY = EPD_DIRECTION_X;
   gWinColor = EPD_COLOR_BLACK;

   SetTextPos(10,10);
   epdpr("Waiting for data...");

   gLargeFont = EPD_SIZE_SINGLE;
   NextLine(3);
   epdpr("Found the following AP:");

   NextLine(1);
   epdpr("AP MAC: %02X%02X",APmac[7],APmac[6]);
   epdpr("%02X%02X",APmac[5],APmac[4]);
   epdpr("%02X%02X",APmac[3],APmac[2]);
   epdpr("%02X%02X",APmac[1],APmac[0]);

   NextLine(1);
   epdpr("Ch: %d RSSI: %d LQI: %d",gCurrentChannel,mLastRSSI,mLastLqi);

   NextLine(1);
   epdpr("Tag ");
   DrawTagMac();

   NextLine(1);
#if DISPLAY_WIDTH >= 640
   epdpr("VBat: %d mV, Txing: %d mV, Displaying: %d mV",
         gBattV,gTxBattV,gRefreshBattV);
#else
// Don't have room for all three, display lowest
   epdpr("VBat: ");
   if(gRefreshBattV != 0 && gRefreshBattV < gTxBattV) {
      epdpr("%d mV",gRefreshBattV);
   }
   else {
      epdpr("%d mV",gTxBattV);
   }
#endif

   NextLine(1);
   DrawFwVer();

#ifndef DISABLE_BARCODES
   gTempY = gTempY + (2 * FONT_HEIGHT);
   printBarcode(gMacString,48,gTempY);
   gTempY += FONT_HEIGHT;
#endif

#ifndef LEAN_VERSION
   loadRawBitmap(receive,100,gTempY + (2 * FONT_HEIGHT),EPD_COLOR_BLACK);
#endif
   addOverlay();
}

void showAPFound() 
{
   if(displayCustomImage(CUSTOM_IMAGE_APFOUND)) {
      return;
   }
   UpdateVBatt();
   DrawScreen(DrawAPFound);
}

void DrawNoAP() 
{
   gDirectionY = EPD_DIRECTION_X;
   gWinColor = EPD_COLOR_BLACK;
   gLargeFont = EPD_SIZE_DOUBLE;
   gCharY = 0;
   HorzCenterLine(14);
   epdpr("No AP found :(");
   gLargeFont = EPD_SIZE_SINGLE;
   NextLine(3);
   HorzCenterLine(36);
   epdpr("We'll try again in a little while...");

#ifndef LEAN_VERSION
// receive bitmap is 56 x 56

   loadRawBitmap(receive,(DISPLAY_WIDTH - 56)/2,(DISPLAY_HEIGHT-56)/2,
                 EPD_COLOR_BLACK);
// failed bitmap is 48 x 48
   loadRawBitmap(failed,(DISPLAY_WIDTH-48)/2,(DISPLAY_HEIGHT-48)/2,EPD_COLOR_RED);
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
   gLargeFont = EPD_SIZE_SINGLE;
   gDirectionY = EPD_DIRECTION_X;
   gWinColor = EPD_COLOR_BLACK;
   CenterLine(8);
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
// failed bitmap is 48 x 48
   loadRawBitmap(failed,(DISPLAY_WIDTH-48)/2,(DISPLAY_HEIGHT-48)/2,EPD_COLOR_RED);
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
// failed bitmap is 48 x 48
   loadRawBitmap(failed,(DISPLAY_WIDTH-48)/2,(DISPLAY_HEIGHT-48)/2,EPD_COLOR_RED);
#endif
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
