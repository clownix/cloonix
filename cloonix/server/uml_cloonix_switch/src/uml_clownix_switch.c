/*****************************************************************************/
/*    Copyright (C) 2006-2021 clownix@clownix.net License AGPL-3             */
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
#include "rpc_c2c.h"
#include "layout_rpc.h"
#include "eventfull.h"
#include "slowperiodic.h"
#include "file_read_write.h"
#include "utils_cmd_line_maker.h"
#include "layout_topo.h"
#include "rpc_qmonitor.h"
#include "qmonitor.h"
#include "qmp.h"
#include "qhvc0.h"
#include "doorways_mngt.h"
#include "doorways_sock.h"
#include "xwy.h"
#include "c2c.h"
#include "mulan_mngt.h"
#include "endp_mngt.h"
#include "hop_event.h"
#include "blkd_sub.h"
#include "blkd_data.h"
#include "cloonix_conf_info.h"
#include "unix2inet.h"
#include "qmp_dialog.h"
#include "dpdk_ovs.h"
#include "suid_power.h"
#include "edp_mngt.h"
#include "snf_dpdk_process.h"
#include "nat_dpdk_process.h"
#include "d2d_dpdk_process.h"
#include "a2b_dpdk_process.h"

static t_topo_clc g_clc;
static t_cloonix_conf_info *g_cloonix_conf_info;
static int g_i_am_in_cloonix;
static char g_i_am_in_cloonix_name[MAX_NAME_LEN];

static int g_cloonix_lock_fd = 0;
static int g_machine_is_kvm_able;
static char g_user[MAX_NAME_LEN];
static char **g_saved_environ;
static int g_machine_nb_cpu;
static int g_machine_hugepages_nb;
static int g_machine_hugepages_size;

