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
#include <fcntl.h>



#include "io_clownix.h"
#include "launcher.h"
#include "config_json.h"
#include "cnt.h"
#include "cnt_utils.h"


struct t_elem;
/****************************************************************************/
typedef struct t_loop
{
  char pth[MAX_PATH_LEN];
  char loop[MAX_NAME_LEN];
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
static t_loop *alloc_loop(char *pth, char *loop)
{
  t_loop *cur = (t_loop *) malloc(sizeof(t_loop));
  memset(cur, 0, sizeof(t_loop));
  strncpy(cur->pth, pth, MAX_PATH_LEN-1);
  strncpy(cur->loop, loop, MAX_NAME_LEN-1);
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
static int losetup_get(char *bulk, char *image, char *resp)
{
  int result = -1;
  char cmd[MAX_PATH_LEN];
  char *losetup = get_losetup_bin();
  FILE *fp;
  char *ptr;
  memset(resp, 0, MAX_PATH_LEN);
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1,
           "%s -l | grep %s/%s | awk '{print $1}'", losetup, bulk, image);
  fp = my_popen(cmd, "r");
  if (fp == NULL)
    KERR("ERROR %s %s", bulk, image);
  else
    {
    if (!fgets(resp, MAX_PATH_LEN-1, fp))
      pclose(fp);
    else
      {
      pclose(fp);
      ptr = strchr(resp, '\r');
      if (ptr)
        *ptr = 0;
      ptr = strchr(resp, '\n');
      if (ptr)
        *ptr = 0;
      if (strlen(resp) == 0)
        KERR("%s", cmd);
      else
        result = 0;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int loop_mount_create(char *bulk, char *image, char *loop)
{
  int result = -1;
  char *losetup = get_losetup_bin();
  char *mount = get_mount_bin();
  char cmd[MAX_PATH_LEN];
  char resp[MAX_PATH_LEN];
  FILE *fp;

  memset(loop, 0, MAX_PATH_LEN);
  memset(resp, 0, MAX_PATH_LEN);
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1, "%s -fPL %s/%s 2>&1", losetup, bulk, image);
  fp = my_popen(cmd, "r");
  if (fp == NULL)
    KERR("ERROR %s %s", bulk, image);
  else if (fgets(resp, MAX_PATH_LEN-1, fp))
    {
    pclose(fp);
    KERR("ERROR %s %s %s", bulk, image, resp);
    }
  else
    {
    pclose(fp);
    memset(resp, 0, MAX_PATH_LEN);
    if (losetup_get(bulk, image, resp))
      KERR("ERROR %s %s %s", bulk, image, resp);
    else
      {
      strncpy(loop, resp, MAX_PATH_LEN-1); 
      snprintf(cmd, MAX_PATH_LEN-1, "%s -o loop %s %s/mnt/%s 2>&1",
               mount, loop, bulk, image);
      fp = my_popen(cmd, "r");
      if (fp == NULL)
        KERR("ERROR %s %s %s", bulk, image, resp);
      else
        {
        if (fgets(resp, MAX_PATH_LEN-1, fp))
          KERR("ERROR %s %s %s", bulk, image, resp);
        else
          result = 0;
        pclose(fp);
        }
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int loop_mount_exists(char *bulk, char *image)
{
  int result = 0;
  char resp[MAX_PATH_LEN];
  char mnt[MAX_PATH_LEN];
  char cmd[MAX_PATH_LEN];
  char *mount = get_mount_bin();
  FILE *fp;
  char *ptr;
  char *loop = resp;

  memset(cmd, 0, MAX_PATH_LEN);
  memset(mnt, 0, MAX_PATH_LEN);
  snprintf(mnt, MAX_PATH_LEN-1, "%s/mnt/%s", bulk, image);
  if (!losetup_get(bulk, image, resp))
    { 
    memset(cmd, 0, MAX_PATH_LEN);
    snprintf(cmd, MAX_PATH_LEN-1,
             "%s | grep %s | awk '{print $3}' 2>&1", mount, loop);
    fp = my_popen(cmd, "r");
    if (fp == NULL)
      KERR("ERROR %s %s", bulk, image);
    else if (!fgets(resp, MAX_PATH_LEN-1, fp))
      pclose(fp);
    else
      {
      pclose(fp);
      ptr = strchr(resp, '\r');
      if (ptr)
        *ptr = 0;
      ptr = strchr(resp, '\n');
      if (ptr)
        *ptr = 0;
      if (!strcmp(resp, mnt))
        result = 1;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void mount_del(char *bulk, char *image, char *cnt_dir, char *loop)
{
  char mnt[MAX_PATH_LEN];
  char cmd[MAX_PATH_LEN];
  char resp[MAX_PATH_LEN];
  char *mount = get_mount_bin();
  char *umount = get_umount_bin();
  char *rsp = resp;
  FILE *fp1, *fp2;

  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1, "%s | grep %s | awk '{print $3}'",
           mount, cnt_dir);
  fp1 = my_popen(cmd, "r");
  if (fp1 == NULL)
    KERR("ERROR %s", cmd);
  else if (fgets(resp, MAX_PATH_LEN-1, fp1))
    {
    memset(cmd, 0, MAX_PATH_LEN);
    snprintf(cmd, MAX_PATH_LEN-1, "%s %s", umount, rsp);
    fp2 = my_popen(cmd, "r");
    if (fp2 == NULL)
      KERR("ERROR %s", cmd);
    pclose(fp2);
    }
  pclose(fp1);
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1, "%s | grep %s | awk '{print $3}'",mount,loop);
  fp1 = my_popen(cmd, "r");
  if (fp1 == NULL)
    KERR("ERROR %s", cmd);
  else
    {
    if (fgets(resp, MAX_PATH_LEN-1, fp1))
      {
      memset(cmd, 0, MAX_PATH_LEN);
      snprintf(cmd, MAX_PATH_LEN-1, "%s %s", umount, rsp);
      fp2 = my_popen(cmd, "r");
      if (fp2 == NULL)
        KERR("ERROR %s", cmd);
      else
        {
        if (fgets(resp, MAX_PATH_LEN-1, fp1))
          KERR("ERROR %s %s", cmd, resp);
        pclose(fp2);
        snprintf(mnt, MAX_PATH_LEN-1, "%s/mnt/%s", bulk, image);
        if (rmdir(mnt))
          KERR("ERROR Dir: %s could not be deleted\n", mnt);
        }
      }
    pclose(fp1);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void losetup_del(char *loop)
{
  char cmd[MAX_PATH_LEN];
  char *losetup = get_losetup_bin();
  FILE *fp;
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1, "%s -d %s 2>&1", losetup, loop);
  fp = my_popen(cmd, "r");
  if (fp == NULL)
    KERR("ERROR %s", loop);
  else 
    pclose(fp);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void remove_loop(char *name, char *bulk, char *image, char *cnt_dir)
{
  int count = 0;
  char resp[MAX_PATH_LEN];
  while (!losetup_get(bulk, image, resp))
    {
    mount_del(bulk, image, cnt_dir, resp);
    losetup_del(resp);
    count += 1;
    if (count > 10)
      {
      KERR("ERROR %s %s %s", name, image, resp);
      break;
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int loop_img_add(char *name, char *bulk, char *image, char *cnt_dir)
{
  int result = -1;
  char pth[MAX_PATH_LEN];
  char loop[MAX_PATH_LEN];
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
    if (!loop_mount_exists(bulk, image))
      KERR("ERROR %s %s %s", name, bulk, image);
    else
      {
      alloc_loop_elem(cur, name);
      result = 0;
      }
    }
  else
    {
    remove_loop(name, bulk, image, cnt_dir);
    if (loop_mount_create(bulk, image, loop))
      KERR("ERROR %s %s %s", name, bulk, image);
    else
      {
      cur = alloc_loop(pth, loop);
      alloc_loop_elem(cur, name);
      result = 0;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int loop_img_del(char *name, char *bulk, char *image, char *cnt_dir)
{
  int result = -1;
  t_loop *loop;
  t_elem *elem = find_elem(name);
  if (elem == NULL)
    KERR("ERROR %s", name);
  else
    {
    result = 0;
    loop = free_loop_elem(elem);
    if (loop->head_elem == NULL)
      {
      if (!loop_mount_exists(bulk, image))
        KERR("ERROR %s %s %s", name, bulk, image);
      remove_loop(name, bulk, image, cnt_dir);
      free_loop(loop);
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *loop_img_get(char *name, char *bulk, char *image)
{
  static char mnt[MAX_PATH_LEN];
  char *result = NULL;
  t_loop *cur;
  t_elem *elem;
  char pth[MAX_PATH_LEN];
  memset(mnt, 0, MAX_PATH_LEN);
  memset(pth, 0, MAX_PATH_LEN);
  snprintf(mnt, MAX_PATH_LEN-1, "%s/mnt/%s", bulk, image);
  snprintf(pth, MAX_PATH_LEN-1, "%s/%s", bulk, image);
  cur = find_loop(pth); 
  elem = find_elem(name);
  if (cur == NULL)
    KERR("ERROR %s %s %s", name, bulk, image);
  else if (elem == NULL)
    KERR("ERROR %s %s %s", name, bulk, image);
  else if (!loop_mount_exists(bulk,image))
    KERR("ERROR %s %s %s", name, bulk, image);
  else
    result = mnt;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void loop_img_init(void)
{
  g_head_loop = NULL;
}
/*--------------------------------------------------------------------------*/
