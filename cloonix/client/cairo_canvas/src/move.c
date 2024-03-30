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

#define REPULSE_NODE 1000000.0
#define REPULSE_ETH 1000.0
#define SPRING_K  2.0
#define SPRING_SIZE 35
#define FRICTION_LAN 60
#define FRICTION_NODE 30
#define CIRCLE1 20000.0
#define CIRCLE2 500000.0
#define CIRCLE3 1000000.0 
#define IS_CLOSE_RANGE_ETH 70
#define IS_CLOSE_RANGE_NODE 200
#define MAX_VELOCITY 100.0
#define MIN_VELOCITY 0.1
#define MAX_ACCELERATION 25.0
#define MIN_ACCELERATION 0.1
#define ACCELERATION_DIVISER 200

extern CrCanvas *glob_canvas;

/****************************************************************************/

/****************************************************************************/
static void set_random_repulsion_force(t_bank_item *bitem, int factor)
{     
  double negx=1, negy=1, fx, fy;
  if (rand() % 2)
    negx = -1;
  if (rand() % 2)
    negy = -1;
  fx = negx*((rand()%factor) + 1);
  fy = negy*((rand()%factor) + 1);
  if (bitem->bank_type == bank_type_eth)
    {
    bitem->pbi.force_eth_x += fx;
    bitem->pbi.force_eth_y += fy;
    }
  else
    {
    bitem->pbi.force_x += fx;
    bitem->pbi.force_y += fy;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void set_horizon_repulse_force(t_bank_item *bitem)
{
  double how_far;
  if (bitem->bank_type != bank_type_eth)
    { 
    how_far = (bitem->pbi.position_x * bitem->pbi.position_x) +
              (bitem->pbi.position_y * bitem->pbi.position_y);
    if (how_far > CIRCLE3)
      {
      bitem->pbi.force_x -= (bitem->pbi.position_x);
      bitem->pbi.force_y -= (bitem->pbi.position_y);
      }
    else if (how_far > CIRCLE2)
      {
      bitem->pbi.force_x -= (bitem->pbi.position_x)/2;
      bitem->pbi.force_y -= (bitem->pbi.position_y)/2;
      }
    else if (how_far > CIRCLE1)
      {
      bitem->pbi.force_x -= (bitem->pbi.position_x)/4;
      bitem->pbi.force_y -= (bitem->pbi.position_y)/4;
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void set_repulse_force_eth(t_bank_item *bitem, t_bank_item *cur)
{     
  double dist;
  double vx, vy, cvx, cvy;
  if (bitem->bank_type != bank_type_eth)
    KERR("ERROR %s %d %d", bitem->name, bitem->num, bitem->bank_type);
  else if (cur->bank_type != bank_type_eth)
    KERR("ERROR %s %d %d", cur->name, cur->num, cur->bank_type);
  else
    {
    vx = bitem->pbi.position_x - cur->pbi.position_x;
    vy = bitem->pbi.position_y - cur->pbi.position_y;
    dist = vx*vx + vy*vy;
    if (dist < IS_CLOSE_RANGE_ETH)
      {
      set_random_repulsion_force(bitem, REPULSE_ETH);
      }
    else
      {
      cvx = (vx*REPULSE_ETH)/(dist);
      cvy = (vy*REPULSE_ETH)/(dist);
      bitem->pbi.force_eth_x += cvx;
      bitem->pbi.force_eth_y += cvy; 
      }
    }
}     
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void set_repulse_force_group(t_bank_item *bitem, t_bank_item *cur)
{
  double dist, vx, vy, cvx, cvy;
  if (bitem->bank_type == bank_type_eth)
    KERR("ERROR %s %d", bitem->name, bitem->num);
  else if (cur->bank_type == bank_type_eth)
    KERR("ERROR %s %d", cur->name, cur->num);
  else
    {
    vx = bitem->pbi.position_x - cur->pbi.position_x;
    vy = bitem->pbi.position_y - cur->pbi.position_y;
    dist = vx*vx + vy*vy;
    if (dist < IS_CLOSE_RANGE_NODE)
      {
      set_random_repulsion_force(bitem, REPULSE_NODE);
      }
    else
      {
      cvx = (vx*REPULSE_NODE)/(dist);
      cvy = (vy*REPULSE_NODE)/(dist);
      bitem->pbi.force_x += cvx;
      bitem->pbi.force_y += cvy;
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void get_force_from_dist(double dist, double *vx, double *vy)
{
  double spring_k = SPRING_K;
  double spring_size = SPRING_SIZE;
  double modifier = 1;
  if (dist > 2*spring_size)
    modifier = 40;
  else if (dist > 1.9*spring_size)
    modifier = 35;
  else if (dist > 1.8*spring_size)
    modifier = 30;
  else if (dist > 1.7*spring_size)
    modifier = 25;
  else if (dist > 1.6*spring_size)
    modifier = 20;
  else if (dist > 1.5*spring_size)
    modifier = 16;
  else if (dist > 1.4*spring_size)
    modifier = 10;
  else if (dist > 1.3*spring_size)
    modifier = 8;
  else if (dist > 1.2*spring_size)
    modifier = 4;
  else if (dist > 1.1*spring_size)
    modifier = 2;
  *vx = (dist - spring_size) * spring_k * (*vx) * modifier;
  *vy = (dist - spring_size) * spring_k * (*vy) * modifier;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void edge_exert_forces_not_eth(t_bank_item *eth, t_bank_item *lan)
{     
  double vx, vy, dist;
  if ((eth->bank_type != bank_type_eth) && (eth->bank_type != bank_type_sat))
    KERR("ERROR %s %d %d", eth->name, eth->num, eth->bank_type);
  else if (lan->bank_type != bank_type_lan)
    KERR("ERROR %s %d %d", lan->name, lan->num, lan->bank_type);
  else
    {
    vx = eth->pbi.position_x - lan->pbi.position_x;
    vy = eth->pbi.position_y - lan->pbi.position_y;
    dist = sqrt(vx*vx + vy*vy);
    get_force_from_dist(dist, &vx, &vy);
    lan->pbi.force_x += vx;
    eth->pbi.force_x += -vx;
    lan->pbi.force_y += vy;
    eth->pbi.force_y += -vy;
    }
} 
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void edge_exert_forces_eth(t_bank_item *eth, t_bank_item *lan)
{
  double vx, vy;
  double spring_k = SPRING_K;
  if (eth->bank_type != bank_type_eth)
    KERR("ERROR %d", eth->bank_type);
  else if (lan->bank_type != bank_type_lan)
    KERR("ERROR %d", lan->bank_type);
  else
    {
    vx = eth->pbi.position_x - lan->pbi.position_x;
    vy = eth->pbi.position_y - lan->pbi.position_y;
    vx = vx * spring_k;
    vy = vy * spring_k;
    eth->pbi.force_eth_x += -vx;
    eth->pbi.force_eth_y += -vy;
    }
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
static void min_max_the_acceleration(double *acc_x, double *acc_y)
{
  if (*acc_x > MAX_ACCELERATION)
    *acc_x = MAX_ACCELERATION;
  if (*acc_x < -MAX_ACCELERATION)
    *acc_x = -MAX_ACCELERATION;
  if (*acc_y > MAX_ACCELERATION)
    *acc_y = MAX_ACCELERATION;
  if (*acc_y < -MAX_ACCELERATION)
    *acc_y = -MAX_ACCELERATION;
  if (((*acc_x) > -MIN_ACCELERATION) && ((*acc_x) < MIN_ACCELERATION))
    *acc_x = 0;
  if (((*acc_y) > -MIN_ACCELERATION) && ((*acc_y) < MIN_ACCELERATION))
    *acc_y = 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void max_the_velocity(t_bank_item *bitem)
{
  double level1=MAX_VELOCITY/2;
  double level2=MAX_VELOCITY/2 + MAX_VELOCITY/4;
  double level3=MAX_VELOCITY/2 + MAX_VELOCITY/4 + MAX_VELOCITY/8;
  double level4=MAX_VELOCITY;

  if (bitem->pbi.velocity_x >= level4)
    bitem->pbi.velocity_x = level4;
  else if (bitem->pbi.velocity_x >= level3)
    bitem->pbi.velocity_x = level3;
  else if (bitem->pbi.velocity_x >= level2)
    bitem->pbi.velocity_x = level2;
  else if (bitem->pbi.velocity_x >= level1)
    bitem->pbi.velocity_x = level1;

  if (bitem->pbi.velocity_x <= -level4)
    bitem->pbi.velocity_x = -level4;
  else if (bitem->pbi.velocity_x <= -level3)
    bitem->pbi.velocity_x = -level3;
  else if (bitem->pbi.velocity_x <= -level2)
    bitem->pbi.velocity_x = -level2;
  else if (bitem->pbi.velocity_x <= -level1)
    bitem->pbi.velocity_x = -level1;

  if (bitem->pbi.velocity_y >= level4)
    bitem->pbi.velocity_y = level4;
  else if (bitem->pbi.velocity_y >= level3)
    bitem->pbi.velocity_y = level3;
  else if (bitem->pbi.velocity_y >= level2)
    bitem->pbi.velocity_y = level2;
  else if (bitem->pbi.velocity_y >= level1)
    bitem->pbi.velocity_y = level1;

  if (bitem->pbi.velocity_y <= -level4)
    bitem->pbi.velocity_y = -level4;
  else if (bitem->pbi.velocity_y <= -level3)
    bitem->pbi.velocity_y = -level3;
  else if (bitem->pbi.velocity_y <= -level2)
    bitem->pbi.velocity_y = -level2;
  else if (bitem->pbi.velocity_y <= -level1)
    bitem->pbi.velocity_y = -level1;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void move_single_step (t_bank_item *bitem)
{
  double acceleration_x, acceleration_y;
  acceleration_x = bitem->pbi.force_x / bitem->pbi.mass;
  acceleration_y = bitem->pbi.force_y / bitem->pbi.mass;

  acceleration_x /= ACCELERATION_DIVISER;
  acceleration_y /= ACCELERATION_DIVISER;

  min_max_the_acceleration(&acceleration_x, &acceleration_y);
  bitem->pbi.velocity_x += acceleration_x;
  bitem->pbi.velocity_y += acceleration_y;
  max_the_velocity(bitem);
  if (bitem->bank_type == bank_type_lan)
    {
    bitem->pbi.velocity_x -= (bitem->pbi.velocity_x * FRICTION_LAN)/100;
    bitem->pbi.velocity_y -= (bitem->pbi.velocity_y * FRICTION_LAN)/100;
    }
  else
    {
    bitem->pbi.velocity_x -= (bitem->pbi.velocity_x * FRICTION_NODE)/100;
    bitem->pbi.velocity_y -= (bitem->pbi.velocity_y * FRICTION_NODE)/100;
    }
  if ((bitem->pbi.velocity_x > -MIN_VELOCITY) &&
      (bitem->pbi.velocity_x < MIN_VELOCITY))
    bitem->pbi.velocity_x = 0;
  if ((bitem->pbi.velocity_y > -MIN_VELOCITY) &&
      (bitem->pbi.velocity_y < MIN_VELOCITY))
    bitem->pbi.velocity_y = 0;
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
  if (bitem->bank_type != bank_type_eth)
    KERR("ERROR %d", bitem->bank_type);
  else
    {
    mat = cr_item_get_matrix(CR_ITEM(bitem->root));
    cairo_matrix_transform_point(mat, &x1, &y1);
    if (authorized_to_move(bitem))
      move_manager_rotate(bitem, x1,  y1 );
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void move_single_step_eth (t_bank_item *bitem)
{ 
  double  acc_x, acc_y, x1, y1;
  if (bitem->bank_type != bank_type_eth)
    KERR("ERROR %d", bitem->bank_type);
  else
    {
    acc_x = bitem->pbi.force_eth_x / bitem->pbi.mass;
    acc_y = bitem->pbi.force_eth_y / bitem->pbi.mass;
    x1 = bitem->pbi.position_x + acc_x;
    y1 = bitem->pbi.position_y + acc_y;
    move_bitem_eth (bitem, x1, y1);
    bitem->pbi.force_eth_x = 0.0;
    bitem->pbi.force_eth_y = 0.0;
    }
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
static void spring_force_sat(t_bank_item *bitem)
{         
  t_list_bank_item *cur;
  cur = bitem->head_edge_list;
  while (cur)
    { 
    edge_exert_forces_not_eth(cur->bitem->att_eth, cur->bitem->att_lan);
    cur = cur->next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void spring_force_node(t_bank_item *bitem, t_bank_item *eth)
{
  t_list_bank_item *cur;
  cur = eth->head_edge_list;
  while (cur)
    {
    edge_exert_forces_not_eth(cur->bitem->att_eth, cur->bitem->att_lan);
    cur = cur->next;
    }
  bitem->pbi.force_x += eth->pbi.force_x;
  bitem->pbi.force_y += eth->pbi.force_y;
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
  if (eth->bank_type != bank_type_eth)
    KERR("ERROR %d", eth->bank_type);
  else
    {
    while (cur)
      {
      if (cur->bitem->bank_type != bank_type_eth)
        KERR("ERROR %d", cur->bitem->bank_type);
      else
        {
        if (eth != cur->bitem)
          set_repulse_force_eth(eth, cur->bitem);
        }
      cur = cur->next;
      }
    move_single_step_eth(eth);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void spring_force_item(t_bank_item *bitem)
{
  t_list_bank_item *cur;
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
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void repulsion_force_item(t_bank_item *bitem)
{
  t_bank_item *cur = bitem->group_head;
  while (cur)
    {
    if ((bitem != cur) && (cur->bank_type != bank_type_eth))
      set_repulse_force_group(bitem, cur);
    cur = cur->group_next;
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

      if ((cur->bank_type != bank_type_eth) &&
          (cur->bank_type != bank_type_edge) &&
          (cur->bank_type != bank_type_lan)) 
        {
        if (cur->bank_type == bank_type_sat)
          spring_force_sat(cur);
        else
          spring_force_item(cur);
        }
      cur = cur->glob_next;
      }
    group_cur = get_head_connected_groups();
    while (group_cur)
      {
      cur = group_cur->bitem->group_head;

      while (cur)
        {
        if (cur->bank_type != bank_type_eth)
          repulsion_force_item(cur);
        cur = cur->group_next;
        }
      group_cur = group_cur->next;
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
}
/*--------------------------------------------------------------------------*/


