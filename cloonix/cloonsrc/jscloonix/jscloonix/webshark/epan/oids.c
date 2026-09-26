/* oids.c
 * Object IDentifier Support
 *
 * (c) 2007, Luis E. Garcia Ontanon <luis@ontanon.org>
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include <glib.h>
#include <stdio.h>
#include <string.h>

#include <wsutil/report_message.h>

#include <epan/strutil.h>
#include <epan/wmem_scopes.h>
#include "uat.h"
#include "prefs.h"
#include "proto.h"
#include "packet.h"
#include "dissectors/packet-ber.h"
#include <wsutil/ws_assert.h>

#define D(level,args) do if (debuglevel >= level) { printf args; printf("\n"); fflush(stdout); } while(0)

#include "oids.h"

static int debuglevel;

static const oid_value_type_t unknown_type =    { FT_BYTES,  BASE_NONE, BER_CLASS_ANY, BER_TAG_ANY,             0,  -1, OID_KEY_TYPE_WRONG,   0};

static oid_info_t oid_root = { 0, NULL, OID_KIND_UNKNOWN, NULL, &unknown_type, -2, NULL, NULL, NULL};

static void prepopulate_oids(void) {
	if (!oid_root.children) {
		char* debug_env = getenv("WIRESHARK_DEBUG_MIBS");
		uint32_t subid;

		debuglevel = debug_env ? (int)strtoul(debug_env,NULL,10) : 0;

		oid_root.children = wmem_tree_new(wmem_epan_scope());

		subid = 0; oid_add("itu-t",1,&subid);
		subid = 1; oid_add("iso",1,&subid);
		subid = 2; oid_add("joint-iso-itu-t",1,&subid);
	}
}

static oid_info_t* add_oid(const char* name, oid_kind_t kind, const oid_value_type_t* type, oid_key_t* key, unsigned oid_len, uint32_t *subids) {
	unsigned i = 0;
	oid_info_t* c = &oid_root;

	prepopulate_oids();
	oid_len--;

	do {
		oid_info_t* n = (oid_info_t *)wmem_tree_lookup32(c->children,subids[i]);

		if(n) {
			if (i == oid_len) {
				if (n->name) {
					if (!g_str_equal(n->name,name)) {
						D(2,("Renaming Oid from: %s -> %s, this means the same oid is registered more than once",n->name,name));
					}
					wmem_free(wmem_epan_scope(), n->name);
				}

				n->name = wmem_strdup(wmem_epan_scope(), name);

				if (! n->value_type) {
					n->value_type = type;
				}

				return n;
			}
		} else {
			n = wmem_new(wmem_epan_scope(), oid_info_t);
			n->subid = subids[i];
			n->kind = kind;
			n->children = wmem_tree_new(wmem_epan_scope());
			n->value_hfid = -2;
			n->key = key;
			n->parent = c;
			n->bits = NULL;

			wmem_tree_insert32(c->children,n->subid,n);

			if (i == oid_len) {
				n->name = wmem_strdup(wmem_epan_scope(), name);
				n->value_type = type;
				n->kind = kind;
				return n;
			} else {
				n->name = NULL;
				n->value_type = NULL;
				n->kind = OID_KIND_UNKNOWN;
			}
		}
		c = n;
	} while(++i);

	ws_assert_not_reached();
	return NULL;
}

// NOLINTNEXTLINE(misc-no-recursion)
void oid_add(const char* name, unsigned oid_len, uint32_t *subids) {
	ws_assert(subids && *subids <= 2);
	if (oid_len) {
		char* sub = oid_subid2string(NULL, subids,oid_len);
		D(3,("\tOid (from subids): %s %s ",name?name:"NULL", sub));
		add_oid(name,OID_KIND_UNKNOWN,NULL,NULL,oid_len,subids);
		wmem_free(NULL, sub);
	} else {
		D(1,("Failed to add Oid: %s (from subids)",name?name:"NULL"));
	}
}

void oid_add_from_string(const char* name, const char *oid_str) {
	uint32_t* subids;
	unsigned oid_len = oid_string2subid(NULL, oid_str, &subids);

	if (oid_len) {
		char* sub = oid_subid2string(NULL, subids,oid_len);
		D(3,("\tOid (from string): %s %s ",name?name:"NULL", sub));
		add_oid(name,OID_KIND_UNKNOWN,NULL,NULL,oid_len,subids);
		wmem_free(NULL, sub);
	} else {
		D(1,("Failed to add Oid: %s %s ",name?name:"NULL", oid_str?oid_str:NULL));
	}
	wmem_free(NULL, subids);
}

extern void oid_add_from_encoded(const char* name, const uint8_t *oid, int oid_len) {
	uint32_t* subids = NULL;
	unsigned subids_len = oid_encoded2subid(NULL, oid, oid_len, &subids);

	if (subids_len) {
		char* sub = oid_subid2string(NULL, subids,subids_len);
		D(3,("\tOid (from encoded): %s %s ",name, sub));
		add_oid(name,OID_KIND_UNKNOWN,NULL,NULL,subids_len,subids);
		wmem_free(NULL, sub);
	} else {
		char* bytestr = bytes_to_str_punct(NULL, oid, oid_len, ':');
		D(1,("Failed to add Oid: %s [%d]%s ",name?name:"NULL", oid_len, bytestr));
		wmem_free(NULL, bytestr);
	}
	wmem_free(NULL, subids);
}


void oid_pref_init(module_t *nameres)
{
    prefs_register_static_text_preference(nameres, "load_smi_modules_static",
                            "Enable OID resolution: N/A",
                            "Support for OID resolution was not compiled into this version of Wireshark");

    prefs_register_static_text_preference(nameres, "suppress_smi_errors_static",
                            "Suppress SMI errors: N/A",
                            "Support for OID resolution was not compiled into this version of Wireshark");

    prefs_register_static_text_preference(nameres, "smi_module_path",
                            "SMI (MIB and PIB) modules and paths: N/A",
                            "Support for OID resolution was not compiled into this version of Wireshark");
}

void oids_init(void) {
	prepopulate_oids();
	D(1,("libsmi disabled oid resolution not enabled"));
}

void oids_cleanup(void) {
	D(1,("libsmi disabled oid resolution not enabled"));
}

char* oid_subid2string(wmem_allocator_t *scope, uint32_t* subids, unsigned len) {
	return rel_oid_subid2string(scope, subids, len, true);
}
char* rel_oid_subid2string(wmem_allocator_t *scope, uint32_t* subids, unsigned len, bool is_absolute) {

	wmem_strbuf_t *oid_str;
	size_t oid_str_len;

	if(!subids || len == 0)
		return wmem_strdup(scope, "*** Empty OID ***");

	oid_str = wmem_strbuf_new(scope, "");

	if (!is_absolute)
		wmem_strbuf_append_c(oid_str, '.');

	do {
		wmem_strbuf_append_printf(oid_str, "%u.",*subids++);
	} while(--len);

	/* Remove trailing "." (which is guaranteed to be there) */
	oid_str_len = wmem_strbuf_get_len(oid_str);
	wmem_strbuf_truncate(oid_str, oid_str_len - 1);

	return wmem_strbuf_finalize(oid_str);
}

