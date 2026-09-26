/* range.h
 * Range routines
 *
 * Dick Gooris <gooris@lucent.com>
 * Ulf Lamping <ulf.lamping@web.de>
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __RANGE_H__
#define __RANGE_H__

#include <epan/wmem_scopes.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/** @file
 * Range strings a variant of value_strings
 */

/**@todo where's the best place for these? */
#define MAX_SCTP_PORT 65535
#define MAX_TCP_PORT 65535
#define MAX_UDP_PORT 65535
#define MAX_DCCP_PORT 65535

typedef struct range_admin_tag {
    uint32_t low;
    uint32_t high;
} range_admin_t;
#define RANGE_ADMIN_T_INITIALIZER { 0, 0 }

/** user specified range(s) */
typedef struct epan_range {
    unsigned        nranges;   /**< number of entries in ranges */
    range_admin_t   ranges[];  /**< flexible array of range entries */
} range_t;

/**
 * Return value from range_convert_str().
 */
typedef enum {
    CVT_NO_ERROR,
    CVT_SYNTAX_ERROR,
    CVT_NUMBER_TOO_BIG
} convert_ret_t;

range_t *range_empty(wmem_allocator_t *scope);

convert_ret_t range_convert_str(wmem_allocator_t *scope, range_t **range, const char *es, uint32_t max_value);
convert_ret_t range_convert_str_work(wmem_allocator_t *scope, range_t **range, const char *es,
    uint32_t max_value, bool err_on_max);

bool value_is_in_range(const range_t *range, uint32_t val);

bool range_add_value(wmem_allocator_t *scope, range_t **range, uint32_t val);

bool range_remove_value(wmem_allocator_t *scope, range_t **range, uint32_t val);

bool ranges_are_equal(const range_t *a, const range_t *b);

void range_foreach(range_t *range, void (*callback)(uint32_t val, void *ptr), void *ptr);

char *range_convert_range(wmem_allocator_t *scope, const range_t *range);

range_t *range_copy(wmem_allocator_t *scope, const range_t *src);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __RANGE_H__ */
