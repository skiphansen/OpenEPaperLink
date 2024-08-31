/***************************************************
   This code is heavily based on adafruit_feather_quote.ino
   from https://github.com/adafruit/Adafruit_Learning_System_Guides.git
 
   Ported to OEPL by Skip Hansen 2024
 
   Original file header:
*/

// SPDX-FileCopyrightText: 2019 Dan Cogliano for Adafruit Industries
//
// SPDX-License-Identifier: MIT

/***************************************************
 * Quote Display for Adafruit ePaper FeatherWings
 * For use with Adafruit tricolor and monochrome ePaper FeatherWings
 * 
 * Adafruit invests time and resources providing this open source code.
 * Please support Adafruit and open source hardware by purchasing
 * products from Adafruit!
 * 
 * Written by Dan Cogliano for Adafruit Industries
 * Copyright (c) 2019 Adafruit Industries
 * 
 * Notes: 
 * Update the secrets.h file with your WiFi details
 * Uncomment the ePaper display type you are using below.
 * Change the SLEEP setting to define the time between quotes
 */

#include "contentmanager.h"
#include "adafruit_quote.h"

#define LOGA(format, ... ) Serial.printf(format,## __VA_ARGS__)

#if 1
#define LOG(format, ... ) Serial.printf(format,## __VA_ARGS__)
#else
#define LOG(format, ... )
#endif

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <MD5Builder.h>
#include <locale.h>
#include <time.h>

#include <map>

#include "commstructs.h"
#include "makeimage.h"
#include "newproto.h"
#include "storage.h"
#include "language.h"
#include "settings.h"
#include "system.h"
#include "tag_db.h"
#include "truetype.h"
#include "util.h"
#include "web.h"

#if 1
// get string length in pixels
// set text font prior to calling this
uint16_t AdaFruitQuote::getStringLength(const char *str, int strlength)
{
   String Temp = str;

   if(strlength != 0) {
      Temp = Temp.substring(0,strlength);
   }
   return GetStringWidth(spr,Temp,FontName,FontSize);
}

// Convert Unicode characters to ASCII
void AdaFruitQuote::Unicode2Ascii(char *str)
{
   char *pIn = str;
   char *pOut = str;
   char Char;
   char Ascii;
   uint32_t Ucode;
   int Bytes = 0;

   while((Char = *pIn++)) {
      if(Char < 0x80) {
      // Plain old ASCII
         *pOut++ = Char;
      }
      else if(Char < 0xc0) {
         Ucode = Char;
         Bytes = 1;
      }
      else if(Char <= 0xe0) {
      // 2 byte sequence
         Bytes = 2;
         Ucode = ((Char & 0xf) << 12) | ((*pIn++ & 0x3f) << 6) | (*pIn++ & 0x3f);
      }
      else if(Char <= 0xf0) {
      // 3 byte sequence
         Bytes = 3;
         Ucode = ((Char & 0xf) << 12) | ((*pIn++ & 0x3f) << 6) | (*pIn++ & 0x3f);
      }
      else {
         Bytes = 3;
         Ucode = ((Char & 0x7) << 18) | ((*pIn++ & 0x3f) << 12) | 
            ((*pIn++ & 0x3f) << 6) | (*pIn++ & 0x3f);
      }

      if(Bytes > 0) {
         Ascii = 0;
         if(Ucode >= 0x20 && Ucode < 0x7f) {
         // Plain old ASCII
            Ascii = (char) Ucode;
         }
         else switch(Ucode) {
            case 0x2018: // Single left quote
            case 0x2019: // Single right quote
               Ascii = '\'';
               break;

            case 0x201c:   // left double quote
            case 0x201d:   // right double quote
               Ascii = '\"';
               break;

            default:
               break;
         }

         LOG("%d byte Unicode U+%04lx -> ",Bytes,Ucode);
         if(Ascii != 0) {
            LOG("'%c'\n",Ascii);
            *pOut++ = Ascii;
         }
         else {
            LOG("ignored\n");
         }
         Bytes = 0;
      }
   }
   *pOut = 0;
}

