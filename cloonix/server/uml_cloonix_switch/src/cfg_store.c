/*****************************************************************************/
/*    Copyright (C) 2006-2022 clownix@clownix.net License AGPL-3             */
/*                                                                           */
/*  This program is free software: you can redistribute it and/or modify     */
/*  it under the terms of the GNU Affero General Public License as           */
/*  published by the Free Software Foundation, either version 3 of the       */
/*  License, or (at your option) any later version.                          */
/*                                                                           */
/*  This program is distributed in the hope that it will be useful,          */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of           */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            */
/*  GNU Affero General Public License for more details.a                     */
/*                                                                           */
/*  You should have received a copy of the GNU Affero General Public License */
/*  along with this program.  If not, see <http://www.gnu.org/licenses/>.    */
/*                                                                           */
/*****************************************************************************/
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
#include "utils_cmd_line_maker.h"
#include "stats_counters.h"
#include "layout_rpc.h"
#include "layout_topo.h"
#include "file_read_write.h"
#include "suid_power.h"
#include "cnt.h"
#include "kvm.h"
#include "ovs_nat.h"
#include "ovs_c2c.h"
#include "ovs_tap.h"
#include "ovs_snf.h"
#include "qga_dialog.h"


/*---------------------------------------------------------------------------*/
static t_cfg cfg;
static int32_t vm_id_tab[MAX_VM];
static t_zombie *head_zombie;
static int nb_zombie;
static int glob_vm_id;
static t_newborn *head_newborn;
/*---------------------------------------------------------------------------*/
static int g_hysteresis_send_topo_info;

int get_conf_rank(void);

