#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include "../oepl-definitions.h"
#include "soc.h"
#include "board.h"
#include "barcode.h"
#include "asmUtil.h"
#include "drawing.h"
#include "printf.h"
#include "screen.h"
#include "eeprom.h"
#include "board.h"
#include "adc.h"
#include "cpu.h"
#include "powermgt.h"
#include "settings.h"
#include "userinterface.h"
#include "logging.h"
#include "draw_common.h"
#include "packed_font.h"

DrawingFunction gDrawingFunct;


// #define VERBOSE_DEBUGDRAWING
#ifdef VERBOSE_DEBUGDRAWING
   #define LOGV(format, ... ) pr(format,## __VA_ARGS__)
   #define LOG_HEXV(x,y) DumpHex(x,y)
#else
   #define LOGV(format, ... )
   #define LOG_HEXV(x,y)
#endif

// Logical units
__xdata int16_t gWinX;
__xdata int16_t gWinEndY;
__xdata int16_t gCharX;
__xdata int16_t gCharY;
__xdata int16_t gLeftMargin;

__xdata int16_t gBmpX;
__xdata int16_t gBmpY;


// Physical units

// Pixel we are drawing currently 0 -> SCREEN_WIDTH - 1
__xdata int16_t gDrawX;
// Line we are drawing currently 0 -> SCREEN_HEIGHT - 1
__xdata int16_t gDrawY;


__xdata int16_t gPhyX;
__xdata int16_t gPhyY;
__xdata int16_t gPartY;       // y coord of first line in part
__xdata int16_t gWinY;

__xdata int16_t gWinDrawX;
__xdata int16_t gWinDrawY;
__xdata int8_t gCharWidth;
__xdata int8_t gCharHeight;

__xdata uint16_t gWinBufNdx;
__bit gWinColor;
__bit gLargeFont;
__bit gDirectionY;
__bit g2BitsPerPixel;   // Input file
__bit gDrawFromFlash;
__bit gCenterLine;

// NB: 8051 data / code space saving KLUDGE!
// Use the locally in a routine but DO NOT call anything if you care
// about the value !!
__xdata uint16_t TempU16;
__xdata uint16_t TempU8;

__xdata uint32_t gEEpromAdr;

bool setWindowX(uint16_t start,uint16_t width);
bool setWindowY(uint16_t start,uint16_t height);
void SetWinDrawNdx(void);
void DoPass(void);

// Screen is 128 x 296 with 2 bits per pixel 
// The B/W data is loaded first then the red/yellow.

// 128 x 296 / 8 = 4736 bytes, we have BLOCK_XFER_BUFFER_SIZE which is
// about 2079 bytes so we need to load the image in parts.
// Lets use 74 lines per part (1184 bytes) and 4 total parts.
// We make two passes, one for B&W pixels and one for red/yellow

#define TOTAL_PART         4
#define LINES_PER_PART     74
#define BYTES_PER_LINE     (SCREEN_WIDTH / 8)
#define PIXELS_PER_PART    (SCREEN_WIDTH * LINES_PER_PART)
#define BYTES_PER_PART     (PIXELS_PER_PART / 8)
#define BYTES_PER_PLANE    (BYTES_PER_LINE * SCREEN_HEIGHT)

// scratch buffer of BLOCK_XFER_BUFFER_SIZE (0x457 / 2079 bytes)
extern uint8_t __xdata blockbuffer[];

#define eih ((struct EepromImageHeader *__xdata) blockbuffer)
void drawImageAtAddress(uint32_t addr) __reentrant 
{
// Clear overlay flags since we are drawing a new screen
   gLowBatteryShown = false;
   noAPShown = false;
   gEEpromAdr = addr;
   gDrawFromFlash = true;

   powerUp(INIT_EEPROM);
   eepromRead(gEEpromAdr,blockbuffer,sizeof(struct EepromImageHeader));
   gEEpromAdr += sizeof(struct EepromImageHeader);

   if(eih->dataType == DATATYPE_IMG_RAW_1BPP) {
      g2BitsPerPixel = false;
   }
   else if(eih->dataType == DATATYPE_IMG_RAW_2BPP) {
      g2BitsPerPixel = true;
   }
   else {
      LOGA("dataType 0x%x not supported\n",eih->dataType);
      DumpHex(blockbuffer,sizeof(struct EepromImageHeader));
      powerDown(INIT_EEPROM);
      return;
   }


   gRedPass = false;
   screenTxStart();
   DoPass();
   gRedPass = true;
   screenTxStart();
   DoPass();
// Finished with SPI flash
   powerDown(INIT_EEPROM);
   drawWithSleep();
   #undef eih
}

