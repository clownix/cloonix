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
#include <errno.h>
#include <sys/wait.h>


#include "io_clownix.h"
#include "rpc_clownix.h"
#include "docker.h"
#include "docker_images.h"


/*--------------------------------------------------------------------------*/
typedef struct t_image_info
{
  char brandtype[MAX_NAME_LEN];
  char image_name[MAX_NAME_LEN];
  char image_id[MAX_NAME_LEN];
} t_image_info;
/*--------------------------------------------------------------------------*/
typedef struct t_container_info
{
  char brandtype[MAX_NAME_LEN];
  char image_name[MAX_NAME_LEN];
  char id[MAX_NAME_LEN];
  int pid;
} t_container_info;
/*--------------------------------------------------------------------------*/
static t_image_info g_image[MAX_IMAGE_TAB];
static int g_image_nb;
static t_container_info g_container[MAX_CONTAINER_TAB];
static int g_container_nb;
static int g_podman_ok;
static int g_docker_ok;
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void docker_podman_images(char *cmd, char *brandtype)
{
  int nb, result = 0;
  char resp[MAX_PATH_LEN];
  char it1[MAX_NAME_LEN];
  char it2[MAX_NAME_LEN];
  char it3[MAX_NAME_LEN];
  FILE *fp;
  char *ptr;
  fp = popen(cmd, "r");
  if (fp == NULL)
    KERR("ERROR %s", cmd);
  else
    {
    while (result == 0)
      {
      if (!fgets(resp, MAX_PATH_LEN-1, fp))
        {
        result = -1;
        }
      else if (strlen(resp) == 0)
        {
        result = -1;
        KERR("%s", cmd);
        }
      else
        {
        ptr = strchr(resp, '\r');
        if (ptr)
          *ptr = 0;
        ptr = strchr(resp, '\n');
        if (ptr)
          *ptr = 0;
        if (strncmp(resp, "REPOSITORY", strlen("REPOSITORY")))
          {
          if (sscanf(resp, "%s %s %s", it1, it2, it3) != 3)
            KERR("ERROR %s", resp);
          else
            {
            nb = g_image_nb;
            g_image_nb += 1;
            strncpy(g_image[nb].brandtype, brandtype, MAX_NAME_LEN-1);
            strncpy(g_image[nb].image_name, it1, MAX_NAME_LEN-1);
            strncpy(g_image[nb].image_id, it3, MAX_NAME_LEN-1);
            }
          }
        }
      }
    pclose(fp);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int get_container_pid(char *brandtype, char *id)
{
  int pid = 0;
  char cmd[MAX_PATH_LEN];
  char resp[MAX_PATH_LEN];
  FILE *fp;
  memset(cmd, 0, MAX_PATH_LEN);
  if (!strcmp(brandtype, "docker"))
    {
    if (g_docker_ok)
      snprintf(cmd, MAX_PATH_LEN-1,
             "%s inspect -f \'{{.State.Pid}}\' %s 2>&1", DOCKER_BIN, id);
    }
  else if (!strcmp(brandtype, "podman"))
    {
    if (g_podman_ok)
      snprintf(cmd, MAX_PATH_LEN-1,
             "%s inspect -f \'{{.State.Pid}}\' %s 2>&1", PODMAN_BIN, id);
    }
  else
    KOUT("ERROR: %s", brandtype);
  if (strlen(cmd))
    {
    fp = popen(cmd, "r");
    if (fp == NULL)
      KERR("ERROR %s", cmd);
    else
      {
      if ((!fgets(resp, MAX_PATH_LEN-1, fp)) ||
          (strlen(resp) == 0))
        {
        KERR("%s", cmd);
        }
      else
        {
        if (sscanf(resp, "%d", &pid) != 1)
          KERR("ERROR %s", resp);
        }
      pclose(fp);
      }
    }
  return pid;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void container_ls(char *cmd, char *brandtype)
{
  int nb, result = 0;
  char resp[MAX_PATH_LEN];
  char it1[MAX_NAME_LEN];
  char it2[MAX_NAME_LEN];
  FILE *fp;
  char *ptr;
  fp = popen(cmd, "r");
  if (fp == NULL)
    KERR("ERROR %s", cmd);
  else
    {
    while (result == 0)
      {
      if (!fgets(resp, MAX_PATH_LEN-1, fp))
        {
        result = -1;
        }
      else if (strlen(resp) == 0)
        {
        result = -1;
        KERR("%s", cmd);
        }
      else
        {
        ptr = strchr(resp, '\r');
        if (ptr)
          *ptr = 0;
        ptr = strchr(resp, '\n');
        if (ptr)
          *ptr = 0;
        if (strncmp(resp, "CONTAINER", strlen("CONTAINER")))
          {
          if (sscanf(resp, "%s %s", it1, it2) != 2)
            KERR("ERROR %s", resp);
          else
            {
            ptr = strrchr(it2, ':');
            if (ptr)
              *ptr = 0;
            if (strlen(it2) == 0)
              KERR("ERROR %s", resp);
            else
              {
              nb = g_container_nb;
              g_container_nb += 1;
              strncpy(g_container[nb].brandtype, brandtype, MAX_NAME_LEN-1);
              strncpy(g_container[nb].id, it1, MAX_NAME_LEN-1);
              strncpy(g_container[nb].image_name,it2,MAX_NAME_LEN-1);
              g_container[nb].pid = get_container_pid(brandtype, it1);
              }
            }
          }
        }
      }
    pclose(fp);
    }
}
/*--------------------------------------------------------------------------*/
 
/****************************************************************************/
static void dump_docker_images(void)
{
  char cmd[MAX_PATH_LEN];
  if (g_docker_ok)
    {
    memset(cmd, 0, MAX_PATH_LEN);
    snprintf(cmd, MAX_PATH_LEN-1, "%s images", DOCKER_BIN);
    docker_podman_images(cmd, "docker");
    }
  if (g_podman_ok)
    {
    memset(cmd, 0, MAX_PATH_LEN);
    snprintf(cmd, MAX_PATH_LEN-1, "%s images", PODMAN_BIN);
    docker_podman_images(cmd, "podman");
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void dump_container_ls(void)
{
  char cmd[MAX_PATH_LEN];
  if (g_docker_ok)
    {
    memset(cmd, 0, MAX_PATH_LEN);
    snprintf(cmd, MAX_PATH_LEN-1, "%s ps", DOCKER_BIN);
    container_ls(cmd, "docker");
    }
  if (g_podman_ok)
    {
    memset(cmd, 0, MAX_PATH_LEN);
    snprintf(cmd, MAX_PATH_LEN-1, "%s ps", PODMAN_BIN);
    container_ls(cmd, "podman");
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int docker_images_container_name_already_used(char *brandtype, char *name)
{
  int result = 0;
  char cmd[MAX_PATH_LEN];
  char line[MAX_PATH_LEN];
  char *ptr;
  FILE *fp;
  memset(cmd, 0, MAX_PATH_LEN);
  if (!strcmp(brandtype, "docker"))
    {
    if (g_docker_ok)
      snprintf(cmd, MAX_PATH_LEN-1, "%s ps | awk '{print $NF}' 2>&1", DOCKER_BIN);
    }
  else if (!strcmp(brandtype, "podman"))
    {
    if (g_podman_ok)
      snprintf(cmd, MAX_PATH_LEN-1, "%s ps | awk '{print $NF}' 2>&1", PODMAN_BIN);
    }
  else
    KOUT("ERROR: %s", brandtype);
  if (strlen(cmd))
    {
    fp = popen(cmd, "r");
    if (fp == NULL)
      KERR("ERROR %s", cmd);
    else
      {
      while(fgets(line, MAX_PATH_LEN-1, fp))
        {
        ptr = strchr(line, '\r');
        if (ptr)
          *ptr = 0;
        ptr = strchr(line, '\n');
        if (ptr)
          *ptr = 0;
        if (!strcmp(name, line))
          {
          KERR("ERROR NAME USED %s", name);
          result = 1;
          }
        }
      pclose(fp);
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void docker_images_rebuild(void)
{
  g_image_nb = 0;
  g_container_nb = 0;
  memset(g_image, 0, MAX_IMAGE_TAB * sizeof(t_image_info));
  memset(g_container, 0, MAX_CONTAINER_TAB * sizeof(t_container_info));
  dump_docker_images();
  dump_container_ls();
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void docker_images_update(int llid)
{
  int i;
  char resp[MAX_PATH_LEN];
  docker_images_rebuild();
  snprintf(resp, MAX_PATH_LEN-1, "cloonsuid_docker_resp_image_begin");
  rpct_send_poldiag_msg(llid, type_hop_suid_power, resp);
  for (i=0; i<g_image_nb; i++)
    {
    memset(resp, 0, MAX_PATH_LEN);
    snprintf(resp, MAX_PATH_LEN-1,
             "cloonsuid_docker_resp_image_name: brandtype=%s name=%s",
             g_image[i].brandtype, g_image[i].image_name);
    rpct_send_poldiag_msg(llid, type_hop_suid_power, resp);
    }
  snprintf(resp, MAX_PATH_LEN-1, "cloonsuid_docker_resp_image_end");
  rpct_send_poldiag_msg(llid, type_hop_suid_power, resp);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int docker_images_get_pid(char *brandtype, char *image_name, char *id)
{
  int i, pid = 0;
  if ((strcmp(brandtype, "docker")) &&  
      (strcmp(brandtype, "podman")))
    KOUT("ERROR: %s", brandtype);
  for (i=0; i<g_container_nb; i++)
    {
    if ((!strcmp(g_container[i].brandtype, brandtype)) &&
        (!strcmp(g_container[i].image_name, image_name)) &&
        (!strcmp(g_container[i].id, id)))
      pid = g_container[i].pid;
    }
  return pid;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int docker_images_exits(char *brandtype, char *name)
{
  int i, result = 0;
  for (i=0; i<g_image_nb; i++)
    {
    if ((!strcmp(g_image[i].brandtype, brandtype)) &&
        ((!strcmp(g_image[i].image_name, name)) ||
         (!strcmp(g_image[i].image_id, name))))
      result = 1;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void docker_images_init(void)
{
  g_docker_ok = 0;
  g_podman_ok = 0;
  g_image_nb = 0;
  g_container_nb = 0;
  memset(g_image, 0, MAX_IMAGE_TAB * sizeof(t_image_info));
  memset(g_container, 0, MAX_CONTAINER_TAB * sizeof(t_container_info));
  if (!access(DOCKER_BIN, F_OK))
    g_docker_ok = 1;
  else
    KERR("WARNING binary %s not found", DOCKER_BIN);
  if (!access(PODMAN_BIN, F_OK))
    g_podman_ok = 1;
  else
    KERR("WARNING binary %s not found", PODMAN_BIN);
}
/*--------------------------------------------------------------------------*/
