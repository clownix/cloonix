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
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <math.h>
#include <execinfo.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>


#include "io_clownix.h"
#include "rpc_clownix.h"
#include "doors_rpc.h"
#include "util_sock.h"
#include "sock.h"
#include "llid_traffic.h"
#include "llid_x11.h"
#include "llid_backdoor.h"
#include "dispach.h"
#include "doorways_sock.h"


enum {
  auto_state_idle = 0,
  auto_state_wait_agent_link,
  auto_state_complete_link_ok,
};

#define MAX_AUTO_RESP 500

typedef struct t_llid_traf
{
  int dido_llid;
  int is_associated;
  char name[MAX_NAME_LEN];
  int  backdoor_llid; 
  char auto_resp[MAX_AUTO_RESP];
  int  auto_must_send_resp;
  int  auto_state;
  int  auto_timer_ref;
//  int  zero_queue_series;
  long long auto_timer_abs;
  char *xauth_cookie_format;
  int display_port; 
  int display_sock_x11; 
} t_llid_traf;

static t_llid_traf *g_llid_traf[CLOWNIX_MAX_CHANNELS];
static void traf_shutdown(int dido_llid, int line);
static char g_buf[MAX_A2D_LEN];
/*--------------------------------------------------------------------------*/


/*****************************************************************************/
/*
static void trace_read_write(char *act, int len, char *buf)
{
  KERR("%s: %d: %02X %02X %02X %02X %02X %02X %02X %02X   %02X %02X %02X %02X  %02X %02X %02X %02X", act, len,
  buf[0] & 0xFF, buf[1] & 0xFF, buf[2] & 0xFF, buf[3] & 0xFF,
  buf[4] & 0xFF, buf[5] & 0xFF, buf[6] & 0xFF, buf[7] & 0xFF,
  buf[0+8] & 0xFF, buf[1+8] & 0xFF, buf[2+8] & 0xFF, buf[3+8] & 0xFF,
  buf[4+8] & 0xFF, buf[5+8] & 0xFF, buf[6+8] & 0xFF, buf[7+8] & 0xFF);
}
*/
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static char *get_cloonix_config_path(void)
{
  return ("/mnt/cloonix_config_fs/config");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int in_cloonix_file_exists(void)
{
  int err, result = 0;
  err = access(get_cloonix_config_path(), R_OK);
  if (!err)
    result = 1;
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void cookie_info_store(t_llid_traf *lt, char *cookie)
{
  int len, C0, C1;
  char c0, c1;
  len = strlen(cookie) + 1;
  lt->xauth_cookie_format = (char *) clownix_malloc(len-4, 9);
  memcpy(lt->xauth_cookie_format, cookie+4, len);
  if ((sscanf(&(cookie[0]), "%02X", &C0) == 1) &&
      (sscanf(&(cookie[2]), "%02X", &C1) == 1))
    {
    c0 = (char) (C0 & 0xFF);
    c1 = (char) (C1 & 0xFF);
    lt->display_port = ((c0 & 0xFF) << 8) + (c1 & 0xFF);
    }
  else
    lt->display_port = 0;
}
/*--------------------------------------------------------------------------*/



/****************************************************************************/
static void fill_200_char_resp(char *buf, char *start, 
                               char *name, int display_sock_x11) 
{
  char empyness[200];
  memset(empyness, ' ', 200);
  snprintf(buf, MAX_AUTO_RESP, "%s DBSSH_CLI_DOORS_RESP name=%s display_sock_x11=%d%s", 
           start, name, display_sock_x11, empyness);
  buf[199] = 0;
  if (strlen(buf) != 199)
    KOUT("ERROR %d ", (int) strlen(buf));
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
char *get_gbuf(void)
{
  return g_buf;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_llid_traf *llid_traf_get(int dido_llid)
{
  t_llid_traf *lt = g_llid_traf[dido_llid];
  return lt;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void send_to_traf_client(int dido_llid, int val, int len, char *buf)
{
  if (dispach_send_to_traf_client(dido_llid, val, len, buf))
    {
    KERR("ERROR %d %d %d", dido_llid, len, val);
    traf_shutdown(dido_llid, __LINE__);
    }
  else
    {
    if (val != doors_val_none)
      DOUT(FLAG_HOP_DOORS, "CLIENT_TX: %d %s", dido_llid, buf);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void clean_auto_timer(t_llid_traf *lt)
{
  if (lt->auto_timer_abs)
    clownix_timeout_del(lt->auto_timer_abs, lt->auto_timer_ref,
                        __FILE__, __LINE__);
  lt->auto_timer_abs = 0;
  lt->auto_timer_ref = 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_close_dido_llid(void *data)
{
  t_llid_traf *lt;
  unsigned long ul_llid = (unsigned long) data;
  int dido_llid = (int)ul_llid;
  doorways_clean_llid(dido_llid);
  lt = llid_traf_get(dido_llid);
  if (lt == NULL)
    KERR("ERROR %d", dido_llid);
  else
    {
    g_llid_traf[dido_llid] = NULL;
    clownix_free(lt, __FUNCTION__);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void differed_dido_llid_end(int dido_llid)
{
  unsigned long ul_llid = (unsigned long) dido_llid;
  clownix_timeout_add(2,timer_close_dido_llid,(void *)ul_llid,NULL,NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void traf_shutdown(int dido_llid, int line)
{
  t_llid_traf *lt = llid_traf_get(dido_llid);
  int  headsize = sock_header_get_size();
  if (lt)
    {
    differed_dido_llid_end(lt->dido_llid);
    if (lt->backdoor_llid)
      x11_doors_close(lt->backdoor_llid, lt->dido_llid);
    if (lt->is_associated)
      {
      if (lt->backdoor_llid)
        {
        memset(g_buf, 0, MAX_A2D_LEN);
        strcpy(g_buf+headsize, LABREAK);
        llid_backdoor_tx(lt->name, lt->backdoor_llid, lt->dido_llid, 
                         strlen(LABREAK) + 1, header_type_ctrl_agent, 
                         header_val_del_dido_llid, g_buf);
        DOUT(FLAG_HOP_DOORS, "BACKDOOR_TX: %d %s", dido_llid, g_buf+headsize);
        }
      llid_backdoor_del_traf(lt->name, lt->backdoor_llid, dido_llid);
      }
    clean_auto_timer(lt);
    if (lt->auto_must_send_resp)
      {
      send_to_traf_client(lt->dido_llid, doors_val_link_ko,
                          strlen(lt->auto_resp) + 1, lt->auto_resp);
      }
    }
  else
    KERR("ERROR NO RECORD %d %d", dido_llid, line);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_auto(void *data)
{
  unsigned long ul_llid = (unsigned long) data;
  t_llid_traf *lt = llid_traf_get((int) ul_llid);
  if (lt)
    {
    lt->auto_timer_abs = 0;
    lt->auto_timer_ref = 0;
    traf_shutdown(lt->dido_llid, __LINE__);
    }
  else
    KERR("ERROR NO RECORD %d", (int) ul_llid);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void arm_auto_timer_with_resp(t_llid_traf *lt, char *resp, char *name,
                                     int val)
{
  unsigned long ul_llid = (unsigned long) (lt->dido_llid);
  clean_auto_timer(lt);
  fill_200_char_resp(lt->auto_resp, resp, name, 0);
  clownix_timeout_add(val, timer_auto, (void *) ul_llid,
                      &(lt->auto_timer_abs), &(lt->auto_timer_ref));
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void llid_traf_alloc(int dido_llid)
{
  t_llid_traf *cur;
  if (dido_llid <= 0)
    KOUT("ERROR  ");
  if (dido_llid >= CLOWNIX_MAX_CHANNELS)
    KOUT("ERROR  ");
  if (llid_traf_get(dido_llid))
    KOUT("ERROR  ");
  cur = (t_llid_traf *) clownix_malloc(sizeof(t_llid_traf), 9);
  memset(cur, 0, sizeof(t_llid_traf));
  g_llid_traf[dido_llid] = cur; 
  cur->dido_llid = dido_llid;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void llid_traf_associate(t_llid_traf *llid_traf, char *name, 
                                int llid_backdoor, int dido_llid)
{
  llid_traf->is_associated = 1;
  strncpy(llid_traf->name, name, MAX_NAME_LEN - 1);
  llid_traf->backdoor_llid = llid_backdoor; 
  llid_backdoor_add_traf(name, llid_backdoor, dido_llid);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int link_associate(t_llid_traf *lt, char *cookie, 
                          char *name, int llid_backdoor)
{
  int in_idx_x11;
  cookie_info_store(lt, cookie);
  if (in_cloonix_file_exists())
    in_idx_x11 = lt->display_port - 6000;
  else
    in_idx_x11 = 0;
  llid_traf_associate(lt, name, llid_backdoor, lt->dido_llid);
  lt->auto_state = auto_state_wait_agent_link;
  arm_auto_timer_with_resp(lt, "KO link_associate timeout", name, 3000);
  return in_idx_x11;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int start_link2agent_auto(t_llid_traf *lt, int len, char *data)
{
  int res, llid_backdoor, in_idx_x11, rlen, result = -1;
  int  headsize = sock_header_get_size();
  char *name;
  char *ptrs, *ptre, *cookie;
  memset(lt->auto_resp, 0, 2*MAX_NAME_LEN);
  lt->auto_must_send_resp = 1;
  ptrs = strstr(data, "DBSSH_CLI_DOORS_REQ name=");
  if (!ptrs)
    {
    fill_200_char_resp(lt->auto_resp, "KO err1", "noname", 0);
    }
  else
    {
    ptrs = strstr(data, "name=");
    ptrs += strlen("name="); 
    ptre = strstr(ptrs, " cookie="); 
    if (!ptre)
      {
      KERR("%s", data);
      fill_200_char_resp(lt->auto_resp, "KO err2", "noname", 0);
      }
    else
      {
      *ptre = 0;
      name = ptrs;
      cookie = ptre + strlen(" cookie=");
      res = llid_backdoor_get_info(name, &llid_backdoor);
      if (res)
        {
        if (res == -2)
          fill_200_char_resp(lt->auto_resp,"KO_bad_connection",name,0);
        else
          fill_200_char_resp(lt->auto_resp, "KO_name_not_found",name,0);
        }
      else
        {
        if (llid_backdoor_ping_status_is_ok(name))
          {
          in_idx_x11 = link_associate(lt, cookie, name, llid_backdoor);
          memset(g_buf, 0, MAX_A2D_LEN);
          rlen = snprintf(g_buf+headsize,MAX_A2D_LEN-1, DBSSH_SERV_DOORS_REQ, 
                          in_idx_x11, lt->xauth_cookie_format);
          llid_backdoor_tx(lt->name, lt->backdoor_llid, lt->dido_llid, 
                           rlen + 1, header_type_ctrl_agent, 
                           header_val_add_dido_llid, g_buf);
          DOUT(FLAG_HOP_DOORS, "BACKDOOR_TX: %d %s",  
                                     lt->dido_llid, g_buf+headsize);
          result = 0;
          }
        else
          fill_200_char_resp(lt->auto_resp,"KO_ping_to_agent",name,0);
        }
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void send_resp_ok_to_traf_client(int dido_llid, int display_sock_x11) 
{
  t_llid_traf *lt = llid_traf_get(dido_llid);
  if (lt)
    {
    lt->display_sock_x11 = display_sock_x11;
    lt->auto_state = auto_state_complete_link_ok;
    clean_auto_timer(lt);
    fill_200_char_resp(lt->auto_resp, "OK", lt->name, lt->display_sock_x11); 
    lt->auto_must_send_resp = 0;
    send_to_traf_client(lt->dido_llid, doors_val_link_ok,
                        strlen(lt->auto_resp)+1,lt->auto_resp);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void llid_traf_tx_to_client(char *name, int dido_llid,
                            int len, int type, int val,
                            char  *buf)
{
  t_llid_traf *lt = llid_traf_get(dido_llid);
  int display_sock_x11;
  if (lt)
    {
    if (strcmp(lt->name, name))
      KOUT("ERROR %s %s", lt->name, name);
    if ((lt->auto_state == auto_state_complete_link_ok) &&
       (type == header_type_traffic) && (val == header_val_none))
      {
      send_to_traf_client(dido_llid, doors_val_none, len, buf);
      }
    else if (type == header_type_ctrl_agent)
      {
      if (lt->auto_state == auto_state_wait_agent_link) 
        {
        if (val == header_val_add_dido_llid) 
          {
          if (sscanf(buf, DBSSH_SERV_DOORS_RESP, &display_sock_x11) == 1)
            send_resp_ok_to_traf_client(lt->dido_llid, display_sock_x11); 
          else
            {
            KERR("ERROR %s", buf);
            traf_shutdown(dido_llid, __LINE__);
            }
          }
        else
          KOUT("ERROR %d %d", type, val);
        }
      if (val == header_val_del_dido_llid)
        {
        if (doorways_tx_or_rx_still_in_queue(dido_llid))
          KERR("WARNING %d", dido_llid);
        else  
          traf_shutdown(dido_llid, __LINE__);
        }
      }
    else
      KERR("ERROR %d", lt->auto_state);
    }
  else
    KERR("ERROR NO RECORD %d %d %d", dido_llid, type, val);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int llid_traf_get_display_port(int dido_llid) 
{
  int result = 0;
  t_llid_traf *lt = llid_traf_get(dido_llid);
  if (lt)
    result = lt->display_port;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void llid_traf_delete(int dido_llid)
{
  traf_shutdown(dido_llid, __LINE__);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void llid_traf_rx_from_client(int dido_llid, int len, char *buf)
{
  int  headsize = sock_header_get_size();
  int len_max = MAX_A2D_LEN - headsize;
  int len_done, len_to_do, chosen_len;
  t_llid_traf *lt;
  lt = llid_traf_get(dido_llid);
  if (!lt)
    {
    DOUT(FLAG_HOP_DOORS, "CLIENT_RX: %d %s", dido_llid, buf);
    llid_traf_alloc(dido_llid);
    lt = llid_traf_get(dido_llid);
    if (!lt)
      KOUT("ERROR  ");
    if (len > len_max)
      KOUT("ERROR %d %d", len, len_max);
    if (start_link2agent_auto(lt, len, buf))
      {
      traf_shutdown(lt->dido_llid, __LINE__);
      }
    }
  else if (lt->is_associated)
    {
    len_to_do = len;
    len_done = 0;
    while (len_to_do)
      {
      if (len_to_do >= len_max)
        chosen_len = len_max;
      else
        chosen_len = len_to_do;
      memcpy(g_buf+headsize, buf+len_done, chosen_len);
      if (lt->backdoor_llid)
        {
        llid_backdoor_tx(lt->name, lt->backdoor_llid, lt->dido_llid, 
                         chosen_len, header_type_traffic,
                         header_val_none, g_buf);
        }
      else
        KOUT("ERROR  ");
      len_done += chosen_len;
      len_to_do -= chosen_len;
      }
    }
  else
    KOUT("ERROR  ");
}
/*--------------------------------------------------------------------------*/

