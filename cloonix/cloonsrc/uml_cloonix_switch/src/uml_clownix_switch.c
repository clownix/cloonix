/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <pwd.h>
#include <dirent.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>

#include "io_clownix.h"
#include "pcap_relay.h"
#include "rpc_clownix.h"
#include "doors_rpc.h"
#include "cfg_store.h"
#include "heartbeat.h"
#include "commun_daemon.h"
#include "machine_create.h"
#include "event_subscriber.h"
#include "system_callers.h"
#include "pid_clone.h"
#include "automates.h"
#include "lan_to_name.h"
#include "llid_trace.h"
#include "eventfull.h"
#include "slowperiodic.h"
#include "targz.h"
#include "file_read_write.h"
#include "utils_cmd_line_maker.h"
#include "qmp.h"
#include "doorways_mngt.h"
#include "gui_server_mngt.h"
#include "doorways_sock.h"
#include "hop_event.h"
#include "cloonix_conf_info.h"
#include "qmp_dialog.h"
#include "qga_dialog.h"
#include "ovs.h"
#include "ovs_snf.h"
#include "ovs_nat_main.h"
#include "ovs_tap.h"
#include "ovs_phy.h"
#include "ovs_a2b.h"
#include "ovs_c2c.h"
#include "suid_power.h"
#include "cnt.h"
#include "mactopo.h"
#include "crun.h"
#include "util_sock.h"
#include "proxymous.h"
#include "proxycrun.h"

static t_topo_clc g_clc;
static t_cloonix_conf_info *g_cloonix_conf_info;
static int g_i_am_inside_cloonix;
static char g_i_am_inside_cloonix_name[MAX_NAME_LEN];
static char g_proxymous_dir[MAX_PATH_LEN];

static int g_uml_cloonix_started;
static int g_cloonix_lock_fd;
static int g_machine_is_kvm_able;
static char g_config_path[MAX_PATH_LEN];
static int g_conf_rank;
static int g_running_in_crun;
static char g_main_log[MAX_PATH_LEN];

char *get_proxymous_dir(void)
{
  return g_proxymous_dir;
}

t_cloonix_conf_info *get_cloonix_conf_info(void)
{
  return g_cloonix_conf_info;
}

int get_running_in_crun(void)
{
  return g_running_in_crun;
}

int get_uml_cloonix_started(void)
{
  return g_uml_cloonix_started;
}

int get_conf_rank(void)
{
  return g_conf_rank;
}

void cloonix_lock_fd_close(void)
{
  if (g_cloonix_lock_fd)
    close(g_cloonix_lock_fd);
}

static void usage(char *name)
{
  KERR("ERROR usage %s", name);
  exit(-1);
}

static void check_for_another_instance(char *clownlock, int keep_fd)
{
  int fd;
  struct flock lock;
  fd = open (clownlock, O_WRONLY|O_CREAT, S_IRWXU);
  if (fd < 0)
    {
    printf("%s %s %d\n", __FUNCTION__, clownlock, errno);
    KOUT("%s %d",clownlock, errno);
    }
  memset (&lock, 0, sizeof(lock));
  lock.l_type = F_WRLCK;
  lock.l_whence = SEEK_SET;
  lock.l_pid = getpid();
  if (fcntl (fd, F_SETLK, &lock) == -1)
    {
    if (close(fd))
      {
      printf("%s %d\n", __FUNCTION__,  errno);
      KOUT("%d", errno);
      }
    fd = open (clownlock, O_RDONLY);
    if (fd == -1)
      {
      printf("%s %s %d\n", __FUNCTION__, clownlock, errno);
      KOUT("%s %d", clownlock, errno);
      }
    memset (&lock, 0, sizeof(lock));
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    if (fcntl (fd, F_GETLK, &lock))
      {
      lock.l_pid = 0;
      KERR("ERROR fcntl F_GETLK %s: %s", clownlock, strerror(errno));
      }
    printf ("\ncloonix-main-server (pid=%d) already running, stop it:\n",
             lock.l_pid);
    printf("\n\ncloonix stop %s\n\n\n", cfg_get_cloonix_name());
    KOUT("Another instance with same work directory found");
    }
  if (!keep_fd)
   {
    lock.l_type = F_UNLCK;
    fcntl (fd, F_SETLK, &lock);
    if (close (fd))
      {
      printf("ERR close %d\n", errno);
      KOUT("%d", errno);
      }
    }
  else
    {
    cfg_set_lock_fd(fd);
    g_cloonix_lock_fd = fd;
    }
}

