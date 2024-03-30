/*****************************************************************************/
/*    Copyright (C) 2006-2024 clownix@clownix.net License AGPL-3             */
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
#include <errno.h>
#include "io_clownix.h"
#include "util_sock.h"
#include "doorways_sock.h"
#include "llid_traffic.h"
#include "openssh_traf.h"
#include "llid_x11.h"
#include "llid_xwy.h"
#include "dispach.h"

char *get_switch_path(void);

typedef struct t_flow_ctrl
{
  int ident_flow_timeout;
  int dido_llid;
} t_flow_ctrl;

static t_transfert *g_head_transfert;
static t_transfert *g_dido_llid[CLOWNIX_MAX_CHANNELS];
static t_transfert *g_inside_llid[CLOWNIX_MAX_CHANNELS];
static char g_buf[MAX_DOORWAYS_BUF_LEN];
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
char *get_g_buf(void)
{
  return g_buf;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static t_transfert *get_dido_transfert(int dido_llid)
{
  return (g_dido_llid[dido_llid]);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static t_transfert *get_inside_transfert(int inside_llid)
{
  return (g_inside_llid[inside_llid]);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void timeout_flow_ctrl(void *data)
{
  t_flow_ctrl *fc = (t_flow_ctrl *) data;
  t_transfert *tf = get_dido_transfert(fc->dido_llid);
  if (tf)
    {
    if (tf->ident_flow_timeout == fc->ident_flow_timeout)
      {
      channel_rx_local_flow_ctrl(tf->inside_llid, 0);
      }
    }
  free(data);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int flow_ctrl_activation_done(t_transfert *tf)
{
  int result = 0;
  int qsiz = (int) channel_get_tx_queue_len(tf->dido_llid);
  t_flow_ctrl *fc;
  if (qsiz > 1000000)
    {
    fc = (t_flow_ctrl *) malloc(sizeof(t_flow_ctrl));
    tf->ident_flow_timeout += 1;
    fc->ident_flow_timeout = tf->ident_flow_timeout;
    fc->dido_llid = tf->dido_llid;
    channel_rx_local_flow_ctrl(tf->inside_llid, 0);
    clownix_timeout_add(1, timeout_flow_ctrl, (void *)fc, NULL, NULL);
    result = 1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int dispatch_get_dido_llid_with_inside_llid(int inside_llid, int *init_done)
{
  int result = 0;
  t_transfert *t = get_inside_transfert(inside_llid);
  if (t)
    {
    result = t->dido_llid;
    *init_done = t->init_done;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void dispatch_set_init_done_with_inside_llid(int inside_llid)
{
  t_transfert *t = get_inside_transfert(inside_llid);
  if (t)
    {
    t->init_done = 1;
    }
}
/*--------------------------------------------------------------------------*/


/*****************************************************************************/
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
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void free_transfert(int dido_llid, int inside_llid)
{
  t_transfert *olt, *ilt;
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
  if (olt->type == doors_type_dbssh)
    llid_traf_delete(dido_llid);
  else
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
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void dispach_free_transfert(int dido_llid, int inside_llid)
{
  free_transfert(dido_llid, inside_llid);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
t_transfert *dispach_get_dido_transfert(int dido_llid)
{
  return (g_dido_llid[dido_llid]);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
t_transfert *dispach_get_inside_transfert(int inside_llid)
{
  return (g_inside_llid[inside_llid]);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
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
    free_transfert(dido_llid, inside_llid);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
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
      if (doorways_tx(ilt->dido_llid, 0, doors_type_switch,
                      doors_val_none, len, buf))
        {
        KERR(" ");
        free_transfert(ilt->dido_llid, ilt->inside_llid);
        }

      }
    else
      free_transfert(ilt->dido_llid, ilt->inside_llid);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int in_rx_spice(int inside_llid, int fd)
{
  t_transfert *ilt;
  int len = 0;
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
    if (flow_ctrl_activation_done(ilt))
      {
      len = 0;
      }
    else
      {
      len = read (fd, g_buf, MAX_DOORWAYS_BUF_LEN);
      if (len <= 0) 
        {
        free_transfert(ilt->dido_llid, ilt->inside_llid);
        len = 0;
        }
      else
        {
        if (msg_exist_channel(ilt->dido_llid))
          {
          if (doorways_tx(ilt->dido_llid, 0, doors_type_spice,
                          doors_val_none, len, g_buf))
            {
            KERR(" ");
            free_transfert(ilt->dido_llid, ilt->inside_llid);
            len = 0;
            }
          flow_ctrl_activation_done(ilt);
          }
        else
          {
          free_transfert(ilt->dido_llid, ilt->inside_llid);
          len = 0;
          }
        }
      }
    }
  return len;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void dispach_door_llid(int dido_llid)
{
  if ((dido_llid < 1) || (dido_llid >= CLOWNIX_MAX_CHANNELS))
    KOUT("%d", dido_llid);
}
/*--------------------------------------------------------------------------*/
    
/*****************************************************************************/
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
    free_transfert(dido_llid, inside_llid);
    }
}
/*--------------------------------------------------------------------------*/
    
/*****************************************************************************/
static void dispach_door_rx_switch(int dido_llid, int val, int len, char *buf)
{
  t_transfert *olt;
  int inside_llid = 0;
  char *path = get_switch_path();

  if (val == doors_val_init_link)
    {
    olt = get_dido_transfert(dido_llid);
    if (olt)
      KOUT("type:%d beat:%d", olt->type, olt->beat_count);
    inside_llid = string_client_unix(path, in_err_gene, in_rx_switch, "switch");
    if (!inside_llid)
      {
      KOUT(" ");
      doorways_tx(dido_llid, 0, doors_type_switch,
                  doors_val_link_ko, len, buf);
      }
    else
      {
      alloc_transfert(dido_llid, inside_llid, doors_type_switch);
      doorways_tx(dido_llid, 0, doors_type_switch,
                  doors_val_link_ok, len, buf);
      }
    }
  else if (val == doors_val_none)
    {
    olt = get_dido_transfert(dido_llid);
    if (!olt)
      KOUT(" ");
    else
      {
      if (dido_llid != olt->dido_llid)
        KOUT(" ");
      string_tx(olt->inside_llid, len, buf);
      }
    }
  else
    KERR("%d", val);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void dispach_door_rx_spice(int dido_llid, int val, int len, char *buf)
{
  t_transfert *olt;
  int fd, inside_llid;
  if (val == doors_val_init_link)
    {
    olt = get_dido_transfert(dido_llid);
    if (olt)
      KOUT("type:%d beat:%d", olt->type, olt->beat_count);
    if (util_nonblock_client_socket_unix(buf, &fd))
      {
      KERR("%s", buf);
      doorways_tx(dido_llid, 0, doors_type_spice,
                  doors_val_link_ko, strlen("KO") + 1, "KO");
      }
    else
      {
      inside_llid = msg_watch_fd(fd, in_rx_spice, in_err_gene, "spice");
      if (!inside_llid)
        {
        KERR(" ");
        doorways_tx(dido_llid, 0, doors_type_spice,
                    doors_val_link_ko, strlen("KO") + 1, "KO");
        }
      else
        {
        alloc_transfert(dido_llid, inside_llid, doors_type_spice);
        doorways_tx(dido_llid, 0, doors_type_spice,
                    doors_val_link_ok,  strlen("OK") + 1, "OK");
        }
      }
    }
  else if (val == doors_val_none)
    {
    olt = get_dido_transfert(dido_llid);
    if (!olt)
      KERR(" ");
    else
      {
      if (dido_llid != olt->dido_llid)
        KOUT(" ");
      if (msg_exist_channel(olt->inside_llid))
        {
        watch_tx(olt->inside_llid, len, buf);
        }
      else
        free_transfert(olt->dido_llid, olt->inside_llid);
      }
    }
  else
    KERR("%d", val);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int dispach_door_rx_xwy(int type, int dido_llid, int tid, int val,
                                int len, char *buf)
{
  int inside_llid;
  int result = 0;
  t_transfert *ilt, *olt;
  if (val == doors_val_init_link)
    {
    olt = get_dido_transfert(dido_llid);
    if (olt)
      KOUT("%d %d type:%d beat:%d", olt->dido_llid, olt->inside_llid,
                                    olt->type, olt->beat_count);
    inside_llid = llid_xwy_ctrl();
    if (inside_llid == 0)
      result = 1;
    else
      {
      ilt = get_inside_transfert(inside_llid);
      if (ilt)
        KOUT("%d %d type:%d beat:%d", ilt->dido_llid, ilt->inside_llid,
                                      ilt->type, ilt->beat_count);
      doorways_tx(dido_llid,inside_llid,type,doors_val_link_ok,3,"OK");
      alloc_transfert(dido_llid, inside_llid, type);
      }
    }
  else if (val == doors_val_xwy)
    {
    ilt = get_inside_transfert(tid);
    olt = get_dido_transfert(dido_llid);
    if ((!ilt) || (!olt) || (ilt != olt))
      {
      doorways_tx(dido_llid, 0, type,doors_val_link_ko, 3, "KO");
      KERR("%d %d  %p  %p", dido_llid, tid, ilt, olt);
      }
    else
      {
      if (tid != ilt->inside_llid)
        KOUT(" ");
      if (dido_llid != ilt->dido_llid)
        KOUT("%d %d", dido_llid, ilt->dido_llid);
      if (msg_exist_channel(ilt->inside_llid))
        {
        watch_tx(ilt->inside_llid, len, buf);
        }
      else
        {
        doorways_tx(dido_llid, 0, type, doors_val_link_ko, 3, "KO");
        free_transfert(ilt->dido_llid, ilt->inside_llid);
        }
      }
    }
  else
    KOUT("%d", val);
  return result;
}
/*--------------------------------------------------------------------------*/


/*****************************************************************************/
static void dispach_door_rx_openssh(int dido_llid, int val, int len, char *buf)
{
  t_transfert *olt;
  int inside_llid;
  olt = get_dido_transfert(dido_llid);
  if (!olt)
    {
    inside_llid = openssh_rx_from_client_init(dido_llid, len, buf);
    if (inside_llid > 0)
      alloc_transfert(dido_llid, inside_llid, doors_type_openssh);
    }
  else
    {
    if (!msg_exist_channel(olt->inside_llid))
      free_transfert(olt->dido_llid, olt->inside_llid);
    else
      openssh_tx_to_nat(olt->inside_llid, len, buf);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void dispach_door_rx_dbssh(int dido_llid, int val, int len, char *buf)
{
  t_transfert *olt;
  olt = get_dido_transfert(dido_llid);
  if (!olt)
    alloc_transfert(dido_llid, 0, doors_type_dbssh);
  llid_traf_rx_from_client(dido_llid, len, buf);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void dispach_door_rx(int dido_llid, int tid, int type, int val, 
                     int len, char *buf)
{ 
  if ((dido_llid < 1) || (dido_llid >= CLOWNIX_MAX_CHANNELS))
    KOUT("%d", dido_llid);

  switch (type)
    {

    case doors_type_switch:
      dispach_door_rx_switch(dido_llid, val, len, buf);
      break;

    case doors_type_spice:
      dispach_door_rx_spice(dido_llid, val, len, buf);
      break;

    case doors_type_dbssh:
      dispach_door_rx_dbssh(dido_llid, val, len, buf);
      break;

    case doors_type_dbssh_x11_ctrl:
      receive_ctrl_x11_from_client(dido_llid, val, len, buf);
      break;

    case doors_type_dbssh_x11_traf:
      receive_traf_x11_from_client(dido_llid, val, len, buf);
      break;

    case doors_type_openssh:
      dispach_door_rx_openssh(dido_llid, val, len, buf);
      break;

    case doors_type_xwy_main_traf:
    case doors_type_xwy_x11_flow:
      if (dispach_door_rx_xwy(type, dido_llid, tid, val, len, buf))
        {
        doorways_tx(dido_llid, 0, type, doors_val_link_ko, 3, "KO");
        KERR("ERROR X11 %d %d %s", val, len, buf);
        }
      break;

    default:
      KOUT("%d", dido_llid);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int dispach_send_to_traf_client(int dido_llid, int val, int len, char *buf)
{
  int result = -1;
  if (msg_exist_channel(dido_llid))
    {
    doorways_tx(dido_llid, 0, doors_type_dbssh, val, len, buf);
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int dispach_send_to_openssh_client(int dido_llid, int val, int len, char *buf)
{
  int result = -1;
  if (msg_exist_channel(dido_llid))
    {
    doorways_tx(dido_llid, 0, doors_type_openssh, val, len, buf);
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void timer_heartbeat(void *data)
{
  t_transfert *cur = g_head_transfert;
  while(cur)
    {
    cur->beat_count += 1;
    cur = cur->next;
    }
  clownix_timeout_add(10, timer_heartbeat, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void  dispach_init(void)
{
  g_head_transfert = NULL;
  memset(g_dido_llid, 0, CLOWNIX_MAX_CHANNELS * sizeof(t_transfert *));
  memset(g_inside_llid, 0, CLOWNIX_MAX_CHANNELS * sizeof(t_transfert *));
  clownix_timeout_add(10, timer_heartbeat, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/
