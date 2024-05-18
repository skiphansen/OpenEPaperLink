#ifndef _CC111X_H_
#define _CC111X_H_

#include <stdint.h>

#define  RFTXRX_VECTOR  0    // RF TX done / RX ready
#define  ADC_VECTOR     1    // ADC End of Conversion
#define  URX0_VECTOR    2    // USART0 RX Complete
#define  URX1_VECTOR    3    // USART1 RX Complete
#define  ENC_VECTOR     4    // AES Encryption/Decryption Complete
#define  ST_VECTOR      5    // Sleep Timer Compare
#define  P2INT_VECTOR   6    // Port 2 Inputs
#define  UTX0_VECTOR    7    // USART0 TX Complete
#define  DMA_VECTOR     8    // DMA Transfer Complete
#define  T1_VECTOR      9    // Timer 1 (16-bit) Capture/Compare/Overflow
#define  T2_VECTOR      10   // Timer 2 (MAC Timer) Overflow
#define  T3_VECTOR      11   // Timer 3 (8-bit) Capture/Compare/Overflow
#define  T4_VECTOR      12   // Timer 4 (8-bit) Capture/Compare/Overflow
#define  P0INT_VECTOR   13   // Port 0 Inputs
#define  UTX1_VECTOR    14   // USART1 TX Complete
#define  P1INT_VECTOR   15   // Port 1 Inputs
#define  RF_VECTOR      16   // RF General Interrupts
#define  WDT_VECTOR     17   // Watchdog Overflow in Timer Mode

// ADC input channels

#define ADC_CHAN_AIN0   0
#define ADC_CHAN_AIN1   1
#define ADC_CHAN_AIN2   2
#define ADC_CHAN_AIN3   3
#define ADC_CHAN_AIN4   4
#define ADC_CHAN_AIN5   5
#define ADC_CHAN_AIN6   6
#define ADC_CHAN_AIN7   7
#define ADC_CHAN_AIN0_1 8
#define ADC_CHAN_AIN2_3 9
#define ADC_CHAN_AIN4_5 10
#define ADC_CHAN_AIN6_7 11
#define ADC_CHAN_GND    12
#define ADC_CHAN_VREF   13
#define ADC_CHAN_TEMP   14
#define ADC_CHAN_VDD_3  15

static __idata __at (0x00) unsigned char R0;
static __idata __at (0x01) unsigned char R1;
static __idata __at (0x02) unsigned char R2;
static __idata __at (0x03) unsigned char R3;
static __idata __at (0x04) unsigned char R4;
static __idata __at (0x05) unsigned char R5;
static __idata __at (0x06) unsigned char R6;
static __idata __at (0x07) unsigned char R7;

__sbit __at (0x80) P0_0;
__sbit __at (0x81) P0_1;
__sbit __at (0x82) P0_2;
__sbit __at (0x83) P0_3;
__sbit __at (0x84) P0_4;
__sbit __at (0x85) P0_5;
__sbit __at (0x86) P0_6;
__sbit __at (0x87) P0_7;
__sbit __at (0x90) P1_0;
__sbit __at (0x91) P1_1;
__sbit __at (0x92) P1_2;
__sbit __at (0x93) P1_3;
__sbit __at (0x94) P1_4;
__sbit __at (0x95) P1_5;
__sbit __at (0x96) P1_6;
__sbit __at (0x97) P1_7;
__sbit __at (0xa0) P2_0;
__sbit __at (0xa1) P2_1;
__sbit __at (0xa2) P2_2;
__sbit __at (0xa3) P2_3;
__sbit __at (0xa4) P2_4;
__sbit __at (0xa5) P2_5;
__sbit __at (0xa6) P2_6;
__sbit __at (0xa7) P2_7;
__sbit __at (0xa8) RFTXRXIE;  // RF TX/RX FIFO interrupt enable
__sbit __at (0xa9) ADCIE;     // ADC Interrupt Enable
__sbit __at (0xaa) URX0IE;    // USART0 RX Interrupt Enable
__sbit __at (0xab) URX1IE;    // USART1 RX Interrupt Enable
__sbit __at (0xac) ENCIE;     // AES Encryption/Decryption Interrupt Enable
__sbit __at (0xad) STIE;      // Sleep Timer Interrupt Enable
__sbit __at (0xaf) EA;        // Global Interrupt Enable

__sbit __at (0xAF) IEN_EA;

