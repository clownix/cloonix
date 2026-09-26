/* color_filters.c
 * Routines for color filters
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
/*
 * Updated 1 Dec 10 jjm
 */

#include <config.h>
#define WS_LOG_DOMAIN LOG_DOMAIN_EPAN

#include <glib.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <wsutil/file_util.h>
#include <wsutil/report_message.h>
#include <wsutil/ws_assert.h>

#include <epan/packet.h>
#include "color_filters.h"
#include "file.h"
#include <epan/dfilter/dfilter.h>
#include <epan/prefs.h>
#include <epan/epan_dissect.h>

#define RED_COMPONENT(x)   (uint16_t) (((((x) >> 16) & 0xff) * 65535 / 255))
#define GREEN_COMPONENT(x) (uint16_t) (((((x) >>  8) & 0xff) * 65535 / 255))
#define BLUE_COMPONENT(x)  (uint16_t) ( (((x)        & 0xff) * 65535 / 255))


static GSList *color_filter_list;

static GSList *color_filter_deleted_list;
static GSList *color_filter_valid_list;

static bool filters_enabled = true;

static bool tmp_colors_set;

color_filter_t *
color_filter_new(const char *name,
                 const char *filter_string,
                 color_t     *bg_color,
                 color_t     *fg_color,
                 bool         disabled)
{
    color_filter_t *colorf;

    colorf                      = g_new0(color_filter_t, 1);
    colorf->filter_name         = g_strdup(name);
    colorf->filter_text         = g_strdup(filter_string);
    colorf->bg_color            = *bg_color;
    colorf->fg_color            = *fg_color;
    colorf->disabled            = disabled;
    return colorf;
}




static void color_filters_add_tmp(GSList **cfl)
{
    char           *name = NULL;
    uint32_t        i;
    char**         bg_colors;
    char**         fg_colors;
    unsigned long   cval;
    color_t         bg_color, fg_color;
    color_filter_t *colorf;
    ws_assert(strlen(prefs.gui_colorized_fg)==69);
    ws_assert(strlen(prefs.gui_colorized_bg)==69);
    fg_colors = g_strsplit(prefs.gui_colorized_fg, ",", -1);
    bg_colors = g_strsplit(prefs.gui_colorized_bg, ",", -1);
    for ( i=1 ; i<=10 ; i++ ) {
        name = ws_strdup_printf("%s%02d",CONVERSATION_COLOR_PREFIX,i);
        cval = strtoul(fg_colors[i-1], NULL, 16);
        fg_color.red = RED_COMPONENT(cval);
        fg_color.green = GREEN_COMPONENT(cval);
        fg_color.blue = BLUE_COMPONENT(cval);
        cval = strtoul(bg_colors[i-1], NULL, 16);
        bg_color.red = RED_COMPONENT(cval);
        bg_color.green = GREEN_COMPONENT(cval);
        bg_color.blue = BLUE_COMPONENT(cval);
        colorf = color_filter_new(name, NULL, &bg_color, &fg_color, true);
        colorf->filter_text = g_strdup("frame");
        *cfl = g_slist_append(*cfl, colorf);
        g_free(name);
    }
    g_strfreev(fg_colors);
    g_strfreev(bg_colors);
}

static int color_filters_find_by_name_cb(const void *arg1, const void *arg2)
{
    const color_filter_t *colorf = (const color_filter_t *)arg1;
    const char           *name   = (const char *)arg2;
    return strcmp(colorf->filter_name, name);
}


void color_filter_delete(color_filter_t *colorf)
{
    g_free(colorf->filter_name);
    g_free(colorf->filter_text);
    dfilter_free(colorf->c_colorfilter);
    g_free(colorf);
}

static void color_filter_delete_cb(void *filter_arg)
{
    color_filter_t *colorf = (color_filter_t *)filter_arg;
    color_filter_delete(colorf);
}

void color_filter_list_delete(GSList **cfl)
{
    g_slist_free_full(*cfl, color_filter_delete_cb);
    *cfl = NULL;
}

