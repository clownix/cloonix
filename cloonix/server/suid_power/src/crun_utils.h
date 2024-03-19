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

#define SYNCHRO 1
#define DAEMON 0 


typedef struct t_eth_mac
{
  char mac[8];
} t_eth_mac;

void crun_beat(int llid);
int crun_utils_unlink_sub_dir_files(char *dir);
int crun_utils_delete_crun_stop(char *name, int crun_pid);
int crun_utils_delete_overlay(char *name, char *cnt_dir, char *bulk,
                             char *image, int is_persistent);
int crun_utils_delete_net_nspace(char *nspace);
int crun_utils_create_net(char *mountbear, char *mounttmp, char *image,
                          char *name, char *cnt_dir, char *nspace,
                          int cloonix_rank, int vm_id,
                          int nb_eth, t_eth_mac *eth_mac,
                          char *agent_dir, char *startup_env);
int crun_utils_create_crun_create(char *cnt_dir, char *name);
int crun_utils_create_crun_start(char *name);
int crun_utils_create_overlay(char *path, char *image, int is_persistent);
int crun_utils_create_config_json(char *path, char *rootfs, char *nspace,
                                  char *cloonix_dropbear, char *mounttmp,
                                  int is_persistent, char *startup_env,
                                  char *name);
void crun_utils_init(char *var_root);
char *get_mnt_loop_dir(void);
void lio_clean_all_llid(void);
char *lio_linear(char *argv[]);
FILE *cmd_lio_popen(char *cmd, int *child_pid);
int execute_cmd(char *cmd);
void log_write_req(char *line);
int force_waitpid(int pid, int *status);
/*--------------------------------------------------------------------------*/
