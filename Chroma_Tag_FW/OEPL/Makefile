BUILD ?= chroma74y
LUT ?= atc1441
FLAGS += -DBUILD=$(BUILD)

ifeq ($(BUILD),chroma74y)
BOARD=chroma74
else ifeq ($(BUILD),chroma74r)
BOARD=chroma74
FLAGS += -DPROXY_BUILD
else
BOARD=$(BUILD)
endif

ifeq ($(BOARD),chroma74)
ifeq ($(LUT),atc1441)
FLAGS += -DATC1441_LUT
else ifeq ($(LUT),dmitry)
FLAGS += -DDMITRY_LUT
endif
endif

FW_VER := $(shell grep "\#define FW_VERSION" ../common/settings.h | grep -v "suffix" | awk '{print $$3}' | cut -c 3-)

#file containing main() must be first!
SOURCES	= main.c eeprom.c drawing.c comms.c settings.c chars.c cpu.c 
SOURCES += board.c powermgt.c syncedproto.c userinterface.c
include ../make/common.mk
SOURCES += $(SOC_DIR)/wdt.c $(SOC_DIR)/timer.c $(SOC_DIR)/sleep.c 
SOURCES += $(SOC_DIR)/adc.c $(SOC_DIR)/radio.c
SOURCES += $(SOC_DIR)/soc.c $(SOC_DIR)/printf.c
SOURCES += board.c

ifeq ($(OLD),y)
SOURCES += uc8154.c
else
SOURCES += screen.c
endif


EEPROMDRV ?= eeprom.c
SOURCES += $(EEPROMDRV)
FLAGS += -DPROXY_BUILD
FLAGS += -I. -MD
FLAGS += -DLEAN_VERSION

BUILD_DIR=$(BUILDS_DIR)/$(BUILD)
IMAGE_NAME?=$(BUILD)_${FW_VER}
include $(FIRMWARE_ROOT)/make/rules.mk


