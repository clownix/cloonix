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

#define SYNCHRO 1
#define DAEMON 0 


typedef struct t_eth_mac
{
  char mac[8];
} t_eth_mac;

int crun_utils_user_nspacecrun_init(char *nspacecrun, int crun_pid);
void crun_utils_prepare_with_delete(char *name, char *nspacecrun,
                                    int vm_id, int nb_eth, int uid_image,
                                    int is_privileged);
int crun_utils_user_eth_create(char *name, char *nspacecrun,
                               int cloonix_rank, int vm_id, int nb_eth,
                               t_eth_mac *eth_mac);
int crun_utils_get_vswitchd_pid(char *name);
void crun_utils_startup_env(char *mountbear, char *startup_env, int nb_eth);
void crun_beat(int llid);
int crun_utils_unlink_sub_dir_files(char *dir);
void crun_utils_delete_crun_stop(char *name, int pid, int uid_image,
                                 int is_privileged, char *mountbear);
void crun_utils_delete_crun_delete(char *name, char *nspacecrun,
                                   int vm_id, int nb_eth,
                                   int uid_image, int is_privileged);
int crun_utils_delete_overlay1(char *name, char *cnt_dir);
int crun_utils_delete_overlay2(char *name, char *cnt_dir);
int crun_utils_delete_net_nspace(char *nspace);
int crun_utils_create_net(char *brandtype, int is_persistent,
                          int is_privileged, char *mountbear,
                          char *image, char *name, char *cnt_dir,
                          char *nspacecrun, int cloonix_rank,
                          int vm_id, int nb_eth, t_eth_mac *eth_mac,
                          char *agent_dir);

int crun_utils_create_crun_create(char *cnt_dir, char *name, int ovspid,
                                  int uid_image, int is_privileged);
int crun_utils_create_crun_start(char *name, int ovspid, int uid_image,
                                 int is_privileged);
int crun_utils_create_overlay(char *mountbear, char *mounttmp,
                              char *name, char *cnt_dir, char *mnt_tar_image,
                              char *image, int uid_image, char *agent_dir,
                              char *bulk_rootfs, int is_persistent,
                              int is_privileged, char *brandtype);
int crun_utils_create_config_json(char *path, char *rootfs, char *nspace,
                                  char *cloonix_dropbear, char *mounttmp,
                                  int is_persistent, int is_privileged,
                                  char *brandtype, char *startup_env,
                                  char *vmount, char *name, int ovspid,
                                  int uid_image, char *sbin_init,
                                  char *sh_starter,
                                 int subuid_start, int subuid_qty,
                                 int subgid_start, int subgid_qty);
void crun_utils_init(char *var_root);
char *get_mnt_loop_dir(void);
void lio_clean_all_llid(void);
char *lio_linear(char *argv[]);
FILE *cmd_lio_popen(char *cmd, int *child_pid, int uid);
int execute_cmd(char *cmd, int do_log, int uid);
void log_write_req(char *line);
int force_waitpid(int pid, int *status);
/*--------------------------------------------------------------------------*/
