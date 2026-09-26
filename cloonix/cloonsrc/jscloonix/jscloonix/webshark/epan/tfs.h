/* tfs.h
 * true_false strings
 * Copyright 2007, Jaap Keuter <jaap.keuter@xs4all.nl>
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __TFS_H__
#define __TFS_H__

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


typedef struct true_false_string {
    const char *true_string;   /**< The string presented when true  */
    const char *false_string;  /**< The string presented when false */
} true_false_string;

const char *tfs_get_string(bool value, const true_false_string *tfs);

#define tfs_get_true_false(value)   tfs_get_string(value, NULL)

extern const true_false_string tfs_yes_no;
extern const true_false_string tfs_no_yes;
extern const true_false_string tfs_set_notset;
extern const true_false_string tfs_enabled_disabled;
extern const true_false_string tfs_disabled_enabled;
extern const true_false_string tfs_ok_error;
extern const true_false_string tfs_error_ok;
extern const true_false_string tfs_success_fail;
extern const true_false_string tfs_fail_success;
extern const true_false_string tfs_on_off;
extern const true_false_string tfs_off_on;
extern const true_false_string tfs_ack_nack;
extern const true_false_string tfs_odd_even;
extern const true_false_string tfs_allow_block;
extern const true_false_string tfs_restricted_not_restricted;
extern const true_false_string tfs_not_restricted_restricted;
extern const true_false_string tfs_accept_reject;
extern const true_false_string tfs_more_nomore;
extern const true_false_string tfs_present_absent;
extern const true_false_string tfs_present_not_present;
extern const true_false_string tfs_active_inactive;
extern const true_false_string tfs_activated_deactivated;
extern const true_false_string tfs_found_not_found;
extern const true_false_string tfs_command_response;
extern const true_false_string tfs_response_command;
extern const true_false_string tfs_capable_not_capable;
extern const true_false_string tfs_supported_not_supported;
extern const true_false_string tfs_not_supported_supported;
extern const true_false_string tfs_used_notused;
extern const true_false_string tfs_high_low;
extern const true_false_string tfs_high_normal;
extern const true_false_string tfs_low_normal;
extern const true_false_string tfs_pressed_not_pressed;
extern const true_false_string tfs_implemented_not_implemented;
extern const true_false_string tfs_requested_not_requested;
extern const true_false_string tfs_reliable_not_reliable;
extern const true_false_string tfs_allowed_not_allowed;
extern const true_false_string tfs_not_allowed_allowed;
extern const true_false_string tfs_accepted_not_accepted;
extern const true_false_string tfs_detected_not_detected;
extern const true_false_string tfs_available_not_available;
extern const true_false_string tfs_shared_independent;
extern const true_false_string tfs_valid_invalid;
extern const true_false_string tfs_invalid_valid;
extern const true_false_string tfs_group_unique_name;
extern const true_false_string tfs_inuse_not_inuse;
extern const true_false_string tfs_critical_not_critical;
extern const true_false_string tfs_complete_incomplete;
extern const true_false_string tfs_valid_not_valid;
extern const true_false_string tfs_do_not_clear_clear;
extern const true_false_string tfs_confirmed_unconfirmed;
extern const true_false_string tfs_enforced_not_enforced;
extern const true_false_string tfs_possible_not_possible;
extern const true_false_string tfs_required_not_required;
extern const true_false_string tfs_registered_not_registered;
extern const true_false_string tfs_provisioned_not_provisioned;
extern const true_false_string tfs_included_not_included;
extern const true_false_string tfs_allocated_by_receiver_sender;
extern const true_false_string tfs_asynchronous_synchronous;
extern const true_false_string tfs_protocol_sensative_bit_transparent;
extern const true_false_string tfs_full_half;
extern const true_false_string tfs_acknowledged_not_acknowledged;
extern const true_false_string tfs_segmentation_no_segmentation;
extern const true_false_string tfs_response_request;
extern const true_false_string tfs_defined_not_defined;
extern const true_false_string tfs_constructed_primitive;
extern const true_false_string tfs_client_server;
extern const true_false_string tfs_server_client;
extern const true_false_string tfs_preferred_no_preference;
extern const true_false_string tfs_encrypt_do_not_encrypt;
extern const true_false_string tfs_down_up;
extern const true_false_string tfs_up_down;
extern const true_false_string tfs_uplink_downlink;
extern const true_false_string tfs_s2c_c2s;
extern const true_false_string tfs_open_closed;
extern const true_false_string tfs_external_internal;
extern const true_false_string tfs_changed_not_changed;
extern const true_false_string tfs_needed_not_needed;
extern const true_false_string tfs_selected_not_selected;
extern const true_false_string tfs_add_drop;
extern const true_false_string tfs_no_extension_extension;
extern const true_false_string tfs_user_provider;
extern const true_false_string tfs_applicable_not_applicable;
extern const true_false_string tfs_current_not_yet;
extern const true_false_string tfs_should_be_traced_should_not_be_traced;
extern const true_false_string tfs_activate_do_not_activate;
extern const true_false_string tfs_data_pdu_control_pdu;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __TFS_H__ */

