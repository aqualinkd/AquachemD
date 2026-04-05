#ifndef UTILS_H_
#define UTILS_H_

#include <syslog.h>
#include <strings.h>
#include <ctype.h>
//#include <stdbool.h>
//#include <stdint.h>
//#include <time.h>

void LOG(const int msg_level, const char * format, ...);
void FORCE_LOG(const int msg_level, const char * format, ...);

char *cleanwhitespace(char *str);
bool text2bool(char *str);
char *bool2text(bool val);
float degFtoC(float degF);
float degCtoF(float degC);

#define STR_FULL_LENGTH  -1. // For function strncasefind(), cleanalloc(), etc.
const char *strncasestr(const char *haystack, const char *needle, int length);
char *cleanalloc(char *str, int length);
char *strcsub(char *dst, int dst_len, const char *src, char find, char replace);


#define isMASKSET(bitmask, mask) ((bitmask & mask) == mask)
#define setMASK(bitmask, mask)    (bitmask |= mask)
#define removeMASK(bitmask, mask) (bitmask &= ~mask)

#define ACD_MAX(x, y) (((x) > (y)) ? (x) : (y))
#define ACD_MIN(x, y) (((x) < (y)) ? (x) : (y))
#define ACD_CLAMP(value, min_val, max_val) ((value) > (max_val) ? (max_val) : ((value) < (min_val) ? (min_val) : (value)))


/*
#define LOG_EMERG       0       // system is unusable 
#define LOG_ALERT       1       // action must be taken immediately 
#define LOG_CRIT        2       // critical conditions 
#define LOG_ERR         3       // error conditions
#define LOG_WARNING     4       // warning conditions 
#define LOG_NOTICE      5       // normal but significant condition 
#define LOG_INFO        6       // informational 
#define LOG_DEBUG       7       // debug-level messages 
*/


static inline const char* log_priority_to_str(int level) {
    switch (level) {
        case LOG_ERR:     return "Error";
        case LOG_WARNING: return "Warning";
        case LOG_NOTICE:  return "Notice";
        case LOG_INFO:    return "Info";
        case LOG_DEBUG:   return "Debug";
        default:          return "Unknown";
    }
}

static inline int log_str_to_priority(const char *name) {
    if (!name) return -1; // Safety check

    if (strcasecmp(name, "Error") == 0)   return LOG_ERR;
    if (strcasecmp(name, "Warning") == 0) return LOG_WARNING;
    if (strcasecmp(name, "Notice") == 0)  return LOG_NOTICE;
    if (strcasecmp(name, "Info") == 0)    return LOG_INFO;
    if (strcasecmp(name, "Debug") == 0)   return LOG_DEBUG;

    return -1; // Or return a default like LOG_DEBUG
}


#endif // UTILS_H_