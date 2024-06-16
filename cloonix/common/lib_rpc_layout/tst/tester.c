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
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <math.h>
#include "io_clownix.h"
#include "rpc_clownix.h"
#include "layout_rpc.h"

#define SAMPLE_CLOWNIX_SERVER "/tmp/test_tux2tux"
static int i_am_client        = 0;
static int count_layout_move_on_off = 0;
static int count_layout_width_height = 0;
static int count_layout_center_scale = 0;
static int count_layout_zoom = 0;
static int count_layout_event_sub = 0;
static int count_layout_lan = 0;
static int count_layout_sat = 0;
static int count_layout_node = 0;
static int count_layout_modif= 0;
static int count_layout_save_params_resp = 0;
static int count_layout_save_params_req = 0;


/*****************************************************************************/
static void print_all_count(void)
{
  printf("\n\n");
  printf("START COUNT\n");
  printf("%d\n", count_layout_move_on_off);
  printf("%d\n", count_layout_center_scale);
  printf("%d\n", count_layout_width_height);
  printf("%d\n", count_layout_zoom);
  printf("%d\n", count_layout_event_sub);
  printf("%d\n", count_layout_lan);
  printf("%d\n", count_layout_sat);
  printf("%d\n", count_layout_node);
  printf("%d\n", count_layout_modif);
  printf("%d\n", count_layout_save_params_resp);
  printf("%d\n", count_layout_save_params_req);
  printf("END COUNT\n");
}
/*---------------------------------------------------------------------------*/


void rpct_recv_sigdiag_msg(int llid, int tid, char *line){KOUT(" ");};
void rpct_recv_poldiag_msg(int llid, int tid, char *line){KOUT(" ");};

void rpct_recv_kil_req(int llid, int tid){KOUT(" ");};
void rpct_recv_pid_req(int llid, int tid, char *name, int num){KOUT(" ");};
void rpct_recv_pid_resp(int llid, int tid,
                        char *name, int num, int toppid, int pid){KOUT(" ");};

void rpct_recv_hop_sub(int llid, int tid, int flags_hop){KOUT(" ");};
void rpct_recv_hop_unsub(int llid, int tid){KOUT(" ");};
void rpct_recv_hop_msg(int llid, int tid, int flags_hop, char *txt){KOUT(" ");};


