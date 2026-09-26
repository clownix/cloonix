/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#define MAX_ARG_LEN 1300
#define NB_ARG 50
int ovs_vsctl(char *ovs_bin, char *ovs_dir, char *multi_arg_cmd);
int ovs_vsctl_quiet(char *ovs_bin, char *ovs_dir, char *multi_arg_cmd);
int create_ovsdb_server_conf(char *ovs_bin, char *ovs_dir);

void init_environ(char *net, char *ovs_bin, char *ovs_dir);
int ovs_popen_last_said(char *ovs_dir, char *needle);
int ovs_popen_read_lines(char *ovs_dir, char lines[][MAX_NAME_LEN], int max);
int call_ovs_popen_ns(char *ovs_dir, char **argv, int quiet,
                      const char *fct, int line, char *netns);
int call_ovs_popen(char *ovs_dir, char **argv, int quiet,
                   const char *fct, int line);

int ovs_execv_ovs_vswitchd(char *net, char *ovs_bin, char *ovs_dir);
int ovs_execv_ovsdb_server(char *net, char *ovs_bin, char *ovs_dir);

char *get_pidfile_ovsdb(char *ovs_dir);
char *get_pidfile_ovs(char *ovs_dir);

