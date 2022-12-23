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
#include "crun_utils.h"
#include "crun.h"
#include "loop_img.h"

int get_cloonix_rank(void);

/****************************************************************************/
typedef struct t_crun
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
  char mountbear[MAX_PATH_LEN];
  char mounttmp[MAX_PATH_LEN];
  int nb_eth;
  t_eth_mac eth_mac[MAX_ETH_VM];
  int cloonix_rank;
  int vm_id;
  int crun_pid;
  int crun_pid_count;
  int is_persistent;
  struct t_crun *prev;
  struct t_crun *next;
} t_crun;
/*--------------------------------------------------------------------------*/

static t_crun *g_head_crun;

/****************************************************************************/
static t_crun *find_crun(char *name)
{ 
  t_crun *cur = g_head_crun;
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
static void alloc_crun(char *name, char *bulk, char *image, int nb_eth,
                            char *nspace, int cloonix_rank,
                            int vm_id, char *cnt_dir, char *agent_dir,
                            char *customer_launch)
{
  t_crun *cur = (t_crun *) malloc(sizeof(t_crun));
  memset(cur, 0, sizeof(t_crun));
  strncpy(cur->name, name, MAX_NAME_LEN-1);
  strncpy(cur->bulk, bulk, MAX_PATH_LEN-1);
  strncpy(cur->image, image, MAX_NAME_LEN-1);
  strncpy(cur->cnt_dir, cnt_dir, MAX_PATH_LEN-1);
  strncpy(cur->customer_launch, customer_launch, MAX_PATH_LEN-1);
  strncpy(cur->agent_dir, agent_dir, MAX_PATH_LEN-1);
  strncpy(cur->nspace, nspace, MAX_NAME_LEN-1);
  snprintf(cur->nspace_path, MAX_PATH_LEN-1, "%s%s", PATH_NAMESPACE, nspace);
  snprintf(cur->rootfs_path, MAX_PATH_LEN-1,"%s/%s/%s",cnt_dir,name,ROOTFS);
  snprintf(cur->mountbear, MAX_PATH_LEN-1,"%s/%s/mnt", cnt_dir, name);
  snprintf(cur->mounttmp, MAX_PATH_LEN-1,"%s/%s/tmp", cnt_dir, name);
  cur->nb_eth = nb_eth;
  cur->cloonix_rank = cloonix_rank;
  cur->vm_id = vm_id; 
  if (g_head_crun)
    g_head_crun->prev = cur;
  cur->next = g_head_crun;
  g_head_crun = cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void free_crun(t_crun *cur)
{
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur->next)
    cur->next->prev = cur->prev;
  if (cur == g_head_crun)
    g_head_crun = cur->next;
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
static void urgent_destroy_crun(t_crun *cur)
{
  char cnt_dir[MAX_PATH_LEN];
  char name[MAX_NAME_LEN];
  memset(cnt_dir, 0, MAX_PATH_LEN);
  memset(name, 0, MAX_NAME_LEN);
  strncpy(cnt_dir, cur->cnt_dir, MAX_PATH_LEN-1); 
  strncpy(name, cur->name, MAX_NAME_LEN-1); 
  crun_utils_delete_crun_stop(name, cur->crun_pid);
  crun_utils_delete_overlay(name, cnt_dir, cur->bulk,
                            cur->image, cur->is_persistent);
  free_crun(cur);
  unlink_all_files(cnt_dir, name);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void crun_beat(int llid)
{
  int wstatus;
  char resp[MAX_PATH_LEN];
  t_crun *next, *cur = g_head_crun;
  while (cur)
    {
    next = cur->next;
    if (cur->crun_pid != 0)
      {
      cur->crun_pid_count += 1;
      waitpid(cur->crun_pid, &wstatus, WNOHANG);
      if (kill(cur->crun_pid, 0))
        {
        if (cur->crun_pid_count > 100)
          {
          KERR("ERROR %d %s", cur->crun_pid, cur->name);
          memset(resp, 0, MAX_PATH_LEN);
          snprintf(resp, MAX_PATH_LEN-1,
                   "cloonsuid_crun_killed name=%s crun_pid=%d",
                   cur->name, cur->crun_pid);
          rpct_send_sigdiag_msg(llid, type_hop_suid_power, resp);
          urgent_destroy_crun(cur);
          }
        }
      }
    cur = next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void crun_recv_poldiag_msg(int llid, int tid, char *line)
{
  KERR("ERROR %s", line);
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
  t_crun *cur = find_crun(name);
  memset(resp, 0, MAX_PATH_LEN);
  memset(bulk_image, 0, MAX_PATH_LEN);
  snprintf(bulk_image, MAX_PATH_LEN-1, "%s/%s", bulk, image); 
  snprintf(resp, MAX_PATH_LEN-1,
           "cloonsuid_crun_create_net_resp_ko name=%s", name);
  snprintf(nspace, MAX_NAME_LEN-1, "%s_%d_%d",
           BASE_NAMESPACE, cloonix_rank, vm_id);
  if (cur != NULL)
    KERR("ERROR %s", name);
  else if (access(bulk_image, F_OK))
    KERR("ERROR %s %s", name, bulk_image);
  else if (nb_eth >= MAX_ETH_VM)
    KERR("ERROR %s too big nb_eth:%d", name, nb_eth);
  else
    {
    alloc_crun(name, bulk, image, nb_eth, nspace,
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
  t_crun *cur;
  cur = find_crun(name);
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
        if (crun_utils_create_net(cur->image, name, cur->cnt_dir,
                                 cur->nspace, cur->cloonix_rank, cur->vm_id,
                                 cur->nb_eth, cur->eth_mac, cur->agent_dir,
                                 cur->customer_launch))
          {
          KERR("ERROR %s", name);
          snprintf(resp, MAX_PATH_LEN-1,
          "cloonsuid_crun_create_net_resp_ko name=%s", name);
          }
        else
          {
          snprintf(resp, MAX_PATH_LEN-1,
          "cloonsuid_crun_create_net_resp_ok name=%s", name);
          }
        }
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void create_config_json(char *line, char *resp,
                               char *name, int is_persistent)
{
  t_crun *cur;
  char path[MAX_PATH_LEN];
  char *cnt_dir, *image;
  cur = find_crun(name);
  if (cur == NULL)
    {
    KERR("ERROR %s", line);
    snprintf(resp, MAX_PATH_LEN-1,
             "cloonsuid_crun_create_config_json_resp_ko name=%s",
             name);
    }
  else
    {
    image = cur->image;
    if (is_persistent)
      {
      memset(cur->rootfs_path, 0, MAX_PATH_LEN);
      snprintf(cur->rootfs_path, MAX_PATH_LEN-1, "%s/%s", get_mnt_loop_dir(), image);
      cur->is_persistent = is_persistent;
      }
    cnt_dir = cur->cnt_dir;
    memset(path, 0, MAX_PATH_LEN);
    snprintf(path, MAX_PATH_LEN-1, "%s/%s", cnt_dir, cur->name);
    if (crun_utils_create_config_json(path,cur->rootfs_path,cur->nspace_path,
                                     cur->mountbear, cur->mounttmp,
                                     is_persistent))
      {
      KERR("ERROR %s", line);
      snprintf(resp, MAX_PATH_LEN-1,
               "cloonsuid_crun_create_config_json_resp_ko name=%s",
               name);
      }
    else
      {
      snprintf(resp, MAX_PATH_LEN-1,
               "cloonsuid_crun_create_config_json_resp_ok name=%s",
               name);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void create_loop_img(char *line, char *resp,
                            char *name, int is_persistent)
{
  t_crun *cur;
  cur = find_crun(name);
  if (cur == NULL)
    {
    KERR("ERROR %s", line);
    snprintf(resp, MAX_PATH_LEN-1,
             "cloonsuid_crun_create_loop_img_resp_ko name=%s", name);
    }
  else
    {
    if (loop_img_add(name, cur->bulk, cur->image, cur->cnt_dir, is_persistent))
      {
      KERR("ERROR %s", line);
      snprintf(resp, MAX_PATH_LEN-1,
               "cloonsuid_crun_create_loop_img_resp_ko name=%s", name);
      }
    else
      {
      snprintf(resp, MAX_PATH_LEN-1,
               "cloonsuid_crun_create_loop_img_resp_ok name=%s", name);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void create_overlay(char *line, char *resp,
                           char *name, int is_persistent)
{
  t_crun *cur;
  char path[MAX_PATH_LEN];
  char *cnt_dir;
  char *loop_img;
  cur = find_crun(name);
  if (cur == NULL)
    {
    KERR("ERROR %s", line);
    snprintf(resp, MAX_PATH_LEN-1,
             "cloonsuid_crun_create_overlay_resp_ko name=%s", name);
    }
  else 
    {
    loop_img = loop_img_get(name, cur->bulk, cur->image);
    if (loop_img == NULL)
      {
      KERR("ERROR %s", line);
      snprintf(resp, MAX_PATH_LEN-1,
               "cloonsuid_crun_create_overlay_resp_ko name=%s", name);
      }
    else
      { 
      cnt_dir = cur->cnt_dir;
      memset(path, 0, MAX_PATH_LEN);
      snprintf(path, MAX_PATH_LEN-1, "%s/%s", cnt_dir, cur->name);
      if (crun_utils_create_overlay(path, loop_img, is_persistent))
        {
        KERR("ERROR %s", line);
        snprintf(resp, MAX_PATH_LEN-1,
                 "cloonsuid_crun_create_overlay_resp_ko name=%s", name);
        }
      else
        {
        snprintf(resp, MAX_PATH_LEN-1,
                 "cloonsuid_crun_create_overlay_resp_ok name=%s", name);
        }
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void create_crun_start(char *line, char *resp, char *name)
{
  t_crun *cur;
  cur = find_crun(name);
  if (cur == NULL)
    {
    KERR("ERROR %s", line);
    snprintf(resp, MAX_PATH_LEN-1,
             "cloonsuid_crun_create_crun_start_resp_ko name=%s", name);
    }
  else
    {
    cur->crun_pid = crun_utils_create_crun_create(cur->cnt_dir, cur->name);
    if (cur->crun_pid == 0)
      {
      KERR("ERROR %s", line);
      snprintf(resp, MAX_PATH_LEN-1,
               "cloonsuid_crun_create_crun_start_resp_ko name=%s", name);
      }
    else if (crun_utils_create_crun_start(name))
      {
      KERR("ERROR %s", line);
      snprintf(resp, MAX_PATH_LEN-1,
               "cloonsuid_crun_create_crun_start_resp_ko name=%s", name);
      }
    else
      {
      snprintf(resp, MAX_PATH_LEN-1,
      "cloonsuid_crun_create_crun_start_resp_ok name=%s crun_pid=%d",
      name, cur->crun_pid);
      }
    crun_utils_delete_net_nspace(cur->nspace);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void delete_crun(char *line, char *resp, char *nm)
{
  char cnt_dir[MAX_PATH_LEN];
  char name[MAX_NAME_LEN];
  t_crun *cur;
  cur = find_crun(nm);
  if (cur == NULL)
    {
    KERR("ERROR %s", line);
    snprintf(resp, MAX_PATH_LEN-1,
             "cloonsuid_crun_delete_resp_ko name=%s", nm);
    }
  else
    {
    memset(cnt_dir, 0, MAX_PATH_LEN);
    memset(name, 0, MAX_NAME_LEN);
    strncpy(cnt_dir, cur->cnt_dir, MAX_PATH_LEN-1); 
    strncpy(name, cur->name, MAX_NAME_LEN-1); 
    if (crun_utils_delete_crun_stop(nm, cur->crun_pid))
      KERR("ERROR %s", line);
    crun_utils_delete_overlay(cur->name, cur->cnt_dir, cur->bulk,
                             cur->image, cur->is_persistent);
    snprintf(resp, MAX_PATH_LEN-1,
             "cloonsuid_crun_delete_resp_ok name=%s", nm);
    free_crun(cur);
    unlink_all_files(cnt_dir, name);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void crun_recv_sigdiag_msg(int llid, int tid, char *line)
{
  t_crun *cur;
  char name[MAX_NAME_LEN];
  char bulk[MAX_PATH_LEN];
  char image[MAX_NAME_LEN];
  char resp[MAX_PATH_LEN];
  char cnt_dir[MAX_PATH_LEN];
  char agent_dir[MAX_PATH_LEN];
  char customer_launch[MAX_PATH_LEN];
  char mac[6], *ptr;
  int len, num, nb_eth, cloonix_rank, vm_id, is_persistent;

  cloonix_rank = get_cloonix_rank();
  memset(resp, 0, MAX_PATH_LEN);
  if (sscanf(line, 
  "cloonsuid_crun_create_net name=%s bulk=%s "
  "image=%s nb=%d vm_id=%d cnt_dir=%s agent_dir=%s "
  "is_persistent=%d",
  name, bulk, image, &nb_eth, &vm_id,
  cnt_dir, agent_dir, &is_persistent) == 8)
    {
    ptr = strstr(line, "customer_launch=");
    if (ptr == NULL)
      {
      KERR("ERROR %s", line);
      snprintf(resp, MAX_PATH_LEN-1,
               "cloonsuid_crun_create_net_resp_ko name=%s", name);
      } 
    else if (loop_img_check(bulk, image, is_persistent))
      {
      KERR("ERROR FILE SYSTEM BUSY (persistent is unique) %s", line);
      snprintf(resp, MAX_PATH_LEN-1,
               "cloonsuid_crun_create_net_resp_ko name=%s", name);
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
  "cloonsuid_crun_create_eth name=%s num=%d "
  "mac=0x%02hhx:0x%02hhx:0x%02hhx:0x%02hhx:0x%02hhx:0x%02hhx",
  name, &num, &(mac[0]),&(mac[1]),&(mac[2]),&(mac[3]),&(mac[4]),&(mac[5])))
    {
    create_net_eth(line, resp, name, num, mac);
    }
  else if (sscanf(line,
  "cloonsuid_crun_create_config_json name=%s is_persistent=%d",
  name, &is_persistent) == 2)
    {
    create_config_json(line, resp, name, is_persistent);
    }
  else if (sscanf(line,
  "cloonsuid_crun_create_loop_img name=%s is_persistent=%d",
  name, &is_persistent) == 2)
    {
    create_loop_img(line, resp, name, is_persistent);
    }
  else if (sscanf(line,
  "cloonsuid_crun_create_overlay name=%s is_persistent=%d",
  name, &is_persistent) == 2)
    {
    create_overlay(line, resp, name, is_persistent);
    }
  else if (sscanf(line,
  "cloonsuid_crun_create_crun_start name=%s", name) == 1)
    {
    create_crun_start(line, resp, name);
    }
  else if (sscanf(line, 
  "cloonsuid_crun_delete name=%s", name) == 1)
    {
    delete_crun(line, resp, name);
    }
  else if (sscanf(line,
  "cloonsuid_crun_ERROR name=%s", name) == 1)
    {
    cur = find_crun(name);
    if (cur != NULL)
      {
      KERR("ERROR MUST ERASE %s, %s", name, line);
      crun_utils_delete_net_nspace(cur->nspace);
      urgent_destroy_crun(cur);
      }
    }

  else
    KERR("ERROR %s", line);
  if (strlen(resp))
    rpct_send_sigdiag_msg(llid, tid, resp);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void crun_kill_all(void)
{
  char resp[MAX_PATH_LEN];
  t_crun *next, *cur = g_head_crun;
  while (cur)
    {
    next = cur->next;
    delete_crun("kill all crun procedure", resp, cur->name);
    cur = next;
    }
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
void crun_init(void)
{
  g_head_crun = NULL;
  crun_utils_init();
  loop_img_init();
}
/*--------------------------------------------------------------------------*/
