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

void LogSettings(void);

#define SETTINGS_MAGIC 0xABBA5AA5
#define SUBGHZ_SETTINGS_VERSION 0x01
typedef struct {
   uint32_t Magic;            // SETTINGS_MAGIC
   uint8_t SubGhzSettingsVer; // SUBGHZ_SETTINGS_VERSION
   uint8_t CurrentChannel;
   uint8_t SubGhzBand;
   uint8_t Padding[128-sizeof(struct tagsettings) - 7];
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
   gCurrentChannel = 0;
   gSubGhzBand = BAND_UNKNOWN;
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

   xMemCopyShort((void*)&tagSettings,(void*)&Settings->OeplSettings,sizeof(tagSettings));
   if(Settings->Magic != SETTINGS_MAGIC || 
      Settings->SubGhzSettingsVer != SUBGHZ_SETTINGS_VERSION) 
   {  // settings not set. load the defaults
      LOGA("Loaded defaults settingsVer 0x%x Magic 0x%lx\n",
           Settings->SubGhzSettingsVer,Settings->Magic);
      loadDefaultSettings();
      return;
   }
// settings are valid
   gCurrentChannel = Settings->CurrentChannel;
   gSubGhzBand = Settings->SubGhzBand;

   LOGA("Loaded settings:\n");
   LogSettings();

#ifndef ISDEBUGBUILD
// Invalidate the settings in the SPI flash, we'll write them back later.
// This so the user can clear flash by removing the batteries during 
// the first screen refresh.
   SETTINGS_LOG("Invalidate settings\n");
   gTempBuf320[0] = 0;
   eepromWrite(EEPROM_SETTINGS_AREA_START,(void*)&gTempBuf320,1);
#endif
}

void writeSettings() 
{
// check if the settings match the settings in the eeprom
   eepromRead(EEPROM_SETTINGS_AREA_START,(void*)Settings,sizeof(*Settings));
   if(Settings->Magic == SETTINGS_MAGIC
      && memcmp((void*)&Settings->OeplSettings,(void*)tagSettings,sizeof(tagSettings)) == 0
      && Settings->CurrentChannel == gCurrentChannel)
   {
      SETTINGS_LOG("No change\n");
      return;
   }

   if(Settings->OeplSettings.fixedChannel != tagSettings.fixedChannel) {
      SETTINGS_LOG("Fixed channel %d -> %d\n",
                   Settings->OeplSettings.fixedChannel,
                   tagSettings.fixedChannel);

      if(tagSettings.fixedChannel != 0) {
         if(tagSettings.fixedChannel < 100) {
            LOGE("Ignored fixedChannel %d\n",Settings->OeplSettings.fixedChannel);
            tagSettings.fixedChannel = 0;
         }
         else if(tagSettings.fixedChannel >= 200) {
            gSubGhzBand = BAND_915;
         }
         else {
            gSubGhzBand = BAND_868;
         }
      }
   }

   xMemSet((void *)Settings,0,sizeof(SubGhzSettings));
   xMemCopyShort((void*)&Settings->OeplSettings,(void*)&tagSettings,sizeof(tagSettings));
   Settings->Magic = SETTINGS_MAGIC;
   Settings->SubGhzSettingsVer = SUBGHZ_SETTINGS_VERSION;
   Settings->CurrentChannel = gCurrentChannel;
   Settings->SubGhzBand = gSubGhzBand;

#if 0
   SETTINGS_LOG("Wrote:\n");
   SETTINGS_LOG_HEX((void *)Settings,sizeof(SubGhzSettings));
#endif
   eepromErase(EEPROM_SETTINGS_AREA_START,1);
   eepromWrite(EEPROM_SETTINGS_AREA_START,(void*)Settings,sizeof(SubGhzSettings));
   LOGA("Saved settings:\n");
   LogSettings();
}
#undef Settings

void LogSettings() 
{
   LOGA("  Scan After Timeout %d\n",tagSettings.enableScanForAPAfterTimeout);
   LOGA("  fixedChannel %d\n",tagSettings.fixedChannel);
   LOGA("  batLowVoltage %d\n",tagSettings.batLowVoltage);
   LOGA("  enableFastBoot %d\n",tagSettings.enableFastBoot);
   LOGA("  Last ch %d\n",gCurrentChannel);
// The following are not that interesting
   SETTINGS_LOG("  customMode %d\n",tagSettings.customMode);
   SETTINGS_LOG("  min Check In Time %d\n",tagSettings.minimumCheckInTime);
   SETTINGS_LOG("  gSubGhzBand %d\n",gSubGhzBand);
}


