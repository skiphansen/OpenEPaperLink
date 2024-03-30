
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>

#include "userinterface.h"
#include "asmUtil.h"

#include "board.h"
#include "comms.h"
#include "cpu.h"
// #include "font.h"
#include "lut.h"
#include "powermgt.h"
#include "printf.h"
#include "../oepl-definitions.h"
#include "../oepl-proto.h"
#include "screen.h"
#include "settings.h"
#include "bitmaps.h"
#include "sleep.h"
#include "syncedproto.h"  // for APmac / Channel
#include "timer.h"
#include "drawing.h"

const uint16_t __code fwVersion = FW_VERSION;
const char __code fwVersionSuffix[] = FW_VERSION_SUFFIX;

bool __xdata lowBatteryShown;
bool __xdata noAPShown;


void addOverlay() 
{
#if 0
   loadRawBitmap(battery,SCREEN_WIDTH - 24,SCREEN_HEIGHT - 16,EPD_COLOR_BLACK);
   loadRawBitmap(ant,SCREEN_WIDTH - 24,6,EPD_COLOR_BLACK);
   loadRawBitmap(cross,SCREEN_WIDTH - 16,13,EPD_COLOR_RED);
#endif

#if 1
   if(epdPrintBegin(SCREEN_WIDTH/2,SCREEN_HEIGHT/2,EPD_DIRECTION_Y,EPD_SIZE_SINGLE,EPD_COLOR_BLACK)) {
      epdpr("LW");
   }
#else
   if(currentChannel == 0 && tagSettings.enableNoRFSymbol) {
      loadRawBitmap(ant,SCREEN_WIDTH - 24,6,EPD_COLOR_BLACK);
      loadRawBitmap(cross,SCREEN_WIDTH - 16,13,EPD_COLOR_RED);
      noAPShown = true;
   }
   else {
      noAPShown = false;
   }
   if(batteryVoltage < tagSettings.batLowVoltage 
      && tagSettings.enableLowBatSymbol)
   {
      loadRawBitmap(battery,SCREEN_WIDTH - 24,SCREEN_HEIGHT - 16,EPD_COLOR_BLACK);
      lowBatteryShown = true;
   }
   else {
      lowBatteryShown = false;
   }
#endif
#ifdef ISDEBUGBUILD
#if 0
   epdPrintBegin(87, 0, EPD_DIRECTION_Y, EPD_SIZE_SINGLE, EPD_COLOR_RED);
   epdpr("DEBUG");
   epdPrintEnd();
#endif
#endif
}

