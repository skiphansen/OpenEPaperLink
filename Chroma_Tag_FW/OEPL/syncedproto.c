#define __packed

#include "syncedproto.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "asmUtil.h"
#include "comms.h"
#include "cpu.h"
#include "drawing.h"
#include "eeprom.h"
// #include "i2cdevices.h"
#include "powermgt.h"
#include "printf.h"
#include "../oepl-definitions.h"
#include "../oepl-proto.h"
#include "radio.h"
#include "screen.h"
#include "settings.h"
#include "sleep.h"
#include "timer.h"
#include "userinterface.h"
#include "wdt.h"
// #include "uart.h"
// #include "flash.h"
#include "logging.h"

// download-stuff
/* oepl-proto.h has:
#define BLOCK_PART_DATA_SIZE 99
#define BLOCK_MAX_PARTS 42
#define BLOCK_DATA_SIZE 4096UL
#define BLOCK_XFER_BUFFER_SIZE BLOCK_DATA_SIZE + sizeof(struct blockData)
#define BLOCK_REQ_PARTS_BYTES 6
 
which were picked from tags which have 8k of SRAM, we only have 4K so 
this will not do! 
*/ 

#undef BLOCK_DATA_SIZE
#undef BLOCK_XFER_BUFFER_SIZE

#define BLOCK_DATA_SIZE    (1UL*BLOCK_PART_DATA_SIZE)
#define BLOCK_XFER_BUFFER_SIZE BLOCK_DATA_SIZE + sizeof(struct blockData)


uint8_t __xdata blockbuffer[BLOCK_XFER_BUFFER_SIZE];
static struct blockRequest __xdata curBlock;  // used by the block-requester, contains the next request that we'll send
static uint8_t __xdata curDispDataVer[8];
static struct AvailDataInfo __xdata xferDataInfo;  // holds the AvailDataInfo during the transfer
static bool __xdata requestPartialBlock;         // if we should ask the AP to get this block from the host or not
#define BLOCK_TRANSFER_ATTEMPTS 5

static uint8_t xferImgSlot = 0xFF;          // holds current transfer slot in progress
uint8_t __xdata curImgSlot = 0xFF;          // currently shown image
static uint32_t __xdata curHighSlotId;  // current highest ID, will be incremented before getting written to a new slot
static uint8_t __xdata nextImgSlot;     // next slot in sequence for writing
static uint8_t __xdata imgSlots;
static uint32_t __xdata eeSize;

#define OTA_UPDATE_SIZE 0x8000   // 32k

// stuff we need to keep track of related to the network/AP
uint8_t __xdata APmac[8];
uint16_t __xdata APsrcPan;
uint8_t __xdata mSelfMac[8];
static uint8_t __xdata seq;
uint8_t __xdata currentChannel;

// buffer we use to prepare/read packets
uint8_t __xdata inBuffer[128];
static uint8_t __xdata outBuffer[128];

// determines if the tagAssociated loop in main.c performs a rapid next checkin
bool __xdata fastNextCheckin;

struct MacFrameBcast __xdata gBcastFrame;

// other stuff we shouldn't have to put here...
static uint32_t __xdata markerValid = EEPROM_IMG_VALID;

extern void executeCommand(uint8_t cmd);  // this is defined in main.c

// tools

static bool pktIsUnicast(void);
static bool pktIsBcast(void);

uint8_t __xdata getPacketType() 
{
   if(pktIsBcast()) {
      return ((uint8_t *)inBuffer)[sizeof(struct MacFrameBcast)];
   }
   if(pktIsUnicast()) {
   // normal frame
      return ((uint8_t *)inBuffer)[sizeof(struct MacFrameNormal)];
   }
   return 0;
}

static bool pktIsUnicast() 
{
   #define fcs ((const struct MacFcs *__xdata) inBuffer)
   if(fcs->frameType == 1 && fcs->destAddrType == 3 && fcs->srcAddrType == 3
      && fcs->panIdCompressed == 1) 
   {  // normal frame
      return true;
   }
   return false;
   #undef fcs
}

static bool pktIsBcast()
{
   #define fcs ((const struct MacFcs *__xdata) inBuffer)
   if(fcs->frameType == 1 && fcs->destAddrType == 2 && fcs->srcAddrType == 3
      && fcs->panIdCompressed == 0)
   {
   // broadcast frame
      return true;
   }
   return false;
   #undef fcs
}

bool checkCRC(const void *p, const uint8_t len) 
{
   uint8_t total = 0;
   for(uint8_t c = 1; c < len; c++) {
      total += ((uint8_t *)p)[c];
   }
   // pr("CRC: rx %d, calc %d\n", ((uint8_t *)p)[0], total);
   return((uint8_t *)p)[0] == total;
}

static void addCRC(void *p, const uint8_t len) 
{
   uint8_t total = 0;
   for(uint8_t c = 1; c < len; c++) {
      total += ((uint8_t *)p)[c];
   }
   ((uint8_t *)p)[0] = total;
}

// radio stuff
static void sendPing() 
{
   outBuffer[0] = sizeof(struct MacFrameBcast) + 1 + 2;
   UpdateBcastFrame();
   outBuffer[sizeof(struct MacFrameBcast) + 1] = PKT_PING;
   radioTx(outBuffer);
}

