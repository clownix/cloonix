/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <fcntl.h>
#include <sched.h>
#include <unistd.h>
#include <sys/mount.h>
#include <signal.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "utils.h"
#include "rxtx.h"
#include "tun_tap.h"
#include "sched.h"
#include "circle.h"
#include "replace.h"

static char g_net_name[MAX_NAME_LEN];
static char g_netns_namespace[MAX_PATH_LEN];
static char g_a2b_name[MAX_NAME_LEN];
static char g_vhost0[MAX_NAME_LEN];
static char g_vhost1[MAX_NAME_LEN];
static char g_root_path[MAX_PATH_LEN];
static char g_ctrl_path[MAX_PATH_LEN];
static char g_a2b_path[MAX_PATH_LEN];

void rpct_recv_pid_req(int llid, int tid, char *name, int num)
{
  ctrl_link_pid_req(llid, tid, name, num);
}

void rpct_recv_kil_req(int llid, int tid)
{
  ctrl_link_stop();
}

void rpct_recv_poldiag_msg(int llid, int tid, char *line)
{
  KERR("ERROR %s", line);
}

static int config_replace(char *line, int llid, int tid, char *resp,
                          int cli_llid, int cli_tid, char *name)
{
  int dir, result = -1;
  unsigned char t[4];
  unsigned char s[4];
  if (result && (!strncmp(line, "replace_ipdst", strlen("replace_ipdst"))))
    {
    if (sscanf(line,
        "replace_ipdst dir=%d %hhu.%hhu.%hhu.%hhu with %hhu.%hhu.%hhu.%hhu",
         &dir, &(t[0]), &(t[1]), &(t[2]), &(t[3]),
         &(s[0]), &(s[1]), &(s[2]), &(s[3])) == 9)
      {
      result = 0;
      snprintf(resp, MAX_PATH_LEN-1,
               "a2b_param_config_ok %d %d %s", cli_llid, cli_tid, name);
      replace_cnf(dir, "replace_ipdst", t, s);
      }
    }
  if (result && (!strncmp(line, "replace_ipsrc", strlen("replace_ipsrc"))))
    {
    if (sscanf(line,
        "replace_ipsrc dir=%d %hhu.%hhu.%hhu.%hhu with %hhu.%hhu.%hhu.%hhu",
         &dir, &(t[0]), &(t[1]), &(t[2]), &(t[3]),
         &(s[0]), &(s[1]), &(s[2]), &(s[3])) == 9)
      {
      result = 0;
      snprintf(resp, MAX_PATH_LEN-1,
               "a2b_param_config_ok %d %d %s", cli_llid, cli_tid, name);
      replace_cnf(dir, "replace_ipsrc", t, s);
      }
    }
  if (result && (!strncmp(line, "unreplace_ipdst", strlen("unreplace_ipdst"))))
    {
    if (sscanf(line, "unreplace_ipdst dir=%d", &dir) == 1)
      {
      result = 0;
      snprintf(resp, MAX_PATH_LEN-1,
               "a2b_param_config_ok %d %d %s", cli_llid, cli_tid, name);
      replace_cnf(dir, "unreplace_ipdst", NULL, NULL);
      }
    }
  if (result && (!strncmp(line, "unreplace_ipsrc", strlen("unreplace_ipsrc"))))
    {
    if (sscanf(line, "unreplace_ipsrc dir=%d", &dir) == 1)
      {
      result = 0;
      snprintf(resp, MAX_PATH_LEN-1,
               "a2b_param_config_ok %d %d %s", cli_llid, cli_tid, name);
      replace_cnf(dir, "unreplace_ipsrc", NULL, NULL);
      }
    }
  return result;
}

