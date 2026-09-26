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

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "pcap_record.h"
#include "eventfull.h"
#include "tun_tap.h"
#include "inotify_trigger.h"

static char g_net_name[MAX_NAME_LEN];
static char g_netns_namespace[MAX_PATH_LEN];
static char g_snf_name[MAX_NAME_LEN];
static char g_root_path[MAX_PATH_LEN];
static char g_ctrl_path[MAX_PATH_LEN];
static char g_snf_path[MAX_PATH_LEN];

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

void rpct_recv_sigdiag_msg(int llid, int tid, char *line)
{
  char resp[MAX_PATH_LEN];
  char mac[MAX_NAME_LEN];
  char name[MAX_NAME_LEN];
  int num, status, cmd;
  memset(resp, 0, MAX_PATH_LEN);
  if (llid != ctrl_link_llid())
    KERR("ERROR %d %d", llid, ctrl_link_llid());
  if (tid != type_hop_snf)
    KERR("ERROR %d %d", tid, type_hop_snf);
  if (!strncmp(line,
  "cloonsnf_suidroot", strlen("cloonsnf_suidroot")))
    {
    snprintf(resp, MAX_PATH_LEN-1, "cloonsnf_suidroot_ok %s", g_snf_name);
    rpct_send_sigdiag_msg(llid, tid, resp);
    }
  else if (sscanf(line,
  "cloonsnf_mac_spyed_tx_on=%63s", mac) == 1) 
    {
    tun_tap_add_mac(1, mac);
    }
  else if (sscanf(line,
  "cloonsnf_mac_spyed_rx_on=%63s", mac) == 1)
    {
    tun_tap_add_mac(0, mac);
    }
  else if (!strcmp(line,
  "cloonsnf_mac_spyed_purge"))
    {
    tun_tap_purge_mac();
    }
  else if (sscanf(line,
  "cloonsnf_sync_wireshark_req %63s %d %d", name, &num, &cmd) == 3) 
    {
    if (cmd)
      {
      status = 2;
      snprintf(resp, MAX_PATH_LEN-1,
               "cloonsnf_sync_wireshark_resp %s %d %d",
               name, num, status);
      rpct_send_sigdiag_msg(llid, tid, resp);
      } 
    else
      {
      if ((!access(g_ctrl_path, R_OK)) && (!inotify_get_state()))
        {
        status = 0;
        }
      else
        {
        KERR("WARNING WIRESHARK SPY NOT READY %d", inotify_get_state());
        status = 1;
        }
      snprintf(resp, MAX_PATH_LEN-1,
               "cloonsnf_sync_wireshark_resp %s %d %d",
               name, num, status);
      rpct_send_sigdiag_msg(llid, tid, resp);
      }
    }
  else
    KERR("ERROR %s", line);
}

/* 2 seconds, deliberately generous. */
#define SNF_SLOW_START_MS 2000

static unsigned long g_start_ms;
static unsigned long g_ms_setns;
static unsigned long g_ms_tap;
static unsigned long g_ms_socket;

static void snf_startup_report(void)
{
  unsigned long total;
  if ((!g_start_ms) || (!g_ms_setns) || (!g_ms_tap) || (!g_ms_socket))
    return;
  total = cloonix_now_ms() - g_start_ms;
  if (total < SNF_SLOW_START_MS)
    return;
  KERR("WARNING %s: startup took %lu ms -- setns %lu, tap open %lu, "
       "socket served %lu, the rest to the event loop %lu. The main server "
       "gives up connecting after about 12 seconds", g_snf_name, total,
       g_ms_setns - g_start_ms, g_ms_tap - g_ms_setns,
       g_ms_socket - g_ms_tap, cloonix_now_ms() - g_ms_socket);
}

int main (int argc, char *argv[])
{
  char *root = g_root_path;
  char *snf = g_snf_name;
  int fd;
  if (argc != 5)
    KOUT("%d", argc);
  umask(0000);
  memset(g_net_name, 0, MAX_NAME_LEN);
  memset(g_netns_namespace, 0, MAX_PATH_LEN);
  memset(g_root_path, 0, MAX_PATH_LEN);
  memset(g_ctrl_path, 0, MAX_PATH_LEN);
  memset(g_snf_path, 0, MAX_PATH_LEN);
/* Setuid binary: its arguments are not trusted.                            */
  if (!name_is_sane(argv[1]))
    KOUT("ERROR refused net name");
  if (!path_is_sane(argv[2]))
    KOUT("ERROR refused root path");
  if (!name_is_sane(argv[3]))
    KOUT("ERROR refused snf name");
  if (!name_is_sane(argv[4]))
    KOUT("ERROR refused fifo name");
  strncpy(g_net_name,  argv[1], MAX_NAME_LEN-1);
  strncpy(g_root_path, argv[2], MAX_PATH_LEN-1);
  strncpy(g_snf_name,  argv[3], MAX_NAME_LEN-1);
  snprintf(g_ctrl_path, MAX_PATH_LEN-1,"%s/%s/c%s", root, SNF_DIR, snf);
  snprintf(g_snf_path, MAX_PATH_LEN-1,"%s/%s/%s", root, SNF_DIR, argv[4]);
  snprintf(g_netns_namespace, MAX_PATH_LEN-1, "%s%s_%s",
           PATH_NAMESPACE, BASE_NAMESPACE, g_net_name);

  g_start_ms = cloonix_now_ms();
  msg_mngt_init("snf", IO_MAX_BUF_LEN);
  set_cloonix_name(g_net_name);
  fd = open(g_netns_namespace, O_RDONLY|O_CLOEXEC);
  if (fd < 0) 
    KOUT(" ");
  if (setns(fd, CLONE_NEWNET) != 0)
    KOUT(" ");
  close(fd);
  g_ms_setns = cloonix_now_ms();
  if (tun_tap_open(snf))
    KOUT("Problem tap %s\n", snf);
  g_ms_tap = cloonix_now_ms();
  ctrl_link_init(g_net_name, "snf", g_snf_name, type_hop_snf, g_ctrl_path);
  ctrl_link_keep_path(g_snf_path);
  g_ms_socket = cloonix_now_ms();
  if (daemon(0,0))
    KOUT("ERROR daemon %s", strerror(errno));
  eventfull_init(g_snf_name);
  pcap_record_init(g_snf_path);
  /* The tap is open: root is not needed again, and is not kept. */
  cloonix_drop_root();
  cloonix_set_pid(getpid());
  snf_startup_report();
  msg_mngt_loop();
  return 0;
}

