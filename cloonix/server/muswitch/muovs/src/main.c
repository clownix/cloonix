/*****************************************************************************/
/*    Copyright (C) 2006-2019 cloonix@cloonix.net License AGPL-3             */
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
#include <stdint.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <libgen.h>



#include "ioc.h"
#include "ovs_execv.h"
#include "pcap_fifo.h"
#include "ring_client.h"

#define CLOONIX_DIAG_LOG  "cloonix_diag.log"

static char g_ovs_bin[MAX_PATH_LEN];
static char g_dpdk_dir[MAX_PATH_LEN];
static int g_ovsdb_launched;
static int g_ovs_launched;
static int g_cloonix_listen_fd;
static int g_cloonix_fd;
static int g_ovsdb_pid;
static int g_ovs_pid;


/*****************************************************************************/
void linker_helper1_fct(void)
{
  printf("useless");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int get_cloonix_listen_fd(void)
{
  return g_cloonix_listen_fd;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int get_cloonix_fd(void)
{
  return g_cloonix_fd;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static char *mydirname(char *path)
{
  static char tmp[MAX_PATH_LEN];
  char *pdir;
  memset(tmp, 0, MAX_PATH_LEN);
  memcpy(tmp, path, MAX_PATH_LEN-1);
  pdir = dirname(tmp);
  return (pdir);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int is_directory_writable(char *path)
{
  int result = 0;
  struct stat stat_file;
  if (!stat(path, &stat_file))
    {
    if ((stat_file.st_mode & S_IFMT) == S_IFDIR)
      if (!access(path, W_OK))
        result = 1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int file_exists(char *path)
{
  int err, result = 0;
  err = access(path, F_OK);
  if (!err)
    result = 1;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void unlink_files(char *dpdk_dir)
{
  char arg[NB_ARG][MAX_ARG_LEN];
  if (!access(dpdk_dir, F_OK))
    {
    memset(arg, 0, MAX_ARG_LEN * NB_ARG);
    snprintf(arg[0], MAX_PATH_LEN-1,"/bin/rm");
    snprintf(arg[1], MAX_PATH_LEN-1,"-fdR");
    snprintf(arg[2], MAX_ARG_LEN-1,"%s/*", dpdk_dir);
    if (call_my_popen(dpdk_dir, 3, arg)) 
      KERR(" ");
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void unlink_dir(char *dpdk_dir)
{
  char arg[NB_ARG][MAX_ARG_LEN];
  memset(arg, 0, MAX_ARG_LEN * NB_ARG);
  snprintf(arg[0], MAX_PATH_LEN-1,"/bin/rm");
  snprintf(arg[1], MAX_PATH_LEN-1,"-fdR");
  snprintf(arg[2], MAX_ARG_LEN-1,"%s/dpdk", dpdk_dir);
  if (call_my_popen(dpdk_dir, 3, arg)) 
    KERR(" ");
  while (!access(arg[2], F_OK))
    {
    if (call_my_popen(dpdk_dir, 3, arg)) 
      KERR(" ");
    usleep(10000);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void timeout_rpct_heartbeat(t_all_ctx *all_ctx, void *data)
{
  rpct_heartbeat((void *) all_ctx);
  clownix_timeout_add(all_ctx, 100, timeout_rpct_heartbeat, NULL, NULL, NULL);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void timeout_blkd_heartbeat(t_all_ctx *all_ctx, void *data)
{
  blkd_heartbeat((void *) all_ctx);
  clownix_timeout_add(all_ctx, 1, timeout_blkd_heartbeat, NULL, NULL, NULL);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void collect_eventfull(t_all_ctx *all_ctx, int tidx,
                              int *nb_pkt_tx, int *nb_bytes_tx,
                              int *nb_pkt_rx, int *nb_bytes_rx)
{
  *nb_pkt_tx = all_ctx->g_traf_endp[tidx].nb_pkt_tx;
  *nb_bytes_tx = all_ctx->g_traf_endp[tidx].nb_bytes_tx;
  *nb_pkt_rx = all_ctx->g_traf_endp[tidx].nb_pkt_rx;
  *nb_bytes_rx = all_ctx->g_traf_endp[tidx].nb_bytes_rx;
  all_ctx->g_traf_endp[tidx].nb_pkt_tx = 0;
  all_ctx->g_traf_endp[tidx].nb_bytes_tx = 0;
  all_ctx->g_traf_endp[tidx].nb_pkt_rx = 0;
  all_ctx->g_traf_endp[tidx].nb_bytes_rx = 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void eventfull_endp(t_all_ctx *all_ctx, int cloonix_llid, int tidx)
{
  int nb_pkt_tx, nb_pkt_rx, nb_bytes_tx, nb_bytes_rx;
  char txt[2*MAX_NAME_LEN];
  collect_eventfull(all_ctx, tidx, &nb_pkt_tx, &nb_bytes_tx,
                                   &nb_pkt_rx, &nb_bytes_rx);
  memset(txt, 0, 2*MAX_NAME_LEN);
  snprintf(txt, (2*MAX_NAME_LEN) - 1,
           "endp_eventfull_tx_rx %u %d %d %d %d %d",
           (unsigned int) cloonix_get_msec(), tidx, nb_pkt_tx, nb_bytes_tx,
           nb_pkt_rx, nb_bytes_rx);
  rpct_send_evt_msg(all_ctx, cloonix_llid, 0, txt);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int mycmp(char *req, char *targ)
{
  int result = strncmp(req, targ, strlen(targ));
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void log_write_req_resp(char *line, char *respb)
{
  FILE *fp_log;
  char pth[MAX_PATH_LEN];
  memset(pth, 0, MAX_PATH_LEN);
  snprintf(pth, MAX_PATH_LEN, "%s", g_dpdk_dir);
  if ((strlen(pth) + MAX_NAME_LEN) >= MAX_PATH_LEN)
    KOUT("%d", (int) strlen(pth));
  strcat(pth, "/");
  strcat(pth, CLOONIX_DIAG_LOG);
  fp_log = fopen(pth, "a+");
  if (fp_log)
    {
    fprintf(fp_log, "%s\n", line);
    fprintf(fp_log, "%s\n\n", respb);
    fflush(fp_log);
    fclose(fp_log);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_evt_msg(void *ptr, int llid, int tid, char *line)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_report(void *ptr, int llid, t_blkd_item *item)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_pid_resp(void *ptr, int llid, int tid, char *name, int num,
                        int toppid, int pid)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_hop_msg(void *ptr, int llid, int tid, int flags_hop, char *txt)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_hop_sub(void *ptr, int llid, int tid, int flags_hop)
{
  t_all_ctx *all_ctx = (t_all_ctx *) ptr;
  DOUT((void *) all_ctx, FLAG_HOP_DIAG, "Hello from lan %s", all_ctx->g_name);
  rpct_hop_print_add_sub((void *) all_ctx, llid, tid, flags_hop);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_hop_unsub(void *ptr, int llid, int tid)
{
  rpct_hop_print_del_sub(ptr, llid);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_kil_req(void *ptr, int llid, int tid)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_pid_req(void *ptr, int llid, int tid, char *name, int num)
{
  t_all_ctx *all_ctx = (t_all_ctx *) ptr;
  if (strcmp(name, all_ctx->g_name))
    KERR("%s %s", name, all_ctx->g_name);
  if (all_ctx->g_num != num)
    KERR("%s %d %d", name, num, all_ctx->g_num);
  DOUT(all_ctx, FLAG_HOP_DIAG, "%s %s", __FUNCTION__, name);
  if ((g_ovs_pid == 0) && (g_ovsdb_pid == 0)) 
    rpct_send_pid_resp(ptr, llid, tid, name, num, 0, getpid());
  else
    rpct_send_pid_resp(ptr, llid, tid, name, num, g_ovs_pid, g_ovsdb_pid);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_cli_resp(void *ptr, int llid, int tid,
                     int cli_llid, int cli_tid, char *line)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int check_uid(void)
{
  int result = -1;
  uid_t prev_uid, uid;
  prev_uid = geteuid();
  seteuid(0);
  uid = geteuid();
  if (uid == 0)
    result = 0;
  seteuid(prev_uid);
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void add_eth_br(t_all_ctx *all_ctx, char *respb,
                                           char *name, int num, int spy)
{
  char *bin = g_ovs_bin;
  char *db = g_dpdk_dir;
  if (ovs_execv_add_eth(all_ctx, bin, db, name, num))
    {
    KERR("%s %d", name, num);
    snprintf(respb, MAX_PATH_LEN-1,
             "KO cloonixovs_add_eth name=%s num=%d spy=%d", name, num, spy);
    }
  else if (ovs_execv_add_spy(all_ctx, bin, db, name, num, spy))
    {
    KERR("%s", name);
    snprintf(respb, MAX_PATH_LEN-1,
             "KO cloonixovs_add_eth name=%s num=%d spy=%d", name, num, spy);
    }
  else if (ovs_execv_add_spy_eth(all_ctx, bin, db, name, num))
    {
    KERR("%s %d", name, num);
    snprintf(respb, MAX_PATH_LEN-1,
             "KO cloonixovs_add_eth name=%s num=%d spy=%d", name, num, spy);
    }
  else if (ring_add_dpdkr(spy))
    {
    KERR("%s %d", name, num);
    snprintf(respb, MAX_PATH_LEN-1,
             "KO cloonixovs_add_eth name=%s num=%d spy=%d", name, num, spy);
    }
  else
    {
    snprintf(respb, MAX_PATH_LEN-1,
             "OK cloonixovs_add_eth name=%s num=%d spy=%d", name, num, spy);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void del_eth_br(t_all_ctx *all_ctx, char *respb,
                                           char *name, int num, int spy)
{
  char *bin = g_ovs_bin;
  char *db = g_dpdk_dir;
  if (ovs_execv_del_eth(all_ctx, bin, db, name, num))
    {
    KERR("%s %d", name, num);
    snprintf(respb, MAX_PATH_LEN-1,
             "KO cloonixovs_del_eth name=%s num=%d spy=%d", name, num, spy);
    }
  else if (ring_add_dpdkr(spy))
    {
    KERR("%s %d", name, num);
    snprintf(respb, MAX_PATH_LEN-1,
             "KO cloonixovs_del_eth name=%s num=%d spy=%d", name, num, spy);
    }
  else
    {
    snprintf(respb, MAX_PATH_LEN-1,
             "OK cloonixovs_del_eth name=%s num=%d spy=%d", name, num, spy);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void add_lan_br(t_all_ctx *all_ctx, char *respb, char *name)
{
  if (ovs_execv_add_lan(all_ctx, g_ovs_bin, g_dpdk_dir, name))
    {
    KERR("%s", name);
    snprintf(respb, MAX_PATH_LEN-1,
               "KO cloonixovs_add_lan lan_name=%s", name);
    }
  else
    {
    snprintf(respb, MAX_PATH_LEN-1,
             "OK cloonixovs_add_lan lan_name=%s", name);
    pcap_fifo_init(all_ctx, g_dpdk_dir, name);
    }
  usleep(100000);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void add_lan_eth_br(t_all_ctx *all_ctx, char *respb,
                           char *lan, char *name, int num)
{
  if (ovs_execv_add_lan_eth(all_ctx,g_ovs_bin,g_dpdk_dir,lan,name,num))
    {
    KERR("%s %s %d", lan, name, num);
    snprintf(respb, MAX_PATH_LEN-1,
             "KO cloonixovs_add_lan_eth lan_name=%s name=%s num=%d",
             lan, name, num);
    }
  else
    {
    snprintf(respb, MAX_PATH_LEN-1,
             "OK cloonixovs_add_lan_eth lan_name=%s name=%s num=%d",
             lan, name, num);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_diag_msg(void *ptr, int llid, int tid, char *line)
{
  t_all_ctx *all_ctx = (t_all_ctx *) ptr;
  int num, spy, cloonix_llid;
  char respb[MAX_PATH_LEN];
  char lan[MAX_NAME_LEN];
  char name[MAX_NAME_LEN];
  DOUT(all_ctx, FLAG_HOP_DIAG, "%s", line);
  if (!file_exists(g_dpdk_dir))
    KOUT("%s", g_dpdk_dir);
  if (!is_directory_writable(g_dpdk_dir))
    KOUT("%s", g_dpdk_dir);
  memset(respb, 0, MAX_PATH_LEN); 
  if (!mycmp(line, "cloonixovs_req_suidroot"))
    {
    if (check_uid())
      snprintf(respb, MAX_PATH_LEN-1, "cloonixovs_resp_suidroot_ko");
    else
      snprintf(respb, MAX_PATH_LEN-1, "cloonixovs_resp_suidroot_ok");
    }
  else if (!mycmp(line, "cloonixovs_req_ovsdb"))
    {
    snprintf(respb, MAX_PATH_LEN-1, "cloonixovs_resp_ovsdb_ok");
    if (g_ovsdb_launched == 0)
      {
      g_ovsdb_launched = 1;
      unlink_files(g_dpdk_dir);
      g_ovsdb_pid = ovs_execv_daemon(all_ctx, 0, g_ovs_bin, g_dpdk_dir);
      if (g_ovsdb_pid <= 0)
        snprintf(respb, MAX_PATH_LEN-1, "cloonixovs_resp_ovsdb_ko");
      }
    }
  else if (!mycmp(line, "cloonixovs_req_ovs"))
    {
    snprintf(respb, MAX_PATH_LEN-1, "cloonixovs_resp_ovs_ok");
    if (g_ovs_launched == 0)
      {
      g_ovs_launched = 1;
      g_ovs_pid = ovs_execv_daemon(all_ctx, 1, g_ovs_bin, g_dpdk_dir);
      if (g_ovs_pid <= 0)
        snprintf(respb, MAX_PATH_LEN-1, "cloonixovs_resp_ovs_ko");
      else if (ring_dpdkr_init(all_ctx, g_dpdk_dir))
        snprintf(respb, MAX_PATH_LEN-1, "cloonixovs_resp_ovs_ko");
      sleep(1);
      }
    }
  else if (sscanf(line, "cloonixovs_add_eth name=%s num=%d spy=%d",
                        name, &num, &spy) == 3)
    {
    add_eth_br(all_ctx, respb, name, num, spy);
    }
  else if (sscanf(line, "cloonixovs_del_eth name=%s num=%d spy=%d",
                        name, &num, &spy) == 3) 
    {
    del_eth_br(all_ctx, respb, name, num, spy);
    }
  else if (sscanf(line, "cloonixovs_add_lan lan_name=%s", name) == 1)
    {
    add_lan_br(all_ctx, respb, name);
    }
  else if (sscanf(line, "cloonixovs_del_lan lan_name=%s", name) == 1)
    {
    if (ovs_execv_del_lan(all_ctx, g_ovs_bin, g_dpdk_dir, name))
      snprintf(respb, MAX_PATH_LEN-1,
               "KO cloonixovs_del_lan lan_name=%s", name);
    else
      snprintf(respb, MAX_PATH_LEN-1,
               "OK cloonixovs_del_lan lan_name=%s", name);
    }
  else if (sscanf(line, "cloonixovs_add_lan_eth lan_name=%s name=%s num=%d",
                        lan, name, &num) == 3)
    {
    add_lan_eth_br(all_ctx, respb, lan, name, num);
    }
  else if (sscanf(line, "cloonixovs_del_lan_eth lan_name=%s name=%s num=%d",
                        lan, name, &num) == 3)
    {
    if (ovs_execv_del_lan_eth(all_ctx,g_ovs_bin,g_dpdk_dir,lan,name,num))
      snprintf(respb, MAX_PATH_LEN-1,
               "KO cloonixovs_del_lan_eth lan_name=%s name=%s num=%d",
               lan, name, num);
    else
      snprintf(respb, MAX_PATH_LEN-1,
               "OK cloonixovs_del_lan_eth lan_name=%s name=%s num=%d",
               lan, name, num);
    }
  else if (!strcmp(line, "cloonixovs_req_destroy"))
    { 
    if (g_ovs_pid)
      kill(g_ovs_pid, SIGKILL);
    if (g_ovsdb_pid)
      kill(g_ovsdb_pid, SIGKILL);
    unlink_dir(g_dpdk_dir);
    sync();
    exit(0);
    }
  else
    KOUT("%s", line);
  cloonix_llid = blkd_get_cloonix_llid((void *) all_ctx);
  if (!cloonix_llid)
    KOUT(" ");
  if (cloonix_llid != llid)
    KOUT("%d %d", cloonix_llid, llid);

  log_write_req_resp(line, respb);

  rpct_send_diag_msg(all_ctx, llid, tid, respb);

}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void rpct_recv_app_msg(void *ptr, int llid, int tid, char *line)
{
  KERR("%s", line);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_cli_req(void *ptr, int llid, int tid,
                    int cli_llid, int cli_tid, char *line)
{
  KERR("%s", line);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void rx_cloonix_cb(t_all_ctx *all_ctx, int llid, int len, char *buf)
{
  if (rpct_decoder(all_ctx, llid, len, buf))
    {
    KOUT("%s", buf);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void cloonix_err_cb(void *ptr, int llid, int err, int from)
{
  exit(0);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void eventfull_can_be_sent(t_all_ctx *all_ctx, void *data)
{
  int llid = blkd_get_cloonix_llid((void *) all_ctx);
  int is_blkd, cidx = msg_exist_channel(all_ctx, llid, &is_blkd, __FUNCTION__);
  if (cidx)
    {
    eventfull_endp(all_ctx, llid, 0);
    }
  clownix_timeout_add(all_ctx, 5, eventfull_can_be_sent, NULL, NULL, NULL);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void cloonix_connect(void *ptr, int llid, int llid_new)
{
  t_all_ctx *all_ctx = (t_all_ctx *) ptr;
  int cloonix_llid = blkd_get_cloonix_llid(ptr);
  if (!cloonix_llid)
    blkd_set_cloonix_llid(ptr, llid_new);
  msg_mngt_set_callbacks (all_ctx, llid_new, cloonix_err_cb, rx_cloonix_cb);
  g_cloonix_fd = get_fd_with_llid(all_ctx, llid_new);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static t_all_ctx *cloonix_part_init(char **argv)
{
  t_all_ctx *all_ctx;
  int llid;
  char *dir_sock;
  cloonix_set_pid(getpid());
  all_ctx = msg_mngt_init((char *) argv[1], 0, IO_MAX_BUF_LEN);
  if (!strcmp(argv[1], "ovsdb"))
    blkd_set_our_mutype((void *) all_ctx, endp_type_ovsdb);
  else
    KOUT("%s", argv[1]); 
  memset(all_ctx->g_net_name, 0, MAX_NAME_LEN);
  memset(all_ctx->g_name, 0, MAX_NAME_LEN);
  memset(all_ctx->g_path, 0, MAX_PATH_LEN);
  memcpy(all_ctx->g_net_name, argv[0], MAX_NAME_LEN-1);
  memcpy(all_ctx->g_name, argv[1], MAX_NAME_LEN-1);
  memcpy(all_ctx->g_path, argv[2], MAX_PATH_LEN-1);
  if (file_exists(all_ctx->g_path))
    KOUT("PROBLEM WITH: %s EXISTS!", all_ctx->g_path);
  dir_sock = mydirname(all_ctx->g_path);
  if (!file_exists(dir_sock))
    KOUT("PROBLEM WITH: %s DOES NOT EXIST!", dir_sock);
  if (!is_directory_writable(dir_sock))
    KOUT("PROBLEM WITH: %s NOT WRITABLE!", dir_sock);
  llid = string_server_unix(all_ctx, all_ctx->g_path, cloonix_connect);
  if (llid == 0)
    KOUT("PROBLEM WITH: %s", all_ctx->g_path);
  g_cloonix_listen_fd = get_fd_with_llid(all_ctx, llid);
  return all_ctx;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int main (int argc, char *argv[])
{
  t_all_ctx *all_ctx;
  char net[2*MAX_NAME_LEN];
  char name[2*MAX_NAME_LEN];
  char sock[2*MAX_PATH_LEN];
  char *ctl_argv[4] = {net, name, sock, NULL};
  g_ovsdb_launched = 0;
  g_ovsdb_pid = 0;
  g_ovs_launched = 0;
  g_ovs_pid = 0;
  seteuid(0);
  setegid(0);
  umask(0000);
  if (argc != 6)
    KOUT("wrong params nb: net,name,sock,ovs_bin,dpdk_dir as params");
  memset(net, 0, MAX_NAME_LEN);
  memset(name, 0, MAX_NAME_LEN);
  memset(sock, 0, MAX_PATH_LEN);
  memset(g_ovs_bin, 0, MAX_PATH_LEN);
  memset(g_dpdk_dir, 0, MAX_PATH_LEN);
  memcpy(net,  argv[1], MAX_NAME_LEN-1);
  memcpy(name, argv[2], MAX_NAME_LEN-1);
  memcpy(sock, argv[3], MAX_PATH_LEN-1);
  memcpy(g_ovs_bin, argv[4], MAX_PATH_LEN-1);
  memcpy(g_dpdk_dir, argv[5], MAX_PATH_LEN-1);
  all_ctx = cloonix_part_init(ctl_argv);
  clownix_timeout_add(all_ctx, 500, eventfull_can_be_sent, NULL, NULL, NULL);
  clownix_timeout_add(all_ctx, 100, timeout_rpct_heartbeat, NULL, NULL, NULL);
  clownix_timeout_add(all_ctx, 100, timeout_blkd_heartbeat, NULL, NULL, NULL);
  msg_mngt_loop(all_ctx);
  return 0;
}
/*--------------------------------------------------------------------------*/

