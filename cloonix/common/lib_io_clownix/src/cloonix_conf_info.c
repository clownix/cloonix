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
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>



#include "io_clownix.h"
#include "cloonix_conf_info.h"

/*--------------------------------------------------------------------------*/
static t_cloonix_conf_info g_name2info[MAX_CLOONIX_SERVERS];
static char g_all_names[MAX_CLOONIX_SERVERS * MAX_NAME_LEN];
static int g_cloonix_nb;
static char g_cloonix_version[MAX_NAME_LEN];


/*****************************************************************************/
static void alloc_record(char *name, char *ip, uint32_t ipnum, int port, 
                         int novnc_port, char *passwd, uint32_t udp_ip,
                         int c2c_udp_port_low)
{
  if (g_cloonix_nb == MAX_CLOONIX_SERVERS - 1)
    KOUT("%d", g_cloonix_nb);
  strncpy(g_name2info[g_cloonix_nb].name, name, MAX_NAME_LEN-1);
  snprintf(g_name2info[g_cloonix_nb].doors,MAX_NAME_LEN-1,"%s:%d",ip,port);
  g_name2info[g_cloonix_nb].ip = ipnum;
  g_name2info[g_cloonix_nb].port = port;
  g_name2info[g_cloonix_nb].novnc_port = novnc_port;
  strncpy(g_name2info[g_cloonix_nb].passwd, passwd, MSG_DIGEST_LEN-1);
  g_name2info[g_cloonix_nb].c2c_udp_ip = udp_ip;
  g_name2info[g_cloonix_nb].c2c_udp_port_low = c2c_udp_port_low;
  g_cloonix_nb += 1;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int can_read_file(char *path)
{
  int result = 0;
  struct stat stat_file;
  if (!stat(path, &stat_file))
    {
    if ((stat_file.st_mode & S_IFMT) == S_IFREG)
      if (!access(path, R_OK))
        result = 1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static int get_len_of_file(char *file_name)
{
  int result = -1;
  struct stat statbuf;
  if (stat(file_name, &statbuf) == 0)
    result = statbuf.st_size;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void read_complete_file(char *file_name, int len, char *buf)
{
  int fd, readlen;
  fd = open(file_name, O_RDONLY);
  if (fd <= 0)
    KOUT("Error %d opening file %s", errno, file_name);
  readlen = read(fd, buf, len);
  if (readlen != len)
    KOUT("Error reading file %s %d %d", file_name, len, readlen);
  buf[len] = 0;
  close (fd);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int find_entry(char *buf, char *entry, char *value, int max)
{
  char *ptr_end, *ptr_start;
  int  len, result = -1;
  ptr_start = buf;
  ptr_start = strstr(ptr_start, entry);
  if (ptr_start)
    {
    ptr_end = strchr(ptr_start, '\n');
    if (!ptr_end)
      KOUT("%s", ptr_start);
    len = ptr_end - ptr_start;
    if (len >= max)
      KOUT("%s      %d", ptr_start, len);
    if ((len - strlen(entry)) <= 1)
      KOUT("%s      %d  %s", ptr_start, len, entry);
    memcpy(value, ptr_start + strlen(entry), len - strlen(entry)); 
    value[len - strlen(entry)] = 0;
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int extract_info(char *buf)
{
  char *ptr_end, *ptr_start;
  char name[MAX_NAME_LEN];
  char ip[MAX_NAME_LEN];
  char c2c_udp_ip[MAX_NAME_LEN];
  char passwd[MSG_DIGEST_LEN];
  int  c2c_udp_port_low, novnc_port, port, result = -1;
  uint32_t ipnum, udp_ip;
  if (find_entry(buf, "CLOONIX_VERSION=", g_cloonix_version, MAX_NAME_LEN)) 
    KERR(" ");
  else
    {
    ptr_start = buf;
    while (ptr_start)
      {
      ptr_start = strstr(ptr_start, "CLOONIX_NET:");
      if (ptr_start)
        {
        ptr_end = strchr(ptr_start, '}');
        if (!ptr_end)
          KOUT("%s", ptr_start);
        *ptr_end = 0;
        if (sscanf(ptr_start, 
                   "CLOONIX_NET: %s { cloonix_ip %s cloonix_port %d "
                   "novnc_port %d cloonix_passwd %s c2c_udp_ip %s "
                   "c2c_udp_port_low %d",
                   name, ip, &port, &novnc_port, passwd,
                   c2c_udp_ip, &c2c_udp_port_low) != 7)
          KOUT("%s", ptr_start);
        ptr_start = ptr_end + 1;
        if (ip_string_to_int(&ipnum, ip))
          KOUT("Bad ip in:\n%s", ptr_start);
        if (ip_string_to_int(&udp_ip, c2c_udp_ip))
          KOUT("Bad c2c_udp_ip in:\n%s", ptr_start);
        if ((port < 1) || (port > 0xFFFF))
          KOUT("Bad port in:\n%s", ptr_start);
        if ((novnc_port < 1) || (novnc_port > 0xFFFF))
          KOUT("Bad novnc_port in:\n%s", ptr_start);
        if ((c2c_udp_port_low < 1) || (c2c_udp_port_low > 0xFFFF))
          KOUT("Bad c2c_udp_port_low in:\n%s", ptr_start);
        alloc_record(name, ip, ipnum, port, novnc_port, passwd,
                     udp_ip, c2c_udp_port_low);
        sprintf(g_all_names + strlen(g_all_names), "%s, ", name);
        result = 0;
        }
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
char *cloonix_conf_info_get_names(void)
{
  return (g_all_names);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
t_cloonix_conf_info *cloonix_cnf_info_get(char *name, int *rank)
{
  int i;
  t_cloonix_conf_info *result = NULL;
  for (i=0; i<g_cloonix_nb; i++)
    {
    if (!strcmp(g_name2info[i].name, name))
      {
      result = &(g_name2info[i]);
      *rank = i+1;
      break;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void cloonix_conf_info_get_all(int *nb, t_cloonix_conf_info **tab)
{
  *nb = g_cloonix_nb;
  *tab = g_name2info;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
char *cloonix_conf_info_get_version(void)
{
  return g_cloonix_version;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
char *cloonix_conf_info_get_tree(void)
{
  return("/usr/libexec/cloonix");
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int cloonix_conf_info_init(char *path_conf)
{
  int len, result;
  char *buf;
  g_cloonix_nb = 0;
  memset(g_name2info, 0, MAX_CLOONIX_SERVERS * sizeof(t_cloonix_conf_info));
  memset(g_all_names, 0, MAX_CLOONIX_SERVERS * MAX_NAME_LEN);
  memset(g_cloonix_version, 0, MAX_NAME_LEN);
  if (!can_read_file(path_conf))
    KOUT("Cannot read file: %s", path_conf);
  len = get_len_of_file(path_conf);
  if (len < 0)
    KOUT("Cannot get len of file: %s", path_conf);
  if (len > 100000)
    KOUT("Config file %s too big", path_conf);
  buf = (char *) clownix_malloc((len+1) *sizeof(char),13);
  read_complete_file(path_conf, len, buf);
  result = extract_info(buf);
  clownix_free(buf, __FUNCTION__);
  return result;
}
/*--------------------------------------------------------------------------*/

void cloonix_conf_linker_helper(void)
{
} 


