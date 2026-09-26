#ifndef WITHOUT_JPG
#include <Arduino.h>
#include <vector>
#include <TJpg_Decoder.h>

#include "TFT_eSPI.h"
#include "DrawJPG.h"
using namespace JPG_defines;

#define ENABLE_LOGGING  1
#if ENABLE_LOGGING && __has_include("logging.h") 
#include "logging.h"
#else
#define LOG(format, ...)
#define LOG_RAW(format, ...)
#endif

// needed because the jpeg drawing callback does not include a user argument
class DrawJPG *DrawJPG::pClass = NULL;

struct Color {
    uint8_t r, g, b;
    Color() : r(0), g(0), b(0) {}
    Color(uint16_t value_) : 
          r(((value_ >> 8) & 0xF8) | ((value_ >> 13) & 0x07)),
          g(((value_ >> 3) & 0xFC) | ((value_ >> 9) & 0x03)),
          b(((value_ << 3) & 0xF8) | ((value_ >> 2) & 0x07)) {}
    Color(uint8_t r_, uint8_t g_, uint8_t b_) : r(r_), g(g_), b(b_) {}
};

DrawJPG::DrawJPG(fs::FS *contentFS) : contentFS(contentFS)
{
   if(pClass != NULL) {
      LOG("pClass %p, this %p\n",pClass,this);
      bTooManyInstances = true;
   }
   else {
      pClass = this;
      bTooManyInstances = false;
      XsprOffset = 0;
      YsprOffset = 0;
      Options = 0;
   }
}

DrawJPG::~DrawJPG()
{
   if(pClass == this) {
      pClass = NULL;
   }
}


