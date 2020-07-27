/*****************************************************************************/
/*    Copyright (C) 2006-2020 clownix@clownix.net License AGPL-3             */
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
#include "endp_mngt.h"
#include "cfg_store.h"
#include "utils_cmd_line_maker.h"
#include "c2c.h"
#include "stats_counters.h"
#include "endp_evt.h"
#include "c2c_utils.h"
#include "layout_rpc.h"
#include "layout_topo.h"
#include "file_read_write.h"
#include "dpdk_ovs.h"
#include "dpdk_tap.h"
#include "dpdk_snf.h"
#include "dpdk_d2d.h"
#include "dpdk_a2b.h"
#include "vhost_eth.h"
#include "suid_power.h"
#include "edp_mngt.h"
#include "edp_evt.h"
#include "nat_dpdk_process.h"


/*---------------------------------------------------------------------------*/
static t_cfg cfg;
static int32_t vm_id_tab[MAX_VM];
static t_zombie *head_zombie;
static int nb_zombie;
static int glob_vm_id;
static t_newborn *head_newborn;
/*---------------------------------------------------------------------------*/

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
  int type, result = 0;
  t_sc2c *c2c = c2c_find(name);
  t_d2d_cnx *d2d = dpdk_d2d_find(name);
  memset(use, 0, MAX_PATH_LEN);
  if (c2c)
    {
    snprintf(use, MAX_NAME_LEN, "%s is used by a sat c2c", name);
    result = 1;
    }
  else if (d2d)
    {
    snprintf(use, MAX_NAME_LEN, "%s is used by a d2d", name);
    result = 1;
    }
  else if (dpdk_a2b_exists(name))
    {
    snprintf(use, MAX_NAME_LEN, "%s is used by a a2b", name);
    result = 1;
    }
  else if ((!strcmp(name, "doors")) ||
           (!strcmp(name, "uml_cloonix_switch")) ||
           (!strcmp(name, "cloonix")))
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
  else if ((endp_mngt_exists(name, 0, &type)) ||
           (edp_mngt_exists(name, &type)))
    {
    if (type == endp_type_tap)
      snprintf(use, MAX_NAME_LEN, "%s is used by a tap", name);
    else if (type == endp_type_wif)
      snprintf(use, MAX_NAME_LEN, "%s is used by a wif", name);
    else if (type == endp_type_pci)
      snprintf(use, MAX_NAME_LEN, "%s is used by a pci", name);
    else if (type == endp_type_phy)
      snprintf(use, MAX_NAME_LEN, "%s is used by a phy", name);
    else if (type == endp_type_snf)
      snprintf(use, MAX_NAME_LEN, "%s is used by a snf", name);
    else if (type == endp_type_nat)
      snprintf(use, MAX_NAME_LEN, "%s is used by a nat", name);
    else if (type == endp_type_c2c)
      snprintf(use, MAX_NAME_LEN, "%s is used by a c2c", name);
    else
      KERR("%d ", type);
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
void cfg_free_vm_id(int vm_id)
{
  if (!vm_id_tab[vm_id])
    KOUT(" ");
  vm_id_tab[vm_id] = 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cfg_alloc_vm_id(void)
{
  int found = glob_vm_id;
  if (vm_id_tab[found])
    KOUT(" ");
  vm_id_tab[found] = 1;
  do
    {
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
  muendp_wlan_death(vm->kvm.name, vm->kvm.nb_tot_eth);
  layout_del_vm(vm->kvm.name);
  extract_vm(&cfg, vm);
  clownix_free(vm, __FUNCTION__);
  llid_trace_vm_delete(id);
  nat_dpdk_vm_event();
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
static void topo_vlg(t_lan_group *vlg, t_lan_attached *lan_att)
{
  int i, j=0, len;
  char *ascii_lan;

  vlg->nb_lan = 0;
  for (i=0; i<MAX_TRAF_ENDPOINT; i++)
    {
    if (lan_att[i].lan_num)
      vlg->nb_lan += 1;
    }

  len = vlg->nb_lan * sizeof(t_lan_group_item);
  vlg->lan = (t_lan_group_item *) clownix_malloc(len, 29);
  memset(vlg->lan, 0, len);

  for (i=0; i<MAX_TRAF_ENDPOINT; i++)
    {
    if (lan_att[i].lan_num)
      {
      ascii_lan = lan_get_with_num(lan_att[i].lan_num);
      if (!ascii_lan)
        KOUT("%d", lan_att[i].lan_num);
      strncpy(vlg->lan[j].lan, ascii_lan, MAX_NAME_LEN-1);
      j += 1;
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void fill_topo_kvm(t_topo_kvm *kvm, t_vm *vm)
{
  memcpy(kvm, &(vm->kvm), sizeof(t_topo_kvm));
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void fill_topo_c2c(t_topo_c2c *c2c, t_endp *endp)
{
  memcpy(c2c, &(endp->c2c), sizeof(t_topo_c2c));
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void fill_topo_sat(t_topo_sat *sat, t_endp *endp)
{
  strncpy(sat->name, endp->name, MAX_NAME_LEN-1);
  sat->type = endp->endp_type;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void fill_topo_endp(t_topo_endp *topo_endp, t_endp *endp)
{
  strncpy(topo_endp->name, endp->name, MAX_NAME_LEN-1);
  topo_endp->num = endp->num;
  topo_endp->type = endp->endp_type;
  topo_vlg(&(topo_endp->lan), endp->lan_attached);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static t_topo_info *alloc_all_fields(int nb_vm, int nb_dpdk_endp,
                                     int nb_vhost, int nb_endp_edp,
                                     int nb_endp_d2d, int nb_endp_a2b,
                                     int nb_phy, int nb_pci,
                                     int nb_bridges, int nb_mirrors,
                                     int nb_d2d, int nb_a2b)
{
  t_topo_info *topo = (t_topo_info *) clownix_malloc(sizeof(t_topo_info), 3);
  int len;
  memset(topo, 0, sizeof(t_topo_info));
  topo->nb_kvm = nb_vm;
  topo->nb_a2b = nb_a2b;
  topo->nb_d2d = nb_d2d;
  topo->nb_c2c = endp_mngt_get_nb(endp_type_c2c);
  topo->nb_sat = endp_mngt_get_nb_sat();
  topo->nb_sat += edp_mngt_get_qty();
  topo->nb_endp = endp_mngt_get_nb_all() + nb_dpdk_endp + nb_vhost;
  topo->nb_endp += nb_endp_edp + nb_endp_d2d + nb_endp_a2b;
  topo->nb_phy = nb_phy;
  topo->nb_pci = nb_pci;
  topo->nb_bridges = nb_bridges;
  topo->nb_mirrors = nb_mirrors;
 if (topo->nb_kvm)
    {
    topo->kvm =
    (t_topo_kvm *)clownix_malloc(topo->nb_kvm * sizeof(t_topo_kvm),3);
    memset(topo->kvm, 0, topo->nb_kvm * sizeof(t_topo_kvm));
    }
  if (topo->nb_c2c)
    {
    topo->c2c =
    (t_topo_c2c *)clownix_malloc(topo->nb_c2c * sizeof(t_topo_c2c),3);
    memset(topo->c2c, 0, topo->nb_c2c * sizeof(t_topo_c2c));
    }
  if (topo->nb_d2d)
    {
    topo->d2d =
    (t_topo_d2d *)clownix_malloc(topo->nb_d2d * sizeof(t_topo_d2d),3);
    memset(topo->d2d, 0, topo->nb_d2d * sizeof(t_topo_d2d));
    }
  if (topo->nb_a2b)
    {
    topo->a2b =
    (t_topo_a2b *)clownix_malloc(topo->nb_a2b * sizeof(t_topo_a2b),3);
    memset(topo->a2b, 0, topo->nb_a2b * sizeof(t_topo_a2b));
    }
  if (topo->nb_sat)
    {
    topo->sat =
    (t_topo_sat *)clownix_malloc(topo->nb_sat * sizeof(t_topo_sat),3);
    memset(topo->sat, 0, topo->nb_sat * sizeof(t_topo_sat));
    }

  if (topo->nb_endp)
    {
    topo->endp =
    (t_topo_endp *)clownix_malloc(topo->nb_endp * sizeof(t_topo_endp),3);
    memset(topo->endp, 0, topo->nb_endp * sizeof(t_topo_endp));
    }

  if (topo->nb_phy)
    {
    topo->phy =
    (t_topo_phy *)clownix_malloc(topo->nb_phy * sizeof(t_topo_phy),3);
    memset(topo->phy, 0, topo->nb_phy * sizeof(t_topo_phy));
    }

 if (topo->nb_pci)
    {
    topo->pci =
    (t_topo_pci *)clownix_malloc(topo->nb_pci * sizeof(t_topo_pci),3);
    memset(topo->pci, 0, topo->nb_pci * sizeof(t_topo_pci));
    }

 if (topo->nb_bridges)
    {
    len = topo->nb_bridges * sizeof(t_topo_bridges);
    topo->bridges = (t_topo_bridges *)clownix_malloc(len,3);
    memset(topo->bridges, 0, len);
    }

 if (topo->nb_mirrors)
    {
    len = topo->nb_mirrors * sizeof(t_topo_mirrors);
    topo->mirrors = (t_topo_mirrors *)clownix_malloc(len,3);
    memset(topo->mirrors, 0, len);
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
static void fill_topo_d2d(t_topo_d2d *topo_d2d, t_d2d_cnx *d2d)
{
  memset(topo_d2d, 0, sizeof(t_topo_d2d));
  strncpy(topo_d2d->name, d2d->name, MAX_NAME_LEN);
  strncpy(topo_d2d->dist_cloonix, d2d->dist_cloonix, MAX_NAME_LEN);
  strncpy(topo_d2d->lan, d2d->lan, MAX_NAME_LEN);
  topo_d2d->local_is_master = d2d->local_is_master;
  topo_d2d->dist_tcp_ip           = d2d->dist_tcp_ip;
  topo_d2d->dist_tcp_port         = d2d->dist_tcp_port;
  topo_d2d->loc_udp_ip            = d2d->loc_udp_ip;
  topo_d2d->dist_udp_ip           = d2d->dist_udp_ip;
  topo_d2d->loc_udp_port          = d2d->loc_udp_port;
  topo_d2d->dist_udp_port         = d2d->dist_udp_port;
  topo_d2d->tcp_connection_peered = d2d->tcp_connection_peered;
  topo_d2d->udp_connection_peered = d2d->udp_connection_peered;
  topo_d2d->ovs_lan_attach_ready  = d2d->ovs_lan_attach_ready;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
static void fill_topo_a2b(t_topo_a2b *topo_a2b, t_a2b_cnx *a2b)
{
  memset(topo_a2b, 0, sizeof(t_topo_a2b));
  strncpy(topo_a2b->name, a2b->name, MAX_NAME_LEN);
  topo_a2b->delay[0] = a2b->side[0].delay;
  topo_a2b->loss[0]  = a2b->side[0].loss;
  topo_a2b->qsize[0] = a2b->side[0].qsize;
  topo_a2b->bsize[0] = a2b->side[0].bsize;
  topo_a2b->brate[0] = a2b->side[0].brate;
  topo_a2b->delay[1] = a2b->side[1].delay;
  topo_a2b->loss[1]  = a2b->side[1].loss;
  topo_a2b->qsize[1] = a2b->side[1].qsize;
  topo_a2b->bsize[1] = a2b->side[1].bsize;
  topo_a2b->brate[1] = a2b->side[1].brate;
}
/*---------------------------------------------------------------------------*/



/*****************************************************************************/
t_topo_info *cfg_produce_topo_info(void)
{
  int i, nb_vm, nb_endp, nb_dpdk_ovs_endp;
  int  nb_vhost_endp, nb_endp_edp, nb_d2d, nb_endp_d2d, nb_a2b, nb_endp_a2b;
  int i_c2c=0, i_sat=0, i_endp=0; 
  t_vm  *vm  = cfg_get_first_vm(&nb_vm);
  t_endp *next, *cur;
  t_topo_phy *phy;
  t_topo_pci *pci;
  t_topo_bridges *bridges;
  t_topo_mirrors *mirrors;
  t_topo_endp *dpdk_ovs_endp=dpdk_ovs_translate_topo_endp(&nb_dpdk_ovs_endp);
  t_topo_endp *vhost_endp = vhost_eth_translate_topo_endp(&nb_vhost_endp);
  t_topo_endp *edp_endp = edp_mngt_translate_topo_endp(&nb_endp_edp);
  t_topo_endp *d2d_endp = dpdk_d2d_mngt_translate_topo_endp(&nb_endp_d2d);
  t_topo_endp *a2b_endp = dpdk_a2b_mngt_translate_topo_endp(&nb_endp_a2b);
  int nb_phy = suid_power_get_topo_phy(&phy);
  int nb_pci = suid_power_get_topo_pci(&pci);
  int nb_bridges = suid_power_get_topo_bridges(&bridges);
  int nb_mirrors = suid_power_get_topo_mirrors(&mirrors);
  t_d2d_cnx *d2d = dpdk_d2d_get_first(&nb_d2d);
  t_a2b_cnx *a2b = dpdk_a2b_get_first(&nb_a2b);

  t_topo_info *topo = alloc_all_fields(nb_vm, nb_dpdk_ovs_endp, nb_vhost_endp,
                                       nb_endp_edp, nb_endp_d2d, nb_endp_a2b,
                                       nb_phy, nb_pci, nb_bridges, nb_mirrors,
                                       nb_d2d, nb_a2b);

  memcpy(&(topo->clc), &(cfg.clc), sizeof(t_topo_clc));

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

  if (topo->nb_d2d)
    {
    for (i=0; i<topo->nb_d2d; i++)
      {
      if (!d2d)
        KOUT(" ");
      fill_topo_d2d(&(topo->d2d[i]), d2d);
      d2d = d2d->next;
      }
    if (d2d)
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

  cur = endp_mngt_get_first(&nb_endp);
  for (i=0; i<nb_endp; i++)
    {
    if (!cur)
      KOUT("%d %d", nb_endp, i);
    next = endp_mngt_get_next(cur);
    if (cur->num == 0)
      {
      switch (cur->endp_type)
        {

        case endp_type_c2c:
          if (cur->c2c.name[0])
            { 
            if (i_c2c == topo->nb_c2c)
              KOUT(" ");
            fill_topo_c2c(&(topo->c2c[i_c2c]), cur);
            i_c2c += 1;
            }
          break;

  
        case endp_type_wif:
          if (cur->pid != 0)
            {
            if (i_sat == topo->nb_sat)
              KOUT(" ");
            fill_topo_sat(&(topo->sat[i_sat]), cur);
            i_sat += 1;
            }
          break;

        case endp_type_kvm_sock:
        case endp_type_kvm_dpdk:
        case endp_type_kvm_vhost:
        case endp_type_kvm_wlan:
        case endp_type_phy:
        case endp_type_pci:
        case endp_type_tap:
        case endp_type_snf:
        case endp_type_nat:
          break;
  
        default:
          KOUT("%d", cur->endp_type);
        }
      }
    if ((cur->pid != 0) || (cur->endp_type == endp_type_kvm_wlan))
      {
      fill_topo_endp(&(topo->endp[i_endp]), cur);
      i_endp += 1; 
      }
    clownix_free(cur, __FUNCTION__);
    cur = next;
    }
  if (cur)
    KOUT(" ");
  for (i=0; i<nb_dpdk_ovs_endp; i++)
    {
    if (dpdk_ovs_endp[i].type != endp_type_kvm_dpdk)
      KOUT("%d", dpdk_ovs_endp[i].type);
    if (i_endp == topo->nb_endp)
      KOUT(" ");
    copy_endp(&(topo->endp[i_endp]), &(dpdk_ovs_endp[i]));
    i_endp += 1;
    }

  for (i=0; i<nb_endp_d2d; i++)
    {
    if (i_endp == topo->nb_endp)
      KOUT(" ");
    copy_endp(&(topo->endp[i_endp]), &(d2d_endp[i]));
    i_endp += 1;
    }

  for (i=0; i<nb_endp_a2b; i++)
    {
    if (i_endp == topo->nb_endp)
      KOUT(" ");
    copy_endp(&(topo->endp[i_endp]), &(a2b_endp[i]));
    i_endp += 1;
    }


  for (i=0; i<nb_vhost_endp; i++)
    {
    if (i_endp == topo->nb_endp)
      KOUT(" ");
    copy_endp(&(topo->endp[i_endp]), &(vhost_endp[i]));
    i_endp += 1;
    }

  for (i=0; i<nb_endp_edp; i++)
    {
    if (i_endp == topo->nb_endp)
      KOUT(" ");
    copy_endp(&(topo->endp[i_endp]), &(edp_endp[i]));
    i_endp += 1;
    if (i_sat == topo->nb_sat)
      KOUT(" ");
    strncpy(topo->sat[i_sat].name, edp_endp[i].name, MAX_NAME_LEN-1);
    topo->sat[i_sat].type = edp_endp[i].type;
    i_sat += 1;
    }

  for (i=0; i<nb_phy; i++)
    {
    memcpy(&(topo->phy[i]), &(phy[i]), sizeof(t_topo_phy)); 
    } 

  for (i=0; i<nb_pci; i++)
    {
    memcpy(&(topo->pci[i]), &(pci[i]), sizeof(t_topo_pci)); 
    } 

  for (i=0; i<nb_bridges; i++)
    {
    memcpy(&(topo->bridges[i]), &(bridges[i]), sizeof(t_topo_bridges));
    }

  for (i=0; i<nb_mirrors; i++)
    {
    memcpy(&(topo->mirrors[i]), &(mirrors[i]), sizeof(t_topo_mirrors));
    }

  clownix_free(dpdk_ovs_endp, __FUNCTION__);
  clownix_free(vhost_endp, __FUNCTION__);
  clownix_free(edp_endp, __FUNCTION__);
  topo->nb_c2c = i_c2c;
  topo->nb_sat = i_sat;
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
        snprintf(vmname, MAX_NAME_LEN-1, topo_kvm->name);
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
void cfg_init(void)
{
  int i;
  memset(&cfg, 0, sizeof(t_cfg));
  for (i=0; i<MAX_VM; i++)
    vm_id_tab[i] = 0;
  head_zombie = NULL;
  nb_zombie = 0;
  glob_vm_id = 1;;
}
/*---------------------------------------------------------------------------*/