char *used_binaries[] =
{
  "/bin/rm",
  "/bin/bash",
  "/bin/sh",
  "/bin/echo",
  "/bin/dd",
  "/bin/chmod",
  "/bin/cp",
  "/bin/ln",
  NULL,
};
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void init_g_user(void)
{
  struct passwd *pass;
  pass = getpwuid(getuid());
  snprintf(g_user, MAX_NAME_LEN-1, "%s", pass->pw_name);
  if (strlen(g_user) == 0)
    {
    printf("Could not get user name\n");
    KOUT("Could not get user name");
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
uint32_t get_cpu_mask(void)
{
  return (g_cloonix_conf_info->cpu_mask);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *get_user(void)
{
  return g_user;
}
/*--------------------------------------------------------------------------*/

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
  printf("\n\n%s /usr/local/bin/cloonix/cloonix_config nemo\n\n\n", name);
  exit(-1);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void  check_used_binaries_presence(t_topo_clc *conf)
{
  int i=0;
  char *suid_power_bin = utils_get_suid_power_bin_path();
  while(used_binaries[i])
    {
    if (!file_exists(used_binaries[i], X_OK))
      {
      printf("\"%s\" not found or not executable\n", used_binaries[i]);
      KOUT("\"%s\" not found or not executable\n", used_binaries[i]);
      }
    i++;
    } 
  if (!file_exists(util_get_xorrisofs(), X_OK))
    {
    printf("\"%s\" not found or not executable\n", util_get_xorrisofs());
    KOUT("\"%s\" not found or not executable\n", util_get_xorrisofs());
    }
  if (!file_exists(suid_power_bin, X_OK))
    {
    printf("\"%s\" not found or not executable\n", suid_power_bin);
    KOUT("\"%s\" not found or not executable\n", suid_power_bin);
    }

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
    printf ("\numl_cloonix_switch (pid=%d) already running, please kill it\n",
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
  if (!file_exists(path2, F_OK))
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
  mk_endp_dir();
  mk_dtach_dir();
  mk_dpdk_ovs_db_dir();
  mk_dpdk_dir();
  sprintf(path1, "%s/cloonix_lock",  cfg_get_root_work());
  check_for_another_instance(path1, 0);
}
/*--------------------------------------------------------------------------*/


/*****************************************************************************/
void uml_clownix_switch_error_cb(void *ptr, int llid, int err, int from)
{
  llid_trace_free(llid, 0, __FUNCTION__);
  doorways_err_cb (llid);
  endp_mngt_err_cb (llid);
  mulan_err_cb (llid);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void uml_clownix_switch_rx_cb(int llid, int len, char *buf)
{
  int result;
    result = doors_io_basic_decoder(llid, len, buf);
  if (result == -1)
    result = doors_io_c2c_decoder(llid, len, buf);
  if (result == -1)
    result = doors_io_layout_decoder(llid, len, buf);
  if (result == -1)
    result = doors_io_qmonitor_decoder(llid, len, buf);
  if (result == -1)
    result = doors_xml_decoder(llid, len, buf);
  if (result == -1)
    result = rpct_decoder(NULL, llid, len, buf);
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
static void connect_from_client_unix(void *ptr, int llid, int llid_new)
{
  llid_trace_alloc(llid_new,"CLIENT_SRV_UX",0,0, type_llid_trace_client_unix);
  msg_mngt_set_callbacks (llid_new, uml_clownix_switch_error_cb, 
                                    uml_clownix_switch_rx_cb);
  event_print("Listen:%d, New client connection: %d", llid, llid_new);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void launching(void)
{
  char clownlock[MAX_PATH_LEN];
  char *dtach = utils_get_dtach_bin_path();
  if (!file_exists(dtach, X_OK))
    {
    printf("\"%s\" not found or not executable\n", dtach);
    KOUT("\"%s\" not found or not executable\n", dtach);
    }
  set_cloonix_name(cfg_get_cloonix_name());
  printf("\n\n");
  printf("     Cloonix Version:        %s\n",cfg_get_version());
  printf("     Cloonix Name:           %s\n",cfg_get_cloonix_name());
  printf("     Cloonix Tree:           %s\n",cfg_get_bin_dir());
  printf("     Work Zone Path:         %s\n",cfg_get_root_work());
  printf("     Bulk Path:              %s\n",cfg_get_bulk());
  printf("     Server Doors Port:      %d\n",cfg_get_server_port());
  printf("\n\n\n");
  printf(" uml_cloonix_switch now running\n\n");
  if ((strlen(cfg_get_cloonix_name()) == 0) || 
      (strlen(cfg_get_bin_dir()) == 0)      || 
      (strlen(cfg_get_root_work()) == 0)    || 
      (strlen(cfg_get_bulk()) == 0))
    {
    printf("BADCONF\n");
    KOUT("BADCONF");
    }
  if ((1 << g_machine_nb_cpu) <= g_cloonix_conf_info->cpu_mask)
    {
    printf("WARNING! Probable dpdk config error, "
           "nb cpu on host: %d cpu_mask Ox%x\n\n",
            g_machine_nb_cpu, g_cloonix_conf_info->cpu_mask);
    }
  if (g_machine_hugepages_size < 1024*1024)
    {
    printf("WARNING! Probable dpdk config error, hugepages_size: %d\n\n",
            g_machine_hugepages_size);
    }
  if ((g_cloonix_conf_info->socket_mem) >
      (g_machine_hugepages_nb * (g_machine_hugepages_size/1024)))
    {
    printf("WARNING! Probable dpdk config error, "
           "1 giga hugepages: %d mem wanted: %dM\n\n",
           g_machine_hugepages_nb, g_cloonix_conf_info->socket_mem);
    }

  daemon(0,0);
  doorways_first_start();
  sprintf(clownlock, "%s/cloonix_lock", cfg_get_root_work());
  check_for_another_instance(clownlock, 1);
  init_xwy();
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
static void callback_after_work_dir_clean(void *data, int status, char *nm)
{
  int llid;
  if (data || nm)
    {
    printf("%s %d\n", __FUNCTION__, __LINE__);
    KOUT(" ");
    }
  if (status)
    {
    printf("%s %d\n", __FUNCTION__, __LINE__);
    KOUT("%d", status);
    }
  my_mkdir(cfg_get_work(), 0);
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
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void eval_bash(char *input, char *output, int max)
{
  FILE *fp;
  char *ptr;
  fp = popen(input, "r");
  fgets(output, max, fp);
  output[max] = 0;
  pclose(fp);
  ptr = strrchr(output, '/');
  if (!ptr)
    KOUT("%s %s", input, output);
  if (strncmp(ptr, "/endofline", strlen("/endofline")))
    KOUT("%s %s", input, output);
  *ptr = 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static t_topo_clc *get_parsed_config(char *name)
{
  char input[2*MAX_PATH_LEN];
  t_topo_clc *conf = NULL;
  t_cloonix_conf_info *cloonix_conf = cloonix_conf_info_get(name);
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
    snprintf(conf->username, MAX_NAME_LEN, "%s", g_user); 
    conf->username[MAX_NAME_LEN-1] = 0;
    snprintf(conf->network, MAX_NAME_LEN, "%s", cloonix_conf->name); 
    conf->network[MAX_NAME_LEN-1] = 0;
    conf->server_port = cloonix_conf->port; 
    snprintf(input, 2*MAX_PATH_LEN-1,"echo %s/%s/endofline",
             cloonix_conf_info_get_work(), 
                                                  cloonix_conf->name);
    eval_bash(input, conf->work_dir, MAX_PATH_LEN-1);
    snprintf(input, 2*MAX_PATH_LEN-1,"echo %s/endofline",
             cloonix_conf_info_get_tree()); 
    eval_bash(input, conf->bin_dir, MAX_PATH_LEN-1);
    snprintf(input, 2*MAX_PATH_LEN-1,"echo %s/endofline",
             cloonix_conf_info_get_bulk()); 
    eval_bash(input, conf->bulk_dir, MAX_PATH_LEN-1);
    g_cloonix_conf_info = cloonix_conf;
    }
  return conf;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int test_machine_nb_cpu(void)
{
  int nb_cpu = 0;
  FILE *fhd;
  char result[500];
  fhd = fopen("/proc/cpuinfo", "r");
  if (fhd)
    {
    while(fgets(result, 500, fhd))
      {
      if (!strncmp(result, "processor", strlen("processor")))
        nb_cpu += 1;
      }
    fclose(fhd);
    }
  return nb_cpu;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void test_machine_nb_hugePages(int *nb_hpages, int *hp_size)
{
  int nb;
  FILE *fhd;
  char result[500];
  fhd = fopen("/proc/meminfo", "r");
  if (fhd)
    {
    while(fgets(result, 500, fhd))
      {
      if (sscanf(result, "HugePages_Total: %d", &nb) == 1)
        *nb_hpages = nb;
      if (sscanf(result, "Hugepagesize: %d", &nb) == 1)
        *hp_size = nb;
      }
    fclose(fhd);
    }
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
static void connect_dummy(void *ptr, int llid, int llid_new)
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
  *name = g_i_am_in_cloonix_name;
  return g_i_am_in_cloonix;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char **get_saved_environ(void)
{
  return (g_saved_environ);
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
static char **save_environ(void)
{
  static char username[MAX_NAME_LEN];
  static char spice_env[MAX_NAME_LEN];
  static char home[MAX_PATH_LEN];
  static char *environ_normal[] = {username,home,spice_env,NULL };
  char **environ;
  memset(home, 0, MAX_PATH_LEN);
  snprintf(home, MAX_PATH_LEN-1, "HOME=%s", getenv("HOME"));
  memset(username, 0, MAX_NAME_LEN);
  snprintf(username, MAX_NAME_LEN-1, "USER=%s", getenv("USER"));
  environ = environ_normal;
  snprintf(spice_env, MAX_NAME_LEN-1, "SPICE_DEBUG_ALLOW_MC=1");
  return environ;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int main (int argc, char *argv[])
{
  long long date_us;
  t_topo_clc *conf;
  struct timespec ts;

  if (clock_gettime(CLOCK_MONOTONIC, &ts))
    KOUT(" ");
  g_i_am_in_cloonix = i_am_inside_cloonix(g_i_am_in_cloonix_name);

  unix2inet_init();
  init_g_user();
  job_for_select_init();
  utils_init();
  recv_init();
  g_cloonix_lock_fd = 0;
  g_machine_is_kvm_able = test_machine_is_kvm_able();
  g_machine_nb_cpu = test_machine_nb_cpu();
  test_machine_nb_hugePages(&g_machine_hugepages_nb,&g_machine_hugepages_size);
  if (argc != 3)
    usage(argv[0]);
  if (cloonix_conf_info_init(argv[1]))
    KOUT("%s", argv[1]);
  conf = get_parsed_config(argv[2]);
  if (!conf)
    KOUT(" ");
  cfg_init();
  cfg_set_host_conf(conf);
  check_used_binaries_presence(conf);
  lan_init();
  llid_trace_init();
  event_subscriber_init();
  doors_io_basic_xml_init(string_tx);
  doors_io_c2c_init(string_tx);
  doors_io_layout_xml_init(string_tx);
  doors_io_qmonitor_xml_init(string_tx);
  blkd_sub_init();
  blkd_data_init();
  automates_init();
  g_saved_environ = save_environ();
  msg_mngt_init(cfg_get_cloonix_name(), IO_MAX_BUF_LEN);
  if (tst_port_is_not_used(conf->server_port))
    {
    printf("Port: %d is in use!!\n", conf->server_port);
    KOUT("Port: %d is in use!!", conf->server_port);
    }
  doorways_sock_init();
  doorways_init(cfg_get_root_work(), cfg_get_server_port(), 
                g_cloonix_conf_info->passwd);
  eventfull_init();
  slowperiodic_init();
  if (!file_exists(get_doorways_bin(), X_OK))
    {
    printf("\n\nFile: \"%s\" not found or not executable\n\n", 
           get_doorways_bin());
    KOUT("\n\nFile: \"%s\" not found or not executable\n\n", 
         get_doorways_bin());
    }
  mk_and_tst_work_path();
  pid_clone_init();
  check_for_work_dir_inexistance(callback_after_work_dir_clean);
  init_heartbeat();
  stats_counters_init();
  stats_counters_sysinfo_init();
  hop_init();
  init_c2c();
  init_qmonitor();
  qmp_dialog_init();
  qmp_init();
  init_qhvc0();
  mulan_init();
  endp_mngt_init();
  dpdk_ovs_init(g_cloonix_conf_info->lcore_mask,
                g_cloonix_conf_info->socket_mem,
                g_cloonix_conf_info->cpu_mask);
  suid_power_init();
  edp_mngt_init();
  snf_dpdk_init();
  nat_dpdk_init();
  a2b_dpdk_init();
  d2d_dpdk_init();
  date_us = cloonix_get_usec();
  srand((int) (date_us & 0xFFFF));
  msg_mngt_loop();
  return 0;
}
/*--------------------------------------------------------------------------*/
