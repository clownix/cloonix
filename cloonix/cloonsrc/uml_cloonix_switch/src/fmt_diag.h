/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
int fmt_tx_add_lan_rstp(int tid, char *lan, int val);
int fmt_tx_add_lan(int tid, char *lan);
int fmt_tx_del_lan(int tid, char *lan);
int fmt_tx_add_tap(int tid, char *name, char *vhost, char *mac);
int fmt_tx_del_tap(int tid, char *name, char *vhost);
int fmt_tx_add_phy(int tid, char *name, char *vhost, char *mac, int num_macvlan);
int fmt_tx_del_phy(int tid, char *name, char *vhost, int num_macvlan);
int fmt_tx_vhost_up(int tid, char *name, int num, char *vhost);
int fmt_tx_add_snf_lan(int tid, char *name, int num, char *vhost, char *lan);
int fmt_tx_del_snf_lan(int tid, char *name, int num, char *vhost, char *lan);
int fmt_tx_add_lan_endp(int tid, char *name, int num, char *vhost, char *lan);
int fmt_tx_del_lan_endp(int tid, char *name, int num, char *vhost, char *lan);
int fmt_tx_macvlan_mac(int type, char *macv, char *nmac);
void fmt_rx_rpct_recv_sigdiag_msg(int llid, int tid, char *line);

