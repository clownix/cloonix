/** @file
 * Flow sequence analysis
 *
 * Copied from gtk/graph_analysis.h
 *
 * Copyright 2004, Verso Technologies Inc.
 * By Alejandro Vaquero <alejandrovaquero@yahoo.com>
 *
 * based on rtp_analysis.c and io_stat
 *
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __EPAN_SEQUENCE_ANALYSIS_H__
#define __EPAN_SEQUENCE_ANALYSIS_H__

#include <glib.h>

#include "packet_info.h"
#include "tap.h"
#include "address.h"
#include "wsutil/file_util.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define MAX_NUM_NODES 40

/** defines info types for graph analysis additional information */
typedef enum _ga_info_type {
    GA_INFO_TYPE_NONE=0,
    GA_INFO_TYPE_RTP
} ga_info_type;

/** defines an entry for the graph analysis */
typedef struct _seq_analysis_item {
    uint32_t frame_number;
    address src_addr;
    uint16_t port_src;
    address dst_addr;
    uint16_t port_dst;
    char *frame_label;              /**< the label on top of the arrow */
    char *time_str;                 /**< timestamp */
    char *comment;                  /**< a comment that appears at the right of the graph */
    uint16_t conv_num;              /**< The conversation number. Used for coloring VoIP calls. */
    unsigned fg_color;              /**< Foreground color, 0xRRGGBB. Qt only. */
    unsigned bg_color;              /**< Background color, 0xRRGGBB. Qt only. */
    bool has_color_filter;          /**< Set if packet has color filter. Qt only. */
    bool display;                   /**< indicate if the packet is displayed or not in the graph */
    unsigned src_node;              /**< this is used by graph_analysis.c to identify the node */
    unsigned dst_node;              /**< a node is an IP address that will be displayed in columns */
    uint16_t line_style;            /**< the arrow line width in pixels*/
    ga_info_type info_type;         /**< type of info for item */
    void *info_ptr;                 /**< ptr to info for item */
} seq_analysis_item_t;

typedef struct _seq_analysis_info {
    const char* name;                  /**< Name of sequence analysis */
    bool        any_addr;              /**< any addr (DL+net) vs net-only */
    int         nconv;                 /**< number of conversations in the list */
    GQueue*     items;                 /**< list of seq_analysis_info_t */
    GHashTable *ht;                    /**< hash table of seq_analysis_info_t */
    address nodes[MAX_NUM_NODES];      /**< horizontal node list */
    uint8_t occurrence[MAX_NUM_NODES]; /**< horizontal occurrence list 0|1 */
    uint32_t num_nodes;                /**< actual number of nodes */
} seq_analysis_info_t;

typedef struct register_analysis register_analysis_t;

void register_seq_analysis(const char* name, const char* ui_name, const int proto_id, const char* tap_listener, unsigned tap_flags, tap_packet_cb tap_func);
const char* sequence_analysis_get_name(register_analysis_t* analysis);
const char* sequence_analysis_get_tap_listener_name(register_analysis_t* analysis);
const char* sequence_analysis_get_ui_name(register_analysis_t* analysis);
tap_packet_cb sequence_analysis_get_packet_func(register_analysis_t* analysis);
unsigned sequence_analysis_get_tap_flags(register_analysis_t* analysis);
seq_analysis_item_t* sequence_analysis_create_sai_with_addresses(packet_info *pinfo, seq_analysis_info_t *sainfo);
void sequence_analysis_use_color_filter(packet_info *pinfo, seq_analysis_item_t *sai);
void sequence_analysis_use_col_info_as_label_comment(packet_info *pinfo, seq_analysis_item_t *sai);
register_analysis_t* sequence_analysis_find_by_name(const char* name);
void sequence_analysis_table_iterate_tables(wmem_foreach_func func, void *user_data);
seq_analysis_info_t *sequence_analysis_info_new(void);
void sequence_analysis_info_free(seq_analysis_info_t * sainfo);
void sequence_analysis_list_sort(seq_analysis_info_t *sainfo);
void sequence_analysis_list_free(seq_analysis_info_t *sainfo);
int sequence_analysis_get_nodes(seq_analysis_info_t *sainfo);
void sequence_analysis_free_nodes(seq_analysis_info_t *sainfo);
void sequence_analysis_dump_to_file(FILE *of, seq_analysis_info_t *sainfo, unsigned first_node);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __EPAN_SEQUENCE_ANALYSIS_H__ */
