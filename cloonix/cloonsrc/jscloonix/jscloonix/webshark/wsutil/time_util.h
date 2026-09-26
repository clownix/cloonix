/** @file
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __TIME_UTIL_H__
#define __TIME_UTIL_H__

#include <wireshark.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/** Converts a broken down date representation, relative to UTC,
 * to a timestamp
 */

time_t mktime_utc(struct tm *tm);

/** Validate the values in a time_t.
 * Currently checks tm_year, tm_mon, tm_mday, tm_hour, tm_min, and tm_sec;
 * disregards tm_wday, tm_yday, and tm_isdst.
 *
 * @param tm The struct tm to validate.
 */

bool tm_is_valid(struct tm *tm);

uint64_t create_timestamp(void);


void ws_tzset(void);


struct timespec *ws_clock_get_realtime(struct timespec *ts);


struct tm *ws_localtime_r(const time_t *timep, struct tm *result);


struct tm *ws_gmtime_r(const time_t *timep, struct tm *result);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __TIME_UTIL_H__ */
