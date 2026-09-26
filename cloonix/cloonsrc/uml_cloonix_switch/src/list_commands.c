/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <asm/types.h>
#include <unistd.h>
#include <string.h>
#include "io_clownix.h"
#include "commun_daemon.h"
#include "rpc_clownix.h"
#include "cfg_store.h"
#include "lan_to_name.h"
#include "ovs.h"
#include "cnt.h"
#include "kvm.h"
#include "ovs_c2c.h"
#include "ovs_a2b.h"
#include "ovs_tap.h"
#include "ovs_phy.h"
#include "ovs_nat_main.h"
#include "layout_store.h"
#include "wifi_air.h"
#include "c2c_chainlan.h"

#define MAX_LIST_AIRS 32

static int can_increment_index(int val)
{
  int result = 1;
  if (val >= MAX_LIST_COMMANDS_QTY)
    {
    KERR("TOO MANY COMMANDS IN LIST");
    result = 0;
    }
  return result;
}

static int cmd_append(t_list_commands *list, int len, char *format, ...)
{
  int written, room = MAX_LIST_COMMANDS_LEN - len;
  va_list ap;
  if (room <= 1)
    return len;
  va_start(ap, format);
  written = vsnprintf(list->cmd + len, room, format, ap);
  va_end(ap);
  if ((written < 0) || (written >= room))
    return MAX_LIST_COMMANDS_LEN - 1;
  return len + written;
}

/* eth=<n>: the eths are born not capturable, sniff=on makes one so */
static void eth_tab_to_str(char *out, int nb_tot_eth, t_eth_table *eth_tab)
{
  (void) eth_tab;
  memset(out, 0, MAX_NAME_LEN);
  snprintf(out, MAX_NAME_LEN-1, "%d", nb_tot_eth);
}

static int snf_line(int idx, t_list_commands *hlist, char *name, char *itf)
{
  if (can_increment_index(idx))
    {
    cmd_append(&(hlist[idx]), 0, "cloonix cli %s cnf %s%s sniff=on",
               cfg_get_cloonix_name(), name, itf);
    idx += 1;
    }
  return idx;
}

static int sniffed(int endp_type)
{
  return ((endp_type == endp_type_eths) || (endp_type == endp_type_taps) ||
          (endp_type == endp_type_nats) || (endp_type == endp_type_c2cs) ||
          (endp_type == endp_type_phyas) || (endp_type == endp_type_phyms));
}

/* the items of one interface, and the two sides of an a2b */
static int snf_items(int result, t_list_commands *hlist)
{
  int i, nb;
  t_ovs_tap *tap = ovs_tap_get_first(&nb);
  t_ovs_phy *phy = ovs_phy_get_first(&nb);
  t_ovs_nat_main *nat = ovs_nat_main_get_first(&nb);
  t_ovs_c2c *c2c = ovs_c2c_get_first(&nb);
  t_ovs_a2b *a2b = ovs_a2b_get_first(&nb);
  for (; tap; tap = tap->next)
    if (sniffed(tap->endp_type))
      result = snf_line(result, hlist, tap->name, "");
  for (; phy; phy = phy->next)
    if (sniffed(phy->endp_type))
      result = snf_line(result, hlist, phy->name, "");
  for (; nat; nat = nat->next)
    if (sniffed(nat->endp_type))
      result = snf_line(result, hlist, nat->name, "");
  for (; c2c; c2c = c2c->next)
    if (sniffed(c2c->topo.endp_type))
      result = snf_line(result, hlist, c2c->name, "");
  for (; a2b; a2b = a2b->next)
    for (i = 0; i < 2; i++)
      if (sniffed(a2b->side[i].endp_type))
        result = snf_line(result, hlist, a2b->name, i ? " B" : " A");
  return result;
}