uint8_t detectAP(const uint8_t channel) __reentrant 
{
   static uint32_t __xdata t;

#if 0
   radioRxEnable(false, true);
   radioSetChannel(channel);
#else
   radioInit();
   radioRxEnable(false, true);
   radioSetChannel(200);
#endif
   radioRxFlush();
   radioRxEnable(true, true);
   for(uint8_t c = 1; c <= MAXIMUM_PING_ATTEMPTS; c++) {
      sendPing();
      t = timerGet() + (TIMER_TICKS_PER_MS * PING_REPLY_WINDOW * 100);
      while(timerGet() < t) {
         static int8_t __xdata ret;
         ret = radioRx();
         if(ret > 1) {
            if(
#if 0
               inBuffer[sizeof(struct MacFrameNormal) + 1] == channel &&
#endif
               getPacketType() == PKT_PONG) 
            {
               if(pktIsUnicast()) {
                  static struct MacFrameNormal *__xdata f;
                  f = (struct MacFrameNormal *)inBuffer;
                  xMemCopyShort(APmac, (void *) f->src, 8);
                  APsrcPan = f->pan;
                  return c;
               }
            }
         }
      }
   }
   return 0;
}

// data xfer stuff
static void sendShortAvailDataReq() 
{
   outBuffer[0] = sizeof(struct MacFrameBcast) + 1 + 2;
   outBuffer[sizeof(struct MacFrameBcast) + 1] = PKT_AVAIL_DATA_SHORTREQ;
   UpdateBcastFrame();
   radioTx(outBuffer);
}

// outBuffer: 
// len   contents
// 1     <tx data len> 
// 17    <MacFrameBcast>
// 1     PKT_AVAIL_DATA_REQ
// 21    <AvailDataReq>
// 1     <crc> (actually a 1 byte checksum of <AvailDataReq>)
// Total tx len data = 42 (including frame len)
static void sendAvailDataReq() 
{
   struct AvailDataReq *__xdata availreq = (struct AvailDataReq *)(outBuffer + 2 + sizeof(struct MacFrameBcast));
   outBuffer[0] = sizeof(struct MacFrameBcast) + sizeof(struct AvailDataReq) + 2 + 2;
   UpdateBcastFrame();
   outBuffer[sizeof(struct MacFrameBcast) + 1] = PKT_AVAIL_DATA_REQ;
// TODO: send some (more) meaningful data
   availreq->hwType = HW_TYPE;
   availreq->wakeupReason = wakeUpReason;
   availreq->lastPacketRSSI = mLastRSSI;
   availreq->lastPacketLQI = mLastLqi;
   availreq->temperature = temperature;
   availreq->batteryMv = batteryVoltage;
   availreq->capabilities = 0;
   availreq->tagSoftwareVersion = fwVersion;
   availreq->currentChannel = currentChannel;
   availreq->customMode = tagSettings.customMode;
   addCRC(availreq, sizeof(struct AvailDataReq));
   radioTx(outBuffer);
}

struct AvailDataInfo *__xdata getAvailDataInfo() 
{
   radioRxEnable(true, true);
   uint32_t __xdata t;
   for(uint8_t c = 0; c < DATA_REQ_MAX_ATTEMPTS; c++) {
      PROTO_LOG("Full AvlData, try %d\n",c);
      sendAvailDataReq();
      t = timerGet() + (TIMER_TICKS_PER_MS * DATA_REQ_RX_WINDOW_SIZE);
      while(timerGet() < t) {
         int8_t __xdata ret = radioRx();
         if(ret > 1) {
            if(getPacketType() == PKT_AVAIL_DATA_INFO) {
               if(checkCRC(inBuffer + sizeof(struct MacFrameNormal) + 1, sizeof(struct AvailDataInfo))) {
                  struct MacFrameNormal *__xdata f = (struct MacFrameNormal *)inBuffer;
                  xMemCopyShort(APmac, (void *)f->src, 8);
                  APsrcPan = f->pan;
                  dataReqLastAttempt = c;
                  return(struct AvailDataInfo *)(inBuffer + sizeof(struct MacFrameNormal) + 1);
               }
            }
         }
      }
   }
   dataReqLastAttempt = DATA_REQ_MAX_ATTEMPTS;
   return NULL;
}

struct AvailDataInfo *__xdata getShortAvailDataInfo() 
{
   PROTO_LOG("Short AvlData\n");
   radioRxEnable(true, true);
   uint32_t __xdata t;
   for(uint8_t c = 0; c < DATA_REQ_MAX_ATTEMPTS; c++) {
      sendShortAvailDataReq();
      // sendAvailDataReq();
      t = timerGet() + (TIMER_TICKS_PER_MS * DATA_REQ_RX_WINDOW_SIZE);
      while(timerGet() < t) {
         int8_t __xdata ret = radioRx();
         if(ret > 1) {
            if(getPacketType() == PKT_AVAIL_DATA_INFO) {
               if(checkCRC(inBuffer + sizeof(struct MacFrameNormal) + 1, sizeof(struct AvailDataInfo))) {
                  struct MacFrameNormal *__xdata f = (struct MacFrameNormal *)inBuffer;
                  xMemCopyShort(APmac, (void *)f->src, 8);
                  APsrcPan = f->pan;
                  dataReqLastAttempt = c;
                  return(struct AvailDataInfo *)(inBuffer + sizeof(struct MacFrameNormal) + 1);
               }
            }
         }
      }
   }
   dataReqLastAttempt = DATA_REQ_MAX_ATTEMPTS;
   return NULL;
}