void afterFlashScreenSaver() 
{
#if 0
    if (displayCustomImage(CUSTOM_IMAGE_LONGTERMSLEEP)) return;
    selectLUT(EPD_LUT_DEFAULT);
    clearScreen();
    setColorMode(EPD_MODE_NORMAL, EPD_MODE_INVERT);

#if (SCREEN_WIDTH == 152) || (SCREEN_WIDTH == 200)  // 1.54"
    epdPrintBegin(0, 0, EPD_DIRECTION_X, EPD_SIZE_SINGLE, EPD_COLOR_BLACK);
    epdpr("OpenEPaperLink");
    epdPrintEnd();

    epdPrintBegin(100, 32, EPD_DIRECTION_X, EPD_SIZE_SINGLE, EPD_COLOR_RED);
    epdpr("v%04X", fwVersion);
    epdPrintEnd();

    epdPrintBegin(0, 16, EPD_DIRECTION_X, EPD_SIZE_SINGLE, EPD_COLOR_BLACK);
    epdpr("I'm fast asleep");
    epdPrintEnd();

    epdPrintBegin(0, 32, EPD_DIRECTION_X, EPD_SIZE_SINGLE, EPD_COLOR_BLACK);
    epdpr("UwU");
    epdPrintEnd();

    epdPrintBegin(0, 48, EPD_DIRECTION_X, EPD_SIZE_SINGLE, EPD_COLOR_BLACK);
    epdpr("To wake me:");
    epdPrintEnd();

    epdPrintBegin(0, 64, EPD_DIRECTION_X, EPD_SIZE_SINGLE, EPD_COLOR_BLACK);
    epdpr("-Remove batteries");
    epdPrintEnd();
    epdPrintBegin(0, 80, EPD_DIRECTION_X, EPD_SIZE_SINGLE, EPD_COLOR_BLACK);
    epdpr("-Short contacts");
    epdPrintEnd();
    epdPrintBegin(0, 96, EPD_DIRECTION_X, EPD_SIZE_SINGLE, EPD_COLOR_BLACK);
    epdpr("-Reinsert batteries");
    epdPrintEnd();
    epdPrintBegin(0, 112, EPD_DIRECTION_X, EPD_SIZE_SINGLE, EPD_COLOR_BLACK);
    epdpr("openepaperlink.de");
    epdPrintEnd();

    epdPrintBegin(0, 128, EPD_DIRECTION_X, EPD_SIZE_SINGLE, EPD_COLOR_RED);
    epdpr("%02X:%02X", mSelfMac[7], mSelfMac[6]);
    epdpr(":%02X:%02X", mSelfMac[5], mSelfMac[4]);
    epdpr(":%02X:%02X", mSelfMac[3], mSelfMac[2]);
    epdpr(":%02X:%02X", mSelfMac[1], mSelfMac[0]);
    epdPrintEnd();
#endif

#if (SCREEN_WIDTH == 128)  // 2.9"
    epdPrintBegin(0, 295, EPD_DIRECTION_Y, EPD_SIZE_DOUBLE, EPD_COLOR_BLACK);
    epdpr("OpenEPaperLink");
    epdPrintEnd();

    epdPrintBegin(8, 40, EPD_DIRECTION_Y, EPD_SIZE_SINGLE, EPD_COLOR_BLACK);
    epdpr("v%04X", fwVersion);
    epdPrintEnd();

    epdPrintBegin(32, 290, EPD_DIRECTION_Y, EPD_SIZE_SINGLE, EPD_COLOR_BLACK);
    epdpr("I'm fast asleep... UwU   To wake me:");
    epdPrintEnd();

    epdPrintBegin(48, 275, EPD_DIRECTION_Y, EPD_SIZE_SINGLE, EPD_COLOR_BLACK);
    epdpr("- Remove both batteries");
    epdPrintEnd();
    epdPrintBegin(64, 275, EPD_DIRECTION_Y, EPD_SIZE_SINGLE, EPD_COLOR_BLACK);
    epdpr("- Short battery contacts");
    epdPrintEnd();
    epdPrintBegin(80, 275, EPD_DIRECTION_Y, EPD_SIZE_SINGLE, EPD_COLOR_BLACK);
    epdpr("- Reinsert batteries");
    epdPrintEnd();
    epdPrintBegin(112, 293, EPD_DIRECTION_Y, EPD_SIZE_SINGLE, EPD_COLOR_BLACK);
    epdpr("openepaperlink.de");
    epdPrintEnd();
#if (HW_TYPE == SOLUM_M2_BW_29_LOWTEMP)
    epdPrintBegin(110, 155, EPD_DIRECTION_Y, EPD_SIZE_SINGLE, EPD_COLOR_BLACK);
#else
    epdPrintBegin(110, 155, EPD_DIRECTION_Y, EPD_SIZE_SINGLE, EPD_COLOR_RED);
#endif
    epdpr("%02X:%02X", mSelfMac[7], mSelfMac[6]);
    epdpr(":%02X:%02X", mSelfMac[5], mSelfMac[4]);
    epdpr(":%02X:%02X", mSelfMac[3], mSelfMac[2]);
    epdpr(":%02X:%02X", mSelfMac[1], mSelfMac[0]);
    epdPrintEnd();
#endif
#if (SCREEN_WIDTH == 400)  // 4.2"
    epdPrintBegin(3, 3, EPD_DIRECTION_X, EPD_SIZE_DOUBLE, EPD_COLOR_BLACK);
    epdpr("OpenEPaperLink");
    epdPrintEnd();

    epdPrintBegin(360, 8, EPD_DIRECTION_X, EPD_SIZE_SINGLE, EPD_COLOR_BLACK);
    epdpr("v%04X", fwVersion);
    epdPrintEnd();

    epdPrintBegin(10, 48, EPD_DIRECTION_X, EPD_SIZE_SINGLE, EPD_COLOR_BLACK);
    epdpr("I'm fast asleep... UwU   To wake me:");
    epdPrintEnd();

    epdPrintBegin(20, 70, EPD_DIRECTION_X, EPD_SIZE_SINGLE, EPD_COLOR_BLACK);
    epdpr("- Remove both batteries");
    epdPrintEnd();
    epdPrintBegin(20, 86, EPD_DIRECTION_X, EPD_SIZE_SINGLE, EPD_COLOR_BLACK);
    epdpr("- Short battery contacts");
    epdPrintEnd();
    epdPrintBegin(20, 102, EPD_DIRECTION_X, EPD_SIZE_SINGLE, EPD_COLOR_BLACK);
    epdpr("- Reinsert batteries");
    epdPrintEnd();
    epdPrintBegin(3, 283, EPD_DIRECTION_X, EPD_SIZE_SINGLE, EPD_COLOR_BLACK);
    epdpr("openepaperlink.de");
    epdPrintEnd();

    epdPrintBegin(255, 283, EPD_DIRECTION_X, EPD_SIZE_SINGLE, EPD_COLOR_RED);
    epdpr("%02X:%02X", mSelfMac[7], mSelfMac[6]);
    epdpr(":%02X:%02X", mSelfMac[5], mSelfMac[4]);
    epdpr(":%02X:%02X", mSelfMac[3], mSelfMac[2]);
    epdpr(":%02X:%02X", mSelfMac[1], mSelfMac[0]);
    epdPrintEnd();

#endif
    drawWithSleep();
#endif
}

