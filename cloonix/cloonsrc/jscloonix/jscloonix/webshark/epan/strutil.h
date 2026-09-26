/* strutil.h
 * String utility definitions
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __STRUTIL_H__
#define __STRUTIL_H__

#include <epan/wmem_scopes.h>
#include <wsutil/str_util.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

const unsigned char *find_line_end(const unsigned char *data, const unsigned char *dataend,
    const unsigned char **eol);

int        get_token_len(const unsigned char *linep, const unsigned char *lineend,
    const unsigned char **next_token);

bool       hex_str_to_bytes(const char *hex_str, GByteArray *bytes,
    bool force_separators);

bool hex_str_to_bytes_encoding(const char *hex_str, GByteArray *bytes, const char **endptr,
                                   const unsigned encoding, const bool fail_if_partial);

bool       uri_to_bytes(const char *uri_str, GByteArray *bytes, size_t len);

bool       uri_str_to_bytes(const char *uri_str, GByteArray *bytes);

bool       rel_oid_str_to_bytes(const char *oid_str, GByteArray *bytes, bool is_absolute);

bool       oid_str_to_bytes(const char *oid_str, GByteArray *bytes);

GByteArray *byte_array_dup(const GByteArray *ba);

bool byte_array_equal(GByteArray *ba1, GByteArray *ba2);

char*     xml_escape(const char *unescaped);

uint8_t * convert_string_to_hex(const char *string, size_t *nbytes);

char * convert_string_case(const char *string, bool case_insensitive);

void IA5_7BIT_decode(unsigned char * dest, const unsigned char* src, int len);

#define FORMAT_LABEL_REPLACE_SPACE      (0x1 << 0)

size_t ws_label_strcpy(char *label_str, size_t bufsize, size_t pos, const uint8_t *str, int flags);

size_t ws_label_strcat(char *label_str, size_t bufsize, const uint8_t *str, int flags);

unsigned char module_check_valid_name(const char *name, bool lower_only);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __STRUTIL_H__ */
