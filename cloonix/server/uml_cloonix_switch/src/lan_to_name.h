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
enum {
  item_cnt=7,
  item_kvm,
  item_tap,
  item_phy,
  item_nat,
  item_a2b,
  item_c2c,
};

/*---------------------------------------------------------------------------*/
typedef struct t_item_mac
{
  char mac[MAX_NAME_LEN];
  struct t_item_mac *next;
} t_item_mac;
/*---------------------------------------------------------------------------*/
typedef struct t_item_el
{
  int item_type;
  char item_name[MAX_NAME_LEN];
  int item_num;
  t_item_mac *head_mac;
  struct t_item_el *prev;
  struct t_item_el *next;
} t_item_el;
/*---------------------------------------------------------------------------*/


t_item_el *lan_get_head_item(char *name);
int  lan_add_name(char *name, int item_type, char *item_name, int item_num);
void lan_add_mac(char *name, int item_type, char *item_name, int item_num, char *mac);
int  lan_del_name(char *name, int item_type, char *item_name, int item_num);
int  lan_get_with_name(char *name);
void lan_init(void);
/*---------------------------------------------------------------------------*/



