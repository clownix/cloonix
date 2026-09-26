/** @file
 * Declarations of routines for {fragment,segment} reassembly
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/* make sure that all flags that are set in a fragment entry is also set for
 * the flags field of fd_head !!!
 */

#ifndef REASSEMBLE_H
#define REASSEMBLE_H

#define FD_DEFRAGMENTED		0x0001

#define FD_OVERLAP		0x0002

#define FD_OVERLAPCONFLICT	0x0004

#define FD_MULTIPLETAILS	0x0008

#define FD_TOOLONGFRAGMENT	0x0010

#define FD_SUBSET_TVB           0x0020

#define FD_PARTIAL_REASSEMBLY   0x0040

#define FD_BLOCKSEQUENCE        0x0100

#define FD_DATALEN_SET		0x0400

typedef struct _fragment_item {
	struct _fragment_item *next;
	uint32_t frame;			/**< frame number where the fragment is from */
	uint32_t	offset;			/**< fragment number for FD_BLOCKSEQUENCE, byte
					 * offset otherwise */
	uint32_t	len;			/**< fragment length */
	uint32_t flags;			/**< XXX - do some of these apply only to reassembly
					 * heads and others only to fragments within
					 * a reassembly? */
	tvbuff_t *tvb_data;
} fragment_item;

typedef struct _fragment_head {
	struct _fragment_item *next;
	struct _fragment_item *first_gap;	/**< pointer to last fragment before first gap.
					 * NULL if there is no fragment starting at offset 0 */
	unsigned ref_count; 		/**< reference count in reassembled_table */
	uint32_t contiguous_len;	/**< contiguous length from head up to first gap */
	uint32_t frame;			/**< maximum of all frame numbers added to reassembly */
	uint32_t	len;			/**< When flags&FD_BLOCKSEQUENCE and FD_DEFRAGMENTED
					 * are set, the number of bytes of the full datagram.
					 * Otherwise not valid. */
	uint32_t fragment_nr_offset;	/**< offset for frame numbering, for sequences, where the
					 * provided fragment number of the first fragment does
					 * not start with 0 */
	uint32_t datalen;		/**< When flags&FD_BLOCKSEQUENCE is set, the
					 * index of the last block (segments in
					 * datagram + 1); otherwise the number of
					 * bytes of the full datagram. Only valid in
					 * the first item of the fragments list when
					 * flags&FD_DATALEN is set.*/
	uint32_t reassembled_in;		/**< frame where this PDU was reassembled,
					 * only valid when FD_DEFRAGMENTED is set */
	uint8_t reas_in_layer_num;	/**< The current "depth" or layer number in the current
					 * frame where reassembly was completed.
					 * Example: in SCTP there can be several data chunks and
					 * we want the reassembled tvb for the final segment only. */
	uint32_t flags;			/**< XXX - do some of these apply only to reassembly
					 * heads and others only to fragments within
					 * a reassembly? */
	tvbuff_t *tvb_data;
	const char *error;
} fragment_head;

#define REASSEMBLE_FLAGS_NO_FRAG_NUMBER		0x0001

#define REASSEMBLE_FLAGS_802_11_HACK		0x0002

#define REASSEMBLE_FLAGS_AGING  0x0001

typedef void * (*fragment_temporary_key)(const packet_info *pinfo,
    const uint32_t id, const void *data);

typedef void * (*fragment_persistent_key)(const packet_info *pinfo,
    const uint32_t id, const void *data);

typedef struct {
	GHashTable *fragment_table;
	GHashTable *reassembled_table;
	fragment_temporary_key temporary_key_func;
	fragment_persistent_key persistent_key_func;
	GDestroyNotify free_temporary_key_func;		/* temporary key destruction function */
} reassembly_table;

typedef struct {
	GHashFunc hash_func;				/* hash function */
	GEqualFunc equal_func;				/* comparison function */
	fragment_temporary_key temporary_key_func;	/* temporary key creation function */
	fragment_persistent_key persistent_key_func;	/* persistent key creation function */
	GDestroyNotify free_temporary_key_func;		/* temporary key destruction function */
	GDestroyNotify free_persistent_key_func;	/* persistent key destruction function */
} reassembly_table_functions;

