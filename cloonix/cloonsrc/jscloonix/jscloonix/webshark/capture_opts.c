/* capture_opts.c
 * Routines for capture options setting
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <config.h>
#include <wireshark.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <ws_exit_codes.h>
#include "capture_opts.h"
#include "ringbuffer.h"
#include <wsutil/clopts_common.h>
#include <wsutil/cmdarg_err.h>
#include <wsutil/file_util.h>
#include <wsutil/ws_pipe.h>
#include <wsutil/ws_assert.h>


char *cloonix_pipe_name;
char *cloonix_pipe_title;

static void set_cloonix_pipe_title(const char *pipe_title)
{
    cloonix_pipe_title = g_strdup(pipe_title);
}
static void set_cloonix_pipe_name(const char *pipe_name)
{
    cloonix_pipe_name = g_strdup(pipe_name);
}
char *get_cloonix_pipe_title(void)
{   
  return cloonix_pipe_title;
}      
char *get_cloonix_pipe_name(void)
{   
  return cloonix_pipe_name;
}      

void
capture_opts_init(capture_options *capture_opts)
{
    capture_opts->ifaces                          = g_array_new(FALSE, FALSE, sizeof(interface_options));
    capture_opts->all_ifaces                      = g_array_new(FALSE, FALSE, sizeof(interface_t));
    capture_opts->num_selected                    = 0;
    capture_opts->default_options.name            = NULL;
    capture_opts->default_options.descr           = NULL;
    capture_opts->default_options.ifname          = NULL;
    capture_opts->default_options.hardware        = NULL;
    capture_opts->default_options.display_name    = NULL;
    capture_opts->default_options.cfilter         = NULL;
    capture_opts->default_options.has_snaplen     = false;
    capture_opts->default_options.snaplen         = WTAP_MAX_PACKET_SIZE_STANDARD;
    capture_opts->default_options.linktype        = -1; /* use interface default */
    capture_opts->default_options.promisc_mode    = true;
    capture_opts->default_options.if_type         = IF_WIRED;
    capture_opts->default_options.extcap          = NULL;
    capture_opts->default_options.extcap_fifo     = NULL;
    capture_opts->default_options.extcap_args     = NULL;
    capture_opts->default_options.extcap_pid      = WS_INVALID_PID;
    capture_opts->default_options.extcap_pipedata = NULL;
    capture_opts->default_options.extcap_stderr   = NULL;
    capture_opts->default_options.extcap_stdout_watch = 0;
    capture_opts->default_options.extcap_stderr_watch = 0;
    capture_opts->default_options.extcap_control_in  = NULL;
    capture_opts->default_options.extcap_control_out = NULL;
    capture_opts->default_options.buffer_size     = DEFAULT_CAPTURE_BUFFER_SIZE;
    capture_opts->default_options.monitor_mode    = false;
    capture_opts->default_options.timestamp_type  = NULL;
    capture_opts->saving_to_file                  = false;
    capture_opts->save_file                       = NULL;
    capture_opts->group_read_access               = false;
    capture_opts->use_pcapng                      = true;
    capture_opts->update_interval                 = DEFAULT_UPDATE_INTERVAL; /* 100 ms */
    capture_opts->real_time_mode                  = true;
    capture_opts->show_info                       = true;
    capture_opts->restart                         = false;
    capture_opts->orig_save_file                  = NULL;

    capture_opts->has_file_duration               = false;
    capture_opts->file_duration                   = 60.0;             /* 1 min */
    capture_opts->has_file_interval               = false;
    capture_opts->has_nametimenum                 = false;
    capture_opts->file_interval                   = 60;               /* 1 min */
    capture_opts->has_file_packets                = false;
    capture_opts->file_packets                    = 0;
    capture_opts->has_ring_num_files              = false;
    capture_opts->ring_num_files                  = RINGBUFFER_MIN_NUM_FILES;

    capture_opts->has_autostop_files              = false;
    capture_opts->autostop_files                  = 1;
    capture_opts->has_autostop_packets            = false;
    capture_opts->autostop_packets                = 0;
    capture_opts->has_autostop_written_packets    = false;
    capture_opts->autostop_written_packets        = 0;
    capture_opts->has_autostop_filesize           = false;
    capture_opts->autostop_filesize               = 1000;             /* 1 MB */
    capture_opts->has_autostop_duration           = false;
    capture_opts->autostop_duration               = 60.0;             /* 1 min */

    capture_opts->output_to_pipe                  = false;
    capture_opts->capture_child                   = true;
    capture_opts->stop_after_extcaps              = false;
    capture_opts->wait_for_extcap_cbs             = false;
    capture_opts->print_file_names                = false;
    capture_opts->print_name_to                   = NULL;
    capture_opts->temp_dir                        = NULL;
    capture_opts->compress_type                   = NULL;
    capture_opts->closed_msg                      = NULL;
    capture_opts->extcap_terminate_id             = 0;
    capture_opts->capture_filters_list            = NULL;
}


