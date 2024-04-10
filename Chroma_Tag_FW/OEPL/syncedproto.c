#define __packed


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
#include "powermgt.h"
#include "printf.h"
#include "../oepl-definitions.h"
#include "../oepl-proto.h"
#include "syncedproto.h"
#include "radio.h"
#include "screen.h"
#include "settings.h"
#include "sleep.h"
#include "timer.h"
#include "userinterface.h"
#include "wdt.h"
#include "adc.h"
#include "logging.h"

// Kludge to save a few bytes of xcata and code
#define eih ((struct EepromImageHeader *__xdata) blockbuffer)

uint8_t __xdata blockbuffer[BLOCK_XFER_BUFFER_SIZE];
static struct blockRequest __xdata curBlock;  // used by the block-requester, contains the next request that we'll send
static uint8_t __xdata curDispDataVer[8];
static struct AvailDataInfo __xdata xferDataInfo;  // holds the AvailDataInfo during the transfer
static bool __xdata gNewBlock;         // if we should ask the AP to get this block from the host or not
#define BLOCK_TRANSFER_ATTEMPTS 5

static uint8_t xferImgSlot = 0xFF;          // holds current transfer slot in progress
uint8_t __xdata curImgSlot = 0xFF;          // currently shown image
static uint32_t __xdata curHighSlotId;  // current highest ID, will be incremented before getting written to a new slot
static uint8_t __xdata nextImgSlot;     // next slot in sequence for writing

#define OTA_UPDATE_SIZE 0x8000   // 32k

// stuff we need to keep track of related to the network/AP
uint8_t __xdata APmac[8];
uint16_t __xdata APsrcPan;
uint8_t __xdata mSelfMac[8];
static uint8_t __xdata seq;
uint8_t __xdata currentChannel;
__bit gOTA;

// buffer we use to prepare/read packets
uint8_t __xdata inBuffer[128];
static uint8_t __xdata outBuffer[128];

// determines if the tagAssociated loop in main.c performs a rapid next checkin
bool __xdata fastNextCheckin;
uint8_t __xdata gSubBlockID;

struct MacFrameBcast __xdata gBcastFrame;

// other stuff we shouldn't have to put here...
static uint32_t __xdata markerValid = EEPROM_IMG_VALID;

extern void executeCommand(uint8_t cmd);  // this is defined in main.c

// tools

static bool pktIsUnicast(void);
static bool pktIsBcast(void);
void DumpCurBlock(void);

