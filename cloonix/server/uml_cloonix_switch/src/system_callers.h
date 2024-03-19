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
void mk_cnt_dir(void);
void mk_endp_dir(void);
void mk_dtach_screen_dir(void);
int unlink_sub_dir_files(char *dir, char *err);
int unlink_sub_dir_files_except_dir(char *dir, char *err);

int mk_machine_dirs(char *name, int vm_id);
int rm_machine_dirs(int vm_id, char *err);

int mk_cnt_dirs(char *dir_path, char *name);
int rm_cnt_dirs(char *dir_path, char *name);

void my_cp_file(char *dsrc, char *ddst, char *name);
void my_cp_link(char *dir_src, char *dir_dst, char *name);
void my_mv_link(char *dir_src, char *dir_dst, char *name);
void my_mv_file(char *dsrc, char *ddst, char *name);
int  my_mkdir(char *dst_dir, int wr_all);
void my_cp_dir(char *src_dir, char *dst_dir,
                      char *src_name, char *dst_name);
void my_mv_dir(char *src_dir,char *dst_dir,
                      char *src_name,char *dst_name);
int get_pty(int *master_fdp, int *slave_fdp, char *slave_name);
int check_pid_is_clownix( int pid, int vm_id);

void mk_ovs_db_dir(void);






