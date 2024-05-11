#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
typedef void (*StrFormatOutputFunc)(uint32_t param /* low byte is data, bits 24..31 is char */) __reentrant;
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
#include "font.h"

DrawingFunction gDrawingFunct;

#pragma callee_saves prvPrintFormat
void prvPrintFormat(StrFormatOutputFunc formatF, uint16_t formatD, const char __code *fmt, va_list vl) __reentrant __naked;

#define VERBOSE_DEBUGDRAWING
#ifdef VERBOSE_DEBUGDRAWING
   #define LOGV(format, ... ) pr(format,## __VA_ARGS__)
   #define LOG_HEXV(x,y) DumpHex(x,y)
#else
   #define LOGV(format, ... )
   #define LOG_HEXV(x,y)
#endif

// Pixel we are drawing currently 0 -> SCREEN_WIDTH - 1
__xdata int16_t gDrawX;
// Line we are drawing currently 0 -> SCREEN_HEIGHT - 1
__xdata int16_t gDrawY;

// Logical units
__xdata int16_t gWinX;
__xdata int16_t gWinEndX;
__xdata int16_t gWinEndY;
__xdata int16_t gCharX;
__xdata int16_t gCharY;

// Physical units
__xdata int16_t gPhyX;
__xdata int16_t gPhyY;
__xdata int16_t gPartY;       // y coord of first line in part
__xdata int16_t gWinY;

__xdata int16_t gWinDrawX;
__xdata int16_t gWinDrawY;
__xdata int8_t gCharWidth;
__xdata int8_t gFontHeight;
__xdata int16_t gTempX;
__xdata int16_t gTempY;

__xdata uint16_t gWinBufNdx;
__bit gWinColor;
__bit gLargeFont;
__bit gDirectionY;
__bit g2BitsPerPixel;   // Input file
__bit gDrawFromFlash;

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
void loadRawBitmap(uint8_t *bmp,uint16_t x,uint16_t y,bool color) 
{
   uint8_t Width = bmp[0];

   LOGV("gDrawY %d\n",gDrawY);
   LOGV("ld bmp x %d, y %d, color %d\n",x,y,color);

   if(color != gRedPass) {
      return;
   }

   if(setWindowY(y,bmp[1])) {
   // Nothing to do Y limit are outside of what we're drawing at the moment
      return;
   }
   gWinColor = color;
#ifdef DEBUGDRAWING
   if((x & 0x7) != 0) {
      LOG("loadRawBitmap invaild x %x\n",x);
   }
   if((Width & 0x7) != 0) {
      LOG("loadRawBitmap invaild Width %x\n",Width);
   }
#endif
   setWindowX(x,Width);

   TempU16 = gWinDrawY - gWinY;
   TempU16 = TempU16 * Width;
   TempU16 = TempU16 >> 3;
   bmp += (TempU16 + 2);

   while(Width) {
      blockbuffer[gWinBufNdx++] |= *bmp++;
      Width = Width - 8;
   }
}

// physical X = 127 - logical y
// physical Y = 295 - logical x
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
   gWinEndX = start + width;
   gPhyY = (SCREEN_HEIGHT - 1) - start;
   gWinDrawY = gDrawY;

   LOGV("gDrawY %d start %d end %d",gDrawY,start,gWinEndX);
   if(gPhyY >= gDrawY && gPhyY < gDrawY + width) {
      gWinY = gPhyY - width + 1;
      LOGV("\n");
      return false;
   }
   LOGV(" outside window\n");
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
static void epdPutchar(uint32_t data) __reentrant 
{
   uint16_t InMask;
   uint16_t FontBits;
   uint8_t OutMask;

   if(gWinColor != gRedPass) {
      return;
   }

   OutMask = (uint8_t) (data >> 24);   // Character we are displaying
   TempU16 = gFontIndexTbl[OutMask - 0x20];
   gCharWidth = TempU16 >> 12;
   if(gLargeFont) {
      gCharWidth = gCharWidth * 2;
   }
   TempU8 = gCharX;
   gCharX += gCharWidth + 1;

   if(setWindowX(TempU8,gCharWidth)) {
      return;
   }
   setWindowY(gCharY,gFontHeight);
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
   FontBits = gPackedData[TempU16 & 0xfff];
   LOGV("TempU16 %d gFontHeight %d FontBits 0x%x\n",TempU16,gFontHeight,FontBits);

   for(TempU8 = 0; TempU8 < gFontHeight; TempU8++) {
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

void epdPrintBegin(uint16_t x,uint16_t y,bool direction,bool fontsize,bool color) 
{
   gLargeFont = fontsize;
   gDirectionY = direction;
   gWinColor = color;
   gCharX = x;
   gCharY = y;
}

void epdpr(const char __code *fmt, ...) __reentrant 
{
    va_list vl;
    va_start(vl, fmt);
    LOGV("epdpr '%s'\n",fmt);
    prvPrintFormat(epdPutchar, 0, fmt, vl);
    va_end(vl);
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

