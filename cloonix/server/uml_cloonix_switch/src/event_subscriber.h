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
enum
{
  sub_evt_min,
  sub_evt_print,
  sub_evt_sys,
  sub_evt_topo,
  topo_small_event,
  sub_evt_max,
};

typedef struct t_small_evt
{
  char name[MAX_NAME_LEN];
  char param1[MAX_PATH_LEN];
  char param2[MAX_PATH_LEN];
  int evt;
}t_small_evt;

void event_break_of_llid_connection(int llid);
void event_print (const char * format, ...);
void event_subscriber_init(void);
void event_subscribe(int sub_evt, int llid, int tid);
void event_unsubscribe(int sub_evt, int llid);
void event_subscriber_send(int sub_evt, void *info);
/*---------------------------------------------------------------------------*/