__sfr __at (0x80) P0;

__sfr __at (0x86) U0CSR;

// PCON (0x87) - Power Mode Control
__sfr __at (0x87) PCON;
#define PCON_IDLE                         0x01

__sfr __at (0x88) TCON;
__sfr __at (0x89) P0IFG;
__sfr __at (0x8A) P1IFG;
__sfr __at (0x8B) P2IFG;
__sfr __at (0x8C) PICTL;
__sfr __at (0x8D) P1IEN;
__sfr __at (0x8F) P0INP;
__sfr __at (0x90) P1;
__sfr __at (0x91) RFIM;
__sfr __at (0x93) XPAGE;      //really called MPAGE
__sfr __at (0x93) _XPAGE;     //really called MPAGE
__sfr __at (0x95) ENDIAN;
__sfr __at (0x98) S0CON;

// IEN2 (0x9A) - Interrupt Enable 2 Register
__sfr __at (0x9A) IEN2;
#define IEN2_WDTIE                        0x20
#define IEN2_P1IE                         0x10
#define IEN2_UTX1IE                       0x08
#define IEN2_I2STXIE                      0x08
#define IEN2_UTX0IE                       0x04
#define IEN2_P2IE                         0x02
#define IEN2_USBIE                        0x02
#define IEN2_RFIE                         0x01


// S1CON (0x9B) - CPU Interrupt Flag 3
__sfr __at (0x9B) S1CON;
#define S1CON_RFIF_1                      0x02
#define S1CON_RFIF_0                      0x01

__sfr __at (0x9C) T2CT;
__sfr __at (0x9D) T2PR;       //used by radio for storage
__sfr __at (0x9E) TCTL;
__sfr __at (0xA0) P2;

// WORIRQ (0xA1) - Sleep Timer Interrupt Control
__sfr __at (0xA1) WORIRQ;
#define WORIRQ_EVENT0_MASK                0x10
#define WORIRQ_EVENT0_FLAG                0x01

// WORCTL (0xA2) - Sleep Timer Control
__sfr __at (0xA2) WORCTRL;
#define WORCTL_WOR_RESET                  0x04
#define WORCTL_WOR_RES                    0x03
#define WORCTL_WOR_RES1                   0x02
#define WORCTL_WOR_RES0                   0x01

__sfr __at (0xA3) WOREVT0;
__sfr __at (0xA4) WOREVT1;
__sfr __at (0xA5) WORTIME0;
__sfr __at (0xA6) WORTIME1;

// IEN0 (0xA8) – Interrupt Enable 0 Register
__sfr __at (0xA8) IEN0;
#define IEN0_EA               0x80     // Enable All
#define IEN0_STIE             0x20     // Sleep Timer
#define IEN0_ENCIE            0x10     // AES
#define IEN0_URX1IE           0x08     // USART1 Rx
#define IEN0_URX0IE           0x04     // USART0 Rx
#define IEN0_ADCIE            0x02     // ADC
#define IEN0_RFTXRXIE         0x01     // RF Rx/Tx

__sfr __at (0xA9) IP0;
__sfr __at (0xAB) FWT;
__sfr __at (0xAC) FADDRL;
__sfr __at (0xAD) FADDRH;
__sfr __at (0xAE) FCTL;
__sfr __at (0xAF) FWDATA;
__sfr __at (0xB1) ENCDI;
__sfr __at (0xB2) ENCDO;
__sfr __at (0xB3) ENCCS;
__sfr __at (0xB4) ADCCON1;
__sfr __at (0xB5) ADCCON2;
__sfr __at (0xB6) ADCCON3;
__sfr __at (0xB8) IEN1;
__sfr __at (0xB9) IP1;
__sfr __at (0xBA) ADCL;
__sfr __at (0xBB) ADCH;
__sfr __at (0xBC) RNDL;
__sfr __at (0xBD) RNDH;

// SLEEP (0xBE) - Sleep Mode Control
__sfr __at (0xBE) SLEEP;
#define SLEEP_USB_EN                      0x80
#define SLEEP_XOSC_S                      0x40
#define SLEEP_HFRC_S                      0x20
#define SLEEP_RST                         0x18
#define SLEEP_RST0                        0x08
#define SLEEP_RST1                        0x10
#define SLEEP_OSC_PD                      0x04
#define SLEEP_MODE                        0x03
#define SLEEP_MODE1                       0x02
#define SLEEP_MODE0                       0x01