void showSplashScreen() 
{
#if 0
    if (displayCustomImage(CUSTOM_IMAGE_SPLASHSCREEN)) return;
    powerUp(INIT_EPD);

#if (HW_TYPE != SOLUM_M2_BW_29_LOWTEMP)
    selectLUT(EPD_LUT_NO_REPEATS);
#endif

    clearScreen();
    setColorMode(EPD_MODE_NORMAL, EPD_MODE_INVERT);

#if (SCREEN_WIDTH == 152) || (SCREEN_WIDTH == 200)  // 1.54"
    epdPrintBegin(5, 55, EPD_DIRECTION_X, EPD_SIZE_DOUBLE, EPD_COLOR_BLACK);
    epdpr("Starting");
    epdPrintEnd();

#ifndef LEAN_VERSION
    loadRawBitmap(oepli, 12, 12, EPD_COLOR_BLACK);
    loadRawBitmap(cloud, 12, 0, EPD_COLOR_RED);
#endif

    epdPrintBegin(5, 136, EPD_DIRECTION_X, EPD_SIZE_SINGLE, EPD_COLOR_RED);
    epdpr("%02X%02X", mSelfMac[7], mSelfMac[6]);
    epdpr("%02X%02X", mSelfMac[5], mSelfMac[4]);
    epdpr("%02X%02X", mSelfMac[3], mSelfMac[2]);
    epdpr("%02X%02X", mSelfMac[1], mSelfMac[0]);
    epdPrintEnd();

    epdPrintBegin(2, 104, EPD_DIRECTION_X, EPD_SIZE_SINGLE, EPD_COLOR_BLACK);
    addCapabilities();
    epdPrintEnd();

    epdPrintBegin(2, 120, EPD_DIRECTION_X, EPD_SIZE_SINGLE, EPD_COLOR_BLACK);
    epdpr("zbs154 %04X%s", fwVersion, fwVersionSuffix);
    epdPrintEnd();

#ifdef ISDEBUGBUILD
    epdPrintBegin(5, 78, EPD_DIRECTION_X, EPD_SIZE_DOUBLE, EPD_COLOR_RED);
    epdpr("DEBUG");
    epdPrintEnd();
#endif

#endif

#if (SCREEN_WIDTH == 128)  // 2.9"

    epdPrintBegin(0, 295, EPD_DIRECTION_Y, EPD_SIZE_DOUBLE, EPD_COLOR_BLACK);
    epdpr("OpenEPaperLink");
    epdPrintEnd();

#ifdef ISDEBUGBUILD
    epdPrintBegin(35, 280, EPD_DIRECTION_Y, EPD_SIZE_DOUBLE, EPD_COLOR_BLACK);
    epdpr("DEBUG");
    epdPrintEnd();
#endif

    epdPrintBegin(64, 295, EPD_DIRECTION_Y, EPD_SIZE_SINGLE, EPD_COLOR_BLACK);
    addCapabilities();
    epdPrintEnd();

    epdPrintBegin(80, 295, EPD_DIRECTION_Y, EPD_SIZE_SINGLE, EPD_COLOR_BLACK);
    epdpr("zbs29 %04X%s", fwVersion, fwVersionSuffix);
    epdPrintEnd();

    epdPrintBegin(105, 270, EPD_DIRECTION_Y, EPD_SIZE_SINGLE, EPD_COLOR_RED);
    epdpr("MAC: %02X:%02X", mSelfMac[7], mSelfMac[6]);
    epdpr(":%02X:%02X", mSelfMac[5], mSelfMac[4]);
    epdpr(":%02X:%02X", mSelfMac[3], mSelfMac[2]);
    epdpr(":%02X:%02X", mSelfMac[1], mSelfMac[0]);
    epdPrintEnd();

    uint8_t __xdata buffer[17];
    spr(buffer, "%02X%02X", mSelfMac[7], mSelfMac[6]);
    spr(buffer + 4, "%02X%02X", mSelfMac[5], mSelfMac[4]);
    spr(buffer + 8, "%02X%02X", mSelfMac[3], mSelfMac[2]);
    spr(buffer + 12, "%02X%02X", mSelfMac[1], mSelfMac[0]);
    printBarcode(buffer, 120, 284);
#ifndef LEAN_VERSION
   
    loadRawBitmap(cloud, 0, 0, EPD_COLOR_RED);
    loadRawBitmap(oepli, 0, 12, EPD_COLOR_BLACK);
    
#endif
#endif

#if (SCREEN_WIDTH == 176)  // 2.7"

    epdPrintBegin(1, SCREEN_HEIGHT - 2, EPD_DIRECTION_Y, EPD_SIZE_DOUBLE, EPD_COLOR_BLACK);
    epdpr("OpenEPaperLink");
    epdPrintEnd();

#ifdef ISDEBUGBUILD
    epdPrintBegin(35, SCREEN_HEIGHT - 16, EPD_DIRECTION_Y, EPD_SIZE_DOUBLE, EPD_COLOR_BLACK);
    epdpr("DEBUG");
    epdPrintEnd();
#endif

    epdPrintBegin(64, SCREEN_HEIGHT - 1, EPD_DIRECTION_Y, EPD_SIZE_SINGLE, EPD_COLOR_BLACK);
    addCapabilities();
    epdPrintEnd();

    epdPrintBegin(SCREEN_WIDTH - 36, SCREEN_HEIGHT - 1, EPD_DIRECTION_Y, EPD_SIZE_SINGLE, EPD_COLOR_BLACK);
    epdpr("zbs27 %04X%s", fwVersion, fwVersionSuffix);
    epdPrintEnd();

    epdPrintBegin(SCREEN_WIDTH - 17, SCREEN_HEIGHT - 26, EPD_DIRECTION_Y, EPD_SIZE_SINGLE, EPD_COLOR_RED);
    epdpr("MAC: %02X:%02X", mSelfMac[7], mSelfMac[6]);
    epdpr(":%02X:%02X", mSelfMac[5], mSelfMac[4]);
    epdpr(":%02X:%02X", mSelfMac[3], mSelfMac[2]);
    epdpr(":%02X:%02X", mSelfMac[1], mSelfMac[0]);
    epdPrintEnd();

    uint8_t __xdata buffer[17];
    spr(buffer, "%02X%02X", mSelfMac[7], mSelfMac[6]);
    spr(buffer + 4, "%02X%02X", mSelfMac[5], mSelfMac[4]);
    spr(buffer + 8, "%02X%02X", mSelfMac[3], mSelfMac[2]);
    spr(buffer + 12, "%02X%02X", mSelfMac[1], mSelfMac[0]);
    printBarcode(buffer, 120, 284);
#ifndef LEAN_VERSION
    loadRawBitmap(oepli, 24, 12, EPD_COLOR_BLACK);
    loadRawBitmap(cloud, 24, 0, EPD_COLOR_RED);
#endif
#endif

#if (SCREEN_WIDTH == 400)  // 4.2"
    epdPrintBegin(3, 3, EPD_DIRECTION_X, EPD_SIZE_DOUBLE, EPD_COLOR_BLACK);
    epdpr("Starting");
    epdPrintEnd();

    epdPrintBegin(2, 252, EPD_DIRECTION_X, EPD_SIZE_SINGLE, EPD_COLOR_BLACK);
    addCapabilities();
    epdPrintEnd();

    epdPrintBegin(3, 268, EPD_DIRECTION_X, EPD_SIZE_SINGLE, EPD_COLOR_BLACK);
    epdpr("zbs42 %04X%s", fwVersion, fwVersionSuffix);
    epdPrintEnd();
    epdPrintBegin(3, 284, EPD_DIRECTION_X, EPD_SIZE_SINGLE, EPD_COLOR_RED);
    epdpr("MAC: %02X:%02X", mSelfMac[7], mSelfMac[6]);
    epdpr(":%02X:%02X", mSelfMac[5], mSelfMac[4]);
    epdpr(":%02X:%02X", mSelfMac[3], mSelfMac[2]);
    epdpr(":%02X:%02X", mSelfMac[1], mSelfMac[0]);
    epdPrintEnd();
#ifndef LEAN_VERSION
    loadRawBitmap(oepli, 136, 22, EPD_COLOR_BLACK);
    loadRawBitmap(cloud, 136, 10, EPD_COLOR_RED);
#endif
    uint8_t __xdata buffer[17];
    spr(buffer, "%02X%02X", mSelfMac[7], mSelfMac[6]);
    spr(buffer + 4, "%02X%02X", mSelfMac[5], mSelfMac[4]);
    spr(buffer + 8, "%02X%02X", mSelfMac[3], mSelfMac[2]);
    spr(buffer + 12, "%02X%02X", mSelfMac[1], mSelfMac[0]);
    printBarcode(buffer, 392, 264);
    printBarcode(buffer, 384, 264);

#endif
    drawWithSleep();
    powerDown(INIT_EPD);
#endif
}

