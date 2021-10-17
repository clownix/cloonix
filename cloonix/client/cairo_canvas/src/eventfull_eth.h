/*****************************************************************************/
/*    Copyright (C) 2006-2021 clownix@clownix.net License AGPL-3             */
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
typedef struct t_eventfull
{
  int nb_endp;
  t_eventfull_endp *endp;
} t_eventfull;
/*---------------------------------------------------------------------------*/
void event_full_timeout_blink_off(void);
/*---------------------------------------------------------------------------*/
void eventfull_arrival(int nb_endp, t_eventfull_endp *endp);
/*---------------------------------------------------------------------------*/
void eventfull_obj_create(char *name);
/*---------------------------------------------------------------------------*/
void eventfull_obj_delete(char *name);
/*---------------------------------------------------------------------------*/
void eventfull_init(void);
/*---------------------------------------------------------------------------*/

