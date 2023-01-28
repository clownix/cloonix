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
enum
{
  error_death_noerr = 0,
  error_death_abort,
  error_death_nopid,
  error_death_noovs,
  error_death_noovstime,
  error_death_noovseth,
  error_death_addeth,
  error_death_vdeeth,
  error_death_addser,
  error_death_run,
  error_death_timeout,
  error_death_wakeup,
  error_death_cdrom,
  error_death_umlerr,
  error_death_qemu_quiterr,
  error_death_qemuerr,
  error_death_kvmerr,
  error_death_kvmcb,
  error_death_kvmmonitor,
  error_death_kvmstartko,
  error_death_qmp,
  error_death_pid_diseapeared,
  error_death_timeout_no_pid,
};
void machine_death(char *name, int error_death);
void machine_recv_add_vm(int llid, int tid, t_topo_kvm *kvm, int vm_id); 
int get_nb_mask(char *mask_string);
void machine_init(void);

/*****************************************************************************/
typedef void (*t_dtach_duplicate_callback)(int status, char *name);
/*---------------------------------------------------------------------------*/
typedef struct t_check_dtach_duplicate
{
  char name[MAX_NAME_LEN];
  char msg[MAX_NAME_LEN];
  t_dtach_duplicate_callback cb;
} t_check_dtach_duplicate;
/*---------------------------------------------------------------------------*/
void dtach_duplicate_check(char *name, t_dtach_duplicate_callback cb);
/*---------------------------------------------------------------------------*/
void timeout_start_vm_create_automaton(void *data);
/*---------------------------------------------------------------------------*/
void poweroff_vm(int llid, int tid, t_vm *vm);
/*---------------------------------------------------------------------------*/