extern const reassembly_table_functions addresses_reassembly_table_functions;
extern const reassembly_table_functions addresses_ports_reassembly_table_functions;
void reassembly_table_register(reassembly_table *table,
		      const reassembly_table_functions *funcs);
void reassembly_table_init(reassembly_table *table,
		      const reassembly_table_functions *funcs);
void reassembly_table_destroy(reassembly_table *table);

fragment_head * fragment_add(reassembly_table *table, tvbuff_t *tvb, const int offset,
	     const packet_info *pinfo, const uint32_t id, const void *data,
	     const uint32_t frag_offset, const uint32_t frag_data_len,
	     const bool more_frags);
fragment_head * fragment_add_multiple_ok(reassembly_table *table, tvbuff_t *tvb,
			 const int offset, const packet_info *pinfo,
			 const uint32_t id, const void *data,
			 const uint32_t frag_offset,
			 const uint32_t frag_data_len,
			 const bool more_frags);

fragment_head * fragment_add_out_of_order(reassembly_table *table, tvbuff_t *tvb,
                          const int offset, const packet_info *pinfo,
                          const uint32_t id, const void *data,
                          const uint32_t frag_offset,
                          const uint32_t frag_data_len,
                          const bool more_frags, const uint32_t frag_frame);
fragment_head * fragment_add_check(reassembly_table *table, tvbuff_t *tvb, const int offset,
		   const packet_info *pinfo, const uint32_t id,
		   const void *data, const uint32_t frag_offset,
		   const uint32_t frag_data_len, const bool more_frags);

fragment_head * fragment_add_check_with_fallback(reassembly_table *table, tvbuff_t *tvb, const int offset,
		   const packet_info *pinfo, const uint32_t id,
		   const void *data, const uint32_t frag_offset,
		   const uint32_t frag_data_len, const bool more_frags,
		   const uint32_t fallback_frame);

fragment_head * fragment_add_seq(reassembly_table *table, tvbuff_t *tvb, const int offset,
		 const packet_info *pinfo, const uint32_t id, const void *data,
		 const uint32_t frag_number, const uint32_t frag_data_len,
		 const bool more_frags, const uint32_t flags);

fragment_head * fragment_add_seq_check(reassembly_table *table, tvbuff_t *tvb, const int offset,
		       const packet_info *pinfo, const uint32_t id,
		       const void *data,
		       const uint32_t frag_number, const uint32_t frag_data_len,
		       const bool more_frags);

fragment_head * fragment_add_seq_802_11(reassembly_table *table, tvbuff_t *tvb,
			const int offset, const packet_info *pinfo,
			const uint32_t id, const void *data,
			const uint32_t frag_number, const uint32_t frag_data_len,
			const bool more_frags);

fragment_head * fragment_add_seq_next(reassembly_table *table, tvbuff_t *tvb, const int offset,
		      const packet_info *pinfo, const uint32_t id,
		      const void *data, const uint32_t frag_data_len,
		      const bool more_frags);

fragment_head * fragment_add_seq_single(reassembly_table *table, tvbuff_t *tvb,
            const int offset, const packet_info *pinfo, const uint32_t id,
            const void* data, const uint32_t frag_data_len,
            const bool first, const bool last,
            const uint32_t max_frags);

fragment_head * fragment_add_seq_single_aging(reassembly_table *table, tvbuff_t *tvb,
            const int offset, const packet_info *pinfo, const uint32_t id,
            const void* data, const uint32_t frag_data_len,
            const bool first, const bool last,
            const uint32_t max_frags, const uint32_t max_age);

void fragment_start_seq_check(reassembly_table *table, const packet_info *pinfo,
			 const uint32_t id, const void *data,
			 const uint32_t tot_len);

fragment_head * fragment_end_seq_next(reassembly_table *table, const packet_info *pinfo,
		      const uint32_t id, const void *data);

void fragment_add_seq_offset(reassembly_table *table, const packet_info *pinfo, const uint32_t id,
                    const void *data, const uint32_t fragment_offset);

void fragment_set_tot_len(reassembly_table *table, const packet_info *pinfo,
		     const uint32_t id, const void *data, const uint32_t tot_len);

void fragment_reset_tot_len(reassembly_table *table, const packet_info *pinfo,
		       const uint32_t id, const void *data, const uint32_t tot_len);

