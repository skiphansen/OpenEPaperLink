#ifndef _LOGGING_H_
#define _LOGGING_H_

#define DEBUG_LOG    1
#define VERBOSE_LOG  1

void DumpHex(void *AdrIn,int Len);
int LogPrintf(char *fmt, ...);
char *CnvTime(time_t Time);
char *Mac2String(tagRecord* taginfo);
#define _LOG(format, ...) Serial.printf(format,## __VA_ARGS__)

// ALOG always logs
#define ALOG(format, ... ) _LOG(format,## __VA_ARGS__)

// ELOG always logs
#define ELOG(format, ...) _LOG("%s#%d: " format,__FUNCTION__,__LINE__,## __VA_ARGS__)

#if DEBUG_LOG != 0
   #define LOG(format, ... ) _LOG("%s: " format,__FUNCTION__,## __VA_ARGS__)
   #define LOG_RAW(format, ... ) _LOG(format,## __VA_ARGS__)
   #define DUMP_HEX(x,y) DumpHex(x,y)
#else
   #define LOG(format, ... )
   #define LOG_RAW(format, ... )
   #define DUMP_HEX(x,y)
#endif   // DEBUG_LOGGING

#if VERBOSE_LOG == 1
   #define VLOG(format, ... ) _LOG("%u %s: " format,clock_time(),__FUNCTION__,## __VA_ARGS__)
   #define VLOG_RAW(format, ... ) _LOG(format,## __VA_ARGS__)
   #define VDUMP_HEX(x,y) DumpHex(x,y)
#else
   #define VLOG(format, ... )
   #define VLOG_RAW(format, ... )
   #define VDUMP_HEX(x,y)
#endif   // VERBOSE_DEUG_LOGGING

#endif   // _LOGGING_H_

