/** @file
 * Routines for handling character sets
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef __CHARSETS_H__
#define __CHARSETS_H__


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*
 * Translation tables that map the upper 128 code points in single-byte
 * "extended ASCII" character encodings to Unicode code points in the
 * Basic Multilingual Plane.
 */

/* Table for windows-1250 */
extern const gunichar2 charset_table_cp1250[0x80];
/* Table for windows-1251 */
extern const gunichar2 charset_table_cp1251[0x80];
/* Table for windows-1252 */
extern const gunichar2 charset_table_cp1252[0x80];

/* Tables for ISO-8859-X */
extern const gunichar2 charset_table_iso_8859_2[0x80];
extern const gunichar2 charset_table_iso_8859_3[0x80];
extern const gunichar2 charset_table_iso_8859_4[0x80];
extern const gunichar2 charset_table_iso_8859_5[0x80];
extern const gunichar2 charset_table_iso_8859_6[0x80];
extern const gunichar2 charset_table_iso_8859_7[0x80];
extern const gunichar2 charset_table_iso_8859_8[0x80];
extern const gunichar2 charset_table_iso_8859_9[0x80];
extern const gunichar2 charset_table_iso_8859_10[0x80];
extern const gunichar2 charset_table_iso_8859_11[0x80];
extern const gunichar2 charset_table_iso_8859_13[0x80];
extern const gunichar2 charset_table_iso_8859_14[0x80];
extern const gunichar2 charset_table_iso_8859_15[0x80];
extern const gunichar2 charset_table_iso_8859_16[0x80];

/* Tables for Mac character sets */
extern const gunichar2 charset_table_mac_roman[0x80];

/* Tables for DOS code pages */
extern const gunichar2 charset_table_cp437[0x80];
extern const gunichar2 charset_table_cp855[0x80];
extern const gunichar2 charset_table_cp866[0x80];

/*
 * Translation tables that map the lower 128 code points in single-byte
 * ISO 646-based character encodings to Unicode code points in the
 * Basic Multilingual Plane.
 */
extern const gunichar2 charset_table_iso_646_basic[0x80];

/* Tables for EBCDIC code pages */
extern const gunichar2 charset_table_ebcdic[256];
extern const gunichar2 charset_table_ebcdic_cp037[256];
extern const gunichar2 charset_table_ebcdic_cp500[256];

uint8_t * get_ascii_string(wmem_allocator_t *scope, const uint8_t *ptr, int length);

uint8_t * get_utf_8_string(wmem_allocator_t *scope, const uint8_t *ptr, int length);

uint8_t * get_iso_646_string(wmem_allocator_t *scope, const uint8_t *ptr, int length, const gunichar2 table[0x80]);

uint8_t * get_8859_1_string(wmem_allocator_t *scope, const uint8_t *ptr, int length);

uint8_t * get_unichar2_string(wmem_allocator_t *scope, const uint8_t *ptr, int length, const gunichar2 table[0x80]);

uint8_t * get_ucs_2_string(wmem_allocator_t *scope, const uint8_t *ptr, int length, unsigned encoding);

uint8_t * get_utf_16_string(wmem_allocator_t *scope, const uint8_t *ptr, int length, unsigned encoding);

uint8_t * get_ucs_4_string(wmem_allocator_t *scope, const uint8_t *ptr, int length, unsigned encoding);

uint8_t * get_ts_23_038_7bits_string_packed(wmem_allocator_t *scope, const uint8_t *ptr,
        const int bit_offset, int no_of_chars);

uint8_t * get_ts_23_038_7bits_string_unpacked(wmem_allocator_t *scope, const uint8_t *ptr,
        int length);

uint8_t * get_etsi_ts_102_221_annex_a_string(wmem_allocator_t *scope, const uint8_t *ptr,
        int length);

uint8_t * get_ascii_7bits_string(wmem_allocator_t *scope, const uint8_t *ptr,
        const int bit_offset, int no_of_chars);

uint8_t * get_nonascii_unichar2_string(wmem_allocator_t *scope, const uint8_t *ptr, int length, const gunichar2 table[256]);

uint8_t * get_gb18030_string(wmem_allocator_t *scope, const uint8_t *ptr, int length);

uint8_t * get_euc_kr_string(wmem_allocator_t *scope, const uint8_t *ptr, int length);

uint8_t * get_t61_string(wmem_allocator_t *scope, const uint8_t *ptr, int length);

uint8_t * get_dect_standard_8bits_string(wmem_allocator_t *scope, const uint8_t *ptr, int length);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __CHARSETS_H__ */

