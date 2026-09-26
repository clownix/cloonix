/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#define MAX_CLOONIX_SERVERS 100
typedef struct t_cloonix_conf_info
{
  char name[MAX_NAME_LEN];
  char doors[MAX_NAME_LEN];
  int  doors_llid;
  int  connect_llid;
  uint32_t  ip;
  int  port;
  char passwd[MSG_DIGEST_LEN];
  uint32_t c2c_udp_ip;
  int c2c_udp_port_low;
  int web_port;
} t_cloonix_conf_info;

char *cloonix_conf_info_get_version(void);
char *cloonix_conf_info_get_names(void);
t_cloonix_conf_info *cloonix_cnf_info_get(char *name, int *rank);
int cloonix_conf_info_init(char *path_conf);

