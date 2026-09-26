/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <asm/types.h>
#include <unistd.h>
#include <string.h>
#include <sys/un.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "event_subscriber.h"
#include "lan_to_name.h"
#include "llid_trace.h"
#include "system_callers.h"
#include "commun_daemon.h"
#include "cfg_store.h"
#include "layout_store.h"
#include "utils_cmd_line_maker.h"
#include "file_read_write.h"
#include "suid_power.h"
#include "cnt.h"
#include "kvm.h"
#include "ovs_nat_main.h"
#include "ovs_a2b.h"
#include "ovs_c2c.h"
#include "ovs_tap.h"
#include "ovs_phy.h"
#include "ovs_snf.h"
#include "qga_dialog.h"

static t_cfg cfg;
static int32_t vm_id_tab[MAX_VM];
static t_zombie *head_zombie;
static int nb_zombie;
static int glob_vm_id;

static t_zombie *cfg_is_a_zombie_with_vm_id(int vm_id);
static t_newborn *head_newborn;
static int g_hysteresis_send_topo_info;

int get_conf_rank(void);

static t_vm *find_vm(char *name)
{
  int i;
  t_vm *cur = cfg.vm_head;
  t_vm *result = NULL;
  for (i=0; i<cfg.nb_vm; i++)
    {
    if (!cur)
      KOUT(" ");
    if (!strcmp(cur->kvm.name, name))
      {
      result = cur;
      break;
      }
    cur = cur->next;
    }
  if (i == cfg.nb_vm)
    if (cur)
      KOUT(" ");
  return result;    
}

/* eth<i>, wlan<i>, A or B: the words of an interface on a plug line */
int cfg_name_is_iface(char *name)
{
  char *d = NULL;
  if ((!strcmp(name, "A")) || (!strcmp(name, "B")))
    return 1;
  if (!strncmp(name, "eth", 3))
    d = name + 3;
  else if (!strncmp(name, "wlan", 4))
    d = name + 4;
  if ((!d) || (!d[0]))
    return 0;
  for (; *d; d++)
    if ((*d < '0') || (*d > '9'))
      return 0;
  return 1;
}

int cfg_name_is_in_use(int is_lan, char *name, char *use)
{
  int nb_eth, result = 0;
  memset(use, 0, MAX_PATH_LEN);
  /* The one place defining the shape of an object or lan name. */
  if (!name_is_sane(name))
    {
    snprintf(use, MAX_PATH_LEN-1, "bad name %s: letters, digits, _ - . "
             "only, not starting with - or ., under %d chars",
             (name && name[0]) ? name : "(empty)", MAX_NAME_LEN);
    result = 1;
    }
  else if (is_lan && cfg_name_is_iface(name))
    {
    snprintf(use, MAX_PATH_LEN-1, "%s is the word of an interface, a lan "
             "cannot take it", name);
    result = 1;
    }
  else if ((!strcmp(name, "bulk")) || (!strcmp(name, "cloonix")) ||
           (!strcmp(name, "lan")) || (!strcmp(name, "all")))
    {
    snprintf(use, MAX_PATH_LEN-1, "%s is for system use", name);
    result = 1;
    }
  else if (!strcmp(name, CNTWIFI_LAN))
    {
    snprintf(use, MAX_PATH_LEN-1, "%s is the air of the cnt radios, no item "
             "or lan may take the name", name);
    result = 1;
    }
  else if (!strcmp(name, KVMWIFI_LAN))
    {
    snprintf(use, MAX_PATH_LEN-1, "%s is the air of the kvm radios, no item "
             "or lan may take the name", name);
    result = 1;
    }
  else if (cfg_is_a_zombie(name))
    {
    snprintf(use, MAX_PATH_LEN-1, "%s is used by vm zombie", name);
    result = 1;
    }
  else if (cfg_is_a_newborn(name))
    {
    snprintf(use, MAX_PATH_LEN-1, "%s is used by vm newborn", name);
    result = 1;
    }
  else if (find_vm(name))
    {
    snprintf(use, MAX_PATH_LEN-1, "%s is used by running vm", name);
    result = 1;
    }
  else if (cnt_name_exists(name, &nb_eth))
    {
    snprintf(use, MAX_PATH_LEN-1, "%s is used by container", name);
    result = 1;
    }
  else if (ovs_nat_main_exists(name))
    {
    snprintf(use, MAX_PATH_LEN-1, "%s is used by nat", name);
    result = 1;
    }
  else if (ovs_a2b_exists(name))
    {
    snprintf(use, MAX_PATH_LEN-1, "%s is used by a2b", name);
    result = 1;
    }
  else if (ovs_c2c_exists(name))
    {
    snprintf(use, MAX_PATH_LEN-1, "%s is used by c2c", name);
    result = 2;
    }
  else if (ovs_tap_exists(name))
    {
    snprintf(use, MAX_PATH_LEN-1, "%s is used by tap", name);
    result = 1;
    }
  else if ((!is_lan) && (lan_get_with_name(name)))
    {
    snprintf(use, MAX_PATH_LEN-1, "%s is a lan", name);
    result = 1;
    }
  return result;
}

