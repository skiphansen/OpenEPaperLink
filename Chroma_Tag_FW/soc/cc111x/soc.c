#include <stdbool.h>
#include "asmUtil.h"
#include "eeprom.h"
#include "printf.h"
#include "screen.h"
#include "board.h"
#include "adc.h"
#include "powermgt.h"
#include "settings.h"
#include "logging.h"

void boardGetOwnMac();

char __xdata gMacString[17];
__bit gEEpromFailure;

#pragma callee_saves prvReadSetting
//returns token length if found, copies at most maxLen. returns -1 if not found
static int8_t prvReadSetting(uint8_t type, uint8_t __xdata *dst, uint8_t maxLen) 
{
   static const uint8_t __xdata magicNum[4] = {0x56, 0x12, 0x09, 0x85};
   uint8_t __xdata tmpBuf[4];
   uint8_t pg, gotLen = 0;
   __bit found = false;
   
   for (pg = 0; pg < 10; pg++) {
      uint16_t __pdata addr = mathPrvMul16x8(EEPROM_ERZ_SECTOR_SZ, pg);
      uint16_t __pdata ofst = 4;
      
      eepromRead(addr, tmpBuf, 4);
      if (xMemEqual(tmpBuf, magicNum, 4)) {
         while (ofst < EEPROM_ERZ_SECTOR_SZ) {
         // first byte is type, (0xff for done), second is length
            eepromRead(addr + ofst, tmpBuf, 2);
            if (tmpBuf[0] == 0xff) {
               break;
            }
            
            if (tmpBuf[0] == type && tmpBuf[1] >= 2) {
               uint8_t copyLen = gotLen = tmpBuf[1] - 2;
               if (copyLen > maxLen) {
                  copyLen = maxLen;
               }
               eepromRead(2 + addr + ofst, dst, copyLen);
               found = true;
            }
            ofst += tmpBuf[1];
         }
      }
   }
   return found ? gotLen : -1;
}

void boardInitStage2(void)
{
   powerUp(INIT_EEPROM);

#ifndef SFDP_DISABLED
   if(!eepromInit()) {
      pr("failed to init eeprom\n");
      while(1);
   }
#endif
#ifdef SCREEN_EXPECTS_VCOM
   if(prvReadSetting(0x23,&mScreenVcom,1) < 0) {
      NV_DATA_LOG("failed to get VCOM\n");
#if BUILD == chroma42r
      mScreenVcom = 8;     // a sane value .8v from one example panel
#else
      mScreenVcom = 0x28;  //a sane value
#endif
   }
   else {
      NV_DATA_LOG("VCOM: 0x%02x\n", mScreenVcom);
   }
#endif
   
   if(prvReadSetting(0x12,&mAdcSlope,2) < 0) {
      NV_DATA_LOG("failed to get ADC slope\n");
      mAdcSlope = 2470; //a sane value
   }
   else {
      NV_DATA_LOG("ADC slope %d\n",mAdcSlope);
   }

   if(prvReadSetting(0x09,&mAdcIntercept,2) < 0) {
      NV_DATA_LOG("failed to get ADC intercept\n");
      mAdcIntercept = 755; //a sane value
   }
   else {
      NV_DATA_LOG("ADC mAdcIntercept %d\n",mAdcIntercept);
   }

   boardGetOwnMac();
   powerDown(INIT_EEPROM);
// On some board (Chroma29 for example) we don't know how to deal
// with the display's GPIO until we know the SN.
   screenInitGPIO(false);
   irqsOn();
}

