/* ui/recent_utils.c
 * Stub implementations of recent-file menu functions for webshark.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "config.h"
#include <stdio.h>
#include <stdbool.h>
#include "ui/recent_utils.h"

void add_menu_recent_capture_file(const char *cf_name, bool force)
{
    (void)cf_name; (void)force;
}

void menu_recent_file_write_all(FILE *rf)
{
    (void)rf;
}

void cfilter_combo_recent_write_all(FILE *rf)
{
    (void)rf;
}

bool dfilter_combo_add_recent(const char *dftext)
{
    (void)dftext;
    return true;
}

void dfilter_recent_combo_write_all(FILE *rf)
{
    (void)rf;
}