bool DrawJPG::InternalDrawCB(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *p)
{
   int Xoff;
   int Yoff;

#if 0
   if(y == 16) {
      LOG("%dx%d @ %d,%d\n",w,h,x,y);
      DUMP_HEX(p,w*h*2);
   }
#endif

   if(bRotate) {
      Xoff = Yoffset + YsprOffset;
      Yoff = Xoffset + XsprOffset;
   }
   else {
      Xoff = Xoffset + XsprOffset;
      Yoff = Yoffset + YsprOffset;
   }

#if 1
   if (ScalingFactor == SCALE_1_TO_1) {
#if 0
      pSpr->pushImage(x + Xoff,y + Yoff,w,h,p);
#else
      int y0 = y + Yoff;
      for(int i = 0; i < h; i++) {
         int x0 = x + Xoff;
         for(int j = 0; j < w; j++) {
#if 0
            if(y == 16) {
               LOG("%d,%d = 0x%x\n",x0,y0,*p);
            }
#endif
            pSpr->drawPixel(x0++,y0,*p++);
         }
         y0++;
      }
#endif
   }
#else
   if(!bRotate) {
      if (ScalingFactor == SCALE_1_TO_1) {
         pSpr->pushImage(x + Xoff,y + Yoff,w,h,bitmap);
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
#endif

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

bool DrawJPG::DrawCB(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap)
{
   return pClass->InternalDrawCB(x,y,w,h,bitmap);
}

int DrawJPG::DrawJpg(String Filename,TFT_eSprite &spr)
{
   int Ret = -1; // Assume the worse
   int ErrLine = 0;
   JRESULT jErr = JDR_OK;

   do {
      if(bTooManyInstances) {
         LOG("Error Too Many Instances\n");
         ErrLine = __LINE__;
         break;
      }
      pSpr = &spr;
      LOG("pSpr %p\n",pSpr);
      TJpgDec.setSwapBytes(false);
      TJpgDec.setJpgScale(1);
      TJpgDec.setCallback(DrawCB);

      jErr = TJpgDec.getFsJpgSize(&JpgWidth,&JpgHeight,Filename,*contentFS);
      if(jErr != JDR_OK) {
         LOG("getFsJpgSize failed %d\n",jErr);
         ErrLine = __LINE__;
         break;
      }

      int TempWidth = JpgWidth;
      int TempHeight = JpgHeight;
      int SprWidth = pSpr->width();
      int SprHeight = pSpr->height();

      LOG("%s: %dx%d\n",Filename.c_str(),JpgWidth,JpgHeight);

      bRotate = false;
      if(JpgHeight > JpgWidth) {
         switch(Options & ROTATE_MODE_MASK) {
            case ROTATE_MODE_OFF:   // never rotate
               break;

            case ROTATE_MODE_ON:    // always roate
               LOG("Rotating image\n");
               bRotate = true;
               break;

            case ROTATE_MODE_FIT:   // only rotate when scaling 
               if(JpgWidth > SprWidth || JpgHeight > SprHeight) {
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
         TempWidth = JpgHeight;
         TempHeight = JpgWidth;
      }

      int NewJpgWidth;
      int NewJpgHeight;
      if(TempWidth <= SprWidth && TempHeight <= SprHeight) {
      // No scaling required
         LOG("No scaling required\n");
         ScalingFactor = SCALE_1_TO_1;
         NewJpgWidth = TempWidth;
         NewJpgHeight = TempHeight;
      }
      else {
         LOG("Scaling needed, png %dx%d, spr %dx%d\n",
             JpgWidth,JpgHeight,SprWidth,SprHeight);
         unsigned int xScale = (SCALE_1_TO_1 * SprWidth) / TempWidth;
         unsigned int yScale = (SCALE_1_TO_1 * SprHeight) / TempHeight;
         ScalingFactor = xScale < yScale ? xScale : yScale;
         LOG("xScale %u yScale %u ScalingFactor %u\n",xScale,yScale,ScalingFactor);
         NewJpgWidth = (( TempWidth * ScalingFactor) + (SCALE_1_TO_1 / 2)) / SCALE_1_TO_1;
         NewJpgHeight = ((TempHeight * ScalingFactor) + (SCALE_1_TO_1 / 2)) / SCALE_1_TO_1;
         LOG("Scaling png to ");
         if(bRotate) {
            LOG_RAW("%dx%d\n",NewJpgHeight,NewJpgWidth);
         }
         else {
            LOG_RAW("%dx%d\n",NewJpgWidth,NewJpgHeight);
         }
      }

      switch(Options & X_ALIGN_MASK) {
         case X_ALIGN_CENTER:
            Xoffset = (SprWidth - NewJpgWidth) / 2;
            break;

         case X_ALIGN_LEFT:
            Xoffset = 0;
            break;

         case X_ALIGN_RIGHT:
            Xoffset = SprWidth - NewJpgWidth;
            break;

         default:
            ErrLine = __LINE__;
            break;
      }

      switch(Options & Y_ALIGN_MASK) {
         case Y_ALIGN_CENTER:
            Yoffset = (SprHeight - NewJpgHeight) / 2;
            break;

         case Y_ALIGN_TOP:
            LOG("Y_ALIGN_TOP\n");
            Yoffset = 0;
            break;

         case Y_ALIGN_BOTTOM:
            LOG("Y_ALIGN_BOTTOM\n");
            Yoffset = SprHeight - NewJpgHeight;
            break;

         default:
            ErrLine = __LINE__;
            break;
      }

      if(ErrLine != 0) {
         break;
      }
      LOG("Xoffset %d Yoffset %d\n",Xoffset,Yoffset);
   // Decode JPG file into SPR
      if((jErr = TJpgDec.drawFsJpg(0,0,Filename,*contentFS)) != JDR_OK) {
         ErrLine = __LINE__;
         break;
      }
      Ret = 0;
   } while(false);

   if(ErrLine != 0) {
      LOG_RAW("%s: Error %d on line %d\n",__FUNCTION__,jErr,ErrLine);
   }

   return Ret;
}


void DrawJPG::SetSprOffsets(int x,int y) 
{
   XsprOffset = x;
   YsprOffset = y;
   LOG("XsprOffset %d YsprOffset %d\n",XsprOffset,YsprOffset);
}

void DrawJPG::SetOptions(uint32_t options)
{
   Options = options;
}

#endif // WITHOUT_JPG


