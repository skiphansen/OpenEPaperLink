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

// eeprom map
#define EEPROM_SETTINGS_AREA_START  (0x08000UL)
#define EEPROM_SETTINGS_AREA_LEN    (0x04000UL)
//some free space here
#define EEPROM_UPDATA_AREA_START    (0x10000UL)
#define EEPROM_UPDATE_AREA_LEN      (0x09000UL)
#define EEPROM_IMG_START            (0x19000UL)
#define EEPROM_IMG_EACH             (0x17000UL)
//till end of eeprom really. do not put anything after - it will be erased at pairing time!!!
#define EEPROM_PROGRESS_BYTES       (192)

#define IMAGE_SLOTS                 ((EEPROM_SIZE - EEPROM_IMG_START)/EEPROM_IMG_EACH)

#include "../boardCommon.h"

#endif
