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
#include "eeprom.h"
#include "../../oepl-definitions.h"
#include "../../oepl-proto.h"
#include "syncedproto.h"
#include "logging.h"

#define SETTINGS_MAGIC 0xABBA5AA5
typedef struct {
   uint32_t Magic;   // SETTINGS_MAGIC
   uint8_t CurrentChannel;
   uint8_t Padding[128-sizeof(struct tagsettings) - 5];
   struct tagsettings OeplSettings;
} SubGhzSettings;

struct tagsettings __xdata tagSettings;

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
   tagSettings.fixedChannel = 0;
   tagSettings.batLowVoltage = BATTERY_VOLTAGE_MINIMUM;
}

void loadSettingsFromBuffer(uint8_t* p) 
{
   if(*p == SETTINGS_STRUCT_VERSION) {
      SETTINGS_LOG("Saved settings from AP\n");
      xMemCopyShort((void*)tagSettings, (void*)p, sizeof(struct tagsettings));
      writeSettings();
   }
   else {
      SETTINGS_LOG("WTF ver %d\n",*p);
   }
}

#if 0
// add an upgrade strategy whenever you update the struct version
static void upgradeSettings() 
{
}
#endif

#define Settings  ((SubGhzSettings *__xdata) gTempBuf320)
void loadSettings() 
{

   eepromRead(EEPROM_SETTINGS_AREA_START,gTempBuf320,sizeof(SubGhzSettings));
   SETTINGS_LOG("Read:\n");
   SETTINGS_LOG_HEX(gTempBuf320,sizeof(SubGhzSettings));

   xMemCopyShort((void*)&tagSettings,(void*)&Settings->OeplSettings,sizeof(tagSettings));
   if(tagSettings.settingsVer == 0xFF || Settings->Magic != SETTINGS_MAGIC) {
   // settings not set. load the defaults
      SETTINGS_LOG("Loaded defaults settingsVer 0x%x Magic 0x%lx\n",
                   tagSettings.settingsVer,Settings->Magic);
      loadDefaultSettings();
      currentChannel = 0;
      return;
   }
   currentChannel = Settings->CurrentChannel;
// settings are valid
   SETTINGS_LOG("Settings loaded\n");
}

void writeSettings() 
{
// check if the settings match the settings in the eeprom
   eepromRead(EEPROM_SETTINGS_AREA_START,(void*)Settings,sizeof(*Settings));
   if(Settings->Magic == SETTINGS_MAGIC
      && memcmp((void*)&Settings->OeplSettings,(void*)tagSettings,sizeof(tagSettings)) == 0
      && Settings->CurrentChannel == currentChannel)
   {
      SETTINGS_LOG("No change\n");
      return;
   }
   xMemSet((void *)Settings,0,sizeof(SubGhzSettings));
   Settings->Magic = SETTINGS_MAGIC;
   Settings->CurrentChannel = currentChannel;
   xMemCopyShort((void*)&Settings->OeplSettings,(void*)&tagSettings,sizeof(tagSettings));
   SETTINGS_LOG("Wrote:\n");
   SETTINGS_LOG_HEX((void *)Settings,sizeof(SubGhzSettings));
   eepromErase(EEPROM_SETTINGS_AREA_START,1);
   eepromWrite(EEPROM_SETTINGS_AREA_START,(void*)Settings,sizeof(SubGhzSettings));
   SETTINGS_LOG("Saved settings\n");
}
#undef Settings

void invalidateSettingsEEPROM() 
{
   int32_t __xdata valid = 0x0000;
   SETTINGS_LOG("Invalidate settings\n");
   eepromWrite(EEPROM_SETTINGS_AREA_START, (void*)&valid, 4);
}