/*****************************************************************************/
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
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cfg_name_is_in_use(int is_lan, char *name, char *use)
{
  int nb_eth, result = 0;
  memset(use, 0, MAX_PATH_LEN);
  if ((!strcmp(name, "doors")) ||
           (!strcmp(name, "uml_cloonix_switch")) ||
           (!strcmp(name, "cloon")))
    {
    snprintf(use, MAX_NAME_LEN, "%s is for system use", name);
    result = 1;
    }
  else if (cfg_is_a_zombie(name))
    {
    snprintf(use, MAX_NAME_LEN, "%s is used by vm zombie", name);
    result = 1;
    }
  else if (cfg_is_a_newborn(name))
    {
    snprintf(use, MAX_NAME_LEN, "%s is used by vm newborn", name);
    result = 1;
    }
  else if (find_vm(name))
    {
    snprintf(use, MAX_NAME_LEN, "%s is used by running vm", name);
    result = 1;
    }
  else if (cnt_name_exists(name, &nb_eth))
    {
    snprintf(use, MAX_NAME_LEN, "%s is used by container", name);
    result = 1;
    }
  else if (ovs_nat_exists(name))
    {
    snprintf(use, MAX_NAME_LEN, "%s is used by nat", name);
    result = 1;
    }
  else if (ovs_c2c_exists(name))
    {
    snprintf(use, MAX_NAME_LEN, "%s is used by c2c", name);
    result = 1;
    }
  else if (ovs_tap_exists(name))
    {
    snprintf(use, MAX_NAME_LEN, "%s is used by tap", name);
    result = 1;
    }
  else if ((!is_lan) && (lan_get_with_name(name)))
    {
    snprintf(use, MAX_NAME_LEN, "%s is a lan", name);
    result = 1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
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
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void cfg_free_obj_id(int vm_id)
{
  if (!vm_id_tab[vm_id])
    KOUT(" ");
  vm_id_tab[vm_id] = 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
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
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static t_vm *alloc_vm(t_topo_kvm *kvm, int vm_id)
{
  t_vm *vm = (t_vm *) clownix_malloc(sizeof(t_vm),24);
  memset(vm, 0, sizeof(t_vm));
  memcpy(&(vm->kvm), kvm, sizeof(t_topo_kvm));
  vm->kvm.vm_id = vm_id;
  return vm;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
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
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
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
  cfg.nb_vm += 1;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cfg_set_vm(t_topo_kvm *kvm, int vm_id, int llid)
{
  int result = -1;
  t_vm *vm = find_vm(kvm->name);
  if (!vm)
    {
    vm = alloc_vm(kvm, vm_id);
    insert_vm(vm);
    layout_add_vm(kvm->name, llid);
    result = 0;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cfg_unset_vm(t_vm *vm)
{
  int id = vm->kvm.vm_id;
  if (vm->wake_up_eths != NULL)
    {
    KERR("BUG %s", vm->kvm.name);
    free_wake_up_eths(vm);
    }
  layout_del_vm(vm->kvm.name);
  extract_vm(&cfg, vm);
  clownix_free(vm, __FUNCTION__);
  llid_trace_vm_delete(id);
  ovs_nat_vm_event();
  return id;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
t_vm *cfg_get_vm(char *name) 
{
  t_vm *tmpvm = find_vm(name);
  return tmpvm;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
t_vm   *cfg_get_first_vm(int *nb)
{
  *nb = cfg.nb_vm; return cfg.vm_head;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void cfg_set_lock_fd(int fd)
{
  cfg.lock_fd = fd;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
int  cfg_get_lock_fd(void)
{
  return (cfg.lock_fd);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
t_zombie *cfg_is_a_zombie_with_vm_id(int vm_id)
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
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
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
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
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
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
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
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cfg_zombie_qty(void)
{
  return nb_zombie;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
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
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
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
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
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
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void cfg_set_host_conf(t_topo_clc *conf)
{
  if (cfg.clc.network[0])
    KOUT(" ");
  memcpy(&(cfg.clc), conf, sizeof(t_topo_clc));
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
t_topo_clc *cfg_get_topo_clc(void)
{
  return (&(cfg.clc));
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cfg_get_server_port(void)
{
  int result = cfg.clc.server_port;
  if (!result)
    KOUT(" ");
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *cfg_get_ctrl_doors_sock(void)
{
  static char path[MAX_PATH_LEN];
  memset(path, 0, MAX_PATH_LEN);
  sprintf(path, "%s/%s", cfg_get_root_work(), DOORS_CTRL_SOCK);
  return path;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *cfg_get_root_work(void)
{
  if (cfg.clc.work_dir[0] == 0)
    KOUT(" ");
  return(cfg.clc.work_dir);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *cfg_get_work(void)
{
  static char path[MAX_PATH_LEN];
  memset(path, 0, MAX_PATH_LEN);
  strncpy(path,cfg_get_root_work(),MAX_PATH_LEN-1-strlen(CLOONIX_VM_WORKDIR));
  strcat(path, "/");
  strcat(path, CLOONIX_VM_WORKDIR);
  return(path);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *cfg_get_work_vm(int vm_id)
{
  static char path[MAX_PATH_LEN];
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path,MAX_PATH_LEN-1, "%s/vm%d", cfg_get_work(), vm_id);
  return(path);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *cfg_get_bin_dir(void)
{
  if (cfg.clc.bin_dir[0] == 0)
    KOUT(" ");
  return(cfg.clc.bin_dir);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *cfg_get_bulk(void)
{
  static char path[MAX_PATH_LEN];
  if (cfg.clc.bulk_dir[0] == 0)
    KOUT(" ");
  memset(path, 0, MAX_PATH_LEN);
  sprintf(path,"%s", cfg.clc.bulk_dir);
  return path;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *cfg_get_cloonix_name(void)
{
  return (cfg.clc.network);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *cfg_get_version(void)
{
  return (cfg.clc.version);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void fill_topo_cnt(t_topo_cnt *topo_cnt, t_cnt *cnt)
{
  memcpy(topo_cnt, &(cnt->cnt), sizeof(t_topo_cnt));
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void fill_topo_kvm(t_topo_kvm *kvm, t_vm *vm)
{
  memcpy(kvm, &(vm->kvm), sizeof(t_topo_kvm));
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void fill_topo_nat(t_topo_nat *topo_nat, t_ovs_nat *nat)
{
  memset(topo_nat, 0, sizeof(t_topo_nat));
  strncpy(topo_nat->name, nat->name, MAX_NAME_LEN);
  topo_nat->endp_type = nat->endp_type;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void fill_topo_c2c(t_topo_c2c *topo_c2c, t_ovs_c2c *c2c)
{
  memcpy(topo_c2c, &(c2c->topo), sizeof(t_topo_c2c));
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void fill_topo_tap(t_topo_tap *topo_tap, t_ovs_tap *tap)
{
  memset(topo_tap, 0, sizeof(t_topo_tap));
  strncpy(topo_tap->name, tap->name, MAX_NAME_LEN);
  topo_tap->endp_type = tap->endp_type;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static t_topo_info *alloc_all_fields(int nb_vm, int nb_cnt,
                                     int nb_nat, int nb_c2c, int nb_tap,
                                     int nb_endp, int nb_bridges)
{
  t_topo_info *topo = (t_topo_info *) clownix_malloc(sizeof(t_topo_info), 3);
  int len;
  memset(topo, 0, sizeof(t_topo_info));
  topo->nb_cnt = nb_cnt;
  topo->nb_kvm = nb_vm;
  topo->nb_nat = nb_nat;
  topo->nb_c2c = nb_c2c;
  topo->nb_tap = nb_tap;
  topo->nb_endp = nb_endp;
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
  if (topo->nb_endp)
    {
    topo->endp =
    (t_topo_endp *)clownix_malloc(topo->nb_endp * sizeof(t_topo_endp),3);
    memset(topo->endp, 0, topo->nb_endp * sizeof(t_topo_endp));
    }
 if (topo->nb_bridges)
    {
    len = topo->nb_bridges * sizeof(t_topo_bridges);
    topo->bridges = (t_topo_bridges *)clownix_malloc(len,3);
    memset(topo->bridges, 0, len);
    }

  return topo;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void copy_endp(t_topo_endp *dst, t_topo_endp *src)
{
  int len, i;
  memcpy(dst, src, sizeof(t_topo_endp));
  len = src->lan.nb_lan * sizeof(t_lan_group_item);
  dst->lan.lan = (t_lan_group_item *) clownix_malloc(len, 19);
  memset(dst->lan.lan, 0, len);
  dst->lan.nb_lan = src->lan.nb_lan;
  for (i=0; i<src->lan.nb_lan; i++)
    strncpy(dst->lan.lan[i].lan, src->lan.lan[i].lan, MAX_NAME_LEN-1);
  clownix_free(src->lan.lan, __FUNCTION__);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
t_topo_info *cfg_produce_topo_info(void)
{
  int i, nb_tap, nb_c2c, nb_nat, nb_vm, nb_cnt;
  int nb_endp_kvm, nb_endp_cnt, nb_endp_tap, nb_endp_nat;
  int nb_endp_c2c, i_endp=0;
  t_vm  *vm  = cfg_get_first_vm(&nb_vm);
  t_cnt *cnt = cnt_get_first_cnt(&nb_cnt);
  t_topo_bridges *bridges;
  t_topo_endp *c2c_endp = ovs_c2c_translate_topo_endp(&nb_endp_c2c);
  t_topo_endp *nat_endp = ovs_nat_translate_topo_endp(&nb_endp_nat);
  t_topo_endp *tap_endp = ovs_tap_translate_topo_endp(&nb_endp_tap);
  t_topo_endp *cnt_endp = cnt_translate_topo_endp(&nb_endp_cnt);
  t_topo_endp *kvm_endp = kvm_translate_topo_endp(&nb_endp_kvm);
  int nb_bridges = suid_power_get_topo_bridges(&bridges);
  t_ovs_nat *nat = ovs_nat_get_first(&nb_nat);
  t_ovs_c2c *c2c = ovs_c2c_get_first(&nb_c2c);
  t_ovs_tap *tap = ovs_tap_get_first(&nb_tap);
  int nb_endp = nb_endp_cnt+nb_endp_kvm+nb_endp_tap+nb_endp_nat+nb_endp_c2c;
  t_topo_info *topo = alloc_all_fields(nb_vm, nb_cnt, nb_nat, nb_c2c, nb_tap,
                                       nb_endp, nb_bridges);
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

  for (i=0; i<nb_endp_kvm; i++)
    {
    if (i_endp == topo->nb_endp)
      KOUT(" ");
    copy_endp(&(topo->endp[i_endp]), &(kvm_endp[i]));
    i_endp += 1;
    }

  for (i=0; i<nb_endp_cnt; i++)
    {
    if (i_endp == topo->nb_endp)
      KOUT(" ");
    copy_endp(&(topo->endp[i_endp]), &(cnt_endp[i]));
    i_endp += 1;
    }

  for (i=0; i<nb_endp_tap; i++)
    {
    if (i_endp == topo->nb_endp)
      KOUT(" ");
    copy_endp(&(topo->endp[i_endp]), &(tap_endp[i]));
    i_endp += 1;
    }

  for (i=0; i<nb_endp_nat; i++)
    {
    if (i_endp == topo->nb_endp)
      KOUT(" ");
    copy_endp(&(topo->endp[i_endp]), &(nat_endp[i]));
    i_endp += 1;
    }

  for (i=0; i<nb_endp_c2c; i++)
    {
    if (i_endp == topo->nb_endp)
      KOUT(" ");
    copy_endp(&(topo->endp[i_endp]), &(c2c_endp[i]));
    i_endp += 1;
    }

  for (i=0; i<nb_bridges; i++)
    {
    memcpy(&(topo->bridges[i]), &(bridges[i]), sizeof(t_topo_bridges));
    }

  clownix_free(cnt_endp, __FUNCTION__);
  topo->nb_endp = i_endp;
  return topo;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cfg_get_vm_locked(t_vm *vm)
{
  return (vm->locked_vm);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void cfg_set_vm_locked(t_vm *vm)
{
  vm->locked_vm = 1;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void cfg_reset_vm_locked(t_vm *vm)
{
  vm->locked_vm = 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cfg_get_name_with_mac(char *mac, char *vmname)
{
  int i, j, nb_eth, result = -1;
  t_vm *cur = cfg.vm_head;
  t_topo_kvm *topo_kvm;
  t_eth_table *eth_table;
  char *addr;
  char mac_vm[MAX_NAME_LEN];
  for (i=0; i<cfg.nb_vm; i++)
    {
    if (!cur)
      KOUT(" ");
    topo_kvm = &(cur->kvm); 
    nb_eth = topo_kvm->nb_tot_eth;
    eth_table = topo_kvm->eth_table;
    for (j=0; j<nb_eth; j++)
      {
      addr = eth_table[j].mac_addr;
      sprintf(mac_vm,"%02X:%02X:%02X:%02X:%02X:%02X",
                 addr[0] & 0xFF, addr[1] & 0xFF, addr[2] & 0xFF,
                 addr[3] & 0xFF, addr[4] & 0xFF, addr[5] & 0xFF);
      if (!strcmp(mac_vm, mac))
        {
        result = 0;
        memset(vmname, 0, MAX_NAME_LEN);
        strncpy(vmname, topo_kvm->name, MAX_NAME_LEN-1);
        break;
        }
      }
    if (result == 0)
      break;
    cur = cur->next;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void timer_send_topo_info(void *data)
{
  event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
  g_hysteresis_send_topo_info = 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void cfg_hysteresis_send_topo_info(void)
{
  if (g_hysteresis_send_topo_info == 0)
    {
    g_hysteresis_send_topo_info = 1;
    clownix_timeout_add(100, timer_send_topo_info, NULL, NULL, NULL);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void cfg_send_vm_evt_cloonix_ga_ping_ok(char *name)
{
  t_small_evt vm_evt;
  memset(&vm_evt, 0, sizeof(vm_evt));
  strncpy(vm_evt.name, name, MAX_NAME_LEN-1);
  vm_evt.evt = vm_evt_cloonix_ga_ping_ok;
  event_subscriber_send(topo_small_event, (void *) &vm_evt);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void cfg_send_vm_evt_cloonix_ga_ping_ko(char *name)
{
  t_small_evt vm_evt;
  memset(&vm_evt, 0, sizeof(vm_evt));
  strncpy(vm_evt.name, name, MAX_NAME_LEN-1);
  vm_evt.evt = vm_evt_cloonix_ga_ping_ko;
  event_subscriber_send(topo_small_event, (void *) &vm_evt);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void cfg_send_vm_evt_qmp_conn_ok(char *name)
{
  t_small_evt vm_evt;
  memset(&vm_evt, 0, sizeof(t_small_evt));
  strncpy(vm_evt.name, name, MAX_NAME_LEN-1);
  vm_evt.evt = vm_evt_qmp_conn_ok;
  event_subscriber_send(topo_small_event, (void *) &vm_evt);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
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
/*---------------------------------------------------------------------------*/
