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

void AdaFruitQuote::BreakIntoLines(const char *str, int linesize)
{
   char *cp;
   char *pLastEnd;
   bool bEnd = false;
   uint16_t LineWidth;
   DisplayLine Line;

   Lines.clear();
   if(WordBuf != NULL) {
      free(WordBuf);
   }
   WordBuf = strdup(str);
   cp = WordBuf;
   Line.Line = cp;

   Line.LineWidth = 0;
   LOG("new string to wrap: '%s'\n",str);

   while(!bEnd) {
   // Skip spaces
      while(*cp == ' ') {
         cp++;
      }
   // Skip to the end of word
      while(true) {
         if(*cp == ' ') {
            *cp = 0;
            break;
         }
         if(!*cp) {
            bEnd = true;
            break;
         }
         cp++;
      }
      LineWidth = getStringLength(Line.Line);
      LOG("width '%s' = %d\n",Line.Line,LineWidth);

      if(LineWidth > linesize) {
      // Too wide
         if(Line.LineWidth == 0) {
         // first word on line is too long, truncate it
            Line.Line[linesize] = 0;
            cp++;
            Line.LineWidth = getStringLength(Line.Line);
         }
         else {
         // Last word doesn't fit, remove last word from line
            if(!bEnd) {
               *cp = ' ';
            }
            bEnd = false;
            cp = pLastEnd;
            *cp++ = 0;
         }
         Lines.push_back(Line);  // Add line
         LOG("Added line %d '%s'\n",Lines.size(),Line.Line);
         Line.Line = cp;
      }
      else {
      // Still fits, continue scanning
         Line.LineWidth = LineWidth;
         if(bEnd) {
            Lines.push_back(Line);  // Add line
            LOG("Added line %d '%s'\n",Lines.size(),Line.Line);
         }
         else {
            pLastEnd = cp;
            *cp++ = ' ';
         }
      }
   }
}

