/** @file
 *
 * Definitions for file structures and routines
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __FILE_H__
#define __FILE_H__

#include <wiretap/wtap.h>
#include <epan/epan.h>
#include <epan/fifo_string_cache.h>
#include <ui/packet_range.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef enum {
    CF_OK,      /**< operation succeeded */
    CF_ERROR    /**< operation got an error (function may provide err with details) */
} cf_status_t;

typedef enum {
    CF_READ_OK,      /**< operation succeeded */
    CF_READ_ERROR,   /**< operation got an error (function may provide err with details) */
    CF_READ_ABORTED  /**< operation aborted by user */
} cf_read_status_t;

typedef enum {
    CF_WRITE_OK,      /**< operation succeeded */
    CF_WRITE_ERROR,   /**< operation got an error (function may provide err with details) */
    CF_WRITE_ABORTED  /**< operation aborted by user */
} cf_write_status_t;

typedef enum {
    CF_PRINT_OK,            /**< print operation succeeded */
    CF_PRINT_OPEN_ERROR,    /**< print operation failed while opening printer */
    CF_PRINT_WRITE_ERROR    /**< print operation failed while writing to the printer */
} cf_print_status_t;

typedef enum {
    cf_cb_file_opened,
    cf_cb_file_closing,
    cf_cb_file_closed,
    cf_cb_file_read_started,
    cf_cb_file_read_finished,
    cf_cb_file_reload_started,
    cf_cb_file_reload_finished,
    cf_cb_file_rescan_started,
    cf_cb_file_rescan_finished,
    cf_cb_file_retap_started,
    cf_cb_file_retap_finished,
    cf_cb_file_merge_started, /* Qt only */
    cf_cb_file_merge_finished, /* Qt only */
    cf_cb_file_fast_save_finished,
    cf_cb_file_save_started,
    cf_cb_file_save_finished,
    cf_cb_file_save_failed,
    cf_cb_file_save_stopped
} cf_cbs;

typedef void (*cf_callback_t) (int event, void *data, void *user_data);

typedef struct {
    const char    *string;
    size_t         string_len;
    capture_file  *cf;
    field_info    *finfo;
    field_info    *prev_finfo;
    bool           frame_matched;
    bool           halt;
} match_data;

int test_for_directory(const char *path);
char *get_dirname(char *path);
const char *get_basename(const char *path);
char *find_last_pathname_separator(const char *path);
bool file_exists(const char *fname);
bool copy_file_binary_mode(const char *from_filename, const char *to_filename);



extern void cf_set_max_records(unsigned max_records);
extern void cf_callback_add(cf_callback_t func, void *user_data);
extern void cf_callback_remove(cf_callback_t func, void *user_data);
cf_status_t cf_open(capture_file *cf, const char *fname, unsigned int type, bool is_tempfile, int *err);
void cf_close(capture_file *cf);
cf_read_status_t cf_read(capture_file *cf, bool reloading);
bool cf_read_record(capture_file *cf, const frame_data *fdata, wtap_rec *rec, Buffer *buf);
bool cf_read_record_no_alert(capture_file *cf, const frame_data *fdata, wtap_rec *rec, Buffer *buf);
bool cf_read_current_record(capture_file *cf);
cf_read_status_t cf_continue_tail(capture_file *cf, volatile int to_read,
                                  wtap_rec *rec, Buffer *buf, int *err,
                                  fifo_string_cache_t *frame_dup_cache, GChecksum *frame_cksum);
void cf_fake_continue_tail(capture_file *cf);
cf_read_status_t cf_finish_tail(capture_file *cf, wtap_rec *rec,
                                Buffer *buf, int *err,
                                fifo_string_cache_t *frame_dup_cache, GChecksum *frame_cksum);
bool cf_can_write_with_wiretap(capture_file *cf);
bool cf_can_save(capture_file *cf);
bool cf_can_save_as(capture_file *cf);
bool cf_has_unsaved_data(capture_file *cf);

cf_write_status_t cf_save_records(capture_file * cf, const char *fname, unsigned save_format);


