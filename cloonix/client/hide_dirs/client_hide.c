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
#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <syslog.h>

#include "glob_common.h"

#define MAX_NARGS 40
#define PROCESS_STACK 500*1024

void hide_real_machine_cli(void);

static FILE *g_log_cmd;
static char *g_home;
static char *g_display;
static char *g_user;
static char *g_xauthority;

static char *g_id_rsa_cisco;


/****************************************************************************/
static int get_pid_num_and_name(char *name)
{
  FILE *fp;
  char ps_cmd[MAX_PATH_LEN];
  char line[MAX_PATH_LEN];
  char tmp_name[MAX_PATH_LEN];
  int pid, result = 0;
  snprintf(ps_cmd, MAX_PATH_LEN-1, "%s axo pid,args", PS_BIN);
  fp = popen(ps_cmd, "r");
  if (fp == NULL)
    KERR("ERROR %s %d", ps_cmd, errno);
  else
    {
    memset(line, 0, MAX_PATH_LEN);
    while (fgets(line, MAX_PATH_LEN-1, fp))
      {
      if (sscanf(line,
         "%d /usr/libexec/cloonix/server/cloonix-main-server "
         "/usr/libexec/cloonix/common/etc/cloonix.cfg %s",
         &pid, tmp_name) == 2)
       {
       result = pid;
       strncpy(name, tmp_name, MAX_NAME_LEN-1);
       break;
       }
      }
    pclose(fp);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int lib_io_running_in_crun(char *name)
{
  int pid, result = 0;
  char *file_name = "/proc/1/cmdline";
  char buf[MAX_PATH_LEN];
  FILE *fd = fopen(file_name, "r");
  if (fd == NULL)
    KERR("WARNING: Cannot open %s", file_name);
  else
    {
    memset(buf, 0, MAX_PATH_LEN);
    if (!fgets(buf, MAX_PATH_LEN-1, fd))
      KERR("WARNING: Cannot read %s", file_name);
    else if (strstr(buf, CRUN_STARTER))
      {
      result = 1;
      if (name)
        {
        memset(name, 0, MAX_NAME_LEN);
        pid = get_pid_num_and_name(name);
        if (pid == 0)
          KERR("WARNING NO CLOONIX SERVER RUNNING");
        }
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int get_process_pid(char *cmdpath, char *sock)
{
  FILE *fp;
  char line[MAX_PATH_LEN];
  char name[MAX_NAME_LEN];
  char cmd[MAX_PATH_LEN];
  int pid, result = 0;
  fp = popen("/usr/libexec/cloonix/common/ps axo pid,args", "r");
  if (fp == NULL)
    KERR("ERROR %s %s", cmdpath, sock);
  else
    {
    memset(line, 0, MAX_PATH_LEN);
    while (fgets(line, MAX_PATH_LEN-1, fp))
      {
      if (strstr(line, sock))
        {
        if (strstr(line, cmdpath))
          {
          if (sscanf(line, "%d %s %400c", &pid, name, cmd))
            {
            result = pid;
            break;
            }
          }
        }
      }
    pclose(fp);
    }
  return result;
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static void kill_previous_wireshark_process(char *sock)
{
  int pid_wireshark, pid_dumpcap;
  pid_dumpcap   = get_process_pid(DUMPCAP_BIN, sock);
  pid_wireshark = get_process_pid(WIRESHARK_BIN,  sock);
  if (pid_wireshark)
    {
    kill(pid_wireshark, SIGKILL);
    KERR("WARNING KILL WIRESHARK %d", pid_wireshark);
    }
  if (pid_dumpcap)
    {
    kill(pid_dumpcap, SIGKILL);
    }
  if (pid_wireshark || pid_dumpcap)
    sleep(2);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int file_exists(char *path)
{
  int err, result = 0;
  err = access(path, F_OK);
  if (!err)
    result = 1;
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void fill_distant_xauthority(char *net)
{
  char cmd[2*MAX_PATH_LEN];
  memset(cmd, 0, 2*MAX_PATH_LEN);
  snprintf(cmd, 2*MAX_PATH_LEN-1,
           "/usr/libexec/cloonix/common/cloonix-ctrl %s %s cnf fix",
           CLOONIX_CFG, net);
  system(cmd);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void set_env_global_cloonix(char *net)
{
  clearenv();
  if (g_home)
    setenv("HOME", g_home, 1);
  if (g_display)
    setenv("DISPLAY", g_display, 1);
  if (g_user)
    setenv("USER", g_user, 1);
  if (g_xauthority)
    setenv("XAUTHORITY", g_xauthority, 1);
  setenv("PATH", "/usr/libexec/cloonix/common:/usr/libexec/cloonix/server", 1);
  setenv("LC_ALL", "C", 1);
  setenv("LANG", "C", 1);
  setenv("SHELL", "/usr/libexec/cloonix/common/bash", 1);
  setenv("TERM", "xterm", 1);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
/*
# The following private id_rsa corresponds to the id_rsa.pub that
# is used in file step1_make_preconf_iso.sh for the cisco making.
*/
/****************************************************************************/
static void create_cloonix_private_id_rsa_cisco(void)
{
  size_t len, wlen;
  FILE *fh;
  char *rsa =
  "-----BEGIN RSA PRIVATE KEY-----\n"
  "MIIEowIBAAKCAQEAxkepoiuYZD/HnpXL4raEarJi7EsMFkr5NoC/cv9SF8CPweIF\n"
  "JM66qe0CUd9Jw97Ov9ulTLPnoUQYFbjb/abOa7etijRx3vsvahgQSjYpOBXCngkd\n"
  "zooAvWymLYsBsZQo0QNtXdagP0lmti5eEX+/uCv/+IWGFx7tBT8UF8s58DruDRpm\n"
  "dwzGgh/fr2FwOklxjQO71gkIzd/fpQXevsj93CsUH3UzTrvTn8BrfrKABAAeH2Qd\n"
  "1yxTHRPcf9syEzMMA8YtwCPPO8vqobMt1tH6Yrl/c7DkxqCftWjPVx0mMJ/SHBOQ\n"
  "P4NMVr+fE6R0Y1WuWVwojs86uLAtOFBl5i+VmwIDAQABAoIBAAlCzZyCdsKv6+3v\n"
  "Ry+WoMavAEnTE4RzCgLOrqJ7ZGUxnEVM/jqC4VsQc9xJFpPsczGo26aifH4exRU2\n"
  "pifJw7hqQtPCsVLd3pARAanFr9UrxwREnrzH21L9oSFdbb3Skrl4dII+hQuPrRlz\n"
  "PveIRPcgLvt3mRS5YA6vrIuT9WfP85qt7QD4ibcDtb5+y93m0ayJty8YcgNCfqrF\n"
  "VdW40deeC4xzfBR6Q6edF4lho9O8Zl/9OTI3n/aS6xBl3qdN9y2hcIU9VTJQPErk\n"
  "jJMPb2ugZCeiXiFCPcZQGwn7oVeQsIbkBpkA7aYjATgIfsr7FVjZu8xkEhxr5aEQ\n"
  "9i/eiKECgYEA/zfrqdCvaYxi2bOa3u1nvRSsUbwsH6OFYye5WQhClqig+MQLN8Nu\n"
  "QZop4NswfwFnsIundcDgWzlT/EnuIA6qcsEW8Y4jRTi1uJqJDyDS3dHFXQHnu6c8\n"
  "oEoHJ5r32r93kbWRaJm+sbBrTOA2bH9bJX5NvfYCHMl0fbiY3UceRqsCgYEAxuMa\n"
  "29qBmjdwL11Udk/rMOLGABIITqL+PtRxsDGT54qG6jkfK6Lb0ZfMo7BfXM65Xr3W\n"
  "e875ErgaZZQaJOd2E1CEEWb0NWe+lMmfwJKz4q/p3bI5FL9ZX6Jr/wpJUDFXv051\n"
  "pxn5PPm9gFzvgkoFYSK5DoG6wmYoh4aGgPD3rNECgYEAhEHMZEH6xO21RC/o7+GD\n"
  "Qt71tZ2YGAU7WHj7egHnz/8u+/tL/OfPuTtUvGuaJBbsTvbwHvuGyH9a4IDHX+F5\n"
  "vuIFK8SGzpZmxXV/1VEjNURBzMLx/bLang3+yy1ph/h01BONePFDev17fWkriuos\n"
  "p69eRjS4P4a+UXBZ90GllOUCgYBVpWrljj0Nah43Z1t974B6ds2JLjrBklMmP1oN\n"
  "4+urY+4hYyPXKLS8l0AapVMLpkIRWHLKsiB0PS+w2ow/pCUmwB9/VvSHIvvhGspe\n"
  "pU4tqk9tltgZ5STZmBolpApaLEV7LpBfu0GnTmyaoGrLkpCqecdzRc5k9JUzd2zo\n"
  "jdw6YQKBgEB5T9gNNRpjjD2D8yJmyVDsCcHKUWO4kPejGhIQJ1hFLBsFv3DFmga8\n"
  "Ku+HluGo0TDABJUv1JBpVDeHBNprX3mHgud4fxKvoeG4eBfrjW5QsqIL34mrzT2b\n"
  "Tf6PmCDYsKOA79p2IYNG0aRiVMyTjb0JaDi/QJY65WR6Q5uoyCae\n"
  "-----END RSA PRIVATE KEY-----";
  if (access(g_id_rsa_cisco, F_OK))
    {
    len = strlen(rsa);
    fh = fopen(g_id_rsa_cisco, "w");
    if (fh == NULL)
      KOUT("ERROR1 %s", g_id_rsa_cisco);
    wlen = fwrite(rsa, 1, len, fh);
    if (wlen != len)
      KOUT("ERROR2 %s", g_id_rsa_cisco);
    fclose(fh);
    chmod(g_id_rsa_cisco, 0400);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int get_ip_pass_port(FILE *fh, char *ip, char *pass, int *port)
{
  int novnc_port, result = -1;
  char line[100];

  if (!fgets(line, 100, fh))
    printf("ERROR1 get_ip_pass_port\n"); 
  else if (sscanf(line, "  cloonix_ip %s", ip) != 1)
    printf("ERROR2 get_ip_pass_port %s\n", line); 

  else if (!fgets(line, 100, fh))
    printf("ERROR3 get_ip_pass_port\n"); 
  else if (sscanf(line, "  cloonix_port %d", port) != 1)
    printf("ERROR4 get_ip_pass_port %s\n", line); 

  else if (!fgets(line, 100, fh))
    printf("ERROR5 get_ip_pass_port\n"); 
  else if (sscanf(line, "  novnc_port %d", &novnc_port) != 1)
    printf("ERROR6 get_ip_pass_port %s\n", line); 

  else if (!fgets(line, 100, fh))
    printf("ERROR7 get_ip_pass_port\n"); 
  else if (sscanf(line, "  cloonix_passwd %s", pass) != 1)
    printf("ERROR8 get_ip_pass_port %s\n", line); 

  else
    result = 0;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int check_net_name(char *cnf, char *name,
                          char *ip, char *pass, int *port)
{
  int result = 0;
  FILE *fh;
  char line[100];
  char nm[80];
  fh = fopen(cnf, "r");
  if (fh == NULL) 
    KOUT("%s", cnf);
  while (fgets(line, 100, fh) != NULL)
    {
    if (sscanf(line, "CLOONIX_NET: %s {", nm) == 1)
      {
      if (!strcmp(nm, name))  
        {
        if (get_ip_pass_port(fh, ip, pass, port))
          KOUT("%s %s", cnf, line);
        result = 1;
        break;
        }
      }
    }
    fclose(fh);
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void log_ascii_cmd(char *argv[])
{
  int i;
  if (g_log_cmd != NULL)
    {
    for (i=0;  (argv[i] != NULL); i++)
      fprintf(g_log_cmd, "%s ", argv[i]);
    fprintf(g_log_cmd, "\n");
    fflush(g_log_cmd);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void init_log_cmd(char *network_name)
{
  char log_path[MAX_PATH_LEN];
  memset(log_path, 0, MAX_PATH_LEN);
  snprintf(log_path, MAX_PATH_LEN-1,
  "/var/lib/cloonix/%s/log/debug_cmd.log", network_name);
  g_log_cmd = fopen(log_path, "a");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void process_ocp(int argc, char **argv, char **new_argv,
                        char *ip, int port, char *passwd)
{
  static char sock[MAX_PATH_LEN];
  static char dist[MAX_PATH_LEN];
  char ocp_param[MAX_PATH_LEN];
  char tmpnet[MAX_NAME_LEN];
  int running_in_crun = lib_io_running_in_crun(tmpnet);
  char ipl[MAX_NAME_LEN];
  memset(ipl, 0, MAX_NAME_LEN);
  if (running_in_crun && (!strcmp(tmpnet, argv[2])))
    strncpy(ipl, "127.0.0.1", MAX_NAME_LEN);
  else
    strncpy(ipl, ip, MAX_NAME_LEN);
  memset(sock, 0, MAX_PATH_LEN);
  memset(dist, 0, MAX_PATH_LEN);
  memset(ocp_param, 0, MAX_PATH_LEN);
  if (argc != 9)
    KOUT("ERROR81 PARAM NUMBER");
  snprintf(sock, MAX_PATH_LEN-1, 
           "/var/lib/cloonix/%s/%s", argv[2], NAT_MAIN_DIR);
  mkdir(sock, 0777);
  snprintf(ocp_param, MAX_PATH_LEN-1,
           "%s=%d=%s=nat_%s@user=admin=ip=%s=port=22=cloonix_info_end",
           ipl, port, passwd, argv[3], argv[4]);
  new_argv[0] = "/usr/libexec/cloonix/common/cloonix-u2i-scp";
  new_argv[1] = sock;
  new_argv[2] = "-i";
  new_argv[3] = g_id_rsa_cisco;
  if (!strcmp(argv[7], "y"))
    {
    new_argv[4] = "-r";
    if (!strcmp(argv[8], "dist"))
      {
      snprintf(dist, MAX_PATH_LEN-1, "%s:%s", ocp_param, argv[5]);
      new_argv[5] = dist;
      new_argv[6] = argv[6];
      }
    else if (!strcmp(argv[8], "loc"))
      {
      snprintf(dist, MAX_PATH_LEN-1, "%s:%s", ocp_param, argv[6]);
      new_argv[5] = argv[5];
      new_argv[6] = dist;
      }
    else
      KOUT("ERROR81 PARAM NUMBER");
    }
  else if (!strcmp(argv[7], "n"))
    {
    if (!strcmp(argv[8], "dist"))
      {
      snprintf(dist, MAX_PATH_LEN-1, "%s:%s", ocp_param, argv[5]);
      new_argv[4] = dist;
      new_argv[5] = argv[6];
      }
    else if (!strcmp(argv[8], "loc"))
      {
      snprintf(dist, MAX_PATH_LEN-1, "%s:%s", ocp_param, argv[6]);
      new_argv[4] = argv[5];
      new_argv[5] = dist;
      }
    else
      KOUT("ERROR82 PARAM NUMBER");
    }
  else
    KOUT("ERROR8 PARAM NUMBER");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void process_osh(int argc, char **argv, char **new_argv,
                        char *ip, int port, char *passwd)
{
  static char sock[MAX_PATH_LEN];
  static char ocp_param[MAX_PATH_LEN];
  int i;
  char tmpnet[MAX_NAME_LEN];
  int running_in_crun = lib_io_running_in_crun(tmpnet);
  char ipl[MAX_NAME_LEN];
  memset(ipl, 0, MAX_NAME_LEN);
  if (running_in_crun && (!strcmp(tmpnet, argv[2])))
    strncpy(ipl, "127.0.0.1", MAX_NAME_LEN);
  else
    strncpy(ipl, ip, MAX_NAME_LEN);
  memset(sock, 0, MAX_PATH_LEN);
  memset(ocp_param, 0, MAX_PATH_LEN);
  if (argc < 5)
    KOUT("ERROR91 PARAM NUMBER");
  snprintf(sock, MAX_PATH_LEN-1,
           "/var/lib/cloonix/%s/%s", argv[2], NAT_MAIN_DIR);
  mkdir(sock, 0777); 
  snprintf(ocp_param, MAX_PATH_LEN-1,
          "%s=%d=%s=nat_%s@user=admin=ip=%s=port=22=cloonix_info_end",
          ipl, port, passwd, argv[3], argv[4]);
  new_argv[0] = "/usr/libexec/cloonix/common/cloonix-u2i-ssh";
  new_argv[1] = sock;
  new_argv[2] = "-i";
  new_argv[3] = g_id_rsa_cisco;
  new_argv[4] = ocp_param;
  for (i=0; i<20; i++)
    {
    if (argc > 5+i)
      new_argv[5+i] = argv[5+i];
    }
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static void fill_ipport(char *net, char *ip, int port, char *ipport)
{
  int running_in_crun;
  char tmpnet[MAX_NAME_LEN];
  char *pox = PROXYSHARE_IN;
  memset(ipport, 0, MAX_PATH_LEN);
  running_in_crun = lib_io_running_in_crun(tmpnet);
  if (running_in_crun && (!strcmp(tmpnet, net)))
    snprintf(ipport, MAX_PATH_LEN-1, "%s_%s/proxy_pmain.sock", pox, net);
  else
    snprintf(ipport, MAX_PATH_LEN-1, "%s:%d", ip, port);
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static int initialise_new_argv(int argc, char **argv, char **new_argv,
                               char *ip, int port, char *passwd)
{
  static char ipport[MAX_PATH_LEN];
  static char title[MAX_NAME_LEN];
  static char sock[2*MAX_PATH_LEN];
  static char param[2*MAX_PATH_LEN];
  int i, result = 0;

  memset(ipport, 0, MAX_PATH_LEN);
  memset(title, 0, MAX_NAME_LEN);
  memset(sock, 0, 2*MAX_PATH_LEN);
  memset(param, 0, 2*MAX_PATH_LEN);
/*CLOONIX_LSH-----------------------*/
  if (!strcmp("lsh", argv[1]))
    {
    new_argv[0] = "/usr/libexec/cloonix/common/bash";
    }
/*CLOONIX_DSH-----------------------*/
  else if (!strcmp("dsh", argv[1]))
    {
    new_argv[0] = XWYCLI_BIN;
    new_argv[1] = CLOONIX_CFG;
    new_argv[2] = argv[2];
    new_argv[3] = "-cmd";
    new_argv[4] = "/usr/libexec/cloonix/common/bash";
    }
/*CLOONIX_CLI-----------------------*/
  else if (!strcmp("cli", argv[1]))
    {
    if (argc < 4)
      {
      new_argv[0] = "/usr/libexec/cloonix/common/cloonix-ctrl";
      new_argv[1] = CLOONIX_CFG;
      }
    else
      {
      new_argv[0] = "/usr/libexec/cloonix/common/cloonix-ctrl";
      new_argv[1] = CLOONIX_CFG;
      new_argv[2] = argv[2];
      new_argv[3] = argv[3];
      for (i=0; i<MAX_NARGS; i++)
        {
        if (argc > 4+i)
          new_argv[4+i] = argv[4+i];
        }
      }
    }
/*CLOONIX_GUI-----------------------*/
  else if (!strcmp("gui", argv[1]))
    {
    new_argv[0] = "/usr/libexec/cloonix/common/cloonix-gui";
    new_argv[1] = CLOONIX_CFG;
    new_argv[2] = argv[2];
    }
/*CLOONIX_SCP-----------------------*/
  else if (!strcmp("scp", argv[1]))
    {
    if (argc < 5)
      KOUT("ERROR5 PARAM NUMBER %d", argc);
    fill_ipport(argv[2], ip, port, ipport);
    new_argv[0] = "/usr/libexec/cloonix/common/cloonix-dropbear-scp";
    new_argv[1] = ipport;
    new_argv[2] = passwd;
    for (i=0; i<MAX_NARGS; i++)
      {
      if (argc > 3+i)
        new_argv[3+i] = argv[3+i];
      }
    }
/*CLOONIX_SSH-----------------------*/
  else if (!strcmp("ssh", argv[1]))
    {
    if ((argc != 4) && (argc != 5))
      KOUT("ERROR5 PARAM NUMBER %d", argc);
    fill_ipport(argv[2], ip, port, ipport);
    new_argv[0] = "/usr/libexec/cloonix/common/cloonix-dropbear-ssh";
    new_argv[1] = ipport;
    new_argv[2] = passwd;
    new_argv[3] = argv[3];
    if (argc == 5)
      new_argv[4] = argv[4];
    }
/*CLOONIX_SHK-----------------------*/
  else if (!strcmp("shk", argv[1]))
    {
    if (argc != 4)
      KOUT("ERROR5 PARAM NUMBER %d", argc);
    snprintf(title, MAX_NAME_LEN-1, "%s/%s", argv[2], argv[3]);
    snprintf(param, 399, "/var/lib/cloonix/%s/%s/%s",
                         argv[2], DTACH_SOCK, argv[3]);
    new_argv[0]  = URXVT_BIN;
    new_argv[1]  = "-T";
    new_argv[2]  = title;
    new_argv[3]  = "-e";
    new_argv[4]  = XWYCLI_BIN;
    new_argv[5]  = CLOONIX_CFG;
    new_argv[6]  = argv[2];
    new_argv[7]  = "-cmd";
    new_argv[8]  = "/usr/libexec/cloonix/server/cloonix-dtach";
    new_argv[9]  = "-a";
    new_argv[10] = param;
    }


/*CLOONIX_SHC-----------------------*/
  else if (!strcmp("shc", argv[1]))
    {
    if (argc != 4)
      KOUT("ERROR5 PARAM NUMBER %d", argc);
    snprintf(title, MAX_NAME_LEN-1, "%s/%s", argv[2], argv[3]);
    snprintf(param, 399, "/usr/libexec/cloonix/server/cloonix-crun "
                         "--log=/var/lib/cloonix/%s/log/debug_crun.log "
                         "--root=/var/lib/cloonix/%s/%s/ exec %s /bin/sh",
                         argv[2], argv[2], CRUN_DIR, argv[3]);
    new_argv[0] = URXVT_BIN;
    new_argv[1] = "-T";
    new_argv[2] = title;
    new_argv[3] = "-e";
    new_argv[4] = XWYCLI_BIN;
    new_argv[5] = CLOONIX_CFG;
    new_argv[6] = argv[2];
    new_argv[7] = "-crun";
    new_argv[8] = param;
    }

/*CLOONIX_ICE-----------------------*/
  else if (!strcmp("ice", argv[1]))
    {
    if (argc != 5)
      KOUT("ERROR5 PARAM NUMBER %d", argc);
    fill_ipport(argv[2], ip, port, ipport);
    snprintf(title, MAX_NAME_LEN-1, "--title=%s/%s", argv[2], argv[3]);
    snprintf(sock, 2*MAX_PATH_LEN-1, "/var/lib/cloonix/%s/vm/vm%s/spice_sock", argv[2], argv[4]);
    new_argv[0] = "/usr/libexec/cloonix/common/cloonix-spicy";
    new_argv[1] = title;
    new_argv[2] = "-d";
    new_argv[3] = ipport;
    new_argv[4] = "-c";
    new_argv[5] = sock;
    new_argv[6] = "-w";
    new_argv[7] = passwd;
    }
/*CLOONIX_WSK-----------------------*/
  else if (!strcmp("wsk", argv[1]))
    {
    if (argc != 5)
      KOUT("ERROR5 PARAM NUMBER %d", argc);
    snprintf(sock, 2*MAX_PATH_LEN-1, "/var/lib/cloonix/%s/snf/%s_%s",
             argv[2], argv[3], argv[4]);
    snprintf(title, MAX_PATH_LEN-1, "%s,%s,eth%s", argv[2],  argv[3], argv[4]);
    kill_previous_wireshark_process(sock);
    new_argv[0] = XWYCLI_BIN;
    new_argv[1] = CLOONIX_CFG;
    new_argv[2] = argv[2];
    new_argv[3] = "-dae";
    new_argv[4] = WIRESHARK_BIN;
    new_argv[5] = "-i";
    new_argv[6] = sock;
    new_argv[7] = "-t";
    new_argv[8] = title;
    }
/*CLOONIX_OVS-----------------------*/
  else if (!strcmp("ovs", argv[1]))
    {
    snprintf(sock, 2*MAX_PATH_LEN-1,
             "--db=unix:/var/lib/cloonix/%s/ovsdb_server.sock", argv[2]);
    new_argv[0] = XWYCLI_BIN;
    new_argv[1] = CLOONIX_CFG;
    new_argv[2] = argv[2];
    new_argv[3] = "-cmd";
    new_argv[4] = "/usr/libexec/cloonix/server/cloonix-ovs-vsctl";
    new_argv[5] = sock;
    for (i=0; i<10; i++)
      {
      if (argc > 3+i)
        new_argv[6+i] = argv[3+i];
      }
    }
/*CLOONIX_OCP-----------------------*/
  else if (!strcmp("ocp", argv[1]))
    {
    process_ocp(argc, argv, new_argv, ip, port, passwd);
    }
/*CLOONIX_OSH-----------------------*/
  else if (!strcmp("osh", argv[1]))
    {
    process_osh(argc, argv, new_argv, ip, port, passwd);
    }
  else
    {
    printf("ERROR10 PARAM %s\n", argv[1]);
    result = -1;
    KERR("ERROR10 PARAM %s\n", argv[1]);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int main(int argc, char *argv[])
{
  char passwd[MAX_NAME_LEN];
  char ip[MAX_NAME_LEN];
  int  port, pid;
  char *cfg = CLOONIX_CFG;
  char *new_argv[MAX_NARGS+10];
  g_id_rsa_cisco = "/tmp/cloonix_private_id_rsa_cisco";
  g_xauthority = getenv("XAUTHORITY");
  g_home = getenv("HOME");
  g_display = getenv("DISPLAY");
  g_user = getenv("USER");
  g_log_cmd = NULL;
  memset(new_argv, 0, (MAX_NARGS+10)*sizeof(char *));
  if (argc < 2)
    KOUT("ERROR1 PARAMS");
  else if (argc < 3)
    {
    if ((strcmp("lsh", argv[1])) &&
        (strcmp("cli", argv[1])))
      KOUT("ERROR1 PARAMS");
    set_env_global_cloonix("nemo");
    }
  else if (!check_net_name(cfg, argv[2], ip, passwd, &port))
    KOUT("ERROR3 PARAMS");
  else
    {
    init_log_cmd(argv[2]);
    if (!strcmp("gui", argv[1]))
      {
      hide_real_machine_cli();
      fill_distant_xauthority(argv[2]);
      setenv("NO_AT_BRIDGE", "1", 1);
      }
    else if (strcmp("ice", argv[1]))
      {
      set_env_global_cloonix(argv[2]);
      }
    }
/*--------------------------------------------------------------------------*/
  if (initialise_new_argv(argc, argv, new_argv, ip, port, passwd))
    KOUT("ERROR10 PARAM %s", argv[1]);
/*--------------------------------------------------------------------------*/
  log_ascii_cmd(new_argv);
/*--------------------------------------------------------------------------*/
  if ((!strcmp("ice", argv[1])) ||
      (!strcmp("wsk", argv[1])) ||
      (!strcmp("shc", argv[1])) ||
      (!strcmp("shk", argv[1])))
    {
    pid = fork();
    if (pid < 0)
      KOUT("ERROR10 PARAM %s", argv[1]);
    else if (pid == 0)
      {
      execv(new_argv[0], new_argv);
      KOUT("ERROR execv");
      }
    }
  else
    {
    if ((!strcmp("osh", argv[1])) ||
        (!strcmp("ocp", argv[1])) ||
        (!strcmp("lsh", argv[1])))
      create_cloonix_private_id_rsa_cisco();
    execv(new_argv[0], new_argv);
    KOUT("ERROR execv %s not good", new_argv[0]);
    }
/*--------------------------------------------------------------------------*/
}
