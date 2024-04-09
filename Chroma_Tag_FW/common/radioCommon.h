#ifndef _RADIO_COMMON_H_
#define _RADIO_COMMON_H_

#include <stdbool.h>
#include <stdint.h>

//sub-GHz 866 Mhz channels start at 100
#define FIRST_866_CHAN              (100)
#define NUM_866_CHANNELS            (6)

//sub-GHz 915 Mhz channels start at 200
#define FIRST_915_CHAN              (200)
#define NUM_915_CHANNELS            (25)

#define RADIO_MAX_PACKET_LEN        (125) //useful payload, not including the crc

#define ADDR_MODE_NONE              (0)
#define ADDR_MODE_SHORT             (2)
#define ADDR_MODE_LONG              (3)

#define FRAME_TYPE_BEACON           (0)
#define FRAME_TYPE_DATA             (1)
#define FRAME_TYPE_ACK              (2)
#define FRAME_TYPE_MAC_CMD          (3)

#define SHORT_MAC_UNUSED            (0x10000000UL) //for radioRxFilterCfg's myShortMac

extern uint8_t __xdata inBuffer[128];
// Lowest reading of battery voltage while transmitting
extern uint16_t __xdata gTxBattV;

void radioInit(void);
//waits for tx end
void radioTx(const void __xdata *packet);

void radioRxEnable(__bit on);

#pragma callee_saves radioSetTxPower
void radioSetTxPower(int8_t dBm);   //-30..+10 dBm

#pragma callee_saves radioSetChannel
void radioSetChannel(uint8_t ch);

void radioRxFlush(void);
int8_t radioRx(void);

#endif