char *cf_get_display_name(capture_file *cf);
char *cf_get_basename(capture_file *cf);
void cf_set_tempfile_source(capture_file *cf, char *source);
const char *cf_get_tempfile_source(capture_file *cf);
int cf_get_packet_count(capture_file *cf);
bool cf_is_tempfile(capture_file *cf);
void cf_set_tempfile(capture_file *cf, bool is_tempfile);
void cf_set_drops_known(capture_file *cf, bool drops_known);
void cf_set_drops(capture_file *cf, uint32_t drops);
bool cf_get_drops_known(capture_file *cf);
uint32_t cf_get_drops(capture_file *cf);
void cf_set_rfcode(capture_file *cf, dfilter_t *rfcode);
cf_status_t cf_filter_packets(capture_file *cf, char *dfilter, bool force);
void cf_reftime_packets(capture_file *cf);
unsigned long cf_get_computed_elapsed(capture_file *cf);
void cf_redissect_packets(capture_file *cf);
cf_read_status_t cf_retap_packets(capture_file *cf);
typedef enum {
    print_range_selected_only,    /* selected frame(s) only (currently only one) */
    print_range_marked_only,      /* marked frames only */
    print_range_all_displayed,    /* all frames currently displayed */
    print_range_all_captured      /* all frames in capture */
} print_range_e;

typedef struct {
//    print_stream_t *stream;       /* the stream to which we're printing */
//    print_format_e format;        /* plain text or PostScript */
    bool to_file;             /* true if we're printing to a file */
    char *file;                   /* file output pathname */
    char *cmd;                    /* print command string */
    packet_range_t range;

    bool print_summary;       /* true if we should print summary line. */
    bool print_col_headings;  /* true if we should print column headings */
//    print_dissections_e print_dissections;
    bool print_hex;           /* true if we should print hex data;
                                   * false if we should print only if not dissected. */
    unsigned hexdump_options;        /* Hexdump options if print_hex is true. */
    bool print_formfeed;      /* true if a formfeed should be printed before
                                   * each new packet */
} print_args_t;

cf_print_status_t cf_print_packets(capture_file *cf, print_args_t *print_args,
                                   bool show_progress_bar);
bool cf_find_packet_protocol_tree(capture_file *cf, const char *string,
                                      search_direction dir, bool multiple);

/**
 * Find field with a label that contains the text string cfile->sfilter in
 * a protocol tree.
 *
 * @param cf the capture file
 * @param tree the protocol tree
 * @return The first field in the tree that matched the string if found, NULL otherwise
 */
extern field_info* cf_find_string_protocol_tree(capture_file *cf, proto_tree *tree);

/**
 * Find packet whose summary line contains a specified text string.
 *
 * @param cf the capture file
 * @param string the string to find
 * @param dir direction in which to search
 * @return true if a packet was found, false otherwise
 */
bool cf_find_packet_summary_line(capture_file *cf, const char *string,
                                     search_direction dir);

/**
 * Find packet whose data contains a specified byte string.
 *
 * @param cf the capture file
 * @param string the string to find
 * @param string_size the size of the string to find
 * @param dir direction in which to search
 * @param multiple whether to look for the next occurrence of the same string
 * in the current packet, or to only match once per frame
 * @return true if a packet was found, false otherwise
 */
bool cf_find_packet_data(capture_file *cf, const uint8_t *string,
                             size_t string_size, search_direction dir,
                             bool multiple);

/**
 * Find packet that matches a compiled display filter.
 *
 * @param cf the capture file
 * @param sfcode the display filter to match
 * @param dir direction in which to search
 * @return true if a packet was found, false otherwise
 */
bool cf_find_packet_dfilter(capture_file *cf, dfilter_t *sfcode,
                                search_direction dir);

/**
 * Find packet that matches a display filter given as a text string.
 *
 * @param cf the capture file
 * @param filter the display filter to match
 * @param dir direction in which to search
 * @return true if a packet was found, false otherwise
 */
bool
cf_find_packet_dfilter_string(capture_file *cf, const char *filter,
                              search_direction dir);

