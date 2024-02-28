#define __packed

#include <string.h>
#include "asmUtil.h"
#include "printf.h"
#include "radio.h"
#include "comms.h"
#include "settings.h"
#include "logging.h"

static uint8_t __xdata mCommsBuf[COMMS_MAX_PACKET_SZ];
uint8_t __xdata mLastLqi;
int8_t __xdata mLastRSSI;

