/*****************************************************************************/
/*    Copyright (C) 2006-2023 clownix@clownix.net License AGPL-3             */
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
#include <gtk/gtk.h>
#include <string.h>
#include <stdlib.h>
#include "io_clownix.h"
#include "rpc_clownix.h"
#include "commun_consts.h"
#include "bank.h"
#include "move.h"
#include "cloonix.h"
#include "popup.h"
#include "pid_clone.h"
#include "main_timer_loop.h"
#include "menu_utils.h"
#include "menus.h"
#include "bdplot.h"

/*--------------------------------------------------------------------------*/
static int g_timeout_flipflop_freeze;
static int g_one_eventfull_has_arrived;
static int glob_eventfull_has_arrived;
/*--------------------------------------------------------------------------*/

typedef struct t_ping_evt
{
  char name[MAX_NAME_LEN];
  int evt;
} t_ping_evt;

/*--------------------------------------------------------------------------*/
typedef struct t_chrono_act
{
  char name[MAX_NAME_LEN];
  int num;
  int is_tg;
  int switch_on;
} t_chrono_act;
/*--------------------------------------------------------------------------*/
void topo_repaint_request(void);

void eventfull_has_arrived(void)
{
  g_one_eventfull_has_arrived = 1;
  glob_eventfull_has_arrived = 0;
}


/****************************************************************************/
static int choice_pbi_flag(int ping_ok, int cga_ping_ok, int qmp_conn_ok)
{
  int result = flag_qmp_conn_ko;
  if (qmp_conn_ok)
    result = flag_qmp_conn_ok;
  if ((ping_ok) || (cga_ping_ok))
    result = flag_ping_ok;
  return result;
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static void timer_ping_evt_node(t_bank_item *bitem, int evt)
{
  switch (evt)
    {
    case vm_evt_ping_ok:
      bitem->pbi.pbi_node->node_evt_ping_ok = 1;
      break;
    case vm_evt_ping_ko:
      bitem->pbi.pbi_node->node_evt_ping_ok = 0;
      break;
    case vm_evt_cloonix_ga_ping_ok:
      bitem->pbi.pbi_node->node_evt_cga_ping_ok = 1;
      break;
    case vm_evt_cloonix_ga_ping_ko:
      bitem->pbi.pbi_node->node_evt_cga_ping_ok = 0;
      break;
    case vm_evt_qmp_conn_ok:
      bitem->pbi.pbi_node->node_evt_qmp_conn_ok = 1;
      break;
    case vm_evt_qmp_conn_ko:
      bitem->pbi.pbi_node->node_evt_qmp_conn_ok = 0;
      break;
    default:
      KOUT(" ");
    }
  bitem->pbi.flag = choice_pbi_flag(bitem->pbi.pbi_node->node_evt_ping_ok, 
                               bitem->pbi.pbi_node->node_evt_cga_ping_ok,
                               bitem->pbi.pbi_node->node_evt_qmp_conn_ok);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_ping_evt(void *data)
{
  t_bank_item *bitem;
  t_ping_evt *evt = (t_ping_evt *) data;
  bitem = look_for_node_with_id(evt->name);
  if (bitem)
    timer_ping_evt_node(bitem, evt->evt);
  else
    {
    bitem = look_for_cnt_with_id(evt->name);
    if (bitem)
      {
      if (evt->evt == vm_evt_cloonix_ga_ping_ok)
        bitem->pbi.pbi_cnt->cnt_evt_ping_ok = 1;
      else if (evt->evt == vm_evt_cloonix_ga_ping_ko)
        bitem->pbi.pbi_cnt->cnt_evt_ping_ok = 0;
      else
        KERR("ERROR %d", evt->evt);
      }
    }
  clownix_free(data, __FUNCTION__);
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
void ping_enqueue_evt(char *name, int evt)
{
  t_ping_evt *data = (t_ping_evt *) clownix_malloc(sizeof(t_ping_evt), 16); 
  memset(data, 0, sizeof(t_ping_evt));
  strncpy(data->name, name, MAX_NAME_LEN-1);
  data->evt = evt; 
  clownix_timeout_add(1, timer_ping_evt, (void *) data, NULL, NULL);
}
/*--------------------------------------------------------------------------*/


/*****************************************************************************/
//static void print_all_mallocs(void)
//{
//  int i;
//  unsigned long *mallocs;
//  printf("MALLOCS: ");
//  mallocs = get_clownix_malloc_nb();
//  for (i=0; i<MAX_MALLOC_TYPES; i++)
//    printf("%d:%02lu ", i, mallocs[i]);
//  printf("\n%d\n", get_nb_running_pids());
//  printf("\n%d, %d\n", get_nb_total_items(), get_max_edge_nb_per_item());
//}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void timeout_periodic_work(void *data)
{
  static int count_repaint = 0;
  static int count_move = 0;
  int nb = get_nb_total_items();
  count_repaint += 1;
  count_move += 1;
  if (nb < 200)
    {
    count_repaint = 0;
    count_move = 0;
    }
  else if (nb < 400)
    {
    if (count_repaint == 5) 
      count_repaint = 0;
    if (count_move == 2) 
      count_move = 0;
    }
  else if (nb < 800)
    {
    if (count_repaint == 10) 
      count_repaint = 0;
    if (count_move == 5) 
      count_move = 0;
    }
  else
    {
    if (count_repaint == 15) 
      count_repaint = 0;
    if (count_move == 10) 
      count_move = 0;
    }
  if (count_move == 0)
    move_manager_single_step();
  if (count_repaint == 0)
    topo_repaint_request();

  clownix_timeout_add(2, timeout_periodic_work, NULL, NULL, NULL);
}
/*---------------------------------------------------------------------------*/



/*****************************************************************************/
gboolean refresh_request_timeout (gpointer data)
{
  popup_timeout();
  clownix_timer_beat();
  if (g_one_eventfull_has_arrived)
    {
    glob_eventfull_has_arrived++;
    if (glob_eventfull_has_arrived >= 1000)
      KOUT("CONTACT LOST");
    }
  return TRUE;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void request_move_stop_go(int stop)
{
  if (stop)
    g_timeout_flipflop_freeze = 0;
  else
    g_timeout_flipflop_freeze = 1;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
int get_move_freeze(void)
{
  return (!g_timeout_flipflop_freeze);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void request_trace_item(int is_tg, char *name, int num)
{
  t_chrono_act *ca;
  ca = (t_chrono_act *)clownix_malloc(sizeof(t_chrono_act),16);
  memset(ca, 0, sizeof(t_chrono_act));
  strcpy(ca->name, name);  
  ca->num = num; 
  ca->is_tg = is_tg; 
  ca->switch_on = 1;
}
/*---------------------------------------------------------------------------*/


