#ifndef __CONFIG_H__
#define __CONFIG_H__

#define MAX_DECODE_AS_PROMPT_LEN    200


#define PACKAGE "wireshark"
#define VERSION_EXTRA ""
#define VERSION "4.5.0"
#define VERSION_MAJOR 4
#define VERSION_MINOR 5
#define VERSION_MICRO 0
#define LOG_VERSION "0.9.0"
#define PLUGIN_PATH_ID ""
#define VERSION_FLAVOR "Development Build"
#define HAVE_CLOCK_GETTIME 1
#define HAVE_TIMESPEC_GET 1
#define HAVE_LOCALTIME_R 1
#define HAVE_GMTIME_R 1
#define HAVE_TIMEGM 1
#define HAVE_TZSET 1
#define HAVE_IFADDRS_H 1
#define HAVE_GETIFADDRS 1
#define HAVE_GRP_H 1
#define ZLIBNG_VERSION_STRING ""
#define MINIZIP_VERSION ""
#define MINIZIPNG_VERSION ""
#define HAVE_LINUX_SOCKIOS_H 1
#define HAVE_LINUX_IF_BONDING_H 1
#define HAVE_NETDB_H 1
#define HAVE_PWD_H 1
#define HAVE_STRPTIME 1
#define HAVE_MEMMEM 1
#define HAVE_MEMRCHR 1
#define HAVE_STRERRORNAME_NP 1
#define HAVE_VASPRINTF 1
#define HAVE_SYS_UTSNAME_H 1
#define HAVE_STRUCT_TM_TM_GMTOFF 1
#define HAVE_STRUCT_TM_TM_ZONE 1
#define HAVE_TZNAME 1
#define HAVE_SSIZE_T 1


#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <glib.h>

#include <syslog.h>


#define ws_error(format, a...)                          \
 do {                                                    \
    syslog(LOG_ERR | LOG_USER, "KERR KILL %s"            \
    " line:%d   " format "\n\n",  \
    (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__), \
     __LINE__, ## a);                                    \
    exit(-1);                                            \
    } while (0)

#define ws_critical ws_error

#define ws_debug(format, a...)                               \
 do {                                                    \
    syslog(LOG_ERR | LOG_USER, "KERR: %s"                 \
    " line:%d " format "\n",   \
    (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__), \
     __LINE__, ## a);                                    \
    } while (0)


#define ws_noisy ws_debug

#define ws_warning ws_debug
#define ws_message ws_debug
#define ws_info ws_debug
/* ws_log(domain, level, fmt, ...) — discard domain and level, pass rest to ws_debug */
#define ws_log(domain, level, format, a...) ws_debug(format, ## a)
#define KERR ws_debug


#include <ws_log_defs.h>
#endif /* __CONFIG_H__ */
