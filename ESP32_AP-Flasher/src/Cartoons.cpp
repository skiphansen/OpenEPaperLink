#ifndef WITHOUT_CARTOONS
#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <FS.h>
#include <PNGdec.h>

#include "TFT_eSPI.h"
#include "tag_db.h"
#include "makeimage.h"
#include "contentmanager.h"
#include "util.h"
#include "web.h"
#include "storage.h"
#include <DrawPNG.h>

#define ENABLE_LOGGING  1
#if ENABLE_LOGGING && __has_include("logging.h") 
#include "logging.h"
#else
#define LOG(format, ...)
#define LOG_RAW(format, ...)
#endif

File PngFile;

/* 
 Q: Is there an interface for automated systems to access comics and metadata?
 A: Yes. You can get comics through the JSON interface, at URLs like
 https://xkcd.com/info.0.json (current comic) and
 https://xkcd.com/614/info.0.json (comic #614).
 
Typical response: 
 
{
  "month": "9",
  "num": 3297,
  "link": "",
  "year": "2026",
  "news": "",
  "safe_title": "OH Scale",
  "transcript": "",
  "alt": "To access distant parts of the project, I ended up building a regular-scale passenger train inside each rail.",
  "img": "https://imgs.xkcd.com/comics/oh_scale.png",
  "title": "OH Scale",
  "day": "11"
}
*/

static void *PngOpen(const char *filename, int32_t *size);
static void PngClose(void *handle);
static int32_t PngRead(PNGFILE *handle, uint8_t *buffer, int32_t length);
static int32_t PngSeek(PNGFILE *handle, int32_t position);

// #define FILENAME "/size_and_lifespan.png"
// #define FILENAME "/xkcd_917.png"
int Cartoons(TFT_eSprite &spr, JsonObject &cfgobj, const tagRecord *taginfo, imgParam &imageParams)
{
   int Ret = -1; // Assume the worse
   class DrawPNG *png = NULL;
   int Err;
   String Path("/temp/xkcd_");
   do {
      util::printHeap();
      int bIsRandom = cfgobj["random"].as<int>();
#ifdef FILENAME
      Path = FILENAME;
#else
      String Filename;
      JsonDocument doc;
      String Url("https://xkcd.com/info.0.json");

      LOG("Url = \"%s\"\n",Url.c_str());

      if(!util::httpGetJson(Url,doc,5000)) {
         ELOG("httpGetJson of %s failed\n",Url.c_str());
         break;
      }

      Url = doc["img"].as<String>();
      LOG("image Url = \"%s\"\n",Url.c_str());
      int XkcdNumber = doc["num"].as<int>();

      if(bIsRandom) {
         srand(millis());
         XkcdNumber = rand() % (XkcdNumber + 1);
         Url = "https://xkcd.com/" + String(XkcdNumber) + "/info.0.json";
         if(!util::httpGetJson(Url,doc,5000)) {
            ELOG("httpGetJson of %s failed\n",Url.c_str());
            break;
         }
      }

      Url = doc["img"].as<String>();
      LOG("Url = \"%s\"\n",Url.c_str());
      Path += String(XkcdNumber) + ".png";

      if(!contentFS->exists(Path)){
         if((Err = DownloadURL(Url,Path)) != 200) {
            LOG("DownloadURL returned %d\n",Err);
            break;
         }
      }
#endif
      PngFileCBs_t CBs = {PngOpen,PngClose,PngRead,PngSeek};

      png = new DrawPNG(&CBs);
      if(png == NULL) {
         LOG("new DrawPNG failed\n");
         break;
      }
      LOG("Calling DrawPng with %s\n",Path.c_str());
      if((Ret = png->DrawPng(Path,spr)) != 0) {
         LOG("DrawPng failed %d\n",Ret);
         break;
      }

   // 0: Dithering disable
   // 1: Burkes Dithering
   // 2: Special ordered dithering (selected by holding shift key when drag&dropping

   // for airport_meeting.png dither 0 looks beat
      imageParams.dither = 0;
      LOG("imageParams.dither %d\n",imageParams.dither);
   } while(false);

   if(png != NULL) {
      delete png;
   }

#ifndef FILENAME
   if(contentFS->exists(Path)) {
      contentFS->remove(Path);
   }
#endif
   util::printHeap();
   LOG("Returning %d\n",Ret);
   return Ret;
}

static void *PngOpen(const char *filename, int32_t *size) 
{
   PngFile = contentFS->open(filename, "r");
    if (!PngFile) return NULL;
    *size = PngFile.size();
    return &PngFile;
}

static void PngClose(void *handle) 
{
   PngFile.close();
}

static int32_t PngRead(PNGFILE *handle, uint8_t *buffer, int32_t length) 
{
    if (!PngFile) return 0;
    return PngFile.read(buffer, length);
}

static int32_t PngSeek(PNGFILE *handle, int32_t position) 
{
    if (!PngFile) return 0;
    return PngFile.seek(position);
}
#endif   // WITHOUT_CARTOONS