t_vm *find_vm_with_id(int vm_id)
{
  int i;
  t_vm *cur = cfg.vm_head;
  t_vm *result = NULL;
  for (i=0; i<cfg.nb_vm; i++)
    {
    if (!cur)
      KOUT(" ");
    if (cur->kvm.vm_id == vm_id)
      { 
      result = cur;
      break;
      }
    cur = cur->next;
    }
  if (i == cfg.nb_vm)
    if (cur)
      KOUT(" ");
  return result;   
}

void cfg_free_obj_id(int vm_id)
{
  if (!vm_id_tab[vm_id])
    KOUT(" ");
  vm_id_tab[vm_id] = 0;
}

int cfg_alloc_obj_id(void)
{
  int found = glob_vm_id;
  int count = 0;
  if (vm_id_tab[found])
    KOUT(" ");
  vm_id_tab[found] = 1;
  do
    {
    count += 1;
    if (count > MAX_VM)
      KOUT("%d", count);
    glob_vm_id += 1;
    if (glob_vm_id == MAX_VM)
      glob_vm_id = 1;
    } while((vm_id_tab[glob_vm_id]) || 
             cfg_is_a_zombie_with_vm_id(glob_vm_id));
  if (find_vm_with_id(found))
    KOUT("%d ", found);
  if (find_vm_with_id(glob_vm_id))
    KOUT("%d ", glob_vm_id);
  if (cfg_is_a_zombie_with_vm_id(found))
    KOUT("%d ", found);
  if (cfg_is_a_zombie_with_vm_id(glob_vm_id))
    KOUT("%d ", glob_vm_id);
  return found;
}

static t_vm *alloc_vm(t_topo_kvm *kvm, int vm_id)
{
  t_vm *vm = (t_vm *) clownix_malloc(sizeof(t_vm),24);
  memset(vm, 0, sizeof(t_vm));
  memcpy(&(vm->kvm), kvm, sizeof(t_topo_kvm));
  vm->kvm.vm_id = vm_id;
  return vm;
}

static void check_vm_chain(const char *fct)
{
  int walked = 0;
  t_vm *cur = cfg.vm_head;
  t_vm *prev = NULL;
  int limit = cfg.nb_vm + 2;

  if ((cfg.nb_vm < 0) || (cfg.nb_vm > MAX_VM))
    {
    KERR("ERROR VMCHAIN %s: nb_vm=%d is not a sane count", fct, cfg.nb_vm);
    return;
    }
  if ((cfg.nb_vm == 0) && (cur))
    {
    KERR("ERROR VMCHAIN %s: nb_vm=0 but vm_head is set (%s)",
         fct, cur->kvm.name);
    return;
    }
  while ((cur) && (walked < limit))
    {
    if (cur->prev != prev)
      KERR("ERROR VMCHAIN %s: %s has a wrong prev at position %d",
           fct, cur->kvm.name, walked);
    walked += 1;
    prev = cur;
    cur = cur->next;
    }
  if (cur)
    KERR("ERROR VMCHAIN %s: chain is longer than nb_vm=%d, still going "
         "after %d node(s) -- the next walker to trust the count will run "
         "off the end", fct, cfg.nb_vm, walked);
  else if (walked != cfg.nb_vm)
    KERR("ERROR VMCHAIN %s: chain holds %d node(s) but nb_vm=%d",
         fct, walked, cfg.nb_vm);
}

