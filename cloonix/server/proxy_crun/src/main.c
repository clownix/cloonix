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

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "doorways_sock.h"
#include "proxy_crun.h"
#include "util_sock.h"
#include "c2c_peer.h"


static char g_proxyshare[MAX_NAME_LEN];
static char g_unix_sig_stream[MAX_PATH_LEN];
static int g_llid_sig;


/****************************************************************************/
static void heartbeat (int delta)
{
  static int count = 0;
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
        KOUT(" ");
      }
    fclose(fhd);
    }
  if (!found)
    KOUT(" ");
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
  if (access( dev_file, F_OK))
    printf("\nWARNING %s not found kvm unusable\n", dev_file);
  else if (!i_have_read_write_access(dev_file))
    printf("\nWARNING %s not writable kvm unusable\n", dev_file);
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
      printf("\nHint: sudo chmod 0666 /dev/kvm\n");
      printf("OR: sudo setfacl -m u:${USER}:rw /dev/kvm\n\n");
      }
    if (module_access_is_ko("/dev/vhost-net"))
      {
      printf("\nHint: sudo chmod 0666 /dev/vhost-net\n");
      printf("OR: sudo setfacl -m u:${USER}:rw /dev/vhost-net\n\n");
      }
    if (module_access_is_ko("/dev/net/tun"))
      {
      printf("\nHint: sudo chmod 0666 /dev/net/tun\n");
      printf("OR: sudo setfacl -m u:${USER}:rw /dev/net/tun\n\n");
      }
    }
  else
    printf("\nWARNING NO hardware virtualisation kvm unusable\n\n");
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
char *get_proxyshare(void)
{
  return g_proxyshare;
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
  KOUT("ERROR SIG %d %d %d %d", g_llid_sig, llid, err, from);
} 
/*--------------------------------------------------------------------------*/
  
/****************************************************************************/
static void rx_proxy_sig_cb (int llid, int len, char *buf)
{ 
  sig_process_rx(g_proxyshare, buf);
} 
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void connect_from_sig_client(int llid, int llid_new)
{
KERR("PROXY CONNECTED %d %d", llid, llid_new);
  msg_mngt_set_callbacks (llid_new, err_proxy_sig_cb, rx_proxy_sig_cb);
  g_llid_sig = llid_new;
} 
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int get_llid_sig(void)
{
  return g_llid_sig;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int main(int argc, char **argv)
{
  if (argc != 2)
    {
    printf("\nBAD NUMBER OF ARGS proxyshare directory must be given\n\n");
    exit(-1);
    }
  umask(0000);
  test_dev_kvm();
  g_llid_sig = 0;
  c2c_peer_init();
  doorways_sock_init(0);
  msg_mngt_init("proxy", IO_MAX_BUF_LEN);
  msg_mngt_heartbeat_init(heartbeat);
  doors_io_basic_xml_init(stub_tx);
  memset(g_proxyshare, 0, MAX_NAME_LEN);
  memset(g_unix_sig_stream, 0, MAX_PATH_LEN);
  strncpy(g_proxyshare, argv[1], MAX_NAME_LEN-1);
  snprintf(g_unix_sig_stream, MAX_PATH_LEN-1,
           "%s/proxy_sig_stream.sock", g_proxyshare);
  proxy_sig_server(g_unix_sig_stream, connect_from_sig_client);
  X_init(g_proxyshare);  
  daemon(0,0);
  msg_mngt_loop();
  return 0; 
}
/*--------------------------------------------------------------------------*/