void DoPass()
{
   uint8_t Part;
   uint16_t i;
   uint8_t Mask = 0x80;
   uint8_t Value = 0;

   gPartY = 0;
   gDrawY = 0;

   einkSelect();
   for(Part = 0; Part < TOTAL_PART; Part++) {
      if(gDrawFromFlash) {
      // Read 90 lines of pixels
         if(gRedPass && !g2BitsPerPixel) {
         // Dummy pass
            xMemSet(blockbuffer,0,BYTES_PER_PART);
         }
         else {
            eepromRead(gEEpromAdr,blockbuffer,BYTES_PER_PART);
            gEEpromAdr += BYTES_PER_PART;
         }
#if 0
         for(i = 0; i < LINES_PER_PART; i++) {
            addOverlay();
            gDrawY++;
         }
#endif
      }
      else {
         xMemSet(blockbuffer,0,BYTES_PER_PART);
         for(i = 0; i < LINES_PER_PART; i++) {
            gDrawingFunct();
            gDrawY++;
         }
      }

      for(i = 0; i < BYTES_PER_PART; i++) {
         while(Mask != 0) {
            if(gRedPass) {
            // red/yellow pixel, 1 bit
               Value <<= 1;
               if(!(blockbuffer[i] & Mask)) {
                  Value |= 1;
               }
               if(Mask & 0b00000001) {
               // Value ready, send it
                  screenByteTx(Value);
               }
            }
            else {
            // B/W pixel, 2 bits
               Value <<= 2;
               if(blockbuffer[i] & Mask) {
                  Value |= PIXEL_BLACK;
               }
               if(Mask & 0b00010001) {
               // Value ready, send it
                  screenByteTx(Value);
               }
            }

            Mask >>= 1; // next bit
         }
         Mask = 0x80;
      }
      gPartY += LINES_PER_PART;
   }
   einkDeselect();
}

void DrawScreen(DrawingFunction DrawIt)
{
// Clear overlay flags since we are drawing a new screen
   gLowBatteryShown = false;
   noAPShown = false;

   gDrawFromFlash = false;
   g2BitsPerPixel = true;
   gRedPass = false;
   gDrawingFunct = DrawIt;
   screenTxStart();
   DoPass();
   gRedPass = true;
   screenTxStart();
   DoPass();
   drawWithSleep();
}

