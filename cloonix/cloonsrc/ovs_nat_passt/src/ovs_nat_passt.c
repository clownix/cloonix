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
#include "stall_detect.h"
#include "rpc_clownix.h"
#include "passt_shuttle.h"
#include "tun_tap.h"
#include "machine.h"

/* NAT_TRACE: the per-beat nat chatter, OFF. */

#define NAT_TRACE 0
#if NAT_TRACE
#define NAT_DBG(format, a...) KERR(format, ## a)
#else
#define NAT_DBG(format, a...) do { } while (0)
#endif

static int  g_llid_prxy;
static long long g_proxy_fed_ms;
/* The proxymous leg has its own watchdog: a beat a second from here, fed
   by nat_proxy_beat_resp, the limit and the forgiveness of ctrl_link. */
#define PROXY_WATCHDOG_MS 20000
#define PROXY_BEAT_MS 1000
#define PROXY_WATCHDOG_POLL_BEATS 100
static t_stall g_proxy_stall;
static char g_net_name[MAX_NAME_LEN];
static char g_netns_namespace[MAX_PATH_LEN];
static char g_nat_name[MAX_NAME_LEN];
static char g_vhost[MAX_NAME_LEN];
static char g_root_path[MAX_PATH_LEN];
static char g_ctrl_path[MAX_PATH_LEN];
static char g_prxy_path[MAX_PATH_LEN];
static char g_nat_path[MAX_PATH_LEN];
static char g_passt_sock_path[MAX_PATH_LEN];
static int  g_passt_fd = -1;
static int  g_fd_rx_from_tap = -1;
static int  g_fd_tx_to_tap = -1;

 
    
char *get_nat_name(void)
{   
  return (g_nat_name);
}   

static void err_prxy_cb (int llid, int err, int from)
{
  KERR("WARNING nat %s: the proxymous connection closed, stopping",
       get_nat_name());
  unlink(g_prxy_path);
  ctrl_link_stop();
}

static void rx_prxy_cb (int llid, int len, char *buf)
{
  if (rpct_decoder(llid, len, buf))
    KOUT("ERROR %s", buf);
}

static void heartbeat (int delta)
{
  static int count_ticks_blkd = 0;
  int llid;
  (void)delta;
  /* Drain the passt write backlog every tick. */
  passt_shuttle_heartbeat();
  count_ticks_blkd += 1;
  if (count_ticks_blkd == 10)
    {
    count_ticks_blkd = 0;
    if (g_llid_prxy == 0)
      {
      llid = string_client_unix(g_prxy_path, err_prxy_cb, rx_prxy_cb, "prxy");
      if (llid != 0)
        g_llid_prxy = llid;
      }
    }
}

/* PASST_CONNECT_TRIES of 200 ms, under the 50 the switch waits. */
#define PASST_CONNECT_TRIES 40
static int g_hello_tid;
static int g_passt_tries;
static int g_passt_connecting;
static void timer_passt_connect(void *data)
{
  char resp[MAX_PATH_LEN];
  if (g_passt_fd < 0)
    g_passt_fd = passt_shuttle_connect(g_passt_sock_path);
  NAT_DBG("TRACE passt connect try %d on %s: fd %d (llid switch %d, proxy %d)",
          g_passt_tries, g_passt_sock_path, g_passt_fd, ctrl_link_llid(), g_llid_prxy);
  if (g_passt_fd >= 0)
    {
    g_passt_connecting = 0;
    passt_shuttle_set_tx(g_fd_tx_to_tap);
    passt_shuttle_init(g_fd_rx_from_tap, g_passt_fd);
    snprintf(resp, MAX_PATH_LEN-1, "nat_suidroot_ok %s", get_nat_name());
    rpct_send_sigdiag_msg(ctrl_link_llid(), g_hello_tid, resp);
    }
  else if (g_passt_tries < PASST_CONNECT_TRIES)
    {
    g_passt_tries += 1;
    clownix_timeout_add(20, timer_passt_connect, NULL, NULL, NULL);
    }
  else
    {
    g_passt_connecting = 0;
    KERR("ERROR passt connect %s: not there after %d tries",
         g_passt_sock_path, g_passt_tries);
    snprintf(resp, MAX_PATH_LEN-1, "nat_suidroot_ko %s", get_nat_name());
    rpct_send_sigdiag_msg(ctrl_link_llid(), g_hello_tid, resp);
    }
}

void rpct_recv_pid_req(int llid, int tid, char *name, int num)
{
  ctrl_link_pid_req(llid, tid, name, num);
}

/* The proxy socket is proxymous', made for this nat and gone with it.    */
void rpct_recv_kil_req(int llid, int tid)
{
  unlink(g_prxy_path);
  ctrl_link_stop();
}

