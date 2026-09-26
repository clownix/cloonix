/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
void nonblock_send(int fd, char *buf, int len);
int nonblock_queued(int fd);
unsigned long nonblock_written(int fd);
void nonblock_flush(int fd, int ms);
void nonblock_prepare_fd_set(int fd, fd_set *outfd);
void nonblock_process_events(fd_set *outfd);
void nonblock_del_fd(int fd);
void nonblock_add_fd(int fd);
void nonblock_init(void);
