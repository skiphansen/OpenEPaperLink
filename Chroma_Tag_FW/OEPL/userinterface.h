#ifndef _UI_H_
#define _UI_H_

#include <stdint.h>
#ifndef DISABLE_UI

void addOverlay();
bool displayCustomImage(uint8_t imagetype);
void afterFlashScreenSaver();
void showSplashScreen();
void showApplyUpdate();
void showFailedUpdate();
void showAPFound();
void showNoAP();
void showLongTermSleep();
void showNoEEPROM();

#else

// Dummy functions to reduce flash size while debugging other things
#define afterFlashScreenSaver()
#define showLongTermSleep()
#define showNoEEPROM()
#define addOverlay()
#define showSplashScreen()
#define showAPFound()
#define showNoAP()
#define displayCustomImage(X) false
#define showFailedUpdate()
#define showApplyUpdate()
#endif

extern const uint16_t __code fwVersion;
extern __bit gLowBatteryShown;
extern __bit noAPShown;
extern uint16_t __xdata gUpdateFwVer;
extern uint8_t __xdata gUpdateErr;

#endif
