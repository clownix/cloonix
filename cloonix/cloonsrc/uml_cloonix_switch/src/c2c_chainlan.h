/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
int c2c_chainlan_store_cmd(char *name, int cmd_mac_mangle, uint8_t *mac);
int c2c_chainlan_mac_mangle(char *name, uint8_t *mac);
void c2c_chainlan_update_endp_type(char *name, int endp_type);
void c2c_chainlan_snf_started(char *name, int num, char *vhost);
void c2c_chainlan_resp_add_lan(int is_ko, char *name, int num,
                               char *vhost, char *lan);
void c2c_chainlan_resp_del_lan(int is_ko, char *name, int num,
                               char *vhost, char *lan);
int c2c_chainlan_action_heartbeat(t_ovs_c2c *c2c, char *attlan);
int c2c_chainlan_add(t_ovs_c2c *cur);
int c2c_chainlan_del(t_ovs_c2c *cur);
int c2c_chainlan_partial_del(t_ovs_c2c *cur);
void c2c_chainlan_init(void);
/* the lan asked for a c2c, kept in recv_add_lan.c: 1 and lan filled, or 0 */
int shadown_lan_exists(char *name, int num, char *lan);

