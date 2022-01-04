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

#define WIDTH 300
#define HEIGH 200


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

void cloonix_get_xvt(char *xvt);
char *local_get_cloonix_name(void);
char *get_password(void);
char *get_doors_client_addr(void);
/*****************************************************************************/







