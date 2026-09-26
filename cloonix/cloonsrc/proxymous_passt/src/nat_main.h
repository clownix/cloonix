/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
typedef struct t_ctx_nat t_ctx_nat;

typedef struct t_ctx_nat
{
  int llid_listen;
  int llid_sig;
  char nat[MAX_NAME_LEN];
  char nat_prxy_path[MAX_PATH_LEN];
  char ports[MAX_PATH_LEN];
  char passt_sock_path[MAX_PATH_LEN];
  struct t_ctx_nat *prev;
  struct t_ctx_nat *next;
} t_ctx_nat;
char *get_net_name(void);
t_ctx_nat *get_ctx_nat(char *nat);
int  nat_main_request(char *name, int on, char *ports, int expected);
void nat_main_kil_req(void);
void nat_main_passt_died(void);
void nat_main_init(void);
