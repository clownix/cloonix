/* capture_dissectors.h
 * Routines for handling capture dissectors
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __CAPTURE_DISSECTORS_H__
#define __CAPTURE_DISSECTORS_H__

#include <glib.h>
#include <wiretap/wtap.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Table of counts of packets of various types. */
typedef struct {
    GHashTable  *counts_hash;
} packet_counts;

typedef struct {
    GHashTable *counts;
} capture_packet_info_t;

typedef bool (*capture_dissector_t)(const uint8_t *pd, int offset, int len,
        capture_packet_info_t *cpinfo, const union wtap_pseudo_header *pseudo_header);

struct capture_dissector_handle;
typedef struct capture_dissector_handle *capture_dissector_handle_t;

void capture_dissector_init(void);
void capture_dissector_cleanup(void);
void register_capture_dissector_table(const char *name, const char *ui_name);
capture_dissector_handle_t create_capture_dissector_handle(capture_dissector_t dissector, int proto);
capture_dissector_handle_t find_capture_dissector(const char *name);
capture_dissector_handle_t register_capture_dissector(const char *name, capture_dissector_t dissector, int proto);
void capture_dissector_add_uint(const char *name, const uint32_t pattern, capture_dissector_handle_t handle);
bool try_capture_dissector(const char *name, const uint32_t pattern, const uint8_t *pd, int offset, int len, capture_packet_info_t *cpinfo, const union wtap_pseudo_header *pseudo_header);
bool call_capture_dissector(capture_dissector_handle_t handle, const uint8_t *pd, int offset, int len, capture_packet_info_t *cpinfo, const union wtap_pseudo_header *pseudo_header);
uint32_t capture_dissector_get_count(packet_counts *counts, const int proto);
void capture_dissector_increment_count(capture_packet_info_t *cpinfo, const int proto);

#ifdef __cplusplus
}
#endif

#endif /* __CAPTURE_DISSECTORS_H__ */
