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
typedef struct t_llid_dido
{
  int dido_llid;
  struct t_llid_dido *prev;
  struct t_llid_dido *next;
} t_llid_dido;
/*--------------------------------------------------------------------------*/
typedef struct t_backdoor_vm
{                       
  char name[MAX_NAME_LEN];
  char backdoor[MAX_PATH_LEN];
  int backdoor_llid; 
  int backdoor_fd;
  int connect_count;
  long long heartbeat_abeat;
  int heartbeat_ref;
  long long connect_abs_beat_timer;
  int connect_ref_timer;
  t_llid_dido *llid_dido_traf;
  t_rx_pktbuf rx_pktbuf;
  int ping_agent_rx;
  int last_ping_agent_rx;
  int fail_ping_agent_rx;
  int ping_status;
  int ping_id;
  int reboot_id;
  int halt_id;
  int cloonix_up_and_running;
  int cloonix_not_yet_connected;
  struct t_backdoor_vm *prev;
  struct t_backdoor_vm *next;
} t_backdoor_vm;
/*--------------------------------------------------------------------------*/
int  llid_backdoor_get_info(char *name, int *llid_backdoor);
void llid_backdoor_add_traf(char *name, int llid_backdoor, int llid_dido_traf);
void llid_backdoor_del_traf(char *name, int llid_backdoor, int llid_dido_traf);
void llid_backdoor_tx(char *name, int llid_backdoor, int llid_dido_traf,
                      int len, int type, int val, char  *buf);
void llid_backdoor_tx_x11_open_to_agent(int backdoor_llid, int llid, int idx);
void llid_backdoor_tx_x11_close_to_agent(int backdoor_llid, int llid, int idx);
void llid_backdoor_begin_unix(char *name, char *path);
void llid_backdoor_end_unix(char *name);
void llid_backdoor_tx_halt_to_agent(char *name);
void llid_backdoor_tx_reboot_to_agent(char *name);
void *llid_backdoor_get_first(char **name, int *backdoor_llid);
void *llid_backdoor_get_next(char **name, int *backdoor_llid, void *ptr_cur);
void llid_backdoor_init(void);
int  llid_backdoor_ping_status_is_ok(char *name);
void llid_backdoor_cloonix_up_vport_and_running(char *name);
void llid_backdoor_cloonix_down_and_not_running(char *name);
void llid_backdoor_low_tx(int llid, int len, char *buf);

t_backdoor_vm *llid_backdoor_vm_get_with_name(char *name);
/*--------------------------------------------------------------------------*/
