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

#include "io_clownix.h"
#include "ovs_get.h"

typedef struct t_list_names
{
  char name[MAX_NAME_LEN];
  struct t_list_names *prev;
  struct t_list_names *next;
} t_list_names;

static int g_nb_brgs;
static t_topo_bridges g_brgs[MAX_OVS_BRIDGES];
static t_list_names *g_head_names;

/****************************************************************************/
static int name_exists_in_list(char *line)
{
  int result = 0;
  char cpy_line[MAX_PATH_LEN];
  char *ptr_start, *ptr_end;
  t_list_names *cur = g_head_names;

  strncpy(cpy_line, line, MAX_PATH_LEN-1);
  cpy_line[MAX_PATH_LEN-1] = 0;
  ptr_start = cpy_line;
  ptr_end = ptr_start + strcspn(ptr_start, " \r\n\t");
  *ptr_end = 0;
  while (cur)
    {
    if ((strlen(cur->name) > 1) &&
        (!strncmp(line, cur->name, strlen(cur->name))))
      result = 1;
    cur = cur->next;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void update_bridges(char *bin, char *sock)
{
  FILE *fh;
  char cmd[2*MAX_PATH_LEN];
  char line[MAX_PATH_LEN];
  char *ptr_start, *ptr_end;
  memset(cmd, 0, 2*MAX_PATH_LEN);
  snprintf(cmd, 2*MAX_PATH_LEN-1, "%s --db=unix:%s list-br", bin, sock); 
  fh = popen(cmd, "r");
  if (fh)
    {
    while (fgets(line, MAX_PATH_LEN-1, fh) != NULL)
      {
      if ((!strncmp(line, "_b_", strlen("_b_"))) ||
          (name_exists_in_list(line)))
        {
        ptr_start = g_brgs[g_nb_brgs].br;
        strncpy(g_brgs[g_nb_brgs].br, line, MAX_NAME_LEN-1);
        ptr_end = ptr_start + strcspn(ptr_start, " \r\n\t");
        *ptr_end = 0;
        if (strlen(ptr_start))
          {
          g_nb_brgs += 1;
          if (g_nb_brgs == MAX_OVS_BRIDGES)
            KOUT("%d %s", g_nb_brgs, line);
          }
        }
      }
    fclose(fh);
    }
  else
    KERR("%s", cmd);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void update_ports_bridge(char *bin, char *sock, t_topo_bridges *tbr)
{
  FILE *fh;
  char cmd[2*MAX_PATH_LEN];
  char line[MAX_PATH_LEN];
  char *ptr_start, *ptr_end;
  memset(cmd, 0, 2*MAX_PATH_LEN);
  snprintf(cmd, 2*MAX_PATH_LEN-1, "%s --db=unix:%s list-ports %s",
           bin, sock, tbr->br);
  fh = popen(cmd, "r");
  if (fh)
    {
    while (fgets(line, MAX_PATH_LEN-1, fh) != NULL)
      {
      if ((!strncmp(line, "_p_", strlen("_p_"))) ||
          (name_exists_in_list(line)))
        {
        ptr_start = tbr->ports[tbr->nb_ports];
        strncpy(ptr_start, line, MAX_NAME_LEN-1);
        ptr_end = ptr_start + strcspn(ptr_start, " \r\n\t");
        *ptr_end = 0;
        if (strlen(ptr_start))
          {
          tbr->nb_ports += 1;
          if (tbr->nb_ports == MAX_OVS_PORTS)
            KOUT("%d %s", tbr->nb_ports, line);
          }
        }
      }
    fclose(fh);
    }
  else
    KERR("%s", cmd);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int estimate_max_string_size(void)
{
  int i, j, result = (10 * MAX_NAME_LEN);
  for (i=0; i<g_nb_brgs; i++)
    {
    result += 2*MAX_NAME_LEN;
    for (j=0; j<g_brgs[i].nb_ports; j++)
      result += 2*MAX_NAME_LEN;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int format_bridges(char *msg, int idx)
{
  int i, len = 0;
  char *name = g_brgs[idx].br;
  int nb_ports = g_brgs[idx].nb_ports;
  len += sprintf(msg+len, "<br> %s %d </br>", name, nb_ports);
  for (i=0; i<g_brgs[idx].nb_ports; i++)
    len += sprintf(msg+len, "<port> %s </port>", g_brgs[idx].ports[i]);
  return len;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void format_msg(char *msg)
{
  int i, len = 0;
  len += sprintf(msg+len, "<ovs_topo_config>");
  len += sprintf(msg+len, "<ovs_bridges> %d ", g_nb_brgs);
  for (i=0; i<g_nb_brgs; i++)
    len += format_bridges(msg+len, i); 
  len += sprintf(msg+len, "</ovs_bridges>");
  len += sprintf(msg+len, "</ovs_topo_config>");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_get_rec_name(char *name, int on)
{
  t_list_names *cur = g_head_names;
  while (cur)
    {
    if (!strcmp(cur->name, name))
      break;
    cur = cur->next;
    }
  if (on)
    {
    if (!cur)
      {
      cur = (t_list_names *) malloc(sizeof(t_list_names));
      memset(cur, 0, sizeof(t_list_names));
      strncpy(cur->name, name, MAX_NAME_LEN-1);
      if (g_head_names)
        g_head_names->prev = cur;
      cur->next = g_head_names;
      g_head_names = cur;
      }
    }
  else
    {
    if (!cur)
      KERR("%s", name);
    else
      {
      if (cur->prev)
        cur->prev->next = cur->next;
      if (cur->next)
        cur->next->prev = cur->prev;
      if (cur == g_head_names)
        g_head_names = cur->next;
      free(cur);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *ovs_get_topo(char *bin_dir, char *work_dir)
{
  int i, len;
  char bin_vsctl[MAX_PATH_LEN];
  char ovsdb_sock[MAX_PATH_LEN];
  char *msg;
  memset(bin_vsctl, 0, MAX_PATH_LEN);
  memset(ovsdb_sock, 0, MAX_PATH_LEN);
  snprintf(bin_vsctl,MAX_PATH_LEN-1,"%s/server/dpdk/bin/ovs-vsctl",bin_dir);
  snprintf(ovsdb_sock,MAX_PATH_LEN-1,"%s/dpdk/ovsdb_server.sock",work_dir);
  g_nb_brgs = 0;
  memset(g_brgs, 0, MAX_OVS_BRIDGES * sizeof(t_topo_bridges));
  if (!access(ovsdb_sock, F_OK))
    {
    update_bridges(bin_vsctl, ovsdb_sock);
    for (i=0; i<g_nb_brgs; i++)
      update_ports_bridge(bin_vsctl, ovsdb_sock, &(g_brgs[i]));
    }
  len = estimate_max_string_size();
  msg = (char *) malloc(len);
  memset(msg, 0, len);
  format_msg(msg);
  return msg;  
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_get_init(void)
{
  g_nb_brgs = 0;
  memset(g_brgs, 0, MAX_OVS_BRIDGES * sizeof(t_topo_bridges));
  g_head_names = NULL;
}
/*--------------------------------------------------------------------------*/
