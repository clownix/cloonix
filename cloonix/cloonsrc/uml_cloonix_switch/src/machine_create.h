/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
enum
{
  error_death_noerr = 0,
  error_death_abort,
  error_death_nopid,
  error_death_wakeup,
  error_death_qemu_quiterr,
  error_death_qemuerr,
  error_death_qmp,
  error_death_pid_diseapeared,
  error_death_timeout_no_pid,
};
void machine_death(char *name, int error_death);
void machine_recv_add_vm(int llid, int tid, t_topo_kvm *kvm, int vm_id); 

void poweroff_vm(int llid, int tid, t_vm *vm);

