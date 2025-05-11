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
typedef struct t_custom_vm
{
  char name[MAX_NAME_LEN];
  int  type;
  int  is_full_virt;
  int  is_persistent;
  int  is_sda_disk;
  int  no_qemu_ga;
  int  natplug_flag;
  int  natplug;
  int  current_number;
  int  cpu;
  int  mem;
  int  nb_tot_eth;
  t_eth_table eth_tab[MAX_ETH_VM];
} t_custom_vm;

int get_vm_config_flags(t_custom_vm *cust_vm, int *natplug);
void get_custom_vm (t_custom_vm **cust_vm);
void set_bulkvm(int nb, t_slowperiodic *slowperiodic);
void menu_choice_kvm(void);
void menu_dialog_kvm_init(void);

