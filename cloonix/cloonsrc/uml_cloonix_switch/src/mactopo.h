/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
int mactopo_test_exists(char *name, int num);
int mactopo_add_req(int item_type, char *name, int num, char *lan,
                    char *vhost, unsigned char *mac_addr, char *err);
void mactopo_snf_add(char *name, int num, char *lan, int llid);
void mactopo_snf_del(char *name, int num, char *lan, int llid);
void mactopo_del_req(int item_type, char *name, int num, char *lan);
void mactopo_del_resp(int item_type, char *name, int num, char *lan);
void mactopo_add_resp(int is_ok, int item_type, char *name, int num, char *lan);
void mactopo_init(void);
