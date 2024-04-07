#include "sleep.h"
#include "cpu.h"
#include "asmUtil.h"
#include "wdt.h"
#include "printf.h"
#include "settings.h"
#include "logging.h"


void DumpStack(void);

void WOR_ISR(void) __interrupt (5)
{
// Clear Sleep Timer CPU interrupt flag (IRCON.STIF = 0)
   IRCON &= ~IRCON_STIE;

   // Clear Sleep Timer Module Interrupt Flag (WORIRQ.EVENT0_FLAG = 0)
   WORIRQ &= ~WORIRQ_EVENT0_FLAG;

// Clear the [SLEEP.MODE] bits, because an interrupt can also occur
// before the SoC has actually entered Power Mode 2.
   SLEEP &= ~SLEEP_MODE;
}

// assumes you left only one int enabled!
void sleepTillInt(void) 
{
   PCON |= 0x01; // Enter PM2
}

#define SET_PM0   (SLEEP_MODE_PM0 | SLEEP_OSC_PD)
#define SET_PM2   (SLEEP_MODE_PM2 | SLEEP_OSC_PD)
#define SET_PM3   (SLEEP_MODE_PM3 | SLEEP_OSC_PD)
// Defining this here rather than locally in sleepForMsec saves 25 bytes of flash
uint8_t static __xdata PM2_BUF[7] = {
   SET_PM2,SET_PM2,SET_PM2,SET_PM2,SET_PM2,SET_PM2,SET_PM0
};

uint8_t static __xdata PM3_BUF[7] = {
   SET_PM3,SET_PM3,SET_PM3,SET_PM3,SET_PM3,SET_PM3,SET_PM0
};

void sleepForMsec(uint32_t units)
{
   struct DmaDescr __xdata dmaDesc = {
      .dstAddrHi = 0xdf,
      .dstAddrLo = 0xbe,   //SLEEP REG
      .vlen = 0,           //tranfer given number of bytes
      .lenLo = 7,          //exactly 7 bytes
      .trig = 0,           //manual trigger
      .tmode = 1,          //block xfer
      .wordSize = 0,       //byte-by-byte xfer
      .priority = 2,       //higher-than-cpu prio
      .irqmask = 0,        //most definitely do NOT cause an irq
      .dstinc = 0,         //do not increment DST (write SLEEP reg repeatedly)
      .srcinc = 1,         //increment source
   };
   uint8_t forever;
      
// our units are 31.25msec, caller used msec
   units = mathPrvDiv32x16(units,31);

   if(units == 0) {
      forever = 1;
      dmaDesc.srcAddrHi = ((uint16_t)PM3_BUF) >> 8;
      dmaDesc.srcAddrLo = (uint8_t)PM3_BUF;
   }
   else {
      forever = 0;
      dmaDesc.srcAddrHi = ((uint16_t)PM2_BUF) >> 8;
      dmaDesc.srcAddrLo = (uint8_t)PM2_BUF;
   }
// disable non-wake irqs
   IRCON &= (uint8_t)~IRCON_STIE;
// Clear Sleep Timer Module Interrupt Flag (WORIRQ.EVENT0_FLAG = 0)
   WORIRQ &= ~WORIRQ_EVENT0_FLAG;

// Enable Sleep Timer Module Interrupt (WORIRQ.EVENT0_MASK = 1)
   WORIRQ |= WORIRQ_EVENT0_MASK;
   IEN0 = IEN0_STIE | IEN0_EA;
   uint8_t tmp;

   while(forever || units != 0) {
      uint16_t now;
   // Switch system clock source to HS RCOSC and max CPU speed:
      SLEEP &= ~SLEEP_OSC_PD;
      while(!(SLEEP & SLEEP_HFRC_S));
   // we are now running at 13MHz from the RC osc
      CLKCON = (CLKCON & CLKCON_TICKSPD_MASK) | CLKCON_OSC | CLKSPD_DIV_2;
      while(!(CLKCON & CLKCON_OSC));
      SLEEP |= SLEEP_OSC_PD;

   // Set LS XOSC as the Sleep Timer clock source (CLKCON.OSC32 = 0)
      CLKCON &= ~CLKCON_OSC32;
   
      if(units > 0xfff0L) {
    // we can sleep for up to 33 mins effectively this way
         now = 0xfff0;
         units -= 0xfff0L;
      }
      else {
         now = units;
         units = 0;
      }
      now += 2;   //counter starts at 2 due to how we init it
      
   // enable WOR irq
      WORIRQ = forever ? 0x00 : 0x10;
      IRCON = 0;
      
      DMAARM |= DMAARM_ABORT | DMAARM0;
      DMA0CFG = (uint16_t)&dmaDesc;
      DMAARM = DMAARM0;
      
   // Reset Sleep Timer. 
      WORCTRL = WORCTL_WOR_RESET;
      while(WORTIME0);
      
      WORCTRL = WORCTL_WOR_RES1;    //div2^10 (approx 31.25msec)
      WOREVT1 = now >> 8;
      WOREVT0 = now & 0xff;
      
// ** Beginning of timing critical code ***
// The following is from the CC1110Fx/CC1111Fx ERRATA NOTE (swrz022c)

   // Wait until a positive 32 kHz edge
      tmp = WORTIME0;
      while(tmp == WORTIME0); 
      
      MEMCTR |= MEMCTR_CACHD;
      if(forever) {
         forever = 0;   // exit loop if entry to PM3 fails !
         SLEEP = SET_PM3;
      }
      else {
         SLEEP = SET_PM2;
      }
      
   // Enter power mode as described in chapter "Power Management Control"
   // in the data sheet. Make sure DMA channel 0 is triggered just before
   // setting [PCON.IDLE].
      __asm__(
         "  nop            \n"
         "  nop            \n"
         "  nop            \n"
      );
      if(SLEEP & 0x03) {
         __asm__("MOV 0xD7,#0x01"); // DMAREQ = 0x01;
         __asm__("NOP");            // Needed to perfectly align the DMA transfer.
         __asm__("ORL 0x87,#0x01"); // PCON |= 0x01 -- Now in PM3;
         __asm__("NOP");            // First call when awake
      }
// ** End of timing critical code **

   // Enable Flash Cache.
      MEMCTR &= ~MEMCTR_CACHD;

   // Wait until HS RCOSC is stable
      while(!(SLEEP & SLEEP_HFRC_S));

#if 0
   // Set LS XOSC as the clock oscillator for the Sleep Timer (CLKCON.OSC32 = 0)
      CLKCON &= ~CLKCON_OSC32;
#else
      SLEEP = 0;              //SLEEP.MODE = 0 to use HFXO
      while (!(SLEEP & 0x20));   //wait for HFRC to stabilize
      CLKCON = 0x79;          //high speed RC osc, timer clock is 203.125KHz, 13MHz system clock
      while (!(SLEEP & 0x40));   //wait for HFXO to stabilize

      uint8_t i, j;
      //we need to delay more (chip erratum)
      for (i = 0; i != 128; i++) for (j = 0; j != 128; j++) __asm__ ("nop");

      CLKCON = 0x39;          //switch to HFXO
      while (CLKCON & 0x40);     //wait for the switch
      CLKCON = 0x38;          //go to 26MHz system and timer speed,  timer clock is Fosc / 128 = 203.125KHz
      SLEEP = SLEEP_OSC_PD;   // power down the unused (HFRC oscillator)
#endif
   }

}

