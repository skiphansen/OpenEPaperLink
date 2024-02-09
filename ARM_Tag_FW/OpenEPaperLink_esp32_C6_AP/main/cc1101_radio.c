// Large portions of this code was copied from:
//  https://github.com/nopnop2002/esp-idf-cc1101 with the following copyright

/*
 * Copyright (c) 2011 panStamp <contact@panstamp.com>
 * Copyright (c) 2016 Tyler Sommer <contact@tylersommer.pro>
 * 
 * This file is part of the CC1101 project.
 * 
 * CC1101 is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * any later version.
 * 
 * CC1101 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with CC1101; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA   02110-1301 
 * USA
 * 
 * Author: Daniel Berenguer
 * Creation date: 03/03/2011
 */

#include "sdkconfig.h"
#ifdef CONFIG_OEPL_SUBGIG_SUPPORT

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <driver/spi_master.h>
#include "cc1101_radio.h"
#include "radio.h"
#define LOG(format, ... ) printf("%s: " format,__FUNCTION__,## __VA_ARGS__)

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <driver/spi_master.h>
#include <driver/gpio.h>
#include "esp_log.h"

#include "sdkconfig.h"

/**
 * RF STATES
 */
enum RFSTATE {
   RFSTATE_IDLE = 0,
   RFSTATE_RX,
   RFSTATE_TX
};

/**
 * Type of transfers
 */
#define WRITE_BURST                     0x40
#define READ_SINGLE                     0x80
#define READ_BURST                      0xC0

/**
 * Type of register
 */
#define CC1101_CONFIG_REGISTER    READ_SINGLE
#define CC1101_STATUS_REGISTER    READ_BURST

/**
 * PATABLE & FIFO's
 */
#define CC1101_PATABLE     0x3E  // PATABLE address
#define CC1101_TXFIFO      0x3F  // TX FIFO address
#define CC1101_RXFIFO      0x3F  // RX FIFO address

/**                              
 * Command strobes               
 */                              
#define CC1101_SRES        0x30  // Reset CC1101 chip
#define CC1101_SFSTXON     0x31  // Enable and calibrate frequency synthesizer (if MCSM0.FS_AUTOCAL=1). If in RX (with CCA):
// Go to a wait state where only the synthesizer is running (for quick RX / TX turnaround).
#define CC1101_SXOFF       0x32  // Turn off crystal oscillator
#define CC1101_SCAL        0x33  // Calibrate frequency synthesizer and turn it off. SCAL can be strobed from IDLE mode without
// setting manual calibration mode (MCSM0.FS_AUTOCAL=0)
#define CC1101_SRX         0x34  // Enable RX. Perform calibration first if coming from IDLE and MCSM0.FS_AUTOCAL=1
#define CC1101_STX         0x35  // In IDLE state: Enable TX. Perform calibration first if MCSM0.FS_AUTOCAL=1.
// If in RX state and CCA is enabled: Only go to TX if channel is clear
#define CC1101_SIDLE       0x36  // Exit RX / TX, turn off frequency synthesizer and exit Wake-On-Radio mode if applicable
#define CC1101_SWOR        0x38  // Start automatic RX polling sequence (Wake-on-Radio) as described in Section 19.5 if
// WORCTRL.RC_PD=0               
#define CC1101_SPWD        0x39  // Enter power down mode when CSn goes high
#define CC1101_SFRX        0x3A  // Flush the RX FIFO buffer. Only issue SFRX in IDLE or RXFIFO_OVERFLOW states
#define CC1101_SFTX        0x3B  // Flush the TX FIFO buffer. Only issue SFTX in IDLE or TXFIFO_UNDERFLOW states
#define CC1101_SWORRST     0x3C  // Reset real time clock to Event1 value
#define CC1101_SNOP        0x3D  // No operation. May be used to get access to the chip status byte

