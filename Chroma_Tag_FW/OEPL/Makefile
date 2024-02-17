BUILD		?= chroma74y

FW_VER := $(shell grep "\#define FW_VERSION" ../common/settings.h | grep -v "suffix" | awk '{print $$3}' | cut -c 3-)

#file containing main() must be first!
SOURCES	= main.c eeprom.c drawing.c comms.c settings.c chars.c cpu.c 
SOURCES += board.c powermgt.c syncedproto.c userinterface.c
include ../make/common.mk
SOURCES += $(SOC_DIR)/wdt.c $(SOC_DIR)/timer.c $(SOC_DIR)/sleep.c 
SOURCES += $(SOC_DIR)/adc.c $(SOC_DIR)/radio.c $(SOC_DIR)/aes.c 
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
# Generic EEPROM routines that use SFDP for configuration are GREAT
# **IF** you can spare the code space.  We can't !!!
# Disabling SFDP saves 1538 bytes.
FLAGS += -DSFDP_DISABLED

BUILD_DIR=$(BUILDS_DIR)/$(BUILD)
IMAGE_NAME?=$(BUILD)_${FW_VER}
include $(FIRMWARE_ROOT)/make/rules.mk


