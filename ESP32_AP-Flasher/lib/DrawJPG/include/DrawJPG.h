#ifndef _DRAWJPG_H_
#define _DRAWJPG_H_

namespace JPG_defines {
   #define X_ALIGN_SHIFT   0
   #define X_ALIGN_MASK    0x3
   #define X_ALIGN_CENTER  0
   #define X_ALIGN_LEFT    1
   #define X_ALIGN_RIGHT   2

   #define Y_ALIGN_SHIFT   2
   #define Y_ALIGN_MASK    (0x3 <<Y_ALIGN_SHIFT)
   #define Y_ALIGN_CENTER  (0 << Y_ALIGN_SHIFT)
   #define Y_ALIGN_TOP     (1 << Y_ALIGN_SHIFT)
   #define Y_ALIGN_BOTTOM  (2 << Y_ALIGN_SHIFT)

   #define ROTATE_MODE_SHIFT  4
   #define ROTATE_MODE_MASK   (0x3 << ROTATE_MODE_SHIFT)
   #define ROTATE_MODE_OFF    (0 << ROTATE_MODE_SHIFT)
   #define ROTATE_MODE_ON     (1 << ROTATE_MODE_SHIFT)
   #define ROTATE_MODE_FIT    (2 << ROTATE_MODE_SHIFT)

   #define SCALE_1_TO_1 32768
}

class DrawJPG {
public:
   DrawJPG(fs::FS *contentFS);
   ~DrawJPG();
   int DrawJpg(String Filename,TFT_eSprite &spr);
   void SetSprOffsets(int x,int y);
   void SetOptions(uint32_t options);

private:
   bool InternalDrawCB(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap);
   static bool DrawCB(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap);
   static class DrawJPG *pClass;
   bool bTooManyInstances;
   bool bRotate;
   int Xoffset;
   int Yoffset;
   int XsprOffset;
   int YsprOffset;
   uint16_t JpgWidth;
   uint16_t JpgHeight;

// SCALE_1_TO_1 = 100%, ie none  (SCALE_1_TO_1 / 2) = reduce resolution by 2
   unsigned int ScalingFactor;   
   uint32_t Options;
   TFT_eSPI *pSpr;
   fs::FS *contentFS;
};

#endif   // _DRAWJPG_H_


