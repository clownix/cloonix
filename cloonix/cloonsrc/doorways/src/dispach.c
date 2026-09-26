/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "io_clownix.h"
#include "util_sock.h"
#include "doorways_sock.h"
#include "llid_traffic.h"
#include "dispach.h"

static t_transfert *g_head_transfert;
static t_transfert *g_dido_llid[CLOWNIX_MAX_CHANNELS];
static t_transfert *g_inside_llid[CLOWNIX_MAX_CHANNELS];

static t_transfert *get_dido_transfert(int dido_llid)
{
  return (g_dido_llid[dido_llid]);
}

static t_transfert *get_inside_transfert(int inside_llid)
{
  return (g_inside_llid[inside_llid]);
}

static void alloc_transfert(int dido_llid, int inside_llid, int type)
{
  t_transfert *olt, *ilt;
  olt = g_dido_llid[dido_llid];
  ilt = g_inside_llid[inside_llid];
  if (olt || ilt)
    KOUT(" ");
  olt = (t_transfert *) clownix_malloc(sizeof(t_transfert), 10);
  memset(olt, 0, sizeof(t_transfert));
  olt->dido_llid = dido_llid;
  olt->inside_llid = inside_llid;
  olt->type = type;
  g_dido_llid[dido_llid] = olt; 
  if (inside_llid)
    g_inside_llid[inside_llid] = olt; 
  if (g_head_transfert)
    g_head_transfert->prev = olt;
  olt->next =  g_head_transfert;
  g_head_transfert = olt;
}

static void free_transfert(int dido_llid, int inside_llid, int show, int line)
{
  t_transfert *olt, *ilt;
  if ((show) && ((dido_llid == 0) || (inside_llid == 0)))
    KERR("WARNING %d %d %d", dido_llid, inside_llid, line);
  olt = g_dido_llid[dido_llid];
  ilt = g_inside_llid[inside_llid];
  g_dido_llid[dido_llid] = NULL;
  g_inside_llid[inside_llid] = NULL;
  if (!olt)
    KOUT(" ");
  if (ilt)
    {
    if (olt != ilt)
      KOUT(" ");
    }
  if (olt->inside_llid != inside_llid)
    KOUT(" ");
  if (olt->dido_llid != dido_llid)
    KOUT(" ");
  doorways_clean_llid(dido_llid);
  if (msg_exist_channel(inside_llid))
    msg_delete_channel(inside_llid);
  if (olt->prev)
    olt->prev->next = olt->next;
  if (olt->next)
    olt->next->prev = olt->prev;
  if (olt == g_head_transfert)
    g_head_transfert = olt->next;
  clownix_free(olt, __FUNCTION__);
}

void in_err_gene(int inside_llid, int err, int from)
{
  int dido_llid;
  t_transfert *ilt;
  ilt = get_inside_transfert(inside_llid);
  if (!ilt)
    {
    KERR("%d", inside_llid);
    if (msg_exist_channel(inside_llid))
      msg_delete_channel(inside_llid);
    }
  else
    {
    if (inside_llid != ilt->inside_llid)
      KOUT(" ");
    dido_llid = ilt->dido_llid;
    free_transfert(dido_llid, inside_llid, 1,  __LINE__);
    }
}

static void in_rx_switch(int inside_llid, int len, char *buf)
{
  t_transfert *ilt;
  ilt = get_inside_transfert(inside_llid);
  if (!ilt)
    {
    KERR(" ");
    if (msg_exist_channel(inside_llid))
      msg_delete_channel(inside_llid);
    }
  else
    {
    if (inside_llid != ilt->inside_llid)
      KOUT(" ");
    if (msg_exist_channel(ilt->dido_llid))
      {
      if (doorways_tx_bufraw(ilt->dido_llid, 0, doors_type_switch,
                             doors_val_none, len, buf))
        {
        KERR("ERROR");
        free_transfert(ilt->dido_llid, ilt->inside_llid, 1, __LINE__);
        }

      }
    else
      free_transfert(ilt->dido_llid, ilt->inside_llid, 1, __LINE__);
    }
}

void dispach_door_llid(int listen_llid, int dido_llid)
{
  if ((dido_llid < 1) || (dido_llid >= CLOWNIX_MAX_CHANNELS))
    KOUT("ERROR %d %d", listen_llid, dido_llid);
}
    
void dispach_door_end(int dido_llid)
{
  int inside_llid;
  t_transfert *olt;
  olt = get_dido_transfert(dido_llid);
  if (olt)
    {
    if (dido_llid != olt->dido_llid)
      KOUT(" ");
    inside_llid = olt->inside_llid;
    /* dbssh only: tell the agent (guest socket closed, command killed) and
       free the traffic record; the llid itself is freed once by free_transfert
       below. llid_traf_delete here armed a deferred close on a reused llid. */
    if (olt->type == doors_type_dbssh)
      llid_traf_client_gone(dido_llid);
    free_transfert(dido_llid, inside_llid, 0, __LINE__);
    }
}
    
