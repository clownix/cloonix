/** @file
 * Main status bar stubs for webshark.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef __MAIN_STATUSBAR_H__
#define __MAIN_STATUSBAR_H__
#include <glib.h>
static inline void statusbar_push_temporary_msg(const char *msg_format, ...) {
    (void)msg_format;
}
static inline void statusbar_push_file_layer_msg(const char *msg_format, ...) {
    (void)msg_format;
}
static inline void statusbar_pop_file_layer_msg(void) {}
static inline void statusbar_push_filter_msg(const char *msg_format, ...) {
    (void)msg_format;
}
static inline void statusbar_pop_filter_msg(void) {}
static inline void statusbar_push_expert_msg(int severity) { (void)severity; }
static inline void statusbar_pop_expert_msg(void) {}
#endif