static void mk_and_tst_work_path(void)
{
  struct stat st;
  char path1[MAX_PATH_LEN];
  char path2[MAX_PATH_LEN];
  char *ptr;
  char *argv[3];
  char *net = cfg_get_cloonix_name();
  memset(argv, 0, 3*sizeof(char *)); 
  argv[0] = pthexec_cloonfs_suid_power();
  argv[1] = NULL;
  if (lio_system(argv))
    KERR("ERROR %s", pthexec_cloonfs_suid_power());
  memset(path1, 0, MAX_PATH_LEN);
  memset(path2, 0, MAX_PATH_LEN);
  strncpy(path1, cfg_get_root_work(), MAX_PATH_LEN-1); 
  ptr = strrchr(path1, '/');
  if (!ptr)
    {
    printf("%d Bad work dir in config: %s\n", __LINE__, cfg_get_root_work());
    KOUT("%s", cfg_get_root_work());
    }
  *ptr = 0;
  strcpy(path2, path1);
  if (!strlen(path1))
    {
    printf("%d Bad work dir in config: %s\n", __LINE__, cfg_get_root_work());
    KOUT("%s", cfg_get_root_work());
    }
  ptr = strrchr(path1, '/');
  if (!ptr)
    {
    printf("%d Bad work dir in config: %s\n", __LINE__,cfg_get_root_work());
    KOUT("%s", cfg_get_root_work());
    }
  *ptr = 0;
  if (!file_exists(path2, R_OK))
    {
    if (!strlen(path1))
      {
      printf("%d Bad work dir in config: %s\n",__LINE__,cfg_get_root_work());
      KOUT("%s", cfg_get_root_work());
      }
    if (!file_exists(path1, W_OK))
      {
      printf("%d Bad work dir in config: %s\n",__LINE__,cfg_get_root_work());
      KOUT("%s", cfg_get_root_work());
      }
    my_mkdir(path2, 0);
    }
  if (!file_exists(path2, W_OK))
    {
    printf("%d Bad work dir in config: %s\n", __LINE__, cfg_get_root_work());
    KOUT("%s", cfg_get_root_work());
    }
  my_mkdir(cfg_get_root_work(), 0);
  my_mkdir(cfg_get_bulk(), 1);
  /* Known name in /tmp: 0700 and ours, or another user owns the sockets. */
  snprintf(g_proxymous_dir, MAX_PATH_LEN-1, "%s_%s", PROXYSHARE_IN, net);
  my_mkdir(g_proxymous_dir, 0);
  if ((stat(g_proxymous_dir, &st)) || (st.st_uid != getuid()))
    {
    printf("\nERROR %s exists and is not yours\n\n", g_proxymous_dir);
    KOUT("ERROR %s exists and is not yours", g_proxymous_dir);
    }
  mk_cnt_dir();
  mk_endp_dir();
  mk_dtach_screen_dir();
  mk_ovs_db_dir();
  snprintf(path1, sizeof(path1), "%s/cloonix_lock",  cfg_get_root_work());
  check_for_another_instance(path1, 0);
  snprintf(g_main_log, MAX_PATH_LEN-1, "%s/%s",
           utils_get_log_dir(), DEBUG_LOG_MAIN);
} 

void uml_clownix_switch_error_cb(int llid, int err, int from)
{
  llid_trace_free(llid, 0, __FUNCTION__);
  doorways_err_cb (llid);
}