/* If a valid OID string, return number of numbers */
static unsigned check_num_oid(const char* str) {
	const char* r = str;
	char c = '.';
	unsigned n = 0;

	D(8,("check_num_oid: '%s'",str));
	if (!r) return 0;

	do {
		D(9,("\tcheck_num_oid: '%c' %u",*r,n));
		switch(*r) {
			case '.': case '\0':
				n++;
				if (c == '.') return 0;
				break;
			case '1' : case '2' : case '3' : case '4' : case '5' :
			case '6' : case '7' : case '8' : case '9' : case '0' :
				continue;
			default:
				return 0;
		}
	} while((c = *r++));

	return n;
}

/* Set subids_p to an array of found numbers, return number of numbers */
unsigned oid_string2subid(wmem_allocator_t *scope, const char* str, uint32_t** subids_p) {
	const char* r = str;
	uint32_t* subids;
	uint32_t* subids_overflow;
	unsigned n = check_num_oid(str);
	/*
	 * we cannot handle sub-ids greater than 32bytes
	 * keep a pilot subid of 64 bytes to check the limit
	 */
	uint64_t subid = 0;

	D(6,("oid_string2subid: str='%s'",str));

	if (!n) {
		*subids_p = NULL;
		return 0;
	}

	*subids_p = subids = wmem_alloc0_array(scope, uint32_t, n);
	subids_overflow = subids + n;
	do switch(*r) {
		case '.':
			subid = 0;
			subids++;
			continue;
		case '1' : case '2' : case '3' : case '4' : case '5' :
		case '6' : case '7' : case '8' : case '9' : case '0' :
			subid *= 10;
			subid += *r - '0';

			if( subids >= subids_overflow ||  subid > 0xffffffff) {
				wmem_free(scope, *subids_p);
				*subids_p=NULL;
				return 0;
			}

			*(subids) *= 10;
			*(subids) += *r - '0';
			continue;
		case '\0':
			break;
		default:
			return 0;
	} while(*r++);

	return n;
}


