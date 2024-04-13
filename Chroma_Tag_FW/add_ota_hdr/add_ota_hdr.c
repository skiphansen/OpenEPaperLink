#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <sys/stat.h>

#ifdef _WIN32
int optind = 1;
int optopt;
char *optarg;
int getopt(int argc,char **argv,char *opts);
#define strdup _strdup
#else
#include <unistd.h>
#endif

#include "ota_hdr.h"

#define OPTION_STRING  "chp"

// filename format: <board_name>_full_<version>.bin
int32_t gCRC = 0;
OtaHeader gOtaHeader;

// from crc32.c
uint32_t crc32(uint32_t crc, const void *buf, size_t size);
uint32_t small_crc32(uint32_t crc,uint8_t *Data, size_t size);
int CreateOtaFile(const char *FilePath);

char gOutFile[80];
char *gImage = NULL;
char *gOutPath = NULL;

void Usage(void);
int CrcCheck(FILE *fp,uint32_t DataLen);
int CrcCheckFile(const char *File);
int ParseFile(const char *File);

int main(int argc, char *argv[])
{
   int Option;
   int Ret = 0; // Assume the best
   int FileNameArg = 1;
   bool bCrcCheck = false;
   bool bParse = false;

   while(Ret == 0 && (Option = getopt(argc,argv,OPTION_STRING)) != -1) {
      switch(Option) {
         case 'c':   // display CRC and exit
            bCrcCheck = true;
            break;

         case 'p':
            bParse = true;
            break;

         case 'h':
         case '?':
            Ret = EINVAL;
            break;
      }
      FileNameArg++;
   }

   do {
      if((argc - FileNameArg) != 1) {
         Ret = EINVAL;
         break;
      }

      if(bCrcCheck) {
         Ret = CrcCheckFile(argv[FileNameArg]);
         break;
      }

      if(bParse) {
         Ret = ParseFile(argv[FileNameArg]);
         break;
      }
   // No options, default - create OTA file
      Ret = CreateOtaFile(argv[FileNameArg]);
   } while(false);

   if(Ret == EINVAL) {
      Usage();
   }

   if(gImage != NULL) {
      free(gImage);
   }

   if(gOutPath != NULL) {
      free(gOutPath);
   }

   exit(Ret);
}

// Input filename format: <board_name>_full_<version>.bin
// Out filename format: <board_name>_ota_<version>.bin
int InitHdr(const char *Path)
{
   int Ret = 0;
   char *cp;
   int FwVer;
   char const *Filename;

   do {
      if((gOutPath = strdup(Path)) == NULL) {
         Ret = ENOMEM;
         break;
      }

      if((cp = strrchr(gOutPath,'/')) != NULL) {
         Filename = cp + 1;
      }
      else {
         Filename = gOutPath;
      }

      gOtaHeader.HdrVersion = OTA_HDR_VER;
      cp = strstr(Filename,"_full_");
      if(cp == NULL) {
         printf("Error: filename does not contain '_full_'\n");
         Ret = EINVAL;
         break;
      }

      *cp = 0;
      if(strlen(Filename) > OTA_TYPE_LEN) {
         printf("Error: filename too long\n");
         Ret = EINVAL;
         break;
      }
      strcpy(gOtaHeader.ImageType,Filename);

      if(sscanf(cp + 1,"full_%x",&FwVer) != 1) {
         printf("Error: filename does not end with a version number\n");
         Ret = EINVAL;
         break;
      }

      if(FwVer > 0xffff) {
         printf("Error: version number > ffff\n");
         Ret = EINVAL;
         break;
      }
      gOtaHeader.ImageVer = (uint16_t) FwVer;
      sprintf(cp,"_ota_%04x.bin",FwVer);
   } while(false);

   if(Ret == EINVAL) {
      printf("Invalid filename %s\n",Filename);
   }
   return Ret;
}

int CreateOtaFile(const char *FilePath)
{
   int Ret;
   FILE *fout = NULL;

   do {
      if((Ret = InitHdr(FilePath)) != 0) {
         break;
      }

      if((Ret = CrcCheckFile(FilePath)) != 0) {
         break;
      }
      gOtaHeader.Crc = gCRC;

      printf("Opening %s\n",gOutPath);
      if((fout = fopen(gOutPath,"w")) == NULL) {
         printf("fopen(%s) failed - %s\n",gOutPath,strerror(errno));
         Ret = errno;
         break;
      }

      if(fwrite(&gOtaHeader,sizeof(gOtaHeader),1,fout) != 1) {
         printf("fwrite failed - %s\n",strerror(errno));
         Ret = errno;
         break;
      }

      if(fwrite(gImage,gOtaHeader.ImageLen,1,fout) != 1) {
         printf("fwrite failed - %s\n",strerror(errno));
         Ret = errno;
         break;
      }
   } while(false);

   if(fout != NULL) {
      fclose(fout);
   }

   return Ret;
}