void uml_clownix_switch_rx_cb(int llid, int len, char *buf)
{
  int result;
  result = doors_io_basic_decoder(llid, len, buf);
  if (result == -1)
    result = doors_json_decoder(llid, len, buf);
  if (result == -1)
    result = rpct_decoder(llid, len, buf);
  if (result == -1)
    {
    /* The ko must leave before the channel: a close here would drop it
       (msg_delete_channel purges the tx chain), so the client closes. */
    KERR("ERROR unknown json message received: %.*s",
         (int) strcspn(buf, "\n"), buf);
    send_status_ko(llid, 0, "unknown message for this server");
    }
  if (result == -2)
    {
    KERR("ERROR bad digest in a message received: %.*s",
         (int) strcspn(buf, "\n"), buf);
    llid_trace_free(llid, 0, __FUNCTION__);
    }
}

static void connect_from_client_unix(int llid, int llid_new)
{
  llid_trace_alloc(llid_new,"CLIENT_SRV_UX",0,0, type_llid_trace_client_unix);
  msg_mngt_set_callbacks (llid_new, uml_clownix_switch_error_cb, 
                                    uml_clownix_switch_rx_cb);
}

static void timer_proxymous_init(void *data)
{
  proxymous_init(g_cloonix_conf_info->passwd);
}

static int passwd_is_shipped_default(char *passwd);

static void timer_everything_is_ready(void *data)
{
  printf("\n    UML_CLOONIX_SWITCH NOW RUNNING\n\n");
  if (daemon(0,0))
    KOUT("ERROR daemon %s", strerror(errno));
  gui_server_init(cfg_get_cloonix_name(), cfg_get_root_work(),
                  cfg_get_web_port(), g_cloonix_conf_info->passwd,
                  g_cloonix_conf_info->doors);
  g_uml_cloonix_started = 1;
  KERR("INFO started, version %s, doors port %d, https port %d%s",
       cfg_get_version(), cfg_get_server_port(), cfg_get_web_port(),
       passwd_is_shipped_default(g_cloonix_conf_info->passwd) ?
       ", with the shipped cloonix_passwd" : "");
  proxycrun_transmit_write_start_status_file_ready();
}

static void timer_openvswitch_ok(void *data)
{
  if (!proxycrun_connect_proxy_ok())
    {
    clownix_timeout_add(10, timer_openvswitch_ok, NULL, NULL, NULL);
    }
  else if (!get_daemon_done())
    {
    clownix_timeout_add(10, timer_openvswitch_ok, NULL, NULL, NULL);
    }
  else
    {
    clownix_timeout_add(30, timer_everything_is_ready, NULL, NULL, NULL);
    }
}

static void launching(void)
{
  char clownlock[MAX_PATH_LEN];
  char *dtach = utils_get_dtach_bin_path();
  char *net = cfg_get_cloonix_name();
  if (!file_exists(dtach, X_OK))
    {
    printf("\"%s\" not found or not executable\n", dtach);
    KOUT("\"%s\" not found or not executable\n", dtach);
    }
  g_running_in_crun = pthexec_running_in_crun(NULL);
  printf("\n\n");
  printf("     Version:      %s\n",cfg_get_version());
  printf("     Name:         %s (rank:%d)\n", net, get_conf_rank());
  printf("     Config:       %s\n", g_config_path);
  printf("     Binaries:     %s/cloonfs\n",cfg_get_bin_dir());
  printf("     Work Zone:    %s\n",cfg_get_root_work());
  printf("     Bulk Path:    %s\n",cfg_get_bulk());
  printf("     Share Path:   %s\n", CLOONIX_SHARE_DIR);
  printf("     Sock Dir:     %s\n", g_proxymous_dir);
  printf("     Doors Port:   %d\n",cfg_get_server_port());
  if (cfg_get_web_port())
    printf("     Https Port:   %d\n", cfg_get_web_port());
  else
    printf("     Https Port:   none, this net has no canvas\n");
  printf("\n\n\n");
  if ((strlen(cfg_get_cloonix_name()) == 0) || 
      (strlen(cfg_get_bin_dir()) == 0)      || 
      (strlen(cfg_get_root_work()) == 0)    || 
      (strlen(cfg_get_bulk()) == 0) )
    {
    printf("BADCONF\n");
    KOUT("BADCONF");
    }
  ovs_init();
  crun_init();
  ovs_snf_init();
  pcap_relay_init();
  ovs_nat_main_init();
  ovs_tap_init();
  ovs_phy_init();
  ovs_a2b_init();
  ovs_c2c_init();
  doorways_first_start();
  snprintf(clownlock, sizeof(clownlock), "%s/cloonix_lock", cfg_get_root_work());
  check_for_another_instance(clownlock, 1);
  clownix_timeout_add(100, timer_proxymous_init, NULL, NULL, NULL);
  clownix_timeout_add(10, timer_openvswitch_ok, NULL, NULL, NULL);
}

