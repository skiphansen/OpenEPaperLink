#include <stdlib.h>

#include "asmUtil.h"
#include "screen.h"
#include "eeprom.h"
#include "printf.h"
#include "board.h"
#include "cpu.h"
#include "settings.h"
#include "powermgt.h"
#include "logging.h"

// NB: be VERY careful about doing any logging in this file,
// the UART and SPI flash port are the SAME so it's very
// easy to fuck up the flash routines you are trying to
// debug by adding logging!  
// 
// Specifically don't log while the CS is active !
#undef EEPROM_LOG
#if 1
   #define EEPROM_LOG(format, ... ) pr(format,## __VA_ARGS__)
#else
   #define EEPROM_LOG(format, ... )
#endif

#ifdef DEBUGEEPROM
void LogRDSR(void);
#else
#define LogRDSR()
#endif

#ifndef SFDP_DISABLED
static uint32_t __xdata mEepromSize;
static uint8_t __xdata mOpcodeErz4K = 0, mOpcodeErz32K = 0, mOpcodeErz64K = 0;
#else
#define mEepromSize     EEPROM_SIZE
#define mOpcodeErz4K    EEPROM_4K_ERASE_OPCODE
#define mOpcodeErz32K   EEPROM_32K_ERASE_OPCODE
#define mOpcodeErz64K   EEPROM_64K_ERASE_OPCODE
#endif

#ifndef SFDP_DISABLED
uint32_t eepromGetSize(void) 
{
   return mEepromSize;
}
#endif

void eepromReadStart(uint32_t addr) __reentrant 
{
   eepromPrvSelect();
   eepromByte(0x03);
   eepromByte(addr >> 16);
   eepromByte(addr >> 8);
   eepromByte(addr & 0xff);
}

void eepromRead(uint32_t addr, void __xdata *dstP, uint16_t len) __reentrant 
{
   uint8_t __xdata *dst = (uint8_t __xdata *)dstP;
   eepromPrvSelect();
   eepromByte(0x03);
   eepromByte(addr >> 16);
   eepromByte(addr >> 8);
   eepromByte(addr & 0xff);

   while(len--) {
      *dst++ = eepromByte(0);
   }
   eepromPrvDeselect();
}

static void eepromPrvSimpleCmd(uint8_t cmd) 
{
   eepromPrvSelect();
   eepromByte(cmd);
   eepromPrvDeselect();
}

// Wait for any write operation to complete
static void eepromPrvBusyWait(void) 
{
   eepromPrvSelect();
   eepromByte(0x05);
   while(eepromByte(0x00) & 1);
   eepromPrvDeselect();
}

static void eepromWriteLL(uint32_t addr, const void __xdata *srcP, uint16_t len) 
{
   const uint8_t __xdata *src = (const uint8_t __xdata *)srcP;

   eepromPrvBusyWait();
   eepromPrvSimpleCmd(0x06);
   eepromPrvSelect();
   eepromByte(0x02);
   eepromByte(addr >> 16);
   eepromByte(addr >> 8);
   eepromByte(addr & 0xff);

   while(len--) {
      eepromByte(*src++);
   }
   eepromPrvDeselect();
}

#ifndef DEBUGEEPROM
void eepromDeepPowerDown(void) 
{
   eepromPrvBusyWait();
   eepromPrvSimpleCmd(0xb9);
   gEEPROM_PoweredUp = false;
}
#else
// Debug version
void LogRDSR()
{
   uint8_t Status;

   eepromPrvSelect();
   eepromByte(0x05);
   Status = eepromByte(0x00);
   eepromPrvDeselect();
   EEPROM_LOG("RDSR 0x%x\n",Status);
}

void eepromDeepPowerDown(void) 
{
   uint8_t Status;

   eepromPrvSelect();
   eepromByte(0x05);
   Status = eepromByte(0x00);
   eepromPrvDeselect();
   EEPROM_LOG("RDSR 0x%x\n",Status);
   if(Status & 1) {
      EEPROM_LOG("waiting...");
      eepromPrvBusyWait();
   }
   eepromPrvSimpleCmd(0xb9);
   if(Status & 1) {
      EEPROM_LOG("\n");
   }
   gEEPROM_PoweredUp = false;
   LOG_CONFIG("EEPROM PowerDown");
}
#endif

void eepromWakeFromPowerdown(void) 
{
   eepromPrvSimpleCmd(0xab);
// In the following a value of 255 for B delays for about 49 microseconds
// tRES1 is 8.8 us for the MX25V8006E so
// lets use 52 for about 10 microseconds
   __asm__(
      "  mov   B, #52         \n"
      "00004$:          \n"
      "  djnz  B, 00004$      \n"
   );
   gEEPROM_PoweredUp = true;
   LogRDSR();
   LOG_CONFIG("EEPROM wake");
}

#ifndef SFDP_DISABLED

#pragma callee_saves eepromPrvSfdpRead
static void eepromPrvSfdpRead(uint16_t ofst, uint8_t __xdata *dst, uint8_t len) 
{
   eepromPrvSelect();
   eepromByte(0x5a);  // cmd
   eepromByte(0);     // addr
   eepromByte(ofst >> 8);
   eepromByte(ofst);
   eepromByte(0x00);  // dummy
   while(len--) {
      *dst++ = eepromByte(0);
   }
   eepromPrvDeselect();
}