/* the eths made capturable, and the airs captured */
static int build_snf_cmd(int offset, t_list_commands *hlist)
{
  char airs[MAX_LIST_AIRS][MAX_NAME_LEN];
  char itf[MAX_NAME_LEN];
  int i, nb, result = offset;
  t_vm *vm = cfg_get_first_vm(&nb);
  t_cnt *cnt = cnt_get_first_cnt(&nb);
  for (; vm; vm = vm->next)
    for (i = 0; i < vm->kvm.nb_tot_eth; i++)
      if (vm->kvm.eth_table[i].endp_type == endp_type_eths)
        {
        snprintf(itf, MAX_NAME_LEN, " eth%d", i);
        result = snf_line(result, hlist, vm->kvm.name, itf);
        }
  for (; cnt; cnt = cnt->next)
    for (i = 0; i < cnt->cnt.nb_tot_eth; i++)
      if (cnt->cnt.eth_table[i].endp_type == endp_type_eths)
        {
        snprintf(itf, MAX_NAME_LEN, " eth%d", i);
        result = snf_line(result, hlist, cnt->cnt.name, itf);
        }
  result = snf_items(result, hlist);
  nb = wifi_air_snf_names(airs, MAX_LIST_AIRS);
  for (i = 0; i < nb; i++)
    result = snf_line(result, hlist, airs[i], "");
  return result;
}

static int build_add_vm_cmd(int offset, t_list_commands *hlist, 
                            t_topo_kvm *kvm)
{
  int i, win, len = 0;
  int result = offset;
  t_list_commands *list = &(hlist[offset]);
  char eth_desc[MAX_NAME_LEN];
  unsigned char *mc;
  if (can_increment_index(result))
    {
    eth_tab_to_str(eth_desc, kvm->nb_tot_eth, kvm->eth_table);
    /* add win is --windows --uefi and full virt, add freebsd --freebsd */
    win = (kvm->vm_config_flags & VM_CONFIG_FLAG_WINDOWS);
    len = cmd_append(list, len, 
           "cloonix cli %s add %s %s ram=%d cpu=%d eth=%s",
            cfg_get_cloonix_name(), win ? "win" :
            (kvm->vm_config_flags & VM_CONFIG_FLAG_FREEBSD) ? "freebsd" :
            "kvm", kvm->name, kvm->mem, kvm->cpu, eth_desc);
    len = cmd_append(list, len, " %s", kvm->rootfs_input);
    if (kvm->vm_config_flags & VM_CONFIG_FLAG_PERSISTENT)
      len = cmd_append(list, len, " --persistent");
    if ((!win) && (kvm->vm_config_flags & VM_CONFIG_FLAG_FULL_VIRT))
      len = cmd_append(list, len, " --fullvirt");
    if ((!win) && (kvm->vm_config_flags & VM_CONFIG_FLAG_UEFI))
      len = cmd_append(list, len, " --uefi");
    if (kvm->vm_config_flags & VM_CONFIG_FLAG_NO_QEMU_GA)
      len = cmd_append(list, len, " --no_qemu_ga");
    if (kvm->vm_config_flags & VM_CONFIG_FLAG_NO_REBOOT)
      len = cmd_append(list, len, " --no_reboot");
    if (kvm->vm_config_flags & VM_CONFIG_FLAG_INSTALL_CDROM)
      len = cmd_append(list, len, " install_cdrom=%s",
                                         kvm->install_cdrom);
    if (kvm->vm_config_flags & VM_CONFIG_FLAG_ADDED_CDROM)
      len = cmd_append(list, len, " added_cdrom=%s",
                                         kvm->added_cdrom);
    if (kvm->vm_config_flags & VM_CONFIG_FLAG_ADDED_DISK)
      len = cmd_append(list, len, " added_disk=%s",
                                         kvm->added_disk);
    if (kvm->nb_tot_nb_vwif)
      len = cmd_append(list, len, " wlan=%d", kvm->nb_tot_nb_vwif);
    for (i=0; i < kvm->nb_tot_eth; i++)
      {
      if (kvm->eth_table[i].randmac == 0)
        {
        mc = kvm->eth_table[i].mac_addr;
	len = cmd_append(list, len,
        " mac_addr=eth%d:%02x:%02x:%02x:%02x:%02x:%02x", i,
                           mc[0]&0xff, mc[1]&0xff, mc[2]&0xff,
                           mc[3]&0xff, mc[4]&0xff, mc[5]&0xff);
        }
      }
    result += 1;
    }
  return result;
}

