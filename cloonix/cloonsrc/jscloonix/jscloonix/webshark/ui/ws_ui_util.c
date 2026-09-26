/* ui/ws_ui_util.c
 * GUI-independent utility stubs for webshark.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "config.h"
#include <stdlib.h>
#include "ui/ws_ui_util.h"
#include "epan/color_filters.h"

/* Called by the epan core to let the GUI process pending events.
   In webshark there's no GUI event loop — this is a no-op.        */
void main_window_update(void)
{
}

/* Called to exit the application cleanly. */
void exit_application(int status)
{
    exit(status);
}

/* Called when a colour filter is loaded from the colour filter file.
   In webshark we don't use colour filters visually, so discard.    */
void color_filter_add_cb(color_filter_t *colorf, void *user_data)
{
    (void)colorf; (void)user_data;
}
