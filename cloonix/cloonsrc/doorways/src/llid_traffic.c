/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <math.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "doors_rpc.h"
#include "util_sock.h"
#include "llid_traffic.h"
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
  int  del_pending;
  int  del_pending_queue;
  long long auto_timer_abs;
  int  end_timer_ref;
  long long end_timer_abs;
  int  backdoor_told;
  char *xauth_cookie_format;
  int display_port; 
  /* flow control of the leg, see pace() */
  int agent_xoff;
  int bd_full;
  int rx_paused;
  int told_xoff;
  int watched;
  int stall_q;
  long long stall_ms;
  int stall_said;
  struct t_llid_traf *wprev;
  struct t_llid_traf *wnext;
} t_llid_traf;

static t_llid_traf *g_llid_traf[CLOWNIX_MAX_CHANNELS];
static t_llid_traf *g_watch;
static int g_watch_timer;
/* Traffic from the agent for a session the client already left. */
static int g_late_traffic;
static int g_late_del;
static int g_late_open;
static void traf_shutdown(int dido_llid, int line);

static void cookie_info_store(t_llid_traf *lt, char *cookie)
{
  int len;
  unsigned int C0, C1;
  char c0, c1;
  len = strlen(cookie) + 1;
  lt->xauth_cookie_format = (char *) clownix_malloc(len-4, 9);
  memcpy(lt->xauth_cookie_format, cookie+4, len-4);
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

static void fill_200_char_resp(char *buf, char *start, 
                               char *name) 
{
  char empyness[200];
  memset(empyness, ' ', 200);
  snprintf(buf, MAX_AUTO_RESP, "%s DBSSH_CLI_DOORS_RESP name=%s%s", 
           start, name, empyness);
  buf[199] = 0;
  if (strlen(buf) != 199)
    KOUT("ERROR %d ", (int) strlen(buf));
}

static t_llid_traf *llid_traf_get(int dido_llid)
{
  if ((dido_llid <= 0) || (dido_llid >= CLOWNIX_MAX_CHANNELS))
    return NULL;
  return (g_llid_traf[dido_llid]);
}

/* Leg flow control: client unread while agent xoff or guest backdoor over
   msg_tx_queue_high; held 10 s without shrinking warns. */
#define STALL_MS 10000

static void watch_del(t_llid_traf *lt)
{
  if (!lt->watched)
    return;
  if (lt->wprev)
    lt->wprev->wnext = lt->wnext;
  if (lt->wnext)
    lt->wnext->wprev = lt->wprev;
  if (lt == g_watch)
    g_watch = lt->wnext;
  lt->wprev = lt->wnext = NULL;
  lt->watched = 0;
}

static void tell_agent_flow(t_llid_traf *lt, int val)
{
  char buf[MAX_A2D_LEN];
  int headsize = sock_header_get_size();
  char *txt = (val == header_val_xoff) ? "xoff" : "xon";
  if ((!lt->is_associated) || (!lt->backdoor_llid) || (lt->backdoor_told))
    return;
  strcpy(buf + headsize, txt);
  llid_backdoor_tx(lt->name, lt->backdoor_llid, lt->dido_llid,
                   strlen(txt) + 1, header_type_ctrl_agent, val, buf);
}

static int held_bytes(t_llid_traf *lt)
{
  int q = 0;
  if (msg_exist_channel(lt->dido_llid))
    q += (int) channel_get_tx_queue_len(lt->dido_llid);
  if (lt->backdoor_llid && msg_exist_channel(lt->backdoor_llid))
    q += (int) channel_get_tx_queue_len(lt->backdoor_llid);
  return q;
}

static void stall_check(t_llid_traf *lt)
{
  long long now = cloonix_get_msec();
  int q = held_bytes(lt);
  if ((!lt->stall_ms) || (q < lt->stall_q))
    {
    lt->stall_ms = now;
    lt->stall_said = 0;
    }
  lt->stall_q = q;
  if ((!lt->stall_said) && (q) && (now - lt->stall_ms >= STALL_MS))
    {
    lt->stall_said = 1;
    KERR("WARNING byte pipe %d of %s held %d s, %d bytes queued and not "
         "shrinking", lt->dido_llid, lt->name, STALL_MS / 1000, q);
    }
}

static void timer_watch(void *data);

static void pace(t_llid_traf *lt)
{
  int bd = (lt->backdoor_llid && msg_exist_channel(lt->backdoor_llid));
  int cl = msg_exist_channel(lt->dido_llid);
  int want;
  if ((!lt->bd_full) && bd && msg_tx_queue_high(lt->backdoor_llid))
    lt->bd_full = 1;
  else if ((lt->bd_full) && ((!bd) || msg_tx_queue_low(lt->backdoor_llid)))
    lt->bd_full = 0;
  want = (lt->bd_full || lt->agent_xoff);
  if (cl && (want != lt->rx_paused))
    {
    channel_rx_local_flow_ctrl(lt->dido_llid, want);
    lt->rx_paused = want;
    }
  if ((!lt->told_xoff) && cl && msg_tx_queue_high(lt->dido_llid))
    {
    tell_agent_flow(lt, header_val_xoff);
    lt->told_xoff = 1;
    }
  else if ((lt->told_xoff) && ((!cl) || msg_tx_queue_low(lt->dido_llid)))
    {
    tell_agent_flow(lt, header_val_xon);
    lt->told_xoff = 0;
    }
  if ((lt->rx_paused || lt->told_xoff) && (!lt->watched))
    {
    lt->watched = 1;
    lt->stall_ms = 0;
    lt->wnext = g_watch;
    if (g_watch)
      g_watch->wprev = lt;
    g_watch = lt;
    if (!g_watch_timer)
      {
      g_watch_timer = 1;
      clownix_timeout_add(2, timer_watch, NULL, NULL, NULL);
      }
    }
  else if ((!lt->rx_paused) && (!lt->told_xoff))
    watch_del(lt);
  if (lt->watched)
    stall_check(lt);
}

static void timer_watch(void *data)
{
  t_llid_traf *next, *cur = g_watch;
  while (cur)
    {
    next = cur->wnext;
    pace(cur);
    cur = next;
    }
  if (g_watch)
    clownix_timeout_add(2, timer_watch, NULL, NULL, NULL);
  else
    g_watch_timer = 0;
}

static void send_to_traf_client(int dido_llid, int val, int len, char *buf)
{
  if (dispach_send_to_traf_client(dido_llid, val, len, buf))
    {
    KERR("ERROR %d %d %d", dido_llid, len, val);
    traf_shutdown(dido_llid, __LINE__);
    }
}

static void clean_auto_timer(t_llid_traf *lt)
{
  if (lt->auto_timer_abs)
    clownix_timeout_del(lt->auto_timer_abs, lt->auto_timer_ref,
                        __FILE__, __LINE__);
  lt->auto_timer_abs = 0;
  lt->auto_timer_ref = 0;
}

static void timer_close_dido_llid(void *data)
{
  t_llid_traf *lt;
  unsigned long ul_llid = (unsigned long) data;
  int dido_llid = (int)ul_llid;
  lt = llid_traf_get(dido_llid);
  if (lt == NULL)
    KERR("ERROR %d", dido_llid);
  else
    {
    /* Clean the llid only while the record is ours: the end callback runs
       llid_traf_client_gone which frees the record, so look it up again after. */
    lt->end_timer_abs = 0;
    lt->end_timer_ref = 0;
    doorways_clean_llid(dido_llid);
    lt = llid_traf_get(dido_llid);
    if (lt)
      {
      watch_del(lt);
      g_llid_traf[dido_llid] = NULL;
      clownix_free(lt, __FUNCTION__);
      }
    }
}

static void differed_dido_llid_end(t_llid_traf *lt)
{
  unsigned long ul_llid = (unsigned long) lt->dido_llid;
  if (lt->end_timer_abs == 0)
    clownix_timeout_add(2, timer_close_dido_llid, (void *)ul_llid,
                        &(lt->end_timer_abs), &(lt->end_timer_ref));
}

/* Tells the agent the session is over and unlists it from the backdoor,
   once: the agent's end and the client's end can both ask for it. */
static void tell_backdoor_gone(t_llid_traf *lt)
{
  char buf[MAX_A2D_LEN];
  int  headsize = sock_header_get_size();
  if ((lt->is_associated) && (!lt->backdoor_told))
    {
    lt->backdoor_told = 1;
    if (lt->backdoor_llid)
      {
      strcpy(buf+headsize, LABREAK);
      llid_backdoor_tx(lt->name, lt->backdoor_llid, lt->dido_llid,
                       strlen(LABREAK) + 1, header_type_ctrl_agent,
                       header_val_del_dido_llid, buf);
      }
    llid_backdoor_del_traf(lt->name, lt->backdoor_llid, lt->dido_llid);
    }
}

static void traf_shutdown(int dido_llid, int line)
{
  t_llid_traf *lt = llid_traf_get(dido_llid);
  if (lt)
    {
    differed_dido_llid_end(lt);
    tell_backdoor_gone(lt);
    clean_auto_timer(lt);
    if ((lt->auto_must_send_resp) && (msg_exist_channel(lt->dido_llid)))
      {
      send_to_traf_client(lt->dido_llid, doors_val_link_ko,
                          strlen(lt->auto_resp) + 1, lt->auto_resp);
      }
    }
  else
    KERR("ERROR NO RECORD %d %d", dido_llid, line);
}

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

static void arm_auto_timer_with_resp(t_llid_traf *lt, char *resp, char *name,
                                     int val)
{
  unsigned long ul_llid = (unsigned long) (lt->dido_llid);
  clean_auto_timer(lt);
  fill_200_char_resp(lt->auto_resp, resp, name);
  clownix_timeout_add(val, timer_auto, (void *) ul_llid,
                      &(lt->auto_timer_abs), &(lt->auto_timer_ref));
}

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

static void llid_traf_associate(t_llid_traf *llid_traf, char *name, 
                                int llid_backdoor, int dido_llid)
{
  llid_traf->is_associated = 1;
  strncpy(llid_traf->name, name, MAX_NAME_LEN - 1);
  llid_traf->backdoor_llid = llid_backdoor; 
  llid_backdoor_add_traf(name, llid_backdoor, dido_llid);
}

static void link_associate(t_llid_traf *lt, char *cookie, 
                          char *name, int llid_backdoor)
{
  cookie_info_store(lt, cookie);
  llid_traf_associate(lt, name, llid_backdoor, lt->dido_llid);
  lt->auto_state = auto_state_wait_agent_link;
  arm_auto_timer_with_resp(lt, "KO link_associate timeout", name, 3000);
}

/* the leg into a guest carries the wire version of its client */
static void check_wire_version(char *data)
{
  static int last_reported = CLOONIX_WIRE_VERSION;
  char *ptr, who[MAX_NAME_LEN];
  int ver = -1;
  memset(who, 0, MAX_NAME_LEN);
  ptr = strstr(data, "name=");
  if ((!ptr) || (sscanf(ptr, "name=%63s", who) != 1))
    strncpy(who, "?", MAX_NAME_LEN-1);
  ptr = strstr(data, " wire=");
  if (ptr)
    {
    if (sscanf(ptr, " wire=%d", &ver) != 1)
      ver = -1;
    }
  if (ver == CLOONIX_WIRE_VERSION)
    return;
  if (ver == last_reported)
    return;
  last_reported = ver;
  if (ver == -1)
    KERR("WARNING a client opened a leg into %s with no wire version, so it "
         "is older than %d, from before the numbering was fixed. This cloonix "
         "speaks %d. The leg is allowed through; if anything behaves oddly on "
         "it, this is the first thing to suspect",
         who, CLOONIX_WIRE_VERSION_MIN, CLOONIX_WIRE_VERSION);
  else if (ver < CLOONIX_WIRE_VERSION_MIN)
    KERR("WARNING a client of a leg into %s speaks wire version %d, older "
         "than the %d where the numbering was fixed. This cloonix speaks %d. "
         "The leg is allowed through; if anything behaves oddly on it, this "
         "is the first thing to suspect",
         who, ver, CLOONIX_WIRE_VERSION_MIN, CLOONIX_WIRE_VERSION);
  else
    KERR("WARNING a client of a leg into %s speaks wire version %d, this "
         "cloonix speaks %d. Both are %d or later so they agree on everything "
         "%d knew about, and the leg is allowed through",
         who, ver, CLOONIX_WIRE_VERSION, CLOONIX_WIRE_VERSION_MIN,
         CLOONIX_WIRE_VERSION_MIN);
}

static int start_link2agent_auto(t_llid_traf *lt, int len, char *data)
{
  char buf[MAX_A2D_LEN];
  int res, llid_backdoor, rlen, result = -1;
  int  headsize = sock_header_get_size();
  char *name;
  char *ptrs, *ptre, *cookie;
  memset(lt->auto_resp, 0, 2*MAX_NAME_LEN);
  lt->auto_must_send_resp = 1;
  ptrs = strstr(data, "DBSSH_CLI_DOORS_REQ name=");
  if (!ptrs)
    {
    fill_200_char_resp(lt->auto_resp, "KO err1", "noname");
    }
  else
    {
    check_wire_version(data);
    ptrs = strstr(data, "name=");
    ptrs += strlen("name="); 
    ptre = strstr(ptrs, " cookie="); 
    if (!ptre)
      {
      KERR("%s", data);
      fill_200_char_resp(lt->auto_resp, "KO err2", "noname");
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
          fill_200_char_resp(lt->auto_resp,"KO_bad_connection",name);
        else
          fill_200_char_resp(lt->auto_resp, "KO_name_not_found",name);
        }
      else
        {
        if (llid_backdoor_ping_status_is_ok(name))
          {
          link_associate(lt, cookie, name, llid_backdoor);
          rlen = snprintf(buf+headsize,MAX_A2D_LEN-1, DBSSH_SERV_DOORS_REQ, 
                          lt->xauth_cookie_format);
          llid_backdoor_tx(lt->name, lt->backdoor_llid, lt->dido_llid, 
                           rlen + 1, header_type_ctrl_agent, 
                           header_val_add_dido_llid, buf);
          result = 0;
          }
        else
          fill_200_char_resp(lt->auto_resp,"KO_ping_to_agent",name);
        }
      }
    }
  return result;
}

