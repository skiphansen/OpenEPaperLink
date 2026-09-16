#ifndef _DRAWPNG_H_
#define _DRAWPNG_H_

class DrawPNG {
public:
   bool DrawPng(String Filename,TFT_eSprite &spr, const tagRecord *taginfo, imgParam &imageParams);

// NB: PngFile must be static and public since PngOpen(), PngClose(), PngRead() 
// and PngSeek() don't have an user argument that can be used to point to the 
// File structure.  REVISIT ME !

   static File PngFile;

private:
   static void *PngOpen(const char *filename, int32_t *size);
   static void PngClose(void *handle);
   static int32_t PngRead(PNGFILE *handle, uint8_t *buffer, int32_t length);
   static int32_t PngSeek(PNGFILE *handle, int32_t position);
   static int pngDrawCallback(PNGDRAW *pDraw);

   int DrawCB(PNGDRAW *pDraw);

   int Xoffset;
   int Yoffset;
   int ScalingFactor;
   PNG png; // PNG structure (about 50K of RAM)
   TFT_eSPI *pSpr;
};

#endif   // _DRAWPNG_H_