void showApplyUpdate() 
{
#if 0
    setColorMode(EPD_MODE_NORMAL, EPD_MODE_INVERT);
    selectLUT(1);
    clearScreen();
    setColorMode(EPD_MODE_IGNORE, EPD_MODE_NORMAL);

#if (SCREEN_WIDTH == 152)|| (SCREEN_WIDTH == 200) 
    epdPrintBegin(12, 60, EPD_DIRECTION_X, EPD_SIZE_DOUBLE, EPD_COLOR_BLACK);
#endif
#if (SCREEN_WIDTH == 128)
    epdPrintBegin(48, 220, EPD_DIRECTION_Y, EPD_SIZE_DOUBLE, EPD_COLOR_BLACK);
#endif

#if (SCREEN_WIDTH == 400)
    epdPrintBegin(136, 134, EPD_DIRECTION_X, EPD_SIZE_DOUBLE, EPD_COLOR_BLACK);
#endif

    epdpr("Updating!");
    epdPrintEnd();
    drawNoWait();
#endif
}

void showFailedUpdate() 
{
#if 0
    setColorMode(EPD_MODE_NORMAL, EPD_MODE_INVERT);
    selectLUT(1);
    clearScreen();
    setColorMode(EPD_MODE_IGNORE, EPD_MODE_NORMAL);
#if (SCREEN_WIDTH == 152)|| (SCREEN_WIDTH == 200) 
    epdPrintBegin(18, 60, EPD_DIRECTION_X, EPD_SIZE_SINGLE, EPD_COLOR_BLACK);
#endif
#if (SCREEN_WIDTH == 128)
    epdPrintBegin(48, 270, EPD_DIRECTION_Y, EPD_SIZE_DOUBLE, EPD_COLOR_BLACK);
#endif

#if (SCREEN_WIDTH == 400)
    epdPrintBegin(68, 134, EPD_DIRECTION_X, EPD_SIZE_DOUBLE, EPD_COLOR_BLACK);
#endif
    epdpr("Invalid OTA FW!");
    epdPrintEnd();
    drawWithSleep();
#endif
}

