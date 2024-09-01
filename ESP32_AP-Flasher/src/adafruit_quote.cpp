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
      LOG("width %d '%s'\n",w,&WordBuf[linestart]);

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
// Try quote font first
   SelectFont(qfont);

   int scrwidth = spr.width();
   LOG("Screen width %d\n",scrwidth);
   LOG("Screen height %d\n",spr.height());
   int linecount = getLineCount(quote,scrwidth);
   int lineheightquote = FontSize;
   LOG("line height quote %d\n",lineheightquote);
   LOG("line height author %d\n",AuthorFontSize);
   int maxlines = (spr.height() - AuthorFontSize) / lineheightquote;
   LOG("maxlines %d\n",maxlines);
   LOG("linecount %d\n",linecount);
   int topmargin = 0;

   if(linecount > maxlines) {
   // too long for default font size
   // next attempt, reduce lineheight to .8 size
      LOG("linecount too big, reduce lineheight\n");
      lineheightquote = .8 * lineheightquote;
      maxlines = (spr.height() - AuthorFontSize) / lineheightquote;
      if(linecount > maxlines) {
      // next attempt, use small font
         LOG("linecount still too big, try small font\n");
         bsmallfont = true;
         SelectFont(sfont);

         lineheightquote = FontSize;
         maxlines = (spr.height() - AuthorFontSize) / lineheightquote;
         linecount = getLineCount(quote,scrwidth);
         if(linecount > maxlines) {
         // final attempt, last resort is to reduce the lineheight to make it fit
            lineheightquote = (spr.height() - AuthorFontSize) / linecount;
         }
      }
      LOG("maxlines changed to %d\n",maxlines);
      LOG("line height changed to %d\n",lineheightquote);
      LOG("linecount changed to %d\n",linecount);
   }

   if(linecount <= maxlines) {
      topmargin = (spr.height() - AuthorFontSize - linecount*lineheightquote)/2;
#if 0
      if(!bsmallfont) {
         topmargin+=lineheightquote-4;
      }
#endif
   }
   String line = wrapWord(quote,scrwidth);

   int counter = 0;
   y += topmargin;
   while(line.length() > 0) {
      counter++;
      LOG("printing line %d: \"%s\"\n",counter,line.c_str());
      drawString(spr,line,x,y,FontName,TL_DATUM,TFT_BLACK,FontSize);
      y += lineheightquote;
      line = wrapWord(NULL,scrwidth);
   }
}

#define AUTHOR_RIGHT_MARGIN   2  // to edge of display
#define AUTHOR_LEFT_MARGIN    2  // from accent  line
#define AUTHOR_TOP_MARGIN     2  // from accent  line
#define AUTHOR_BOTTOM_MARGIN  2  // from bottom edge of display

void AdaFruitQuote::printAuthor(const char *author)
{
   int32_t x0 = 0;
   int32_t y0;
   int32_t x1;
   int32_t y1;

   SelectFont(afont);
   int x = getStringLength(author);
   y0 = spr.height() - 1 - (FontSize/2) - AUTHOR_BOTTOM_MARGIN;
   x1 = spr.width() - 1 - x - AUTHOR_LEFT_MARGIN - AUTHOR_RIGHT_MARGIN;
   y1 = spr.height() - 1 - FontSize - AUTHOR_BOTTOM_MARGIN - AUTHOR_TOP_MARGIN;

// draw line from left edge of screen to middle of the author
   spr.drawLine(x0,y0,x1,y0,TFT_BLACK);
// draw line from middle of the author to top of author
   spr.drawLine(x1,y0,x1,y1,TFT_BLACK);
// draw line from top of the author to right edige of screen
   spr.drawLine(x1,y1,spr.width()-1,y1,TFT_BLACK);
   x1 += AUTHOR_LEFT_MARGIN;
   y1 += AUTHOR_TOP_MARGIN;

   LOG("author width %d x1 %ld y1 %ld\n",x,x1,y1);

   LOG("printing author: \"%s\"\n",author);
   drawString(spr,author,x1,y1,FontName,TL_DATUM,TFT_RED,FontSize);
}

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
   LOG("spr %d X %d\n",spr.width(),spr.height());

#if 0
   const bool success = util::httpGetJson(QuoteUrl,doc,5000);
   if(!success) {
      LOG("httpGetJson failed\n");
      return;
   }
   JsonObject QuoteData = doc[0];
   LOG("Raw Json:\n");
   serializeJson(doc, Serial);
   LOG("\n");
#else
   const char *Raw = "[{\"text\":\"If you want to build a ship, don't drum up people to collect wood and don't assign them tasks and work, but rather teach them to long for the endless immensity of the sea\",\"author\":\"Antoine de Saint-Exupery\"}]";
   deserializeJson(doc,Raw);
   JsonObject QuoteData = doc[0];
#endif

   String Quote = QuoteData["text"].as<String>();
   String Author = QuoteData["author"].as<String>();

   Quote = "\"" + Quote + "\"";

   Text = strdup(Quote.c_str());
   Unicode2Ascii(Text);

   LOG("Quote: %s\n",Text);
   LOG("Author: \"%s\"\n",Author.c_str());

   qfont = Template["qfont"];
   sfont = Template["sfont"];
   if(Template.containsKey("afont")) {
      afont = Template["afont"];
      AuthorFontName = afont[0].as<const char *>();
      AuthorFontSize = afont[1].as<int>();
   }
   else {
      AuthorFontSize = 0;
   }

   printQuote(Text);
   free(Text);

   if(AuthorFontSize > 0) {
      Text = strdup(Author.c_str());
      Unicode2Ascii(Text);
      printAuthor(Text);
   }
}