static t_topo_clc *get_parsed_config(char *name)
{
  t_topo_clc *conf = NULL;
  t_cloonix_conf_info *cloonix_conf;
  cloonix_conf = cloonix_cnf_info_get(name, &g_conf_rank);
  if (!cloonix_conf)
    {
    printf("name:%s not in config file\n", name);
    KOUT("name:%s not in config file", name);
    }
  else
    {
    conf = &g_clc;
    memset(conf, 0, sizeof(t_topo_clc));
    snprintf(conf->version,MAX_NAME_LEN-1,"%s",cloonix_conf_info_get_version());
    snprintf(conf->username, MAX_NAME_LEN-1, "%s", getenv("USER"));
    if (strlen(conf->username) == 0)
      KERR("ERROR USER");
    snprintf(conf->network, MAX_NAME_LEN, "%s", cloonix_conf->name); 
    conf->network[MAX_NAME_LEN-1] = 0;
    conf->server_port = cloonix_conf->port;
    conf->web_port = cloonix_conf->web_port; 
    strcpy(conf->bin_dir, pthexec_usr_libexec_cloonix());
    g_cloonix_conf_info = cloonix_conf;
    }
  return conf;
}

static int test_machine_is_kvm_able(void)
{
  int found = 0;
  FILE *fhd;
  char result[500];
  fhd = fopen("/proc/cpuinfo", "r");
  if (fhd)
    {
    while(!found)
      {
      if (!fgets(result, 500, fhd))
        break;
      if (!strncmp(result, "flags", strlen("flags")))
        found = 1;
      }
    fclose(fhd);
    }
  if (found)
    {
    found = 0;
    if ((strstr(result, "vmx")) || (strstr(result, "svm")))
      found = 1;
    }
  return found;
}

static void connect_dummy(int llid, int llid_new)
{
  printf("%s %d\n", __FUNCTION__, __LINE__);
  KOUT(" ");
}

/* A net killed a second ago still holds the port while it finishes. */
static void wait_port_free(int port)
{
  struct sockaddr_in addr;
  int fd, i, busy = 1, optval = 1;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(port);
  for (i = 0; busy && (i < 40); i++)
    {
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
      KOUT("ERROR socket: %s", strerror(errno));
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    busy = bind(fd, (struct sockaddr *) &addr, sizeof(addr));
    close(fd);
    if (busy)
      {
      if (i == 0)
        printf("Port: %d is busy, waiting for it\n", port);
      usleep(250000);
      }
    }
  if (i > 1)
    KERR("WARNING port %d was %s after %d ms", port,
         busy ? "still in use" : "free", i * 250);
}

static int tst_port_is_not_used(int port)
{
  int result = -1;
  int llid = string_server_inet(port, connect_dummy, "tst");
  if (msg_exist_channel(llid))
    {
    result = 0;
    msg_delete_channel(llid);
    }
  return result;
}

int inside_cloonix(char **name)
{
  *name = g_i_am_inside_cloonix_name;
  return g_i_am_inside_cloonix;
}

static int passwd_is_shipped_default(char *passwd)
{
  static const char * const shipped[] = {
    "nemoclown", "mitoclown", "fidoclown",
    "cucuclown", "loloclown", "enduclown", NULL};
  int i;
  for (i=0; shipped[i]; i++)
    if (!strcmp(passwd, shipped[i]))
      return 1;
  return 0;
}

