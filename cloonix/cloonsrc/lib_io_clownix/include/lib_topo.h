/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
typedef struct t_topo_cnt_chain
{
  t_topo_cnt cnt;
  struct t_topo_cnt_chain *prev;
  struct t_topo_cnt_chain *next;
} t_topo_cnt_chain;
typedef struct t_topo_kvm_chain
{
  t_topo_kvm kvm;
  struct t_topo_kvm_chain *prev;
  struct t_topo_kvm_chain *next;
} t_topo_kvm_chain;
typedef struct t_topo_c2c_chain
{
  t_topo_c2c c2c;
  struct t_topo_c2c_chain *prev;
  struct t_topo_c2c_chain *next;
} t_topo_c2c_chain;
typedef struct t_topo_tap_chain
{
  t_topo_tap tap;
  struct t_topo_tap_chain *prev;
  struct t_topo_tap_chain *next;
} t_topo_tap_chain;
typedef struct t_topo_a2b_chain
{
  t_topo_a2b a2b;
  struct t_topo_a2b_chain *prev;
  struct t_topo_a2b_chain *next;
} t_topo_a2b_chain;
typedef struct t_topo_nat_chain
{
  t_topo_nat nat;
  struct t_topo_nat_chain *prev;
  struct t_topo_nat_chain *next;
} t_topo_nat_chain;
typedef struct t_topo_phy_chain
{
  t_topo_phy phy;
  struct t_topo_phy_chain *prev;
  struct t_topo_phy_chain *next;
} t_topo_phy_chain;
typedef struct t_topo_lan_chain
{
  char lan[MAX_NAME_LEN];
  struct t_topo_lan_chain *prev;
  struct t_topo_lan_chain *next;
} t_topo_lan_chain;
typedef struct t_topo_edge_chain
{
  char name[MAX_NAME_LEN];
  int num;
  char lan[MAX_NAME_LEN];
  struct t_topo_edge_chain *prev;
  struct t_topo_edge_chain *next;
} t_topo_edge_chain;
typedef struct t_topo_differences
{
  t_topo_cnt_chain  *add_cnt;
  t_topo_cnt_chain  *del_cnt;
  t_topo_kvm_chain  *add_kvm;
  t_topo_kvm_chain  *del_kvm;
  t_topo_c2c_chain  *add_c2c;
  t_topo_c2c_chain  *del_c2c;
  t_topo_tap_chain  *add_tap;
  t_topo_tap_chain  *del_tap;
  t_topo_a2b_chain  *add_a2b;
  t_topo_a2b_chain  *del_a2b;
  t_topo_nat_chain  *add_nat;
  t_topo_nat_chain  *del_nat;
  t_topo_phy_chain  *add_phy;
  t_topo_phy_chain  *del_phy;
  t_topo_lan_chain  *add_lan;
  t_topo_lan_chain  *del_lan;
  t_topo_edge_chain *add_edge;
  t_topo_edge_chain *del_edge;
} t_topo_differences;
t_topo_info *topo_duplicate(t_topo_info *ref);
int topo_compare(t_topo_info *topoA, t_topo_info *topoB);
void topo_free_topo(t_topo_info *topo);