static int build_add_cnt_cmd(int offset, t_list_commands *hlist,
                             t_topo_cnt *cnt)
{
  int i, len = 0;
  int result = offset;
  t_list_commands *list = &(hlist[offset]);
  char eth_desc[MAX_NAME_LEN];
  char shortbrand[MAX_NAME_LEN];
  unsigned char *mc;
  if (can_increment_index(result))
    {
    eth_tab_to_str(eth_desc, cnt->nb_tot_eth, cnt->eth_table);
    if (!strcmp(cnt->brandtype, "brandcnt"))
      strcpy(shortbrand, "cnt");
    else
      KOUT("ERROR %s (only brandcnt)", cnt->brandtype);
    len = cmd_append(list, len, "cloonix cli %s add %s %s eth=%s %s",
            cfg_get_cloonix_name(), shortbrand, cnt->name, eth_desc, cnt->image);
    if (cnt->is_persistent)
      len = cmd_append(list, len, " --persistent");
    if (strlen(cnt->startup_env))
      len = cmd_append(list, len, strchr(cnt->startup_env, ' ') ?
                       " startup_env=\"%s\"" : " startup_env=%s",
                       cnt->startup_env);
    if (strlen(cnt->vmount))
      len = cmd_append(list, len, strchr(cnt->vmount, ' ') ?
                       " vmount=\"%s\"" : " vmount=%s", cnt->vmount);
    if (cnt->nb_tot_nb_vwif)
      len = cmd_append(list, len, " wlan=%d", cnt->nb_tot_nb_vwif);
    for (i=0; i < cnt->nb_tot_eth; i++)
      {
      if (cnt->eth_table[i].randmac == 0)
        {
        mc = cnt->eth_table[i].mac_addr;
        len = cmd_append(list, len,
        " mac_addr=eth%d:%02x:%02x:%02x:%02x:%02x:%02x", i,
                           mc[0]&0xff, mc[1]&0xff, mc[2]&0xff,
                           mc[3]&0xff, mc[4]&0xff, mc[5]&0xff);
        }
      }
    result += 1;
    }
  return result;
}

static int build_add_sat_cmd(int offset, t_list_commands *hlist,
                             char *sat_type, char *name)
{
  int len = 0, result = offset;;
  t_list_commands *list = &(hlist[offset]);
  if (can_increment_index(result))
    {
    len = cmd_append(list, len, "cloonix cli %s add %s %s", 
                   cfg_get_cloonix_name(), sat_type, name);
    result += 1;
    }
  return result;
}

/* ports as the nat keeps them, tcp:8080:80,udp:53:53, or none */
static int build_add_nat_cmd(int offset, t_list_commands *hlist,
                             t_ovs_nat_main *nat)
{
  char ports[MAX_PATH_LEN], proto[MAX_NAME_LEN], *tok, *save = NULL;
  int host, guest, len = 0, result = offset;
  t_list_commands *list = &(hlist[offset]);
  if (can_increment_index(result))
    {
    len = cmd_append(list, len, "cloonix cli %s add nat %s",
                     cfg_get_cloonix_name(), nat->name);
    memset(ports, 0, MAX_PATH_LEN);
    strncpy(ports, nat->ports, MAX_PATH_LEN-1);
    for (tok = strtok_r(ports, ",", &save); tok;
         tok = strtok_r(NULL, ",", &save))
      {
      if (!strcmp(tok, "none"))
        continue;
      if (sscanf(tok, "%63[a-z]:%d:%d", proto, &host, &guest) != 3)
        KERR("ERROR nat %s: %s is no port dump commands can say",
             nat->name, tok);
      else
        len = cmd_append(list, len, " publish=%d:%d%s", host, guest,
                         strcmp(proto, "udp") ? "" : "/udp");
      }
    result += 1;
    }
  return result;
}

