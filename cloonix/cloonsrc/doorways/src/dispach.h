/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
typedef struct t_transfert
{
  int type;
  int dido_llid;
  int inside_llid;
  int beat_count;
  int paused_in;     /* reads from the server stopped, the client is behind */
  int paused_out;    /* reads from the client stopped, the server is behind */
  struct t_transfert *prev;
  struct t_transfert *next;
} t_transfert;

void dispach_door_llid(int listen_llid, int llid);
void dispach_door_end(int llid);
char *get_switch_path(void);
void doorways_rx_bufraw(int llid, int tid,
                        int len_bufraw, char *doors_bufraw,
                        int type, int val, int len, char *buf);
int  dispach_send_to_traf_client(int llid, int val, int len, char *buf);
void in_err_gene(int inside_llid, int err, int from);
void dispach_init(void);

