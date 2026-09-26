/* color_filters.h
 * Definitions for color filters
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef  __COLOR_FILTERS_H__
#define  __COLOR_FILTERS_H__

#include <glib.h>

#include <wsutil/color.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct epan_dissect;

#define COLORFILTERS_FILE_NAME          "colorfilters"

#define CONVERSATION_COLOR_PREFIX       "___conversation_color_filter___"
typedef struct _color_filter {
    char      *filter_name;         /* name of the filter */
    char      *filter_text;         /* text of the filter expression */
    color_t    bg_color;            /* background color for packets that match */
    color_t    fg_color;            /* foreground color for packets that match */
    bool       disabled;            /* set if the filter is disabled */
    struct epan_dfilter *c_colorfilter;  /* compiled filter expression */
} color_filter_t;

typedef void (*color_filter_add_cb_func)(color_filter_t *colorf, void *user_data);
bool color_filters_init(char** err_msg, color_filter_add_cb_func add_cb);
bool color_filters_reload(char** err_msg, color_filter_add_cb_func add_cb);
void color_filters_cleanup(void);
bool color_filters_used(void);
void color_filters_prime_edt(struct epan_dissect *edt);

 bool
color_filters_use_proto(int proto_id);

 const color_filter_t *
color_filters_colorize_packet(struct epan_dissect *edt);

 void color_filters_clone(void *user_data, color_filter_add_cb_func add_cb);

 bool color_filters_import(const char *path, void *user_data, char **err_msg, color_filter_add_cb_func add_cb);

 bool color_filters_read_globals(void *user_data, char** err_msg, color_filter_add_cb_func add_cb);

 bool color_filters_apply(GSList *tmp_cfl, GSList *edit_cfl, char** err_msg);

 color_filter_t *color_filter_new(
    const char *name, const char *filter_string,
    color_t *bg_color, color_t *fg_color, bool disabled);

 void color_filter_delete(color_filter_t *colorf);

 void color_filter_list_delete(GSList **cfl);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif

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
