/** @file
 *
 * Simple dialog stubs for webshark (no GUI required).
 * All dialogs fall through to stderr.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __SIMPLE_DIALOG_H__
#define __SIMPLE_DIALOG_H__

#include <stdarg.h>
#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ESD_TYPE_INFO,
    ESD_TYPE_WARN,
    ESD_TYPE_ERROR,
    ESD_TYPE_CONFIRMATION,
    ESD_TYPE_STOP
} ESD_TYPE_E;

#define ESD_BTN_NONE    0x00
#define ESD_BTN_OK      0x01
#define ESD_BTN_CANCEL  0x02
#define ESD_BTN_YES     0x04
#define ESD_BTN_NO      0x08
#define ESD_BTN_CLEAR   0x10
#define ESD_BTN_SAVE    0x20

typedef void * (*simple_dialog_cb_t)(int button, void *data);

/* Show a dialog — in webshark we just log to stderr */
static inline void *
simple_dialog(ESD_TYPE_E type, int btn_mask, const char *msg_format, ...)
{
    (void)type; (void)btn_mask;
    va_list ap;
    va_start(ap, msg_format);
    vfprintf(stderr, msg_format, ap);
    fputc('\n', stderr);
    va_end(ap);
    return NULL;
}

static inline void
simple_message_box(ESD_TYPE_E type, const char *primary,
                   const char *secondary, const char *msg_format, ...)
{
    (void)type; (void)primary; (void)secondary;
    va_list ap;
    va_start(ap, msg_format);
    vfprintf(stderr, msg_format, ap);
    fputc('\n', stderr);
    va_end(ap);
}

/* These are provided by the UI backend (simple_dialog_stubs.c for webshark) */
void vsimple_error_message_box(const char *msg_format, va_list ap);
void vsimple_warning_message_box(const char *msg_format, va_list ap);
void simple_error_message_box(const char *msg_format, ...)
    G_GNUC_PRINTF(1, 2);

static inline void
simple_dialog_set_cb(void *dialog, simple_dialog_cb_t callback, void *data)
{
    (void)dialog; (void)callback; (void)data;
}

static inline int
simple_dialog_run(void *dialog)
{
    (void)dialog;
    return ESD_BTN_OK;
}

#ifdef __cplusplus
}
#endif

#endif /* __SIMPLE_DIALOG_H__ */