static bool processBlockPart(const struct blockPart *bp) 
{
   uint16_t __xdata start = bp->blockPart * BLOCK_PART_DATA_SIZE;
   uint16_t __xdata size = BLOCK_PART_DATA_SIZE;
   // validate if it's okay to copy data
   if(bp->blockId != curBlock.blockId) {
      return false;
   }
   if(start >= (sizeof(blockbuffer) - 1)) return false;
   if(bp->blockPart > BLOCK_MAX_PARTS) return false;
   if((start + size) > sizeof(blockbuffer)) {
      size = sizeof(blockbuffer) - start;
   }

   if(checkCRC(bp, sizeof(struct blockPart) + BLOCK_PART_DATA_SIZE)) {
      //  copy block data to buffer
      xMemCopy((void *)(blockbuffer + start), (const void *)bp->data, size);
      // we don't need this block anymore, set bit to 0 so we don't request it again
      curBlock.requestedParts[bp->blockPart / 8] &= ~(1 << (bp->blockPart % 8));
      return true;
   } else {
      return false;
   }
}

static bool blockRxLoop(const uint32_t timeout) 
{
   uint32_t __xdata t;
   bool success = false;
   radioRxEnable(true, true);
   t = timerGet() + (TIMER_TICKS_PER_MS * (timeout + 20));
   while(timerGet() < t) {
      int8_t __xdata ret = radioRx();
      if(ret > 1) {
         if(getPacketType() == PKT_BLOCK_PART) {
            struct blockPart *bp = (struct blockPart *)(inBuffer + sizeof(struct MacFrameNormal) + 1);
            success = processBlockPart(bp);
         }
      }
   }
   radioRxEnable(false, true);
   radioRxFlush();
   return success;
}

static struct blockRequestAck *__xdata continueToRX() 
{
   struct blockRequestAck *ack = (struct blockRequestAck *)(inBuffer + sizeof(struct MacFrameNormal) + 1);
   ack->pleaseWaitMs = 0;
   return ack;
}

static void sendBlockRequest() 
{
   xMemSet(outBuffer,0,sizeof(struct MacFrameNormal) + sizeof(struct blockRequest) + 2 + 2);
   struct MacFrameNormal *__xdata f = (struct MacFrameNormal *)(outBuffer + 1);
   struct blockRequest *__xdata blockreq = (struct blockRequest *)(outBuffer + 2 + sizeof(struct MacFrameNormal));
   outBuffer[0] = sizeof(struct MacFrameNormal) + sizeof(struct blockRequest) + 2 + 2;
   if(requestPartialBlock) {
      ;
      outBuffer[sizeof(struct MacFrameNormal) + 1] = PKT_BLOCK_PARTIAL_REQUEST;
   } else {
      outBuffer[sizeof(struct MacFrameNormal) + 1] = PKT_BLOCK_REQUEST;
   }
   xMemCopyShort((void *)(f->src), (void *) mSelfMac, 8);
   xMemCopyShort((void *)(f->dst), (void *)APmac, 8);
   f->fcs.frameType = 1;
   f->fcs.secure = 0;
   f->fcs.framePending = 0;
   f->fcs.ackReqd = 0;
   f->fcs.panIdCompressed = 1;
   f->fcs.destAddrType = 3;
   f->fcs.frameVer = 0;
   f->fcs.srcAddrType = 3;
   f->seq = seq++;
   f->pan = APsrcPan;
   xMemCopyShort((void *)blockreq, (void *)&curBlock, sizeof(struct blockRequest));
   addCRC(blockreq, sizeof(struct blockRequest));
   radioTx(outBuffer);
}

static struct blockRequestAck *__xdata performBlockRequest() __reentrant 
{
   static uint32_t __xdata t;
   radioRxEnable(true, true);
   radioRxFlush();
   for(uint8_t c = 0; c < 30; c++) {
      sendBlockRequest();
      t = timerGet() + (TIMER_TICKS_PER_MS * (7UL + c / 10));
      do {
         static int8_t __xdata ret;
         ret = radioRx();
         if(ret > 1) {
            switch(getPacketType()) {
               case PKT_BLOCK_REQUEST_ACK:
                  if(checkCRC((inBuffer + sizeof(struct MacFrameNormal) + 1), sizeof(struct blockRequestAck)))
                     return(struct blockRequestAck *)(inBuffer + sizeof(struct MacFrameNormal) + 1);
                  break;

               case PKT_BLOCK_PART:
               // block already started while we were waiting for a get block reply
               // pr("!");
               // processBlockPart((struct blockPart *)(inBuffer + sizeof(struct MacFrameNormal) + 1));
                  return continueToRX();
                  break;

               case PKT_CANCEL_XFER:
                  return NULL;

               default:
                  PROTO_LOG("pkt type %02X\n", getPacketType());
                  break;
            }
         }
      } while(timerGet() < t);
   }
   return continueToRX();
   // return NULL;
}

