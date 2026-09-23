#ifndef _DRAWPNG_H_
#define _DRAWPNG_H_

typedef struct {
   PNG_OPEN_CALLBACK *pfnOpen;
   PNG_CLOSE_CALLBACK *pfnClose;
   PNG_READ_CALLBACK *pfnRead;
   PNG_SEEK_CALLBACK *pfnSeek;
} PngFileCBs_t;

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

class DrawPNG {
public:
   DrawPNG(PngFileCBs_t *PngFileCBs);
   int DrawPng(String Filename,TFT_eSprite &spr);
   void SetSprOffsets(int x,int y);
   void SetOptions(uint32_t options);

private:
   static int pngDrawCallback(PNGDRAW *pDraw);
   int DrawCB(PNGDRAW *pDraw);

   bool bRotate;
   int Xoffset;
   int Yoffset;
   int XsprOffset;
   int YsprOffset;
   int PngWidth;
   int PngHeight;

// SCALE_1_TO_1 = 100%, ie none  (SCALE_1_TO_1 / 2) = reduce resolution by 2
   unsigned int ScalingFactor;   
   uint32_t Options;
   PNG png; // PNG structure (about 50K of RAM)
   TFT_eSPI *pSpr;
   PngFileCBs_t *pCBs;
};

#endif   // _DRAWPNG_H_


