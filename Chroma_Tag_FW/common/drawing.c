#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
typedef void (*StrFormatOutputFunc)(uint32_t param /* low byte is data, bits 24..31 is char */) __reentrant;
#include "../oepl-definitions.h"
#include "barcode.h"
#include "asmUtil.h"
#include "drawing.h"
#include "printf.h"
#include "screen.h"
#include "eeprom.h"
#include "chars.h"
#include "board.h"
#include "adc.h"
#include "cpu.h"
#include "powermgt.h"
#include "settings.h"
#include "userinterface.h"
#include "logging.h"
#include "font.h"

#pragma callee_saves prvPrintFormat
void prvPrintFormat(StrFormatOutputFunc formatF, uint16_t formatD, const char __code *fmt, va_list vl) __reentrant __naked;

// #define VERBOSE_DEBUGDRAWING
#ifdef VERBOSE_DEBUGDRAWING
   #define LOGV(format, ... ) pr(format,## __VA_ARGS__)
   #define LOG_HEXV(x,y) DumpHex(x,y)
#else
   #define LOGV(format, ... )
   #define LOG_HEXV(x,y)
#endif

// Line we are drawing currently 0 -> SCREEN_HEIGHT - 1
__xdata int16_t gDrawX;
__xdata int16_t gDrawY;

__xdata int16_t gWinX;
__xdata int16_t gWinEndX;
__xdata int16_t gWinY;
__xdata int16_t gWinEndY;

__xdata int16_t gPartY;       // y coord of first line in part
__xdata int16_t gWinDrawX;
__xdata int16_t gWinDrawY;
__xdata int16_t gCharX;
__xdata int16_t gCharY;
__xdata int8_t gFontWidth;
__xdata int8_t gFontHeight;


__xdata uint16_t gWinBufNdx;
__bit gWinColor;
__bit gLargeFont;
__bit gDirectionY;
__bit g2BitsPerPixel;

// NB: 8051 data / code space saving KLUDGE!
// Use the locally in a routine but DO NOT call anything if you care
// about the value !!
__xdata uint16_t TempU16;
__xdata uint16_t TempU8;

bool setWindowX(uint16_t start,uint16_t width);
bool setWindowY(uint16_t start,uint16_t height);
void SetWinDrawNdx(void);

// Screen is 640 x 384 with 2 bits per pixel we need 61,440 (60K) bytes
// which of course we don't have.
// Read data as 64 chunks of 960 bytes (480 bytes of b/w, 480 bytes of r/y),
// convert to pixels and them out.
#define LINES_PER_PART     6  
#define TOTAL_PART         64 
#define BYTES_PER_LINE     (SCREEN_WIDTH / 8)
#define PIXELS_PER_PART    (SCREEN_WIDTH * LINES_PER_PART)
#define BYTES_PER_PART     (PIXELS_PER_PART / 8)
#define BYTES_PER_PLANE    (BYTES_PER_LINE * SCREEN_HEIGHT)

// scratch buffer of BLOCK_XFER_BUFFER_SIZE (0x457 / 1,111 bytes)
extern uint8_t __xdata blockbuffer[];

#define eih ((struct EepromImageHeader *__xdata) blockbuffer)
void drawImageAtAddress(uint32_t addr) __reentrant 
{
   uint32_t Adr = addr;
   uint8_t Part;
   uint16_t i;
   uint16_t j;
   uint8_t Mask = 0x80;
   uint8_t Value = 0;
   uint8_t Pixel;

   powerUp(INIT_EEPROM);
   eepromRead(Adr,blockbuffer,sizeof(struct EepromImageHeader));
   Adr += sizeof(struct EepromImageHeader);

   if(eih->dataType == DATATYPE_IMG_RAW_1BPP) {
      g2BitsPerPixel = false;
   }
   else if(eih->dataType == DATATYPE_IMG_RAW_2BPP) {
      g2BitsPerPixel = true;
   }
   else {
      LOG("dataType 0x%x not supported\n",eih->dataType);
      DumpHex(blockbuffer,sizeof(struct EepromImageHeader));
      powerDown(INIT_EEPROM);
      return;
   }

   screenTxStart(false);
   gPartY = 0;
   gDrawY = 0;
   for(Part = 0; Part < TOTAL_PART; Part++) {
#if 1
   // Read 6 lines of b/w pixels
      eepromRead(Adr,blockbuffer,BYTES_PER_PART);
      if(g2BitsPerPixel) {
      // Read 6 lines of red/yellow pixels
         eepromRead(Adr+BYTES_PER_PLANE,&blockbuffer[BYTES_PER_PART],
                    BYTES_PER_PART);
      }
      else {
         xMemSet(&blockbuffer[BYTES_PER_PART],0,BYTES_PER_PART);
      }
      Adr += BYTES_PER_PART;
#else
      xMemSet(blockbuffer,0,BYTES_PER_PART * 2);
#endif

      for(i = 0; i < LINES_PER_PART; i++) {
         addOverlay();
         gDrawY++;
      }
      j = BYTES_PER_PART;
      for(i = 0; i < BYTES_PER_PART; i++) {
         while(Mask != 0) {
         // B/W bit
            if(blockbuffer[i] & Mask) {
               Pixel = PIXEL_BLACK;
            }
            else {
               Pixel = PIXEL_WHITE;
            }

         // red/yellow W bit
            if(blockbuffer[j] & Mask) {
               Pixel = PIXEL_RED_YELLOW;
            }
            Value <<= 4;
            Value |= Pixel;
            if(Mask & 0b10101010) {
            // Value ready, send it
               screenByteTx(Value);
            }
            Mask >>= 1; // next bit
         }
         Mask = 0x80;
         j++;
      }
      gPartY += LINES_PER_PART;
   }
// Finished with SPI flash
   powerDown(INIT_EEPROM);
   drawWithSleep();
   #undef eih
}

