/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
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
#include "file_read_write.h"

static t_cloonix_conf_info g_name2info[MAX_CLOONIX_SERVERS];
static char g_all_names[MAX_CLOONIX_SERVERS * MAX_NAME_LEN];
static int g_cloonix_nb;
static char g_cloonix_version[MAX_NAME_LEN];

static void alloc_record(char *name, char *ip, uint32_t ipnum, int port, 
                         char *passwd, uint32_t udp_ip,
                         int c2c_udp_port_low, int web_port)
{
  if (g_cloonix_nb == MAX_CLOONIX_SERVERS - 1)
    KOUT("%d", g_cloonix_nb);
  strncpy(g_name2info[g_cloonix_nb].name, name, MAX_NAME_LEN-1);
  snprintf(g_name2info[g_cloonix_nb].doors,MAX_NAME_LEN-1,"%s:%d",ip,port);
  g_name2info[g_cloonix_nb].ip = ipnum;
  g_name2info[g_cloonix_nb].port = port;
  strncpy(g_name2info[g_cloonix_nb].passwd, passwd, MSG_DIGEST_LEN-1);
  g_name2info[g_cloonix_nb].c2c_udp_ip = udp_ip;
  g_name2info[g_cloonix_nb].c2c_udp_port_low = c2c_udp_port_low;
  g_name2info[g_cloonix_nb].web_port = web_port;
  g_cloonix_nb += 1;
}

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

static char *find_key(char *buf, char *key)
{
  int len = (int) strlen(key);
  char *p = buf;
  while ((p = strstr(p, key)) != NULL)
    {
    if ((p[len] == ' ') || (p[len] == '\t'))
      return p;
    p += len;
    }
  return NULL;
}

static int extract_info(char *buf)
{
  char *ptr_end, *ptr_start;
  char name[MAX_NAME_LEN];
  char ip[MAX_NAME_LEN];
  char c2c_udp_ip[MAX_NAME_LEN];
  char passwd[128];
  int  c2c_udp_port_low, port, result = -1;
  uint32_t ipnum, udp_ip;
  if (find_entry(buf, "CLOONIX_VERSION=", g_cloonix_version, MAX_NAME_LEN)) 
    KERR("ERROR no CLOONIX_VERSION= in the config");
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
        /* Keyword fields in any order; the web ports are optional. */
        int web_port = 0;
        { char *p;
          p = find_key(ptr_start, "CLOONIX_NET:");
          if (!p || sscanf(p, "CLOONIX_NET: %63s", name) != 1)
            KOUT("%s", ptr_start);
          p = find_key(ptr_start, "cloonix_ip");
          if (!p || sscanf(p, "cloonix_ip %63s", ip) != 1)
            KOUT("%s", ptr_start);
          p = find_key(ptr_start, "cloonix_port");
          if (!p || sscanf(p, "cloonix_port %d", &port) != 1)
            KOUT("%s", ptr_start);
          p = find_key(ptr_start, "cloonix_passwd");
          if (!p || sscanf(p, "cloonix_passwd %127s", passwd) != 1)
            KOUT("%s", ptr_start);
          p = find_key(ptr_start, "c2c_udp_ip");
          if (!p || sscanf(p, "c2c_udp_ip %63s", c2c_udp_ip) != 1)
            KOUT("%s", ptr_start);
          p = find_key(ptr_start, "c2c_udp_port_low");
          if (!p || sscanf(p, "c2c_udp_port_low %d", &c2c_udp_port_low) != 1)
            KOUT("%s", ptr_start);
          p = find_key(ptr_start, "cloonix_web");
          if (p) sscanf(p, "cloonix_web %d", &web_port);
        }
        ptr_start = ptr_end + 1;
        if (ip_string_to_int(&ipnum, ip))
          KOUT("Bad ip in:\n%s", ptr_start);
        if (ip_string_to_int(&udp_ip, c2c_udp_ip))
          KOUT("Bad c2c_udp_ip in:\n%s", ptr_start);
        if ((port < 1) || (port > 0xFFFF))
          KOUT("Bad port in:\n%s", ptr_start);
        if ((c2c_udp_port_low < 1) || (c2c_udp_port_low > 0xFFFF))
          KOUT("Bad c2c_udp_port_low in:\n%s", ptr_start);
        alloc_record(name, ip, ipnum, port, passwd,
                     udp_ip, c2c_udp_port_low, web_port);
        sprintf(g_all_names + strlen(g_all_names), "%s, ", name);
        result = 0;
        }
      }
    }
  return result;
}

char *cloonix_conf_info_get_names(void)
{
  return (g_all_names);
}

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

char *cloonix_conf_info_get_version(void)
{
  return g_cloonix_version;
}

int cloonix_conf_info_init(char *path_conf)
{
  int len, result;
  char *buf, err[MAX_PATH_LEN];
  g_cloonix_nb = 0;
  memset(g_name2info, 0, MAX_CLOONIX_SERVERS * sizeof(t_cloonix_conf_info));
  memset(g_all_names, 0, MAX_CLOONIX_SERVERS * MAX_NAME_LEN);
  memset(g_cloonix_version, 0, MAX_NAME_LEN);
  buf = read_whole_file(path_conf, &len, err);
  if (!buf)
    KOUT("Cannot read file: %s %s", path_conf, err);
  if (len > 100000)
    KOUT("Config file %s too big", path_conf);
  result = extract_info(buf);
  clownix_free(buf, __FUNCTION__);
  return result;
}
