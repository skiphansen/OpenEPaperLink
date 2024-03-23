#include <stdbool.h>
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
#include "userinterface.h"
#include "settings.h"
#include "logging.h"

#define VERBOSE_DEBUGDRAWING
#ifdef VERBOSE_DEBUGDRAWING
   #define LOGV(format, ... ) pr(format,## __VA_ARGS__)
   #define LOG_HEXV(x,y) DumpHex(x,y)
#else
   #define LOGV(format, ... )
   #define LOG_HEXV(x,y)
#endif

// Line we are drawing currently 0 -> SCREEN_HEIGHT - 1
__xdata int16_t gDrawY;

__xdata int16_t gWinX;
__xdata int16_t gWinEndX;
__xdata int16_t gWinY;
__xdata int16_t gWinEndY;

__xdata int16_t gWinDrawX;
__xdata int16_t gWinDrawY;

__idata uint16_t gWinBufNdx;
__bit gWinColor;

// NB: 8051 data / code space saving KLUDGE!
// Use the locally in a routine but DO NOT call anything while you care
// about the value !!
__idata uint16_t TempU16;

bool setWindowX(uint16_t start,uint16_t width);
bool setWindowY(uint16_t start,uint16_t height);
bool SetWindowPixels(uint8_t Pixels);
void SetWinDrawNdx(void);

#if 1

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

   if(eih->dataType != DATATYPE_IMG_RAW_2BPP) {
      LOG("dataType 0x%x not supported\n",eih->dataType);
      return;
   }
   screenTxStart(false);
   gDrawY = 0;
   for(Part = 0; Part < TOTAL_PART; Part++) {
#if 1
   // Read 6 lines of b/w pixels
      eepromRead(Adr,blockbuffer,BYTES_PER_PART);
   // Read 6 lines of red/yellow pixels
      eepromRead(Adr+BYTES_PER_PLANE,&blockbuffer[BYTES_PER_PART],BYTES_PER_PART);
      Adr += BYTES_PER_PART;
#else
      xMemSet(blockbuffer,0,BYTES_PER_PART * 2);
#endif

      addOverlay();
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
      gDrawY += LINES_PER_PART;
   }
// Finished with SPI flash
   powerDown(INIT_EEPROM);

   screenTxEnd();
//    drawWithSleep();
   #undef eih
}
#else
// color bar test

#define IMAGE_BYTES_2BPP   ((SCREEN_HEIGHT * SCREEN_WIDTH) / 4)
void drawImageAtAddress(uint32_t addr) __reentrant 
{
   uint16_t x;
   uint16_t y;
   uint8_t z = 0;
   uint8_t data;
   uint8_t pixel = 0;

   DRAW_LOG("Sending image\n");
   screenTxStart(false);
   for(y = 0; y < SCREEN_HEIGHT; y++) {
      for(x = 0; x < SCREEN_WIDTH; x += 2) {
#if 0
         if(z++ >= 39) {
            z = 0;
            pixel++;
            if(pixel >= 8) {
               pixel = 0;
            }
         }
#else
         pixel = 6;
         if(y == SCREEN_HEIGHT / 2) {
            pixel = 0;
         }
         else {
            if(x == SCREEN_WIDTH / 2) {
               pixel = 7;
            }
         }
#endif
         data = (pixel << 4) | pixel;
         screenByteTx(data);
      }
   }
   DRAW_LOG("Done\n");
   screenTxEnd();
}
#endif


#pragma callee_saves myStrlen
static uint16_t myStrlen(const char *str)
{
   const char * __xdata strP = str;
   
   while (charsPrvDerefAndIncGenericPtr(&strP));
   
   return strP - str;
}

#if 0
void clearScreen()
{
   uint16_t i = IMAGE_BYTES_2BPP;
   screenTxStart(false);
   while(i-- != 0) {
      screenByteTx((PIXEL_WHITE << 4) | PIXEL_WHITE);
   }
   screenTxEnd();
}

void setPosXY(uint16_t x, uint16_t y) {
    shortCommand1(CMD_XSTART_POS, (uint8_t)(x / 8));
    commandBegin(CMD_YSTART_POS);
    epdSend((y) & 0xff);
    epdSend((y) >> 8);
    commandEnd();
}
#endif

#if 0
#if 1
#define setWindowX(start,end) (true);
#else
bool setWindowX(uint16_t start, uint16_t end) 
{
}
#endif

// return true if the window is outside of range we're drawing at the moment
bool setWindowY(uint16_t start, uint16_t end) 
{
   if(start > ) {
   }


}
#endif

// x,y where to put bmp.  (x must be a multiple of 8)
// bmp[0] =  bmp width in pixels (must be a multiple of 8)
// bmp[1] =  bmp height in pixels
// bmp[2...] = pixel data 1BBP
void loadRawBitmap(uint8_t *bmp,uint16_t x,uint16_t y,bool color) 
{
   uint16_t size;

   LOGV("gDrawY %d\n",gDrawY);
   LOGV("ld bmp x %d, y %d, color %d",x,y,color);
   LOGV(" 0x%x\n",bmp);

   if(setWindowY(y,bmp[1])) {
   // Nothing to do Y limit are outside of what we're drawing at the moment
      return;
   }
   gWinColor = color;
   setWindowX(x,bmp[0]);
   size = bmp[0] * bmp[1];

   TempU16 = gWinDrawY - gWinY;
   TempU16 = TempU16 * bmp[0];
   TempU16 = TempU16 >> 3;
   bmp += (TempU16 + 2);

   while(size--) {
      if(SetWindowPixels(*bmp++)) {
      // We're done
         break;
      }
   }
}

void SetWinDrawNdx()
{
   LOGV("SetWinDrawNdx: gWinDrawY %d gWinY %d gDrawY %d\n",
        gWinDrawY,gWinY,gDrawY);
   gWinBufNdx = gWinDrawX >> 3;
   LOGV("1 %d\n",gWinBufNdx);
   gWinBufNdx += (gWinDrawY - gDrawY) * BYTES_PER_LINE;
   LOGV("2 %d\n",gWinBufNdx);
   if(gWinColor) {
      gWinBufNdx += BYTES_PER_PART;
      LOGV("3 %d\n",gWinBufNdx);
   }
}

bool SetWindowPixels(uint8_t Pixels)
{
   blockbuffer[gWinBufNdx++] |= Pixels;
   gWinDrawX += 8;
   if(gWinDrawX >= gWinEndX) {
   // Next line
      gWinDrawX = gWinX;
      gWinDrawY++;
      if(gWinDrawY >= gWinEndY || (gWinDrawY - gDrawY) >= LINES_PER_PART) {
      // Stop, outside of window
         LOGV("SetWindowPixels stop gWinDrawY %d gDrawY %d\n",gWinDrawY,gDrawY);
         return true;
      }
      SetWinDrawNdx();
   }
   return false;
}

// Set window X position and width in pixels
bool setWindowX(uint16_t start,uint16_t width) 
{
#ifdef DEBUGDRAWING
   if((start & 0x7) != 0) {
      LOG("Invalid start 0x%x\n",start);
   }
   if((width & 0x7) != 0) {
      LOG("Invalid start 0x%x\n",width);
   }
#endif
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
   gWinEndY = gWinY + height;
   if(gDrawY >= start && gDrawY < gWinEndY) {
      gWinY = start;
      gWinDrawY = gDrawY;
      return false;
   }
   LOGV("Outside of window, gDrawY %d start %d end %d\n",
        gDrawY,start,gWinEndY);
   return true;
}