#define SLEEP_RST_POR_BOD                 (0x00 << 3)
#define SLEEP_RST_EXT                     (0x01 << 3)
#define SLEEP_RST_WDT                     (0x02 << 3)

#define SLEEP_MODE_PM0                    (0x00)
#define SLEEP_MODE_PM1                    (0x01)
#define SLEEP_MODE_PM2                    (0x02)
#define SLEEP_MODE_PM3                    (0x03)

// IRCON (0xC0) - CPU Interrupt Flag 4 - bit accessible SFR register
__sfr __at (0xC0) IRCON;
#define IRCON_STIE            0x80     // Sleep Timer
#define IRCON_POIF            0x20     // Port 0
#define IRCON_T4IF            0x10     // Timer 4
#define IRCON_T3IF            0x08     // Timer 3
#define IRCON_T2IF            0x04     // Timer 2
#define IRCON_T1IF            0x02     // Timer 1
#define IRCON_DMAIF           0x01     // DMA

__sfr __at (0xC1) U0DBUF;
__sfr __at (0xC2) U0BAUD;
__sfr __at (0xC4) U0UCR;
__sfr __at (0xC5) U0GCR;

// CLKCON (0xC6) - Clock Control
__sfr __at (0xC6) CLKCON;
#define CLKCON_OSC32          0x80  // 32k clock osc select 0: Xtal 1: RC
#define CLKCON_OSC            0x40  // system clock osc select 0: Xtal 1:RC
#define CLKCON_TICKSPD_MASK   0x38  // bit mask, for timer ticks output setting
#define CLKCON_TICKSPD(x)     (x << 3) // for timer tick divider select 0 = /1
#define CLKCON_CLKSPD_MASK    0x07  // bit mask, for the clock speed
#define CLKCON_CLKSPD(x)      (x)   // system clk divider 0 = /1

#define TICKSPD_DIV_1                     (0x00 << 3)
#define TICKSPD_DIV_2                     (0x01 << 3)
#define TICKSPD_DIV_4                     (0x02 << 3)
#define TICKSPD_DIV_8                     (0x03 << 3)
#define TICKSPD_DIV_16                    (0x04 << 3)
#define TICKSPD_DIV_32                    (0x05 << 3)
#define TICKSPD_DIV_64                    (0x06 << 3)
#define TICKSPD_DIV_128                   (0x07 << 3)

#define CLKSPD_DIV_1                      (0x00)
#define CLKSPD_DIV_2                      (0x01)
#define CLKSPD_DIV_4                      (0x02)
#define CLKSPD_DIV_8                      (0x03)
#define CLKSPD_DIV_16                     (0x04)
#define CLKSPD_DIV_32                     (0x05)
#define CLKSPD_DIV_64                     (0x06)
#define CLKSPD_DIV_128                    (0x07)

// MEMCTR (0xC7) - Memory Arbiter Control
__sfr __at (0xC7) MEMCTR;
#define MEMCTR_CACHD                      0x02
#define MEMCTR_PREFD                      0x01

__sfr __at (0xC9) WDCTL;
__sfr __at (0xCA) T3CNT;
__sfr __at (0xCB) T3CTL;
__sfr __at (0xCC) T3CCTL0;
__sfr __at (0xCD) T3CC0;
__sfr __at (0xCE) T3CCTL1;
__sfr __at (0xCF) T3CC1;

__sfr __at (0xD1) DMAIRQ;
__sfr16 __at (0xD3D2) DMA1CFG;
__sfr16 __at (0xD5D4) DMA0CFG;

// DMAARM (0xD6) - DMA Channel Arm
__sfr __at (0xD6) DMAARM;
#define DMAARM_ABORT                      0x80
#define DMAARM4                           0x10
#define DMAARM3                           0x08
#define DMAARM2                           0x04
#define DMAARM1                           0x02
#define DMAARM0                           0x01

__sfr __at (0xD7) DMAREQ;
__sfr __at (0xD8) TIMIF;
__sfr __at (0xD9) RFD;
__sfr16 __at (0xDBDA) T1CC0;  //used by timer for storage
__sfr16 __at (0xDDDC) T1CC1;
__sfr16 __at (0xDFDE) T1CC2;

