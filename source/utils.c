

#define SYSLOG_NAMES
#include <stdlib.h>
#include <syslog.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <systemd/sd-journal.h>
#include <string.h>
#include <ctype.h>


#include "utils.h"
#include "config.h"




void LOG(const int msg_level, const char * format, ...)
{
  va_list args;

  if ( msg_level > _acdconfig_.log_level) {
    return;
  }

  va_start(args, format);
  sd_journal_printv(msg_level, format, args);
  va_end(args);

  if (_acdconfig_.deamonize == false) {
    char message[1024];
    va_list args;
    va_start(args, format);
    int len = vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    // Trim all trailing newlines and carriage returns
    while (len > 0 && (message[len - 1] == '\n' || message[len - 1] == '\r')) {
      message[--len] = '\0';
    }

    printf("%-7.7s: %s\n", log_priority_to_str(msg_level), message);
    //if (msg_level <= LOG_ERR)
    //  fprintf(stderr, "%s: %s\n", log_priority_to_str(msg_level), message);
  }
}

//Move existing pointer
char *cleanwhitespace(char *str)
{
  char *end;

  if (str == NULL)
    return str;

  // Trim leading space
  while(isspace(*str)) str++;

  if(*str == 0)  // All spaces?
    return str;

  // Trim trailing space
  end = str + strlen(str) - 1;
  while(end > str && isspace(*end)) end--;

  // Write new null terminator
  if (end != (str + strlen(str) - 1) )
    *(end+1) = 0;

  return str;
}

// Return a new string with whitespace trimmed and memory allocated for it. Caller must free.
// str: input string to clean and allocate
// length: if STR_FULL_LENGTH, allocate for entire cleaned string; otherwise allocate for specified length
char *cleanalloc(char *str, int length)
{
  if (str == NULL)
    return NULL;

  str = cleanwhitespace(str);

  size_t str_len = strlen(str);
  char  *result;

  if (length == STR_FULL_LENGTH || length <= 0)
  {
    // Allocate for string + null terminator (already present but be explicit)
    result = (char *)malloc(str_len + 1);
    if (!result) return NULL;
    strcpy(result, str);
  }
  else
  {
    result = (char *)malloc(length + 1);   // +1 to guarantee room for null
    if (!result) return NULL;
    strncpy(result, str, length);
    result[length] = '\0';                 // ensure null terminated
  }

  // Guarantee null termination — fastest check is last byte
  if (result[str_len] != '\0')
    result[str_len] = '\0';

  return result;
}

// Search for needle in haystack, case-insensitive, length-limited.
// length: max bytes of haystack to search, or STR_FULL_LENGTH for full string.
// Returns pointer to first match in haystack, or NULL if not found / error.
const char *strncasestr(const char *haystack, const char *needle, int length)
{
  // Null guard
  if (!haystack || !needle) return NULL;

  // Empty needle matches at start (consistent with strstr behaviour)
  if (!*needle) return haystack;

  // Determine actual search window
  size_t haystack_len = (length == STR_FULL_LENGTH) ? strlen(haystack) : (size_t)length;
  size_t needle_len   = strlen(needle);

  // Can't match if needle is longer than the search window
  if (needle_len > haystack_len) return NULL;

  // Pre-compute first character of needle in both cases to avoid
  // tolower() on every byte — we only call strncasecmp on a first-char hit
  unsigned char first_lo = (unsigned char)tolower((unsigned char)needle[0]);
  unsigned char first_hi = (unsigned char)toupper((unsigned char)needle[0]);

  // Last position in haystack where a match could start
  const char *end = haystack + haystack_len - needle_len;

  for (const char *h = haystack; h <= end; h++)
  {
    unsigned char c = (unsigned char)*h;

    // Fast first-character filter
    if (c != first_lo && c != first_hi)
      continue;

    // First char matched — check the rest
    if (strncasecmp(h, needle, needle_len) == 0)
      return h;
  }

  return NULL;
}

// Replace all occurrences of 'find' with 'replace' in 'src', copying to 'dst'.
// dst: destination buffer to write result (must be pre-allocated by caller)
// dst_len is the total size of the destination buffer, including space for null terminator.
// src: input string to process
// find: character to replace, use '-', not "-", to avoid confusion with string literals
// replace: character to replace with, use ' '
// Returns dst on success

char *strcsub(char *dst, int dst_len, const char *src, char find, char replace) 
{
  int i;
  int len = strlen(src);

  // Standard safety: Ensure we don't overflow the destination buffer
  if (dst_len > 0) {
    len = ACD_MIN(len, dst_len - 1); // -1 to leave room for \0
  }

  for (i = 0; i < len; i++) {
    dst[i] = (src[i] == find) ? replace : src[i];
  }
  dst[i] = '\0';

  return dst;
}


bool text2bool(char *str)
{
  str = cleanwhitespace(str);
  if (strcasecmp (str, "YES") == 0 || strcasecmp (str, "ON") == 0)
    return true;
  else
    return false;
}

char *bool2text(bool val)
{
  if(val == true)
    return "YES";
  else
    return "NO";
}

// (50°F - 32) x .5556 = 10°C
float degFtoC(float degF)
{
  return ((degF-32) / 1.8);
}
// 30°C x 1.8 + 32 = 86°F 
float degCtoF(float degC)
{
  return (degC * 1.8 + 32);
}