#ifndef _BOARD_H_
#define _BOARD_H_

#include "u1.h"

#define CHROMA74
#define HW_TYPE                     0x80

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

// Generic EEPROM routines that use SFDP for configuration are GREAT **IF** 
// you can spare the code space, but we can't !!!
// Disable SFDP to save 1538 bytes.
#define SFDP_DISABLED

/* eeprom map
 
EEPROM size: 0x10.0000 / 1024k / 1 Megabyte 
Image size:  (640 x 384 x 3 bits/pixel) / 8 bit / byte = 92160 bytes 
rounded up to erase bundary = 94,208 / 0x1.7000 
Image slots: 10 
 
| Start Adr | End Adr  | Size | Usage |
|     -     |    -     |   -  |   -   |
| 0x0.0000  | 0x0.7fff |  32k | Factory NVRAM
| 0x0.8000  | 0x0.9fff |   8k | Chroma Settings
| 0x0.a000  | 0x0.bfff |   8k | OEPL Settings
| 0x0.c000  | 0x0.ffff |   8k | unused
| 0x1.0000  | 0x1.8fff |  36k | OTA FW update
| 0x1.9000  | 0x4.6fff |  92k | Image slot 0
| 0x4.7000  | 0x5.dfff |  92k | Image slot 1
| 0x5.e000  | 0x7.4fff |  92k | Image slot 2
| 0x7.5000  | 0x8.bfff |  92k | Image slot 3
| 0x8.c000  | 0xa.2fff |  92k | Image slot 4
| 0xa.3000  | 0xb.bfff |  92k | Image slot 5
| 0xb.a000  | 0xe.7fff |  92k | Image slot 6
| 0xe.8000  | 0xd.0fff |  92k | Image slot 7
| 0xd.1000  | 0xe.7fff |  92k | Image slot 8
| 0xe.8000  | 0xf.efff |  92k | Image slot 9
| 0xf.f000  | 0xf.ffff |   4k | unused

*/
#define EEPROM_SETTINGS_AREA_START  (0x08000UL)
#define EEPROM_SETTINGS_AREA_LEN    (0x04000UL)
//some free space here
#define EEPROM_UPDATA_AREA_START    (0x10000UL)
#define EEPROM_UPDATE_AREA_LEN      (0x09000UL)
#define EEPROM_IMG_START            (0x19000UL)
#define EEPROM_IMG_EACH             (0x17000UL)
#define EEPROM_IMG_SECTORS          (EEPROM_IMG_EACH / EEPROM_ERZ_SECTOR_SZ)
//till end of eeprom really. do not put anything after - it will be erased at pairing time!!!
#define EEPROM_PROGRESS_BYTES       (192)

#define IMAGE_SLOTS                 ((EEPROM_SIZE - EEPROM_IMG_START)/EEPROM_IMG_EACH)

#include "../boardCommon.h"

#endif
