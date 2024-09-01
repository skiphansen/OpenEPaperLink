#ifndef _ADAFRUIT_QUOTE_H_
#define _ADAFRUIT_QUOTE_H_
class AdaFruitQuote {
public:
   AdaFruitQuote(TFT_eSprite &spr,JsonDocument &Template) : 
      spr(spr), 
      Template(Template),
      WordBuf(NULL),
      Text(NULL) 
   {}
   ~AdaFruitQuote();


   void Draw();
private:
   TFT_eSprite &spr;
   JsonDocument &Template;
   JsonArray qfont;
   JsonArray sfont;
   JsonArray afont;
   char *WordBuf;
   char *Text;
   int lineend;
   int bufflen;
   uint16_t height;
   uint16_t DrawX;
   uint16_t DrawY;
   bool bsmallfont;
   const char *FontName;
   int FontSize;

   uint16_t getStringLength(const char *str, int strlength = 0);
   void Unicode2Ascii(char *str);
   char *wrapWord(const char *str, int linesize);
   int getLineCount(const char *str, int scrwidth);
   void printQuote(const char *quote);
   void printQuote(String &quote);
   void printAuthor(const char *author);
   void SelectFont(JsonArray &Font);
};

#endif   // _ADAFRUIT_QUOTE_H_