// RFST (0xE1) - RF Strobe Commands
__sfr __at (0xE1) RFST;
#define RFST_SFSTXON                      0x00
#define RFST_SCAL                         0x01
#define RFST_SRX                          0x02
#define RFST_STX                          0x03
#define RFST_SIDLE                        0x04
#define RFST_SNOP                         0x05

__sfr __at (0xE2) T1CNTL;
__sfr __at (0xE3) T1CNTH;
// T1CTL (0xE4) - Timer 1 Control and Status
__sfr __at (0xE4) T1CTL;
#define T1CTL_CH2IF                       0x80 // Timer 1 channel 2 interrupt flag
#define T1CTL_CH1IF                       0x40 // Timer 1 channel 1 interrupt flag
#define T1CTL_CH0IF                       0x20 // Timer 1 channel 0 interrupt flag
#define T1CTL_OVFIF                       0x10 // Timer 1 counter overflow interrupt flag
#define T1CTL_DIV                         0x0C
#define T1CTL_DIV0                        0x04
#define T1CTL_DIV1                        0x08
#define T1CTL_MODE                        0x03
#define T1CTL_MODE0                       0x01
#define T1CTL_MODE1                       0x02

__sfr __at (0xE5) T1CCTL0;
__sfr __at (0xE6) T1CCTL1;
__sfr __at (0xE7) T1CCTL2;
__sfr __at (0xE8) IRCON2;

// RFIF (0xE9) - RF Interrupt Flags
__sfr __at (0xE9) RFIF;
#define RFIF_IRQ_TXUNF                    0x80
#define RFIF_IRQ_RXOVF                    0x40
#define RFIF_IRQ_TIMEOUT                  0x20
#define RFIF_IRQ_DONE                     0x10
#define RFIF_IRQ_CS                       0x08
#define RFIF_IRQ_PQT                      0x04
#define RFIF_IRQ_CCA                      0x02
#define RFIF_IRQ_SFD                      0x01

__sfr __at (0xEA) T4CNT;
__sfr __at (0xEB) T4CTL;
__sfr __at (0xEC) T4CCTL0;
__sfr __at (0xED) T4CC0;      //used by radio for storage
__sfr __at (0xEE) T4CCTL1;
__sfr __at (0xEF) T4CC1;      //used by radio for storage

// PERCFG (0xF1) - Peripheral Control
__sfr __at (0xF1) PERCFG;
#define PERCFG_T1CFG    0x40  // timer 1 alternate location
#define PERCFG_T3CFG    0x20  // timer 3 alternate location
#define PERCFG_T4CFG    0x10  // timer 4 alternate location
#define PERCFG_U1CFG    0x02  // USART 1 alternate location 0: P0, 1: P1
#define PERCFG_U0CFG    0x01  // USART 0 alternate location

__sfr __at (0xF2) ADCCFG;
__sfr __at (0xF3) P0SEL;
__sfr __at (0xF4) P1SEL;
__sfr __at (0xF5) P2SEL;
__sfr __at (0xF6) P1INP;

// P2INP (0xF7) - Port 2 Input Mode
__sfr __at (0xF7) P2INP;

// Configure P2 as 0: pull-up or 1: pull-down
#define P2INP_PDUP2                       0x80  // = P2

// Configure P1:2 to P1:7 as 0: pull-up or 1: pull-down
// P1:0 and P1:1 don't have pullup or pulldowns
#define P2INP_PDUP1                       0x40

// Configure all P0 pins as 0: pull-up or 1: pull-down
#define P2INP_PDUP0                       0x20

// P2.0 to P2.4 0: pull-up/pull-down, 1: tristate
#define P2INP_MDP2                        0x1F
#define P2INP_MDP2_0                      (0x01)
#define P2INP_MDP2_1                      (0x02)
#define P2INP_MDP2_2                      (0x04)
#define P2INP_MDP2_3                      (0x08)
#define P2INP_MDP2_4                      (0x10)


// U1CSR (0xF8) - USART 1 Control and Status - bit accessible SFR register
__sfr __at (0xF8) U1CSR;
#define U1CSR_MODE                        0x80  // 1 = uart, 0 = SPI
#define U1CSR_RE                          0x40
#define U1CSR_SLAVE                       0x20
#define U1CSR_FE                          0x10
#define U1CSR_ERR                         0x08
#define U1CSR_RX_BYTE                     0x04
#define U1CSR_TX_BYTE                     0x02
#define U1CSR_ACTIVE                      0x01

__sfr __at (0xF9) U1DBUF;
__sfr __at (0xFA) U1BAUD;

