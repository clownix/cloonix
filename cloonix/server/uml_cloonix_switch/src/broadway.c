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
#include <sys/wait.h>


#include "io_clownix.h"
#include "rpc_clownix.h"
#include "cfg_store.h"

void hide_real_machine(void);
int get_glob_req_self_destruction(void);
char *get_ascii_broadway_port(void);
int get_uml_cloonix_started(void);
int g_broadwayd_on;

/*--------------------------------------------------------------------------*/
static char g_net_name[MAX_NAME_LEN];
static char *g_ascii_port;
static char *g_argv_broadwayd[5];
static int  g_broadwayd_pid;
static int  g_forbid_delayed_gui;
static int  g_gui_pid;
static char *g_display;
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void set_all_env(void)
{
  char run_dir[MAX_PATH_LEN];
  setenv("GDK_BACKEND", "broadway", 1);
  setenv("BROADWAY_DISPLAY", ":0", 1);
  setenv("LC_ALL", "C", 1);
  setenv("LANG", "C", 1);

  memset(run_dir, 0, MAX_PATH_LEN);
  snprintf(run_dir, MAX_PATH_LEN-1,
  "/var/lib/cloonix/%s/runbroadway", g_net_name);
  setenv("XDG_RUNTIME_DIR", run_dir, 1);
  setenv("XDG_CACHE_HOME", run_dir, 1);
  setenv("XDG_DATA_HOME", run_dir, 1);

  memset(run_dir, 0, MAX_PATH_LEN);
  snprintf(run_dir, MAX_PATH_LEN-1,
  "/var/lib/cloonix/%s/runbroadway/.config", g_net_name);
  setenv("XDG_CONFIG_HOME", run_dir, 1);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void clean_all_llid(void)
{ 
  int i, llid, nb_chan = get_clownix_channels_nb();
  for (i=0; i<=nb_chan; i++)
    {
    llid = channel_get_llid_with_cidx(i);
    if ((llid) && (msg_exist_channel(llid)))
      msg_delete_channel(llid);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void delayed_gui(void *data)
{
  char *argv_gui[] = {"/usr/libexec/cloonix/client/cloonix-gui",
                      "/usr/libexec/cloonix/common/etc/cloonix.cfg",
                      g_net_name, NULL};
  if (g_gui_pid != 0)
    {
    KERR("ERROR GUI pid exists: %d", g_gui_pid);
    kill(g_gui_pid, SIGKILL);
    g_gui_pid = 0;
    }
  else
    {
    if ((g_gui_pid = fork()) < 0)
      KOUT("ERROR");
    if (g_gui_pid == 0)
      {  
      clean_all_llid();
      clearenv();
      if (g_display)
        setenv("DISPLAY", g_display, 1);
      set_all_env();
      hide_real_machine();
      execv(argv_gui[0], argv_gui);
      KOUT("ERROR GUI execv %d", getpid());
      }
    }
  g_forbid_delayed_gui = 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void try_relaunch_gui(void)
{
  if (g_gui_pid != 0)
    KERR("WARNING MISS RELAUNCH GUI pid: %d", g_gui_pid);
  else if (g_forbid_delayed_gui == 0)
    {
    g_forbid_delayed_gui = 1;
    clownix_timeout_add(10, delayed_gui, NULL, NULL, NULL);
    }
  else
    KERR("WARNING MISS RELAUNCH GUI");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int get_broadwayd_pid(void)
{
  FILE *fp;
  char line[MAX_PATH_LEN];
  char name[MAX_NAME_LEN];
  char cmd[MAX_PATH_LEN];
  char cmd2[MAX_PATH_LEN];
  int pid, result = 0;
  fp = popen("/usr/libexec/cloonix/common/ps", "r");
  if (fp == NULL)
    KERR("ERROR /usr/libexec/cloonix/common/ps");
  else
    {
    memset(line, 0, MAX_PATH_LEN);
    memset(cmd, 0, MAX_PATH_LEN);
    snprintf(cmd, MAX_PATH_LEN-1, "%s -p %s", BROADWAYD_BIN, g_ascii_port);
    while (fgets(line, MAX_PATH_LEN-1, fp))
      {
      if (strstr(line, BROADWAYD_BIN))
        {
        if (sscanf(line, "%d %s %200c", &pid, name, cmd2) == 3)
          {
          if (!strncmp(cmd2, cmd, strlen(cmd)))
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
static void timer_broadway_heartbeat(void *data)
{
  int pid, status;

  if (g_broadwayd_on == 0)
    {
    clownix_timeout_add(300, timer_broadway_heartbeat, NULL, NULL, NULL);
    return;
    }
  if (get_glob_req_self_destruction())
    return;
  if (!get_uml_cloonix_started())
    {
    clownix_timeout_add(100, timer_broadway_heartbeat, NULL, NULL, NULL);
    return;
    }
  if (g_gui_pid != 0)
    {
    if ((pid = waitpid(g_gui_pid, &status, WNOHANG)) > 0)
      {
      if (pid != g_gui_pid)
        KERR("ERROR %s %d %d GUI", g_net_name, pid, g_gui_pid);
      KERR("WARNING  GUI WAIT DONE %s %d", g_net_name, g_gui_pid);
      g_gui_pid = 0;
      try_relaunch_gui();
      }
    else if (kill(g_gui_pid, 0))
      {
      KERR("WARNING GUI PID GONE %s  %d", g_net_name, getpid());
      g_gui_pid = 0;
      try_relaunch_gui();
      }
    }
  else
    { 
    try_relaunch_gui();
    }

  if ((g_broadwayd_pid) &&
      ((pid = waitpid(g_broadwayd_pid, &status, WNOHANG)) > 0))
    {
    if (pid != g_broadwayd_pid)
      KERR("ERROR %s %d %d BROADWAYD", g_net_name, pid, g_broadwayd_pid);
    else
      {
      KERR("WARNING BROADWAYD WAITPID %s %d", g_net_name, g_broadwayd_pid);
      g_broadwayd_pid = 0;
      }
    }
  if (get_broadwayd_pid() == 0)
    {
    if (g_broadwayd_pid)
      {
      KERR("ERROR %s pid exists:%d BROADWAYD", g_net_name, g_broadwayd_pid);
      kill(g_broadwayd_pid, SIGKILL);
      g_broadwayd_pid = 0;
      }
    else
      {
      if ((g_broadwayd_pid = fork()) < 0)
        KOUT("ERROR");
      if (g_broadwayd_pid == 0)
        {
        clean_all_llid();
        clearenv();
        set_all_env();
        execv(g_argv_broadwayd[0], g_argv_broadwayd);
        KOUT("ERROR BROADWAYD execv %d", getpid());
        }
      }
    }
  clownix_timeout_add(200, timer_broadway_heartbeat, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void broadway_exit(void)
{
  if (g_gui_pid)
    kill(g_gui_pid, SIGKILL);
  if (g_broadwayd_pid)
    kill(g_broadwayd_pid, SIGKILL);
  g_gui_pid = 0;
  g_broadwayd_pid = 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void broadway_on_off(int on)
{
  g_broadwayd_on = on;
  if (g_broadwayd_on == 0)
    broadway_exit();
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void broadway_init(void)
{
  char *net = cfg_get_cloonix_name();
  memset(g_net_name, 0, MAX_NAME_LEN);
  strncpy(g_net_name, net, MAX_NAME_LEN-1);
  g_broadwayd_on = 0;
  g_ascii_port = get_ascii_broadway_port();
  g_argv_broadwayd[0] = BROADWAYD_BIN;
  g_argv_broadwayd[1] = "-p";
  g_argv_broadwayd[2] = g_ascii_port;
  g_argv_broadwayd[3] = NULL;
  g_forbid_delayed_gui = 0;
  g_gui_pid = 0;
  g_broadwayd_pid = 0;
  g_display = getenv("DISPLAY");
  clownix_timeout_add(100, timer_broadway_heartbeat, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/