void showAPFound() 
{
#if 0
    if (displayCustomImage(CUSTOM_IMAGE_APFOUND)) return;
    powerUp(INIT_EPD);

    clearScreen();
    setColorMode(EPD_MODE_NORMAL, EPD_MODE_INVERT);
#if (HW_TYPE != SOLUM_M2_BW_29_LOWTEMP)
    selectLUT(1);
#endif

#if (SCREEN_WIDTH == 176)
    epdPrintBegin(0, SCREEN_HEIGHT - 10, EPD_DIRECTION_Y, EPD_SIZE_DOUBLE, EPD_COLOR_BLACK);
    epdpr("Waiting for data");
    epdPrintEnd();
    epdPrintBegin(48, SCREEN_HEIGHT - 17, EPD_DIRECTION_Y, EPD_SIZE_SINGLE, EPD_COLOR_BLACK);
    epdpr("Found the following AP:");
    epdPrintEnd();

    epdPrintBegin(64, SCREEN_HEIGHT - 3, EPD_DIRECTION_Y, EPD_SIZE_SINGLE, EPD_COLOR_BLACK);
    epdpr("AP MAC: %02X:%02X", APmac[7], APmac[6]);
    epdpr(":%02X:%02X", APmac[5], APmac[4]);
    epdpr(":%02X:%02X", APmac[3], APmac[2]);
    epdpr(":%02X:%02X", APmac[1], APmac[0]);
    epdPrintEnd();
    epdPrintBegin(80, SCREEN_HEIGHT - 3, EPD_DIRECTION_Y, EPD_SIZE_SINGLE, EPD_COLOR_BLACK);
    epdpr("Ch: %d RSSI: %d LQI: %d", currentChannel, mLastRSSI, mLastLqi);
    epdPrintEnd();

    epdPrintBegin(SCREEN_WIDTH - 30, SCREEN_HEIGHT - 18, EPD_DIRECTION_Y, EPD_SIZE_SINGLE, EPD_COLOR_BLACK);
    epdpr("Tag MAC: %02X:%02X", mSelfMac[7], mSelfMac[6]);
    epdpr(":%02X:%02X", mSelfMac[5], mSelfMac[4]);
    epdpr(":%02X:%02X", mSelfMac[3], mSelfMac[2]);
    epdpr(":%02X:%02X", mSelfMac[1], mSelfMac[0]);
    epdPrintEnd();

    uint8_t __xdata buffer[17];
    spr(buffer, "%02X%02X", mSelfMac[7], mSelfMac[6]);
    spr(buffer + 4, "%02X%02X", mSelfMac[5], mSelfMac[4]);
    spr(buffer + 8, "%02X%02X", mSelfMac[3], mSelfMac[2]);
    spr(buffer + 12, "%02X%02X", mSelfMac[1], mSelfMac[0]);
    printBarcode(buffer, 168, SCREEN_HEIGHT - 30);
#ifndef LEAN_VERSION
    loadRawBitmap(receive, 36, 1, EPD_COLOR_BLACK);
#endif
#endif

#if (SCREEN_WIDTH == 128)
    epdPrintBegin(0, 285, EPD_DIRECTION_Y, EPD_SIZE_DOUBLE, EPD_COLOR_BLACK);
    epdpr("Waiting for data...");
    epdPrintEnd();
    epdPrintBegin(48, 278, EPD_DIRECTION_Y, EPD_SIZE_SINGLE, EPD_COLOR_BLACK);
    epdpr("Found the following AP:");
    epdPrintEnd();
    epdPrintBegin(64, 293, EPD_DIRECTION_Y, EPD_SIZE_SINGLE, EPD_COLOR_BLACK);
    epdpr("AP MAC: %02X:%02X", APmac[7], APmac[6]);
    epdpr(":%02X:%02X", APmac[5], APmac[4]);
    epdpr(":%02X:%02X", APmac[3], APmac[2]);
    epdpr(":%02X:%02X", APmac[1], APmac[0]);
    epdPrintEnd();
    epdPrintBegin(80, 293, EPD_DIRECTION_Y, EPD_SIZE_SINGLE, EPD_COLOR_BLACK);
    epdpr("Ch: %d RSSI: %d LQI: %d", currentChannel, mLastRSSI, mLastLqi);
    epdPrintEnd();

    epdPrintBegin(103, 258, EPD_DIRECTION_Y, EPD_SIZE_SINGLE, EPD_COLOR_BLACK);
    epdpr("Tag MAC: %02X:%02X", mSelfMac[7], mSelfMac[6]);
    epdpr(":%02X:%02X", mSelfMac[5], mSelfMac[4]);
    epdpr(":%02X:%02X", mSelfMac[3], mSelfMac[2]);
    epdpr(":%02X:%02X", mSelfMac[1], mSelfMac[0]);
    epdPrintEnd();

    uint8_t __xdata buffer[17];
    spr(buffer, "%02X%02X", mSelfMac[7], mSelfMac[6]);
    spr(buffer + 4, "%02X%02X", mSelfMac[5], mSelfMac[4]);
    spr(buffer + 8, "%02X%02X", mSelfMac[3], mSelfMac[2]);
    spr(buffer + 12, "%02X%02X", mSelfMac[1], mSelfMac[0]);
    printBarcode(buffer, 120, 253);
#ifndef LEAN_VERSION
    loadRawBitmap(receive, 36, 14, EPD_COLOR_BLACK);
#endif
#endif
#if (SCREEN_WIDTH == 152)  || (SCREEN_WIDTH == 200) // 1.54"
    epdPrintBegin(25, 0, EPD_DIRECTION_X, EPD_SIZE_DOUBLE, EPD_COLOR_BLACK);
    epdpr("Waiting");
    epdPrintEnd();
    epdPrintBegin(3, 32, EPD_DIRECTION_X, EPD_SIZE_DOUBLE, EPD_COLOR_BLACK);
    epdpr("for data...");
    epdPrintEnd();

    epdPrintBegin(5, 64, EPD_DIRECTION_X, EPD_SIZE_SINGLE, EPD_COLOR_BLACK);
    epdpr("AP MAC:");
    epdPrintEnd();
    epdPrintBegin(5, 80, EPD_DIRECTION_X, EPD_SIZE_SINGLE, EPD_COLOR_BLACK);
    epdpr("%02X%02X", APmac[7], APmac[6]);
    epdpr("%02X%02X", APmac[5], APmac[4]);
    epdpr("%02X%02X", APmac[3], APmac[2]);
    epdpr("%02X%02X", APmac[1], APmac[0]);
    epdPrintEnd();

    epdPrintBegin(5, 96, EPD_DIRECTION_X, EPD_SIZE_SINGLE, EPD_COLOR_BLACK);
    epdpr("Ch:%d rssi:%d lqi:%d", currentChannel, mLastRSSI, mLastLqi);
    epdPrintEnd();

    epdPrintBegin(5, 120, EPD_DIRECTION_X, EPD_SIZE_SINGLE, EPD_COLOR_BLACK);
    epdpr("Tag MAC:");
    epdPrintEnd();

    epdPrintBegin(5, 136, EPD_DIRECTION_X, EPD_SIZE_SINGLE, EPD_COLOR_RED);
    epdpr("%02X%02X", mSelfMac[7], mSelfMac[6]);
    epdpr("%02X%02X", mSelfMac[5], mSelfMac[4]);
    epdpr("%02X%02X", mSelfMac[3], mSelfMac[2]);
    epdpr("%02X%02X", mSelfMac[1], mSelfMac[0]);
    epdPrintEnd();
#endif
#if (SCREEN_WIDTH == 400)
    epdPrintBegin(10, 10, EPD_DIRECTION_X, EPD_SIZE_DOUBLE, EPD_COLOR_BLACK);
    epdpr("Waiting for data...");
    epdPrintEnd();
    epdPrintBegin(48, 80, EPD_DIRECTION_X, EPD_SIZE_SINGLE, EPD_COLOR_BLACK);
    epdpr("Found the following AP:");
    epdPrintEnd();
    epdPrintBegin(48, 96, EPD_DIRECTION_X, EPD_SIZE_SINGLE, EPD_COLOR_BLACK);
    epdpr("AP MAC: %02X:%02X", APmac[7], APmac[6]);
    epdpr(":%02X:%02X", APmac[5], APmac[4]);
    epdpr(":%02X:%02X", APmac[3], APmac[2]);
    epdpr(":%02X:%02X", APmac[1], APmac[0]);
    epdPrintEnd();
    epdPrintBegin(48, 112, EPD_DIRECTION_X, EPD_SIZE_SINGLE, EPD_COLOR_BLACK);
    epdpr("Ch: %d RSSI: %d LQI: %d", currentChannel, mLastRSSI, mLastLqi);
    epdPrintEnd();

    epdPrintBegin(366, 258, EPD_DIRECTION_Y, EPD_SIZE_SINGLE, EPD_COLOR_BLACK);
    epdpr("Tag MAC: %02X:%02X", mSelfMac[7], mSelfMac[6]);
    epdpr(":%02X:%02X", mSelfMac[5], mSelfMac[4]);
    epdpr(":%02X:%02X", mSelfMac[3], mSelfMac[2]);
    epdpr(":%02X:%02X", mSelfMac[1], mSelfMac[0]);
    epdPrintEnd();

    uint8_t __xdata buffer[17];
    spr(buffer, "%02X%02X", mSelfMac[7], mSelfMac[6]);
    spr(buffer + 4, "%02X%02X", mSelfMac[5], mSelfMac[4]);
    spr(buffer + 8, "%02X%02X", mSelfMac[3], mSelfMac[2]);
    spr(buffer + 12, "%02X%02X", mSelfMac[1], mSelfMac[0]);
    printBarcode(buffer, 392, 253);
    printBarcode(buffer, 384, 253);
#ifndef LEAN_VERSION
    loadRawBitmap(receive, 100, 170, EPD_COLOR_BLACK);
#endif
#endif
    addOverlay();
    drawWithSleep();
    powerDown(INIT_EPD);
#endif
}

