/*****************************************************************************/
/*    Copyright (C) 2006-2024 clownix@clownix.net License AGPL-3             */
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
struct t_vm;

typedef struct t_wake_up_eths
{
  int llid;
  int tid;
  int state;
  int eth_count;
  int nb_reqs_with_no_resp;
  long long automate_abs_beat; 
  int automate_ref;
  char name[MAX_NAME_LEN];
  char error_report[MAX_PRINT_LEN];
  long long abs_beat; 
  int ref;
  int destroy_requested;
} t_wake_up_eths;


typedef struct t_zombie
{ 
  char name[MAX_NAME_LEN];
  int vm_id;
  struct t_zombie *next;
  struct t_zombie *prev;
} t_zombie;

typedef struct t_newborn
{
  char name[MAX_NAME_LEN];
  struct t_newborn *next;
  struct t_newborn *prev;
} t_newborn;

/*---------------------------------------------------------------------------*/
typedef struct t_vm
  {
  t_topo_kvm kvm;
  int pid_of_cp_clone;
  int locked_vm;
  int vm_to_be_killed;
  int vm_poweroff_done;
  int count_no_pid_yet;
  int qmp_conn;
  t_wake_up_eths *wake_up_eths;
  int pid;
  int ram;
  int mem_rss;
  int cpu;
  unsigned long previous_utime;
  unsigned long previous_cutime;
  unsigned long previous_stime;
  unsigned long previous_cstime;
  char **launcher_argv; 
  struct t_vm *prev;
  struct t_vm *next;
  } t_vm;
/*---------------------------------------------------------------------------*/
typedef struct t_cfg
  {
  t_topo_clc clc;
  int lock_fd;
  int nb_vm;
  t_vm *vm_head;
  } t_cfg;
/*---------------------------------------------------------------------------*/
char *cfg_get_work(void);
char *cfg_get_work_vm(int vm_id);
char *cfg_get_root_work(void);
char *cfg_get_bulk(void);
char *cfg_get_bin_dir(void);

void cfg_set_host_conf(t_topo_clc *config);
t_topo_clc *cfg_get_topo_clc(void);

int  cfg_get_server_port(void);

char *cfg_get_ctrl_doors_sock(void);

/*---------------------------------------------------------------------------*/
int cfg_set_vm(t_topo_kvm *kvm, int vm_id, int llid); 

/*---------------------------------------------------------------------------*/
int cfg_unset_vm(t_vm *vm);

/*---------------------------------------------------------------------------*/
t_vm *find_vm_with_id(int vm_id);
t_vm   *cfg_get_vm(char *name);
int cfg_get_vm_locked(t_vm *vm);
void cfg_set_vm_locked(t_vm *vm);
void cfg_reset_vm_locked(t_vm *vm);
/*---------------------------------------------------------------------------*/
t_vm   *cfg_get_first_vm(int *nb);
void cfg_inc_lan_stats_tx_idx(int delta);


void cfg_set_lock_fd(int fd);
int  cfg_get_lock_fd(void);
int  cfg_alloc_obj_id(void);
void cfg_free_obj_id(int vm_id);


t_zombie *cfg_is_a_zombie_with_vm_id(int vm_id);
t_zombie *cfg_is_a_zombie(char *name);

void cfg_del_zombie(char *name);
void cfg_add_zombie(int vm_id, char *name);

void cfg_add_newborn(char *name);
void cfg_del_newborn(char *name);
t_newborn *cfg_is_a_newborn(char *name);

char *cfg_get_cloonix_name(void);
char *cfg_get_version(void);

/*---------------------------------------------------------------------------*/
void cfg_init(void);
/*---------------------------------------------------------------------------*/



int32_t cfg_get_trace_fd_qty(int type);


t_topo_info *cfg_produce_topo_info(void);


int cfg_zombie_qty(void);

/*****************************************************************************/
void recv_init(void);
void recv_coherency_lock(void);
int  recv_coherency_locked(void);
void recv_coherency_unlock(void);
/*---------------------------------------------------------------------------*/
t_vm   *cfg_get_first_vm(int *nb);
int cfg_name_is_in_use(int is_lan, char *name, char *use);
/*---------------------------------------------------------------------------*/
int cfg_get_name_with_mac(char *mac, char *vmname);
/*---------------------------------------------------------------------------*/
void cfg_send_vm_evt_qmp_conn_ok(char *name);
void cfg_send_vm_evt_cloonix_ga_ping_ok(char *name);
void cfg_send_vm_evt_cloonix_ga_ping_ko(char *name);
void cfg_hysteresis_send_topo_info(void);
/*---------------------------------------------------------------------------*/





