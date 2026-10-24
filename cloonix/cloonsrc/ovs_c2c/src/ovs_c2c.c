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
#include "rxtx.h"
#include "tun_tap.h"
#include "udp.h"
#include "packet_arp_mangle.h"

typedef struct t_timer_data
{
  uint8_t wait_for_probe_idx;
  char dist_ip[MAX_NAME_LEN]; 
  uint16_t dist_port;
} t_timer_data;

#define MAX_WAIT_FOR_PROBE 255
static uint8_t g_wait_for_probe;
static uint8_t g_wait_for_probe_idx[MAX_WAIT_FOR_PROBE];

static int  g_is_master;
static char g_net_name[MAX_NAME_LEN];
static char g_netns_namespace[MAX_PATH_LEN];
static char g_c2c_name[MAX_NAME_LEN];
static char g_vhost[MAX_NAME_LEN];
static char g_root_path[MAX_PATH_LEN];
static char g_ctrl_path[MAX_PATH_LEN];
static char g_c2c_path[MAX_PATH_LEN];
static uint16_t g_udp_port;

int get_is_master(void)
{
  return (g_is_master);
}

void rpct_recv_pid_req(int llid, int tid, char *name, int num)
{
  ctrl_link_pid_req(llid, tid, name, num);
}

void rpct_recv_kil_req(int llid, int tid)
{
  ctrl_link_stop();
}

void reply_probe_udp(uint8_t probe_idx)
{
  char resp[MAX_PATH_LEN];
  snprintf(resp, MAX_PATH_LEN-1,"c2c_receive_probe_udp %s %hhu",
           g_c2c_name, probe_idx);
  rpct_send_sigdiag_msg(ctrl_link_llid(), type_hop_c2c, resp);
}

void reply_enter_traffic_udp(void)
{
  char resp[MAX_PATH_LEN];
  snprintf(resp, MAX_PATH_LEN-1,"c2c_enter_traffic_udp_ack %s",g_c2c_name);
  rpct_send_sigdiag_msg(ctrl_link_llid(), type_hop_c2c, resp);
}

void rpct_recv_poldiag_msg(int llid, int tid, char *line)
{
  KERR("ERROR %s", line);
}

static void timeout_udp_probe(void *data)
{
  t_timer_data *timer_data = (t_timer_data *)data;
  uint8_t wait_for_probe_idx = timer_data->wait_for_probe_idx;
  if (g_wait_for_probe_idx[wait_for_probe_idx])
    {
    KERR("WARNING: udp_probe sent to %s %hu idx %hhu did not echo",
         timer_data->dist_ip, timer_data->dist_port, wait_for_probe_idx);
    }
  free(timer_data);
}

static uint8_t arm_timeout_udp_probe(char *dist_ip, uint16_t dist_port)
{
  t_timer_data *timer_data = (t_timer_data *) malloc(sizeof(t_timer_data));
  memset(timer_data, 0, sizeof(t_timer_data));
  g_wait_for_probe += 1;
  if (g_wait_for_probe == 0)
    g_wait_for_probe = 1;
  timer_data->wait_for_probe_idx = g_wait_for_probe;
  strncpy(timer_data->dist_ip, dist_ip, MAX_NAME_LEN-1);
  timer_data->dist_port = dist_port;
  clownix_timeout_add(300,timeout_udp_probe,(void *)timer_data,NULL,NULL);
  return g_wait_for_probe;
}

