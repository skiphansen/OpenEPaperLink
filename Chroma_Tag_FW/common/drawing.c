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
#include "settings.h"
#include "logging.h"


#if 1
extern uint8_t __xdata blockbuffer[];


// Screen is 640 x 384 with 2 bits per pixel we need 61,440 (60K) bytes
// which of course we don't have.
// Read data was 60 chunks for 1024 bytes
//
#define BYTES_PER_PLANE    ((SCREEN_HEIGHT * SCREEN_WIDTH) / 8)
#define IMAGE_BYTES_2BPP   (BYTES_PER_PLANE * 2)

#define eih ((struct EepromImageHeader *__xdata) blockbuffer)
void drawImageAtAddress(uint32_t addr) __reentrant 
{
   uint32_t Adr = addr;
   uint8_t Part;
   uint16_t i;
   uint16_t j;
   uint8_t Mask = 0x80;
   uint8_t Value;
   uint8_t Pixel;

   DRAW_LOG("Reading header @ 0x%lx\n",Adr);
   eepromRead(Adr,blockbuffer,sizeof(struct EepromImageHeader));
   DRAW_LOG_HEX(blockbuffer,sizeof(struct EepromImageHeader));
   Adr += sizeof(struct EepromImageHeader);

   if(eih->dataType != DATATYPE_IMG_RAW_2BPP) {
      LOG("dataType 0x%x not supported\n",eih->dataType);
      return;
   }
   screenTxStart(false);
   for(Part = 0; Part < 60; Part++) {
   // Read 4096 (512 bytes) worth of b/w pixels
      DRAW_LOG("Reading @ 0x%lx 0x%lx\n",Adr,Adr+BYTES_PER_PLANE);
      eepromRead(Adr,blockbuffer,512);
      DRAW_LOG_HEX(blockbuffer,512);
   // Read 512 red/yellow pixels
      eepromRead(Adr+BYTES_PER_PLANE,&blockbuffer[512],512);
      DRAW_LOG("red/yellow:\n");
      DRAW_LOG_HEX(&blockbuffer[512],512);
      Adr += 512;
      j = 512;
      for(i = 0; i < 512; i++) {
#if 0
         DRAW_LOG("i 0x%x BYTES_PER_PLANE + i 0x%x bw 0x%x red 0x%x\n",
                  i,
                  BYTES_PER_PLANE + i,
                  blockbuffer[i],
                  blockbuffer[BYTES_PER_PLANE + i]);
#endif
         while(Mask != 0) {
         // B/W bit
            DRAW_LOG("mask 0x%x\n",Mask);
            if(blockbuffer[i] & Mask) {
               Pixel = PIXEL_BLACK;
            }
            else {
               Pixel = PIXEL_WHITE;
            }

         // red/yellow W bit
#if 1
            if(blockbuffer[j] & Mask) {
               Pixel = PIXEL_RED_YELLOW;
            }
#endif
            Value <<= 4;
            Value |= Pixel;
            if(Mask & 0b10101010) {
            // Value ready, send it
//             DRAW_LOG("send 0x%x\n",Value);
               screenByteTx(Value);
#if 0
               if(Value != 0) {
                  DRAW_LOG("b/w:\n");
                  DRAW_LOG_HEX(blockbuffer,512);
                  DRAW_LOG("red/yellow:\n");
                  DRAW_LOG_HEX(&blockbuffer[512],512);
                  break;
               }
#endif
            }
            Mask >>= 1; // next bit
         }
         Mask = 0x80;
         j++;
      }
   }
   screenTxEnd();
   addOverlay();
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
