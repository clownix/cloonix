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
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <libgen.h>



#include "io_clownix.h"
#include "ovs_cmd.h"
#include "ovs_execv.h"
#include "ovs_drv.h"


static int g_ovs_launched;
static int g_ovsdb_launched;


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
void action_vhost_up(char *bin, char *db, char *respb,
                     char *name, int num, char *vhost)
{
  if (ovs_cmd_vhost_up(bin, db, name, num, vhost))
    {
    snprintf(respb, MAX_PATH_LEN-1,
             "KO ovs_vhost_up name=%s num=%d vhost=%s", name,num,vhost);
    }
  else
    {
    snprintf(respb, MAX_PATH_LEN-1,
             "OK ovs_vhost_up name=%s num=%d vhost=%s", name,num,vhost);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void action_add_snf_lan(char *bin, char *db, char *respb,
                        char *name, int num, char *vhost, char *lan)
{
  if (ovs_cmd_add_snf_lan(bin, db, name, num, vhost, lan))
    {
    snprintf(respb, MAX_PATH_LEN-1,
             "KO ovs_add_snf_lan name=%s num=%d vhost=%s lan=%s",
             name, num, vhost, lan);
    }
  else
    {
    snprintf(respb, MAX_PATH_LEN-1,
             "OK ovs_add_snf_lan name=%s num=%d vhost=%s lan=%s",
             name, num, vhost, lan);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void action_del_snf_lan(char *bin, char *db, char *respb,
                        char *name, int num, char *vhost, char *lan)
{
  if (ovs_cmd_del_snf_lan(bin, db, name, num, vhost, lan))
    {
    snprintf(respb, MAX_PATH_LEN-1,
             "KO ovs_del_snf_lan name=%s num=%d vhost=%s lan=%s",
             name, num, vhost, lan);
    }
  else
    {
    snprintf(respb, MAX_PATH_LEN-1,
             "OK ovs_del_snf_lan name=%s num=%d vhost=%s lan=%s",
             name, num, vhost, lan);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void action_add_lan(char *bin, char *db, char *respb, char *lan)
{
  if (ovs_cmd_add_lan(bin, db, lan))
    {
    snprintf(respb, MAX_PATH_LEN-1, "KO ovs_add_lan lan=%s", lan);
    KERR("%s", respb);
    }
  else
    {
    snprintf(respb, MAX_PATH_LEN-1, "OK ovs_add_lan lan=%s", lan);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void action_del_lan(char *bin, char *db, char *respb, char *lan)
{
  if (ovs_cmd_del_lan(bin, db, lan))
    {
    snprintf(respb, MAX_PATH_LEN-1, "KO ovs_del_lan lan=%s", lan);
    KERR("%s", respb);
    }
  else
    snprintf(respb, MAX_PATH_LEN-1, "OK ovs_del_lan lan=%s", lan);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void action_add_lan_endp(char *bin, char *db, char *respb,
                         char *lan, char *name, int num, char *vhost)
{
  if (ovs_cmd_add_lan_endp(bin, db, lan, vhost))
    {
    snprintf(respb, MAX_PATH_LEN-1,
             "KO ovs_add_lan_endp name=%s num=%d vhost=%s lan=%s",
             name, num, vhost, lan);
    KERR("%s", respb);
    }
  else
    {
    snprintf(respb, MAX_PATH_LEN-1,
             "OK ovs_add_lan_endp name=%s num=%d vhost=%s lan=%s",
             name, num, vhost, lan);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void action_del_lan_endp(char *bin, char *db, char *respb,
                         char *lan, char *name, int num, char *vhost)
{
  if (ovs_cmd_del_lan_endp(bin, db, lan, vhost))
    {
    snprintf(respb, MAX_PATH_LEN-1,
             "KO ovs_del_lan_endp name=%s num=%d vhost=%s lan=%s",
             name, num, vhost, lan);
    KERR("%s", respb);
    }
  else
    {
    snprintf(respb, MAX_PATH_LEN-1,
             "OK ovs_del_lan_endp name=%s num=%d vhost=%s lan=%s",
             name, num, vhost, lan);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void action_req_ovsdb(char *bin, char *db, char *net, char *respb)
{
  int pid;
  if (g_ovsdb_launched == 0)
    {
    g_ovsdb_launched = 1;
    if (create_ovsdb_server_conf(bin, db))
      {
      KERR("ERROR CREATION DB");
      snprintf(respb, MAX_PATH_LEN-1, "ovs_resp_ovsdb_ko");
      }
    else
      {
      sync();
      usleep(10000);
      sync();
      pid = ovs_execv_ovsdb_server(net, bin, db);
      if (pid <= 0)
        {
        KERR("ERROR CREATION DB SERVER");
        snprintf(respb, MAX_PATH_LEN-1, "ovs_resp_ovsdb_ko");
        }
      else
        {
        snprintf(respb, MAX_PATH_LEN-1, "ovs_resp_ovsdb_ok pid=%d", pid);
        }
      }
    }
  else
    {
    KERR("ERROR DB SERVER");
    snprintf(respb, MAX_PATH_LEN-1, "ovs_resp_ovsdb_ko");
    }
  sync();
  usleep(1000);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void action_req_ovs_switch(char *bin, char *db, char *net, char *respb)
{
  int pid;
  if (g_ovs_launched == 0)
    {
    g_ovs_launched = 1;
    pid = ovs_execv_ovs_vswitchd(net, bin, db);
    if (pid <= 0)
      {
      KERR("BAD START OVS");
      snprintf(respb, MAX_PATH_LEN-1, "ovs_resp_ovs_ko");
      }
    else
      {
      snprintf(respb, MAX_PATH_LEN-1, "ovs_resp_ovs_ok pid=%d", pid);
      }
    }
  else
    {
    KERR("OVS LAUNCH ALREADY TRIED");
    snprintf(respb, MAX_PATH_LEN-1, "ovs_resp_ovs_ko");
    }
  sync();
  usleep(1000);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void action_req_suidroot(char *respb)
{
  g_ovs_launched = 0;
  g_ovsdb_launched = 0;
  ovs_execv_init();
  if (check_uid())
    snprintf(respb, MAX_PATH_LEN-1, "ovs_resp_suidroot_ko");
  else
    snprintf(respb, MAX_PATH_LEN-1, "ovs_resp_suidroot_ok");
}
/*---------------------------------------------------------------------------*/


