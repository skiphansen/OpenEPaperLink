#define __packed

#include <string.h>
#include "asmUtil.h"
#include "printf.h"
#include "radio.h"
#include "comms.h"

static uint8_t __xdata mCommsBuf[COMMS_MAX_PACKET_SZ];
uint8_t __xdata mLastLqi = 0;
int8_t __xdata mLastRSSI = 0;

int8_t commsRxUnencrypted(void __xdata *data) 
{
   uint8_t __xdata *dstData = (uint8_t __xdata *)data;
   uint8_t __xdata *__xdata rxedBuf;
   int8_t ret;

   ret = radioRxDequeuePktGet((void __xdata *__xdata) &rxedBuf,
                              &mLastLqi,&mLastRSSI);

   if(ret < 0) {
      ret = COMMS_RX_ERR_NO_PACKETS;
   } 
   else {
      xMemCopyShort(dstData,rxedBuf,ret);
      pr("Got %d byte packet",ret);
      uint8_t i;
      for(i = 0; i < ret; i++) {
         if((i & 0xf) == 0) {
            pr("\n");
         }
         pr("%02x ",dstData[i]);
      }
      pr("\n");
      radioRxDequeuedPktRelease();
   }

   return ret;
}

bool commsTxUnencrypted(const void __xdata *packetP, uint8_t len) 
{
   const uint8_t __xdata *packet = (const uint8_t __xdata *)packetP;

   if(len > COMMS_MAX_PACKET_SZ) {
      return false;
   }
   memset(mCommsBuf, 0, COMMS_MAX_PACKET_SZ);
   xMemCopyShort(mCommsBuf + 1, packet, len);

   mCommsBuf[0] = len + RADIO_PAD_LEN_BY;

   return radioTx(mCommsBuf);;
}

