#ifndef _BOARD_H_
#define _BOARD_H_

#include "soc.h"
#include "u1.h"

#define CHROMA74
#define BOARD_NAME "Chroma42"
#define HW_TYPE    0x83

extern const char * __code gBoardName;

#if BUILD == chroma42y
#define BWY
#endif

//eeprom spi
#define EEPROM_SIZE              0x00100000L
#define EEPROM_4K_ERASE_OPCODE   0x20
#define EEPROM_32K_ERASE_OPCODE  0
#define EEPROM_64K_ERASE_OPCODE  0xD8
#define eepromByte               u1byte
#define eepromPrvSelect()     do { __asm__("nop"); P2_0 = 0; __asm__("nop"); } while(0)
#define eepromPrvDeselect()      do { __asm__("nop"); P2_0 = 1; __asm__("nop"); } while(0)

//debug uart (enable only when needed, on some boards it inhibits eeprom access)
#define dbgUartOn()                 u1setUartMode()
#define dbgUartOff()                u1setEepromMode()
#define dbgUartByte                 u1byte

/* eeprom map
 
EEPROM size: 0x10.0000 / 1024k / 1 Megabyte 
Image size:  (400 x 300 x 3 bits/pixel) / 8 bit / byte = 45000 bytes 
rounded up to erase boundary = 45,056 / 0x.b000 / 44k
Image slots: 21
 
| Start Adr | End Adr  | Size | Usage |
|     -     |    -     |   -  |   -   |
| 0x0.0000  | 0x0.1fff |   8k | Factory NVRAM
| 0x0.2000  | 0x0.5fff |  16k | Factory LUT ???
| 0x0.6000  | 0x0.7fff |  32k | unused ???
| 0x0.c000  | 0x0.dfff |   4k | OEPL Settings
| 0x0.e000  | 0x0.ffff |   4k | OEPL NVRAM
| 0x1.0000  | 0x1.8fff |  36k | OTA FW update
| 0x1.9000  | 0x2.3fff |  44k | Image slot 0 
| 0x2.4000  | 0x2.efff |  44k | Image slot 1
... 
| 0xf.5000  | 0xf.efff |  44k | Image slot 20

*/
#define EEPROM_FACTORY_NVRAM_ADR       (0x00000UL)
#define EEPROM_FACTORY_NVRAM_LEN       (EEPROM_ERZ_SECTOR_SZ * 2)

#define EEPROM_SETTINGS_AREA_START     (0x0c000UL)
#define EEPROM_SETTINGS_AREA_LEN       (EEPROM_ERZ_SECTOR_SZ)

#define EEPROM_OEPL_NVRAM_ADR          (0x0d000UL)
#define EEPROM_OEPL_NVRAM_LEN          (EEPROM_ERZ_SECTOR_SZ)
//some free space here

#define EEPROM_UPDATA_AREA_START    (0x10000UL)
#define EEPROM_UPDATE_AREA_LEN      (0x09000UL)

#define EEPROM_IMG_START            (0x19000UL)
#define EEPROM_IMG_EACH             (0x0b000UL)
#define EEPROM_IMG_SECTORS          (EEPROM_IMG_EACH / EEPROM_ERZ_SECTOR_SZ)
#define IMAGE_SLOTS                 ((EEPROM_SIZE - EEPROM_IMG_START)/EEPROM_IMG_EACH)

#include "../boardCommon.h"

#endif