// U1UCR (0xFB) - USART 1 UART Control
__sfr __at (0xFB) U1UCR;
#define U1UCR_FLUSH                       0x80
#define U1UCR_FLOW                        0x40
#define U1UCR_D9                          0x20
#define U1UCR_BIT9                        0x10
#define U1UCR_PARITY                      0x08
#define U1UCR_SPB                         0x04
#define U1UCR_STOP                        0x02
#define U1UCR_START                       0x01

// U1GCR (0xFC) - USART 1 Generic Control
__sfr __at (0xFC) U1GCR;
#define U1GCR_CPOL            0x80
#define U1GCR_CPHA            0x40
#define U1GCR_ORDER           0x20  // 0: LSB first, 1: MSB first 
#define U1GCR_BAUD_MASK       0x1F
#define U1GCR_BAUD_E(x)       (x)

__sfr __at (0xFD) P0DIR;
__sfr __at (0xFE) P1DIR;

// P2DIR (0xFF) - Port 2 Direction
__sfr __at (0xFF) P2DIR;

// ms P2DIR bits
#define P2DIR_PRIP0_0                     (0x00 << 6)
#define P2DIR_PRIP0_1                     (0x01 << 6)
#define P2DIR_PRIP0_2                     (0x02 << 6)
#define P2DIR_PRIP0_3                     (0x03 << 6)

#define P2DIR_DIRP2_4                     (0x10)
#define P2DIR_DIRP2_3                     (0x08)
#define P2DIR_DIRP2_2                     (0x04)
#define P2DIR_DIRP2_1                     (0x02)
#define P2DIR_DIRP2_0                     (0x01)


static __xdata __at (0xdf00) unsigned char SYNC1;
static __xdata __at (0xdf01) unsigned char SYNC0;
static __xdata __at (0xdf02) unsigned char PKTLEN;
static __xdata __at (0xdf03) unsigned char PKTCTRL1;
static __xdata __at (0xdf04) unsigned char PKTCTRL0;
static __xdata __at (0xdf05) unsigned char ADDR;
static __xdata __at (0xdf06) unsigned char CHANNR;
static __xdata __at (0xdf07) unsigned char FSCTRL1;
static __xdata __at (0xdf08) unsigned char FSCTRL0;
static __xdata __at (0xdf09) unsigned char FREQ2;
static __xdata __at (0xdf0a) unsigned char FREQ1;
static __xdata __at (0xdf0b) unsigned char FREQ0;
static __xdata __at (0xdf0c) unsigned char MDMCFG4;
static __xdata __at (0xdf0d) unsigned char MDMCFG3;
static __xdata __at (0xdf0e) unsigned char MDMCFG2;
static __xdata __at (0xdf0f) unsigned char MDMCFG1;
static __xdata __at (0xdf10) unsigned char MDMCFG0;
static __xdata __at (0xdf11) unsigned char DEVIATN;
static __xdata __at (0xdf12) unsigned char MCSM2;
static __xdata __at (0xdf13) unsigned char MCSM1;
static __xdata __at (0xdf14) unsigned char MCSM0;
static __xdata __at (0xdf15) unsigned char FOCCFG;
static __xdata __at (0xdf16) unsigned char BSCFG;
static __xdata __at (0xdf17) unsigned char AGCCTRL2;
static __xdata __at (0xdf18) unsigned char AGCCTRL1;
static __xdata __at (0xdf19) unsigned char AGCCTRL0;
static __xdata __at (0xdf1a) unsigned char FREND1;
static __xdata __at (0xdf1b) unsigned char FREND0;
static __xdata __at (0xdf1c) unsigned char FSCAL3;
static __xdata __at (0xdf1d) unsigned char FSCAL2;
static __xdata __at (0xdf1e) unsigned char FSCAL1;
static __xdata __at (0xdf1f) unsigned char FSCAL0;
static __xdata __at (0xdf23) unsigned char TEST2;
static __xdata __at (0xdf24) unsigned char TEST1;
static __xdata __at (0xdf25) unsigned char TEST0;
static __xdata __at (0xdf27) unsigned char PA_TABLE7;
static __xdata __at (0xdf28) unsigned char PA_TABLE6;
static __xdata __at (0xdf29) unsigned char PA_TABLE5;
static __xdata __at (0xdf2a) unsigned char PA_TABLE4;
static __xdata __at (0xdf2b) unsigned char PA_TABLE3;
static __xdata __at (0xdf2c) unsigned char PA_TABLE2;
static __xdata __at (0xdf2d) unsigned char PA_TABLE1;
static __xdata __at (0xdf2e) unsigned char PA_TABLE0;
static __xdata __at (0xdf2f) unsigned char IOCFG2;
static __xdata __at (0xdf30) unsigned char IOCFG1;
static __xdata __at (0xdf31) unsigned char IOCFG0;
static __xdata __at (0xdf36) unsigned char PARTNUM;
static __xdata __at (0xdf37) unsigned char VERSION;
static __xdata __at (0xdf38) unsigned char FREQEST;
static __xdata __at (0xdf39) unsigned char LQI;
static __xdata __at (0xdf3a) unsigned char RSSI;