void showNoAP() 
{
#if 0
    if (displayCustomImage(CUSTOM_IMAGE_NOAPFOUND)) return;
    powerUp(INIT_EPD);
#if (HW_TYPE != SOLUM_M2_BW_29_LOWTEMP)
    selectLUT(EPD_LUT_NO_REPEATS);
#endif

    setColorMode(EPD_MODE_NORMAL, EPD_MODE_INVERT);
    clearScreen();
#if (SCREEN_WIDTH == 128) || (SCREEN_WIDTH == 176) // 2,9"
    epdPrintBegin(0, 285, EPD_DIRECTION_Y, EPD_SIZE_DOUBLE, EPD_COLOR_BLACK);
    epdpr("No AP found :(");
    epdPrintEnd();
    epdPrintBegin(48, 285, EPD_DIRECTION_Y, EPD_SIZE_SINGLE, EPD_COLOR_BLACK);
    epdpr("We'll try again in a");
    epdPrintEnd();
    epdPrintBegin(64, 285, EPD_DIRECTION_Y, EPD_SIZE_SINGLE, EPD_COLOR_BLACK);
    epdpr("little while...");
    epdPrintEnd();
#ifndef LEAN_VERSION
    loadRawBitmap(receive, 36, 24, EPD_COLOR_BLACK);
    loadRawBitmap(failed, 42, 26, EPD_COLOR_RED);
#endif
#endif
#if (SCREEN_WIDTH == 152) || (SCREEN_WIDTH == 200)  // 1.54"
    epdPrintBegin(40, 0, EPD_DIRECTION_X, EPD_SIZE_DOUBLE, EPD_COLOR_BLACK);
    epdpr("No AP");
    epdPrintEnd();
    epdPrintBegin(22, 32, EPD_DIRECTION_X, EPD_SIZE_DOUBLE, EPD_COLOR_BLACK);
    epdpr("found :(");
    epdPrintEnd();

    epdPrintBegin(8, 76, EPD_DIRECTION_X, EPD_SIZE_SINGLE, EPD_COLOR_BLACK);
    epdpr("We'll try again in");
    epdPrintEnd();
    epdPrintBegin(25, 92, EPD_DIRECTION_X, EPD_SIZE_SINGLE, EPD_COLOR_BLACK);
    epdpr("a little while");
    epdPrintEnd();
#endif
#if (SCREEN_WIDTH == 400)  // 4.2"
    epdPrintBegin(10, 10, EPD_DIRECTION_X, EPD_SIZE_DOUBLE, EPD_COLOR_BLACK);
    epdpr("No AP found :(");
    epdPrintEnd();
    epdPrintBegin(10, 274, EPD_DIRECTION_X, EPD_SIZE_SINGLE, EPD_COLOR_BLACK);
    epdpr("We'll try again in a little while");
    epdPrintEnd();
#ifndef LEAN_VERSION
    loadRawBitmap(receive, 76, 120, EPD_COLOR_BLACK);
    loadRawBitmap(failed, 82, 122, EPD_COLOR_RED);
#endif
#endif
    addOverlay();
    drawWithSleep();
    powerDown(INIT_EPD);
#endif
}

