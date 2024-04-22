#ifndef SYNCED_H
#define SYNCED_H

#include <stdint.h>
#include "settings.h"

extern uint8_t __xdata gCurrentChannel;
extern uint8_t __xdata gSubGhzBand;
extern uint8_t __xdata APmac[];
extern uint8_t __xdata curImgSlot;
extern bool __xdata fastNextCheckin;

/* 
oepl-proto.h defines:
#define BLOCK_MAX_PARTS 42
#define BLOCK_DATA_SIZE 4096UL
#define BLOCK_XFER_BUFFER_SIZE BLOCK_DATA_SIZE + sizeof(struct blockData)
 
These values were picked for tags which have 8k of SRAM, but CC1110 based 
SubGhz tags only have 4K so this is not going to work for these tags.

For CC1110 tags the request is broken into 2 "sub-blocks" of 21 99 byte parts. 
*/ 
#define BLOCK_MAX_PARTS_SUBGIG    21UL
#define BLOCK_DATA_SIZE_SUBGIG    (BLOCK_MAX_PARTS_SUBGIG * BLOCK_PART_DATA_SIZE)

#undef BLOCK_XFER_BUFFER_SIZE
#define BLOCK_XFER_BUFFER_SIZE (sizeof(struct blockData) + BLOCK_DATA_SIZE_SUBGIG)

extern uint8_t __xdata blockbuffer[BLOCK_XFER_BUFFER_SIZE];
extern bool checkCRC(const void *p, const uint8_t len);
extern uint8_t __xdata findSlotDataTypeArg(uint8_t arg) __reentrant;
extern uint8_t __xdata findNextSlideshowImage(uint8_t start) __reentrant;
extern uint8_t getEepromImageDataArgument(const uint8_t slot);
extern struct AvailDataInfo *__xdata getAvailDataInfo(void);
extern struct AvailDataInfo *__xdata getShortAvailDataInfo(void);
extern void drawImageFromEeprom(const uint8_t imgSlot, uint8_t lut);
extern void eraseImageBlocks(void);
extern bool processAvailDataInfo(struct AvailDataInfo *__xdata avail);
extern void initializeProto(void);
extern uint8_t detectAP(const uint8_t channel);
#endif