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
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/ip.h>
#include <net/if.h>
#include <linux/ethtool.h>
#include <linux/sockios.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>


#include "io_clownix.h"
#include "net_phy.h"

typedef struct t_elem_phy
{
  char name[MAX_NAME_LEN];
  char phy_mac[MAX_NAME_LEN];
  struct t_elem_phy *prev;
  struct t_elem_phy *next;
} t_elem_phy;


static t_topo_info_phy g_topo_phy[MAX_PHY];
static int g_nb_phy;
static t_elem_phy *g_head_elem_phy;

/*****************************************************************************/
static void free_list_elem_phy(void)
{
  t_elem_phy *next, *cur = g_head_elem_phy;
  while (cur)
    {
    next = cur->next;
    free(cur);
    cur = next;
    }
  g_head_elem_phy = NULL;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void get_list_elem_phy(void)
{
  DIR *dirptr;
  struct dirent *ent;
  char *sysclassnet = "/sys/class/net";
  t_elem_phy *cur;

  free_list_elem_phy();
  if (access(sysclassnet, R_OK))
    KERR("ERROR %s", sysclassnet);
  else
    {
    dirptr = opendir(sysclassnet);
    if (dirptr)
      {
      while ((ent = readdir(dirptr)) != NULL)
        {
        if (!strcmp(ent->d_name, "."))
          continue;
        if (!strcmp(ent->d_name, ".."))
          continue;
        if (!strcmp(ent->d_name, "lo"))
          continue;
        if (ent->d_type == DT_DIR)
          continue;
        if (strlen(ent->d_name) >= MAX_NAME_LEN)
          KERR("ERROR %lu  %s", strlen(ent->d_name), ent->d_name);
        else
          {
          cur = (t_elem_phy *) malloc(sizeof(t_elem_phy));
          memset(cur, 0, sizeof(t_elem_phy)); 
          strncpy(cur->name, ent->d_name, MAX_NAME_LEN-1);
          if (g_head_elem_phy)
            g_head_elem_phy->prev = cur;
          cur->next = g_head_elem_phy;
          g_head_elem_phy = cur;
          }
        }
      if (closedir(dirptr))
        KERR("ERROR  %d", errno);
      }
   }
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static int driver_has_mac_pci(char *name, char *phy_mac)
{
  int result = 0;
  char path[MAX_PATH_LEN];
  FILE *fh;
  memset(phy_mac, 0, MAX_NAME_LEN);
  sprintf(path, "/sys/class/net/%s/device/uevent", name);
  fh = fopen(path, "r");
  if (fh)
    {
    fclose(fh);
    sprintf(path, "/sys/class/net/%s/address", name);
    fh = fopen(path, "r");
    if (fh)
      {
      if (fgets(phy_mac, MAX_NAME_LEN-1, fh) == NULL)
        KERR("ERROR %s", name);
      else
        result = 1;
      fclose(fh);
      }
    else
      KERR("ERROR %s", path);
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void filter_out_elem_phy(void)
{
  t_elem_phy *next, *cur = g_head_elem_phy;
  while (cur)
    {
    next = cur->next;
    if (!driver_has_mac_pci(cur->name, cur->phy_mac))
      {
      if (cur->prev)
        cur->prev->next = cur->next;
      if (cur->next)
        cur->next->prev = cur->prev;
      if (cur == g_head_elem_phy)
        g_head_elem_phy = cur->next;
      free(cur);
      }
    cur = next;
    }
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
t_topo_info_phy *net_phy_get(int *nb)
{
  t_elem_phy *cur;
  g_nb_phy = 0;
  get_list_elem_phy();
  filter_out_elem_phy();
  cur = g_head_elem_phy;
  while (cur)
    {
    memset(&(g_topo_phy[g_nb_phy]), 0, sizeof(t_topo_info_phy));
    strncpy(g_topo_phy[g_nb_phy].name, cur->name, MAX_NAME_LEN-1);
    strncpy(g_topo_phy[g_nb_phy].phy_mac, cur->phy_mac, MAX_NAME_LEN-1);
    g_nb_phy += 1;
    cur = cur->next;
    }
  *nb = g_nb_phy;
  return g_topo_phy;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void net_phy_init(void)
{
  memset(g_topo_phy, 0, MAX_PHY * sizeof(t_topo_info_phy));
  g_nb_phy = 0;
  g_head_elem_phy = NULL;
  get_list_elem_phy();
  filter_out_elem_phy();
}
/*--------------------------------------------------------------------------*/
