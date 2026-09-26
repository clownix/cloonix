/** @file
 *
 * Handle command-line arguments common to various programs
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __CLOPTS_COMMON_H__
#define __CLOPTS_COMMON_H__

#include <wireshark.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*
 * Long options.
 * For long options with no corresponding short options, we define values
 * outside the range of ASCII graphic characters, make that the last
 * component of the entry for the long option, and have a case for that
 * option in the switch statement.
 */
// Base value for capture related long options
#define LONGOPT_BASE_CAPTURE        1000
// Base value for dissector related long options
#define LONGOPT_BASE_DISSECTOR      2000
// Base value for application specific long options
#define LONGOPT_BASE_APPLICATION    3000
// Base value for GUI specific long options
#define LONGOPT_BASE_GUI            4000

#define LONGOPT_READ_CAPTURE_COMMON \
    {"read-file", ws_required_argument, NULL, 'r' }, \

#define OPTSTRING_READ_CAPTURE_COMMON \
    "r:"

 int
get_natural_int(const char *string, const char *name);

 int
get_positive_int(const char *string, const char *name);

 uint32_t
get_uint32(const char *string, const char *name);

static inline uint32_t
get_guint32(const char *string, const char *name) { return get_uint32(string, name); }

 uint32_t
get_nonzero_uint32(const char *string, const char *name);

static inline uint32_t
get_nonzero_guint32(const char *string, const char *name) { return get_nonzero_uint32(string, name); }

 uint64_t
get_uint64(const char *string, const char *name);

 uint64_t
get_nonzero_uint64(const char *string, const char *name);

 double
get_positive_double(const char *string, const char *name);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __CLOPTS_COMMON_H__ */
