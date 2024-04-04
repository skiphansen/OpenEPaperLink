#include "board.h"
#include "cpu.h"
#include "u1.h"
#include "powermgt.h"
#include "printf.h"
#include "settings.h"
#include "logging.h"

// Port 0 pins
// P0.0 out gpio        EPD BS1 
// P0.1 in gpio         TP6/TP16
// P0.2 in gpio         n/c? (EPD nCS1 on some models)
// P0.3 out peripheral  SPI EEPROM CLK    USART1 alt1
// P0.4 out peripheral  SPI EEPROM MOSI   USART1 alt1
// P0.5 in  peripheral  SPI EEPROM MISO   USART1 alt1
// P0.6 out gpio        EPD nENABLE
// P0.7 out gpio        EPD D_nCMD
// 
// Port 1 pins
// P1.0 in  gpio        EPD BUSY
// P1.1 out gpio        EPD nCS
// P1.2 out gpio        EPD nRESET
// P1.3 out peripheral  EPD DO/SCK  USART0 
// P1.4 in  gpio        ? 
// P1.5 out peripheral  EPD D1/SDIN USART0
// P1.6 out peripheral  TXD USART1 alt2
// P1.7 in  peripheral  RXD USART1 alt2
// 
// Port 2 pins
// P2.0 output gpio        EEPROM nCS  
// P2.1 DBG_DAT
// P2.2 DBG_CLK
// P2.3 XOSC32_Q1
// P2.4 XOSC32_Q2
// 
// USART0 is always configured to use port 1 in SPI mode to control the EPD
// 
// NB:
// USART1 is configured to use port 0 in SPI mode to control the SPI EEPROM
// OR
// USART1 is configured to use port 1 in UART mode for use as a debug UART.
// 
// This is accomplished by setting PERCFG and P1SEL dynamically depending on
// how USART1 is being used.
// 
void PortInit()
{
   PERCFG = PERCFG_U0CFG;  // USART0 on P1

// Set port 0 default output value (all high except BS1)
   P0 =  P0_EEPROM_CLK | P0_EEPROM_MOSI | P0_EPD_nENABLE | P0_EPD_D_nCMD;

// Port 0 output pins
   P0DIR = P0_EPD_BS1 | P0_EEPROM_CLK | P0_EEPROM_MOSI 
         | P0_EPD_nENABLE | P0_EPD_D_nCMD;

// Port 0 peripheral pins
   P0SEL = P0_EEPROM_CLK | P0_EEPROM_MOSI | P0_EEPROM_MISO;

// Set port 1 default output value (all high)
   P1 = P1_EPD_nCS0 | P1_EPD_nRESET | P1_EPD_SCK 
      | P1_EPD_DI | P1_SERIAL_OUT;      

// Port 1 output pins
   P1DIR = P1_EPD_nCS0 | P1_EPD_nRESET | P1_EPD_SCK 
         | P1_EPD_DI | P1_SERIAL_OUT;

// Port 1 peripheral pins
   P1SEL = P1_EPD_SCK | P1_EPD_DI | P1_SERIAL_OUT | P1_SERIAL_IN;

// Both USART0 and USART1 are assigned to P1, the P2SEL register defines
// which USART gets P1.4 and P1.5.  In our case it should be USART0
//   P2SEL = P2SEL_PRI3P1;

// Set default output value EEPROM CS high
   P2 = P2_EEPROM_nCS;
   P2DIR = P2_EEPROM_nCS;
   LOG_CONFIG("PortInit");
}

#ifdef DEBUG_CHIP_CFG
void LogConfig(const char __code *Msg)
{
   uint8_t Status;
   uint8_t P0Dir = P0DIR;
   uint8_t P0Sel = P0SEL;
   uint8_t P0Data = P0;
   uint8_t P1Dir = P1DIR;
   uint8_t P1Data = P1;
   uint8_t P2Sel = P2SEL;
   uint8_t PERCfg = PERCFG;
   uint8_t P1Sel = P1SEL;
   uint8_t U1Baud = U1BAUD;
   uint8_t U1Gcr = U1GCR;
   uint8_t U1Csr = U1CSR;
   LOG("%s\n",Msg);
   LOG("P0Dir 0x%x\n",P0Dir);
   LOG("P0Sel 0x%x\n",P0Sel);
   LOG("P0Data 0x%x\n",P0Data);
   LOG("P1Dir 0x%x\n",P1Dir);
   LOG("P1Data 0x%x\n",P1Data);
   LOG("P2Sel 0x%x\n",P2Sel);
   LOG("PERCfg 0x%x\n",PERCfg);
   LOG("P1Sel 0x%x\n",P1Sel);
   LOG("U1Baud 0x%x\n",U1Baud);
   LOG("U1Gcr 0x%x\n",U1Gcr);
   LOG("U1Csr 0x%x\n",U1Csr);
   LOG("UartSelected 0x%x\n",gUartSelected);
}
#endif

void powerPortsDownForSleep(void)
{
#if 0
   P0 = 0b01000000;
   P1 = 0b01000000;
   P2 = 0b00000001;
   P0DIR = 0b11111111;
   P1DIR = 0b01101110;
   P2DIR = 0b00000001;
   P0SEL = 0;
   P1SEL = 0;
   P2SEL = 0;
#endif
}


