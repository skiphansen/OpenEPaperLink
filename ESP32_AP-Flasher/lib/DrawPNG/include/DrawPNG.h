#ifndef _DRAWPNG_H_
#define _DRAWPNG_H_

typedef struct {
   PNG_OPEN_CALLBACK *pfnOpen;
   PNG_CLOSE_CALLBACK *pfnClose;
   PNG_READ_CALLBACK *pfnRead;
   PNG_SEEK_CALLBACK *pfnSeek;
} PngFileCBs_t;

#define SCALE_1_TO_1 32768

class DrawPNG {
public:
   DrawPNG(PngFileCBs_t *PngFileCBs);
   bool DrawPng(String Filename,TFT_eSprite &spr);
   void SetSprOffsets(int x,int y);

private:
   static void *PngOpen(const char *filename, int32_t *size);
   static void PngClose(void *handle);
   static int32_t PngRead(PNGFILE *handle, uint8_t *buffer, int32_t length);
   static int32_t PngSeek(PNGFILE *handle, int32_t position);
   static int pngDrawCallback(PNGDRAW *pDraw);

   int DrawCB(PNGDRAW *pDraw);

   int Xoffset;
   int Yoffset;
   int XsprOffset;
   int YsprOffset;
// SCALE_1_TO_1 = 100%, ie none  (SCALE_1_TO_1 / 2) = reduce resolution by 2
   unsigned int ScalingFactor;   
   PNG png; // PNG structure (about 50K of RAM)
   TFT_eSPI *pSpr;
   PngFileCBs_t *pCBs;
};

#endif   // _DRAWPNG_H_


