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
char *mydirname(char *path);
void mymkdir(char *dst_dir);
int is_directory_writable(char *path);
int is_directory_readable(char *path);
int is_file_readable(char *path);
char *mybasename(char *path);
char *read_whole_file(char *file_name, int *len, char *err);
int write_whole_file(char *file_name, char *buf, int len, char *err);
int file_exists(char *path, int mode);
char *get_cloonix_config_path(void);
int i_am_inside_cloon(char *name);
void make_config_cloonix_vm_name(char *path, char *name);
void make_config_cloonix_vm_p9_host_share(char *path, int has_p9_host_share);
/*---------------------------------------------------------------------------*/