static void extract_vm(t_cfg *cf, t_vm *vm)
{
  t_vm *cur;
  if (!vm)
    KOUT(" ");
  if (cf->nb_vm <= 0)
    KOUT(" ");
  cur = cf->vm_head;
  if (cur == vm)
    {
    cf->vm_head = cur->next;
    if (cur->next)
      cur->next->prev = NULL;
    }
  else
    {
    if (vm->next)
      vm->next->prev = vm->prev;
    if (vm->prev)
      vm->prev->next = vm->next;
    }
  cf->nb_vm -= 1;
  vm->next = NULL;
  vm->prev = NULL;
  check_vm_chain("extract_vm");
}

static void insert_vm(t_vm *vm)
{
  int i;
  t_vm *cur;
  if (!vm)
    KOUT(" ");
  cur = cfg.vm_head;
  if (cfg.nb_vm > 0)
    {
    for(i=0; i < cfg.nb_vm - 1; i++)
      {
      if (!cur)
        KOUT(" ");
      cur = cur->next;
      }
    if (cur->next)
      KOUT(" ");
    cur->next = vm;
    vm->prev = cur;
    }
  else
    {
    if (cfg.nb_vm != 0)
      KOUT(" ");
    if (cur)
      KOUT(" ");
    cfg.vm_head = vm;
    }
  /* Terminate explicitly: this function is what makes vm the tail, so the
     terminator belongs here and not in alloc_vm's memset. */
  vm->next = NULL;
  cfg.nb_vm += 1;
  check_vm_chain("insert_vm");
}

int cfg_set_vm(t_topo_kvm *kvm, int vm_id, int llid)
{
  int result = -1;
  t_vm *vm = find_vm(kvm->name);
  if (!vm)
    {
    vm = alloc_vm(kvm, vm_id);
    insert_vm(vm);
    result = 0;
    }
  return result;
}

int cfg_unset_vm(t_vm *vm)
{
  int id = vm->kvm.vm_id;
  if (vm->wake_up_eths != NULL)
    {
    KERR("BUG %s", vm->kvm.name);
    free_wake_up_eths(vm);
    }
  extract_vm(&cfg, vm);
  clownix_free(vm, __FUNCTION__);
  llid_trace_vm_delete(id);
  ovs_nat_main_vm_event();
  return id;
}

t_vm *cfg_get_vm(char *name) 
{
  t_vm *tmpvm = find_vm(name);
  return tmpvm;
}

t_vm   *cfg_get_first_vm(int *nb)
{
  *nb = cfg.nb_vm; return cfg.vm_head;
}

void cfg_set_lock_fd(int fd)
{
  cfg.lock_fd = fd;
}

static t_zombie *cfg_is_a_zombie_with_vm_id(int vm_id)
{
  int i;
  t_zombie *result = NULL;
  t_zombie *cur = head_zombie;
  for (i=0; i<nb_zombie; i++)
    {
    if (!cur)
      KOUT(" ");
    if (cur->vm_id == vm_id)
      {
      result = cur;
      break;
      }
    cur = cur->next;
    }
  if (!result && cur)
    KOUT(" ");
  return result;
}

t_zombie *cfg_is_a_zombie(char *name)
{
  int i;
  t_zombie *result = NULL;
  t_zombie *cur = head_zombie;
  for (i=0; i<nb_zombie; i++)
    {
    if (!cur)
      KOUT(" ");
    if (!strcmp(cur->name, name))
      {
      result = cur;
      break;
      }
    cur = cur->next;
    }
  if (!result && cur)
    KOUT(" ");
  return result; 
}

void cfg_del_zombie(char *name)
{
  t_zombie *target = cfg_is_a_zombie(name);
  if (target)
    {
    if (target->next)
      target->next->prev = target->prev;
    if (target->prev)
      target->prev->next = target->next;
    if (target == head_zombie)
      head_zombie = target->next;
    clownix_free(target, __FUNCTION__);
    nb_zombie--;
    }
}

void cfg_add_zombie(int vm_id, char *name)
{
  t_zombie *target = (t_zombie *) clownix_malloc(sizeof(t_zombie),26);
  if (cfg_is_a_zombie_with_vm_id(vm_id))
    KOUT("%s %d", name, vm_id);
  memset(target, 0, sizeof(t_zombie));
  strncpy(target->name, name, MAX_NAME_LEN-1);
  target->vm_id = vm_id;
  if (head_zombie)
    head_zombie->prev = target;
  target->next = head_zombie;
  head_zombie = target;
  nb_zombie++;
  if (nb_zombie > MAX_VM-5)
    KOUT("%d %d", nb_zombie, MAX_VM-5);
}

