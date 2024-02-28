#include "timer.h"
#include "cpu.h"


// T1CC0 increments at at a 0.38743 Hz rate
// 26 mhz / 128 / 8 / 65536
// system clock divided by 128 tick speed (CLKCON register), 
// divided by 8 timer prescaler (T1CTL register), divided by 16 bits

void T1_ISR(void) __interrupt (9)
{
	T1CC0++;
}

// Return 24 bit timer which increments at a 33,854.16 Hz rate 
// (29.54 microseconds / count)
// The timer rolls over about every 11 minutes
uint32_t timerGet(void)
{
	union {
		struct {
			uint8_t tL;
			uint8_t tH;
			uint16_t hi;
		};
		uint32_t ret;
	} val;
	
	do {
		val.hi = T1CC0;
		val.tL = T1CNTL;	//read order is important due ot hardware buffering
		val.tH = T1CNTH;
	} while (val.hi != T1CC0);
	
	return val.ret;
}

uint8_t timerGetLowBits(void)
{
	return T1CNTL;
}

void timerInit(void)
{
	//we user T1CC0 as storage for high u16 of time
	T1CC0 = 0;
	
	T1CTL = 0;			//timer off
	T1CCTL0 = 0;		//all capture channels reset
	T1CCTL1 = 0;
	T1CCTL2 = 0;
	T1CNTL = 0;			//timer counter reset
	T1CTL = 0b00000101;	//on, prescaler is 1/128 (rate is 25390.625 Hz)
	TIMIF |= 1 << 6;	//irq on t1 overflow enabled
	IEN1 |= 1 << 1;		//timer 1 irq on
}

void timerDelay(uint32_t ticks)
{
	uint32_t start = timerGet();
	while (timerGet() - start <= ticks);
}