static void sendXferCompletePacket() 
{
   xMemSet(outBuffer,0,sizeof(struct MacFrameNormal) + 2 + 4);
   struct MacFrameNormal *__xdata f = (struct MacFrameNormal *)(outBuffer + 1);
   outBuffer[0] = sizeof(struct MacFrameNormal) + 2 + 2;
   outBuffer[sizeof(struct MacFrameNormal) + 1] = PKT_XFER_COMPLETE;
   xMemCopyShort((void *)(f->src), (void *) mSelfMac, 8);
   xMemCopyShort((void *)(f->dst), (void *) APmac, 8);
   f->fcs.frameType = 1;
   f->fcs.secure = 0;
   f->fcs.framePending = 0;
   f->fcs.ackReqd = 0;
   f->fcs.panIdCompressed = 1;
   f->fcs.destAddrType = 3;
   f->fcs.frameVer = 0;
   f->fcs.srcAddrType = 3;
   f->pan = APsrcPan;
   f->seq = seq++;
   radioTx(outBuffer);
}

static void sendXferComplete() 
{
   radioRxEnable(true, true);

   PROTO_LOG("XFC ");
   for(uint8_t c = 0; c < 16; c++) {
      sendXferCompletePacket();
      uint32_t __xdata start = timerGet();
      while((timerGet() - start) < (TIMER_TICKS_PER_MS * 6UL)) {
         int8_t __xdata ret = radioRx();
         if(ret > 1) {
            if(getPacketType() == PKT_XFER_COMPLETE_ACK) {
               PROTO_LOG("ACK\n");
               return;
            }
         }
      }
   }
   PROTO_LOG("NACK!\n");
   return;
}

bool validateBlockData() 
{
   struct blockData *bd = (struct blockData *)blockbuffer;
   // pr("expected len = %04X, checksum=%04X\n", bd->size, bd->checksum);
   uint16_t t = 0;
   for(uint16_t c = 0; c < bd->size; c++) {
      t += bd->data[c];
   }
   return bd->checksum == t;
}

// EEprom related stuff
static uint32_t getAddressForSlot(const uint8_t s) 
{
   return EEPROM_IMG_START + (EEPROM_IMG_EACH * s);
}

static void getNumSlots() 
{
   eeSize = eepromGetSize();
   uint16_t nSlots = mathPrvDiv32x16(eeSize - EEPROM_IMG_START, EEPROM_IMG_EACH >> 8) >> 8;
   if(eeSize < EEPROM_IMG_START || !nSlots) {
      EEPROM_LOG("EEPROM: eeprom is too small\n");
      while(1)
         ;
   }
   else if(nSlots >> 8) {
      EEPROM_LOG("EEPROM: eeprom is too big, some will be unused\n");
      imgSlots = 254;
   }
   else {
      imgSlots = nSlots;
   }
   PROTO_LOG("%d slots\n", imgSlots);
}

static uint8_t findSlotVer(const uint8_t *ver) 
{
#ifdef DEBUGBLOCKS
   return 0xFF;
#endif
   // return 0xFF;  // remove me! This forces the tag to re-download each and every upload without checking if it's already in the eeprom somewhere
   for(uint8_t c = 0; c < imgSlots; c++) {
      struct EepromImageHeader __xdata *eih = (struct EepromImageHeader __xdata *)blockbuffer;
      eepromRead(getAddressForSlot(c), eih, sizeof(struct EepromImageHeader));
      if(xMemEqual4(&eih->validMarker, &markerValid)) {
         if(xMemEqual(&eih->version, (void *)ver, 8)) {
            return c;
         }
      }
   }
   return 0xFF;
}

// making this reentrant saves one byte of idata...
uint8_t __xdata findSlotDataTypeArg(uint8_t arg) __reentrant 
{
   arg &= (0xF8);  // unmatch with the 'preload' bit and LUT bits
   for(uint8_t c = 0; c < imgSlots; c++) {
      struct EepromImageHeader __xdata *eih = (struct EepromImageHeader __xdata *)blockbuffer;
      eepromRead(getAddressForSlot(c), eih, sizeof(struct EepromImageHeader));
      if(xMemEqual4(&eih->validMarker, &markerValid)) {
         if((eih->argument & 0xF8) == arg) {
            return c;
         }
      }
   }
   return 0xFF;
}

uint8_t getEepromImageDataArgument(const uint8_t slot) 
{
   struct EepromImageHeader __xdata *eih = (struct EepromImageHeader __xdata *)blockbuffer;
   eepromRead(getAddressForSlot(slot), eih, sizeof(struct EepromImageHeader));
   return eih->argument;
}

uint8_t __xdata findNextSlideshowImage(uint8_t start) __reentrant 
{
   struct EepromImageHeader __xdata *eih = (struct EepromImageHeader __xdata *)blockbuffer;
   uint8_t c = start;
   while(1) {
      c++;
      if(c > imgSlots) c = 0;
      if(c == start) return c;
      eepromRead(getAddressForSlot(c), eih, sizeof(struct EepromImageHeader));
      if(xMemEqual4(&eih->validMarker, &markerValid)) {
         if((eih->argument & 0xF8) == (CUSTOM_IMAGE_SLIDESHOW << 3)) {
            return c;
         }
      }
   }
}

