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
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <libgen.h>



#include "io_clownix.h"
#include "ovs_execv.h"
#include "ovs_drv.h"

void clean_all_upon_error(void);
char *get_ovs_bin(void);
char *get_dpdk_dir(void);
int get_ovsdb_pid(void);
void set_ovsdb_pid(int pid);
int get_ovs_pid(void);
void set_ovs_pid(int pid);
char *get_net_name(void);
int get_ovs_launched(void);
int get_ovsdb_launched(void);
void set_ovs_launched(int val);
void set_ovsdb_launched(int val);
void unlink_files(char *dpdk_dir);


/*****************************************************************************/
static int check_uid(void)
{
  int result = -1;
  uid_t prev_uid, uid;
  prev_uid = geteuid();
  seteuid(0);
  uid = geteuid();
  if (uid == 0)
    result = 0;
  seteuid(prev_uid);
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void action_add_lan_pci_dpdk(char *respb, char *lan, char *pci)
{
  char *bin = get_ovs_bin();
  char *db = get_dpdk_dir();
  if (ovs_execv_add_pci_dpdk(bin, db, lan, pci))
    {
    snprintf(respb, MAX_PATH_LEN-1,
    "KO cloonixovs_add_lan_pci_dpdk lan=%s pci=%s", lan, pci);
    KERR("%s", respb);
    }
  else
    {
    snprintf(respb, MAX_PATH_LEN-1,
    "OK cloonixovs_add_lan_pci_dpdk lan=%s pci=%s", lan, pci);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void action_del_lan_pci_dpdk(char *respb, char *lan, char *pci)
{
  char *bin = get_ovs_bin();
  char *db = get_dpdk_dir();
  if (ovs_execv_del_pci_dpdk(bin, db, lan, pci))
    {
    snprintf(respb, MAX_PATH_LEN-1,
    "KO cloonixovs_del_lan_pci_dpdk lan=%s pci=%s", lan, pci);
    KERR("%s", respb);
    }
  else
    {
    snprintf(respb, MAX_PATH_LEN-1,
    "OK cloonixovs_del_lan_pci_dpdk lan=%s pci=%s", lan, pci);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void action_add_lan_phy_port(char *respb, char *lan, char *name)
{
  char *bin = get_ovs_bin();
  char *db = get_dpdk_dir();
  if (ovs_execv_add_phy_port(bin, db, lan, name))
    {
    snprintf(respb, MAX_PATH_LEN-1,
    "KO cloonixovs_add_lan_phy_port lan=%s phy=%s", lan, name);
    KERR("%s", respb);
    }
  else
    {
    snprintf(respb, MAX_PATH_LEN-1,
    "OK cloonixovs_add_lan_phy_port lan=%s phy=%s", lan, name);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void action_del_lan_phy_port(char *respb, char *lan, char *name)
{
  char *bin = get_ovs_bin();
  char *db = get_dpdk_dir();
  if (ovs_execv_del_phy_port(bin, db, lan, name))
    {
    snprintf(respb, MAX_PATH_LEN-1,
    "KO cloonixovs_del_lan_phy_port lan=%s phy=%s", lan, name);
    KERR("%s", respb);
    }
  else
    {
    snprintf(respb, MAX_PATH_LEN-1,
    "OK cloonixovs_del_lan_phy_port lan=%s phy=%s", lan, name);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void action_add_eth_br(char *respb, char *name, int num, char *mac)
{
  char *bin = get_ovs_bin();
  char *db = get_dpdk_dir();
  if (ovs_execv_add_eth(bin, db, name, num))
    {
    snprintf(respb, MAX_PATH_LEN-1,
             "KO cloonixovs_add_eth name=%s num=%d", name, num);
    KERR("%s", respb);
    }
  else
    {
    snprintf(respb, MAX_PATH_LEN-1,
             "OK cloonixovs_add_eth name=%s num=%d", name, num);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void action_del_eth_br(char *respb, char *name, int num)
{
  char *bin = get_ovs_bin();
  char *db = get_dpdk_dir();
  if (ovs_execv_del_eth(bin, db, name, num))
    {
    snprintf(respb, MAX_PATH_LEN-1,
             "KO cloonixovs_del_eth name=%s num=%d", name, num);
    KERR("%s", respb);
    }
  else
    {
    snprintf(respb, MAX_PATH_LEN-1,
             "OK cloonixovs_del_eth name=%s num=%d", name, num);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void action_add_lan_br(char *respb, char *lan)
{
  char *bin = get_ovs_bin();
  char *db = get_dpdk_dir();
  if (ovs_execv_add_lan_br(bin, db, lan))
    {
    snprintf(respb, MAX_PATH_LEN-1, "KO cloonixovs_add_lan_br lan=%s", lan);
    KERR("%s", respb);
    }
  else
    {
    snprintf(respb, MAX_PATH_LEN-1, "OK cloonixovs_add_lan_br lan=%s", lan);
    }
  usleep(100000);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void action_del_lan_br(char *respb, char *lan)
{
  char *bin = get_ovs_bin();
  char *db = get_dpdk_dir();
  if (ovs_execv_del_lan_br(bin, db, lan))
    {
    snprintf(respb, MAX_PATH_LEN-1, "KO cloonixovs_del_lan_br lan=%s", lan);
    KERR("%s", respb);
    }
  else
    snprintf(respb, MAX_PATH_LEN-1, "OK cloonixovs_del_lan_br lan=%s", lan);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void action_add_lan_snf_br(char *respb, char *lan, char *name)
{
  char *bin = get_ovs_bin();
  char *db = get_dpdk_dir();
  if (ovs_execv_add_lan_snf(bin, db, lan, name))
    {
    snprintf(respb, MAX_PATH_LEN-1,
             "KO cloonixovs_add_lan_snf lan=%s name=%s", lan, name);
    KERR("%s", respb);
    }
  else
    {
    snprintf(respb, MAX_PATH_LEN-1,
             "OK cloonixovs_add_lan_snf lan=%s name=%s", lan, name);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void action_del_lan_snf_br(char *respb, char *lan, char *name)
{
  char *bin = get_ovs_bin();
  char *db = get_dpdk_dir();
  if (ovs_execv_del_lan_snf(bin, db, lan, name))
    {
    snprintf(respb, MAX_PATH_LEN-1,
             "KO cloonixovs_del_lan_snf lan=%s name=%s", lan, name);
    KERR("%s", respb);
    }
  else
    {
    snprintf(respb, MAX_PATH_LEN-1,
             "OK cloonixovs_del_lan_snf lan=%s name=%s", lan, name);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void action_add_lan_nat_br(char *respb, char *lan, char *name)
{
  char *bin = get_ovs_bin();
  char *db = get_dpdk_dir();
  if (ovs_execv_add_lan_nat(bin, db, lan, name))
    {
    snprintf(respb, MAX_PATH_LEN-1,
             "KO cloonixovs_add_lan_nat lan=%s name=%s", lan, name);
    KERR("%s", respb);
    }
  else
    {
    snprintf(respb, MAX_PATH_LEN-1,
             "OK cloonixovs_add_lan_nat lan=%s name=%s", lan, name);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void action_del_lan_nat_br(char *respb, char *lan, char *name)
{
  char *bin = get_ovs_bin();
  char *db = get_dpdk_dir();
  if (ovs_execv_del_lan_nat(bin, db, lan, name))
    {
    snprintf(respb, MAX_PATH_LEN-1,
             "KO cloonixovs_del_lan_nat lan=%s name=%s", lan, name);
    KERR("%s", respb);
    }
  else
    {
    snprintf(respb, MAX_PATH_LEN-1,
             "OK cloonixovs_del_lan_nat lan=%s name=%s", lan, name);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void action_add_lan_d2d_br(char *respb, char *lan, char *name)
{
  char *bin = get_ovs_bin();
  char *db = get_dpdk_dir();
  if (ovs_execv_add_lan_d2d(bin, db, lan, name))
    {
    snprintf(respb, MAX_PATH_LEN-1,
             "KO cloonixovs_add_lan_d2d lan=%s name=%s", lan, name);
    KERR("%s", respb);
    }
  else
    {
    snprintf(respb, MAX_PATH_LEN-1,
             "OK cloonixovs_add_lan_d2d lan=%s name=%s", lan, name);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void action_del_lan_d2d_br(char *respb, char *lan, char *name)
{
  char *bin = get_ovs_bin();
  char *db = get_dpdk_dir();
  if (ovs_execv_del_lan_d2d(bin, db, lan, name))
    {
    snprintf(respb, MAX_PATH_LEN-1,
             "KO cloonixovs_del_lan_d2d lan=%s name=%s", lan, name);
    KERR("%s", respb);
    }
  else
    {
    snprintf(respb, MAX_PATH_LEN-1,
             "OK cloonixovs_del_lan_d2d lan=%s name=%s", lan, name);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void action_add_lan_a2b_br(char *respb, char *lan, char *name)
{
  char *bin = get_ovs_bin();
  char *db = get_dpdk_dir();
  if (ovs_execv_add_lan_a2b(bin, db, lan, name))
    {
    snprintf(respb, MAX_PATH_LEN-1,
             "KO cloonixovs_add_lan_a2b lan=%s name=%s", lan, name);
    KERR("%s", respb);
    }
  else
    {
    snprintf(respb, MAX_PATH_LEN-1,
             "OK cloonixovs_add_lan_a2b lan=%s name=%s", lan, name);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void action_del_lan_a2b_br(char *respb, char *lan, char *name)
{
  char *bin = get_ovs_bin();
  char *db = get_dpdk_dir();
  if (ovs_execv_del_lan_a2b(bin, db, lan, name))
    {
    snprintf(respb, MAX_PATH_LEN-1,
             "KO cloonixovs_del_lan_a2b lan=%s name=%s", lan, name);
    KERR("%s", respb);
    }
  else
    {
    snprintf(respb, MAX_PATH_LEN-1,
             "OK cloonixovs_del_lan_a2b lan=%s name=%s", lan, name);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void action_add_lan_b2a_br(char *respb, char *lan, char *name)
{
  char *bin = get_ovs_bin();
  char *db = get_dpdk_dir();
  if (ovs_execv_add_lan_b2a(bin, db, lan, name))
    {
    snprintf(respb, MAX_PATH_LEN-1,
             "KO cloonixovs_add_lan_b2a lan=%s name=%s", lan, name);
    KERR("%s", respb);
    }
  else
    {
    snprintf(respb, MAX_PATH_LEN-1,
             "OK cloonixovs_add_lan_b2a lan=%s name=%s", lan, name);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void action_del_lan_b2a_br(char *respb, char *lan, char *name)
{
  char *bin = get_ovs_bin();
  char *db = get_dpdk_dir();
  if (ovs_execv_del_lan_b2a(bin, db, lan, name))
    {
    snprintf(respb, MAX_PATH_LEN-1,
             "KO cloonixovs_del_lan_b2a lan=%s name=%s", lan, name);
    KERR("%s", respb);
    }
  else
    {
    snprintf(respb, MAX_PATH_LEN-1,
             "OK cloonixovs_del_lan_b2a lan=%s name=%s", lan, name);
    }
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void action_add_lan_eth_br(char *respb, char *lan, char *name, int num)
{
  char *bin = get_ovs_bin();
  char *db = get_dpdk_dir();
  if (ovs_execv_add_lan_eth(bin, db, lan, name, num))
    {
    snprintf(respb, MAX_PATH_LEN-1,
             "KO cloonixovs_add_lan_eth lan=%s name=%s num=%d",
             lan, name, num);
    KERR("%s", respb);
    }
  else
    {
    snprintf(respb, MAX_PATH_LEN-1,
             "OK cloonixovs_add_lan_eth lan=%s name=%s num=%d",
             lan, name, num);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void action_del_lan_eth_br(char *respb, char *lan, char *name, int num)
{
  char *bin = get_ovs_bin();
  char *db = get_dpdk_dir();
  if (ovs_execv_del_lan_eth(bin, db, lan, name, num))
    {
    snprintf(respb, MAX_PATH_LEN-1,
             "KO cloonixovs_del_lan_eth lan=%s name=%s num=%d",
             lan, name, num);
    KERR("%s", respb);
    }
  else
    {
    snprintf(respb, MAX_PATH_LEN-1,
             "OK cloonixovs_del_lan_eth lan=%s name=%s num=%d",
             lan, name, num);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void action_add_tap_br(char *respb, char *name)
{
  char *bin = get_ovs_bin();
  char *db = get_dpdk_dir();
  char mac[MAC_ADDR_LEN];
  if (ovs_execv_add_tap(bin, db, name))
    {
    snprintf(respb, MAX_PATH_LEN-1, "KO cloonixovs_add_tap name=%s", name);
    KERR("%s", respb);
    }
  else if (ovs_execv_get_tap_mac(name, mac))
    {
    snprintf(respb, MAX_PATH_LEN-1, "KO cloonixovs_add_tap name=%s", name);
    KERR("%s", respb);
    }
  else
    {
    snprintf(respb, MAX_PATH_LEN-1,
    "OK cloonixovs_add_tap name=%s mac=%02x:%02x:%02x:%02x:%02x:%02x",
                           name, mac[0]&0xff, mac[1]&0xff, mac[2]&0xff,
                                 mac[3]&0xff, mac[4]&0xff, mac[5]&0xff);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void action_del_tap_br(char *respb, char *name)
{
  char *bin = get_ovs_bin();
  char *db = get_dpdk_dir();
  if (ovs_execv_del_tap(bin, db, name))
    {
    snprintf(respb, MAX_PATH_LEN-1, "KO cloonixovs_del_tap name=%s", name);
    }
  else
    {
    snprintf(respb, MAX_PATH_LEN-1, "OK cloonixovs_del_tap name=%s", name);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void action_add_lan_tap_br(char *respb, char *lan, char *name)
{
  char *bin = get_ovs_bin();
  char *db = get_dpdk_dir();
  if (ovs_execv_add_lan_tap(bin, db, lan, name))
    {
    snprintf(respb, MAX_PATH_LEN-1,
             "KO cloonixovs_add_lan_tap lan=%s name=%s", lan, name);
    KERR("%s", respb);
    }
  else
    snprintf(respb, MAX_PATH_LEN-1,
             "OK cloonixovs_add_lan_tap lan=%s name=%s", lan, name);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void action_del_lan_tap_br(char *respb, char *lan, char *name)
{
  char *bin = get_ovs_bin();
  char *db = get_dpdk_dir();
  if (ovs_execv_del_lan_tap(bin, db, lan, name))
    {
    snprintf(respb, MAX_PATH_LEN-1,
             "KO cloonixovs_del_lan_tap lan=%s name=%s", lan, name);
    }
  else
    snprintf(respb, MAX_PATH_LEN-1,
             "OK cloonixovs_del_lan_tap lan=%s name=%s", lan, name);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int action_add_eth_req(char *line, char *name, int *num, char *mac)
{
  int i, result = 0;
  char *ptr = line;
  int mc[MAC_ADDR_LEN];
  if (!strncmp("cloonixovs_add_eth", line, strlen("cloonixovs_add_eth")))
    {
    if (sscanf(line, "cloonixovs_add_eth name=%s num=%d", name, num) == 2)
      {
      result = 1;
      ptr = strstr(ptr, "mac");
      if (ptr == NULL)
        {
        result = 0;
        KERR("NOT ENOUGH MAC FOR DPDK ETH %s %d", name, *num);
        }
      else if (sscanf(ptr, "mac=%02X:%02X:%02X:%02X:%02X:%02X",
                           &(mc[0]), &(mc[1]), &(mc[2]),
                           &(mc[3]), &(mc[4]), &(mc[5])) != 6)
        {
        result = 0;
        KERR("BAD MAC FORMAT DPDK ETH %s %d", name, *num);
        }
      else
        {
        for (i=0; i<MAC_ADDR_LEN; i++)
          mac[i] = mc[i] & 0xFF;
        }
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void action_req_ovsdb(char *bin, char *db, char *respb,
                            uint32_t lcore_mask, uint32_t socket_mem,
                            uint32_t cpu_mask)
{
  int pid;
  int ovsdb_pid = get_ovsdb_pid();
  char *net = get_net_name();
  int ovsdb_launched = get_ovsdb_launched();
  if (!dpdk_is_usable())
    {
    KERR("ERROR DPDK NOT USABLE");
    snprintf(respb, MAX_PATH_LEN-1, "cloonixovs_resp_ovsdb_ko");
    }
  else if (ovsdb_launched == 0)
    {
    set_ovsdb_launched(1);
    unlink_files(db);
    if (create_ovsdb_server_conf(bin, db))
      {
      KERR("ERROR CREATION DB");
      snprintf(respb, MAX_PATH_LEN-1, "cloonixovs_resp_ovsdb_ko");
      clean_all_upon_error();
      }
    else
      {
      sync();
      usleep(5000);
      pid = ovs_execv_ovsdb_server(net,bin,db,lcore_mask,socket_mem,cpu_mask);
      if (pid <= 0)
        {
        KERR("ERROR CREATION DB SERVER");
        snprintf(respb, MAX_PATH_LEN-1, "cloonixovs_resp_ovsdb_ko");
        clean_all_upon_error();
        }
      else
        {
        snprintf(respb, MAX_PATH_LEN-1, "cloonixovs_resp_ovsdb_ok");
        set_ovsdb_pid(pid);
        }
      }
    }
  else
    {
    if (ovsdb_pid > 0)
      {
      snprintf(respb, MAX_PATH_LEN-1, "cloonixovs_resp_ovsdb_ok");
      }
    else
      {
      KERR("ERROR DB SERVER");
      snprintf(respb, MAX_PATH_LEN-1, "cloonixovs_resp_ovsdb_ko");
      clean_all_upon_error();
      }
    }
  sync();
  usleep(5000);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void action_req_ovs_switch(char *bin, char *db, char *respb,
                                  uint32_t lcore_mask, uint32_t socket_mem,
                                  uint32_t cpu_mask)
{
  int pid;
  int ovs_pid = get_ovs_pid();
  char *net = get_net_name();
  int ovs_launched = get_ovs_launched();
  if (!dpdk_is_usable())
    {
    snprintf(respb, MAX_PATH_LEN-1, "cloonixovs_resp_ovs_ko");
    }
  else if (ovs_launched == 0)
    {
    set_ovs_launched(1);
    pid = ovs_execv_ovs_vswitchd(net,bin,db,lcore_mask,socket_mem,cpu_mask);
    if (pid <= 0)
      {
      KERR("BAD START OVS");
      clean_all_upon_error();
      snprintf(respb, MAX_PATH_LEN-1, "cloonixovs_resp_ovs_ko");
      }
    else
      {
      snprintf(respb, MAX_PATH_LEN-1, "cloonixovs_resp_ovs_ok");
      set_ovs_pid(pid);
      }
    }
  else
    {
    if (ovs_pid > 0)
      {
      KERR("OVS LAUNCH ALREADY DONE");
      snprintf(respb, MAX_PATH_LEN-1, "cloonixovs_resp_ovs_ok");
      }
    else
      {
      KERR("OVS LAUNCH ALREADY TRIED");
      snprintf(respb, MAX_PATH_LEN-1, "cloonixovs_resp_ovs_ko");
      clean_all_upon_error();
      }
    }
  sync();
  usleep(5000);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void action_req_suidroot(char *respb)
{
  ovs_execv_init();
  if (check_uid())
    snprintf(respb, MAX_PATH_LEN-1, "cloonixovs_resp_suidroot_ko");
  else
    snprintf(respb, MAX_PATH_LEN-1, "cloonixovs_resp_suidroot_ok");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void action_req_destroy(void)
{
  char *db = get_dpdk_dir();
  int ovsdb_pid = get_ovsdb_pid();
  int ovs_pid = get_ovs_pid();
  if (ovs_pid > 0)
    kill(ovs_pid, SIGKILL);
  if (ovsdb_pid > 0)
    kill(ovsdb_pid, SIGKILL);
  usleep(10000);
  unlink_dir(db);
  sync();
  exit(0);
}
/*---------------------------------------------------------------------------*/

