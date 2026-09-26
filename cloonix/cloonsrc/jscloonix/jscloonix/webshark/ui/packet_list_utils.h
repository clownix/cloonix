/** @file
 * Packet list utility stubs for webshark (no Qt GUI).
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef __PACKET_LIST_UTILS_H__
#define __PACKET_LIST_UTILS_H__

#include <stdbool.h>
#include "epan/frame_data.h"
#include "epan/proto.h"

static inline void packet_list_clear(void) {}
static inline void packet_list_recolor_packets(void) {}
static inline void packet_list_freeze(void) {}
static inline void packet_list_thaw(void) {}
static inline void packet_list_recreate_visible_rows(void) {}
static inline void packet_list_queue_draw(void) {}
static inline bool packet_list_select_row_from_data(frame_data *fdata) {
    (void)fdata;
    return false;
}
static inline void packet_list_select_finfo(field_info *finfo) {
    (void)finfo;
}
static inline bool packet_list_multi_select_active(void) { return false; }
static inline int  packet_list_append(void *col_data, frame_data *fdata) {
    (void)col_data; (void)fdata; return 0;
}
#endif /* __PACKET_LIST_UTILS_H__ */
