#ifndef _LOGGING_H_
#define _LOGGING_H_

#define LOG(format, ... ) pr(format,## __VA_ARGS__)

#ifdef DEBUGEEPROM
   #define EEPROM_LOG(format, ... ) pr(format,## __VA_ARGS__)
#else
   #define EEPROM_LOG(format, ... )
#endif

#ifdef DEBUGMAIN
   #define MAIN_LOG(format, ... ) pr(format,## __VA_ARGS__)
#else
   #define MAIN_LOG(format, ... )
#endif

#ifdef DEBUGPROTO
   #define PROTO_LOG(format, ... ) pr(format,## __VA_ARGS__)
#else
   #define PROTO_LOG(format, ... )
#endif

#ifdef DEBUGSETTINGS
   extern const char __code gSettingsPrefix[];
   #define PROTO_LOG(format, ... ) pr("%s" format,gSettingsPrefix, ## __VA_ARGS__)
#else
#define SETTINGS_LOG(format, ... )
#endif

#endif   // _LOGGING_H_

