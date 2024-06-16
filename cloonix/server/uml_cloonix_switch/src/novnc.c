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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "util_sock.h"
#include "cfg_store.h"
#include "pid_clone.h"
#include "novnc.h"
#include "file_read_write.h"

#define PYTHONHOME     "/usr/libexec/cloonix/common"
#define BIN_XVFB       "/usr/libexec/cloonix/server/cloonix-novnc-Xvfb"
#define BIN_XSETROOT   "/usr/libexec/cloonix/server/cloonix-novnc-xsetroot"
#define BIN_WM2        "/usr/libexec/cloonix/server/cloonix-novnc-wm2"
#define BIN_X11VNC     "/usr/libexec/cloonix/server/cloonix-novnc-x11vnc"
#define BIN_WEBSOCKIFY "/usr/libexec/cloonix/server/cloonix-novnc-websockify"
#define CERT           "--cert=/usr/libexec/cloonix/server/novnc.pem"
#define WEB            "--web=/usr/libexec/cloonix/common/share/novnc/"

static int g_Xvfb;
static int g_wm2;
static int g_x11vnc;
static int g_websockify;
static int g_rank;
static int g_port1;
static int g_port2;
static char g_net_name[MAX_NAME_LEN];
static char g_display[MAX_NAME_LEN];
static char g_ascii_port[MAX_NAME_LEN];

static int x11vnc(void *data);
static void timer_wm2(void *data);
static int g_end_novnc_currently_on;
static int g_terminate;


