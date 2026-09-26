/** @file
 * Utilities to convert strings to integers
 *
 * Copyright 2016, Dario Lombardo
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef _WS_STRTOI_H
#define _WS_STRTOI_H

#include <stdbool.h>
#include <inttypes.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

bool ws_strtoi64(const char* str, const char** endptr, int64_t* cint);
bool ws_strtoi32(const char* str, const char** endptr, int32_t* cint);
bool ws_strtoi16(const char* str, const char** endptr, int16_t* cint);
bool ws_strtoi8 (const char* str, const char** endptr, int8_t*  cint);
bool ws_strtoi (const char* str, const char** endptr, int*  cint);

bool ws_strtou64(const char* str, const char** endptr, uint64_t* cint);
bool ws_strtou32(const char* str, const char** endptr, uint32_t* cint);
bool ws_strtou16(const char* str, const char** endptr, uint16_t* cint);
bool ws_strtou8 (const char* str, const char** endptr, uint8_t*  cint);
bool ws_strtou (const char* str, const char** endptr, unsigned*  cint);


bool ws_hexstrtou64(const char* str, const char** endptr, uint64_t* cint);
bool ws_hexstrtou32(const char* str, const char** endptr, uint32_t* cint);
bool ws_hexstrtou16(const char* str, const char** endptr, uint16_t* cint);
bool ws_hexstrtou8 (const char* str, const char** endptr, uint8_t*  cint);
bool ws_hexstrtou (const char* str, const char** endptr, unsigned*  cint);

bool ws_basestrtou64(const char* str, const char** endptr, uint64_t* cint, int base);
bool ws_basestrtou32(const char* str, const char** endptr, uint32_t* cint, int base);
bool ws_basestrtou16(const char* str, const char** endptr, uint16_t* cint, int base);
bool ws_basestrtou8 (const char* str, const char** endptr, uint8_t*  cint, int base);
bool ws_basestrtou (const char* str, const char** endptr, unsigned*  cint, int base);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
