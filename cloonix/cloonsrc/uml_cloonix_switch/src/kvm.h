/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
typedef struct t_ethv_cnx
{
  char name[MAX_NAME_LEN];
  int num;
  char vhost[MAX_NAME_LEN];
  int endp_type;
  int ready;
  int vhost_up;
  char lan[MAX_NAME_LEN];
  char lan_added[MAX_NAME_LEN];
  int waiting_ack_add_lan;
  int waiting_ack_del_lan;
  int attached_lan_ok;
  int llid;
  int tid;
  int del_snf_ethv_sent;
  struct t_ethv_cnx *prev;
  struct t_ethv_cnx *next;
} t_ethv_cnx;
void kvm_topo_endp_push(void);
int kvm_exists(char *name, int num);
/* The eth record exists, ready or not: see the body. */
int kvm_eth_present(char *name, int num);
void kvm_add_whole_vm(char *name, int nb_tot_eth, t_eth_table *eth_tab);
void kvm_resp_add_lan(int is_ko, char *name, int num, char *vhost, char *lan);
void kvm_resp_del_lan(int is_ko, char *name, int num, char *vhost, char *lan);
int kvm_add_lan(int llid, int tid, char *name, int num,
                     char *lan, int endp_type, t_eth_table *eth_tab,
                     char *info);
int kvm_del_lan(int llid, int tid, char *name, int num,
                     char *lan, int endp_type);
void kvm_del(int llid, int tid, char *name, int num,
             int endp_type, int *can_delete);
int kvm_lan_added_exists(char *name, int num);
int kvm_dyn_snf(char *name, int num, int val);
t_ethv_cnx *get_first_head_ethv(void);
void kvm_init(void);
int kvm_vhost_up(char *name, int num, int set);
