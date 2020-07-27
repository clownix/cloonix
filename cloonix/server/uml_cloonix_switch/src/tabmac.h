/*****************************************************************************/
/*    Copyright (C) 2006-2020 clownix@clownix.net License AGPL-3             */
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
typedef struct t_tabmac_snf
{
  char name[MAX_NAME_LEN];
  int  num;
  char mac[MAX_NAME_LEN];  
} t_tabmac_snf;

int tabmac_update_for_d2d(t_peer_mac tabmac[MAX_PEER_MAC]);
int tabmac_update_for_snf(t_tabmac_snf tabmac_snf[MAX_PEER_MAC]);
void tabmac_process_possible_change(void);
/*--------------------------------------------------------------------------*/

