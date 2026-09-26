/* conversation_filter.h
 * Routines for dissector-generated conversation filters for use as
 * display and color filters
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __DISSECTOR_FILTERS_H__
#define __DISSECTOR_FILTERS_H__

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

extern void conversation_filters_init(void);
typedef bool (*is_filter_valid_func)(struct _packet_info *pinfo, void *user_data);
typedef char* (*build_filter_string_func)(struct _packet_info *pinfo, void *user_data);
void register_conversation_filter(const char *proto_name, const char *display_name,
                           is_filter_valid_func is_filter_valid, build_filter_string_func build_filter_string, void *user_data);

void register_log_conversation_filter(const char *proto_name, const char *display_name,
                                    is_filter_valid_func is_filter_valid, build_filter_string_func build_filter_string, void *user_data);
void add_conversation_filter_protocol(const char *proto_name);
extern void conversation_filters_cleanup(void);
char *conversation_filter_from_packet(struct _packet_info *pinfo);
char *conversation_filter_from_log(struct _packet_info *pinfo);

typedef struct conversation_filter_s {
    const char *              proto_name;
    const char *              display_name;
    is_filter_valid_func      is_filter_valid;
    build_filter_string_func  build_filter_string;
    void *                    user_data;
} conversation_filter_t;

extern GList *packet_conv_filter_list;
extern GList *log_conv_filter_list;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* conversation_filter.h */
