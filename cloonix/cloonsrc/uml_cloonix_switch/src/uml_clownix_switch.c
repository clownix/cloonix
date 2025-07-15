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



#include "io_clownix.h"
#include "rpc_clownix.h"
#include "doors_rpc.h"
#include "cfg_store.h"
#include "heartbeat.h"
#include "stats_counters.h"
#include "stats_counters_sysinfo.h"
#include "commun_daemon.h"
#include "machine_create.h"
#include "event_subscriber.h"
#include "system_callers.h"
#include "pid_clone.h"
#include "automates.h"
#include "lan_to_name.h"
#include "llid_trace.h"
#include "layout_rpc.h"
#include "eventfull.h"
#include "slowperiodic.h"
#include "file_read_write.h"
#include "utils_cmd_line_maker.h"
#include "layout_topo.h"
#include "qmp.h"
#include "doorways_mngt.h"
#include "doorways_sock.h"
#include "xwy.h"
#include "novnc.h"
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
static int g_novnc_port;
static int g_running_in_crun;
static char g_main_log[MAX_PATH_LEN];

/*****************************************************************************/
void log_main_write_req(char *line)
{   
  FILE *fp_log;
  if (!strlen(g_main_log))
    KERR("ERROR %s", line);
  else
    {
    fp_log = fopen(g_main_log, "a+");
    if (fp_log)
      {
      fprintf(fp_log, "%s\n", line);
      fflush(fp_log);
      fclose(fp_log);
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *get_proxymous_dir(void)
{
  return g_proxymous_dir;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
t_cloonix_conf_info *get_cloonix_conf_info(void)
{
  return g_cloonix_conf_info;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int get_running_in_crun(void)
{
  return g_running_in_crun;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int get_uml_cloonix_started(void)
{
  return g_uml_cloonix_started;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int get_conf_rank(void)
{
  return g_conf_rank;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void cloonix_lock_fd_close(void)
{
  if (g_cloonix_lock_fd)
    close(g_cloonix_lock_fd);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int machine_is_kvm_able(void)
{
  return g_machine_is_kvm_able;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void usage(char *name)
{
  KERR("ERROR usage %s", name);
  exit(-1);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void check_for_another_instance(char *clownlock, int keep_fd)
{
  int fd;
  struct flock lock;
  fd = open (clownlock, O_WRONLY|O_CREAT, S_IRWXU);
  if (fd <= 0)
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
    fcntl (fd, F_GETLK, &lock);
    printf ("\ncloonix-main-server (pid=%d) already running, please kill it\n",
             lock.l_pid);
    printf(" \n\n\tcloonix_cli %s kil \n\n\n", cfg_get_cloonix_name());
    KOUT("Another instance with same work directory found\n");
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
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void mk_and_tst_work_path(void)
{
  char path1[MAX_PATH_LEN];
  char path2[MAX_PATH_LEN];
  char *ptr;
  char *argv[3];
  char *net = cfg_get_cloonix_name();
  memset(argv, 0, 3*sizeof(char *)); 
  argv[0] = utils_get_suid_power_bin_path();
  argv[1] = NULL;
  if (lio_system(argv))
    KERR("ERROR %s", utils_get_suid_power_bin_path());
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
  my_mkdir(cfg_get_root_work(), 1);
  my_mkdir(cfg_get_bulk(), 1);
  snprintf(g_proxymous_dir, MAX_PATH_LEN-1, "%s_%s", PROXYSHARE_IN, net);
  my_mkdir(g_proxymous_dir, 1);
  mk_cnt_dir();
  mk_endp_dir();
  mk_dtach_screen_dir();
  mk_ovs_db_dir();
  sprintf(path1, "%s/cloonix_lock",  cfg_get_root_work());
  check_for_another_instance(path1, 0);
  snprintf(g_main_log, MAX_PATH_LEN-1, "%s/%s",
           utils_get_log_dir(), DEBUG_LOG_MAIN);
} 
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void uml_clownix_switch_error_cb(int llid, int err, int from)
{
  llid_trace_free(llid, 0, __FUNCTION__);
  doorways_err_cb (llid);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void uml_clownix_switch_rx_cb(int llid, int len, char *buf)
{
  int result;
    result = doors_io_basic_decoder(llid, len, buf);
  if (result == -1)
    result = doors_io_layout_decoder(llid, len, buf);
  if (result == -1)
    result = doors_xml_decoder(llid, len, buf);
  if (result == -1)
    result = rpct_decoder(llid, len, buf);
  if (result == -1)
    {
    KERR("Unknown xml message received \n%s\n", buf);
    event_print("Unknown xml message received \n%s\n", buf);
    llid_trace_free(llid, 0, __FUNCTION__);
    }
  if (result == -2)
    {
    KERR("Bad digest in message received \n%s\n", buf);
    event_print("Bad digest in message received \n%s\n", buf);
    llid_trace_free(llid, 0, __FUNCTION__);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void connect_from_client_unix(int llid, int llid_new)
{
  llid_trace_alloc(llid_new,"CLIENT_SRV_UX",0,0, type_llid_trace_client_unix);
  msg_mngt_set_callbacks (llid_new, uml_clownix_switch_error_cb, 
                                    uml_clownix_switch_rx_cb);
  event_print("Listen:%d, New client connection: %d", llid, llid_new);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_proxymous_init(void *data)
{
  proxymous_init(g_novnc_port, g_cloonix_conf_info->passwd);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_everything_is_ready(void *data)
{
  printf("\n    UML_CLOONIX_SWITCH NOW RUNNING\n\n");
  daemon(0,0);
  g_uml_cloonix_started = 1;
  proxycrun_transmit_write_start_status_file_ready();
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
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
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
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
  set_cloonix_name(cfg_get_cloonix_name());
  printf("\n\n");
  printf("     Version:      %s\n",cfg_get_version());
  printf("     Name:         %s (rank:%d)\n", net, get_conf_rank());
  printf("     Config:       %s\n", g_config_path);
  printf("     Binaries:     %s/cloonfs\n",cfg_get_bin_dir());
  printf("     Work Zone:    %s\n",cfg_get_root_work());
  printf("     Bulk Path:    %s\n",cfg_get_bulk());
  printf("     Doors Port:   %d\n",cfg_get_server_port());
  printf("     Web Port:     %d (Not Activated)\n", g_novnc_port);
  printf("\n\n\n");
  if ((strlen(cfg_get_cloonix_name()) == 0) || 
      (strlen(cfg_get_bin_dir()) == 0)      || 
      (strlen(cfg_get_root_work()) == 0)    || 
      (strlen(cfg_get_bulk()) == 0) || (g_novnc_port == 0))
    {
    printf("BADCONF\n");
    KOUT("BADCONF");
    }
  ovs_init();
  crun_init();
  ovs_snf_init();
  ovs_nat_main_init();
  ovs_tap_init();
  ovs_phy_init();
  ovs_a2b_init();
  ovs_c2c_init();
  doorways_first_start();
  sprintf(clownlock, "%s/cloonix_lock", cfg_get_root_work());
  check_for_another_instance(clownlock, 1);
  init_xwy(cfg_get_cloonix_name(), get_conf_rank());
  init_novnc(cfg_get_cloonix_name(), get_conf_rank());
  clownix_timeout_add(100, timer_proxymous_init, NULL, NULL, NULL);
  clownix_timeout_add(10, timer_openvswitch_ok, NULL, NULL, NULL);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static t_topo_clc *get_parsed_config(char *name)
{
  t_topo_clc *conf = NULL;
  t_cloonix_conf_info *cloonix_conf;
  cloonix_conf = cloonix_cnf_info_get(name, &g_conf_rank);
  if (!cloonix_conf)
    {
    printf("name:%s not in config file\n", name);
    KOUT("name:%s not in config file\n", name);
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
    strcpy(conf->bin_dir, pthexec_usr_libexec_cloonix());
    g_novnc_port = cloonix_conf->novnc_port;
    g_cloonix_conf_info = cloonix_conf;
    }
  return conf;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
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
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void connect_dummy(int llid, int llid_new)
{
  printf("%s %d\n", __FUNCTION__, __LINE__);
  KOUT(" ");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
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
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int inside_cloonix(char **name)
{
  *name = g_i_am_inside_cloonix_name;
  return g_i_am_inside_cloonix;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void check_for_work_dir_inexistance(void)
{
  struct stat stat_path;
  char err[2*MAX_PATH_LEN];
  char *root_wk = cfg_get_root_work();
  if (stat(root_wk, &stat_path) == 0)
    {
    if (unlink_sub_dir_files(root_wk, err))
      {
      printf( "FATAL ERROR!!!!\nPath: \"%s\" already exists\n"
              "please remove it and restart \n\n", root_wk);
      KOUT("ERROR removing %s %s", root_wk, err);
      }
    else
      printf( "Path: \"%s\" already exists, removing it\n\n", root_wk);
    }
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static int get_info_exists(char *binary, char *pattern)
{
  FILE *fp;
  char ps_cmd[MAX_PATH_LEN];
  char line[MAX_PATH_LEN];
  int result = 0;
  snprintf(ps_cmd, MAX_PATH_LEN-1, "%s axo pid,args", pthexec_ps_bin());
  fp = popen(ps_cmd, "r");
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
    pclose(fp);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
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
  binary = pthexec_cloonfs_ovs_ovsbd();
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
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int main (int argc, char *argv[])
{
  uint64_t date_us;
  t_topo_clc *conf;
  struct timespec ts;
  int llid;
  memset(g_main_log, 0, MAX_PATH_LEN);
  g_uml_cloonix_started = 0;;
  g_cloonix_lock_fd = 0;
  umask(0000);
  if (clock_gettime(CLOCK_MONOTONIC, &ts))
    KOUT(" ");
  pthexec_init();
  g_i_am_inside_cloonix = i_am_inside_cloonix(g_i_am_inside_cloonix_name);
  memset(g_proxymous_dir, 0, MAX_PATH_LEN);

  g_novnc_port = 0;
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
  cfg_init();
  cfg_set_host_conf(conf);
  lan_init();
  llid_trace_init();
  event_subscriber_init();
  doors_io_basic_xml_init(string_tx);
  doors_io_layout_xml_init(string_tx);
  automates_init();

  msg_mngt_init(cfg_get_cloonix_name(), IO_MAX_BUF_LEN);
  if (tst_port_is_not_used(conf->server_port))
    {
    printf("Port: %d is in use!!\n", conf->server_port);
    KOUT("Port: %d is in use!!", conf->server_port);
    }
  else if (tst_binaries_with_cloonix_name(cfg_get_cloonix_name()))
    {
    printf("A cloonix binary is running with %s!!\n", cfg_get_cloonix_name());
    KOUT("A cloonix binary is running with %s!!!", cfg_get_cloonix_name());
    }
  doorways_sock_init(0);
  doorways_init(cfg_get_cloonix_name(), cfg_get_root_work(),
                cfg_get_server_port(), g_cloonix_conf_info->passwd);
  eventfull_init();
  slowperiodic_init();
  if (!file_exists(get_doorways_bin(), X_OK))
    {
    printf("\n\nFile: \"%s\" not found or not executable\n\n", 
           get_doorways_bin());
    KOUT("\n\nFile: \"%s\" not found or not executable\n\n", 
         get_doorways_bin());
    }
  check_for_work_dir_inexistance();
  mk_and_tst_work_path();
  pid_clone_init();
  my_mkdir(cfg_get_work(), 1);
  init_heartbeat();
  stats_counters_init();
  stats_counters_sysinfo_init();
  mactopo_init();
  hop_init();
  qmp_dialog_init();
  qmp_init();
  qga_dialog_init();
  suid_power_init();
  cnt_init();
  date_us = cloonix_get_usec();
  srand((int) (date_us & 0xFFFF));
  layout_topo_init();
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
/*--------------------------------------------------------------------------*/