void showLongTermSleep() 
{
#if 0
    if (displayCustomImage(CUSTOM_IMAGE_LONGTERMSLEEP)) return;
    selectLUT(EPD_LUT_NO_REPEATS);
    setColorMode(EPD_MODE_NORMAL, EPD_MODE_INVERT);
    clearScreen();
#if (SCREEN_WIDTH == 128)  // 2.9"
    epdPrintBegin(0, 295, EPD_DIRECTION_Y, EPD_SIZE_SINGLE, EPD_COLOR_BLACK);
    epdpr("zZ");
    epdPrintEnd();
#else
    epdPrintBegin(2, SCREEN_HEIGHT - 16, EPD_DIRECTION_X, EPD_SIZE_SINGLE, EPD_COLOR_BLACK);
    epdpr("zZ");
    epdPrintEnd();
#endif
    addOverlay();
    drawWithSleep();
#endif
}

void showNoEEPROM() 
{
#if 0
    selectLUT(EPD_LUT_NO_REPEATS);
    clearScreen();
    setColorMode(EPD_MODE_NORMAL, EPD_MODE_INVERT);
#if (SCREEN_WIDTH == 128)  // 2.9"
    epdPrintBegin(0, 285, EPD_DIRECTION_Y, EPD_SIZE_DOUBLE, EPD_COLOR_BLACK);
    epdpr("EEPROM FAILED :(");
    epdPrintEnd();
    epdPrintBegin(64, 285, EPD_DIRECTION_Y, EPD_SIZE_SINGLE, EPD_COLOR_BLACK);
    epdpr("Sleeping forever :'(");
    epdPrintEnd();
#ifndef LEAN_VERSION
    loadRawBitmap(failed, 42, 26, EPD_COLOR_RED);
#endif
#endif

#if (SCREEN_WIDTH == 176)  // 2.9"
    epdPrintBegin(1, 260, EPD_DIRECTION_Y, EPD_SIZE_DOUBLE, EPD_COLOR_BLACK);
    epdpr("EEPROM FAILED :(");
    epdPrintEnd();
    epdPrintBegin(160, 200, EPD_DIRECTION_Y, EPD_SIZE_SINGLE, EPD_COLOR_BLACK);
    epdpr("Sleeping forever :'(");
    epdPrintEnd();
#ifndef LEAN_VERSION
    loadRawBitmap(failed, 80, 114, EPD_COLOR_RED);
#endif
#endif

#if (SCREEN_WIDTH == 152) || (SCREEN_WIDTH == 200)  // 1.54"
    epdPrintBegin(26, 0, EPD_DIRECTION_X, EPD_SIZE_DOUBLE, EPD_COLOR_BLACK);
    epdpr("EEPROM ");
    epdPrintEnd();
    epdPrintBegin(8, 32, EPD_DIRECTION_X, EPD_SIZE_DOUBLE, EPD_COLOR_BLACK);
    epdpr("FAILED :(");
    epdPrintEnd();
#ifndef LEAN_VERSION
    loadRawBitmap(failed, 60, 72, EPD_COLOR_RED);
#endif
    epdPrintBegin(3, 136, EPD_DIRECTION_X, EPD_SIZE_SINGLE, EPD_COLOR_BLACK);
    epdpr("Sleeping forever :'(");
    epdPrintEnd();
#endif
#if (SCREEN_WIDTH == 400)  // 4.2"
    epdPrintBegin(50, 3, EPD_DIRECTION_X, EPD_SIZE_DOUBLE, EPD_COLOR_BLACK);
    epdpr("EEPROM FAILED :(");
    epdPrintEnd();
#ifndef LEAN_VERSION
    loadRawBitmap(failed, 176, 126, EPD_COLOR_RED);
#endif
    epdPrintBegin(100, 284, EPD_DIRECTION_X, EPD_SIZE_SINGLE, EPD_COLOR_BLACK);
    epdpr("Sleeping forever :'(");
    epdPrintEnd();
#endif
    drawWithSleep();
#endif
}