static void send_resp_ok_to_traf_client(int dido_llid) 
{
  t_llid_traf *lt = llid_traf_get(dido_llid);
  if (lt)
    {
    lt->auto_state = auto_state_complete_link_ok;
    clean_auto_timer(lt);
    fill_200_char_resp(lt->auto_resp, "OK", lt->name); 
    lt->auto_must_send_resp = 0;
    send_to_traf_client(lt->dido_llid, doors_val_link_ok,
                        strlen(lt->auto_resp)+1,lt->auto_resp);
    }
}

void llid_traf_tx_to_client(char *name, int dido_llid,
                            int len, int type, int val,
                            char  *buf)
{
  t_llid_traf *lt = llid_traf_get(dido_llid);
  if (lt && strcmp(lt->name, name))
    KERR("WARNING %s spoke on the byte pipe %d of %s, dropped",
         name, dido_llid, lt->name);
  else if (lt)
    {
    if ((lt->auto_state == auto_state_complete_link_ok) &&
       (type == header_type_traffic) && (val == header_val_none))
      {
      send_to_traf_client(dido_llid, doors_val_none, len, buf);
      lt = llid_traf_get(dido_llid);
      if (lt)
        pace(lt);
      }
    else if ((lt->auto_state == auto_state_complete_link_ok) &&
             (type == header_type_ctrl_agent) &&
             ((val == header_val_xoff) || (val == header_val_xon)))
      {
      lt->agent_xoff = (val == header_val_xoff);
      pace(lt);
      }
    else if (type == header_type_ctrl_agent)
      {
      if (lt->auto_state == auto_state_wait_agent_link) 
        {
        if (val == header_val_add_dido_llid) 
          {
          if (!strncmp(buf, DBSSH_SERV_DOORS_RESP,
                       strlen(DBSSH_SERV_DOORS_RESP)))
            send_resp_ok_to_traf_client(lt->dido_llid); 
          else
            {
            KERR("WARNING %s refused a byte pipe: %.200s", name, buf);
            traf_shutdown(dido_llid, __LINE__);
            }
          }
        else
          {
          KERR("WARNING %s sent %d %d on a byte pipe not yet open", name,
               type, val);
          traf_shutdown(dido_llid, __LINE__);
          }
        }
      if (val == header_val_del_dido_llid)
        {
        /* A close with bytes still in flight is ordinary: the peer asks to go
           while its last output is on the way out. */
        if ((msg_exist_channel(dido_llid)) &&
            (doorways_tx_or_rx_still_in_queue(dido_llid)))
          {
          lt->del_pending = 1;
          lt->del_pending_queue = 0;
          }
        else  
          traf_shutdown(dido_llid, __LINE__);
        }
      }
    else
      KERR("ERROR %d", lt->auto_state);
    }
  else if ((type == header_type_traffic) && (val == header_val_none))
    g_late_traffic += 1;
  else if ((type == header_type_ctrl_agent) &&
           (val == header_val_add_dido_llid))
    g_late_open += 1;
  else if ((type == header_type_ctrl_agent) &&
           ((val == header_val_del_dido_llid) || (val == header_val_xoff) ||
            (val == header_val_xon)))
    g_late_del += 1;
  else
    KERR("ERROR no session %d for header type %d val %d",
         dido_llid, type, val);
}

