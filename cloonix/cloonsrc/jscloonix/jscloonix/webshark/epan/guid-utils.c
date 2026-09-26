/* guid-utils.c
 * GUID handling
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 *
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"
#include <string.h>
#include <glib.h>
#include <epan/epan.h>
#include <wsutil/unicode-utils.h>
#include <epan/wmem_scopes.h>
#include "guid-utils.h"

static wmem_tree_t *guid_to_name_tree;


void guids_add_guid(const e_guid_t *guid, const char *name)
{
	wmem_tree_key_t guidkey[2];
	uint32_t g[4];

	g[0]=guid->data1;

	g[1]=guid->data2;
	g[1]<<=16;
	g[1]|=guid->data3;

	g[2]=guid->data4[0];
	g[2]<<=8;
	g[2]|=guid->data4[1];
	g[2]<<=8;
	g[2]|=guid->data4[2];
	g[2]<<=8;
	g[2]|=guid->data4[3];

	g[3]=guid->data4[4];
	g[3]<<=8;
	g[3]|=guid->data4[5];
	g[3]<<=8;
	g[3]|=guid->data4[6];
	g[3]<<=8;
	g[3]|=guid->data4[7];

	guidkey[0].key=g;
	guidkey[0].length=4;
	guidkey[1].length=0;

	wmem_tree_insert32_array(guid_to_name_tree, &guidkey[0], (char *) name);
}

void guids_delete_guid(const e_guid_t *guid)
{
	wmem_tree_key_t guidkey[2];
	uint32_t g[4];

	g[0] = guid->data1;

	g[1] = guid->data2;
	g[1] <<= 16;
	g[1] |= guid->data3;

	g[2] = guid->data4[0];
	g[2] <<= 8;
	g[2] |= guid->data4[1];
	g[2] <<= 8;
	g[2] |= guid->data4[2];
	g[2] <<= 8;
	g[2] |= guid->data4[3];

	g[3] = guid->data4[4];
	g[3] <<= 8;
	g[3] |= guid->data4[5];
	g[3] <<= 8;
	g[3] |= guid->data4[6];
	g[3] <<= 8;
	g[3] |= guid->data4[7];

	guidkey[0].key = g;
	guidkey[0].length = 4;
	guidkey[1].length = 0;

	void *data = wmem_tree_lookup32_array(guid_to_name_tree, &guidkey[0]);

	if (data != NULL) {
		// This will "remove" the entry by setting its data to NULL
		wmem_tree_insert32_array(guid_to_name_tree, &guidkey[0], NULL);
	}

}

const char * guids_get_guid_name(const e_guid_t *guid, wmem_allocator_t *scope )
{
	wmem_tree_key_t guidkey[2];
	uint32_t g[4];
	char *name;

	g[0]=guid->data1;

	g[1]=guid->data2;
	g[1]<<=16;
	g[1]|=guid->data3;

	g[2]=guid->data4[0];
	g[2]<<=8;
	g[2]|=guid->data4[1];
	g[2]<<=8;
	g[2]|=guid->data4[2];
	g[2]<<=8;
	g[2]|=guid->data4[3];

	g[3]=guid->data4[4];
	g[3]<<=8;
	g[3]|=guid->data4[5];
	g[3]<<=8;
	g[3]|=guid->data4[6];
	g[3]<<=8;
	g[3]|=guid->data4[7];

	guidkey[0].key=g;
	guidkey[0].length=4;
	guidkey[1].length=0;

	if((name = (char *)wmem_tree_lookup32_array(guid_to_name_tree, &guidkey[0]))){
		return name;
	}

	return NULL;
}


void guids_init(void)
{
	guid_to_name_tree=wmem_tree_new(wmem_epan_scope());
}


const char * guids_resolve_guid_to_str(const e_guid_t *guid, wmem_allocator_t *scope)
{
	const char *name;

	name=guids_get_guid_name(guid, scope);
	if(name){
		return name;
	}

	return wmem_strdup_printf(scope, "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
				guid->data1, guid->data2, guid->data3,
				guid->data4[0], guid->data4[1],
				guid->data4[2], guid->data4[3],
				guid->data4[4], guid->data4[5],
				guid->data4[6], guid->data4[7]);
}

int guid_cmp(const e_guid_t *g1, const e_guid_t *g2)
{
	if (g1->data1 != g2->data1) {
		return (g1->data1 < g2->data1) ? -1 : 1;
	}

	if (g1->data2 != g2->data2) {
		return (g1->data2 < g2->data2) ? -1 : 1;
	}

	if (g1->data3 != g2->data3) {
		return (g1->data3 < g2->data3) ? -1 : 1;
	}

	return memcmp(&g1->data4[0], &g2->data4[0], 8);
}

unsigned guid_hash(const e_guid_t *guid)
{
	return g_int64_hash((const int64_t *)guid);
}

/*
 * Editor modelines  -  https://www.wireshark.org/tools/modelines.html
 *
 * Local variables:
 * c-basic-offset: 8
 * tab-width: 8
 * indent-tabs-mode: t
 * End:
 *
 * vi: set shiftwidth=8 tabstop=8 noexpandtab:
 * :indentSize=8:tabSize=8:noTabs=false:
 */
