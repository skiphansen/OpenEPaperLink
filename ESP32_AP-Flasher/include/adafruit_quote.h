#ifndef _ADAFRUIT_QUOTE_H_
#define _ADAFRUIT_QUOTE_H_
class AdaFruitQuote {
public:
   AdaFruitQuote(TFT_eSprite &spr,JsonObject &cfgobj,tagRecord *taginfo,imgParam &imageParams) : spr(spr), cfgobj(cfgobj), taginfo(taginfo), imageParams(imageParams) {}

   void Draw();
private:
   TFT_eSprite &spr;
   JsonObject &cfgobj;
   tagRecord *taginfo;
   imgParam &imageParams;
};

#endif   // _ADAFRUIT_QUOTE_H_