void DrawScreen(DrawingFunction DrawIt)
{
   uint8_t Part;
   uint16_t i;
   uint16_t j;
   uint8_t Mask = 0x80;
   uint8_t Value = 0;
   uint8_t Pixel;

   screenTxStart(false);
   gPartY = 0;
   gDrawY = 0;
   for(Part = 0; Part < TOTAL_PART; Part++) {
      xMemSet(blockbuffer,0,BYTES_PER_PART * 2);
      for(i = 0; i < LINES_PER_PART; i++) {
         DrawIt();
         gDrawY++;
      }
      j = BYTES_PER_PART;
      for(i = 0; i < BYTES_PER_PART; i++) {
         while(Mask != 0) {
         // B/W bit
            if(blockbuffer[i] & Mask) {
               Pixel = PIXEL_BLACK;
            }
            else {
               Pixel = PIXEL_WHITE;
            }

         // red/yellow W bit
            if(blockbuffer[j] & Mask) {
               Pixel = PIXEL_RED_YELLOW;
            }
            Value <<= 4;
            Value |= Pixel;
            if(Mask & 0b10101010) {
            // Value ready, send it
               screenByteTx(Value);
            }
            Mask >>= 1; // next bit
         }
         Mask = 0x80;
         j++;
      }
      gPartY += LINES_PER_PART;
   }
// Finished with SPI flash
   drawWithSleep();
}



#pragma callee_saves myStrlen
static uint16_t myStrlen(const char *str)
{
   const char * __xdata strP = str;
   
   while (charsPrvDerefAndIncGenericPtr(&strP));
   
   return strP - str;
}

#if 0
void setPosXY(uint16_t x, uint16_t y) {
    shortCommand1(CMD_XSTART_POS, (uint8_t)(x / 8));
    commandBegin(CMD_YSTART_POS);
    epdSend((y) & 0xff);
    epdSend((y) >> 8);
    commandEnd();
}
#endif

