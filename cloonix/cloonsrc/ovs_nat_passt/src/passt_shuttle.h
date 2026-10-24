/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */

/* Connect to the passt socket: the fd, or -1. */
int passt_shuttle_connect(char *socket_path);

void passt_shuttle_tap_to_passt(int tap_fd, int passt_fd);

void passt_shuttle_passt_to_tap(int passt_fd, int tap_fd);

void passt_shuttle_init(int tap_fd, int passt_fd);
void passt_shuttle_set_tx(int fd_tx_to_tap);
/* From the heartbeat: drains the write backlog. */
void passt_shuttle_heartbeat(void);
int passt_shuttle_connected(void);
int passt_shuttle_generation(void);
int passt_shuttle_alive(void);
