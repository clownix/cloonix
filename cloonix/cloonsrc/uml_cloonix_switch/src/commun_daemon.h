/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef COMMUN_DAEMON_H
#define COMMUN_DAEMON_H
typedef struct t_llid_tid
{
  int llid;
  int tid;
} t_llid_tid;

typedef struct t_lst_pid
{
  char name[MAX_NAME_LEN];
  int pid;
  int unusedalign;
} t_lst_pid;

#define STAT_FORMAT "%*d (%299s %*s %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u "\
                    "%lu %lu %lu %lu %*d %*d %*u %*u %*d %*u %lu"

typedef struct t_pid_info
{
        unsigned long      utime    ;
        unsigned long      cutime   ;
        unsigned long      stime    ;
        unsigned long      cstime   ;
        unsigned long      rss      ;
        char               comm[MAX_PATH_LEN];
} t_pid_info;

enum
  {
  auto_idle = 0,
  auto_delay_possible_ovs_start,
  auto_create_vm_launch,
  auto_create_vm_connect,
  auto_max,
  };

#define DIR_CONF "config"
#define FILE_COW "cow"
#define CLOONIX_FILE_NAME "name"
#define CLOONIX_INTERNAL_COM "cloonix_internal_com"
#define DIR_CLOONIX_DISKS "disks"
#define QMP_UNIX "qmp"
#define QGA_UNIX "qga"
#define QBACKDOOR_UNIX "qdagent"

#endif
