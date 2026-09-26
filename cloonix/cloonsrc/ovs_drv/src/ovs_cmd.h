/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
int ovs_cmd_vhost_up(char *ovs_bin, char *ovs_dir,
                     char *name, int num, char *vhost);
int ovs_cmd_add_snf_lan(char *ovs_bin, char *ovs_dir, char *name, int num,
                        char *vhost, char *lan);
int ovs_cmd_del_snf_lan(char *ovs_bin, char *ovs_dir, char *name, int num,
                        char *vhost, char *lan);
int ovs_cmd_add_lan_endp(char *ovs_bin, char *ovs_dir, char *lan, char *vhost);
int ovs_cmd_del_lan_endp(char *ovs_bin, char *ovs_dir, char *lan, char *vhost);

int ovs_cmd_add_lan(char *ovs_bin, char *ovs_dir, char *lan);
int ovs_cmd_add_lan_rstp(char *ovs_bin, char *ovs_dir, char *lan, int val);
int ovs_cmd_del_lan(char *ovs_bin, char *ovs_dir, char *lan);