static int check_for_work_dir_inexistance(char *net_name)
{
  FILE *fp;
  char line[MAX_PATH_LEN];
  char cmd[MAX_PATH_LEN];
  struct stat stat_path;
  int child_pid, result = 0;

  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1, "%s %s erase_workdir",
           pthexec_cloonfs_suid_power(), net_name);
  fp = pthexec_popen(cmd, &child_pid);
  if (fp == NULL)
    KERR("ERROR %s %d", cmd, errno);
  else
    {
    memset(line, 0, MAX_PATH_LEN);
    while (fgets(line, MAX_PATH_LEN-1, fp))
      printf("\n%s", line);
    pthexec_pclose(fp, child_pid);
    }
  if (stat(cfg_get_root_work(), &stat_path) == 0)
    result = -1;
  return result;
}

static int get_info_exists(char *binary, char *pattern)
{
  FILE *fp;
  char ps_cmd[MAX_PATH_LEN];
  char line[MAX_PATH_LEN];
  int child_pid, result = 0;
  snprintf(ps_cmd, MAX_PATH_LEN-1, "%s axo pid,args", pthexec_ps_bin());
  fp = pthexec_popen(ps_cmd, &child_pid);
  if (fp == NULL)
    KERR("ERROR %s %d", ps_cmd, errno);
  else
    {
    memset(line, 0, MAX_PATH_LEN);
    while (fgets(line, MAX_PATH_LEN-1, fp))
      {
      if ((strstr(line, binary)) &&
          (strstr(line, pattern)))
        {
        result = 1;
        break;
        }
      }
    pthexec_pclose(fp, child_pid);
    }
  return result;
}

static int tst_binaries_with_cloonix_name(char *cloonix_name)
{
  char pattern1[MAX_PATH_LEN];
  char *binary;
  int result = 0;
  memset(pattern1, 0, MAX_PATH_LEN);
  snprintf(pattern1, MAX_PATH_LEN-1, "/var/lib/cloonix/%s", cloonix_name);
  binary = pthexec_cloonfs_doorways(); 
  if (get_info_exists(binary, pattern1))
    {
    KERR("ERROR ps exists %s %s", binary, pattern1); 
    result = 1;
    }
  binary = pthexec_cloonfs_ovs_drv(); 
  if (get_info_exists(binary, pattern1))
    {
    KERR("ERROR ps exists %s %s", binary, pattern1); 
    result = 1;
    }
  binary = pthexec_cloonfs_ovs_ovsdb();
  if (get_info_exists(binary, pattern1))
    {
    KERR("ERROR ps exists %s %s", binary, pattern1); 
    result = 1;
    }
  binary = pthexec_cloonfs_ovs_vswitchd();
  if (get_info_exists(binary, pattern1))
    {
    KERR("ERROR ps exists %s %s", binary, pattern1); 
    result = 1;
    }
  binary = pthexec_cloonfs_suid_power();
  if (get_info_exists(binary, pattern1))
    {
    KERR("ERROR ps exists %s %s", binary, pattern1); 
    result = 1;
    }
  return result;
}

static void helper_check_main_binaries(char *bin)
{
  if (!file_exists(bin, X_OK))
    {
    printf("\n\nFile: \"%s\" not found or not executable\n\n", bin);
    KOUT("File: \"%s\" not found or not executable", bin);
    }
}

static void check_main_binaries(void)
{
  char *bin;
  bin = pthexec_crun_bin();
  helper_check_main_binaries(bin);
  bin = pthexec_ovsdb_tool_bin();
  helper_check_main_binaries(bin);
  bin = pthexec_cloonfs_ovs_vswitchd();
  helper_check_main_binaries(bin);
  bin = pthexec_cloonfs_ovs_drv();
  helper_check_main_binaries(bin);
  bin = pthexec_cloonfs_ovs_ovsdb();
  helper_check_main_binaries(bin);
  bin = pthexec_cloonfs_suid_power();
  helper_check_main_binaries(bin);
  bin = pthexec_cloonfs_doorways();
  helper_check_main_binaries(bin);
  bin = pthexec_cloonfs_gui_server();
  helper_check_main_binaries(bin);
}

