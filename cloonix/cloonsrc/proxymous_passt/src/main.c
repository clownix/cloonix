/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#include <stdio.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <signal.h>
#include <unistd.h>
#include <sys/resource.h>
#include <syslog.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/select.h>
#include <fcntl.h>
#include <linux/tcp.h>
#include <dirent.h>

#include "io_clownix.h"
#include "file_read_write.h"
#include "rpc_clownix.h"
#include "doorways_sock.h"
#include "proxy_crun.h"
#include "util_sock.h"
#include "c2c_peer.h"
#include "nat_main.h"
#include "passt_spawn.h"
#include "demo_exec.h"
#include "dir_watch.h"
#include "pthexec.h"

static char g_net_name[MAX_NAME_LEN];
static char g_ctrl_path[MAX_PATH_LEN];
static char g_unix_sig_stream[MAX_PATH_LEN];
static char g_proxyshare_dir[MAX_PATH_LEN];
static int g_llid_ctrl;
static int g_llid_sig;
static int g_watchdog_ok;

void set_config_rapid_beat(void);
void jsweb_relay_rapid_beat(void);

char *get_net_name(void)
{
  return g_net_name;
}

void write_start_status_file(int is_ready)
{
  char path[MAX_PATH_LEN];
  FILE *fp;
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path,MAX_PATH_LEN-1,"%s/proxymous_start_status",g_proxyshare_dir);
  fp = fopen(path, "w");
  if (!fp)
    KOUT("ERROR WRITING %s", path);
  if (is_ready)
    fprintf(fp, "server_is_ready");
  else
    fprintf(fp, "server_not_ready_please_wait");
  fclose(fp);
}

static void fct_timeout_self_destruct(void *data)
{
  if (g_watchdog_ok == 0)
    {
    KERR("WARNING PROXYMOUS: no news from the main server, stopping");
    passt_spawn_stop();
    udp_proxy_end();
    exit(0);
    }
}

static int check_and_set_uid(void)
{
  int result = -1;
  uid_t uid;
  if (seteuid(0))
    KTRC("seteuid %s", strerror(errno));
  if (setegid(0))
    KTRC("setegid %s", strerror(errno));
  uid = geteuid();
  if (uid == 0)
    {
    result = 0;
    if (setuid(0))
      KERR("ERROR setuid");
    if (setgid(0))
      KERR("ERROR setgid");
    }
  return result;
}

static void heartbeat (int delta)
{
  static int count = 0;
  set_config_rapid_beat();
  jsweb_relay_rapid_beat();
  count += 1;
  if (count == 100)
    {
    count = 0;
    sig_process_heartbeat();
    if (passt_spawn_beat() == 0)
      nat_main_passt_died();
    }
}

/* qemu beside proxymous (bundle) or in the rootfs of the toy */
static int has_kvm(void)
{
  char path[MAX_PATH_LEN];
  char *dir = pthexec_self_bin_dir();
  snprintf(path, MAX_PATH_LEN-1, "%s/cloonix-qemu-system", dir);
  if (access(path, R_OK) == 0)
    return 1;
  snprintf(path, MAX_PATH_LEN-1,
           "%s/../rootfs/cloonix/cloonfs/bin/cloonix-qemu-system", dir);
  return (access(path, R_OK) == 0);
}

static void test_dev_kvm(void)
{
  char why[MAX_NAME_LEN];
  if (machine_is_kvm_able())
    {
    if (dev_file_rw_ko("/dev/kvm", why))
      {
      printf("\nWARNING /dev/kvm %s, kvm needs it to be rw\n", why);
      printf("\n\tHint:\nsudo usermod -a -G kvm <user>\n");
      printf("\tOR:\nsudo chmod 0666 /dev/kvm\n");
      printf("\tOR:\nsudo setfacl -m u:<user>:rw /dev/kvm\n\n");
      }
    }
  else
    printf("\nWARNING NO hardware virtualisation kvm needs it\n\n");
}

static void stub_tx(int llid, int len, char *buf)
{   
  KOUT("ERROR %s", buf);
}     

static void err_proxy_sig_cb (int llid, int err, int from)
{ 
  if (msg_exist_channel(llid))
    msg_delete_channel(llid);
} 
  
static void rx_proxy_sig_cb (int llid, int len, char *buf)
{ 
  sig_process_rx(g_proxyshare_dir, buf);
} 

static void connect_from_sig_client(int llid, int llid_new)
{
  msg_mngt_set_callbacks (llid_new, err_proxy_sig_cb, rx_proxy_sig_cb);
  g_llid_sig = llid_new;
  dir_watch_note_server(llid_new);
} 

static void err_ctrl_cb (int llid, int err, int from)
{
  if (msg_exist_channel(llid))
    msg_delete_channel(llid);
}

static void rx_ctrl_cb (int llid, int len, char *buf)
{
  if (rpct_decoder(llid, len, buf))
    KOUT("ERROR %s", buf);
}

static void connect_from_ctrl_client(int llid, int llid_new)
{
  msg_mngt_set_callbacks (llid_new, err_ctrl_cb, rx_ctrl_cb);
  g_llid_ctrl = llid_new;
  if (msg_exist_channel(llid))
    msg_delete_channel(llid);
}

