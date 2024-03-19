#include <stdbool.h>
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
#include "settings.h"
#include "logging.h"


#if 0
void drawImageAtAddress(uint32_t addr)
{
   uint32_t __xdata clutAddr;
   uint8_t __xdata iter;
   
   clutAddr = drawPrvParseHeader(addr);
   if (!clutAddr)
      return;
   drawPrvLoadAndMapClut(clutAddr);
   
   screenTxStart(false);
   for (iter = 0; iter < SCREEN_DATA_PASSES; iter++) {
      
      drawPrvDecodeImageOnce();
      screenEndPass();
   }
   
   screenTxEnd();
   screenShutdown();
}


extern uint8_t __xdata blockbuffer[];
#define drawBuffer   blockbuffer

#define IMAGE_BYTES_2BPP   ((SCREEN_HEIGHT * SCREEN_WIDTH) / 4)
void drawImageAtAddress(uint32_t addr) __reentrant 
{
   static struct EepromImageHeader* __xdata eih;
   eih = (struct EepromImageHeader*)drawBuffer;
   uint32_t Adr = addr;
   eepromRead(Adr,drawBuffer,sizeof(struct EepromImageHeader));
   Adr += sizeof(struct EepromImageHeader);

   if(eih->dataType == DATATYPE_IMG_RAW_2BPP) {
      beginFullscreenImage();
      beginWriteFramebuffer(EPD_COLOR_BLACK);
      epdSelect();
      for(uint16_t c = 0; c < IMAGE_BYTES_2BPP; c++) {
         if(c % 512 == 0) {
            epdDeselect();
            eepromRead(Adr,drawBuffer,512);
            Adr += 512;
            epdSelect();
         }
         epdSend(drawBuffer[c & 0x1ff]);
         if(c == IMAGE_BYTES_2BPP / 2) {
         // time to switch to red pixels
            epdDeselect();
            endWriteFramebuffer();
            epdSelect();
            beginWriteFramebuffer(EPD_COLOR_RED);
            epdDeselect();
            epdSelect();
         }
      }
      epdDeselect();
      endWriteFramebuffer();
   }
   else {
      pr("DRAW: Image with type 0x%02X was requested, but we don't know what to do with that currently...\n", eih->dataType);
   }
   addOverlay();
   drawWithSleep();
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