/* the settings of the objects: a2b, cnt caps, rstp, mac_mangle */
static int build_cnf_cmd(int result, t_list_commands *hlist)
{
  char lans[MAX_LIST_AIRS][MAX_NAME_LEN];
  char *net = cfg_get_cloonix_name();
  char *dir_word[2] = { "A2B", "B2A" };
  uint8_t m[6];
  int i, d, nb;
  t_ovs_a2b *a2b = ovs_a2b_get_first(&nb);
  t_cnt *cnt = cnt_get_first_cnt(&nb);
  t_ovs_c2c *c2c = ovs_c2c_get_first(&nb);
  for (; a2b; a2b = a2b->next)
    for (d = 0; d < 2; d++)
      {
      for (i = 0; i < A2B_NB_CNF; i++)
        if ((a2b->cnf_set[d][i]) && (a2b->cnf_val[d][i] || (i >= 4)) &&
            can_increment_index(result))
          {
          cmd_append(&(hlist[result]), 0, "cloonix cli %s cnf %s %s %s=%d",
                     net, a2b->name, dir_word[d], ovs_a2b_cnf_word(i),
                     a2b->cnf_val[d][i]);
          result += 1;
          }
      if ((a2b->cnf_repsrc[d][0]) && can_increment_index(result))
        {
        cmd_append(&(hlist[result]), 0, "cloonix cli %s cnf %s %s "
                   "replace_ipsrc %s", net, a2b->name, dir_word[d],
                   a2b->cnf_repsrc[d]);
        result += 1;
        }
      if ((a2b->cnf_repdst[d][0]) && can_increment_index(result))
        {
        cmd_append(&(hlist[result]), 0, "cloonix cli %s cnf %s %s "
                   "replace_ipdst %s", net, a2b->name, dir_word[d],
                   a2b->cnf_repdst[d]);
        result += 1;
        }
      }
  for (; cnt; cnt = cnt->next)
    if ((cnt->lim_cpu || cnt->lim_ram) && can_increment_index(result))
      {
      cmd_append(&(hlist[result]), 0, "cloonix cli %s cnf %s cpu=%d ram=%d",
                 net, cnt->cnt.name, cnt->lim_cpu, cnt->lim_ram);
      result += 1;
      }
  nb = lan_rstp_names(lans, MAX_LIST_AIRS);
  for (i = 0; i < nb; i++)
    if (can_increment_index(result))
      {
      cmd_append(&(hlist[result]), 0, "cloonix cli %s cnf %s rstp=on",
                 net, lans[i]);
      result += 1;
      }
  for (; c2c; c2c = c2c->next)
    if (c2c_chainlan_mac_mangle(c2c->name, m) && can_increment_index(result))
      {
      cmd_append(&(hlist[result]), 0, "cloonix cli %s cnf %s "
                 "mac_mangle=%02X:%02X:%02X:%02X:%02X:%02X", net, c2c->name,
                 m[0], m[1], m[2], m[3], m[4], m[5]);
      result += 1;
      }
  return result;
}

/* the far side of a c2c is made by its master: a comment there */
static int build_add_c2c_cmd(int offset, t_list_commands *hlist, 
                             char *name, char *dist_cloon, int master)
{
  int len = 0, result = offset;
  t_list_commands *list = &(hlist[offset]);
  if (can_increment_index(result))
    {
    if (master)
      len = cmd_append(list, len, "cloonix cli %s add c2c %s %s",
                       cfg_get_cloonix_name(), name, dist_cloon);
    else
      len = cmd_append(list, len, "# cloonix cli %s add c2c %s %s",
                       dist_cloon, name, cfg_get_cloonix_name());
    result += 1;
    }
  return result;
}

/* One "plug" line per cabled endpoint; returns the next free index.        */
static int add_lan_line(int idx, t_list_commands *hlist,
                        char *name, int num, char *lan)
{
  if (can_increment_index(idx))
    {
    cmd_append(&(hlist[idx]), 0, "cloonix cli %s plug %s%s %s",
               cfg_get_cloonix_name(), name, wifi_air_iface(name, num), lan);
    idx += 1;
    }
  return idx;
}

