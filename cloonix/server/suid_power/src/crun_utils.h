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
typedef struct t_eth_mac
{
  char mac[8];
} t_eth_mac;

void crun_beat(int llid);
char *get_losetup_bin(void);
char *get_ext4fuse_bin(void);
char *get_mount_bin(void);
char *get_umount_bin(void);
int crun_utils_unlink_sub_dir_files(char *dir);
int crun_utils_delete_crun_stop(char *name, int crun_pid);
int crun_utils_delete_overlay(char *name, char *cnt_dir, char *bulk,
                             char *image, int is_persistent);
int crun_utils_delete_net_nspace(char *nspace);
int crun_utils_create_net(char *image, char *name, char *cnt_dir,
                         char *nspace, int cloonix_rank, int vm_id,
                         int nb_eth, t_eth_mac *eth_mac, char *agent_dir,
                         char *customer_launch);
int crun_utils_create_crun_create(char *cnt_dir, char *name);
int crun_utils_create_crun_start(char *name);
int crun_utils_create_overlay(char *path, char *image, int is_persistent);
int crun_utils_create_config_json(char *path, char *rootfs, char *nspace,
                                 char *cloonix_dropbear, 
                                 char *mounttmp, int is_persistent);
int crun_utils_docker_veth(char *name, int pid, int vm_id,
                           int nb_eth, t_eth_mac *eth_mac);
int dirs_agent_create_mnt_tmp(char *cnt_dir, char *name);
void dirs_agent_copy_starter_crun(char *cnt_dir, char *name,
                             char *agent_dir, char *launch);
void dirs_agent_copy_starter_docker(char *cnt_dir, char *name,
                             char *agent_dir, char *launch);
void crun_utils_init(void);
char *get_mnt_loop_dir(void);
/*--------------------------------------------------------------------------*/