// JM 10339094 B
void boardGetOwnMac()
{
// Set mSelfMac from the device SN stored in the factory "NVRAM".
// Note: apparently some boards have a 6 character SN and some have a 7.
// Only the first 6 charcters are used.
   xMemSet(gTempBuf320,0x00,sizeof(gTempBuf320));
   
   if(prvReadSetting(0x2a,gTempBuf320,7) < 0 && prvReadSetting(1,gTempBuf320,6) < 0) {
      gEEpromFailure = true;
      return;
   }

   mSelfMac[7] = 0x44;
   mSelfMac[6] = 0x67;
   mSelfMac[3] = gTempBuf320[2];
   mSelfMac[2] = gTempBuf320[3];
   mSelfMac[1] = gTempBuf320[4];
   mSelfMac[0] = gTempBuf320[5];
   gTempBuf320[2] = 0;
   LOGA("SN %s%02x%02x",gTempBuf320,mSelfMac[3],mSelfMac[2]);
   LOGA("%02x%02x\n",mSelfMac[1],mSelfMac[0]);
#if 0
// Dimtry's original code ignored the first two characters of the SN, but
// they seem useful to me.
   mSelfMac[5] = 0x4b;
   mSelfMac[4] = 0x7a;
#else
// Since the first 2 characters are known to be upper case ASCII subtract
// 'A' from the value to convert it from a 7 bit value to a 5 bit value.
// 
// This allows us to use the extra 3 bits to convey something else like the
// hardware variation.
// 
// i.e. Multiple incompatible hardware variations that need different FW 
// images, but otherwise are compatible can share a single HWID and the 
// AP but the AP can parse the "extra" bits in the MAC address to 
// select the correct OTA image.

   mSelfMac[5] = gTempBuf320[0] - 'A';
   mSelfMac[4] = gTempBuf320[1] - 'A';
#endif
   spr(gMacString,"%02X%02X",mSelfMac[7],mSelfMac[6]);
   spr(gMacString+4,"%02X%02X",mSelfMac[5],mSelfMac[4]);
   spr(gMacString+8,"%02X%02X",mSelfMac[3],mSelfMac[2]);
   spr(gMacString+12,"%02X%02X",mSelfMac[1],mSelfMac[0]);
}


//copied to ram, after update has been verified, interrupts have been disabled, 
//and eepromReadStart() has been called.
//Does not return (resets using WDT)
//this func wraps the update code and returns its address (in DPTR), len in B
static uint32_t prvUpdateApplierGet(void) __naked
{
   __asm__(
      "  mov   DPTR, #00098$        \n"
      "  mov   A, #00099$        \n"
      "  clr   C                 \n"
      "  subb  A, DPL            \n"
      "  mov   B, A              \n"
      "  ret                     \n"
      
      ///actual updater code
      "00098$:                \n"
   
      "  mov   B, #32            \n"
      //erase all flash
      "  clr   _FADDRH           \n"
      "  clr   _FADDRL           \n"   
      "00001$:                \n"
      "  orl   _FCTL, #0x01         \n"
      "  nop                     \n"   //as per datasheet
      "00002$:                \n"
      "  mov   A, _FCTL          \n"
      "  jb    A.7, 00002$       \n"
      "  inc   _FADDRH           \n"
      "  inc   _FADDRH           \n"
      "  djnz  B, 00001$            \n"
      
      //write all 32K
      //due to the 40 usec timeout, we wait each time to avoid it
      "  mov   DPTR, #0       \n"
      
      "00003$:                \n"
      "  mov   _FADDRH, DPH         \n"
      "  mov   _FADDRL, DPL         \n"
      "  inc   DPTR              \n"
      "  mov   _FWT, #0x22       \n"
      
      //get two bytes
   
      "   mov   B, #2               \n"
      
      "00090$:                \n"
      "  mov   _U1DBUF, #0x00    \n"
      
      "00091$:                \n"
      "  mov   A, _U1CSR            \n"
      "  jnb   A.1, 00091$       \n"
      
      "  anl   _U1CSR, #~0x02    \n"
      
      "00092$:                \n"
      "  mov   A, _U1CSR            \n"
      "  jb    A.0, 00092$       \n"
      
      "  push  _U1DBUF           \n"
      "  djnz  B, 00090$            \n"
      
      //write two bytes
      "  orl   _FCTL, #0x02         \n"
      
      //wait for fwbusy to go low
      "00012$:                \n"
      "  mov   A, _FCTL          \n"
      "  jb    A.6, 00012$       \n"
      
      "  pop   A                 \n"
      "  pop   _FWDATA           \n"
      "  mov   _FWDATA, A        \n"
      
      //wait for swbusy to be low
      "00004$:                \n"
      "  mov   A, _FCTL          \n"
      "  jb    A.6, 00004$       \n"
      
      "  anl   _FCTL, #~0x02        \n"
      
      //wait for busy to be low
      "00005$:                \n"
      "  mov   A, _FCTL          \n"
      "  jb    A.7, 00005$       \n"
      
      //loop for next two bytes
      "  mov   A, DPH            \n"
      "  cjne  A, #0x40, 00003$     \n"
      
      //done
   
      //WDT reset
      "  mov   _WDCTL, #0x0b        \n"
      "00007$:                \n"
      "  sjmp  00007$            \n"
      
      "00099$:                \n"
   );
}