#define DEL_PENDING_MAX_SEC 30

void llid_traf_heartbeat(void)
{
  int i, queue;
  t_llid_traf *lt;
  if (g_late_traffic)
    {
    KERR("WARNING %d traffic chunks arrived for sessions whose client had "
         "already left, dropped", g_late_traffic);
    g_late_traffic = 0;
    }
  if (g_late_open)
    {
    KERR("WARNING %d byte pipe(s) opened by an agent after their client had "
         "left, closed by the del sent at that leave", g_late_open);
    g_late_open = 0;
    }
  if (g_late_del)
    {
    /* The agent's end-of-session for a client that left first: the
       session was already shut down on its side, nothing to do. */
    KTRC("%d agent session ends arrived after their client had left",
         g_late_del);
    g_late_del = 0;
    }
  for (i = 0; i < CLOWNIX_MAX_CHANNELS; i++)
    {
    lt = g_llid_traf[i];
    if ((!lt) || (!lt->del_pending))
      continue;
    /* Ask about the channel only while there is one */
    if (!msg_exist_channel(lt->dido_llid))
      {
      lt->auto_must_send_resp = 0;
      lt->del_pending = 0;
      traf_shutdown(lt->dido_llid, __LINE__);
      continue;
      }
    if (!doorways_tx_or_rx_still_in_queue(lt->dido_llid))
      {
      lt->del_pending = 0;
      traf_shutdown(lt->dido_llid, __LINE__);
      continue;
      }
    /* A queue that shrinks is a slow client being served, not a stuck
       one: the seconds count only while nothing leaves. */
    queue = doorways_tx_queue_bytes(lt->dido_llid);
    if ((lt->del_pending_queue) && (queue < lt->del_pending_queue))
      lt->del_pending = 1;
    else
      lt->del_pending += 1;
    lt->del_pending_queue = queue;
    if (lt->del_pending > DEL_PENDING_MAX_SEC)
      {
      KERR("ERROR llid %d asked to close and nothing left its queue for "
           "%d sec, %d bytes still in it, forcing it", lt->dido_llid,
           DEL_PENDING_MAX_SEC, queue);
      lt->del_pending = 0;
      traf_shutdown(lt->dido_llid, __LINE__);
      }
    }
}

