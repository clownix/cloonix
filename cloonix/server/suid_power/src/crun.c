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
#include "crun_utils.h"
#include "crun.h"
#include "tar_img.h"

int get_cloonix_rank(void);
char *get_net_name(void);

/****************************************************************************/
typedef struct t_crun
{
  char brandtype[MAX_NAME_LEN];
  char name[MAX_NAME_LEN];
  char nspacecrun[MAX_PATH_LEN];
  char bulk[MAX_PATH_LEN];
  char image[MAX_NAME_LEN];
  char cnt_dir[MAX_PATH_LEN];
  char agent_dir[MAX_PATH_LEN];
  char rootfs_path[MAX_PATH_LEN];
  char nspacecrun_path[2*MAX_PATH_LEN];
  char mountbear[MAX_PATH_LEN];
  char mounttmp[MAX_PATH_LEN];
  char sbin_init[MAX_PATH_LEN];
  char sh_starter[MAX_PATH_LEN];
  int uid_image;
  int nb_eth;
  t_eth_mac eth_mac[MAX_ETH_VM];
  int cloonix_rank;
  int vm_id;
  int crun_pid;
  int ovspid;
  int crun_pid_count;
  int is_persistent;
  int is_privileged;
  int subuid_start;
  int subuid_qty;
  int subgid_start;
  int subgid_qty;
  int delete_first_finalization_countdown;
  int delete_second_finalization_countdown;
  int delete_third_finalization_countdown;
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

/*****************************************************************************/
static char *random_str(void)
{
  static char str[8];
  int num, i;
  for (i=0; i<7; i++)
    {
    num = rand();
    num %= 26;
    str[i] = 'A'+ num;
    }
  str[7] = 0;
  return str;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void alloc_crun(char *name, char *bulk, char *image, int uid_image,
                       int nb_eth, char *nspacecrun, int cloonix_rank,
                       int vm_id, char *cnt_dir, char *agent_dir,
                       int is_persistent, int is_privileged,
                       char *brandtype, char *sbin_init, char *sh_starter,
                       int subuid_start, int subuid_qty,
                       int subgid_start, int subgid_qty)
{
  char *rnd = random_str();
  t_crun *cur = (t_crun *) malloc(sizeof(t_crun));
  memset(cur, 0, sizeof(t_crun));
  strncpy(cur->name, name, MAX_NAME_LEN-1);
  strncpy(cur->bulk, bulk, MAX_PATH_LEN-1);
  strncpy(cur->image, image, MAX_NAME_LEN-1);
  strncpy(cur->sbin_init, sbin_init, MAX_PATH_LEN-1);
  strncpy(cur->sh_starter, sh_starter, MAX_PATH_LEN-1);
  cur->uid_image = uid_image;
  strncpy(cur->cnt_dir, cnt_dir, MAX_PATH_LEN-1);
  strncpy(cur->agent_dir, agent_dir, MAX_PATH_LEN-1);
  strncpy(cur->nspacecrun, nspacecrun, MAX_PATH_LEN-1);
  snprintf(cur->nspacecrun_path, 2*MAX_PATH_LEN-1, "%s%s",
           PATH_NAMESPACE, nspacecrun);
  snprintf(cur->mountbear, MAX_PATH_LEN-1,"%s/%s/mnt%s", cnt_dir, name, rnd);
  snprintf(cur->mounttmp, MAX_PATH_LEN-1,"%s/%s/tmp%s", cnt_dir, name, rnd);
  cur->nb_eth = nb_eth;
  cur->cloonix_rank = cloonix_rank;
  cur->is_persistent = is_persistent;
  cur->is_privileged = is_privileged;
  cur->subuid_start = subuid_start;
  cur->subuid_qty = subuid_qty;
  cur->subgid_start = subgid_start;
  cur->subgid_qty = subgid_qty;
  strncpy(cur->brandtype, brandtype, MAX_NAME_LEN);
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

/****************************************************************************/
static void delete_first_finalization(t_crun *cur)
{   
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1,
           "%s %s/tmp", pthexec_umount_bin(), cur->mountbear);
  execute_cmd(cmd, 0, 0);
  crun_utils_delete_crun_delete(cur->name, cur->nspacecrun,
                                cur->vm_id, cur->nb_eth,
                                cur->uid_image, cur->is_privileged);
  cur->delete_second_finalization_countdown = 1;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void delete_second_finalization(t_crun *cur)
{
  if (cur->is_persistent == 0)
    crun_utils_delete_overlay1(cur->name, cur->cnt_dir);
  cur->delete_third_finalization_countdown = 1;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void delete_third_finalization(t_crun *cur)
{ 
  char pth[MAX_PATH_LEN];
  if (cur->is_persistent == 0)
    crun_utils_delete_overlay2(cur->name, cur->cnt_dir);
  if (tar_img_del(cur->name, cur->bulk, cur->image, cur->cnt_dir,
                  cur->is_persistent, cur->brandtype))
    KERR("ERROR %s %s %s", cur->name, cur->image, cur->cnt_dir);
  memset(pth, 0, MAX_PATH_LEN);
  snprintf(pth, MAX_PATH_LEN-1, "%s/%s", cur->cnt_dir, cur->name);
  unlink_sub_dir_files(pth);
  free_crun(cur);
} 
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void crun_beat(int llid)
{
  char resp[MAX_PATH_LEN];
  t_crun *next, *cur = g_head_crun;
  while (cur)
    {
    next = cur->next;
    if (cur->delete_first_finalization_countdown)
      {
      cur->delete_first_finalization_countdown -= 1;
      if (cur->delete_first_finalization_countdown == 0)
        delete_first_finalization(cur);
      }
    else if (cur->delete_second_finalization_countdown)
      {
      cur->delete_second_finalization_countdown -= 1;
      if (cur->delete_second_finalization_countdown == 0)
        delete_second_finalization(cur);
      }
    else if (cur->delete_third_finalization_countdown)
      {
      cur->delete_third_finalization_countdown -= 1;
      if (cur->delete_third_finalization_countdown == 0)
        delete_third_finalization(cur);
      }
    else if (cur->crun_pid != 0)
      {
      cur->crun_pid_count += 1;
      if (kill(cur->crun_pid, 0))
        {
        if (cur->crun_pid_count > 500)
          {
          cur->crun_pid = 0;
          KERR("ERROR %d %s", cur->crun_pid, cur->name);
          memset(resp, 0, MAX_PATH_LEN);
          snprintf(resp, MAX_PATH_LEN-1,
                   "cloonsuid_crun_killed name=%s crun_pid=%d",
                   cur->name, cur->crun_pid);
          rpct_send_sigdiag_msg(llid, type_hop_suid_power, resp);
          crun_utils_delete_crun_stop(cur->name, 0, cur->uid_image,
                                      cur->is_privileged, cur->mountbear);
          cur->delete_first_finalization_countdown = 3;
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
static void create_alloc(char *line, char *resp, char *name, char *bulk,
                         char *image, int uid_image, int nb_eth,
                         int cloonix_rank, int vm_id, char *cnt_dir,
                         char *agent_dir, int is_persistent, int is_privileged,
                         char *brandtype, char *sbin_init, char *sh_starter,
                         int subuid_start, int subuid_qty,
                         int subgid_start, int subgid_qty)
{
  char nspacecrun[MAX_PATH_LEN];
  t_crun *cur = find_crun(name);
  memset(resp, 0, MAX_PATH_LEN);
  snprintf(resp, MAX_PATH_LEN-1,
           "cloonsuid_crun_create_net_resp_ko name=%s", name);
  snprintf(nspacecrun, MAX_PATH_LEN-1, "%s_%s_%s_%d_%d",
           BASE_NAMESPACE, get_net_name(), name, cloonix_rank, vm_id);
  if (cur != NULL)
    KERR("ERROR %s", name);
  else if (nb_eth >= MAX_ETH_VM)
    KERR("ERROR %s too big nb_eth:%d", name, nb_eth);
  else
    {
    alloc_crun(name, bulk, image, uid_image, nb_eth, nspacecrun,
               cloonix_rank, vm_id, cnt_dir, agent_dir, is_persistent,
               is_privileged, brandtype, sbin_init, sh_starter,
               subuid_start, subuid_qty, subgid_start, subgid_qty);
    memset(resp, 0, MAX_PATH_LEN);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void create_net_eth(char *line, char *resp,
                           char *name, int num, char *mac) 
{
  t_crun *cur;
  cur = find_crun(name);
  if (cur != NULL)
    {
    if (num >= MAX_ETH_VM)
      KERR("ERROR %s", line);
    else
      {
      memcpy(cur->eth_mac[num].mac, mac, 6);
      if (num+1 < cur->nb_eth) 
        {
        memset(resp, 0, MAX_PATH_LEN);
        }
      else
        {
        if (crun_utils_create_net(cur->brandtype, cur->is_persistent,
                                  cur->is_privileged, cur->mountbear,
                                  cur->image, name, cur->cnt_dir,
                                  cur->nspacecrun, cur->cloonix_rank,
                                  cur->vm_id, cur->nb_eth, cur->eth_mac,
                                  cur->agent_dir))
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
static void create_config_json(char *line, char *resp, char *name,
                               int is_persistent, int is_privileged,
                               char *brandtype, char *startup_env,
                               char *vmount)
{
  int ovspid;
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
    cnt_dir = cur->cnt_dir;
    if (cur->is_persistent != is_persistent)
      KOUT("ERROR %s %s %d", name, brandtype, is_persistent);
    if (strcmp(brandtype, cur->brandtype))
      KOUT("ERROR %s %s %d", name, brandtype, is_persistent);
    image = cur->image;
    if (is_persistent)
      {
      if (!strcmp(brandtype, "brandzip"))
        snprintf(cur->rootfs_path, MAX_PATH_LEN-1, "%s/%s",
                 get_mnt_loop_dir(), image);
      else if (!strcmp(brandtype, "brandcvm"))
        snprintf(cur->rootfs_path, MAX_PATH_LEN-1, "%s/%s",
                 cur->bulk, image);
      else
        KOUT("ERROR %s %s %d", name, brandtype, is_persistent);
      }
    else
      snprintf(cur->rootfs_path, MAX_PATH_LEN-1,"%s/%s/%s",
               cnt_dir, name, ROOTFS);
    memset(path, 0, MAX_PATH_LEN);
    snprintf(path, MAX_PATH_LEN-1, "%s/%s", cnt_dir, cur->name);

    ovspid = crun_utils_get_vswitchd_pid(get_net_name());
    cur->ovspid = ovspid;
    if (ovspid == 0)
      {
      KERR("ERROR %s", line);
      snprintf(resp, MAX_PATH_LEN-1,
               "cloonsuid_crun_create_config_json_resp_ko name=%s", name);
      }
    else if (crun_utils_create_config_json(cur->image, path, cur->rootfs_path,
                                      cur->nspacecrun_path, cur->mountbear,
                                      cur->mounttmp, is_persistent,
                                      is_privileged, brandtype, startup_env,
                                      vmount, cur->name, ovspid,
                                      cur->uid_image,
                                      cur->sbin_init, cur->sh_starter,
                                      cur->subuid_start, cur->subuid_qty,
                                      cur->subgid_start, cur->subgid_qty))
      {
      KERR("ERROR %s", line);
      snprintf(resp, MAX_PATH_LEN-1,
               "cloonsuid_crun_create_config_json_resp_ko name=%s", name);
      }
    else
      {
      crun_utils_startup_env(cur->mountbear, startup_env, cur->nb_eth);
      snprintf(resp, MAX_PATH_LEN-1,
               "cloonsuid_crun_create_config_json_resp_ok name=%s", name);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void create_tar_img(char *line, char *resp, char *name,
                           int is_persistent, int is_privileged,
                           char *brandtype)
{
  t_crun *cur;
  cur = find_crun(name);
  if (cur == NULL)
    {
    KERR("ERROR %s", line);
    snprintf(resp, MAX_PATH_LEN-1,
             "cloonsuid_crun_create_tar_img_resp_ko name=%s", name);
    }
  else
    {
    if (tar_img_add(name, cur->bulk, cur->image, cur->uid_image,
                    cur->cnt_dir, is_persistent, brandtype))
      {
      KERR("ERROR %s", line);
      snprintf(resp, MAX_PATH_LEN-1,
               "cloonsuid_crun_create_tar_img_resp_ko name=%s", name);
      }
    else
      {
      snprintf(resp, MAX_PATH_LEN-1,
               "cloonsuid_crun_create_tar_img_resp_ok name=%s", name);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void create_overlay(char *line, char *resp, char *name,
                           int is_persistent, int is_privileged,
                           char *brandtype, char *bulk_rootfs)
{
  char mnt_tar_img[MAX_PATH_LEN];
  t_crun *cur;
  cur = find_crun(name);
  if (cur == NULL)
    {
    KERR("ERROR %s", line);
    snprintf(resp, MAX_PATH_LEN-1,
             "cloonsuid_crun_create_overlay_resp_ko name=%s", name);
    }
  else if (strcmp(cur->brandtype, brandtype)) 
    {
    KERR("ERROR %s", line);
    snprintf(resp, MAX_PATH_LEN-1,
             "cloonsuid_crun_create_overlay_resp_ko name=%s", name);
    }
  else
    {
    if (tar_img_rootfs_get(name, cur->bulk, cur->image, brandtype, mnt_tar_img))
      {
      KERR("ERROR %s", line);
      snprintf(resp, MAX_PATH_LEN-1,
               "cloonsuid_crun_create_overlay_resp_ko name=%s", name);
      }
    else
      { 
      if (crun_utils_create_overlay(cur->mountbear, cur->mounttmp,
                                    cur->name, cur->cnt_dir, mnt_tar_img,
                                    cur->image, cur->uid_image,
                                    cur->agent_dir, bulk_rootfs,
                                    is_persistent, is_privileged,
                                    brandtype))
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
    crun_utils_prepare_with_delete(name, cur->nspacecrun, cur->vm_id,
                                   cur->nb_eth, cur->uid_image,
                                   cur->is_privileged);
    cur->crun_pid = crun_utils_create_crun_create(cur->cnt_dir, name,
                                            cur->ovspid, cur->uid_image,
                                            cur->is_privileged);
    if (cur->crun_pid == 0)
      {
      KERR("ERROR %s", line);
      snprintf(resp, MAX_PATH_LEN-1,
               "cloonsuid_crun_create_crun_start_resp_ko name=%s", name);
      }
    else if (crun_utils_user_nspacecrun_init(cur->nspacecrun,cur->crun_pid))
      {
      KERR("ERROR %s", line);
      snprintf(resp, MAX_PATH_LEN-1,
               "cloonsuid_crun_create_crun_start_resp_ko name=%s", name);
      }
    else if (crun_utils_user_eth_create(name, cur->nspacecrun,
                                        cur->cloonix_rank, cur->vm_id,
                                        cur->nb_eth, cur->eth_mac))
      {
      KERR("ERROR %s", line);
      snprintf(resp, MAX_PATH_LEN-1,
               "cloonsuid_crun_create_crun_start_resp_ko name=%s", name);
      }
    else if (crun_utils_create_crun_start(name, cur->ovspid, cur->uid_image,
                                          cur->is_privileged))
      {
      KERR("ERROR %s", line);
      snprintf(resp, MAX_PATH_LEN-1,
               "cloonsuid_crun_create_crun_start_resp_ko name=%s", name);
      }
    else
      {
      snprintf(resp, MAX_PATH_LEN-1,
      "cloonsuid_crun_create_crun_start_resp_ok name=%s crun_pid=%d mountbear=%s",
      name, cur->crun_pid, cur->mountbear);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void delete_crun(char *line, char *resp, char *name)
{
  t_crun *cur;
  cur = find_crun(name);
  if (cur == NULL)
    {
    KERR("ERROR %s", line);
    snprintf(resp, MAX_PATH_LEN-1,
             "cloonsuid_crun_delete_resp_ko name=%s", name);
    }
  else
    {
    crun_utils_delete_crun_stop(name, cur->crun_pid, cur->uid_image,
                                cur->is_privileged, cur->mountbear);
    cur->crun_pid = 0;
    cur->delete_first_finalization_countdown = 3;
    snprintf(resp, MAX_PATH_LEN-1,
             "cloonsuid_crun_delete_resp_ok name=%s", name);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void extract_env_vmount(char *line, char *startup_env, char *vmount)
{
  int len, lenv;
  char  *ptr1, *ptr2, *ptrv1, *ptrv2;
  memset(startup_env, 0, MAX_PATH_LEN);
  memset(vmount, 0, 4*MAX_PATH_LEN);
  ptr1 = strstr(line, "<startup_env_keyid>");
  if(!ptr1)
    KOUT("%s", line);
  ptr2 = strstr(line, "</startup_env_keyid>");
  if(!ptr2)
    KOUT("%s", line);
  len = strlen("<startup_env_keyid>");
  if (strlen(ptr1) - len >= MAX_PATH_LEN)
    KOUT("%s", line);
  ptrv1 = strstr(line, "<startup_vmount>");
  if(!ptrv1)
    KOUT("%s", line);
  ptrv2 = strstr(line, "</startup_vmount>");
  if(!ptrv2)
    KOUT("%s", line);
  lenv = strlen("<startup_vmount>");
  if (strlen(ptrv1) - lenv >= 4*MAX_PATH_LEN)
    KOUT("%s", line);
  *ptr2 = 0;
  *ptrv2 = 0;
  strncpy(startup_env, ptr1+len, MAX_PATH_LEN-1);
  strncpy(vmount, ptrv1+lenv, MAX_PATH_LEN-1);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void crun_recv_sigdiag_msg(int llid, int tid, char *line)
{
  t_crun *cur;
  char name[MAX_NAME_LEN];
  char brandtype[MAX_NAME_LEN];
  char bulk[MAX_PATH_LEN];
  char bulk_rootfs[MAX_PATH_LEN];
  char image[MAX_NAME_LEN];
  char resp[MAX_PATH_LEN];
  char cnt_dir[MAX_PATH_LEN];
  char agent_dir[MAX_PATH_LEN];
  char mac[6], *ptr;
  int  uid_image, num, nb_eth, cloonix_rank, vm_id;
  int  is_persistent, is_privileged;
  char startup_env[MAX_PATH_LEN];
  char sbin_init[MAX_PATH_LEN];
  char sh_starter[MAX_PATH_LEN];
  char vmount[4*MAX_PATH_LEN];
  int subuid_start, subuid_qty, subgid_start, subgid_qty;

  cloonix_rank = get_cloonix_rank();
  memset(resp, 0, MAX_PATH_LEN);
  if (sscanf(line, 
  "cloonsuid_crun_create_net name=%s bulk=%s "
  "image=%s nb=%d vm_id=%d cnt_dir=%s agent_dir=%s "
  "is_persistent=%d is_privileged=%d brandtype=%s",
  name, bulk, image, &nb_eth, &vm_id,
  cnt_dir, agent_dir, &is_persistent, &is_privileged, brandtype) == 10)
    {
    if (tar_img_check(bulk, image, &uid_image, is_persistent,
                      brandtype, sbin_init, sh_starter,
                      &subuid_start, &subuid_qty,
                      &subgid_start, &subgid_qty))
      {
      KERR("ERROR %s", line);
      snprintf(resp, MAX_PATH_LEN-1,
               "cloonsuid_crun_create_net_resp_ko name=%s", name);
      } 
    else
      {
      if (uid_image == 0)
        {
        if (is_privileged == 0)
          {
          is_privileged = 1;
          }
        }
      create_alloc(line, resp, name, bulk, image, uid_image, nb_eth,
                   cloonix_rank, vm_id, cnt_dir, agent_dir,
                   is_persistent, is_privileged, brandtype, sbin_init,
                   sh_starter, subuid_start, subuid_qty,
                   subgid_start, subgid_qty);
      }
    }
  else if (sscanf(line,
  "cloonsuid_crun_create_eth name=%s num=%d "
  "mac=0x%02hhx:0x%02hhx:0x%02hhx:0x%02hhx:0x%02hhx:0x%02hhx",
  name, &num, &(mac[0]),&(mac[1]),&(mac[2]),&(mac[3]),&(mac[4]),&(mac[5])))
    {
    cur = find_crun(name);
    if (cur == NULL)
      KERR("ERROR %s, %s", name, line);
    else
      create_net_eth(line, resp, name, num, mac);
    }
  else if (sscanf(line,
  "cloonsuid_crun_create_config_json name=%s", name) == 1)
    {
    cur = find_crun(name);
    if (cur == NULL)
      KERR("ERROR %s, %s", name, line);
    else
      {
      extract_env_vmount(line, startup_env, vmount);
      create_config_json(line, resp, name, cur->is_persistent,
                         cur->is_privileged, cur->brandtype,
                         startup_env, vmount);
      }
    }
  else if (sscanf(line,
  "cloonsuid_crun_create_tar_img name=%s", name) == 1)
    {
    cur = find_crun(name);
    if (cur == NULL)
      KERR("ERROR %s, %s", name, line);
    else
      {
      create_tar_img(line, resp, name, cur->is_persistent,
                     cur->is_privileged, cur->brandtype);
      }
    }
  else if (sscanf(line,
  "cloonsuid_crun_create_overlay name=%s", name) == 1)
    {
    cur = find_crun(name);
    if (cur == NULL)
      KERR("ERROR %s, %s", name, line);
    else
      { 
      snprintf(bulk_rootfs, MAX_PATH_LEN-1, "%s/%s", cur->bulk, cur->image);
      create_overlay(line, resp, name, cur->is_persistent, cur->is_privileged,
                     cur->brandtype, bulk_rootfs);
      }
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
      crun_utils_delete_crun_stop(cur->name, cur->crun_pid, cur->uid_image,
                                  cur->is_privileged, cur->mountbear);
      cur->crun_pid = 0;
      cur->delete_first_finalization_countdown = 3;
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
void crun_init(char *var_root)
{
  g_head_crun = NULL;
  crun_utils_init(var_root);
  tar_img_init();
}
/*--------------------------------------------------------------------------*/
