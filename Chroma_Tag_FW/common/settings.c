#define __packed
#include "settings.h"

// #include <flash.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "asmUtil.h"
#include "powermgt.h"
#include "printf.h"
#include "syncedproto.h"
#include "eeprom.h"
#include "../../oepl-definitions.h"
#include "../../oepl-proto.h"
#include "logging.h"

#ifdef DEBUGSETTINGS
const char __code gSettingsPrefix[] = "SETTINGS: ";
#endif

#define SETTINGS_MAGIC 0xABBA5AA5

struct tagsettings __xdata tagSettings = {0};
extern uint8_t __xdata blockbuffer[];

void loadDefaultSettings() 
{
   tagSettings.settingsVer = SETTINGS_STRUCT_VERSION;
   tagSettings.enableFastBoot = DEFAULT_SETTING_FASTBOOT;
   tagSettings.enableRFWake = DEFAULT_SETTING_RFWAKE;
   tagSettings.enableTagRoaming = DEFAULT_SETTING_TAGROAMING;
   tagSettings.enableScanForAPAfterTimeout = DEFAULT_SETTING_SCANFORAP;
   tagSettings.enableLowBatSymbol = DEFAULT_SETTING_LOWBATSYMBOL;
   tagSettings.enableNoRFSymbol = DEFAULT_SETTING_NORFSYMBOL;
   tagSettings.customMode = 0;
   tagSettings.fastBootCapabilities = 0;
   tagSettings.minimumCheckInTime = INTERVAL_BASE;
   tagSettings.fixedChannel = 1;
   tagSettings.batLowVoltage = BATTERY_VOLTAGE_MINIMUM;
}

void loadSettingsFromBuffer(uint8_t* p) 
{
   SETTINGS_LOG("received settings from AP\n");
   switch(*p) {
      case SETTINGS_STRUCT_VERSION:  // the current tag struct
         xMemCopyShort((void*)tagSettings, (void*)p, sizeof(struct tagsettings));
         break;

      default:
         SETTINGS_LOG("received something we couldn't really process, version %d\n");
         break;
   }
   writeSettings();
}

static bool compareSettings() 
{
   uint8_t *__xdata settingsTempBuffer = malloc(sizeof(struct tagsettings));
   bool Ret = false;

// check if the settings match the settings in the eeprom
   eepromRead(EEPROM_SETTINGS_AREA_START, (void*)settingsTempBuffer, sizeof(struct tagsettings));
   if(memcmp((void*)settingsTempBuffer,(void*)tagSettings,sizeof(struct tagsettings)) == 0) {
   // same
      Ret = true;
   }

   free(settingsTempBuffer);
   // different
   return false;
}

#if 0
// add an upgrade strategy whenever you update the struct version
static void upgradeSettings() 
{
}
#endif

void loadSettings() 
{
   uint8_t *__xdata settingsTempBuffer = malloc(sizeof(struct tagsettings));

   eepromRead(EEPROM_SETTINGS_AREA_START + 4, (void*)settingsTempBuffer, sizeof(struct tagsettings));
   xMemCopyShort((void*)&tagSettings, (void*)settingsTempBuffer, sizeof(struct tagsettings));
   uint32_t __xdata valid = 0;
   eepromRead(EEPROM_SETTINGS_AREA_START, (void*)&valid, 4);
   xMemCopy((void*)tagSettings, (void*)settingsTempBuffer, sizeof(struct tagsettings));
   if(tagSettings.settingsVer == 0xFF || valid != SETTINGS_MAGIC) {
   // settings not set. load the defaults
      loadDefaultSettings();
      SETTINGS_LOG("Loaded defaults\n");
   }
#if 0
   else if(tagSettings.settingsVer < SETTINGS_STRUCT_VERSION) {
   // upgrade
      upgradeSettings();
      SETTINGS_LOG("Upgraded from previous version\n");
   }
#endif
   else {
   // settings are valid
      SETTINGS_LOG("Loaded from EEPROM\n");
   }
   free(settingsTempBuffer);
}

void writeSettings() 
{
   if(compareSettings()) {
      SETTINGS_LOG("No change\n");
      return;
   }
   eepromErase(EEPROM_SETTINGS_AREA_START, 1);
   uint32_t __xdata valid = SETTINGS_MAGIC;
   eepromWrite(EEPROM_SETTINGS_AREA_START, (void*)&valid, 4);
   eepromWrite(EEPROM_SETTINGS_AREA_START + 4, (void*)&tagSettings, sizeof(tagSettings));
   SETTINGS_LOG("Save settings\n");
}

void invalidateSettingsEEPROM() 
{
   int32_t __xdata valid = 0x0000;
   SETTINGS_LOG("Invalidate settings\n");
   eepromWrite(EEPROM_SETTINGS_AREA_START, (void*)&valid, 4);
}