// 0xDF3B: MARCSTATE - Main Radio Control State Machine State
static __xdata __at (0xdf3b) unsigned char MARCSTATE;
#define MARC_STATE_SLEEP                  0x00
#define MARC_STATE_IDLE                   0x01
#define MARC_STATE_VCOON_MC               0x03
#define MARC_STATE_REGON_MC               0x04
#define MARC_STATE_MANCAL                 0x05
#define MARC_STATE_VCOON                  0x06
#define MARC_STATE_REGON                  0x07
#define MARC_STATE_STARTCAL               0x08
#define MARC_STATE_BWBOOST                0x09
#define MARC_STATE_FS_LOCK                0x0A
#define MARC_STATE_IFADCON                0x0B
#define MARC_STATE_ENDCAL                 0x0C
#define MARC_STATE_RX                     0x0D
#define MARC_STATE_RX_END                 0x0E
#define MARC_STATE_RX_RST                 0x0F
#define MARC_STATE_TXRX_SWITCH            0x10
#define MARC_STATE_RX_OVERFLOW            0x11
#define MARC_STATE_FSTXON                 0x12
#define MARC_STATE_TX                     0x13
#define MARC_STATE_TX_END                 0x14
#define MARC_STATE_RXTX_SWITCH            0x15
#define MARC_STATE_TX_UNDERFLOW           0x16

static __xdata __at (0xdf3c) unsigned char PKTSTATUS;
static __xdata __at (0xdf3d) unsigned char VCO_VC_DAC;


struct DmaDescr {
   //SDCC allocates bitfields lo-to-hi
   uint8_t srcAddrHi, srcAddrLo;
   uint8_t dstAddrHi, dstAddrLo;
   uint8_t lenHi     : 5;
   uint8_t vlen      : 3;
   uint8_t lenLo;
   uint8_t trig      : 5;
   uint8_t tmode     : 2;
   uint8_t wordSize  : 1;
   uint8_t priority  : 2;
   uint8_t m8        : 1;
   uint8_t irqmask      : 1;
   uint8_t dstinc    : 2;
   uint8_t srcinc    : 2;
};

#define UCSR_ACTIVE     0x01  // USART transmit/receive active status 0:idle 1:busy
#define UCSR_TX_BYTE    0x02  // Transmit byte status 0:Byte not transmitted 1:Last byte transmitted
#define UCSR_RX_BYTE    0x04  // Receive byte status 0:No byte received 1:Received byte ready
#define UCSR_ERR        0x08  // UART parity error status 0:No error 1:parity error
#define UCSR_FE         0x10  // UART framing error status 0:No error 1:incorrect stop bit level
#define UCSR_SLAVE      0x20  // SPI master or slave mode select 0:master 1:slave
#define UCSR_RE         0x40  // UART receiver enable 0:disabled 1:enabled
#define UCSR_MODE       0x80  // USART mode select 0:SPI 1:UART

// P2SEL bits
#define P2SEL_PRI3P1    0x40  // USART1 has priority over USART0 when assigned same pins
#define P2SEL_PRI2P1    0x20  // TIMER3 has priority over USART1 when assigned same pins
#define P2SEL_PRI1P1    0x10  // TIMER4 has priority over TIMER1 when assigned same pins
#define P2SEL_PRI0P1    0x08  // TIMER1 has priority over USART0 when assigned same pins
#define P2SEL_SELP2_4   0x04  // P2.4 Peripheral function
#define P2SEL_SELP2_3   0x02  // P2.3 Peripheral function
#define P2SEL_SELP2_0   0x01  // P2.0 Peripheral function 


#endif
