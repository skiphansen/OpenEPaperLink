#ifndef _OTA_HDR_H_
#define _OTA_HDR_H_

#define OTA_HDR_VER        1
#define OTA_TYPE_LEN       16

#ifndef GCC_PACKED
#if defined(__GNUC__)
#define GCC_PACKED __attribute__ ((packed))
#else
#define GCC_PACKED
#endif
#endif

typedef struct {
   uint32_t Crc;           
   uint16_t ImageVer;      // FW Version
   uint16_t ImageLen;
   uint8_t  HdrVersion;    // OTA_HDR_VER
   char     ImageType[OTA_TYPE_LEN]; // From OTA bin filename
} GCC_PACKED OtaHeader;

#define OTA_ERR_HDR_VER       1
#define OTA_ERR_WRONG_BOARD   2
#define OTA_ERR_INVALID_LEN   3
#define OTA_ERR_INVALID_CRC   4
#define OTA_ERR_INVALID_HDR   5

#endif   // _OTA_HDR_H_