void rpct_recv_sigdiag_msg(int llid, int tid, char *line)
{
  char resp[MAX_PATH_LEN];
  uint8_t mac[6];
  char dist_ip[MAX_NAME_LEN]; 
  uint16_t dist_port;
  char label[MAX_NAME_LEN];
  uint8_t probe_idx;

  memset(resp, 0, MAX_PATH_LEN);
  if (llid != ctrl_link_llid())
    KERR("ERROR %d %d", llid, ctrl_link_llid());
  if (tid != type_hop_c2c)
    KERR("ERROR %d %d", tid, type_hop_c2c);

  if (!strncmp(line,
  "c2c_suidroot", strlen("c2c_suidroot")))
    {
    snprintf(resp, MAX_PATH_LEN-1, "c2c_suidroot_ok %s", g_c2c_name);
    rpct_send_sigdiag_msg(llid, tid, resp);
    }

  else if (sscanf(line,
  "c2c_get_free_udp_port %63s proxycrun %hu", g_c2c_name, &g_udp_port) == 2)
    {
    if (udp_init(g_udp_port))
      {
      snprintf(resp, MAX_PATH_LEN-1,
      "c2c_get_free_udp_port_ko %s", g_c2c_name);
      KERR("ERROR %s", line);
      }
    else
      {
      snprintf(resp, MAX_PATH_LEN-1,
      "c2c_get_free_udp_port_ok %s udp_port=%hu", g_c2c_name, g_udp_port);
      }
    rpct_send_sigdiag_msg(llid, tid, resp);
    }
  else if (sscanf(line,
  "c2c_send_probe_udp %63s ip %63s port %hu probe_idx %hhu",
    g_c2c_name, dist_ip, &dist_port, &probe_idx) == 4) 
    {
    if (probe_idx == 0)
      probe_idx = arm_timeout_udp_probe(dist_ip, dist_port);
    if (probe_idx == 0)
      KOUT("ERROR"); 
    memset(label, 0, MAX_NAME_LEN);
    snprintf(label, MAX_NAME_LEN-1, "cloonix_udp_probe %hhu", probe_idx);
    udp_tx_sig_send(strlen(label)+1, (uint8_t *) label);
    }
  else if (sscanf(line,
  "c2c_enter_traffic_udp %63s probe_idx %hhu", g_c2c_name, &probe_idx) == 2)
    {
    if (get_is_master())
      g_wait_for_probe_idx[probe_idx] = 0;
    udp_enter_traffic_mngt();
    reply_enter_traffic_udp();
    }
  else if (sscanf(line,
  "c2c_mac_mangle %63s %hhX:%hhX:%hhX:%hhX:%hhX:%hhX", g_c2c_name,
    &(mac[0]), &(mac[1]), &(mac[2]), &(mac[3]), &(mac[4]), &(mac[5])) == 7)
    {
    KTRC("%s", line);
    rxtx_mac_mangle(mac);
    }
  else
    KERR("ERROR %s", line);
}

char *get_net_name(void)
{
  return (g_net_name);
}

char *get_c2c_name(void)
{
  return (g_c2c_name);
}

int main (int argc, char *argv[])
{
  char *root = g_root_path;
  char *c2c = g_c2c_name;
  int i, pid, fd_rx_from_tap, fd_tx_to_tap;
  if (argc != 6)
    KOUT("ERROR %d", argc);
  umask(0000);
  g_wait_for_probe = 0;
  for (i=0; i<MAX_WAIT_FOR_PROBE; i++)
    g_wait_for_probe_idx[i] = 0;
  memset(g_net_name, 0, MAX_NAME_LEN);
  memset(g_netns_namespace, 0, MAX_PATH_LEN);
  memset(g_root_path, 0, MAX_PATH_LEN);
  memset(g_ctrl_path, 0, MAX_PATH_LEN);
  memset(g_c2c_name, 0, MAX_NAME_LEN);
  memset(g_vhost, 0, MAX_NAME_LEN);
  memset(g_c2c_path, 0, MAX_PATH_LEN);
/* Setuid binary: its arguments are not trusted. */
  if (!name_is_sane(argv[1]))
    KOUT("ERROR refused net name");
  if (!path_is_sane(argv[2]))
    KOUT("ERROR refused root path");
  if (!name_is_sane(argv[3]))
    KOUT("ERROR refused c2c name");
  if (!name_is_sane(argv[4]))
    KOUT("ERROR refused vhost name");
  strncpy(g_net_name,  argv[1], MAX_NAME_LEN-1);
  strncpy(g_root_path, argv[2], MAX_PATH_LEN-1);
  strncpy(g_c2c_name,  argv[3], MAX_NAME_LEN-1);
  strncpy(g_vhost,  argv[4], MAX_NAME_LEN-1);
  if (!strcmp(argv[5], "master"))
    g_is_master = 1;
  else if (!strcmp(argv[5], "slave"))
    g_is_master = 0;
  else
    KOUT("ERROR %s", argv[5]);
  snprintf(g_ctrl_path, MAX_PATH_LEN-1,"%s/%s/c%s", root, C2C_DIR, c2c);
  snprintf(g_c2c_path, MAX_PATH_LEN-1,"%s/%s/%s", root, C2C_DIR, c2c);
  snprintf(g_netns_namespace, MAX_PATH_LEN-1, "%s%s_%s",
           PATH_NAMESPACE, BASE_NAMESPACE, g_net_name);

  set_cloonix_name(g_net_name);
  pid = tun_tap_open(g_netns_namespace, g_vhost,
                             &fd_rx_from_tap, &fd_tx_to_tap);

  init_packet_arp_mangle();
  msg_mngt_init("c2c", TRAF_TAP_BUF_LEN);
  rxtx_init(fd_rx_from_tap, fd_tx_to_tap);
  ctrl_link_init(g_net_name, "c2c", g_c2c_name, type_hop_c2c, g_ctrl_path);
  ctrl_link_add_path(g_c2c_path);
  ctrl_link_add_child(pid);
  if (daemon(0,0))
    KOUT("ERROR daemon %s", strerror(errno));
  /* The tap is open: root is not needed again, and is not kept. */
  cloonix_drop_root();
  cloonix_set_pid(getpid());
  msg_mngt_loop();
  return 0;
}