void fragment_truncate(reassembly_table *table, const packet_info *pinfo,
		       const uint32_t id, const void *data, const uint32_t tot_len);

uint32_t fragment_get_tot_len(reassembly_table *table, const packet_info *pinfo,
		     const uint32_t id, const void *data);

void fragment_set_partial_reassembly(reassembly_table *table,
				const packet_info *pinfo, const uint32_t id,
				const void *data);

fragment_head * fragment_get(reassembly_table *table, const packet_info *pinfo,
	     const uint32_t id, const void *data);

fragment_head * fragment_get_reassembled_id(reassembly_table *table, const packet_info *pinfo, const uint32_t id);

tvbuff_t * fragment_delete(reassembly_table *table, const packet_info *pinfo,
		const uint32_t id, const void *data);

typedef struct _fragment_items {
    int        *ett_fragment;
    int        *ett_fragments;

    int        *hf_fragments;                  /* FT_NONE     */
    int        *hf_fragment;                   /* FT_FRAMENUM */
    int        *hf_fragment_overlap;           /* FT_BOOLEAN  */
    int        *hf_fragment_overlap_conflict;  /* FT_BOOLEAN  */
    int        *hf_fragment_multiple_tails;    /* FT_BOOLEAN  */
    int        *hf_fragment_too_long_fragment; /* FT_BOOLEAN  */
    int        *hf_fragment_error;             /* FT_FRAMENUM */
    int        *hf_fragment_count;             /* FT_UINT32   */
    int        *hf_reassembled_in;             /* FT_FRAMENUM */
    int        *hf_reassembled_length;         /* FT_UINT32   */
    int        *hf_reassembled_data;           /* FT_BYTES    */

    const char *tag;
} fragment_items;

tvbuff_t * process_reassembled_data(tvbuff_t *tvb, const int offset, packet_info *pinfo,
    const char *name, fragment_head *fd_head, const fragment_items *fit,
    bool *update_col_infop, proto_tree *tree);

bool show_fragment_tree(fragment_head *ipfd_head, const fragment_items *fit,
    proto_tree *tree, packet_info *pinfo, tvbuff_t *tvb, proto_item **fi);

bool show_fragment_seq_tree(fragment_head *ipfd_head, const fragment_items *fit,
    proto_tree *tree, packet_info *pinfo, tvbuff_t *tvb, proto_item **fi);

extern void reassembly_tables_init(void);

extern void
reassembly_table_cleanup(void);

#define REASSEMBLE_ITEMS_DEFINE(var_prefix, name_prefix) \
    static int ett_##var_prefix##_fragment; \
    static int ett_##var_prefix##_fragments; \
    static int hf_##var_prefix##_fragments; \
    static int hf_##var_prefix##_fragment; \
    static int hf_##var_prefix##_fragment_overlap; \
    static int hf_##var_prefix##_fragment_overlap_conflicts; \
    static int hf_##var_prefix##_fragment_multiple_tails; \
    static int hf_##var_prefix##_fragment_too_long_fragment; \
    static int hf_##var_prefix##_fragment_error; \
    static int hf_##var_prefix##_fragment_count; \
    static int hf_##var_prefix##_reassembled_in; \
    static int hf_##var_prefix##_reassembled_length; \
    static int hf_##var_prefix##_reassembled_data; \
    static int hf_##var_prefix##_segment; \
    static const fragment_items var_prefix##_fragment_items = { \
        &ett_##var_prefix##_fragment, \
        &ett_##var_prefix##_fragments, \
        &hf_##var_prefix##_fragments, \
        &hf_##var_prefix##_fragment, \
        &hf_##var_prefix##_fragment_overlap, \
        &hf_##var_prefix##_fragment_overlap_conflicts, \
        &hf_##var_prefix##_fragment_multiple_tails, \
        &hf_##var_prefix##_fragment_too_long_fragment, \
        &hf_##var_prefix##_fragment_error, \
        &hf_##var_prefix##_fragment_count, \
        &hf_##var_prefix##_reassembled_in, \
        &hf_##var_prefix##_reassembled_length, \
        &hf_##var_prefix##_reassembled_data, \
        name_prefix " fragments" \
    }

