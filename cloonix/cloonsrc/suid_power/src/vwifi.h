/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
/* suid-power side, air_proc.c: the medium is asked, never run here */
char *vwifi_why_no_radio(void);
int vwifi_radios_create(char *cnt, int vm_id, char *nspacecrun, int nb,
                        uint8_t *mac_vwif);
void vwifi_radios_delete(char *name);
void vwifi_air_msg(int llid, int tid, char *line);
void vwifi_init(void);
void vwifi_after_suidroot_init(void);

/* the medium process, vwifi.c */
void medium_main(int fd_sync, int fd_async);

/* the pipes between the two: one line a message */
#define AIR_LINE_BUF 4096
int  air_lines(int fd, char *buf, int *len, void (*cb)(char *line));
void air_line_send(int fd, const char *fmt, ...)
     __attribute__((format(printf, 2, 3)));
