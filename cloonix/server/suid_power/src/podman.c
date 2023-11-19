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
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/wait.h>
#include <ctype.h>


#include "io_clownix.h"
#include "rpc_clownix.h"
#include "podman.h"
#include "podmanimages.h"
#include "crun_utils.h"

enum
{
  end_launch_state_start=5,
  end_launch_state_dropbear,
};

/*--------------------------------------------------------------------------*/
typedef struct t_timeout_end_launch
{
  int end_launch_state;
  int count;
  int llid;
  int tid;
  int vm_id;
  char brandtype[MAX_NAME_LEN];
  char name[MAX_NAME_LEN];
  char image[MAX_NAME_LEN];
  char id[MAX_NAME_LEN];
} t_timeout_end_launch;
/*--------------------------------------------------------------------------*/
typedef struct t_container
{
  char brandtype[MAX_NAME_LEN];
  char name[MAX_NAME_LEN];
  char image[MAX_NAME_LEN];
  char id[MAX_NAME_LEN];
  int vm_id;
  int pid;
  int pid_count;
  int nb_eth;
  t_eth_mac eth_mac[MAX_ETH_VM];
  struct t_container *prev;
  struct t_container *next;
} t_container;
/*--------------------------------------------------------------------------*/
static t_container *g_head_container;
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void free_doc(t_container *cur)
{
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur->next)
    cur->next->prev = cur->prev;
  if (cur == g_head_container)
    g_head_container = cur->next;
  free(cur);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void delete_podman(t_container *cur)
{
  FILE *fp;
  char cmd[2*MAX_PATH_LEN];
  char line[MAX_PATH_LEN];
  char *ptr;
  char *name = cur->name;
  int pid, wstatus, child_pid;
  memset(cmd, 0, 2*MAX_PATH_LEN);
  snprintf(cmd, 2*MAX_PATH_LEN-1, "%s rm -f %s", PODMAN_BIN, name);
 if ((pid = fork()) < 0)
    KOUT(" ");
  if (pid == 0)
    {
    fp = cmd_lio_popen(cmd, &child_pid);
    if (fp == NULL)
      KERR("ERROR %s", cmd);
    else
      {
      if (fgets( line, sizeof line, fp))
        {
        ptr = strchr(line, '\r');
        if (ptr)
          *ptr = 0;
        ptr = strchr(line, '\n');
        if (ptr)
          *ptr = 0;
        if ((strcmp(line, name)) && 
            (strncmp(line, cur->id, 12)))
          KERR("ERROR %s %s", line, cmd);
        }
      else
        KERR("ERROR %s", cmd);
      pclose(fp);
      if (force_waitpid(child_pid, &wstatus))
        KERR("ERROR %s", cmd);
      }
    exit(0);
    }
  if (force_waitpid(pid, &wstatus))
    KERR("ERROR");
  free_doc(cur);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_container *find_doc(char *brandtype, char *name)
{
  t_container *cur = g_head_container;
  while(cur)
    {
    if ((!strcmp(cur->brandtype, brandtype)) &&
        (!strcmp(cur->name, name)))
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void alloc_doc(char *brandtype, char *name, char *image, char *id,
                      int vm_id, int pid)
{
  t_container *cur = find_doc(brandtype, name);
  if (cur)
    KOUT("ERROR: %s EXISTS", name);
  else
    {
    cur = (t_container *) malloc(sizeof(t_container)); 
    memset(cur, 0, sizeof(t_container));
    strncpy(cur->brandtype, brandtype, MAX_NAME_LEN-1);
    strncpy(cur->name, name, MAX_NAME_LEN-1);
    strncpy(cur->image, image, MAX_NAME_LEN-1);
    strncpy(cur->id, id, MAX_NAME_LEN-1);
    cur->vm_id = vm_id;
    cur->pid = pid;
    if (g_head_container)
      g_head_container->prev = cur;
    cur->next = g_head_container;
    g_head_container = cur;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timeout_end_launch(void *data)
{
  t_timeout_end_launch *tel = (t_timeout_end_launch *) data;
  char resp[MAX_PATH_LEN];
  int pid, wstatus;
  char cmd[2*MAX_PATH_LEN];

  if (tel->end_launch_state == end_launch_state_start)
    {
    podmanimages_rebuild();
    pid = podmanimages_get_pid(tel->brandtype, tel->image, tel->id);
    if (pid == 0)
      {
      KERR("ERROR %s %s %s %d", tel->name, tel->image, tel->id, tel->count);
      tel->count += 1;
      if (tel->count == 5)
        {
        KERR("ERROR %s %s %s", tel->name, tel->image, tel->id);
        snprintf(resp, MAX_PATH_LEN-1,
        "cloonsuid_podman_resp_launch_ko brandtype=%s name=%s vm_id=%d image=%s",
        tel->brandtype, tel->name, tel->vm_id, tel->image);
        if (msg_exist_channel(tel->llid))
          rpct_send_sigdiag_msg(tel->llid, tel->tid, resp);
        free(tel);
        }
      else
        clownix_timeout_add(40, timeout_end_launch, (void *)tel, NULL, NULL);
      }
    else if (find_doc(tel->brandtype, tel->name))
      {
      KERR("ERROR %s %s %s", tel->name, tel->image, tel->id);
      snprintf(resp, MAX_PATH_LEN-1,
      "cloonsuid_podman_resp_launch_ko brandtype=%s name=%s vm_id=%d image=%s",
      tel->brandtype, tel->name, tel->vm_id, tel->image);
      if (msg_exist_channel(tel->llid))
        rpct_send_sigdiag_msg(tel->llid, tel->tid, resp);
      free(tel);
      }
    else
      {
      alloc_doc(tel->brandtype,tel->name,tel->image,tel->id,tel->vm_id,pid);
      snprintf(resp, MAX_PATH_LEN-1,
      "cloonsuid_podman_resp_launch_ok brandtype=%s name=%s vm_id=%d image=%s pid=%d",
      tel->brandtype, tel->name, tel->vm_id, tel->image, pid);
      if (msg_exist_channel(tel->llid))
        rpct_send_sigdiag_msg(tel->llid, tel->tid, resp);
      tel->end_launch_state = end_launch_state_dropbear;
      clownix_timeout_add(40, timeout_end_launch, (void *)tel, NULL, NULL);
      }
    }
  else if (tel->end_launch_state == end_launch_state_dropbear)
    {
    memset(cmd, 0, 2*MAX_PATH_LEN);
    if ((pid = fork()) < 0)
      KOUT(" ");
    if (pid == 0)
      {
      snprintf(cmd, 2*MAX_PATH_LEN-1,
      "%s exec %s /mnt/cloonix_config_fs/podman_init_starter.sh",
      PODMAN_BIN, tel->id);
      if (execute_cmd(cmd))
        KERR("ERROR %s", cmd);
      exit(0);
      }
    else
      {
      if (force_waitpid(pid, &wstatus))
        KERR("ERROR");
      free(tel);
      }
    }
  else
    {
    KERR("ERROR %s %s %s", tel->name, tel->image, tel->id);
    free(tel);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void suppress_previous_name(char *name)
{
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1,"%s rm %s", PODMAN_BIN, name);
  if (execute_cmd(cmd))
    KERR("ERROR %s", cmd);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void create_env_cmd(char *startup_env, char *env)
{
  char *ptr_start, *ptr;
  int i, nb = 0;
  char tmp[2*MAX_NAME_LEN];
  memset(env, 0, MAX_PATH_LEN);
  ptr = startup_env;
  if (strlen(ptr))
    {
    nb = 1;
    while(ptr)
      {
      ptr = strchr(ptr, ' ');
      if (ptr)
        {
        nb += 1;
        ptr += 1;
        }
      }
    ptr_start = startup_env;
    for (i=0; i<nb; i++)
      {
      ptr = strchr(ptr_start, ' ');
      if (ptr)
        {
        *ptr = 0;
        ptr += 1;
        }
      memset(tmp, 0, 2*MAX_NAME_LEN);
      snprintf(tmp, 2*MAX_NAME_LEN-1, "--env=%s ", ptr_start);
      strcat(env, tmp); 
      ptr_start = ptr; 
      }
    }
  else
    strcat(env, " "); 
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int no_entrypoint_found(char *id_image)
{
  int child_pid, wstatus, result = 1;
  FILE *fp;
  char line[MAX_PATH_LEN];
  char cmd[MAX_PATH_LEN];
  char *end;
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf (cmd, MAX_PATH_LEN-1, 
            "%s inspect -f '{{.Config.Cmd}}' %s", PODMAN_BIN, id_image);
  fp = cmd_lio_popen(cmd, &child_pid);
  if (fp == NULL)
    KERR("ERROR %s", cmd);
  else
    {
    if (fgets( line, sizeof line, fp))
      {
      end = strchr(line, '\r');
      if (end)
        *end = 0;
      end = strchr(line, '\n');
      if (end)
        *end = 0;
      if (strlen(line) > 4)
        result = 0; 
      }
    else
      KERR("ERROR %s", cmd);
    pclose(fp);
    if (force_waitpid(child_pid, &wstatus))
      KERR("ERROR");
    }
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf (cmd, MAX_PATH_LEN-1,
            "%s inspect -f '{{.Config.Entrypoint}}' %s", PODMAN_BIN, id_image);
  fp = cmd_lio_popen(cmd, &child_pid);
  if (fp == NULL)
    KERR("ERROR %s", cmd);
  else
    {
    if (fgets( line, sizeof line, fp))
      {
      end = strchr(line, '\r');
      if (end)
        *end = 0;
      end = strchr(line, '\n');
      if (end)
        *end = 0;
      if (strlen(line) > 3)
        result = 0;
      }
    else
      KERR("ERROR %s", cmd);
    pclose(fp);
    if (force_waitpid(child_pid, &wstatus))
      KERR("ERROR");
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int launch_podman(char *cnt_dir, char *brandtype, char *name,
                         char *image, char *agent_dir, char *startup_env,
                         char *id, char *id_image)
{
  FILE *fp;
  char line[MAX_PATH_LEN];
  char cmd[3*MAX_PATH_LEN];
  char str1[MAX_PATH_LEN];
  char env[MAX_PATH_LEN];
  char entrypoint[MAX_NAME_LEN];
  char *mountbear = str1;
  int child_pid, wstatus, result = -1;

  if (dirs_agent_create_mnt_tmp(cnt_dir, name))
    KERR("ERROR %s %s %s", cnt_dir, name, agent_dir);
  else
    {
    memset(entrypoint, 0, MAX_NAME_LEN);
    strncpy(entrypoint, " ", MAX_NAME_LEN-1);
    create_env_cmd(startup_env, env);
    suppress_previous_name(name);
    memset(str1, 0, MAX_PATH_LEN);
    snprintf(str1, MAX_PATH_LEN-1,"%s/%s/mnt", cnt_dir, name);
    dirs_agent_copy_starter(cnt_dir, name, agent_dir);
    if (no_entrypoint_found(id_image))
      strncpy(entrypoint, "sleep 7777d", MAX_NAME_LEN-1);
    memset(cmd, 0, 3*MAX_PATH_LEN);
    snprintf(cmd, 3*MAX_PATH_LEN-1,
    "%s run --name=%s %s --net=none --detach --privileged "
    "--mount type=bind,source=/lib/modules,target=/lib/modules "
    "--mount type=bind,source=%s,target=/mnt %s %s",
    PODMAN_BIN, name, env, mountbear, image, entrypoint);
    fp = cmd_lio_popen(cmd, &child_pid);
    if (fp == NULL)
      KERR("ERROR %s", cmd);
    else
      {
      if (fgets( line, sizeof line, fp))
        {
        if (strlen(line) > 12)
          {
          line[12] = 0;
          strncpy(id, line, MAX_NAME_LEN-1);
          result = 0;
          }
        else
          KERR("ERROR %s %s", line, cmd);
        }
      else
        KERR("ERROR %s", cmd);
      pclose(fp);
      if (force_waitpid(child_pid, &wstatus))
        KERR("ERROR");
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void podman_recv_poldiag_msg(int llid, int tid, char *line)
{
  char *rx="cloonsuid_podman_req_imgs";
  if (!strncmp(line, rx, strlen(rx)))
    podmanimages_update(llid);
  else
    KERR("%s", line);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void podman_recv_sigdiag_msg(int llid, int tid, char *line)
{
  char id[MAX_NAME_LEN];
  char type[MAX_NAME_LEN];
  char name[MAX_NAME_LEN];
  char image[MAX_NAME_LEN];
  char id_image[MAX_NAME_LEN];
  char agent_dir[MAX_PATH_LEN];
  char cnt_dir[MAX_PATH_LEN];
  char startup_env[MAX_PATH_LEN];
  int i, vm_id, num, len; 
  char mac[6];
  char resp[MAX_PATH_LEN];
  char *ptr1, *ptr2;
  t_container *cur;
  t_timeout_end_launch *tel;

  memset(resp, 0, MAX_PATH_LEN);
  
  if (sscanf(line,
  "cloonsuid_podman_req_launch brandtype=%s name=%s vm_id=%d "
  "image=%s cnt_dir=%s agent=%s",
    type, name, &vm_id, image, cnt_dir, agent_dir) == 6)
    {
    if (strcmp(type, "podman"))
      {
      KERR("ERROR %s", line);
      snprintf(resp, MAX_PATH_LEN-1,
      "cloonsuid_podman_resp_launch_ko brandtype=%s name=%s vm_id=%d image=%s",
      type, name, vm_id, image);
      }
    else if (!podmanimages_exits(type, image, id_image))
      {
      KERR("ERROR %s", line);
      snprintf(resp, MAX_PATH_LEN-1,
      "cloonsuid_podman_resp_launch_ko brandtype=%s name=%s vm_id=%d image=%s",
      type, name, vm_id, image);
      }
    else if (podmanimages_container_name_already_used(type, name))
      {
      KERR("ERROR podmanimages_container_name_already_used %s", line);
      snprintf(resp, MAX_PATH_LEN-1,
      "cloonsuid_podman_resp_launch_ko brandtype=%s name=%s vm_id=%d image=%s",
      type, name, vm_id, image);
      }
    else
      {
      memset(startup_env, 0, MAX_PATH_LEN);
      ptr1 = strstr(line, "<startup_env_keyid>");
      if(!ptr1)
        KOUT("%s", line);
      ptr2 = strstr(line, "</startup_env_keyid>");
      if(!ptr2)
        KOUT("%s", line);
      *ptr2 = 0;
      len = strlen("<startup_env_keyid>");
      if (strlen(ptr1) - len >= MAX_PATH_LEN)
        KOUT("%s", line);
      strncpy(startup_env, ptr1+len, MAX_PATH_LEN-1);

      if (!launch_podman(cnt_dir, type, name, image, agent_dir,
                         startup_env, id, id_image))
        {
        tel = (t_timeout_end_launch *) malloc(sizeof(t_timeout_end_launch));
        memset(tel, 0, sizeof(t_timeout_end_launch));
        tel->end_launch_state = end_launch_state_start;
        tel->llid = llid;
        tel->tid = tid;
        tel->vm_id = vm_id;
        strncpy(tel->brandtype, type, MAX_NAME_LEN-1);
        strncpy(tel->name, name, MAX_NAME_LEN-1);
        strncpy(tel->image, image, MAX_NAME_LEN-1);
        strncpy(tel->id, id, MAX_NAME_LEN-1);
        clownix_timeout_add(40, timeout_end_launch, (void *)tel, NULL, NULL);
        }
      else
        {
        KERR("ERROR %s", line);
        snprintf(resp, MAX_PATH_LEN-1,
        "cloonsuid_podman_resp_launch_ko brandtype=%s name=%s vm_id=%d image=%s",
        type, name, vm_id, image);
        }
      }
    }
  else if (sscanf(line,
  "cloonsuid_podman_create_eth brandtype=%s name=%s num=%d "
  "mac=0x%02hhx:0x%02hhx:0x%02hhx:0x%02hhx:0x%02hhx:0x%02hhx",
    type, name, &num,
    &(mac[0]),&(mac[1]),&(mac[2]),&(mac[3]),&(mac[4]),&(mac[5])) == 9)
    {
    if (strcmp(type, "podman"))
      KERR("ERROR %s", line);
    else
      {
      cur = find_doc(type, name);
      if (cur == NULL)
        KERR("ERROR %s", line);
      else if (num != cur->nb_eth)
        KERR("ERROR %s", line);
      else if (num >= MAX_ETH_VM)
        KERR("ERROR %s", line);
      else
        {
        for (i=0;i<6;i++)
          cur->eth_mac[num].mac[i] = mac[i];
        cur->nb_eth += 1;
        }
      }
    }
  else if (sscanf(line,
      "cloonsuid_podman_create_eth_done brandtype=%s name=%s", type, name) == 2)
    {
    if (strcmp(type, "podman"))
      KERR("ERROR %s", line);
    else
      {
      cur = find_doc(type, name);
      if (cur == NULL)
        KERR("ERROR %s", line);
      else
        {
        if (crun_utils_podman_veth(name, cur->pid, cur->vm_id,
                                   cur->nb_eth, cur->eth_mac))
          {
          KERR("ERROR %s", line);
          snprintf(resp, MAX_PATH_LEN-1,
          "cloonsuid_podman_create_eth_done_ko brandtype=%s name=%s", type, name);
          }
        else
          snprintf(resp, MAX_PATH_LEN-1,
          "cloonsuid_podman_create_eth_done_ok brandtype=%s name=%s", type, name);
        }
      }
    }
  else if (sscanf(line,
  "cloonsuid_podman_delete brandtype=%s name=%s", type, name) == 2)
    {
    if (strcmp(type, "podman"))
      KERR("ERROR %s", line);
    else
      {
      cur = find_doc(type, name);
      if (strcmp(type, "podman"))
        {
        KERR("ERROR %s", line);
        snprintf(resp, MAX_PATH_LEN-1,
        "cloonsuid_podman_delete_resp_ko brandtype=%s name=%s", type, name);
        }
      else if (cur == NULL)
        {
        KERR("ERROR %s", line);
        snprintf(resp, MAX_PATH_LEN-1,
        "cloonsuid_podman_delete_resp_ko brandtype=%s name=%s", type, name);
        }
      else
        {
        delete_podman(cur);
        snprintf(resp, MAX_PATH_LEN-1,
        "cloonsuid_podman_delete_resp_ok brandtype=%s name=%s", type, name);
        }
      }
    }
  else if (sscanf(line,
  "cloonsuid_podman_ERROR brandtype=%s name=%s", type, name) == 2)
    {
    if (strcmp(type, "podman"))
      KERR("ERROR %s", line);
    else
      {
      cur = find_doc(type, name);
      if (cur)
        delete_podman(cur);
      else
        KERR("ERROR %s", line);
      }
    }
  else
    KERR("%s", line);
  if (strlen(resp))
    rpct_send_sigdiag_msg(llid, tid, resp);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void podman_beat(int llid)
{
  char resp[MAX_PATH_LEN];
  t_container *next, *cur = g_head_container;
  while (cur)
    {
    next = cur->next;
    if (cur->pid != 0)
      {
      cur->pid_count += 1;
      if (kill(cur->pid, 0))
        {
        if (cur->pid_count > 100)
          {
          cur->pid_count = 0;
          KERR("ERROR %d %s %s", cur->pid, cur->name, cur->id);
          memset(resp, 0, MAX_PATH_LEN);
          snprintf(resp, MAX_PATH_LEN-1,
          "cloonsuid_podman_killed brandtype=%s name=%s id=%s pid=%d",
          cur->brandtype, cur->name, cur->id, cur->pid);
          rpct_send_sigdiag_msg(llid, type_hop_suid_power, resp);
          }
        }
      }
    cur = next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void podman_kill_all(void)
{
  t_container *next, *cur = g_head_container;
  while (cur)
    {
    next = cur->next;
    delete_podman(cur);
    cur = next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void podman_init(void)
{
  podmanimages_init();
  g_head_container = NULL;
}
/*--------------------------------------------------------------------------*/
