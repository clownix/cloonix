/* unicode-utils.h
 * Unicode utility definitions
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 2006 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __UNICODEUTIL_H__
#define __UNICODEUTIL_H__

#include <wireshark.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef WS_DEBUG_UTF_8
#define DEBUG_UTF_8_ENABLED true
#else
#define DEBUG_UTF_8_ENABLED false
#endif

extern const int ws_utf8_seqlen[256];

/** Given the first byte in an UTF-8 encoded code point,
 * return the length of the multibyte sequence, or *ZERO*
 * if the byte is invalid as the first byte in a multibyte
 * sequence.
 */
#define ws_utf8_char_len(ch)  (ws_utf8_seqlen[(ch)])

/*
 * Given a wmem scope, a pointer, and a length, treat the string of bytes
 * referred to by the pointer and length as a UTF-8 string, and return a
 * pointer to a UTF-8 string, allocated using the wmem scope, with all
 * ill-formed sequences replaced with the Unicode REPLACEMENT CHARACTER
 * according to the recommended "best practices" given in the Unicode
 * Standard and specified by W3C/WHATWG.
 */
 uint8_t *
ws_utf8_make_valid(wmem_allocator_t *scope, const uint8_t *ptr, ssize_t length);

/*
 * Same as ws_utf8_make_valid() but returns a wmem_strbuf_t.
 */
 wmem_strbuf_t *
ws_utf8_make_valid_strbuf(wmem_allocator_t *scope, const uint8_t *ptr, ssize_t length);

#define IS_LEAD_SURROGATE(uchar2) \
    ((uchar2) >= 0xd800 && (uchar2) < 0xdc00)
#define IS_TRAIL_SURROGATE(uchar2) \
    ((uchar2) >= 0xdc00 && (uchar2) < 0xe000)
#define SURROGATE_VALUE(lead, trail) \
    (((((lead) - 0xd800) << 10) | ((trail) - 0xdc00)) + 0x10000)

#ifdef	__cplusplus
}
#endif

#endif /* __UNICODEUTIL_H__ */
