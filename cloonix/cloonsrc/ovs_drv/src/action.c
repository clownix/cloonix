/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <libgen.h>

#include "io_clownix.h"
#include "ovs_cmd.h"
#include "ovs_execv.h"

static int g_ovs_launched;
static int g_ovsdb_launched;

static int check_uid(void)
{
  int result = -1;
  uid_t prev_uid, uid;
  prev_uid = geteuid();
  if (seteuid(0))
    KERR("ERROR %s", strerror(errno));
  uid = geteuid();
  if (uid == 0)
    result = 0;
  if (seteuid(prev_uid))
    KERR("ERROR %s", strerror(errno));
  return result;
}

int action_resp(char *respb, int failed, char *format, ...)
{
  va_list ap;
  int len;
  len = snprintf(respb, MAX_PATH_LEN-1, "%s ", failed?"KO":"OK");
  va_start(ap, format);
  vsnprintf(respb+len, MAX_PATH_LEN-1-len, format, ap);
  va_end(ap);
  return failed;
}

void action_vhost_up(char *bin, char *db, char *respb,
                     char *name, int num, char *vhost)
{
  if (action_resp(respb, ovs_cmd_vhost_up(bin, db, name, num, vhost),
                  "ovs_vhost_up name=%s num=%d vhost=%s", name, num, vhost))
    KERR("ERROR %s %d: %s cannot be set up, it does not exist in the net "
         "namespace", name, num, vhost);
}

void action_add_snf_lan(char *bin, char *db, char *respb,
                        char *name, int num, char *vhost, char *lan)
{
  action_resp(respb, ovs_cmd_add_snf_lan(bin, db, name, num, vhost, lan),
              "ovs_add_snf_lan name=%s num=%d vhost=%s lan=%s",
              name, num, vhost, lan);
}

void action_del_snf_lan(char *bin, char *db, char *respb,
                        char *name, int num, char *vhost, char *lan)
{
  action_resp(respb, ovs_cmd_del_snf_lan(bin, db, name, num, vhost, lan),
              "ovs_del_snf_lan name=%s num=%d vhost=%s lan=%s",
              name, num, vhost, lan);
}

void action_add_lan(char *bin, char *db, char *respb, char *lan)
{
  if (action_resp(respb, ovs_cmd_add_lan(bin, db, lan),
                  "ovs_add_lan lan=%s", lan))
    KERR("%s", respb);
}

void action_add_lan_rstp(char *bin, char *db, char *respb, char *lan, int val)
{
  if (action_resp(respb, ovs_cmd_add_lan_rstp(bin, db, lan, val),
                  "ovs_add_lan_rstp lan=%s val=%d", lan, val))
    KERR("%s", respb);
}

void action_del_lan(char *bin, char *db, char *respb, char *lan)
{
  if (action_resp(respb, ovs_cmd_del_lan(bin, db, lan),
                  "ovs_del_lan lan=%s", lan))
    KERR("%s", respb);
}

void action_add_lan_endp(char *bin, char *db, char *respb,
                         char *lan, char *name, int num, char *vhost)
{
  if (action_resp(respb, ovs_cmd_add_lan_endp(bin, db, lan, vhost),
                  "ovs_add_lan_endp name=%s num=%d vhost=%s lan=%s",
                  name, num, vhost, lan))
    KERR("%s", respb);
}

void action_del_lan_endp(char *bin, char *db, char *respb,
                         char *lan, char *name, int num, char *vhost)
{
  if (action_resp(respb, ovs_cmd_del_lan_endp(bin, db, lan, vhost),
                  "ovs_del_lan_endp name=%s num=%d vhost=%s lan=%s",
                  name, num, vhost, lan))
    KERR("%s", respb);
}

void action_req_ovsdb(char *bin, char *db, char *net, char *respb)
{
  int pid;
  if (g_ovsdb_launched == 0)
    {
    g_ovsdb_launched = 1;
    if (create_ovsdb_server_conf(bin, db))
      {
      KERR("ERROR CREATION DB");
      snprintf(respb, MAX_PATH_LEN-1, "ovs_resp_ovsdb_ko");
      }
    else
      {
      usleep(100000);
      pid = ovs_execv_ovsdb_server(net, bin, db);
      if (pid <= 0)
        {
        KERR("ERROR CREATION DB SERVER");
        snprintf(respb, MAX_PATH_LEN-1, "ovs_resp_ovsdb_ko");
        }
      else
        {
        snprintf(respb, MAX_PATH_LEN-1, "ovs_resp_ovsdb_ok pid=%d", pid);
        }
      }
    }
  else
    {
    KERR("ERROR DB SERVER");
    snprintf(respb, MAX_PATH_LEN-1, "ovs_resp_ovsdb_ko");
    }
  usleep(10000);
}

void action_req_ovs_switch(char *bin, char *db, char *net, char *respb)
{
  int pid;
  if (g_ovs_launched == 0)
    {
    g_ovs_launched = 1;
    pid = ovs_execv_ovs_vswitchd(net, bin, db);
    if (pid <= 0)
      {
      KERR("BAD START OVS");
      snprintf(respb, MAX_PATH_LEN-1, "ovs_resp_ovs_ko");
      }
    else
      {
      snprintf(respb, MAX_PATH_LEN-1, "ovs_resp_ovs_ok pid=%d", pid);
      }
    }
  else
    {
    KERR("OVS LAUNCH ALREADY TRIED");
    snprintf(respb, MAX_PATH_LEN-1, "ovs_resp_ovs_ko");
    }
  usleep(10000);
}

void action_req_suidroot(char *respb)
{
  g_ovs_launched = 0;
  g_ovsdb_launched = 0;
  if (check_uid())
    snprintf(respb, MAX_PATH_LEN-1, "ovs_resp_suidroot_ko");
  else
    snprintf(respb, MAX_PATH_LEN-1, "ovs_resp_suidroot_ok");
}