static color_filter_t * color_filter_clone(color_filter_t *colorf)
{
    color_filter_t *new_colorf;
    new_colorf                      = g_new(color_filter_t, 1);
    new_colorf->filter_name         = g_strdup(colorf->filter_name);
    new_colorf->filter_text         = g_strdup(colorf->filter_text);
    new_colorf->bg_color            = colorf->bg_color;
    new_colorf->fg_color            = colorf->fg_color;
    new_colorf->disabled            = colorf->disabled;
    new_colorf->c_colorfilter       = NULL;
    return new_colorf;
}

static void color_filter_list_clone_cb(void *filter_arg, void *cfl_arg)
{
    GSList **cfl = (GSList **)cfl_arg;
    color_filter_t *new_colorf;
    new_colorf = color_filter_clone((color_filter_t *)filter_arg);
    *cfl = g_slist_append(*cfl, new_colorf);
}

static GSList * color_filter_list_clone(GSList *cfl)
{
    GSList *new_list = NULL;

    g_slist_foreach(cfl, color_filter_list_clone_cb, &new_list);

    return new_list;
}

void cloonix_add_filter_exp(GSList **cfl, char *name, char *filter_exp,
                            uint16_t bg_r, uint16_t bg_g, uint16_t bg_b,
                            uint16_t fg_r, uint16_t fg_g, uint16_t fg_b)
{
  color_t bg_color, fg_color;
  color_filter_t *colorf;
  dfilter_t *temp_dfilter = NULL;
  df_error_t *df_err = NULL;
  dfilter_compile(filter_exp, &temp_dfilter, &df_err);
  fg_color.red   = fg_r;
  fg_color.green = fg_g;
  fg_color.blue  = fg_b;
  bg_color.red   = bg_r;
  bg_color.green = bg_g;
  bg_color.blue  = bg_b;
  colorf = color_filter_new(name, filter_exp, &bg_color, &fg_color, 0);
  colorf->c_colorfilter = temp_dfilter;
  *cfl = g_slist_append(*cfl, colorf);
}


static bool color_filters_get(char** err_msg, color_filter_add_cb_func add_cb)
{
  color_filters_add_tmp(&color_filter_list);
  cloonix_add_filter_exp(&color_filter_list, "ICMP errors", "icmp.type in { 3..5, 11 }",
                                             4718, 10030, 11796, 47031, 63479, 29812);
  cloonix_add_filter_exp(&color_filter_list, "ARP", "arp",
                                             64250, 61680, 55255, 4718, 10030, 11796);
  cloonix_add_filter_exp(&color_filter_list, "ICMP", "icmp",
                                             64764, 57568, 65535, 4718, 10030, 11796);
  cloonix_add_filter_exp(&color_filter_list, "TCP RST", "tcp.flags.reset eq 1",
                                             42148, 0, 0, 65535, 64764, 40092);
  cloonix_add_filter_exp(&color_filter_list, "TCP RST", "tcp.flags.reset eq 1",
                                             42148, 0, 0, 65535, 64764, 40092);
  cloonix_add_filter_exp(&color_filter_list, "IPv4 TTL low or unexpected",
  "(ip.dst != 224.0.0.0/4 && ip.ttl < 5) || (ip.dst == 224.0.0.0/24 && ip.dst != 224.0.0.251 && ip.ttl != 1)",
                                             42148, 0, 0, 60652, 61680, 60395);
  cloonix_add_filter_exp(&color_filter_list, "Checksum Errors",
  "eth.fcs.status==\"Bad\" || ip.checksum.status==\"Bad\" || tcp.checksum.status==\"Bad\" || udp.checksum.status==\"Bad\"",
                                             4718, 10030, 11796, 63479, 34695, 34695);
  cloonix_add_filter_exp(&color_filter_list, "TCP SYN/FIN", "tcp.flags & 0x02 || tcp.flags.fin == 1",
                                             41026, 41026, 41026, 4718, 10030, 11796);
  cloonix_add_filter_exp(&color_filter_list, "TCP", "tcp", 59345,58980,65535,4718,10030,11796);
  cloonix_add_filter_exp(&color_filter_list, "UDP", "udp", 56026,61166,65535,4718,10030,11796);
  cloonix_add_filter_exp(&color_filter_list, "Broadcast", "eth[0] & 1",65535,65535,65535,47802,48573,46774);
  return true;
}

