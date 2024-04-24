#ifndef _BOARD_COMMON_H_
#define _BOARD_COMMON_H_

#include <stdint.h>

#define SET_EPD_BS1(x)     do { __asm__("nop"); P0_0 = x; __asm__("nop"); } while(0)
#define SET_EPD_nCS(x)     do { __asm__("nop"); P1_1 = x; __asm__("nop"); } while(0)
#define SET_EPD_nCS1(x)    do { __asm__("nop"); P0_2 = x; __asm__("nop"); } while(0)
#define EPD_BUSY()         (P1_0)
#define SET_EPD_nRST(x)    do { __asm__("nop"); P1_2 = x; __asm__("nop"); } while(0)
#define SET_EPD_nENABLE(x) do { __asm__("nop"); P0_6 = x; __asm__("nop"); } while(0)
#define SET_EPD_DAT_CMD(x) do { __asm__("nop"); P0_7 = x; __asm__("nop"); } while(0)
#define SET_EEPROM_CS(x)   do { __asm__("nop"); P2_0 = x; __asm__("nop"); } while(0)

// P0 bits
#define P0_EPD_BS1         0x01  // P0.0
#define P0_TP6             0x02
#define P0_EPD_nCS1        0x04  // some boards needing two EPD cs
#define P0_EEPROM_CLK      0x08
#define P0_EEPROM_MOSI     0x10
#define P0_EEPROM_MISO     0x20
#define P0_EPD_nENABLE     0x40
#define P0_EPD_D_nCMD      0x80

// P1 bits
#define P1_EPD_BUSY        0x01  // P1.0
#define P1_EPD_nCS0        0x02
#define P1_EPD_nRESET      0x04
#define P1_EPD_SCK         0x08
#define P1_BIT_4           0x10  // unknown usage, n/c on some boards
#define P1_EPD_DI          0x20
#define P1_SERIAL_OUT      0x40
#define P1_SERIAL_IN       0x80

// P2 bits
#define P2_EEPROM_nCS      0x01  // P2.0
#define P2_DBG_DAT         0x02
#define P2_DBG_CLK         0x04
#define P2_XOSC32_Q1       0x08
#define P2_XOSC32_Q2       0x10

extern uint8_t __xdata mSelfMac[];
extern char __xdata gMacString[17];

extern __xdata __at (0xfda2) uint8_t gTempBuf320[320];

#pragma callee_saves screenPrvWaitByteSent
static inline void screenPrvWaitByteSent(void)
{
   while (U0CSR & 0x01);
}

#pragma callee_saves einkSelect
static inline void einkSelect(void)
{
   P1 &= (uint8_t) ~P1_EPD_nCS0;
   __asm__("nop");   //60ns between select and anything else as per spec. at our clock speed that is less than a single cycle, so delay a cycle
}

#pragma callee_saves einkSelect1
static inline void einkSelect1(void)
{
   P0 &= (uint8_t) ~P0_EPD_nCS1;
   __asm__("nop");   //60ns between select and anything else as per spec. at our clock speed that is less than a single cycle, so delay a cycle
}

#pragma callee_saves einkDeselect
static inline void einkDeselect(void)
{
   screenPrvWaitByteSent();
   __asm__("nop");   //20ns between select and anything else as per spec. at our clock speed that is less than a single cycle, so delay a cycle
   P1 |= P1_EPD_nCS0;
   __asm__("nop");   //40ns between deselect select and reselect as per spec. at our clock speed that is less than a single cycle, so delay a cycle
}

#pragma callee_saves einkDeselect1
static inline void einkDeselect1(void)
{
   screenPrvWaitByteSent();
   __asm__("nop");   //20ns between select and anything else as per spec. at our clock speed that is less than a single cycle, so delay a cycle
   P0 |= P0_EPD_nCS1;
   __asm__("nop");   //40ns between deselect select and reselect as per spec. at our clock speed that is less than a single cycle, so delay a cycle
}

#pragma callee_saves powerPortsDownForSleep
void powerPortsDownForSleep(void);

//early - before most things
#pragma callee_saves boardInitStage2
void boardInitStage2(void);

void PortInit(void);

//late, after eeprom
void InitBcastFrame(void);
void UpdateBcastFrame(void);

//some sanity checks
#include "eeprom.h"


#if !EEPROM_SETTINGS_AREA_START
   #error "settings cannot be at address 0"
#endif

#if (EEPROM_SETTINGS_AREA_LEN % EEPROM_ERZ_SECTOR_SZ) != 0
   #error "settings area must be an integer number of eeprom blocks"
#endif

#if (EEPROM_SETTINGS_AREA_START % EEPROM_ERZ_SECTOR_SZ) != 0
   #error "settings must begin at an integer number of eeprom blocks"
#endif

#if (EEPROM_IMG_EACH % EEPROM_ERZ_SECTOR_SZ) != 0
   #error "each image must be an integer number of eeprom blocks"
#endif

#if (EEPROM_IMG_START % EEPROM_ERZ_SECTOR_SZ) != 0
   #error "images must begin at an integer number of eeprom blocks"
#endif

#if (EEPROM_UPDATE_AREA_LEN % EEPROM_ERZ_SECTOR_SZ) != 0
   #error "update must be an integer number of eeprom blocks"
#endif

#if (EEPROM_UPDATA_AREA_START % EEPROM_ERZ_SECTOR_SZ) != 0
   #error "images must begin at an integer number of eeprom blocks"
#endif

#endif
