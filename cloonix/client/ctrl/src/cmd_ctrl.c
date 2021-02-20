/*****************************************************************************/
/*    Copyright (C) 2006-2021 clownix@clownix.net License AGPL-3             */
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
/*---------------------------------------------------------------------------*/
#include "io_clownix.h"
#include "rpc_clownix.h"
#include "doorways_sock.h"
#include "client_clownix.h"
#include "file_read_write.h"
#include "cmd_help_fn.h"
#include "layout_rpc.h"
#include "cloonix_conf_info.h"
/*---------------------------------------------------------------------------*/
void layout_exit_upon_layout_param(void);
static t_hop_list *g_hop_list;
static int g_hop_list_nb_item;
static char glob_layout_path[MAX_PATH_LEN];


t_cloonix_conf_info *get_own_cloonix_conf_info(void);

/*****************************************************************************/
char *get_glob_layout_path(void)
{
  return (glob_layout_path);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void work_dir_resp(int tid, t_topo_clc *conf)
{
  printf("CLOONIX_VERSION=%s\n", conf->version);
  printf("CLOONIX_TREE=%s\n", conf->bin_dir);
  printf("CLOONIX_WORK=%s\n", conf->work_dir);
  printf("CLOONIX_BULK=%s\n", conf->bulk_dir);
  exit(0);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void callback_print(int tid, char *info)
{
  printf("%s\n", info);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void callback_sys(int tid, t_sys_info *sys)
{
  printf("%s", to_ascii_sys(sys));
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void callback_end(int tid, int status, char *err)
{
  if (tid)
    KOUT(" ");
  if (!status)
    {
    printf("OK %s\n", err);
    exit (0);
    }
  else
    {
    printf("%s\n", err);
    exit (-1);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void callback_list_commands(int tid, int qty, t_list_commands *list)
{
  int i;
  for (i=0; i<qty; i++)
    printf("\n%s", list[i].cmd);
  printf("\n");
  exit(0);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void callback_topo_names(int tid, t_topo_info *topo)
{
  int i;
  char *rootfs_type;
  for (i=0; i<topo->nb_kvm; i++)
    {
    if (topo->kvm[i].vm_config_flags & VM_CONFIG_FLAG_PERSISTENT)
      rootfs_type = "persistent writes rootfs";
    else
      rootfs_type = "evanescent writes rootfs";
    printf("\n%s %s\n", topo->kvm[i].name, rootfs_type);
    printf("Rootfs:%s\n", topo->kvm[i].rootfs_used);
    if (topo->kvm[i].vm_config_flags & VM_FLAG_DERIVED_BACKING)
      printf("Backing:%s\n", topo->kvm[i].rootfs_backing); 
    }
  printf("\n\n");
  exit(0);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static char *get_type_endp(int type)
{
  char *result = "notfound";
  switch (type)
    {
    case endp_type_pci:
      result = "pci"; 
      break;
    case endp_type_phy:
      result = "phy"; 
      break;
    case endp_type_kvm_dpdk:
      result = "dpdk"; 
      break;
    case endp_type_kvm_sock:
      result = "sock"; 
      break;
    case endp_type_kvm_wlan:
      result = "wlan"; 
      break;
    case endp_type_c2c:
      result = "c2c"; 
      break;
    case endp_type_snf:
      result = "snf"; 
      break;
    case endp_type_tap:
      result = "tap"; 
      break;
    case endp_type_nat:
      result = "nat"; 
      break;
    case endp_type_a2b:
      result = "a2b"; 
      break;
    case endp_type_wif:
      result = "wif"; 
      break;
    case endp_type_d2d:
      result = "d2d"; 
      break;
    default:
      KOUT("%d", type);
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void print_endpoint_info(t_topo_endp *endp)
{
  int i, nb = endp->lan.nb_lan;
  t_lan_group_item *lan = endp->lan.lan;
  printf("\n%s:%s %d  lan:", get_type_endp(endp->type), endp->name, endp->num);  
  for (i=0; i<nb; i++)
    printf(" %s", lan[i].lan);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void print_phy_info(t_topo_phy *phy)
{
  printf("\n%s index:%d flags:0x%X drv:%s pci:%s mac:%s vendor:%s device:%s", 
         phy->name, phy->index, phy->flags, phy->drv, phy->pci, phy->mac,
         phy->vendor, phy->device);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void print_pci_info(t_topo_pci *pci)
{
  printf("\n%s drv:%s unused:%s", pci->pci, pci->drv, pci->unused);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void print_bridges_info(t_topo_bridges *br)
{
  int i; 
  printf("\novs_bridge:%s nb_ports:%d", br->br, br->nb_ports);
  for (i=0; i<br->nb_ports; i++)
    printf(" %s", br->ports[i]);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void print_mirrors_info(t_topo_mirrors *mir)
{
  printf("\novs_mirror:%s", mir->mir);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void d2d_info(t_topo_d2d *d2d)
{
  uint16_t tcp_port, loc_udp_port, dist_udp_port;
  int local_is_master, tcp_connection_peered;
  int udp_connection_peered, ovs_lan_attach_ready;
  char tcp_ip[MAX_NAME_LEN];
  char loc_udp_ip[MAX_NAME_LEN];
  char dist_udp_ip[MAX_NAME_LEN];
  char *dist = d2d->dist_cloonix;
  char *lan = d2d->lan;
  char *nm = d2d->name;
  int_to_ip_string (d2d->dist_tcp_ip, tcp_ip);
  int_to_ip_string (d2d->dist_udp_ip, dist_udp_ip);
  int_to_ip_string (d2d->loc_udp_ip, loc_udp_ip);
  tcp_port = d2d->dist_tcp_port;
  loc_udp_port = d2d->loc_udp_port;
  dist_udp_port = d2d->dist_udp_port;
  local_is_master = d2d->local_is_master;
  tcp_connection_peered = d2d->tcp_connection_peered;
  udp_connection_peered = d2d->udp_connection_peered;
  ovs_lan_attach_ready = d2d->ovs_lan_attach_ready;
  if (local_is_master)
    {
    printf("\nd2d:%s:Local_is_master", nm);
    if (tcp_connection_peered)
      {
      printf("\nd2d:%s:tcp_peered %s %s:%hu", nm, dist, tcp_ip, tcp_port);
      }
    else
      {
      printf("\nd2d:%s:tcp_not_peered %s %s:%hu", nm, dist, tcp_ip, tcp_port);
      }
    }
  else
    {
    printf("\nd2d:%s:distant_is_master", nm);
    printf("\nd2d:%s:tcp_peered %s", nm, dist);
    }
  if (udp_connection_peered)
    printf("\nd2d:%s:udp_peered", nm);
  else
    printf("\nd2d:%s:udp_not_peered", nm);
  if (ovs_lan_attach_ready)
    printf("\nd2d:%s:ovs_lan_attached %s", nm, lan);
  else
    printf("\nd2d:%s:ovs_lan_not_attached", nm);
  printf("\nd2d:%s:udp_local %s:%hu", nm, loc_udp_ip, loc_udp_port);
  printf("\nd2d:%s:udp_distant %s:%hu", nm, dist_udp_ip, dist_udp_port);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void a2b_info(t_topo_a2b *a2b)
{
  int i, *delay, *loss, *brate;
  delay = a2b->delay;
  loss  = a2b->loss;
  brate = a2b->brate;
  for (i=0; i<2; i++)
    {
    printf("\na2b:%s:side%d: delay=%d loss=%d rate=%d",
           a2b->name, i, delay[i], loss[i], brate[i]);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void callback_topo_topo(int tid, t_topo_info *topo)
{
  int i, type;
  printf("\n");


  for (i=0; i<topo->nb_kvm; i++)
    {
    if (i == 0)
      printf("\n");
    if (topo->kvm[i].vm_config_flags & VM_FLAG_CLOONIX_AGENT_PING_OK)
      printf("\n%s vm_id: %d ok", topo->kvm[i].name, topo->kvm[i].vm_id);
    else
      printf("\n%s vm_id: %d ko", topo->kvm[i].name, topo->kvm[i].vm_id);
    }
  for (i=0; i<topo->nb_c2c; i++)
    {
    if (i == 0)
      printf("\n");
    printf("\nc2c:%s", topo->c2c[i].name);
    }

  for (i=0; i<topo->nb_d2d; i++)
    {
    if (i == 0)
      printf("\n");
    d2d_info(&(topo->d2d[i]));
    }

  for (i=0; i<topo->nb_a2b; i++)
    {
    if (i == 0)
      printf("\n");
    a2b_info(&(topo->a2b[i]));
    }

  for (i=0; i<topo->nb_sat; i++)
    {
    if (i == 0)
      printf("\n");
    type = topo->sat[i].type;
    if (type == endp_type_tap) 
      printf("\ntap:%s", topo->sat[i].name);
    else if (type == endp_type_snf) 
      printf("\nsnf:%s", topo->sat[i].name);
    else if (type == endp_type_nat) 
      printf("\nnat:%s", topo->sat[i].name);
    else if (type == endp_type_wif) 
      printf("\nwif:%s", topo->sat[i].name);
    else 
      printf("\nerror:%s", topo->sat[i].name);
    printf("\n");
    }
  for (i=0; i<topo->nb_endp; i++)
    {
    if (i == 0)
      printf("\n");
    print_endpoint_info(&(topo->endp[i]));
    }

  for (i=0; i<topo->nb_phy; i++)
    {
    if (i == 0)
      printf("\n");
    print_phy_info(&(topo->phy[i]));
    }

  for (i=0; i<topo->nb_pci; i++)
    {
    if (i == 0)
      printf("\n");
    print_pci_info(&(topo->pci[i]));
    }

  for (i=0; i<topo->nb_bridges; i++)
    {
    if (i == 0)
      printf("\n");
    print_bridges_info(&(topo->bridges[i]));
    }

  for (i=0; i<topo->nb_mirrors; i++)
    {
    if (i == 0)
      printf("\n");
    print_mirrors_info(&(topo->mirrors[i]));
    }



  printf("\n\n");
  exit(0);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void callback_pid(int tid, int qty, t_pid_lst *pid)
{
  int i;
  printf("\n");
  for (i=0; i<qty; i++)
    printf("\n  %10s  %d", pid[i].name, pid[i].pid);
  printf("\n");
  printf("\n");
  printf("\n");
  exit(0);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int param_tester(char *param, int min, int max)
{
  int result;
  char *endptr;
  result = (int) strtol(param, &endptr, 10);
  if ((endptr[0] != 0) || ( result < min ) || ( result > max))
    result = -1;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int param_get(char *param)
{
  int result;
  char *endptr;
  result = (int) strtol(param, &endptr, 10);
  if (endptr[0] != 0)
    result = -1;
  return result;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
int cmd_kill(int argc, char **argv)
{
  init_connection_to_uml_cloonix_switch();
  client_kill_daemon(0, callback_end);
  return 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cmd_delall(int argc, char **argv)
{
  init_connection_to_uml_cloonix_switch();
  send_layout_event_sub(get_clownix_main_llid(), 0, 666);
  client_del_all(0, callback_end);
  return 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cmd_name_dump(int argc, char **argv)
{
  init_connection_to_uml_cloonix_switch();
  client_topo_sub(0, callback_topo_names);
  return 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cmd_topo_dump(int argc, char **argv)
{
  init_connection_to_uml_cloonix_switch();
  client_topo_sub(0, callback_topo_topo);
  return 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cmd_config_dump(int argc, char **argv)
{
  init_connection_to_uml_cloonix_switch();
  client_get_path(0, work_dir_resp);
  return 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cmd_list_commands(int argc, char **argv)
{
  init_connection_to_uml_cloonix_switch();
  client_list_commands(0, callback_list_commands);
  return 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cmd_lay_commands(int argc, char **argv)
{
  init_connection_to_uml_cloonix_switch();
  client_lay_commands(0, callback_list_commands);
  return 0;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
int cmd_pid_dump(int argc, char **argv)
{
  init_connection_to_uml_cloonix_switch();
  client_req_pids(0,callback_pid);
  return 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cmd_halt_vm(int argc, char **argv)
{
  int result = -1;
  char *name; 
  if (argc == 1)
    {
    name = argv[0];
    if (strlen(name)>2)
      {
      result = 0;
      init_connection_to_uml_cloonix_switch();
      client_halt_vm(0, callback_end, name, 1);
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cmd_reboot_vm(int argc, char **argv)
{
  int result = -1;
  char *name;
  if (argc == 1)
    {
    name = argv[0];
    if (strlen(name)>2)
      {
      result = 0;
      init_connection_to_uml_cloonix_switch();
      client_reboot_vm(0, callback_end, name, 1);
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cmd_qhalt_vm(int argc, char **argv)
{
  int result = -1;
  char *name;
  if (argc == 1)
    {
    name = argv[0];
    if (strlen(name)>2)
      {
      result = 0;
      init_connection_to_uml_cloonix_switch();
      client_halt_vm(0, callback_end, name, 0);
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cmd_qreboot_vm(int argc, char **argv)
{
  int result = -1;
  char *name;
  if (argc == 1)
    {
    name = argv[0];
    if (strlen(name)>2)
      {
      result = 0;
      init_connection_to_uml_cloonix_switch();
      client_reboot_vm(0, callback_end, name, 0);
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int local_cmd_sav(int is_full, int argc, char **argv)
{
  int result = -1;
  char *name, *sav_rootfs_path;
  if (argc == 2)
    {
    name = argv[0];
    sav_rootfs_path = argv[1];
    if ((strlen(name)>1) && (strlen(sav_rootfs_path)>1))
      {
      result = 0;
      init_connection_to_uml_cloonix_switch();
      client_sav_vm(0, callback_end, name, is_full, sav_rootfs_path);
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cmd_sav_full(int argc, char **argv)
{
  int result = local_cmd_sav(1, argc, argv);
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cmd_sav_derived(int argc, char **argv)
{
  int result = local_cmd_sav(0, argc, argv);
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cmd_del_vm(int argc, char **argv)
{
  int result = -1;
  char *name;
  if (argc == 1)
    {
    result = 0;
    name = argv[0];
    init_connection_to_uml_cloonix_switch();
    client_del_vm(0, callback_end, name);
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cmd_add_tap(int argc, char **argv)
{
  int result = -1;
  if (argc == 1)
    {
    result = 0;
    init_connection_to_uml_cloonix_switch();
    client_add_sat(0, callback_end, argv[0], endp_type_tap, NULL);
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cmd_add_phy(int argc, char **argv)
{
  int result = -1;
  if (argc == 1)
    {
    result = 0;
    init_connection_to_uml_cloonix_switch();
    client_add_sat(0, callback_end, argv[0], endp_type_phy, NULL);
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cmd_add_pci(int argc, char **argv)
{
  int result = -1;
  if (argc == 1)
    {
    result = 0;
    init_connection_to_uml_cloonix_switch();
    client_add_sat(0, callback_end, argv[0], endp_type_pci, NULL);
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cmd_add_wif(int argc, char **argv)
{
  int result = -1;
  if (argc == 1)
    {
    result = 0;
    init_connection_to_uml_cloonix_switch();
    client_add_sat(0, callback_end, argv[0], endp_type_wif, NULL);
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cmd_add_snf(int argc, char **argv)
{
  int result = -1;
  if (argc == 1)
    {
    result = 0;
    init_connection_to_uml_cloonix_switch();
    client_add_sat(0, callback_end, argv[0], endp_type_snf, NULL);
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cmd_add_a2b(int argc, char **argv)
{
  int result = -1;
  if (argc == 1)
    {
    result = 0;
    init_connection_to_uml_cloonix_switch();
    client_add_a2b(0, callback_end, argv[0]);
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cmd_cnf_a2b(int argc, char **argv)
{
  int dir, val, cmd, result = -1;
  if (argc != 4)
    KERR(" ");
  else
    {
    dir = param_tester(argv[1], 0, 1);
    val = param_get(argv[3]);
    if (!strcmp("delay", argv[2]))
      cmd = a2b_type_delay;
    else if (!strcmp("loss", argv[2]))
      cmd = a2b_type_loss;
    else if (!strcmp("qsize", argv[2]))
      cmd = a2b_type_qsize;
    else if (!strcmp("bsize", argv[2]))
      cmd = a2b_type_bsize;
    else if (!strcmp("rate", argv[2]))
      cmd = a2b_type_brate;
    else
      cmd = -1;
    if ((dir >= 0) && (val >= 0) && (cmd > 0))
      {
      result = 0;
      init_connection_to_uml_cloonix_switch();
      client_cnf_a2b(0, callback_end, argv[0], dir, cmd, val);
      }
    else
      KERR("%s %d %d %d", argv[0], dir, cmd, val);
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cmd_add_nat(int argc, char **argv)
{
  int result = -1;
  if (argc == 1)
    {
    result = 0;
    init_connection_to_uml_cloonix_switch();
    client_add_sat(0, callback_end, argv[0], endp_type_nat, NULL);
    }
  return result;
}
/*---------------------------------------------------------------------------*/



/*****************************************************************************/
int cmd_del_sat(int argc, char **argv)
{
  int result = -1;
  char *name;
  if (argc == 1)
    {
    name =  argv[0];
    result = 0;
    init_connection_to_uml_cloonix_switch();
    client_del_sat(0, callback_end, name);
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cmd_add_d2d(int argc, char **argv)
{
  int result = -1;
  t_cloonix_conf_info *cnf, *local_cnf;
  char *d2d_name;
  char *slave_cloonix;
  if (argc == 2)
    {
    d2d_name =  argv[0];
    cnf = cloonix_conf_info_get(argv[1]);
    if (!cnf)
      printf("\nd2d dest names: %s\n\n", cloonix_conf_info_get_names());
    else
      {
      result = 0;
      slave_cloonix = argv[1];
      printf("\nd2d is at: %s\n\n", cnf->doors);
      init_connection_to_uml_cloonix_switch();
      local_cnf = get_own_cloonix_conf_info();
      client_add_d2d(0, callback_end, d2d_name, local_cnf->d2d_udp_ip,
                     slave_cloonix, cnf->ip, (cnf->port & 0xFFFF),
                     cnf->passwd, cnf->d2d_udp_ip);
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cmd_add_c2c(int argc, char **argv)
{
  int result = -1;
  t_cloonix_conf_info *cnf;
  char *c2c;
  t_c2c_req_info c2c_req_info;
  memset(&c2c_req_info, 0, sizeof(t_c2c_req_info));
  if (argc == 2)
    {
    c2c =  argv[0];
    cnf = cloonix_conf_info_get(argv[1]);
    if (!cnf)
      printf("\nc2c dest names: %s\n\n", cloonix_conf_info_get_names());
    else
      {
      result = 0;
      strncpy(c2c_req_info.cloonix_slave, argv[1], MAX_NAME_LEN-1);
      c2c_req_info.ip_slave = cnf->ip;
      c2c_req_info.port_slave = cnf->port;
      strncpy(c2c_req_info.passwd_slave, cnf->passwd, MSG_DIGEST_LEN-1);
      printf("\nc2c is at: %s\n\n", cnf->doors);
      init_connection_to_uml_cloonix_switch();
      client_add_sat(0, callback_end, c2c, endp_type_c2c, &c2c_req_info);
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cmd_del_c2c(int argc, char **argv)
{
  int result = -1;
  char *c2c;
  if (argc == 1)
    {
    c2c =  argv[0];
    result = 0;
    init_connection_to_uml_cloonix_switch();
    client_del_sat(0, callback_end, c2c);
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cmd_add_vl2sat(int argc, char **argv)
{
  int num, result = -1;
  char *name, *lan;
  if (argc == 3)
    {
    name = argv[0];
    num = param_tester(argv[1], 0, MAX_SOCK_VM);
    if (num != -1)
      {
      result = 0;
      lan = argv[2];
      init_connection_to_uml_cloonix_switch();
      client_add_lan_endp(0, callback_end, name, num, lan);
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cmd_del_vl2sat(int argc, char **argv)
{
  int num, result = -1;
  char *name, *lan;
  if (argc == 3)
    {
    name = argv[0];
    num = param_tester(argv[1], 0, MAX_SOCK_VM);
    if (num != -1)
      {
      result = 0;
      lan = argv[2];
      init_connection_to_uml_cloonix_switch();
      client_del_lan_endp(0, callback_end, name, num, lan);
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cmd_dpdk_ovs_cnf(int argc, char **argv)
{
  int lcore, mem, cpu, result = -1;
  if (argc == 3)
    {
    if (sscanf(argv[0], "0x%X", &lcore) != 1)
      {
      if (sscanf(argv[0], "0x%x", &lcore) != 1)
        {
        printf("Bad param: %s\n", argv[0]); 
        lcore = -1;
        }
      }
    if (sscanf(argv[1], "%d", &mem) != 1) 
      {
      printf("Bad param: %s\n", argv[1]); 
      mem = -1;
      }
    if (sscanf(argv[2], "0x%X", &cpu) != 1)
      {
      if (sscanf(argv[2], "0x%x", &cpu) != 1)
        {
        printf("Bad param: %s\n", argv[2]);
        cpu = -1;
        }
      }
    if ((lcore != -1) && (mem != -1) && (cpu != -1))
      {
      result = 0;
      init_connection_to_uml_cloonix_switch();
      client_dpdk_ovs_cnf(0, callback_end, lcore, mem, cpu);
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cmd_event_print(int argc, char **argv)
{
    init_connection_to_uml_cloonix_switch();
  client_print_sub(0, callback_print);
  return 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cmd_event_sys(int argc, char **argv)
{
    init_connection_to_uml_cloonix_switch();
  client_sys_sub(0, callback_sys);
  return 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void get_proper_bandwidth(int band, char *str)
{
  memset(str, 0, MAX_NAME_LEN);
  if (band > 1048576)
    snprintf(str, MAX_NAME_LEN-1, "%dMB", band/1048576);
  else if (band > 1024)
    snprintf(str, MAX_NAME_LEN-1, "%dKB", band/1024);
  else
    snprintf(str, MAX_NAME_LEN-1, "%dB", band);


}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void blkd_item_print(t_blkd_item *it)
{
  char bd_tx[MAX_NAME_LEN];
  char bd_rx[MAX_NAME_LEN];
  get_proper_bandwidth(it->bandwidth_tx, bd_tx);
  get_proper_bandwidth(it->bandwidth_rx, bd_rx);
  printf("llid:%d sock:%s name:%s num:%d rank:%d tidx:%d pid:%d\n", 
          it->llid, it->sock, it->rank_name, it->rank_num, it->rank_tidx, 
          it->rank, it->pid);
  printf("llid:%d TX flow_ctl:%d stop:%d select:%d fifo:%d queue:%d drop:%lld bw:%s\n",
                                               it->llid, 
                                               it->dist_flow_ctrl_tx,
                                               it->stop_tx,
                                               it->sel_tx, it->fifo_tx, 
                                               it->queue_tx, 
                                               it->drop_tx, bd_tx);
  printf("llid:%d RX flow_ctl:%d stop:%d select:%d fifo:%d queue:%d drop:%lld bw:%s\n",
                                               it->llid, 
                                               it->dist_flow_ctrl_rx,
                                               it->stop_rx,
                                               it->sel_rx, it->fifo_rx,
                                               it->queue_rx, 
                                               it->drop_rx, bd_rx);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void blkd_reports_cb(int tid, t_blkd_reports *blkd)
{
  int i;
  t_blkd_item *it;
  printf("\n\n**********************************************\n");
  for (i=0; i<blkd->nb_blkd_reports; i++)
    {
    it = &(blkd->blkd_item[i]);
    blkd_item_print(it);
    printf("----------------------------------------------\n");
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cmd_event_blkd(int argc, char **argv)
{
    init_connection_to_uml_cloonix_switch();
  client_blkd_reports_sub(0, 1, blkd_reports_cb);
  return 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void hop_event_cb(int tid, char *name, char *txt)
{
  int len;
  char empty[MAX_NAME_LEN];
  memset(empty, ' ', MAX_NAME_LEN);
  len = 15 - strlen(name);
  if (len > 0)
    {
    empty[len] = 0;
    strcat(name, empty);
    }
  printf("%s: %s\n", name, txt);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void hop_callback_end(int nb, t_hop_list *list)
{
  int result = -1;
  int flags_hop = 0xFFFF;
  if (g_hop_list_nb_item)
    result = 0;
  if (!strncmp(g_hop_list[0].name, "hop_doors", strlen("hop_doors")))
    flags_hop = 8;
  else if (!strncmp(g_hop_list[0].name, "hop_app", strlen("hop_app")))
    flags_hop = 4;
  else if (!strncmp(g_hop_list[0].name, "hop_diag", strlen("hop_diag")))
    flags_hop = 2;
  else if (!strncmp(g_hop_list[0].name, "hop_evt", strlen("hop_evt")))
    flags_hop = 1;
  else if (!strncmp(g_hop_list[0].name, "hop_all", strlen("hop_all")))
    flags_hop = 0xFFFF;
  else
    result = -1;
  if (result == 0)
    {
    client_set_hop_event(hop_event_cb);
    client_get_hop_event(0, flags_hop, g_hop_list_nb_item, g_hop_list);
    }
  g_hop_list_nb_item = 0;
  free(g_hop_list);
  if (result)
    {
    printf("\n\t\thop_doors\n");
    printf("\t\thop_app\n");
    printf("\t\thop_diag\n");
    printf("\t\thop_evt\n");
    printf("\t\thop_all\n");
    printf("\n\n");
    exit(0);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cmd_event_hop(int argc, char **argv)
{
  int i, result = 0;
  char *ptr;
  g_hop_list_nb_item = argc;
  g_hop_list = (t_hop_list *) malloc(sizeof(t_hop_list) * g_hop_list_nb_item);
  memset(g_hop_list, 0, sizeof(t_hop_list) * g_hop_list_nb_item);
  for (i=0; i<g_hop_list_nb_item; i++)
    {
    ptr = strchr(argv[i], ',');
    if (ptr)
      {
      *ptr = 0;
      sscanf(ptr+1, "%d", &(g_hop_list[i].num)); 
      }
    strncpy(g_hop_list[i].name, argv[i], MAX_NAME_LEN-1);
    }
  init_connection_to_uml_cloonix_switch();
  client_set_hop_name_list(hop_callback_end);
  client_get_hop_name_list(0);
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void mud_cli_dialog_cb(int tid, char *name, int eth,
                              char *line, int status)
{
  if (status)
    {
    printf("RESPKO DEST NOT FOUND: %s %d %s\n\n", name, eth, line);
    exit(1);
    }
  else
    {
    printf("%s\n\n", line);
    exit(0);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cmd_snf_on(int argc, char **argv)
{
  int result = -1;
  if (argc == 1)
    {
    result = 0;
    init_connection_to_uml_cloonix_switch();
    set_mud_cli_dialog_callback(mud_cli_dialog_cb);
    client_mud_cli_cmd(0, argv[0], 0, "-rec_start");
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cmd_snf_off(int argc, char **argv)
{
  int result = -1;
  if (argc == 1)
    {
    result = 0;
    init_connection_to_uml_cloonix_switch();
    set_mud_cli_dialog_callback(mud_cli_dialog_cb);
    client_mud_cli_cmd(0, argv[0], 0, "-rec_stop");
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cmd_snf_get_file(int argc, char **argv)
{
  int result = -1;
  if (argc == 1)
    {
    result = 0;
    init_connection_to_uml_cloonix_switch();
    set_mud_cli_dialog_callback(mud_cli_dialog_cb);
    client_mud_cli_cmd(0, argv[0], 0, "-get_conf");
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cmd_snf_set_file(int argc, char **argv)
{
  int result = -1;
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  if (argc == 2)
    {
    result = 0;
    snprintf(cmd, MAX_PATH_LEN-1, "-set_conf %s", argv[1]);
    init_connection_to_uml_cloonix_switch();
    set_mud_cli_dialog_callback(mud_cli_dialog_cb);
    client_mud_cli_cmd(0, argv[0], 0, cmd);
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cmd_mud_lan(int argc, char **argv)
{
  int result = -1;
  if (argc == 2)
    {
    result = 0;
    init_connection_to_uml_cloonix_switch();
    set_mud_cli_dialog_callback(mud_cli_dialog_cb);
    client_mud_cli_cmd(0, argv[0], 0, argv[1]);
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cmd_mud_sat(int argc, char **argv)
{
  int result = -1;
  if (argc == 2)
    {
    result = 0;
    init_connection_to_uml_cloonix_switch();
    set_mud_cli_dialog_callback(mud_cli_dialog_cb);
    client_mud_cli_cmd(0, argv[0], 0, argv[1]);
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cmd_mud_eth(int argc, char **argv)
{
  int eth, result = -1;
  if (argc == 3)
    {
    eth = param_tester(argv[1], 0, MAX_SOCK_VM);
    if (eth != -1)
      {
      result = 0;
      init_connection_to_uml_cloonix_switch();
      set_mud_cli_dialog_callback(mud_cli_dialog_cb);
      client_mud_cli_cmd(0, argv[0], eth, argv[2]);
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void print_stats_counts(t_stats_counts *stats_counts)
{
  int i;

  if (stats_counts->nb_items)
    {
    printf("%d\n", stats_counts->nb_items);
    for (i=0; i<stats_counts->nb_items; i++)
      {
      printf("tx: ms:%d ptx:%d btx:%d prx:%d brx:%d\n",
                            stats_counts->item[i].time_ms,
                            stats_counts->item[i].ptx,
                            stats_counts->item[i].btx,
                            stats_counts->item[i].prx,
                            stats_counts->item[i].brx);
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void stats_endp_cb(int tid, char *name, int num, 
                         t_stats_counts *stats_counts, int status)
{
  if (status)
    {
    printf("RESPKO\n");
    exit(1); 
    }
  printf("\n%s %d\n", name, num);
  print_stats_counts(stats_counts);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void stats_sysinfo_cb(int tid, char *name,
                             t_stats_sysinfo *si, 
                             char *df, int status)
{
  if (status)
    {
    printf("RESPKO\n");
    exit(1);
    }
  printf("\n%s %d\n", name, si->time_ms);
  printf("    uptime:     %lu\n", si->uptime);
  printf("    load1:      %lu\n", si->load1);
  printf("    load5:      %lu\n", si->load5);
  printf("    load15:     %lu\n", si->load15);
  printf("    totalram:   %lu\n", si->totalram);
  printf("    freeram:    %lu\n", si->freeram);
  printf("    cachedram:  %lu\n", si->cachedram);
  printf("    sharedram:  %lu\n", si->sharedram);
  printf("    bufferram:  %lu\n", si->bufferram);
  printf("    totalswap:  %lu\n", si->totalswap);
  printf("    freeswap:   %lu\n", si->freeswap);
  printf("    procs:      %lu\n", si->procs);
  printf("    totalhigh   %lu\n", si->totalhigh);
  printf("    freehigh    %lu\n", si->freehigh);
  printf("    mem_unit    %lu\n", si->mem_unit);
  printf("    process_utime  %lu\n", si->process_utime);
  printf("    process_stime  %lu\n", si->process_stime);
  printf("    process_cutime %lu\n", si->process_cutime);
  printf("    process_cstime %lu\n", si->process_cstime);
  printf("    process_rss    %lu\n", si->process_rss);
  printf("\n%s\n", df);
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
int cmd_sub_sysinfo(int argc, char **argv)
{
  int result = -1;
  if (argc == 1)
    {
    result = 0;
    init_connection_to_uml_cloonix_switch();
    client_evt_stats_sysinfo_sub(0, argv[0], 1, stats_sysinfo_cb);
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cmd_sub_endp(int argc, char **argv)
{
  int num, result = -1;
  if (argc == 2)
    {
    num = param_tester(argv[1], 0, MAX_SOCK_VM);
    if (num != -1)
      {
      result = 0;
      init_connection_to_uml_cloonix_switch();
      client_evt_stats_endp_sub(0, argv[0], num, 1, stats_endp_cb);
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cmd_hwsim_config(int argc, char **argv)
{
  int num, result = -1;
  if (argc == 3)
    {
    num = param_tester(argv[1], 0, MAX_SOCK_VM);
    if (num != -1)
      {
      result = 0;
      init_connection_to_uml_cloonix_switch();
      set_mud_cli_dialog_callback(mud_cli_dialog_cb);
      client_mud_cli_cmd(0, argv[0], num, argv[2]);
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cmd_hwsim_dump(int argc, char **argv)
{
  int num, result = -1;
  if (argc == 2)
    {
    num = param_tester(argv[1], 0, MAX_SOCK_VM);
    if (num != -1)
      {
      result = 0;
      init_connection_to_uml_cloonix_switch();
      set_mud_cli_dialog_callback(mud_cli_dialog_cb);
      client_mud_cli_cmd(0, argv[0], num, "dump_config");
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
static void qmp_cb(int tid, char *name, char *line, int status)
{
  printf("\n%s", line);
  if (status)
    {
    printf("\n");
    exit(1);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void qmp_sub_cb(int tid, char *name, char *line, int status)
{
  qmp_cb(tid, name, line, status);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void qmp_nosub_cb(int tid, char *name, char *line, int status)
{
  qmp_cb(tid, name, line, status);
  exit(0);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cmd_sub_qmp(int argc, char **argv)
{
  int result = -1;
  if ((argc == 0) || (argc == 1))
    {
    result = 0;
    init_connection_to_uml_cloonix_switch();
    set_qmp_callback(qmp_sub_cb);
    if (argc == 1)
      client_qmp_sub(0, argv[0]);
    else
      client_qmp_sub(0, NULL);
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cmd_snd_qmp(int argc, char **argv)
{
  int result = -1;
  if (argc == 2)
    {
    result = 0;
    init_connection_to_uml_cloonix_switch();
    set_qmp_callback(qmp_nosub_cb);
    client_qmp_snd(0, argv[0], argv[1]);
    }
  return result;
}
/*---------------------------------------------------------------------------*/
