/** @file
 * Types utility definitions
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __TYPE_UTIL_H__
#define __TYPE_UTIL_H__

#include <inttypes.h>

uint64_t        type_util_double_to_uint64(double value);
double          type_util_uint64_to_double(uint64_t value);

#define         double_to_uint64(value)   ((uint64_t)(value))
#define         uint64_to_double(value)   ((double)(value))

#endif /* __TYPE_UTIL_H__ */
