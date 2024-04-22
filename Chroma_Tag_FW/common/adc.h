#ifndef _ADC_H_
#define _ADC_H_

#include <stdint.h>

extern uint16_t __xdata mAdcSlope;
extern uint16_t __xdata mAdcIntercept;
extern uint16_t __xdata gRawA2DValue;

// virtual sources only. returns adcval << 4
// data returned in gRawA2DValue to save some code space passing
// values around.

// virtual sources only.
// sets gRawA2DValue set to adcval << 4
// To save some code space passing values around.

void ADCRead(uint8_t input);

// convert raw readding of ADC_CHAN_VDD_3 to millivolts accurate within 3%
#define ADCScaleVDD(x) mathPrvMul16x8(mathPrvDiv16x8(x,35),4)

// Converts gRawA2DValue to degrees C in gTemperature
void ADCScaleTemperature(void);

#endif