/**
 * Find marked packet.
 *
 * @param cf the capture file
 * @param dir direction in which to search
 * @return true if a packet was found, false otherwise
 */
bool cf_find_packet_marked(capture_file *cf, search_direction dir);

/**
 * Find time-reference packet.
 *
 * @param cf the capture file
 * @param dir direction in which to search
 * @return true if a packet was found, false otherwise
 */
bool cf_find_packet_time_reference(capture_file *cf, search_direction dir);

/**
 * GoTo Packet with the given row.
 *
 * @param cf the capture file
 * @param row the row to go to
 * @param exact if true, fail if the row exists and is filtered (not displayed)
 * if false, go to the nearest displayed packet instead
 * @return true if this row exists, false otherwise
 */
bool cf_goto_frame(capture_file *cf, unsigned row, bool exact);

/**
 * Go to frame specified by currently selected protocol tree field.
 * (Go To Corresponding Packet)
 * @todo this is ugly and should be improved!
 *
 * @param cf the capture file
 * @return true if this packet exists, false otherwise
 */
bool cf_goto_framenum(capture_file *cf);

/**
 * Select the packet in the given row.
 *
 * @param cf the capture file
 * @param frame the frame to be selected
 */
void cf_select_packet(capture_file *cf, frame_data *frame);

/**
 * Unselect all packets, if any.
 *
 * @param cf the capture file
 */
void cf_unselect_packet(capture_file *cf);

/**
 * Mark a particular frame in a particular capture.
 *
 * @param cf the capture file
 * @param frame the frame to be marked
 */
void cf_mark_frame(capture_file *cf, frame_data *frame);

/**
 * Unmark a particular frame in a particular capture.
 *
 * @param cf the capture file
 * @param frame the frame to be unmarked
 */
void cf_unmark_frame(capture_file *cf, frame_data *frame);

/**
 * Ignore a particular frame in a particular capture.
 *
 * @param cf the capture file
 * @param frame the frame to be ignored
 */
void cf_ignore_frame(capture_file *cf, frame_data *frame);

/**
 * Unignore a particular frame in a particular capture.
 *
 * @param cf the capture file
 * @param frame the frame to be unignored
 */
void cf_unignore_frame(capture_file *cf, frame_data *frame);

void cf_update_section_comment(capture_file *cf, char *comment);

/**
 * Update(replace) the comments on a capture from the SHB data block
 *
 * @param cf the capture file
 * @param shb_idx the index of the SHB (0-indexed)
 * @param comments a NULL-terminated string array of comments. The function
 * takes ownership of the string array and frees it and the contents.
 */
void cf_update_section_comments(capture_file *cf, unsigned shb_idx, char **comments);

/*
 * Get the packet block for a packet (record).
 * If the block has been edited, it returns the result of the edit,
 * otherwise it returns the block from the file.
 *
 * @param cf the capture file
 * @param fd the frame_data structure for the frame
 * @returns A block (use wtap_block_unref to free) or NULL if there is none.
 */
wtap_block_t cf_get_packet_block(capture_file *cf, const frame_data *fd);

/**
 * Update(replace) the block on a capture from a frame
 *
 * @param cf the capture file
 * @param fd the frame_data structure for the frame
 * @param new_block the block replacing the old block
 *
 * @return true if the block is modified for the first time. false if
 * the block was already modified before, in which case the caller is
 * responsible for updating the comment count.
 */
bool cf_set_modified_block(capture_file *cf, frame_data *fd, const wtap_block_t new_block);

/**
 * What types of comments does this file have?
 *
 * @param cf the capture file
 * @return bitset of WTAP_COMMENT_ values
 */
uint32_t cf_comment_types(capture_file *cf);

/**
 * Add a resolved address to this file's list of resolved addresses.
 *
 * @param cf the capture file
 * @param addr a string representing an IPv4 or IPv6 address
 * @param name a string containing a name corresponding to that address
 * @return true if it succeeds, false if not
 */
bool cf_add_ip_name_from_string(capture_file *cf, const char *addr, const char *name);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* file.h */
