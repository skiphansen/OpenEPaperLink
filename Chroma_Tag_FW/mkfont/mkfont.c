#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>

#ifdef _WIN32
int optind = 1;
int optopt;
char *optarg;
int getopt(int argc,char **argv,char *opts);
#define strdup _strdup
#else
#include <unistd.h>
#endif

#define __code
#include "font.h"

#define DEFINED_CHARS   96
#define FONT_HEIGHT  16
#define FONT_WIDTH   10

#define OPTION_STRING  "lv"

#define LOG(format, ... ) fprintf(stderr,format,## __VA_ARGS__)
#define LOGV(format, ... ) if(gVerbose) LOG(format,## __VA_ARGS__)

uint16_t gFontIndexTbl[DEFINED_CHARS];
uint16_t gPackedData[DEFINED_CHARS * FONT_HEIGHT];
uint16_t gFontData[DEFINED_CHARS][FONT_HEIGHT];
bool gVerbose = false;

int PackFont(void);
int ListFont(void);
void FillFontData(void);

int main(int argc, char *argv[])
{
   int Ret = 0; // Assume the best
   int Option;
   bool bList = false;

   while(Ret == 0 && (Option = getopt(argc,argv,OPTION_STRING)) != -1) {
      switch(Option) {
         case 'l':
            bList = true;
            break;

         case 'v':
            gVerbose = true;
            break;

         case '?':
            Ret = EINVAL;
            break;
      }
   }

   if(Ret != 0) {
      printf("Usage: mkfont [-v] > <output_file>\n");
      printf("        mkfont -l\n");
   }
   else if(bList) {
      ListFont();
   }
   else {
      PackFont();
   }
   return Ret;
}

int PackFont()
{
   int FontDataOffset = 0;
   int FontDataLen;
   int i;
   int j;
   int FirstBit;
   int LastBit;
   int IndexValue;
   int CharWidth;
   int TotalCharWidth = 0;
   uint8_t *pPackedData = (uint8_t *) gPackedData;

   FillFontData();

// Create a 7 bit wide space (averge char with is 6.7 bits)
   for(i = 0; i < DEFINED_CHARS; i++) {
      for(j = 0; j < FONT_WIDTH; j++) {
         if(gFontData[i][j] != 0) {
            break;
         }
      }

      if(j == FONT_WIDTH) {
      // completely Empty, space or not implmented
         if(i != 0) {
            LOGV("Font for '%c' not defined\n",(char) ' ' + i);
            exit(1);
         }
         FirstBit = 0;
         CharWidth = 7;
      }
      else {
         FirstBit = j;
         LastBit = 0;
         while(j < FONT_WIDTH) {
            if(gFontData[i][j] != 0) {
               LastBit = j;
            }
            j++;
         }
         CharWidth = LastBit - FirstBit + 1;
      }
      TotalCharWidth += CharWidth;
      LOGV("Char '%c' width %d bits\n",' ' + i,CharWidth);
   // Save index to packed font data and character width
      IndexValue = (CharWidth << 12) | FontDataOffset;
      gFontIndexTbl[i] = (uint16_t) IndexValue;
      FontDataOffset += CharWidth;
   // copy bits to packed data array
      memcpy(pPackedData,&gFontData[i][FirstBit],CharWidth * 2);
      pPackedData += CharWidth * 2;
   }
   FontDataLen = FontDataOffset;

   for(i = 0; i < DEFINED_CHARS; i++) {
      CharWidth = gFontIndexTbl[i] >> 12;
      FontDataOffset = gFontIndexTbl[i] & 0xfff;
      LOGV("Char '%c' width %d, data offset %d\n  ",' ' + i, CharWidth,
             FontDataOffset);
      for(j = 0; j < CharWidth; j++) {
         LOGV("%s0x%04x",j == 0 ?  "" : ", ",gPackedData[FontDataOffset]);
         FontDataOffset++;
      }
      LOGV("\n");
   }

   LOGV("Average char width %d.%d bits\n",
       TotalCharWidth / DEFINED_CHARS,
       TotalCharWidth % DEFINED_CHARS);

   LOG("Unpacked font data size %ld.\n",sizeof(font));
   LOG("Packed font data size %ld.\n",FontDataOffset * sizeof(uint16_t));
   LOG("Packed font index size %ld.\n",sizeof(gFontIndexTbl));
   LOG("Packing font file saved %ld bytes.\n",
       sizeof(font) 
       - sizeof(gFontIndexTbl) 
       - (FontDataOffset * sizeof(uint16_t)));

   printf("static const uint16_t __code gFontIndexTbl[96] = {\n");
   for(i = 0; i < DEFINED_CHARS; i++) {
      CharWidth = gFontIndexTbl[i] >> 12;
      FontDataOffset = gFontIndexTbl[i] & 0xfff;
      printf("   0x%04x,",(unsigned int) gFontIndexTbl[i]);
      printf("   // Char ");
      if((' ' + i) == 0x7f) {
         printf("0x%x",' ' + i);
      }
      else {
         printf("'%c'",' ' + i);
      }
      printf(" width %d, data offset 0x%x\n",CharWidth,FontDataOffset);
   }
   printf("\n};\n\n");

   printf("static const uint16_t __code gPackedData[%d] = {",FontDataLen);
   for(i = 0; i < FontDataLen; i++) {
      if((i % 8) == 0) {
         printf("\n// 0x%x\n   ",i);
      }
      printf("0x%04x,",(unsigned int) gPackedData[i]);
   }
   printf("\n};\n\n");

   return 0;
}

