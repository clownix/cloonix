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
#include <errno.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>


#include "io_clownix.h"
#include "config_json.h"
#include "crun.h"
#include "net_phy.h"
#include "crun_utils.h"
#include "utils.h"


struct t_elem;
/****************************************************************************/
typedef struct t_loop
{
  char bulktarget[MAX_PATH_LEN];
  char brandtype[MAX_NAME_LEN];
  int uid_image;
  int is_persistent;
  struct t_elem *head_elem;
  struct t_loop *prev;
  struct t_loop *next;
} t_loop;
/*--------------------------------------------------------------------------*/
typedef struct t_elem
{
  t_loop *loop;
  char name[MAX_NAME_LEN];
  struct t_elem *prev;
  struct t_elem *next;
} t_elem;
/*--------------------------------------------------------------------------*/


static t_loop *g_head_loop;


/****************************************************************************/
static int get_user_name(int uid_image, char *user_name)
{
  FILE *fh;
  char line[MAX_PATH_LEN];
  char no_use1[MAX_PATH_LEN];
  int i, uid, gid, found = 0, result = -1;
  char *ptr;
  
  fh = fopen("/etc/passwd", "r");
  if (fh)
    {
    while(fgets(line, MAX_PATH_LEN-1, fh))
      {
      for (i=0; i<4; i++)
        {
        ptr = strchr(line, ':');
        if (!ptr)
          KERR("ERROR %s", line);
        else
          *ptr = ' ';
        }
      if (sscanf(line, "%s %s %d %d ", user_name, no_use1, &uid, &gid) == 4)
        {
        if ((uid == uid_image) && (gid == uid_image))
          {
          found = 1;
          result = 0;
          break;
          }
        }
      }
    fclose(fh);
    } 
  else
    KERR("ERROR READING /etc/passwd");
  if (!found)
    KERR("ERROR /etc/passwd does not have uid_image %d", uid_image);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void get_subuid_subgid(char *user_name,
                              int *subuid_start, int *subuid_qty,
                              int *subgid_start, int *subgid_qty)
{
  int i, found, ustart, uqty, gstart, gqty;
  char line[MAX_PATH_LEN];
  char name[MAX_PATH_LEN];
  FILE *fh;
  char *ptr;
  found = 0;
  fh = fopen("/etc/subuid", "r");
  if (fh)
    {
    while(fgets(line, MAX_PATH_LEN-1, fh))
      {
      for (i=0; i<2; i++)
        {
        ptr = strchr(line, ':');
        if (!ptr)
          KERR("ERROR %s", line);
        else
          *ptr = ' ';
        }
      if (sscanf(line, "%s %d %d", name, &ustart, &uqty) == 3)
        {
        if (!strcmp(name, user_name))
          {
          found = 1;
          break;
          }
        }
      }
    fclose(fh);
    }
  else 
    KERR("ERROR READING /etc/subuid");
  if (!found)
    KERR("ERROR /etc/subuid does not have %s", user_name);
  else
    {
    found = 0;
    fh = fopen("/etc/subgid", "r");
    if (fh)
      {
      while(fgets(line, MAX_PATH_LEN-1, fh))
        {
        for (i=0; i<2; i++)
          {
          ptr = strchr(line, ':');
          if (!ptr)
            KERR("ERROR %s", line);
          else
            *ptr = ' ';
          }
        if (sscanf(line, "%s %d %d", name, &gstart, &gqty) == 3)
          {
          if (!strcmp(name, user_name))
            {
            found = 1;
            break;
            }
          }
        }
      fclose(fh);
      }
    else
      KERR("ERROR READING /etc/subuid");
    if (!found)
      KERR("ERROR /etc/subuid does not have %s", user_name);
    else
      {
      *subuid_start = ustart;
      *subuid_qty = uqty;
      *subgid_start = gstart;
      *subgid_qty = gqty;
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int get_path_uid_image(char *bulktarget, int *uid_image,
                              int *subuid_start, int *subuid_qty,
                              int *subgid_start, int *subgid_qty)
{
  struct stat statbuf;
  char user_name[MAX_PATH_LEN];
  int result = -1;
  *uid_image = 0;
  *subuid_start = 0;
  *subuid_qty = 0;
  *subgid_start = 0;
  *subgid_qty = 0;
  if (stat(bulktarget, &statbuf) == 0)
    {
    *uid_image = statbuf.st_uid;
    result = 0;
    }
  else
    {
    KERR("ERROR with %s", bulktarget);
    }
  if ((*uid_image) == 0)
    {
    }
  else
    {
    if (!get_user_name(*uid_image, user_name))
      {
      get_subuid_subgid(user_name, subuid_start, subuid_qty,
                        subgid_start, subgid_qty);
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_loop *find_bulk_target(char *bulktarget, char *brandtype)
{
  t_loop *cur = g_head_loop;
  while(cur)
    {
    if ((!strcmp(cur->bulktarget, bulktarget)) &&
        (!strcmp(cur->brandtype, brandtype)))
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_elem *find_elem(char *name)
{
  t_elem *elem = NULL;
  t_loop *cur = g_head_loop;
  while(cur)
    {
    elem = cur->head_elem;
    while(elem)
      {
      if (!strcmp(elem->name, name))
        break;
      elem = elem->next;
      }
    if (elem)
      break;
    cur = cur->next;
    }
  return elem;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_loop *alloc_loop(char *bulktarget, int is_persistent,
                          char *brandtype, int uid_image)
{
  t_loop *cur = (t_loop *) malloc(sizeof(t_loop));
  memset(cur, 0, sizeof(t_loop));
  strncpy(cur->bulktarget, bulktarget, MAX_PATH_LEN-1);
  strncpy(cur->brandtype, brandtype, MAX_NAME_LEN-1);
  cur->is_persistent = is_persistent;
  cur->uid_image = uid_image;
  cur->next = g_head_loop;
  if (g_head_loop)
    g_head_loop->prev = cur;
  g_head_loop = cur; 
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void free_loop(t_loop *cur)
{
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur->next)
    cur->next->prev = cur->prev;
  if (cur == g_head_loop)
    g_head_loop = cur->next;
  free(cur);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void alloc_loop_elem(t_loop *cur, char *name)
{
  t_elem *elem = (t_elem *) malloc(sizeof(t_elem));
  memset(elem, 0, sizeof(t_elem));
  strncpy(elem->name, name, MAX_NAME_LEN-1); 
  elem->loop = cur;
  elem->next = cur->head_elem; 
  if (cur->head_elem)
    cur->head_elem->prev = elem;
  cur->head_elem = elem;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_loop *free_loop_elem(t_elem *elem)
{
  t_loop *loop = elem->loop;
  if (elem->prev)
    elem->prev->next = elem->next;
  if (elem->next)
    elem->next->prev = elem->prev;
  if (elem == loop->head_elem)
    loop->head_elem = elem->next;
  free(elem);
  return loop;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int zipmount_create(char *bulk, char *image, int uid_image,
                           int is_persistent)
{
  int child_pid, wstatus, result = -1;
  char cmd[2*MAX_PATH_LEN];
  char resp[MAX_PATH_LEN];
  char path[MAX_PATH_LEN];
  FILE *fp;

  memset(resp, 0, MAX_PATH_LEN);
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1, "%s/%s", get_mnt_loop_dir(), image);
  if (mkdir(path, 0777))
    {
    if (errno != EEXIST)
      KERR("ERROR %s, %d", path, errno);
    else
      KERR("ERROR ALREADY EXISTS %s", path);
    }
  else
    {
    if (is_persistent)
      {
      memset(cmd, 0, 2*MAX_PATH_LEN);
      snprintf(cmd, 2*MAX_PATH_LEN-1, "%s -o uid=%d -o gid=%d %s/%s %s/%s",
               pthexec_fusezip_bin(), uid_image, uid_image, bulk, image,
               get_mnt_loop_dir(), image);
      if (execute_cmd(cmd, 1, uid_image))
        KERR("ERROR %s", cmd);
      else
        result = 0;
      }
    else
      {
      memset(cmd, 0, 2*MAX_PATH_LEN);
      snprintf(cmd, 2*MAX_PATH_LEN-1, "%s -o uid=%d -o gid=%d -r %s/%s  %s/%s",
      pthexec_fusezip_bin(), uid_image, uid_image, bulk, image,
      get_mnt_loop_dir(), image);
      if (execute_cmd(cmd, 1, 0))
        KERR("ERROR %s", cmd);
      else
        result = 0;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int zipmount_exists(char *bulk, char *image,
                           int is_persistent, int must_exist)
{
  int child_pid, wstatus, result = 0;
  char cmd[2*MAX_PATH_LEN];
  char fuse[MAX_PATH_LEN];
  char mnt[MAX_PATH_LEN];
  char resp[MAX_PATH_LEN];
  FILE *fp;
  char *ptr;

  memset(cmd, 0, 2*MAX_PATH_LEN);
  memset(mnt, 0, MAX_PATH_LEN);
  memset(fuse, 0, MAX_PATH_LEN);
  snprintf(mnt, MAX_PATH_LEN-1, "%s/%s", get_mnt_loop_dir(), image);
  snprintf(fuse, MAX_PATH_LEN-1,
           "\"cloonix-fuse-zip on %s/%s type fuse.cloonix-fuse-zip\"",
           get_mnt_loop_dir(), image);
  snprintf(cmd, 2*MAX_PATH_LEN-1,
  "%s | %s %s | %s '{print $3}'", pthexec_mount_bin(),
                                  pthexec_grep_bin(),
                                  fuse, pthexec_awk_bin());
  fp = cmd_lio_popen(cmd, &child_pid, 0);
  if (fp == NULL)
    {
    log_write_req("PREVIOUS CMD KO 4");
    KERR("ERROR %s", cmd);
    }
  else
    {
    if (fgets(resp, MAX_PATH_LEN-1, fp))
      {
      ptr = strchr(resp, '\r');
      if (ptr)
        *ptr = 0;
      ptr = strchr(resp, '\n');
      if (ptr)
        *ptr = 0;
      if (strcmp(resp, mnt))
        {
        log_write_req("PREVIOUS CMD KO 5");
        KERR("ERROR %s %s %s %s", bulk, image, resp, mnt);
        }
      else
        {
        result = 1;
        if (!must_exist)
          KERR("ERROR %s %s", cmd, resp);
        }
      }
    else
      {
      if (must_exist)
        KERR("ERROR %s", cmd);
      }
    pclose(fp);
    if (force_waitpid(child_pid, &wstatus))
      KERR("ERROR");
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void mount_del_item(char *cnt_dir, char *name, char *ident)
{
  char cmd[2*MAX_PATH_LEN];
  char resp[MAX_PATH_LEN];
  FILE *fp;
  int child_pid, wstatus;
  memset(cmd, 0, 2*MAX_PATH_LEN);
  snprintf(cmd, 2*MAX_PATH_LEN-1,
           "%s | %s %s/%s/%s | %s '{print $3}'",
           pthexec_mount_bin(), pthexec_grep_bin(),
           cnt_dir, name, ident, pthexec_awk_bin());
  fp = cmd_lio_popen(cmd, &child_pid, 0);
  if (fp == NULL)
    {   
    log_write_req("PREVIOUS CMD KO 6");
    KERR("ERROR %s", cmd);
    }
  else
    {    
    if (fgets(resp, MAX_PATH_LEN-1, fp))
      {
      memset(cmd, 0, 2*MAX_PATH_LEN);
      snprintf(cmd, 2*MAX_PATH_LEN-1, "%s %s", pthexec_umount_bin(), resp);
      if (execute_cmd(cmd, 1, 0))
        KERR("ERROR %s", cmd);
      }
    pclose(fp);
    if (force_waitpid(child_pid, &wstatus))
      KERR("ERROR");
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void mount_del(char *bulk, char *image, char *cnt_dir, char *name)
{
  char cmd[2*MAX_PATH_LEN];
  char resp[MAX_PATH_LEN];
  FILE *fp;
  int child_pid, wstatus;
  mount_del_item(cnt_dir, name, "rootfs");
  mount_del_item(cnt_dir, name, "tmpfs");
  mount_del_item(cnt_dir, name, "mnt");
  memset(cmd, 0, 2*MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1,
           "%s | %s fuse.cloonix-fuse-zip | %s %s/%s | %s '{print $3}'",
           pthexec_mount_bin(), pthexec_grep_bin(),
           pthexec_grep_bin(), get_mnt_loop_dir(),
           image, pthexec_awk_bin());
  fp = cmd_lio_popen(cmd, &child_pid, 0);
  if (fp == NULL)
    {
    log_write_req("PREVIOUS CMD KO 7");
    KERR("ERROR %s", cmd);
    }
  else
    {
    if (fgets(resp, MAX_PATH_LEN-1, fp))
      {
      memset(cmd, 0, 2*MAX_PATH_LEN);
      snprintf(cmd, 2*MAX_PATH_LEN-1, "%s %s", pthexec_umount_bin(), resp);
      if (execute_cmd(cmd, 1, 0))
        KERR("ERROR %s", cmd);
      }
    pclose(fp);
    if (force_waitpid(child_pid, &wstatus))
      KERR("ERROR");
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int tar_img_add(char *name, char *bulk, char *image, int uid_image,
                char *cnt_dir, int is_persistent, char *brandtype)
{
  int result = -1;
  char bulktarget[MAX_PATH_LEN];
  t_loop *cur;
  t_elem *elem;
  memset(bulktarget, 0, MAX_PATH_LEN);
  snprintf(bulktarget, MAX_PATH_LEN-1, "%s/%s", bulk, image);
  cur = find_bulk_target(bulktarget, brandtype); 
  elem = find_elem(name);
  if ((!strcmp(image, "self_rootfs")) && (is_persistent))
    KERR("ERROR %s %s %s", name, bulk, image);
  else if (elem)
    KERR("ERROR %s %s %s", name, bulk, image);
  else if (cur)
    {
    if (cur->is_persistent != is_persistent)
      KERR("ERROR %s %s %s %d %d", name, bulk, image,
                                   cur->is_persistent, is_persistent);
    else if (is_persistent)
      KERR("ERROR %s %s %s", name, bulk, image);
    else if ((!strcmp(brandtype, "brandzip")) &&
             (!zipmount_exists(bulk, image, 0, 1)))
      KERR("ERROR %s %s %s", name, bulk, image);
    else
      {
      alloc_loop_elem(cur, name);
      result = 0;
      }
    }
  else if (!strcmp(brandtype, "brandzip"))
    {
    if (zipmount_exists(bulk, image, is_persistent, 0))
      {
      KERR("ERROR %s %s %s", name, bulk, image);
      mount_del(bulk, image, cnt_dir, name);
      }
    if (zipmount_exists(bulk, image, is_persistent, 0))
      {
      KERR("ERROR %s %s %s", name, bulk, image);
      }
    else
      {
      if (zipmount_create(bulk, image, uid_image, is_persistent))
        KERR("ERROR %s %s %s", name, bulk, image);
      else
        {
        cur = alloc_loop(bulktarget, is_persistent,
                         brandtype, uid_image);
        alloc_loop_elem(cur, name);
        result = 0;
        }
      }
    }
  else if (!strcmp(brandtype, "brandcvm"))
    {
    cur = alloc_loop(bulktarget, is_persistent,
                     brandtype, uid_image);
    alloc_loop_elem(cur, name);
    result = 0;
    }
  else
    KOUT("ERROR %s %s %s", name, bulk, image);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int tar_img_del(char *name, char *bulk, char *image, char *cnt_dir,
                int is_persistent, char *brandtype)
{
  char mnt[MAX_PATH_LEN];
  int count = 0, result = -1;
  t_loop *loop;
  t_elem *elem = find_elem(name);
  char resp[MAX_PATH_LEN];

  if (elem == NULL)
    KERR("ERROR %s", name);
  else
    {
    result = 0;
    loop = free_loop_elem(elem);
    if (loop == NULL)
      KOUT("ERROR %s %s %s", name, bulk, image);
    if (strcmp(loop->brandtype, brandtype))
      KERR("ERROR %s %s %s", name, bulk, image);
    if ((strcmp(loop->brandtype, "brandzip")) &&
        (strcmp(loop->brandtype, "brandcvm")))
      KERR("ERROR %s %s %s", name, bulk, image);
    if (loop->is_persistent != is_persistent)
      KERR("ERROR %s %s %s %d %d", name, bulk, image,
            loop->is_persistent, is_persistent);
    if (loop->head_elem == NULL)
      {
      if (!strcmp(loop->brandtype, "brandzip"))
        {
        if (!zipmount_exists(bulk, image, is_persistent, 1))
          KERR("ERROR %s %s %s", name, bulk, image);
        mount_del(bulk, image, cnt_dir, name);
        if (zipmount_exists(bulk, image, is_persistent, 0))
          KERR("ERROR %s %s %s", name, bulk, image);
        snprintf(mnt, MAX_PATH_LEN-1, "%s/%s", get_mnt_loop_dir(), image);
        if (rmdir(mnt))
          KERR("ERROR %s %s %d", mnt, image, errno);
        }
      free_loop(loop);
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int tar_img_rootfs_get(char *name, char *bulk, char *image,
                       char *brandtype, char *mnt_tar_img)
{
  int result = -1;
  t_loop *cur;
  t_elem *elem;
  char bulktarget[MAX_PATH_LEN];
  memset(bulktarget, 0, MAX_PATH_LEN);
  memset(mnt_tar_img, 0, MAX_PATH_LEN);
  snprintf(bulktarget, MAX_PATH_LEN-1, "%s/%s", bulk, image);
  cur = find_bulk_target(bulktarget, brandtype); 
  elem = find_elem(name);
  if (cur == NULL)
    KERR("ERROR %s %s %s", name, bulk, image);
  else if (strcmp(cur->brandtype, brandtype))
    KERR("ERROR %s %s %s", name, bulk, image);
  else if (elem == NULL)
    KERR("ERROR %s %s %s", name, bulk, image);
  else if (!strcmp(cur->brandtype, "brandzip"))
    { 
    snprintf(mnt_tar_img, MAX_PATH_LEN-1, "%s/%s", get_mnt_loop_dir(), image);
    if (!zipmount_exists(bulk, image, cur->is_persistent, 1))
      KERR("ERROR %s %s %s", name, bulk, image);
    else
      result = 0;
    }
  else if (!strcmp(cur->brandtype, "brandcvm"))
    { 
    snprintf(mnt_tar_img, MAX_PATH_LEN-1, "%s", bulktarget);
    result = 0;
    }
  else
    KERR("ERROR %s %s %s", name, bulk, image);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int tar_img_check(char *bulk, char *image, int *uid_image, 
                  int is_persistent, char *brandtype,
                  char *sbin_init, char *sh_starter,
                  int *subuid_start, int *subuid_qty,
                  int *subgid_start, int *subgid_qty)
{
  int result = 0;
  char bulktarget[MAX_PATH_LEN];
  struct stat sb;
  t_loop *cur;
  memset(sbin_init, 0, MAX_PATH_LEN);
  memset(sh_starter, 0, MAX_PATH_LEN);
  memset(bulktarget, 0, MAX_PATH_LEN);
  snprintf(bulktarget, MAX_PATH_LEN-1, "%s/%s", bulk, image);
  cur = find_bulk_target(bulktarget, brandtype);
  if (get_path_uid_image(bulktarget, uid_image, subuid_start, subuid_qty,
                         subgid_start, subgid_qty))
    {
    KERR("ERROR %s", bulktarget);
    result = -1;
    }
  else if (!strcmp(brandtype, "brandzip"))
    {
    strcpy(sh_starter, "/bin/sh");
    strcpy(sbin_init,"/cloonixmnt/cnf_fs/cloonixsbininit");
    if (lstat(bulktarget, &sb) == -1)
      {
      KERR("ERROR %s", bulktarget);
      result = -1;
      }
    else if ((sb.st_mode & S_IFMT) != S_IFREG)
      {
      KERR("ERROR %s %d", bulktarget, (sb.st_mode & S_IFMT));
      result = -1;
      }
    else
      {
      if (is_persistent)
        {
        if (access(bulktarget, W_OK))
          {
          KERR("ERROR %s", bulktarget);
          result = -1;
          }
        }
      else
        {
        if (access(bulktarget, R_OK))
          {
          KERR("ERROR %s", bulktarget);
          result = -1;
          }
        }
      }
    }
  else if (!strcmp(brandtype, "brandcvm"))
    {
    strcpy(sh_starter, "/bin/sh");
    strcpy(sbin_init,"/cloonixmnt/cnf_fs/cloonixsbininit");
    if (is_persistent)
      {
      if (access(bulktarget, W_OK))
        {
        KERR("ERROR %s", bulktarget);
        result = -1;
        }
      }
    else
      {
      if (access(bulktarget, R_OK))
        {
        KERR("ERROR %s", bulktarget);
        result = -1;
        }
      }
    }
  if ((result == 0) && (cur))
    {
    if (cur->is_persistent != is_persistent)
      {
      KERR("ERROR %s %s %d %d",bulk,image,cur->is_persistent,is_persistent);
      result = -1;
      }
    else if (is_persistent)
      {
      KERR("ERROR  %s %s", bulk, image);
      result = -1;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int tar_img_exists(char *bulk, char *image, char *brandtype)
{
  int result = 0;
  char bulktarget[MAX_PATH_LEN];
  t_loop *cur;
  memset(bulktarget, 0, MAX_PATH_LEN);
  snprintf(bulktarget, MAX_PATH_LEN-1, "%s/%s", bulk, image);
  cur = find_bulk_target(bulktarget, brandtype);
  if (cur)
    result = 1;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void tar_img_init(void)
{
  g_head_loop = NULL;
}
/*--------------------------------------------------------------------------*/