static void eraseUpdateBlock() 
{
   eepromErase(eeSize - OTA_UPDATE_SIZE, OTA_UPDATE_SIZE / EEPROM_ERZ_SECTOR_SZ);
}

static void eraseImageBlock(const uint8_t c) 
{
   eepromErase(getAddressForSlot(c), EEPROM_IMG_EACH / EEPROM_ERZ_SECTOR_SZ);
}

static void saveUpdateBlockData(uint8_t blockId) 
{
   eepromWrite(eeSize - OTA_UPDATE_SIZE + (blockId * BLOCK_DATA_SIZE), blockbuffer + sizeof(struct blockData), BLOCK_DATA_SIZE);
}

static void saveImgBlockData(const uint8_t imgSlot, const uint8_t blockId) 
{
   uint16_t length = EEPROM_IMG_EACH - (sizeof(struct EepromImageHeader) + (blockId * BLOCK_DATA_SIZE));
   if(length > 4096) length = 4096;

   eepromWrite(getAddressForSlot(imgSlot) + sizeof(struct EepromImageHeader) + (blockId * BLOCK_DATA_SIZE), blockbuffer + sizeof(struct blockData), length);
}

void eraseImageBlocks() 
{
   for(uint8_t c = 0; c < imgSlots; c++) {
      eraseImageBlock(c);
   }
}

void drawImageFromEeprom(const uint8_t imgSlot, uint8_t lut) 
{
   lut;
   drawImageAtAddress(getAddressForSlot(imgSlot));
}

static uint32_t getHighSlotId() 
{
   uint32_t temp = 0;
   for(uint8_t __xdata c = 0; c < imgSlots; c++) {
      struct EepromImageHeader __xdata *eih = (struct EepromImageHeader __xdata *)blockbuffer;
      eepromRead(getAddressForSlot(c), eih, sizeof(struct EepromImageHeader));
      if(xMemEqual4(&eih->validMarker, &markerValid)) {
         if(temp < eih->id) {
            temp = eih->id;
            nextImgSlot = c;
         }
      }
   }
   PROTO_LOG("high id=%lu in %d\n", temp, nextImgSlot);
   return temp;
}

// data transfer stuff
static uint8_t __xdata partsThisBlock;
static uint8_t __xdata blockAttempts;  // these CAN be local to the function, but for some reason, they won't survive sleep?
                                           // they get overwritten with  7F 32 44 20 00 00 00 00 11, I don't know why.

static bool getDataBlock(const uint16_t blockSize) 
{
   blockAttempts = BLOCK_TRANSFER_ATTEMPTS;
   if(blockSize == BLOCK_DATA_SIZE) {
      partsThisBlock = BLOCK_MAX_PARTS;
      xMemSet(curBlock.requestedParts,0xFF,BLOCK_REQ_PARTS_BYTES);
   }
   else {
      partsThisBlock = (sizeof(struct blockData) + blockSize) / BLOCK_PART_DATA_SIZE;
      if((sizeof(struct blockData) + blockSize) % BLOCK_PART_DATA_SIZE) partsThisBlock++;
      xMemSet(curBlock.requestedParts,0x00,BLOCK_REQ_PARTS_BYTES);
      for(uint8_t c = 0; c < partsThisBlock; c++) {
         curBlock.requestedParts[c / 8] |= (1 << (c % 8));
      }
   }

   requestPartialBlock = false;  // this forces the AP to request the block data from the host

   while(blockAttempts--) {
#ifndef DEBUGBLOCKS
      pr("REQ %d ", curBlock.blockId);
#else
      pr("REQ %d[", curBlock.blockId);
      for(uint8_t c = 0; c < BLOCK_MAX_PARTS; c++) {
         if((c != 0) && (c % 8 == 0)) pr("][");
         if(curBlock.requestedParts[c / 8] & (1 << (c % 8))) {
            pr("R");
         } else {
            pr("_");
         }
      }
      pr("]\n");
#endif
      powerUp(INIT_RADIO);
      struct blockRequestAck *__xdata ack = performBlockRequest();

      if(ack == NULL) {
         PROTO_LOG("Cancelled req\n");
         return false;
      }
      if(ack->pleaseWaitMs) {  // SLEEP - until the AP is ready with the data
         if(ack->pleaseWaitMs < 35) {
            timerDelay(ack->pleaseWaitMs * TIMER_TICKS_PER_MS);
         } else {
            doSleep(ack->pleaseWaitMs - 10);
            powerUp(INIT_UART | INIT_RADIO);
            radioRxEnable(true, true);
         }
      } else {
         // immediately start with the reception of the block data
      }
      blockRxLoop(290);  // BLOCK RX LOOP - receive a block, until the timeout has passed
      powerDown(INIT_RADIO);

#ifdef DEBUGBLOCKS
      pr("RX  %d[", curBlock.blockId);
      for(uint8_t c = 0; c < BLOCK_MAX_PARTS; c++) {
         if((c != 0) && (c % 8 == 0)) pr("][");
         if(curBlock.requestedParts[c / 8] & (1 << (c % 8))) {
            pr(".");
         } else {
            pr("R");
         }
      }
      pr("]\n");
#endif
      // check if we got all the parts we needed, e.g: has the block been completed?
      bool blockComplete = true;
      for(uint8_t c1 = 0; c1 < partsThisBlock; c1++) {
         if(curBlock.requestedParts[c1 / 8] & (1 << (c1 % 8))) blockComplete = false;
      }

      if(blockComplete) {
#ifndef DEBUGBLOCKS
         pr("- COMPLETE\n");
#endif
         if(validateBlockData()) {
            // block download complete, validated
            return true;
         } else {
            for(uint8_t c = 0; c < partsThisBlock; c++) {
               curBlock.requestedParts[c / 8] |= (1 << (c % 8));
            }
            requestPartialBlock = false;
            PROTO_LOG("blk failed validation!\n");
         }
      }
      else {
#ifndef DEBUGBLOCKS
         pr("- INCOMPLETE\n");
#endif
         // block incomplete, re-request a partial block
         requestPartialBlock = true;
      }
   }
   PROTO_LOG("failed getting block\n");
   return false;
}