// x,y where to put bmp.  (x must be a multiple of 8)
// bmp[0] =  bmp width in pixels (must be a multiple of 8)
// bmp[1] =  bmp height in pixels
// bmp[2...] = pixel data 1BBP
void loadRawBitmap(uint8_t *bmp)
{
   uint8_t Width;
   uint8_t Bits;

   if(gWinColor != gRedPass || setWindowX(gBmpX,bmp[1])) {
   // Nothing to do Y limit are outside of what we're drawing at the moment
      return;
   }
   Width = bmp[0];
   setWindowY(gBmpY,Width);

   LOGV("ld bmp gBmpX %d, gBmpY %d, color %d\n",gBmpX,gBmpY,gWinColor);
   LOGV("gDrawY %d height %d\n",gDrawY,bmp[1]);

   TempU16 = gWinDrawY - gWinY;
   LOGV("TempU16 %d\n",TempU16);
   TempU16 = TempU16 * Width;
   LOGV("TempU16 %d\n",TempU16);
   TempU16 = TempU16 >> 3;
   LOGV("TempU16 %d\n",TempU16);
   bmp += (TempU16 + 2);

   if((gPhyX & 0x7) != 0 || (Width & 0x7) != 0) {
      uint8_t OutMask = 0x80 >> (gPhyX & 0x7);
      uint8_t InMask = 0x80;
      LOGV("Using slow method\nwidth %d ",Width);

      Bits = *bmp++;
      while(Width--) {
         LOGV("Bits 0x%02x InMask 0x%02x OutMask 0x%02x\n",Bits,InMask,OutMask);
         if(Bits & InMask) {
            blockbuffer[gWinBufNdx] |= OutMask;
         }
         LOGV("blockbuffer[%d] 0x%02x\n",gWinBufNdx,blockbuffer[gWinBufNdx]);
         InMask >>= 1;
         if(InMask == 0) {
            Bits = *bmp++;
            InMask = 0x80;
            LOGV("New Bits 0x%02x\n",Bits);
         }

         OutMask >>= 1;
         if(OutMask == 0) {
            OutMask = 0x80;
            gWinBufNdx++;
            LOGV("new blockbuffer[%d] 0x%02x\n",gWinBufNdx,blockbuffer[gWinBufNdx]);
         }
         LOGV("Width %d ",Width);
      }
      LOGV("blockbuffer[%d] 0x%02x\n",gWinBufNdx,blockbuffer[gWinBufNdx]);
   }
   else {
      LOGV("Using fast method width %d bits 0x%02x\n",Width,*bmp);
      while(Width) {
         blockbuffer[gWinBufNdx] |= *bmp++;
         LOGV("blockbuffer[%d] 0x%02x\n",gWinBufNdx,blockbuffer[gWinBufNdx]);
         gWinBufNdx++;
         Width -= 8;
      }
   }
}

// physical Y = 295 - logical X
// physical X = logical Y
void SetWinDrawNdx()
{
   LOGV("SetWinDrawNdx: gPartY %d gPhyX %d gPhyY %d gDrawY %d\n",
        gPartY,gPhyX,gPhyY,gDrawY);
   gWinBufNdx = gPhyX >> 3;
   LOGV("gWinBufNdx 1 %d\n",gWinBufNdx);
   gWinBufNdx += (gWinDrawY - gPartY) * BYTES_PER_LINE;
   LOGV("gWinBufNdx 2 %d\n",gWinBufNdx);
}

// Set logical window X position and width in pixels
// return true if the window is outside of range we're drawing at the moment
bool setWindowX(uint16_t start,uint16_t width) 
{
   gWinX = start;
   gWinDrawX = start;
   gPhyY = (SCREEN_HEIGHT - 1) - start;
   gWinDrawY = gDrawY;

   if(gPhyY >= gDrawY && gPhyY < gDrawY + width) {
      LOGV("gDrawY %d start %d end %d\n",gDrawY,start,start + width);
      gWinY = gPhyY - width + 1;
      return false;
   }
   return true;
}

// Set logical window Y position and height in pixels
// return true if the window is outside of range we're drawing at the moment
bool setWindowY(uint16_t start,uint16_t height) 
{
   gPhyX = start;
   gWinEndY = start + height;
   SetWinDrawNdx();
   return false;
}

// https://raw.githubusercontent.com/basti79/LCD-fonts/master/10x16_vertikal_MSB_1.h

