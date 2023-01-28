/*****************************************************************************/
/*    Copyright (C) 2006-2023 clownix@clownix.net License AGPL-3             */
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
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include "io_clownix.h"
#include "lib_topo.h"
#include "doorways_sock.h"
#include "rpc_clownix.h"

#define IP "127.0.0.1"
#define PORT 0xA658
#define TST_BLKD_MAX_QTY 300
#define TST_MAX_QTY 55
#define MAX_INTF_TST 20
#define MAX_LEN 2000


static int i_am_client = 0;
static int count_status_ok;
static int count_status_ko;
static int count_work_dir_req=0;
static int count_work_dir_resp=0;
static int count_add_vm=0;
static int count_sav_vm=0;
static int count_del_name=0;
static int count_add_lan_endp=0;
static int count_del_lan_endp=0;
static int count_kill_uml_clownix=0;
static int count_del_all=0;
static int count_list_pid_req=0;
static int count_list_pid_resp=0;
static int count_list_commands_req=0;
static int count_list_commands_resp=0;
static int count_topo_small_event_sub=0;
static int count_topo_small_event_unsub=0;
static int count_topo_small_event=0;
static int count_event_topo_sub=0;
static int count_event_topo_unsub=0;
static int count_event_topo=0;
static int count_evt_print_sub=0;
static int count_evt_print_unsub=0;
static int count_event_print=0;
static int count_event_sys_sub=0;
static int count_event_sys_unsub=0;
static int count_event_sys=0;
static int count_vmcmd=0;

static int count_eventfull=0;
static int count_eventfull_sub=0;
static int count_slowperiodic_qcow2=0;
static int count_slowperiodic_img=0;
static int count_slowperiodic_sub=0;
static int count_slowperiodic_docker=0;
static int count_slowperiodic_podman=0;

static int count_evt_stats_endp_sub=0;
static int count_evt_stats_endp=0;
static int count_evt_stats_sysinfo_sub=0;
static int count_evt_stats_sysinfo=0;

static int count_evt_txt_doors = 0;
static int count_evt_txt_doors_sub = 0;
static int count_evt_txt_doors_unsub = 0;
static int count_hop_name_list = 0;
static int count_hop_get_name_list = 0;


static int count_sigdiag_msg = 0;
static int count_poldiag_msg = 0;
static int count_rpc_kil_req  = 0;
static int count_rpc_pid_req  = 0;
static int count_rpc_pid_resp = 0;
static int count_cmd_resp_txt = 0;
static int count_cmd_resp_txt_sub = 0;
static int count_cmd_resp_txt_unsub = 0;

static int count_snf_add = 0;
static int count_c2c_add = 0;
static int count_c2c_peer_create = 0;
static int count_c2c_peer_conf = 0;
static int count_c2c_peer_ping = 0;

static int count_color_item = 0;

static int count_nat_add = 0;
static int count_phy_add = 0;
static int count_tap_add = 0;
static int count_cnt_add = 0;

static int count_a2b_add = 0;
static int count_a2b_cnf = 0;
static int count_c2c_cnf = 0;
static int count_nat_cnf = 0;