#define CC1101_STATE_SLEEP             0x00
#define CC1101_STATE_IDLE              0x01
#define CC1101_STATE_XOFF              0x02
#define CC1101_STATE_VCOON_MC          0x03
#define CC1101_STATE_REGON_MC          0x04
#define CC1101_STATE_MANCAL            0x05   
#define CC1101_STATE_VCOON             0x06
#define CC1101_STATE_REGON             0x07
#define CC1101_STATE_STARTCAL          0x08
#define CC1101_STATE_BWBOOST           0x09
#define CC1101_STATE_FS_LOCK           0x0A
#define CC1101_STATE_IFADCON           0x0B
#define CC1101_STATE_ENDCAL            0x0C
#define CC1101_STATE_RX                0x0D           
#define CC1101_STATE_RX_END            0x0E
#define CC1101_STATE_RX_RST            0x0F
#define CC1101_STATE_TXRX_SWITCH       0x10
#define CC1101_STATE_RXFIFO_OVERFLOW   0x11
#define CC1101_STATE_FSTXON            0x12
#define CC1101_STATE_TX                0x13
#define CC1101_STATE_TX_END            0x14
#define CC1101_STATE_RXTX_SWITCH       0x15
#define CC1101_STATE_TXFIFO_UNDERFLOW  0x16

// Masks for first byte read from RXFIFO
#define CC1101_NUM_RXBYTES_MASK        0x7f
#define CC1101_RXFIFO_OVERFLOW_MASK    0x80

// Masks for last byte read from RXFIFO
#define CC1101_LQI_MASK                0x7f
#define CC1101_CRC_OK_MASK             0x6f

#define CC1101_DEFVAL_IOCFG2		      0x2E
#define CC1101_DEFVAL_IOCFG1		      0x2E
#define CC1101_DEFVAL_IOCFG0		      0x06
#define CC1101_DEFVAL_FIFOTHR		      0x07
#define CC1101_DEFVAL_RCCTRL1		      0x41
#define CC1101_DEFVAL_RCCTRL0		      0x00
#define CC1101_DEFVAL_AGCTEST		      0x3F
#define CC1101_DEFVAL_MCSM1			   0x20
#define CC1101_DEFVAL_WORCTRL		      0xFB

RfSetting gFixedConfig[] = {
	{CC1101_IOCFG2, CC1101_DEFVAL_IOCFG2},
	{CC1101_IOCFG1, CC1101_DEFVAL_IOCFG1},
	{CC1101_IOCFG0, CC1101_DEFVAL_IOCFG0},
	{CC1101_FIFOTHR,CC1101_DEFVAL_FIFOTHR},
	{CC1101_RCCTRL1, CC1101_DEFVAL_RCCTRL1},
	{CC1101_RCCTRL0, CC1101_DEFVAL_RCCTRL0},
	{CC1101_MCSM1,CC1101_DEFVAL_MCSM1},
	{CC1101_WORCTRL,CC1101_DEFVAL_WORCTRL},
	{0xff,0},
};

void CC1101_writeBurstReg(uint8_t regAddr,uint8_t* buffer,uint8_t len);
void CC1101_readBurstReg(uint8_t *buffer,uint8_t regAddr,uint8_t len);
void CC1101_cmdStrobe(uint8_t cmd);
void CC1101_wakeUp(void);
uint8_t CC1101_readReg(uint8_t regAddr, uint8_t regType);
void CC1101_writeReg(uint8_t regAddr, uint8_t value);
void CC1101_reset(void);
void CC1101_setRxState(void);
void CC1101_setTxState(void);

spi_device_handle_t gSpiHndl;

#define readConfigReg(regAddr)   CC1101_readReg(regAddr, CC1101_CONFIG_REGISTER)
#define readStatusReg(regAddr)   CC1101_readReg(regAddr, CC1101_STATUS_REGISTER)
#define setIdleState()           CC1101_cmdStrobe(CC1101_SIDLE)
#define flushRxFifo()            CC1101_cmdStrobe(CC1101_SFRX)
#define flushTxFifo()            CC1101_cmdStrobe(CC1101_SFTX)