char *get_proxyshare(void)
{
  return g_proxyshare_dir;
}

int get_llid_sig(void)
{
  return g_llid_sig;
}

void rpct_recv_kil_req(int llid, int tid)
{ 
  nat_main_kil_req();
  unlink(g_ctrl_path);
  exit(0);
}
 
void rpct_recv_pid_req(int llid, int tid, char *name, int num)
{ 
  g_watchdog_ok = 1;
  if (llid != g_llid_ctrl)
    KERR("ERROR %s %d %d", name, llid, g_llid_ctrl);
  if (tid != type_hop_proxymous)
    KERR("ERROR %s %d", name, tid);
  rpct_send_pid_resp(llid, tid, name, num, cloonix_get_pid(), getpid());
}     

static void check_and_create_tmp_dir(char *tmp_dir)
{
  DIR *dir;
  int result;
  struct stat st;
  dir = opendir(tmp_dir);
  if (dir)
    {
    if (access(tmp_dir, W_OK))
      KOUT("ERROR %s exists but is not writable", tmp_dir);
    if ((stat(tmp_dir, &st)) || (st.st_uid != getuid()))
      KOUT("ERROR %s exists and is not ours", tmp_dir);
    closedir(dir);
    }
  else if (errno == ENOENT)
    {
    result = mkdir(tmp_dir, 0700);
    if (result)
      KOUT("ERROR CANNOT MKDIR %s", tmp_dir);
    if (access(tmp_dir, W_OK))
      KOUT("ERROR %s", tmp_dir);
    }
  else
    KOUT("ERROR %s opendir %d", tmp_dir, errno);
}

int main(int argc, char **argv)
{

  if (argc != 3)
    {
    printf("\nBAD ARG proxymous_root_path must be given\n\n");
    exit(-1);
    }
  umask(0000);
  /* Descriptor limit to 4096, capped at the hard limit, never lowered. */
  {
  struct rlimit rl;
  if (getrlimit(RLIMIT_NOFILE, &rl) == 0)
    {
    rlim_t was = rl.rlim_cur;
    rlim_t want = 4096;
    if (want > rl.rlim_max)
      want = rl.rlim_max;
    if (want < was)
      want = was;
    rl.rlim_cur = want;
    if (setrlimit(RLIMIT_NOFILE, &rl) == 0)
      KTRC("descriptor limit raised from %lu to %lu (hard limit %lu)",
           (unsigned long) was, (unsigned long) rl.rlim_cur,
           (unsigned long) rl.rlim_max);
    else
      KERR("WARNING could not raise the descriptor limit from %lu "
           "(errno=%d): the net dies if this is reached",
           (unsigned long) was, errno);
    }
  }
  check_and_set_uid();
  if (has_kvm())
    test_dev_kvm();
  g_llid_sig = 0;
  g_llid_ctrl = 0;
  g_watchdog_ok = 0;
  c2c_peer_init();
  udp_proxy_init_all();
  doorways_sock_init(0);
  msg_mngt_init("proxy", TRAF_TAP_BUF_LEN);
  msg_mngt_heartbeat_init(heartbeat);
  doors_io_basic_json_init(stub_tx);
  nat_main_init();
  memset(g_net_name, 0, MAX_NAME_LEN);
  memset(g_ctrl_path, 0, MAX_PATH_LEN);
  memset(g_proxyshare_dir, 0, MAX_PATH_LEN);
  strncpy(g_proxyshare_dir, argv[1], MAX_PATH_LEN-1);
  strncpy(g_net_name, argv[2], MAX_NAME_LEN-1);
  snprintf(g_ctrl_path, MAX_PATH_LEN-1,"%s/%s",
           g_proxyshare_dir, PROXYMOUS);
  check_and_create_tmp_dir(g_proxyshare_dir);
  if (!access(g_ctrl_path, R_OK))
    {
    KERR("ERROR %s exists ERASING", g_ctrl_path);
    unlink(g_ctrl_path);
    }
  string_server_unix(g_ctrl_path, connect_from_ctrl_client, "ctrl");
  memset(g_unix_sig_stream, 0, MAX_PATH_LEN);
  snprintf(g_unix_sig_stream, MAX_PATH_LEN-1,
           "%s/proxymous_sig_stream.sock", g_proxyshare_dir);
  proxy_sig_server(g_unix_sig_stream, connect_from_sig_client);

  write_start_status_file(0);

  if (daemon(0,0))
    KOUT("ERROR daemon %s", strerror(errno));
  if (demo_exec_init(g_proxyshare_dir))
    KERR("WARNING PROXYMOUS no host demo runner, demos stay in the crun");
  dir_watch_init();
  clownix_timeout_add(3000, fct_timeout_self_destruct, NULL, NULL, NULL);
  msg_mngt_loop();
  /* The loop only returns if the event machinery gave up, which is a fault
     and not a step. */
  KERR("ERROR PROXYMOUS msg_mngt_loop returned, exiting");
  return 0; 
}