static char * capture_opts_generate_display_name(const char *friendly_name,
                                                 const char *name)
{
    return ws_strdup_printf("%s: %s", friendly_name, name);
}

static void fill_in_interface_opts_defaults(interface_options *interface_opts,
                                            const capture_options *capture_opts)
{
    interface_opts->cfilter = g_strdup(capture_opts->default_options.cfilter);
    interface_opts->snaplen = capture_opts->default_options.snaplen;
    interface_opts->has_snaplen = capture_opts->default_options.has_snaplen;
    interface_opts->linktype = capture_opts->default_options.linktype;
    interface_opts->promisc_mode = capture_opts->default_options.promisc_mode;
    interface_opts->extcap_fifo = g_strdup(capture_opts->default_options.extcap_fifo);
    interface_opts->extcap_args = NULL;
    interface_opts->extcap_pid = WS_INVALID_PID;
    interface_opts->extcap_pipedata = NULL;
    interface_opts->extcap_stderr = NULL;
    interface_opts->extcap_stdout_watch = 0;
    interface_opts->extcap_stderr_watch = 0;
    interface_opts->extcap_control_in = g_strdup(capture_opts->default_options.extcap_control_in);
    interface_opts->extcap_control_out = g_strdup(capture_opts->default_options.extcap_control_out);
    interface_opts->buffer_size = capture_opts->default_options.buffer_size;
    interface_opts->monitor_mode = capture_opts->default_options.monitor_mode;
    interface_opts->timestamp_type  = capture_opts->default_options.timestamp_type;
}

static void fill_in_interface_opts_from_ifinfo(interface_options *interface_opts,
                                               const if_info_t *if_info)
{
    interface_opts->name = g_strdup(if_info->name);
    interface_opts->hardware = g_strdup(if_info->vendor_description);
    if (if_info->friendly_name != NULL) {
        interface_opts->descr = g_strdup(if_info->friendly_name);
        interface_opts->display_name = capture_opts_generate_display_name(if_info->friendly_name, if_info->name);
    } else {
        interface_opts->descr = NULL;
        interface_opts->display_name = g_strdup(if_info->name);
    }
    interface_opts->ifname = NULL;
    interface_opts->if_type = if_info->type;
    interface_opts->extcap = g_strdup(if_info->extcap);
}

static int capture_opts_add_iface_opt(capture_options *capture_opts, const char *optarg_str_p)
{
    GList       *if_list;
    int         err;
    char        *err_str;
    interface_options interface_opts;
    if (capture_opts->capture_child) {
    } else if (g_strcmp0(optarg_str_p, "-") == 0) {
        interface_opts.name = g_strdup(optarg_str_p);
        interface_opts.descr = g_strdup("Standard input");
        interface_opts.hardware = NULL;
        interface_opts.display_name = g_strdup(interface_opts.descr);
        interface_opts.ifname = NULL;
        interface_opts.if_type = IF_STDIN;
        interface_opts.extcap = g_strdup(capture_opts->default_options.extcap);
    }
    fill_in_interface_opts_defaults(&interface_opts, capture_opts);
    g_array_append_val(capture_opts->ifaces, interface_opts);
    return 0;
}


int capture_opts_add_opt(capture_options *capture_opts, int opt, const char *optarg_str_p)
{
    int status, snaplen;
    ws_statb64 fstat;
    switch(opt) {
    case 't':
        set_cloonix_pipe_title((const char *)optarg_str_p);
        break;
    case 'i':
        set_cloonix_pipe_name((const char *)optarg_str_p);
        break;
    default:
        ws_assert_not_reached();
    }
    return 0;
}