void showNoMAC() 
{
#if 0
    selectLUT(EPD_LUT_NO_REPEATS);
    clearScreen();
    setColorMode(EPD_MODE_NORMAL, EPD_MODE_INVERT);
#if (SCREEN_WIDTH == 128)  // 2.9"
    epdPrintBegin(0, 285, EPD_DIRECTION_Y, EPD_SIZE_DOUBLE, EPD_COLOR_BLACK);
    epdpr("NO MAC SET :(");
    epdPrintEnd();
    epdPrintBegin(64, 285, EPD_DIRECTION_Y, EPD_SIZE_SINGLE, EPD_COLOR_BLACK);
    epdpr("Sleeping forever :'(");
    epdPrintEnd();
#ifndef LEAN_VERSION
    loadRawBitmap(failed, 42, 26, EPD_COLOR_RED);
#endif
#endif
#if (SCREEN_WIDTH == 152) || (SCREEN_WIDTH == 200)  // 1.54"
    epdPrintBegin(20, 0, EPD_DIRECTION_X, EPD_SIZE_DOUBLE, EPD_COLOR_BLACK);
    epdpr("NO MAC");
    epdPrintEnd();
    epdPrintBegin(30, 32, EPD_DIRECTION_X, EPD_SIZE_DOUBLE, EPD_COLOR_BLACK);
    epdpr("SET :(");
    epdPrintEnd();
#ifndef LEAN_VERSION
    loadRawBitmap(failed, 60, 72, EPD_COLOR_RED);
#endif
    epdPrintBegin(3, 136, EPD_DIRECTION_X, EPD_SIZE_SINGLE, EPD_COLOR_BLACK);
    epdpr("Sleeping forever :'(");
    epdPrintEnd();
#endif
#if (SCREEN_WIDTH == 400)  // 4.2"
    epdPrintBegin(100, 3, EPD_DIRECTION_X, EPD_SIZE_DOUBLE, EPD_COLOR_BLACK);
    epdpr("NO MAC SET :(");
    epdPrintEnd();
#ifndef LEAN_VERSION
    loadRawBitmap(failed, 176, 126, EPD_COLOR_RED);
#endif
    epdPrintBegin(100, 284, EPD_DIRECTION_X, EPD_SIZE_SINGLE, EPD_COLOR_BLACK);
    epdpr("Sleeping forever :'(");
    epdPrintEnd();
#endif
    drawWithSleep();
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

