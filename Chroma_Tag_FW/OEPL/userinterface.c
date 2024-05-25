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
#include "draw_common.h"
#include "ota_hdr.h"
#include "logging.h"

#define DISPLAY_HEIGHT_CHARS  (DISPLAY_HEIGHT / FONT_HEIGHT)
void DrawTagMac(void);
void DrawFwVer(void);
void SetDrawingDefaults(void);
void DisplayLogo(void);
void DisplayLowestVbat(void);

static const char __code gNewLine[] = "\n";
static const char __code gDoubleSpace[] = "\n\n";

__xdata int16_t gSaveX;
__xdata int16_t gSaveY;

void VertCenterLine()
{
   gCharY = (DISPLAY_HEIGHT - FONT_HEIGHT) / 2;
   if(gLargeFont) {
      gCharY -= (FONT_HEIGHT / 2);
   }
}

void addOverlay() 
{
#ifdef DEBUG_FORCE_OVERLAY
// force icons to be display for testing
   gLowBattery = true;
   gCurrentChannel = 0;
   gLowBatteryShown = false;
   tagSettings.enableNoRFSymbol = true;
   tagSettings.enableLowBatSymbol = true;
#endif

   if(gCurrentChannel == 0 && tagSettings.enableNoRFSymbol) {
      gWinColor = EPD_COLOR_BLACK;
      gBmpX = DISPLAY_WIDTH - 24;
      gBmpY = 6;
      loadRawBitmap(ant);
      gWinColor = EPD_COLOR_RED;
      gBmpX = DISPLAY_WIDTH - 16;
      gBmpY = 13;
      loadRawBitmap(cross);
      noAPShown = true;
   }
   else {
      noAPShown = false;
   }
   if(gLowBattery && tagSettings.enableLowBatSymbol) {
      gWinColor = EPD_COLOR_BLACK;
      gBmpX = DISPLAY_WIDTH - 24;
      gBmpY = DISPLAY_HEIGHT - 16;
      loadRawBitmap(battery);
      gLowBatteryShown = true;
   }
   else {
      gLowBatteryShown = false;
   }

#ifdef FW_VERSION_SUFFIX
   SetDrawingDefaults();
   gCharY = DISPLAY_HEIGHT - FONT_HEIGHT - 2;

#ifndef BWY
// Yellow is very low constrast, make it black for BWY displays
   gWinColor = EPD_COLOR_RED;
#endif
   epdpr("DEBUG - ");
   DrawFwVer();
#endif
}