int cfg_zombie_qty(void)
{
  return nb_zombie;
}

void cfg_add_newborn(char *name)
{
  t_newborn *target = (t_newborn *) clownix_malloc(sizeof(t_newborn),26);
  if (cfg_is_a_newborn(name))
    KOUT("%s", name);
  memset(target, 0, sizeof(t_newborn));
  strncpy(target->name, name, MAX_NAME_LEN-1);
  if (head_newborn)
    head_newborn->prev = target;
  target->next = head_newborn;
  head_newborn = target;
}

void cfg_del_newborn(char *name)
{
  t_newborn *target = cfg_is_a_newborn(name);
  if (target)
    {
    if (target->next)
      target->next->prev = target->prev;
    if (target->prev)
      target->prev->next = target->next;
    if (target == head_newborn)
      head_newborn = target->next;
    clownix_free(target, __FUNCTION__);
    }
}

t_newborn *cfg_is_a_newborn(char *name)
{
  t_newborn *result = NULL;
  t_newborn *cur = head_newborn;
  while (cur)
    {
    if (!strcmp(cur->name, name))
      {
      result = cur;
      break;
      }
    cur = cur->next;
    }
  return result;
}

void cfg_set_host_conf(t_topo_clc *conf)
{
  if (cfg.clc.network[0])
    KOUT(" ");
  memcpy(&(cfg.clc), conf, sizeof(t_topo_clc));
}

t_topo_clc *cfg_get_topo_clc(void)
{
  return (&(cfg.clc));
}

int cfg_get_server_port(void)
{
  int result = cfg.clc.server_port;
  if (!result)
    KOUT(" ");
  return result;
}

int cfg_get_web_port(void)
{
  return cfg.clc.web_port;
}

char *cfg_get_ctrl_doors_sock(void)
{
  static char path[MAX_PATH_LEN];
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, sizeof(path), "%s/%s", cfg_get_root_work(), DOORS_CTRL_SOCK);
  return path;
}

char *cfg_get_root_work(void)
{
  static char path[MAX_PATH_LEN];
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, sizeof(path), "/var/lib/cloonix/%s", cfg.clc.network);
  return(path);
}

char *cfg_get_work(void)
{
  static char path[MAX_PATH_LEN];
  memset(path, 0, MAX_PATH_LEN);
  strncpy(path, cfg_get_root_work(), MAX_PATH_LEN - 1 - strlen(VM_WORKDIR));
  strcat(path, "/");
  strcat(path, VM_WORKDIR);
  return(path);
}

char *cfg_get_work_vm(int vm_id)
{
  static char path[MAX_PATH_LEN];
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path,MAX_PATH_LEN-1, "%s/vm%d", cfg_get_work(), vm_id);
  return(path);
}

char *cfg_get_bin_dir(void)
{
  if (cfg.clc.bin_dir[0] == 0)
    KOUT(" ");
  return(cfg.clc.bin_dir);
}

char *cfg_get_bulk(void)
{
  static char path[MAX_PATH_LEN];
  snprintf(path, sizeof(path), "/var/lib/cloonix/bulk");
  return path;
}

char *cfg_get_bulk_host(void)
{
  static char path[MAX_PATH_LEN];
  snprintf(path, sizeof(path), "/var/lib/cloonix/bulk_host");
  return path;
}

char *cfg_get_cloonix_name(void)
{
  return (cfg.clc.network);
}

char *cfg_get_version(void)
{
  return (cfg.clc.version);
}

static void fill_topo_cnt(t_topo_cnt *topo_cnt, t_cnt *cnt)
{
  memcpy(topo_cnt, &(cnt->cnt), sizeof(t_topo_cnt));
}

static void fill_topo_kvm(t_topo_kvm *kvm, t_vm *vm)
{
  memcpy(kvm, &(vm->kvm), sizeof(t_topo_kvm));
}

static void fill_topo_nat(t_topo_nat *topo_nat, t_ovs_nat_main *nat)
{
  memset(topo_nat, 0, sizeof(t_topo_nat));
  strncpy(topo_nat->name, nat->name, MAX_NAME_LEN-1);
  topo_nat->endp_type = nat->endp_type;
}

