BUILD		?= chroma74y

FW_VER=01

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

BUILD_DIR=$(BUILDS_DIR)/$(BUILD)
include $(FIRMWARE_ROOT)/make/rules.mk

IMAGE_NAME?=$(BUILD)_${FW_VER}

