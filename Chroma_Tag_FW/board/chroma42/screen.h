#ifndef _SCREEN_H_
#define _SCREEN_H_

#include <stdbool.h>
#include <stdint.h>

//i hate globals, but for 8051 this makes life a lot easier, sorry :(
extern uint8_t __xdata mScreenVcom;
extern int8_t __xdata mCurTemperature;
extern __bit gRedPass;
extern __bit gDrawFromFlash;
#define SCREEN_EXPECTS_VCOM

#define SCREEN_WIDTH             400L
#define SCREEN_HEIGHT            300L

#define SCREEN_BYTE_FILL         0x66  //white for normal mode

#define PIXEL_BLACK              3
#define PIXEL_WHITE              0
#define PIXEL_RED_YELLOW         2

#define EPD_COLOR_BLACK          0
#define EPD_COLOR_RED            1

#define EPD_DIRECTION_X          0
#define EPD_DIRECTION_Y          1

#define EPD_SIZE_SINGLE          0
#define EPD_SIZE_DOUBLE          1

void screenShutdown(void);
void screenTest(void);
void screenTxStart();

#pragma callee_saves screenByteTx
void screenByteTx(uint8_t byte);
void drawWithSleep(void);

void P1INT_ISR(void) __interrupt (15);


// 350 bytes here which do not retain data in power modes PM2 or PM3,
// we use 320 of them for mScreenRow which doesn't need to be retained
__xdata __at (0xfda2) uint8_t mScreenRow[320];  

#endif