static void fill_topo_a2b(t_topo_a2b *topo_a2b, t_ovs_a2b *a2b)
{
  memset(topo_a2b, 0, sizeof(t_topo_a2b));
  strncpy(topo_a2b->name, a2b->name, MAX_NAME_LEN-1);
  topo_a2b->endp_type0 = a2b->side[0].endp_type;
  topo_a2b->endp_type1 = a2b->side[1].endp_type;
}

static void fill_topo_c2c(t_topo_c2c *topo_c2c, t_ovs_c2c *c2c)
{
  memcpy(topo_c2c, &(c2c->topo), sizeof(t_topo_c2c));
}

static void fill_topo_tap(t_topo_tap *topo_tap, t_ovs_tap *tap)
{
  memset(topo_tap, 0, sizeof(t_topo_tap));
  strncpy(topo_tap->name, tap->name, MAX_NAME_LEN-1);
  topo_tap->endp_type = tap->endp_type;
}

static void fill_topo_phy(t_topo_phy *topo_phy, t_ovs_phy *phy)
{
  memset(topo_phy, 0, sizeof(t_topo_phy));
  strncpy(topo_phy->name, phy->vhost, MAX_NAME_LEN-1);
  topo_phy->endp_type = phy->endp_type;
}

static t_topo_info *alloc_all_fields(int nb_vm, int nb_cnt, int nb_nat,
                                     int nb_a2b, int nb_c2c, int nb_tap,
                                     int nb_phy, int nb_endp, int nb_bridges,
                                     int nb_info_phy)
{
  t_topo_info *topo = (t_topo_info *) clownix_malloc(sizeof(t_topo_info), 3);
  int len;
  memset(topo, 0, sizeof(t_topo_info));
  topo->nb_cnt = nb_cnt;
  topo->nb_kvm = nb_vm;
  topo->nb_nat = nb_nat;
  topo->nb_a2b = nb_a2b;
  topo->nb_c2c = nb_c2c;
  topo->nb_tap = nb_tap;
  topo->nb_phy = nb_phy;
  topo->nb_endp = nb_endp;
  topo->nb_info_phy = nb_info_phy;
  topo->nb_bridges = nb_bridges;
  if (topo->nb_cnt)
    {
    topo->cnt =
    (t_topo_cnt *)clownix_malloc(topo->nb_cnt * sizeof(t_topo_cnt),3);
    memset(topo->cnt, 0, topo->nb_cnt * sizeof(t_topo_cnt));
    }
  if (topo->nb_kvm)
    {
    topo->kvm =
    (t_topo_kvm *)clownix_malloc(topo->nb_kvm * sizeof(t_topo_kvm),3);
    memset(topo->kvm, 0, topo->nb_kvm * sizeof(t_topo_kvm));
    }
  if (topo->nb_nat)
    {
    topo->nat =
    (t_topo_nat *)clownix_malloc(topo->nb_nat * sizeof(t_topo_nat),3);
    memset(topo->nat, 0, topo->nb_nat * sizeof(t_topo_nat));
    }
  if (topo->nb_a2b)
    {
    topo->a2b =
    (t_topo_a2b *)clownix_malloc(topo->nb_a2b * sizeof(t_topo_a2b),3);
    memset(topo->a2b, 0, topo->nb_a2b * sizeof(t_topo_a2b));
    }
  if (topo->nb_c2c)
    {
    topo->c2c =
    (t_topo_c2c *)clownix_malloc(topo->nb_c2c * sizeof(t_topo_c2c),3);
    memset(topo->c2c, 0, topo->nb_c2c * sizeof(t_topo_c2c));
    }
  if (topo->nb_tap)
    {
    topo->tap =
    (t_topo_tap *)clownix_malloc(topo->nb_tap * sizeof(t_topo_tap),3);
    memset(topo->tap, 0, topo->nb_tap * sizeof(t_topo_tap));
    }
  if (topo->nb_phy)
    {
    topo->phy =
    (t_topo_phy *)clownix_malloc(topo->nb_phy * sizeof(t_topo_phy),3);
    memset(topo->phy, 0, topo->nb_phy * sizeof(t_topo_phy));
    }
  if (topo->nb_info_phy)
    {
    topo->info_phy =
    (t_topo_info_phy *)clownix_malloc(topo->nb_info_phy * sizeof(t_topo_info_phy),3);
    memset(topo->info_phy, 0, topo->nb_info_phy * sizeof(t_topo_info_phy));
    }
  if (topo->nb_bridges)
    {
    len = topo->nb_bridges * sizeof(t_topo_bridges);
    topo->bridges = (t_topo_bridges *)clownix_malloc(len,3);
    memset(topo->bridges, 0, len);
    }

  return topo;
}