const char *RegNamesCC1101[] = {
   "IOCFG2",   // 0x00 GDO2 output pin configuration
   "IOCFG1",   // 0x01 GDO1 output pin configuration
   "IOCFG0",   // 0x02 GDO0 output pin configuration
   "FIFOTHR",  // 0x03 RX FIFO and TX FIFO thresholds
   "SYNC1",    // 0x04 Sync word, high INT8U
   "SYNC0",    // 0x05 Sync word, low INT8U
   "PKTLEN",   // 0x06 Packet length
   "PKTCTRL1", // 0x07 Packet automation control
   "PKTCTRL0", // 0x08 Packet automation control
   "ADDR",     // 0x09 Device address
   "CHANNR",   // 0x0A Channel number
   "FSCTRL1",  // 0x0B Frequency synthesizer control
   "FSCTRL0",  // 0x0C Frequency synthesizer control
   "FREQ2",    // 0x0D Frequency control word, high INT8U
   "FREQ1",    // 0x0E Frequency control word, middle INT8U
   "FREQ0",    // 0x0F Frequency control word, low INT8U
   "MDMCFG4",  // 0x10 Modem configuration
   "MDMCFG3",  // 0x11 Modem configuration
   "MDMCFG2",  // 0x12 Modem configuration
   "MDMCFG1",  // 0x13 Modem configuration
   "MDMCFG0",  // 0x14 Modem configuration
   "DEVIATN",  // 0x15 Modem deviation setting
   "MCSM2",    // 0x16 Main Radio Control State Machine configuration
   "MCSM1",    // 0x17 Main Radio Control State Machine configuration
   "MCSM0",    // 0x18 Main Radio Control State Machine configuration
   "FOCCFG",   // 0x19 Frequency Offset Compensation configuration
   "BSCFG",    // 0x1A Bit Synchronization configuration
   "AGCCTRL2", // 0x1B AGC control
   "AGCCTRL1", // 0x1C AGC control
   "AGCCTRL0", // 0x1D AGC control
   "WOREVT1",  // 0x1E High INT8U Event 0 timeout
   "WOREVT0",  // 0x1F Low INT8U Event 0 timeout
   "WORCTRL",  // 0x20 Wake On Radio control
   "FREND1",   // 0x21 Front end RX configuration
   "FREND0",   // 0x22 Front end TX configuration
   "FSCAL3",   // 0x23 Frequency synthesizer calibration
   "FSCAL2",   // 0x24 Frequency synthesizer calibration
   "FSCAL1",   // 0x25 Frequency synthesizer calibration
   "FSCAL0",   // 0x26 Frequency synthesizer calibration
   "RCCTRL1",  // 0x27 RC oscillator configuration
   "RCCTRL0",  // 0x28 RC oscillator configuration
   "FSTEST",   // 0x29 Frequency synthesizer calibration control
   "PTEST",    // 0x2A Production test
   "AGCTEST",  // 0x2B AGC test
   "TEST2",    // 0x2C Various test settings
   "TEST1",    // 0x2D Various test settings
   "TEST0",    // 0x2E Various test settings
   "0x2f",     // 0x2f
//CC1101 Strobe commands
   "SRES",     // 0x30 Reset chip.
   "SFSTXON",  // 0x31 Enable and calibrate frequency synthesizer (if MCSM0.FS_AUTOCAL=1).
   "SXOFF",    // 0x32 Turn off crystal oscillator.
   "SCAL",     // 0x33 Calibrate frequency synthesizer and turn it off
   "SRX",      // 0x34 Enable RX. Perform calibration first if coming from IDLE and
   "STX",      // 0x35 In IDLE state: Enable TX. Perform calibration first if
   "SIDLE",    // 0x36 Exit RX / TX, turn off frequency synthesizer and exit
   "SAFC",     // 0x37 Perform AFC adjustment of the frequency synthesizer
   "SWOR",     // 0x38 Start automatic RX polling sequence (Wake-on-Radio)
   "SPWD",     // 0x39 Enter power down mode when CSn goes high.
   "SFRX",     // 0x3A Flush the RX FIFO buffer.
   "SFTX",     // 0x3B Flush the TX FIFO buffer.
   "SWORRST",  // 0x3C Reset real time clock.
   "SNOP",     // 0x3D No operation. May be used to pad strobe commands to two
};


// SPI Stuff
#if CONFIG_SPI2_HOST
   #define HOST_ID SPI2_HOST
#elif CONFIG_SPI3_HOST
   #define HOST_ID SPI3_HOST
#endif

/*
 * RF state
 */
static uint8_t gRfState;

#define cc1101_Select()    gpio_set_level(CONFIG_CSN_GPIO, LOW)
#define cc1101_Deselect()  gpio_set_level(CONFIG_CSN_GPIO, HIGH)
#define wait_Miso()        while(gpio_get_level(CONFIG_MISO_GPIO)>0)
#define getGDO0state()     gpio_get_level(CONFIG_GDO0_GPIO)
#define wait_GDO0_high()   while(!getGDO0state())
#define wait_GDO0_low()    while(getGDO0state())