// x,y where to put bmp.  (x must be a multiple of 8)
// bmp[0] =  bmp width in pixels (must be a multiple of 8)
// bmp[1] =  bmp height in pixels
// bmp[2...] = pixel data 1BBP
void loadRawBitmap(uint8_t *bmp,uint16_t x,uint16_t y,bool color) 
{
   uint8_t Width = bmp[0];

   LOGV("gDrawY %d\n",gDrawY);
   LOGV("ld bmp x %d, y %d, color %d\n",x,y,color);

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

void SetWinDrawNdx()
{
   LOGV("SetWinDrawNdx: gWinDrawY %d gWinY %d gDrawY %d\n",
        gWinDrawY,gWinY,gDrawY);
   gWinBufNdx = gWinDrawX >> 3;
   LOGV("1 %d\n",gWinBufNdx);
   gWinBufNdx += (gWinDrawY - gPartY) * BYTES_PER_LINE;
   LOGV("2 %d\n",gWinBufNdx);
   if(gWinColor) {
      gWinBufNdx += BYTES_PER_PART;
      LOGV("3 %d\n",gWinBufNdx);
   }
}

// Set window X position and width in pixels
bool setWindowX(uint16_t start,uint16_t width) 
{
   gWinX = start;
   gWinDrawX = start;
   gWinEndX = start + width;
   LOGV("gWinEndX %d\n",gWinEndX);
   SetWinDrawNdx();
   return false;
}

// Set window Y position and height in pixels
// return true if the window is outside of range we're drawing at the moment
bool setWindowY(uint16_t start,uint16_t height) 
{
   gWinEndY = start + height;
   if(gDrawY >= start && gDrawY < gWinEndY) {
      gWinY = start;
      gWinDrawY = gDrawY;
      return false;
   }
   LOGV("Outside of window, gDrawY %d start %d end %d\n",
        gDrawY,start,gWinEndY);
   return true;
}

// https://raw.githubusercontent.com/basti79/LCD-fonts/master/10x16_vertikal_MSB_1.h

// Writes a single character to the framebuffer
// Routine is specific to a 10w x 16h font, uc8159 controller
// 
// Note: 
//  The first bit on the left is the MSB of the second byte.  
//  The last bit on the rigth is the LSB of the first byte.  
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
void writeCharEPD(uint8_t c) 
{
   uint16_t InMask;
   uint16_t FontBits;
   uint8_t OutMask;

   OutMask = (0x80 >> (gCharX & 0x7));
   c -= 0x20;

   LOGV("gCharY %d gCharX %d OutMask 0x%x\n",gCharY,gCharX,OutMask);
   LOGV("gPartY %d gDrawX %d writeCharEPD c 0x%x\n",gDrawY,gDrawX,c);
   if(!gDirectionY) {
      if(gLargeFont) {
         InMask = 0x8000 >> ((gDrawY - gWinY) / 2);
      }
      else {
         InMask = 0x8000 >> (gDrawY - gWinY);
      }
      TempU16 = (gCharY - gWinY) * 2;

      LOGV("  InMask 0x%x blockbuffer 0x%x\n",InMask,blockbuffer[gWinBufNdx]);
      for(TempU8 = 0; TempU8 < gFontWidth; TempU8++) {
         FontBits = (font[c][TempU16 + 1] << 8) | font[c][TempU16];
         LOGV("  FontBits 0x%x\n",FontBits);
         if(gLargeFont) {
            if(TempU8 & 1) {
               TempU16 += 2;
            }
         }
         else {
            TempU16 += 2;
         }
         if(FontBits & InMask) {
            blockbuffer[gWinBufNdx] |= OutMask;
         }
         OutMask = OutMask >> 1;
         if(OutMask == 0) {
            LOGV("  Next out byte blockbuffer 0x%x\n",blockbuffer[gWinBufNdx]);
            gWinBufNdx++;
            OutMask = 0x80;
         }
      }
   }
   else {
      InMask = 0x1;
      TempU16 = (gDrawY - gWinY) * 2;
      FontBits = (font[c][TempU16 + 1] << 8) | font[c][TempU16];

      LOGV("blockbuffer 0x%x FontBits 0x%x\n",blockbuffer[gWinBufNdx],FontBits);
      while(InMask != 0) {
         LOGV("  OutMask 0x%x InMask 0x%x gWinBufNdx 0x%x\n",OutMask,InMask,gWinBufNdx);
         if(FontBits & InMask) {
            blockbuffer[gWinBufNdx] |= OutMask;
         }
         InMask = InMask << 1;
         OutMask = OutMask >> 1;
         if(OutMask == 0) {
            LOGV("  Next out byte blockbuffer 0x%x\n",blockbuffer[gWinBufNdx]);
            gWinBufNdx++;
            OutMask = 0x80;
         }
      }
      LOGV("blockbuffer 0x%x\n",blockbuffer[gWinBufNdx]);
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

#pragma callee_saves epdPutchar
static void epdPutchar(uint32_t data) __reentrant 
{
   if(gDirectionY) {
      gFontHeight = gLargeFont ? FONT_WIDTH * 2 : FONT_WIDTH;
   }
   else {
      gFontHeight = gLargeFont ? FONT_HEIGHT * 2 : FONT_HEIGHT;
   }

   if(!setWindowY(gCharY,gFontHeight)) {
      if(gDirectionY) {
         gFontWidth = gLargeFont ? FONT_HEIGHT * 2 : FONT_HEIGHT;
      }
      else {
         gFontWidth = gLargeFont ? FONT_WIDTH * 2 : FONT_WIDTH;
      }
      setWindowX(gCharX,gFontWidth);
      writeCharEPD(data >> 24);
   }

   if(gDirectionY) {
      TempU16 = gLargeFont ? FONT_HEIGHT * 2 : FONT_HEIGHT;
      gCharY += gFontHeight + 1;
   }
   else {
      TempU16 = gLargeFont ? FONT_WIDTH * 2 : FONT_WIDTH;
      gCharX += gFontWidth + 1;
   }
}

void epdpr(const char __code *fmt, ...) __reentrant 
{
    va_list vl;
    va_start(vl, fmt);
    LOGV("epdpr '%s'\n",fmt);
    prvPrintFormat(epdPutchar, 0, fmt, vl);
    va_end(vl);
}