unsigned oid_encoded2subid(wmem_allocator_t *scope, const uint8_t *oid_bytes, int oid_len, uint32_t** subids_p) {
	return oid_encoded2subid_sub(scope, oid_bytes, oid_len, subids_p, true);
}
unsigned oid_encoded2subid_sub(wmem_allocator_t *scope, const uint8_t *oid_bytes, int oid_len, uint32_t** subids_p,
		bool is_first) {
	int i;
	unsigned n = is_first ? 1 : 0;
	uint32_t* subids;
	uint32_t* subid_overflow;
	/*
	 * we cannot handle sub-ids greater than 32bytes
	 * have the subid in 64 bytes to be able to check the limit
	 */
	uint64_t subid = 0;

	for (i=0; i<oid_len; i++) { if (! (oid_bytes[i] & 0x80 )) n++; }

	*subids_p = subids = (uint32_t *)wmem_alloc(scope, sizeof(uint32_t)*n);
	subid_overflow = subids+n;

	/* If n is 0 or 1 (depending on how it was initialized) then we found
	 * no bytes in the OID with first bit cleared, so initialize our one
	 * byte (if any) to zero and return. This *seems* to be the right thing
	 * to do in this situation, and at the very least it avoids
	 * uninitialized memory errors that would otherwise occur. */
	if (is_first && n == 1) {
		*subids = 0;
		return n;
	}
	else if (!is_first && n == 0) {
		return n;
	}

	for (i=0; i<oid_len; i++){
		uint8_t byte = oid_bytes[i];

		subid <<= 7;
		subid |= byte & 0x7F;

		if (byte & 0x80) {
			continue;
		}

		if (is_first) {
			uint32_t subid0 = 0;

			if (subid >= 40) { subid0++; subid-=40; }
			if (subid >= 40) { subid0++; subid-=40; }

			*subids++ = subid0;

			is_first = false;
		}

		if( subids >= subid_overflow || subid > 0xffffffff) {
			/* scope may be NULL in which case we must free our
			 * useless buffer before returning */
			wmem_free(scope, *subids_p);
			*subids_p = NULL;
			return 0;
		}

		*subids++ = (uint32_t)subid;
		subid = 0;
	}

	ws_assert(subids == subid_overflow);

	return n;
}

oid_info_t* oid_get(unsigned len, uint32_t* subids, unsigned* matched, unsigned* left) {
	oid_info_t* curr_oid = &oid_root;
	unsigned i;

	if(!(subids && *subids <= 2)) {
		*matched = 0;
		*left = len;
		return curr_oid;
	}

	for( i=0; i < len; i++) {
		oid_info_t* next_oid = (oid_info_t *)wmem_tree_lookup32(curr_oid->children,subids[i]);
		if (next_oid) {
			curr_oid = next_oid;
		} else {
			goto done;
		}
	}
done:
	*matched = i;
	*left = len - i;
	return curr_oid;
}


