/*****************************************************************************/
/*    Copyright (C) 2006-2018 cloonix@cloonix.net License AGPL-3             */
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
static int count_add_sat=0;
static int count_sav_vm=0;
static int count_sav_vm_all=0;
static int count_del_sat=0;
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
static int count_mucli_dialog_req = 0;
static int count_mucli_dialog_resp = 0;

static int count_evt_stats_endp_sub=0;
static int count_evt_stats_endp=0;
static int count_evt_stats_sysinfo_sub=0;
static int count_evt_stats_sysinfo=0;
static int count_blkd_reports=0;
static int count_blkd_reports_sub=0;

static int count_rpct_recv_report=0;

static int count_evt_txt_doors = 0;
static int count_evt_txt_doors_sub = 0;
static int count_evt_txt_doors_unsub = 0;
static int count_hop_name_list = 0;
static int count_hop_get_name_list = 0;


static int count_evt_msg = 0;
static int count_app_msg = 0;
static int count_diag_msg = 0;
static int count_mucli_req = 0;
static int count_mucli_resp = 0;
static int count_rpc_pid_req  = 0;
static int count_rpc_pid_resp = 0;
static int count_cmd_resp_txt = 0;
static int count_cmd_resp_txt_sub = 0;
static int count_cmd_resp_txt_unsub = 0;

static int count_qmp_sub = 0;
static int count_qmp_req = 0;
static int count_qmp_resp = 0;


