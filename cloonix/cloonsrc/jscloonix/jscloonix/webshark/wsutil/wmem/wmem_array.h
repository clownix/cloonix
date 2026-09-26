/** @file
 * Definitions for the Wireshark Memory Manager Array
 * Copyright 2013, Evan Huus <eapache@gmail.com>
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __WMEM_ARRAY_H__
#define __WMEM_ARRAY_H__

#include <string.h>
#include <glib.h>

#include "wmem_core.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/** @addtogroup wmem
 *  @{
 *    @defgroup wmem-array Array
 *
 *    A resizable array implementation on top of wmem.
 *
 *    @{
 */

struct _wmem_array_t;

typedef struct _wmem_array_t wmem_array_t;


wmem_array_t *
wmem_array_sized_new(wmem_allocator_t *allocator, size_t elem_size,
                     unsigned alloc_count)
G_GNUC_MALLOC;


wmem_array_t *
wmem_array_new(wmem_allocator_t *allocator, const size_t elem_size)
G_GNUC_MALLOC;


void
wmem_array_grow(wmem_array_t *array, const unsigned to_add);


void
wmem_array_set_null_terminator(wmem_array_t *array);


void
wmem_array_bzero(wmem_array_t *array);


void
wmem_array_append(wmem_array_t *array, const void *in, unsigned count);

#define wmem_array_append_one(ARRAY, VAL) \
    wmem_array_append((ARRAY), &(VAL), 1)


void *
wmem_array_index(wmem_array_t *array, unsigned array_index);


int
wmem_array_try_index(wmem_array_t *array, unsigned array_index, void *val);


void
wmem_array_sort(wmem_array_t *array, int (*compar)(const void*,const void*));


void *
wmem_array_get_raw(wmem_array_t *array);


unsigned
wmem_array_get_count(wmem_array_t *array);

/* Truncates the underlying array to the elements contained within
 * (including null terminator if set), frees the wmem_array_t
 * structure, and returns a pointer to the raw array. The wmem_array_t
 * struct cannot be used after this is called. This is for when you are
 * done adding elements to the array but still need the underlying array.
 */

void *
wmem_array_finalize(wmem_array_t *array);


void
wmem_destroy_array(wmem_array_t *array);

/**   @}
 *  @} */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __WMEM_ARRAY_H__ */

/*
 * Editor modelines  -  https://www.wireshark.org/tools/modelines.html
 *
 * Local variables:
 * c-basic-offset: 4
 * tab-width: 8
 * indent-tabs-mode: nil
 * End:
 *
 * vi: set shiftwidth=4 tabstop=8 expandtab:
 * :indentSize=4:tabSize=8:noTabs=true:
 */
