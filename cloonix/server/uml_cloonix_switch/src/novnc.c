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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "util_sock.h"
#include "cfg_store.h"
#include "pid_clone.h"
#include "novnc.h"
#include "file_read_write.h"
#include "utils_cmd_line_maker.h"

#define BIN_XVFB       "/usr/libexec/cloonix/cloonfs/cloonix-novnc-Xvfb"
#define BIN_XSETROOT   "/usr/libexec/cloonix/cloonfs/cloonix-novnc-xsetroot"
#define BIN_WM2        "/usr/libexec/cloonix/cloonfs/cloonix-novnc-wm2"
#define BIN_X11VNC     "/usr/libexec/cloonix/cloonfs/cloonix-novnc-x11vnc"
#define BIN_NGINX      "/usr/libexec/cloonix/cloonfs/cloonix-novnc-nginx"

#define BIN_WEBSOCKIFY "/usr/libexec/cloonix/cloonfs/cloonix-novnc-websockify-js"
#define NODE_PATH      "/usr/share/nodejs:/usr/share/nodejs/node_modules"
#define WEB            "--web=/usr/libexec/cloonix/cloonfs/share/noVNC/"

static int g_pid_Xvfb;
static int g_pid_wm2;
static int g_pid_x11vnc;
static int g_pid_websockify;
static int g_pid_nginx_master;
static int g_pid_nginx_worker;
static int g_rank;
static int g_port_display;
static int g_count_websockify_timer;
static int g_count_nginx_timer1;
static int g_count_nginx_timer2;
static int g_port_display_start;
static char g_net_name[MAX_NAME_LEN];
static char g_display[MAX_NAME_LEN];
static char g_x11vnc_log[MAX_PATH_LEN];

static int x11vnc(void *data);
static void timer_wm2(void *data);
static int g_end_novnc_currently_on;
static int g_terminate;
static int g_start;

int get_running_in_crun(void);
void hide_real_machine_serv(void);


