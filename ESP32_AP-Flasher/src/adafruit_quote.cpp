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

#if 0
// get string length in pixels
// set text font prior to calling this
int AdaFruitQuote::getStringLength(const char *str, int strlength = 0)
{
   char buff[1024];
   int16_t x, y;
   uint16_t w, h;
   uint16_t StringWidth;

   if(strlength == 0) {
      strcpy(buff, str);
   }
   else {
      strncpy(buff, str, strlength);
      buff[strlength] = '\0';
   }
   StringWidth = drawString(spr,LocationS,gDrawX,gDrawY,date[2],TL_DATUM,TFT_BLACK,CharHeight);
   epd.getTextBounds(buff, 0, 0, &x, &y, &w, &h);
   return(w);  
}

// word wrap routine
// first time send string to wrap
// 2nd and additional times: use empty string
// returns substring of wrapped text.
char *AdaFruitQuote::wrapWord(const char *str, int linesize)
{
   int linestart = 0;
   if(strlen(str) == 0) {
   // additional line from original string
      linestart = lineend + 1;
      lineend = bufflen;
      LOG("existing string to wrap, starting at position %d: %s\n",
          linestart,&WordBuf[linestart]);
   }
   else {
      LOG("new string to wrap: %s\n",str);
      WordBuf = strdup(str);
      linestart = 0;
      lineend = strlen(buff);
      bufflen = lineend;
   }
   uint16_t w;
   int lastwordpos = linestart;
   int wordpos = linestart + 1;
   while(true) {
   // Skip spaces
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
         return &WordBuf[linestart];
      } 
      else if(wordpos >= bufflen) {
      // first word too long or end of string, send it anyway
         WordBuf[wordpos] = '\0';
         lineend = wordpos;
         return &WordBuf[linestart];        
      }
      lastwordpos = wordpos;
      wordpos++;
   }
}

// return # of lines created from word wrap
int AdaFruitQuote::getLineCount(const char *str, int scrwidth)
{
  int linecount = 0;
  String line = wrapWord(str,scrwidth);

  while(line.length() > 0) {
    linecount++;
    line = wrapWord("",scrwidth);
  }
  return linecount;  
}

int AdaFruitQuote::getLineHeight(const GFXfont *font = NULL)
{
  int height;
  if(font == NULL) {
    height = 12;
  }
  else {
    height = (uint8_t)pgm_read_byte(&font->yAdvance);
  }
  return height;
}

void AdaFruitQuote::printQuote(TFT_eSprite spr,String &quote)
{
   int x = 0;
   int y = 0;
   bool bsmallfont = false;
#if 0
   epd.setTextColor(EPD_BLACK);
   epd.setFont(qfont);
   epd.setTextSize(1);
#endif

   int scrwidth = imageParams.width - 8;
   LOG("Screen width %d\n",scrwidth);
   LOG("Screen height %d\n",imageParams.height);
   int linecount = getLineCount(quote.c_str(),scrwidth);
   int lineheightquote = getLineHeight(qfont);
   int lineheightauthor = getLineHeight(afont);
   int lineheightother = getLineHeight(ofont);
   int maxlines = (imageParams.height() - (lineheightauthor + lineheightother)) / lineheightquote;
   LOG("maxlines %d\n",maxlines);
   LOG("line height %d\n",lineheightquote);
   LOG("linecount %d\n",linecount);
   int topmargin = 0;
   if(linecount > maxlines) {
   // too long for default font size
   // next attempt, reduce lineheight to .8 size
      lineheightquote = .8 * lineheightquote;
      maxlines = (imageParams.height - (lineheightauthor + lineheightother)) / lineheightquote;
      if(linecount > maxlines) {
      // next attempt, use small font
         epd.setFont(smallfont);
         bsmallfont = true;
         epd.setTextSize(1);
         lineheightquote = getLineHeight(smallfont);
         maxlines = (imageParams.height- (lineheightauthor + lineheightother)) / lineheightquote;
         linecount = getLineCount(quote.c_str(),scrwidth);
         if(linecount > maxlines) {
            // final attempt, last resort is to reduce the lineheight to make it fit
            lineheightquote = (epd.imageParams - (lineheightauthor + lineheightother)) / linecount;
         }
      }
      LOG("maxlines has changed to %d\n",maxlines);
      LOG("line height has changed to %d\n",lineheightquote);
      LOG("linecount has changed to %d\n",linecount);
   }

   if(linecount <= maxlines) {
      topmargin = (imageParams.height - (lineheightauthor + lineheightother) - linecount*lineheightquote)/2;
      if(!bsmallfont)
         topmargin+=lineheightquote-4;
      //Serial.println("topmargin = " + String(topmargin));
   }
   String line = wrapWord(quote.c_str(),scrwidth);

   int counter = 0;
   epd.setTextColor(EPD_BLACK);
   while(line.length() > 0) {
      counter++;
      LOG("printing line %d: \"%s\"\n",counter,line.c_str);
      epd.setCursor(x +4, y + topmargin);
      epd.print(line);
      y += lineheightquote;
      line = wrapWord("",scrwidth);
   }
}

