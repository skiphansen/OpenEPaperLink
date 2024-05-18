#ifndef _SCREEN_H_
#define _SCREEN_H_

#include <stdbool.h>
#include <stdint.h>

extern uint8_t __xdata mScreenVcom;
extern int8_t __xdata mCurTemperature;
extern __bit gRedPass;
extern __bit gDrawFromFlash;
#define SCREEN_EXPECTS_VCOM

// Physical
#define SCREEN_WIDTH             128L
#define SCREEN_HEIGHT            296L

// Logical
#define DISPLAY_WIDTH             SCREEN_HEIGHT
#define DISPLAY_HEIGHT            SCREEN_WIDTH

// or 26 x 8 characters with a 10 x 16 font


/*
       Logical->rotated right 90 deg and mirrored -> Physical
          Physical                          Logical
    0,0 +-----------+ 127,0      0, 0 +----------------------+ 295,0  
        |  part 0   |            -----|                      |        
        |    -      |            cable|                      |        
   0,73 |  part 1   | 127,73     -----|                      |        
        |    -      |           0,127 +----------------------+ 295,127
  0.147 |  part 2   | 127,147                                         
        |    -      |                                                 
  0,221 |  part 3   | 127,221                                         
  0,295 +-----------+ 127,295                                         
          | cable |                                                   
                                 
16 bytes per physical line  
So physical X = 127 - logical y
physical Y = 295 - logical x
 
+ (logical y
*/
#if 0
#if SCREEN_HEIGHT > SCREEN_WIDTH
#define SCREEN_ROTATED           1
#else
#define SCREEN_ROTATED           0
#endif
#endif

#define SCREEN_ROTATED           0

// UI screen layout constants for 2.9"

// Location of OELP logo (CloudTop)
#define LOGO_ON_RIGHT            1
#define SPLASH_LOGO_X            DISPLAY_WIDTH
#define SPLASH_LOGO_Y            0
#define SPLASH_ADJ_START(BMP)    gBmpX -= BMP##[0]


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

void screenInitGPIO(bool bActivate);
void screenShutdown(void);
void screenTxStart();

#pragma callee_saves screenByteTx
void screenByteTx(uint8_t byte);
void drawWithSleep(void);

void P1INT_ISR(void) __interrupt (15);


// 350 bytes here which do not retain data in power modes PM2 or PM3,
// we use 320 of them for mScreenRow which doesn't need to be retained
__xdata __at (0xfda2) uint8_t mScreenRow[320];  

#endif