/****************************************************************************/
static int get_pid_num(char *binstring, char *display)
{
  FILE *fp;
  char ps_cmd[MAX_PATH_LEN];
  char line[MAX_PATH_LEN];
  int pid, result = 0;
  snprintf(ps_cmd, MAX_PATH_LEN-1, "%s axo  pid,args", PS_BIN);
  fp = popen(ps_cmd, "r");
  if (fp == NULL)
    KERR("ERROR %s", PS_BIN);
  else
    {
    memset(line, 0, MAX_PATH_LEN);
    while (fgets(line, MAX_PATH_LEN-1, fp))
      {
      if (strstr(line, binstring))
        {
        if (sscanf(line, "%d", &pid) == 1)
          {
          if (strstr(line, display))
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
  if (g_terminate == 0)
    {
    if (g_end_novnc_currently_on == 0)
      end_novnc(0);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void kXvfb(void *unused_data, int status, char *name)
{
  g_Xvfb = 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void kx11vnc(void *unused_data, int status, char *name)
{
  g_x11vnc = 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void kwm2(void *unused_data, int status, char *name)
{
  g_wm2 = 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void kwebsockify(void *unused_data, int status, char *name)
{
  g_websockify = 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int Xvfb(void *data)
{
  char *argv[] = {BIN_XVFB, g_display, "-nolisten", "tcp", "-noreset",
                                       "-dpms", "-br", "-ac", NULL};
  execv(argv[0], argv);
  KOUT("ERROR execv %s", argv[0]);
  return 0;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int x11vnc(void *data)
{
  char *argv[]={BIN_X11VNC, "-quiet", "-N", "-nopw", "-localhost", "-shared",
                            "-noshm", "-noxdamage", "-cursor", "arrow",
                            "-remap", "DEAD", "-ncache", "10", "-dpms",
                            "-display", g_display, NULL};

  execv(argv[0], argv);
  KOUT("ERROR execv %s", argv[0]);
  return 0;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int wm2(void *data)
{
  char *argv[]={BIN_WM2, g_display, NULL};
  setenv("DISPLAY", g_display, 1);
  setenv("CLOONIX_NET", g_net_name, 1);
  execv(argv[0], argv);
  KOUT("ERROR execv %s", argv[0]);
  return 0;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int xsetroot(void *data)
{
  char *argv[] = {BIN_XSETROOT, "-display", g_display, "-solid", "grey", NULL};
  execv(argv[0], argv);
  KOUT("ERROR execv %s", argv[0]);
  return 0;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int websockify(void *data)
{
  char addr_port[MAX_NAME_LEN];
  char *argv[]={BIN_WEBSOCKIFY, "--run-once",
                WEB, CERT, g_ascii_port, addr_port, NULL};
  setenv("PYTHONHOME", PYTHONHOME, 1);
  memset (addr_port, 0, MAX_NAME_LEN);
  snprintf(addr_port, MAX_NAME_LEN-1, "localhost:%d", g_port1);
  execv(argv[0], argv);
  KOUT("ERROR execv %s", argv[0]);
  return 0;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void timer_end_last(void *data)
{
  g_end_novnc_currently_on = 0; 
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void timer_websockify(void *data)
{
  int pid = get_pid_num(BIN_X11VNC, g_display);
  if (g_terminate == 0)
    {
    if (pid)
      {
      g_websockify=pid_clone_launch(websockify, kwebsockify,
                                    NULL,NULL,NULL,NULL,"vnc",-1,1);
      clownix_timeout_add(50, timer_end_last, NULL, NULL, NULL);
      }
    else
      {
      clownix_timeout_add(10, timer_websockify, NULL, NULL, NULL);
      }
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void timer_x11vnc(void *data)
{
  if (g_terminate == 0)
    {
    g_x11vnc=pid_clone_launch(x11vnc,kx11vnc,NULL,NULL,NULL,NULL,"vnc",-1,1);
    clownix_timeout_add(10, timer_websockify, NULL, NULL, NULL);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void timer_wm2(void *data)
{
  if (g_terminate == 0)
    {
    pid_clone_launch(xsetroot,NULL,NULL,NULL,NULL,NULL,"vnc",-1,1);
    g_wm2=pid_clone_launch(wm2,kwm2,NULL,NULL,NULL,NULL,"vnc",-1,1);
    clownix_timeout_add(10, timer_x11vnc, NULL, NULL, NULL);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int check_before_start(void)
{
  char addr_port[MAX_NAME_LEN];
  int pid, nostart = 0;
  memset (addr_port, 0, MAX_NAME_LEN);
  snprintf(addr_port, MAX_NAME_LEN-1, "localhost:%d", g_port1);
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
  return nostart;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void timer_Xvfb(void *data)
{
  int nostart = check_before_start();
  if (g_terminate == 0)
    {
    if (nostart == 0)
      {
      g_Xvfb=pid_clone_launch(Xvfb,kXvfb,NULL,NULL,NULL,NULL,"vnc",-1,1);
      clownix_timeout_add(10, timer_wm2, NULL, NULL, NULL);
      }
    else
      {
      clownix_timeout_add(10, timer_Xvfb, NULL, NULL, NULL);
      }
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void timer_restart_novnc(void *data)
{
  int nostart = check_before_start();
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
void end_novnc(int terminate)
{
  g_terminate = terminate;
  if (g_end_novnc_currently_on == 0)
    {
    g_end_novnc_currently_on = 1; 
    if (g_websockify)
      kill(g_websockify, SIGKILL);
    g_websockify = 0;
    if (g_x11vnc)
      kill(g_x11vnc, SIGKILL);
    g_x11vnc = 0;
    if (g_wm2)
      kill(g_wm2, SIGKILL);
    g_wm2 = 0;
    if (g_Xvfb)
      kill(g_Xvfb, SIGKILL);
    g_Xvfb = 0;
    clownix_timeout_add(10, timer_restart_novnc, NULL, NULL, NULL);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void timer_monitor(void *data)
{
  char addr_port[MAX_NAME_LEN];
  int pid;
  memset (addr_port, 0, MAX_NAME_LEN);
  snprintf(addr_port, MAX_NAME_LEN-1, "localhost:%d", g_port1);
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
  clownix_timeout_add(100, timer_monitor, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void kill_before_start(void)
{
  char lock_file[MAX_NAME_LEN];
  char addr_port[MAX_NAME_LEN];
  int pid;
  memset (addr_port, 0, MAX_NAME_LEN);
  memset (lock_file, 0, MAX_NAME_LEN);
  snprintf(addr_port, MAX_NAME_LEN-1, "localhost:%d", g_port1);
  snprintf(lock_file, MAX_NAME_LEN-1, "/tmp/.X%d-lock", (NOVNC_DISPLAY + g_rank));
  pid = get_pid_num(BIN_XVFB, g_display);
  if (pid)
    {
    KERR("WARNING KILL OF PID OF XVFB pid: %d", pid);
    kill(pid, SIGKILL);
    }
  pid = get_pid_num(BIN_WM2, g_display);
  if (pid)
    {
    KERR("WARNING KILL OF PID OF WM2 pid: %d", pid);
    kill(pid, SIGKILL);
    }
  pid = get_pid_num(BIN_WEBSOCKIFY, addr_port);
  if (pid)
    {
    KERR("WARNING KILL OF PID OF WEBSOCKIFY pid: %d", pid);
    kill(pid, SIGKILL);
    }
  pid = get_pid_num(BIN_X11VNC, g_display);
  if (pid)
    {
    KERR("WARNING KILL OF PID OF X11VNC pid: %d", pid);
    kill(pid, SIGKILL);
    }
  if (!unlink(lock_file))
    KERR("WARNING ERASE OF LOCK FILE OF XVFB file: %s", lock_file);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void init_novnc(char *net_name, int rank, char *ascii_port)
{
  g_end_novnc_currently_on = 1;
  g_terminate = 0;
  g_websockify = 0;
  g_x11vnc = 0;
  g_wm2 = 0;
  g_Xvfb = 0;
  if (!file_exists(BIN_XVFB, X_OK))
    KOUT("\"%s\" not found or not executable\n", BIN_XVFB);
  if (!file_exists(BIN_WM2, X_OK))
    KOUT("\"%s\" not found or not executable\n", BIN_WM2);
  if (!file_exists(BIN_X11VNC, X_OK))
    KOUT("\"%s\" not found or not executable\n", BIN_X11VNC);
  if (!file_exists(BIN_WEBSOCKIFY, X_OK))
    KOUT("\"%s\" not found or not executable\n", BIN_WEBSOCKIFY);
  memset (g_net_name, 0, MAX_NAME_LEN);
  memset (g_display, 0, MAX_NAME_LEN);
  memset (g_ascii_port, 0, MAX_NAME_LEN);
  strncpy(g_net_name, net_name, MAX_NAME_LEN-1);
  strncpy(g_ascii_port, ascii_port, MAX_NAME_LEN-1);
  g_rank = rank;
  snprintf(g_display, MAX_NAME_LEN-1, ":%d", (NOVNC_DISPLAY + g_rank));
  kill_before_start();
  g_port1 = 5900 + NOVNC_DISPLAY + g_rank;
  g_port2 = 5900 + NOVNC_DISPLAY + g_rank+1;
  g_Xvfb=pid_clone_launch(Xvfb,kXvfb,NULL,NULL,NULL,NULL,"vnc",-1,1);
  clownix_timeout_add(100, timer_wm2, NULL, NULL, NULL);
  clownix_timeout_add(500, timer_monitor, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

