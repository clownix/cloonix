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
typedef struct t_custom_cnt
{
  char name[MAX_NAME_LEN];
  int  nb_tot_eth;
  t_eth_table eth_table[MAX_ETH_VM];
  char startup_env[MAX_PATH_LEN];
  char vmount[MAX_SIZE_VMOUNT];
  int current_number;
  int is_persistent_zip;
  int is_persistent_cvm;
  int is_privileged_zip;
  int is_privileged_cvm;
} t_custom_cnt;

void get_custom_cnt(t_custom_cnt **cust_cnt);
void menu_choice_cnt(void);
void menu_dialog_cnt_init(void);

