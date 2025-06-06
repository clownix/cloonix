/*****************************************************************************/
/*    Copyright (C) 2006-2025 clownix@clownix.net License AGPL-3             */
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
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include "io_clownix.h"
#include "lib_topo.h"
#include "xml_interface.h"
#include "rpc_clownix.h"
#include "header_sock.h"

/*---------------------------------------------------------------------------*/
static char *sndbuf = NULL;

/*---------------------------------------------------------------------------*/


enum 
  {
  bnd_min = 0,

  bnd_snf_add,

  bnd_c2c_add,
  bnd_c2c_peer_create,
  bnd_c2c_peer_conf,
  bnd_c2c_peer_ping,

  bnd_color_item,
  bnd_novnc_on_off,

  bnd_hop_get_list,
  bnd_hop_list,
  bnd_hop_evt_doors_sub,
  bnd_hop_evt_doors_unsub,
  bnd_hop_evt_doors,

  bnd_status_ok,
  bnd_status_ko,
  bnd_fix_display,
  bnd_add_vm,
  bnd_sav_vm,
  bnd_del_name,
  bnd_add_lan_endp,
  bnd_del_lan_endp,
  bnd_kill_uml_clownix,
  bnd_del_all,
  bnd_list_pid_req,
  bnd_list_pid_resp,
  bnd_list_commands_req,
  bnd_list_commands_resp,
  bnd_topo_small_event_sub,
  bnd_topo_small_event_unsub,
  bnd_topo_small_event,
  bnd_event_topo_sub,
  bnd_event_topo_unsub,
  bnd_event_topo,
  bnd_evt_print_sub,
  bnd_evt_print_unsub,
  bnd_event_print,
  bnd_event_sys_sub,
  bnd_event_sys_unsub,
  bnd_event_sys,
  bnd_work_dir_req,
  bnd_work_dir_resp,
  bnd_vmcmd,
  bnd_eventfull_sub,
  bnd_eventfull,
  bnd_slowperiodic_sub,
  bnd_slowperiodic_qcow2,
  bnd_slowperiodic_img,
  bnd_slowperiodic_cvm,
  bnd_sub_evt_stats_endp,
  bnd_evt_stats_endp,
  bnd_sub_evt_stats_sysinfo,
  bnd_evt_stats_sysinfo,

  bnd_nat_add,
  bnd_phy_add,
  bnd_cnt_add,
  bnd_tap_add,
  bnd_a2b_add,
  bnd_a2b_cnf,
  bnd_lan_cnf,
  bnd_xyx_cnf,
  bnd_nat_cnf,

  bnd_sync_wireshark_req,
  bnd_sync_wireshark_resp,

  bnd_max,
  };