uint8_t __xdata getPacketType() 
{
   PROTO_LOG("pkt Type");
   if(pktIsBcast()) {
      PROTO_LOG(" 0x%x\n",
                ((uint8_t *)inBuffer)[sizeof(struct MacFrameBcast)]);
      return ((uint8_t *)inBuffer)[sizeof(struct MacFrameBcast)];
   }
   if(pktIsUnicast()) {
   // normal frame
      PROTO_LOG(" 0x%x\n",
                ((uint8_t *)inBuffer)[sizeof(struct MacFrameNormal)]);
      return ((uint8_t *)inBuffer)[sizeof(struct MacFrameNormal)];
   }
   PROTO_LOG(" unknown\n");
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

void DumpHex(const uint8_t *__xdata Data, const uint16_t __xdata Len)
{
   uint16_t i;
   for(i = 0; i < Len; i++) {
      if(i != 0 && (i & 0xf) == 0) {
         LOG("\n");
      }
      LOG("%02x ",Data[i]);
   }
   LOG("\n");
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

uint8_t detectAP(const uint8_t channel) __reentrant 
{
   static uint32_t __xdata t;

   radioRxEnable(false);
   radioSetChannel(channel);
   radioRxFlush();
   radioRxEnable(true);
   LOGA("Trying ch %d",channel);
   for(uint8_t c = 1; c <= MAXIMUM_PING_ATTEMPTS; c++) {
      outBuffer[0] = sizeof(struct MacFrameBcast) + 1 + 2;
      UpdateBcastFrame();
      outBuffer[sizeof(struct MacFrameBcast) + 1] = PKT_PING;
      radioTx(outBuffer);
      t = timerGet() + (TIMER_TICKS_PER_MS * PING_REPLY_WINDOW);
      while(timerGet() < t) {
         if(radioRx() > 1
            && inBuffer[sizeof(struct MacFrameNormal) + 1] == channel 
            && getPacketType() == PKT_PONG
            && pktIsUnicast())
         {
            #define f ((struct MacFrameNormal *)inBuffer)
            xMemCopyShort(APmac,f->src,8);
            APsrcPan = f->pan;
            #undef f
            LOGA(" AP found\n");
            return c;
         }
      }
      LOGA(".");
   }
   LOGA("\n");
   return 0;
}

// outBuffer: 
// len   contents
// 1     <tx data len> 
// 17    <MacFrameBcast>
// 1     PKT_AVAIL_DATA_REQ
// 21    <AvailDataReq>
// 1     <crc> (actually a 1 byte checksum of <AvailDataReq>)
// Total tx len data = 42 (including frame len)
#define availreq ((struct AvailDataReq * __xdata)&outBuffer[2 + sizeof(struct MacFrameBcast)])
static void sendAvailDataReq() 
{
   outBuffer[0] = sizeof(struct MacFrameBcast) + sizeof(struct AvailDataReq) + 2 + 2;
   UpdateBcastFrame();

   outBuffer[sizeof(struct MacFrameBcast) + 1] = PKT_AVAIL_DATA_REQ;
// TODO: send some (more) meaningful data
   availreq->hwType = HW_TYPE;
   availreq->wakeupReason = wakeUpReason;
   availreq->lastPacketRSSI = mLastRSSI;
   availreq->lastPacketLQI = mLastLqi;
   ADCRead(ADC_CHAN_TEMP);
   ADCScaleTemperature();
   availreq->temperature = gTemperature;
   availreq->batteryMv = gBattV;
   availreq->capabilities = 0;
   availreq->tagSoftwareVersion = fwVersion;
   availreq->currentChannel = currentChannel;
   availreq->customMode = tagSettings.customMode;
   addCRC(availreq, sizeof(struct AvailDataReq));
   LogSummary();
   radioTx(outBuffer);
}
#undef AvailDataReq

struct AvailDataInfo *__xdata getAvailDataInfo() 
{
   radioRxEnable(true);
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
   uint32_t __xdata t;
   PROTO_LOG("Get Short AvlData\n");
   radioRxEnable(true);

   for(uint8_t c = 0; c < DATA_REQ_MAX_ATTEMPTS; c++) {
      outBuffer[0] = sizeof(struct MacFrameBcast) + 1 + 2;
      outBuffer[sizeof(struct MacFrameBcast) + 1] = PKT_AVAIL_DATA_SHORTREQ;
      UpdateBcastFrame();
      radioTx(outBuffer);
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

static void processBlockPart(const struct blockPart *bp) 
{
   uint16_t __xdata start;
   uint16_t __xdata size = BLOCK_PART_DATA_SIZE;

   start = (bp->blockPart - gSubBlockID) * BLOCK_PART_DATA_SIZE;

// validate if it's okay to copy data
   if(bp->blockId != curBlock.blockId) {
      BLOCK_LOG("not expected blockId %d != %d\n",bp->blockId,curBlock.blockId);
      return;
   }

   if(bp->blockPart > BLOCK_MAX_PARTS) {
      BLOCK_LOG("bad blockPart\n");
      return;
   }

   if(bp->blockPart < gSubBlockID) {
      BLOCK_LOG("blockPart < gSubBlockID\n");
      return;
   }

   if(start + size > sizeof(blockbuffer)) {
      BLOCK_LOG("bad start 0x%x gSubBlockID %d size %u\n",
                start,gSubBlockID,size);
      return;
   }

#if 0
   if((start + size) > sizeof(blockbuffer)) {
   // ??? why wrap, does this happen ???
      BLOCK_LOG("size %d -> ",size);
      size = sizeof(blockbuffer) - start;
      BLOCK_LOG("%d\n",size);
   }
#endif
   BLOCK_LOG("Got blockId %d part %d\n",bp->blockId,bp->blockPart);

   if(checkCRC(bp,sizeof(struct blockPart) + BLOCK_PART_DATA_SIZE)) {
   //  copy block data to buffer
      xMemCopy((void *)(blockbuffer + start), (const void *)bp->data, size);
   // we don't need this block anymore, set bit to 0 so we don't request it again
      curBlock.requestedParts[bp->blockPart / 8] &= ~(1 << (bp->blockPart % 8));
#ifndef DEBUGBLOCKS
   LOGA(".");
#endif
      return;
   }
   LOGE("checkCRC failed\n");
   return;
}

static void blockRxLoop(const uint32_t timeout) 
{
   uint32_t __xdata t;
   uint8_t i;

   radioRxEnable(true);
   t = timerGet() + (TIMER_TICKS_PER_MS * (timeout + 20));
   while(timerGet() < t) {
      if(radioRx() > 1) {
         if(getPacketType() == PKT_BLOCK_PART) {
         // sizeof(struct MacFrameNormal)  = 17
         // packet type 1
         // BLOCK_PART_DATA_SIZE = 99
            #define BP (struct blockPart *)(inBuffer + sizeof(struct MacFrameNormal) + 1)
            processBlockPart(BP);
            #undef bp
            for(i = 0; i < BLOCK_REQ_PARTS_BYTES; i++) {
               if(curBlock.requestedParts[i] != 0) {
                  break;
               }
            }
            if(i == BLOCK_REQ_PARTS_BYTES) {
            // got all of the parts we were expecting, bail
#ifndef DEBUGBLOCKS
               LOG("\n");
#endif
               break;
            }
         }
      }
   }
   radioRxEnable(false);
   radioRxFlush();
}

static struct blockRequestAck *__xdata continueToRX() 
{
   #define ack ((struct blockRequestAck *)(inBuffer + sizeof(struct MacFrameNormal) + 1))
   ack->pleaseWaitMs = 0;
   return ack;
   #undef ack
}

#define f ((struct MacFrameNormal * __xdata)(outBuffer + 1))
//   struct MacFrameNormal *__xdata f = (struct MacFrameNormal *)(outBuffer + 1);
static void sendBlockRequest() 
{
   xMemSet(outBuffer,0,sizeof(struct MacFrameNormal) + sizeof(struct blockRequest) + 2 + 2);
//   struct MacFrameNormal *__xdata f = (struct MacFrameNormal *)(outBuffer + 1);
   struct blockRequest *__xdata blockreq = (struct blockRequest *)(outBuffer + 2 + sizeof(struct MacFrameNormal));
   outBuffer[0] = sizeof(struct MacFrameNormal) + sizeof(struct blockRequest) + 2 + 2;
   if(gNewBlock) {
      outBuffer[sizeof(struct MacFrameNormal) + 1] = PKT_BLOCK_REQUEST;
   }
   else {
      outBuffer[sizeof(struct MacFrameNormal) + 1] = PKT_BLOCK_PARTIAL_REQUEST;
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
#undef f

static struct blockRequestAck *__xdata performBlockRequest() __reentrant 
{
   static uint32_t __xdata t;
   radioRxEnable(true);
   radioRxFlush();
   for(uint8_t c = 0; c < 30; c++) {
      sendBlockRequest();
//      t = timerGet() + (TIMER_TICKS_PER_MS * (7UL + c / 10));
      t = timerGet() + (TIMER_TICKS_PER_SECOND * 30UL);
      BLOCK_LOG("wait until %lu\n",t);
      do {
         if(radioRx() > 1) {
            switch(getPacketType()) {
               case PKT_BLOCK_REQUEST_ACK:
                  BLOCK_LOG("blk req ack\n");
                  if(checkCRC((inBuffer + sizeof(struct MacFrameNormal) + 1), sizeof(struct blockRequestAck)))
                     return(struct blockRequestAck *)(inBuffer + sizeof(struct MacFrameNormal) + 1);
                  break;

               case PKT_BLOCK_PART:
               // block already started while we were waiting for a get block reply
               // pr("!");
               // processBlockPart((struct blockPart *)(inBuffer + sizeof(struct MacFrameNormal) + 1));
                  BLOCK_LOG("blk part\n");
                  return continueToRX();

               case PKT_CANCEL_XFER:
                  BLOCK_LOG("cancel xferpart\n");
                  return NULL;

               default:
                  PROTO_LOG("ignored pkt type %02X\n", getPacketType());
                  return NULL;
                  break;
            }
         }
      } while(timerGet() < t);
      BLOCK_LOG("%lu done waiting\n",t);
   }
   return continueToRX();
}

#define f ((struct MacFrameNormal * __xdata)(outBuffer + 1))
static void sendXferCompletePacket() 
{
   xMemSet(outBuffer,0,sizeof(struct MacFrameNormal) + 2 + 4);
   outBuffer[0] = sizeof(struct MacFrameNormal) + 2 + 2;
   outBuffer[sizeof(struct MacFrameNormal) + 1] = PKT_XFER_COMPLETE;
   xMemCopyShort((void *)(f->src), (void *) mSelfMac,8);
   xMemCopyShort((void *)(f->dst), (void *) APmac,8);
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
#undef f

static void sendXferComplete() 
{
   radioRxEnable(true);

   PROTO_LOG("XFC ");
   for(uint8_t c = 0; c < 16; c++) {
      sendXferCompletePacket();
      uint32_t __xdata start = timerGet();
      while((timerGet() - start) < (TIMER_TICKS_PER_MS * XFER_COMPLETE_REPLY_WINDOW)) {
         if(radioRx() > 1) {
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

// EEprom related stuff
static uint32_t getAddressForSlot(const uint8_t s) 
{
   return EEPROM_IMG_START + (EEPROM_IMG_EACH * s);
}

static uint8_t findSlotVer(const uint8_t *ver) 
{
#ifdef FORCE_IMG_DL
// Force re-download each and every upload without checking if it's 
// already in the eeprom somewhere
   return 0xFF;
#else
   for(uint8_t c = 0; c < IMAGE_SLOTS; c++) {
      eepromRead(getAddressForSlot(c), eih, sizeof(struct EepromImageHeader));
      if(xMemEqual4(&eih->validMarker, &markerValid)) {
         if(xMemEqual(&eih->version, (void *)ver, 8)) {
            return c;
         }
      }
   }
   return 0xFF;
#endif
}

// making this reentrant saves one byte of idata...
uint8_t __xdata findSlotDataTypeArg(uint8_t arg) __reentrant 
{
   arg &= (0xF8);  // unmatch with the 'preload' bit and LUT bits
   for(uint8_t c = 0; c < IMAGE_SLOTS; c++) {
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
   eepromRead(getAddressForSlot(slot), eih, sizeof(struct EepromImageHeader));
   return eih->argument;
}

uint8_t __xdata findNextSlideshowImage(uint8_t start) __reentrant 
{
   uint8_t c = start;
   while(1) {
      c++;
      if(c > IMAGE_SLOTS) {
         c = 0;
      }
      if(c == start) return c;
      eepromRead(getAddressForSlot(c), eih, sizeof(struct EepromImageHeader));
      if(xMemEqual4(&eih->validMarker, &markerValid)) {
         if((eih->argument & 0xF8) == (CUSTOM_IMAGE_SLIDESHOW << 3)) {
            return c;
         }
      }
   }
}

static void eraseImageBlock(const uint8_t c) 
{
   eepromErase(getAddressForSlot(c),EEPROM_IMG_SECTORS);
}

void eraseImageBlocks() 
{
   for(uint8_t c = 0; c < IMAGE_SLOTS; c++) {
      eraseImageBlock(c);
   }
}

void drawImageFromEeprom(const uint8_t imgSlot, uint8_t lut) 
{
   lut;

   LOGA("Drawing image in slot %d\n",imgSlot);
   drawImageAtAddress(getAddressForSlot(imgSlot));
}

static uint32_t getHighSlotId() 
{
   uint32_t temp = 0;
   for(uint8_t __xdata c = 0; c < IMAGE_SLOTS; c++) {
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

// Total number of active blocks in this block (NOT subblock),  this will 
// alway be BLOCK_MAX_PARTS except for the last block of a transfer
static uint8_t __xdata gPartsThisBlock;
// offset into into the standard block buffer for the current subblock
// We do not have room from the standard block so we are emulating one
// to keep from needing to modify AP code.
// 0->BLOCK_MAX_PARTS in BLOCK_MAX_PARTS_SUBGIG increment
static uint16_t __xdata gBlockChecksum; 

struct blockData gBlockData;

// these COULD be local to the function, but for some reason, they won't survive sleep?
// they get overwritten with  7F 32 44 20 00 00 00 00 11, I don't know why.
static uint8_t __xdata blockAttempts;

// blockSize: number of data bytes wanted, excluding overhead
// Will alway be BLOCK_DATA_SIZE except for the last block for the transfer
// which may be < BLOCK_DATA_SIZE
static bool getDataBlock(const uint16_t blockSize) 
{
   uint8_t SubBlock;
   uint16_t Len;
   uint32_t Adr;
   uint8_t *__xdata pData;
   __bit NewSubBlock = true;

   Adr = curBlock.blockId * BLOCK_DATA_SIZE;
   if(gOTA) {
      BLOCK_LOG("ota ");
      Adr += EEPROM_UPDATA_AREA_START;
   }
   else {
      Adr += getAddressForSlot(xferImgSlot) + sizeof(struct EepromImageHeader); 
   }
   BLOCK_LOG("blk adr 0x%lx\n",Adr);

   blockAttempts = BLOCK_TRANSFER_ATTEMPTS;
   gSubBlockID = 0;  // new block starting
   gNewBlock = true;
   gBlockData.size = blockSize;  // should match the header once it's received

   xMemSet(curBlock.requestedParts,0x00,BLOCK_REQ_PARTS_BYTES);

   while(blockAttempts--) {
      if(NewSubBlock) {
#ifndef DEBUGBLOCKS
         LOGA("REQ %d part %d",curBlock.blockId,gSubBlockID);
#endif
         NewSubBlock = false;
         gPartsThisBlock = gBlockData.size / BLOCK_PART_DATA_SIZE;
         if((gBlockData.size % BLOCK_PART_DATA_SIZE) != 0) {
            gPartsThisBlock++;
         }
         if(gPartsThisBlock > BLOCK_MAX_PARTS_SUBGIG) {
            gPartsThisBlock = BLOCK_MAX_PARTS_SUBGIG;
         }
         SubBlock = gSubBlockID;
         for(uint8_t c = 0; c < gPartsThisBlock; c++) {
            curBlock.requestedParts[SubBlock / 8] |= (1 << (SubBlock % 8));
            SubBlock++;
         }
      }
      BLOCK_LOG("Tx ");
      DumpCurBlock();
      struct blockRequestAck *__xdata ack = performBlockRequest();

      if(ack == NULL) {
         PROTO_LOG("Cancelled req\n");
         return false;
      }
      if(ack->pleaseWaitMs) {  
      // SLEEP - until the AP is ready with the data
         BLOCK_LOG("pleaseWaitMs %u\n",ack->pleaseWaitMs);
         if(ack->pleaseWaitMs < 35) {
            timerDelay(ack->pleaseWaitMs * TIMER_TICKS_PER_MS);
         }
         else {
            doSleep(ack->pleaseWaitMs - 10);
            radioRxEnable(true);
         }
      }
   // BLOCK RX LOOP - receive a block, until the timeout has passed
      blockRxLoop(290);  
      BLOCK_LOG("Rx ");
      DumpCurBlock();

   // check if we got all the parts we needed, e.g: has the block been completed?
      bool blockComplete = true;
      SubBlock = gSubBlockID;
      for(uint8_t i = 0; i < gPartsThisBlock; i++) {
         if(curBlock.requestedParts[SubBlock / 8] & (1 << (SubBlock % 8))) {
            blockComplete = false;
            break;
         }
         SubBlock++;
      }

      if(blockComplete) {
      // This subblock this complete
         Len = gPartsThisBlock * BLOCK_PART_DATA_SIZE;
         pData = blockbuffer;

         if(gSubBlockID == 0) {
         // First subblock, save the block size and checksum for later
            #define bd ((struct blockData *)blockbuffer)
            gBlockData.size = bd->size;
            gBlockData.checksum = bd->checksum;
            #undef bd
            BLOCK_LOG("blk size %d chksum 0x%x\n",
                      gBlockData.size,gBlockData.checksum);
            gBlockChecksum = 0;
         // Adjust data pointer and Len
            Len -= sizeof(gBlockData);
            pData += sizeof(gBlockData);
         }

         if(Len > gBlockData.size) {
            Len = gBlockData.size;
         }
         gBlockData.size -= Len;

      // Write this subblock to EEPROM
         BLOCK_LOG("write %d @ 0x%lx\n",Len,Adr);
         powerUp(INIT_EEPROM);
         eepromWrite(Adr,(const void __xdata *) pData,Len);
         powerDown(INIT_EEPROM);
         Adr += Len;

      // Update the checksum for this subblock
         while(Len-- > 0) {
            gBlockChecksum += *pData++;
         }
         gSubBlockID += gPartsThisBlock;
         BLOCK_LOG("SubBlockID %d left %d chksum 0x%x\n",
                   gSubBlockID,gBlockData.size,gBlockChecksum);

      // Is the entire block complete?
         if(gBlockData.size != 0) {
         // More Subblocks to come
            blockComplete = false;
            NewSubBlock = true;
         }
      }
      gNewBlock = false;
      if(blockComplete) {
         BLOCK_LOG("blk complete\n");
         if(gBlockData.checksum != gBlockChecksum) {
            LOGE("blk failed validation!\n");
            break;
         }
         else {
         // block download complete, validated
            return true;
         }
      }
      else {
      // block incomplete, re-request a partial block
         BLOCK_LOG("blk incomplete\n");
      }
   }
   return false;
}

static uint16_t __xdata dataRequestSize;
static uint16_t __xdata otaSize;

static bool downloadFWUpdate(const struct AvailDataInfo *__xdata avail) 
{
// check if we already started the transfer of this information & haven't completed it
   if(xMemEqual((const void *__xdata) &avail->dataVer,(const void *__xdata) &xferDataInfo.dataVer,8)
      && xferDataInfo.dataSize) 
   {
      // looks like we did. We'll carry on where we left off.
   }
   else {
      OTA_LOG("Start OTA\n");
   // start, or restart the transfer from 0. Copy data from the 
   // AvailDataInfo struct, and the struct intself. 
   // This forces a new transfer
      curBlock.blockId = 0;
      xMemCopy8(&curBlock.ver,&avail->dataVer);
      curBlock.type = avail->dataType;
      xMemCopyShort(&xferDataInfo,(void *)avail,sizeof(struct AvailDataInfo));
      eepromErase(EEPROM_UPDATA_AREA_START,OTA_UPDATE_SIZE / EEPROM_ERZ_SECTOR_SZ);
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

      gOTA = true;
      if(getDataBlock(dataRequestSize)) {
      // succesfully downloaded datablock, save to eeprom
         curBlock.blockId++;
         xferDataInfo.dataSize -= dataRequestSize;
      }
      else {
         // failed to get the block we wanted, we'll stop for now, maybe resume later
         return false;
      }
   }
   wdt60s();
   powerUp(INIT_EEPROM);
#if 0
   if(!validateMD5(EEPROM_SIZE - OTA_UPDATE_SIZE, otaSize)) {
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
#endif

   // no more data, download complete
   return true;
}

uint16_t __xdata imageSize;
static bool downloadImageDataToEEPROM(const struct AvailDataInfo *__xdata avail) 
{
// check if we already started the transfer of this information & haven't completed it
   if(xMemEqual((const void *__xdata) &avail->dataVer,(const void *__xdata) & xferDataInfo.dataVer, 8) 
      && (xferDataInfo.dataTypeArgument == avail->dataTypeArgument) 
      && xferDataInfo.dataSize) 
   {  // looks like we did. We'll carry on where we left off.
      LOGA("restarting img dl\n");
   }
   else {
   // new transfer
      powerUp(INIT_EEPROM);

   // go to the next image slot
      uint8_t startingSlot = nextImgSlot;

   // if we encounter a special image type (preloadImage or specialType is 
   // non zero) then start looking from slot 0, to prevent the image being 
   // overwritten when we do an OTA update.
   // sh: I think there's a bug here which only occurs when the next if
   // is trun and nextImgSlot is the last slot
      if((avail->dataTypeArgument & 0xFC) != 0x00) {
         LOG("Changing startingSlot from %d to 0\n",startingSlot);
         DumpHex((const uint8_t *__xdata)avail,sizeof(struct AvailDataInfo));
         startingSlot = 0;
      }

      while(1) {
         nextImgSlot++;
         if(nextImgSlot >= IMAGE_SLOTS) {
            nextImgSlot = 0;
         }
         if(nextImgSlot == startingSlot) {
            powerDown(INIT_EEPROM);
            LOGE("No slots\n");
            return true;
         }
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

      eepromErase(getAddressForSlot(xferImgSlot),EEPROM_IMG_SECTORS);
      powerDown(INIT_EEPROM);
      LOGA("new dl to %d\n", xferImgSlot);
   // start, or restart the transfer. Copy data from the AvailDataInfo struct, 
   // theand the struct intself. This forces a new transfer
      curBlock.blockId = 0;
      xMemCopy8(&curBlock.ver,&avail->dataVer);
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
         curBlock.blockId++;
         xferDataInfo.dataSize -= dataRequestSize;
      }
      else {
      // failed to get the block we wanted, we'll stop for now, probably resume later
         return false;
      }
   }
// no more data, download complete

   powerUp(INIT_EEPROM);
   xMemCopy8(&eih->version, &xferDataInfo.dataVer);
   eih->validMarker = EEPROM_IMG_VALID;
   eih->id = ++curHighSlotId;
   eih->size = imageSize;
   eih->dataType = xferDataInfo.dataType;
   eih->argument = xferDataInfo.dataTypeArgument;

   BLOCK_LOG("BLOCKS: Now writing datatype 0x%02X to slot %d\n",
             xferDataInfo.dataType,xferImgSlot);
   eepromWrite(getAddressForSlot(xferImgSlot),eih,sizeof(struct EepromImageHeader));
   powerDown(INIT_EEPROM);

   return true;
}

// this is related to the function below, but if declared -inside- the function, it gets cleared during sleep...
struct imageDataTypeArgStruct __xdata arg;  
inline bool processImageDataAvail(struct AvailDataInfo *__xdata avail) 
{
   *((uint8_t *)arg) = avail->dataTypeArgument;
   if(arg.preloadImage) {
      PROTO_LOG("Preloading img type 0x%02X from 0x%02X\n",
                arg.specialType,avail->dataTypeArgument);
      powerUp(INIT_EEPROM);
      switch(arg.specialType) {
      // check if a slot with this argument is already set; if so, erase. 
      // Only one of each arg type should exist
         default: {
               uint8_t slot = findSlotDataTypeArg(avail->dataTypeArgument);
               if(slot != 0xFF) {
                  eraseImageBlock(slot);
               }
            } break;
            // regular image preload, there can be multiple of this type in the EEPROM
         case CUSTOM_IMAGE_NOCUSTOM: {
               // check if a version of this already exists
               uint8_t slot = findSlotVer(&(avail->dataVer));
               if(slot != 0xFF) {
                  sendXferComplete();
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
         sendXferComplete();
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
         sendXferComplete();
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
            sendXferComplete();

          // mark as completed and draw from EEPROM
            xMemCopyShort(&xferDataInfo, (void *)avail, sizeof(struct AvailDataInfo));
            xferDataInfo.dataSize = 0;  // mark as transfer not pending

            wdt60s();
            curImgSlot = findImgSlot;
            drawImageFromEeprom(findImgSlot, arg.lut);
         }
         else {
         // not found in cache, prepare to download
            LOGA("dl img\n");
            if(downloadImageDataToEEPROM(avail)) {
            // sets xferImgSlot to the right slot
               PROTO_LOG("dl done\n");
               sendXferComplete();

            // not preload, draw now
               wdt60s();
               curImgSlot = xferImgSlot;
               drawImageFromEeprom(xferImgSlot, arg.lut);
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
   gOTA = false;

   switch(avail->dataType) {
      case DATATYPE_IMG_BMP:
      case DATATYPE_IMG_DIFF:
      case DATATYPE_IMG_RAW_1BPP:
      case DATATYPE_IMG_RAW_2BPP:
         return processImageDataAvail(avail);

      case DATATYPE_TAG_CONFIG_DATA:
         if(xferDataInfo.dataSize == 0 && xMemEqual((const void *__xdata) & avail->dataVer, (const void *__xdata) & xferDataInfo.dataVer, 8)) {
            PROTO_LOG("same as last ignored\n");
            sendXferComplete();
            return true;
         }
         curBlock.blockId = 0;
         xMemCopy8(&curBlock.ver,&avail->dataVer);
         curBlock.type = avail->dataType;
         xMemCopyShort(&xferDataInfo, (void *)avail, sizeof(struct AvailDataInfo));
         wdt10s();
         if(getDataBlock(avail->dataSize)) {
            xferDataInfo.dataSize = 0;  // mark as transfer not pending
            powerUp(INIT_EEPROM);
            loadSettingsFromBuffer(sizeof(struct blockData) + blockbuffer);
            powerDown(INIT_EEPROM);
            sendXferComplete();
            return true;
         }
         return false;

      case DATATYPE_COMMAND_DATA:
         PROTO_LOG("CMD rx\n");
         sendXferComplete();
         executeCommand(avail->dataTypeArgument);
         return true;

      case DATATYPE_FW_UPDATE:
         powerUp(INIT_EEPROM);
         if(downloadFWUpdate(avail)) {
            OTA_LOG("Download complete\n");
            sendXferComplete();

            powerUp(INIT_EEPROM);
            if(validateFWMagic()) {
               OTA_LOG("Valid magic number\n");
               showApplyUpdate();
               wdt60s();
               powerUp(INIT_EEPROM);
               eepromRead(EEPROM_UPDATA_AREA_START,mScreenRow,256);
               DumpHex(mScreenRow,256);
               eepromReadStart(EEPROM_UPDATA_AREA_START);
               selfUpdate();
            // Never returns, ends in WDT reset
            }
            else {
               OTA_LOG("OTA: Invalid magic number!\n");
               fastNextCheckin = true;
               powerDown(INIT_EEPROM);
               wakeUpReason = WAKEUP_REASON_FAILED_OTA_FW;
               showFailedUpdate();
               xMemSet(curDispDataVer,0x00,8);
            }
         }
         else {
            return false;
         }
         break;

      default:
         pr("dataType 0x%x ignored\n",avail->dataType);
         break;
   }
   return false;
}

bool validateFWMagic() 
{
#if 1
   return true;
#else
   flashRead(0x8b, (void *)(blockbuffer + 1024), 256);
   eepromRead(EEPROM_SIZE - OTA_UPDATE_SIZE,blockbuffer,256);
   if(xMemEqual((const void *__xdata)(blockbuffer + 1024), (const void *__xdata)(blockbuffer + 0x8b), 8)) {
      OTA_LOG("OTA: magic number matches! good fw\n");
      return true;
   }
   OTA_LOG("OTA: this probably isn't a (recent) firmware file\n");
   return false;
#endif
}

void initializeProto() 
{
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

void DumpCurBlock()
{
#ifdef DEBUGBLOCKS
      LOG("REQ %d [",curBlock.blockId);
      for(uint8_t c = 0; c < BLOCK_MAX_PARTS; c++) {
         if((c != 0) && (c % 8 == 0)) {
            pr("][");
         }
         if(curBlock.requestedParts[c / 8] & (1 << (c % 8))) {
            pr("R");
         }
         else {
            pr("_");
         }
      }
      pr("]\n");
#endif
}

