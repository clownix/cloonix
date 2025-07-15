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
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>



#include "io_clownix.h"
#include "rpc_clownix.h"
#include "commun_daemon.h"
#include "cnt.h"
#include "crun.h"
#include "hop_event.h"
#include "cfg_store.h"
#include "utils_cmd_line_maker.h"
#include "uml_clownix_switch.h"
#include "file_read_write.h"


/*****************************************************************************/
void crun_sigdiag_resp(int llid, char *line)
{
  int crun_pid;
  t_cnt *cur;
  char name[MAX_NAME_LEN];
  char req[5*MAX_PATH_LEN];
  char mountbear[2*MAX_PATH_LEN];
  memset(req, 0, 5*MAX_PATH_LEN);
  memset(mountbear, 0, 2*MAX_PATH_LEN);
  if (sscanf(line, 
  "cloonsuid_crun_create_net_resp_ok name=%s", name) == 1)
    {
    cur = find_cnt(name);
    if (cur == NULL) 
      KERR("ERROR %s", name);
    else
      {
      snprintf(req, 5*MAX_PATH_LEN-1, 
               "cloonsuid_crun_create_config_json name=%s "
               "<startup_env_keyid>%s</startup_env_keyid> "
               "<startup_vmount>%s</startup_vmount>\n",
               name, cur->cnt.startup_env, cur->cnt.vmount);
      if (send_sig_suid_power(llid, req))
        KERR("ERROR %d %s", llid, name);
      }
    }
  else if (sscanf(line,
  "cloonsuid_crun_create_config_json_resp_ok name=%s", name) == 1)
    {
    cur = find_cnt(name);
    if (cur == NULL)
      KERR("ERROR %s", name);
    else
      {
      snprintf(req, 5*MAX_PATH_LEN-1,
               "cloonsuid_crun_create_tar_img name=%s", name);
      if (send_sig_suid_power(llid, req))
        KERR("ERROR %d %s", llid, name);
      }
    }
  else if (sscanf(line,
  "cloonsuid_crun_create_tar_img_resp_ok name=%s", name) == 1)
    {
    cur = find_cnt(name);
    if (cur == NULL)
      KERR("ERROR %s", name);
    else
      {
      snprintf(req, 5*MAX_PATH_LEN-1,
               "cloonsuid_crun_create_overlay name=%s", name);
      if (send_sig_suid_power(llid, req))
        KERR("ERROR %d %s", llid, name);
      }
    }
  else if (sscanf(line,
  "cloonsuid_crun_create_overlay_resp_ok name=%s", name) == 1)
    {
    cur = find_cnt(name);
    if (cur == NULL)
      KERR("ERROR %s", name);
    else
      {
      snprintf(req, 5*MAX_PATH_LEN-1,
               "cloonsuid_crun_create_crun_start name=%s", name);
      if (send_sig_suid_power(llid, req))
        KERR("ERROR %d %s", llid, name);
      }
    }
  else if (sscanf(line,
  "cloonsuid_crun_create_crun_start_resp_ok name=%s crun_pid=%d mountbear=%s",
  name, &crun_pid, mountbear) == 3)
    {
    cur = find_cnt(name);
    if (cur == NULL)
      KERR("ERROR %s", name);
    else
      {
      strncpy(cur->mountbear, mountbear, MAX_PATH_LEN-1);
      cur->cnt_pid = crun_pid;
      utils_send_status_ok(&(cur->cli_llid), &(cur->cli_tid));
      cnt_vhost_and_doors_begin(cur);
      }
    }
  else if (sscanf(line, 
  "cloonsuid_crun_delete_resp_ok name=%s", name) == 1)
    {
    cur = find_cnt(name);
    if (cur == NULL) 
      KERR("ERROR %s", name);
    else
      {
      utils_send_status_ok(&(cur->cli_llid), &(cur->cli_tid));
      cnt_free_cnt(name);
      }
    }
  else if (sscanf(line,
  "cloonsuid_crun_killed name=%s crun_pid=%d", name, &crun_pid) == 2)
    {
    KERR("ERROR %s", line);
    cur = find_cnt(name);
    if (cur == NULL) 
      KERR("ERROR %s", name);
    else
      {
      utils_send_status_ko(&(cur->cli_llid),&(cur->cli_tid),"CNT KILLED");
      cnt_free_cnt(name);
      }
    }
  else if ((sscanf(line,
  "cloonsuid_crun_create_net_resp_ko name=%s", name) == 1) ||
           (sscanf(line,
  "cloonsuid_crun_create_tar_img_resp_ko name=%s", name) == 1) ||
           (sscanf(line,
  "cloonsuid_crun_create_overlay_resp_ko name=%s", name) == 1) ||
           (sscanf(line,
  "cloonsuid_crun_create_crun_start_resp_ko name=%s", name) == 1) ||
           (sscanf(line,
  "cloonsuid_crun_delete_resp_ko name=%s", name) == 1))
    {
    KERR("ERROR %s", line);
    cur = find_cnt(name);
    if (cur != NULL)
      {
      snprintf(req, MAX_PATH_LEN-1,
      "cloonsuid_crun_ERROR name=%s", name);
      if (send_sig_suid_power(llid, req))
        KERR("ERROR %d %s", llid, name);
      utils_send_status_ko(&(cur->cli_llid),&(cur->cli_tid),"CNT ERROR");
      cnt_free_cnt(name);
      }
    }
  else if (sscanf(line, 
  "cloonsuid_crun_ERROR name=%s", name) == 1)
    {
    KERR("ERROR %s", name);
    cur = find_cnt(name);
    if (cur != NULL)
      {
      utils_send_status_ko(&(cur->cli_llid),&(cur->cli_tid),"ERROR CNT");
      cnt_free_cnt(name);
      }
    }
  else
    KERR("ERROR %s", line);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int crun_create(int llid, int vm_id, t_topo_cnt *cnt, char *agent)
{
  int i, result = -1;
  char req[2*MAX_PATH_LEN];
  char *image;
  char *cnt_dir = utils_get_cnt_dir();
  unsigned char *mac;
  char option1[MAX_PATH_LEN];
  char option2[MAX_PATH_LEN];
  char *bulk = cfg_get_bulk();

  image = cnt->image;
  if (get_running_in_crun())
    {
    memset(option1, 0, MAX_PATH_LEN);
    memset(option2, 0, MAX_PATH_LEN);
    snprintf(option1, MAX_PATH_LEN-1, "%s/%s", cfg_get_bulk(), image);
    snprintf(option2, MAX_PATH_LEN-1, "%s/%s", cfg_get_bulk_host(), image);
    if (!file_exists(option1, R_OK))
      {
      if (file_exists(option2, R_OK))
        bulk = cfg_get_bulk_host();
      else
        KERR("ERROR %s and %s do not exist", option1, option2);
      }
    }
  memset(req, 0, 2*MAX_PATH_LEN);
  snprintf(req, 2*MAX_PATH_LEN-1, 
  "cloonsuid_crun_create_net name=%s "
  "bulk=%s image=%s nb=%d vm_id=%d cnt_dir=%s "
  "agent_dir=%s is_persistent=%d is_privileged=%d brandtype=%s",
  cnt->name, bulk, image, cnt->nb_tot_eth,
  vm_id, cnt_dir, agent, cnt->is_persistent, cnt->is_privileged,
  cnt->brandtype);
  if (send_sig_suid_power(llid, req))
    {
    KERR("ERROR %s Bad command create_net to suid_power", cnt->name);
    cnt_free_cnt(cnt->name);
    }
  else
    {
    result = 0;
    for (i=0; i<cnt->nb_tot_eth; i++)
      {
      memset(req, 0, 2*MAX_PATH_LEN);
      mac = cnt->eth_table[i].mac_addr;
      snprintf(req, 2*MAX_PATH_LEN-1,
      "cloonsuid_crun_create_eth name=%s num=%d "
      "mac=0x%02hhx:0x%02hhx:0x%02hhx:0x%02hhx:0x%02hhx:0x%02hhx",
      cnt->name, i, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
      if (send_sig_suid_power(llid, req))
        {
        KERR("ERROR %s", cnt->name);
        cnt_free_cnt(cnt->name);
        result = -1;
        }
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void crun_init(void)
{
}
/*--------------------------------------------------------------------------*/


