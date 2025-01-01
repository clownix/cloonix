/*****************************************************************************/
/*    Copyright (C) 2006-2025 clownix@clownix.net License AGPL-3             */
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
void local_dropbear_add_llid(int dido_llid);
void local_dropbear_del_llid(int dido_llid);
int local_dropbear_init_dido(int dido_llid);
void local_dropbear_x11_open_to_agent(int dido_llid, int sub_dido_idx);
void local_dropbear_receive_from_client(int dido_llid, int len, char *buf);
void local_dropbear_receive_x11_from_client(int idx_display_sock_x11,
                                            int sub_dido_idx,
                                            int len, char *buf);
void local_dropbear_init(void);
/*--------------------------------------------------------------------------*/