bool color_filters_init(char** err_msg, color_filter_add_cb_func add_cb)
{
    color_filter_list_delete(&color_filter_list);
    return color_filters_get(err_msg, add_cb);
}

bool color_filters_reload(char** err_msg, color_filter_add_cb_func add_cb)
{
    color_filter_deleted_list = g_slist_concat(color_filter_deleted_list, color_filter_list);
    color_filter_list = NULL;
    return color_filters_get(err_msg, add_cb);
}

void color_filters_cleanup(void)
{
    color_filter_list_delete(&color_filter_deleted_list);
}

typedef struct _color_clone
{
    void *user_data;
    color_filter_add_cb_func add_cb;
} color_clone_t;

static void color_filters_clone_cb(void *filter_arg, void *user_data)
{
    color_clone_t* clone_data = (color_clone_t*)user_data;
    color_filter_t * new_colorf = color_filter_clone((color_filter_t *)filter_arg);

    clone_data->add_cb (new_colorf, clone_data->user_data);
}

void color_filters_clone(void *user_data, color_filter_add_cb_func add_cb)
{
    color_clone_t clone_data;

    clone_data.user_data = user_data;
    clone_data.add_cb = add_cb;
    g_slist_foreach(color_filter_list, color_filters_clone_cb, &clone_data);
}


static void color_filter_compile_cb(void *filter_arg, void *err)
{
    color_filter_t *colorf = (color_filter_t *)filter_arg;
    char **err_msg = (char**)err;
    df_error_t *df_err = NULL;
    ws_assert(colorf->c_colorfilter == NULL);
    if (colorf->disabled) return;
    if (!dfilter_compile(colorf->filter_text, &colorf->c_colorfilter, &df_err)) {
        *err_msg = ws_strdup_printf("Could not compile color filter name: \"%s\" text: \"%s\".\n%s",
                      colorf->filter_name, colorf->filter_text, df_err->msg);
        df_error_free(&df_err);
    }
}

static void color_filter_validate_cb(void *filter_arg, void *err)
{
    color_filter_t *colorf = (color_filter_t *)filter_arg;
    char **err_msg = (char**)err;
    df_error_t *df_err = NULL;
    ws_assert(colorf->c_colorfilter == NULL);
    if (colorf->disabled) return;
    if (!dfilter_compile(colorf->filter_text, &colorf->c_colorfilter, &df_err)) {
        *err_msg = ws_strdup_printf("Disabling color filter name: \"%s\" filter: \"%s\".\n%s",
                      colorf->filter_name, colorf->filter_text, df_err->msg);
        df_error_free(&df_err);
        colorf->disabled = true;
    }
}

bool color_filters_apply(GSList *tmp_cfl, GSList *edit_cfl, char** err_msg)
{
    bool ret = true;
    *err_msg = NULL;
    color_filter_deleted_list = g_slist_concat(color_filter_deleted_list, color_filter_list);
    color_filter_list = NULL;
    color_filter_list_delete(&color_filter_valid_list);
    color_filter_valid_list = color_filter_list_clone(tmp_cfl);
    color_filter_valid_list = g_slist_concat(color_filter_valid_list,
                                             color_filter_list_clone(edit_cfl) );

    g_slist_foreach(color_filter_valid_list, color_filter_validate_cb, err_msg);
    if (*err_msg != NULL) {
        ret = false;
    }
    color_filter_list = color_filter_list_clone(color_filter_valid_list);
    g_slist_foreach(color_filter_list, color_filter_compile_cb, err_msg);
    if (*err_msg != NULL) {
        ret = false;
    }
    return ret;
}

bool color_filters_used(void)
{
    return color_filter_list != NULL && filters_enabled;
}