static uint16_t __xdata dataRequestSize;
static uint16_t __xdata otaSize;

#if 0
static bool downloadFWUpdate(const struct AvailDataInfo *__xdata avail) 
{
   return false;
   // check if we already started the transfer of this information & haven't completed it
   if(xMemEqual((const void *__xdata) & avail->dataVer, (const void *__xdata) & xferDataInfo.dataVer, 8) && xferDataInfo.dataSize) {
      // looks like we did. We'll carry on where we left off.
   } else {
#if defined(DEBUGOTA)
      pr("OTA: Start update!\n");
#endif
      // start, or restart the transfer from 0. Copy data from the AvailDataInfo struct, and the struct intself. This forces a new transfer
      curBlock.blockId = 0;
      xMemCopy8(&(curBlock.ver), &(avail->dataVer));
      curBlock.type = avail->dataType;
      xMemCopyShort(&xferDataInfo, (void *)avail, sizeof(struct AvailDataInfo));
      eraseUpdateBlock();
      otaSize = xferDataInfo.dataSize;
   }

   while(xferDataInfo.dataSize) {
      wdt10s();
      if(xferDataInfo.dataSize > BLOCK_DATA_SIZE) {
         // more than one block remaining
         dataRequestSize = BLOCK_DATA_SIZE;
      } else {
         // only one block remains
         dataRequestSize = xferDataInfo.dataSize;
      }
      if(getDataBlock(dataRequestSize)) {
         // succesfully downloaded datablock, save to eeprom
         powerUp(INIT_EEPROM);
         saveUpdateBlockData(curBlock.blockId);
         powerDown(INIT_EEPROM);
         curBlock.blockId++;
         xferDataInfo.dataSize -= dataRequestSize;
      } else {
         // failed to get the block we wanted, we'll stop for now, maybe resume later
         return false;
      }
   }
   wdt60s();
   powerUp(INIT_EEPROM);
   if(!validateMD5(eeSize - OTA_UPDATE_SIZE, otaSize)) {
#if defined(DEBUGOTA)
      pr("OTA: MD5 verification failed!\n");
#endif
      // if not valid, restart transfer from the beginning
      curBlock.blockId = 0;
      powerDown(INIT_EEPROM);
      return false;
   }
#if defined(DEBUGOTA)
   pr("OTA: MD5 pass!\n");
#endif

   // no more data, download complete
   return true;
}
#endif