void AdaFruitQuote::printQuote(const char *quote)
{
   int lineheightquote;
   int Height;
   int linecount;
   int maxlines;

   bsmallfont = false;
// Try quote font first
   SelectFont(qfont);
   lineheightquote = FontSize;
   Height = AreaHeight - AuthorFontSize;

   LOG("Sprite width %d\n",AreaWidth);
   LOG("Sprite height %d\n",AreaHeight);
   LOG("line height quote %d\n",FontSize);
   LOG("line height author %d\n",AuthorFontSize);

   BreakIntoLines(quote,AreaWidth);

   maxlines = Height / lineheightquote;
   linecount = Lines.size();
   LOG("maxlines %d\n",maxlines);
   LOG("linecount %d\n",linecount);
   
   if(Lines.size() > maxlines) {
   // too long for default font size
   // next attempt, reduce lineheight to .8 size
      LOG("linecount too big, reduce lineheight\n");
      lineheightquote = .8 * lineheightquote;
      maxlines = Height / lineheightquote;
      if(linecount > maxlines) {
      // next attempt, use small font
         LOG("linecount still too big, try small font\n");
         bsmallfont = true;
         SelectFont(sfont);
         lineheightquote = FontSize;
         maxlines = Height / lineheightquote;
         BreakIntoLines(quote,AreaWidth);
         linecount = Lines.size();
         if(linecount > maxlines) {
         // final attempt, last resort is to reduce the lineheight to make it fit
            LOG("linecount still too big, lineheightquote %d",lineheightquote);
            lineheightquote = Height / linecount;
            LOG(" reduced to %d\n",lineheightquote);
         }
      }
      LOG("Final linecount %d\n",linecount);
      LOG("Final maxlines %d\n",maxlines);
      LOG("Final line height %d\n",lineheightquote);
   }

   int y = 0;
   if(linecount <= maxlines) {
      y = (Height - linecount*lineheightquote)/2;
   }
   y += OffsetY;

   int MaxWidth = 0;
   for(int i = 0; i < Lines.size(); i++) {
      if(MaxWidth < Lines[i].LineWidth) {
         MaxWidth = Lines[i].LineWidth;
      }
   }
// Center longest line from quote in sprite
   int x = (AreaWidth - MaxWidth) / 2;
   LOG("Quote indent %d\n",x);
   x += OffsetX;

   for(int i = 0; i < Lines.size(); i++) {
      LOG("printing line %d: '%s' @ %d, %d\n",i + 1,Lines[i].Line,x,y);
      drawString(spr,Lines[i].Line,x,y,FontName,TL_DATUM,TFT_BLACK,FontSize);
      y += lineheightquote;
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
   y0 = OffsetY + AreaHeight - 1 - (FontSize/2) - AUTHOR_BOTTOM_MARGIN;
   x1 = AreaWidth - 1 - x - AUTHOR_LEFT_MARGIN - AUTHOR_RIGHT_MARGIN;
   y1 = OffsetY + AreaHeight - 1 - FontSize - AUTHOR_BOTTOM_MARGIN 
        - AUTHOR_TOP_MARGIN;

// draw line from left edge of screen to middle of the author
   spr.drawLine(x0,y0,x1,y0,TFT_BLACK);
// draw line from middle of the author to top of author
   spr.drawLine(x1,y0,x1,y1,TFT_BLACK);
// draw line from top of the author to right edige of screen
   spr.drawLine(x1,y1,AreaWidth-1,y1,TFT_BLACK);
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

#if 0
void AdaFruitQuote::Draw() 
{
   uint16_t LineWidth;

   String QuoteUrl = "https://www.adafruit.com/api/quotes.php";
   DynamicJsonDocument doc(2000);

   LOG("AdaFruitQuote\n");
   LOG("spr %d X %d\n",spr.width(),spr.height());

#if 1
   const bool success = util::httpGetJson(QuoteUrl,doc,5000);
   if(!success) {
      LOG("httpGetJson failed\n");
      return;
   }
   JsonObject QuoteData = doc[0];
   LOG("Raw Json:\n");
   serializeJson(doc, Serial);
   LOG("\n");
   File f = contentFS->open("/quote.txt","a");
   serializeJson(doc,f);
   f.write((const uint8_t *)"\n",1);
   f.close();
#else
//   const char *Raw = "[{\"text\":\"If you want to build a ship, don't drum up people to collect wood and don't assign them tasks and work, but rather teach them to long for the endless immensity of the sea\",\"author\":\"Antoine de Saint-Exupery\"}]";
//  const char *Raw = "[{\"text\":\"A strong spirit transcends rules\",\"author\":\"Prince\"}]";
   const char *Raw = "[{\"text\":\"What we call the beginning is often the end. And to make an end is to make a beginning. The end is where we start from\",\"author\":\"T. S. Eliot\"}]";
   deserializeJson(doc,Raw);
   JsonObject QuoteData = doc[0];
#endif

   OffsetX = Template["position"][0];
   OffsetY = Template["position"][1];
   AreaWidth = Template["position"][2];
   AreaHeight = Template["position"][3];
   LOG("Area %d x %d @ %d, %d\n",AreaWidth,AreaHeight,OffsetX,OffsetY);

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
#else
extern bool bDumpFontHex;
void AdaFruitQuote::DumpFont()
{
   char Character = ' ';
   char Chars[33];
   int i = 0;
   
   LOG("Dumping %s\n",FontName);
   while(Character < 0x80) {
      Chars[0] = Character++;
      Chars[1] = 0;
      String Line = Chars;
      spr.fillRect(0,0,spr.width(),spr.height(),TFT_WHITE);
      drawString(spr,Line,10,spr.height()/2,FontName,TL_DATUM,TFT_BLACK,FontSize);
      if(i++ >= 16) {
         i = 0;
         vTaskDelay(100);
      }
   }
}

void AdaFruitQuote::Draw() 
{
   struct {
      const char *FontName;
      int FontSize;
   } FontList[] = {
      {"tahoma9.vlw",9},
      {"REFSAN12.vlw",12},
      {"calibrib16.vlw",16},
      {"twcondensed20.vlw",20},
      {"bahnschrift20.vlw",20},
      {"bahnschrift30.vlw",30},
//    {"bahnschrift70.vlw",70},
      {"calibrib30.vlw",30},
      {NULL}
   };
   bDumpFontHex = true;   

   for(int i = 0; FontList[i].FontName != NULL; i++) {
      FontName = FontList[i].FontName;
      FontSize = FontList[i].FontSize;
      DumpFont();
   }

   FontName = "bahnschrift20";
   FontSize = 20;

   String Line = "dp$/ \\ AWg{|}~";
   drawString(spr,Line,20,20,FontName,TL_DATUM,TFT_BLACK,FontSize);
   bDumpFontHex = false;   
}
#endif