int CrcCheck(FILE *fp,uint32_t DataLen)
{
   gImage = malloc(DataLen);
   int Ret = 0;

   do {
      if(gImage == NULL) {
         printf("Malloc for %" PRIu32 " bytes failed\n",DataLen);
         Ret = ENOMEM;
         break;
      }
      if(fread(gImage,DataLen,1,fp) != 1) {
         Ret = errno;
         printf("%s#%d: \n",__FUNCTION__,__LINE__);
         printf("Error: fread failed - %s\n",strerror(errno));
         break;
      }
   // update CRC 
      gCRC = crc32(gCRC,gImage,DataLen);
   } while(false);

   return Ret;
}

int CrcCheckFile(const char *File)
{
   FILE *fp = NULL;
   int Ret = 0;   // assume the best
   struct stat Stat;

   do {
      if(stat(File,&Stat) != 0) {
         Ret = errno;
         printf("Error: stat(%s) failed - %s\n",File,strerror(errno));
         break;
      }

      if(Stat.st_size > 0x8000) {
         printf("Error: File too big\n");
         Ret = EINVAL;
         break;
      }
      gOtaHeader.ImageLen = (uint16_t) Stat.st_size;

      if((fp = fopen(File,"rb")) == NULL) {
         Ret = errno;
         printf("Error: unable to open %s for read - %s\n",File,strerror(errno));
         break;
      }

      if((Ret = CrcCheck(fp,Stat.st_size)) == 0) {
         printf("%s: %ld bytes, CRC 0x%08x\n",File,Stat.st_size,gCRC);
      }
   } while(false);

   if(fp != NULL) {
      fclose(fp);
   }

   return Ret;
}

int ParseFile(const char *File)
{
   FILE *fp = NULL;
   int Ret = 0;   // assume the best
   struct stat Stat;

   do {
      if(stat(File,&Stat) != 0) {
         Ret = errno;
         printf("Error: stat(%s) failed - %s\n",File,strerror(errno));
         break;
      }

      if((fp = fopen(File,"rb")) == NULL) {
         Ret = errno;
         printf("Error: unable to open %s for read - %s\n",File,strerror(errno));
         break;
      }

      if(fread(&gOtaHeader,sizeof(gOtaHeader),1,fp) != 1) {
         Ret = errno;
         printf("%s#%d: \n",__FUNCTION__,__LINE__);
         printf("Error: fread failed - %s\n",strerror(errno));
         break;
      }

      if(gOtaHeader.HdrVersion != OTA_HDR_VER) {
         printf("Invalid HdrVersion %d\n",gOtaHeader.HdrVersion);
         break;
      }

      if(gOtaHeader.ImageLen > 0x8000) {
         printf("Invalid image len %u\n",gOtaHeader.ImageLen);
         break;
      }

      if(Stat.st_size != gOtaHeader.ImageLen + sizeof(gOtaHeader)) {
         printf("Error: filesize and ImageLen don't match\n");
         break;
      }

      if((Ret = CrcCheck(fp,gOtaHeader.ImageLen)) != 0) {
         break;
      }

      if(gOtaHeader.Crc != gCRC) {
         printf("Error: Invalid CRC 0x%04x, calculated 0x%04x\n",
                gOtaHeader.Crc,gCRC);
         break;
      }
      printf("OTA update is valid:\n");
      printf("  CRC 0x%04x\n",gOtaHeader.Crc);
      printf("  ImageVer 0x%04x\n",gOtaHeader.ImageVer);
      printf("  ImageLen %d\n",gOtaHeader.ImageLen);
      printf("  HdrVersion %d\n",gOtaHeader.HdrVersion);
      printf("  ImageType %s\n",gOtaHeader.ImageType);
   } while(false);

   if(fp != NULL) {
      fclose(fp);
   }

   return Ret;
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

void Usage()
{
   printf("add_ota_hdr compiled "__DATE__ " "__TIME__"\n");
   printf("Usage: add_ota_hdr [options] <path to full binary file>\n");
   printf("\t-c\tCalculate CRC of specified file\n");
   printf("\t-p\tParse specified OTA file\n");
   printf("\t-h\tHelp\n");
}


