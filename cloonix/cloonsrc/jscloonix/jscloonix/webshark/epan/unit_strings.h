/* unit_strings.h
 * Units to append to field values
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __UNIT_STRINGS_H__
#define __UNIT_STRINGS_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


typedef struct unit_name_string {
    char *singular;     /**< name to use for 1 unit */
    char *plural;       /**< name to use for < 1 or > 1 units */
} unit_name_string;

const char* unit_name_string_get_value(uint32_t value, const unit_name_string* units);
const char* unit_name_string_get_value64(uint64_t value, const unit_name_string* units);
const char* unit_name_string_get_double(double value, const unit_name_string* units);
extern const unit_name_string units_foot_feet;
extern const unit_name_string units_bit_bits;
extern const unit_name_string units_byte_bytes;
extern const unit_name_string units_byte_bytespsecond;
extern const unit_name_string units_octet_octets;
extern const unit_name_string units_word_words;
extern const unit_name_string units_tick_ticks;
extern const unit_name_string units_meters;
extern const unit_name_string units_meter_meters;
extern const unit_name_string units_centimeters;
extern const unit_name_string units_centimeter_centimeters;
extern const unit_name_string units_millimeters;
extern const unit_name_string units_millimeter_millimeters;
extern const unit_name_string units_week_weeks;
extern const unit_name_string units_day_days;
extern const unit_name_string units_hour_hours;
extern const unit_name_string units_hours;
extern const unit_name_string units_minute_minutes;
extern const unit_name_string units_minutes;
extern const unit_name_string units_second_seconds; // full unit name "second[s?]"
extern const unit_name_string units_seconds;        //only seconds abbreviation "s"
extern const unit_name_string units_millisecond_milliseconds; // full unit name "millisecond[s?]"
extern const unit_name_string units_milliseconds;        //only seconds abbreviation "ms"
extern const unit_name_string units_microsecond_microseconds; // full unit name "microsecond[s?]"
extern const unit_name_string units_microseconds;        //only seconds abbreviation "us"
extern const unit_name_string units_nanosecond_nanoseconds; // full unit name "nanosecond[s?]"
extern const unit_name_string units_nanoseconds; //only seconds abbreviation "ns"
extern const unit_name_string units_nanometers;
extern const unit_name_string units_degree_degrees;
extern const unit_name_string units_degree_celsius;
extern const unit_name_string units_degree_bearing;
extern const unit_name_string units_centibels;
extern const unit_name_string units_decibels;
extern const unit_name_string units_dbm;
extern const unit_name_string units_dbi;
extern const unit_name_string units_dbhz;
extern const unit_name_string units_mbm;
extern const unit_name_string units_percent;
extern const unit_name_string units_khz;
extern const unit_name_string units_ghz;
extern const unit_name_string units_mhz;
extern const unit_name_string units_hz;
extern const unit_name_string units_hz_s;
extern const unit_name_string units_kbit;
extern const unit_name_string units_kbps;
extern const unit_name_string units_kibps;
extern const unit_name_string units_pkts;
extern const unit_name_string units_pkts_per_sec;
extern const unit_name_string units_km;
extern const unit_name_string units_kmh;
extern const unit_name_string units_m_s;
extern const unit_name_string units_cm_s;
extern const unit_name_string units_mm_s;
extern const unit_name_string units_milliamps;
extern const unit_name_string units_microwatts;
extern const unit_name_string units_volt;
extern const unit_name_string units_grams_per_second;
extern const unit_name_string units_meter_sec;
extern const unit_name_string units_meter_sec_squared;
extern const unit_name_string units_bit_sec;
extern const unit_name_string units_segment_remaining;
extern const unit_name_string units_frame_frames;
extern const unit_name_string units_revolutions_per_minute;
extern const unit_name_string units_kilopascal;
extern const unit_name_string units_newton_metre;
extern const unit_name_string units_liter_per_hour;
extern const unit_name_string units_amp;
extern const unit_name_string units_watthour;
extern const unit_name_string units_watt;
extern const unit_name_string units_bpm;
extern const unit_name_string units_calorie;
extern const unit_name_string units_cycle_cycles;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __UNIT_STRINGS_H__ */