__bit eepromInit(void) 
{
   uint8_t __xdata buf[8];
   uint8_t i, nParamHdrs;
   __bit Ret = false;

// process SFDP
   eepromPrvSfdpRead(0, buf, 8);
   do {
      if(buf[0] != 0x53 || buf[1] != 0x46 || buf[2] != 0x44 || buf[3] != 0x50 
         || buf[7] != 0xff) {
         EEPROM_LOG("SFDP: header not found\n");
         break;
      }

      if(buf[5] != 0x01) {
         EEPROM_LOG("SFDP: version wrong: %u.%d\n", buf[5], buf[4]);
         break;
      }

      nParamHdrs = buf[6];
      if(nParamHdrs == 0xff)  // that case is very unlikely and we just do not care
         nParamHdrs--;

      // now we need to find the JEDEC parameter table header
      for(i = 0; i <= nParamHdrs; i++) {
         eepromPrvSfdpRead(mathPrvMul8x8(i, 8) + 8, buf, 8);
         if(buf[0] == 0x00 && buf[2] == 0x01 && buf[3] >= 9) {
            uint8_t j;

            eepromPrvSfdpRead(*(uint16_t __xdata *)(buf + 4),gTempBuf320,9 * 4);
            if((gTempBuf320[0] & 3) != 1) {
               EEPROM_LOG("SFDP: no 4K ERZ\n");
               break;
            }
            if(!(gTempBuf320[0] & 0x04)) {
               EEPROM_LOG("SFDP: no large write buf\n");
               break;
            }
            if((gTempBuf320[2] & 0x06)) {
               EEPROM_LOG("SFDP: addr.len != 3\n");
               break;
            }

            if(!gTempBuf320[1] || gTempBuf320[1] == 0xff) {
               EEPROM_LOG("SFDP: 4K ERZ opcode invalid\n");
               break;
            }
            mOpcodeErz4K = gTempBuf320[1];

            if(gTempBuf320[7] & 0x80) {
               EEPROM_LOG("SFDP: device too big\n");
               break;
            }
            uint8_t __xdata t;

            if(t = gTempBuf320[7])
               mEepromSize = 0x00200000UL;
            else if(t = gTempBuf320[6])
               mEepromSize = 0x00002000UL;
            else if(t = gTempBuf320[5])
               mEepromSize = 0x00000020UL;
            else {
               EEPROM_LOG("SFDP: device so small?!\n");
               break;
            }

            while(t) {
               mEepromSize <<= 1;
               t >>= 1;
            }

            // get erase opcodes
            for(j = 0x1c; j < 0x24; j += 2) {
               uint8_t instr = gTempBuf320[j + 1];

               if(!instr || instr == 0xff)
                  continue;

               switch(gTempBuf320[j]) {
                  case 0x0c:
                     if(mOpcodeErz4K != instr) {
                        EEPROM_LOG("4K ERZ opcode disagreement\n");
                        return false;
                     }
                     break;

                  case 0x0f:  // 32K erase
                     mOpcodeErz32K = instr;
                     break;

                  case 0x10:  // 64K erase
                     mOpcodeErz64K = instr;
                     break;
               }
            }

            EEPROM_LOG("EEPROM accepted\n");
            EEPROM_LOG(" ERZ opcodes: \n");
            if(mOpcodeErz4K) {
               EEPROM_LOG(" 4K:  %02xh\n", mOpcodeErz4K);
            }
            if(mOpcodeErz32K) {
               EEPROM_LOG(" 32K: %02xh\n", mOpcodeErz32K);
            }
            if(mOpcodeErz64K) {
               EEPROM_LOG(" 64K: %02xh\n", mOpcodeErz64K);
            }
            EEPROM_LOG(" Size: 0x%*08lx\n", (uint16_t)&mEepromSize);
            Ret = true;
         }
      }
   } while(false);
   if(!Ret) {
      EEPROM_LOG("SFDP: no JEDEC table of expected version found\n");
   }
   return Ret;
}
#endif

void eepromWrite(uint32_t addr, const void __xdata *srcP, uint16_t len) __reentrant 
{
   const uint8_t __xdata *src = (const uint8_t __xdata *)srcP;

   while(len) {
      uint16_t lenNow = EEPROM_WRITE_PAGE_SZ - (addr & (EEPROM_WRITE_PAGE_SZ - 1));

      if(lenNow > len) {
         lenNow = len;
      }
      eepromWriteLL(addr, src, lenNow);
      addr += lenNow;
      src += lenNow;
      len -= lenNow;
   }
}

bool eepromErase(uint32_t addr, uint16_t nSec) __reentrant 
{
   uint8_t now;

   if(((uint16_t)addr) & 0x0fff) {
      return false;
   }

   for(; nSec; nSec -= now) {
      eepromPrvSimpleCmd(0x06);
      eepromPrvSelect();

      if(nSec >= 16 && !(uint16_t)addr && mOpcodeErz64K) {
      // erase 64K
         eepromByte(mOpcodeErz64K);
         now = 16;
      }
      else if(nSec >= 8 && !(((uint16_t)addr) & 0x7fff) && mOpcodeErz32K) {  
      // erase 32K
         eepromByte(mOpcodeErz32K);
         now = 8;
      }
      else {
      // erase 4K
         eepromByte(mOpcodeErz4K);
         now = 1;
      }

      eepromByte(addr >> 16);
      eepromByte(addr >> 8);
      eepromByte(addr);
      eepromPrvDeselect();
      eepromPrvBusyWait();
      addr += mathPrvMul16x8(EEPROM_ERZ_SECTOR_SZ, now);
   }
   return true;
}