// word wrap routine
// first time send string to wrap
// 2nd and additional times: use empty string
// returns substring of wrapped text.
char *AdaFruitQuote::wrapWord(const char *str, int linesize)
{
   int linestart = 0;
   char *Ret;

   if(str == NULL) {
   // additional line from original string
      linestart = lineend + 1;
      lineend = bufflen;
   }
   else {
      if(WordBuf != NULL) {
         free(WordBuf);
      }
      WordBuf = strdup(str);
      linestart = 0;
      lineend = strlen(WordBuf);
      bufflen = lineend;
      LOG("new string to wrap: %s bufflen %d\n",str,bufflen);
   }
   uint16_t w;
   int lastwordpos = linestart;
   int wordpos = linestart + 1;
   if(str == NULL && linestart < bufflen) {
      LOG("existing string to wrap, starting at position %d: %s\n",
          linestart,&WordBuf[linestart]);
   }
   while(true) {
   // Skip spaces
      if(linestart > bufflen) {
      // End of line
         linestart = bufflen;
         break;
      }
      while(WordBuf[wordpos] == ' ' && wordpos < bufflen) {
         wordpos++;
      }
   // Skip to end of word
      while(WordBuf[wordpos] != ' ' && wordpos < bufflen) {
         wordpos++;
      }
      if(wordpos < bufflen) {
         WordBuf[wordpos] = '\0';
      }
      w = getStringLength(&WordBuf[linestart]);
      if(wordpos < bufflen) {
         WordBuf[wordpos] = ' ';
      }
      if(w > linesize) {
         WordBuf[lastwordpos] = '\0';
         lineend = lastwordpos;
         break;
      } 
      else if(wordpos >= bufflen) {
      // first word too long or end of string, send it anyway
         WordBuf[wordpos] = '\0';
         lineend = wordpos;
         break;
      }
      lastwordpos = wordpos;
      wordpos++;
   }
   Ret = &WordBuf[linestart];
   LOG("Returning '%s'\n",Ret);
   return Ret;
}

// return # of lines created from word wrap
int AdaFruitQuote::getLineCount(const char *str, int scrwidth)
{
  int linecount = 0;
  String line = wrapWord(str,scrwidth);

  while(line.length() > 0) {
    linecount++;
    line = wrapWord(NULL,scrwidth);
  }
  return linecount;  
}

void AdaFruitQuote::printQuote(const char *quote)
{
   int x = 0;
   int y = 0;
   bsmallfont = false;
#if 0
   epd.setTextColor(EPD_BLACK);
   epd.setFont(qfont);
   epd.setTextSize(1);
#endif
// Try quote font first
   SelectFont(qfont);

   int scrwidth = spr.width();
   LOG("Screen width %d\n",scrwidth);
   LOG("Screen height %d\n",spr.height());
   int linecount = getLineCount(quote,scrwidth);
   int lineheightquote = qfont[1].as<int>();
   int lineheightauthor = afont[1].as<int>();
   int lineheightother = ofont[1].as<int>();
   int maxlines = (spr.height() - (lineheightauthor + lineheightother)) / lineheightquote;
   LOG("maxlines %d\n",maxlines);
   LOG("line height %d\n",lineheightquote);
   LOG("linecount %d\n",linecount);
   int topmargin = 0;

   if(linecount > maxlines) {
   // too long for default font size
   // next attempt, reduce lineheight to .8 size
      lineheightquote = .8 * lineheightquote;
      maxlines = (spr.height() - (lineheightauthor + lineheightother)) / lineheightquote;
      if(linecount > maxlines) {
      // next attempt, use small font
         bsmallfont = true;
         SelectFont(sfont);

         lineheightquote = FontSize;
         maxlines = (spr.height() - (lineheightauthor + lineheightother)) / lineheightquote;
         linecount = getLineCount(quote,scrwidth);
         if(linecount > maxlines) {
         // final attempt, last resort is to reduce the lineheight to make it fit
            lineheightquote = (spr.height() - (lineheightauthor + lineheightother)) / linecount;
         }
      }
      LOG("maxlines has changed to %d\n",maxlines);
      LOG("line height has changed to %d\n",lineheightquote);
      LOG("linecount has changed to %d\n",linecount);
   }

   if(linecount <= maxlines) {
      topmargin = (spr.height() - (lineheightauthor + lineheightother) - linecount*lineheightquote)/2;
      if(!bsmallfont)
         topmargin+=lineheightquote-4;
      //Serial.println("topmargin = " + String(topmargin));
   }
   String line = wrapWord(quote,scrwidth);

   int counter = 0;
   y += topmargin;
   x += 4;
   while(line.length() > 0) {
      counter++;
      LOG("printing line %d: \"%s\"\n",counter,line.c_str());
      drawString(spr,line,x,y,FontName,TL_DATUM,TFT_BLACK,FontSize);
      y += lineheightquote;
      line = wrapWord(NULL,scrwidth);
   }
}