void selfUpdate(void)
{
   uint32_t updaterInfo = prvUpdateApplierGet();
   
   xMemCopyShort(mScreenRow, (void __xdata*)(uint16_t)updaterInfo, updaterInfo >> 16);
         
   DMAARM = 0xff; //all DMA channels off
   IEN0 = 0;   //ints off
   
   MEMCTR = 3; //cache and prefetch off
   
   __asm__(
      "  mov dptr, #_mScreenRow     \n"
      "  clr a                \n"
      "  jmp @a+dptr             \n"
   );
}

void clockingAndIntsInit(void)
{
   uint8_t i, j;
   
   IEN0 = 0;
   IEN1 = 0;
   IEN2 = 0;

// enable cache and prefetch
   MEMCTR = 0; 
   
   SLEEP = 0;              //SLEEP.MODE = 0 to use HFXO
   while (!(SLEEP & SLEEP_HFRC_S));   //wait for HFRC to stabilize
// high speed RC osc, timer clock is 203.125KHz, 13MHz system clock
//   CLKCON = 0x79;          //high speed RC osc, timer clock is 203.125KHz, 13MHz system clock
   CLKCON = CLKCON_OSC | CLKCON_TICKSPD(7) | CLKCON_CLKSPD(1);
   while (!(SLEEP & SLEEP_XOSC_S));   //wait for HFXO to stabilize
   
   //we need to delay more (chip erratum)
   for (i = 0; i != 128; i++) for (j = 0; j != 128; j++) __asm__ ("nop");
   
// switch to HFXO
   CLKCON = CLKCON_CLKSPD(1) | CLKCON_TICKSPD(7);
   while(CLKCON & CLKCON_OSC);   //wait for the switch
// Select 26MHz system clock and 203.125KHz timer clock
   CLKCON = CLKCON_CLKSPD(0) | CLKCON_TICKSPD(7);

   SLEEP = SLEEP_OSC_PD;   //power down the unused (HFRC oscillator)
}

uint8_t rndGen8(void)
{
   ADCCON1 |= 4;
   while (ADCCON1 & 0x0c);
   return RNDH ^ RNDL;
}

uint32_t rndGen32(void) __naked
{
   __asm__ (
      //there simply is no way to get SDCC to generate this anywhere near as cleanly
      "  lcall  _rndGen8      \n"
      "  push   DPL        \n"
      "  lcall  _rndGen8      \n"
      "  push   DPL        \n"
      "  lcall  _rndGen8      \n"
      "  push   DPL        \n"
      "  lcall  _rndGen8      \n"
      "  pop    DPH        \n"
      "  pop    B       \n"
      "  pop    A       \n"
      "  ret               \n" 
   );
}

void rndSeed(uint8_t seedA, uint8_t seedB)
{
   RNDL = seedA;
   RNDL = seedB;
}