/* a radio off the default air of its kind: on another one, or on none */
static int radio_lines(int idx, t_list_commands *hlist, char *name,
                       int nb_eth, int nb_vwif,
                       char airs[MAX_VWIF][MAX_NAME_LEN], char *dflt)
{
  int i;
  for (i = 0; (i < nb_vwif) && (i < MAX_VWIF); i++)
    {
    if ((airs[i][0]) && strcmp(airs[i], dflt))
      idx = add_lan_line(idx, hlist, name, nb_eth + i, airs[i]);
    else if ((!airs[i][0]) && can_increment_index(idx))
      {
      cmd_append(&(hlist[idx]), 0, "cloonix cli %s unplug %s wlan%d %s",
                 cfg_get_cloonix_name(), name, i, dflt);
      idx += 1;
      }
    }
  return idx;
}

static int build_add_lan_kvm_cmd(int offset, t_list_commands *hlist)
{
  t_ethv_cnx *cur = get_first_head_ethv();
  int nb, result = offset;
  t_vm *vm = cfg_get_first_vm(&nb);
  while(cur)
    {
    if (cur->attached_lan_ok)
      result = add_lan_line(result, hlist, cur->name, cur->num, cur->lan);
    cur = cur->next;
    }
  while (vm)
    {
    result = radio_lines(result, hlist, vm->kvm.name, vm->kvm.nb_tot_eth,
                         vm->kvm.nb_tot_nb_vwif, vm->kvm.vwif_air,
                         KVMWIFI_LAN);
    vm = vm->next;
    }
  return result;
}

static int build_add_lan_cnt_cmd(int offset, t_list_commands *hlist)
{
  int i, nb;
  t_cnt *cur = cnt_get_first_cnt(&nb);
  int result = offset;
  while(cur)
    {
    for (i=0; i<cur->cnt.nb_tot_eth; i++)
      {
      if (cur->att_lan[i].lan_attached_ok)
        result = add_lan_line(result, hlist, cur->cnt.name, i,
                              cur->att_lan[i].lan);
      }
    result = radio_lines(result, hlist, cur->cnt.name, cur->cnt.nb_tot_eth,
                         cur->cnt.nb_tot_nb_vwif, cur->cnt.vwif_air,
                         CNTWIFI_LAN);
    cur = cur->next;
    }
  return result;
}

static int build_add_lan_tap_cmd(int offset, t_list_commands *hlist)
{
  int nb;
  t_ovs_tap *cur = ovs_tap_get_first(&nb);
  int result = offset;
  while(cur)
    {
    if (strlen(cur->lan))
      result = add_lan_line(result, hlist, cur->name, 0, cur->lan);
    cur = cur->next;
    }
  return result;
}

static int build_add_lan_phy_cmd(int offset, t_list_commands *hlist)
{
  int nb;
  t_ovs_phy *cur = ovs_phy_get_first(&nb);
  int result = offset;
  while(cur)
    {
    if (strlen(cur->lan))
      result = add_lan_line(result, hlist, cur->name, 0, cur->lan);
    cur = cur->next;
    }
  return result;
}

static int build_add_lan_nat_cmd(int offset, t_list_commands *hlist)
{
  int nb;
  t_ovs_nat_main *cur = ovs_nat_main_get_first(&nb);
  int result = offset;
  while(cur)
    {
    if (strlen(cur->lan))
      result = add_lan_line(result, hlist, cur->name, 0, cur->lan);
    cur = cur->next;
    }
  return result;
}

/* The lan asked, not the one attached yet: the attachment follows it on a
   heartbeat, and a peer not connected has none. */
static int build_add_lan_c2c_cmd(int offset, t_list_commands *hlist)
{
  int nb;
  char lan[MAX_NAME_LEN];
  t_ovs_c2c *cur = ovs_c2c_get_first(&nb);
  int result = offset;
  while(cur)
    {
    if (shadown_lan_exists(cur->name, 0, lan))
      result = add_lan_line(result, hlist, cur->name, 0, lan);
    cur = cur->next;
    }
  return result;
}

static int build_add_lan_a2b_cmd(int offset, t_list_commands *hlist)
{
  int i, nb;
  t_ovs_a2b *cur = ovs_a2b_get_first(&nb);
  int result = offset;
  while(cur)
    {
    for (i=0; i<2; i++)
      {
      if (strlen(cur->side[i].lan))
        result = add_lan_line(result, hlist, cur->name, i, cur->side[i].lan);
      }
    cur = cur->next;
    }
  return result;
}

