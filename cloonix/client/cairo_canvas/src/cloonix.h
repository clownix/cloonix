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

#define WIDTH 300
#define HEIGH 200

typedef struct t_custom_vm
{
  char name[MAX_NAME_LEN];
  char kvm_used_rootfs[MAX_PATH_LEN];
  char kvm_p9_host_share[MAX_PATH_LEN];
  int  type;
  int  is_uefi;
  int  is_full_virt;
  int  is_persistent;
  int  is_sda_disk;
  int  is_cisco;
  int  has_p9_host_share;
  int  current_number;
  int  cpu;
  int  mem;
  int  nb_tot_eth;
  t_eth_table eth_tab[MAX_SOCK_VM + MAX_DPDK_VM + MAX_WLAN_VM];
} t_custom_vm;


t_custom_vm *get_ptr_custom_vm (void);

void get_custom_vm (t_custom_vm **cust_vm);

void set_buttons_colors_red(void);
void set_buttons_colors_green(void);
void set_main_window_coords(int x, int y, int width, int heigh);
void refresh_eth_button_label(void);
void mouse_3rd_button_eth(void);
char *get_current_directory(void);
char *get_path_to_qemu_spice(void);
char *get_local_cloonix_tree(void);
char *get_distant_cloonix_tree(void);


void work_dir_resp(int tid, t_topo_clc *conf);
char *get_spice_vm_path(int vm_id);


char *get_cmd_path(void);
int inside_cloonix(char **name);

char **get_argv_local_xwy(char *name);

int get_vm_config_flags(t_custom_vm *cust_vm);
void cloonix_get_xvt(char *xvt);
char *local_get_cloonix_name(void);
char *get_password(void);
char *get_doors_client_addr(void);
/*****************************************************************************/







