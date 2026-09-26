/* simple_dialog_stubs.c
 * Stub implementations of UI dialog functions for webshark (no Qt).
 * All dialogs fall through to stderr.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include <stdio.h>
#include <stdarg.h>

#include "ui/simple_dialog.h"

void vsimple_error_message_box(const char *msg_format, va_list ap)
{
    vfprintf(stderr, msg_format, ap);
    fputc('\n', stderr);
}

void vsimple_warning_message_box(const char *msg_format, va_list ap)
{
    vfprintf(stderr, msg_format, ap);
    fputc('\n', stderr);
}

void simple_error_message_box(const char *msg_format, ...)
{
    va_list ap;
    va_start(ap, msg_format);
    vfprintf(stderr, msg_format, ap);
    fputc('\n', stderr);
    va_end(ap);
}
