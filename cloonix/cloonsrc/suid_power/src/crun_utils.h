/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */


typedef struct t_eth_mac
{
  unsigned char mac[8];
} t_eth_mac;

char *crun_cgroup_manager(int is_privileged);
char *crun_id(char *name);
int crun_utils_user_nspacecrun_init(char *name, char *nspacecrun,
                                    int crun_pid);
void suid_trace(int kind, char *name, char *txt);
void crun_utils_nspace_created(char *name, char *nspacecrun, int crun_pid);
void crun_utils_prepare_with_delete(char *name, char *nspacecrun,
                                    int vm_id, int nb_eth, int uid_image,
                                    int is_privileged);
int crun_utils_user_eth_create(char *name, char *nspacecrun, int cloonix_rank,
                               int vm_id, int nb_eth, t_eth_mac *eth_mac);

int crun_utils_get_vswitchd_pid(char *name);
void crun_utils_startup_env(char *mountbear, char *startup_env,
                            int nb_vwif, char *mac_vwif, int nb_eth);
void crun_beat(int llid);
void crun_utils_delete_crun_stop(char *name, int pid, int uid_image,
                                 int is_privileged, char *mountbear);
void crun_utils_delete_crun_delete(char *name, char *nspacecrun,
                                   int vm_id, int nb_eth,
                                   int uid_image, int is_privileged);
int crun_utils_mount_exists(char *path, char *found, int found_len);
int crun_utils_umount_tree(char *path);
int crun_utils_delete_overlay1(char *name, char *cnt_dir);
int crun_utils_delete_overlay2(char *name, char *cnt_dir);
int crun_utils_create_net(char *brandtype, int is_persistent,
                          int is_privileged, char *mountbear,
                          char *image, char *name, char *cnt_dir,
                          char *nspacecrun, int cloonix_rank,
                          int vm_id, int nb_eth, t_eth_mac *eth_mac);

int crun_utils_update(char *name, int crun_pid, int is_privileged,
                      int cpu, int ram, char *why, int why_len);
int crun_utils_create_crun_create(char *cnt_dir, char *name, int ovspid,
                                  int uid_image, int is_privileged);
int crun_utils_create_crun_start(char *name, int ovspid, int uid_image,
                                 int is_privileged);
int crun_utils_create_overlay(char *mountbear, char *mounttmp,
                              char *name, char *cnt_dir, char *lower,
                              char *image, int uid_image,
                              int is_persistent, int is_privileged,
                              char *brandtype, int subuid_start,
                              int subuid_qty, int subgid_start,
                              int subgid_qty);
int dirs_agent_create_mnt_tmp(char *mountbear, char *mounttmp);
