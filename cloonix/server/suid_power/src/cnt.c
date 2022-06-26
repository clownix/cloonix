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
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>
#include <dirent.h>
#include <sys/wait.h>



#include "io_clownix.h"
#include "rpc_clownix.h"
#include "launcher.h"
#include "cnt_utils.h"
#include "cnt.h"
#include "loop_img.h"

int get_cloonix_rank(void);

/****************************************************************************/
typedef struct t_cnt
{
  char name[MAX_NAME_LEN];
  char nspace[MAX_NAME_LEN];
  char bulk[MAX_PATH_LEN];
  char image[MAX_NAME_LEN];
  char cnt_dir[MAX_PATH_LEN];
  char customer_launch[MAX_PATH_LEN];
  char agent_dir[MAX_PATH_LEN];
  char rootfs_path[MAX_PATH_LEN];
  char nspace_path[MAX_PATH_LEN];
  int nb_eth;
  t_eth_mac eth_mac[MAX_ETH_VM];
  int cloonix_rank;
  int vm_id;
  int crun_pid;
  int crun_pid_count;
  struct t_cnt *prev;
  struct t_cnt *next;
} t_cnt;
/*--------------------------------------------------------------------------*/

static t_cnt *g_head_cnt;

/****************************************************************************/
static t_cnt *find_cnt(char *name)
{ 
  t_cnt *cur = g_head_cnt;
  while (cur)
    {
    if (!strcmp(cur->name, name))
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void alloc_cnt(char *name, char *bulk, char *image, int nb_eth,
                            char *nspace, int cloonix_rank,
                            int vm_id, char *cnt_dir, char *agent_dir,
                            char *customer_launch)
{
  t_cnt *cur = (t_cnt *) malloc(sizeof(t_cnt));
  memset(cur, 0, sizeof(t_cnt));
  strncpy(cur->name, name, MAX_NAME_LEN-1);
  strncpy(cur->bulk, bulk, MAX_PATH_LEN-1);
  strncpy(cur->image, image, MAX_NAME_LEN-1);
  strncpy(cur->cnt_dir, cnt_dir, MAX_PATH_LEN-1);
  strncpy(cur->customer_launch, customer_launch, MAX_PATH_LEN-1);
  strncpy(cur->agent_dir, agent_dir, MAX_PATH_LEN-1);
  strncpy(cur->nspace, nspace, MAX_NAME_LEN-1);
  snprintf(cur->nspace_path, MAX_PATH_LEN-1, "%s/%s", PATH_NAMESPACE, nspace);
  snprintf(cur->rootfs_path, MAX_PATH_LEN-1,"%s/%s/%s",cnt_dir,name,ROOTFS);
  cur->nb_eth = nb_eth;
  cur->cloonix_rank = cloonix_rank;
  cur->vm_id = vm_id; 
  if (g_head_cnt)
    g_head_cnt->prev = cur;
  cur->next = g_head_cnt;
  g_head_cnt = cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void free_cnt(t_cnt *cur)
{
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur->next)
    cur->next->prev = cur->prev;
  if (cur == g_head_cnt)
    g_head_cnt = cur->next;
  free(cur);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void unlink_sub_dir_files(char *dirname)
{
  char pth[MAX_PATH_LEN];
  DIR *dirptr;
  struct dirent *ent;
  char *fname;
  dirptr = opendir(dirname);
  if (dirptr)
    {
    while ((ent = readdir(dirptr)) != NULL)
      {
      if (!strcmp(ent->d_name, "."))
        continue;
      if (!strcmp(ent->d_name, ".."))
        continue;
      fname = ent->d_name;
      memset(pth, 0, MAX_PATH_LEN);
      snprintf(pth, MAX_PATH_LEN-1, "%s/%s", dirname, fname);
      if(ent->d_type == DT_DIR)
        unlink_sub_dir_files(pth);
      else if (unlink(pth))
        {
        KERR("ERROR File: %s could not be deleted\n", pth);
        break;
        }
      }
    if (closedir(dirptr))
      KERR("ERROR %d", errno);
    if (rmdir(dirname))
      KERR("ERROR Dir: %s could not be deleted\n", dirname);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void unlink_all_files(char *cnt_dir, char *name)
{
  char pth[MAX_PATH_LEN];
  memset(pth, 0, MAX_PATH_LEN);
  snprintf(pth, MAX_PATH_LEN-1, "%s/%s", cnt_dir, name);
  unlink_sub_dir_files(pth);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void urgent_destroy_cnt(t_cnt *cur)
{
  char cnt_dir[MAX_PATH_LEN];
  char name[MAX_NAME_LEN];
  memset(cnt_dir, 0, MAX_PATH_LEN);
  memset(name, 0, MAX_NAME_LEN);
  strncpy(cnt_dir, cur->cnt_dir, MAX_PATH_LEN-1); 
  strncpy(name, cur->name, MAX_NAME_LEN-1); 
  cnt_utils_delete_crun_stop(name, cur->crun_pid);
  cnt_utils_delete_overlay(name, cnt_dir, cur->bulk, cur->image);
  cnt_utils_delete_net(cur->nspace);
  free_cnt(cur);
  unlink_all_files(cnt_dir, name);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void cnt_beat(int llid)
{
  int wstatus;
  char resp[MAX_PATH_LEN];
  t_cnt *next, *cur = g_head_cnt;
  while (cur)
    {
    next = cur->next;
    if (cur->crun_pid != 0)
      {
      cur->crun_pid_count += 1;
      waitpid(cur->crun_pid, &wstatus, WNOHANG);
      if (kill(cur->crun_pid, 0))
        {
        if (cur->crun_pid_count > 10)
          {
          KERR("ERROR %d %s", cur->crun_pid, cur->name);
          memset(resp, 0, MAX_PATH_LEN);
          snprintf(resp, MAX_PATH_LEN-1,
                   "cloonsuid_cnt_killed name=%s crun_pid=%d",
                   cur->name, cur->crun_pid);
          rpct_send_sigdiag_msg(llid, type_hop_suid_power, resp);
          urgent_destroy_cnt(cur);
          }
        }
      }
    cur = next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void cnt_recv_poldiag_msg(int llid, int tid, char *line)
{
  char name[MAX_NAME_LEN];
  char resp[MAX_PATH_LEN];
  memset(resp, 0, MAX_PATH_LEN);
  if (!strcmp(line,
  "cloonsuid_cnt_get_infos"))
    {
    snprintf(resp, MAX_PATH_LEN-1, "%s", "cloonsuid_cnt_infos");
    }
  else if (sscanf(line,
  "cloonsuid_cnt_get_crun_run_check %s", name) == 1) 
    {
    if (!cnt_utils_create_crun_run_check(name))
      {
      snprintf(resp, MAX_PATH_LEN-1,
      "cloonsuid_cnt_get_crun_run_check_resp_ok %s", name);
      } 
    else
      {
      snprintf(resp, MAX_PATH_LEN-1,
      "cloonsuid_cnt_get_crun_run_check_resp_ko %s", name);
      } 
    }
  else
    KERR("ERROR %s", line);
  if (strlen(resp))
    rpct_send_poldiag_msg(llid, tid, resp);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void create_net(char *line, char *resp, char *name, char *bulk,
                       char *image, int nb_eth, int cloonix_rank,
                       int vm_id, char *cnt_dir, char *agent_dir,
                       char *customer_launch)
{
  char nspace[MAX_NAME_LEN];
  char bulk_image[MAX_PATH_LEN];
  t_cnt *cur = find_cnt(name);
  memset(resp, 0, MAX_PATH_LEN);
  memset(bulk_image, 0, MAX_PATH_LEN);
  snprintf(bulk_image, MAX_PATH_LEN-1, "%s/%s", bulk, image); 
  snprintf(resp, MAX_PATH_LEN-1,
           "cloonsuid_cnt_create_net_resp_ko name=%s", name);
  snprintf(nspace, MAX_NAME_LEN-1, "%s_%d_%d",
           BASE_NAMESPACE, cloonix_rank, vm_id);
  if (cur != NULL)
    KERR("ERROR %s", name);
  else if (access(bulk_image, F_OK))
    KERR("ERROR %s", name);
  else if (nb_eth >= MAX_ETH_VM)
    KERR("ERROR %s", name);
  else
    {
    alloc_cnt(name, bulk, image, nb_eth, nspace,
                    cloonix_rank, vm_id, cnt_dir, agent_dir,
                    customer_launch);
    memset(resp, 0, MAX_PATH_LEN);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void create_net_eth(char *line, char *resp, char *name,
                           int num, char *mac) 
{
  t_cnt *cur;
  cur = find_cnt(name);
  if (cur != NULL)
    {
    if (num >= MAX_ETH_VM)
      KERR("ERROR %s", name);
    else
      {
      memcpy(cur->eth_mac[num].mac, mac, 6);
      if (num+1 < cur->nb_eth) 
        {
        memset(resp, 0, MAX_PATH_LEN);
        }
      else
        {
        if (cnt_utils_create_net(cur->bulk, cur->image, name, cur->cnt_dir,
                                 cur->nspace, cur->cloonix_rank, cur->vm_id,
                                 cur->nb_eth, cur->eth_mac, cur->agent_dir,
                                 cur->customer_launch))
          {
          KERR("ERROR %s", name);
          snprintf(resp, MAX_PATH_LEN-1,
          "cloonsuid_cnt_create_net_resp_ko name=%s", name);
          }
        else
          {
          snprintf(resp, MAX_PATH_LEN-1,
          "cloonsuid_cnt_create_net_resp_ok name=%s", name);
          }
        }
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void create_config_json(char *line, char *resp, char *name)
{
  t_cnt *cur;
  char path[MAX_PATH_LEN];
  char *cnt_dir;
  cur = find_cnt(name);
  if (cur == NULL)
    {
    KERR("ERROR %s", line);
    snprintf(resp, MAX_PATH_LEN-1,
             "cloonsuid_cnt_create_config_json_resp_ko name=%s",
             name);
    }
  else
    {
    cnt_dir = cur->cnt_dir;
    memset(path, 0, MAX_PATH_LEN);
    snprintf(path, MAX_PATH_LEN-1, "%s/%s", cnt_dir, cur->name);
    if (cnt_utils_create_config_json(path,cur->rootfs_path,cur->nspace_path))
      {
      KERR("ERROR %s", line);
      snprintf(resp, MAX_PATH_LEN-1,
               "cloonsuid_cnt_create_config_json_resp_ko name=%s",
               name);
      }
    else
      {
      snprintf(resp, MAX_PATH_LEN-1,
               "cloonsuid_cnt_create_config_json_resp_ok name=%s",
               name);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void create_loop_img(char *line, char *resp, char *name)
{
  t_cnt *cur;
  cur = find_cnt(name);
  if (cur == NULL)
    {
    KERR("ERROR %s", line);
    snprintf(resp, MAX_PATH_LEN-1,
             "cloonsuid_cnt_create_loop_img_resp_ko name=%s", name);
    }
  else
    {
    if (loop_img_add(name, cur->bulk, cur->image, cur->cnt_dir))
      {
      KERR("ERROR %s", line);
      snprintf(resp, MAX_PATH_LEN-1,
               "cloonsuid_cnt_create_loop_img_resp_ko name=%s", name);
      }
    else
      {
      snprintf(resp, MAX_PATH_LEN-1,
               "cloonsuid_cnt_create_loop_img_resp_ok name=%s", name);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void create_overlay(char *line, char *resp, char *name)
{
  t_cnt *cur;
  char path[MAX_PATH_LEN];
  char *cnt_dir;
  char *loop_img;
  cur = find_cnt(name);
  if (cur == NULL)
    {
    KERR("ERROR %s", line);
    snprintf(resp, MAX_PATH_LEN-1,
             "cloonsuid_cnt_create_overlay_resp_ko name=%s", name);
    }
  else 
    {
    loop_img = loop_img_get(name, cur->bulk, cur->image);
    if (loop_img == NULL)
      {
      KERR("ERROR %s", line);
      snprintf(resp, MAX_PATH_LEN-1,
               "cloonsuid_cnt_create_overlay_resp_ko name=%s", name);
      }
    else
      { 
      cnt_dir = cur->cnt_dir;
      memset(path, 0, MAX_PATH_LEN);
      snprintf(path, MAX_PATH_LEN-1, "%s/%s", cnt_dir, cur->name);
      if (cnt_utils_create_overlay(path, loop_img))
        {
        KERR("ERROR %s", line);
        snprintf(resp, MAX_PATH_LEN-1,
                 "cloonsuid_cnt_create_overlay_resp_ko name=%s", name);
        }
      else
        {
        snprintf(resp, MAX_PATH_LEN-1,
                 "cloonsuid_cnt_create_overlay_resp_ok name=%s", name);
        }
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void create_crun_start(char *line, char *resp, char *name)
{
  t_cnt *cur;
  cur = find_cnt(name);
  if (cur == NULL)
    {
    KERR("ERROR %s", line);
    snprintf(resp, MAX_PATH_LEN-1,
             "cloonsuid_cnt_create_crun_start_resp_ko name=%s", name);
    }
  else
    {
    cur->crun_pid = cnt_utils_create_crun_create(cur->cnt_dir, cur->name);
    if (cur->crun_pid == 0)
      {
      KERR("ERROR %s", line);
      snprintf(resp, MAX_PATH_LEN-1,
               "cloonsuid_cnt_create_crun_start_resp_ko name=%s", name);
      }
    else if (cnt_utils_create_crun_start(name))
      {
      KERR("ERROR %s", line);
      snprintf(resp, MAX_PATH_LEN-1,
               "cloonsuid_cnt_create_crun_start_resp_ko name=%s", name);
      }
    else
      {
      snprintf(resp, MAX_PATH_LEN-1,
      "cloonsuid_cnt_create_crun_start_resp_ok name=%s crun_pid=%d",
      name, cur->crun_pid);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void delete_cnt(char *line, char *resp, char *nm)
{
  char cnt_dir[MAX_PATH_LEN];
  char name[MAX_NAME_LEN];
  t_cnt *cur;
  cur = find_cnt(nm);
  if (cur == NULL)
    {
    KERR("ERROR %s", line);
    snprintf(resp, MAX_PATH_LEN-1,
             "cloonsuid_cnt_delete_resp_ko name=%s", nm);
    }
  else
    {
    memset(cnt_dir, 0, MAX_PATH_LEN);
    memset(name, 0, MAX_NAME_LEN);
    strncpy(cnt_dir, cur->cnt_dir, MAX_PATH_LEN-1); 
    strncpy(name, cur->name, MAX_NAME_LEN-1); 
    if (cnt_utils_delete_crun_stop(nm, cur->crun_pid))
      KERR("ERROR %s", line);
    cnt_utils_delete_overlay(cur->name, cur->cnt_dir, cur->bulk, cur->image);
    if (cnt_utils_delete_net(cur->nspace))
      {
      KERR("ERROR %s", line);
      snprintf(resp, MAX_PATH_LEN-1,
               "cloonsuid_cnt_delete_resp_ko name=%s", nm);
      }
    else
      snprintf(resp, MAX_PATH_LEN-1,
               "cloonsuid_cnt_delete_resp_ok name=%s", nm);
    free_cnt(cur);
    unlink_all_files(cnt_dir, name);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void cnt_recv_sigdiag_msg(int llid, int tid, char *line)
{
  t_cnt *cur;
  char name[MAX_NAME_LEN];
  char bulk[MAX_PATH_LEN];
  char image[MAX_NAME_LEN];
  char resp[MAX_PATH_LEN];
  char cnt_dir[MAX_PATH_LEN];
  char agent_dir[MAX_PATH_LEN];
  char customer_launch[MAX_PATH_LEN];
  char mac[6], *ptr;
  int len, num, nb_eth, cloonix_rank, vm_id;

  cloonix_rank = get_cloonix_rank();
  memset(resp, 0, MAX_PATH_LEN);
  if (sscanf(line, 
  "cloonsuid_cnt_create_net name=%s bulk=%s "
  "image=%s nb=%d vm_id=%d cnt_dir=%s agent_dir=%s",
  name, bulk, image, &nb_eth, &vm_id,
  cnt_dir, agent_dir) == 7)
    {
    ptr = strstr(line, "customer_launch=");
    if (ptr == NULL)
      {
      KERR("ERROR %s", line);
      snprintf(resp, MAX_PATH_LEN-1,
               "cloonsuid_cnt_create_net_resp_ko name=%s", name);
      } 
    else
      {
      len = strlen("customer_launch=");
      strncpy(customer_launch, ptr+len, MAX_PATH_LEN-1); 
      create_net(line, resp, name, bulk, image, nb_eth,
                 cloonix_rank, vm_id, cnt_dir, agent_dir,
                 customer_launch);
      }
    }
  else if (sscanf(line,
  "cloonsuid_cnt_create_eth name=%s num=%d "
  "mac=0x%02hhx:0x%02hhx:0x%02hhx:0x%02hhx:0x%02hhx:0x%02hhx",
  name, &num, &(mac[0]),&(mac[1]),&(mac[2]),&(mac[3]),&(mac[4]),&(mac[5])))
    {
    create_net_eth(line, resp, name, num, mac);
    }
  else if (sscanf(line,
  "cloonsuid_cnt_create_config_json name=%s", name) == 1)
    {
    create_config_json(line, resp, name);
    }
  else if (sscanf(line,
  "cloonsuid_cnt_create_loop_img name=%s", name) == 1)
    {
    create_loop_img(line, resp, name);
    }
  else if (sscanf(line,
  "cloonsuid_cnt_create_overlay name=%s", name) == 1)
    {
    create_overlay(line, resp, name);
    }
  else if (sscanf(line,
  "cloonsuid_cnt_create_crun_start name=%s", name) == 1)
    {
    create_crun_start(line, resp, name);
    }
  else if (sscanf(line, 
  "cloonsuid_cnt_delete name=%s", name) == 1)
    {
    delete_cnt(line, resp, name);
    }
  else if (sscanf(line,
  "cloonsuid_cnt_ERROR name=%s", name) == 1)
    {
    cur = find_cnt(name);
    if (cur != NULL)
      {
      KERR("ERROR MUST ERASE %s, %s", name, line);
      urgent_destroy_cnt(cur);
      }
    }
  else
    KERR("ERROR %s", line);
  if (strlen(resp))
    rpct_send_sigdiag_msg(llid, tid, resp);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void cnt_kill_all(void)
{
  char resp[MAX_PATH_LEN];
  t_cnt *next, *cur = g_head_cnt;
  while (cur)
    {
    next = cur->next;
    delete_cnt("kill all cnt procedure", resp, cur->name);
    cur = next;
    }
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
void cnt_init(void)
{
  g_head_cnt = NULL;
  cnt_utils_init();
  loop_img_init();
}
/*--------------------------------------------------------------------------*/
