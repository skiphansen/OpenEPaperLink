#ifndef _WDT_H_
#define _WDT_H_

#include <stdint.h>


#pragma callee_saves wdtOn
void wdtOn(void);

#pragma callee_saves wdtOff
void wdtOff(void);

#pragma callee_saves wdtPet
void wdtPet(void);

#pragma callee_saves wdtSetResetVal
void wdtSetResetVal(uint32_t val);		//speed is CPU-specific. On ZBS it is 62KHz or so

#pragma callee_saves wdtDeviceReset
void wdtDeviceReset(void);

// Dummy functions for now.  The WDT on the CC1110 supports a maximum
// timeout of 1 second.  Eventually an hardware/interrupt driven solution ?
#define wdtOn()   \
    do {          \
    } while (0)   

#define wdtOff()  \
    do {          \
    } while (0)   

#define wdtPet()  \
       do {       \
       } while (0)   

#define wdt10s()  \
    do {          \
    } while (0)   

#define wdt30s()  \
    do {          \
    } while (0)   

#define wdt60s()  \
    do {          \
    } while (0)   


#define wdt120s() \
    do {          \
    } while (0)   

#endif