void AdaFruitQuote::printAuthor(String author)
{
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
}

void AdaFruitQuote::printOther(String other)
{
  epd.setFont(ofont);
  int lineheightother = getLineHeight(ofont);
  int ypos = epd.height() - 2;
  if (ofont == NULL)
    ypos = epd.height() - (lineheightother - 2);
  epd.setTextColor(EPD_BLACK);
  epd.setCursor(4,ypos);
  epd.print(other);
}
#else
void AdaFruitQuote::printQuote(TFT_eSprite spr,String &quote)
{
}

#endif

AdaFruitQuote::~AdaFruitQuote()
{
   if(WordBuf != NULL) {
      free(WordBuf);
   }
}

void AdaFruitQuote::Draw() 
{
   String QuoteUrl = "https://www.adafruit.com/api/quotes.php";
   DynamicJsonDocument doc(2000);

   LOG("AdaFruitQuote\n");

   const bool success = util::httpGetJson(QuoteUrl,doc,5000);
   if(!success) {
      LOG("httpGetJson failed\n");
      return;
   }
   JsonObject QuoteData = doc[0];

   String Quote = QuoteData["text"].as<String>();
   String Author = QuoteData["author"].as<String>();

   Quote = "\"" + Quote + "\"";

   LOG("Quote: %s\n",Quote.c_str());
   LOG("Author: \"%s\"\n",Author.c_str());

   const auto &qfont = Template["qfont"];
   const auto &sfont = Template["sfont"];
   const auto &afont = Template["afont"];
   const auto &ofont = Template["ofont"];


   LOG("qFont: %s %d\n",(const char *) qfont[0],(int) qfont[1]);
   LOG("sFont: %s %d\n",(const char *) sfont[0],(int) sfont[1]);
   LOG("aFont: %s %d\n",(const char *) afont[0],(int) afont[1]);
   LOG("oFont: %s %d\n",(const char *) ofont[0],(int) ofont[1]);

   DrawX = 0;
   DrawY = 0;
   drawString(spr,qfont[0],DrawX,DrawY,qfont[0],TL_DATUM,TFT_BLACK,qfont[1]);
   DrawY += (int) qfont[1];

   drawString(spr,sfont[0],DrawX,DrawY,sfont[0],TL_DATUM,TFT_BLACK,sfont[1]);
   DrawY += (int) sfont[1];

   drawString(spr,afont[0],DrawX,DrawY,afont[0],TL_DATUM,TFT_BLACK,afont[1]);
   DrawY += (int) afont[1];

   drawString(spr,ofont[0],DrawX,DrawY,ofont[0],TL_DATUM,TFT_BLACK,ofont[1]);

//    printQuote(Quote);
}