static void
prime_edt(void *data, void *user_data)
{
    color_filter_t *colorf = (color_filter_t *)data;
    epan_dissect_t *edt    = (epan_dissect_t *)user_data;

    if (colorf->c_colorfilter != NULL)
        epan_dissect_prime_with_dfilter(edt, colorf->c_colorfilter);
}

/* Prime the epan_dissect_t with all the compiler
 * color filters in 'color_filter_list'. */
void
color_filters_prime_edt(epan_dissect_t *edt)
{
    if (color_filters_used())
        g_slist_foreach(color_filter_list, prime_edt, edt);
}



static int
find_proto(const void *data, const void *user_data)
{
    color_filter_t *colorf = (color_filter_t *)data;
    int proto_id = GPOINTER_TO_INT(user_data);

    if ((!colorf->disabled) && colorf->c_colorfilter != NULL) {
        if (dfilter_interested_in_proto(colorf->c_colorfilter, proto_id)) {
            return 0;
        }
    }
    return -1;
}

bool
color_filters_use_proto(int proto_id)
{
    GSList *item = NULL;
    if (color_filters_used())
        item = g_slist_find_custom(color_filter_list, GINT_TO_POINTER(proto_id), find_proto);
    return (item != NULL);
}

/* * Return the color_t for later use */
const color_filter_t *
color_filters_colorize_packet(epan_dissect_t *edt)
{
    GSList         *curr;
    color_filter_t *colorf;

    /* If we have color filters, "search" for the matching one. */
    if ((edt->tree != NULL) && (color_filters_used())) {
        curr = color_filter_list;

        while(curr != NULL) {
            colorf = (color_filter_t *)curr->data;
            if ( (!colorf->disabled) &&
                 (colorf->c_colorfilter != NULL) &&
                 dfilter_apply_edt(colorf->c_colorfilter, edt)) {
                return colorf;
            }
            curr = g_slist_next(curr);
        }
    }

    return NULL;
}

static int
read_filters_file(const char *path, FILE *f, void *user_data, color_filter_add_cb_func add_cb)
{
#define INIT_BUF_SIZE 128
    char     *name;
    char     *filter_exp;
    uint32_t  name_len         = INIT_BUF_SIZE;
    uint32_t  filter_exp_len   = INIT_BUF_SIZE;
    uint32_t  i                = 0;
    int       c;
    uint16_t  fg_r, fg_g, fg_b, bg_r, bg_g, bg_b;
    bool      disabled         = false;
    bool      skip_end_of_line = false;
    int       ret = 0;

    name = (char *)g_malloc(name_len + 1);
    filter_exp = (char *)g_malloc(filter_exp_len + 1);

    while (1) {

        if (skip_end_of_line) {
            do {
                c = ws_getc_unlocked(f);
            } while (c != EOF && c != '\n');
            if (c == EOF)
                break;
            disabled = false;
            skip_end_of_line = false;
        }

        while ((c = ws_getc_unlocked(f)) != EOF && g_ascii_isspace(c)) {
            if (c == '\n') {
                continue;
            }
        }

        if (c == EOF)
            break;

        if (c == '!') {
            disabled = true;
            continue;
        }

        /* skip # comments and invalid lines */
        if (c != '@') {
            skip_end_of_line = true;
            continue;
        }

        /* we get the @ delimiter.
         * Format is:
         * @name@filter expression@[background r,g,b][foreground r,g,b]
         */

        /* retrieve name */
        i = 0;
        while (1) {
            c = ws_getc_unlocked(f);
            if (c == EOF || c == '@')
                break;
            if (i >= name_len) {
                /* buffer isn't long enough; double its length.*/
                name_len *= 2;
                name = (char *)g_realloc(name, name_len + 1);
            }
            name[i++] = c;
        }
        name[i] = '\0';

        if (c == EOF) {
            break;
        } else if (i == 0) {
            skip_end_of_line = true;
            continue;
        }

        /* retrieve filter expression */
        i = 0;
        while (1) {
            c = ws_getc_unlocked(f);
            if (c == EOF || c == '@')
                break;
            if (i >= filter_exp_len) {
                /* buffer isn't long enough; double its length.*/
                filter_exp_len *= 2;
                filter_exp = (char *)g_realloc(filter_exp, filter_exp_len + 1);
            }
            filter_exp[i++] = c;
        }
        filter_exp[i] = '\0';

        if (c == EOF) {
            break;
        } else if (i == 0) {
            skip_end_of_line = true;
            continue;
        }

        /* retrieve background and foreground colors */
        if (fscanf(f,"[%hu,%hu,%hu][%hu,%hu,%hu]",
                   &bg_r, &bg_g, &bg_b, &fg_r, &fg_g, &fg_b) == 6) {

            /* we got a complete color filter */

            color_t bg_color, fg_color;
            color_filter_t *colorf;
            dfilter_t *temp_dfilter = NULL;
            df_error_t *df_err = NULL;

            if (!disabled && !dfilter_compile(filter_exp, &temp_dfilter, &df_err)) {
                report_warning("Disabling color filter: Could not compile \"%s\" in colorfilters file \"%s\".\n%s", name, path, df_err->msg);
                df_error_free(&df_err);

                /* skip_end_of_line = true; */
                disabled = true;
            }

            fg_color.red = fg_r;
            fg_color.green = fg_g;
            fg_color.blue = fg_b;

            bg_color.red = bg_r;
            bg_color.green = bg_g;
            bg_color.blue = bg_b;

            colorf = color_filter_new(name, filter_exp, &bg_color,
                                      &fg_color, disabled);
            if(user_data == &color_filter_list) {
                GSList **cfl = (GSList **)user_data;

                /* internal call */
                colorf->c_colorfilter = temp_dfilter;
                *cfl = g_slist_append(*cfl, colorf);
            } else {
                /* external call */
                /* just editing, don't need the compiled filter */
                dfilter_free(temp_dfilter);
                add_cb(colorf, user_data);
            }
        }    /* if sscanf */

        skip_end_of_line = true;
    }

    if (ferror(f))
        ret = errno;

    g_free(name);
    g_free(filter_exp);
    return ret;
}


