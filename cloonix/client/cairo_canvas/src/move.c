/*****************************************************************************/
/*    Copyright (C) 2006-2018 cloonix@cloonix.net License AGPL-3             */
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
#include <libcrcanvas.h>
#include <math.h>
#include <stdlib.h>
#include "io_clownix.h"
#include "rpc_clownix.h"
#include "commun_consts.h"
#include "bank.h"
#include "move.h"
#include "interface.h"
#include "external_bank.h"
#include "main_timer_loop.h"

#define REPULSE_K 300.0
#define SPRING_K 5.0
#define FRICTION  10.0
#define SPRING_SIZE 35
#define SPRING_MIN_ACTIV 1 
#define HORIZON_DIA 10000000.0 
#define MAX_ACCEL 3
#define MAX_SPEED 20
#define MIN_SPEED 0.1 

extern CrCanvas *glob_canvas;

static double horizon_dia;
static double repulse_k; 
static double spring_k; 
static double friction; 
static double spring_size; 
/****************************************************************************/

/****************************************************************************/
static void set_horizon_repulse_force(t_bank_item *bitem)
{
  double how_far;
  double factor = repulse_k;
  how_far = (bitem->pbi.position_x * bitem->pbi.position_x) +
            (bitem->pbi.position_y * bitem->pbi.position_y);
  if (how_far > horizon_dia)
    {
    bitem->pbi.force_x -= (bitem->pbi.position_x/horizon_dia)*factor;
    bitem->pbi.force_y -= (bitem->pbi.position_y/horizon_dia)*factor;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int is_lan(t_bank_item *bitem)
{
  if (bitem->bank_type == bank_type_lan)
    return 1;
  return 0; 
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void set_repulse_force(t_bank_item *bitem, t_bank_item *cur)
{
  double neg, dist,  factor;
  double vx, vy, cvx, cvy;
  vx = bitem->pbi.position_x - cur->pbi.position_x;
  vy = bitem->pbi.position_y - cur->pbi.position_y;
  dist = vx*vx + vy*vy;
  if ((is_lan(bitem)) || (is_lan(cur))) 
    factor = repulse_k/100;
  else
    factor = repulse_k;
  if (dist)
    {
    cvx = (vx/dist)*factor;
    cvy = (vy/dist)*factor;
    if ((cvx > SPRING_MIN_ACTIV) || (cvx < -SPRING_MIN_ACTIV))
      bitem->pbi.force_x += cvx;
    if ((cvy > SPRING_MIN_ACTIV) || (cvy < -SPRING_MIN_ACTIV))
      bitem->pbi.force_y += cvy;
    }
  else
    {
    if (rand() % 2)
      neg = -1;
    else
      neg = 1;
    bitem->pbi.force_x += neg * factor * (rand() % 200 + 1)/200;
    if (rand() % 2)
      neg = -1;
    else
      neg = 1;
    bitem->pbi.force_y += neg * factor * (rand() % 200 + 1)/200;
    }
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static void eth_repulse_force(t_bank_item *eth, t_bank_item *neigh)
{
  double neg;
  double vx, vy, dist;
  vx = eth->pbi.tx - neigh->pbi.tx;
  vy = eth->pbi.ty - neigh->pbi.ty;
  dist = vx*vx + vy*vy;
  if (dist)
    {
    eth->pbi.force_x += (vx/dist)*500;
    eth->pbi.force_y += (vy/dist)*500;
    }
  else
    {
    if (rand() % 2)
      neg = -1;
    else
      neg = 1;
    eth->pbi.force_x += neg * 0.1;
    if (rand() % 2)
      neg = -1;
    else
      neg = 1;
    eth->pbi.force_y += neg * 0.1;
    }
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static void edge_exert_forces (t_bank_item *eth, t_bank_item *lan)
{
  double vx, vy, dist;
  vx = eth->pbi.position_x - lan->pbi.position_x;
  vy = eth->pbi.position_y - lan->pbi.position_y;
  dist = sqrt(vx*vx + vy*vy);
  if (dist)
    {
    vx = ((dist - spring_size) * vx * spring_k)/dist;
    vy = ((dist - spring_size) * vy * spring_k)/dist;
    }
  else
    {
    vx = spring_size * vx * spring_k;
    vy = spring_size * vy * spring_k;
    }

  if ((vx > SPRING_MIN_ACTIV) || (vx < -SPRING_MIN_ACTIV))
    {
    lan->pbi.force_x += vx;
    eth->pbi.force_x += -vx;
    }
  if ((vy > SPRING_MIN_ACTIV) || (vy < -SPRING_MIN_ACTIV))
    {
    lan->pbi.force_y += vy;
    eth->pbi.force_y += -vy;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void edge_exert_forces_eth (t_bank_item *eth, t_bank_item *lan)
{
  double vx, vy;
  vx = eth->pbi.position_x - lan->pbi.position_x;
  vy = eth->pbi.position_y - lan->pbi.position_y;
  vx = vx * spring_k/2;
  vy = vy * spring_k/2;
  if ((vx > SPRING_MIN_ACTIV) || (vx < -SPRING_MIN_ACTIV))
    eth->pbi.force_x += -vx;
  if ((vy > SPRING_MIN_ACTIV) || (vy < -SPRING_MIN_ACTIV))
    eth->pbi.force_y += -vy;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int grabbed_is_in_edge_list(t_list_bank_item *head_edges)
{
  int result = 0;
  t_list_bank_item *cur = head_edges;
  while (cur)
    {
    if (cur->bitem->pbi.grabbed)
      {
      result = 1;
      break;
      }
    cur = cur->next;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int authorized_to_move(t_bank_item *bitem)
{
  int result = 1;
  t_list_bank_item *cur;
  if (bitem->pbi.grabbed)
    result = 0;
  else
    {
    if (grabbed_is_in_edge_list(bitem->head_edge_list))
      result = 0;
    cur = bitem->head_eth_list;
    while (cur)
      {
      if (cur->bitem->pbi.grabbed)
        {
        result = 0;
        break;
        }
      if (grabbed_is_in_edge_list(cur->bitem->head_edge_list))
        {
        result = 0;
        break;
        }
      cur = cur->next;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static double speed_control(double speed)
{
  double result = speed; 
  if (result > MAX_SPEED)
    result = MAX_SPEED;
  if (result < -MAX_SPEED)
    result = -MAX_SPEED;
  if ((result > 0) && (result < MIN_SPEED))
    result = 0;
  if ((result < 0) && (result > -MIN_SPEED))
    result = 0;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static double accel_control(double accel)
{
  double result = accel;
  if (result > MAX_ACCEL)
    result = MAX_ACCEL;
  if (result < -MAX_ACCEL)
    result = -MAX_ACCEL;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void move_single_step (t_bank_item *bitem)
{
  double acceleration_x, acceleration_y;
  double frict;
  frict = friction;
  if (bitem->pbi.force_x > 0)
    {
    bitem->pbi.force_x -= bitem->pbi.velocity_x * frict;
    if (bitem->pbi.force_x < 0)
      bitem->pbi.force_x /= 2;
    }
  else if (bitem->pbi.force_x < 0)
    {
    bitem->pbi.force_x -= bitem->pbi.velocity_x * frict;
    if (bitem->pbi.force_x > 0)
      bitem->pbi.force_x /= 2;
    }
  if (bitem->pbi.force_y > 0)
    {
    bitem->pbi.force_y -= bitem->pbi.velocity_y * frict;
    if (bitem->pbi.force_y < 0)
      bitem->pbi.force_y /= 2;
    }
  else if (bitem->pbi.force_y < 0)
    {
    bitem->pbi.force_y -= bitem->pbi.velocity_y * frict;
    if (bitem->pbi.force_y > 0)
      bitem->pbi.force_y /= 2;
    }
  if (is_lan(bitem)) 
    {
    bitem->pbi.force_x /= 20;
    bitem->pbi.force_y /= 20;
    }
  acceleration_x = bitem->pbi.force_x / bitem->pbi.mass;
  acceleration_y = bitem->pbi.force_y / bitem->pbi.mass;
  acceleration_x = accel_control(acceleration_x);
  acceleration_y = accel_control(acceleration_y);
  bitem->pbi.velocity_x += acceleration_x;
  bitem->pbi.velocity_y += acceleration_y;
  bitem->pbi.velocity_x = speed_control(bitem->pbi.velocity_x);
  bitem->pbi.velocity_y = speed_control(bitem->pbi.velocity_y);
  if (authorized_to_move(bitem))
    move_manager_translate(bitem, bitem->pbi.velocity_x, bitem->pbi.velocity_y);
  bitem->pbi.force_x = 0.0;
  bitem->pbi.force_y = 0.0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void move_bitem_eth (t_bank_item *bitem, double x1, double y1)
{
  cairo_matrix_t *mat;
  mat = cr_item_get_matrix(CR_ITEM(bitem->root));
  cairo_matrix_transform_point(mat, &x1, &y1);
  if (authorized_to_move(bitem))
    move_manager_rotate(bitem, x1,  y1 );
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void move_single_step_eth (t_bank_item *bitem)
{ 
  double  acc_x, acc_y, x1, y1;
  acc_x = bitem->pbi.force_x / bitem->pbi.mass;
  acc_y = bitem->pbi.force_y / bitem->pbi.mass;
  x1 = bitem->pbi.position_x + acc_x;
  y1 = bitem->pbi.position_y + acc_y;
  move_bitem_eth (bitem, x1, y1);
  bitem->pbi.force_x = 0.0;
  bitem->pbi.force_y = 0.0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void move_manager_update_item(t_bank_item *bitem)
{
  t_list_bank_item *cur;
  cur = bitem->head_eth_list;
  while (cur)
    {
    topo_get_absolute_coords(cur->bitem); 
    cur = cur->next;
    }
  topo_get_absolute_coords(bitem); 
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void spring_force_node(t_bank_item *node, t_bank_item *eth)
{
  t_list_bank_item *cur;
  cur = eth->head_edge_list;
  while (cur)
    {
    edge_exert_forces (cur->bitem->att_eth, cur->bitem->att_lan);
    cur = cur->next;
    }
  node->pbi.force_x += eth->pbi.force_x;
  node->pbi.force_y += eth->pbi.force_y;
  eth->pbi.force_x = 0;
  eth->pbi.force_y = 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void spring_force_eth(t_bank_item *eth)
{
  t_list_bank_item *cur;
  cur = eth->head_edge_list;
  while (cur)
    {
    edge_exert_forces_eth (cur->bitem->att_eth, cur->bitem->att_lan);
    cur = cur->next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void repulsion_force_amongs_eth(t_list_bank_item *head_eth, 
                                        t_bank_item *eth)
{
  t_list_bank_item *cur = head_eth;
  while (cur)
    {
    if (eth != cur->bitem)
      eth_repulse_force(eth, cur->bitem);
    cur = cur->next;
    }
  move_single_step_eth(eth);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void spring_force_item(t_bank_item *bitem)
{
  t_list_bank_item *cur;
  if ((bitem->bank_type == bank_type_node) ||
      ((bitem->bank_type == bank_type_sat) && 
        (bitem->pbi.mutype == endp_type_a2b)))
    {
    cur = bitem->head_eth_list;
    while (cur)
      {
      spring_force_node(bitem, cur->bitem);
      cur = cur->next;
      }
    cur = bitem->head_eth_list;
    while (cur)
      {
      spring_force_eth(cur->bitem);
      cur = cur->next;
      }
    cur = bitem->head_eth_list;
    while (cur)
      {
      repulsion_force_amongs_eth(bitem->head_eth_list, cur->bitem);
      cur = cur->next;
      }
    }
  else
    {
    cur = bitem->head_edge_list;
    while (cur)
      {
      edge_exert_forces(cur->bitem->att_eth, cur->bitem->att_lan);
      cur = cur->next;
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void repulsion_force_item(t_bank_item *bitem)
{
  t_bank_item *cur = bitem->group_head;
  while (cur)
    {
    if (bitem != cur)
      set_repulse_force(bitem, cur);
    cur = cur->group_next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void set_close_range_repulse_force(t_bank_item *bitem, t_bank_item *cur)
{
  double neg, dist,  factor, min_dist;
  double vx, vy;
  vx = bitem->pbi.position_x - cur->pbi.position_x;
  vy = bitem->pbi.position_y - cur->pbi.position_y;
  dist = vx*vx + vy*vy;
  factor = repulse_k/100;
  if (bitem->bank_type == bank_type_node) 
    min_dist = 2000;
  else
    min_dist = 300;


  if (dist == 0)
    {
    if (rand() % 2)
      neg = -1;
    else
      neg = 1;
    bitem->pbi.force_x += neg * factor;
    if (rand() % 2)
      neg = -1;
    else
      neg = 1;
    bitem->pbi.force_y += neg * factor;
    }
  else if (dist < min_dist)
    {
    if (vx < 0)
      vx = -1;
    else
      vx = 1;
    if (vy < 0)
      vy = -1;
    else
      vy = 1;
    bitem->pbi.force_x += vx*factor;
    bitem->pbi.force_y += vy*factor;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void close_range_repulse_force(t_bank_item *bitem)
{
  t_bank_item *cur;
  cur = get_first_glob_bitem();
  while (cur)
    {
    if ((cur != bitem) && (cur->bank_type != bank_type_eth)) 
      set_close_range_repulse_force(bitem, cur);
    cur = cur->glob_next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void move_manager_single_step(void)
{
  t_bank_item *cur;
  t_list_bank_item *group_cur;
  if (!get_move_freeze())
    {
    cur = get_first_glob_bitem();
    while (cur)
      {
      if (cur->bank_type != bank_type_eth)  
        spring_force_item(cur);
      cur = cur->glob_next;
      }
    group_cur = get_head_connected_groups();
    while (group_cur)
      {
      cur = group_cur->bitem->group_head;
      while (cur)
        {
        repulsion_force_item(cur);
        cur = cur->group_next;
        }
      group_cur = group_cur->next;
      }
  
    cur = get_first_glob_bitem();
    while (cur)
      {
      if (cur->bank_type != bank_type_eth) 
        close_range_repulse_force(cur);
      cur = cur->glob_next;
      }
    cur = get_first_glob_bitem();
    while (cur)
      {
      set_horizon_repulse_force(cur);
      if (cur->bank_type != bank_type_eth)  
        move_single_step (cur);
      cur = cur->glob_next;
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void move_init(void)
{
  horizon_dia = HORIZON_DIA;
  repulse_k   = REPULSE_K;
  spring_k    = SPRING_K;
  friction    = FRICTION;
  spring_size = SPRING_SIZE;

}
/*--------------------------------------------------------------------------*/


