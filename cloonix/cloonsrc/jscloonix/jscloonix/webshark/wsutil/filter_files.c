/* filter_files.c
 * Code for reading and writing the filters file.
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <config.h>
#define WS_LOG_DOMAIN LOG_DOMAIN_WSUTIL
#include "filter_files.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <wsutil/file_util.h>
#include <wsutil/report_message.h>
#include <wsutil/ws_assert.h>

/*
 * Read in a list of filters.
 *
 * On error, report the error via the UI.
 */

#define INIT_BUF_SIZE  128

static GList *
add_filter_entry(GList *fl, const char *filt_name, const char *filt_expr)
{
    filter_def *filt;

    filt         = g_new(filter_def, 1);
    filt->name   = g_strdup(filt_name);
    filt->strval = g_strdup(filt_expr);
    return g_list_prepend(fl, filt);
}

static void
free_filter_entry(void * data)
{
    filter_def *filt = (filter_def*)data;
    g_free(filt->name);
    g_free(filt->strval);
    g_free(filt);
}

void ws_filter_list_free(filter_list_t *fl)
{
    g_list_free_full(fl->list, free_filter_entry);
    g_free(fl);
}

static GList *
remove_filter_entry(GList *fl, GList *fl_entry)
{
    filter_def *filt;

    filt = (filter_def *) fl_entry->data;
    g_free(filt->name);
    g_free(filt->strval);
    g_free(filt);
    return g_list_remove_link(fl, fl_entry);
}

static int
skip_whitespace(FILE *ff)
{
    int c;

    while ((c = getc(ff)) != EOF && c != '\n' && g_ascii_isspace(c))
        ;
    return c;
}

static int
getc_crlf(FILE *ff)
{
    int c;

    c = getc(ff);
    if (c == '\r') {
        /* Treat CR-LF at the end of a line like LF, so that if we're reading
         * a Windows-format file on UN*X, we handle it the same way we'd handle
         * a UN*X-format file. */
        c = getc(ff);
        if (c != EOF && c != '\n') {
            /* Put back the character after the CR, and process the CR normally. */
            ungetc(c, ff);
            c = '\r';
        }
    }
    return c;
}


/*
 * Add a new filter to the end of a list.
 */
void
ws_filter_list_add(filter_list_t *fl, const char *name,
                    const char *expression)
{
    fl->list = add_filter_entry(fl->list, name, expression);
}

static int
compare_def(const void *def, const void *name)
{
    return g_strcmp0(((filter_def *)def)->name, name);
}

GList *ws_filter_list_find(filter_list_t *list, const char *name)
{
    return g_list_find_custom(list->list, name, compare_def);
}

/*
 * Remove a filter from a list.
 */
bool
ws_filter_list_remove(filter_list_t *list, const char *name)
{
    GList      *p;

    p = g_list_find_custom(list->list, name, compare_def);
    if (p == NULL)
        return false;
    list->list = remove_filter_entry(list->list, p);
    return true;
}

