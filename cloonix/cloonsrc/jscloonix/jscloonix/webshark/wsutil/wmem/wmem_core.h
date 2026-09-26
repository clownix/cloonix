/** @file
 * Definitions for the Wireshark Memory Manager Core
 * Copyright 2012, Evan Huus <eapache@gmail.com>
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __WMEM_CORE_H__
#define __WMEM_CORE_H__

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <glib.h>
#include <ws_posix_compat.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct _wmem_allocator_t;
typedef struct _wmem_allocator_t wmem_allocator_t;
typedef enum _wmem_allocator_type_t {
    WMEM_ALLOCATOR_SIMPLE,
    WMEM_ALLOCATOR_BLOCK,
    WMEM_ALLOCATOR_STRICT,
    WMEM_ALLOCATOR_BLOCK_FAST
} wmem_allocator_type_t;

void * wmem_alloc(wmem_allocator_t *allocator, const size_t size)
G_GNUC_MALLOC
G_GNUC_ALLOC_SIZE(2);

#define wmem_new(allocator, type) \
    ((type*)wmem_alloc((allocator), sizeof(type)))

#define wmem_safe_mult_type_size(type, num) \
    ((((num) <= 0) || ((size_t)sizeof(type) > (G_MAXSSIZE / (size_t)(num)))) ? 0 : (sizeof(type) * (num)))

#define wmem_alloc_array(allocator, type, num) \
    ((type*)wmem_alloc((allocator), wmem_safe_mult_type_size(type, (num))))

void * wmem_alloc0(wmem_allocator_t *allocator, const size_t size)
G_GNUC_MALLOC
G_GNUC_ALLOC_SIZE(2);

#define wmem_new0(allocator, type) \
    ((type*)wmem_alloc0((allocator), sizeof(type)))

#define wmem_alloc0_array(allocator, type, num) \
    ((type*)wmem_alloc0((allocator), wmem_safe_mult_type_size(type, (num))))

void wmem_free(wmem_allocator_t *allocator, void *ptr);
void * wmem_realloc(wmem_allocator_t *allocator, void *ptr, const size_t size)
G_GNUC_ALLOC_SIZE(3);
void wmem_free_all(wmem_allocator_t *allocator);
void wmem_gc(wmem_allocator_t *allocator);
void wmem_destroy_allocator(wmem_allocator_t *allocator);
wmem_allocator_t * wmem_allocator_new(const wmem_allocator_type_t type);
void wmem_init(void);
void wmem_cleanup(void);
void wmem_enter_scope(wmem_allocator_t *allocator);
void wmem_leave_scope(wmem_allocator_t *allocator);
bool wmem_in_scope(wmem_allocator_t *allocator);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __WMEM_CORE_H__ */

