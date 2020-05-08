/*****************************************************************************/
/*    Copyright (C) 2006-2020 clownix@clownix.net License AGPL-3             */
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
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <libgen.h>


#include "ioc.h"
#include "sock_fd.h"

int  tap_fd_open(t_all_ctx *all_ctx, char *tap_name);
int  wif_fd_open(t_all_ctx *all_ctx, char *tap_name);
int  raw_fd_open(t_all_ctx *all_ctx, char *tap_name);


typedef struct t_timer_data
{
  int timeout_mulan_id;
  int tidx;
  int num;
  char lan[MAX_NAME_LEN];
} t_timer_data;


/*****************************************************************************/
static void db_rpct_send_diag_msg(t_all_ctx *all_ctx,
                                 int llid, int tid, char *line)
{
  DOUT(all_ctx, FLAG_HOP_DIAG, "%s", line);
  rpct_send_diag_msg(all_ctx, llid, tid, line);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void resp_send_mueth_con_ko(t_all_ctx *all_ctx, char *lan,
                                   int num, int tidx)
{
  char resp[MAX_PATH_LEN];
  int llid, is_blkd, cidx;
  t_traf_endp *traf = &(all_ctx->g_traf_endp[tidx]);
  llid = traf->con_llid;
  if (llid)
    {
    cidx = msg_exist_channel(all_ctx, llid, &is_blkd, __FUNCTION__);
    if (cidx)
      {
      snprintf(resp, MAX_PATH_LEN-1, 
               "cloonix_resp_connect_ko lan=%s name=%s num=%d tidx=%d",     
               lan, all_ctx->g_name, num, tidx);
      db_rpct_send_diag_msg(all_ctx, llid, traf->con_tid, resp);
      }
    else
      KERR("%s %s", all_ctx->g_name, lan);
    traf->timeout_mulan_id += 1;
    traf->con_llid = 0;
    traf->con_tid = 0;
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void resp_send_mueth_con_ok(t_all_ctx *all_ctx, char *lan, 
                                   int num, int tidx)
{
  char resp[MAX_PATH_LEN];
  char slan[MAX_NAME_LEN];
  int rank, llid, cidx, is_blkd, snum, stidx;
  t_traf_endp *traf = &(all_ctx->g_traf_endp[tidx]);
  llid = traf->con_llid;
  if (llid)
    {
    cidx = msg_exist_channel(all_ctx, llid, &is_blkd, __FUNCTION__);
    if (cidx)
      {
      rank = blkd_get_rank((void *)all_ctx,traf->llid_traf,slan,&snum,&stidx);
      if (strcmp(slan, lan))
        KERR("%s %s", lan, slan);
      if ((num != snum) || (tidx != stidx))
        KERR("%s %d %d %d %d", lan, num, snum, tidx, stidx);
      snprintf(resp, MAX_PATH_LEN-1, 
               "cloonix_resp_connect_ok lan=%s name=%s num=%d tidx=%d rank=%d",     
               lan, all_ctx->g_name, num, tidx, rank);
      db_rpct_send_diag_msg(all_ctx, llid, traf->con_tid, resp);
      }
    else
      KERR("%s %s", all_ctx->g_name, lan);
    traf->timeout_mulan_id += 1;
    traf->con_llid = 0;
    traf->con_tid = 0;
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int lan_diag(t_all_ctx *all_ctx, char *line, 
                    int *num, int *tidx, char *resp_lan)
{
  int is_blkd, result = 0;
  char err[MAX_ERR_LEN];
  char lan[MAX_NAME_LEN];
  char name[MAX_NAME_LEN];
  char sock[MAX_PATH_LEN];
  char opening[MAX_PATH_LEN];
  uint32_t rank;
  t_blkd *blkd;
  t_traf_endp *traf;
  memset(err, 0, MAX_ERR_LEN);
  if (sscanf(line, 
             "mulan_req_start lan=%s name=%s num=%d tidx=%d rank=%d",
             lan, name, num, tidx, &rank) == 5)
    {
    if (strcmp(all_ctx->g_name, name))
      KOUT("%s %s", all_ctx->g_name, name);
    resp_send_mueth_con_ok(all_ctx, lan, *num, *tidx);
    }
  else if (sscanf(line, 
           "mulan_req_end lan=%s name=%s num=%d tidx=%d rank=%d",
           lan, name, num, tidx, &rank) == 5)
    {
    if (strcmp(all_ctx->g_name, name))
      KOUT("%s %s", all_ctx->g_name, name);
    sock_fd_finish(all_ctx, *tidx);
    }
  else if (sscanf(line, 
           "muend_resp_handshake_ok lan=%s name=%s num=%d tidx=%d",
           lan, name, num, tidx) == 4)
    {
    traf = &(all_ctx->g_traf_endp[*tidx]);
    if (strcmp(all_ctx->g_name, name))
      KOUT("%s %s", all_ctx->g_name, name);
    if (!msg_exist_channel(all_ctx, traf->llid_lan, &is_blkd, __FUNCTION__))
      KOUT("%s %s %d %d", lan, name, *num, traf->llid_lan);
    if (strcmp(traf->con_name, lan))
      KOUT("%s %s", traf->con_name, lan);
    snprintf(resp_lan, MAX_RESP_LEN-1, 
             "muend_req_rank lan=%s name=%s num=%d tidx=%d",
             lan, name, *num, *tidx);
    }
  else if (sscanf(line, 
           "muend_resp_rank_alloc_ko lan=%s name=%s num=%d tidx=%d",
           lan, name, num, tidx) == 4)
    {
    if (strcmp(all_ctx->g_name, name))
      KOUT("%s %s", all_ctx->g_name, name);
    sock_fd_finish(all_ctx, *tidx);
    KERR("%s", line);
    }
  else if (sscanf(line, 
     "muend_resp_rank_alloc_ok lan=%s name=%s num=%d tidx=%d rank=%d traf=%s",
     lan, name, num, tidx, &rank, sock) == 6)
    {
    if (strcmp(all_ctx->g_name, name))
      KOUT("%s %s", all_ctx->g_name, name);
    if (all_ctx->g_num != *num)
      KOUT("%s %d %d", name, all_ctx->g_num, *num);
    if (sock_fd_open(all_ctx, lan, *tidx, sock))
      {
      sock_fd_finish(all_ctx, *tidx);
      KERR("%s", line);
      }
    else
      {
      traf = &(all_ctx->g_traf_endp[*tidx]);
      blkd_set_rank((void *)all_ctx,traf->llid_traf,(int)rank,lan,*num,*tidx);
      memset(opening, 0, MAX_PATH_LEN);
      snprintf(opening, MAX_PATH_LEN-1,"TRAFFIC_OPENING: name=%s num=%d tidx=%d",
               name, *num, *tidx);
      blkd = blkd_create_tx_full_copy(strlen(opening) + 1, opening, 0, 0, 0);
      blkd_put_tx((void *) all_ctx, 1, &(traf->llid_traf), blkd);
      snprintf(resp_lan, MAX_RESP_LEN-1, 
               "muend_ack_rank lan=%s name=%s num=%d tidx=%d rank=%d", 
               lan, all_ctx->g_name, *num, *tidx, rank); 
      }
    }
  else
    result = -1;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_evt_msg(void *ptr, int llid, int tid, char *line)
{
  int llid_traf, rank, stop;
  DOUT(ptr, FLAG_HOP_EVT, "%s", line);
  if (sscanf(line,
           "cloonix_evt_peer_flow_control_tx rank=%d stop=%d",
           &rank, &stop) == 2)
    {
    llid_traf = blkd_get_llid_with_rank(ptr, rank);
    if (!llid_traf)
      KERR("%s", line);
    else
      blkd_tx_local_flow_control(ptr, llid_traf, stop);
    }
  else
    KERR("%s", line);
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
static void send_req_handshake_to_mulan(t_all_ctx *all_ctx, 
                                        char *lan, char *name, 
                                        int num, int tidx)
{
  char msg[MAX_PATH_LEN];
  memset(msg, 0, MAX_PATH_LEN);
  snprintf(msg, MAX_PATH_LEN-1, 
           "muend_req_handshake lan=%s name=%s num=%d tidx=%d",
           lan, name, num, tidx);
  db_rpct_send_diag_msg(all_ctx, all_ctx->g_traf_endp[tidx].llid_lan, 0, msg);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_hop_sub(void *ptr, int llid, int tid, int flags_hop)
{
  t_all_ctx *all_ctx = (t_all_ctx *) ptr;
  rpct_hop_print_add_sub(ptr, llid, tid, flags_hop);
  DOUT(ptr, FLAG_HOP_DIAG, "Hello from %s",  all_ctx->g_name);
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
  rpct_send_pid_resp(ptr, llid, tid, name, num, cloonix_get_pid(), getpid());
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void fct_timeout_mulan(t_all_ctx *all_ctx, void *data)
{
  t_timer_data *td = (t_timer_data *) data;
  int id, tidx;
  t_traf_endp *traf;
  if (td == NULL)
    KOUT(" ");
  tidx = td->tidx;
  id = td->timeout_mulan_id;
  if ((tidx < 0) || (tidx > MAX_TRAF_ENDPOINT))
    KOUT("%d", tidx);
  traf = &(all_ctx->g_traf_endp[tidx]);
  if (id == traf->timeout_mulan_id)
    {
    resp_send_mueth_con_ko(all_ctx, td->lan, td->num, td->tidx);
    sock_fd_finish(all_ctx, td->tidx);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void rx_cb (t_all_ctx *all_ctx, int llid, int len, char *buf)
{
  if (rpct_decoder(all_ctx, llid, len, buf))
    KOUT("%s", buf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void err_cb (void *ptr, int llid, int err, int from)
{
  t_all_ctx *all_ctx = (t_all_ctx *) ptr;
  int i, is_blkd, tidx = -1;
  if (msg_exist_channel(all_ctx, llid, &is_blkd, __FUNCTION__))
    msg_delete_channel(all_ctx, llid);
  for (i=0; i<MAX_TRAF_ENDPOINT; i++)
    {
    if (llid == all_ctx->g_traf_endp[i].llid_lan)
      {
      tidx = i;
      break;
      }
    }
  if (tidx != -1)
    all_ctx->g_traf_endp[tidx].llid_lan = 0;
  else
    KERR(" ");
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
static int check_and_set_uid(void)
{
  int result = -1;
  uid_t uid;
  seteuid(0);
  uid = geteuid();
  if (uid == 0)
    {
    result = 0;
    umask (0000);
    if (setuid(0))
      KOUT(" ");
    if (setgid(0))
      KOUT(" ");
    }
  return result;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
static void connect_req_process(void *ptr, int llid, int tid, 
                               char *resp_cloonix, char *sock, char *lan, 
                               char *name, int num, int tidx)
{
  t_all_ctx *all_ctx = (t_all_ctx *) ptr;
  int is_blkd;
  t_traf_endp *traf = &(all_ctx->g_traf_endp[tidx]);
  t_timer_data *td;
  if (msg_exist_channel(all_ctx, traf->llid_lan, &is_blkd, __FUNCTION__))
    {
    snprintf(resp_cloonix, MAX_PATH_LEN-1,
             "cloonix_resp_connect_ko lan=%s name=%s num=%d tidx=%d",
             lan, name, num, tidx);
    KERR("%s %s", name, lan);
    }
  else
    {
    traf->llid_lan = string_client_unix(all_ctx, sock, err_cb, rx_cb);
    if (traf->llid_lan == 0)
      {
      snprintf(resp_cloonix, MAX_PATH_LEN-1,
               "cloonix_resp_connect_ko lan=%s name=%s num=%d tidx=%d",
               lan, name, num, tidx);
      KERR("%s", sock);
      }
    else
      {
      td = (t_timer_data *) malloc(sizeof(t_timer_data));
      memset(td, 0, sizeof(t_timer_data));
      td->tidx = tidx;
      td->num = num;
      strncpy(td->lan, lan, MAX_NAME_LEN-1);
      td->timeout_mulan_id = traf->timeout_mulan_id;
      clownix_timeout_add(all_ctx, 400, fct_timeout_mulan,
                          (void *)td, NULL, NULL);
      strncpy(traf->con_name, lan, MAX_NAME_LEN-1);
      traf->con_llid = llid;
      traf->con_tid = tid;
      send_req_handshake_to_mulan(all_ctx, lan, name, num, tidx);
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_diag_msg(void *ptr, int llid, int tid, char *line)
{
  t_all_ctx *all_ctx = (t_all_ctx *) ptr;
  char resp_cloonix[MAX_PATH_LEN];
  char resp_lan[MAX_PATH_LEN];
  char sock[MAX_PATH_LEN];
  char lan[MAX_NAME_LEN];
  char name[MAX_NAME_LEN];
  int tidx, num=-1, cloonix_llid;

  memset(resp_cloonix, 0, MAX_PATH_LEN);
  memset(resp_lan, 0, MAX_PATH_LEN);

  DOUT(ptr, FLAG_HOP_DIAG, "%s", line);
/*---------------------------------------------------------------------------*/
  if (!strcmp(line, "cloonix_req_quit"))
    {
    exit(0);
    }
/*---------------------------------------------------------------------------*/
  else if (sscanf(line,
           "cloonix_req_connect sock=%s lan=%s name=%s num=%d tidx=%d",
           sock, lan, name, &num, &tidx) == 5)
    {
    if (strcmp(all_ctx->g_name, name))
      KOUT("%s %s", all_ctx->g_name, name);
    connect_req_process(ptr, llid, tid, resp_cloonix, sock, lan, 
                        name, num, tidx);
    }
/*---------------------------------------------------------------------------*/
  else if (sscanf(line, 
           "cloonix_req_disconnect lan=%s name=%s num=%d tidx=%d",
           lan, name, &num, &tidx) == 4)
    {
    if (strcmp(all_ctx->g_name, name))
      KOUT("%s %s", all_ctx->g_name, name);
    snprintf(resp_cloonix, MAX_PATH_LEN-1, 
             "cloonix_resp_disconnect_ok lan=%s name=%s num=%d tidx=%d",     
             lan, name, num, tidx);
    sock_fd_finish(all_ctx, tidx);
    }
  else if (!mycmp(line, "cloonix_req_suidroot"))
    {
    if (check_and_set_uid())
      snprintf(resp_cloonix, MAX_PATH_LEN-1, "cloonix_resp_suidroot_ko");
    else
      snprintf(resp_cloonix, MAX_PATH_LEN-1, "cloonix_resp_suidroot_ok");
    }
  else if (!mycmp(line, "cloonix_req_tap"))
    {
    if (tap_fd_open(all_ctx, all_ctx->g_name))
      {
      snprintf(resp_cloonix, MAX_PATH_LEN-1, "cloonix_resp_tap_ko");
      KERR(" ");
      }
    else
      snprintf(resp_cloonix, MAX_PATH_LEN-1, "cloonix_resp_tap_ok");
    }
  else if (!mycmp(line, "cloonix_req_wif"))
    {
    if (wif_fd_open(all_ctx, all_ctx->g_name))
      {
      snprintf(resp_cloonix, MAX_PATH_LEN-1, "cloonix_resp_wif_ko");
      KERR(" ");
      }
    else
      snprintf(resp_cloonix, MAX_PATH_LEN-1, "cloonix_resp_wif_ok");
    }
  else if (!mycmp(line, "cloonix_req_phy"))
    {
    if (raw_fd_open(all_ctx, all_ctx->g_name))
      {
      snprintf(resp_cloonix, MAX_PATH_LEN-1, "cloonix_resp_phy_ko");
      KERR(" ");
      }
    else
      snprintf(resp_cloonix, MAX_PATH_LEN-1, "cloonix_resp_phy_ok");
    }
  else if (!mycmp(line, "cloonix_req_snf"))
    {
    snprintf(resp_cloonix, MAX_PATH_LEN-1, "cloonix_resp_snf_ok");
    }
  else if (!strcmp(line, "cloonix_req_c2c"))
    {
    snprintf(resp_cloonix, MAX_PATH_LEN-1, "cloonix_resp_c2c_ok");
    }
  else if (!strcmp(line, "cloonix_req_a2b"))
    {
    snprintf(resp_cloonix, MAX_PATH_LEN-1, "cloonix_resp_a2b_ok");
    }
  else if (!strcmp(line, "cloonix_req_nat"))
    {
    snprintf(resp_cloonix, MAX_PATH_LEN-1, "cloonix_resp_nat_ok");
    }
  else if (!strcmp(line, "cloonix_req_kvm_sock"))
    {
    snprintf(resp_cloonix, MAX_PATH_LEN-1, "cloonix_resp_kvm_sock_ok");
    }
  else if (lan_diag(all_ctx, line, &num, &tidx, resp_lan))
    KERR("%s", line);
/*---------------------------------------------------------------------------*/
  if (resp_cloonix[0])
    {
    cloonix_llid = blkd_get_cloonix_llid((void *) all_ctx);
    if (!cloonix_llid)
      KOUT(" ");
    if (cloonix_llid != llid)
      KOUT("%d %d", cloonix_llid, llid);
    db_rpct_send_diag_msg(all_ctx, llid, 0, resp_cloonix);
    }
  if (resp_lan[0])
    {
    if (!all_ctx->g_traf_endp[tidx].llid_lan)
      KOUT(" ");
    if (all_ctx->g_traf_endp[tidx].llid_lan != llid)
      KOUT("%d %d", all_ctx->g_traf_endp[tidx].llid_lan, llid);
    db_rpct_send_diag_msg(all_ctx, llid, 0, resp_lan);
    }
}
/*---------------------------------------------------------------------------*/