/**
 * Arduino Macros
 */
#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
#define bitSet(value, bit) ((value) |= (1UL << (bit)))
#define bitClear(value, bit) ((value) &= ~(1UL << (bit)))
#define bitWrite(value, bit, bitvalue) ((bitvalue) ? bitSet(value, bit) : bitClear(value, bit))
#define delayMicroseconds(us) esp_rom_delay_us(us)
#define LOW  0
#define HIGH 1

bool spi_write_byte(uint8_t* Dataout,size_t DataLength)
{
   spi_transaction_t SPITransaction;

   if(DataLength > 0) {
      memset(&SPITransaction,0,sizeof(spi_transaction_t));
      SPITransaction.length = DataLength * 8;
      SPITransaction.tx_buffer = Dataout;
      SPITransaction.rx_buffer = NULL;
      spi_device_transmit(gSpiHndl,&SPITransaction);
   }

   return true;
}

bool spi_read_byte(uint8_t* Datain,uint8_t* Dataout,size_t DataLength)
{
   spi_transaction_t SPITransaction;

   if(DataLength > 0) {
      memset(&SPITransaction,0,sizeof(spi_transaction_t));
      SPITransaction.length = DataLength * 8;
      SPITransaction.tx_buffer = Dataout;
      SPITransaction.rx_buffer = Datain;
      spi_device_transmit(gSpiHndl,&SPITransaction);
   }

   return true;
}

uint8_t spi_transfer(uint8_t address)
{
   uint8_t datain[1];
   uint8_t dataout[1];
   dataout[0] = address;
   //spi_write_byte(dev, dataout, 1 );
   //spi_read_byte(datain, dataout, 1 );

   spi_transaction_t SPITransaction;
   memset(&SPITransaction,0,sizeof(spi_transaction_t));
   SPITransaction.length = 8;
   SPITransaction.tx_buffer = dataout;
   SPITransaction.rx_buffer = datain;
   spi_device_transmit(gSpiHndl,&SPITransaction);

   return datain[0];
}

/**
 * CC1101_wakeUp
 * 
 * Wake up CC1101 from Power Down state
 */
void CC1101_wakeUp(void)
{
   cc1101_Select();
   wait_Miso();
   cc1101_Deselect();
}

/**
 * CC1101_writeReg
 * 
 * Write single register into the CC1101 IC via SPI
 * 
 * @param regAddr Register address
 * @param value Value to be writen
 */
void CC1101_writeReg(uint8_t regAddr, uint8_t value) 
{
   if(regAddr < 0x3f) {
      LOG("0x%x -> %s(0x%x)\n",value,RegNamesCC1101[regAddr],regAddr);
   }
   else {
      LOG("0x%x -> 0x%x\n",value,regAddr);
   }
   cc1101_Select();                                // Select CC1101
   wait_Miso();                                       // Wait until MISO goes low
   spi_transfer(regAddr);                       // Send register address
   spi_transfer(value);                         // Send value
   cc1101_Deselect();                              // Deselect CC1101
}

/**
 * CC1101_writeBurstReg
 * 
 * Write multiple registers into the CC1101 IC via SPI
 * 
 * @param regAddr Register address
 * @param buffer Data to be writen
 * @param len Data length
 */
void CC1101_writeBurstReg(uint8_t regAddr, uint8_t* buffer, uint8_t len)
{
   uint8_t addr, i;

   LOG("%d bytes to 0x%x\n",len,regAddr);

   addr = regAddr | WRITE_BURST; // Enable burst transfer
   cc1101_Select();
   wait_Miso();
   spi_transfer(addr);

   for(i = 0; i < len; i++) {
      spi_transfer(buffer[i]);   // Send value
   }

   cc1101_Deselect();
}

/**
 * CC1101_cmdStrobe
 * 
 * Send command strobe to the CC1101 IC via SPI
 * 
 * @param cmd Command strobe
 */         
void CC1101_cmdStrobe(uint8_t cmd) 
{
   cc1101_Select();
   wait_Miso();
   spi_transfer(cmd);
   cc1101_Deselect();
}

