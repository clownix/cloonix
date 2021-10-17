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

enum
  {
  bnd_rpct_min = 0,
  bnd_rpct_sigdiag_msg,
  bnd_rpct_poldiag_msg,
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
#define MUSIGDIAG_MSG_O  "<sigdiag_msg>\n"\
                         "  <tid> %d </tid>\n"

#define MUSIGDIAG_MSG_I "<sigdiag_msg_delimiter>%s</sigdiag_msg_delimiter>\n"

#define MUSIGDIAG_MSG_C  "</sigdiag_msg>"
/*---------------------------------------------------------------------------*/
#define MUPOLDIAG_MSG_O  "<poldiag_msg>\n"\
                         "  <tid> %d </tid>\n"

#define MUPOLDIAG_MSG_I "<poldiag_msg_delimiter>%s</poldiag_msg_delimiter>\n"

#define MUPOLDIAG_MSG_C  "</poldiag_msg>"
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
void rpct_send_sigdiag_msg(int llid, int tid, char *line);
void rpct_send_poldiag_msg(int llid, int tid, char *line);
void rpct_recv_sigdiag_msg(int llid, int tid, char *line);
void rpct_recv_poldiag_msg(int llid, int tid, char *line);
/*---------------------------------------------------------------------------*/
void rpct_send_pid_req(int llid, int tid, char *name, int num);
void rpct_recv_pid_req(int llid, int tid, char *name, int num);
/*---------------------------------------------------------------------------*/
void rpct_send_kil_req(int llid, int tid);
void rpct_recv_kil_req(int llid, int tid);
/*---------------------------------------------------------------------------*/
void rpct_send_pid_resp(int llid, int tid,
                        char *name, int num, int toppid, int pid);
void rpct_recv_pid_resp(int llid, int tid,
                        char *name, int num, int toppid, int pid);
/*---------------------------------------------------------------------------*/
void rpct_send_hop_sub(int llid, int tid, int flags_hop);
void rpct_recv_hop_sub(int llid, int tid, int flags_hop);
/*---------------------------------------------------------------------------*/
void rpct_send_hop_unsub(int llid, int tid);
void rpct_recv_hop_unsub(int llid, int tid);
/*---------------------------------------------------------------------------*/
void rpct_send_hop_msg(int llid, int tid,
                      int flags_hop, char *txt);
void rpct_recv_hop_msg(int llid, int tid,
                      int flags_hop, char *txt);
/*---------------------------------------------------------------------------*/
void rpct_hop_print_add_sub(int llid, int tid, int flags_hop);
void rpct_hop_print_del_sub(int llid);
void rpct_hop_print(int flags_hop, const char * format, ...);
/*---------------------------------------------------------------------------*/
typedef void (*t_rpct_tx)(int llid, int len, char *buf);
int  rpct_decoder(int llid, int len, char *str_rx);
void rpct_redirect_string_tx(t_rpct_tx rpc_tx);
void rpct_init(t_rpct_tx rpc_tx);
/****************************************************************************/