int capture_opts_print_if_capabilities(if_capabilities_t *caps,
                                   const interface_options *interface_opts,
                                   int queries)
{
    GList *lt_entry, *ts_entry;

    if (caps->primary_msg) {
        cmdarg_err("The capabilities of the capture device "
                   "\"%s\" could not be obtained (%s).%s%s",
                   interface_opts->name, caps->primary_msg,
                   caps->secondary_msg ? "\n" : "",
                   caps->secondary_msg ? caps->secondary_msg : "");
        return WS_EXIT_INVALID_CAPABILITY;
    }

    if (queries & CAPS_QUERY_LINK_TYPES) {
        if (caps->data_link_types == NULL) {
            cmdarg_err("The capture device \"%s\" has no data link types.",
                       interface_opts->name);
            return WS_EXIT_IFACE_HAS_NO_LINK_TYPES;
        }
        if (caps->can_set_rfmon)
            printf("Data link types of interface %s when not in monitor mode (use option -y to set):\n",
                   interface_opts->name);
        else
            printf("Data link types of interface %s (use option -y to set):\n",
                   interface_opts->name);
        if (caps->can_set_rfmon) {
            printf("Data link types of interface %s when in monitor mode (use option -y to set):\n",
                   interface_opts->name);
        }
    }

    if (queries & CAPS_QUERY_TIMESTAMP_TYPES) {
        if (caps->timestamp_types == NULL) {
            cmdarg_err("The capture device \"%s\" has no timestamp types.",
                       interface_opts->name);
            return WS_EXIT_IFACE_HAS_NO_TIMESTAMP_TYPES;
        }
    }
    return EXIT_SUCCESS;
}

void capture_opts_print_interfaces(GList *if_list)
{
    int         i;
    GList       *if_entry;
    if_info_t   *if_info;
    i = 1;
    for (if_entry = g_list_first(if_list); if_entry != NULL;
         if_entry = g_list_next(if_entry)) {
        if_info = (if_info_t *)if_entry->data;
        printf("%d. %s", i++, if_info->name);
        if (if_info->friendly_name != NULL){
            printf(" (%s)", if_info->friendly_name);
        } else {
            if (if_info->vendor_description != NULL)
                printf(" (%s)", if_info->vendor_description);
        }
        printf("\n");
    }
}


void capture_opts_trim_snaplen(capture_options *capture_opts, int snaplen_min)
{
    unsigned i;
    interface_options *interface_opts;

    if (capture_opts->ifaces->len > 0) {
        for (i = 0; i < capture_opts->ifaces->len; i++) {
            interface_opts = &g_array_index(capture_opts->ifaces, interface_options, 0);
            if (interface_opts->snaplen < 1)
                interface_opts->snaplen = WTAP_MAX_PACKET_SIZE_STANDARD;
            else if (interface_opts->snaplen < snaplen_min)
                interface_opts->snaplen = snaplen_min;
        }
    } else {
        if (capture_opts->default_options.snaplen < 1)
            capture_opts->default_options.snaplen = WTAP_MAX_PACKET_SIZE_STANDARD;
        else if (capture_opts->default_options.snaplen < snaplen_min)
            capture_opts->default_options.snaplen = snaplen_min;
    }
}


void capture_opts_trim_ring_num_files(capture_options *capture_opts)
{
    if (capture_opts->ring_num_files > RINGBUFFER_MAX_NUM_FILES) {
        cmdarg_err("Too many ring buffer files (%u). Reducing to %u.\n", capture_opts->ring_num_files, RINGBUFFER_MAX_NUM_FILES);
        capture_opts->ring_num_files = RINGBUFFER_MAX_NUM_FILES;
    } else if (capture_opts->ring_num_files > RINGBUFFER_WARN_NUM_FILES) {
        cmdarg_err("%u is a lot of ring buffer files.\n", capture_opts->ring_num_files);
    }
#if RINGBUFFER_MIN_NUM_FILES > 0
    else if (capture_opts->ring_num_files < RINGBUFFER_MIN_NUM_FILES) {
        cmdarg_err("Too few ring buffer files (%u). Increasing to %u.\n", capture_opts->ring_num_files, RINGBUFFER_MIN_NUM_FILES);
        capture_opts->ring_num_files = RINGBUFFER_MIN_NUM_FILES;
    }
#endif
}

int capture_opts_default_iface_if_necessary(capture_options *capture_opts,
                                        const char *capture_device)
{
    int status;
    if (capture_opts->num_selected != 0 || capture_opts->ifaces->len != 0) {
        return 0;
    }
    if (capture_device != NULL) {
        status = capture_opts_add_iface_opt(capture_opts, capture_device);
        return status;
    }
    return capture_opts_add_iface_opt(capture_opts, "1");
}

#ifndef S_IFIFO
#define S_IFIFO _S_IFIFO
#endif
#ifndef S_ISFIFO
#define S_ISFIFO(mode)  (((mode) & S_IFMT) == S_IFIFO)
#endif

