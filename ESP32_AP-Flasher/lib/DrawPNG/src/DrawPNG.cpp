#ifndef WITHOUT_PNG
#include <Arduino.h>
#include <vector>
#include <PNGdec.h>

#include "TFT_eSPI.h"

#include "DrawPNG.h"

#define ENABLE_LOGGING  1
#if ENABLE_LOGGING && __has_include("logging.h") 
#include "logging.h"
#else
#define LOG(format, ...)
#define LOG_RAW(format, ...)
#endif

struct Color {
    uint8_t r, g, b;
    Color() : r(0), g(0), b(0) {}
    Color(uint16_t value_) : 
          r(((value_ >> 8) & 0xF8) | ((value_ >> 13) & 0x07)),
          g(((value_ >> 3) & 0xFC) | ((value_ >> 9) & 0x03)),
          b(((value_ << 3) & 0xF8) | ((value_ >> 2) & 0x07)) {}
    Color(uint8_t r_, uint8_t g_, uint8_t b_) : r(r_), g(g_), b(b_) {}
};

DrawPNG::DrawPNG(PngFileCBs_t *PngFileCBs) : pCBs(PngFileCBs)
{
   XsprOffset = 0;
   YsprOffset = 0;
   Options = 0;
}


int DrawPNG::DrawCB(PNGDRAW *pDraw) 
{
   int y = pDraw->y;
   int iWidth = pDraw->iWidth;
   int Xoff;
   int Yoff;
   uint16_t usPixels[iWidth]; 

   if(bRotate) {
      Xoff = Yoffset + YsprOffset;
      Yoff = Xoffset + XsprOffset;
   }
   else {
      Xoff = Xoffset + XsprOffset;
      Yoff = Yoffset + YsprOffset;
   }
   // Convert line data to RGB565
   png.getLineAsRGB565(pDraw,usPixels,PNG_RGB565_LITTLE_ENDIAN,0xffffffff);

#if 0
   if (pDraw->y == 0) {
      LOG("y %d w %d iPitch %d iPixelType %d bpp %d\n",
          pDraw->y,iWidth,pDraw->iPitch,pDraw->iPixelType,pDraw->iBpp);
      LOG("After getLineAsRGB565\n");
      Color color;
      uint16_t iColor;
      for (int i = 0; i < iWidth;i++) {
         iColor = usPixels[i];
         if ((i % 8) == 0) {
            LOG_RAW("\n%d: ",i);
            //   DUMP_HEX(&usPixels[i],16);
         }
         LOG_RAW("%d, ",iColor);
      }
      LOG("\n");
   }
#endif

   if(!bRotate) {
      if (ScalingFactor == SCALE_1_TO_1) {
         y += Yoff;
         for (int i = 0; i < iWidth; i++) {
            pSpr->drawPixel(i + Xoff,y,usPixels[i]);
         }
      }
      else {
         y = (y * ScalingFactor) / SCALE_1_TO_1;
         y += Yoff;
         unsigned int x;
         for (int i = 0; i < iWidth; i++) {
            x = (i * ScalingFactor) / SCALE_1_TO_1;
            pSpr->drawPixel(x + Xoff,y,usPixels[i]);
         }
      }
   }
   else {
   // Rotate image
      if (ScalingFactor == SCALE_1_TO_1) {
         y += Yoff;
         for (int i = 0; i < iWidth; i++) {
            pSpr->drawPixel(y,i + Xoff,usPixels[iWidth - 1 - i]);
         }
      }
      else {
            y = (y * ScalingFactor) / SCALE_1_TO_1;
            y += Yoff;
            unsigned int x;
            for (int i = 0; i < iWidth; i++) {
               x = (i * ScalingFactor) / SCALE_1_TO_1;
               pSpr->drawPixel(y,x + Xoff,usPixels[iWidth - 1 - i]);
            }
      }
   }

#if 0
   if (pDraw->y == 0) {
      LOG_RAW("Readback\n");
      Color color;
      uint16_t iColor;
      for (int i = 0; i < pDraw->iWidth;i++) {
#if 1
         color = pSpr->readPixel(i + Xoffset,y);
         if ((i % 8) == 0) {
            LOG_RAW("\n%d: ",i);
            //   DUMP_HEX(&usPixels[i],16);
         }
         LOG_RAW("%d:%d:%d, ",color.r,color.g,color.b);
#else
         iColor = pSpr->readPixel(i + Xoffset,y);
         if ((i % 8) == 0) {
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

int DrawPNG::DrawPng(String Filename,TFT_eSprite &spr)
{
   int Ret = -1; // Assume the worse
   int ErrLine = 0;
   int err;
   bool bPngOpened = false;

   do {
      pSpr = &spr;
      err = png.open(Filename.c_str(),pCBs->pfnOpen,pCBs->pfnClose,
                     pCBs->pfnRead,pCBs->pfnSeek,DrawPNG::pngDrawCallback);
      if(err != PNG_SUCCESS) {
         ErrLine = __LINE__;
         break;
      }
      bPngOpened = true;

      PngWidth = png.getWidth();
      PngHeight = png.getHeight();
      int TempWidth = PngWidth;
      int TempHeight = PngHeight;
      int SprWidth = pSpr->width();
      int SprHeight = pSpr->height();

      LOG("%s: %dx%d, %d bpp, color type: %d\n",Filename.c_str(),
          PngWidth,PngHeight,png.getBpp(),png.getPixelType());

      bRotate = false;
      if(PngHeight > PngWidth) {
         switch(Options & ROTATE_MODE_MASK) {
            case ROTATE_MODE_OFF:   // never rotate
               break;

            case ROTATE_MODE_ON:    // always roate
               LOG("Rotating image\n");
               bRotate = true;
               break;

            case ROTATE_MODE_FIT:   // only rotate when scaling 
               if(PngWidth > SprWidth || PngHeight > SprHeight) {
               // scaling required
                  LOG("Rotating image, won't fit without scaling otherwise\n");
                  bRotate = true;
               }
               break;

            default:
               ErrLine = __LINE__;
               break;
         }
      }

      if(ErrLine != 0) {
         break;
      }

      if(bRotate) {
      // swap TempHeight & TempWidth for scaling and 
      // centering calculations
         TempWidth = PngHeight;
         TempHeight = PngWidth;
      }

      int NewPngWidth;
      int NewPngHeight;
      if(TempWidth <= SprWidth && TempHeight <= SprHeight) {
      // No scaling required
         LOG("No scaling required\n");
         ScalingFactor = SCALE_1_TO_1;
         NewPngWidth = TempWidth;
         NewPngHeight = TempHeight;
      }
      else {
         LOG("Scaling needed, png %dx%d, spr %dx%d\n",
             PngWidth,PngHeight,SprWidth,SprHeight);
         unsigned int xScale = (SCALE_1_TO_1 * SprWidth) / TempWidth;
         unsigned int yScale = (SCALE_1_TO_1 * SprHeight) / TempHeight;
         ScalingFactor = xScale < yScale ? xScale : yScale;
         LOG("xScale %u yScale %u ScalingFactor %u\n",xScale,yScale,ScalingFactor);
         NewPngWidth = (( TempWidth * ScalingFactor) + (SCALE_1_TO_1 / 2)) / SCALE_1_TO_1;
         NewPngHeight = ((TempHeight * ScalingFactor) + (SCALE_1_TO_1 / 2)) / SCALE_1_TO_1;
         LOG("Scaling png to ");
         if(bRotate) {
            LOG_RAW("%dx%d\n",NewPngHeight,NewPngWidth);
         }
         else {
            LOG_RAW("%dx%d\n",NewPngWidth,NewPngHeight);
         }
      }

      switch(Options & X_ALIGN_MASK) {
         case X_ALIGN_CENTER:
            Xoffset = (SprWidth - NewPngWidth) / 2;
            break;

         case X_ALIGN_LEFT:
            Xoffset = 0;
            break;

         case X_ALIGN_RIGHT:
            Xoffset = SprWidth - NewPngWidth;
            break;

         default:
            ErrLine = __LINE__;
            break;
      }

      switch(Options & Y_ALIGN_MASK) {
         case Y_ALIGN_CENTER:
            Yoffset = (SprHeight - NewPngHeight) / 2;
            break;

         case Y_ALIGN_TOP:
            LOG("Y_ALIGN_TOP\n");
            Yoffset = 0;
            break;

         case Y_ALIGN_BOTTOM:
            LOG("Y_ALIGN_BOTTOM\n");
            Yoffset = SprHeight - NewPngHeight;
            break;

         default:
            ErrLine = __LINE__;
            break;
      }

      if(ErrLine != 0) {
         break;
      }
      LOG("Xoffset %d Yoffset %d\n",Xoffset,Yoffset);

   // Decode PNG file into SPR
      err = png.decode(this,0);
      if(err != PNG_SUCCESS) {
         ErrLine = __LINE__;
         break;
      }
      Ret = 0;
   } while(false);

   if(bPngOpened) {
      png.close();
   }

   if(ErrLine != 0) {
      LOG_RAW("%s: Error %d on line %d\n",__FUNCTION__,err,ErrLine);
   }

   return Ret;
}


void DrawPNG::SetSprOffsets(int x,int y)
{
   XsprOffset = x;
   YsprOffset = y;
   LOG("XsprOffset %d YsprOffset %d\n",XsprOffset,YsprOffset);
}

void DrawPNG::SetOptions(uint32_t options)
{
   Options = options;
}

#endif // WITHOUT_PNG