void DrawAfterFlashScreenSaver() 
{
   SetDrawingDefaults();
#if DISPLAY_WIDTH > 296
// wider than 2.9"
   DisplayLogo();
   epdpr(gDoubleSpace);
   epdpr("\f\tI'm fast asleep . . . UwU\n\n");
   gSaveX = gCharX;
   epdpr("To \twake me:\n");
   epdpr("- Remove all batteries\n");
   epdpr("- Short battery contacts\n");
   epdpr("- Reinsert batteries\n\n");
   epdpr("\fopenepaperlink.de\n\n");
// drop indent
   gCharX = gSaveX;
   DrawTagMac();
#ifndef DISABLE_BARCODES
   epdpr(gDoubleSpace);
   printBarcode(gMacString,gSaveX,gCharY);
#endif
#else
   epdpr("\t\bOpenEPaperLink\n\b");
// 2.9" or smaller
   epdpr("openepaperlink.de\n");
// 1.5 line spacing
   gCharY += FONT_HEIGHT / 2;
   epdpr("I'm fast\t asleep . . . to wake me:");
   epdpr("\nRemove batteries, short battery\n");
   epdpr("contacts, reinsert batteries.");
// 1.5 line spacing
   gCharY += FONT_HEIGHT / 2;
   gLeftMargin = 0;
   epdpr(gNewLine);
   DrawTagMac();
#endif
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

void DisplayLogo(void);

void DrawSplashScreen()
{
   SetDrawingDefaults();

#if DISPLAY_WIDTH == 296
// 2.9"
   epdpr("OpenEPaperLink\n\n");
#elif DISPLAY_WIDTH >= 640
// 7.4" or wider
   DisplayLogo();
   epdpr("\t\b\nStarting . . .\n\n\b");
#endif
   DrawFwVer();
   epdpr("\nVBat: %d mV\n",gBootBattV);
   epdpr("Temperature: %dC\n\n",gTemperature);
   DrawTagMac();

#ifndef DISABLE_BARCODES
   epdpr(gDoubleSpace);
   printBarcode(gMacString,gLeftMargin,gCharY);
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
   SetDrawingDefaults();
   DisplayLogo();
#if DISPLAY_WIDTH >= 640
   gLargeFont = EPD_SIZE_DOUBLE;
#else
// Large font won't fit on one line, two looks odd
   gLargeFont = EPD_SIZE_SINGLE;
#endif
   VertCenterLine();
   epdpr("\fFlashing v%04x . . .",gUpdateFwVer);
}

void showApplyUpdate()
{
   DrawScreen(DrawApplyUpdate);
}


void DrawFailedUpdate() 
{
   SetDrawingDefaults();
   DisplayLogo();
   gLargeFont = EPD_SIZE_DOUBLE;
   epdpr("\fOTA FAILED :(\n\n");
   gWinColor = EPD_COLOR_RED;
   switch(gUpdateErr) {
      case OTA_ERR_INVALID_HDR:
         epdpr("\fNot OTA image");
         break;

      case OTA_ERR_WRONG_BOARD:
         epdpr("\fWrong OTA image");
         break;

      case OTA_ERR_INVALID_CRC:
         epdpr("\fCRC error");
         break;

      default:
         epdpr("\fError Code %d",gUpdateErr);
         break;
   }
}

void showFailedUpdate()
{
   DrawScreen(DrawFailedUpdate);
}

void DrawAPFound() 
{
   SetDrawingDefaults();
   DisplayLogo();
   epdpr("\f\t\bWaiting for data . . .\n\b");
   gSaveX = gCharX;
   gSaveY = gCharY;
   epdpr("\nFound the following AP:\n");
   epdpr("AP MAC: %02X%02X",APmac[7],APmac[6]);
   epdpr("%02X%02X",APmac[5],APmac[4]);
   epdpr("%02X%02X",APmac[3],APmac[2]);
   epdpr("%02X%02X\n",APmac[1],APmac[0]);
   epdpr("Ch: %d RSSI: %d LQI: %d\n",gCurrentChannel,mLastRSSI,mLastLqi);

#if DISPLAY_WIDTH > 296
// wider than 2.9"
   epdpr("\nVBat: %d mV, Txing: %d mV, Displaying: %d mV\n\n",
         gBattV,gTxBattV,gRefreshBattV);
// Don't show FW for BETAs, addOverlay will show it
   DrawFwVer();
   epdpr("\n\nTag ");
   DrawTagMac();
   epdpr("\n");
#ifndef DISABLE_BARCODES
   printBarcode(gMacString,gSaveX,gCharY);
#endif

#ifndef LEAN_VERSION
// Display receive on left
   gBmpX = (gSaveX - receive[0]) / 2;
   gBmpY = (DISPLAY_HEIGHT - receive[1]) / 2;
   loadRawBitmap(receive);
#endif
#else
// 2.9" or smaller
// Don't have room for all three battery readings, display the lowest
   DisplayLowestVbat();
#ifndef FW_VERSION_SUFFIX
// Don't show FW for BETAs, addOverlay will show it
   DrawFwVer();
#endif
#endif
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
   SetDrawingDefaults();
   DisplayLogo();
   epdpr("\f\bNo AP found :(\n\b\n");

#if DISPLAY_WIDTH > 296
   epdpr("\fWe'll try again in a little while . . .");
// receive bitmap is 56 x 56, center it on the display
   gBmpX = (DISPLAY_WIDTH - 56)/2;
   gBmpY = (DISPLAY_HEIGHT - 56)/2;
#else
   epdpr("We'll try again in a");
// receive bitmap is 56 x 56, center between the end of the current line
// and the right side of the display
   gBmpY = gCharY;
   gBmpX = gCharX + (DISPLAY_WIDTH - gCharX - 56)/2;
   if(gBmpX & 0x7) {
   // round back to byte boundary
      gBmpX -= (gBmpX & 0x7);
   }
   epdpr("\nlittle while . . .");
#endif

#ifndef LEAN_VERSION
   loadRawBitmap(receive);
// failed bitmap is 48 x 48, adjust starting position to
// overlay the receive ICON
   gBmpY += (56 - 48);
   gWinColor = EPD_COLOR_RED;
   loadRawBitmap(failed);
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
   SetDrawingDefaults();
   DisplayLogo();
   gCharY += FONT_HEIGHT;
   epdpr("\f\t\bzzZZZ . . .\n\b\n");
   DisplayLowestVbat();
   addOverlay();
}

void showLongTermSleep()
{
   if(displayCustomImage(CUSTOM_IMAGE_LONGTERMSLEEP)) {
      return;
   }
   DrawScreen(DrawLongTermSleep);
}


void DrawNoEEPROM() 
{
   SetDrawingDefaults();
   DisplayLogo();
   gLargeFont = EPD_SIZE_DOUBLE;

   epdpr("\fEEPROM FAILED :(\n\n");
   epdpr("\fSleeping forever . . .\n\n");

#ifndef LEAN_VERSION
// failed bitmap is 48 x 48
   gBmpX = (DISPLAY_WIDTH - 48)/2;
   gBmpY = gCharY;
   gWinColor = EPD_COLOR_RED;

   loadRawBitmap(failed);
#endif
}

void showNoEEPROM()
{
   DrawScreen(DrawNoEEPROM);
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

void SetDrawingDefaults()
{
   gDirectionY = EPD_DIRECTION_X;
   gWinColor = EPD_COLOR_BLACK;
   gLargeFont = EPD_SIZE_SINGLE;
   gCharX = 0;
   gLeftMargin = 0;
   gCharY = 0;
   gCenterLine = false;
}

void DisplayLogo()
{
#ifndef LEAN_VERSION
#ifdef SPLASH_LOGO_X
   gBmpX = SPLASH_LOGO_X;
   gBmpY = SPLASH_LOGO_Y;
#else
// Center Logo on top line
   gWinColor = EPD_COLOR_RED;
   gBmpY = 0;
   gBmpX = (DISPLAY_WIDTH - CloudTop[0]) / 2;
   loadRawBitmap(CloudTop);

   gWinColor = EPD_COLOR_BLACK;
   gBmpY += CloudTop[1];
   loadRawBitmap(oepli);

   gBmpY += oepli[1];
   gWinColor = EPD_COLOR_RED;
   loadRawBitmap(CloudBottom);
   gWinColor = EPD_COLOR_BLACK;
   gCharX = gBmpX;
#endif

#if DISPLAY_WIDTH > 296
// Space down from logo a bit
   gCharY = gBmpY + CloudBottom[1] + (FONT_HEIGHT * 2);
#else
   gCharY = gBmpY + CloudBottom[1];
#endif
#endif
}

void DisplayLowestVbat()
{
   epdpr("VBat: %d mV\n",gRefreshBattV != 0 && gRefreshBattV < gTxBattV ? 
         gRefreshBattV:gTxBattV);
}
#endif
