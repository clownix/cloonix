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
int crun_utils_unlink_sub_dir_files(char *dir);
int my_mkdir(char *dst_dir);
char *get_var_crun_log(void);
int force_waitpid(int pid, int *status);
void log_write_req(char *line);
void lio_clean_all_llid(void);
char *lio_linear(char *argv[]);
FILE *cmd_lio_popen(char *cmd, int *child_pid, int uid);
int execute_cmd(char *cmd, int do_log, int uid);
void crun_utils_startup_env(char *mountbear, char *startup_env,
                            int nb_vwif, char *mac, int nb_eth);
int crun_utils_create_config_json(char *image, char *path, char *rootfs,
                                 char *nspacecrun,
                                 char *mountbear, char *mounttmp,
                                 int is_persistent, int is_privileged,
                                 char *brandtype,
                                 char *startup_env, char *vmount,
                                 char *name, int ovspid, int uid_image,
                                 char *sbin_init, char *sh_starter,
                                 int subuid_start, int subuid_qty,
                                 int subgid_start, int subgid_qty);
void utils_init(char *log);
/*--------------------------------------------------------------------------*/



