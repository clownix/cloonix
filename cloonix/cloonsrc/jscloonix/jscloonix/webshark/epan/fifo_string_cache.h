/* fifo_string_cache.h
 * A string cache, possibly with a bounded size, using FIFO order to control
 * the size.
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef __FIFO_STRING_CACHE_H__
#define __FIFO_STRING_CACHE_H__

#include <stdbool.h>

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct {
    GHashTable          *set;
    GSList              *head;
    GSList              *tail;
    unsigned            max_entries;
} fifo_string_cache_t;

void fifo_string_cache_init(fifo_string_cache_t *fcache, unsigned max_entries, GDestroyNotify string_free_func);
void fifo_string_cache_free(fifo_string_cache_t *fcache);
bool fifo_string_cache_contains(fifo_string_cache_t *fcache, const char *entry);
bool fifo_string_cache_insert(fifo_string_cache_t *fcache, const char *entry);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __FIFO_STRING_CACHE_H__ */
