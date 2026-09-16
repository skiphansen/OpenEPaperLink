#ifndef WITHOUT_PNG
#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <FS.h>
#include <vector>
#include <PNGdec.h>

#include "tag_db.h"
#include "makeimage.h"
#include "TFT_eSPI.h"
#include "contentmanager.h"
#include "web.h"
#include "storage.h"
#include "DrawPNG.h"

#define ENABLE_LOGGING  1
#if ENABLE_LOGGING && __has_include("logging.h") 
#include "logging.h"
#else
#define LOG(format, ...)
#define LOG_RAW(format, ...)
#endif

File DrawPNG::PngFile;

void *DrawPNG::PngOpen(const char *filename, int32_t *size) 
{
   DrawPNG::PngFile = contentFS->open(filename, "r");
    if (!DrawPNG::PngFile) return NULL;
    *size = DrawPNG::PngFile.size();
    return &DrawPNG::PngFile;
}

void DrawPNG::PngClose(void *handle) 
{
   DrawPNG::PngFile.close();
}

int32_t DrawPNG::PngRead(PNGFILE *handle, uint8_t *buffer, int32_t length) 
{
    if (!DrawPNG::PngFile) return 0;
    return DrawPNG::PngFile.read(buffer, length);
}

int32_t DrawPNG::PngSeek(PNGFILE *handle, int32_t position) {
    if (!DrawPNG::PngFile) return 0;
    return DrawPNG::PngFile.seek(position);
}

int DrawPNG::DrawCB(PNGDRAW *pDraw) 
{
   int y = pDraw->y + Yoffset;
   int iWidth = pDraw->iWidth;

   uint16_t usPixels[iWidth]; 
    
    // Convert line data to RGB565
    if(pDraw->y == 0) {
       LOG("y %d w %d iPitch %d iPixelType %d bpp %d\n",
           pDraw->y,iWidth,pDraw->iPitch,pDraw->iPixelType,pDraw->iBpp);
       png.getLineAsRGB565(pDraw,usPixels,PNG_RGB565_LITTLE_ENDIAN,0xffffffff);
       LOG("After getLineAsRGB565\n");
       Color color;
       uint16_t iColor;
       for(int i = 0; i < iWidth;i++) {
#if 0
          color = Color(usPixels[i]);
          if((i % 8) == 0) {
             LOG_RAW("\n%d: ",i);
          //   DUMP_HEX(&usPixels[i],16);
          }
          LOG_RAW("%d:%d:%d, ",color.r,color.g,color.b);
#else
          iColor = usPixels[i];
          if((i % 8) == 0) {
             LOG_RAW("\n%d: ",i);
          //   DUMP_HEX(&usPixels[i],16);
          }
          LOG_RAW("%d, ",iColor);
#endif
       }
       LOG("\n");
    }
    else {
       png.getLineAsRGB565(pDraw,usPixels,PNG_RGB565_LITTLE_ENDIAN,0xffffffff);
    }


    for(int i = 0; i < iWidth; i++) {
       pSpr->drawPixel(i + Xoffset,y,usPixels[i]);
    }

#if 0
    if(pDraw->y == 0) {
       LOG_RAW("Readback\n");
       Color color;
       uint16_t iColor;
       for(int i = 0; i < pDraw->iWidth;i++) {
#if 1
          color = pSpr->readPixel(i + Xoffset,y);
          if((i % 8) == 0) {
             LOG_RAW("\n%d: ",i);
          //   DUMP_HEX(&usPixels[i],16);
          }
          LOG_RAW("%d:%d:%d, ",color.r,color.g,color.b);
#else
          iColor = pSpr->readPixel(i + Xoffset,y);
          if((i % 8) == 0) {
             LOG_RAW("\n%d: ",i);
          //   DUMP_HEX(&usPixels[i],16);
          }
          LOG_RAW("%d, ",iColor);
#endif
       }
       LOG_RAW("\n");
    }
#endif
    
    return 1;
}

int DrawPNG::pngDrawCallback(PNGDRAW *pDraw) 
{
   DrawPNG *p = static_cast<DrawPNG *>(pDraw->pUser);

   return p->DrawCB(pDraw);
}

bool DrawPNG::DrawPng(String Filename,TFT_eSprite &spr, const tagRecord *taginfo, imgParam &imageParams)
{
   bool Ret = false; // Assume the worse
   LOG("\n");
   int ErrLine = 0;
   int err;
   bool bPngOpened = false;

   do {
      pSpr = &spr;
      err = png.open(Filename.c_str(),PngOpen,PngClose,PngRead,PngSeek,
                     DrawPNG::pngDrawCallback);
      if(err != PNG_SUCCESS) {
         ErrLine = __LINE__;
         break;
      }
      bPngOpened = true;

      int PngWidth = png.getWidth();
      int PngHeight = png.getHeight();
      int SprWidth = pSpr->width();
      int SprHeight = pSpr->height();

      LOG("%s: %dx%d, %d bpp, color type: %d\n",Filename.c_str(),
          PngWidth,PngHeight,png.getBpp(),png.getPixelType());

      if(PngWidth <= SprWidth && PngHeight <= SprHeight) {
      // No scaling required
         LOG("No scaling required\n");
      }
      else {
         LOG("Scaling needed, ignored\n");
         break;
      }
      Xoffset = (SprWidth - PngWidth) / 2;
      Yoffset = (SprHeight - PngHeight) / 2;
   // Decode PNG file into SPR
      err = png.decode(this,0);
      if(err != PNG_SUCCESS) {
         ErrLine = __LINE__;
         break;
      }
      Ret = true;
   } while(false);

   if(bPngOpened) {
      png.close();
   }

   if(ErrLine != 0) {
      LOG_RAW("%s: Error %d on line %d\n",__FUNCTION__,err,ErrLine);
   }

   return Ret;
}


#endif // WITHOUT_PNG