/*****************************************************************************/
static void debug_print_cmd(char **argv)
{
  int i, len = 0;
  char info[MAX_PRINT_LEN];
  char logfile[MAX_PATH_LEN];
  FILE *fp_log;
  memset(info, 0, MAX_PRINT_LEN);
  memset(logfile, 0, MAX_PATH_LEN);
  snprintf(logfile, MAX_PATH_LEN-1, "%s/novnc_cmd.log", utils_get_log_dir());
  fp_log = fopen(logfile, "a+");
  if (fp_log)
    {
    for (i=0; argv[i]; i++)
      len += sprintf(info+len, "%s ", argv[i]);
    fprintf(fp_log, "%s\n", info);
    fflush(fp_log);
    fclose(fp_log);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int check_free_tcp_port(int port)
{
  struct sockaddr_in serv_addr;
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  int result = -1;
  if(sock < 0)
    KERR("ERROR socket");
  else
    { 
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);
    if (!bind(sock, (struct sockaddr *) &serv_addr, sizeof(serv_addr)))
      result = 0;
    if (close (sock) < 0 )
      KERR("ERROR socket close");
    }
  return result;
} 
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int get_free_port_display(int start_port, int range)
{
  int i, result = 0;
  for (i=0; i<range; i++)
    {
    if (!check_free_tcp_port(start_port+i))
      {
      result = start_port+i;
      break;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int get_free_tcp_port(void)
{
  struct sockaddr_in serv_addr;
  socklen_t len;
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  int result = 0;
  if(sock < 0)
    KERR("ERROR socket");
  else
    {
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = 0;
    if (bind(sock, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
      KERR("ERROR socket bind");
    else
      {
      len = sizeof(serv_addr);
      if (getsockname(sock, (struct sockaddr *)&serv_addr, &len) == -1)
        KERR("ERROR socket getsockname");
      else
        result = ntohs(serv_addr.sin_port);
      }
    if (close (sock) < 0 )
      KERR("ERROR socket close");
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void setup_nginx_conf(char *websockify_port, char *proxymous_sock)
{ 
  char err[MAX_PRINT_LEN];
  char path[MAX_PATH_LEN];
  char *data_conf;
  int len;
  snprintf(path, MAX_PATH_LEN, "%s/nginx.conf", utils_get_nginx_conf_dir());
  unlink(path);
  len = strlen(CONFIG_NGINX) + MAX_PATH_LEN + MAX_PATH_LEN;
  data_conf = (char *) malloc(len);
  memset(data_conf, 0, len);
  snprintf(data_conf, len-1, CONFIG_NGINX, websockify_port, proxymous_sock);
  if (write_whole_file(path, data_conf, strlen(data_conf), err))
    KERR("ERROR %s", err);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int get_nginx_master_pid_num(void)
{
  FILE *fp;
  char pattern[MAX_PATH_LEN];
  char ps_cmd[MAX_PATH_LEN];
  char line[MAX_PATH_LEN];
  int pid, result = 0;
  memset(pattern, 0, MAX_PATH_LEN);
  snprintf(pattern, MAX_PATH_LEN-1, "/var/lib/cloonix/%s/nginx", g_net_name);
  snprintf(ps_cmd, MAX_PATH_LEN-1, "%s axo pid,args", PS_BIN);
  fp = popen(ps_cmd, "r");
  if (fp == NULL)
    KERR("ERROR %s %d", ps_cmd, errno);
  else 
    {
    memset(line, 0, MAX_PATH_LEN);
    while (fgets(line, MAX_PATH_LEN-1, fp))
      {
      if ((strstr(line, "cloonix-novnc-nginx")) &&
          (strstr(line, "master process")))
        {
        if (sscanf(line, "%d", &pid) == 1)
          {
          result = pid;
          break;
          }
        }
      }
    pclose(fp);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int get_nginx_worker_pid_num(int master_pid)
{   
  FILE *fp;
  char ps_cmd[MAX_PATH_LEN];
  char line[MAX_PATH_LEN];
  int ppid, pid, result = 0;
  snprintf(ps_cmd, MAX_PATH_LEN-1, "%s axo ppid,pid,args", PS_BIN);
  fp = popen(ps_cmd, "r");
  if (fp == NULL)
    KERR("ERROR %s %d", ps_cmd, errno);
  else
    {
    memset(line, 0, MAX_PATH_LEN);
    while (fgets(line, MAX_PATH_LEN-1, fp))
      {
      if (sscanf(line, "%d %d", &ppid, &pid) == 2)
        {
        if ((master_pid == ppid) && (strstr(line, "worker process")))
          {
          result = pid;
          break;
          }
        }
      }
    pclose(fp);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int get_pid_num(char *binstring, char *display)
{
  FILE *fp;
  char ps_cmd[MAX_PATH_LEN];
  char line[MAX_PATH_LEN];
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
      if (strstr(line, binstring))
        {
        if (sscanf(line, "%d", &pid) == 1)
          {
          if (display == NULL)
            {
            result = pid;
            break;
            }
          else if (strstr(line, display))
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

/*****************************************************************************/
static void restart_all_upon_problem(void)
{

  if (g_terminate)
    return;

  if (g_end_novnc_currently_on == 0)
    end_novnc(0);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void kXvfb(void *unused_data, int status, char *name)
{
  g_pid_Xvfb = 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void kx11vnc(void *unused_data, int status, char *name)
{
  g_pid_x11vnc = 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void kwm2(void *unused_data, int status, char *name)
{
  g_pid_wm2 = 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void kwebsockify(void *unused_data, int status, char *name)
{
  g_pid_websockify = 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void nginx_ko(void *unused_data, int status, char *name)
{                   
  int pid1, pid2;
  if (g_pid_nginx_master)
    {
    pid2 = get_nginx_worker_pid_num(g_pid_nginx_master);
    kill(g_pid_nginx_master, SIGKILL);
    if (pid2)
      {
      KERR("WARNING KILLING NGINX WORKER pid: %d", pid2);
      kill(pid2, SIGKILL);
      }
    }
  pid1 = get_nginx_master_pid_num();
  if (pid1)
    {
    KERR("WARNING KILLING NGINX MASTER pid: %d", pid1);
    pid2 = get_nginx_worker_pid_num(pid1);
    kill(pid1, SIGKILL);
    if (pid2)
      {
      KERR("WARNING KILLING NGINX WORKER pid: %d", pid2);
      kill(pid2, SIGKILL);
      }
    }
  KERR("ERROR nginx TERMINATION KO");
  g_pid_nginx_master = 0;
  g_pid_nginx_worker = 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int Xvfb(void *data)
{
  char *argv[] = {BIN_XVFB, g_display, "-nolisten", "tcp", "-noreset",
                                       "-dpms", "-br", "-ac", NULL};

  if (g_terminate)
    return 0;

  debug_print_cmd(argv);
  execv(argv[0], argv);
  KOUT("ERROR execv %s", argv[0]);
  return 0;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int x11vnc(void *data)
{
  char *argv[]={BIN_X11VNC, "-N", "-nopw", "-localhost", "-shared", 
                            "-repeat", "-forever", "-verbose", "-logfile",
                            g_x11vnc_log, "-noshm", "-noxdamage", "-cursor",
                            "arrow", "-remap", "DEAD", "-ncache", "10",
                            "-dpms", "-display", g_display, NULL};

  if (g_terminate)
    return 0;

  clearenv();
  setenv("XDG_SESSION_TYPE", "x11", 1);
  setenv("DISPLAY", g_display, 1);
  debug_print_cmd(argv);
  execv(argv[0], argv);
  KOUT("ERROR execv %s", argv[0]);
  return 0;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int wm2(void *data)
{
  char *argv[]={BIN_WM2, g_display, NULL};

  if (g_terminate)
    return 0;

  clearenv();
  setenv("XDG_SESSION_TYPE", "x11", 1);
  setenv("DISPLAY", g_display, 1);
  setenv("CLOONIX_NET", g_net_name, 1);
  debug_print_cmd(argv);
  execv(argv[0], argv);
  KOUT("ERROR execv %s", argv[0]);
  return 0;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int xsetroot(void *data)
{
  char *argv[] = {BIN_XSETROOT, "-display", g_display, "-solid", "grey", NULL};

  if (g_terminate)
    return 0;

  debug_print_cmd(argv);
  execv(argv[0], argv);
  KOUT("ERROR execv %s", argv[0]);
  return 0;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int websockify(void *data)
{
  int websockify_port;
  char proxymous_sock[MAX_PATH_LEN];
  char addr_display[MAX_NAME_LEN];
  char websockify_ascii_port[MAX_NAME_LEN];
  char *argv[]={BIN_WEBSOCKIFY,WEB,websockify_ascii_port,addr_display,NULL};

  if (g_terminate)
    return 0;
    
  if (!get_running_in_crun())
    hide_real_machine_serv();
  setenv("NODE_PATH", NODE_PATH, 1);
  if (g_port_display == 0)
    KOUT("ERROR");
  memset(addr_display, 0, MAX_NAME_LEN);
  snprintf(addr_display, MAX_NAME_LEN-1, "localhost:%d", g_port_display);
  websockify_port = get_free_tcp_port();
  memset(websockify_ascii_port, 0, MAX_NAME_LEN);
  snprintf(websockify_ascii_port, MAX_NAME_LEN-1, "%d", websockify_port);
  memset(proxymous_sock, 0, MAX_PATH_LEN);
  snprintf(proxymous_sock, MAX_PATH_LEN-1,
           "%s_%s/proxy_pweb.sock", PROXYSHARE_IN, g_net_name);
  unlink(proxymous_sock);
  setup_nginx_conf(websockify_ascii_port, proxymous_sock); 
  debug_print_cmd(argv);
  execv(argv[0], argv);
  KOUT("ERROR execv %s", argv[0]);
  return 0;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int nginx_ok(void *data)
{
  char path[MAX_PATH_LEN+1];
  char *argv[]={BIN_NGINX, "-p", path, NULL};
  if (g_terminate)
    return 0;
  memset (path, 0, MAX_PATH_LEN+1);
  snprintf(path, MAX_PATH_LEN, "%s", utils_get_nginx_dir());
  debug_print_cmd(argv);
  execv(argv[0], argv);
  KOUT("ERROR execv %s", argv[0]);
  return 0;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void timer_end_last(void *data)
{

  if (g_terminate)
    return;

  g_pid_nginx_master = get_nginx_master_pid_num();
  if (g_pid_nginx_master)
    {
    g_pid_nginx_worker = get_nginx_worker_pid_num(g_pid_nginx_master);
    if (g_pid_nginx_worker)
      {
      g_end_novnc_currently_on = 0; 
      }
    else
      {
      KERR("WARNING FAIL NGINX WORKER");
      clownix_timeout_add(100, timer_end_last, NULL, NULL, NULL);
      }
    }
  else
    {
    KERR("WARNING FAIL NGINX MASTER");
    clownix_timeout_add(100, timer_end_last, NULL, NULL, NULL);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void timer_nginx(void *data)
{
  int pid;
  char addr_port[MAX_NAME_LEN];

  if (g_terminate)
    return;

  if (g_port_display == 0)
    KOUT("ERROR");
  memset (addr_port, 0, MAX_NAME_LEN);
  snprintf(addr_port, MAX_NAME_LEN-1, "localhost:%d", g_port_display);
  pid = get_pid_num(BIN_WEBSOCKIFY, addr_port);
  if (g_terminate == 0)
    {
    if (pid)
      {
      pid_clone_launch(nginx_ok, nginx_ko, NULL,NULL,NULL,NULL,"vnc",-1,1);
      clownix_timeout_add(10, timer_end_last, NULL, NULL, NULL);
      }
    else
      {
      g_count_nginx_timer2 += 1;
      if (g_count_nginx_timer2 > 100)
        {
        KERR("WARNING NGINX START");
        g_count_nginx_timer2 = 0;
        }
      clownix_timeout_add(10, timer_nginx, NULL, NULL, NULL);
      }
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void timer_websockify(void *data)
{
  int pid;

  if (g_terminate)
    return;

  pid = get_pid_num(BIN_X11VNC, g_display);
  if (g_terminate == 0)
    {
    if (pid)
      {
      g_pid_websockify=pid_clone_launch(websockify, kwebsockify,
                                    NULL,NULL,NULL,NULL,"vnc",-1,1);
      g_count_nginx_timer1 = 0;
      g_count_nginx_timer2 = 0;
      clownix_timeout_add(10, timer_nginx, NULL, NULL, NULL);
      }
    else
      {
      g_count_websockify_timer += 1;
      if (g_count_websockify_timer > 100)
        {
        KERR("WARNING X11VNC START");
        g_count_websockify_timer = 0;
        }
      clownix_timeout_add(10, timer_websockify, NULL, NULL, NULL);
      }
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void timer_x11vnc(void *data)
{

  if (g_terminate)
    return;

  g_pid_x11vnc=pid_clone_launch(x11vnc, kx11vnc, NULL, NULL,
                                NULL, NULL, "vnc", -1, 1);
  g_count_websockify_timer = 0;
  clownix_timeout_add(10, timer_websockify, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void timer_wm2(void *data)
{

  if (g_terminate)
    return;

  pid_clone_launch(xsetroot,NULL,NULL,NULL,NULL,NULL,"vnc",-1,1);
  g_pid_wm2=pid_clone_launch(wm2,kwm2,NULL,NULL,NULL,NULL,"vnc",-1,1);
  clownix_timeout_add(10, timer_x11vnc, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int check_before_start(void)
{
  char lock_file[MAX_NAME_LEN];
  char addr_port[MAX_NAME_LEN];
  int pid, pid2, nostart = 0;

  if (g_terminate)
    return 0;

  if (g_port_display == 0)
    KOUT("ERROR");
  memset (lock_file, 0, MAX_NAME_LEN);
  snprintf(lock_file, MAX_NAME_LEN-1, "/tmp/.X%d-lock",(NOVNC_DISPLAY+g_rank));
  memset (addr_port, 0, MAX_NAME_LEN);
  snprintf(addr_port, MAX_NAME_LEN-1, "localhost:%d", g_port_display);
  pid = get_pid_num(BIN_XVFB, g_display);
  if (pid)
    {
    KERR("WARNING KILLING XVFB pid: %d", pid);
    kill(pid, SIGKILL);
    nostart = 1;
    }
  pid = get_pid_num(BIN_WM2, g_display);
  if (pid)
    {
    KERR("WARNING KILLING WM2 pid: %d", pid);
    kill(pid, SIGKILL);
    nostart = 1;
    }
  pid = get_pid_num(BIN_WEBSOCKIFY, addr_port);
  if (pid)
    {
    KERR("WARNING KILLING WEBSOCKIFY pid: %d", pid);
    kill(pid, SIGKILL);
    nostart = 1;
    }
  pid = get_pid_num(BIN_X11VNC, g_display);
  if (pid)
    {
    KERR("WARNING KILLING X11VNC pid: %d", pid);
    kill(pid, SIGKILL);
    nostart = 1;
    }
  pid = get_nginx_master_pid_num();
  if (pid)
    {
    KERR("WARNING KILLING NGINX MASTER pid: %d", pid);
    pid2 = get_nginx_worker_pid_num(pid);
    kill(pid, SIGKILL);
    if (pid2)
      {
      KERR("WARNING KILLING NGINX WORKER pid: %d", pid2);
      kill(pid2, SIGKILL);
      }
    nostart = 1;
    }
  if (!unlink(lock_file))
    KERR("WARNING ERASE OF LOCK FILE OF XVFB file: %s", lock_file);
  return nostart;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void timer_Xvfb(void *data)
{

  if (g_terminate)
    return;

  g_pid_Xvfb=pid_clone_launch(Xvfb,kXvfb,NULL,NULL,NULL,NULL,"vnc",-1,1);
  clownix_timeout_add(10, timer_wm2, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void timer_restart_novnc(void *data)
{
  int nostart;

  if (g_terminate)
    return;

  nostart = check_before_start();
  if (nostart == 0)
    {
    if (g_terminate == 0)
      clownix_timeout_add(10, timer_Xvfb, NULL, NULL, NULL);
    }
  else
    {
    clownix_timeout_add(10, timer_restart_novnc, NULL, NULL, NULL);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void timer_monitor(void *data)
{
  char addr_port[MAX_NAME_LEN];
  int pid;

  if (g_terminate)
    return;

  if (g_port_display == 0)
    KOUT("ERROR");
  memset (addr_port, 0, MAX_NAME_LEN);
  snprintf(addr_port, MAX_NAME_LEN-1, "localhost:%d", g_port_display);
  pid = get_pid_num(BIN_XVFB, g_display);
  if (!pid)
    restart_all_upon_problem();
  pid = get_pid_num(BIN_WM2, g_display);
  if (!pid)
    restart_all_upon_problem();
  pid = get_pid_num(BIN_WEBSOCKIFY, addr_port);
  if (!pid)
    restart_all_upon_problem();
  pid = get_pid_num(BIN_X11VNC, g_display);
  if (!pid)
    restart_all_upon_problem();
  pid = get_nginx_master_pid_num();
  if (!pid)
    restart_all_upon_problem();
  clownix_timeout_add(100, timer_monitor, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int end_novnc(int terminate)
{
  int result = -1;

  if (g_terminate)
    return result;

  if (g_end_novnc_currently_on)
    KERR("ERROR END NOVNC (IS ON) %d", g_end_novnc_currently_on);
  else
    result = 0;

  g_start = 0;
  g_terminate = terminate;
  g_end_novnc_currently_on = 1; 
  if (g_pid_nginx_master)
    kill(g_pid_nginx_master, SIGKILL);
  g_pid_nginx_master = 0;
  if (g_pid_nginx_worker)
    kill(g_pid_nginx_worker, SIGKILL);
  g_pid_nginx_worker = 0;
  if (g_pid_websockify)
    kill(g_pid_websockify, SIGKILL);
  g_pid_websockify = 0;
  if (g_pid_x11vnc)
    kill(g_pid_x11vnc, SIGKILL);
  g_pid_x11vnc = 0;
  if (g_pid_wm2)
    kill(g_pid_wm2, SIGKILL);
  g_pid_wm2 = 0;
  if (g_pid_Xvfb)
    kill(g_pid_Xvfb, SIGKILL);
  g_pid_Xvfb = 0;
  if (terminate)
    {
    memset (g_display, 0, MAX_NAME_LEN);
    g_port_display = 0;
    }
  else
    clownix_timeout_add(10, timer_restart_novnc, NULL, NULL, NULL);
  return result;
}   
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int start_novnc(void)
{
  int result = -1;
  if ((g_start != 1) && (g_terminate != 0))
    {
    if (!file_exists(BIN_XVFB, X_OK))
      KERR("\"%s\" not found or not executable\n", BIN_XVFB);
    else if (!file_exists(BIN_WM2, X_OK))
      KERR("\"%s\" not found or not executable\n", BIN_WM2);
    else if (!file_exists(BIN_X11VNC, X_OK))
      KERR("\"%s\" not found or not executable\n", BIN_X11VNC);
    else if (!file_exists(BIN_WEBSOCKIFY, X_OK))
      KERR("\"%s\" not found or not executable\n", BIN_WEBSOCKIFY);
    else if (!file_exists(BIN_NGINX, X_OK))
      KERR("\"%s\" not found or not executable\n", BIN_NGINX);
    else if (g_port_display)
      KERR("ERROR CANNOT START, port g_port_display %d initialized",
            g_port_display);
    else
      {
      result = 0;
      g_start = 1;
      g_end_novnc_currently_on = 1;
      g_terminate = 0;
      g_port_display = get_free_port_display(g_port_display_start, 10);
      if (g_port_display == 0)
        KERR("ERROR NO FREE PORT FROM %d range %d", g_port_display_start, 10);
      else
        {
        snprintf(g_display, MAX_NAME_LEN-1, ":%d", g_port_display - 5900);
        clownix_timeout_add(10, timer_Xvfb, NULL, NULL, NULL);
        clownix_timeout_add(500, timer_monitor, NULL, NULL, NULL);
        }
      }
    }
  else
    KERR("ERROR START NOVNC %d %d", g_start, g_terminate);
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void init_novnc(char *net_name, int rank)
{
  g_start = 0;
  g_terminate = 1;
  g_end_novnc_currently_on = 0;
  g_pid_websockify = 0;
  g_pid_x11vnc = 0;
  g_pid_wm2 = 0;
  g_pid_Xvfb = 0;
  g_pid_nginx_master = 0;
  g_pid_nginx_worker = 0;
  memset (g_display, 0, MAX_NAME_LEN);
  memset (g_net_name, 0, MAX_NAME_LEN);
  memset (g_x11vnc_log, 0, MAX_PATH_LEN);
  snprintf(g_x11vnc_log, MAX_PATH_LEN-1,
           "/var/lib/cloonix/%s/log/x11vnc.log", net_name);
  strncpy(g_net_name, net_name, MAX_NAME_LEN-1);
  g_rank = rank;
  g_port_display_start = 5900 + NOVNC_DISPLAY + g_rank;
  g_port_display = 0;
}
/*--------------------------------------------------------------------------*/

