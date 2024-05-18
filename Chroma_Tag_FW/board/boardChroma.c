#include "soc.h"
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
// NB: since the EPD isn't powered until we use it DO NOT connect 
// the EPD SPI pins as SPI initally and drive all of the control pins
// LOW to avoid back powering the EPD via the control pins
void PortInit()
{
   PERCFG = PERCFG_U0CFG;  // USART0 on P1

// Set port 0 default output value
   P0 =  P0_EEPROM_CLK | P0_EEPROM_MOSI | P0_EPD_nENABLE;

// Port 0 output pins
   P0DIR = P0_EPD_BS1 | P0_EEPROM_CLK | P0_EEPROM_MOSI 
         | P0_EPD_nENABLE | P0_EPD_D_nCMD
#if BOARD == chroma42
         | P0_EPD_nCS1
#endif
      ;
// Port 0 peripheral pins
   P0SEL = P0_EEPROM_CLK | P0_EEPROM_MOSI | P0_EEPROM_MISO;

// Set port 1 default output value
   P1 = P1_SERIAL_OUT;      

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
}

#ifdef DEBUG_CHIP_CFG

static uint8_t g_Status;
static uint8_t gP0Dir;
static uint8_t gP0Sel;
static uint8_t gP0Data;
static uint8_t gP1Dir;
static uint8_t gP1Data;
static uint8_t gP2Sel;
static uint8_t gPERCfg;
static uint8_t gP1Sel;
static uint8_t gU1Baud;
static uint8_t gU1Gcr;
static uint8_t gU1Csr;
static uint8_t gP0INP;
static uint8_t gP1INP;
static uint8_t gP2INP;
static uint8_t gP2Dir;
static uint8_t gP2Data;

void CopyCfg()
{
   gP0Sel = P0SEL;
   gP0Dir = P0DIR;
   gP0Data = P0;
   gP1Dir = P1DIR;
   gP0INP = P0INP;

   gP1Sel = P1SEL;
   gP1Dir = P1DIR;
   gP1Data = P1;
   gP1Dir = P1DIR;
   gP1INP = P1INP;

   gP2Sel = P2SEL;
   gP2Dir = P2DIR;
   gP2Data = P2;
   gP2INP = P2INP;

   gPERCfg = PERCFG;
   gU1Baud = U1BAUD;
   gU1Gcr = U1GCR;
   gU1Csr = U1CSR;
}

void PrintCfg(const char __code *Msg)
{
   LOG("Chip cfg %s\n",Msg);
   LOG("P0Sel 0x%x\n",gP0Sel);
   LOG("P0Dir 0x%x\n",gP0Dir);
   LOG("P0Data 0x%x\n",gP0Data);
   LOG("P0Inp 0x%x\n\n",gP0INP);

   LOG("P1Sel 0x%x\n",gP1Sel);
   LOG("P1Dir 0x%x\n",gP1Dir);
   LOG("P1Data 0x%x\n",gP1Data);
   LOG("P1Inp 0x%x\n\n",gP1INP);

   LOG("P2Sel 0x%x\n",gP2Sel);
   LOG("P2Dir 0x%x\n",gP2Dir);
   LOG("P2Data 0x%x\n",gP2Data);
   LOG("P2Inp 0x%x\n\n",gP2INP);

   LOG("PERCfg 0x%x\n",gPERCfg);
   LOG("U1Baud 0x%x\n",gU1Baud);
   LOG("U1Gcr 0x%x\n",gU1Gcr);
   LOG("U1Csr 0x%x\n",gU1Csr);
}
#endif



