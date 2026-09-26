/** @file
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __WS_ASSERT_H__
#define __WS_ASSERT_H__


#include "config.h"
#include <stdbool.h>
#include <string.h>
#include <wsutil/wmem/wmem.h>

#if defined(ENABLE_ASSERT)
#define WS_ASSERT_ENABLED       1
#elif defined(NDEBUG)
#define WS_ASSERT_ENABLED       0
#else
#define WS_ASSERT_ENABLED       1
#endif

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define ws_assert_if_active(active, expr) \
        do {                                                \
            if ((active) && !(expr))                        \
                ws_error("assertion failed: %s", #expr);    \
        } while (0)

#define ws_abort_if_fail(expr) \
        ws_assert_if_active(true, expr)

#define ws_assert(expr) \
        ws_assert_if_active(WS_ASSERT_ENABLED, expr)


#define ws_assert_streq(s1, s2) \
        ws_assert((s1) && (s2) && strcmp((s1), (s2)) == 0)

#define ws_assert_not_reached() \
        ws_error("assertion \"not reached\" failed")

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __WS_ASSERT_H__ */