/*****************************************************************************/
static void print_all_count(void)
{
  printf("\n\n");
  printf("START COUNT\n");
  printf("%d\n", count_status_ok);
  printf("%d\n", count_status_ko);
  printf("%d\n", count_add_vm);
  printf("%d\n", count_del_name);
  printf("%d\n", count_add_lan_endp);
  printf("%d\n", count_del_lan_endp);
  printf("%d\n", count_kill_uml_clownix);
  printf("%d\n", count_del_all);
  printf("%d\n", count_list_pid_req);
  printf("%d\n", count_list_pid_resp);
  printf("%d\n", count_list_commands_req);
  printf("%d\n", count_list_commands_resp);
  printf("%d\n", count_topo_small_event_sub);
  printf("%d\n", count_topo_small_event_unsub);
  printf("%d\n", count_topo_small_event);
  printf("%d\n", count_event_topo_sub);
  printf("%d\n", count_event_topo_unsub);
  printf("%d\n", count_event_topo);
  printf("%d\n", count_evt_print_sub);
  printf("%d\n", count_evt_print_unsub);
  printf("%d\n", count_event_print);
  printf("%d\n", count_event_sys_sub);
  printf("%d\n", count_event_sys_unsub);
  printf("%d\n", count_event_sys);
  printf("%d\n", count_work_dir_req);
  printf("%d\n", count_work_dir_resp);
  printf("%d\n", count_vmcmd);
  printf("%d\n", count_eventfull_sub);
  printf("%d\n", count_eventfull);
  printf("%d\n", count_slowperiodic_sub);
  printf("%d\n", count_slowperiodic_qcow2);
  printf("%d\n", count_slowperiodic_img);
  printf("%d\n", count_slowperiodic_docker);
  printf("%d\n", count_slowperiodic_podman);
  printf("%d\n", count_sav_vm);

  printf("%d\n", count_evt_stats_endp_sub);
  printf("%d\n", count_evt_stats_endp);

  printf("%d\n", count_evt_stats_sysinfo_sub);
  printf("%d\n", count_evt_stats_sysinfo);

  printf("%d\n", count_evt_txt_doors);
  printf("%d\n", count_evt_txt_doors_sub);
  printf("%d\n", count_evt_txt_doors_unsub);
  printf("%d\n", count_hop_name_list);
  printf("%d\n", count_hop_get_name_list);

  printf("%d\n", count_sigdiag_msg);
  printf("%d\n", count_poldiag_msg);
  printf("%d\n", count_rpc_pid_req);
  printf("%d\n", count_rpc_pid_resp);
  printf("%d\n", count_cmd_resp_txt);
  printf("%d\n", count_cmd_resp_txt_sub);
  printf("%d\n", count_cmd_resp_txt_unsub);

  printf("%d\n", count_snf_add);
  printf("%d\n", count_c2c_add);
  printf("%d\n", count_c2c_peer_create);
  printf("%d\n", count_c2c_peer_conf);
  printf("%d\n", count_c2c_peer_ping);

  printf("%d\n", count_color_item);

  printf("%d\n", count_tap_add);
  printf("%d\n", count_cnt_add);

  printf("%d\n", count_a2b_add);
  printf("%d\n", count_a2b_cnf);
  printf("%d\n", count_c2c_cnf);
  printf("%d\n", count_nat_cnf);

  printf("END COUNT\n");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int my_rand(int max)
{
  unsigned int result = rand();
  result %= max;
  return (int) result;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
static void random_choice_str(char *name, int max_len)
{
  int i, len = my_rand(max_len-1);
  len += 1;
  memset (name, 0 , max_len);
  for (i=0; i<len; i++)
    name[i] = 'A'+ my_rand(26);
  name[len] = 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void random_choice_dirty_str(char *name, int max_len)
{
  int i, len = rand() % (max_len-2);
  len += 1;
  memset (name, 0 , max_len);
  for (i=0; i<len; i++)
    {
    do
      {
      name[i] = (char )(rand());
      } while((name[i] < 0x20) || (name[i] > 0x7E));
    }
  name[len] = 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void random_eventfull_endp(t_eventfull_endp *endp)
{
  random_choice_str(endp->name, MAX_NAME_LEN);
  endp->num  = rand();
  endp->type = rand();
  endp->ram  = rand();
  endp->cpu  = rand();
  endp->ok   = rand();
  endp->ptx   = rand();
  endp->prx   = rand();
  endp->btx   = rand();
  endp->brx   = rand();
  endp->ms   = rand();
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void random_clc(t_topo_clc *conf)
{
  random_choice_str(conf->version,  MAX_NAME_LEN);
  random_choice_str(conf->network,  MAX_NAME_LEN);
  random_choice_str(conf->username, MAX_NAME_LEN);
  random_choice_str(conf->work_dir, MAX_PATH_LEN);
  random_choice_str(conf->bin_dir, MAX_PATH_LEN);
  random_choice_str(conf->bulk_dir, MAX_PATH_LEN);
  conf->server_port = rand();
  conf->flags_config = rand();
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void topo_config_diff(t_topo_clc *cfa, t_topo_clc *cfb)
{
  if (strcmp(cfa->version, cfb->version))
    KERR("%s %s", cfa->version, cfb->version);
  if (strcmp(cfa->network,  cfb->network))
    KERR("%s %s", cfa->network,  cfb->network);
  if (strcmp(cfa->username, cfb->username))
    KERR("%s %s", cfa->username, cfb->username);
  if (strcmp(cfa->work_dir, cfb->work_dir))
    KERR("%s %s", cfa->work_dir, cfb->work_dir);
  if (strcmp(cfa->bin_dir,  cfb->bin_dir))
    KERR("%s %s", cfa->bin_dir,  cfb->bin_dir);
  if (strcmp(cfa->bulk_dir, cfb->bulk_dir))
    KERR("%s %s", cfa->bulk_dir, cfb->bulk_dir);
  if (cfa->server_port != cfb->server_port)
    KERR("%d %d", cfa->server_port, cfb->server_port);
  if (cfa->flags_config != cfb->flags_config)
    KERR("%d %d", cfa->flags_config, cfb->flags_config);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int topo_cnt_diff(t_topo_cnt *icnt, t_topo_cnt *cnt)
{
  int k, l, result = 0;
  if (cnt->vm_id != icnt->vm_id)
    {
    KERR("%d %d", cnt->vm_id, icnt->vm_id);
    result = -1;
    }
  else if (cnt->is_persistent != icnt->is_persistent)
    {
    KERR("%d %d", cnt->vm_id, icnt->vm_id);
    result = -1;
    }
  else if (cnt->ping_ok != icnt->ping_ok)
    {
    KERR("%d %d", cnt->ping_ok, icnt->ping_ok);
    result = -1;
    }
  else if (strcmp(cnt->brandtype, icnt->brandtype))
    {
    KERR("%s %s", cnt->brandtype, icnt->brandtype);
    result = -1;
    }
  else if (strcmp(cnt->name, icnt->name))
    {
    KERR("%s %s", cnt->name, icnt->name);
    result = -1;
    }
  else   if (strcmp(cnt->image, icnt->image))
    {
    KERR("%s %s", cnt->image, icnt->image);
    result = -1;
    }
  else   if (strcmp(cnt->customer_launch, icnt->customer_launch))
    {
    KERR("%s %s", cnt->customer_launch, icnt->customer_launch);
    result = -1;
    }
  else   if (strcmp(cnt->startup_env, icnt->startup_env))
    {
    KERR("%s %s", cnt->startup_env, icnt->startup_env);
    result = -1;
    }

  else
    {
    for (k=0; k < MAX_ETH_VM; k++)
      {
      if (cnt->eth_table[k].endp_type != icnt->eth_table[k].endp_type)
        {
        KERR("%d %d",cnt->eth_table[k].endp_type,icnt->eth_table[k].endp_type);
        result = -1;
        }
      if (cnt->eth_table[k].randmac != icnt->eth_table[k].randmac)
        {
        KERR("%d %d",cnt->eth_table[k].randmac,icnt->eth_table[k].randmac);
        result = -1;
        }
      if (strcmp(cnt->eth_table[k].vhost_ifname,
                 icnt->eth_table[k].vhost_ifname))
        {
        KERR("%s %s", cnt->eth_table[k].vhost_ifname,
                      icnt->eth_table[k].vhost_ifname);
        result = -1;
        }
      for (l=0; l < 6; l++)
        {
        if (cnt->eth_table[k].mac_addr[l] != icnt->eth_table[k].mac_addr[l])
          {
          KERR("%d %d", cnt->eth_table[k].mac_addr[l],
                        icnt->eth_table[k].mac_addr[l]);
          result = -1;
          }
        }
      }
    if (memcmp(cnt->eth_table, icnt->eth_table,
       MAX_ETH_VM*sizeof(t_eth_table)))
      KERR(" ");
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int topo_kvm_diff(t_topo_kvm *ikvm, t_topo_kvm *kvm)
{
  int k, l, result = 0;
  if (strcmp(kvm->name, ikvm->name))
    {
    KERR("%s %s", kvm->name, ikvm->name);
    result = -1;
    }
  if (strcmp(kvm->linux_kernel, ikvm->linux_kernel))
    {
    KERR("%s %s", kvm->linux_kernel, ikvm->linux_kernel);
    result = -1;
    }
  if (strcmp(kvm->rootfs_input, ikvm->rootfs_input))
    {
    KERR("%s %s", kvm->rootfs_input, ikvm->rootfs_input);
    result = -1;
    }
  if (strcmp(kvm->rootfs_used, ikvm->rootfs_used))
    {
    KERR("%s %s", kvm->rootfs_used, ikvm->rootfs_used);
    result = -1;
    }
  if (strcmp(kvm->rootfs_backing, ikvm->rootfs_backing))
    {
    KERR("%s %s", kvm->rootfs_backing, ikvm->rootfs_backing);
    result = -1;
    }
  if (strcmp(kvm->install_cdrom, ikvm->install_cdrom))
    {
    KERR("%s %s", kvm->install_cdrom, ikvm->install_cdrom);
    result = -1;
    }
  if (strcmp(kvm->added_cdrom, ikvm->added_cdrom))
    {
    KERR("%s %s", kvm->added_cdrom, ikvm->added_cdrom);
    result = -1;
    }
  if (strcmp(kvm->added_disk, ikvm->added_disk))
    {
    KERR("%s %s", kvm->added_disk, ikvm->added_disk);
    result = -1;
    }
  if (strcmp(kvm->p9_host_share, ikvm->p9_host_share))
    {
    KERR("%s %s", kvm->p9_host_share, ikvm->p9_host_share);
    result = -1;
    }
  if (kvm->vm_id != ikvm->vm_id)
    {
    KERR("%d %d", kvm->vm_id, ikvm->vm_id);
    result = -1;
    }
  if (kvm->cpu != ikvm->cpu)
    {
    KERR("%d %d", kvm->cpu, ikvm->cpu);
    result = -1;
    }
  if (kvm->mem != ikvm->mem)
    {
    KERR("%d %d", kvm->mem, ikvm->mem);
    result = -1;
    }
  if (kvm->vm_config_flags != ikvm->vm_config_flags)
    {
    KERR("%d %d", kvm->vm_config_flags, ikvm->vm_config_flags);
    result = -1;
    }
  if (kvm->vm_config_param != ikvm->vm_config_param)
    {
    KERR("%d %d", kvm->vm_config_param, ikvm->vm_config_param);
    result = -1;
    }
  if (kvm->nb_tot_eth != ikvm->nb_tot_eth)
    {
    KERR("%d %d", kvm->nb_tot_eth, ikvm->nb_tot_eth);
    result = -1;
    }
  else
    {
    for (k=0; k < MAX_ETH_VM; k++)
      {
      if (kvm->eth_table[k].endp_type != ikvm->eth_table[k].endp_type)
        {
        KERR("%d %d",kvm->eth_table[k].endp_type,ikvm->eth_table[k].endp_type);
        result = -1;
        }
      if (kvm->eth_table[k].randmac != ikvm->eth_table[k].randmac)
        {
        KERR("%d %d",kvm->eth_table[k].randmac,ikvm->eth_table[k].randmac);
        result = -1;
        }
      if (strcmp(kvm->eth_table[k].vhost_ifname,
                 ikvm->eth_table[k].vhost_ifname))
        {
        KERR("%s %s", kvm->eth_table[k].vhost_ifname,
                      ikvm->eth_table[k].vhost_ifname);
        result = -1;
        }
      for (l=0; l < 6; l++)
        {
        if (kvm->eth_table[k].mac_addr[l] != ikvm->eth_table[k].mac_addr[l])
          {
          KERR("%d %d", kvm->eth_table[k].mac_addr[l], 
                        ikvm->eth_table[k].mac_addr[l]);
          result = -1;
          }
        }
      }
    if (memcmp(kvm->eth_table, ikvm->eth_table,
       MAX_ETH_VM*sizeof(t_eth_table)))
      KERR(" ");
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void random_cnt(t_topo_cnt *cnt)
{
  int k, l;
  random_choice_str(cnt->brandtype, MAX_NAME_LEN);
  random_choice_str(cnt->name, MAX_NAME_LEN);
  random_choice_str(cnt->image, MAX_PATH_LEN);
  random_choice_str(cnt->customer_launch, MAX_PATH_LEN);
  random_choice_str(cnt->startup_env, MAX_PATH_LEN);
  cnt->nb_tot_eth = my_rand(MAX_ETH_VM);
  cnt->ping_ok = my_rand(20);
  cnt->vm_id = my_rand(120);
  cnt->is_persistent = my_rand(120);
  for (k=0; k < cnt->nb_tot_eth; k++)
    {
    cnt->eth_table[k].endp_type = (rand() & 0x04) + 1;
    cnt->eth_table[k].randmac = rand();
    random_choice_str(cnt->eth_table[k].vhost_ifname, MAX_NAME_LEN-1);
    for (l=0; l < 6; l++)
      cnt->eth_table[k].mac_addr[l] = rand() & 0xFF;
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void random_kvm(t_topo_kvm *kvm)
{
  int k, l;
  if (my_rand(10))
    random_choice_str(kvm->name, MAX_NAME_LEN);
  if (my_rand(10))
    random_choice_str(kvm->rootfs_input, MAX_PATH_LEN);
  if (my_rand(10))
    random_choice_str(kvm->linux_kernel, MAX_NAME_LEN);
  if (my_rand(10))
    random_choice_str(kvm->rootfs_used, MAX_PATH_LEN);
  if (my_rand(10))
    random_choice_str(kvm->rootfs_backing, MAX_PATH_LEN);
  if (my_rand(10))
    random_choice_str(kvm->install_cdrom, MAX_PATH_LEN);
  if (my_rand(10))
    random_choice_str(kvm->added_cdrom, MAX_PATH_LEN);
  if (my_rand(10))
    random_choice_str(kvm->added_disk, MAX_PATH_LEN);
  if (my_rand(10))
    random_choice_str(kvm->p9_host_share, MAX_PATH_LEN);
  kvm->vm_id = rand();
  kvm->cpu = rand();
  kvm->mem = rand();
  kvm->vm_config_flags = rand();
  kvm->vm_config_param = rand();
  kvm->nb_tot_eth = my_rand(MAX_ETH_VM);
  for (k=0; k < kvm->nb_tot_eth; k++)
    {
    kvm->eth_table[k].endp_type = (rand() & 0x04) + 1;
    kvm->eth_table[k].randmac = rand();
    random_choice_str(kvm->eth_table[k].vhost_ifname, MAX_NAME_LEN-1);
    for (l=0; l < 6; l++)
      kvm->eth_table[k].mac_addr[l] = rand() & 0xFF;
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void random_c2c(t_topo_c2c *c2c)
{
  random_choice_str(c2c->name, MAX_NAME_LEN);
  random_choice_str(c2c->dist_cloon, MAX_NAME_LEN);
  random_choice_str(c2c->lan, MAX_NAME_LEN);
  c2c->local_is_master = rand();
  c2c->dist_tcp_ip = rand();
  c2c->dist_tcp_port = rand();
  c2c->loc_udp_ip = rand();
  c2c->dist_udp_ip = rand();
  c2c->loc_udp_port = rand();
  c2c->dist_udp_port = rand();
  c2c->tcp_connection_peered = rand();
  c2c->udp_connection_peered = rand();
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void random_nat(t_topo_nat *nat)
{
  random_choice_str(nat->name, MAX_NAME_LEN);
  nat->endp_type = rand();
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void random_phy(t_topo_phy *phy)
{
  random_choice_str(phy->name, MAX_NAME_LEN);
  phy->endp_type = rand();
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void random_tap(t_topo_tap *tap)
{
  random_choice_str(tap->name, MAX_NAME_LEN);
  tap->endp_type = rand();
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void random_a2b(t_topo_a2b *a2b)
{
  random_choice_str(a2b->name, MAX_NAME_LEN);
  a2b->endp_type0 = rand();
  a2b->endp_type1 = rand();
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void random_info_phy(t_topo_info_phy *phy)
{
  random_choice_str(phy->name, MAX_NAME_LEN);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void random_bridges(t_topo_bridges *bridges)
{
  int i;
  random_choice_str(bridges->br, MAX_NAME_LEN);
  bridges->nb_ports = my_rand(20);
  for (i=0; i<bridges->nb_ports; i++)
    {
    random_choice_str(bridges->ports[i], MAX_NAME_LEN);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void topo_vlg_gen(t_lan_group *vlg)
{
  int i, len;
  vlg->nb_lan = my_rand(20);
  len = vlg->nb_lan * sizeof(t_lan_group_item);
  vlg->lan = (t_lan_group_item *) clownix_malloc(len, 3);
  memset(vlg->lan, 0, len);
  for (i=0; i<vlg->nb_lan; i++)
    {
    random_choice_str(vlg->lan[i].lan, MAX_NAME_LEN);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void random_endp(t_topo_endp *endp)
{
  random_choice_str(endp->name, MAX_NAME_LEN);
  endp->num = rand();
  endp->type = rand();
  topo_vlg_gen(&(endp->lan));
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static t_topo_info *random_topo_gen(void)
{
  int i;
  t_topo_info *topo = (t_topo_info *) clownix_malloc(sizeof(t_topo_info), 3);
  memset(topo, 0, sizeof(t_topo_info));

  random_clc(&(topo->clc));
  topo->conf_rank = my_rand(50);
  topo->nb_cnt = my_rand(10);
  topo->nb_kvm = my_rand(10);
  topo->nb_c2c = my_rand(10);
  topo->nb_a2b = my_rand(10);
  topo->nb_tap = my_rand(10);
  topo->nb_nat = my_rand(10);
  topo->nb_phy = my_rand(10);
  topo->nb_endp = my_rand(50);
  topo->nb_info_phy = my_rand(10);
  topo->nb_bridges = my_rand(10);

  if (topo->nb_cnt)
    {
    topo->cnt =
    (t_topo_cnt *)clownix_malloc(topo->nb_cnt * sizeof(t_topo_cnt),3);
    memset(topo->cnt, 0, topo->nb_cnt * sizeof(t_topo_cnt));
    for (i=0; i<topo->nb_cnt; i++)
      {
      random_cnt(&(topo->cnt[i]));
      }
    }

  if (topo->nb_kvm)
    {
    topo->kvm =
    (t_topo_kvm *)clownix_malloc(topo->nb_kvm * sizeof(t_topo_kvm),3);
    memset(topo->kvm, 0, topo->nb_kvm * sizeof(t_topo_kvm));
    for (i=0; i<topo->nb_kvm; i++)
      {
      random_kvm(&(topo->kvm[i]));
      memset(topo->kvm[i].rootfs_input, 0, MAX_PATH_LEN);
      }
    }

  if (topo->nb_c2c)
    {
    topo->c2c =
    (t_topo_c2c *)clownix_malloc(topo->nb_c2c * sizeof(t_topo_c2c),3);
    memset(topo->c2c, 0, topo->nb_c2c * sizeof(t_topo_c2c));
    for (i=0; i<topo->nb_c2c; i++)
      random_c2c(&(topo->c2c[i]));
    }

  if (topo->nb_a2b)
    {
    topo->a2b =
    (t_topo_a2b *)clownix_malloc(topo->nb_a2b * sizeof(t_topo_a2b),3);
    memset(topo->a2b, 0, topo->nb_a2b * sizeof(t_topo_a2b));
    for (i=0; i<topo->nb_a2b; i++)
      random_a2b(&(topo->a2b[i]));
    }

  if (topo->nb_nat)
    {
    topo->nat =
    (t_topo_nat *)clownix_malloc(topo->nb_nat * sizeof(t_topo_nat),3);
    memset(topo->nat, 0, topo->nb_nat * sizeof(t_topo_nat));
    for (i=0; i<topo->nb_nat; i++)
      random_nat(&(topo->nat[i]));
    }

  if (topo->nb_phy)
    {
    topo->phy =
    (t_topo_phy *)clownix_malloc(topo->nb_phy * sizeof(t_topo_phy),3);
    memset(topo->phy, 0, topo->nb_phy * sizeof(t_topo_phy));
    for (i=0; i<topo->nb_phy; i++)
      random_phy(&(topo->phy[i]));
    }

  if (topo->nb_tap)
    {
    topo->tap =
    (t_topo_tap *)clownix_malloc(topo->nb_tap * sizeof(t_topo_tap),3);
    memset(topo->tap, 0, topo->nb_tap * sizeof(t_topo_tap));
    for (i=0; i<topo->nb_tap; i++)
      random_tap(&(topo->tap[i]));
    }

  if (topo->nb_endp)
    {
    topo->endp =
    (t_topo_endp *)clownix_malloc(topo->nb_endp * sizeof(t_topo_endp),3);
    memset(topo->endp, 0, topo->nb_endp * sizeof(t_topo_endp));
    for (i=0; i<topo->nb_endp; i++)
      random_endp(&(topo->endp[i]));
    }

  if (topo->nb_info_phy)
    {
    topo->info_phy =
    (t_topo_info_phy *)clownix_malloc(topo->nb_info_phy * sizeof(t_topo_info_phy),3);
    memset(topo->info_phy, 0, topo->nb_info_phy * sizeof(t_topo_info_phy));
    for (i=0; i<topo->nb_info_phy; i++)
      random_info_phy(&(topo->info_phy[i]));
    }

  if (topo->nb_bridges)
    {
    topo->bridges =
    (t_topo_bridges *)clownix_malloc(topo->nb_bridges*sizeof(t_topo_bridges),3);
    memset(topo->bridges, 0, topo->nb_bridges*sizeof(t_topo_bridges));
    for (i=0; i<topo->nb_bridges; i++)
      random_bridges(&(topo->bridges[i]));
    }

  return topo;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void heartbeat (int delta)
{
  int i;
  unsigned long *mallocs;
  static int count = 0;
  count++;
  if (!(count%1000))
    {
    print_all_count();
    printf("MALLOCS: delta %d \n", delta);
    mallocs = get_clownix_malloc_nb();
    for (i=0; i<MAX_MALLOC_TYPES; i++)
      printf("%d:%02lu ", i, mallocs[i]);
    printf("\n\n");
    }
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void rpct_recv_pid_req(int llid, int itid, char *iname, int inum)
{
  static int tid;
  static int num;
  static char name[MAX_NAME_LEN];
  if (i_am_client)
    {
    if (count_rpc_pid_req)
      {
      if (tid != itid)
        KOUT(" ");
      if (num != inum)
        KOUT(" ");
      if (strcmp(name, iname))
        KOUT(" ");
      } 
    tid = rand();
    num = rand();
    memset(name, 0, MAX_NAME_LEN);
    random_choice_str(name, MAX_NAME_LEN);
    rpct_send_pid_req(llid, tid, name, num);
    }
  else
    rpct_send_pid_req(llid, itid, iname, inum);
  count_rpc_pid_req++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_kil_req(int llid, int itid)
{
  static int tid;
  if (i_am_client)
    {
    if (count_rpc_kil_req)
      {
      if (tid != itid)
        KOUT(" ");
      }
    tid = rand();
    rpct_send_kil_req(llid, tid);
    }
  else
    rpct_send_kil_req(llid, itid);
  count_rpc_kil_req++;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void rpct_recv_pid_resp(int llid, int itid,
                       char *iname, int inum, int itoppid, int ipid)
{
  static int tid, pid, num, toppid;
  static char name[MAX_NAME_LEN];
  if (i_am_client)
    {
    if (count_rpc_pid_resp)
      {
      if (tid != itid)
        KOUT(" ");
      if (num != inum)
        KOUT(" ");
      if (pid != ipid)
        KOUT(" ");
      if (toppid != itoppid)
        KOUT(" ");
      if (strcmp(name, iname))
        KOUT(" ");
      }
    tid = rand();
    pid = rand();
    toppid = rand();
    num = rand();
    memset(name, 0, MAX_NAME_LEN);
    random_choice_str(name, MAX_NAME_LEN);
    rpct_send_pid_resp(llid, tid, name, num, toppid, pid);
    }
  else
    rpct_send_pid_resp(llid, itid, iname, inum, itoppid, ipid);
  count_rpc_pid_resp++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_hop_sub(int llid, int itid, int itype_evt)
{
  static int tid;
  static int type_evt;
  if (i_am_client)
    {
    if (count_cmd_resp_txt_sub)
      {
      if (tid != itid)
        KOUT(" ");
      if (type_evt != itype_evt)
        KOUT(" ");
      }
    tid = rand();
    type_evt = rand();
    rpct_send_hop_sub(llid, tid, type_evt);
    }
  else
    rpct_send_hop_sub(llid, itid, itype_evt);
  count_cmd_resp_txt_sub++;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void rpct_recv_hop_unsub(int llid, int itid)
{
  static int tid;
  if (i_am_client)
    {
    if (count_cmd_resp_txt_unsub)
      {
      if (tid != itid)
        KOUT(" ");
      }
    tid = rand();
    rpct_send_hop_unsub(llid, tid);
    }
  else
    rpct_send_hop_unsub(llid, itid);
  count_cmd_resp_txt_unsub++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_hop_msg(int llid, int itid,
                      int itype_evt, char *itxt)
{
  static int tid;
  static int type_evt;
  static char txt[MAX_LEN];
  if (i_am_client)
    {
    if (count_cmd_resp_txt)
      {
      if (tid != itid)
        KOUT(" ");
      if (type_evt != itype_evt)
        KOUT(" ");
      if (strcmp(txt, itxt))
        KOUT(" ");
      }
    tid = rand();
    type_evt = rand();
    memset(txt, 0, MAX_LEN);
    random_choice_dirty_str(txt, MAX_LEN);
    rpct_send_hop_msg(llid, tid, type_evt, txt);
    }
  else
    rpct_send_hop_msg(llid, itid, itype_evt, itxt);
  count_cmd_resp_txt++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_sigdiag_msg(int llid, int itid, char *iline)
{
  static char line[2001];
  static int tid;
  if (i_am_client)
    {
    if (count_sigdiag_msg)
      {
      if (strcmp(iline, line))
        KOUT(" ");
      if (itid != tid)
        KOUT(" ");
      }
    tid = rand();
    random_choice_str(line, 2000);
    rpct_send_sigdiag_msg(llid, tid, line);
    }
  else
    rpct_send_sigdiag_msg(llid, itid, iline);
  count_sigdiag_msg++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_poldiag_msg(int llid, int itid, char *iline)
{
  static char line[2001];
  static int tid;
  if (i_am_client)
    {
    if (count_poldiag_msg)
      {
      if (strcmp(iline, line))
        KOUT(" ");
      if (itid != tid)
        KOUT(" ");
      }
    tid = rand();
    random_choice_str(line, 2000);
    rpct_send_poldiag_msg(llid, tid, line);
    }
  else
    rpct_send_poldiag_msg(llid, itid, iline);
  count_poldiag_msg++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_hop_get_name_list_doors(int llid, int itid)
{
  static int tid;
  if (i_am_client)
    {
    if (count_hop_get_name_list)
      {
      if (itid != tid)
        KOUT(" ");
      }
    tid = rand();
    send_hop_get_name_list_doors(llid, tid);
    }
  else
    send_hop_get_name_list_doors(llid, itid);
  count_hop_get_name_list++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_hop_name_list_doors(int llid, int itid, int inb, t_hop_list *ilist)
{
  int i;
  static int tid;
  static int nb;
  static t_hop_list list[51];
  if (i_am_client)
    {
    if (count_hop_name_list)
      {
      if (inb != nb)
        KOUT("%d %d", inb, nb);
      if (itid != tid)
        KOUT(" ");
      for (i=0; i<nb; i++)
        if (memcmp(&(ilist[i]),  &(list[i]), sizeof(t_hop_list)))
          KOUT("%d \n%s \n\n%s", i, ilist[i].name, list[i].name);
      }
    tid = rand();
    nb = rand()%50;
    memset(list, 0, nb*sizeof(t_hop_list));
    for (i=0; i<nb; i++)
      {
      list[i].type_hop = rand();
      list[i].num = rand();
      random_choice_str(list[i].name, MAX_NAME_LEN);
      }
    send_hop_name_list_doors(llid, tid, nb, list);
    }
  else
    send_hop_name_list_doors(llid, itid, inb, ilist);
  count_hop_name_list++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_hop_evt_doors_sub(int llid, int itid, int iflags_hop,
                            int inb, t_hop_list *ilist)
{
  int i;
  static int tid;
  static int flags_hop;
  static int nb;
  static t_hop_list list[51];
  if (i_am_client)
    {
    if (count_evt_txt_doors_sub)
      {
      if (inb != nb)
        KOUT("%d %d", inb, nb);
      if (iflags_hop != flags_hop)
        KOUT(" ");
      if (itid != tid)
        KOUT(" ");
      for (i=0; i<nb; i++)
        if (memcmp(&(ilist[i]),  &(list[i]), sizeof(t_hop_list)))
          KOUT("%d \n%s \n\n%s", i, ilist[i].name, list[i].name);
      }
    tid = rand();
    flags_hop = rand();
    nb = rand()%50;
    memset(list, 0, nb*sizeof(t_hop_list));
    for (i=0; i<nb; i++)
      {
      list[i].type_hop = rand();
      list[i].num = rand();
      random_choice_str(list[i].name, MAX_NAME_LEN);
      }
    send_hop_evt_doors_sub(llid, tid, flags_hop, nb, list);
    }
  else
    send_hop_evt_doors_sub(llid, itid, iflags_hop, inb, ilist);
  count_evt_txt_doors_sub++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_hop_evt_doors_unsub(int llid, int itid)
{
  static int tid;
  if (i_am_client)
    {
    if (count_evt_txt_doors_unsub)
      {
      if (itid != tid)
        KOUT(" ");
      }
    tid = rand();
    send_hop_evt_doors_unsub(llid, tid);
    }
  else
    send_hop_evt_doors_unsub(llid, itid);
  count_evt_txt_doors_unsub++;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void recv_hop_evt_doors(int llid, int itid, int iflags_hop,
                        char *iname, char *itxt)
{
  static int tid;
  static int flags_hop;
  static char name[MAX_NAME_LEN];
  static char txt[MAX_LEN];
  if (i_am_client)
    {
    if (count_evt_txt_doors)
      {
      if (tid != itid)
        KOUT(" ");
      if (flags_hop != iflags_hop)
        KOUT(" ");
      if (strcmp(txt, itxt))
        KOUT(" ");
      if (strcmp(name, iname))
        KOUT(" ");
      }
    tid = rand();
    flags_hop = rand();
    random_choice_str(name, MAX_NAME_LEN);
    memset(txt, 0, MAX_LEN);
    random_choice_dirty_str(txt, MAX_LEN);
    send_hop_evt_doors(llid, tid, flags_hop, name, txt);
    }
  else
    send_hop_evt_doors(llid, itid, iflags_hop, iname, itxt);
  count_evt_txt_doors++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_evt_stats_endp_sub(int llid, int itid, char *iname, int inum, int isub)
{
  static char name[MAX_NAME_LEN];
  static int tid, sub, num;
  if (i_am_client)
    {
    if (count_evt_stats_endp_sub)
      {
      if (strcmp(iname, name))
        KOUT(" ");
      if (itid != tid)
        KOUT(" ");
      if (isub != sub)
        KOUT(" ");
      if (inum != num)
        KOUT(" ");
      }
    tid = rand();
    num = rand();
    sub = rand();
    random_choice_str(name, MAX_NAME_LEN);
    send_evt_stats_endp_sub(llid, tid, name, num, sub);
    }
  else
    send_evt_stats_endp_sub(llid, itid, iname, inum, isub);
  count_evt_stats_endp_sub++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int diff_stats_counts(t_stats_counts *sca, t_stats_counts *scb)
{
  int i, nb, ok = 1, result = -1;
  if (sca->nb_items != scb->nb_items)
    KERR("%d %d", sca->nb_items, scb->nb_items);
  else
    {
    nb = sca->nb_items;
    for (i=0; i<nb; i++)
      {
      if (sca->item[i].time_ms != scb->item[i].time_ms)
        {
        KERR("%d %d", i, nb);
        ok = 0;
        }
      if (sca->item[i].ptx != scb->item[i].ptx)
        {
        KERR("%d %d", i, nb);
        ok = 0;
        }
      if (sca->item[i].btx != scb->item[i].btx)
        {
        KERR("%d %d", i, nb);
        ok = 0;
        }
      if (sca->item[i].prx != scb->item[i].prx)
        {
        KERR("%d %d", i, nb);
        ok = 0;
        }
      if (sca->item[i].brx != scb->item[i].brx)
        {
        KERR("%d %d", i, nb);
        ok = 0;
        }
      }
    }
  if(ok)
    result = 0;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void random_stats_counts(t_stats_counts *sc)
{
  int i;
  memset(sc, 0, sizeof(t_stats_counts));
  sc->nb_items = (rand() % (MAX_STATS_ITEMS + 1));
  for (i=0; i<sc->nb_items; i++)
    {
    sc->item[i].time_ms = rand();  
    sc->item[i].ptx = rand();  
    sc->item[i].prx = rand();  
    sc->item[i].btx = rand();  
    sc->item[i].brx = rand();  
    } 
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_evt_stats_endp(int llid, int itid, char *inetwork, 
                        char *iname, int inum, 
                        t_stats_counts *istats_counts, int istatus)
{
  static char network[MAX_NAME_LEN];
  static char name[MAX_NAME_LEN];
  static int tid, num, status;
  static t_stats_counts stats_counts;
  if (i_am_client)
    {
    if (count_evt_stats_endp)
      {
      if (strcmp(inetwork, network))
        KOUT(" ");
      if (strcmp(iname, name))
        KOUT(" ");
      if (itid != tid)
        KOUT(" ");
      if (istatus != status)
        KOUT(" ");
      if (inum != num)
        KOUT(" ");
      if (diff_stats_counts(&stats_counts, istats_counts))
        KOUT(" ");
      } 
    tid = rand();
    num = rand();
    status = rand();
    random_stats_counts(&stats_counts);
    random_choice_str(network, MAX_NAME_LEN); 
    random_choice_str(name, MAX_NAME_LEN); 
    send_evt_stats_endp(llid, tid, network, name, num, &stats_counts, status);
    }
  else
    send_evt_stats_endp(llid, itid, inetwork, iname, inum, istats_counts, istatus);
  count_evt_stats_endp++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_evt_stats_sysinfo_sub(int llid, int itid, char *iname, int isub)
{
  static char name[MAX_NAME_LEN];
  static int tid, sub;
  if (i_am_client)
    {
    if (count_evt_stats_sysinfo_sub)
      {
      if (strcmp(iname, name))
        KOUT(" ");
      if (itid != tid)
        KOUT(" ");
      if (isub != sub)
        KOUT(" ");
      }
    tid = rand();
    sub = rand();
    random_choice_str(name, MAX_NAME_LEN);
    send_evt_stats_sysinfo_sub(llid, tid, name, sub);
    }
  else
    send_evt_stats_sysinfo_sub(llid, itid, iname, isub);
  count_evt_stats_sysinfo_sub++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void diff_stats_sysinfo(t_stats_sysinfo *one, t_stats_sysinfo *two)
{
  if (one->time_ms        != two->time_ms)
    KOUT(" ");
  if (one->uptime         != two->uptime)
    KOUT(" ");
  if (one->load1          != two->load1)
    KOUT(" ");
  if (one->load5          != two->load5)
    KOUT(" ");
  if (one->load15         != two->load15)
    KOUT(" ");
  if (one->totalram       != two->totalram)
    KOUT(" ");
  if (one->freeram        != two->freeram)
    KOUT(" ");
  if (one->cachedram      != two->cachedram)
    KOUT(" ");
  if (one->sharedram      != two->sharedram)
    KOUT(" ");
  if (one->bufferram      != two->bufferram)
    KOUT(" ");
  if (one->totalswap      != two->totalswap)
    KOUT(" ");
  if (one->freeswap       != two->freeswap)
    KOUT(" ");
  if (one->procs          != two->procs)
    KOUT(" ");
  if (one->totalhigh      != two->totalhigh)
    KOUT(" ");
  if (one->freehigh       != two->freehigh)
    KOUT(" ");
  if (one->mem_unit       != two->mem_unit)
    KOUT(" ");
  if (one->process_utime  != two->process_utime)
    KOUT(" ");
  if (one->process_stime  != two->process_stime)
    KOUT(" ");
  if (one->process_cutime != two->process_cutime)
    KOUT(" ");
  if (one->process_cstime != two->process_cstime)
    KOUT(" ");
  if (one->process_rss    != two->process_rss)
    KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void random_stats_sysinfo(t_stats_sysinfo *stats)
{
  stats->time_ms        =  rand();
  stats->uptime         =  rand();
  stats->load1          =  rand();
  stats->load5          =  rand();
  stats->load15         =  rand();
  stats->totalram       =  rand();
  stats->freeram        =  rand();
  stats->cachedram      =  rand();
  stats->sharedram      =  rand();
  stats->bufferram      =  rand();
  stats->totalswap      =  rand();
  stats->freeswap       =  rand();
  stats->procs          =  rand();
  stats->totalhigh      =  rand();
  stats->freehigh       =  rand();
  stats->mem_unit       =  rand();
  stats->process_utime  =  rand();
  stats->process_stime  =  rand();
  stats->process_cutime =  rand();
  stats->process_cstime =  rand();
  stats->process_rss    =  rand();
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_evt_stats_sysinfo(int llid, int itid, char *inetwork, char *iname,
                            t_stats_sysinfo *istats, char *idf, int istatus)
{
  static char network[MAX_NAME_LEN];
  static char name[MAX_NAME_LEN];
  static char df[2000];
  static int tid, status;
  static t_stats_sysinfo stats;
  if (i_am_client)
    {
    if (count_evt_stats_sysinfo)
      {
      if (strcmp(inetwork, network))
        KOUT(" ");
      if (strcmp(iname, name))
        KOUT(" ");
      if (itid != tid)
        KOUT(" ");
      if (istatus != status)
        KOUT(" ");
      diff_stats_sysinfo(&stats, istats);
      if (strcmp(idf, df))
        KOUT("%s %s", idf, df);
      }
    tid = rand();
    status = rand();
    random_stats_sysinfo(&stats);
    random_choice_str(name, MAX_NAME_LEN);
    random_choice_str(network, MAX_NAME_LEN); 
    random_choice_str(df, 1999); 
    send_evt_stats_sysinfo(llid, tid, network, name, 
                           &stats, df, status);
    }
  else
    send_evt_stats_sysinfo(llid, itid, inetwork, iname, 
                           istats, idf, istatus);
  count_evt_stats_sysinfo++;
}
/*---------------------------------------------------------------------------*/




/*****************************************************************************/
void recv_status_ok(int llid, int itid, char *ireason)
{
  static char reason[MAX_PRINT_LEN];
  static int tid;
  if (i_am_client)
    {
    if (count_status_ok)
      {
      if (strcmp(ireason, reason))
        KOUT(" ");
      if (itid != tid)
        KOUT(" ");
      }
    tid = rand();
    random_choice_str(reason, MAX_PRINT_LEN);
    send_status_ok(llid, tid, reason);
    }
  else
    send_status_ok(llid, itid, ireason);
  count_status_ok++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_status_ko(int llid, int itid, char *ireason)
{
  static int tid;
  static char reason[MAX_PRINT_LEN];
  if (i_am_client)
    {
    if (count_status_ko)
      {
      if (strcmp(ireason, reason))
        KOUT(" ");
      if (itid != tid)
        KOUT(" ");
      }
    tid = rand();
    random_choice_str(reason, MAX_PRINT_LEN);
    send_status_ko(llid, tid, reason);
    }
  else
    send_status_ko(llid, itid, ireason);
  count_status_ko++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_work_dir_req(int llid, int itid)
{
  static int tid;
  if (i_am_client)
    {
    if (count_work_dir_req)
      {
      if (itid != tid)
        KOUT(" ");
      }
    tid = rand();
    send_work_dir_req(llid, tid);
    }
  else
    send_work_dir_req(llid, itid);
  count_work_dir_req++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_work_dir_resp(int llid, int itid, t_topo_clc *iclc)
{
  static int tid;
  static t_topo_clc clc;
  if (i_am_client)
    {
    if (count_work_dir_resp)
      {
      if (itid != tid)
        KOUT(" ");
      topo_config_diff(&clc, iclc);
      if (memcmp(&clc, iclc, sizeof(t_topo_clc)))
        KOUT(" ");
      }
    tid = rand();
    random_choice_str(clc.version,MAX_NAME_LEN);
    random_choice_str(clc.network,MAX_NAME_LEN);
    random_choice_str(clc.username,MAX_NAME_LEN);
    clc.server_port = rand();
    random_choice_str(clc.bin_dir,MAX_PATH_LEN);
    random_choice_str(clc.work_dir,MAX_PATH_LEN);
    random_choice_str(clc.bulk_dir,MAX_PATH_LEN);
    clc.flags_config = rand();
    send_work_dir_resp(llid, tid, &clc);
    }
  else
    send_work_dir_resp(llid, itid, iclc);
  count_work_dir_resp++;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void recv_topo_small_event_sub(int llid, int itid)
{
  static int tid;
  if (i_am_client)
    {
    if (count_topo_small_event_sub)
      {
      if (itid != tid)
        KOUT(" ");
      }
    tid = rand();
    send_topo_small_event_sub(llid, tid);
    }
  else
    send_topo_small_event_sub(llid, itid);
  count_topo_small_event_sub++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_topo_small_event_unsub(int llid, int itid)
{
  static int tid;
  if (i_am_client)
    {
    if (count_topo_small_event_unsub)
      {
      if (itid != tid)
        KOUT(" ");
      }
    tid = rand();
    send_topo_small_event_unsub(llid, tid);
    }
  else
    send_topo_small_event_unsub(llid, itid);
  count_topo_small_event_unsub++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_topo_small_event(int llid, int itid, char *iname, 
                           char *iparam1, char *iparam2, int ievt)
{
  static char name[MAX_NAME_LEN];
  static char param1[MAX_PATH_LEN];
  static char param2[MAX_PATH_LEN];
  static int evt;
  static int tid;
  if (i_am_client)
    {
    if (count_topo_small_event)
      {
      if (strcmp(iparam1, param1))
        KOUT(" ");
      if (strcmp(iparam2, param2))
        KOUT(" ");
      if (strcmp(iname, name))
        KOUT(" ");
      if (ievt != evt)
        KOUT(" ");
      if (itid != tid)
        KOUT(" ");
      }
    random_choice_str(name, MAX_NAME_LEN);
    random_choice_str(param1, MAX_PATH_LEN);
    random_choice_str(param2, MAX_PATH_LEN);
    evt = rand();
    tid = rand();
    send_topo_small_event(llid, tid, name, param1, param2,  evt);
    }
  else
    send_topo_small_event(llid, itid, iname, iparam1, iparam2, ievt);
  count_topo_small_event++;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void recv_event_topo_sub(int llid, int itid)
{ 
  static int tid;
  if (i_am_client)
    {
    if (count_event_topo_sub)
      {
      if (itid != tid)
        KOUT(" ");
      }
    tid = rand();
    send_event_topo_sub(llid, tid);
    }
  else
    send_event_topo_sub(llid, itid);
  count_event_topo_sub++;
}   
/*---------------------------------------------------------------------------*/
    
/*****************************************************************************/
void recv_event_topo_unsub(int llid, int itid)
{   
  static int tid;
  if (i_am_client)
    {
    if (count_event_topo_unsub)
      {
      if (itid != tid)
        KOUT(" ");
      }
    tid = rand();
    send_event_topo_unsub(llid, tid);
    }
  else
    send_event_topo_unsub(llid, itid);
  count_event_topo_unsub++;
}
/*---------------------------------------------------------------------------*/
  
/*****************************************************************************/
void recv_event_topo(int llid, int itid, t_topo_info *itopo)
{
  int i;
  static t_topo_info *topo;
  static int tid;
  t_topo_info *dup;
  if (i_am_client)
    {
    if (count_event_topo)
      {
      if (topo->conf_rank != itopo->conf_rank)
        KOUT("%d %d", topo->conf_rank, itopo->conf_rank);
      topo_config_diff(&(itopo->clc), &(topo->clc));
      if (topo->nb_kvm != itopo->nb_kvm)
        KOUT("%d %d", topo->nb_kvm, itopo->nb_kvm);
      if (topo->nb_cnt != itopo->nb_cnt)
        KOUT("%d %d", topo->nb_cnt, itopo->nb_cnt);
      for (i=0; i<topo->nb_cnt; i++)
        topo_cnt_diff(&(itopo->cnt[i]), &(topo->cnt[i]));
      for (i=0; i<topo->nb_kvm; i++)
        topo_kvm_diff(&(itopo->kvm[i]), &(topo->kvm[i]));
      if (topo_compare(itopo, topo))
        KOUT("%d ", topo_compare(itopo, topo));
      topo_free_topo(topo);
      if (itid != tid)
        KOUT(" ");
      }
    tid = rand();
    topo = random_topo_gen();
    dup = topo_duplicate(topo);
    if (topo_compare(dup, topo))
      KOUT("%d ", topo_compare(dup, topo));
    topo_free_topo(dup);
    send_event_topo(llid, tid, topo);
    }
  else
    {
    send_event_topo(llid, itid, itopo);
    }
  count_event_topo++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_event_sys_sub(int llid, int itid)
{
  static int tid;
  if (i_am_client)
    {
    if (count_event_sys_sub)
      {
      if (itid != tid)
        KOUT(" ");
      }
    tid = rand();
    send_event_sys_sub(llid, tid);
    }
  else
    send_event_sys_sub(llid, itid);
  count_event_sys_sub++;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void recv_event_sys_unsub(int llid, int itid)
{
  static int tid;
  if (i_am_client)
    {
    if (count_event_sys_unsub)
      {
      if (itid != tid)
        KOUT(" ");
      }
    tid = rand();
    send_event_sys_unsub(llid, tid);
    }
  else
    send_event_sys_unsub(llid, itid);
  count_event_sys_unsub++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_event_sys(int llid, int itid, t_sys_info *isys)
{
  static t_queue_tx Qtx[TST_MAX_QTY];
  static t_sys_info sys; 
  static int tid;
  t_sys_info *msys;
  int i, nb;
  if (i_am_client)
    {
    if (count_event_sys)
      {
      if (memcmp(isys, &sys, sizeof(t_sys_info)-sizeof(t_queue_tx *)))
        KOUT(" ");
      if (isys->nb_queue_tx != sys.nb_queue_tx)
        KOUT(" ");
      nb = isys->nb_queue_tx;
      if (memcmp(isys->queue_tx, Qtx, nb * sizeof(t_queue_tx)))
        KOUT(" ");
      if (itid != tid)
        KOUT(" ");
      }
    tid = rand();
    nb = my_rand(TST_MAX_QTY);
    for (i=0; i<nb; i++)
      {
      Qtx[i].peak_size = rand();
      Qtx[i].size = rand();
      Qtx[i].llid = rand();
      Qtx[i].fd = rand();
      Qtx[i].waked_count_in = rand();
      Qtx[i].waked_count_out = rand();
      Qtx[i].waked_count_err = rand();
      Qtx[i].out_bytes = rand();
      Qtx[i].in_bytes = rand();
      random_choice_str(Qtx[i].name, MAX_PATH_LEN);
      Qtx[i].id = rand();
      Qtx[i].type = rand();
      }
    sys.nb_queue_tx = nb;
    sys.queue_tx = Qtx;
    sys.selects = rand();
    for (i=0; i< MAX_MALLOC_TYPES; i++)
      sys.mallocs[i] = rand();
    for (i=0; i< type_llid_max; i++)
      sys.fds_used[i] = rand();
    sys.cur_channels = rand();
    sys.max_channels = rand();
    sys.cur_channels_recv = rand();
    sys.cur_channels_send = rand();
    sys.clients = rand();
    sys.max_time = rand();
    sys.avg_time = rand();
    sys.above50ms = rand();
    sys.above20ms = rand();
    sys.above15ms = rand();
    msys = (t_sys_info *) clownix_malloc(sizeof(t_sys_info),7);
    memcpy(msys, &sys, sizeof(t_sys_info));
    msys->queue_tx = (t_queue_tx *) clownix_malloc(nb * sizeof(t_queue_tx),7);
    memcpy(msys->queue_tx, &Qtx, nb * sizeof(t_queue_tx));
    send_event_sys(llid, tid, msys);
    sys_info_free(msys);
    }
  else
    {
    msys = (t_sys_info *) clownix_malloc(sizeof(t_sys_info),7);
    memcpy(msys, isys, sizeof(t_sys_info));
    msys->queue_tx = 
    (t_queue_tx *) clownix_malloc(msys->nb_queue_tx * sizeof(t_queue_tx),7);
    memcpy(msys->queue_tx,isys->queue_tx,msys->nb_queue_tx*sizeof(t_queue_tx));
    send_event_sys(llid, itid, msys);
    sys_info_free(msys);
    }
  count_event_sys++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_evt_print_sub(int llid, int itid)
{
  static int tid;
  if (i_am_client)
    {
    if (count_evt_print_sub)
      {
      if (itid != tid)
        KOUT(" ");
      }
    tid = rand();
    send_evt_print_sub(llid, tid);
    }
  else
    send_evt_print_sub(llid, itid);
  count_evt_print_sub++;
}
/*---------------------------------------------------------------------------*/



/*****************************************************************************/
void recv_evt_print_unsub(int llid, int itid)
{
  static int tid;
  if (i_am_client)
    {
    if (count_evt_print_unsub)
      {
      if (itid != tid)
        KOUT(" ");
      }
    tid = rand();
    send_evt_print_unsub(llid, tid);
    }
  else
    send_evt_print_unsub(llid, itid);
  count_evt_print_unsub++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_evt_print(int llid, int itid, char *ireason)
{
  static int tid;
  static char reason[MAX_PRINT_LEN];
  if (i_am_client)
    {
    if (count_event_print)
      {
      if (strcmp(ireason, reason))
        KOUT(" ");
      if (itid != tid)
        KOUT(" ");
      }
    tid = rand();
    random_choice_str(reason, MAX_PRINT_LEN);
    send_evt_print(llid, tid, reason);
    }
  else
    send_evt_print(llid, itid, ireason);
  count_event_print++;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void recv_add_vm(int llid, int itid, t_topo_kvm *ikvm)
{
  static t_topo_kvm kvm;
  static int tid;
  if (i_am_client)
    {
    if (count_add_vm)
      {
      if (topo_kvm_diff(&kvm, ikvm))
        KOUT(" ");
      if (itid != tid)
        KOUT(" ");
      }
    tid = rand();
    memset(&kvm, 0, sizeof(t_topo_kvm));
    random_kvm(&kvm);
    memset(kvm.rootfs_used, 0, MAX_PATH_LEN);
    memset(kvm.rootfs_backing, 0, MAX_PATH_LEN);
    kvm.vm_id = 0;
    send_add_vm(llid, tid, &kvm);
    }
  else
    send_add_vm(llid, itid, ikvm);
  count_add_vm++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_sav_vm(int llid, int itid, char *iname, char *ipath)
{
  static int tid;
  static char name[MAX_NAME_LEN];
  static char path[MAX_PATH_LEN];
  if (i_am_client)
    {
    if (count_sav_vm)
      {
      if (strcmp(iname, name))
        KOUT(" ");
      if (strcmp(ipath, path))
        KOUT(" ");
      if (itid != tid)
        KOUT(" ");
      }
    tid = rand();
    random_choice_str(name, MAX_NAME_LEN);
    random_choice_str(path, MAX_PATH_LEN);
    send_sav_vm(llid, tid, name, path);
    }
  else
    send_sav_vm(llid, itid, iname, ipath);
  count_sav_vm++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_del_name(int llid, int itid, char *iname)
{
  static int tid;
  static char name[MAX_NAME_LEN];
  if (i_am_client)
    {
    if (count_del_name)
      {
      if (strcmp(name,  iname))
        KOUT("%s %s ", name, iname);
      if (itid != tid)
        KOUT(" ");
      }
    tid = rand();
    random_choice_str(name, MAX_NAME_LEN);
    send_del_name(llid, tid, name);
    }
  else
    send_del_name(llid, itid, iname);
  count_del_name++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_add_lan_endp(int llid, int itid, char *iname, int inum , char *ilan) 
{
  static int tid, num;
  static char name[MAX_NAME_LEN];
  static char lan[MAX_NAME_LEN];
  if (i_am_client)
    {
    if (count_add_lan_endp)
      {
      if (strcmp(name, iname))
        KOUT(" ");
      if (strcmp(lan, ilan))
        KOUT(" ");
      if (itid != tid)
        KOUT(" ");
      if (inum != num)
        KOUT(" ");
      }
    tid = rand();
    num = rand();
    random_choice_str(lan, MAX_NAME_LEN);
    random_choice_str(name, MAX_NAME_LEN);
    send_add_lan_endp(llid, tid, name, num, lan);
    }
  else
    send_add_lan_endp(llid, itid, iname, inum, ilan);
  count_add_lan_endp++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_del_lan_endp(int llid, int itid, char *iname, int inum, char *ilan)
{
  static int tid, num;
  static char name[MAX_NAME_LEN];
  static char lan[MAX_NAME_LEN];
  if (i_am_client)
    {
    if (count_del_lan_endp)
      {
      if (strcmp(name, iname))
        KOUT(" ");
      if (strcmp(lan, ilan))
        KOUT(" ");
      if (itid != tid)
        KOUT(" ");
      if (inum != num)
        KOUT(" ");
      }
    tid = rand();
    num = rand();
    random_choice_str(lan, MAX_NAME_LEN);
    random_choice_str(name, MAX_NAME_LEN);
    send_del_lan_endp(llid, tid, name, num, lan);
    }
  else
    send_del_lan_endp(llid, itid, iname, inum, ilan);
  count_del_lan_endp++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_kill_uml_clownix(int llid, int itid)
{
  static int tid;
  if (i_am_client)
    {
    if (count_kill_uml_clownix)
      {
      if (itid != tid)
        KOUT(" ");
      }
    tid = rand();
    send_kill_uml_clownix(llid, tid);
    }
  else
    send_kill_uml_clownix(llid, itid);
  count_kill_uml_clownix++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_del_all(int llid, int itid)
{
  static int tid;
  if (i_am_client)
    {
    if (count_del_all)
      {
      if (itid != tid)
        KOUT(" ");
      }
    tid = rand();
    send_del_all(llid, tid);
    }
  else
    send_del_all(llid, itid);
  count_del_all++;
} 
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void recv_list_pid_req(int llid, int itid)
{
  static int tid;
  if (i_am_client)
    {
    if (count_list_pid_req)
      {
      if (itid != tid)
        KOUT(" ");
      }
    tid = rand();
    send_list_pid_req(llid, tid);
    }
  else
    send_list_pid_req(llid, itid);
  count_list_pid_req++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_list_pid_resp(int llid, int itid, int iqty, t_pid_lst *ilst)
{
  int i;
  static int tid;
  static int qty;
  static t_pid_lst *lst;
  if (i_am_client)
    {
    if (count_list_pid_resp)
      {
      if (itid != tid)
        KOUT(" ");
      if (iqty != qty)
        KOUT("%d %d ", iqty, qty);
      for (i=0; i<qty; i++)
         {
         if (strcmp(lst[i].name, ilst[i].name))
           KOUT("%d %s %s ", i, lst[i].name, ilst[i].name);
         if (lst[i].pid != ilst[i].pid)
           KOUT("%d %d %d ", i, lst[i].pid, ilst[i].pid);
         }
      if (memcmp(lst, ilst, qty*sizeof(t_pid_lst)))
        KOUT(" ");
      clownix_free(lst, __FUNCTION__);
      }
    tid = rand();
    qty = my_rand(50);
    lst = (t_pid_lst *)clownix_malloc(qty*sizeof(t_pid_lst),7);
    memset (lst, 0, qty * sizeof(t_pid_lst));
    for (i=0; i<qty; i++)
      {
      random_choice_str(lst[i].name, MAX_NAME_LEN);
      lst[i].pid = rand();
      }
    send_list_pid_resp(llid, tid, qty, lst);
    }
  else
    send_list_pid_resp(llid, itid, iqty, ilst);
  count_list_pid_resp++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_list_commands_req(int llid, int itid, int iis_layout)
{
  static int tid, is_layout;
  if (i_am_client)
    {
    if (count_list_commands_req)
      {
      if (itid != tid)
        KOUT(" ");
      if (iis_layout != is_layout)
        KOUT(" ");
      }
    tid = rand();
    is_layout = rand();
    send_list_commands_req(llid, tid, is_layout);
    }
  else
    send_list_commands_req(llid, itid, iis_layout);
  count_list_commands_req++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_list_commands_resp(int llid, int itid, int iqty, t_list_commands *ilst)
{
  int i;
  static int tid;
  static int qty;
  static t_list_commands *lst;
  if (i_am_client)
    {
    if (count_list_commands_resp)
      {
      if (itid != tid)
        KOUT(" ");
      if (iqty != qty)
        KOUT("%d %d", iqty, qty);
      for (i=0; i<qty; i++)
        {
        if (strcmp(lst[i].cmd, ilst[i].cmd))
          KOUT("\n:%s:\n:%s:\n", lst[i].cmd, ilst[i].cmd);
        }
      clownix_free(lst, __FUNCTION__);
      }
    tid = rand();
    qty = my_rand(150);
    lst = (t_list_commands *)clownix_malloc(qty*sizeof(t_list_commands),7);
    memset(lst, 0, qty*sizeof(t_list_commands));
    for (i=0; i<qty; i++)
      {
      random_choice_str(lst[i].cmd, MAX_LIST_COMMANDS_LEN);
      }
    send_list_commands_resp(llid, tid, qty, lst);
    }
  else
    send_list_commands_resp(llid, itid, iqty, ilst);
  count_list_commands_resp++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_vmcmd(int llid, int itid, char *iname, int icmd, int iparam)
{
  static int tid, cmd, param;
  static char name[MAX_NAME_LEN];
  if (i_am_client)
    {
    if (count_vmcmd)
      {
      if (strcmp(iname, name))
        KOUT(" ");
      if (cmd != icmd)
        KOUT(" ");
      if (param != iparam)
        KOUT(" ");
      if (itid != tid)
        KOUT(" ");
      }
    tid = rand();
    cmd = rand();
    param = rand();
    random_choice_str(name, MAX_NAME_LEN);
    send_vmcmd(llid, tid, name, cmd, param);
    }
  else
    send_vmcmd(llid, itid, iname, icmd, iparam);
  count_vmcmd++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_eventfull_sub(int llid, int itid)
{
  static int tid;
  if (i_am_client)
    {
    if (count_eventfull_sub)
      {
      if (itid != tid)
        KOUT(" ");
      }
    tid = rand();
    send_eventfull_sub(llid, tid);
    }
  else
    send_eventfull_sub(llid, itid);
  count_eventfull_sub++;
}
/*---------------------------------------------------------------------------*/
#define MAX_TEST_TUX 100
/*****************************************************************************/
void recv_eventfull(int llid, int itid, int inb_endp, t_eventfull_endp *iendp)
{
  int i;
  static int nb_endp, tid;
  static t_eventfull_endp endp[MAX_TEST_TUX];
  if (i_am_client)
    {
    if (count_eventfull)
      {
      if (itid != tid)
        KOUT(" ");
      if (inb_endp != nb_endp)
        KOUT(" ");
      if (memcmp(endp, iendp, nb_endp * sizeof(t_eventfull_endp))) 
        KOUT(" ");
      }
    tid = rand();
    nb_endp = rand() % MAX_TEST_TUX;
    memset(endp, 0, MAX_TEST_TUX * sizeof(t_eventfull_endp));
    for (i=0; i<nb_endp; i++)
      random_eventfull_endp(&(endp[i]));
    send_eventfull(llid, tid, nb_endp, endp);
    }
  else
    send_eventfull(llid, itid, inb_endp, iendp);
  count_eventfull++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_slowperiodic_sub(int llid, int itid)
{
  static int tid;
  if (i_am_client)
    {
    if (count_slowperiodic_sub)
      {
      if (itid != tid)
        KOUT(" ");
      }
    tid = rand();
    send_slowperiodic_sub(llid, tid);
    }
  else
    send_slowperiodic_sub(llid, itid);
  count_slowperiodic_sub++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_slowperiodic_qcow2(int llid, int itid, int inb, t_slowperiodic *ispic)
{
  int i;
  static int nb, tid;
  static t_slowperiodic spic[MAX_TEST_TUX];
  if (i_am_client)
    {
    if (count_slowperiodic_qcow2)
      {
      if (itid != tid)
        KOUT(" ");
      if (inb != nb)
        KOUT(" ");
      if (memcmp(spic, ispic, nb * sizeof(t_slowperiodic)))
        KOUT(" ");
      }
    tid = rand();
    nb = rand() % MAX_TEST_TUX;
    memset(spic, 0, MAX_TEST_TUX * sizeof(t_slowperiodic));
    for (i=0; i<nb; i++)
      random_choice_str(spic[i].name, MAX_NAME_LEN);
    send_slowperiodic_qcow2(llid, tid, nb, spic);
    }
  else
    send_slowperiodic_qcow2(llid, itid, inb, ispic);
  count_slowperiodic_qcow2++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_slowperiodic_img(int llid, int itid, int inb, t_slowperiodic *ispic)
{
  int i;
  static int nb, tid;
  static t_slowperiodic spic[MAX_TEST_TUX];
  if (i_am_client)
    {
    if (count_slowperiodic_img)
      {
      if (itid != tid)
        KOUT(" ");
      if (inb != nb)
        KOUT(" ");
      if (memcmp(spic, ispic, nb * sizeof(t_slowperiodic)))
        KOUT(" ");
      }
    tid = rand();
    nb = rand() % MAX_TEST_TUX;
    memset(spic, 0, MAX_TEST_TUX * sizeof(t_slowperiodic));
    for (i=0; i<nb; i++)
      random_choice_str(spic[i].name, MAX_NAME_LEN);
    send_slowperiodic_img(llid, tid, nb, spic);
    }
  else
    send_slowperiodic_img(llid, itid, inb, ispic);
  count_slowperiodic_img++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_slowperiodic_docker(int llid, int itid, int inb, t_slowperiodic *ispic)
{
  int i;
  static int nb, tid;
  static t_slowperiodic spic[MAX_TEST_TUX];
  if (i_am_client)
    {
    if (count_slowperiodic_docker)
      {
      if (itid != tid)
        KOUT(" ");
      if (inb != nb)
        KOUT(" ");
      if (memcmp(spic, ispic, nb * sizeof(t_slowperiodic)))
        KOUT(" ");
      }
    tid = rand();
    nb = rand() % MAX_TEST_TUX;
    memset(spic, 0, MAX_TEST_TUX * sizeof(t_slowperiodic));
    for (i=0; i<nb; i++)
      random_choice_str(spic[i].name, MAX_NAME_LEN);
    send_slowperiodic_docker(llid, tid, nb, spic);
    }
  else
    send_slowperiodic_docker(llid, itid, inb, ispic);
  count_slowperiodic_docker++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_slowperiodic_podman(int llid, int itid, int inb, t_slowperiodic *ispic)
{
  int i;
  static int nb, tid;
  static t_slowperiodic spic[MAX_TEST_TUX];
  if (i_am_client)
    {
    if (count_slowperiodic_podman)
      {
      if (itid != tid)
        KOUT(" ");
      if (inb != nb)
        KOUT(" ");
      if (memcmp(spic, ispic, nb * sizeof(t_slowperiodic)))
        KOUT(" ");
      }
    tid = rand();
    nb = rand() % MAX_TEST_TUX;
    memset(spic, 0, MAX_TEST_TUX * sizeof(t_slowperiodic));
    for (i=0; i<nb; i++)
      random_choice_str(spic[i].name, MAX_NAME_LEN);
    send_slowperiodic_podman(llid, tid, nb, spic);
    }
  else
    send_slowperiodic_podman(llid, itid, inb, ispic);
  count_slowperiodic_podman++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_nat_add(int llid, int itid, char *iname)
{
  static char name[MAX_NAME_LEN];
  static int tid;
  if (i_am_client)
    {
    if (count_nat_add)
      {
      if (strcmp(iname, name))
        KOUT(" ");
      if (itid != tid)
        KOUT(" ");
      }
    tid = rand();
    random_choice_str(name, MAX_NAME_LEN);
    send_nat_add(llid, tid, name);
    }
  else
    send_nat_add(llid, itid, iname);
  count_nat_add++;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void recv_phy_add(int llid, int itid, char *iname)
{
  static char name[MAX_NAME_LEN];
  static int tid;
  if (i_am_client)
    {
    if (count_phy_add)
      {
      if (strcmp(iname, name))
        KOUT(" ");
      if (itid != tid)
        KOUT(" ");
      }
    tid = rand();
    random_choice_str(name, MAX_NAME_LEN);
    send_phy_add(llid, tid, name);
    }
  else
    send_phy_add(llid, itid, iname);
  count_phy_add++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_cnt_add(int llid, int itid, t_topo_cnt *icnt)
{
  static t_topo_cnt cnt;
  static int tid;
  if (i_am_client)
    {
    if (count_cnt_add)
      {
      if (topo_cnt_diff(&cnt, icnt))
        KOUT(" ");
      if (itid != tid)
        KOUT(" ");
      }
    tid = rand();
    memset(&cnt, 0, sizeof(t_topo_cnt));
    random_cnt(&cnt);
    send_cnt_add(llid, tid, &cnt);
    }
  else
    send_cnt_add(llid, itid, icnt);
  count_cnt_add++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_tap_add(int llid, int itid, char *iname)
{
  static char name[MAX_NAME_LEN];
  static int tid;
  if (i_am_client)
    {
    if (count_tap_add)
      {
      if (strcmp(iname, name))
        KOUT(" ");
      if (itid != tid)
        KOUT(" ");
      }
    tid = rand();
    random_choice_str(name, MAX_NAME_LEN);
    send_tap_add(llid, tid, name);
    }
  else
    send_tap_add(llid, itid, iname);
  count_tap_add++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_a2b_add(int llid, int itid, char *iname)
{
  static char name[MAX_NAME_LEN];
  static int tid;
  if (i_am_client)
    {
    if (count_a2b_add)
      {
      if (strcmp(iname, name))
        KOUT(" ");
      if (itid != tid)
        KOUT(" ");
      }
    tid = rand();
    random_choice_str(name, MAX_NAME_LEN);
    send_a2b_add(llid, tid, name);
    }
  else
    send_a2b_add(llid, itid, iname);
  count_a2b_add++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_c2c_cnf(int llid, int itid, char *iname, char *icmd)
{
  static char name[MAX_NAME_LEN];
  static char cmd[MAX_PRINT_LEN];
  static int tid;
  if (i_am_client)
    {
    if (count_c2c_cnf)
      {
      if (strcmp(iname, name))
        KOUT(" ");
      if (strcmp(icmd, cmd))
        KOUT(" ");
      if (itid != tid)
        KOUT(" ");
      }
    tid = rand();
    random_choice_str(name, MAX_NAME_LEN);
    random_choice_str(cmd, MAX_PRINT_LEN);
    send_c2c_cnf(llid, tid, name, cmd);
    }
  else
    send_c2c_cnf(llid, itid, iname, icmd);
  count_c2c_cnf++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_nat_cnf(int llid, int itid, char *iname, char *icmd)
{
  static char name[MAX_NAME_LEN];
  static char cmd[MAX_PRINT_LEN];
  static int tid;
  if (i_am_client)
    {
    if (count_nat_cnf)
      {
      if (strcmp(iname, name))
        KOUT(" ");
      if (strcmp(icmd, cmd))
        KOUT(" ");
      if (itid != tid)
        KOUT(" ");
      }
    tid = rand();
    random_choice_str(name, MAX_NAME_LEN);
    random_choice_str(cmd, MAX_PRINT_LEN);
    send_nat_cnf(llid, tid, name, cmd);
    }
  else
    send_nat_cnf(llid, itid, iname, icmd);
  count_nat_cnf++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_a2b_cnf(int llid, int itid, char *iname, char *icmd)
{
  static char name[MAX_NAME_LEN];
  static char cmd[MAX_PRINT_LEN];
  static int tid;
  if (i_am_client)
    {
    if (count_a2b_cnf)
      {
      if (strcmp(iname, name))
        KOUT(" ");
      if (strcmp(icmd, cmd))
        KOUT(" ");
      if (itid != tid)
        KOUT(" ");
      }
    tid = rand();
    random_choice_str(name, MAX_NAME_LEN);
    random_choice_str(cmd, MAX_PRINT_LEN);
    send_a2b_cnf(llid, tid, name, cmd);
    }
  else
    send_a2b_cnf(llid, itid, iname, icmd);
  count_a2b_cnf++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_c2c_add(int llid, int itid, char *ic2c_name, uint32_t ilocal_udp_ip,
                  char *islave_cloon, uint32_t iip, uint16_t iport,
                  char *ipasswd, uint32_t iudp_ip)
{
  static char c2c_name[MAX_NAME_LEN];
  static char slave_cloon[MAX_NAME_LEN];
  static char passwd[MSG_DIGEST_LEN];
  static int tid;
  static uint32_t ip, local_udp_ip, udp_ip;
  static uint16_t port;
  if (i_am_client)
    {
    if (count_c2c_add)
      {
      if (strcmp(ipasswd, passwd))
        KOUT(" ");
      if (strcmp(ic2c_name, c2c_name))
        KOUT(" ");
      if (strcmp(islave_cloon, slave_cloon))
        KOUT(" ");
      if (iip != ip)
        KOUT(" ");
      if (ilocal_udp_ip != local_udp_ip)
        KOUT(" ");
      if (iudp_ip != udp_ip)
        KOUT(" ");
      if (itid != tid)
        KOUT(" ");
      if (iport != port)
        KOUT(" ");
      }
    tid = rand();
    ip = rand();
    local_udp_ip = rand();
    udp_ip = rand();
    port = rand() & 0xFFFF;
    random_choice_str(passwd, MSG_DIGEST_LEN);
    random_choice_str(c2c_name, MAX_NAME_LEN);
    random_choice_str(slave_cloon, MAX_NAME_LEN);
    send_c2c_add(llid, tid, c2c_name, local_udp_ip,
                 slave_cloon, ip, port, passwd, udp_ip);
    }
  else
    send_c2c_add(llid, itid, ic2c_name, ilocal_udp_ip,
                 islave_cloon, iip, iport, ipasswd, iudp_ip);
  count_c2c_add++;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void recv_snf_add(int llid, int itid, char *iname, int inum, int ival)
{
  static char name[MAX_NAME_LEN];
  static int tid, num, val;
  if (i_am_client)
    {
    if (count_snf_add)
      {
      if (strcmp(iname, name))
        KOUT(" ");
      if (itid != tid)
        KOUT(" ");
      if (inum != num)
        KOUT(" ");
      if (ival != val)
        KOUT(" ");
      }
    tid = rand();
    num = rand();
    val = rand();
    random_choice_str(name, MAX_NAME_LEN);
    send_snf_add(llid, tid, name, num, val);
    }
  else
    send_snf_add(llid, itid, iname, inum, ival);
  count_snf_add++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_c2c_peer_create(int llid, int itid, char *ic2c_name, int iis_ack,
                          char *idistant_cloon, char *ilocal_cloon)
{
  static char c2c_name[MAX_NAME_LEN];
  static char distant_cloon[MAX_NAME_LEN];
  static char local_cloon[MAX_NAME_LEN];
  static int tid, is_ack;
  if (i_am_client)
    {
    if (count_c2c_peer_create)
      {
      if (strcmp(ic2c_name, c2c_name))
        KOUT(" ");
      if (strcmp(idistant_cloon, distant_cloon))
        KOUT(" ");
      if (strcmp(ilocal_cloon, local_cloon))
        KOUT(" ");
      if (itid != tid)
        KOUT(" ");
      if (iis_ack != is_ack)
        KOUT(" ");
      }
    is_ack = rand();
    tid = rand();
    random_choice_str(c2c_name, MAX_NAME_LEN);
    random_choice_str(distant_cloon, MAX_NAME_LEN);
    random_choice_str(local_cloon, MAX_NAME_LEN);
    send_c2c_peer_create(llid, tid, c2c_name, is_ack,
                         distant_cloon, local_cloon);
    }
  else
    send_c2c_peer_create(llid, itid, ic2c_name, iis_ack,
                         idistant_cloon, ilocal_cloon);
  count_c2c_peer_create++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_c2c_peer_conf(int llid, int itid, char *ic2c_name, int iis_ack,
                        char *idistant_cloon,     char *ilocal_cloon,
                        uint32_t idudp_ip,   uint32_t iludp_ip,
                        uint16_t idudp_port, uint16_t iludp_port)
{
  static char c2c_name[MAX_NAME_LEN];
  static char distant_cloon[MAX_NAME_LEN];
  static char local_cloon[MAX_NAME_LEN];
  static int tid, is_ack;
  static uint32_t dudp_ip, ludp_ip;
  static uint16_t dudp_port, ludp_port;
  if (i_am_client)
    {
    if (count_c2c_peer_conf)
      {
      if (strcmp(ic2c_name, c2c_name))
        KOUT(" ");
      if (itid != tid)
        KOUT(" ");
      if (iis_ack != is_ack)
        KOUT(" ");
      if (strcmp(idistant_cloon, distant_cloon))
        KOUT(" ");
      if (strcmp(ilocal_cloon, local_cloon))
        KOUT(" ");
      if (iludp_ip != ludp_ip)
        KOUT(" ");
      if (idudp_ip != dudp_ip)
        KOUT(" ");
      if (iludp_port != ludp_port)
        KOUT(" ");
      if (idudp_port != dudp_port)
        KOUT(" ");
      }
    tid = rand();
    is_ack = rand();
    ludp_ip = rand();
    dudp_ip = rand();
    ludp_port = rand() & 0xFFFF;
    dudp_port = rand() & 0xFFFF;
    random_choice_str(c2c_name, MAX_NAME_LEN);
    random_choice_str(distant_cloon, MAX_NAME_LEN);
    random_choice_str(local_cloon, MAX_NAME_LEN);
    send_c2c_peer_conf(llid, tid, c2c_name, is_ack, distant_cloon,
                       local_cloon, dudp_ip, ludp_ip,
                       dudp_port, ludp_port);
    }
  else
    send_c2c_peer_conf(llid, itid, ic2c_name, iis_ack, idistant_cloon,
                       ilocal_cloon, idudp_ip, iludp_ip,
                       idudp_port, iludp_port);
  count_c2c_peer_conf++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_c2c_peer_ping(int llid, int itid, char *ic2c_name, int istatus)
{
  static char c2c_name[MAX_NAME_LEN];
  static int tid, status;
  if (i_am_client)
    {
    if (count_c2c_peer_ping)
      {
      if (strcmp(ic2c_name, c2c_name))
        KOUT(" ");
      if (itid != tid)
        KOUT(" ");
      if (istatus != status)
        KOUT(" ");
      }
    tid = rand();
    status = rand();
    random_choice_str(c2c_name, MAX_NAME_LEN);
    send_c2c_peer_ping(llid, tid, c2c_name, status);
    }
  else
    send_c2c_peer_ping(llid, itid, ic2c_name, istatus);
  count_c2c_peer_ping++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_color_item(int llid, int itid, char *iname, int icolor)
{
  static char name[MAX_NAME_LEN];
  static int tid, color;
  if (i_am_client)
    {
    if (count_color_item)
      {
      if (strcmp(iname, name))
        KOUT(" ");
      if (itid != tid)
        KOUT(" ");
      if (icolor != color)
        KOUT(" ");
      }
    tid = rand();
    color = rand();
    random_choice_str(name, MAX_NAME_LEN);
    send_color_item(llid, tid, name, color);
    }
  else
    send_color_item(llid, itid, iname, icolor);
  count_color_item++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
/* FUNCTION: send_first_burst                                              */
/*---------------------------------------------------------------------------*/
static void send_first_burst(int llid)
{
  recv_status_ok(llid, 0, 0);
  recv_status_ko(llid, 0, 0);
  recv_add_vm(llid, 0,NULL);
  recv_del_name(llid, 0, NULL);
  recv_kill_uml_clownix(llid,0);
  recv_del_all(llid,0);
  recv_add_lan_endp(llid, 0, NULL, 0, NULL);
  recv_del_lan_endp(llid, 0, NULL, 0, NULL);
  recv_list_pid_req(llid,0);
  recv_list_pid_resp(llid,0, 0, NULL);
  recv_list_commands_req(llid,0, 0);
  recv_list_commands_resp(llid,0, 0, NULL);
  recv_topo_small_event_sub(llid,0);
  recv_topo_small_event_unsub(llid,0);
  recv_topo_small_event(llid, 0,NULL, NULL, NULL, 0);
  recv_event_topo_sub(llid,0);
  recv_event_topo_unsub(llid,0);
  recv_event_topo(llid,0, NULL);
  recv_evt_print_sub(llid,0);
  recv_evt_print_unsub(llid,0);
  recv_evt_print(llid,0, NULL);
  recv_event_sys_sub(llid,0);
  recv_event_sys_unsub(llid,0);
  recv_event_sys(llid,0, NULL);
  recv_work_dir_req(llid, 0);
  recv_work_dir_resp(llid, 0, NULL);
  recv_vmcmd(llid, 0, NULL, 0, 0);
  recv_eventfull_sub(llid, 0);
  recv_eventfull(llid, 0, 0, NULL);
  recv_slowperiodic_sub(llid, 0);
  recv_slowperiodic_qcow2(llid, 0, 0, NULL);
  recv_slowperiodic_img(llid, 0, 0, NULL);
  recv_slowperiodic_docker(llid, 0, 0, NULL);
  recv_slowperiodic_podman(llid, 0, 0, NULL);
  recv_sav_vm(llid, 0, NULL, NULL);
  recv_evt_stats_endp_sub(llid, 0, NULL, 0, 0);
  recv_evt_stats_endp(llid, 0, NULL, NULL, 0, NULL, 0);
  recv_evt_stats_sysinfo_sub(llid, 0, NULL, 0);
  recv_evt_stats_sysinfo(llid, 0, NULL, NULL, NULL, NULL, 0);

  recv_hop_get_name_list_doors(llid, 0);
  recv_hop_name_list_doors(llid, 0, 0, NULL);
  recv_hop_evt_doors_sub(llid, 0, 0, 0, NULL);
  recv_hop_evt_doors_unsub(llid, 0);
  recv_hop_evt_doors(llid, 0, 0, NULL, NULL);

  rpct_recv_sigdiag_msg(llid, 0, NULL);
  rpct_recv_poldiag_msg(llid, 0, NULL);
  rpct_recv_pid_req(llid, 0, NULL, 0);
  rpct_recv_kil_req(llid, 0);
  rpct_recv_pid_resp(llid, 0, NULL, 0, 0, 0);
  rpct_recv_hop_sub(llid, 0, 0);
  rpct_recv_hop_unsub(llid, 0);
  rpct_recv_hop_msg(llid, 0, 0, NULL);

  recv_snf_add(llid, 0,  NULL, 0, 0);

  recv_c2c_add(llid, 0,  NULL, 0, NULL, 0, 0, NULL, 0);
  recv_c2c_peer_create(llid, 0, NULL, 0, NULL, NULL);
  recv_c2c_peer_conf(llid, 0, NULL, 0, NULL, NULL, 0,0,0,0);
  recv_c2c_peer_ping(llid, 0, NULL, 0);

  recv_color_item(llid, 0, NULL, 0);

  recv_nat_add(llid, 0, NULL);
  recv_phy_add(llid, 0, NULL);
  recv_tap_add(llid, 0, NULL);
  recv_cnt_add(llid, 0, NULL);

  recv_a2b_add(llid, 0, NULL);
  recv_a2b_cnf(llid, 0, NULL, NULL);
  recv_c2c_cnf(llid, 0, NULL, NULL);
  recv_nat_cnf(llid, 0, NULL, NULL);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void usage(char *name)
{
  printf("\n");
  printf("\n%s -s", name);
  printf("\n%s -c", name);
  printf("\n");
  exit (0);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void err_cb (int llid, int err, int from)
{
  printf("ERROR %d  %d %d\n", llid, err, from);
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void rx_cb(int llid, int len, char *buf)
{
  if (doors_io_basic_decoder(llid, len, buf))
    {
    if (rpct_decoder(llid, len, buf))
      KOUT(" ");
    }
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
static void connect_from_client(int llid, int llid_new)
{
  printf("%d connect_from_client\n", llid);
  msg_mngt_set_callbacks (llid_new, err_cb, rx_cb);
}
/*---------------------------------------------------------------------------*/



/*****************************************************************************/
/* FUNCTION:      main                                                       */
/*---------------------------------------------------------------------------*/
int main (int argc, char *argv[])
{
  int llid, port = PORT;
  uint32_t ip;
  if (ip_string_to_int(&ip, IP)) 
    KOUT("%s", IP);
  doors_io_basic_xml_init(string_tx);
  msg_mngt_init("tst", IO_MAX_BUF_LEN);
  if (argc == 2)
    {
    if (!strcmp(argv[1], "-s"))
      {
      msg_mngt_heartbeat_init(heartbeat);
      string_server_inet(port, connect_from_client, "test");
      printf("\n\n\nSERVER\n\n");
      }
    else if (!strcmp(argv[1], "-c"))
      {
      i_am_client = 1;
      msg_mngt_heartbeat_init(heartbeat);
      llid = string_client_inet(ip, port, err_cb, rx_cb, "test");
      send_first_burst(llid);
      }
    else
      usage(argv[0]);
    }
  else
    usage(argv[0]);
  msg_mngt_loop();
  return 0;
}
/*--------------------------------------------------------------------------*/



