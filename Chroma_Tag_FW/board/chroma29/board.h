#ifndef _BOARD_H_
#define _BOARD_H_

#include "soc.h"
#include "u1.h"

#define CHROMA29
#define BOARD_NAME "Chroma29"
#define HW_TYPE    0x82

#ifndef DISABLE_BARCODES
#define DISABLE_BARCODES   // not supported on Chroma29 (yet ?)
#endif

extern const char * __code gBoardName;

//eeprom spi
#define EEPROM_SIZE              0x0020000L
#define EEPROM_4K_ERASE_OPCODE   0x20
#define EEPROM_32K_ERASE_OPCODE  0
#define EEPROM_64K_ERASE_OPCODE  0
#define eepromByte               u1byte
#define eepromPrvSelect()     do { __asm__("nop"); P2_0 = 0; __asm__("nop"); } while(0)
#define eepromPrvDeselect()      do { __asm__("nop"); P2_0 = 1; __asm__("nop"); } while(0)

//debug uart (enable only when needed, on some boards it inhibits eeprom access)
#define dbgUartOn()                 u1setUartMode()
#define dbgUartOff()                u1setEepromMode()
#define dbgUartByte                 u1byte

/* eeprom map
 
EEPROM size: 0x2.0000 / 128k
 
Image size:  (128 x 296 x 3 bits/pixel) / 8 bit / byte = 14,208 bytes 
rounded up to erase boundary = 16384 / 0x.4000 / 16k
Image slots: 4
 
| Start Adr | End Adr  | Size | Usage |
|     -     |    -     |   -  |   -   |
| 0x0.0000  | 0x0.1fff |   8k | Factory NVRAM
| 0x0.2000  | 0x0.2fff |   4k | Factory LUT ???
| 0x0.3000  | 0x0.5fff |  12k | unused
| 0x0.6000  | 0x0.6fff |   4k | OEPL settings
| 0x0.7000  | 0x0.ffff |  36k | OTA
| 0x1.0000  | 0x1.3fff |  16k | slot 0 
| 0x1.4000  | 0x1.7fff |  16k | slot 1 
| 0x1.8000  | 0x1.dfff |  16k | slot 2 
| 0x1.c000  | 0x1.ffff |  16k | slot 3 

*/
#define EEPROM_FACTORY_NVRAM_ADR    (0x00000UL)
#define EEPROM_FACTORY_NVRAM_LEN    (EEPROM_ERZ_SECTOR_SZ * 2)

#define EEPROM_SETTINGS_AREA_START  (0x06000UL)
#define EEPROM_SETTINGS_AREA_LEN    (EEPROM_ERZ_SECTOR_SZ)

#define EEPROM_UPDATA_AREA_START    (0x07000UL)
#define EEPROM_UPDATE_AREA_LEN      (0x09000UL)

#define EEPROM_IMG_START            (0x10000UL)
#define EEPROM_IMG_EACH             (0x04000UL)
#define EEPROM_IMG_SECTORS          (EEPROM_IMG_EACH / EEPROM_ERZ_SECTOR_SZ)
#define IMAGE_SLOTS                 ((EEPROM_SIZE - EEPROM_IMG_START)/EEPROM_IMG_EACH)

#include "../boardCommon.h"

#endif