static int
capture_opts_test_for_fifo(const char *path)
{
    ws_statb64 statb;

    if (ws_stat64(path, &statb) < 0)
        return errno;

    if (S_ISFIFO(statb.st_mode))
        return ESPIPE;
    else
        return 0;
}


void interface_opts_free(interface_options *interface_opts)
{
    if (interface_opts == NULL)
        return;

    g_free(interface_opts->name);
    g_free(interface_opts->descr);
    g_free(interface_opts->hardware);
    g_free(interface_opts->display_name);
    g_free(interface_opts->ifname);
    g_free(interface_opts->cfilter);
    g_free(interface_opts->timestamp_type);
    g_free(interface_opts->extcap);
    g_free(interface_opts->extcap_fifo);
    if (interface_opts->extcap_args)
        g_hash_table_unref(interface_opts->extcap_args);
    if (interface_opts->extcap_pid != WS_INVALID_PID)
        ws_warning("Extcap still running during interface delete");
    g_free(interface_opts->extcap_pipedata);
    if (interface_opts->extcap_stderr)
        g_string_free(interface_opts->extcap_stderr, TRUE);
    g_free(interface_opts->extcap_control_in);
    g_free(interface_opts->extcap_control_out);
}

void capture_opts_del_iface(capture_options *capture_opts, unsigned if_index)
{
    interface_options *interface_opts;
    interface_opts = &g_array_index(capture_opts->ifaces, interface_options, if_index);
    interface_opts_free(interface_opts);
    capture_opts->ifaces = g_array_remove_index(capture_opts->ifaces, if_index);
}

interface_options* interface_opts_from_if_info(capture_options *capture_opts, const if_info_t *if_info)
{
    interface_options *interface_opts = g_new(interface_options, 1);
    fill_in_interface_opts_from_ifinfo(interface_opts, if_info);
    fill_in_interface_opts_defaults(interface_opts, capture_opts);
    return interface_opts;
}

void collect_ifaces(capture_options *capture_opts)
{
    unsigned i;
    interface_t *device;
    interface_options interface_opts;
    for (i = capture_opts->ifaces->len; i != 0; i--)
        capture_opts_del_iface(capture_opts, i-1);
    for (i = 0; i < capture_opts->all_ifaces->len; i++) {
        device = &g_array_index(capture_opts->all_ifaces, interface_t, i);
        if (device->selected) {
            interface_opts.name = g_strdup(device->name);
            interface_opts.descr = g_strdup(device->if_info.friendly_name);
            interface_opts.ifname = NULL;
            interface_opts.hardware = g_strdup(device->if_info.vendor_description);
            interface_opts.display_name = g_strdup(device->display_name);
            interface_opts.linktype = device->active_dlt;
            interface_opts.cfilter = g_strdup(device->cfilter);
            interface_opts.timestamp_type = g_strdup(device->timestamp_type);
            interface_opts.snaplen = device->snaplen;
            interface_opts.has_snaplen = device->has_snaplen;
            interface_opts.promisc_mode = device->pmode;
            interface_opts.if_type = device->if_info.type;
            interface_opts.extcap = g_strdup(device->if_info.extcap);
            interface_opts.extcap_fifo = NULL;
            interface_opts.extcap_pipedata = NULL;
            interface_opts.extcap_args = device->external_cap_args_settings;
            interface_opts.extcap_pid = WS_INVALID_PID;
            if (interface_opts.extcap_args)
                g_hash_table_ref(interface_opts.extcap_args);
            interface_opts.extcap_pipedata = NULL;
            interface_opts.extcap_stderr = NULL;
            interface_opts.extcap_control_in = NULL;
            interface_opts.extcap_control_out = NULL;
            interface_opts.buffer_size =  device->buffer;
            g_array_append_val(capture_opts->ifaces, interface_opts);
        } else {
            continue;
        }
    }
}

void capture_opts_free_link_row(void *elem)
{
    link_row* e = (link_row*)elem;
    if (e != NULL)
        g_free(e->name);
    g_free(elem);
}

void capture_opts_free_interface_t(interface_t *device)
{
    if (device != NULL) {
        g_free(device->name);
        g_free(device->display_name);
        g_free(device->addresses);
        g_free(device->cfilter);
        g_free(device->timestamp_type);
        g_list_free_full(device->links, capture_opts_free_link_row);
        g_free(device->if_info.name);
        g_free(device->if_info.friendly_name);
        g_free(device->if_info.vendor_description);
        g_slist_free_full(device->if_info.addrs, g_free);
        g_free(device->if_info.extcap);
        if (device->external_cap_args_settings) {
                g_hash_table_unref(device->external_cap_args_settings);
        }
    }
}
