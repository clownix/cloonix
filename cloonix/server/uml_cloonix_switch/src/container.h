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
typedef struct t_lan_cnt
{
  char lan[MAX_NAME_LEN];
  int  lan_attached_ok;
} t_lan_cnt;

typedef struct t_cnt
{
  t_topo_cnt cnt;
  t_lan_cnt att_lan[MAX_DPDK_VM];
  int lan_add_waiting_ack;
  int lan_del_waiting_ack;
  int count_add;
  int count_del;
  int cloonix_rank;
  int vm_id;
  int count_llid;
  int cli_llid;
  int cli_tid;
  int crun_pid;
  struct t_cnt *prev;
  struct t_cnt *next;
} t_cnt;
/*--------------------------------------------------------------------------*/
void container_resp_add_lan(int is_ko, char *lan, char *name, int num);
void container_resp_del_lan(int is_ko, char *lan, char *name, int num);
t_cnt *container_get_first_cnt(int *nb);
t_topo_endp *translate_topo_endp_cnt(int *nb); 
int  container_add_lan(int llid, int tid, char *name, int num,
                       char *lan, char *err);
int  container_del_lan(int llid, int tid, char *name, int num,
                       char *lan, char *err);
int  container_name_exists(char *name, int *nb_eth);
void container_poldiag_resp(int llid, char *line);
void container_poldiag_req(int llid, int tid);
void container_sigdiag_resp(int llid, char *line);
int  container_create(int llid, int cli_llid, int cli_tid,
                      int cloonix_rank, int vm_id,
                      t_topo_cnt *cnt, char *info);
int  container_delete(int llid, int cli_llid, int cli_tid,
                      char *name, char *info);
int  container_delete_all(int llid);
void container_timer_beat(int llid);
int  container_info(char *name, int *nb_eth, t_eth_table **eth_table);
int  container_get_all_pid(t_lst_pid **lst_pid);
void container_init(void);
/*---------------------------------------------------------------------------*/