oid_info_t* oid_get_from_encoded(wmem_allocator_t *scope, const uint8_t *bytes, int byteslen, uint32_t** subids_p, unsigned* matched_p, unsigned* left_p) {
	unsigned subids_len = oid_encoded2subid(scope, bytes, byteslen, subids_p);
	return oid_get(subids_len, *subids_p, matched_p, left_p);
}

oid_info_t* oid_get_from_string(wmem_allocator_t *scope, const char *oid_str, uint32_t** subids_p, unsigned* matched, unsigned* left) {
	unsigned subids_len = oid_string2subid(scope, oid_str, subids_p);
	return oid_get(subids_len, *subids_p, matched, left);
}

char *oid_resolved_from_encoded(wmem_allocator_t *scope, const uint8_t *oid, int oid_len) {
	uint32_t *subid_oid = NULL;
	char * ret;
	unsigned subid_oid_length = oid_encoded2subid(NULL, oid, oid_len, &subid_oid);

	ret = oid_resolved(scope, subid_oid_length, subid_oid);
	wmem_free(NULL, subid_oid);
	return ret;
}

char *rel_oid_resolved_from_encoded(wmem_allocator_t *scope, const uint8_t *oid, int oid_len) {
	uint32_t *subid_oid = NULL;
	char* ret;
	unsigned subid_oid_length = oid_encoded2subid_sub(NULL, oid, oid_len, &subid_oid, false);

	ret = rel_oid_subid2string(scope, subid_oid, subid_oid_length, false);
	wmem_free(NULL, subid_oid);
	return ret;
}


unsigned oid_subid2encoded(wmem_allocator_t *scope, unsigned subids_len, uint32_t* subids, uint8_t** bytes_p) {
	unsigned bytelen = 0;
	unsigned i;
	uint32_t subid;
	uint8_t* b;

	if ( !subids || subids_len <= 1) {
		*bytes_p = NULL;
		return 0;
	}

	for (subid=subids[0] * 40, i = 1; i<subids_len; i++, subid=0) {
		subid += subids[i];
		if (subid <= 0x0000007F) {
			bytelen += 1;
		} else if (subid <= 0x00003FFF ) {
			bytelen += 2;
		} else if (subid <= 0x001FFFFF ) {
			bytelen += 3;
		} else if (subid <= 0x0FFFFFFF ) {
			bytelen += 4;
		} else {
			bytelen += 5;
		}
	}

	*bytes_p = b = (uint8_t *)wmem_alloc(scope, bytelen);

	for (subid=subids[0] * 40, i = 1; i<subids_len; i++, subid=0) {
		unsigned len;

		subid += subids[i];
		if ((subid <= 0x0000007F )) len = 1;
		else if ((subid <= 0x00003FFF )) len = 2;
		else if ((subid <= 0x001FFFFF )) len = 3;
		else if ((subid <= 0x0FFFFFFF )) len = 4;
		else len = 5;

		switch(len) {
			default: *bytes_p=NULL; return 0;
			case 5: *(b++) = ((subid & 0xF0000000) >> 28) | 0x80;
			/* FALL THROUGH */
			case 4: *(b++) = ((subid & 0x0FE00000) >> 21) | 0x80;
			/* FALL THROUGH */
			case 3: *(b++) = ((subid & 0x001FC000) >> 14) | 0x80;
			/* FALL THROUGH */
			case 2: *(b++) = ((subid & 0x00003F80) >> 7)  | 0x80;
			/* FALL THROUGH */
			case 1: *(b++) =   subid & 0x0000007F ; break;
		}
	}

	return bytelen;
}

char* oid_encoded2string(wmem_allocator_t *scope, const uint8_t* encoded, unsigned len) {
	uint32_t* subids = NULL;
	char* ret;
	unsigned subids_len = oid_encoded2subid(NULL, encoded, len, &subids);

	if (subids_len) {
		ret = oid_subid2string(scope, subids,subids_len);
	} else {
		ret = wmem_strdup(scope, "");
	}

	wmem_free(NULL, subids);
	return ret;
}

