/** @file
 * packet tap interface   2002 Ronnie Sahlberg
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __TAP_H__
#define __TAP_H__

#include <epan/epan.h>
#include <epan/packet_info.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * Status returned by the per-packet callback.
 */
typedef enum {
	TAP_PACKET_DONT_REDRAW,	/**< Packet processing succeeded, no need to redraw */
	TAP_PACKET_REDRAW,	/**< Packet processing succeeded, must redraw */
	TAP_PACKET_FAILED	/**< Packet processing failed, stop calling this tap */
} tap_packet_status;

typedef unsigned tap_flags_t;

typedef void (*tap_reset_cb)(void *tapdata);
typedef tap_packet_status (*tap_packet_cb)(void *tapdata, packet_info *pinfo, epan_dissect_t *edt, const void *data, tap_flags_t flags);
typedef void (*tap_draw_cb)(void *tapdata);
typedef void (*tap_finish_cb)(void *tapdata);

#define TL_REQUIRES_NOTHING         0x00000000	    /**< nothing */
#define TL_REQUIRES_PROTO_TREE      0x00000001	    /**< non-NULL protocol tree */
#define TL_REQUIRES_COLUMNS         0x00000002	    /**< columns */
#define TL_REQUIRES_ERROR_PACKETS   0x00000004	    /**< include packet even if pinfo->flags.in_error_pkt is set */
#define TL_REQUIRES_PROTOCOLS       0x00000020	    /**< don't fake protocols */

#define TL_IS_DISSECTOR_HELPER      0x00000008	    /**< tap helps a dissector do work
						                             ** but does not, itself, require dissection */

#define TL_IGNORE_DISPLAY_FILTER    0x00000010      /**< use packet, even if it would be filtered out */
#define TL_DISPLAY_FILTER_IGNORED   0x00100000      /**< flag for the conversation handler */

#define TL_IP_AGGREGATION_NULL      0x00000100      /**< default analysis, no aggregation at all */
#define TL_IP_AGGREGATION_ORI       0x00000200      /**< replace with subnets when possible, and keep original data */
#define TL_IP_AGGREGATION_RESERVED  0x00000400      /**< reserved */

typedef struct {
	void (*register_tap_listener)(void);   /* routine to call to register tap listener */
} tap_plugin;

void tap_register_plugin(const tap_plugin *plug);

typedef struct _tap_reg {
    const char *cb_name;
    void (*cb_func)(void);
} tap_reg_t;

void register_all_tap_listeners(tap_reg_t const *tap_reg_listeners);

extern void tap_init(void);

int register_tap(const char *name);

GList* get_tap_names(void);

int find_tap_id(const char *name);

void tap_queue_packet(int tap_id, packet_info *pinfo, const void *tap_specific_data);

void tap_build_interesting(epan_dissect_t *edt);

extern void tap_queue_init(epan_dissect_t *edt);

extern void tap_push_tapped_queue(epan_dissect_t *edt);

void reset_tap_listeners(void);

void draw_tap_listeners(bool draw_all);

GString *register_tap_listener(const char *tapname, void *tapdata,
    const char *fstring, unsigned flags, tap_reset_cb tap_reset,
    tap_packet_cb tap_packet, tap_draw_cb tap_draw,
    tap_finish_cb tap_finish) G_GNUC_WARN_UNUSED_RESULT;

GString *set_tap_dfilter(void *tapdata, const char *fstring);

void tap_listeners_dfilter_recompile(void);

void remove_tap_listener(void *tapdata);

GString *set_tap_flags(void *tapdata, unsigned flags);

bool tap_listeners_require_dissection(void);

bool tap_listeners_require_columns(void);

bool have_tap_listener(int tap_id);

bool have_filtering_tap_listeners(void);

void tap_listeners_load_field_references(epan_dissect_t *edt);

unsigned union_of_tap_listener_flags(void);

const void *fetch_tapped_data(int tap_id, int idx);

extern void tap_cleanup(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __TAP_H__ */