int main (int argc, char *argv[])
{
  uint64_t date_us;
  t_topo_clc *conf;
  struct timespec ts;
  int llid;
  /* stdout may be a file: a line at a time, none carried across a fork */
  setvbuf(stdout, NULL, _IOLBF, 0);
  memset(g_main_log, 0, MAX_PATH_LEN);
  g_uml_cloonix_started = 0;
  g_cloonix_lock_fd = 0;
  umask(0000);
  if (clock_gettime(CLOCK_MONOTONIC, &ts))
    KOUT(" ");
  pthexec_init();
  check_main_binaries();
  g_i_am_inside_cloonix = i_am_inside_cloonix(g_i_am_inside_cloonix_name);
  memset(g_proxymous_dir, 0, MAX_PATH_LEN);

  job_for_select_init();
  utils_init();
  recv_init();
  g_machine_is_kvm_able = test_machine_is_kvm_able();
  if (argc != 3)
    usage(argv[0]);
  memset(g_config_path, 0, MAX_PATH_LEN);
  strncpy(g_config_path, argv[1], MAX_PATH_LEN-1);
  if (cloonix_conf_info_init(g_config_path))
    KOUT("%s", argv[1]);
  conf = get_parsed_config(argv[2]);
  if (!conf)
    KOUT(" ");
  pthexec_set_rank(get_conf_rank());
  cfg_init();
  cfg_set_host_conf(conf);
  /* Before the first KERR: the net of every line comes from here. */
  set_cloonix_name(cfg_get_cloonix_name());
  lan_init();
  llid_trace_init();
  event_subscriber_init();
  doors_io_basic_json_init(string_tx);
  event_trace_json_init();
  automates_init();

  msg_mngt_init(cfg_get_cloonix_name(), IO_MAX_BUF_LEN);
  wait_port_free(conf->server_port);
  if (tst_port_is_not_used(conf->server_port))
    {
    printf("Port: %d is in use!!\n", conf->server_port);
    KOUT("Port: %d is in use!!", conf->server_port);
    }
  else if (tst_binaries_with_cloonix_name(cfg_get_cloonix_name()))
    {
    printf("A cloonix binary is running with %s!!\n", cfg_get_cloonix_name());
    KOUT("a cloonix binary is already running with this name");
    }
  doorways_sock_init(0);
  if (passwd_is_shipped_default(g_cloonix_conf_info->passwd))
    {
    printf("\nWARNING: %s uses the cloonix_passwd shipped in cloonix.cfg, "
           "known to anyone who has the tarball; the doors port and the web "
           "port answer to it. Edit cloonix.cfg, the cloonix_config.py of "
           "the client.\n\n",
           cfg_get_cloonix_name());
    }
  doorways_init(cfg_get_cloonix_name(), cfg_get_root_work(),
                g_cloonix_conf_info->passwd);
  eventfull_init();
  slowperiodic_init();
  targz_init();
  if (check_for_work_dir_inexistance(cfg_get_cloonix_name()))
    {
    printf("\nERROR erasing workdir %s\n\n", cfg_get_root_work());
    KOUT("ERROR erasing workdir %s", cfg_get_root_work());
    }
  mk_and_tst_work_path();
  pid_clone_init();
  my_mkdir(cfg_get_work(), 1);
  init_heartbeat();
  mactopo_init();
  qmp_dialog_init();
  qmp_init();
  qga_dialog_init();
  suid_power_init();
  cnt_init();
  date_us = cloonix_get_usec();
  srand((int) (date_us & 0xFFFF));
  llid = string_server_unix(utils_get_cloonix_switch_path(),
                            connect_from_client_unix, "main_unix");
  if (llid <= 0)
    {
    printf("\nSomething wrong when making or listening to %s\n\n",
           utils_get_cloonix_switch_path());
    printf("%s %d\n", __FUNCTION__, __LINE__);
    KOUT(" ");
    }
  llid_trace_alloc(llid, "CLIENT_LISTEN_UX",0,0,
                   type_llid_trace_listen_client_unix);
  launching();

  msg_mngt_loop();
  return 0;
}
