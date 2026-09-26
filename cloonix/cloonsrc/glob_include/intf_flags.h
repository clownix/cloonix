/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#include <stdint.h>
/* All four report the reason themselves; a caller adds context, not errno. */
int get_intf_flags_iff(char *intf, int *flags);
/* Silent, and the only one that is: the answer no is a normal answer. */
int intf_exists(char *intf);
int set_intf_flags_iff_promisc(char *intf);
int tap_open(char *tap, char *err);
int set_intf_flags_iff_up_and_promisc(char *intf);
int set_intf_flags_iff_up_down(char *intf, int up);
void tap_no_ipv6(char *tap);
int tap_enter_and_open(char *netns, char *tap);
int tap_fork_open(char *netns, char *tap, int pipe_size,
                  void (*child_fn)(int fd_rx_from_parent, int fd_tx_to_parent),
                  int *fd_rx, int *fd_tx);
int tap_child_from_parent(int fd_rx_from_parent, uint8_t **frame, int *gap);
int tap_child_to_parent(int fd_tap, int fd_tx_to_parent, char *tap);
