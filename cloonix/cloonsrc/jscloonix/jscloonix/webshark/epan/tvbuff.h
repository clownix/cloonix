/** @file
 *
 * Testy, Virtual(-izable) Buffer of uint8_t*'s
 *
 * "Testy" -- the buffer gets mad when an attempt is made to access data
 *      beyond the bounds of the buffer. An exception is thrown.
 *
 * "Virtual" -- the buffer can have its own data, can use a subset of
 *      the data of a backing tvbuff, or can be a composite of
 *      other tvbuffs.
 *
 * Copyright (c) 2000 by Gilbert Ramirez <gram@alumni.rice.edu>
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __TVBUFF_H__
#define __TVBUFF_H__

#include <epan/guid-utils.h>
#include <wsutil/inet_addr.h>
#include <wsutil/nstime.h>
#include "wsutil/ws_mempbrk.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct tvbuff;
typedef struct tvbuff tvbuff_t;

typedef void (*tvbuff_free_cb_t)(void*);

tvbuff_t *tvb_new_octet_aligned(tvbuff_t *tvb, uint32_t bit_offset, int32_t no_of_bits);
tvbuff_t *tvb_new_octet_right_aligned(tvbuff_t *tvb, uint32_t bit_offset, int32_t no_of_bits);
tvbuff_t *tvb_new_chain(tvbuff_t *parent, tvbuff_t *backing);
tvbuff_t *tvb_clone(tvbuff_t *tvb);
tvbuff_t *tvb_clone_offset_len(tvbuff_t *tvb, unsigned offset, unsigned len);
void tvb_free(tvbuff_t *tvb);
void tvb_free_chain(tvbuff_t *tvb);
void tvb_set_free_cb(tvbuff_t *tvb, const tvbuff_free_cb_t func);
void tvb_set_child_real_data_tvbuff(tvbuff_t *parent, tvbuff_t *child);
tvbuff_t *tvb_new_child_real_data(tvbuff_t *parent,
    const uint8_t *data, const unsigned length, const int reported_length);
tvbuff_t *tvb_new_real_data(const uint8_t *data,
    const unsigned length, const int reported_length);
tvbuff_t *tvb_new_subset_length_caplen(tvbuff_t *backing,
    const int backing_offset, const int backing_length,
    const int reported_length);
tvbuff_t *tvb_new_subset_length(tvbuff_t *backing,
    const int backing_offset, const int reported_length);
tvbuff_t *tvb_new_subset_remaining(tvbuff_t *backing,
    const int backing_offset);
void tvb_composite_append(tvbuff_t *tvb, tvbuff_t *member);
extern void tvb_composite_prepend(tvbuff_t *tvb, tvbuff_t *member);

tvbuff_t *tvb_new_composite(void);
void tvb_composite_finalize(tvbuff_t *tvb);
unsigned tvb_captured_length(const tvbuff_t *tvb);
int tvb_captured_length_remaining(const tvbuff_t *tvb, const int offset);
unsigned tvb_ensure_captured_length_remaining(const tvbuff_t *tvb, const int offset);
bool tvb_bytes_exist(const tvbuff_t *tvb, const int offset, const int length);
void tvb_ensure_bytes_exist64(const tvbuff_t *tvb, const int offset, const uint64_t length);
void tvb_ensure_bytes_exist(const tvbuff_t *tvb, const int offset, const int length);
bool tvb_offset_exists(const tvbuff_t *tvb, const int offset);
unsigned tvb_reported_length(const tvbuff_t *tvb);
int tvb_reported_length_remaining(const tvbuff_t *tvb, const int offset);
unsigned tvb_ensure_reported_length_remaining(const tvbuff_t *tvb, const int offset);
void tvb_set_reported_length(tvbuff_t *tvb, const unsigned);
void tvb_fix_reported_length(tvbuff_t *tvb);
unsigned tvb_offset_from_real_beginning(const tvbuff_t *tvb);
int tvb_raw_offset(tvbuff_t *tvb);
void tvb_set_fragment(tvbuff_t *tvb);
struct tvbuff *tvb_get_ds_tvb(tvbuff_t *tvb);

uint8_t tvb_get_uint8(tvbuff_t *tvb, const int offset);
static inline uint8_t tvb_get_guint8(tvbuff_t *tvb, const int offset) { return tvb_get_uint8(tvb, offset); }
int8_t tvb_get_int8(tvbuff_t *tvb, const int offset);
static inline int8_t tvb_get_gint8(tvbuff_t *tvb, const int offset) { return tvb_get_int8(tvb, offset); }

uint16_t tvb_get_ntohs(tvbuff_t *tvb, const int offset);
int16_t tvb_get_ntohis(tvbuff_t *tvb, const int offset);
uint32_t tvb_get_ntoh24(tvbuff_t *tvb, const int offset);
int32_t tvb_get_ntohi24(tvbuff_t *tvb, const int offset);
uint32_t tvb_get_ntohl(tvbuff_t *tvb, const int offset);
int32_t tvb_get_ntohil(tvbuff_t *tvb, const int offset);
uint64_t tvb_get_ntoh40(tvbuff_t *tvb, const int offset);
int64_t tvb_get_ntohi40(tvbuff_t *tvb, const int offset);
uint64_t tvb_get_ntoh48(tvbuff_t *tvb, const int offset);
int64_t tvb_get_ntohi48(tvbuff_t *tvb, const int offset);
uint64_t tvb_get_ntoh56(tvbuff_t *tvb, const int offset);
int64_t tvb_get_ntohi56(tvbuff_t *tvb, const int offset);
uint64_t tvb_get_ntoh64(tvbuff_t *tvb, const int offset);
int64_t tvb_get_ntohi64(tvbuff_t *tvb, const int offset);
float tvb_get_ntohieee_float(tvbuff_t *tvb, const int offset);
double tvb_get_ntohieee_double(tvbuff_t *tvb, const int offset);
uint16_t tvb_get_letohs(tvbuff_t *tvb, const int offset);
int16_t tvb_get_letohis(tvbuff_t *tvb, const int offset);
uint32_t tvb_get_letoh24(tvbuff_t *tvb, const int offset);
int32_t tvb_get_letohi24(tvbuff_t *tvb, const int offset);
uint32_t tvb_get_letohl(tvbuff_t *tvb, const int offset);
int32_t tvb_get_letohil(tvbuff_t *tvb, const int offset);
uint64_t tvb_get_letoh40(tvbuff_t *tvb, const int offset);
int64_t tvb_get_letohi40(tvbuff_t *tvb, const int offset);
uint64_t tvb_get_letoh48(tvbuff_t *tvb, const int offset);
int64_t tvb_get_letohi48(tvbuff_t *tvb, const int offset);
uint64_t tvb_get_letoh56(tvbuff_t *tvb, const int offset);
int64_t tvb_get_letohi56(tvbuff_t *tvb, const int offset);
uint64_t tvb_get_letoh64(tvbuff_t *tvb, const int offset);
int64_t tvb_get_letohi64(tvbuff_t *tvb, const int offset);
float tvb_get_letohieee_float(tvbuff_t *tvb, const int offset);
double tvb_get_letohieee_double(tvbuff_t *tvb, const int offset);
uint16_t tvb_get_uint16(tvbuff_t *tvb, const int offset, const unsigned encoding);
static inline uint16_t tvb_get_guint16(tvbuff_t *tvb, const int offset, const unsigned encoding) { return tvb_get_uint16(tvb, offset, encoding); }
int16_t tvb_get_int16(tvbuff_t *tvb, const int offset, const unsigned encoding);
static inline int16_t tvb_get_gint16(tvbuff_t *tvb, const int offset, const unsigned encoding) { return tvb_get_int16(tvb, offset, encoding); }
uint32_t tvb_get_uint24(tvbuff_t *tvb, const int offset, const unsigned encoding);
static inline uint32_t tvb_get_guint24(tvbuff_t *tvb, const int offset, const unsigned encoding) { return tvb_get_uint24(tvb, offset, encoding); }
int32_t tvb_get_int24(tvbuff_t *tvb, const int offset, const unsigned encoding);
static inline int32_t tvb_get_gint24(tvbuff_t *tvb, const int offset, const unsigned encoding) { return tvb_get_int24(tvb, offset, encoding); }
uint32_t tvb_get_uint32(tvbuff_t *tvb, const int offset, const unsigned encoding);
static inline uint32_t tvb_get_guint32(tvbuff_t *tvb, const int offset, const unsigned encoding) { return tvb_get_uint32(tvb, offset, encoding); }
int32_t tvb_get_int32(tvbuff_t *tvb, const int offset, const unsigned encoding);
static inline int32_t tvb_get_gint32(tvbuff_t *tvb, const int offset, const unsigned encoding) { return tvb_get_int32(tvb, offset, encoding); }
uint64_t tvb_get_uint40(tvbuff_t *tvb, const int offset, const unsigned encoding);
static inline uint64_t tvb_get_guint40(tvbuff_t *tvb, const int offset, const unsigned encoding) { return tvb_get_uint40(tvb, offset, encoding); }
int64_t tvb_get_int40(tvbuff_t *tvb, const int offset, const unsigned encoding);
static inline int64_t tvb_get_gint40(tvbuff_t *tvb, const int offset, const unsigned encoding) { return tvb_get_int40(tvb, offset, encoding); }
uint64_t tvb_get_uint48(tvbuff_t *tvb, const int offset, const unsigned encoding);
static inline uint64_t tvb_get_guint48(tvbuff_t *tvb, const int offset, const unsigned encoding) { return tvb_get_uint48(tvb, offset, encoding); }
int64_t tvb_get_int48(tvbuff_t *tvb, const int offset, const unsigned encoding);
static inline int64_t tvb_get_gint48(tvbuff_t *tvb, const int offset, const unsigned encoding) { return tvb_get_int48(tvb, offset, encoding); }
uint64_t tvb_get_uint56(tvbuff_t *tvb, const int offset, const unsigned encoding);
static inline uint64_t tvb_get_guint56(tvbuff_t *tvb, const int offset, const unsigned encoding) { return tvb_get_uint56(tvb, offset, encoding); }
int64_t tvb_get_int56(tvbuff_t *tvb, const int offset, const unsigned encoding);
static inline int64_t tvb_get_gint56(tvbuff_t *tvb, const int offset, const unsigned encoding) { return tvb_get_int56(tvb, offset, encoding); }
uint64_t tvb_get_uint64(tvbuff_t *tvb, const int offset, const unsigned encoding);
static inline uint64_t tvb_get_guint64(tvbuff_t *tvb, const int offset, const unsigned encoding) {return tvb_get_uint64(tvb, offset, encoding); }
int64_t tvb_get_int64(tvbuff_t *tvb, const int offset, const unsigned encoding);
static inline int64_t tvb_get_gint64(tvbuff_t *tvb, const int offset, const unsigned encoding) { return tvb_get_int64(tvb, offset, encoding); }
float tvb_get_ieee_float(tvbuff_t *tvb, const int offset, const unsigned encoding);
double tvb_get_ieee_double(tvbuff_t *tvb, const int offset, const unsigned encoding);

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
#define tvb_get_h_uint16  tvb_get_letohs
#define tvb_get_h_uint32  tvb_get_letohl
#elif G_BYTE_ORDER == G_BIG_ENDIAN
#define tvb_get_h_uint16  tvb_get_ntohs
#define tvb_get_h_uint32  tvb_get_ntohl
#else
#error "Unsupported byte order"
#endif
nstime_t* tvb_get_string_time(tvbuff_t *tvb, const int offset, const int length,
                              const unsigned encoding, nstime_t* ns, int *endoff);
GByteArray* tvb_get_string_bytes(tvbuff_t *tvb, const int offset, const int length,
                                 const unsigned encoding, GByteArray* bytes, int *endoff);

uint32_t tvb_get_ipv4(tvbuff_t *tvb, const int offset);

void tvb_get_ipv6(tvbuff_t *tvb, const int offset, ws_in6_addr *addr);

extern int tvb_get_ipv4_addr_with_prefix_len(tvbuff_t *tvb, int offset,
    ws_in4_addr *addr, uint32_t prefix_len);

extern int tvb_get_ipv6_addr_with_prefix_len(tvbuff_t *tvb, int offset,
    ws_in6_addr *addr, uint32_t prefix_len);

void tvb_get_ntohguid(tvbuff_t *tvb, const int offset, e_guid_t *guid);

void tvb_get_letohguid(tvbuff_t *tvb, const int offset, e_guid_t *guid);
void tvb_get_guid(tvbuff_t *tvb, const int offset, e_guid_t *guid, const unsigned encoding);
uint8_t* tvb_get_bits_array(wmem_allocator_t *scope, tvbuff_t *tvb,
    const int offset, size_t length, size_t *data_length, const unsigned encoding);

uint8_t tvb_get_bits8(tvbuff_t *tvb, unsigned bit_offset, const int no_of_bits);

uint16_t tvb_get_bits16(tvbuff_t *tvb, unsigned bit_offset,
    const int no_of_bits, const unsigned encoding);

uint32_t tvb_get_bits32(tvbuff_t *tvb, unsigned bit_offset,
    const int no_of_bits, const unsigned encoding);
uint64_t tvb_get_bits64(tvbuff_t *tvb, unsigned bit_offset,
    const int no_of_bits, const unsigned encoding);
uint32_t tvb_get_bits(tvbuff_t *tvb, const unsigned bit_offset,
    const int no_of_bits, const unsigned encoding);

void *tvb_memcpy(tvbuff_t *tvb, void *target, const int offset, size_t length);

void *tvb_memdup(wmem_allocator_t *scope, tvbuff_t *tvb, const int offset, size_t length);

const uint8_t *tvb_get_ptr(tvbuff_t *tvb, const int offset, const int length);

int tvb_find_uint8(tvbuff_t *tvb, const int offset, const int maxlength, const uint8_t needle);
static inline int tvb_find_guint8(tvbuff_t* tvb, const int offset,
	const int maxlength, const uint8_t needle) { return tvb_find_uint8(tvb, offset, maxlength, needle); }
int tvb_find_uint16(tvbuff_t *tvb, const int offset, const int maxlength, const uint16_t needle);
static inline int tvb_find_guint16(tvbuff_t* tvb, const int offset,
	const int maxlength, const uint16_t needle) {
	return tvb_find_uint16(tvb, offset, maxlength, needle);
}
int tvb_ws_mempbrk_pattern_uint8(tvbuff_t *tvb, const int offset,
    const int maxlength, const ws_mempbrk_pattern* pattern, unsigned char *found_needle);

static inline int tvb_ws_mempbrk_pattern_guint8(tvbuff_t* tvb, const int offset,
	const int maxlength, const ws_mempbrk_pattern* pattern, unsigned char* found_needle) {
	return tvb_ws_mempbrk_pattern_uint8(tvb, offset, maxlength, pattern, found_needle);
}

unsigned tvb_strsize(tvbuff_t *tvb, const int offset);
unsigned tvb_unicode_strsize(tvbuff_t *tvb, const int offset);
int tvb_strnlen(tvbuff_t *tvb, const int offset, const unsigned maxlength);
char *tvb_format_text(wmem_allocator_t *scope, tvbuff_t *tvb, const int offset, const int size);
char *tvb_format_text_wsp(wmem_allocator_t* allocator, tvbuff_t *tvb, const int offset, const int size);
extern char *tvb_format_stringzpad(wmem_allocator_t *scope, tvbuff_t *tvb, const int offset, const int size);

extern char *tvb_format_stringzpad_wsp(wmem_allocator_t* allocator, tvbuff_t *tvb, const int offset, const int size);
uint8_t *tvb_get_string_enc(wmem_allocator_t *scope,
    tvbuff_t *tvb, const int offset, const int length, const unsigned encoding);
char *tvb_get_ts_23_038_7bits_string_packed(wmem_allocator_t *scope,
    tvbuff_t *tvb, const int bit_offset, int no_of_chars);
char *tvb_get_ts_23_038_7bits_string_unpacked(wmem_allocator_t *scope,
    tvbuff_t *tvb, const int offset, int length);
char *tvb_get_etsi_ts_102_221_annex_a_string(wmem_allocator_t *scope,
    tvbuff_t *tvb, const int offset, int length);
char *tvb_get_ascii_7bits_string(wmem_allocator_t *scope,
    tvbuff_t *tvb, const int bit_offset, int no_of_chars);
uint8_t *tvb_get_stringzpad(wmem_allocator_t *scope,
    tvbuff_t *tvb, const int offset, const int length, const unsigned encoding);

uint8_t *tvb_get_stringz_enc(wmem_allocator_t *scope,
    tvbuff_t *tvb, const int offset, int *lengthp, const unsigned encoding);
const uint8_t *tvb_get_const_stringz(tvbuff_t *tvb,
    const int offset, int *lengthp);
int tvb_get_raw_bytes_as_stringz(tvbuff_t *tvb, const int offset,
    const unsigned bufsize, uint8_t *buffer);
int tvb_get_raw_bytes_as_string(tvbuff_t *tvb, const int offset, char *buffer, size_t bufsize);
bool tvb_ascii_isprint(tvbuff_t *tvb, const int offset, const int length);
bool tvb_utf_8_isprint(tvbuff_t *tvb, const int offset, const int length);
bool tvb_ascii_isdigit(tvbuff_t *tvb, const int offset, const int length);
int tvb_find_line_end(tvbuff_t *tvb, const int offset, int len, int *next_offset, const bool desegment);
int tvb_find_line_end_unquoted(tvbuff_t *tvb, const int offset, int len, int *next_offset);
int tvb_skip_wsp(tvbuff_t *tvb, const int offset, const int maxlength);
int tvb_skip_wsp_return(tvbuff_t *tvb, const int offset);
int tvb_skip_uint8(tvbuff_t *tvb, int offset, const int maxlength, const uint8_t ch);
static inline int tvb_skip_guint8(tvbuff_t *tvb, int offset, const int maxlength, const uint8_t ch) {
	return tvb_skip_uint8(tvb, offset, maxlength, ch);
}
int tvb_get_token_len(tvbuff_t *tvb, const int offset, int len, int *next_offset, const bool desegment);
int tvb_strneql(tvbuff_t *tvb, const int offset, const char *str, const size_t size);
int tvb_strncaseeql(tvbuff_t *tvb, const int offset, const char *str, const size_t size);
int tvb_memeql(tvbuff_t *tvb, const int offset, const uint8_t *str, size_t size);
char *tvb_bytes_to_str_punct(wmem_allocator_t *scope, tvbuff_t *tvb, const int offset, const int len, const char punct);
char *tvb_bytes_to_str(wmem_allocator_t *allocator, tvbuff_t *tvb, const int offset, const int len);
typedef struct dgt_set_t
{
    const unsigned char out[16];
}
dgt_set_t;

const char *tvb_bcd_dig_to_str(wmem_allocator_t *scope,
    tvbuff_t *tvb, const int offset, const int len, const dgt_set_t *dgt,
    bool skip_first);
const char *tvb_bcd_dig_to_str_be(wmem_allocator_t *scope,
    tvbuff_t *tvb, const int offset, const int len, const dgt_set_t *dgt,
    bool skip_first);
char *tvb_get_bcd_string(wmem_allocator_t *scope, tvbuff_t *tvb,
    const int offset, int len, const dgt_set_t *dgt,
    bool skip_first, bool odd, bool bigendian);

int tvb_find_tvb(tvbuff_t *haystack_tvb, tvbuff_t *needle_tvb, const int haystack_offset);

tvbuff_t *tvb_uncompress(tvbuff_t *tvb, const int offset, int comprlen);

tvbuff_t *tvb_uncompress_zlib(tvbuff_t *tvb, const int offset, int comprlen);
tvbuff_t *tvb_child_uncompress(tvbuff_t *parent, tvbuff_t *tvb, const int offset, int comprlen);
tvbuff_t *tvb_child_uncompress_zlib(tvbuff_t *parent, tvbuff_t *tvb, const int offset, int comprlen);
tvbuff_t *tvb_uncompress_lznt1(tvbuff_t *tvb, const int offset, int comprlen);
tvbuff_t *tvb_child_uncompress_lznt1(tvbuff_t *parent, tvbuff_t *tvb, const int offset, int comprlen);
tvbuff_t *tvb_uncompress_zstd(tvbuff_t *tvb, const int offset, int comprlen);
tvbuff_t *tvb_child_uncompress_zstd(tvbuff_t *parent, tvbuff_t *tvb, const int offset, int comprlen);
extern tvbuff_t* base64_to_tvb(tvbuff_t *parent, const char *base64);
extern tvbuff_t* base64_tvb_to_new_tvb(tvbuff_t* parent, int offset, int length);
extern tvbuff_t* base64uri_tvb_to_new_tvb(tvbuff_t* parent, int offset, int length);
wmem_strbuf_t* tvb_get_hpack_huffman_strbuf(wmem_allocator_t *scope, tvbuff_t *tvb, const int offset, const int len);
tvbuff_t* tvb_child_uncompress_hpack_huff(tvbuff_t *parent, int offset, int length);
unsigned tvb_get_varint(tvbuff_t *tvb, unsigned offset, unsigned maxlen, uint64_t *value, const unsigned encoding);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __TVBUFF_H__ */

