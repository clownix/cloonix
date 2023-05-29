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
typedef struct t_item_ident
{
  int bank_type;
  char name[MAX_NAME_LEN];
  char lan[MAX_NAME_LEN];
  int num;
  int joker_param;
  int vm_id;
  int endp_type;
} t_item_ident;


void menu_utils_init(void);
void node_del_val_save(char *name);
void node_dtach_console(GtkWidget *mn, t_item_ident *pm);
void node_qemu_spice(GtkWidget *mn, t_item_ident *pm);
GtkWidget *canvas_cursors(void);
void topo_delete(GtkWidget *mn);
void start_wireshark(char *name, int num);