void AdaFruitQuote::printAuthor(String author)
{
#if 0
  epd.setTextColor(EPD_BLACK);
  epd.setFont(afont);
  int lineheightauthor = getLineHeight(afont);
  int lineheightother = getLineHeight(ofont);
  int x = getStringLength(author.c_str());
  // draw line above author
  epd.drawLine(epd.width() - x - 10, epd.height() - (lineheightauthor + lineheightother) + 2, epd.width(), epd.height() - (lineheightauthor + lineheightother) + 2, EPD_RED);
  epd.drawLine(epd.width() - x - 10, epd.height() - (lineheightauthor + lineheightother) + 2, epd.width() - x - 10,epd.height() - lineheightother - lineheightauthor/3, EPD_RED);
  epd.drawLine(0, epd.height() - lineheightother - lineheightauthor/3, epd.width() - x - 10,epd.height() - lineheightother - lineheightauthor/3, EPD_RED);
  // draw author text
  int cursorx = epd.width() - x - 4;
  int cursory = epd.height() - lineheightother - 2;
  if(afont == NULL)
  {
    cursory = epd.height() - lineheightother - lineheightauthor - 2 ;
  }
  epd.setCursor(cursorx, cursory);
  epd.print(author);
#endif
}

void AdaFruitQuote::printOther(String other)
{
#if 0
  epd.setFont(ofont);
  int lineheightother = getLineHeight(ofont);
  int ypos = epd.height() - 2;
  if (ofont == NULL)
    ypos = epd.height() - (lineheightother - 2);
  epd.setTextColor(EPD_BLACK);
  epd.setCursor(4,ypos);
  epd.print(other);
#endif
}
#endif

AdaFruitQuote::~AdaFruitQuote()
{
   if(WordBuf != NULL) {
      free(WordBuf);
   }

   if(Text != NULL) {
      free(Text);
   }
}

void AdaFruitQuote::SelectFont(JsonArray &Font)
{
   FontName = Font[0].as<const char *>();
   FontSize = Font[1].as<int>();

}

void AdaFruitQuote::Draw() 
{
   uint16_t LineWidth;

   String QuoteUrl = "https://www.adafruit.com/api/quotes.php";
   DynamicJsonDocument doc(2000);

   LOG("AdaFruitQuote\n");

   const bool success = util::httpGetJson(QuoteUrl,doc,5000);
   if(!success) {
      LOG("httpGetJson failed\n");
      return;
   }
   JsonObject QuoteData = doc[0];
   LOG("Raw Json:\n");
   serializeJson(doc, Serial);
   LOG("\n");

   String Quote = QuoteData["text"].as<String>();
   String Author = QuoteData["author"].as<String>();

   Quote = "\"" + Quote + "\"";

   Text = strdup(Quote.c_str());
   Unicode2Ascii(Text);

   LOG("Quote: %s\n",Text);
   LOG("Author: \"%s\"\n",Author.c_str());

   qfont = Template["qfont"];
   sfont = Template["sfont"];
   afont = Template["afont"];
   ofont = Template["ofont"];

   printQuote(Text);
}


