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

enum
  {
  bnd_rpct_min = 0,
  bnd_rpct_blkd_item_sub,
  bnd_rpct_blkd_item,
  bnd_rpct_evt_msg,
  bnd_rpct_diag_msg,
  bnd_rpct_app_msg,
  bnd_rpct_cli_req,
  bnd_rpct_cli_resp,
  bnd_rpct_kil_req,
  bnd_rpct_pid_req,
  bnd_rpct_pid_resp,
  bnd_rpct_hop_evt_sub,
  bnd_rpct_hop_evt_unsub,
  bnd_rpct_hop_evt_msg,
  bnd_rpct_max,
  };

/*---------------------------------------------------------------------------*/
#define BLKD_ITEM_SUB  "<blkd_item_sub>\n"\
                       " <sub> %d </sub>\n"\
                       "</blkd_item_sub>"

#define BLKD_ITEM      "<blkd_item>\n"\
                       "  sock:%s \n"\
                       "  rank_name:%s rank_num:%d rank_tidx:%d \n"\
                       "  rank:%d \n"\
                       "  pid:%d \n"\
                       "  llid:%d \n"\
                       "  fd:%d \n"\
                       "  sel_tx:%d \n"\
                       "  sel_rx:%d \n"\
                       "  fifo_tx:%d \n"\
                       "  fifo_rx:%d \n"\
                       "  queue_tx:%d \n"\
                       "  queue_rx:%d \n"\
                       "  bandwidth_tx:%d \n"\
                       "  bandwidth_rx:%d \n"\
                       "  stop_tx:%d \n"\
                       "  stop_rx:%d \n"\
                       "  flow_ctrl_tx:%d \n"\
                       "  flow_ctrl_rx:%d \n"\
                       "  drop_tx:%lld \n"\
                       "  drop_rx:%lld \n"\
                       "</blkd_item>"


/*---------------------------------------------------------------------------*/
#define MUEVT_MSG_O  "<evt_msg>\n"\
                     "  <tid> %d </tid>\n"

#define MUEVT_MSG_I "<evt_msg_delimiter>%s</evt_msg_delimiter>\n"

#define MUEVT_MSG_C  "</evt_msg>"
/*---------------------------------------------------------------------------*/
#define MUDIAG_MSG_O  "<diag_msg>\n"\
                     "  <tid> %d </tid>\n"

#define MUDIAG_MSG_I "<diag_msg_delimiter>%s</diag_msg_delimiter>\n"

#define MUDIAG_MSG_C  "</diag_msg>"
/*---------------------------------------------------------------------------*/
#define MUAPP_MSG_O  "<app_msg>\n"\
                     "  <tid> %d </tid>\n"

#define MUAPP_MSG_I "<app_msg_delimiter>%s</app_msg_delimiter>\n"

#define MUAPP_MSG_C  "</app_msg>"
/*---------------------------------------------------------------------------*/
#define MUCLI_REQ_O  "<mucli_req>\n"\
                     "  <tid> %d </tid>\n"\
                     "  <cli_llid> %d </cli_llid>\n"\
                     "  <cli_tid> %d </cli_tid>\n"

#define MUCLI_REQ_I  "  <mucli_req_bound>%s</mucli_req_bound>\n"

#define MUCLI_REQ_C  "</mucli_req>"
/*---------------------------------------------------------------------------*/
#define MUCLI_RESP_O  "<mucli_resp>\n"\
                      "  <tid> %d </tid>\n"\
                      "  <cli_llid> %d </cli_llid>\n"\
                      "  <cli_tid> %d </cli_tid>\n"

#define MUCLI_RESP_I  "  <mucli_resp_bound>%s</mucli_resp_bound>\n"

#define MUCLI_RESP_C  "</mucli_resp>"
/*---------------------------------------------------------------------------*/
#define HOP_PID_REQ   "<hop_req_pid>\n"\
                       "  <tid> %d </tid>\n"\
                       "  name:%s num:%d \n"\
                       "</hop_req_pid>"
/*---------------------------------------------------------------------------*/
#define HOP_PID_RESP  "<hop_resp_pid>\n"\
                       "  <tid> %d </tid>\n"\
                       "  name:%s num:%d toppid:%d pid:%d \n"\
                       "</hop_resp_pid>"
/*---------------------------------------------------------------------------*/
#define HOP_KIL_REQ   "<hop_req_kil>\n"\
                       "  <tid> %d </tid>\n"\
                       "</hop_req_kil>"