/* dbssh client end died: tell the agent; no deferred close, the caller
   frees the leg (a second free hit a reused llid). */
void llid_traf_client_gone(int dido_llid)
{
  t_llid_traf *lt = llid_traf_get(dido_llid);
  if (lt)
    {
    tell_backdoor_gone(lt);
    clean_auto_timer(lt);
    if (lt->end_timer_abs)
      clownix_timeout_del(lt->end_timer_abs, lt->end_timer_ref,
                          __FILE__, __LINE__);
    watch_del(lt);
    g_llid_traf[dido_llid] = NULL;
    clownix_free(lt, __FUNCTION__);
    }
}

void llid_traf_delete(int dido_llid)
{
  traf_shutdown(dido_llid, __LINE__);
}

void llid_traf_rx_from_client(int dido_llid, int len, char *rx)
{
  char buf[MAX_A2D_LEN];
  int  headsize = sock_header_get_size();
  int len_max = MAX_A2D_LEN - headsize;
  t_llid_traf *lt;
  if (len > len_max)
    KOUT("ERROR %d %d", len, len_max);
  lt = llid_traf_get(dido_llid);
  if (!lt)
    {
    llid_traf_alloc(dido_llid);
    lt = llid_traf_get(dido_llid);
    if (!lt)
      KOUT("ERROR  ");
    if (len > len_max)
      KOUT("ERROR %d %d", len, len_max);
    if (start_link2agent_auto(lt, len, rx))
      {
      traf_shutdown(lt->dido_llid, __LINE__);
      }
    }
  else if (lt->is_associated)
    {
    memcpy(buf+headsize, rx, len);
    if (lt->backdoor_llid == 0)
      KOUT("ERROR");
    llid_backdoor_tx(lt->name, lt->backdoor_llid, lt->dido_llid, 
                     len, header_type_traffic, header_val_none, buf);
    pace(lt);
    }
  else
    KOUT("ERROR  ");
}

