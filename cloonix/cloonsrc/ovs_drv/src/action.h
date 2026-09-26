/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#define MAX_NETNS_BUF_LEN 500
#define HEADER_NETNS_MSG 6
#define TOT_NETNS_BUF_LEN (MAX_NETNS_BUF_LEN + HEADER_NETNS_MSG)

int action_resp(char *respb, int failed, char *format, ...);

void action_add_lan_endp(char *bin, char *db, char *respb,
                         char *lan, char *name, int num, char *vhost);

void action_del_lan_endp(char *bin, char *db, char *respb,
                         char *lan, char *name, int num, char *vhost);

void action_vhost_up(char *bin, char *db, char *respb,
                     char *name, int num, char *vhost);

void action_add_snf_lan(char *bin, char *db, char *respb,
                        char *name, int num, char *vhost, char *lan);

void action_del_snf_lan(char *bin, char *db, char *respb,
                        char *name, int num, char *vhost, char *lan);

void action_add_lan_rstp(char *bin, char *db, char *respb, char *lan, int val);
void action_add_lan(char *bin, char *db, char *respb, char *lan);

void action_del_lan(char *bin, char *db, char *respb, char *lan);

void action_req_ovsdb(char *bin, char *db, char *net, char *respb);
void action_req_ovs_switch(char *bin, char *db, char *net, char *respb);
void action_req_suidroot(char *respb);