uint16_t __xdata imageSize;
static bool downloadImageDataToEEPROM(const struct AvailDataInfo *__xdata avail) 
{
// check if we already started the transfer of this information & haven't completed it
   if(xMemEqual((const void *__xdata) &avail->dataVer,(const void *__xdata) & xferDataInfo.dataVer, 8) 
      && (xferDataInfo.dataTypeArgument == avail->dataTypeArgument) 
      && xferDataInfo.dataSize) 
   {  // looks like we did. We'll carry on where we left off.
      PROTO_LOG("restarting img dl\n");
   }
   else {
   // new transfer
      powerUp(INIT_EEPROM);

   // go to the next image slot
      uint8_t startingSlot = nextImgSlot;

   // if we encounter a special image type, start looking from slot 0, to 
   // prevent the image being overwritten when we do an OTA update
      if(avail->dataTypeArgument & 0xFC != 0x00) {
         startingSlot = 0;
      }

      while(1) {
         nextImgSlot++;
         if(nextImgSlot >= imgSlots) nextImgSlot = 0;
         if(nextImgSlot == startingSlot) {
            powerDown(INIT_EEPROM);
            PROTO_LOG("No slots\n");
            return true;
         }
         struct EepromImageHeader __xdata *eih = (struct EepromImageHeader __xdata *)blockbuffer;
         eepromRead(getAddressForSlot(nextImgSlot), eih, sizeof(struct EepromImageHeader));
         // check if the marker is indeed valid
         if(xMemEqual4(&eih->validMarker, &markerValid)) {
            struct imageDataTypeArgStruct *eepromDataArgument = (struct imageDataTypeArgStruct *)&(eih->argument);
            // normal type, we can overwrite this
            if(eepromDataArgument->specialType == 0x00) {
               break;
            }
         }
         else {
            // bullshit marker, so safe to overwrite
            break;
         }
      }

      xferImgSlot = nextImgSlot;

      uint8_t __xdata attempt = 5;
      while(attempt--) {
         if(eepromErase(getAddressForSlot(xferImgSlot), EEPROM_IMG_EACH / EEPROM_ERZ_SECTOR_SZ)) {
            goto eraseSuccess;
         }
      }
      powerDown(INIT_RADIO);
      powerUp(INIT_EPD);
      showNoEEPROM();
      powerDown(INIT_EEPROM | INIT_EPD);
      doSleep(-1);
      wdtDeviceReset();
eraseSuccess:
      powerDown(INIT_EEPROM);
      PROTO_LOG("new dl to %d\n", xferImgSlot);
   // start, or restart the transfer. Copy data from the AvailDataInfo struct, 
   // theand the struct intself. This forces a new transfer
      curBlock.blockId = 0;
      xMemCopy8(&(curBlock.ver), &(avail->dataVer));
      curBlock.type = avail->dataType;
      xMemCopyShort(&xferDataInfo, (void *)avail, sizeof(struct AvailDataInfo));
      imageSize = xferDataInfo.dataSize;
   }

   while(xferDataInfo.dataSize) {
      wdt10s();
      if(xferDataInfo.dataSize > BLOCK_DATA_SIZE) {
      // more than one block remaining
         dataRequestSize = BLOCK_DATA_SIZE;
      }
      else {
      // only one block remains
         dataRequestSize = xferDataInfo.dataSize;
      }
      if(getDataBlock(dataRequestSize)) {
      // succesfully downloaded datablock, save to eeprom
         powerUp(INIT_EEPROM);
         timerDelay(TIMER_TICKS_PER_MS * 100);
#ifdef DEBUGBLOCKS
         pr("BLOCKS: Saving block %d to slot %d\n", curBlock.blockId, xferImgSlot);
#endif
         saveImgBlockData(xferImgSlot, curBlock.blockId);
         timerDelay(TIMER_TICKS_PER_MS * 100);
         powerDown(INIT_EEPROM);
         curBlock.blockId++;
         xferDataInfo.dataSize -= dataRequestSize;
      }
      else {
      // failed to get the block we wanted, we'll stop for now, probably resume later
         return false;
      }
   }
   // no more data, download complete

   // validate MD5
   powerUp(INIT_EEPROM);
// borrow the blockbuffer temporarily
   struct EepromImageHeader __xdata *eih = (struct EepromImageHeader __xdata *)blockbuffer;
   xMemCopy8(&eih->version, &xferDataInfo.dataVer);
   eih->validMarker = EEPROM_IMG_VALID;
   eih->id = ++curHighSlotId;
   eih->size = imageSize;
   eih->dataType = xferDataInfo.dataType;
   eih->argument = xferDataInfo.dataTypeArgument;

#ifdef DEBUGBLOCKS
   pr("BLOCKS: Now writing datatype 0x%02X to slot %d\n", xferDataInfo.dataType, xferImgSlot);
#endif
   eepromWrite(getAddressForSlot(xferImgSlot), eih, sizeof(struct EepromImageHeader));
   powerDown(INIT_EEPROM);

   return true;
}

struct imageDataTypeArgStruct __xdata arg;  // this is related to the function below, but if declared -inside- the function, it gets cleared during sleep...
inline bool processImageDataAvail(struct AvailDataInfo *__xdata avail) 
{
   *((uint8_t *)arg) = avail->dataTypeArgument;
   if(arg.preloadImage) {
      PROTO_LOG("Preloading img type 0x%02X from 0x%02X\n",
                arg.specialType,avail->dataTypeArgument);
      powerUp(INIT_EEPROM);
      switch(arg.specialType) {
      // check if a slot with this argument is already set; if so, erase. Only one of each arg type should exist
         default: {
               uint8_t slot = findSlotDataTypeArg(avail->dataTypeArgument);
               if(slot != 0xFF) {
                  eepromErase(getAddressForSlot(slot), EEPROM_IMG_EACH / EEPROM_ERZ_SECTOR_SZ);
               }
            } break;
            // regular image preload, there can be multiple of this type in the EEPROM
         case CUSTOM_IMAGE_NOCUSTOM: {
               // check if a version of this already exists
               uint8_t slot = findSlotVer(&(avail->dataVer));
               if(slot != 0xFF) {
                  powerUp(INIT_RADIO);
                  sendXferComplete();
                  powerDown(INIT_RADIO);
                  return true;
               }
            } break;
         case CUSTOM_IMAGE_SLIDESHOW:
            break;
      }
      powerDown(INIT_EEPROM);
      PROTO_LOG("dl preload img\n");
      if(downloadImageDataToEEPROM(avail)) {
         // sets xferImgSlot to the right slot
         PROTO_LOG("preload done\n");
         powerUp(INIT_RADIO);
         sendXferComplete();
         powerDown(INIT_RADIO);
         return true;
      }
      else {
         return false;
      }
   }
   else {
      // check if we're currently displaying this data payload
      if(xMemEqual((const void *__xdata) & avail->dataVer, (const void *__xdata)curDispDataVer, 8)) {
         // currently displayed, not doing anything except for sending an XFC
         PROTO_LOG("current img, send xfc\n");
         powerUp(INIT_RADIO);
         sendXferComplete();
         powerDown(INIT_RADIO);
         return true;
      }
      else {
         // currently not displayed

         // try to find the data in the SPI EEPROM
         powerUp(INIT_EEPROM);
         uint8_t findImgSlot = findSlotVer(&(avail->dataVer));
         powerDown(INIT_EEPROM);

         // Is this image already in a slot somewhere
         if(findImgSlot != 0xFF) {
            // found a (complete)valid image slot for this version
            powerUp(INIT_RADIO);
            sendXferComplete();
            powerDown(INIT_RADIO);

            // mark as completed and draw from EEPROM
            xMemCopyShort(&xferDataInfo, (void *)avail, sizeof(struct AvailDataInfo));
            xferDataInfo.dataSize = 0;  // mark as transfer not pending

            wdt60s();
            curImgSlot = findImgSlot;
            powerUp(INIT_EPD | INIT_EEPROM);
            drawImageFromEeprom(findImgSlot, arg.lut);
            powerDown(INIT_EPD | INIT_EEPROM);
         }
         else {
// not found in cache, prepare to download
            PROTO_LOG("dl img\n");
            if(downloadImageDataToEEPROM(avail)) {
// sets xferImgSlot to the right slot
               PROTO_LOG("dl done\n");
               powerUp(INIT_RADIO);
               sendXferComplete();
               powerDown(INIT_RADIO);

               // not preload, draw now
               wdt60s();
               curImgSlot = xferImgSlot;
               powerUp(INIT_EPD | INIT_EEPROM);
               drawImageFromEeprom(xferImgSlot, arg.lut);
               powerDown(INIT_EPD | INIT_EEPROM);
            }
            else {
               return false;
            }
         }

         // keep track on what is currently displayed
         xMemCopy8(curDispDataVer, xferDataInfo.dataVer);
         return true;
      }
   }
}