static int build_layout_cmd(int offset, t_list_commands *hlist)
{
  int i, num, nb, result = offset;
  t_layout *cur = layout_get_first(&nb);
  t_list_commands *list;
  for (i=0; i<nb; i++)
    {
    if (cur[i].has_pos)
      {
      if (can_increment_index(result))
        {
        list = &(hlist[result]);
        cmd_append(list, 0, "cloonix cli %s cnf %s x=%d y=%d",
                   cfg_get_cloonix_name(), cur[i].name, cur[i].x, cur[i].y);
        result += 1;
        }
      }
    for (num=0; num<MAX_ETH_VM; num++)
      {
      if (!(cur[i].eth_set & (1U << num)))
        continue;
      if (can_increment_index(result))
        {
        list = &(hlist[result]);
        cmd_append(list, 0, "cloonix cli %s cnf %s%s angle=%d",
                   cfg_get_cloonix_name(), cur[i].name,
                   wifi_air_iface(cur[i].name, num),
                   (int) cur[i].eth_deg[num]);
        result += 1;
        }
      }
    }
  return result;
}

int produce_list_commands(t_list_commands *hlist, int is_layout)
{
  (void)is_layout;
  int i, nb_vm, nb_cnt, result = 0;
  int nb_nat, nb_a2b, nb_c2c, nb_tap, nb_phy;
  t_vm  *vm  = cfg_get_first_vm(&nb_vm);
  t_cnt *cnt = cnt_get_first_cnt(&nb_cnt);
  t_ovs_nat_main *nat = ovs_nat_main_get_first(&nb_nat);
  t_ovs_a2b *a2b = ovs_a2b_get_first(&nb_a2b);
  t_ovs_c2c *c2c = ovs_c2c_get_first(&nb_c2c);
  t_ovs_tap *tap = ovs_tap_get_first(&nb_tap);
  t_ovs_phy *phy = ovs_phy_get_first(&nb_phy);

    for (i=0; i<nb_vm; i++)
      {
      result = build_add_vm_cmd(result, hlist, &(vm->kvm));
      vm = vm->next;
      }
    for (i=0; i<nb_cnt; i++)
      {
      result = build_add_cnt_cmd(result, hlist, &(cnt->cnt));
      cnt = cnt->next;
      }
    while(tap)
      {
      result = build_add_sat_cmd(result, hlist, "tap", tap->name);
      tap = tap->next;
      }
    while(phy)
      {
      if (can_increment_index(result))
        {
        cmd_append(&(hlist[result]), 0, "cloonix cli %s add phy %s%s",
                   cfg_get_cloonix_name(), phy->name,
                   ((phy->endp_type == endp_type_phyms) ||
                    (phy->endp_type == endp_type_phymv)) ? " --macvlan"
                                                           : "");
        result += 1;
        }
      phy = phy->next;
      }
    while(nat)
      {
      result = build_add_nat_cmd(result, hlist, nat);
      nat = nat->next;
      }
    while(a2b)
      {
      result = build_add_sat_cmd(result, hlist, "a2b", a2b->name);
      a2b = a2b->next;
      }
    while(c2c)
      {
      result = build_add_c2c_cmd(result, hlist, c2c->topo.name,
                                 c2c->topo.dist_cloon,
                                 c2c->topo.local_is_master);
      c2c = c2c->next;
      }
    result = build_add_lan_kvm_cmd(result, hlist);
    result = build_add_lan_cnt_cmd(result, hlist);
    result = build_add_lan_tap_cmd(result, hlist);
    result = build_add_lan_phy_cmd(result, hlist);
    result = build_add_lan_nat_cmd(result, hlist);
    result = build_add_lan_a2b_cmd(result, hlist);
    result = build_add_lan_c2c_cmd(result, hlist);
    result = build_snf_cmd(result, hlist);
    result = build_cnf_cmd(result, hlist);
    result = build_layout_cmd(result, hlist);

  return result;
}

