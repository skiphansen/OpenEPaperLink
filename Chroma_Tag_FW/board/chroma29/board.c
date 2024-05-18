#include "../boardChroma.c"

#define P0_EPD_PINS (P0_EPD_BS1 | P0_EPD_D_nCMD)
#define P1_EPD_PINS (P1_EPD_DI | P1_EPD_SCK | P1_EPD_nRESET | P1_EPD_nCS0 | P1_EPD_BUSY)
#if 1
// 
void powerPortsDownForSleep(void)
{
   T1CTL = 0;  //timer off
   U1GCR = 0;  // Disable Rx

// Enable pullups on all P2 pins and select pullups (when enabled) on P0, P1
// (This shouldn't be necessary, this is the default)
   P2INP = 0;

// P1.1 is P1_EPD_nCS0 so we leave it as an output
   P1DIR = P1_EPD_nCS0;

   if(!(mSelfMac[3] & 0xf0)) {
   // JA0xxxxxxx devices do not implement EPD power control

   // set EPD_nCS0 high (it should already be, but make sure)
      P1_1 = 1;

   // Enable pullups on all pins except P1.0 and P1.1 which don't have 
      P0INP = 0;
      P1INP = 0;

   // Set all P0 pins to input mode
      P0DIR = 0;

   }
   else {
   // JA1xxxxxxx devices implement EPD power control
   
   // Disable pullups on pins connected to the EPD
      P0INP = P0_EPD_PINS;
      P1INP = P1_EPD_PINS;
   // Set all pins to input mode except the pins connected to the EPD
      P0DIR = P0_EPD_PINS;
      P1DIR = P1_EPD_PINS;
   // drive EPD pins low prevent back powering the display
      P0 &= ~P0_EPD_PINS;
      P1 &= ~P1_EPD_PINS;
   }

// Disconnect all perpherials from pins
   P0SEL = 0;
   P1SEL = 0;
   P2SEL = 0;
   COPY_CFG();
}
#else
void powerPortsDownForSleep(void)
{
   T1CTL = 0;  //timer off
   U1GCR = 0;  // Disable Rx

// Enable pull-up on P2_EEPROM_nCS
// P0, P1 pins with have pull-up if enabled
   P2INP = P2_DBG_DAT | P2_DBG_CLK | P2_XOSC32_Q1 | P2_XOSC32_Q2;

// Tristate all P0 pins except P0_EEPROM_MOSI and P0_EPD_nENABLE
   P0INP = (uint8_t) ~(P0_EEPROM_MOSI | P0_EPD_nENABLE);

// Tristate all P1 pins except P1_SERIAL_OUT
   P1INP = (uint8_t) ~P1_SERIAL_OUT;

// Set all GPIO ports to input mode
   P0DIR = 0;

// NB: P1_0 and P1_1 do not have pull up support and P1_EPD_nCS0 is P1_1
// so we leave P1_1 as an output and driver it appropriately
   P1DIR = P1_EPD_nCS0;
   if(!(mSelfMac[3] & 0xf0)) {
   // JA0xxxxxxx devices do not implement EPD power control so need
   // toset EPD_nCS0 high (it should already be, but make sure
      P1_1 = 1;
   }
   else {
   // JA1xxxxxxx devices implement EPD power control, make sure power is
   // turned off and drive P1_EPD_nCS0 low to prevent back powering the
   // display
      P1_1 = 0;
   }

   P2DIR = 0;

// Disconnect all perpherials from pins
   P0SEL = 0;
   P1SEL = 0;
   P2SEL = 0;
   COPY_CFG();
}
#endif
