#ifndef WITHOUT_COMICS
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
bool drawComic(String &filename, JsonObject &cfgobj, const tagRecord *taginfo, imgParam &imageParams) 
{
   bool bRet = false;
   int bIsRandom = false;
   bool bNewLatest = false;
   uint32_t Options = 0;
   class DrawPNG *png = NULL;
   int Err;
   TFT_eSprite spr = TFT_eSprite(&tft);
   String Path("/temp/xkcd_");

   initSprite(spr, imageParams.width, imageParams.height, imageParams);
   do {
      bIsRandom = cfgobj["random"].as<int>();
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
         XkcdNumber = random(XkcdNumber + 1);
         Url = "https://xkcd.com/" + String(XkcdNumber) + "/info.0.json";
         if(!util::httpGetJson(Url,doc,5000)) {
            ELOG("httpGetJson of %s failed\n",Url.c_str());
            break;
         }
      }
      else {
         Path += "latest_";
      }

      Url = doc["img"].as<String>();
      LOG("Url = \"%s\"\n",Url.c_str());

      Path += String(XkcdNumber) + ".png";

      if(contentFS->exists(Path)){
         LOG("Already have %s\n",Path.c_str());
      }
      else {
         if((Err = DownloadURL(Url,Path)) != 200) {
            LOG("DownloadURL returned %d\n",Err);
            break;
         }
         bNewLatest = !bIsRandom;
      }
#endif
      int rotation = cfgobj["rotation"].as<int>();
      switch(rotation) {
         case 0:
            break;

         case 1:
            Options |= ROTATE_MODE_ON;
            break;

         case 2:
            Options |= ROTATE_MODE_FIT;
            break;

         default:
            LOG("rotation option %d is not supported\n",rotation);
      }

      PngFileCBs_t CBs = {PngOpen,PngClose,PngRead,PngSeek};
      png = new DrawPNG(&CBs);
      if(png == NULL) {
         LOG("new DrawPNG failed\n");
         break;
      }
      png->SetOptions(Options);

      LOG("Calling DrawPng with %s\n",Path.c_str());
      if((Err = png->DrawPng(Path,spr)) != 0) {
         LOG("DrawPng failed %d\n",Err);
         break;
      }

   // 0: Dithering disable
   // 1: Burkes Dithering
   // 2: Special ordered dithering (selected by holding shift key when drag&dropping
   // 9: automatic (future)
      int dither = cfgobj["dither"].as<int>();
      switch(dither) {
         case 0:
         case 1:
         case 2:
            imageParams.dither = dither;
            break;

         default:
            LOG("rotation options %d is not supported\n",dither);
      }

      LOG("imageParams.dither %d\n",imageParams.dither);
      spr2buffer(spr, filename, imageParams);
      bRet = true;
   } while(false);

   spr.deleteSprite();
   if(png != NULL) {
      delete png;
   }

#ifndef FILENAME
   if(bIsRandom) {
      LOG("Deleting %s\n",Path.c_str());
      contentFS->remove(Path);
   }
   else if(bNewLatest){
   // delete any old images
      File dir = contentFS->open("/temp");
      File file = dir.openNextFile();
      while (file) {
         String Entry = String("/temp/") + file.name();
         LOG("Found '%s'\n",Entry.c_str());
         file.close();
         if(Entry != Path && Entry.indexOf("latest_") > 0) {
            LOG("Deleting %s\n",Entry.c_str());
            contentFS->remove(Entry);
         }
         file = dir.openNextFile();
      }
      dir.close();
   }
#endif
   LOG("Returning %d\n",bRet);
   return bRet;
}

static void *PngOpen(const char *filename, int32_t *size) 
{
   File *pFile = new File(contentFS->open(filename, "r"));
   if (!pFile) return NULL;
   if(!pFile->available()) {
      LOG("Couldn't open \"%s\"\n",filename);
      delete pFile;
      return NULL;
   }
   *size = pFile->size();
   LOG("File \"%s\" opened (%d bytes)\n",filename,*size);

   return pFile;
}

static void PngClose(void *handle) 
{
   File *pFile = static_cast<File *>(handle);
   pFile->close();
   delete pFile;
   LOG("File PngFile closed\n");
}

static int32_t PngRead(PNGFILE *handle, uint8_t *buffer, int32_t length) 
{
   File *pFile = static_cast<File *>(handle->fHandle);
   if (!pFile) return 0;
   return pFile->read(buffer, length);
}

static int32_t PngSeek(PNGFILE *handle, int32_t position) 
{
   File *pFile = static_cast<File *>(handle->fHandle);
   if (!pFile) return 0;
   return pFile->seek(position);
}
#endif   // WITHOUT_COMICS

