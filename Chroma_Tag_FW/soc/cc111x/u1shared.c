#include <stdbool.h>

#include "cpu.h"
#include "u1.h"
#include "board.h"
#include "powermgt.h"
#include "settings.h"

void u1init(void)
{
   U1UCR = U1UCR_STOP;     //no parity, 8 bits per char, normal polarity
}

uint8_t u1byte(uint8_t v)
{
   U1DBUF = v;
// Wait until last byte has been sent
   while(!(U1CSR & U1CSR_TX_BYTE));
   U1CSR &= (uint8_t)~U1CSR_TX_BYTE;
// Wait for USART1 idle
   while(U1CSR & U1CSR_ACTIVE);
// read and return data
   return U1DBUF;
}


/*
   Baud_E   Baud_M   Baudrate          1,000,000 baud Error
   15       59       999755.859375     -0.0245 %
   15       60       1002929.687500    0.29296875 %
*/

//the order of ops in these two functions are very important to avoid glitches on UART.TX
// #define BAUD_115200

void u1setUartMode(void)
{
// Disconnect USART from EEPROM pins
   P0SEL &= (uint8_t) ~(P0_EEPROM_MOSI | P0_EEPROM_CLK | P0_EEPROM_MISO);
      
#ifdef BAUD_115200
   U1BAUD = 34;      //BAUD_M = 34 (115200)
   U1GCR = U1GCR_BAUD_E(12);  //BAUD_E = 12, lsb first
#else
// Default 1meg baud
   U1BAUD = 60;      //BAUD_M = 60
   U1GCR = U1GCR_BAUD_E(15);       //BAUD_E = 15, lsb first
#endif                        //
// UART mode, RX on
   U1CSR = U1CSR_MODE | U1CSR_RE;
   
// Connect USART to debug pins
   P1SEL |= P1_SERIAL_OUT | P1_SERIAL_IN;
// Set USART 1 alternate 2 locaton connecting it to P1
   PERCFG |= PERCFG_U1CFG; 
   gUartSelected = true;
}

void u1setEepromMode(void)
{
// Clear USART 1 alternate 2 location, connecting USART to P0
   PERCFG &= (uint8_t) ~PERCFG_U1CFG;
// Disconnect USART from debug pins
   P1SEL &= (uint8_t) ~(P1_SERIAL_OUT | P1_SERIAL_IN);
   
   U1BAUD = 0;       //F/8 is max for spi - 3.25 MHz
   U1GCR = U1GCR_ORDER | U1GCR_BAUD_E(0x11);  //BAUD_E = 0x11, msb first
   U1CSR = U1CSR_RE;    //SPI master mode, RX on
   
// Connect USART to EEPROM pins
   P0SEL |= P0_EEPROM_MOSI | P0_EEPROM_CLK | P0_EEPROM_MISO;
   gUartSelected = false;
}

