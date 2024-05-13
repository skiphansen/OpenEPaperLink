#include <stdint.h>
#include <stdarg.h>

#include "drawing.h"
#include "draw_common.h"
#include "screen.h"
#include "printf.h"
#include "settings.h"
#include "logging.h"

#pragma callee_saves CalcLineWidth
void CalcLineWidth(uint32_t data) __reentrant 
{
   uint8_t TempU8 = (uint8_t) (data >> 24);   // Character we are displaying
   uint16_t TempU16 = gFontIndexTbl[TempU8 - 0x20];
   gCharWidth = TempU16 >> 12;
   if(gLargeFont) {
      gCharWidth = gCharWidth * 2;
   }
   gCharX += gCharWidth + 1;
}

void epdpr(const char __code *fmt, ...) __reentrant 
{
    va_list vl;
    va_start(vl, fmt);
    SetFontSize();
    if(gCenterLine) {
       gCharX = 0;
       prvPrintFormat(CalcLineWidth,0,fmt,vl);
       gCharX = (DISPLAY_WIDTH - gCharX) / 2;
    }
    prvPrintFormat(epdPutchar,0,fmt,vl);
    va_end(vl);
}

void SetFontSize()
{
   if(gDirectionY) {
      gFontHeight = gLargeFont ? FONT_WIDTH * 2 : FONT_WIDTH;
      gCharWidth = gLargeFont ? FONT_HEIGHT * 2 : FONT_HEIGHT;
   }
   else {
      gFontHeight = gLargeFont ? FONT_HEIGHT * 2 : FONT_HEIGHT;
      gCharWidth = gLargeFont ? FONT_WIDTH * 2 : FONT_WIDTH;
   }
}

void NextLine(uint8_t Lines)
{
   SetFontSize();
   gCharY += Lines * gFontHeight;
   gCharX = gLeftMargin;
}

