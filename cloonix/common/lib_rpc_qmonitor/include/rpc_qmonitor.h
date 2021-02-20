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
void doors_io_qmonitor_xml_init(t_llid_tx llid_tx);
int  doors_io_qmonitor_decoder (int llid, int len, char *buf);
/*---------------------------------------------------------------------------*/
void send_qmonitor(int llid, int tid, char *name, char *data);
void recv_qmonitor(int llid, int tid, char *name, char *data);
/*---------------------------------------------------------------------------*/
void send_sub2qmonitor(int llid, int tid, char *name, int on_off);
void recv_sub2qmonitor(int llid, int tid, char *name, int on_off);
/*---------------------------------------------------------------------------*/