void rpct_recv_sigdiag_msg(int llid, int tid, char *line)
{
  char resp[MAX_PATH_LEN];
  char name[MAX_NAME_LEN];
  char cmd[MAX_NAME_LEN];
  char *msg;
  int cli_llid, cli_tid, dir, val;
  memset(resp, 0, MAX_PATH_LEN);
  if (llid != ctrl_link_llid())
    KERR("ERROR %d %d", llid, ctrl_link_llid());
  if (tid != type_hop_a2b)
    KERR("ERROR %d %d", tid, type_hop_a2b);
  if (!strncmp(line,
  "a2b_suidroot", strlen("a2b_suidroot")))
    {
    snprintf(resp, MAX_PATH_LEN-1, "a2b_suidroot_ok %s", g_a2b_name);
    rpct_send_sigdiag_msg(llid, tid, resp);
    }
  else if (sscanf(line,
  "a2b_param_config %d %d %63s", &cli_llid, &cli_tid, name) == 3)
    {
    snprintf(resp, MAX_PATH_LEN-1, "a2b_param_config_ko %d %d %s",
             cli_llid, cli_tid, name);
    msg = strstr(line, "dir_cmd_val=");
    if (msg)
      {
      msg += strlen("dir_cmd_val=");
      if (sscanf(msg, "dir=%d cmd=%63s val=%d", &dir, cmd, &val) == 3)
        {
        snprintf(resp, MAX_PATH_LEN-1,
                 "a2b_param_config_ok %d %d %s",
                 cli_llid, cli_tid, name);
        sched_cnf(dir, cmd, val);
        }
      }
    else
      {
      msg = strstr(line, "unreplace_ipdst");
      if (!msg)
        msg = strstr(line, "unreplace_ipsrc");
      if (!msg)
        msg = strstr(line, "replace_ipdst");
      if (!msg)
        msg = strstr(line, "replace_ipsrc");
      if (!msg)
        KERR("ERROR %s", line);
      else if (config_replace(msg,llid,tid,resp,cli_llid,cli_tid,name))
        KERR("ERROR %s", line);
      }
    rpct_send_sigdiag_msg(llid, tid, resp);
    }
  else
    KERR("ERROR %s", line);
}

char *get_a2b_name(void)
{
  return (g_a2b_name);
}

int main (int argc, char *argv[])
{
  char *root = g_root_path;
  char *a2b = g_a2b_name;
  int pid0, pid1;
  int fd_rx_from_tap0, fd_tx_to_tap0, fd_rx_from_tap1, fd_tx_to_tap1;
  if (argc != 6)
    KOUT("ERROR %d", argc);
  umask(0000);
  memset(g_net_name, 0, MAX_NAME_LEN);
  memset(g_netns_namespace, 0, MAX_PATH_LEN);
  memset(g_root_path, 0, MAX_PATH_LEN);
  memset(g_ctrl_path, 0, MAX_PATH_LEN);
  memset(g_a2b_name, 0, MAX_NAME_LEN);
  memset(g_vhost0, 0, MAX_NAME_LEN);
  memset(g_vhost1, 0, MAX_NAME_LEN);
  memset(g_a2b_path, 0, MAX_PATH_LEN);

/* Setuid binary: its arguments are not trusted. */
  if (!name_is_sane(argv[1]))
    KOUT("ERROR refused net name");
  if (!path_is_sane(argv[2]))
    KOUT("ERROR refused root path");
  if (!name_is_sane(argv[3]))
    KOUT("ERROR refused a2b name");
  if (!name_is_sane(argv[4]))
    KOUT("ERROR refused vhost0 name");
  if (!name_is_sane(argv[5]))
    KOUT("ERROR refused vhost1 name");
  strncpy(g_net_name,  argv[1], MAX_NAME_LEN-1);
  strncpy(g_root_path, argv[2], MAX_PATH_LEN-1);
  strncpy(g_a2b_name,  argv[3], MAX_NAME_LEN-1);
  strncpy(g_vhost0,  argv[4], MAX_NAME_LEN-1);
  strncpy(g_vhost1,  argv[5], MAX_NAME_LEN-1);
  snprintf(g_ctrl_path, MAX_PATH_LEN-1,"%s/%s/c%s", root, A2B_DIR, a2b);
  snprintf(g_a2b_path, MAX_PATH_LEN-1,"%s/%s/%s", root, A2B_DIR, a2b);
  snprintf(g_netns_namespace, MAX_PATH_LEN-1, "%s%s_%s",
           PATH_NAMESPACE, BASE_NAMESPACE, g_net_name);

  set_cloonix_name(g_net_name);
  pid0 = tun_tap_open(g_netns_namespace, g_vhost0,
                             &fd_rx_from_tap0, &fd_tx_to_tap0);
  pid1 = tun_tap_open(g_netns_namespace, g_vhost1,
                             &fd_rx_from_tap1, &fd_tx_to_tap1);

  msg_mngt_init("a2b", TRAF_TAP_BUF_LEN);
  msg_mngt_heartbeat_ms_set(1);
  rxtx_init(fd_rx_from_tap0, fd_tx_to_tap0, fd_rx_from_tap1, fd_tx_to_tap1);
  ctrl_link_init(g_net_name, "a2b", g_a2b_name, type_hop_a2b, g_ctrl_path);
  ctrl_link_add_path(g_a2b_path);
  ctrl_link_add_child(pid0);
  ctrl_link_add_child(pid1);

  if (daemon(0,0))
    KOUT("ERROR daemon %s", strerror(errno));
  /* The tap is open: root is not needed again, and is not kept. */
  cloonix_drop_root();
  cloonix_set_pid(getpid());
  msg_mngt_loop();
  return 0;
}

