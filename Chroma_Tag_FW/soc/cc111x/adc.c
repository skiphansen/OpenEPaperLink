#include <stdbool.h>
#define __packed
#include "settings.h"
#include "asmUtil.h"
#include "printf.h"
#include "cpu.h"
#include "adc.h"
#include "powermgt.h"

// The ADC / temperature sensor calibration values are read from EEPROM
// during system initialization
uint16_t __xdata mAdcSlope;      //token 0x12
uint16_t __xdata mAdcIntercept;  //token 0x09

uint16_t __xdata gRawA2DValue;
int8_t __xdata gTemperature;

void ADCRead(uint8_t input)  
{
   ADCH = 0;
   ADCCON2 = 0x30 | input;
   ADCCON1 = 0x73;
   while (ADCCON1 & 0x40);
   while (!(ADCCON1 & 0x80));
   
   gRawA2DValue = ADCL;
   gRawA2DValue |= (((uint16_t)ADCH << 8));
   
   ADCCON2 = 0;
}


//in degrees C
   
// to get calibrated temp in units of 0.1 degreesC
// ((RawValue * 1250) - (mAdcIntercept * 0x7ff)) * 1000  / tokenGet(0x12) * 10 / 0x7ff
// Sets gTemperature global
void ADCScaleTemperature()
{
   __bit neg = false;
   uint32_t val;
   
   val = mathPrvMul16x16(gRawA2DValue >> 4,1250) - 
         mathPrvMul16x16(mAdcIntercept,0x7ff);
   //*= 1000
   val = mathPrvMul32x8(val,125);
   val <<= 3;
   
   if(val & 0x80000000) {
      neg = true;
      val = -val;
   }
   
   gTemperature = mathPrvDiv32x16(mathPrvDiv32x16(val,mAdcSlope) + 0x7ff / 2,0x7ff);
   
   if(neg) {
      gTemperature = -gTemperature;
   }
}