void rpct_recv_poldiag_msg(int llid, int tid, char *line)
{
  char name[MAX_NAME_LEN];
  memset(name, 0, MAX_NAME_LEN);
  if (sscanf(line, "nat_proxy_beat_resp %63s", name) == 1)
    {
    if (!strcmp(get_nat_name(), name))
      {
      g_proxy_fed_ms = stall_now_ms();
      stall_answered(&g_proxy_stall);
      }
    else
      KERR("ERROR %s %s", line, get_nat_name());
    }
  else
    KERR("ERROR %s", line);
}

void rpct_recv_sigdiag_msg(int llid, int tid, char *line)
{
  char prxy_passt[MAX_PATH_LEN];
  char resp[MAX_PATH_LEN];
  char name[MAX_NAME_LEN];
  char str_ip[MAX_NAME_LEN];
  int num, cli_llid, cli_tid;
  uint32_t addr_ip;
  uint8_t m[6];
  memset(resp, 0, MAX_PATH_LEN);
  if (llid == g_llid_prxy)
    {
    if (sscanf(line,
    "nat_proxy_hello_resp %63s passt:%299s", name, prxy_passt) == 2)
      {
      if (strcmp(name, get_nat_name()))
        KERR("ERROR %s %s", name, get_nat_name());
      if (strcmp(prxy_passt, g_passt_sock_path))
        KTRC("passt socket %s seen here as %s", prxy_passt, g_passt_sock_path);
      /* The path is authoritative; the socket may not be bound yet, passt
         having just been spawned. */
      g_hello_tid = tid;
      NAT_DBG("TRACE hello_resp from proxy: passt %s, fd %d, connecting %d",
              prxy_passt, g_passt_fd, g_passt_connecting);
      if (g_passt_fd >= 0)
        {
        snprintf(resp, MAX_PATH_LEN-1, "nat_suidroot_ok %s", get_nat_name());
        rpct_send_sigdiag_msg(ctrl_link_llid(), tid, resp);
        }
      else if (!g_passt_connecting)
        {
        g_passt_connecting = 1;
        g_passt_tries = 0;
        timer_passt_connect(NULL);
        }
      }
    else
      KERR("ERROR %s", line);
    }
  else if (llid == ctrl_link_llid())
    {
    if (tid != type_hop_nat_main)
      KERR("ERROR %d %d", tid, type_hop_nat_main);
    if (!strncmp(line,
    "nat_suidroot", strlen("nat_suidroot")))
      {
      if (g_llid_prxy == 0) 
        {
        snprintf(resp, MAX_PATH_LEN-1,
        "nat_suidroot_ko %s", get_nat_name());
        rpct_send_sigdiag_msg(llid, tid, resp);
        }
      else
        {
        memset(resp, 0, MAX_PATH_LEN);
        snprintf(resp, MAX_PATH_LEN-1,
        "nat_proxy_hello_req %s", get_nat_name());
        NAT_DBG("TRACE nat_suidroot from switch llid %d, hello to proxy %d",
                llid, g_llid_prxy);
        rpct_send_sigdiag_msg(g_llid_prxy, 0, resp);
        }
      }
    else if (sscanf(line, "nat_passt_state_req %63s", name) == 1)
      {
      snprintf(resp, MAX_PATH_LEN-1, "nat_passt_state_resp %s %d %d %d",
               get_nat_name(), passt_shuttle_connected(),
               passt_shuttle_generation(), passt_shuttle_alive());
      rpct_send_sigdiag_msg(llid, tid, resp);
      }
    else if (!strncmp(line,
    "nat_machine_begin", strlen("nat_machine_begin")))
      {
      machine_begin();
      }
    else if (sscanf(line,
    "nat_machine_add %63s eth:%d "
    "mac:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
      name, &num, &(m[0]), &(m[1]), &(m[2]), &(m[3]), &(m[4]), &(m[5])) == 8)
      {
      machine_add(name, num, m);
      }
    else if (!strncmp(line,
    "nat_machine_end", strlen("nat_machine_end")))
      {
      machine_end();
      }
  
    else if (sscanf(line,
    "nat_whatip cli_llid=%d cli_tid=%d name=%63s",&cli_llid,&cli_tid,name) == 3)
      {
      memset(resp, 0, MAX_PATH_LEN);
      addr_ip = machine_ip_get(name);
      if (addr_ip)
        {
        int_to_ip_string(addr_ip, str_ip);
        snprintf(resp,MAX_PATH_LEN-1,
        "nat_whatip_ok cli_llid=%d cli_tid=%d name=%s ip=%s",
        cli_llid, cli_tid, name, str_ip);
        }
      else
        {
        snprintf(resp,MAX_PATH_LEN-1,
        "nat_whatip_ko cli_llid=%d cli_tid=%d name=%s",cli_llid,cli_tid,name);
        }
      rpct_send_sigdiag_msg(llid, tid, resp);
      }
    else
      KERR("ERROR %s", line);
    }
  else
    KERR("ERROR %d %d %d", llid, ctrl_link_llid(), g_llid_prxy);
}

