/*****************************************************************************/
/*    Copyright (C) 2006-2024 clownix@clownix.net License AGPL-3             */
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
#include <fcntl.h>
#include <sys/wait.h>


#include "io_clownix.h"
#include "config_json.h"
#include "crun.h"
#include "crun_utils.h"


struct t_elem;
/****************************************************************************/
typedef struct t_loop
{
  char pth[MAX_PATH_LEN];
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
static t_loop *find_loop(char *pth)
{
  t_loop *cur = g_head_loop;
  while(cur)
    {
    if (!strcmp(cur->pth, pth))
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
static t_loop *alloc_loop(char *pth, int is_persistent)
{
  t_loop *cur = (t_loop *) malloc(sizeof(t_loop));
  memset(cur, 0, sizeof(t_loop));
  strncpy(cur->pth, pth, MAX_PATH_LEN-1);
  cur->is_persistent = is_persistent;
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
static int zipmount_create(char *bulk, char *image, int is_persistent)
{
  int child_pid, wstatus, result = -1;
  char cmd[2*MAX_PATH_LEN];
  char resp[MAX_PATH_LEN];
  FILE *fp;

  memset(resp, 0, MAX_PATH_LEN);
  if (is_persistent)
    {
    memset(cmd, 0, 2*MAX_PATH_LEN);
    snprintf(cmd, 2*MAX_PATH_LEN-1, "%s %s/%s  %s/%s",
    FUSEZIP_BIN, bulk, image, get_mnt_loop_dir(), image);
    if (execute_cmd(cmd, 1))
      KERR("ERROR %s", cmd);
    else
      result = 0;
    }
  else
    {
    memset(cmd, 0, 2*MAX_PATH_LEN);
    snprintf(cmd, 2*MAX_PATH_LEN-1, "%s -r %s/%s  %s/%s",
    FUSEZIP_BIN, bulk, image, get_mnt_loop_dir(), image);
    if (execute_cmd(cmd, 1))
      KERR("ERROR %s", cmd);
    else
      result = 0;
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
  "%s | %s %s | %s '{print $3}'", MOUNT_BIN, GREP_BIN, fuse, AWK_BIN);
  fp = cmd_lio_popen(cmd, &child_pid);
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
static void timeout_del_path(void *data)
{
  char *pmnt = (char *)data;
  rmdir(pmnt);
  free(data);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void mount_del(char *bulk, char *image,
                      char *cnt_dir, int is_persistent, char *loop)
{
  char cmd[2*MAX_PATH_LEN];
  char resp[MAX_PATH_LEN];
  FILE *fp;
  char *pmnt;
  int child_pid, wstatus;
  memset(cmd, 0, 2*MAX_PATH_LEN);
  snprintf(cmd, 2*MAX_PATH_LEN-1,
           "%s | %s %s | %s '{print $3}'",
           MOUNT_BIN, GREP_BIN, cnt_dir, AWK_BIN);
  fp = cmd_lio_popen(cmd, &child_pid);
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
      snprintf(cmd, 2*MAX_PATH_LEN-1, "%s %s", UMOUNT_BIN, resp);
      if (execute_cmd(cmd, 1))
        KERR("ERROR %s", cmd);
      }
    pclose(fp);
    if (force_waitpid(child_pid, &wstatus))
      KERR("ERROR");
    }
  memset(cmd, 0, 2*MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1,
           "%s | %s fuse.cloonix-fuse-zip | %s %s/%s | %s '{print $3}'",
           MOUNT_BIN, GREP_BIN, GREP_BIN, get_mnt_loop_dir(), image, AWK_BIN);
  fp = cmd_lio_popen(cmd, &child_pid);
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
      snprintf(cmd, 2*MAX_PATH_LEN-1, "%s %s", UMOUNT_BIN, resp);
      if (execute_cmd(cmd, 1))
        KERR("ERROR %s", cmd);
      pmnt = (char *) malloc(MAX_PATH_LEN);
      memset(pmnt, 0, MAX_PATH_LEN);
      snprintf(pmnt, MAX_PATH_LEN-1, "%s/%s", get_mnt_loop_dir(), image);
      clownix_timeout_add(100, timeout_del_path, (void *)pmnt, NULL, NULL);
      }
    pclose(fp);
    if (force_waitpid(child_pid, &wstatus))
      KERR("ERROR");
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int tar_img_add(char *name, char *bulk, char *image,
                 char *cnt_dir, int is_persistent)
{
  int result = -1;
  char pth[MAX_PATH_LEN];
  t_loop *cur;
  t_elem *elem;
  memset(pth, 0, MAX_PATH_LEN);
  snprintf(pth, MAX_PATH_LEN-1, "%s/%s", bulk, image);
  cur = find_loop(pth); 
  elem = find_elem(name);
  if (elem)
    KERR("ERROR %s %s %s", name, bulk, image);
  else if (cur)
    {
    if (cur->is_persistent != is_persistent)
      KERR("ERROR %s %s %s %d %d", name, bulk, image,
                                   cur->is_persistent, is_persistent);
    else if (is_persistent)
      KERR("ERROR %s %s %s", name, bulk, image);
    else if (!zipmount_exists(bulk, image, 0, 1))
      KERR("ERROR %s %s %s", name, bulk, image);
    else
      {
      alloc_loop_elem(cur, name);
      result = 0;
      }
    }
  else
    {
    if (zipmount_create(bulk, image, is_persistent))
      KERR("ERROR %s %s %s", name, bulk, image);
    else
      {
      cur = alloc_loop(pth, is_persistent);
      alloc_loop_elem(cur, name);
      result = 0;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int tar_img_del(char *name, char *bulk, char *image,
                 char *cnt_dir, int is_persistent)
{
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
    if (loop->is_persistent != is_persistent)
      KERR("ERROR %s %s %s %d %d", name, bulk, image,
            loop->is_persistent, is_persistent);
    if (loop->head_elem == NULL)
      {
      if (!zipmount_exists(bulk, image, is_persistent, 1))
        KERR("ERROR %s %s %s", name, bulk, image);
      mount_del(bulk, image, cnt_dir, 0, NULL);
      if (zipmount_exists(bulk, image, is_persistent, 0))
        KERR("ERROR %s %s %s", name, bulk, image);

      free_loop(loop);
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *tar_img_get(char *name, char *bulk, char *image)
{
  static char mnt[MAX_PATH_LEN];
  char *result = NULL;
  t_loop *cur;
  t_elem *elem;
  char pth[MAX_PATH_LEN];
  memset(mnt, 0, MAX_PATH_LEN);
  memset(pth, 0, MAX_PATH_LEN);
  snprintf(mnt, MAX_PATH_LEN-1, "%s/%s", get_mnt_loop_dir(), image);
  snprintf(pth, MAX_PATH_LEN-1, "%s/%s", bulk, image);
  cur = find_loop(pth); 
  elem = find_elem(name);
  if (cur == NULL)
    KERR("ERROR %s %s %s", name, bulk, image);
  else if (elem == NULL)
    KERR("ERROR %s %s %s", name, bulk, image);
  else if (!zipmount_exists(bulk, image, cur->is_persistent, 1))
    KERR("ERROR %s %s %s", name, bulk, image);
  else
    result = mnt;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int tar_img_check(char *bulk, char *image, int is_persistent)
{
  int result = 0;
  char pth[MAX_PATH_LEN];
  t_loop *cur;
  memset(pth, 0, MAX_PATH_LEN);
  snprintf(pth, MAX_PATH_LEN-1, "%s/%s", bulk, image);
  cur = find_loop(pth);
  if (cur)
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
int tar_img_exists(char *bulk, char *image)
{
  int result = 0;
  char pth[MAX_PATH_LEN];
  t_loop *cur;
  memset(pth, 0, MAX_PATH_LEN);
  snprintf(pth, MAX_PATH_LEN-1, "%s/%s", bulk, image);
  cur = find_loop(pth);
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
