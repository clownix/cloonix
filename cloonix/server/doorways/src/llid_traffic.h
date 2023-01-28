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
void llid_traf_tx_to_client(char *name, int dido_llid,  
                            int len, int type, int val, 
                            char  *buf);
char *get_gbuf(void);
int llid_traf_get_display_port(int dido_llid); 
void llid_traf_delete(int dido_llid);
void llid_traf_rx_from_client(int dido_llid, int len, char *buf);
void send_resp_ok_to_traf_client(int dido_llid, int idx_x11); 
void send_to_traf_client(int dido_llid, int val, int len, char *buf);
void llid_traf_backdoor_destroyed(int backdoor_llid);



/*--------------------------------------------------------------------------*/