static void dispach_door_rx_switch(int dido_llid, int val, int len, char *buf)
{
  t_transfert *olt;
  int inside_llid = 0;
  char *path = get_switch_path();

  if (val == doors_val_init_link)
    {
    olt = get_dido_transfert(dido_llid);
    if (olt)
      {
      /* A second init_link on a leg that already has one: the client is
         confused, we are not. */
      KERR("ERROR second init_link on leg %d (type:%d beat:%d), closing it",
           dido_llid, olt->type, olt->beat_count);
      dispach_door_end(dido_llid);
      return;
      }
    inside_llid = string_client_unix(path, in_err_gene, in_rx_switch, "switch");
    if (!inside_llid)
      {
      KERR("ERROR cannot reach the switch on %s (%s), refusing leg %d",
           path, access(path, F_OK) ? "socket file gone" : "no answer",
           dido_llid);
      doorways_sig_bufraw(dido_llid, 0, doors_type_switch,
                          doors_val_link_ko, "KO");
      return;
      }
    else
      {
      alloc_transfert(dido_llid, inside_llid, doors_type_switch);
      doorways_tx_bufraw(dido_llid, inside_llid, doors_type_switch,
                         doors_val_link_ok, len, buf);
      }
    }
  else if ((val == doors_val_none) || (val == doors_val_c2c))
    {
    olt = get_dido_transfert(dido_llid);
    if (!olt)
      {
      KERR("ERROR data on leg %d before it was opened, dropping", dido_llid);
      }
    else if (dido_llid != olt->dido_llid)
      {
      KOUT("%d %d", dido_llid, olt->dido_llid);
      }
    else
      {
      if (!msg_exist_channel(olt->inside_llid))
        KERR("ERROR dispach fwd: inside_llid=%d channel DEAD - dropping", olt->inside_llid);
      else if (val == doors_val_c2c)
        {
        static const char magic[8] = {
          '\xCA','\xFE','\xDE','\xCA','\xCA','\xFE','\xDE','\xCA'};
        if ((len >= 8) && (!memcmp(buf, magic, 8)))
          string_tx(olt->inside_llid, len, buf);
        else
          {
          char *wrap = clownix_malloc(len + 8, 1);
          memcpy(wrap, magic, 8);
          memcpy(wrap + 8, buf, len);
          string_tx(olt->inside_llid, len + 8, wrap);
          clownix_free(wrap, __FUNCTION__);
          }
        }
      else
        {
        string_tx(olt->inside_llid, len, buf);
        }
      }
    }
  else
    KERR("%d", val);
}

/* A leg carries only these three vals from a client. */
static int dbssh_val_is_known(int val)
{
  return ((val == doors_val_init_link) ||
          (val == doors_val_none) ||
          (val == doors_val_sig));
}

static void dispach_door_rx_dbssh(int dido_llid, int val, int len, char *buf)
{
  static int reported = 0;
  t_transfert *olt;
  if ((!dbssh_val_is_known(val)) && (reported == 0))
    {
    reported = 1;
    KERR("ERROR dbssh leg %d: unknown doors val %d, expected %d, %d or %d. "
         "The client is not the same build as this cloonix",
         dido_llid, val, doors_val_init_link, doors_val_none, doors_val_sig);
    }
  olt = get_dido_transfert(dido_llid);
  if (!olt)
    alloc_transfert(dido_llid, 0, doors_type_dbssh);
  llid_traf_rx_from_client(dido_llid, len, buf);
}

void doorways_rx_bufraw(int dido_llid, int tid,
                        int len_bufraw, char *doors_bufraw,
                        int type, int val, int len, char *buf)
{
  if ((dido_llid < 1) || (dido_llid >= CLOWNIX_MAX_CHANNELS))
    KOUT("%d", dido_llid);

  switch (type)
    {

    case doors_type_switch:
      dispach_door_rx_switch(dido_llid, val, len, buf);
      break;

    case doors_type_dbssh:
      dispach_door_rx_dbssh(dido_llid, val, len, buf);
      break;

    default:
      KERR("ERROR unknown doors type %d on leg %d, closing it", type,
           dido_llid);
      doorways_sig_bufraw(dido_llid, 0, type, doors_val_link_ko, "KO");
      dispach_door_end(dido_llid);
      break;
    }
}

int dispach_send_to_traf_client(int dido_llid, int val, int len, char *buf)
{
  int res;
  res = doorways_tx_bufraw(dido_llid, 0, doors_type_dbssh, val, len, buf);
  return res;
}

static void timer_heartbeat(void *data)
{
  t_transfert *cur = g_head_transfert;
  while(cur)
    {
    cur->beat_count += 1;
    if (cur->inside_llid)
      {
      msg_relay_flow_ctrl(cur->inside_llid, cur->dido_llid, &(cur->paused_in));
      msg_relay_flow_ctrl(cur->dido_llid, cur->inside_llid, &(cur->paused_out));
      }
    cur = cur->next;
    }
  clownix_timeout_add(1, timer_heartbeat, NULL, NULL, NULL);
}

void  dispach_init(void)
{
  g_head_transfert = NULL;
  memset(g_dido_llid, 0, CLOWNIX_MAX_CHANNELS * sizeof(t_transfert *));
  memset(g_inside_llid, 0, CLOWNIX_MAX_CHANNELS * sizeof(t_transfert *));
  clownix_timeout_add(10, timer_heartbeat, NULL, NULL, NULL);
}
