#include <stdint.h>
#include <stdarg.h>

#include "drawing.h"
#include "draw_common.h"
#include "screen.h"
#include "printf.h"
#include "settings.h"
#include "logging.h"

// #define VERBOSE_DEBUGDRAWING
#ifdef VERBOSE_DEBUGDRAWING
   #define LOGV(format, ... ) pr(format,## __VA_ARGS__)
   #define LOG_HEXV(x,y) DumpHex(x,y)
#else
   #define LOGV(format, ... )
   #define LOG_HEXV(x,y)
#endif

#pragma callee_saves CalcLineWidth
void CalcLineWidth(uint32_t data) __reentrant 
{
   uint8_t TempU8 = (uint8_t) (data >> 24);   // Character we are displaying

   if(TempU8 < 0x20) {
   // process all Escapes
      if(TempU8 != '\n') {
      // except new line which would set gCharX to gLeftMargin
         ProcessEscapes(TempU8);
      }
   }
   else {
      uint16_t TempU16 = gFontIndexTbl[TempU8 - 0x20];
      gCharWidth = TempU16 >> 12;
      if(gLargeFont) {
         gCharWidth = gCharWidth * 2;
      }
      gCharX += gCharWidth + 1;
   }
}

/* Supported special characters:

   \b: Toggle bold (large) characters
   \f: center line horizontally (must be first character of string)
   \n: The usual, next line
   \r: Tottle red / black font color
   \t: Set left margin
   \v: Clear left margin
*/
void epdpr(const char __code *fmt, ...) __reentrant 
{
    va_list vl;
    va_start(vl, fmt);

    SetFontSize();
    if(*fmt == '\f') {
    // Center line horizontally
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
      gCharHeight = gLargeFont ? FONT_WIDTH * 2 : FONT_WIDTH;
      gCharWidth = gLargeFont ? FONT_HEIGHT * 2 : FONT_HEIGHT;
   }
   else {
      gCharHeight = gLargeFont ? FONT_HEIGHT * 2 : FONT_HEIGHT;
      gCharWidth = gLargeFont ? FONT_WIDTH * 2 : FONT_WIDTH;
   }
}

void ProcessEscapes(uint8_t Char)
{
   switch(Char) {
      case '\b':  // toggle bold (large) characters
         gLargeFont = !gLargeFont;
         LOGV("\\b gLargeFont %d\n",gLargeFont);
         SetFontSize();
         break;

      case '\f':  // Center line horizontally
      // handled in epdpr()
         break;

      case '\n':
         SetFontSize();
         gCharY += gCharHeight;
         gCharX = gLeftMargin;
         LOGV("\\n gCharX %d gCharY %d\n",gCharX,gCharY);
         break;

      case '\r':  // Tottle red / black
         gWinColor = !gWinColor;
         LOGV("\\r gWinColor %d\n",gWinColor);
         break;

      case '\t':  //Set left margin
         gLeftMargin = gCharX;
         break;

      case '\v':  // Clear left margin
         gLeftMargin = 0;
         break;

      default:
         LOGE("Invalid char 0x%02x\n",Char);
         break;
   }
}

