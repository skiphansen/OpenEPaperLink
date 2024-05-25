#include "../boardChroma.c"

#define P0_EPD_PINS (P0_EPD_BS1 | P0_EPD_D_nCMD)
#define P1_EPD_PINS (P1_EPD_DI | P1_EPD_SCK | P1_EPD_nRESET | P1_EPD_nCS0 | P1_EPD_BUSY)

void powerPortsDownForSleep(void)
{
   T1CTL = 0;  //timer off
   U1GCR = 0;  // Disable Rx

// Enable pullups on all P2 pins and select pullups (when enabled) on P0, P1
// (This shouldn't be necessary, this is the default)
   P2INP = 0;

// P1.1 is P1_EPD_nCS0 so we leave it as an output
   P1DIR = P1_EPD_nCS0;
// As far as we know all Chroma74 verions implement EPD power control

// Disable pullups on pins connected to the EPD
   P0INP = P0_EPD_PINS;
   P1INP = P1_EPD_PINS;
// Set all pins to input mode except the pins connected to the EPD
   P0DIR = P0_EPD_PINS;
   P1DIR = P1_EPD_PINS;
// drive EPD pins low prevent back powering the display
   P0 &= ~P0_EPD_PINS;
   P1 &= ~P1_EPD_PINS;

// Disconnect all perpherials from pins
   P0SEL = 0;
   P1SEL = 0;
   P2SEL = 0;
   COPY_CFG();
}

