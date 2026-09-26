/** @file
 * Collecting of Expert information.
 *
 * For further info, see:
 *    https://gitlab.com/wireshark/wireshark/-/wikis/Development/ExpertInfo
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __EXPERT_H__
#define __EXPERT_H__

#include <epan/proto.h>
#include <epan/packet_info.h>
#include "value_string.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct expert_info_s {
	uint32_t     packet_num;
	int          group;
	int          severity;
	int          hf_index; /* hf_index of the expert item. Might be -1. */
	const char *protocol;
	char        *summary;
	proto_item  *pitem;
} expert_info_t;

typedef struct expert_field
{
	int ei;
	int hf;
} expert_field;

#define EI_INIT_EI -1
#define EI_INIT_HF -1
#define EI_INIT {EI_INIT_EI, EI_INIT_HF}

typedef struct expert_field_info {
	/* ---------- set by dissector --------- */
	const char       *name;
	int               group;
	int               severity;
	const char       *summary;

	/* ------- set by register routines (prefilled by EXPFILL macro, see below) ------ */
	int               id;
	const char       *protocol;
	int               orig_severity; /* Matches severity when registered, used to restore original severity
					  * if UAT severity entry is removed */
	hf_register_info  hf_info;

} expert_field_info;

#define EXPFILL 0, NULL, 0, \
        {0, {NULL, NULL, FT_NONE, BASE_NONE, NULL, 0, NULL, HFILL}}

typedef struct ei_register_info {
	expert_field      *ids;         /**< written to by register() function */
	expert_field_info  eiinfo;      /**< the field info to be registered */
} ei_register_info;

typedef struct expert_module expert_module_t;

#define PRE_ALLOC_EXPERT_FIELDS_MEM 5000

extern int proto_expert;

extern void expert_init(void);

extern void expert_packet_init(void);

extern void expert_cleanup(void);

extern void expert_packet_cleanup(void);

int expert_get_highest_severity(void);

void expert_update_comment_count(uint64_t count);

proto_item * expert_add_info(packet_info *pinfo, proto_item *pi, expert_field *eiindex);

proto_item * expert_add_info_format(packet_info *pinfo, proto_item *pi, expert_field *eiindex,
                       const char *format, ...) G_GNUC_PRINTF(4, 5);

proto_item * proto_tree_add_expert(proto_tree *tree, packet_info *pinfo, expert_field *eiindex,
        tvbuff_t *tvb, int start, int length);

proto_item * proto_tree_add_expert_format(proto_tree *tree, packet_info *pinfo, expert_field *eiindex,
        tvbuff_t *tvb, int start, int length, const char *format, ...) G_GNUC_PRINTF(7, 8);

expert_module_t *expert_register_protocol(int id);

void expert_deregister_expertinfo (const char *abbrev);

void expert_deregister_protocol (expert_module_t *module);

void expert_free_deregistered_expertinfos (void);

const char* expert_get_summary(expert_field *eiindex);

void expert_register_field_array(expert_module_t *module, ei_register_info *ei, const int num_records);

#define EXPERT_CHECKSUM_DISABLED    -2
#define EXPERT_CHECKSUM_UNKNOWN     -1
#define EXPERT_CHECKSUM_GOOD        0
#define EXPERT_CHECKSUM_BAD         1

extern const value_string expert_group_vals[];

extern const value_string expert_severity_vals[];

extern const value_string expert_checksum_vals[];

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __EXPERT_H__ */