// Writes a single character to the framebuffer
// Routine is specific to a 10w x 16h font, uc8159 controller
// 
// Note: 
//  The first bit on the left is the MSB of the second byte.  
//  The last bit on the right is the LSB of the first byte.  
// 
// For example: "L"
// static const uint8_t __code font[96][20]={ // https://raw.githubusercontent.com/basti79/LCD-fonts/master/10x16_vertikal_MSB_1.h
//{0x00,0x00,0xF8,0x1F,0x08,0x00,0x08,0x00,0x08,0x00,0x08,0x00,0x08,0x00,0x08,0x00,0x00,0x00,0x00,0x00}, // 0x4C
//                  0x00,0x00 <- left
//   **********     0xF8,0x1F
//            *     0x08,0x00
//            *     0x08,0x00
//            *     0x08,0x00
//            *     0x08,0x00
//            *     0x08,0x00
//            *     0x08,0x00
//                  0x00,0x00
//                  0x00,0x00 <- right
// ^              ^
// |              |
// |              +--- Bottom
// +-- Top 
// So 16 bits [byte1]:[Byte 0}
#pragma callee_saves epdPutchar
void epdPutchar(uint32_t data) __reentrant 
{
   uint16_t InMask;
   uint16_t FontBits;
   uint8_t OutMask;

   OutMask = (uint8_t) (data >> 24);   // Character we are displaying

   if(OutMask < 0x20) {
      ProcessEscapes(OutMask);
      return;
   }

   TempU16 = gFontIndexTbl[OutMask - 0x20];
   gCharWidth = TempU16 >> 12;
   if(gLargeFont) {
      gCharWidth = gCharWidth * 2;
   }
   TempU8 = gCharX;
   gCharX += gCharWidth + 1;

   if(gWinColor != gRedPass || setWindowX(TempU8,gCharWidth)) {
      return;
   }
   setWindowY(gCharY,gCharHeight);
   TempU16 &= 0xfff;
   InMask = 0x8000;
   LOGV("writeCharEPD '%c' gPhyX %d\n",OutMask,gPhyX);
   LOGV("  gDrawY %d gWinY %d gCharWidth %d\n",gDrawY,gWinY,gCharWidth);
   LOGV("  In byte blockbuffer[%d] 0x%x\n",gWinBufNdx,blockbuffer[gWinBufNdx]);

   OutMask = 0x80 >> (gPhyX & 0x7);
   LOGV("  OutMask 0x%x ",OutMask);

   TempU8 = gCharWidth - (gDrawY - gWinY) - 1;
   if(gLargeFont) {
      TempU8 /= 2;
   }
   TempU16 += TempU8;
   FontBits = gPackedData[TempU16];
   LOGV("TempU16 %d gCharHeight %d FontBits 0x%x\n",TempU16,gCharHeight,FontBits);

   for(TempU8 = 0; TempU8 < gCharHeight; TempU8++) {
      if(FontBits & InMask) {
         blockbuffer[gWinBufNdx] |= OutMask;
      }
      if(gLargeFont) {
         if(TempU8 & 1) {
            InMask = InMask >> 1;
         }
      }
      else {
         InMask = InMask >> 1;
      }
      OutMask = OutMask >> 1;
      if(OutMask == 0) {
         LOGV("  blockbuffer[%d] 0x%x\n",gWinBufNdx,blockbuffer[gWinBufNdx]);
         gWinBufNdx++;
         OutMask = 0x80;
      }
   }
   if(OutMask != 0x80) {
      LOGV("  blockbuffer[%d] 0x%x\n",
           gWinBufNdx,blockbuffer[gWinBufNdx]);
   }
}

#define BARCODE_ROWS    40

#ifndef DISABLE_BARCODES
void printBarcode(const char __xdata *string, uint16_t x, uint16_t y) 
{
   uint8_t OutMask;

   if(gRedPass) {
   // Bar codes are always B&W
      return;
   }
   xMemSet(&gBci,0,sizeof(gBci));
   gBci.str = string;

   uint16_t test = xStrLen(string);

   if(!setWindowY(y,BARCODE_ROWS)) {
      gWinColor = EPD_COLOR_BLACK;
      setWindowX(x, x + barcodeWidth(xStrLen(string)));
      OutMask = (0x80 >> (gWinDrawX & 0x7));
      while(gBci.state != BarCodeDone) {
         if(barcodeNextBar()) {
            blockbuffer[gWinBufNdx] |= OutMask;
         }
         OutMask = OutMask >> 1;
         if(OutMask == 0) {
            LOGV("  Next out byte blockbuffer[%d] 0x%x\n",
                 gWinBufNdx,blockbuffer[gWinBufNdx]);
            gWinBufNdx++;
            OutMask = 0x80;
         }
      }
   }
}
#endif