char* rel_oid_encoded2string(wmem_allocator_t *scope, const uint8_t* encoded, unsigned len) {
	uint32_t* subids = NULL;
	char* ret;
	unsigned subids_len = oid_encoded2subid_sub(NULL, encoded, len, &subids, false);

	if (subids_len) {
		ret = rel_oid_subid2string(scope, subids,subids_len, false);
	} else {
		ret = wmem_strdup(scope, "");
	}

	wmem_free(NULL, subids);
	return ret;
}

unsigned oid_string2encoded(wmem_allocator_t *scope, const char *oid_str, uint8_t **bytes) {
	uint32_t* subids;
	uint32_t subids_len;
	unsigned byteslen;

	if ( (subids_len = oid_string2subid(NULL, oid_str, &subids)) &&
	     (byteslen   = oid_subid2encoded(scope, subids_len, subids, bytes)) ) {
		wmem_free(NULL, subids);
		return byteslen;
	}
	wmem_free(NULL, subids);
	return 0;
}

char *oid_resolved_from_string(wmem_allocator_t *scope, const char *oid_str) {
	uint32_t    *subid_oid;
	unsigned     subid_oid_length;
	char *resolved;

	subid_oid_length = oid_string2subid(NULL, oid_str, &subid_oid);
	resolved         = oid_resolved(scope, subid_oid_length, subid_oid);

	wmem_free(NULL, subid_oid);

	return resolved;
}

char *oid_resolved(wmem_allocator_t *scope, uint32_t num_subids, uint32_t* subids) {
	unsigned matched;
	unsigned left;
	oid_info_t* oid;

	if(! (subids && *subids <= 2 ))
		return wmem_strdup(scope, "*** Malformed OID ***");

	oid = oid_get(num_subids, subids, &matched, &left);

	while (! oid->name ) {
		if (!(oid = oid->parent)) {
			return oid_subid2string(scope, subids,num_subids);
		}
		left++;
		matched--;
	}

	if (left) {
		char *ret,
			  *str1 = oid_subid2string(NULL, subids,matched),
			  *str2 = oid_subid2string(NULL, &(subids[matched]),left);

		ret = wmem_strconcat(scope, oid->name ? oid->name : str1, ".", str2, NULL);
		wmem_free(NULL, str1);
		wmem_free(NULL, str2);
		return ret;
	} else {
		return oid->name ? wmem_strdup(scope, oid->name) : oid_subid2string(scope, subids,matched);
	}
}

extern void oid_both(wmem_allocator_t *scope, unsigned oid_len, uint32_t *subids, char** resolved_p, char** numeric_p) {
	*resolved_p = oid_resolved(scope, oid_len,subids);
	*numeric_p = oid_subid2string(scope, subids,oid_len);
}

extern void oid_both_from_encoded(wmem_allocator_t *scope, const uint8_t *oid, int oid_len, char** resolved_p, char** numeric_p) {
	uint32_t* subids = NULL;
	unsigned subids_len = oid_encoded2subid(NULL, oid, oid_len, &subids);
	*resolved_p = oid_resolved(scope, subids_len,subids);
	*numeric_p = oid_subid2string(scope, subids,subids_len);
	wmem_free(NULL, subids);
}

void oid_both_from_string(wmem_allocator_t *scope, const char *oid_str, char** resolved_p, char** numeric_p) {
	uint32_t *subids;
	unsigned subids_len;

	subids_len  = oid_string2subid(NULL, oid_str, &subids);
	*resolved_p = oid_resolved(scope, subids_len,subids);
	*numeric_p  = oid_subid2string(scope, subids,subids_len);
	wmem_free(NULL, subids);
}

/**
 * Fetch the default OID path.
 */
extern char *
oid_get_default_mib_path(void) {
        return g_strdup("");
}