/*---------------------------------------------------------------------------*/
#define HOP_EVT_O "<hop_event_txt>\n"\
                  "  <tid> %d </tid>\n"\
                  "  <flags_hop> %d </flags_hop>\n"

#define HOP_EVT_C "</hop_event_txt>"
/*---------------------------------------------------------------------------*/
#define HOP_FREE_TXT  "  <hop_free_txt_joker>%s</hop_free_txt_joker>\n"
/*---------------------------------------------------------------------------*/
#define HOP_EVT_SUB "<hop_evt_sub>\n"\
                    "  <tid> %d </tid>\n"\
                    "  <flags_hop> %d </flags_hop>\n"\
                    "</hop_evt_sub>"
/*---------------------------------------------------------------------------*/
#define HOP_EVT_UNSUB "<hop_evt_unsub>\n"\
                      "  <tid> %d </tid>\n"\
                      "</hop_evt_unsub>"

/*---------------------------------------------------------------------------*/
void rpct_send_app_msg(void *ptr, int llid, int tid, char *line);
void rpct_recv_app_msg(void *ptr, int llid, int tid, char *line);
void rpct_send_diag_msg(void *ptr, int llid, int tid, char *line);
void rpct_recv_diag_msg(void *ptr, int llid, int tid, char *line);
void rpct_send_evt_msg(void *ptr, int llid, int tid, char *line);
void rpct_recv_evt_msg(void *ptr, int llid, int tid, char *line);
/*---------------------------------------------------------------------------*/
void rpct_send_cli_req(void *ptr, int llid, int tid,
                    int cli_llid, int cli_tid, char *line);
void rpct_recv_cli_req(void *ptr, int llid, int tid,
                    int cli_llid, int cli_tid, char *line);
void rpct_send_cli_resp(void *ptr, int llid, int tid,
                     int cli_llid, int cli_tid, char *line);
void rpct_recv_cli_resp(void *ptr, int llid, int tid,
                     int cli_llid, int cli_tid, char *line);
/*---------------------------------------------------------------------------*/
void rpct_send_pid_req(void *ptr, int llid, int tid, char *name, int num);
void rpct_recv_pid_req(void *ptr, int llid, int tid, char *name, int num);
/*---------------------------------------------------------------------------*/
void rpct_send_kil_req(void *ptr, int llid, int tid);
void rpct_recv_kil_req(void *ptr, int llid, int tid);
/*---------------------------------------------------------------------------*/
void rpct_send_pid_resp(void *ptr, int llid, int tid,
                        char *name, int num, int toppid, int pid);
void rpct_recv_pid_resp(void *ptr, int llid, int tid,
                        char *name, int num, int toppid, int pid);
/*---------------------------------------------------------------------------*/
void rpct_send_hop_sub(void *ptr, int llid, int tid, int flags_hop);
void rpct_recv_hop_sub(void *ptr, int llid, int tid, int flags_hop);
/*---------------------------------------------------------------------------*/
void rpct_send_hop_unsub(void *ptr, int llid, int tid);
void rpct_recv_hop_unsub(void *ptr, int llid, int tid);
/*---------------------------------------------------------------------------*/
void rpct_send_hop_msg(void *ptr, int llid, int tid,
                      int flags_hop, char *txt);
void rpct_recv_hop_msg(void *ptr, int llid, int tid,
                      int flags_hop, char *txt);
/*---------------------------------------------------------------------------*/
void rpct_hop_print_add_sub(void *ptr, int llid, int tid, int flags_hop);
void rpct_hop_print_del_sub(void *ptr, int llid);
void rpct_hop_print(void *ptr, int flags_hop, const char * format, ...);
/*---------------------------------------------------------------------------*/
typedef void (*t_rpct_tx)(void *ptr, int llid, int len, char *buf);
void rpct_heartbeat(void *ptr);
int  rpct_decoder(void *ptr, int llid, int len, char *str_rx);
void rpct_send_report_sub(void *ptr, int llid, int sub);
void rpct_recv_report_sub(void *ptr, int llid, int sub);
void rpct_send_report(void *ptr, int llid, t_blkd_item *item);
void rpct_recv_report(void *ptr, int llid, t_blkd_item *item);
void rpct_clean_all(void *ptr);
void rpct_redirect_string_tx(void *ptr, t_rpct_tx rpc_tx);
void rpct_send_peer_flow_control(void *ptr, int llid,
                                 char *name, int num, int tidx,
                                 int rank, int stop);
void rpct_init(void *ptr, t_rpct_tx rpc_tx);
/****************************************************************************/

