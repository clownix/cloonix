/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
typedef struct t_ovs_phy
{
  char name[MAX_NAME_LEN];
  unsigned char mac_addr[8];
  char vhost[MAX_NAME_LEN];
  char lan[MAX_NAME_LEN];
  char lan_added[MAX_NAME_LEN];
  int  del_phy_req;
  int  del_snf_ethv_sent;
  int  llid;
  int  tid;
  int  phy_id;
  int  num_macvlan;
  int  endp_type;
  struct t_ovs_phy *prev;
  struct t_ovs_phy *next;
} t_ovs_phy;

t_ovs_phy *ovs_phy_get_first(int *nb_phy);
void ovs_phy_resp_msg_phy(int is_ko, int is_add, char *name, int num, char *vhost);
void ovs_phy_resp_add_lan(int is_ko, char *name, int num,
                          char *vhost, char *lan);
void ovs_phy_resp_del_lan(int is_ko, char *name, int num,
                          char *vhost, char *lan);
int  ovs_phy_dyn_snf(char *name, int val);
void ovs_phy_add(int llid, int tid, char *name, int type);
void ovs_phy_del(int llid, int tid, char *name);
void ovs_phy_add_lan(int llid, int tid, char *name, char *lan);
void ovs_phy_del_lan(int llid, int tid, char *name, char *lan);
void ovs_phy_topo_endp_push(void);
t_ovs_phy *ovs_phy_exists_vhost(char *name);
void ovs_phy_init(void);