/* Endpoint collector: each object module pushes its cabled endpoints here   */
/* through cfg_topo_endp_push, cfg_produce_topo_info then takes the array.   */
static t_topo_endp *g_endp;
static int g_nb_endp;
static int g_max_endp;

void cfg_topo_endp_push(char *name, int num, int type, char *lan)
{
  t_topo_endp *e, *grown;
  int len = sizeof(t_lan_group_item);
  if (g_nb_endp == g_max_endp)
    {
    g_max_endp = 2 * g_max_endp + 64;
    grown = (t_topo_endp *)clownix_malloc(g_max_endp*sizeof(t_topo_endp), 8);
    memset(grown, 0, g_max_endp * sizeof(t_topo_endp));
    if (g_endp)
      {
      memcpy(grown, g_endp, g_nb_endp * sizeof(t_topo_endp));
      clownix_free(g_endp, __FUNCTION__);
      }
    g_endp = grown;
    }
  e = &(g_endp[g_nb_endp]);
  memset(e, 0, sizeof(t_topo_endp));
  e->lan.lan = (t_lan_group_item *) clownix_malloc(len, 30);
  memset(e->lan.lan, 0, len);
  strncpy(e->name, name, MAX_NAME_LEN-1);
  e->num = num;
  e->type = type;
  if (strlen(lan))
    {
    strncpy(e->lan.lan[0].lan, lan, MAX_NAME_LEN-1);
    e->lan.nb_lan = 1;
    }
  g_nb_endp += 1;
}

/* The lan.lan of each entry moves with the entry, only the array is freed.  */
static int collect_topo_endp(t_topo_endp **endp)
{
  int nb;
  g_nb_endp = 0;
  kvm_topo_endp_push();
  cnt_topo_endp_push();
  ovs_tap_topo_endp_push();
  ovs_phy_topo_endp_push();
  ovs_nat_main_topo_endp_push();
  ovs_a2b_topo_endp_push();
  ovs_c2c_topo_endp_push();
  nb = g_nb_endp;
  *endp = NULL;
  if (nb)
    {
    *endp = (t_topo_endp *)clownix_malloc(nb * sizeof(t_topo_endp), 3);
    memcpy(*endp, g_endp, nb * sizeof(t_topo_endp));
    }
  return nb;
}

