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
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>


#include "io_clownix.h"
#include "rpc_clownix.h"
#include "commun_daemon.h"
#include "hop_event.h"
#include "cfg_store.h"
#include "event_subscriber.h"
#include "cnt.h"
#include "utils_cmd_line_maker.h"
#include "docker_images.h"

/*****************************************************************************/
static int mycmp(char *line, char *patern)
{
  int result;
  result = strncmp(line, patern, strlen(patern));
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void docker_sigdiag_resp(int llid, char *line)
{
  t_cnt *cur;
  char type[MAX_NAME_LEN];
  char name[MAX_NAME_LEN];
  char id[MAX_NAME_LEN];
  char image[MAX_NAME_LEN];
  char req[MAX_PATH_LEN];
  char *mac;
  int i, vm_id, pid;

  if (sscanf(line,
  "cloonsuid_docker_create_eth_done_ok brandtype=%s name=%s", type, name) == 2) 
    {
    cur = find_cnt(name);
    if (cur == NULL)
      KERR("ERROR %s", line);
    else
      {
      utils_send_status_ok(&(cur->cli_llid), &(cur->cli_tid));
      cnt_vhost_and_doors_begin(cur);
      }
    }
  else if (sscanf(line,
  "cloonsuid_docker_resp_launch_ok brandtype=%s name=%s "
  "vm_id=%d image=%s pid=%d",
     type, name, &vm_id, image, &pid) == 5)
    {
    cur = find_cnt(name);
    if (cur == NULL)
      KERR("ERROR %s", line);
    else
      {
      cur->cnt_pid = pid;
      for (i=0; i<cur->cnt.nb_tot_eth; i++)
        {
        memset(req, 0, MAX_PATH_LEN);
        mac = cur->cnt.eth_table[i].mac_addr;
        snprintf(req, MAX_PATH_LEN-1,
        "cloonsuid_docker_create_eth brandtype=%s name=%s num=%d "
        "mac=0x%02hhx:0x%02hhx:0x%02hhx:0x%02hhx:0x%02hhx:0x%02hhx",
        cur->cnt.brandtype, cur->cnt.name, i,
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        if (send_sig_suid_power(llid, req))
          KERR("ERROR %s", cur->cnt.name);
        }
      snprintf(req, MAX_PATH_LEN-1,
      "cloonsuid_docker_create_eth_done brandtype=%s name=%s",
      cur->cnt.brandtype, cur->cnt.name);
      if (send_sig_suid_power(llid, req))
        KERR("ERROR %s", cur->cnt.name);
      }
    }
  else if (sscanf(line,
  "cloonsuid_docker_delete_resp_ko brandtype=%s name=%s", type, name) == 2)
    {
    KERR("ERROR %s", line);
    cnt_free_cnt(name);
    }
  else if (sscanf(line,
  "cloonsuid_docker_delete_resp_ok brandtype=%s name=%s", type, name) == 2)
    {
    cnt_free_cnt(name);
    }
  else if ((sscanf(line,
  "cloonsuid_docker_resp_launch_ko brandtype=%s name=%s vm_id=%d image=%s",
  type, name, &vm_id, image) == 4) ||
           (sscanf(line,
  "cloonsuid_docker_killed brandtype=%s name=%s id=%s pid=%d", type, name, id, &pid) == 4) ||
           (sscanf(line,
  "cloonsuid_docker_delete_resp_ko brandtype=%s name=%s", type, name) == 2) ||
           (sscanf(line,
  "cloonsuid_docker_create_eth_done_ko brandtype=%s name=%s", type, name) == 2))
    {
    KERR("ERROR %s", line);
    cur = find_cnt(name);
    if (cur != NULL)
      {
      memset(req, 0, MAX_PATH_LEN);
      snprintf(req, MAX_PATH_LEN-1,
      "cloonsuid_docker_ERROR brandtype=%s name=%s", type, name);
      if (send_sig_suid_power(llid, req))
        KERR("ERROR %d %s", llid, name);
      utils_send_status_ko(&(cur->cli_llid),&(cur->cli_tid),"CNT ERROR");
      cnt_free_cnt(name);
      }
    }
  else
    KERR("ERROR %s", line);
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void docker_poldiag_resp(int llid, char *line)
{
  char name[MAX_NAME_LEN];
  char type[MAX_NAME_LEN];

  if (!mycmp(line, "cloonsuid_docker_resp_image_begin"))
    {
    clean_slave_image_chain();
    }
  else if (sscanf(line,
                  "cloonsuid_docker_resp_image_name: brandtype=%s name=%s",
                  type, name) == 2)
    {
    add_slave_image_doc(type, name);
    }
  else if (!mycmp(line, "cloonsuid_docker_resp_image_end"))
    {
    swap_slave_master_image();
    }
  else
    KERR("ERROR %s", line);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int docker_create(int llid, int vm_id, t_topo_cnt *cnt, char *agent)
{
  int result = -1;
  char req[3*MAX_PATH_LEN];
  char *cnt_dir = utils_get_cnt_dir();
  char *name=cnt->name, *image=cnt->image;
  char *customer_launch = cnt->customer_launch;
  char *startup_env = cnt->startup_env;

  if (!docker_images_exists(cnt->brandtype, image))
    {
    KERR("ERROR image not found name: %s %s  image:%s",
         cnt->brandtype, name, image);
    }
  else
    { 
    memset(req, 0, 3*MAX_PATH_LEN);
    snprintf(req, 3*MAX_PATH_LEN-1, 
    "cloonsuid_docker_req_launch brandtype=%s name=%s vm_id=%d "
    "image=%s cnt_dir=%s agent=%s "
    "<customer_launch_keyid>%s</customer_launch_keyid> "
    "<startup_env_keyid>%s</startup_env_keyid>\n",
    cnt->brandtype, name, vm_id, image, cnt_dir, agent,
    customer_launch, startup_env);
    if (send_sig_suid_power(llid, req))
      {
      KERR("ERROR %s Bad command launch to suid_power", cnt->name);
      cnt_free_cnt(cnt->name);
      }
    else
      result = 0;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void docker_timer_beat(int llid)
{
  char req[MAX_PATH_LEN];
  snprintf(req, MAX_PATH_LEN-1, "cloonsuid_docker_req_imgs");
  if (send_pol_suid_power(llid, req))
    KERR("ERROR %d", llid);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void docker_init(void)
{
  docker_images_init();
}
/*--------------------------------------------------------------------------*/

