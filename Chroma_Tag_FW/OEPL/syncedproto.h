#ifndef SYNCED_H
#define SYNCED_H

#include <stdint.h>
#include "settings.h"

extern uint8_t __xdata currentChannel;
extern uint8_t __xdata APmac[];

extern uint8_t __xdata curImgSlot;
extern bool __xdata fastNextCheckin;

/* 
oepl-proto.h defines:
#define BLOCK_DATA_SIZE 4096UL
#define BLOCK_XFER_BUFFER_SIZE BLOCK_DATA_SIZE + sizeof(struct blockData)
 
These values were picked for tags which have 8k of SRAM, but CC1110 based 
SubGhz tags only have 4K so this is not going to work for these tags.

The standard OEPL block protocol defines a data block as 4K bytes of
data which is written to flash when complete.  The data comes in as
41 "parts" of 99 bytes each and one part of 37 bytes.

For CC1110 tags the request is broken into smaller hunks of 4 "sub-blocks". 
The first 3 subblocks have 11 99 byte parts for 3267 bytes total. 
The last subblock has 8 99 byte parts and one 37 bytes part for a total of 
829 bytes. This adds up to the OEPl standard of 4096 bytes.
*/ 
#define BLOCK_MAX_PARTS_SUBGIG    11UL
#define BLOCK_DATA_SIZE_SUBGIG    (BLOCK_MAX_PARTS_SUBGIG * BLOCK_PART_DATA_SIZE)

#undef BLOCK_XFER_BUFFER_SIZE
#define BLOCK_XFER_BUFFER_SIZE (sizeof(struct blockData) + BLOCK_DATA_SIZE_SUBGIG)

extern uint8_t __xdata blockbuffer[BLOCK_XFER_BUFFER_SIZE];


//extern void setupRadio(void);
//extern void killRadio(void);


extern bool checkCRC(const void *p, const uint8_t len);

extern uint8_t __xdata findSlotDataTypeArg(uint8_t arg) __reentrant;
extern uint8_t __xdata findNextSlideshowImage(uint8_t start) __reentrant;
extern uint8_t getEepromImageDataArgument(const uint8_t slot);

extern struct AvailDataInfo *__xdata getAvailDataInfo();
extern struct AvailDataInfo *__xdata getShortAvailDataInfo();

extern void drawImageFromEeprom(const uint8_t imgSlot, uint8_t lut);
extern void eraseImageBlocks();
extern bool processAvailDataInfo(struct AvailDataInfo *__xdata avail);
extern void initializeProto();
extern uint8_t detectAP(const uint8_t channel);

extern bool validateFWMagic();

#endif