static char bound_list[bnd_max][MAX_CLOWNIX_BOUND_LEN];
/*---------------------------------------------------------------------------*/
static t_llid_tx g_llid_tx;
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void my_msg_mngt_tx(int llid, int len, char *buf)
{
  if (len > MAX_SIZE_BIGGEST_MSG - 1000)
    KOUT("%d %d", len, MAX_SIZE_BIGGEST_MSG);
  if (len > MAX_SIZE_BIGGEST_MSG/2)
    KERR("WARNING LEN MSG %d %d", len, MAX_SIZE_BIGGEST_MSG);
  if (!g_llid_tx)
    KOUT(" ");
  buf[len] = 0;
  g_llid_tx(llid, len + 1, buf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void extract_boundary(char *input, char *output)
{
  int bound_len;
  if (input[0] != '<')
    KOUT("%s\n", input);
  bound_len = strcspn(input, ">");
  if (bound_len >= MAX_CLOWNIX_BOUND_LEN)
    KOUT("%s\n", input);
  if (bound_len < MIN_CLOWNIX_BOUND_LEN)
    KOUT("%s\n", input);
  memcpy(output, input, bound_len);
  output[bound_len] = 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int get_bnd_event(char *bound)
{
  int i, result = 0;
  for (i=bnd_min; i<bnd_max; i++) 
    if (!strcmp(bound, bound_list[i]))
      {
      result = i;
      break;
      }
  return result;
}
/*---------------------------------------------------------------------------*/
 
/*****************************************************************************/
static char *string_to_xml(char *info)
{
  static char buf[MAX_PRINT_LEN];
  char *ptr = buf;
  memset(buf, 0, MAX_PRINT_LEN);
  strncpy(buf, info, MAX_PRINT_LEN);
  buf[MAX_PRINT_LEN-1] = 0;
  if (strlen(buf) == 0)
    sprintf(buf, " ");
  while(ptr)
    {
    ptr = strchr(ptr, ' ');
    if (ptr)
      *ptr = '%';
    }
  ptr = buf;
  while(ptr)
    {
    ptr = strchr(ptr, '\n');
    if (ptr)
      *ptr = '?';
    }
  return buf;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static char *xml_to_string(char *info)
{
  static char buf[MAX_PRINT_LEN];
  char *ptr = buf;
  strncpy(buf, info, MAX_PRINT_LEN);
  buf[MAX_PRINT_LEN-1] = 0;
  while(ptr)
    {
    ptr = strchr(ptr, '%');
    if (ptr)
      *ptr = ' ';
    }
  ptr = buf;
  while(ptr)
    {
   ptr = strchr(ptr, '?');
    if (ptr)
      *ptr = '\n';
    }
  return buf;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void sys_info_free(t_sys_info *sys)
{
  if (sys->queue_tx)   
    clownix_free(sys->queue_tx, __FUNCTION__);
  clownix_free(sys, __FUNCTION__);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int fill_stats(char *buf, int nb, t_stats_count_item *item)
{
  int i, len = 0;
  for (i=0; i<nb; i++)
    {  
    len += sprintf(buf+len, EVT_STATS_ITEM, item[i].time_ms,
                                            item[i].ptx, item[i].btx,
                                            item[i].prx, item[i].brx);
    }
  return len;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void helper_stats(char *msg, int nb, t_stats_count_item *item)
{
  char *ptr;
  int i;
  memset(item, 0, MAX_STATS_ITEMS*sizeof(t_stats_count_item));
  if ((nb < 0) || (nb > MAX_STATS_ITEMS))
    KOUT("%d", nb);
  ptr = msg;
  for (i=0; i<nb; i++)
    {
    ptr = strstr(ptr, "<item>");
    if (!ptr)
      KOUT("%s", msg);
    if (sscanf(ptr, EVT_STATS_ITEM, &(item[i].time_ms), 
                                    &(item[i].ptx), 
                                    &(item[i].btx),
                                    &(item[i].prx), 
                                    &(item[i].brx)) != 5)
      KOUT("%s", msg);
    ptr = strstr(ptr, "</item>");
    if (!ptr)
      KOUT("%s", msg);
    }
  ptr = strstr(ptr, "<item>");
  if (ptr)
    KOUT("%s", msg);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void topo_config_check_str(t_topo_clc *cf, int line)
{
  if (strlen(cf->version)  >= MAX_NAME_LEN)
    KOUT("%d", line);
  if (strlen(cf->network)  >= MAX_NAME_LEN)
    KOUT("%d", line);
  if (strlen(cf->username) >= MAX_NAME_LEN)
    KOUT("%d", line);
  if (strlen(cf->bin_dir)  >= MAX_PATH_LEN)
    KOUT("%d", line);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void topo_config_swapon(t_topo_clc *cf, t_topo_clc *icf)
{
  memcpy(cf, icf, sizeof(t_topo_clc));
  if (strlen(cf->version) == 0)
    strcpy(cf->version, NO_DEFINED_VALUE);
  if (strlen(cf->network) == 0)
    strcpy(cf->network, NO_DEFINED_VALUE);
  if (strlen(cf->username) == 0)
    strcpy(cf->username, NO_DEFINED_VALUE);
  if (strlen(cf->bin_dir) == 0)
    strcpy(cf->bin_dir, NO_DEFINED_VALUE);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void topo_config_swapoff(t_topo_clc  *cf, t_topo_clc *icf)
{
  memcpy(cf, icf, sizeof(t_topo_clc));
  if (!strcmp(cf->version, NO_DEFINED_VALUE))
    memset(cf->version, 0, MAX_NAME_LEN);
  if (!strcmp(cf->network, NO_DEFINED_VALUE))
    memset(cf->network, 0, MAX_NAME_LEN);
  if (!strcmp(cf->username, NO_DEFINED_VALUE))
    memset(cf->username, 0, MAX_NAME_LEN);
  if (!strcmp(cf->bin_dir, NO_DEFINED_VALUE))
    memset(cf->bin_dir, 0, MAX_NAME_LEN);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void topo_kvm_check_str(t_topo_kvm *kvm, int line)
{
  if (strlen(kvm->name) >= MAX_NAME_LEN)
    KOUT("%d", line);
  if (strlen(kvm->linux_kernel) >= MAX_NAME_LEN)
    KOUT("%d", line);
  if (strlen(kvm->rootfs_input) >= MAX_PATH_LEN)
    KOUT("%d", line);
  if (strlen(kvm->rootfs_used) >= MAX_PATH_LEN)
    KOUT("%d", line);
  if (strlen(kvm->rootfs_backing) >= MAX_PATH_LEN)
    KOUT("%d", line);
  if (strlen(kvm->install_cdrom) >= MAX_PATH_LEN)
    KOUT("%d", line);
  if (strlen(kvm->added_cdrom) >= MAX_PATH_LEN)
    KOUT("%d", line);
  if (strlen(kvm->added_disk) >= MAX_PATH_LEN)
    KOUT("%d", line);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void topo_kvm_swapon(t_topo_kvm *kvm, t_topo_kvm *ikvm)
{
  memcpy(kvm, ikvm, sizeof(t_topo_kvm));
  if (strlen(kvm->name) == 0)
    strcpy(kvm->name, NO_DEFINED_VALUE);
  if (strlen(kvm->linux_kernel) == 0)
    strcpy(kvm->linux_kernel, NO_DEFINED_VALUE);
  if (strlen(kvm->rootfs_input) == 0)
    strcpy(kvm->rootfs_input, NO_DEFINED_VALUE);
  if (strlen(kvm->rootfs_used) == 0)
    strcpy(kvm->rootfs_used, NO_DEFINED_VALUE);
  if (strlen(kvm->rootfs_backing) == 0)
    strcpy(kvm->rootfs_backing, NO_DEFINED_VALUE);
  if (strlen(kvm->install_cdrom) == 0)
    strcpy(kvm->install_cdrom, NO_DEFINED_VALUE);
  if (strlen(kvm->added_cdrom) == 0)
    strcpy(kvm->added_cdrom, NO_DEFINED_VALUE);
  if (strlen(kvm->added_disk) == 0)
    strcpy(kvm->added_disk, NO_DEFINED_VALUE);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void topo_kvm_swapoff(t_topo_kvm *kvm, t_topo_kvm *ikvm)
{
  memcpy(kvm, ikvm, sizeof(t_topo_kvm));
  if (!strcmp(kvm->name, NO_DEFINED_VALUE))
    memset(kvm->name, 0, MAX_NAME_LEN);
  if (!strcmp(kvm->linux_kernel, NO_DEFINED_VALUE))
    memset(kvm->linux_kernel, 0, MAX_NAME_LEN);
  if (!strcmp(kvm->rootfs_input, NO_DEFINED_VALUE))
    memset(kvm->rootfs_input, 0, MAX_NAME_LEN);
  if (!strcmp(kvm->rootfs_used, NO_DEFINED_VALUE))
    memset(kvm->rootfs_used, 0, MAX_NAME_LEN);
  if (!strcmp(kvm->rootfs_backing, NO_DEFINED_VALUE))
    memset(kvm->rootfs_backing, 0, MAX_NAME_LEN);
  if (!strcmp(kvm->install_cdrom, NO_DEFINED_VALUE))
    memset(kvm->install_cdrom, 0, MAX_NAME_LEN);
  if (!strcmp(kvm->added_cdrom, NO_DEFINED_VALUE))
    memset(kvm->added_cdrom, 0, MAX_NAME_LEN);
  if (!strcmp(kvm->added_disk, NO_DEFINED_VALUE))
    memset(kvm->added_disk, 0, MAX_NAME_LEN);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_hop_evt_doors_sub(int llid, int tid, int flags_hop,
                            int nb, t_hop_list *list)
{
  int i, len;
  len = sprintf(sndbuf, HOP_EVT_DOORS_SUB_O, tid, flags_hop, nb);
  for (i=0; i<nb; i++)
    {
    if ((strlen(list[i].name) == 0) ||
        (strlen(list[i].name) >= MAX_NAME_LEN))
      KOUT(" ");
    len += sprintf(sndbuf+len, HOP_LIST_NAME_I, list[i].type_hop,
                                                list[i].name, list[i].num);
    }
  len += sprintf(sndbuf+len, HOP_EVT_DOORS_SUB_C);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_hop_evt_doors_unsub(int llid, int tid)
{
  int len;
  len = sprintf(sndbuf, HOP_EVT_DOORS_UNSUB, tid);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_hop_evt_doors(int llid, int tid, int type_evt_sub,
                        char *name, char *txt)
{
  int len;
  if ((!name) || (strlen(name)==0) || (strlen(name) >= MAX_NAME_LEN))
    KOUT(" ");
  if ((!txt) || (strlen(txt)==0) || (strlen(txt) >= MAX_RPC_MSG_LEN))
    KOUT(" ");
  len = sprintf(sndbuf, HOP_EVT_DOORS_O, tid, type_evt_sub, name);
  len += sprintf(sndbuf+len, HOP_FREE_TXT, txt);
  len += sprintf(sndbuf+len, HOP_EVT_DOORS_C);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_hop_get_name_list_doors(int llid, int tid)
{
  int len;
  len = sprintf(sndbuf, HOP_GET_LIST_NAME, tid);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_hop_name_list_doors(int llid, int tid, int nb, t_hop_list *list)
{
  int i, len;
  len = sprintf(sndbuf, HOP_LIST_NAME_O, tid, nb);
  for (i=0; i<nb; i++)
    {
    if ((strlen(list[i].name) == 0) ||
        (strlen(list[i].name) >= MAX_NAME_LEN))
      KOUT(" ");
    len += sprintf(sndbuf+len, HOP_LIST_NAME_I, list[i].type_hop,
                                                list[i].name, list[i].num);
    }
  len += sprintf(sndbuf+len, HOP_LIST_NAME_C);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static t_hop_list *helper_rx_hop_name_list(char *msg, int nb)
{
  int i;
  char *ptr = msg;
  t_hop_list *list = (t_hop_list *) clownix_malloc(nb * sizeof(t_hop_list), 7);
  memset(list, 0, nb * sizeof(t_hop_list));
  for (i=0; i<nb; i++)
    {
    ptr = strstr(ptr, "<hop_type_item>");
    if (!ptr)
      KOUT("%s", msg);
    if (sscanf(ptr, HOP_LIST_NAME_I, &(list[i].type_hop),
                                     list[i].name, &(list[i].num))!= 3)
      KOUT("%s", ptr);
    ptr = strstr(ptr, "</hop_type_item>");
    if (!ptr)
      KOUT("%s", msg);
    }
  ptr = strstr(ptr, "<hop_type_item>");
  if (ptr)
    KOUT("%s", msg);
  return list;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_evt_stats_endp_sub(int llid, int tid, char *name, int num, int sub)
{
  int len;
  if (!name)
    KOUT(" ");
  if (strlen(name) < 1)
    KOUT(" ");
  if (strlen(name) >= MAX_NAME_LEN)
    name[MAX_NAME_LEN-1] = 0;
  len = sprintf(sndbuf, SUB_EVT_STATS_ENDP, tid, name, num, sub);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_evt_stats_endp(int llid, int tid, char *network, 
                         char *name, int num, 
                         t_stats_counts *sc, int status)
{
  int len;
  if (!sc)
    KOUT(" ");
  if (!network)
    KOUT(" ");
  if (strlen(network) < 1)
    KOUT(" ");
  if (strlen(network) >= MAX_NAME_LEN)
    network[MAX_NAME_LEN-1] = 0;
  if (!name)
    KOUT(" ");
  if (strlen(name) < 1)
    KOUT(" ");
  if (strlen(name) >= MAX_NAME_LEN)
    name[MAX_NAME_LEN-1] = 0;
  if ((sc->nb_items < 0) || (sc->nb_items > MAX_STATS_ITEMS)) 
    KOUT("%d ", sc->nb_items);
  len = sprintf(sndbuf, EVT_STATS_ENDP_O, tid, network, name, num, status, 
                                         sc->nb_items);
  len += fill_stats(sndbuf+len, sc->nb_items, sc->item);
  len += sprintf(sndbuf+len, EVT_STATS_ENDP_C);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_evt_stats_sysinfo_sub(int llid, int tid, char *name, int sub)
{
  int len;
  if (!name)
    KOUT(" ");
  if (strlen(name) < 1)
    KOUT(" ");
  if (strlen(name) >= MAX_NAME_LEN)
    name[MAX_NAME_LEN-1] = 0;
  len = sprintf(sndbuf, SUB_EVT_STATS_SYSINFO, tid, name, sub);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_evt_stats_sysinfo(int llid, int tid, char *network, char *name,
                            t_stats_sysinfo *si, char *df, int status)
{
  int len;
  if (!si)
    KOUT(" ");
  if (!network)
    KOUT(" ");
  if (strlen(network) < 1)
    KOUT(" ");
  if (strlen(network) >= MAX_NAME_LEN)
    network[MAX_NAME_LEN-1] = 0;
  if (!name)
    KOUT(" ");
  if (strlen(name) < 1)
    KOUT(" ");
  if (strlen(name) >= MAX_NAME_LEN)
    name[MAX_NAME_LEN-1] = 0;
  if (df)
    {
    if (strlen(df) >= MAX_RPC_MSG_LEN)
      df[MAX_RPC_MSG_LEN-1] = 0;
    }
  len = sprintf(sndbuf, EVT_STATS_SYSINFOO, tid, network, name, status,
                                       si->time_ms,       si->uptime,
                                       si->load1,         si->load5,
                                       si->load15,        si->totalram,     
                                       si->freeram,       si->cachedram,
                                       si->sharedram,     si->bufferram,
                                       si->totalswap,     si->freeswap,
                                       si->procs,         si->totalhigh,
                                       si->freehigh,      si->mem_unit,
                                       si->process_utime, si->process_stime,    
                                       si->process_cutime,si->process_cstime,
                                       si->process_rss);
  if (df && (strlen(df) > 0))
    len += sprintf(sndbuf+len, "<bound_for_df_dumprd>%s</bound_for_df_dumprd>", df);
  else
    len += sprintf(sndbuf+len, "<bound_for_df_dumprd></bound_for_df_dumprd>");
  len += sprintf(sndbuf+len, EVT_STATS_SYSINFOC);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_work_dir_req(int llid, int tid)
{
  int len = 0;
  len = sprintf(sndbuf, WORK_DIR_REQ, tid);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_work_dir_resp(int llid, int tid, t_topo_clc *icf)
{
  t_topo_clc cf;
  int len = 0;
  topo_config_check_str(icf, __LINE__);
  topo_config_swapon(&cf, icf);
  len = sprintf(sndbuf, WORK_DIR_RESP, tid, 
                                       cf.version,
                                       cf.network,
                                       cf.username,
                                       cf.server_port,
                                       cf.bin_dir,
                                       cf.flags_config);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_sync_wireshark_req(int llid, int tid, char *name, int num, int cmd)
{ 
  int len = 0;
  len = sprintf(sndbuf, SYNC_WIRESHARK_REQ, tid, name, num, cmd);
  my_msg_mngt_tx(llid, len, sndbuf);
} 
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_sync_wireshark_resp(int llid,int tid,char *name,int num,int status)
{
  int len = 0;
  len = sprintf(sndbuf, SYNC_WIRESHARK_RESP, tid, name, num, status);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*---------------------------------------------------------------------------*/
  
/*****************************************************************************/
void send_event_topo_sub(int llid, int tid)
{
  int len = 0;
  if (sndbuf == NULL) {KERR("WARNING"); return;}
  len = sprintf(sndbuf, EVENT_TOPO_SUB, tid);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_event_topo_unsub(int llid, int tid)
{
  int len = 0;
  if (sndbuf == NULL) {KERR("WARNING"); return;}
  len = sprintf(sndbuf, EVENT_TOPO_UNSUB, tid);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_topo_small_event_sub(int llid, int tid)
{
  int len = 0;
  if (sndbuf == NULL) {KERR("WARNING"); return;}
  len = sprintf(sndbuf, TOPO_SMALL_EVENT_SUB, tid);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_topo_small_event_unsub(int llid, int tid)
{
  int len = 0;
  if (sndbuf == NULL) {KERR("WARNING"); return;}
  len = sprintf(sndbuf, TOPO_SMALL_EVENT_UNSUB, tid);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_topo_small_event(int llid, int tid, char *name, 
                           char *param1, char *param2, int vm_evt)
{
  int len = 0;
  char parm1[MAX_PATH_LEN];
  char parm2[MAX_PATH_LEN];
  if (sndbuf == NULL) {KERR("WARNING"); return;}
  if (name[0] == 0)
    KOUT(" "); 
  memset(parm1, 0, MAX_PATH_LEN);
  memset(parm2, 0, MAX_PATH_LEN);
  if ((!param1) || (param1[0] == 0))
    strncpy(parm1, "undefined_param1", MAX_PATH_LEN-1);
  else
    strncpy(parm1, param1, MAX_PATH_LEN-1);
  if ((!param2) || (param2[0] == 0))
    strncpy(parm2, "undefined_param2", MAX_PATH_LEN-1);
  else
    strncpy(parm2, param2, MAX_PATH_LEN-1);
  len = sprintf(sndbuf, TOPO_SMALL_EVENT, tid, name, parm1, parm2, vm_evt);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int topo_eth_format(char *buf, int endp_type, int randmac, char *ifname,
                           unsigned char mac[MAC_ADDR_LEN])
{
  int len = 0;
  char tmp_ifname[MAX_NAME_LEN];
  memset(tmp_ifname, 0, MAX_NAME_LEN);
  if (strlen(ifname))
    strncpy(tmp_ifname, ifname, MAX_NAME_LEN-1);
  else
    strncpy(tmp_ifname, "noname", MAX_NAME_LEN-1);
  len += sprintf(buf+len, VM_ETH_TABLE, endp_type, randmac, tmp_ifname,
                     (mac[0]) & 0xFF, (mac[1]) & 0xFF, (mac[2]) & 0xFF,
                     (mac[3]) & 0xFF, (mac[4]) & 0xFF, (mac[5]) & 0xFF);
  return len;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int topo_cnt_format(char *buf, t_topo_cnt *cnt)
{
  int i, len;

  if ((strlen(cnt->name) == 0) ||
      (strlen(cnt->image) == 0) ||
      (strlen(cnt->brandtype) == 0))
    KOUT("name:%s image:%s", cnt->name, cnt->image);

  len = sprintf(buf, EVENT_TOPO_CNT_O, cnt->brandtype, cnt->name, 
                                       cnt->is_persistent,
                                       cnt->is_privileged,
                                       cnt->vm_id,
                                       cnt->color,
                                       cnt->image,  
                                       cnt->ping_ok,
                                       cnt->nb_tot_eth);

  for (i=0; i < cnt->nb_tot_eth; i++)
    len += topo_eth_format(buf+len, cnt->eth_table[i].endp_type, 
                                    cnt->eth_table[i].randmac,
                                    cnt->eth_table[i].vhost_ifname,
                                    cnt->eth_table[i].mac_addr);

  len += sprintf(buf+len, EVENT_TOPO_CNT_C, cnt->startup_env, cnt->vmount);
  return len;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int topo_kvm_format(char *buf, t_topo_kvm *ikvm)
{
  t_topo_kvm kvm;
  int i, len;

  topo_kvm_check_str(ikvm, __LINE__);
  topo_kvm_swapon(&kvm, ikvm);

  len = sprintf(buf, EVENT_TOPO_KVM_O, kvm.name,
                                       kvm.vm_id,
                                       kvm.color,
                                       kvm.install_cdrom,
                                       kvm.added_cdrom,
                                       kvm.added_disk,
                                       kvm.linux_kernel,
                                       kvm.rootfs_used,
                                       kvm.rootfs_backing,
                                       kvm.vm_config_flags,
                                       kvm.vm_config_param,
                                       kvm.mem,
                                       kvm.cpu,
                                       kvm.nb_tot_eth);
  for (i=0; i < kvm.nb_tot_eth; i++)
    len += topo_eth_format(buf+len, kvm.eth_table[i].endp_type,
                                    kvm.eth_table[i].randmac,
                                    kvm.eth_table[i].vhost_ifname,
                                    kvm.eth_table[i].mac_addr);
  len += sprintf(buf+len, EVENT_TOPO_KVM_C);
  return len;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int topo_c2c_format(char *buf, t_topo_c2c *c2c)
{
  int len;
  char lan[MAX_NAME_LEN];
  if (!strlen(c2c->name) || (strlen(c2c->name) >= MAX_NAME_LEN))
     KOUT(" ");
  if (!strlen(c2c->dist_cloon) || (strlen(c2c->dist_cloon) >= MAX_NAME_LEN))
     KOUT(" ");
  memset(lan, 0, MAX_NAME_LEN);
  if (strlen(c2c->attlan) == 0)
    strncpy(lan, "name_to_erase_upon_extract", MAX_NAME_LEN-1); 
  else
    strncpy(lan, c2c->attlan, MAX_NAME_LEN-1); 
  len = sprintf(buf, EVENT_TOPO_C2C, c2c->name,
                                     c2c->endp_type,
                                     c2c->dist_cloon,
                                     lan,
                                     c2c->local_is_master,
                                     c2c->dist_tcp_ip,
                                     c2c->dist_tcp_port,
                                     c2c->loc_udp_ip,
                                     c2c->dist_udp_ip,
                                     c2c->loc_udp_port,
                                     c2c->dist_udp_port,
                                     c2c->tcp_connection_peered,
                                     c2c->udp_connection_peered);
  return len;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int topo_nat_format(char *buf, t_topo_nat *nat)
{
  int len;
  if (!strlen(nat->name) || (strlen(nat->name) >= MAX_NAME_LEN))
     KOUT(" ");
  len = sprintf(buf, EVENT_TOPO_NAT, nat->name, nat->endp_type);
  return len;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int topo_tap_format(char *buf, t_topo_tap *tap)
{
  int len;
  if (!strlen(tap->name) || (strlen(tap->name) >= MAX_NAME_LEN))
     KOUT(" ");
  len = sprintf(buf, EVENT_TOPO_TAP, tap->name, tap->endp_type);
  return len;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int topo_a2b_format(char *buf, t_topo_a2b *a2b)
{
  int len;
  if (!strlen(a2b->name) || (strlen(a2b->name) >= MAX_NAME_LEN))
     KOUT(" ");
  len = sprintf(buf, EVENT_TOPO_A2B, a2b->name, a2b->endp_type0,
                                                a2b->endp_type1);
  return len;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int topo_phy_format(char *buf, t_topo_phy *phy)
{
  int len;
  if (!strlen(phy->name) || (strlen(phy->name) >= MAX_NAME_LEN))
     KOUT(" ");
  len = sprintf(buf, EVENT_TOPO_PHY, phy->name, phy->endp_type);
  return len;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int topo_info_phy_format(char *buf, t_topo_info_phy *phy)
{
  int len;
  if (strlen(phy->name) == 0)
    KOUT(" ");
  len = sprintf(buf, EVENT_TOPO_INFO_PHY, phy->name);
  return len;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int topo_bridges_format(char *buf, t_topo_bridges *bridges)
{
  int i, len;
  if ((bridges->nb_ports < 0) || (bridges->nb_ports >= MAX_OVS_PORTS))
    KOUT("%d", bridges->nb_ports);
  len = sprintf(buf, EVENT_TOPO_BRIDGES_O, bridges->br, bridges->nb_ports);
  for (i=0; i<bridges->nb_ports; i++)
    {
    if (!strlen(bridges->ports[i]))
      KOUT("%d %d", bridges->nb_ports, i);
    len += sprintf(buf+len, EVENT_TOPO_BRIDGES_I, bridges->ports[i]);
    }
  len += sprintf(buf+len, EVENT_TOPO_BRIDGES_C);
  return len;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int topo_endp_format(char *buf, t_topo_endp *endp)
{
  int i, len;
  len = sprintf(buf, EVENT_TOPO_ENDP_O, endp->name,
                                        endp->num,
                                        endp->type,
                                        endp->lan.nb_lan);
  for (i=0; i<endp->lan.nb_lan; i++)
    {
    if (!strlen(endp->lan.lan[i].lan))
      KOUT(" ");
    len += sprintf(buf+len, EVENT_TOPO_LAN, endp->lan.lan[i].lan);
    }
  len += sprintf(buf+len, EVENT_TOPO_ENDP_C);
  return len;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_event_topo(int llid, int tid, t_topo_info *topo)
{
  int i, len = 0;
  t_topo_clc cf;
  topo_config_check_str(&(topo->clc), __LINE__);
  topo_config_swapon(&cf, &(topo->clc));
  len = sprintf(sndbuf, EVENT_TOPO_O, tid, cf.version,
                                           topo->conf_rank,
                                           cf.network,
                                           cf.username,
                                           cf.server_port,
                                           cf.bin_dir,
                                           cf.flags_config,
                                           topo->nb_cnt,
                                           topo->nb_kvm,
                                           topo->nb_c2c,
                                           topo->nb_tap,
                                           topo->nb_phy,
                                           topo->nb_a2b,
                                           topo->nb_nat,
                                           topo->nb_endp,
                                           topo->nb_info_phy,
                                           topo->nb_bridges);
  for (i=0; i<topo->nb_cnt; i++)
    len += topo_cnt_format(sndbuf+len, &(topo->cnt[i]));
  for (i=0; i<topo->nb_kvm; i++)
    len += topo_kvm_format(sndbuf+len, &(topo->kvm[i]));
  for (i=0; i<topo->nb_c2c; i++)
    len += topo_c2c_format(sndbuf+len, &(topo->c2c[i]));
  for (i=0; i<topo->nb_tap; i++)
    len += topo_tap_format(sndbuf+len, &(topo->tap[i]));
  for (i=0; i<topo->nb_phy; i++)
    len += topo_phy_format(sndbuf+len, &(topo->phy[i]));
  for (i=0; i<topo->nb_a2b; i++)
    len += topo_a2b_format(sndbuf+len, &(topo->a2b[i]));
  for (i=0; i<topo->nb_nat; i++)
    len += topo_nat_format(sndbuf+len, &(topo->nat[i]));
  for (i=0; i<topo->nb_endp; i++)
    len += topo_endp_format(sndbuf+len, &(topo->endp[i]));
  for (i=0; i<topo->nb_info_phy; i++)
    len += topo_info_phy_format(sndbuf+len, &(topo->info_phy[i]));
  for (i=0; i<topo->nb_bridges; i++)
    len += topo_bridges_format(sndbuf+len, &(topo->bridges[i]));

  len += sprintf(sndbuf+len, EVENT_TOPO_C);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_evt_print_sub(int llid, int tid)
{
  int len = 0;
  len = sprintf(sndbuf, EVENT_PRINT_SUB, tid);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_evt_print_unsub(int llid, int tid)
{
  int len = 0;
  len = sprintf(sndbuf, EVENT_PRINT_UNSUB, tid);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_evt_print(int llid, int tid, char *info)
{
  int len = 0;
  char *buf = string_to_xml(info);
  len = sprintf(sndbuf, EVENT_PRINT, tid, buf);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_event_sys_sub(int llid, int tid)
{
  int len = 0;
  len = sprintf(sndbuf, EVENT_SYS_SUB, tid);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_event_sys_unsub(int llid, int tid)
{
  int len = 0;
  len = sprintf(sndbuf, EVENT_SYS_UNSUB, tid);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_event_sys(int llid, int tid, t_sys_info *sys)
{
  int i, len, nb  = sys->nb_queue_tx;
  t_queue_tx *Qtx = sys->queue_tx;
  len = sprintf(sndbuf, EVENT_SYS_O, tid, MAX_MALLOC_TYPES);

  for (i=0; i<MAX_MALLOC_TYPES; i++)
    len += sprintf(sndbuf+len, EVENT_SYS_M, sys->mallocs[i]);

  len += sprintf(sndbuf+len, EVENT_SYS_FN, type_llid_max);
  for (i=0; i<type_llid_max; i++)
    len += sprintf(sndbuf+len, EVENT_SYS_FU, sys->fds_used[i]);

  len += sprintf(sndbuf+len, EVENT_SYS_R, sys->selects,
             sys->cur_channels,
             sys->max_channels, sys->cur_channels_recv,
             sys->cur_channels_send,  
             sys->clients, 
             sys->max_time, sys->avg_time, 
             sys->above50ms, sys->above20ms, sys->above15ms,
             sys->nb_queue_tx);

  for (i=0; i<nb; i++)
    {
    len += sprintf(sndbuf+len, EVENT_SYS_ITEM_Q,  Qtx[i].peak_size, 
                   Qtx[i].size, Qtx[i].llid, Qtx[i].fd, 
                   Qtx[i].waked_count_in, Qtx[i].waked_count_out, 
                   Qtx[i].waked_count_err,
                   Qtx[i].out_bytes, Qtx[i].in_bytes, Qtx[i].name,
                   Qtx[i].id, Qtx[i].type); 
    }
  len += sprintf(sndbuf+len, EVENT_SYS_C); 
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*---------------------------------------------------------------------------*/
                                                                        
/*****************************************************************************/
void send_status_ok(int llid, int tid, char *txt)
{
  int len = 0;
  char *buf = string_to_xml(txt);
  len = sprintf(sndbuf, STATUS_OK, tid, buf);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_status_ko(int llid, int tid, char *reason)
{
  int len = 0;
  char *buf = string_to_xml(reason);
  len = sprintf(sndbuf, STATUS_KO, tid, buf);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*---------------------------------------------------------------------------*/
  
/*****************************************************************************/
void send_fix_display(int llid, int tid, char *disp, char *line)
{ 
  int len = 0;
  char *buf = string_to_xml(line);
  len = sprintf(sndbuf, FIX_DISPLAY, tid, disp, buf);
  my_msg_mngt_tx(llid, len, sndbuf);
} 
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void get_one_eth_table(char *buf, int *endp_type, int *randmac,
                              char *ifname,  unsigned char mac[MAC_ADDR_LEN])
{
  int i;
  char *ptr;
  int var[6];
  char tmp_ifname[MAX_NAME_LEN];
  ptr = strstr(buf, "<eth_table>");
  memset(tmp_ifname, 0, MAX_NAME_LEN);
  if (!ptr)
    KOUT("%s\n", buf);
  if (sscanf(ptr, VM_ETH_TABLE, endp_type, randmac, tmp_ifname,
                               &(var[0]), &(var[1]), &(var[2]),
                               &(var[3]), &(var[4]), &(var[5])) != 9) 
      KOUT("%s\n", buf);
  for (i=0; i<6; i++)
    mac[i] = var[i] & 0xFF;
  memset(ifname, 0, MAX_NAME_LEN);
  strncpy(ifname, tmp_ifname, MAX_NAME_LEN-1);
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
static void get_eth_table(char *buf, int nb_tot_eth, t_eth_table *eth_table)
{ 
  int i;
  char *ptr = buf;
  for (i=0; i < nb_tot_eth; i++)
    {
    ptr = strstr(ptr, "<eth_table>");
    if (!ptr)
      KOUT("%s\n%d\n", buf, nb_tot_eth);
    get_one_eth_table(ptr, &(eth_table[i].endp_type), &(eth_table[i].randmac),
                      eth_table[i].vhost_ifname,
                      eth_table[i].mac_addr); 
    ptr = strstr(ptr, "</eth_table>");
    if (!ptr)
      KOUT("%s\n%d\n", buf, nb_tot_eth);
    }
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
static int make_eth_table(char *buf, int nb, t_eth_table *eth_tab)
{
  int i, endp_type, randmac, len = 0;
  char *ifname;
  unsigned char *mac;
  char tmp_ifname[MAX_NAME_LEN];
  for (i=0; i<nb; i++)
    {
    mac = eth_tab[i].mac_addr;
    randmac = eth_tab[i].randmac;
    endp_type = eth_tab[i].endp_type;
    ifname = eth_tab[i].vhost_ifname;
    memset(tmp_ifname, 0, MAX_NAME_LEN);
    if (strlen(ifname))
      strncpy(tmp_ifname, ifname, MAX_NAME_LEN-1);
    else
      strncpy(tmp_ifname, "noname", MAX_NAME_LEN-1);
    len += sprintf(buf+len, VM_ETH_TABLE, endp_type, randmac, tmp_ifname,
                      (mac[0]) & 0xFF, (mac[1]) & 0xFF, (mac[2]) & 0xFF,
                      (mac[3]) & 0xFF, (mac[4]) & 0xFF, (mac[5]) & 0xFF);
    }
  return len;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_add_vm(int llid, int tid, t_topo_kvm *ikvm) 
{
  int len = 0;
  t_topo_kvm kvm;

  topo_kvm_check_str(ikvm, __LINE__);
  topo_kvm_swapon(&kvm, ikvm); 

  len = sprintf(sndbuf, ADD_KVM_O, tid, kvm.name, kvm.vm_id, kvm.color,
                kvm.vm_config_flags, kvm.vm_config_param,
                kvm.mem, kvm.cpu, kvm.nb_tot_eth);

  len += make_eth_table(sndbuf+len, kvm.nb_tot_eth, kvm.eth_table);

  len += sprintf(sndbuf+len, ADD_KVM_C, kvm.linux_kernel, 
                             kvm.rootfs_input, kvm.install_cdrom,
                             kvm.added_cdrom, kvm.added_disk);

  my_msg_mngt_tx(llid, len, sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_sav_vm(int llid, int tid, char *name, char *sav_rootfs_path)
{
  int len = 0;
  if (name[0] == 0)
    KOUT(" ");
  if (sav_rootfs_path[0] == 0)
    KOUT(" ");
  if (strlen(name) >= MAX_NAME_LEN)
    KOUT(" ");
  if (strlen(sav_rootfs_path) >= MAX_PATH_LEN)
    KOUT(" ");
  len = sprintf(sndbuf, SAV_VM, tid, name, sav_rootfs_path);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_del_name(int llid, int tid, char *name)
{
  int len = 0;
  if (name[0] == 0)
    KOUT(" "); 
  if (strlen(name) >= MAX_NAME_LEN)
    KOUT(" ");
  len = sprintf(sndbuf, DEL_SAT, tid, name);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_add_lan_endp(int llid, int tid, char *name, int num, char *lan)
{
  int len = 0;
  if (name[0] == 0)
    KOUT(" ");
  if (strlen(name) >= MAX_NAME_LEN)
    KOUT(" ");
  if (lan[0] == 0)
    KOUT(" ");
  if (strlen(lan) >= MAX_NAME_LEN)
    KOUT(" ");
  len = sprintf(sndbuf, ADD_LAN_ENDP, tid, name, num, lan);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_del_lan_endp(int llid, int tid, char *name, int num, char *lan)
{
  int len = 0;
  if (name[0] == 0)
    KOUT(" ");
  if (strlen(name) >= MAX_NAME_LEN)
    KOUT(" ");
  if (lan[0] == 0)
    KOUT(" ");
  if (strlen(lan) >= MAX_NAME_LEN)
    KOUT(" ");
  len = sprintf(sndbuf, DEL_LAN_ENDP, tid, name, num, lan);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_kill_uml_clownix(int llid, int tid)
{
  int len = 0;
  len = sprintf(sndbuf, KILL_UML_CLOWNIX, tid);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_del_all(int llid, int tid)
{
  int len = 0;
  len = sprintf(sndbuf, DEL_ALL, tid);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_list_pid_req(int llid, int tid)
{
  int len = 0;
  len = sprintf(sndbuf, LIST_PID, tid);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_list_pid_resp(int llid, int tid, int qty,  t_pid_lst *pid_lst)
{
  int i, len = 0;
  len = sprintf(sndbuf, LIST_PID_O, tid, qty);
  for (i=0; i<qty; i++)
    {
    if (strlen(pid_lst[i].name) == 0)
      KOUT(" ");
    len += sprintf(sndbuf+len, LIST_PID_ITEM, pid_lst[i].name, pid_lst[i].pid);
    }
  len += sprintf(sndbuf+len, LIST_PID_C);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_list_commands_req(int llid, int tid, int is_layout)
{ 
  int len = 0;
  len = sprintf(sndbuf, LIST_COMMANDS, tid, is_layout);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_list_commands_resp(int llid, int tid, int qty, t_list_commands *list)
{
  int i, len = 0;
  len = sprintf(sndbuf, LIST_COMMANDS_O, tid, qty);
  for (i=0; i<qty; i++)
    {
    if (strlen(list[i].cmd) == 0)
      KOUT(" ");
    if (strlen(list[i].cmd) >= MAX_LIST_COMMANDS_LEN)
      KOUT("%d", (int) strlen(list[i].cmd));
    len += sprintf(sndbuf+len, LIST_COMMANDS_ITEM, list[i].cmd);
    } 
  len += sprintf(sndbuf+len, LIST_COMMANDS_C);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_vmcmd(int llid, int tid, char *name, int vmcmd, int param)
{
  int len = 0;
  if ((!name) || (strlen(name)==0) || (strlen(name) >= MAX_NAME_LEN))
    KOUT(" ");
  len = sprintf(sndbuf, VMCMD, tid, name, vmcmd, param);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void helper_list_pid_resp(char *msg, int qty, t_pid_lst **lst)
{
  int i;
  char *ptr = msg;
  if (qty)
    {
    *lst = (t_pid_lst *)clownix_malloc(qty * sizeof(t_pid_lst), 5);
    memset ((*lst), 0, qty * sizeof(t_pid_lst));
    }
  else
    *lst = NULL;
  ptr = msg;
  for (i=0; i<qty; i++)
    {
    if (!ptr)
      KOUT(" ");
    ptr = strstr(ptr, "<pid>");
    if (!ptr)
      KOUT("\n\n%s\n\n%s\n\n", msg, ptr);
    if (sscanf(ptr, LIST_PID_ITEM, ((*lst)[i].name), &((*lst)[i].pid)) != 2)
      KOUT(" ");
    ptr = strstr(ptr, "</pid>");
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void helper_list_commands_resp(char *msg, int qty, t_list_commands **list)
{
  int i, len;
  char *ptre, *ptrs = msg;
  if (qty)
    {
    *list = (t_list_commands *)clownix_malloc(qty * sizeof(t_list_commands), 5);
    memset ((*list), 0, qty * sizeof(t_list_commands));
    }
  else
    *list = NULL;
  ptrs = msg;
  for (i=0; i<qty; i++)
    {
    if (!ptrs)
      KOUT(" ");
    ptrs = strstr(ptrs, "<item_list_command_delimiter>");
    if (!ptrs)
      KOUT("%s", msg);
    ptrs += strlen("<item_list_command_delimiter>");
    ptre = strstr(ptrs, "</item_list_command_delimiter>");
    if (!ptre)
      KOUT("%s", msg);
    len = ptre - ptrs;
    if (len >= MAX_LIST_COMMANDS_LEN) 
      KOUT("%d", len);
    memcpy((*list)[i].cmd, ptrs, len);
    (*list)[i].cmd[len] = 0;
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void helper_event_sys(char *msg, t_sys_info *sys, int *tid)
{
  int i, nb_malloc_types, nb_fds_types, len;
  char *ptr = msg;
  t_queue_tx *Qtx;
  memset (sys, 0, sizeof(t_sys_info));
  if (sscanf(msg, EVENT_SYS_O, tid, &nb_malloc_types) != 2)
    KOUT("%s", msg);
  if (nb_malloc_types != MAX_MALLOC_TYPES)
    KOUT(" ");
  for (i=0; i<MAX_MALLOC_TYPES; i++)
    {
    ptr = strstr(ptr, "<m>");
    if (!ptr)
      KOUT(" ");
    if (sscanf(ptr, EVENT_SYS_M, &(sys->mallocs[i])) != 1)
      KOUT(" ");
    ptr = strstr(ptr, "</m>");
    if (!ptr)
      KOUT(" ");
    }
  ptr = strstr(ptr, "<m>");
  if (ptr)
    KOUT(" ");
  ptr = strstr(msg, "<nb_fds_used>"); 
  if (sscanf(ptr, EVENT_SYS_FN, &nb_fds_types) != 1)
    KOUT(" ");
  if (nb_fds_types != type_llid_max)
    KOUT(" ");
  for (i=0; i<type_llid_max; i++)
    {
    ptr = strstr(ptr, "<fd>");
    if (!ptr)
      KOUT(" ");
    if (sscanf(ptr, EVENT_SYS_FU, &(sys->fds_used[i])) != 1)
      KOUT(" ");
    ptr = strstr(ptr, "</fd>");
    if (!ptr)
      KOUT(" ");
    }
  ptr = strstr(ptr, "<fd>");
  if (ptr)
    KOUT(" ");
  ptr = strstr(msg, "<r>");
  if (!ptr)
    KOUT(" ");
  if (sscanf(ptr, EVENT_SYS_R, &(sys->selects),  
       &(sys->cur_channels), &(sys->max_channels), &(sys->cur_channels_recv),
       &(sys->cur_channels_send),  &(sys->clients), &(sys->max_time), 
       &(sys->avg_time), &(sys->above50ms), &(sys->above20ms), 
       &(sys->above15ms), &(sys->nb_queue_tx)) != 12)
        KOUT("%s ", ptr);
  ptr = strstr(ptr, "</r>");
  if (sys->nb_queue_tx)
    {
    len = sys->nb_queue_tx * sizeof(t_queue_tx);
    Qtx=(t_queue_tx *)clownix_malloc(len, 6); 
    memset(Qtx, 0, sys->nb_queue_tx * sizeof(t_queue_tx));
    for (i=0; i<sys->nb_queue_tx; i++)
      {
      ptr = strstr(ptr, "<Qtx>");
      if (!ptr)
        KOUT(" ");
      if (sscanf(ptr, EVENT_SYS_ITEM_Q, &(Qtx[i].peak_size), &(Qtx[i].size), 
                 &(Qtx[i].llid), &(Qtx[i].fd), 
                 &(Qtx[i].waked_count_in), &(Qtx[i].waked_count_out),
                 &(Qtx[i].waked_count_err),
                 &(Qtx[i].out_bytes), &(Qtx[i].in_bytes), Qtx[i].name,
                 &(Qtx[i].id), &(Qtx[i].type)) != 12) 
        KOUT("%s", ptr);
      ptr = strstr(ptr, "</Qtx>");
      if (!ptr)
        KOUT(" ");
      }
    sys->queue_tx = Qtx;
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void helper_fill_topo_cnt(char *msg_in, t_topo_cnt *cnt)
{
  char *ptr1, *ptr2;
  int len = strlen(msg_in);
  char *msg = (char *) malloc(len+1);
  memcpy(msg, msg_in, len);
  msg[len] = 0;
  if (sscanf(msg, EVENT_TOPO_CNT_O, cnt->brandtype, cnt->name, 
                                    &(cnt->is_persistent),
                                    &(cnt->is_privileged),
                                    &(cnt->vm_id),
                                    &(cnt->color),
                                    cnt->image,  
                                    &(cnt->ping_ok),
                                    &(cnt->nb_tot_eth)) != 9)
    KOUT("%s", msg);
  get_eth_table(msg, cnt->nb_tot_eth, cnt->eth_table);

  ptr1 = strstr(msg, "<startup_env_keyid>");
  if(!ptr1)
    KOUT("%s", msg);
  ptr2 = strstr(ptr1, "</startup_env_keyid>");
  if(!ptr2)
    KOUT("%s", msg);
  *ptr2 = 0;
  len = strlen("<startup_env_keyid>");
  if (strlen(ptr1) - len >= MAX_PATH_LEN)
    KOUT("%s", msg);
  strncpy(cnt->startup_env, ptr1+len, MAX_PATH_LEN-1); 
  ptr2 += 1;
  ptr1 = strstr(ptr2, "<vmount_keyid>");
  if(!ptr1)
    KOUT("%s", msg);
  ptr2 = strstr(ptr1, "</vmount_keyid>");
  if(!ptr2)
    KOUT("%s", msg);
  *ptr2 = 0;
  len = strlen("<vmount_keyid>");
  if (strlen(ptr1) - len >= MAX_SIZE_VMOUNT)
    KOUT("%s", msg);
  strncpy(cnt->vmount, ptr1+len, MAX_SIZE_VMOUNT-1);

  if ((strlen(cnt->name) == 0) ||
      (strlen(cnt->image) == 0)) 
    KOUT("%s", msg);
  free(msg);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void helper_fill_topo_kvm(char *msg, t_topo_kvm *kvm)
{
  t_topo_kvm ikvm;
  memset(&ikvm, 0, sizeof(t_topo_kvm));
  if (sscanf(msg, EVENT_TOPO_KVM_O, ikvm.name,
                                    &(ikvm.vm_id),
                                    &(ikvm.color),
                                    ikvm.install_cdrom,
                                    ikvm.added_cdrom,
                                    ikvm.added_disk,
                                    ikvm.linux_kernel,
                                    ikvm.rootfs_used,
                                    ikvm.rootfs_backing,
                                    &(ikvm.vm_config_flags),
                                    &(ikvm.vm_config_param),
                                    &(ikvm.mem),
                                    &(ikvm.cpu),
                                    &(ikvm.nb_tot_eth)) != 14)
    KOUT("%s", msg);
  get_eth_table(msg, ikvm.nb_tot_eth, ikvm.eth_table);
  topo_kvm_swapoff(kvm, &ikvm);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void helper_fill_topo_c2c(char *msg, t_topo_c2c *c2c)
{

  if (sscanf(msg, EVENT_TOPO_C2C, c2c->name,
                                  &(c2c->endp_type),
                                  c2c->dist_cloon,
                                  c2c->attlan,
                                  &(c2c->local_is_master),
                                  &(c2c->dist_tcp_ip),
                                  &(c2c->dist_tcp_port),
                                  &(c2c->loc_udp_ip),
                                  &(c2c->dist_udp_ip),
                                  &(c2c->loc_udp_port),
                                  &(c2c->dist_udp_port),
                                  &(c2c->tcp_connection_peered),
                                  &(c2c->udp_connection_peered)) != 13)
    KOUT("%s", msg);
  if (!strcmp(c2c->attlan, "name_to_erase_upon_extract"))
    memset(c2c->attlan, 0, MAX_NAME_LEN);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void helper_fill_topo_nat(char *msg, t_topo_nat *nat)
{
  if (sscanf(msg, EVENT_TOPO_NAT, nat->name, &(nat->endp_type)) != 2)
    KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void helper_fill_topo_tap(char *msg, t_topo_tap *tap)
{
  if (sscanf(msg, EVENT_TOPO_TAP, tap->name, &(tap->endp_type)) != 2)
    KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void helper_fill_topo_phy(char *msg, t_topo_phy *phy)
{
  if (sscanf(msg, EVENT_TOPO_PHY, phy->name, &(phy->endp_type)) != 2)
    KOUT(" ");
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
static void helper_fill_topo_a2b(char *msg, t_topo_a2b *a2b)
{
  if (sscanf(msg, EVENT_TOPO_A2B, a2b->name, &(a2b->endp_type0),
                                             &(a2b->endp_type1)) != 3)
    KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void helper_fill_topo_info_phy(char *msg, t_topo_info_phy *phy)
{
  if (sscanf(msg, EVENT_TOPO_INFO_PHY, phy->name) != 1)
    KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void helper_fill_topo_bridges(char *msg, t_topo_bridges *bridges)
{
  int i;
  char *ptr = msg;
  if (sscanf(msg,EVENT_TOPO_BRIDGES_O,bridges->br,&(bridges->nb_ports))!=2)
    KOUT("%s", msg);
  if ((bridges->nb_ports < 0) || (bridges->nb_ports >= MAX_OVS_PORTS))
    KOUT("MAX_OVS_PORTS: %d  %s", bridges->nb_ports, msg);
  for (i=0; i<bridges->nb_ports; i++)
    {
    ptr = strstr(ptr, "<port_name>");
    if (!ptr)
      KOUT("%s", msg);
    if (sscanf(ptr, EVENT_TOPO_BRIDGES_I, bridges->ports[i]) != 1)
      KOUT(" ");
    ptr = strstr(ptr, "</port_name>");
    if (!ptr)
      KOUT("%s", msg);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void helper_fill_topo_lan(char *msg, t_lan_group *vlg)
{
  int i, len; 
  char *ptr = msg;
  len = vlg->nb_lan * sizeof(t_lan_group_item);
  vlg->lan = (t_lan_group_item *) clownix_malloc(len, 9);
  memset(vlg->lan, 0, len);
  for (i=0; i<vlg->nb_lan; i++)
    {
    ptr = strstr(ptr, "<lan>");
    if (!ptr)
      KOUT("%s", msg);
    if (sscanf(ptr, EVENT_TOPO_LAN, vlg->lan[i].lan) != 1)
      KOUT(" ");
    ptr = strstr(ptr, "</lan>");
    if (!ptr)
      KOUT("%s", msg);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void helper_fill_topo_endp(char *msg, t_topo_endp *endp)
{
  if (sscanf(msg, EVENT_TOPO_ENDP_O, endp->name,
                                     &(endp->num),
                                     &(endp->type),
                                     &(endp->lan.nb_lan)) != 4)
    KOUT("%s", msg);
  helper_fill_topo_lan(msg, &(endp->lan));
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static t_topo_info *helper_event_topo (char *msg, int *tid)
{
  int i, len;
  char *ptr = msg;
  t_topo_clc icf;
  t_topo_info *topo;
  topo = (t_topo_info *)clownix_malloc(sizeof(t_topo_info), 22);
  memset(topo, 0, sizeof(t_topo_info));
  memset(&icf, 0, sizeof(t_topo_clc));
  if (sscanf(msg, EVENT_TOPO_O, tid, icf.version,
                                     &(topo->conf_rank),
                                     icf.network,
                                     icf.username,
                                     &(icf.server_port),
                                     icf.bin_dir,
                                     &(icf.flags_config),
                                     &(topo->nb_cnt),
                                     &(topo->nb_kvm),
                                     &(topo->nb_c2c),
                                     &(topo->nb_tap),
                                     &(topo->nb_phy),
                                     &(topo->nb_a2b),
                                     &(topo->nb_nat),
                                     &(topo->nb_endp),
                                     &(topo->nb_info_phy),
                                     &(topo->nb_bridges)) != 18)
    KOUT("%s", msg);
  topo_config_swapoff(&(topo->clc), &icf);
  len = topo->nb_cnt*sizeof(t_topo_cnt);
  topo->cnt= (t_topo_cnt *) clownix_malloc(len, 18);
  memset(topo->cnt, 0, len);
  len = topo->nb_kvm*sizeof(t_topo_kvm);
  topo->kvm= (t_topo_kvm *) clownix_malloc(len, 19);
  memset(topo->kvm, 0, len);
  len = topo->nb_c2c*sizeof(t_topo_c2c);
  topo->c2c= (t_topo_c2c *) clownix_malloc(len, 20);
  memset(topo->c2c, 0, len);
  len = topo->nb_tap*sizeof(t_topo_tap);
  topo->tap= (t_topo_tap *) clownix_malloc(len, 21);
  memset(topo->tap, 0, len);
  len = topo->nb_phy*sizeof(t_topo_phy);
  topo->phy= (t_topo_phy *) clownix_malloc(len, 22);
  memset(topo->phy, 0, len);
  len = topo->nb_a2b*sizeof(t_topo_a2b);
  topo->a2b= (t_topo_a2b *) clownix_malloc(len, 23);
  memset(topo->a2b, 0, len);
  len = topo->nb_nat*sizeof(t_topo_nat);
  topo->nat= (t_topo_nat *) clownix_malloc(len, 24);
  memset(topo->nat, 0, len);
  len = topo->nb_endp*sizeof(t_topo_endp);
  topo->endp=(t_topo_endp *)clownix_malloc(len, 25);
  memset(topo->endp, 0, len);
  len = topo->nb_info_phy*sizeof(t_topo_info_phy);
  topo->info_phy= (t_topo_info_phy *) clownix_malloc(len, 26);
  memset(topo->info_phy, 0, len);
  len = topo->nb_bridges*sizeof(t_topo_bridges);
  topo->bridges= (t_topo_bridges *) clownix_malloc(len, 27);
  memset(topo->bridges, 0, len);

  for (i=0; i<topo->nb_cnt; i++)
    {
    ptr = strstr(ptr, "<cnt>");
    if (!ptr)
      KOUT("%d,%d\n%s\n", topo->nb_cnt, i, msg);
    helper_fill_topo_cnt(ptr, &(topo->cnt[i]));
    ptr = strstr(ptr, "</cnt>");
    if (!ptr)
      KOUT(" ");
    }

  for (i=0; i<topo->nb_kvm; i++)
    {
    ptr = strstr(ptr, "<kvm>");
    if (!ptr)
      KOUT("%d,%d\n%s\n", topo->nb_kvm, i, msg);
    helper_fill_topo_kvm(ptr, &(topo->kvm[i]));
    ptr = strstr(ptr, "</kvm>");
    if (!ptr)
      KOUT(" ");
    }

  for (i=0; i<topo->nb_c2c; i++)
    {
    ptr = strstr(ptr, "<c2c>");
    if (!ptr)
      KOUT("%d,%d\n%s\n", topo->nb_c2c, i, msg);
    helper_fill_topo_c2c(ptr, &(topo->c2c[i]));
    ptr = strstr(ptr, "</c2c>");
    if (!ptr)
      KOUT(" ");
    }

  for (i=0; i<topo->nb_tap; i++)
    {
    ptr = strstr(ptr, "<tap>");
    if (!ptr)
      KOUT("%d,%d\n%s\n", topo->nb_tap, i, msg);
    helper_fill_topo_tap(ptr, &(topo->tap[i]));
    ptr = strstr(ptr, "</tap>");
    if (!ptr)
      KOUT(" ");
    }

  for (i=0; i<topo->nb_phy; i++)
    {
    ptr = strstr(ptr, "<phy>");
    if (!ptr)
      KOUT("%d,%d\n%s\n", topo->nb_phy, i, msg);
    helper_fill_topo_phy(ptr, &(topo->phy[i]));
    ptr = strstr(ptr, "</phy>");
    if (!ptr)
      KOUT(" ");
    }


  for (i=0; i<topo->nb_a2b; i++)
    {
    ptr = strstr(ptr, "<a2b>");
    if (!ptr)
      KOUT("%d,%d\n%s\n", topo->nb_a2b, i, msg);
    helper_fill_topo_a2b(ptr, &(topo->a2b[i]));
    ptr = strstr(ptr, "</a2b>");
    if (!ptr)
      KOUT(" ");
    }

  for (i=0; i<topo->nb_nat; i++)
    {
    ptr = strstr(ptr, "<nat>");
    if (!ptr)
      KOUT("%d,%d\n%s\n", topo->nb_nat, i, msg);
    helper_fill_topo_nat(ptr, &(topo->nat[i]));
    ptr = strstr(ptr, "</nat>");
    if (!ptr)
      KOUT(" ");
    }

  for (i=0; i<topo->nb_endp; i++)
    {
    ptr = strstr(ptr, "<endp>");
    if (!ptr)
      KOUT("%d,%d\n%s\n", topo->nb_endp, i, msg);
    helper_fill_topo_endp(ptr, &(topo->endp[i]));
    ptr = strstr(ptr, "</endp>");
    if (!ptr)
      KOUT(" ");
    }
  for (i=0; i<topo->nb_info_phy; i++)
    {
    ptr = strstr(ptr, "<info_phy>");
    if (!ptr)
      KOUT("%d,%d\n%s\n", topo->nb_info_phy, i, msg);
    helper_fill_topo_info_phy(ptr, &(topo->info_phy[i]));
    ptr = strstr(ptr, "</info_phy>");
    if (!ptr)
      KOUT(" ");
    }

  for (i=0; i<topo->nb_bridges; i++)
    {
    ptr = strstr(ptr, "<bridges>");
    if (!ptr)
      KOUT("%d,%d\n%s\n", topo->nb_bridges, i, msg);
    helper_fill_topo_bridges(ptr, &(topo->bridges[i]));
    ptr = strstr(ptr, "</bridges>");
    if (!ptr)
      KOUT(" ");
    }

  return topo;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_eventfull_sub(int llid, int tid)
{
  int len = 0;
  len = sprintf(sndbuf, EVENTFULL_SUB, tid);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_eventfull(int llid, int tid, int nb_endp, t_eventfull_endp *endp)
{
  int i, len = 0;
  len += sprintf(sndbuf+len, EVENTFULL_O, tid, nb_endp);
  for (i=0; i<nb_endp; i++)
    len += sprintf(sndbuf+len, EVENTFULL_ENDP, endp[i].name, endp[i].num,
                                               endp[i].type, endp[i].ram,
                                               endp[i].cpu, endp[i].ok,
                                               endp[i].ptx, endp[i].prx,
                                               endp[i].btx, endp[i].brx,
                                               endp[i].ms);
  len += sprintf(sndbuf+len, EVENTFULL_C);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void helper_eventfull_endp(char *msg, int nb, t_eventfull_endp *endp)
{
  char *ptr = msg;
  int i;
  for (i=0; i<nb; i++)
    {
    ptr = strstr (ptr, "<eventfull_endp>");
    if (!ptr)
      KOUT("%s", msg);
    if (sscanf(ptr, EVENTFULL_ENDP,  endp[i].name,
                                     &(endp[i].num),
                                     &(endp[i].type),
                                     &(endp[i].ram),
                                     &(endp[i].cpu),
                                     &(endp[i].ok),
                                     &(endp[i].ptx),
                                     &(endp[i].prx),
                                     &(endp[i].btx),
                                     &(endp[i].brx),
                                     &(endp[i].ms)) != 11)
      KOUT("%s", msg);
    ptr = strstr (ptr, "</eventfull_endp>");
    if (!ptr)
      KOUT("%s", msg);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_slowperiodic_sub(int llid, int tid)
{
  int len = 0;
  len = sprintf(sndbuf, SLOWPERIODIC_SUB, tid);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_slowperiodic_qcow2(int llid, int tid, int nb, t_slowperiodic *spic)
{
  int i, len = 0;
  len += sprintf(sndbuf+len, SLOWPERIODIC_QCOW2_O, tid, nb);
  for (i=0; i<nb; i++)
    {
    if (strlen(spic[i].name) == 0)
      KOUT("ERROR %d", i);
    len += sprintf(sndbuf+len, SLOWPERIODIC_SPIC, spic[i].name);
    }
  len += sprintf(sndbuf+len, SLOWPERIODIC_QCOW2_C);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_slowperiodic_img(int llid, int tid, int nb, t_slowperiodic *spic)
{
  int i, len = 0;
  len += sprintf(sndbuf+len, SLOWPERIODIC_IMG_O, tid, nb);
  for (i=0; i<nb; i++)
    {
    if (strlen(spic[i].name) == 0)
      KOUT("ERROR %d", i);
    len += sprintf(sndbuf+len, SLOWPERIODIC_SPIC, spic[i].name);
    }
  len += sprintf(sndbuf+len, SLOWPERIODIC_IMG_C);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*---------------------------------------------------------------------------*/
  
/*****************************************************************************/
void send_slowperiodic_cvm(int llid, int tid, int nb, t_slowperiodic *spic)
{
  int i, len = 0;
  len += sprintf(sndbuf+len, SLOWPERIODIC_CVM_O, tid, nb);
  for (i=0; i<nb; i++)
    {
    if (strlen(spic[i].name) == 0)
      KOUT("ERROR %d", i);
    len += sprintf(sndbuf+len, SLOWPERIODIC_SPIC, spic[i].name);
    }
  len += sprintf(sndbuf+len, SLOWPERIODIC_CVM_C);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void helper_slowperiodic(char *msg, int nb, t_slowperiodic *spic)
{
  char *ptr = msg;
  int i;
  for (i=0; i<nb; i++)
    {
    ptr = strstr (ptr, "<slowperiodic_spic>");
    if (!ptr)
      KOUT("%s", msg);
    if (sscanf(ptr, SLOWPERIODIC_SPIC, spic[i].name) != 1)
      KOUT("%s", msg);
    ptr = strstr (ptr, "</slowperiodic_spic>");
    if (!ptr)
      KOUT("%s", msg);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static char *extract_rpc_msg(char *msg, char *bound)
{
  int len;
  char *ptrs, *ptre, *line = NULL;
  char searchbound[MAX_CLOWNIX_BOUND_LEN];
  memset(searchbound, 0, MAX_CLOWNIX_BOUND_LEN); 
  snprintf(searchbound, MAX_CLOWNIX_BOUND_LEN-1, "<%s>", bound);
  ptrs = strstr(msg, searchbound);
  if (!ptrs)
    KOUT("%s", msg);
  ptrs += strlen(searchbound);
  memset(searchbound, 0, MAX_CLOWNIX_BOUND_LEN); 
  snprintf(searchbound, MAX_CLOWNIX_BOUND_LEN-1, "</%s>", bound);
  ptre = strstr(msg, searchbound);
  len = (int) (ptre - ptrs);
  if (ptrs && ptre && (len>0))
    {
    if (len >= MAX_RPC_MSG_LEN)
      len = MAX_RPC_MSG_LEN-1;
    line = (char *) clownix_malloc(MAX_RPC_MSG_LEN, 17);
    memcpy(line, ptrs, len);
    line[len] = 0;
    }
  return line;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_nat_add(int llid, int tid, char *name)
{
  int len = 0;
  if (name[0] == 0)
    KOUT(" ");
  if (strlen(name) >= MAX_NAME_LEN)
    KOUT(" ");
  len = sprintf(sndbuf, NAT_ADD, tid, name);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_phy_add(int llid, int tid, char *name, int type)
{
  int len = 0;
  if (name[0] == 0)
    KOUT(" ");
  if (strlen(name) >= MAX_NAME_LEN)
    KOUT(" ");
  len = sprintf(sndbuf, PHY_ADD, tid, name, type);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_tap_add(int llid, int tid, char *name)
{
  int len = 0;
  if (name[0] == 0)
    KOUT(" ");
  if (strlen(name) >= MAX_NAME_LEN)
    KOUT(" ");
  len = sprintf(sndbuf, TAP_ADD, tid, name);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_cnt_add(int llid, int tid, t_topo_cnt *cnt)
{
  int len = 0;

  if ((strlen(cnt->name) == 0) ||
      (strlen(cnt->image) == 0) ||
      (strlen(cnt->brandtype) == 0))
    KOUT("name:%s image:%s", cnt->name, cnt->image);
  if (strlen(cnt->name) >= MAX_NAME_LEN)
    KOUT(" ");
  if (strlen(cnt->image) >= MAX_PATH_LEN)
    KOUT(" ");
  if (strlen(cnt->startup_env) >= MAX_PATH_LEN)
    KOUT(" ");
  if (strlen(cnt->vmount) >= MAX_SIZE_VMOUNT)
    KOUT(" ");
  len = sprintf(sndbuf, ADD_CNT_O, tid, cnt->brandtype, cnt->name,
                cnt->is_persistent, cnt->is_privileged, cnt->vm_id,
                cnt->color, cnt->ping_ok, cnt->nb_tot_eth);
  len += make_eth_table(sndbuf+len, cnt->nb_tot_eth, cnt->eth_table);
  len += sprintf(sndbuf+len, ADD_CNT_C, cnt->startup_env,
                 cnt->vmount, cnt->image);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_a2b_add(int llid, int tid, char *name)
{
  int len = 0;
  if (name[0] == 0)
    KOUT(" ");
  if (strlen(name) >= MAX_NAME_LEN)
    KOUT(" ");
  len = sprintf(sndbuf, A2B_ADD, tid, name);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_c2c_cnf(int llid, int tid, char *name, char *cmd)
{
  int len = 0;
  if (name[0] == 0)
    KOUT(" ");
  if (strlen(name) >= MAX_NAME_LEN)
    KOUT(" ");
  len = sprintf(sndbuf, C2C_CNF, tid, name, cmd);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_nat_cnf(int llid, int tid, char *name, char *cmd)
{
  int len = 0;
  if (name[0] == 0)
    KOUT(" ");
  if (strlen(name) >= MAX_NAME_LEN)
    KOUT(" ");
  if (cmd[0] == 0)
    KOUT(" ");
  if (strlen(cmd) >= MAX_PRINT_LEN)
    KOUT(" ");
  len = sprintf(sndbuf, NAT_CNF, tid, name, cmd);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_a2b_cnf(int llid, int tid, char *name, char *cmd)
{
  int len = 0;
  if (name[0] == 0)
    KOUT(" ");
  if (strlen(name) >= MAX_NAME_LEN)
    KOUT(" ");
  if (cmd[0] == 0)
    KOUT(" ");
  if (strlen(cmd) >= MAX_PRINT_LEN)
    KOUT(" ");
  len = sprintf(sndbuf, A2B_CNF, tid, name, cmd);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*---------------------------------------------------------------------------*/
  
/*****************************************************************************/
void send_lan_cnf(int llid, int tid, char *name, char *cmd)
{
  int len = 0;
  if (name[0] == 0)
    KOUT(" ");
  if (strlen(name) >= MAX_NAME_LEN)
    KOUT(" ");
  if (cmd[0] == 0)
    KOUT(" ");
  if (strlen(cmd) >= MAX_PRINT_LEN)
    KOUT(" ");
  len = sprintf(sndbuf, LAN_CNF, tid, name, cmd);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_c2c_add(int llid, int tid, char *c2c_name, uint32_t local_udp_ip, 
                  char *dcloon, uint32_t ip, uint16_t port,
                  char *passwd, uint32_t udp_ip, uint16_t c2c_udp_port_low)
{
  int len = 0;
  if (c2c_name[0] == 0)
    KOUT(" ");
  if (strlen(c2c_name) >= MAX_NAME_LEN)
    KOUT(" ");
  if (dcloon[0] == 0)
    KOUT(" ");
  if (strlen(dcloon) >= MAX_NAME_LEN)
    KOUT(" ");
  if (passwd[0] == 0)
    KOUT(" ");
  if (strlen(passwd) >= MSG_DIGEST_LEN)
    KOUT(" ");
  len = sprintf(sndbuf, C2C_ADD, tid, c2c_name, local_udp_ip, 
                dcloon, ip, port, passwd, udp_ip, c2c_udp_port_low);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_snf_add(int llid, int tid, char *name, int num, int val)
{
  int len = 0;
  if (name[0] == 0)
    KOUT(" ");
  if (strlen(name) >= MAX_NAME_LEN)
    KOUT(" ");
  len = sprintf(sndbuf, SNF_ADD, tid, name, num, val);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_c2c_peer_create(int llid, int tid, char *c2c_name, int is_ack,
                          char *lcloon, char *dcloon)
{
  int len = 0;
  if (c2c_name[0] == 0)
    KOUT(" ");
  if (strlen(c2c_name) >= MAX_NAME_LEN)
    KOUT(" ");
  if (lcloon[0] == 0)
    KOUT(" ");
  if (strlen(lcloon) >= MAX_NAME_LEN)
    KOUT(" ");
  if (dcloon[0] == 0)
    KOUT(" ");
  if (strlen(dcloon) >= MAX_NAME_LEN)
    KOUT(" ");
  len = sprintf(sndbuf, C2C_CREATE, tid,c2c_name,is_ack,lcloon,dcloon);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_c2c_peer_conf(int llid, int tid, char *c2c_name, int is_ack,
                        char *lcloon, char *dcloon,
                        uint32_t ludp_ip, uint32_t dudp_ip,
                        uint16_t ludp_port, uint16_t dudp_port)
{
  int len = 0;
  if (c2c_name[0] == 0)
    KOUT(" ");
  if (strlen(c2c_name) >= MAX_NAME_LEN)
    KOUT(" ");
  if (lcloon[0] == 0)
    KOUT(" ");
  if (strlen(lcloon) >= MAX_NAME_LEN)
    KOUT(" ");
  if (dcloon[0] == 0)
    KOUT(" ");
  if (strlen(dcloon) >= MAX_NAME_LEN)
    KOUT(" ");
  len = sprintf(sndbuf, C2C_CONF, tid, c2c_name, is_ack, lcloon, dcloon,
                ludp_ip, dudp_ip, ludp_port, dudp_port);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_c2c_peer_ping(int llid, int tid, char *c2c_name, int status)
{
  int len = 0;
  if (c2c_name[0] == 0)
    KOUT(" ");
  if (strlen(c2c_name) >= MAX_NAME_LEN)
    KOUT(" ");
  len = sprintf(sndbuf, C2C_PING, tid, c2c_name, status);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_color_item(int llid, int tid, char *name, int color)
{
  int len = 0;
  if (name[0] == 0)
    KOUT(" ");
  if (strlen(name) >= MAX_NAME_LEN)
    KOUT(" ");
  len = sprintf(sndbuf, COLOR_ITEM, tid, name, color);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_novnc_on_off(int llid, int tid, int on)
{
  int len = 0;
  len = sprintf(sndbuf, NOVNC_ON_OFF, tid, on);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void local_topo_free_topo(t_topo_info *topo)
{
  int i;
  if (topo)
    {
    for (i=0; i<topo->nb_endp; i++)
      clownix_free(topo->endp[i].lan.lan, __FUNCTION__);
    clownix_free(topo->cnt, __FUNCTION__);
    clownix_free(topo->kvm, __FUNCTION__);
    clownix_free(topo->c2c, __FUNCTION__);
    clownix_free(topo->tap, __FUNCTION__);
    clownix_free(topo->a2b, __FUNCTION__);
    clownix_free(topo->nat, __FUNCTION__);
    clownix_free(topo->phy, __FUNCTION__);
    clownix_free(topo->info_phy, __FUNCTION__);
    clownix_free(topo->bridges, __FUNCTION__);
    clownix_free(topo->endp, __FUNCTION__);
    clownix_free(topo, __FUNCTION__);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void startup_env_get(char *msg_in, t_topo_cnt *cnt)
{
  char *ptr, *ptr1, *ptr2;
  int len = strlen(msg_in);
  char *msg = (char *) malloc(len+1);
  memcpy(msg, msg_in, len);
  msg[len] = 0;
  ptr = strstr(msg, "<image> ");
  if (!ptr)
    KOUT("%s", msg);
  if (sscanf(ptr, "<image> %s </image>", cnt->image) != 1)
    KOUT("%s", ptr);
  ptr1 = strstr(msg, "<startup_env_keyid>");
  if(!ptr1)
    KOUT("%s", msg);
  ptr2 = strstr(ptr1, "</startup_env_keyid>");
  if(!ptr2)
    KOUT("%s", msg);
  *ptr2 = 0;
  len = strlen("<startup_env_keyid>");
  if (strlen(ptr1) - len >= MAX_PATH_LEN)
    KOUT("%s", msg);
  strncpy(cnt->startup_env, ptr1+len, MAX_PATH_LEN-1);
  free(msg);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void vmount_get(char *msg_in, t_topo_cnt *cnt)
{
  char *ptr, *ptr1, *ptr2;
  int len = strlen(msg_in);
  char *msg = (char *) malloc(len+1);
  memcpy(msg, msg_in, len);
  msg[len] = 0;
  ptr = strstr(msg, "<image> ");
  if (!ptr)
    KOUT("%s", msg);
  if (sscanf(ptr, "<image> %s </image>", cnt->image) != 1)
    KOUT("%s", ptr);
  ptr1 = strstr(msg, "<vmount_keyid>");
  if(!ptr1)
    KOUT("%s", msg);
  ptr2 = strstr(ptr1, "</vmount_keyid>");
  if(!ptr2)
    KOUT("%s", msg);
  *ptr2 = 0;
  len = strlen("<vmount_keyid>");
  if (strlen(ptr1) - len >= MAX_SIZE_VMOUNT)
    KOUT("%s", msg);
  strncpy(cnt->vmount, ptr1+len, MAX_SIZE_VMOUNT-1);
  free(msg);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void dispatcher(int llid, int bnd_evt, char *msg)
{
  int len, nb, flags_hop, num, is_layout;
  int cmd, vmcmd, param, status, sub;
  int type, qty, tid, val; 
  uint32_t ip, dudp_ip, ludp_ip;
  uint16_t lport, dport, c2c_udp_port_low;
  t_topo_clc  icf;
  t_topo_clc  cf;
  t_eventfull_endp *eventfull_endp;
  t_slowperiodic *slowperiodic;
  char *parm1, *parm2;
  t_topo_kvm ikvm;
  t_topo_kvm kvm;
  t_topo_cnt cnt;
  char network[MAX_NAME_LEN];
  char dnet[MAX_NAME_LEN];
  char lnet[MAX_NAME_LEN];
  char name[MAX_NAME_LEN];
  char path[MAX_PATH_LEN];
  char param1[MAX_PATH_LEN];
  char param2[MAX_PATH_LEN];
  char lan[MAX_NAME_LEN];
  char info[MAX_PRINT_LEN];
  char *ptr, *line;
  t_pid_lst *pid_lst;
  t_sys_info *sys;
  t_topo_info *topo;
  t_list_commands *list_commands;
  t_stats_counts stats_counts;
  t_stats_sysinfo stats_sysinfo;
  t_hop_list *list;


  switch(bnd_evt)
    {

    case bnd_hop_evt_doors_sub:
      if (sscanf(msg, HOP_EVT_DOORS_SUB_O, &tid, &flags_hop, &qty) != 3)
        KOUT("%s", msg);
      list = helper_rx_hop_name_list(msg, qty);
      recv_hop_evt_doors_sub(llid, tid, flags_hop, qty, list);
      clownix_free(list, __FUNCTION__);
      break;

    case bnd_hop_evt_doors_unsub:
      if (sscanf(msg, HOP_EVT_DOORS_UNSUB, &tid) != 1)
        KOUT("%s", msg);
      recv_hop_evt_doors_unsub(llid, tid);
      break;


    case bnd_hop_evt_doors:
      if (sscanf(msg, HOP_EVT_DOORS_O, &tid, &flags_hop, name) != 3)
        KOUT("%s", msg);
      line = extract_rpc_msg(msg, "hop_free_txt_joker");
      recv_hop_evt_doors(llid, tid, flags_hop, name, line);
      clownix_free(line, __FUNCTION__);
      break;

   case bnd_hop_get_list:
      if (sscanf(msg, HOP_GET_LIST_NAME, &tid) != 1)
        KOUT("%s", msg);
      recv_hop_get_name_list_doors(llid, tid);
      break;

    case bnd_hop_list:
      if (sscanf(msg, HOP_LIST_NAME_O, &tid, &qty) != 2)
        KOUT("%s", msg);
      list = helper_rx_hop_name_list(msg, qty);
      recv_hop_name_list_doors(llid, tid, qty, list);
      clownix_free(list, __FUNCTION__);
      break;

    case bnd_sub_evt_stats_endp:
      if (sscanf(msg, SUB_EVT_STATS_ENDP, &tid, name, &num, &sub) != 4)
        KOUT("%s", msg);
      recv_evt_stats_endp_sub(llid, tid, name, num, sub);
      break;

    case bnd_evt_stats_endp:
      if (sscanf(msg, EVT_STATS_ENDP_O, &tid, network, name, &num,
                                        &status, &(stats_counts.nb_items)) != 6) 
        KOUT("%s", msg);
      helper_stats(msg, stats_counts.nb_items, stats_counts.item);
      recv_evt_stats_endp(llid, tid, network, name, num, &stats_counts,status);
      break;

    case bnd_sub_evt_stats_sysinfo:
      if (sscanf(msg, SUB_EVT_STATS_SYSINFO, &tid, name, &sub) != 3)
        KOUT("%s", msg);
      recv_evt_stats_sysinfo_sub(llid, tid, name, sub);
      break;

    case bnd_evt_stats_sysinfo:
      if (sscanf(msg, EVT_STATS_SYSINFOO, &tid, network, name,
             &status, &(stats_sysinfo.time_ms), &(stats_sysinfo.uptime),
             &(stats_sysinfo.load1), &(stats_sysinfo.load5),
             &(stats_sysinfo.load15), &(stats_sysinfo.totalram), 
             &(stats_sysinfo.freeram), &(stats_sysinfo.cachedram),
             &(stats_sysinfo.sharedram), &(stats_sysinfo.bufferram),
             &(stats_sysinfo.totalswap), &(stats_sysinfo.freeswap),
             &(stats_sysinfo.procs), &(stats_sysinfo.totalhigh),
             &(stats_sysinfo.freehigh), &(stats_sysinfo.mem_unit),
             &(stats_sysinfo.process_utime), &(stats_sysinfo.process_stime),          
             &(stats_sysinfo.process_cutime), &(stats_sysinfo.process_cstime),
             &(stats_sysinfo.process_rss)) != 25)
        KOUT("%s", msg);
      line = extract_rpc_msg(msg, "bound_for_df_dumprd");
      recv_evt_stats_sysinfo(llid, tid, network, name,
                             &stats_sysinfo, line, status);
      clownix_free(line, __FUNCTION__);
      break;

    case bnd_event_sys_sub:
      if (sscanf(msg, EVENT_SYS_SUB, &tid) != 1)
        KOUT("%s", msg);
      recv_event_sys_sub(llid, tid);
      break;
    case bnd_event_sys_unsub:
      if (sscanf(msg, EVENT_SYS_UNSUB, &tid) != 1)
        KOUT("%s", msg);
      recv_event_sys_unsub(llid, tid);
      break;
    case bnd_event_sys:
      sys = (t_sys_info *) clownix_malloc(sizeof(t_sys_info), 21);
      helper_event_sys(msg, sys, &tid);
      recv_event_sys(llid, tid, sys);
      sys_info_free(sys);
      break;

    case bnd_event_topo_sub:
      if (sscanf(msg, EVENT_TOPO_SUB, &tid) != 1)
        KOUT("%s", msg);
      recv_event_topo_sub(llid, tid);
      break;
    case bnd_event_topo_unsub:
      if (sscanf(msg, EVENT_TOPO_UNSUB, &tid) != 1)
        KOUT("%s", msg);
      recv_event_topo_unsub(llid, tid);
      break;
    case bnd_event_topo:
      topo = helper_event_topo(msg, &tid);
      recv_event_topo(llid, tid, topo);
      local_topo_free_topo(topo);
      break;

    case bnd_topo_small_event_sub:
      if (sscanf(msg, TOPO_SMALL_EVENT_SUB, &tid) != 1)
        KOUT("%s", msg);
      recv_topo_small_event_sub(llid, tid);
      break;
    case bnd_topo_small_event_unsub:
      if (sscanf(msg, TOPO_SMALL_EVENT_UNSUB, &tid) != 1)
        KOUT("%s", msg);
      recv_topo_small_event_unsub(llid, tid);
      break;
    case bnd_topo_small_event:
      if (sscanf(msg, TOPO_SMALL_EVENT, &tid, name, 
                 param1, param2, &type) != 5)
        KOUT("%s", msg);
      if (!strcmp(param1, "undefined_param1"))
        parm1 = NULL;
      else
        parm1 = param1; 
      if (!strcmp(param2, "undefined_param2"))
        parm2 = NULL;
      else
        parm2 = param2;
      recv_topo_small_event(llid, tid, name, parm1, parm2, type);
      break;

    case bnd_evt_print_sub:
      if (sscanf(msg, EVENT_PRINT_SUB, &tid) != 1)
        KOUT("%s", msg);
      recv_evt_print_sub(llid, tid);
      break;
    case bnd_evt_print_unsub:
      if (sscanf(msg, EVENT_PRINT_UNSUB, &tid) != 1)
        KOUT("%s", msg);
      recv_evt_print_unsub(llid, tid);
      break;
    case bnd_event_print:
      if (sscanf(msg, EVENT_PRINT, &tid, info) != 2)
        KOUT("%s", msg);
      recv_evt_print(llid, tid, xml_to_string(info));
      break;

    case bnd_status_ok:
      if (sscanf(msg, STATUS_OK, &tid, info) != 2)
        KOUT("%s", msg);
      recv_status_ok(llid, tid, xml_to_string(info));
      break;
    case bnd_status_ko:
      if (sscanf(msg, STATUS_KO, &tid, info) != 2)
        KOUT("%s", msg);
      recv_status_ko(llid, tid, xml_to_string(info));
      break;

    case bnd_fix_display:
      if (sscanf(msg, FIX_DISPLAY, &tid, param1, info) != 3)
        KOUT("%s", msg);
      recv_fix_display(llid, tid, param1, xml_to_string(info));
      break;


    case bnd_add_vm:
      memset(&ikvm, 0, sizeof(t_topo_kvm));
      if (sscanf(msg, ADD_KVM_O, &tid, ikvm.name, &(ikvm.vm_id),
                 &(ikvm.color), &(ikvm.vm_config_flags),
                 &(ikvm.vm_config_param), &(ikvm.mem), &(ikvm.cpu),
                 &(ikvm.nb_tot_eth)) != 9)
        KOUT("%s", msg);
      get_eth_table(msg, ikvm.nb_tot_eth, ikvm.eth_table);
      ptr = strstr(msg, "<linux_kernel>");
      if (!ptr)
        KOUT("%s", msg);
      if (sscanf(ptr, ADD_KVM_C, ikvm.linux_kernel, 
                                 ikvm.rootfs_input, 
                                 ikvm.install_cdrom, 
                                 ikvm.added_cdrom, 
                                 ikvm.added_disk) != 5) 
        KOUT("%s", msg);
      topo_kvm_swapoff(&kvm, &ikvm); 
      recv_add_vm(llid, tid, &kvm);
      break;
    case bnd_sav_vm:
      if (sscanf(msg, SAV_VM, &tid, name, path) != 3)
        KOUT("%s", msg);
      recv_sav_vm(llid, tid, name, path);
      break;

    case bnd_del_name:
      if (sscanf(msg, DEL_SAT, &tid, name) != 2)
        KOUT("%s", msg);
      recv_del_name(llid, tid, name);
      break;

    case bnd_add_lan_endp:
      if (sscanf(msg, ADD_LAN_ENDP, &tid, name, &num, lan) != 4)
        KOUT("%s", msg);
      recv_add_lan_endp(llid, tid, name, num, lan);
      break;

    case bnd_del_lan_endp:
      if (sscanf(msg, DEL_LAN_ENDP, &tid, name,  &num, lan) != 4)
        KOUT("%s", msg);
      recv_del_lan_endp(llid, tid, name, num, lan);
      break;

    case bnd_kill_uml_clownix:
      if (sscanf(msg, KILL_UML_CLOWNIX, &tid) != 1)
        KOUT("%s", msg);
      recv_kill_uml_clownix(llid, tid);
      break;
    case bnd_del_all:
      if (sscanf(msg, DEL_ALL, &tid) != 1)
        KOUT("%s", msg);
      recv_del_all(llid, tid);
      break;
    case bnd_list_pid_req:
      if (sscanf(msg, LIST_PID, &tid) != 1)
        KOUT("%s", msg);
      recv_list_pid_req(llid, tid);
      break;
    case bnd_list_pid_resp:
      if (sscanf(msg, LIST_PID_O, &tid, &qty) != 2)
        KOUT("%s", msg);
      helper_list_pid_resp(msg, qty, &pid_lst);
      recv_list_pid_resp(llid, tid, qty, pid_lst);
      if (qty)
        clownix_free(pid_lst, __FUNCTION__);
      break;

    case bnd_list_commands_req:
      if (sscanf(msg, LIST_COMMANDS, &tid, &is_layout) != 2)
        KOUT("%s", msg);
      recv_list_commands_req(llid, tid, is_layout);
      break;

    case bnd_list_commands_resp:
      if (sscanf(msg, LIST_COMMANDS_O, &tid, &qty) != 2)
        KOUT("%s", msg);
      helper_list_commands_resp(msg, qty, &list_commands);
      recv_list_commands_resp(llid, tid, qty, list_commands);
      if (qty)
        clownix_free(list_commands, __FUNCTION__);
      break;

    case bnd_work_dir_req:
      if (sscanf(msg, WORK_DIR_REQ, &tid) != 1)
        KOUT("%s", msg);
      recv_work_dir_req(llid, tid);
      break;
    case bnd_work_dir_resp:
      memset(&icf, 0, sizeof(t_topo_clc));
      if (sscanf(msg, WORK_DIR_RESP, &tid, 
                                     icf.version,
                                     icf.network,
                                     icf.username,
                                     &(icf.server_port),
                                     icf.bin_dir,
                                     &(icf.flags_config)) != 7)
        KOUT("%s", msg);
      topo_config_swapoff(&cf, &icf);
      recv_work_dir_resp(llid, tid, &cf);
      break;

    case bnd_vmcmd:
      if (sscanf(msg, VMCMD, &tid, name, &vmcmd, &param) != 4)
        KOUT("%s", msg);
      recv_vmcmd(llid, tid, name, vmcmd, param);
      break;

    case bnd_eventfull_sub:
      if (sscanf(msg, EVENTFULL_SUB, &tid) != 1)
        KOUT("%s", msg);
      recv_eventfull_sub(llid, tid);
      break;

    case bnd_eventfull:
      if (sscanf(msg, EVENTFULL_O, &tid, &nb) != 2)
        KOUT("%s", msg);
      len = nb * sizeof(t_eventfull_endp);
      eventfull_endp = (t_eventfull_endp *)clownix_malloc(len, 23); 
      memset(eventfull_endp, 0, len);
      helper_eventfull_endp(msg, nb, eventfull_endp);
      recv_eventfull(llid, tid, nb, eventfull_endp); 
      clownix_free(eventfull_endp, __FUNCTION__);
      break;

    case bnd_slowperiodic_sub:
      if (sscanf(msg, SLOWPERIODIC_SUB, &tid) != 1)
        KOUT("%s", msg);
      recv_slowperiodic_sub(llid, tid);
      break;

    case bnd_slowperiodic_qcow2:
      if (sscanf(msg, SLOWPERIODIC_QCOW2_O, &tid, &nb) != 2)
        KOUT("%s", msg);
      len = nb * sizeof(t_slowperiodic);
      slowperiodic = (t_slowperiodic *)clownix_malloc(len, 23);
      memset(slowperiodic, 0, len);
      helper_slowperiodic(msg, nb, slowperiodic);
      recv_slowperiodic_qcow2(llid, tid, nb, slowperiodic);
      clownix_free(slowperiodic, __FUNCTION__);
      break;

    case bnd_slowperiodic_img:
      if (sscanf(msg, SLOWPERIODIC_IMG_O, &tid, &nb) != 2)
        KOUT("%s", msg);
      len = nb * sizeof(t_slowperiodic);
      slowperiodic = (t_slowperiodic *)clownix_malloc(len, 23);
      memset(slowperiodic, 0, len);
      helper_slowperiodic(msg, nb, slowperiodic);
      recv_slowperiodic_img(llid, tid, nb, slowperiodic);
      clownix_free(slowperiodic, __FUNCTION__);
      break;

    case bnd_slowperiodic_cvm: 
      if (sscanf(msg, SLOWPERIODIC_CVM_O, &tid, &nb) != 2)
        KOUT("%s", msg);
      len = nb * sizeof(t_slowperiodic);
      slowperiodic = (t_slowperiodic *)clownix_malloc(len, 23);
      memset(slowperiodic, 0, len);
      helper_slowperiodic(msg, nb, slowperiodic);
      recv_slowperiodic_cvm(llid, tid, nb, slowperiodic);
      clownix_free(slowperiodic, __FUNCTION__);
      break;

    case bnd_c2c_add:
      if (sscanf(msg, C2C_ADD, &tid, name, &ludp_ip,
                               network, &ip, &dport,
                               param1, &dudp_ip, &c2c_udp_port_low) != 9)
        KOUT("%s", msg);
      recv_c2c_add(llid, tid, name, ludp_ip, network, ip,
                   dport, param1, dudp_ip, c2c_udp_port_low);
      break;

    case bnd_snf_add:
      if (sscanf(msg, SNF_ADD, &tid, name, &num, &val) != 4)
        KOUT("%s", msg);
      recv_snf_add(llid, tid, name, num, val);
      break;

    case bnd_c2c_peer_create:
      if (sscanf(msg, C2C_CREATE, &tid, name, &status, lnet, dnet) != 5)
        KOUT("%s", msg);
      recv_c2c_peer_create(llid, tid, name, status, lnet, dnet);
      break;

    case bnd_c2c_peer_conf:
      if (sscanf(msg, C2C_CONF, &tid, name, &status, lnet, dnet,
                                &ludp_ip, &dudp_ip, &lport, &dport) != 9)
        KOUT("%s", msg);
      recv_c2c_peer_conf(llid, tid, name, status, lnet, dnet,
                         ludp_ip, dudp_ip, lport, dport);
      break;

    case bnd_c2c_peer_ping:
      if (sscanf(msg, C2C_PING, &tid, name, &status) != 3)
        KOUT("%s", msg);
      recv_c2c_peer_ping(llid, tid, name, status);
      break;

    case bnd_color_item:
      if (sscanf(msg, COLOR_ITEM, &tid, name, &num) != 3)
        KOUT("%s", msg);
      recv_color_item(llid, tid, name, num);
      break;

    case bnd_novnc_on_off:
      if (sscanf(msg, NOVNC_ON_OFF, &tid, &num) != 2)
        KOUT("%s", msg);
      recv_novnc_on_off(llid, tid, num);
      break;

    case bnd_tap_add:
      if (sscanf(msg, TAP_ADD, &tid, name) != 2)
        KOUT("%s", msg);
      recv_tap_add(llid, tid, name);
      break;

    case bnd_cnt_add:
      memset(&cnt, 0, sizeof(t_topo_cnt));
      if (sscanf(msg, ADD_CNT_O, &tid, cnt.brandtype, cnt.name,
                 &(cnt.is_persistent), &(cnt.is_privileged),
                 &(cnt.vm_id), &(cnt.color),
                 &(cnt.ping_ok), &(cnt.nb_tot_eth)) != 9)
        KOUT("%s", msg);
      get_eth_table(msg, cnt.nb_tot_eth, cnt.eth_table);
      startup_env_get(msg, &cnt);
      vmount_get(msg, &cnt);
      recv_cnt_add(llid, tid, &cnt);
      break;

    case bnd_nat_add:
      if (sscanf(msg, NAT_ADD, &tid, name) != 2)
        KOUT("%s", msg);
      recv_nat_add(llid, tid, name);
      break;

    case bnd_a2b_add:
      if (sscanf(msg, A2B_ADD, &tid, name) != 2)
        KOUT("%s", msg);
      recv_a2b_add(llid, tid, name);
      break;

    case bnd_phy_add:
      if (sscanf(msg, PHY_ADD, &tid, name, &type) != 3)
        KOUT("%s", msg);
      recv_phy_add(llid, tid, name, type);
      break;

    case bnd_xyx_cnf:
      if (sscanf(msg, C2C_CNF, &tid, name, info) != 3)
        KOUT("%s", msg);
      recv_c2c_cnf(llid, tid, name, info);
      break;

    case bnd_nat_cnf:
       if (sscanf(msg, NAT_CNF, &tid, name, info) != 3)
         KOUT("%s", msg);
       recv_nat_cnf(llid, tid, name, info);
       break;

    case bnd_a2b_cnf:
       if (sscanf(msg, A2B_CNF_BIS, &tid, name) != 2)
         KOUT("%s", msg);
       line = strstr(msg, "<cmd>");
       if (!line)
         KOUT("%s", msg);
       line += strlen("<cmd>");
       ptr = strstr(line, "</cmd>");
       if (!ptr)
         KOUT("%s", msg);
       *ptr = 0;
       recv_a2b_cnf(llid, tid, name, line);
       break;

    case bnd_lan_cnf:
       if (sscanf(msg, LAN_CNF, &tid, name, info) != 3)
         KOUT("%s", msg);
       recv_lan_cnf(llid, tid, name, info);
       break;

    case bnd_sync_wireshark_req:
      if (sscanf(msg, SYNC_WIRESHARK_REQ, &tid, name, &num, &cmd) != 4)
         KOUT("%s", msg);
      recv_sync_wireshark_req(llid, tid, name, num, cmd);
      break;

    case bnd_sync_wireshark_resp:
      if (sscanf(msg, SYNC_WIRESHARK_RESP, &tid, name, &num, &status) != 4)
         KOUT("%s", msg);
      recv_sync_wireshark_resp(llid, tid, name, num, status);
      break;


    default:
      KOUT("%s", msg);
    }
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
char *llid_trace_lib(int type)
{
  char *result;
  switch(type)
    {
    case type_llid_trace_client_unix:
      result = "client";
      break;
    case type_llid_trace_listen_client_unix:
      result = "listen client server";
      break;
    case type_llid_trace_clone:
      result = " ";
      break;
    case type_llid_trace_listen_clone:
      result = "clone process server";
      break;
    case type_llid_trace_endp_kvm:
      result = "kvm";
      break;
    case type_llid_trace_endp_tap:
      result = "tap";
      break;
    case type_llid_trace_endp_ovs:
      result = "ovs";
      break;
    case type_llid_trace_endp_ovsdb:
      result = "ovsdb";
      break;
    case type_llid_trace_jfs:
      result = "jfs";
      break;
    case type_llid_trace_unix_xwy:
      result = "unix xwy control";
      break;
    case type_llid_trace_unix_proxy:
      result = "unix udp proxy";
      break;
    case type_llid_trace_doorways:
      result = "doorways";
      break;
    case type_llid_trace_proxymous:
      result = "proxymous";
      break;
    case type_llid_trace_unix_qmp:
      result = "unix_qmp";
      break;
    case type_llid_trace_endp_suid:
      result = "suid";
      break;
    default:
      KOUT("Error llid type %d", type);
      break;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *prop_flags_ascii_get(int prop_flags)
{
  static char resp[500];
  memset(resp, 0, 500);

  if (prop_flags & VM_CONFIG_FLAG_PERSISTENT)
    strcat(resp, "persistent_write_rootfs ");
  else
    strcat(resp, "evanescent_write_rootfs ");

  if (prop_flags & VM_CONFIG_FLAG_PRIVILEGED)
    strcat(resp, "privileged ");
  else
    strcat(resp, "not privileged ");

  return resp;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int doors_io_basic_decoder (int llid, int len, char *chunk)
{
  int result = -1;
  int bnd_event;
  char bound[MAX_CLOWNIX_BOUND_LEN];
  if ((size_t) len != strlen(chunk) + 1)
    KOUT(" %d %d %s\n", len, (int)strlen(chunk), chunk);
  extract_boundary(chunk, bound);
  bnd_event = get_bnd_event(bound);
  if (bnd_event)
    {
    dispatcher(llid, bnd_event, chunk);
    result = 0;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void doors_io_basic_xml_init(t_llid_tx llid_tx)
{
  g_llid_tx = llid_tx;
  if (!g_llid_tx)
    KOUT(" ");
  sndbuf = get_bigbuf();
  memset (bound_list, 0, bnd_max * MAX_CLOWNIX_BOUND_LEN);
  extract_boundary(STATUS_OK,      bound_list[bnd_status_ok]);
  extract_boundary(STATUS_KO,      bound_list[bnd_status_ko]);
  extract_boundary(FIX_DISPLAY,    bound_list[bnd_fix_display]);
  extract_boundary(ADD_CNT_O,      bound_list[bnd_cnt_add]);
  extract_boundary(ADD_KVM_O,      bound_list[bnd_add_vm]);
  extract_boundary(SAV_VM,         bound_list[bnd_sav_vm]);
  extract_boundary(DEL_SAT,        bound_list[bnd_del_name]);
  extract_boundary(ADD_LAN_ENDP, bound_list[bnd_add_lan_endp]);
  extract_boundary(DEL_LAN_ENDP, bound_list[bnd_del_lan_endp]);
  extract_boundary(KILL_UML_CLOWNIX,bound_list[bnd_kill_uml_clownix]);
  extract_boundary(DEL_ALL,        bound_list[bnd_del_all]);
  extract_boundary(LIST_PID,       bound_list[bnd_list_pid_req]);
  extract_boundary(LIST_PID_O,     bound_list[bnd_list_pid_resp]);
  extract_boundary(LIST_COMMANDS,   bound_list[bnd_list_commands_req]);
  extract_boundary(LIST_COMMANDS_O, bound_list[bnd_list_commands_resp]);
  extract_boundary(TOPO_SMALL_EVENT_SUB, bound_list[bnd_topo_small_event_sub]);
  extract_boundary(TOPO_SMALL_EVENT_UNSUB,
                   bound_list[bnd_topo_small_event_unsub]);
  extract_boundary(TOPO_SMALL_EVENT,   bound_list[bnd_topo_small_event]);
  extract_boundary(EVENT_TOPO_SUB, bound_list[bnd_event_topo_sub]);
  extract_boundary(EVENT_TOPO_UNSUB, bound_list[bnd_event_topo_unsub]);
  extract_boundary(EVENT_TOPO_O,   bound_list[bnd_event_topo]);
  extract_boundary(EVENT_PRINT_SUB,bound_list[bnd_evt_print_sub]);
  extract_boundary(EVENT_PRINT_UNSUB,bound_list[bnd_evt_print_unsub]);
  extract_boundary(EVENT_PRINT,    bound_list[bnd_event_print]);
  extract_boundary(EVENT_SYS_SUB, bound_list[bnd_event_sys_sub]);
  extract_boundary(EVENT_SYS_UNSUB, bound_list[bnd_event_sys_unsub]);
  extract_boundary(EVENT_SYS_O, bound_list[bnd_event_sys]);
  extract_boundary(WORK_DIR_REQ,  bound_list[bnd_work_dir_req]);
  extract_boundary(WORK_DIR_RESP, bound_list[bnd_work_dir_resp]);
  extract_boundary(VMCMD, bound_list[bnd_vmcmd]);
  extract_boundary(EVENTFULL_SUB, bound_list[bnd_eventfull_sub]);
  extract_boundary(EVENTFULL_O, bound_list[bnd_eventfull]);
  extract_boundary(SLOWPERIODIC_SUB, bound_list[bnd_slowperiodic_sub]);
  extract_boundary(SLOWPERIODIC_QCOW2_O, bound_list[bnd_slowperiodic_qcow2]);
  extract_boundary(SLOWPERIODIC_IMG_O, bound_list[bnd_slowperiodic_img]);
  extract_boundary(SLOWPERIODIC_CVM_O, bound_list[bnd_slowperiodic_cvm]);
  extract_boundary(SUB_EVT_STATS_ENDP, bound_list[bnd_sub_evt_stats_endp]);
  extract_boundary(EVT_STATS_ENDP_O, bound_list[bnd_evt_stats_endp]);
  extract_boundary(SUB_EVT_STATS_SYSINFO,bound_list[bnd_sub_evt_stats_sysinfo]);
  extract_boundary(EVT_STATS_SYSINFOO, bound_list[bnd_evt_stats_sysinfo]);

  extract_boundary(HOP_GET_LIST_NAME, bound_list[bnd_hop_get_list]);
  extract_boundary(HOP_LIST_NAME_O, bound_list[bnd_hop_list]);
  extract_boundary(HOP_EVT_DOORS_SUB_O, bound_list[bnd_hop_evt_doors_sub]);
  extract_boundary(HOP_EVT_DOORS_UNSUB, bound_list[bnd_hop_evt_doors_unsub]);
  extract_boundary(HOP_EVT_DOORS_O,   bound_list[bnd_hop_evt_doors]);

  extract_boundary(SNF_ADD, bound_list[bnd_snf_add]);
  extract_boundary(C2C_ADD, bound_list[bnd_c2c_add]);
  extract_boundary(C2C_CREATE, bound_list[bnd_c2c_peer_create]);
  extract_boundary(C2C_CONF, bound_list[bnd_c2c_peer_conf]);
  extract_boundary(C2C_PING, bound_list[bnd_c2c_peer_ping]);

  extract_boundary(COLOR_ITEM, bound_list[bnd_color_item]);
  extract_boundary(NOVNC_ON_OFF, bound_list[bnd_novnc_on_off]);

  extract_boundary(NAT_ADD, bound_list[bnd_nat_add]);
  extract_boundary(PHY_ADD, bound_list[bnd_phy_add]);
  extract_boundary(TAP_ADD, bound_list[bnd_tap_add]);

  extract_boundary(A2B_ADD, bound_list[bnd_a2b_add]);
  extract_boundary(A2B_CNF, bound_list[bnd_a2b_cnf]);
  extract_boundary(LAN_CNF, bound_list[bnd_lan_cnf]);
  extract_boundary(C2C_CNF, bound_list[bnd_xyx_cnf]);
  extract_boundary(NAT_CNF, bound_list[bnd_nat_cnf]);

  extract_boundary(SYNC_WIRESHARK_REQ, bound_list[bnd_sync_wireshark_req]);
  extract_boundary(SYNC_WIRESHARK_RESP, bound_list[bnd_sync_wireshark_resp]);
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void doors_io_basic_tx_set(t_llid_tx llid_tx)
{
  g_llid_tx = llid_tx;
}
/*---------------------------------------------------------------------------*/





