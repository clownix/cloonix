/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
typedef struct t_ovs_tap
{
  char name[MAX_NAME_LEN];
  char vhost[MAX_NAME_LEN];
  unsigned char mac_addr[8];
  char lan[MAX_NAME_LEN];
  char lan_added[MAX_NAME_LEN];
  int  del_tap_req;
  int  del_snf_ethv_sent;
  int  llid;
  int  tid;
  int  stuck_count;
  int  tap_id;
  int  endp_type;
  struct t_ovs_tap *prev;
  struct t_ovs_tap *next;
} t_ovs_tap;

t_ovs_tap *ovs_tap_get_first(int *nb_tap);
void ovs_tap_resp_msg_tap(int is_ko, int is_add, char *name);
void ovs_tap_resp_add_lan(int is_ko, char *name, int num,
                          char *vhost, char *lan);
void ovs_tap_resp_del_lan(int is_ko, char *name, int num,
                          char *vhost, char *lan);
t_ovs_tap *ovs_tap_exists(char *name);
int  ovs_tap_dyn_snf(char *name, int val);
void ovs_tap_add(int llid, int tid, char *name);
void ovs_tap_del(int llid, int tid, char *name);
void ovs_tap_add_lan(int llid, int tid, char *name, char *lan);
void ovs_tap_del_lan(int llid, int tid, char *name, char *lan);
void ovs_tap_topo_endp_push(void);
void ovs_tap_init(void);