t_topo_info *cfg_produce_topo_info(void)
{
  int i, nb_tap, nb_phy, nb_c2c, nb_nat, nb_a2b, nb_vm, nb_cnt;
  t_vm  *vm  = cfg_get_first_vm(&nb_vm);
  t_cnt *cnt = cnt_get_first_cnt(&nb_cnt);
  t_topo_info_phy *info_phy;
  t_topo_bridges *bridges;
  t_topo_endp *endp;
  int nb_endp = collect_topo_endp(&endp);
  int nb_bridges = suid_power_get_topo_bridges(&bridges);
  t_ovs_nat_main *nat = ovs_nat_main_get_first(&nb_nat);
  t_ovs_a2b *a2b = ovs_a2b_get_first(&nb_a2b);
  t_ovs_c2c *c2c = ovs_c2c_get_first(&nb_c2c);
  t_ovs_tap *tap = ovs_tap_get_first(&nb_tap);
  t_ovs_phy *phy = ovs_phy_get_first(&nb_phy);
  int nb_info_phy = suid_power_get_topo_info_phy(&info_phy);
  t_topo_info *topo = alloc_all_fields(nb_vm, nb_cnt, nb_nat, nb_a2b,
                                       nb_c2c, nb_tap, nb_phy, nb_endp,
                                       nb_bridges, nb_info_phy);
  topo->endp = endp;
  memcpy(&(topo->clc), &(cfg.clc), sizeof(t_topo_clc));

  topo->conf_rank = get_conf_rank();

  if (topo->nb_cnt)
    {
    for (i=0; i<topo->nb_cnt; i++)
      {
      if (!cnt)
        KOUT(" ");
      fill_topo_cnt(&(topo->cnt[i]), cnt);
      cnt = cnt->next;
      }
    if (cnt)
      KOUT(" ");
    }

  if (topo->nb_kvm)
    {
    for (i=0; i<topo->nb_kvm; i++)
      {
      if (!vm)
        KOUT(" ");
      fill_topo_kvm(&(topo->kvm[i]), vm);
      vm = vm->next;
      }
    if (vm)
      KOUT(" ");
    }

  if (topo->nb_nat)
    {
    for (i=0; i<topo->nb_nat; i++)
      {
      if (!nat)
        KOUT(" ");
      fill_topo_nat(&(topo->nat[i]), nat);
      nat = nat->next;
      }
    if (nat)
      KOUT(" ");
    }

  if (topo->nb_a2b)
    {
    for (i=0; i<topo->nb_a2b; i++)
      {
      if (!a2b)
        KOUT(" ");
      fill_topo_a2b(&(topo->a2b[i]), a2b);
      a2b = a2b->next;
      }
    if (a2b)
      KOUT(" ");
    }

  if (topo->nb_c2c)
    {
    for (i=0; i<topo->nb_c2c; i++)
      {
      if (!c2c)
        KOUT(" ");
      fill_topo_c2c(&(topo->c2c[i]), c2c);
      c2c = c2c->next;
      }
    if (c2c)
      KOUT(" ");
    }

  if (topo->nb_tap)
    {
    for (i=0; i<topo->nb_tap; i++)
      {
      if (!tap)
        KOUT(" ");
      fill_topo_tap(&(topo->tap[i]), tap);
      tap = tap->next;
      }
    if (tap)
      KOUT(" ");
    }

  if (topo->nb_phy)
    {
    for (i=0; i<topo->nb_phy; i++)
      {
      if (!phy)
        KOUT(" ");
      fill_topo_phy(&(topo->phy[i]), phy);
      phy = phy->next;
      }
    if (phy)
      KOUT(" ");
    }

  for (i=0; i<nb_info_phy; i++)
    {
    memcpy(&(topo->info_phy[i]), &(info_phy[i]), sizeof(t_topo_info_phy));
    }

  for (i=0; i<nb_bridges; i++)
    {
    memcpy(&(topo->bridges[i]), &(bridges[i]), sizeof(t_topo_bridges));
    }

  return topo;
}

int cfg_get_vm_locked(t_vm *vm)
{
  return (vm->locked_vm);
}

void cfg_set_vm_locked(t_vm *vm)
{
  vm->locked_vm = 1;
}

void cfg_reset_vm_locked(t_vm *vm)
{
  vm->locked_vm = 0;
}

static void timer_send_topo_info(void *data)
{
  layout_forget_absent();
  event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
  g_hysteresis_send_topo_info = 0;
}

void cfg_hysteresis_send_topo_info(void)
{
  if (g_hysteresis_send_topo_info == 0)
    {
    g_hysteresis_send_topo_info = 1;
    clownix_timeout_add(100, timer_send_topo_info, NULL, NULL, NULL);
    }
}

void cfg_send_vm_evt_cloonix_ga_ping_ok(char *name)
{
  t_small_evt vm_evt;
  memset(&vm_evt, 0, sizeof(vm_evt));
  strncpy(vm_evt.name, name, MAX_NAME_LEN-1);
  vm_evt.evt = vm_evt_cloonix_ga_ping_ok;
  event_subscriber_send(topo_small_event, (void *) &vm_evt);
}

void cfg_send_vm_evt_cloonix_ga_ping_ko(char *name)
{
  t_small_evt vm_evt;
  memset(&vm_evt, 0, sizeof(vm_evt));
  strncpy(vm_evt.name, name, MAX_NAME_LEN-1);
  vm_evt.evt = vm_evt_cloonix_ga_ping_ko;
  event_subscriber_send(topo_small_event, (void *) &vm_evt);
}

void cfg_send_vm_evt_qmp_conn_ok(char *name)
{
  t_small_evt vm_evt;
  memset(&vm_evt, 0, sizeof(t_small_evt));
  strncpy(vm_evt.name, name, MAX_NAME_LEN-1);
  vm_evt.evt = vm_evt_qmp_conn_ok;
  event_subscriber_send(topo_small_event, (void *) &vm_evt);
}

void cfg_init(void)
{
  int i;
  memset(&cfg, 0, sizeof(t_cfg));
  for (i=0; i<MAX_VM; i++)
    vm_id_tab[i] = 0;
  head_zombie = NULL;
  nb_zombie = 0;
  glob_vm_id = 1;;
  g_hysteresis_send_topo_info = 0;
}