/**
 * CC1101_readReg
 * 
 * Read CC1101 register via SPI
 * 
 * @param regAddr Register address
 * @param regType Type of register: CONFIG_REGISTER or STATUS_REGISTER
 * 
 * Return:
 * Data uint8_t returned by the CC1101 IC
 */
uint8_t CC1101_readReg(uint8_t regAddr,uint8_t regType)
{
   uint8_t addr, val;

   addr = regAddr | regType;
   cc1101_Select();
   wait_Miso();
   spi_transfer(addr);
   val = spi_transfer(0x00);  // Read result
   cc1101_Deselect();

   return val;
}

/**
 * CC1101_readBurstReg
 * 
 * Read burst data from CC1101 via SPI
 * 
 * @param buffer Buffer where to copy the result to
 * @param regAddr Register address
 * @param len Data length
 */
void CC1101_readBurstReg(uint8_t *buffer,uint8_t regAddr,uint8_t len) 
{
   uint8_t addr, i;

   addr = regAddr | READ_BURST;
   cc1101_Select();
   wait_Miso();
   spi_transfer(addr);     // Send register address
   for(i = 0; i < len; i++) {
      buffer[i] = spi_transfer(0x00);        // Read result uint8_t by uint8_t
   }
   cc1101_Deselect();
}

/**
 * reset
 * 
 * Reset CC1101
 */
void CC1101_reset(void) 
{
// See sectin 19.1.2 of the CC1101 spec sheet for reasons for the following
   cc1101_Deselect();
   delayMicroseconds(5);
   cc1101_Select();
   delayMicroseconds(10);  
   cc1101_Deselect();
   delayMicroseconds(41);
   cc1101_Select();

// Wait until MISO goes low indicating XOSC stable
   wait_Miso();         
   spi_transfer(CC1101_SRES); // Send reset command strobe
   wait_Miso();
   cc1101_Deselect();
}


/**
 * CC1101_setRxState
 * 
 * Enter Rx state
 */
void CC1101_setRxState(void)
{
   CC1101_cmdStrobe(CC1101_SRX);
   gRfState = RFSTATE_RX;
}

/**
 * CC1101_setTxState
 * 
 * Enter Tx state
 */
void CC1101_setTxState(void)
{
   CC1101_cmdStrobe(CC1101_STX);
   gRfState = RFSTATE_TX;
}


void CC1101_DumpRegs()
{
   uint8_t regAddr;
   uint8_t value;

   LOG("\n");
   for(regAddr = 0; regAddr < 0x2f; regAddr++) {
      value = CC1101_readReg(regAddr,READ_SINGLE);
      printf("%02x %s: 0x%02X\n",regAddr,RegNamesCC1101[regAddr],value);
   }

   for(regAddr = 0; regAddr < 0x2f; regAddr++) {
      value = CC1101_readReg(regAddr,READ_SINGLE);
      printf("<Register><Name>%s</Name><Value>0x%02X</Value></Register>\n",RegNamesCC1101[regAddr],value);
   }
}


bool CC1101_Tx(uint8_t *TxData,size_t TxLen)
{
   uint8_t marcState;
   bool Ret = false;
   int ErrLine = 0;
   int tries = 0;

// Declare to be in Tx state. This will avoid receiving TxDatas whilst
// transmitting
   gRfState = RFSTATE_TX;

// Enter RX state
   CC1101_setRxState();

   do {
      // Check that the RX state has been entered
      while(tries++ < 1000) {
         marcState = readStatusReg(CC1101_MARCSTATE) & 0x1F;
         if(marcState == CC1101_STATE_RX) {
            break;
         }
         if(marcState == CC1101_STATE_RXFIFO_OVERFLOW) {
            flushRxFifo(); // flush receive queue
         }
      }

      if(marcState != CC1101_STATE_RX) {
      // TODO: MarcState sometimes never enters the expected state; 
      // this is a hack workaround.
         ErrLine = __LINE__;
         break;
      }

      delayMicroseconds(500);

      if(TxLen > 0) {
      // Set data length at the first position of the TX FIFO
         CC1101_writeReg(CC1101_TXFIFO,TxLen);
      // Write data into the TX FIFO
         CC1101_writeBurstReg(CC1101_TXFIFO, TxData,TxLen);
         CC1101_setTxState();
      }

   // Check that TX state is being entered (state = RXTX_SETTLING)
      marcState = readStatusReg(CC1101_MARCSTATE) & 0x1F;
      if(marcState != CC1101_STATE_TX 
         && marcState != CC1101_STATE_TX_END
         && marcState != CC1101_STATE_RXTX_SWITCH)
      {
         ErrLine = __LINE__;
         break;
      }

   // Wait for the sync word to be transmitted
      wait_GDO0_high();

   // Wait until the end of the TxData transmission
      wait_GDO0_low();

   // Check that the TX FIFO is empty
      if((readStatusReg(CC1101_TXBYTES) & 0x7F) != 0) {
         ErrLine = __LINE__;
         break;
      }
      Ret = true;
   } while(false);

   setIdleState();
   flushTxFifo();
   CC1101_setRxState();

   if(ErrLine != 0) {
      LOG("%s#%d: failure\n",__FUNCTION__,ErrLine);
   }

   return Ret;
}

