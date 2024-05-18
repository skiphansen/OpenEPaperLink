#ifndef _LOGGING_H_
#define _LOGGING_H_

extern void DumpHex(const uint8_t *__xdata a, const uint16_t __xdata l);

// LOGA always logs
#define LOGA(format, ... ) pr(format,## __VA_ARGS__)

// LOGE always logs
#define LOGE(format, ... ) pr(format,## __VA_ARGS__)

// LOG logs only when debugging is enable
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

#ifdef DEBUGBLOCKS
   #define BLOCK_LOG(format, ... ) pr(format,## __VA_ARGS__)
#else
   #define BLOCK_LOG(format, ... )
#endif

#ifdef DEBUGSETTINGS
   #define SETTINGS_LOG(format, ... ) pr(format,## __VA_ARGS__)
   #define SETTINGS_LOG_HEX(x,y) DumpHex(x,y)
#else
   #define SETTINGS_LOG(format, ... )
   #define SETTINGS_LOG_HEX(x,y)
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

#ifdef DEBUGDRAWING
   #define DRAW_LOG(format, ... ) pr(format,## __VA_ARGS__)
   #define DRAW_LOG_HEX(x,y) DumpHex(x,y)
#else
   #define DRAW_LOG(format, ... )
   #define DRAW_LOG_HEX(x,y)
#endif

#if defined(DEBUGOTA)
   #define OTA_LOG(format, ... ) pr(format,## __VA_ARGS__)
   #define OTA_LOG_HEX(x,y) DumpHex(x,y)
#else
   #define OTA_LOG(format, ... )
   #define OTA_LOG_HEX(x,y)
#endif

#ifdef DEBUG_CHIP_CFG
   void CopyCfg(void);
   void PrintCfg(const char __code *Msg);
   #define COPY_CFG() CopyCfg()
   #define LOG_CONFIG(x)   CopyCfg();PrintCfg(x)
   #define PRINT_CONFIG(x) PrintCfg(x)
#else
   #define LOG_CONFIG(x)
   #define PRINT_CONFIG(x)
   #define COPY_CFG()
#endif

#endif   // _LOGGING_H_

