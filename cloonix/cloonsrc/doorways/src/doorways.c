/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/time.h>
#include <math.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "doors_rpc.h"
#include "llid_traffic.h"
#include "llid_backdoor.h"
#include "doorways_sock.h"
#include "dispach.h"
#include "doorways.h"
#include "kvmwifi.h"
static int  g_doorways_llid;
static char g_net_name[MAX_NAME_LEN];
static char g_root_work[MAX_PATH_LEN];
static char g_password[MSG_DIGEST_LEN];

char *get_switch_path(void)
{
  static char path[2*MAX_PATH_LEN];
  snprintf(path, 2*MAX_PATH_LEN,  "%s/%s", g_root_work, CLOONIX_SWITCH);
  path[MAX_PATH_LEN-1] = 0;
  return path;
}

char *get_root_work(void)
{
  return g_root_work;
}

static char *get_ctrl_doors(void)
{
  static char path[2*MAX_PATH_LEN];
  snprintf(path, 2*MAX_PATH_LEN, "%s/%s", g_root_work, DOORS_CTRL_SOCK);
  path[MAX_PATH_LEN-1] = 0;
  return path;
}

int get_doorways_llid(void)
{
  return g_doorways_llid;
}

static time_t my_second(void)
{
  static time_t offset = 0;
  struct timeval tv;
  gettimeofday(&tv, NULL);
  if (offset == 0)
    offset = tv.tv_sec;
  return (tv.tv_sec - offset);
}

static void heartbeat (int delta)
{
  static int g_time_count = 0;
  int cur_sec = my_second();
  if (cur_sec >= g_time_count+1)
    {
    g_time_count += 1;
    llid_traf_heartbeat();
    }

}

void doors_recv_command(int llid, int tid, char *name, char *cmd)
{
  char addr[MAX_PATH_LEN];
  memset(addr, 0, MAX_PATH_LEN);
  if (!strcmp(cmd, CLOONIX_UP_VPORT_AND_RUNNING))
    llid_backdoor_cloonix_up_vport_and_running(name);
  else if (!strcmp(cmd, CLOONIX_DOWN_AND_NOT_RUNNING))
    llid_backdoor_cloonix_down_and_not_running(name);
  else if (!strcmp(cmd, REBOOT_REQUEST))
    llid_backdoor_tx_reboot_to_agent(name);
  else if (!strcmp(cmd, REBOOT_ANNOUNCED))
    llid_backdoor_reboot_announced(name);
  else if (!strcmp(cmd, HALT_REQUEST))
    llid_backdoor_tx_halt_to_agent(name);
  else if (!strncmp(cmd, CLIPBOARD_SET, strlen(CLIPBOARD_SET)))
    llid_backdoor_tx_clip_set_to_agent(name, cmd + strlen(CLIPBOARD_SET) + 1);
  else if (!strcmp(cmd, CLIPBOARD_GET))
    llid_backdoor_tx_clip_get_to_agent(name);
  else if (!strncmp(cmd, KVMWIFI_ON, strlen(KVMWIFI_ON)))
    kvmwifi_ask(name, cmd);
  else
    KERR("%s  %s", name, cmd);
}

void rpct_recv_pid_req(int llid, int tid, char *name, int num)
{
  rpct_send_pid_resp(llid, tid, name, num, cloonix_get_pid(), getpid());
}

void rpct_recv_kil_req(int llid, int tid)
{
  exit(0);
}

void doors_recv_add_vm(int llid,int tid,char *name, char *way2agent)
{
  llid_backdoor_begin_unix(name, way2agent);
}

void doors_recv_del_vm(int llid, int tid, char *name)
{
  llid_backdoor_end_unix(name);
}

static void err_ctrl_cb (int llid, int err, int from)
{
  exit(0);
}

static void rx_ctrl_cb (int llid, int len, char *buf)
{
  if (doors_io_basic_decoder(llid, len, buf))
    {
    if (rpct_decoder(llid, len, buf))
      {
      if (doors_json_decoder(llid, len, buf))
        KOUT("%s", buf);
      }
    }
}

static void end_of_init(char *passwd)
{
  static int done = 0;
  if (!done)
    {
    doorways_sock_server(g_net_name, passwd,
                         dispach_door_llid, dispach_door_end,
                         doorways_rx_bufraw);
    llid_backdoor_init();
    done = 1;
    }
}

static void connect_from_ctrl_client(int llid, int llid_new)
{
  msg_mngt_set_callbacks (llid_new, err_ctrl_cb, rx_ctrl_cb);
  g_doorways_llid = llid_new;
  end_of_init(g_password);
}

static void fct_timeout_after_birth_self_destruct(void *data)
{
  if (g_doorways_llid == 0)
    KOUT("DOORS NO CONNECT AFTER BIRTH");
}

static char *get_name(char *root_work)
{
  static char name[MAX_NAME_LEN];
  char big_name[MAX_PATH_LEN];
  char *ptr;
  memset(name, 0, MAX_NAME_LEN);
  memset(big_name, 0, MAX_PATH_LEN);
  strncpy(big_name, root_work, MAX_PATH_LEN-1); 
  ptr = strrchr(big_name, '/');
  if (!ptr)
    snprintf(name, MAX_NAME_LEN-1, "%s", "doorways");
  else
    {
    ptr += 1;
    snprintf(name, MAX_NAME_LEN-1, "%s_%s", "doorways", ptr);
    } 
  return name;
}

int main (int argc, char *argv[])
{
  char *ctrl_path;
  umask(0000);
  doors_json_init();
  doors_io_basic_json_init(string_tx);
  msg_mngt_init(get_name(argv[2]), IO_MAX_BUF_LEN);
  dispach_init();
  g_doorways_llid = 0;
  if (argc != 4)
    KOUT(" ");
  memset(g_net_name, 0, MAX_NAME_LEN);
  memset(g_root_work, 0, MAX_PATH_LEN);
  memset(g_password, 0, MSG_DIGEST_LEN);
  strncpy(g_net_name, argv[1], MAX_NAME_LEN-1);
  set_cloonix_name(g_net_name);
  strncpy(g_root_work, argv[2], MAX_PATH_LEN-1);
  strncpy(g_password, argv[3], MSG_DIGEST_LEN-1);
  doorways_sock_init(0);
  ctrl_path = get_ctrl_doors();
  unlink(ctrl_path);
  msg_mngt_heartbeat_init(heartbeat);
  string_server_unix(ctrl_path, connect_from_ctrl_client, "ctrl");
  if (daemon(0,0))
    KOUT("ERROR daemon %s", strerror(errno));
  cloonix_set_pid(getpid());
  clownix_timeout_add(1000, fct_timeout_after_birth_self_destruct, NULL,
                      NULL, NULL);

  msg_mngt_loop();
  return 0;
}

