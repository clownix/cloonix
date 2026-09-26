/** @file
 *
 * JSON parsing functions.
 *
 * Copyright 2016, Dario Lombardo
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __WSJSON_H__
#define __WSJSON_H__


#include <inttypes.h>
#include <stdbool.h>

#include "jsmn.h"

#ifdef __cplusplus
extern "C" {
#endif

bool json_validate(const uint8_t *buf, const size_t len);
int json_parse(const char *buf, jsmntok_t *tokens, unsigned int max_tokens);
jsmntok_t *json_get_object(const char *buf, jsmntok_t *parent, const char *name);
jsmntok_t *json_get_array(const char *buf, jsmntok_t *parent, const char *name);
int json_get_array_len(jsmntok_t *array);
jsmntok_t *json_get_array_index(jsmntok_t *parent, int idx);
char *json_get_string(char *buf, jsmntok_t *parent, const char *name);
bool json_get_double(char *buf, jsmntok_t *parent, const char *name, double *val);
bool json_get_boolean(char *buf, jsmntok_t *parent, const char *name, bool *val);
bool json_decode_string_inplace(char *text);

#ifdef __cplusplus
}
#endif

#endif