/*****************************************************************************/
static void random_choice_str(char *name, int max_len)
{
  int i, len = rand() % (max_len-2);
  len += 1;
  memset (name, 0 , max_len);
  for (i=0; i<len; i++)
    {
    name[i] = (char )((rand()%20) + 100);
    }
  name[len] = 0;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
static int double_diff(double a, double b)
{
  int result;
  double c = a-b;
  if (fabs(c) > 0.0001)
    result = -1;
  else
    result = 0;
  return result;   
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void heartbeat (int delta)
{
  int i;
  unsigned long *mallocs;
  static int count = 0;
  count++;
  if (!(count%1000))
    {
    printf("MALLOCS: ");
    mallocs = get_clownix_malloc_nb();
    for (i=0; i<MAX_MALLOC_TYPES; i++)
      printf("%d:%02lu ", i, mallocs[i]);
    print_all_count();
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static double randouble(void)
{
  double result;
  result = (double) rand();
  result = trunc(result);
  result /= 10000;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_layout_modif(int llid, int itid, int itype, char *iname, 
                       int inum, int ival1, int ival2)
{
  static int tid;
  static int type;
  static int num;
  static int val1;
  static int val2;
  static char name[MAX_PATH_LEN];
  if (i_am_client)
    {
    if (count_layout_modif)
      {
      if (itid != tid)
        KOUT(" ");
      if (iname)
      if (strcmp(name, iname))
        KOUT(" ");
      }
    random_choice_str(name, MAX_PATH_LEN);
    type   =rand()%(layout_modif_type_max-1)+1;
    num   =rand();
    tid   =rand();
    val1   =rand();
    val2   =rand();
    send_layout_modif(llid, tid,type, name, num, val1, val2);
    }
  else
    send_layout_modif(llid, itid,itype, iname, inum, ival1, ival2);
  count_layout_modif++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_layout_lan(int llid, int itid, t_layout_lan *ilayout_lan)
{
  static int tid;
  static t_layout_lan layout_lan;
  if (i_am_client)
    {
    if (count_layout_lan)
      {
      if (itid != tid)
        KOUT(" ");
      if (strcmp(layout_lan.name, ilayout_lan->name))
        KOUT(" ");
      if (double_diff(layout_lan.x, ilayout_lan->x))
        KOUT(" ");
      if (double_diff(layout_lan.y, ilayout_lan->y))
        KOUT(" ");
      if (ilayout_lan->hidden_on_graph != layout_lan.hidden_on_graph)
        KOUT(" ");
      }
    memset(&layout_lan, 0, sizeof(t_layout_lan));
    random_choice_str(layout_lan.name, MAX_NAME_LEN);
    layout_lan.x      =randouble();
    layout_lan.y      =randouble();
    layout_lan.hidden_on_graph      =rand();
    send_layout_lan(llid, tid, &layout_lan);
    }
  else
    send_layout_lan(llid, itid, ilayout_lan);
  count_layout_lan++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_layout_sat(int llid, int itid, t_layout_sat *ilayout_sat)
{
  static int tid;
  static t_layout_sat layout_sat;
  if (i_am_client)
    {
    if (count_layout_sat)
      {
      if (itid != tid)
        KOUT(" ");
      if (strcmp(layout_sat.name, ilayout_sat->name))
        KOUT("%s %s", layout_sat.name, ilayout_sat->name);
      if (double_diff(layout_sat.x, ilayout_sat->x))
        KOUT(" ");
      if (double_diff(layout_sat.y, ilayout_sat->y))
        KOUT(" ");
      if (layout_sat.hidden_on_graph != ilayout_sat->hidden_on_graph)
        KOUT(" ");
      if (layout_sat.is_a2b != ilayout_sat->is_a2b)
        KOUT(" ");
      if (double_diff(layout_sat.xa, ilayout_sat->xa))
        KOUT(" ");
      if (double_diff(layout_sat.ya, ilayout_sat->ya))
        KOUT(" ");
      if (double_diff(layout_sat.xb, ilayout_sat->xb))
        KOUT(" ");
      if (double_diff(layout_sat.yb, ilayout_sat->yb))
        KOUT(" ");

      }
    memset(&layout_sat, 0, sizeof(t_layout_sat));
    random_choice_str(layout_sat.name, MAX_NAME_LEN);
    layout_sat.x      =randouble();
    layout_sat.y      =randouble();
    layout_sat.xa      =randouble();
    layout_sat.ya      =randouble();
    layout_sat.xb      =randouble();
    layout_sat.yb      =randouble();
    layout_sat.is_a2b     =rand();
    layout_sat.hidden_on_graph      =rand();
    send_layout_sat(llid, tid, &layout_sat);
    }
  else
    send_layout_sat(llid, itid, ilayout_sat);
  count_layout_sat++;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void recv_layout_node(int llid, int itid, t_layout_node *ilayout_node)
{
  int i;
  static int tid;
  static t_layout_node layout_node;
  if (i_am_client)
    {
    if (count_layout_node)
      {
      if (itid != tid)
        KOUT(" ");
      if (layout_node.color != ilayout_node->color)
        KOUT(" ");
      if (layout_node.nb_eth != ilayout_node->nb_eth)
        KOUT(" ");
      if (strcmp(layout_node.name, ilayout_node->name))
        KOUT(" ");
      if (double_diff(layout_node.x, ilayout_node->x))
        KOUT(" ");
      if (double_diff(layout_node.y, ilayout_node->y))
        KOUT(" ");
      if (layout_node.hidden_on_graph != ilayout_node->hidden_on_graph)
        KOUT(" ");
      for (i=0; i<layout_node.nb_eth; i++)
        {
        if (double_diff(layout_node.eth[i].x, ilayout_node->eth[i].x))
          KOUT(" ");
        if (double_diff(layout_node.eth[i].y, ilayout_node->eth[i].y))
          KOUT(" ");
        }
      }
    memset(&layout_node, 0, sizeof(t_layout_node));
    random_choice_str(layout_node.name, MAX_NAME_LEN);
    layout_node.x      =randouble();
    layout_node.y      =randouble();
    layout_node.hidden_on_graph  =rand();
    layout_node.color = rand();
    layout_node.nb_eth = (rand() %(MAX_ETH_VM - 1))+1;
    for (i=0; i<layout_node.nb_eth; i++)
      {
      layout_node.eth[i].x      =randouble();
      layout_node.eth[i].y      =randouble();
      }
    send_layout_node(llid, tid, &layout_node);
    }
  else
    send_layout_node(llid, itid, ilayout_node);
  count_layout_node++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_layout_move_on_off(int llid, int itid, int ion)
{
  static int tid;
  static int on;
  if (i_am_client)
    {
    if (count_layout_move_on_off)
      {
      if (ion != on)
        KOUT(" ");
      if (itid != tid)
        KOUT(" ");
      }
    tid = rand();
    on = rand();
    send_layout_move_on_off(llid, tid, on);
    }
  else
    send_layout_move_on_off(llid, itid, ion);
  count_layout_move_on_off++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_layout_width_height(int llid, int itid, int iwidth, int iheight)
{
  static int tid;
  static int width;
  static int height;
  if (i_am_client)
    {
    if (count_layout_width_height)
      {
      if (iwidth != width)
        KOUT(" ");
      if (iheight != height)
        KOUT(" ");
      if (itid != tid)
        KOUT(" ");
      }
    tid = rand();
    width = rand();
    height = rand();
    send_layout_width_height(llid, tid, width, height);
    }
  else
    send_layout_width_height(llid, itid, iwidth, iheight);
  count_layout_width_height++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_layout_center_scale(int llid, int itid, int ix,int iy,int iw,int ih)
{
  static int tid;
  static int x,y,w,h;
  if (i_am_client)
    {
    if (count_layout_center_scale)
      {
      if (ix != x)
        KOUT(" ");
      if (iy != y)
        KOUT(" ");
      if (iw != w)
        KOUT(" ");
      if (ih != h)
        KOUT(" ");
      if (itid != tid)
        KOUT(" ");
      }
    tid = rand();
    x = rand();
    y = rand();
    w = rand();
    h = rand();
    send_layout_center_scale(llid, tid, x, y, w, h);
    }
  else
    send_layout_center_scale(llid, itid, ix, iy, iw, ih);
  count_layout_center_scale++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_layout_save_params_req(int llid, int itid)
{
  static int tid;
  if (i_am_client)
    {
    if (count_layout_save_params_req)
      {
      if (itid != tid)
        KOUT(" ");
      }
    tid = rand();
    send_layout_save_params_req(llid, tid);
    }
  else
    send_layout_save_params_req(llid, itid);
  count_layout_save_params_req++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_layout_save_params_resp(int llid, int itid, int ion,
                                  int iwidth, int iheight,
                                  int ix, int iy, int iw, int ih)
{
  static int tid;
  static int on, width, height;
  static int x,y,w,h;
  if (i_am_client)
    {
    if (count_layout_save_params_resp)
      {
      if (ion != on)
        KOUT(" ");
      if (iwidth != width)
        KOUT(" ");
      if (iheight != height)
        KOUT(" ");
      if (ix != x)
        KOUT(" ");
      if (iy != y)
        KOUT(" ");
      if (iw != w)
        KOUT(" ");
      if (ih != h)
        KOUT(" ");
      if (itid != tid)
        KOUT(" ");
      } 
    tid = rand();
    on = rand();
    width = rand();
    height = rand();
    x = rand();
    y = rand();
    w = rand();
    h = rand();
    send_layout_save_params_resp(llid, tid, on, width, height, x, y, w, h);
    }
  else
    send_layout_save_params_resp(llid,itid,ion,iwidth,iheight,ix,iy,iw,ih);
  count_layout_save_params_resp++;
}
/*---------------------------------------------------------------------------*/



/*****************************************************************************/
void recv_layout_zoom(int llid, int itid, int izoom)
{
  static int tid;
  static int zoom;
  if (i_am_client)
    {
    if (count_layout_zoom)
      {
      if (izoom != zoom)
        KOUT(" ");
      if (itid != tid)
        KOUT(" ");
      } 
    tid = rand();
    zoom = rand();
    send_layout_zoom(llid, tid, zoom);
    }
  else
    send_layout_zoom(llid, itid, izoom);
  count_layout_zoom++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_layout_event_sub(int llid, int itid, int ion)
{
  static int tid;
  static int on;
  if (i_am_client)
    {
    if (count_layout_event_sub)
      {
      if (ion != on)
        KOUT(" ");
      if (itid != tid)
        KOUT(" ");
      }
    tid = rand();
    on = rand();
    send_layout_event_sub(llid, tid, on);
    }
  else
    send_layout_event_sub(llid, itid, ion);
  count_layout_event_sub++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void send_first_burst(int llid)
{
  recv_layout_move_on_off(llid, 0, 0);
  recv_layout_width_height(llid, 0, 0, 0);
  recv_layout_center_scale(llid, 0, 0, 0, 0, 0);
  recv_layout_zoom(llid, 0, 0);
  recv_layout_event_sub(llid, 0, 0);
  recv_layout_lan(llid, 0, NULL);
  recv_layout_sat(llid, 0, NULL);
  recv_layout_node(llid, 0, NULL);
  recv_layout_modif(llid, 0, 0, NULL, 0, 0, 0);
  recv_layout_save_params_req(llid, 0); 
  recv_layout_save_params_resp(llid, 0, 0, 0, 0, 0, 0, 0, 0); 
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void usage(char *name)
{
  printf("\n");
  printf("\n%s -s", name);
  printf("\n%s -c", name);
  printf("\n");
  exit (0);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void err_cb (int llid, int err, int from)
{
  printf("ERROR %d  %d\n", llid, err);
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
static void rx_cb (int llid, int len, char *buf)
{
  if (doors_io_layout_decoder(llid, len, buf))
    KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void connect_from_client(int llid, int llid_new)
{
  msg_mngt_set_callbacks (llid_new, err_cb, rx_cb);
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
int main (int argc, char *argv[])
{
  int llid;
  doors_io_layout_xml_init(string_tx);
  msg_mngt_init("test", IO_MAX_BUF_LEN);
  if (argc == 2)
    {
    if (!strcmp(argv[1], "-s"))
      {
      unlink(SAMPLE_CLOWNIX_SERVER);
      msg_mngt_heartbeat_init(heartbeat);
      string_server_unix(SAMPLE_CLOWNIX_SERVER, connect_from_client, "o");
      printf("\n\n\nSERVER\n\n");
      }
    else if (!strcmp(argv[1], "-c"))
      {
      i_am_client = 1;
      msg_mngt_heartbeat_init(heartbeat);
      llid = string_client_unix(SAMPLE_CLOWNIX_SERVER, err_cb, rx_cb, "o");
      send_first_burst(llid);
      }
    else
      usage(argv[0]);
    }
  else
    usage(argv[0]);
  msg_mngt_loop();
  return 0;
}
/*--------------------------------------------------------------------------*/