/*****************************************************************************/
static void print_all_count(void)
{
  printf("\n\n");
  printf("START COUNT\n");
  printf("%d\n", count_status_ok);
  printf("%d\n", count_status_ko);
  printf("%d\n", count_add_vm);
  printf("%d\n", count_add_sat);
  printf("%d\n", count_del_sat);
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
  printf("%d\n", count_sav_vm);
  printf("%d\n", count_sav_vm_all);

  printf("%d\n", count_evt_stats_endp_sub);
  printf("%d\n", count_evt_stats_endp);

  printf("%d\n", count_evt_stats_sysinfo_sub);
  printf("%d\n", count_evt_stats_sysinfo);
  printf("%d\n", count_blkd_reports);
  printf("%d\n", count_blkd_reports_sub);
  printf("SPECIAL: %d\n", count_rpct_recv_report);

  printf("%d\n", count_evt_txt_doors);
  printf("%d\n", count_evt_txt_doors_sub);
  printf("%d\n", count_evt_txt_doors_unsub);
  printf("%d\n", count_hop_name_list);
  printf("%d\n", count_hop_get_name_list);

  printf("%d\n", count_evt_msg);
  printf("%d\n", count_app_msg);
  printf("%d\n", count_diag_msg);
  printf("%d\n", count_mucli_req);
  printf("%d\n", count_mucli_resp);
  printf("%d\n", count_rpc_pid_req);
  printf("%d\n", count_rpc_pid_resp);
  printf("%d\n", count_cmd_resp_txt);
  printf("%d\n", count_cmd_resp_txt_sub);
  printf("%d\n", count_cmd_resp_txt_unsub);

  printf("%d\n", count_qmp_sub);
  printf("%d\n", count_qmp_req);
  printf("%d\n", count_qmp_resp);

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
  if (kvm->nb_wlan != ikvm->nb_wlan)
    {
    KERR("%d %d", kvm->nb_wlan, ikvm->nb_wlan);
    result = -1;
    }
  if (kvm->nb_eth != ikvm->nb_eth)
    {
    KERR("%d %d", kvm->nb_eth, ikvm->nb_eth);
    result = -1;
    }
  else
    {
    for (k=0; k < ikvm->nb_eth; k++)
      {
      for (l=0; l < 6; l++)
        {
        if (kvm->eth_params[k].mac_addr[l] != ikvm->eth_params[k].mac_addr[l])
          {
          KERR(" ");
          result = -1;
          }
        }
      }
    for (k=0; k < MAX_ETH_VM; k++)
      {
      for (l=0; l < 6; l++)
        {
        if (kvm->eth_params[k].mac_addr[l] != ikvm->eth_params[k].mac_addr[l])
          {
          KERR("%d %d", kvm->eth_params[k].mac_addr[l], 
                        ikvm->eth_params[k].mac_addr[l]);
          result = -1;
          }
        }
      }


    if (memcmp(kvm->eth_params, ikvm->eth_params,
               MAX_ETH_VM*sizeof(t_eth_params)))
      KERR(" ");
    }
  return result;
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
  kvm->nb_eth = my_rand(MAX_ETH_VM);
  kvm->nb_wlan = my_rand(MAX_ETH_VM);
  for (k=0; k < kvm->nb_eth; k++)
    for (l=0; l < 6; l++)
      kvm->eth_params[k].mac_addr[l] = rand() & 0xFF;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void random_c2c(t_topo_c2c *c2c)
{
  random_choice_str(c2c->name, MAX_NAME_LEN);
  random_choice_str(c2c->master_cloonix, MAX_NAME_LEN);
  random_choice_str(c2c->slave_cloonix, MAX_NAME_LEN);
  c2c->local_is_master = rand();
  c2c->is_peered       = rand();
  c2c->ip_slave        = rand();
  c2c->port_slave      = rand();
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void random_snf(t_topo_snf *snf)
{
  random_choice_str(snf->name, MAX_NAME_LEN);
  random_choice_str(snf->recpath, MAX_PATH_LEN);
  snf->capture_on = rand();
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void random_sat(t_topo_sat *sat)
{
  random_choice_str(sat->name, MAX_NAME_LEN);
  sat->type = rand();
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

  topo->nb_kvm = my_rand(30);
  topo->nb_c2c = my_rand(10);
  topo->nb_snf = my_rand(10);
  topo->nb_sat = my_rand(30);
  topo->nb_endp = my_rand(50);

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

  if (topo->nb_snf)
    {
    topo->snf =
    (t_topo_snf *)clownix_malloc(topo->nb_snf * sizeof(t_topo_snf),3);
    memset(topo->snf, 0, topo->nb_snf * sizeof(t_topo_snf));
    for (i=0; i<topo->nb_snf; i++)
      random_snf(&(topo->snf[i]));
    }

  if (topo->nb_sat)
    {
    topo->sat =
    (t_topo_sat *)clownix_malloc(topo->nb_sat * sizeof(t_topo_sat),3);
    memset(topo->sat, 0, topo->nb_sat * sizeof(t_topo_sat));
    for (i=0; i<topo->nb_sat; i++)
      random_sat(&(topo->sat[i]));
    }

  if (topo->nb_endp)
    {
    topo->endp =
    (t_topo_endp *)clownix_malloc(topo->nb_endp * sizeof(t_topo_endp),3);
    memset(topo->endp, 0, topo->nb_endp * sizeof(t_topo_endp));
    for (i=0; i<topo->nb_endp; i++)
      random_endp(&(topo->endp[i]));
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
void rpct_recv_pid_req(void *ptr, int llid, int itid, 
                       char *iname, int inum)
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
    rpct_send_pid_req(ptr, llid, tid, name, num);
    }
  else
    rpct_send_pid_req(ptr, llid, itid, iname, inum);
  count_rpc_pid_req++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_pid_resp(void *ptr, int llid, int itid,
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
    rpct_send_pid_resp(ptr, llid, tid, name, num, toppid, pid);
    }
  else
    rpct_send_pid_resp(ptr, llid, itid, iname, inum, itoppid, ipid);
  count_rpc_pid_resp++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_hop_sub(void *ptr, int llid, int itid, int itype_evt)
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
    rpct_send_hop_sub(ptr, llid, tid, type_evt);
    }
  else
    rpct_send_hop_sub(ptr, llid, itid, itype_evt);
  count_cmd_resp_txt_sub++;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void rpct_recv_hop_unsub(void *ptr, int llid, int itid)
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
    rpct_send_hop_unsub(ptr, llid, tid);
    }
  else
    rpct_send_hop_unsub(ptr, llid, itid);
  count_cmd_resp_txt_unsub++;
}
/*---------------------------------------------------------------------------*/



/*****************************************************************************/
void rpct_recv_hop_msg(void *ptr, int llid, int itid,
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
    rpct_send_hop_msg(ptr, llid, tid, type_evt, txt);
    }
  else
    rpct_send_hop_msg(ptr, llid, itid, itype_evt, itxt);
  count_cmd_resp_txt++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_evt_msg(void *ptr, int llid, int itid, char *iline)
{
  static char line[2001];
  static int tid;
  if (i_am_client)
    {
    if (count_evt_msg)
      {
      if (strcmp(iline, line))
        KOUT("%s\n\n %s\n\n", iline, line);
      if (itid != tid)
        KOUT(" ");
      }
    tid = rand();
    random_choice_str(line, 2000);
    rpct_send_evt_msg(ptr, llid, tid, line);
    }
  else
    rpct_send_evt_msg(ptr, llid, itid, iline);
  count_evt_msg++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_app_msg(void *ptr, int llid, int itid, char *iline)
{
  static char line[2001];
  static int tid;
  if (i_am_client)
    {
    if (count_app_msg)
      {
      if (strcmp(iline, line))
        KOUT(" ");
      if (itid != tid)
        KOUT(" ");
      }
    tid = rand();
    random_choice_str(line, 2000);
    rpct_send_app_msg(ptr, llid, tid, line);
    }
  else
    rpct_send_app_msg(ptr, llid, itid, iline);
  count_app_msg++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_diag_msg(void *ptr, int llid, int itid, char *iline)
{
  static char line[2001];
  static int tid;
  if (i_am_client)
    {
    if (count_diag_msg)
      {
      if (strcmp(iline, line))
        KOUT(" ");
      if (itid != tid)
        KOUT(" ");
      }
    tid = rand();
    random_choice_str(line, 2000);
    rpct_send_diag_msg(ptr, llid, tid, line);
    }
  else
    rpct_send_diag_msg(ptr, llid, itid, iline);
  count_diag_msg++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_cli_req(void *ptr, int llid, int itid,
                    int icli_llid, int icli_tid, char *iline)
{
  static char line[2001];
  static int tid, cli_llid, cli_tid;
  if (i_am_client)
    {
    if (count_mucli_req)
      {
      if (strcmp(iline, line))
        KOUT(" ");
      if (itid != tid)
        KOUT(" ");
      if (icli_tid != cli_tid)
        KOUT(" ");
      if (icli_llid != cli_llid)
        KOUT(" ");
      }
    tid = rand();
    cli_tid = rand();
    cli_llid = rand();
    random_choice_str(line, 2000);
    rpct_send_cli_req(ptr, llid, tid, cli_llid, cli_tid, line);
    }
  else
    rpct_send_cli_req(ptr, llid, itid, icli_llid, icli_tid, iline);
  count_mucli_req++;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void rpct_recv_cli_resp(void *ptr, int llid, int itid,
                     int icli_llid, int icli_tid, char *iline)
{
  static char line[2001];
  static int tid, cli_llid, cli_tid;
  if (i_am_client)
    {
    if (count_mucli_resp)
      {
      if (strcmp(iline, line))
        KOUT(" ");
      if (itid != tid)
        KOUT(" ");
      if (icli_tid != cli_tid)
        KOUT(" ");
      if (icli_llid != cli_llid)
        KOUT(" ");
      }
    tid = rand();
    cli_tid = rand();
    cli_llid = rand();
    random_choice_str(line, 2000);
    rpct_send_cli_resp(ptr, llid, tid, cli_llid, cli_tid, line);
    }
  else
    rpct_send_cli_resp(ptr, llid, itid, icli_llid, icli_tid, iline);
  count_mucli_resp++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_report(void *ptr, int llid, t_blkd_item *iitem)
{
  static t_blkd_item item;
  if (i_am_client)
    {
    if (count_rpct_recv_report)
      {
      if (memcmp(&item, iitem, sizeof(t_blkd_item)))
        KOUT(" ");
      }
    memset(&item, 0, sizeof(t_blkd_item));
    random_choice_str(item.sock, MAX_PATH_LEN);
    random_choice_str(item.rank_name, MAX_NAME_LEN);
    item.rank_num   = rand();
    item.rank_tidx   = rand();
    item.rank   = rand();
    item.pid   = rand();
    item.llid   = rand();
    item.fd   = rand();
    item.sel_tx   = rand();
    item.sel_rx   = rand();
    item.fifo_tx   = rand();
    item.fifo_rx   = rand();
    item.queue_tx   = rand();
    item.queue_rx   = rand();
    item.bandwidth_tx   = rand();
    item.bandwidth_rx   = rand();
    item.stop_tx   = rand();
    item.stop_rx   = rand();
    item.dist_flow_ctrl_tx   = rand();
    item.dist_flow_ctrl_rx   = rand();
    item.drop_tx   = rand();
    item.drop_tx   *= rand();
    item.drop_rx   = rand();
    item.drop_rx   *= rand();
    rpct_send_report(ptr, llid, &item);
    }
  else
    rpct_send_report(ptr, llid, iitem);
  count_rpct_recv_report++;
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
      topo_config_diff(&(itopo->clc), &(topo->clc));
      if (topo->nb_kvm != itopo->nb_kvm)
        KOUT("%d %d", topo->nb_kvm, itopo->nb_kvm);
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
void recv_blkd_reports_sub(int llid, int itid, int isub)
{
  static int tid, sub;
  if (i_am_client)
    {
    if (count_blkd_reports_sub)
      {
      if (itid != tid)
        KOUT(" ");
      if (isub != sub)
        KOUT(" ");
      }
    tid = rand();
    sub = rand();
    send_blkd_reports_sub(llid, tid, sub);
    }
  else
    send_blkd_reports_sub(llid, itid, isub);
  count_blkd_reports_sub++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_blkd_reports(int llid, int itid, t_blkd_reports *iblkd)
{
  static t_blkd_item g_it[TST_BLKD_MAX_QTY+1];
  static t_blkd_reports blkd;
  static int tid;
  t_blkd_item *it;
  int i;
  if (i_am_client)
    {
    if (count_blkd_reports)
      {
      if (itid != tid)
        KOUT(" ");
      if (blkd.nb_blkd_reports != iblkd->nb_blkd_reports)
        KOUT("%d %d", blkd.nb_blkd_reports, iblkd->nb_blkd_reports);
      for (i=0; i<blkd.nb_blkd_reports; i++)
        {
        if (memcmp(&(g_it[i]), &(iblkd->blkd_item[i]), sizeof(t_blkd_item))) 
          KOUT("%d %d", i, blkd.nb_blkd_reports);
        }
      }
    blkd.nb_blkd_reports = my_rand(TST_BLKD_MAX_QTY) + 1;
    blkd.blkd_item = g_it;
    tid = rand();
    for (i=0; i<blkd.nb_blkd_reports; i++)
      {
      it = &(g_it[i]); 
      random_choice_str(it->sock, MAX_PATH_LEN);
      random_choice_str(it->rank_name, MAX_NAME_LEN);
      it->rank_num = rand();
      it->rank_tidx = rand();
      it->rank = rand();
      it->pid = rand();
      it->llid = rand();
      it->fd = rand();
      it->sel_tx = rand();
      it->sel_rx = rand();
      it->fifo_tx = rand();
      it->fifo_rx = rand();
      it->queue_tx = rand();
      it->queue_rx = rand();
      it->bandwidth_tx = rand();
      it->bandwidth_rx = rand();
      it->stop_tx = rand();
      it->stop_rx = rand();
      it->dist_flow_ctrl_tx = rand();
      it->dist_flow_ctrl_rx = rand();
      it->drop_tx = rand();
      it->drop_tx *= rand();
      it->drop_rx = rand();
      it->drop_rx *= rand();
      }
    send_blkd_reports(llid, tid, &blkd);
    }
  else
    {
    send_blkd_reports(llid, itid, iblkd);
    }
  count_blkd_reports++;
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
void recv_sav_vm(int llid, int itid, char *iname, int itype, char *ipath)
{
  static int tid;
  static int type;
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
      if (itype != type)
        KOUT(" ");
      }
    tid = rand();
    type = rand();
    random_choice_str(name, MAX_NAME_LEN);
    random_choice_str(path, MAX_PATH_LEN);
    send_sav_vm(llid, tid, name, type, path);
    }
  else
    send_sav_vm(llid, itid, iname, itype, ipath);
  count_sav_vm++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_sav_vm_all(int llid, int itid, int itype, char *ipath)
{
  static int tid;
  static int type;
  static char path[MAX_PATH_LEN];
  if (i_am_client)
    {
    if (count_sav_vm_all)
      {
      if (strcmp(ipath, path))
        KOUT(" ");
      if (itid != tid)
        KOUT(" ");
      if (itype != type)
        KOUT(" ");
      }
    tid = rand();
    type = rand();
    random_choice_str(path, MAX_PATH_LEN);
    send_sav_vm_all(llid, tid, type, path);
    }
  else
    send_sav_vm_all(llid, itid, itype, ipath);
  count_sav_vm_all++;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void recv_add_sat(int llid, int itid, char *iname, int imutype, 
                  t_c2c_req_info *ic2c_req_info)
{
  static int tid, mutype;
  static char name[MAX_NAME_LEN];
  static t_c2c_req_info c2c_req_info;
  static t_c2c_req_info *ptr_c2c_req_info;
  if (i_am_client)
    {
    if (count_add_sat)
      {
      if (strcmp(name,  iname))
        KOUT(" ");
      if (itid != tid)
        KOUT(" ");
      if (imutype != mutype)
        KOUT(" ");
      if (ptr_c2c_req_info)
        {
        if (strcmp(ic2c_req_info->cloonix_slave, c2c_req_info.cloonix_slave))
          KOUT("%s  %s",ic2c_req_info->cloonix_slave,c2c_req_info.cloonix_slave);
        if (strcmp(ic2c_req_info->passwd_slave, c2c_req_info.passwd_slave))
          KOUT("%s  %s",ic2c_req_info->passwd_slave,c2c_req_info.passwd_slave);
        if (ic2c_req_info->ip_slave != c2c_req_info.ip_slave)
          KOUT("%d  %d",ic2c_req_info->ip_slave,c2c_req_info.ip_slave);
        if (ic2c_req_info->port_slave != c2c_req_info.port_slave)
          KOUT("%d  %d",ic2c_req_info->port_slave,c2c_req_info.port_slave);
        if (memcmp(ic2c_req_info, &c2c_req_info, sizeof(t_c2c_req_info)))
          KOUT(" ");
        }
      else
        {
        if (ic2c_req_info)
          KOUT(" ");
        }
      }
    if (rand()%10)
      {
      random_choice_str(c2c_req_info.cloonix_slave, MAX_NAME_LEN);
      c2c_req_info.ip_slave = rand();
      c2c_req_info.port_slave = rand();
      random_choice_str(c2c_req_info.passwd_slave, MSG_DIGEST_LEN);
      ptr_c2c_req_info = &c2c_req_info;
      mutype = endp_type_c2c;
      }
    else
      {
      ptr_c2c_req_info = NULL;
      mutype = rand();
      }
    tid = rand();
    random_choice_str(name, MAX_NAME_LEN);
    send_add_sat(llid, tid, name, mutype, ptr_c2c_req_info);
    }
  else
    send_add_sat(llid, itid, iname, imutype, ic2c_req_info);
  count_add_sat++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_del_sat(int llid, int itid, char *isat)
{
  static int tid;
  static char sat[MAX_NAME_LEN];
  if (i_am_client)
    {
    if (count_del_sat)
      {
      if (strcmp(sat,  isat))
        KOUT("%s %s ", sat, isat);
      if (itid != tid)
        KOUT(" ");
      }
    tid = rand();
    random_choice_str(sat, MAX_NAME_LEN);
    send_del_sat(llid, tid, sat);
    }
  else
    send_del_sat(llid, itid, isat);
  count_del_sat++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_add_lan_endp(int llid, int itid, char *isat, int inum , char *ilan) 
{
  static int tid, num;
  static char sat[MAX_NAME_LEN];
  static char lan[MAX_NAME_LEN];
  if (i_am_client)
    {
    if (count_add_lan_endp)
      {
      if (strcmp(sat, isat))
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
    random_choice_str(sat, MAX_NAME_LEN);
    send_add_lan_endp(llid, tid, sat, num, lan);
    }
  else
    send_add_lan_endp(llid, itid, isat, inum, ilan);
  count_add_lan_endp++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_del_lan_endp(int llid, int itid, char *isat, int inum, char *ilan)
{
  static int tid, num;
  static char sat[MAX_NAME_LEN];
  static char lan[MAX_NAME_LEN];
  if (i_am_client)
    {
    if (count_del_lan_endp)
      {
      if (strcmp(sat, isat))
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
    random_choice_str(sat, MAX_NAME_LEN);
    send_del_lan_endp(llid, tid, sat, num, lan);
    }
  else
    send_del_lan_endp(llid, itid, isat, inum, ilan);
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
      if (memcmp(lst, ilst, qty*sizeof(t_pid_lst)))
        KOUT(" ");
      clownix_free(lst, __FUNCTION__);
      }
    tid = rand();
    qty = my_rand(50);
    lst = (t_pid_lst *)clownix_malloc(qty*sizeof(t_pid_lst),7);
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
void recv_list_commands_req(int llid, int itid)
{
  static int tid;
  if (i_am_client)
    {
    if (count_list_commands_req)
      {
      if (itid != tid)
        KOUT(" ");
      }
    tid = rand();
    send_list_commands_req(llid, tid);
    }
  else
    send_list_commands_req(llid, itid);
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
void recv_mucli_dialog_req(int llid, int itid, char *iname, int ieth, 
                           char *iline)
{
  static char name[MAX_NAME_LEN];
  static char line[2001];
  static int tid, eth;
  if (i_am_client)
    {
    if (count_mucli_dialog_req)
      {
      if (strcmp(iline, line))
        KOUT(" ");
      if (strcmp(iname, name))
        KOUT(" ");
      if (itid != tid)
        KOUT(" ");
      if (ieth != eth)
        KOUT(" ");
      }
    random_choice_str(name, MAX_NAME_LEN);
    random_choice_str(line, 2000);
    tid = rand();
    eth = rand();
    send_mucli_dialog_req(llid, tid, name, eth, line);
    }
  else
    send_mucli_dialog_req(llid, itid, iname, ieth, iline);
  count_mucli_dialog_req++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_mucli_dialog_resp(int llid, int itid, char *iname, int ieth, 
                            char *iline, int istatus)
{
  static char name[MAX_NAME_LEN];
  static char line[2001];
  static int tid, eth, status;
  if (i_am_client)
    {
    if (count_mucli_dialog_resp)
      {
      if (strcmp(iline, line))
        KOUT(" ");
      if (strcmp(iname, name))
        KOUT(" ");
      if (itid != tid)
        KOUT(" ");
      if (istatus != status)
        KOUT(" ");
      if (ieth != eth)
        KOUT(" ");
      }
    random_choice_str(name, MAX_NAME_LEN);
    random_choice_str(line, 2000);
    tid = rand();
    status = rand();
    eth = rand();
    send_mucli_dialog_resp(llid, tid, name, eth, line, status);
    }
  else
    send_mucli_dialog_resp(llid, itid, iname, ieth, iline, istatus);
  count_mucli_dialog_resp++;
}
/*---------------------------------------------------------------------------*/



/*****************************************************************************/
void recv_qmp_sub(int llid, int itid, char *iname)
{
  static char name[MAX_NAME_LEN];
  static int tid;
  if (i_am_client)
    {
    if (count_qmp_sub)
      {
      if (strcmp(iname, name))
        KOUT(" ");
      if (itid != tid)
        KOUT(" ");
      }
    tid = rand();
    random_choice_str(name, MAX_NAME_LEN);
    send_qmp_sub(llid, tid, name);
    }
  else
    send_qmp_sub(llid, itid, iname);
  count_qmp_sub++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_qmp_req(int llid, int itid, char *iname, char *iline)
{
  static char name[MAX_NAME_LEN];
  static char line[MAX_RPC_MSG_LEN];
  static int tid;
  if (i_am_client)
    {
    if (count_qmp_req)
      {
      if (strcmp(iname, name))
        KOUT(" ");
      if (strcmp(iline, line))
        KOUT("\n\n%s\n\n%s\n", iline, line);
      if (itid != tid)
        KOUT(" ");
      }
    tid = rand();
    random_choice_str(name, MAX_NAME_LEN);
    random_choice_str(line, MAX_RPC_MSG_LEN);
    send_qmp_req(llid, tid, name, line);
    }
  else
    send_qmp_req(llid, itid, iname, iline);
  count_qmp_req++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_qmp_resp(int llid, int itid, char *iname, char *iline, int istatus)
{
  static char name[MAX_NAME_LEN];
  static char line[MAX_RPC_MSG_LEN];
  static int tid, status;
  if (i_am_client)
    {
    if (count_qmp_resp)
      {
      if (strcmp(iname, name))
        KOUT(" ");
      if (strcmp(iline, line))
        KOUT("\n\n%s\n\n%s\n", iline, line);
      if (itid != tid)
        KOUT(" ");
      if (istatus != status)
        KOUT(" ");
      }
    tid = rand();
    status = rand();
    random_choice_str(name, MAX_NAME_LEN);
    random_choice_str(line, MAX_RPC_MSG_LEN);
    send_qmp_resp(llid, tid, name, line, status);
    }
  else
    send_qmp_resp(llid, itid, iname, iline, istatus);
  count_qmp_resp++;
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
  recv_add_sat(llid, 0, NULL, 0, NULL);
  recv_del_sat(llid, 0, NULL);
  recv_kill_uml_clownix(llid,0);
  recv_del_all(llid,0);
  recv_add_lan_endp(llid, 0, NULL, 0, NULL);
  recv_del_lan_endp(llid, 0, NULL, 0, NULL);
  recv_list_pid_req(llid,0);
  recv_list_pid_resp(llid,0, 0, NULL);
  recv_list_commands_req(llid,0);
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
  recv_sav_vm(llid, 0, NULL, 0, NULL);
  recv_sav_vm_all(llid, 0, 0, NULL);
  recv_mucli_dialog_req(llid, 0, NULL, 0, NULL);
  recv_mucli_dialog_resp(llid, 0, NULL, 0, NULL, 0);
  recv_evt_stats_endp_sub(llid, 0, NULL, 0, 0);
  recv_evt_stats_endp(llid, 0, NULL, NULL, 0, NULL, 0);
  recv_evt_stats_sysinfo_sub(llid, 0, NULL, 0);
  recv_evt_stats_sysinfo(llid, 0, NULL, NULL, NULL, NULL, 0);
  recv_blkd_reports(llid, 0, NULL);
  recv_blkd_reports_sub(llid, 0, 0);

  recv_hop_get_name_list_doors(llid, 0);
  recv_hop_name_list_doors(llid, 0, 0, NULL);
  recv_hop_evt_doors_sub(llid, 0, 0, 0, NULL);
  recv_hop_evt_doors_unsub(llid, 0);
  recv_hop_evt_doors(llid, 0, 0, NULL, NULL);

  rpct_recv_report(NULL, llid, NULL);


  rpct_recv_evt_msg(NULL, llid, 0, NULL);
  rpct_recv_app_msg(NULL, llid, 0, NULL);
  rpct_recv_diag_msg(NULL, llid, 0, NULL);
  rpct_recv_cli_req(NULL, llid, 0, 0, 0, NULL);
  rpct_recv_cli_resp(NULL, llid, 0, 0, 0, NULL);
  rpct_recv_pid_req(NULL, llid, 0, NULL, 0);
  rpct_recv_pid_resp(NULL, llid, 0, NULL, 0, 0, 0);
  rpct_recv_hop_sub(NULL, llid, 0, 0);
  rpct_recv_hop_unsub(NULL, llid, 0);
  rpct_recv_hop_msg(NULL, llid, 0, 0, NULL);

  recv_qmp_sub(llid, 0, NULL);
  recv_qmp_req(llid, 0, NULL, NULL);
  recv_qmp_resp(llid, 0, NULL, NULL, 0);

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
static void err_cb (void *ptr, int llid, int err, int from)
{
  (void) ptr;
  printf("ERROR %d  %d %d\n", llid, err, from);
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void rx_cb(int llid, int len, char *buf)
{
  if (doors_io_basic_decoder(llid, len, buf))
    {
    if (rpct_decoder(NULL, llid, len, buf))
      KOUT(" ");
    }
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
static void connect_from_client(void *ptr, int llid, int llid_new)
{
  (void) ptr;
  printf("%d connect_from_client\n", llid);
  msg_mngt_set_callbacks (llid_new, err_cb, rx_cb);
}
/*---------------------------------------------------------------------------*/



/*****************************************************************************/
/* FUNCTION:      main                                                       */
/*---------------------------------------------------------------------------*/
int main (int argc, char *argv[])
{
  int llid, ip, port = PORT;
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