bool processAvailDataInfo(struct AvailDataInfo *__xdata avail) 
{
   switch(avail->dataType) {
      case DATATYPE_IMG_BMP:
      case DATATYPE_IMG_DIFF:
      case DATATYPE_IMG_RAW_1BPP:
      case DATATYPE_IMG_RAW_2BPP:
         return processImageDataAvail(avail);

      case DATATYPE_TAG_CONFIG_DATA:
         if(xferDataInfo.dataSize == 0 && xMemEqual((const void *__xdata) & avail->dataVer, (const void *__xdata) & xferDataInfo.dataVer, 8)) {
            PROTO_LOG("same as last ignored\n");
            powerUp(INIT_RADIO);
            sendXferComplete();
            powerDown(INIT_RADIO);
            return true;
         }
         curBlock.blockId = 0;
         xMemCopy8(&(curBlock.ver), &(avail->dataVer));
         curBlock.type = avail->dataType;
         xMemCopyShort(&xferDataInfo, (void *)avail, sizeof(struct AvailDataInfo));
         wdt10s();
         if(getDataBlock(avail->dataSize)) {
            xferDataInfo.dataSize = 0;  // mark as transfer not pending
            powerUp(INIT_EEPROM);
            loadSettingsFromBuffer(sizeof(struct blockData) + blockbuffer);
            powerDown(INIT_EEPROM);
            powerUp(INIT_RADIO);
            sendXferComplete();
            powerDown(INIT_RADIO);
            return true;
         }
         return false;
         break;

      case DATATYPE_COMMAND_DATA:
         PROTO_LOG("CMD rx\n");
         powerUp(INIT_RADIO);
         sendXferComplete();
         powerDown(INIT_RADIO);
         executeCommand(avail->dataTypeArgument);
         return true;

      default:
         pr("dataType 0x%x ignored\n",avail->dataType);
         break;
   }
   return false;
}

bool validateFWMagic() 
{
#if 0
   flashRead(0x8b, (void *)(blockbuffer + 1024), 256);
   eepromRead(eeSize - OTA_UPDATE_SIZE, blockbuffer, 256);
   if(xMemEqual((const void *__xdata)(blockbuffer + 1024), (const void *__xdata)(blockbuffer + 0x8b), 8)) {
#ifdef DEBUGOTA
      pr("OTA: magic number matches! good fw\n");
#endif
      return true;
   } else {
#ifdef DEBUGOTA
      pr("OTA: this probably isn't a (recent) firmware file\n");
#endif
      return false;
   }
#else
   return false;
#endif
}

void initializeProto() 
{
   getNumSlots();
   curHighSlotId = getHighSlotId();
}

void InitBcastFrame()
{
   xMemCopyShort(gBcastFrame.src,(void *) mSelfMac,sizeof(gBcastFrame.src));
   gBcastFrame.fcs.frameType = 1;
   gBcastFrame.fcs.ackReqd = 1;
   gBcastFrame.fcs.destAddrType = 2;
   gBcastFrame.fcs.srcAddrType = 3;
   gBcastFrame.dstPan = PROTO_PAN_ID_SUBGHZ;
   gBcastFrame.dstAddr = 0xFFFF;
   gBcastFrame.srcPan = PROTO_PAN_ID_SUBGHZ;
}

void UpdateBcastFrame()
{
   #define TX_FRAME_PTR ((struct MacFrameBcast __xdata *)(outBuffer + 1))
   xMemCopyShort(TX_FRAME_PTR,(void *)&gBcastFrame,sizeof(struct MacFrameBcast));
   TX_FRAME_PTR->seq = seq++;
}
