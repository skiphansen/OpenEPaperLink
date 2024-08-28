#ifndef _ADAFRUIT_QUOTE_H_
#define _ADAFRUIT_QUOTE_H_
class AdaFruitQuote {
public:
   AdaFruitQuote(TFT_eSprite &spr,JsonDocument &Template) : 
      spr(spr), 
      Template(Template),
      WordBuf(NULL) 
   {}
   ~AdaFruitQuote();


   void Draw();
private:
   TFT_eSprite &spr;
   JsonDocument &Template;
   char *WordBuf;
   int lineend;
   int bufflen;
   uint16_t height;
   uint16_t DrawX;
   uint16_t DrawY;

   int getStringLength(const char *str, int strlength = 0);
   char *wrapWord(const char *str, int linesize);
   int getLineCount(const char *str, int scrwidth);
//   int getLineHeight(const GFXfont *font = NULL);
   void printQuote(TFT_eSprite spr,String &quote);
   void printAuthor(String author);
   void printOther(String other);

};

#endif   // _ADAFRUIT_QUOTE_H_