int CC1101_Rx(uint8_t *RxBuf,size_t RxBufLen,uint8_t *pRssi,uint8_t *pLqi)
{
   uint8_t rxBytes = readStatusReg(CC1101_RXBYTES);
   uint8_t Rssi;
   uint8_t Lqi;
   int Ret;

   Ret = rxBytes & CC1101_NUM_RXBYTES_MASK;
// Any uint8_t waiting to be read and no overflow?
   if(rxBytes & CC1101_RXFIFO_OVERFLOW_MASK) {
      LOG("RxFifo overflow\n");
      Ret = -2;
   }
   else if(Ret != 0) {
   // Read RxBuf length
      Ret = readConfigReg(CC1101_RXFIFO);
      // If TxData is too long
      if(Ret > RxBufLen) {
      // Toss the data
         LOG("RxBuf too small %d < %d\n",RxBufLen,Ret);
         Ret = -1;
      }
      else {
      // Read RxBuf TxData
         CC1101_readBurstReg(RxBuf,CC1101_RXFIFO,Ret);
      // Read RSSI
         Rssi = readConfigReg(CC1101_RXFIFO);
      // Read LQI and CRC_OK
         Lqi = readConfigReg(CC1101_RXFIFO);
         if(Lqi & CC1101_CRC_OK_MASK) {
         // CRC is valid
            if(pRssi != NULL) {
               *pRssi = Rssi;
            }
            if(pLqi != NULL) {
               *pLqi = Lqi & CC1101_LQI_MASK;
            }
         }
         else {
         // Crc error, ignore the packet
            Ret = 0;
         }
      }
   } 

   setIdleState();
   flushRxFifo();
   CC1101_setRxState();

   return Ret;
}

bool CC1101_Present()
{
   bool Ret = false;
   uint8_t PartNum = CC1101_readReg(CC1101_PARTNUM, CC1101_STATUS_REGISTER);
   uint8_t ChipVersion = CC1101_readReg(CC1101_VERSION, CC1101_STATUS_REGISTER);

   if(PartNum == 0 && ChipVersion == 20) {
      LOG("CC1101 detected\n");
      Ret = true;
   }

   return Ret;
}

void CC1101_SetConfig(RfSetting *pConfig)
{
   int i;
   uint8_t RegWasSet[CC1101_TEST0 + 1];
   uint8_t Reg;

   memset(RegWasSet,0,sizeof(RegWasSet));
   CC1101_reset();

// Set the fixed registers
   for(i = 0; (Reg = gFixedConfig[i].Reg) != 0xff; i++) {
      CC1101_writeReg(Reg,gFixedConfig[i].Value);
      RegWasSet[Reg] = 1;
   }

   while((Reg = pConfig->Reg) != 0xff) {
      if(RegWasSet[Reg] == 1) {
         LOG("%s value ignored\n",RegNamesCC1101[Reg]);
      }
      else {
         if(RegWasSet[Reg] == 2) {
            LOG("%s value set twice\n",RegNamesCC1101[Reg]);
         }
         CC1101_writeReg(pConfig->Reg,pConfig->Value);
         RegWasSet[Reg] = 2;
      }
      pConfig++;
   }
   for(Reg = 0; Reg <= CC1101_TEST0; Reg++) {
      if(RegWasSet[Reg] == 0) {
         LOG("%s value not set\n",RegNamesCC1101[Reg]);
      }
   }
}
#endif // CONFIG_OEPL_SUBGIG_SUPPORT
