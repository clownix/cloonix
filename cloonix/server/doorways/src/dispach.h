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
typedef struct t_transfert
{
  int type;
  int dido_llid;
  int inside_llid;
  int beat_count;
  int init_done;
  int ident_flow_timeout;
  struct t_transfert *prev;
  struct t_transfert *next;
} t_transfert;

int dispatch_get_dido_llid_with_inside_llid(int inside_llid, int *init_done);
void dispatch_set_init_done_with_inside_llid(int inside_llid);
char *get_g_buf(void);
void dispach_err_switch (int llid, int err);
void dispach_rx_switch(int llid, int len, char *buf);
void dispach_door_llid(int listen_llid, int llid);
void dispach_door_end(int llid);
void doorways_rx_bufraw(int llid, int tid,
                        int len_bufraw, char *doors_bufraw,
                        int type, int val, int len, char *buf);
int  dispach_send_to_traf_client(int llid, int val, int len, char *buf);
int dispach_send_to_openssh_client(int dido_llid, int val, int len, char *buf);
void in_err_gene(int inside_llid, int err, int from);
void dispach_free_transfert(int dido_llid, int inside_llid);
t_transfert *dispach_get_dido_transfert(int dido_llid);
t_transfert *dispach_get_inside_transfert(int inside_llid);
void dispach_init(void);
/*--------------------------------------------------------------------------*/