/* read filters from some other filter file (import) */
bool
color_filters_import(const char *path, void *user_data, char **err_msg, color_filter_add_cb_func add_cb)
{
    FILE     *f;
    int       ret;

    if ((f = ws_fopen(path, "r")) == NULL) {
        *err_msg = ws_strdup_printf("Could not open filter file\n%s\nfor reading: %s.",
                      path, g_strerror(errno));
        return false;
    }

    ret = read_filters_file(path, f, user_data, add_cb);
    if (ret != 0) {
        *err_msg = ws_strdup_printf("Error reading filter file\n\"%s\": %s.",
                                   path, g_strerror(errno));
        fclose(f);
        return false;
    }

    fclose(f);
    return true;
}

struct write_filter_data
{
    FILE     *f;
    bool      only_selected;
};

/* save a single filter */
static void
write_filter(void *filter_arg, void *data_arg)
{
    struct write_filter_data *data = (struct write_filter_data *)data_arg;
    color_filter_t *colorf = (color_filter_t *)filter_arg;
    FILE *f = data->f;

    if ( (!data->only_selected) &&
         (strstr(colorf->filter_name,CONVERSATION_COLOR_PREFIX)==NULL) ) {
        fprintf(f,"%s@%s@%s@[%u,%u,%u][%u,%u,%u]\n",
                colorf->disabled ? "!" : "",
                colorf->filter_name,
                colorf->filter_text,
                colorf->bg_color.red,
                colorf->bg_color.green,
                colorf->bg_color.blue,
                colorf->fg_color.red,
                colorf->fg_color.green,
                colorf->fg_color.blue);
    }
}

/* save filters in a filter file */
static bool
write_filters_file(GSList *cfl, FILE *f, bool only_selected)
{
    struct write_filter_data data;

    data.f = f;
    data.only_selected = only_selected;

    fprintf(f,"# This file was created by wireshark. Edit with care.\n");
    g_slist_foreach(cfl, write_filter, &data);
    return true;
}

