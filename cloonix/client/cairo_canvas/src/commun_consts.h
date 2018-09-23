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
#define MAX_TITLE 80
#define MAX_TEXT MAX_TITLE*20
#define C2C_DIA 30
#define INTF_DIA (NODE_DIA/3)
#define TAP_DIA 30
#define MOVE_CURSOR_MAX 100.0
#define NAT_RAD 14
#define SNIFF_RAD 15



/****************************************************************************/
enum
{
  flag_normal,
  flag_ping_ok,
  flag_ga_ping_ok,
  flag_dtach_launch_ko,
  flag_dtach_launch_ok,
  flag_select,
  flag_attach,
  flag_trace,
};
/*--------------------------------------------------------------------------*/