static void timeout_proxy_watchdog(void *data)
{
  char msg[MAX_PATH_LEN];
  long long silence = stall_now_ms() - g_proxy_fed_ms;
  int gap = stall_beat(&g_proxy_stall);
  int credit = stall_credit_ms(&g_proxy_stall);
  if (gap > 0)
    KERR("WARNING nat %s: this loop did not beat for %d ms (%s), "
         "forgiven to proxymous", get_nat_name(), gap,
         stall_why(&g_proxy_stall));
  if (silence > PROXY_WATCHDOG_MS + credit)
    {
    KERR("WARNING nat %s: proxymous %s for %lld ms (%d ms forgiven), "
         "stopping", get_nat_name(),
         g_llid_prxy ? "said nothing" : "could not be connected",
         silence, credit);
    ctrl_link_stop();
    }
  if (g_llid_prxy)
    {
    memset(msg, 0, MAX_PATH_LEN);
    snprintf(msg, MAX_PATH_LEN-1, "nat_proxy_beat_req %s", get_nat_name());
    rpct_send_poldiag_msg(g_llid_prxy, 0, msg);
    }
  clownix_timeout_add(PROXY_WATCHDOG_POLL_BEATS, timeout_proxy_watchdog,
                      NULL, NULL, NULL);
}

int main (int argc, char *argv[])
{
  char *root = g_root_path;
  char *nat = g_nat_name;
  int pid, fd_rx_from_tap, fd_tx_to_tap;
  if (argc != 5)
    KOUT("ERROR %d", argc);
  umask(0000);
  g_llid_prxy = 0;
  memset(g_net_name, 0, MAX_NAME_LEN);
  memset(g_netns_namespace, 0, MAX_PATH_LEN);
  memset(g_root_path, 0, MAX_PATH_LEN);
  memset(g_ctrl_path, 0, MAX_PATH_LEN);
  memset(g_prxy_path, 0, MAX_PATH_LEN);
  memset(g_nat_name, 0, MAX_NAME_LEN);
  memset(g_vhost, 0, MAX_NAME_LEN);
  memset(g_nat_path, 0, MAX_PATH_LEN);
/* Setuid binary: its arguments are not trusted.                            */
  if (!name_is_sane(argv[1]))
    KOUT("ERROR refused net name");
  if (!path_is_sane(argv[2]))
    KOUT("ERROR refused root path");
  if (!name_is_sane(argv[3]))
    KOUT("ERROR refused nat name");
  if (!name_is_sane(argv[4]))
    KOUT("ERROR refused vhost name");
  strncpy(g_net_name,  argv[1], MAX_NAME_LEN-1);
  strncpy(g_root_path, argv[2], MAX_PATH_LEN-1);
  strncpy(g_nat_name,  argv[3], MAX_NAME_LEN-1);
  strncpy(g_vhost,  argv[4], MAX_NAME_LEN-1);
  snprintf(g_ctrl_path,MAX_PATH_LEN-1,"%s/%s/c%s",root,NAT_MAIN_DIR, nat);
  snprintf(g_nat_path, MAX_PATH_LEN-1,"%s/%s/%s", root, NAT_MAIN_DIR, nat);
  snprintf(g_prxy_path, MAX_PATH_LEN-1,"%s_%s/%s/%s",
          PROXYSHARE_IN, g_net_name, NAT_PROXY_DIR, nat);
  snprintf(g_passt_sock_path, MAX_PATH_LEN-1,"%s_%s/%s/%s_passt",
          PROXYSHARE_IN, g_net_name, NAT_PROXY_DIR, nat);
  snprintf(g_netns_namespace, MAX_PATH_LEN-1, "%s%s_%s",
           PATH_NAMESPACE, BASE_NAMESPACE, g_net_name);
  pid = tun_tap_open(g_netns_namespace, g_vhost,
                             &fd_rx_from_tap, &fd_tx_to_tap);
  msg_mngt_init("nat", TRAF_TAP_BUF_LEN);
  set_cloonix_name(g_net_name);
  msg_mngt_heartbeat_init(heartbeat);
  g_fd_rx_from_tap = fd_rx_from_tap;
  g_fd_tx_to_tap   = fd_tx_to_tap;
  ctrl_link_init(g_net_name, "nat", g_nat_name, type_hop_nat_main,
                 g_ctrl_path);
  ctrl_link_add_path(g_nat_path);
  ctrl_link_add_child(pid);
  g_proxy_fed_ms = stall_now_ms();
  stall_init(&g_proxy_stall, PROXY_BEAT_MS, PROXY_WATCHDOG_MS);
  clownix_timeout_add(PROXY_WATCHDOG_POLL_BEATS, timeout_proxy_watchdog,
                      NULL, NULL, NULL);
  if (daemon(0,0))
    KOUT("ERROR daemon %s", strerror(errno));
  /* The tap is open: root is not needed again, and is not kept. */
  cloonix_drop_root();
  cloonix_set_pid(getpid());
  msg_mngt_loop();
  return 0;
}

