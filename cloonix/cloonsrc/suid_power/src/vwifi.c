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
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <dirent.h>
#include <ctype.h>
#include <errno.h>
#include <dirent.h>


#include "io_clownix.h"
#include "config_json.h"
#include "crun.h"
#include "tar_img.h"
#include "net_phy.h"
#include "crun_utils.h"
#include "utils.h"
#include "vwifi.h"
#include "nl_hwsim.h"




static int g_nb_vwifi;
static t_elem_vwifi *g_head_past_elem_vwifi;
static t_elem_vwifi *g_head_elem_vwifi;
static int g_module_mac80211_hwsim_inserted;

/****************************************************************************/
static void fill_item(char *syshwsim, char *hwsim, char *itemdir, char *itemval)
{
  DIR *dirptr;
  struct dirent *ent;
  char tmppath[MAX_PATH_LEN];
  memset(tmppath, 0, MAX_PATH_LEN);
  snprintf(tmppath, MAX_PATH_LEN-1, "%s/%s/%s", syshwsim, hwsim, itemdir);
  if (!access(tmppath, R_OK))
    {
    dirptr = opendir(tmppath);
    if (!dirptr)
      KERR("ERROR %s", tmppath);
    else
      {
      while ((ent = readdir(dirptr)) != NULL)
        {
        if (!strcmp(ent->d_name, "."))
          continue;
        if (!strcmp(ent->d_name, ".."))
          continue;
        if (ent->d_type == DT_DIR)
          strncpy(itemval, ent->d_name, MAX_NAME_LEN-1);
        }
      if (closedir(dirptr))
        KERR("ERROR  %d", errno);
      }
    }
} 
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void free_list_elem_vwifi(void)
{       
  t_elem_vwifi *next, *cur = g_head_past_elem_vwifi;
  while (cur)
    {
    next = cur->next;
    free(cur);
    cur = next;
    }
  g_head_past_elem_vwifi = g_head_elem_vwifi;
  g_head_elem_vwifi = NULL;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void net_phy_get_hwsim(void)
{
  DIR *dirptr;
  struct dirent *ent;
  char *syshwsim = "/sys/devices/virtual/mac80211_hwsim";
  t_elem_vwifi *cur;

  free_list_elem_vwifi();
  if (access(syshwsim, R_OK))
    KERR("ERROR %s", syshwsim);
  else
    {
    dirptr = opendir(syshwsim);
    if (!dirptr)
      KERR("ERROR %s", syshwsim);
    else
      {
      while ((ent = readdir(dirptr)) != NULL)
        {
        if ((ent->d_type == DT_DIR) &&
            (!strncmp(ent->d_name, "hwsim", strlen("hwsim"))))
          {
          cur = (t_elem_vwifi *) malloc(sizeof(t_elem_vwifi));
          memset(cur, 0, sizeof(t_elem_vwifi));
          strncpy(cur->hwsim, ent->d_name, MAX_NAME_LEN-1);
          if (g_head_elem_vwifi)
            g_head_elem_vwifi->prev = cur;
          cur->next = g_head_elem_vwifi;
          g_head_elem_vwifi = cur;
          fill_item(syshwsim, ent->d_name, "net", cur->net_main);
          fill_item(syshwsim, ent->d_name, "ieee80211", cur->phy_main);
          }
        }
      if (closedir(dirptr))
        KERR("ERROR  %d", errno);
      }
   }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
t_elem_vwifi *get_diff_past_hwsim(int *nb)
{
  t_elem_vwifi *cur, *cur1, *cur0 = g_head_elem_vwifi;
  t_elem_vwifi *head_diff = NULL;
  int found;
  *nb = 0;
  while (cur0)
    {
    found = 0;
    cur1 = g_head_past_elem_vwifi;
    while (cur1)
      {
      if (!strcmp(cur0->hwsim, cur1->hwsim))
        found = 1;
      cur1 = cur1->next;
      }
    if (found == 0)
      {
      cur = (t_elem_vwifi *) malloc(sizeof(t_elem_vwifi));
      memset(cur, 0, sizeof(t_elem_vwifi));
      strncpy(cur->hwsim,    cur0->hwsim,    MAX_NAME_LEN-1);
      strncpy(cur->net_main, cur0->net_main, MAX_NAME_LEN-1);
      strncpy(cur->phy_main, cur0->phy_main, MAX_NAME_LEN-1);
      if (head_diff)
        head_diff->prev = cur;
      cur->next = head_diff;
      head_diff = cur;
      *nb += 1;
      }
    cur0 = cur0->next;
    }
  return(head_diff);  
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int vwifi_lsmod_mac80211_hwsim(void)
{
  FILE *fp;
  int child_pid, wstatus, pid, result = -1;
  char cmd[MAX_PATH_LEN];
  char line[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd,MAX_PATH_LEN-1,"%s | grep mac80211_hwsim",pthexec_lsmod_bin());
  fp = cmd_lio_popen(cmd, &child_pid, 0);
  if (fp == NULL)
    {
    KERR("ERROR %s", cmd);
    log_write_req("PREVIOUS CMD KO");
    }
  else
    {
    if(fgets(line, MAX_PATH_LEN-1, fp))
      {
      result = 0;
      }
    else
      {
      result = 1;
      }
    pclose(fp);
    if (force_waitpid(child_pid, &wstatus))
      {
      KERR("ERROR %s", cmd);
      result = -1;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int vwifi_modprobe_mac80211_hwsim(void)
{
  FILE *fp;
  int child_pid, wstatus, pid, result = -1;
  char cmd[MAX_PATH_LEN];
  char line[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1, "%s mac80211_hwsim radios=0",
           pthexec_modprobe_bin());
  fp = cmd_lio_popen(cmd, &child_pid, 0);
  if (fp == NULL)
    {
    KERR("ERROR %s", cmd);
    log_write_req("PREVIOUS CMD KO");
    }
  else
    {
    if(fgets(line, MAX_PATH_LEN-1, fp))
      {
      KERR("ERROR %s", line);
      }
    else
      {
      result = 0;
      }
    pclose(fp);
    if (force_waitpid(child_pid, &wstatus))
      {
      KERR("ERROR %s", cmd);
      result = -1;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void vwifi_change_name(t_elem_vwifi *cur)
{
  FILE *fp;
  int child_pid, wstatus;
  char cmd[MAX_PATH_LEN];
  char line[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1,
           "%s netns exec %s %s link set name %s %s",
           pthexec_ip_bin(), cur->nspace, pthexec_ip_bin(),
           cur->net_ns, cur->net_main);
  fp = cmd_lio_popen(cmd, &child_pid, 0);
  if (fp == NULL)
    {
    KERR("ERROR %s", cmd);
    log_write_req("PREVIOUS CMD KO");
    }
  else
    {
    if(fgets(line, MAX_PATH_LEN-1, fp))
      {
      KERR("ERROR %s %s", cmd, line);
      log_write_req("PREVIOUS CMD KO");
      }
    pclose(fp);
    if (force_waitpid(child_pid, &wstatus))
      {
      KERR("ERROR %s", cmd);
      log_write_req("PREVIOUS CMD KO");
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int vwifi_insert_nspace(t_elem_vwifi *head_vwifi, char *ns)
{
  FILE *fp;
  int child_pid, wstatus, an_error = 0, result = -1;
  char cmd[MAX_PATH_LEN];
  char line[MAX_PATH_LEN];
  t_elem_vwifi *cur = head_vwifi;
  int nb_net_ns = 0;
  while (cur)
    { 
    snprintf(cur->net_ns, MAX_NAME_LEN-1, "wlan%d", nb_net_ns++);
    memset(cmd, 0, MAX_PATH_LEN);
    snprintf(cmd, MAX_PATH_LEN-1, "%s phy %s set netns name %s",
             pthexec_iw_bin(), cur->phy_main, ns);
    fp = cmd_lio_popen(cmd, &child_pid, 0);
    if (fp == NULL)
      {
      an_error = 1;
      KERR("ERROR %s", cmd);
      log_write_req("PREVIOUS CMD KO");
      }
    else
      {
      if(fgets(line, MAX_PATH_LEN-1, fp))
        {
        an_error = 1;
        KERR("ERROR %s %s", cmd, line);
        log_write_req("PREVIOUS CMD KO");
        }
      pclose(fp);
      if (force_waitpid(child_pid, &wstatus))
        {
        an_error = 1;
        KERR("ERROR %s", cmd);
        log_write_req("PREVIOUS CMD KO");
        }
      strncpy(cur->nspace, ns, MAX_NAME_LEN-1);
      vwifi_change_name(cur);
      }
    cur = cur->next;
    }
  if (an_error == 0)
    result = 0;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void *vwifi_wlan_create(char *name, char *nspacecrun, int nb_vwif,
                        char mac_vwif[6], t_elem_vwifi **vwifi)
{
  void *result = NULL;
  char cmd[MAX_PATH_LEN];
  int nb, res;
  t_elem_vwifi *cur, *next;
  *vwifi = NULL; 
  if (g_module_mac80211_hwsim_inserted == 0)
    {
    KERR("ERROR ADDING VWIFI INTERFACE, mac80211_hwsim NOT INSERTED");
    }
  else
    {
    net_phy_get_hwsim();
    result = nl_hwsim_add_phy(nb_vwif, (uint8_t *)mac_vwif);
    if (result == NULL)
      KERR("ERROR add vwifi %d", nb_vwif);
    else
      {
      net_phy_get_hwsim();
      *vwifi = get_diff_past_hwsim(&nb);
      if (nb != nb_vwif)
        KERR("ERROR %d %d", nb_vwif, nb);
      else if (vwifi_insert_nspace(*vwifi, nspacecrun))
        KERR("ERROR %d %s", nb_vwif, nspacecrun);
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void vwifi_after_suidroot_init(void)
{
  if (vwifi_lsmod_mac80211_hwsim() == 0)
    {
    g_module_mac80211_hwsim_inserted = 1;
    KERR("MODULE mac80211_hwsim  ALREADY INSERTED");
    }
  else if (vwifi_modprobe_mac80211_hwsim() == 0)
    {
    g_module_mac80211_hwsim_inserted = 1;
    KERR("WARNING SUCCESSFULL INSERTION MODULE mac80211_hwsim");
    }
  else
    {
    g_module_mac80211_hwsim_inserted = 0;
    KERR("WARNING MODULE mac80211_hwsim NOT INSERTED");
    }
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void vwifi_init(void)
{
  g_module_mac80211_hwsim_inserted = 0;
  g_head_past_elem_vwifi = NULL;
  g_head_elem_vwifi = NULL;
}
/*--------------------------------------------------------------------------*/
