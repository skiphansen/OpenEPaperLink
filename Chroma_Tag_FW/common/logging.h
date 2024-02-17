#ifndef _LOGGING_H_
#define _LOGGING_H_

#define ELOG(format, ... ) pr("%s#%d" format,__FUNCTION__,__LINE__, ## __VA_ARGS__)

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
   extern const char __code gProtoPrefix[];
   #define PROTO_LOG(format, ... ) pr("%s" format,gProtoPrefix, ## __VA_ARGS__)
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

