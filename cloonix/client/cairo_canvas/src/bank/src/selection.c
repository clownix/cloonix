/*****************************************************************************/
/*    Copyright (C) 2006-2022 clownix@clownix.net License AGPL-3             */
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
#include <string.h>
#include <stdlib.h>
#include "io_clownix.h"
#include "rpc_clownix.h"
#include "bank.h"
#include "bank_item.h"
#include "commun_consts.h"
#include "external_bank.h"
#include "interface.h"

/****************************************************************************/
static t_bank_item *selectioned_lan;


/****************************************************************************/
static int selectioned_resolve(t_bank_item *bitem)
{
  int result = 0;
  switch (bitem->bank_type)
    {
    case bank_type_eth:
      if (selectioned_lan)
        {
        if ((!selectioned_lan->name) || (!strlen(selectioned_lan->name)))
          KOUT(" ");
        if (bitem->att_node->bank_type == bank_type_sat)
          {
          if (bitem->att_node->pbi.endp_type != endp_type_a2b)
            KERR("%s %d", bitem->name, bitem->att_node->pbi.endp_type);
          else
            {
            to_cloonix_switch_create_edge(bitem->name, bitem->num, selectioned_lan->name); 
            }
          }
        else
          {
          to_cloonix_switch_create_edge(bitem->name, bitem->num, selectioned_lan->name); 
          }
        selectioned_lan->pbi.flag = flag_normal;
        selectioned_lan = NULL;
        result = 1;
        }
      break;
    case bank_type_sat:
      if (selectioned_lan)
        {
        if ((!selectioned_lan->name) || (!strlen(selectioned_lan->name)))
          KOUT(" ");
        if (selectioned_lan->pbi.endp_type != endp_type_a2b)
          {
          to_cloonix_switch_create_edge(bitem->name, 0, selectioned_lan->name); 
          }
        selectioned_lan->pbi.flag = flag_normal;
        selectioned_lan = NULL;
        result = 1;
        }
      break;

    default:
      break;
    }
  return result;
}
/*--------------------------------------------------------------------------*/



/****************************************************************************/
static void selectioned_lan_flip_flop(t_bank_item *bitem)
{
  if (!selectioned_lan)
    {
    selectioned_lan = bitem;
    selectioned_lan->pbi.flag = flag_attach;
    }
  else
    {
    if (selectioned_lan && (selectioned_lan == bitem))
      {
      selectioned_lan->pbi.flag = flag_normal;
      selectioned_lan = NULL;
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void selectioned_item_delete(t_bank_item *bitem)
{
  switch(bitem->bank_type)
    {
    case bank_type_node:
    case bank_type_cnt:
    case bank_type_sat:
    case bank_type_eth:
      break;
    case bank_type_lan:
      if (bitem && (selectioned_lan == bitem))
        {
        selectioned_lan->pbi.flag = flag_normal;
        selectioned_lan = NULL;
        }
      break;

    default:
      KOUT("%d", bitem->bank_type);
      break;
    }
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
void selectioned_mouse_button_1_press(t_bank_item *bitem, double x, double y)
{
  bitem->pbi.x = x;
  bitem->pbi.y = y;
  selectioned_resolve(bitem);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void selectioned_flip_flop(t_bank_item *bitem)
{
  if (bitem->bank_type == bank_type_lan)
    selectioned_lan_flip_flop(bitem);
  else
    KOUT(" ");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int is_selectable(t_bank_item *bitem)
{
  int result;
  if (bitem->bank_type == bank_type_lan)
    result = (selectioned_lan == bitem);
  else
    KOUT(" ");
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void selectioned_item_init(void)
{
  selectioned_lan = NULL;
}
/*--------------------------------------------------------------------------*/


