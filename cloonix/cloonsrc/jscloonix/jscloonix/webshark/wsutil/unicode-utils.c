/* unicode-utils.c
 * Unicode utility routines
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 2006 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include "unicode-utils.h"

const int ws_utf8_seqlen[256] = {
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,  /* 0x00...0x0f */
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,  /* 0x10...0x1f */
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,  /* 0x20...0x2f */
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,  /* 0x30...0x3f */
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,  /* 0x40...0x4f */
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,  /* 0x50...0x5f */
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,  /* 0x60...0x6f */
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,  /* 0x70...0x7f */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* 0x80...0x8f */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* 0x90...0x9f */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* 0xa0...0xaf */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* 0xb0...0xbf */
    0,0,2,2,2,2,2,2,2,2,2,2,2,2,2,2,  /* 0xc0...0xcf */
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,  /* 0xd0...0xdf */
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,  /* 0xe0...0xef */
    4,4,4,4,4,0,0,0,0,0,0,0,0,0,0,0,  /* 0xf0...0xff */
};

static inline size_t
utf_8_validate(const uint8_t *start, ssize_t length, const uint8_t **end)
{
    const uint8_t *ptr = start;
    uint8_t ch;
    size_t unichar_len, valid_bytes = 0;

    while (length > 0) {

        ch = *ptr;

        if (ch < 0x80) {
            valid_bytes++;
            ptr++;
            length--;
            continue;
        }

        ch = *ptr;

        if (ch < 0xc2 || ch > 0xf4) {
            ptr++;
            length--;
            *end = ptr;
            return valid_bytes;
        }

        if (ch < 0xe0) { /* 110xxxxx, 2 byte char */
            unichar_len = 2;
        } else if (ch < 0xf0) { /* 1110xxxx, 3 byte char */
            unichar_len = 3;
            ptr++;
            length--;
            if (length < 1) {
                *end = ptr;
                return valid_bytes;
            }
            switch (ch) {
                case 0xe0:
                    if (*ptr < 0xa0 || *ptr > 0xbf) {
                        *end = ptr;
                        return valid_bytes;
                    }
                    break;
                case 0xed:
                    if (*ptr < 0x80 || *ptr > 0x9f) {
                        *end = ptr;
                        return valid_bytes;
                    }
                    break;
                default:
                    if (*ptr < 0x80 || *ptr > 0xbf) {
                        *end = ptr;
                        return valid_bytes;
                    }
            }
        } else { /* 11110xxx, 4 byte char - > 0xf4 excluded above */
            unichar_len = 4;
            ptr++;
            length--;
            if (length < 1) {
                *end = ptr;
                return valid_bytes;
            }
            switch (ch) {
                case 0xf0:
                    if (*ptr < 0x90 || *ptr > 0xbf) {
                        *end = ptr;
                        return valid_bytes;
                    }
                    break;
                case 0xf4:
                    if (*ptr < 0x80 || *ptr > 0x8f) {
                        *end = ptr;
                        return valid_bytes;
                    }
                    break;
                default:
                    if (*ptr < 0x80 || *ptr > 0xbf) {
                        *end = ptr;
                        return valid_bytes;
                    }
            }
            ptr++;
            length--;
            if (length < 1) {
                *end = ptr;
                return valid_bytes;
            }
            if (*ptr < 0x80 || *ptr > 0xbf) {
                *end = ptr;
                return valid_bytes;
            }
        }

        ptr++;
        length--;
        if (length < 1) {
            *end = ptr;
            return valid_bytes;
        }
        if (*ptr < 0x80 || *ptr > 0xbf) {
            *end = ptr;
            return valid_bytes;
        } else {
            ptr++;
            length--;
            valid_bytes += unichar_len;
        }

    }
    *end = ptr;
    return valid_bytes;
}

wmem_strbuf_t *
ws_utf8_make_valid_strbuf(wmem_allocator_t *scope, const uint8_t *ptr, ssize_t length)
{
    wmem_strbuf_t *str;

    str = wmem_strbuf_new_sized(scope, length+1);

    /* See the Unicode Standard conformance chapter at
     * https://www.unicode.org/versions/Unicode15.0.0/ch03.pdf especially
     * Table 3-7 "Well-Formed UTF-8 Byte Sequences" and
     * U+FFFD Substitution of Maximal Subparts. */

    while (length > 0) {
        const uint8_t *prev = ptr;
        size_t valid_bytes = utf_8_validate(prev, length, &ptr);

        if (valid_bytes) {
            wmem_strbuf_append_len(str, prev, valid_bytes);
        }
        length -= ptr - prev;
        prev += valid_bytes;
        if (ptr - prev) {
            wmem_strbuf_append_unichar_repl(str);
        }
    }

    return str;
}

uint8_t *
ws_utf8_make_valid(wmem_allocator_t *scope, const uint8_t *ptr, ssize_t length)
{
    wmem_strbuf_t *str = ws_utf8_make_valid_strbuf(scope, ptr, length);
    return wmem_strbuf_finalize(str);
}

