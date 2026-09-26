/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#define IOCMLC 1
enum {
     kind_client = 0,
     kind_simple_watch_connect,
     kind_simple_watch,
     kind_server,
     kind_server_doors,
     kind_server_proxy_traf_inet,
     kind_server_proxy_traf_unix,
     kind_server_proxy_sig,
     kind_glib_managed
     };

int channel_create(int fd, int kind, char *little_name,
                   t_fd_event rx_cb,  t_fd_event tx_cb, t_fd_error err_cb);
char *channel_get_little_name(int llid);
void channel_delete(int llid);
int channel_is_connect_wait(int llid);
void channel_beat_set (t_heartbeat_cb beat);
void channel_loop(void);
void channel_init(void);
int channel_check_llid(int llid, const char *fct);
int get_fd_with_cidx(int cidx);
void channel_heartbeat_ms_set (int heartbeat_ms);
void called_from_channel_free_llid(int llid);