int ListFont()
{
   int i;
   int j;
   int k;
   uint16_t Mask;

   FillFontData();

   for(i = 0; i < DEFINED_CHARS; i++) {
      printf("Char 0x%x",' ' + i);
      if((' ' + i) != 0x7f) {
         printf(", '%c'",' ' + i);
      }
      printf("\n");

      Mask = 0x8000;
      for(j = 0; j < FONT_HEIGHT; j++) {
         printf("Line %2d: |",j);
         for(k = 0; k < FONT_WIDTH; k++) {
            printf("%c",(gFontData[i][k] & Mask) ? '*' : ' ');
         }
         Mask >>= 1;
         printf("|\n");
      }

      for(j = 0; j < FONT_HEIGHT; j++) {
         printf("0x%04x ",gFontData[i][j]);
      }
      printf("\n");
   }

   return 0;
}

void FillFontData()
{
   int i;
   int j;

// convert font[] from byte array to uint16_t array
   for(i = 0; i < DEFINED_CHARS; i++) {
      for(j = 0; j < FONT_WIDTH; j++) {
         gFontData[i][j] = font[i][j * 2] | (font[i][(j * 2) + 1] << 8);
      }
      LOGV("Char '%c':\n",' ' + i);
      for(j = 0; j < FONT_WIDTH; j++) {
         LOGV("%s0x%04x",j == 0 ?  "" : ", ",gFontData[i][j]);
      }
      if(gVerbose) {
         for(j = 0; j < FONT_WIDTH; j++) {
            LOGV("%s0x%04x",j == 0 ?  "" : ", ",gFontData[i][j]);
         }
      }
      LOGV("\n");
   }
}

#ifdef _WIN32
/*
 * getopt a wonderful little function that handles the command line.
 * available courtesy of AT&T.
 */
int getopt(int argc,char **argv,char *opts)
{
    static int sp = 1;
    register int c;
    register char *cp;

    if(sp == 1)
        if(optind >= argc ||
           argv[optind][0] != '-' || argv[optind][1] == '\0')
            return(EOF);
        else if(strcmp(argv[optind], "--") == 0) {
            optind++;
            return(EOF);
        }
    optopt = c = argv[optind][sp];
    if(c == ':' || (cp=strchr(opts, c)) == NULL) {
        printf("%s: illegal option -- '%c'",argv[0],(char) c);
        if(argv[optind][++sp] == '\0') {
            optind++;
            sp = 1;
        }
        return('?');
    }
    if(*++cp == ':') {
        if(argv[optind][sp+1] != '\0')
            optarg = &argv[optind++][sp+1];
        else if(++optind >= argc) {
            printf("%s: option '%c' requires an argument -- ", argv[0],c);
            sp = 1;
            return('?');
        } else
            optarg = argv[optind++];
        sp = 1;
    } else {
        if(argv[optind][++sp] == '\0') {
            sp = 1;
            optind++;
        }
        optarg = NULL;
    }
    return(c);
}
#endif

