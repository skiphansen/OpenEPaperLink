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

#ifdef DEBUG_NV_DATA
   #define NV_DATA_LOG(format, ... ) pr(format,## __VA_ARGS__)
#else
   #define NV_DATA_LOG(format, ... )
#endif

#ifdef DEBUG_SLEEP
   #define SLEEP_LOG(format, ... ) pr(format,## __VA_ARGS__)
#else
   #define SLEEP_LOG(format, ... )
#endif

#ifdef DEBUG_RX_DATA
   #define RX_DATA_LOG(format, ... ) pr(format,## __VA_ARGS__)
#else
   #define RX_DATA_LOG(format, ... )
#endif

#ifdef DEBUG_TX_DATA
   #define TX_DATA_LOG(format, ... ) pr(format,## __VA_ARGS__)
#else
   #define TX_DATA_LOG(format, ... )
#endif

#ifdef DEBUG_COMMS
   #define COMMS_LOG(format, ... ) pr(format,## __VA_ARGS__)
#else
   #define COMMS_LOG(format, ... )
#endif

#ifdef DEBUG_AP_SEARCH
   #define AP_SEARCH_LOG(format, ... ) pr(format,## __VA_ARGS__)
#else
   #define AP_SEARCH_LOG(format, ... )
#endif



#endif   // _LOGGING_H_