#define REASSEMBLE_INIT_HF_ITEMS(var_prefix, name_prefix, abbrev_prefix) \
	    { &hf_##var_prefix##_fragments, \
            { "Reassembled " name_prefix " fragments", abbrev_prefix ".fragments", \
                FT_NONE, BASE_NONE, NULL, 0x0, NULL, HFILL } \
        }, \
        { &hf_##var_prefix##_fragment, \
            { name_prefix " fragment", abbrev_prefix ".fragment", \
                FT_FRAMENUM, BASE_NONE, NULL, 0x0, NULL, HFILL } \
        }, \
        { &hf_##var_prefix##_fragment_overlap, \
            { name_prefix " fragment overlap", abbrev_prefix ".fragment.overlap", \
                FT_BOOLEAN, BASE_NONE, NULL, 0x0, NULL, HFILL } \
        }, \
        { &hf_##var_prefix##_fragment_overlap_conflicts, \
            { name_prefix " fragment overlapping with conflicting data", abbrev_prefix ".fragment.overlap.conflicts", \
                FT_BOOLEAN, BASE_NONE, NULL, 0x0, NULL, HFILL } \
        }, \
        { &hf_##var_prefix##_fragment_multiple_tails, \
            { name_prefix " has multiple tail fragments", abbrev_prefix ".fragment.multiple_tails", \
                FT_BOOLEAN, BASE_NONE, NULL, 0x0, NULL, HFILL } \
        }, \
        { &hf_##var_prefix##_fragment_too_long_fragment, \
            { name_prefix " fragment too long", abbrev_prefix ".fragment.too_long_fragment", \
                FT_BOOLEAN, BASE_NONE, NULL, 0x0, NULL, HFILL } \
        }, \
        { &hf_##var_prefix##_fragment_error, \
            { name_prefix " defragment error", abbrev_prefix ".fragment.error", \
                FT_FRAMENUM, BASE_NONE, NULL, 0x0, NULL, HFILL } \
        }, \
        { &hf_##var_prefix##_fragment_count, \
            { name_prefix " fragment count", abbrev_prefix ".fragment.count", \
                FT_UINT32, BASE_DEC, NULL, 0x0, NULL, HFILL } \
        }, \
        { &hf_##var_prefix##_reassembled_in, \
            { "Reassembled in", abbrev_prefix ".reassembled.in", \
                FT_FRAMENUM, BASE_NONE, NULL, 0x0, NULL, HFILL } \
        }, \
        { &hf_##var_prefix##_reassembled_length, \
            { "Reassembled length", abbrev_prefix ".reassembled.length", \
                FT_UINT32, BASE_DEC, NULL, 0x0, NULL, HFILL } \
        }, \
        { &hf_##var_prefix##_reassembled_data, \
            { "Reassembled data", abbrev_prefix ".reassembled.data", \
                FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL } \
        }, \
        { &hf_##var_prefix##_segment, \
            { name_prefix " segment", abbrev_prefix ".segment", \
                FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL} \
        }

#define REASSEMBLE_INIT_ETT_ITEMS(var_prefix) \
        &ett_##var_prefix##_fragment, \
        &ett_##var_prefix##_fragments

typedef struct streaming_reassembly_info_t streaming_reassembly_info_t;

streaming_reassembly_info_t* streaming_reassembly_info_new(void);

int reassemble_streaming_data_and_call_subdissector(
	tvbuff_t* tvb, packet_info* pinfo, unsigned offset, int length,
	proto_tree* segment_tree, proto_tree* reassembled_tree, reassembly_table streaming_reassembly_table,
	streaming_reassembly_info_t* reassembly_info, uint64_t cur_frame_num,
	dissector_handle_t subdissector_handle, proto_tree* subdissector_tree, void* subdissector_data,
	const char* label, const fragment_items* frag_hf_items, int hf_segment_data
);

static inline uint64_t
get_virtual_frame_num64(tvbuff_t* tvb, packet_info* pinfo, int offset)
{
	return (((uint64_t)pinfo->num) << 32) + (((uint64_t)pinfo->curr_layer_num) << 24)
		+ ((uint64_t)tvb_raw_offset(tvb) + offset);
}

int additional_bytes_expected_to_complete_reassembly(streaming_reassembly_info_t* reassembly_info);


#endif
