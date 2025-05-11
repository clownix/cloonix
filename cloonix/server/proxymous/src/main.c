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
#include <sys/types.h> 
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <signal.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/select.h>
#include <fcntl.h>
#include <linux/tcp.h>
#include <dirent.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "doorways_sock.h"
#include "proxy_crun.h"
#include "util_sock.h"
#include "c2c_peer.h"
#include "nat_main.h"

static char g_net_name[MAX_NAME_LEN];
static char g_ctrl_path[MAX_PATH_LEN];
static char g_unix_sig_stream[MAX_PATH_LEN];
static char g_proxyshare_dir[MAX_PATH_LEN];
static int g_llid_ctrl;
static int g_llid_sig;
static int g_watchdog_ok;

void set_config_rapid_beat(void);

/****************************************************************************/
char *get_net_name(void)
{
  return g_net_name;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
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
    fprintf(fp, "cloonix_main_server_ready");
  else
    fprintf(fp, "cloonix_main_server_not_ready");
  fclose(fp);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void fct_timeout_self_destruct(void *data)
{
  if (g_watchdog_ok == 0)
    {
    KERR("SELF DESTRUCT WATCHDOG PROXYMOUS");
    KOUT("EXIT");
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int check_and_set_uid(void)
{
  int result = -1;
  uid_t uid;
  seteuid(0);
  setegid(0);
  uid = geteuid();
  if (uid == 0)
    {
    result = 0;
    if (setuid(0))
      KERR("ERROR setuid");
    if (setgid(0))
      KERR("ERROR setgid");
    }
  else 
    KERR("WARNING NOT SUID ROOT");
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void heartbeat (int delta)
{
  static int count = 0;
  set_config_rapid_beat();
  count += 1;
  if (count == 100)
    {
    count = 0;
    sig_process_heartbeat();
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int i_have_read_write_access(char *path)
{
  return ( ! access(path, R_OK|W_OK) );
}
/*--------------------------------------------------------------------------*/
 
/*****************************************************************************/
static int test_machine_is_kvm_able(void)
{
  int found = 0;
  FILE *fhd;
  char *result = NULL;
  fhd = fopen("/proc/cpuinfo", "r");
  if (fhd)
    { 
    result = (char *) malloc(500);
    while(!found)
      {
      if (fgets(result, 500, fhd) != NULL)
        {
        if (!strncmp(result, "flags", strlen("flags")))
          found = 1;
        }
      else 
        KOUT("ERROR");
      }
    fclose(fhd);
    }
  if (!found)
    KOUT("ERROR");
  found = 0;
  if ((strstr(result, "vmx")) || (strstr(result, "svm")))
    found = 1;
  free(result);
  return found;
}
/*---------------------------------------------------------------------------*/
  
/*****************************************************************************/
static int module_access_is_ko(char *dev_file)
{
  int result = -1;
  int fd;
  if (access( dev_file, R_OK))
    printf("\nWARNING %s not found kvm needs it to be rw\n", dev_file);
  else if (!i_have_read_write_access(dev_file))
    printf("\nWARNING %s not writable kvm needs it to be rw\n", dev_file);
  else 
    {
    fd = open(dev_file, O_RDWR);
    if (fd >= 0)
      {
      result = 0;
      close(fd);
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void test_dev_kvm(void)
{
  if (test_machine_is_kvm_able())
    {
    if (module_access_is_ko("/dev/kvm"))
      {
      printf("\n\tHint:\nsudo usermod -a -G kvm <user>\n");
      printf("\tOR:\nsudo chmod 0666 /dev/kvm\n");
      printf("\tOR:\nsudo setfacl -m u:<user>:rw /dev/kvm\n\n");
      }
    if (module_access_is_ko("/dev/vhost-net"))
      {
      printf("\n\tHint:\nsudo usermod -a -G kvm <user>\n");
      printf("\tOR:\nsudo chmod 0666 /dev/vhost-net\n");
      printf("\tOR:\nsudo setfacl -m u:${USER}:rw /dev/vhost-net\n\n");
      }
    if (module_access_is_ko("/dev/net/tun"))
      {
      printf("\n\tHint:\nsudo usermod -a -G kvm <user>\n");
      printf("\tOR:\nsudo chmod 0666 /dev/net/tun\n");
      printf("\tOR:\nsudo setfacl -m u:${USER}:rw /dev/net/tun\n\n");
      }
    }
  else
    printf("\nWARNING NO hardware virtualisation kvm needs it\n\n");
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void stub_tx(int llid, int len, char *buf)
{   
  KOUT("ERROR %s", buf);
}     
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void err_proxy_sig_cb (int llid, int err, int from)
{ 
  KERR("ERROR SIGCRUN TO MASTER %d %d %d", llid, err, from);
  if (msg_exist_channel(llid))
    msg_delete_channel(llid);
} 
/*--------------------------------------------------------------------------*/
  
/****************************************************************************/
static void rx_proxy_sig_cb (int llid, int len, char *buf)
{ 
  sig_process_rx(g_proxyshare_dir, buf);
} 
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void connect_from_sig_client(int llid, int llid_new)
{
  msg_mngt_set_callbacks (llid_new, err_proxy_sig_cb, rx_proxy_sig_cb);
  g_llid_sig = llid_new;
} 
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void err_ctrl_cb (int llid, int err, int from)
{
  KERR("ERROR CTRL TO MASTER %d %d %d", llid, err, from);
  if (msg_exist_channel(llid))
    msg_delete_channel(llid);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void rx_ctrl_cb (int llid, int len, char *buf)
{
  if (rpct_decoder(llid, len, buf))
    KOUT("ERROR %s", buf);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void connect_from_ctrl_client(int llid, int llid_new)
{
  msg_mngt_set_callbacks (llid_new, err_ctrl_cb, rx_ctrl_cb);
  g_llid_ctrl = llid_new;
  if (msg_exist_channel(llid))
    msg_delete_channel(llid);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *get_proxyshare(void)
{
  return g_proxyshare_dir;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int get_llid_sig(void)
{
  return g_llid_sig;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void rpct_recv_kil_req(int llid, int tid)
{ 
  nat_main_kil_req();
  unlink(g_ctrl_path);
  exit(0);
}
/*--------------------------------------------------------------------------*/
 
/****************************************************************************/
void rpct_recv_pid_req(int llid, int tid, char *name, int num)
{ 
  g_watchdog_ok = 1;
  if (llid != g_llid_ctrl)
    KERR("ERROR %s %d %d", name, llid, g_llid_ctrl);
  if (tid != type_hop_proxymous)
    KERR("ERROR %s %d", name, tid);
  rpct_send_pid_resp(llid, tid, name, num, cloonix_get_pid(), getpid());
}     
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void check_and_create_tmp_dir(char *tmp_dir)
{
  DIR *dir;
  int result;
  dir = opendir(tmp_dir);
  if (dir)
    {
    if (access(tmp_dir, W_OK))
      KOUT("ERROR %s exists but is not writable", tmp_dir);
    closedir(dir);
    }
  else if (errno == ENOENT)
    {
    result = mkdir(tmp_dir, 0777);
    if (result)
      KOUT("ERROR CANNOT MKDIR %s", tmp_dir);
    if (access(tmp_dir, W_OK))
      KOUT("ERROR %s", tmp_dir);
    }
  else
    KOUT("ERROR %s opendir %d", tmp_dir, errno);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int main(int argc, char **argv)
{
  char proxyshare_in_dir[MAX_PATH_LEN];
  char x11_unix_dir[MAX_PATH_LEN];

  if (argc != 3)
    {
    printf("\nBAD ARG proxymous_root_path must be given\n\n");
    exit(-1);
    }
  umask(0000);
  check_and_set_uid();
  test_dev_kvm();
  g_llid_sig = 0;
  g_llid_ctrl = 0;
  g_watchdog_ok = 0;
  c2c_peer_init();
  doorways_sock_init(0);
  msg_mngt_init("proxy", TRAF_TAP_BUF_LEN);
  msg_mngt_heartbeat_init(heartbeat);
  doors_io_basic_xml_init(stub_tx);
  nat_main_init();
  memset(g_net_name, 0, MAX_NAME_LEN);
  memset(g_ctrl_path, 0, MAX_PATH_LEN);
  memset(g_proxyshare_dir, 0, MAX_PATH_LEN);
  memset(proxyshare_in_dir, 0, MAX_PATH_LEN);
  strncpy(g_proxyshare_dir, argv[1], MAX_PATH_LEN-1);
  snprintf(x11_unix_dir, MAX_PATH_LEN-1,"%s/X11-unix", argv[1]);
  strncpy(g_net_name, argv[2], MAX_NAME_LEN-1);
  snprintf(proxyshare_in_dir, MAX_PATH_LEN-1,"%s_%s",
           PROXYSHARE_IN, g_net_name );
  snprintf(g_ctrl_path, MAX_PATH_LEN-1,"%s/%s",
           g_proxyshare_dir, PROXYMOUS);
  check_and_create_tmp_dir(g_proxyshare_dir);
  check_and_create_tmp_dir(x11_unix_dir);
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

  if (strcmp(g_proxyshare_dir, proxyshare_in_dir))
    {
    KERR("INFO: PROXYMOUS WITH X11");
    X_init(g_proxyshare_dir);  
    }

  write_start_status_file(0);

  daemon(0,0);
  clownix_timeout_add(3000, fct_timeout_self_destruct, NULL, NULL, NULL);
  msg_mngt_loop();
  return 0; 
}
/*--------------------------------------------------------------------------*/
