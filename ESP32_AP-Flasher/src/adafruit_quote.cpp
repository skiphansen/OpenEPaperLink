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

#include "contentmanager.h"
#include "commstructs.h"
#include "makeimage.h"
#include "newproto.h"
#include "storage.h"
#include "language.h"
#include "settings.h"
#include "system.h"
#include "tag_db.h"
#include "util.h"
#include "web.h"
#include "adafruit_quote.h"

// get string length in pixels
// set text font prior to calling this
uint16_t AdaFruitQuote::getStringLength(const char *str, int strlength)
{
   String Temp = str;

   if(strlength != 0) {
      Temp = Temp.substring(0,strlength);
   }
   return pMeasure->GetStringWidth(Temp);
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

/* 
   lines: from x0,y0 to x1,y0
          from x1,y0 to x1,y1
          from x1,y1 to x2,y1
          from x2,y1 to x2,y0 (middle only)
          from x2,y2 to x3,y0 (middle only)
 
 
                 0,0   x1,y1   x2,y1 (maxX,0)
   top left:      |author+-------
                  +------+
                x0,y0    x1,y0
 
 
               x0,y0 (0,0)   x1,y0  x2,y0        x3,y0
   top middle:    -------------+author+---------------
                               +------*
                             x1,y1    x2,y1

 
               x0,y0 (0,0)   x1,y0  x2,y0
   top right:    --------------+author+
                               +------*
 
 
                 0,0  x0,0    maxX,0
   top right:     -----+author|
                       +------+
                    x0,y0    x1,y0

               x0,y0   x1,y0
                 +------*
bottom left:     |author+-------------------------
                      x1,y1                     x2,y1
 
                            x1,y1   x2,y1
                              +------*
bottom middle:   -------------+author+-------------
               x0,y0        x1,y0  x2,y0        x3,y0
 
                             x1,y1   x2,y1
                               +------*
bottom left:    ---------------+author+
               x0,y0        x1,y0  x2,y0
*/
#define AUTHOR_RIGHT_MARGIN   2  // to edge of display
#define AUTHOR_LEFT_MARGIN    2  // from accent  line
#define AUTHOR_TOP_MARGIN     2  // from accent  line
#define AUTHOR_BOTTOM_MARGIN  2  // from bottom edge of display
void AdaFruitQuote::printAuthor(const char *author)
{
   int32_t x0;
   int32_t y0;
   int32_t x1;
   int32_t y1;
   int32_t x2;
   int32_t x3 = 0;
   int32_t x_text;
   int32_t y_text;
   int AuthorWidth;
   bool bErr = false;
   String Location = AuthorLocation;

   SelectFont(afont);
   AuthorWidth = getStringLength(author);
   Location.toUpperCase();
   LOG("Author \"%s\" location \"%s\" AuthorWidth %d\n",
       author,AuthorLocation,AuthorWidth);

   if(Location == "TL") {
      y_text = 0;
      y0 = FontSize;
      y1 = 0;
      x_text = AUTHOR_LEFT_MARGIN;
      x1 = x_text + AuthorWidth + AUTHOR_RIGHT_MARGIN;
      x2 = AreaWidth - 1;
   }
   else if(Location == "TM") {
      y_text = 0;
      y0 = 0;
      y1 = FontSize;
      x_text = (AreaWidth - AuthorWidth) / 2;
      x1 = x_text - AUTHOR_LEFT_MARGIN;
      x2 = x_text + AuthorWidth + AUTHOR_RIGHT_MARGIN;
      x3 = AreaWidth - 1;
   }
   else if(Location == "TR") {
      y_text = 0;
      y0 = 0;
      y1 = FontSize;
      x_text = AreaWidth - AuthorWidth - 1 - AUTHOR_RIGHT_MARGIN;
      x1 = x_text - AUTHOR_LEFT_MARGIN;
      x2 = AreaWidth - 1;
   }
   else if(Location == "BL") {
      y_text = AreaHeight - 1 - FontSize;
      y0 = AreaHeight - 1 - FontSize - AUTHOR_TOP_MARGIN;
      y1 = AreaHeight - 1;
      x_text = AUTHOR_LEFT_MARGIN;
      x1 = x_text + AuthorWidth + AUTHOR_RIGHT_MARGIN;
      x2 = AreaWidth - 1;
   }
   else if(Location == "BM") {
      y_text = AreaHeight - 1 - FontSize;
      y0 = AreaHeight - 1;
      y1 = AreaHeight - 1 - FontSize - AUTHOR_TOP_MARGIN;
      x_text = (AreaWidth - AuthorWidth) / 2;
      x1 = x_text - AUTHOR_LEFT_MARGIN;
      x2 = x_text + AuthorWidth + AUTHOR_RIGHT_MARGIN;
      x3 = AreaWidth - 1;
   }
   else if(Location == "BR") {
      y_text = AreaHeight - 1 - FontSize;
      y0 = AreaHeight - 1;
      y1 = AreaHeight - 1 - FontSize - AUTHOR_TOP_MARGIN;
      x_text = AreaWidth - AuthorWidth - 1 - AUTHOR_RIGHT_MARGIN;
      x1 = x_text - AUTHOR_LEFT_MARGIN;
      x2 = AreaWidth - 1;
   }
   else {
      LOGA("Invalid author location '%s'\n",AuthorLocation);
      bErr = true;
   }

   if(!bErr) {
      x0 = OffsetX;
      x1 += OffsetX;
      x2 += OffsetX;
      y0 += OffsetY;
      y1 += OffsetY;
      x_text += OffsetX;
      y_text += OffsetY;

      if(Location[0] == 'T') {
         OffsetY += AuthorFontSize;
      }
      AreaHeight -= AuthorFontSize;

      LOG("line %d,%d -> %d,%d\n",x0,y0,x1,y0);
      spr.drawLine(x0,y0,x1,y0,TFT_BLACK);
      LOG("line %d,%d -> %d,%d\n",x1,y0,x1,y1);
      spr.drawLine(x1,y0,x1,y1,TFT_BLACK);
      LOG("line %d,%d -> %d,%d\n",x1,y1,x2,y1);
      spr.drawLine(x1,y1,x2,y1,TFT_BLACK);
      if(x3 > 0) {
         x3 += OffsetX;
         LOG("line %d,%d -> %d,%d\n",x2,y1,x2,y0);
         spr.drawLine(x2,y1,x2,y0,TFT_BLACK);
         LOG("line %d,%d -> %d,%d\n",x2,y0,x3,y0);
         spr.drawLine(x2,y0,x3,y0,TFT_BLACK);
      }

      LOG("printing author: \"%s\" @ %d,%d\n",author,x_text,y_text);
      drawString(spr,author,x_text,y_text,FontName,TL_DATUM,TFT_RED,FontSize);
   }
}

AdaFruitQuote::~AdaFruitQuote()
{
   if(WordBuf != NULL) {
      free(WordBuf);
   }

   if(Text != NULL) {
      free(Text);
   }
   if(pMeasure != NULL) {
      delete pMeasure;
   }
}

void AdaFruitQuote::SelectFont(JsonArray &Font)
{
   FontName = Font[0].as<const char *>();
   FontSize = Font[1].as<int>();

   if(pMeasure != NULL) {
      delete pMeasure;
   }
   pMeasure = new StringWidthMeasure(spr,FontName,FontSize);
}

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
   const char *Raw = 
// "[{\"text\":\"If you want to build a ship, don't drum up people to collect wood and don't assign them tasks and work, but rather teach them to long for the endless immensity of the sea\",\"author\":\"Antoine de Saint-Exupery\"}]";
// "[{\"text\":\"A strong spirit transcends rules\",\"author\":\"Prince\"}]";
// "[{\"text\":\"What we call the beginning is often the end. And to make an end is to make a beginning. The end is where we start from\",\"author\":\"T. S. Eliot\"}]";
// "[{\"text\":\"...the idea is to try to give all of the information to help others to judge the value of your contribution; not just the information that leads to judgment in one particular direction or another.\",\"author\":\"Richard Feynman\"}]";
      "[{\"text\":\"People who are really serious about software should make their own hardware\",\"author\":\"Alan Kay\"}]";

   deserializeJson(doc,Raw);
   JsonObject QuoteData = doc[0];
#endif

   do {
      OffsetX = Template["position"][0];
      OffsetY = Template["position"][1];
      AreaWidth = Template["position"][2];
      AreaHeight = Template["position"][3];
      LOG("Area %d x %d @ %d, %d\n",AreaWidth,AreaHeight,OffsetX,OffsetY);
      if(AreaHeight == 0 || AreaWidth == 0) {
         LOGA("Invalid position\n");
         break;
      }

      qfont = Template["qfont"];
      sfont = Template["sfont"];
      if(Template["border"] == 1) {
         spr.drawRect(OffsetX,OffsetY,AreaWidth,AreaHeight,TFT_BLACK);
      }

      if(Template.containsKey("afont")) {
         afont = Template["afont"];
         if(!afont || afont.size() != 3) {
            LOGA("afont is invalid\n");
            break;
         }
         AuthorFontSize = afont[1].as<int>();
         AuthorLocation = afont[2].as<const char *>();
      // Must call printAuthor first to adjust OffsetY and AreaHeight
         String Author = QuoteData["author"].as<String>();
         Text = strdup(Author.c_str());
         Unicode2Ascii(Text);
         printAuthor(Text);
         free(Text);
         Text = NULL;
      }
      String Quote = QuoteData["text"].as<String>();
      Quote = "\"" + Quote + "\"";
      Text = strdup(Quote.c_str());
      Unicode2Ascii(Text);

      LOG("Quote: %s\n",Text);
      printQuote(Text);
   } while(false);
}