#ifdef DEBUG_OIDS
char* oid_test_a2b(uint32_t num_subids, uint32_t* subids) {
	uint8_t* sub2enc = NULL;
	uint8_t* str2enc = NULL;
	uint32_t* enc2sub = NULL;
	uint32_t* str2sub;
	char* ret;
	char* sub2str = oid_subid2string(NULL, subids, num_subids);
	unsigned sub2enc_len = oid_subid2encoded(NULL, num_subids, subids,&sub2enc);
	unsigned enc2sub_len = oid_encoded2subid(NULL, sub2enc, sub2enc_len, &enc2sub);
	char* enc2str = oid_encoded2string(NULL, sub2enc, sub2enc_len);
	unsigned str2enc_len = oid_string2encoded(NULL, sub2str,&str2enc);
	unsigned str2sub_len = oid_string2subid(sub2str,&str2sub);

	char* sub2enc_str = bytes_to_str_punct(NULL, sub2enc, sub2enc_len, ':');
	char* enc2sub_str = enc2sub ? oid_subid2string(NULL, enc2sub,enc2sub_len) : wmem_strdup(NULL, "-");
	char* str2enc_str = bytes_to_str_punct(NULL, str2enc, str2enc_len, ':');
	char* str2sub_str = str2sub ? oid_subid2string(NULL, str2sub,str2sub_len) : wmem_strdup(NULL, "-");

	ret = wmem_strdup_printf(NULL,
							"oid_subid2string=%s \n"
							"oid_subid2encoded=[%d]%s \n"
							"oid_encoded2subid=%s \n "
							"oid_encoded2string=%s \n"
							"oid_string2encoded=[%d]%s \n"
							"oid_string2subid=%s \n "
							,sub2str
							,sub2enc_len,sub2enc_str
							,enc2sub_str
							,enc2str
							,str2enc_len,str2enc_str,
							,str2sub_str
							);

	wmem_free(NULL, sub2enc_str);
	wmem_free(NULL, enc2sub_str);
	wmem_free(NULL, str2enc_str);
	wmem_free(NULL, str2sub_str);

	wmem_free(NULL, sub2str);
	wmem_free(NULL, enc2sub);
	wmem_free(NULL, sub2enc);
	wmem_free(NULL, str2enc);
	wmem_free(NULL, enc2str);
	return ret;
}

void add_oid_debug_subtree(oid_info_t* oid_info, proto_tree *tree) {
	static const char* oid_kinds[] = { "Unknown", "Node", "Scalar", "Table", "Row", "Column", "Notification", "Group", "Compliance", "Capabilities"};
	static const char* key_types[] = {"OID_KEY_TYPE_WRONG","OID_KEY_TYPE_INTEGER",
										"OID_KEY_TYPE_FIXED_STRING","OID_KEY_TYPE_FIXED_BYTES","OID_KEY_TYPE_STRING",
										"OID_KEY_TYPE_BYTES","OID_KEY_TYPE_NSAP","OID_KEY_TYPE_OID","OID_KEY_TYPE_IPADDR"};
	proto_item* pi = proto_tree_add_debug_text(tree,NULL,0,0,
	"OidInfo: Name='%s' sub-id=%u  kind=%s  hfid=%d",
	oid_info->name ? oid_info->name : "",
	oid_info->subid,
	oid_info->kind <= OID_KIND_CAPABILITIES ? oid_kinds[oid_info->kind] : "BROKEN",
	oid_info->value_hfid);
	proto_tree* pt = proto_item_add_subtree(pi,0);
	oid_key_t* key;

	for(key = oid_info->key; key; key = key->next) {
		proto_tree_add_debug_text(pt,NULL,0,0,
		"Key: name='%s' num_subids=%d type=%s",
		key->name,
		key->key_type <= OID_KEY_TYPE_IPADDR ? key_types[key->key_type] : "BROKEN"
		);
	};

	if (oid_info->parent) {
		pi = proto_tree_add_debug_text(pt,NULL,0,0,"Parent:");
		pt = proto_item_add_subtree(pi,0);
		add_oid_debug_subtree(oid_info->parent, pt);
	}
}
#endif

/*
 * Editor modelines
 *
 * Local Variables:
 * c-basic-offset: 8
 * tab-width: 8
 * indent-tabs-mode: t
 * End:
 *
 * ex: set shiftwidth=8 tabstop=8 noexpandtab:
 * :indentSize=8:tabSize=8:noTabs=false:
 */
