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
#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include "io_clownix.h"
#include "rpc_clownix.h"
#include "doorways_sock.h"
#include "commun_consts.h"
#include "pidwait.h"
#include "bank.h"
#include "pid_clone.h"
#include "topo.h"
#include "client_clownix.h"
#include "menu_utils.h"
#include "cloonix.h"

int get_cloonix_rank(void);
GtkWidget *get_main_window(void);
static t_pid_wait *g_head_pid_waiting;
static t_pid_wait *g_head_pid_active;

static int g_display_pool_fifo[X11_DISPLAY_IDX_XEPHYR_MAX];
static int g_display_pool_read;
static int g_display_pool_write;

typedef struct t_hysteresis
{
  int  type;
  char name[MAX_NAME_LEN];
  struct t_hysteresis *prev;
  struct t_hysteresis *next;
} t_hysteresis;

static t_hysteresis *g_head_hysteresis;


/*****************************************************************************/
static t_hysteresis *hysteresis_find(int type, char *name)
{
  t_hysteresis *cur = g_head_hysteresis;
  while(cur)
    {
    if ((cur->type == type) &&
        (!strcmp(name, cur->name)))
      break;
    cur = cur->next;
    }
  return cur;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static t_hysteresis *hysteresis_alloc(int type, char *name)
{
  t_hysteresis *cur = hysteresis_find(type, name);
  if (cur)
    {
    KERR("ERROR EXISTS %d %s", type, name);
    cur = NULL;
    }
  else
    {
    cur = (t_hysteresis *) malloc(sizeof(t_hysteresis));
    memset(cur, 0, sizeof(t_hysteresis));
    cur->type = type;
    strncpy(cur->name, name, MAX_NAME_LEN-1);
    if (g_head_hysteresis)
      g_head_hysteresis->prev = cur;
    cur->next = g_head_hysteresis;
    g_head_hysteresis = cur;
    }
  return cur;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void hysteresis_free(t_hysteresis *input)
{
  t_hysteresis *cur = hysteresis_find(input->type, input->name);
  if (!cur)
    KERR("ERROR DOES NOT EXIST %d %s", input->type, input->name);
  else if (cur != input)
    KERR("ERROR NOT SAME %p %p", input, cur);
  else
    {
    if (cur->prev)
      cur->prev->next = cur->next;
    if (cur->next)
      cur->next->prev = cur->prev;
    if (cur == g_head_hysteresis)
      g_head_hysteresis = cur->next;
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static t_pid_wait *active_find(int type, char *name, int num)
{ 
  t_pid_wait *cur = g_head_pid_active;
  while(cur)
    {
    if ((cur->type == type) &&
        (!strcmp(name, cur->name)) &&
        (num == cur->num))
      break;
    cur = cur->next;
    } 
  return cur;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
static void init_display_pool(void)
{
  int i, val, rank = get_cloonix_rank();
  for(i = 0; i < X11_DISPLAY_IDX_XEPHYR_MAX-1; i++)
    {
    val = X11_DISPLAY_XWY_RANK_OFFSET * rank + X11_DISPLAY_OFFSET_XEPHYR + i;
    g_display_pool_fifo[i] = val;
    }
  g_display_pool_fifo[X11_DISPLAY_IDX_XEPHYR_MAX - 1] = 0;
  g_display_pool_read = 0;
  g_display_pool_write =  X11_DISPLAY_IDX_XEPHYR_MAX - 1;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int alloc_display_pool(void)
{
  int result = 0;
  if(g_display_pool_read != g_display_pool_write)
    {
    result = g_display_pool_fifo[g_display_pool_read];
    g_display_pool_fifo[g_display_pool_read] = 0;
    g_display_pool_read = g_display_pool_read + 1;
    if (g_display_pool_read == X11_DISPLAY_IDX_XEPHYR_MAX)
      g_display_pool_read = 0;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void free_display_pool(int val)
{
  g_display_pool_fifo[g_display_pool_write] = val;
  g_display_pool_write = g_display_pool_write + 1;
  if (g_display_pool_write == X11_DISPLAY_IDX_XEPHYR_MAX)
    g_display_pool_write = 0;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static int unlink_x11_socket_silent(int num, char *x11path)
{     
  int val, rank = get_cloonix_rank(), result = 0;
  val = X11_DISPLAY_XWY_RANK_OFFSET * rank + X11_DISPLAY_OFFSET_XEPHYR;
  if ((num < val) || (num >= (val + X11_DISPLAY_IDX_XEPHYR_MAX)))
    KOUT("ERROR %d %d", num, val);
  memset(x11path, 0, MAX_PATH_LEN);
  snprintf(x11path, MAX_PATH_LEN-1, X11_DISPLAY_PREFIX, num);
  if (!access(x11path, F_OK))
    {     
    result = 1;
    unlink(x11path);
    }
  return result;
}           
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int unlink_x11_socket_if_exists(int num)
{
  char x11path[MAX_PATH_LEN];
  int result = unlink_x11_socket_silent(num, x11path);
  if (result)
    KERR("ERROR ERASING %s", x11path);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void get_pid_wait_type(int type, char *ascii_type)
{ 
  memset(ascii_type, 0, MAX_NAME_LEN);
  switch (type)
    {
    case type_pid_spicy_gtk:
      strcpy(ascii_type, "SPICY_GTK");
      break;
    case type_pid_xephyr_frame:
      strcpy(ascii_type, "XEPHYR_FRAME");
      break;
    case type_pid_xephyr_session:
      strcpy(ascii_type, "XEPHYR_SESSION");
      break;
    case type_pid_wireshark:
      strcpy(ascii_type, "WIRESHARK"); 
      break;
    case type_pid_dtach:
      strcpy(ascii_type, "DTACH");
      break;
    case type_pid_crun_screen:
      strcpy(ascii_type, "CRUN_SCREEN");
      break;
    default:
      KOUT("ERROR %d", type);
    }
}
/*--------------------------------------------------------------------------*/
 
/****************************************************************************/
static int get_process_pid(char *cmdpath, char *sock)
{
  FILE *fp;
  char line[2*MAX_PATH_LEN];
  char name[MAX_NAME_LEN];
  char cmd[2*MAX_PATH_LEN];
  int pid, result = 0;
  fp = popen("/usr/libexec/cloonix/cloonfs/ps axo pid,args", "r");
  if (fp == NULL)
    KERR("ERROR %s %s", cmdpath, sock);
  else
    {
    memset(line, 0, 2*MAX_PATH_LEN);
    while (fgets(line, 2*MAX_PATH_LEN-1, fp))
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
static void kill_previous_process(int type, char *name, int pid)
{
  char ascii_type[MAX_NAME_LEN];
  get_pid_wait_type(type, ascii_type);
  KERR("WARNING KILL %s %s %d", ascii_type, name, pid);
  if (pid_clone_kill_single(pid))
    {
    KERR("ERROR KILL PID %s %s %d", ascii_type, name, pid);
    kill(pid, SIGKILL);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int kill_previous_spicy_gtk_process(char *name)
{
  int pid = bank_get_spicy_gtk_pid(name);
  if (pid)
    kill_previous_process(type_pid_spicy_gtk, name, pid);
  return pid;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int kill_previous_xephyr_frame_process(char *ident)
{
  int pid;
  pid = get_process_pid(XEPHYR_BIN, ident);
  if (pid)
    kill_previous_process(type_pid_xephyr_frame, ident, pid);
  return pid;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int kill_previous_xephyr_session_process(char *name)
{         
  FILE *fp;
  char line[2*MAX_PATH_LEN];
  int pid, result = 0;
  char ssh[MAX_PATH_LEN];
  char addr[MAX_PATH_LEN];

  memset(ssh,    0, MAX_PATH_LEN);
  memset(addr,   0, MAX_NAME_LEN);
  strncpy(ssh,
  "/usr/libexec/cloonix/cloonfs/cloonix-dropbear-ssh", MAX_PATH_LEN-1);
  strncpy(addr,  get_doors_client_addr(), MAX_PATH_LEN-1);

  fp = popen("/usr/libexec/cloonix/cloonfs/ps axo pid,args", "r");
  if (fp == NULL)
    KERR("ERROR %s", name);
  else 
    { 
    memset(line, 0, 2*MAX_PATH_LEN);
    while (fgets(line, 2*MAX_PATH_LEN-1, fp))
      {
      if ((strstr(line, LXSESSION)) && (strstr(line, ssh)) &&
          (strstr(line, name)) && (strstr(line, addr)))
        {
        if (sscanf(line, "%d", &pid) == 1)
          {
          result = pid;
          kill(pid, SIGKILL);
          }
        }
      }
    pclose(fp);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int kill_previous_dtach_process(char *name)
{
  int pid = bank_get_dtach_pid(name);
  if (pid)
    kill_previous_process(type_pid_dtach, name, pid);
  return pid;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int kill_previous_crun_screen_process(char *name)
{
  int pid = bank_get_crun_screen_pid(name);
  if (pid)
    kill_previous_process(type_pid_crun_screen, name, pid);
  return pid;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int kill_previous_wireshark_process(char *sock)
{
  int result = 0;
  int pid_wireshark = get_process_pid(WIRESHARK_BIN,  sock);
  int pid_dumpcap = get_process_pid(DUMPCAP_BIN, sock);
  if (pid_dumpcap)
    {
    kill_previous_process(type_pid_wireshark, sock, pid_dumpcap);
    result = pid_dumpcap;
    }
  if (pid_wireshark)
    {
    kill_previous_process(type_pid_wireshark, sock, pid_wireshark);
    result = pid_wireshark;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void free_pid(t_pid_wait *cur)
{
  int i, val, rank = get_cloonix_rank();
  char x11path[MAX_PATH_LEN];
  if (cur->type == type_pid_xephyr_frame)
    {
    val = X11_DISPLAY_XWY_RANK_OFFSET * rank + X11_DISPLAY_OFFSET_XEPHYR;
    if ((cur->display < val) ||
        (cur->display >= (val + X11_DISPLAY_IDX_XEPHYR_MAX)))
      KOUT("ERROR %d %d", cur->display, val);
    free_display_pool(cur->display);
    unlink_x11_socket_silent(cur->display, x11path);
    }
  for (i=0; cur->argv[i] != NULL; i++)
    clownix_free(cur->argv[i], __FUNCTION__);
  clownix_free(cur->argv, __FUNCTION__);
  if (cur->next)
    cur->next->prev = cur->prev;
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur == g_head_pid_active)
    g_head_pid_active = cur->next;
  clownix_free(cur, __FUNCTION__);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static char *alloc_argv(char *str)
{   
  int len = strlen(str);
  char *argv = (char *)clownix_malloc(len + 1, 15);
  memset(argv, 0, len + 1);
  strncpy(argv, str, len);
  return argv;
} 
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static t_pid_wait *alloc_pid(int type, char *name, int num,
                             char *ident, char **argv)
{
  int i;
  t_pid_wait *cur = (t_pid_wait *) clownix_malloc(sizeof(t_pid_wait), 8);
  memset(cur, 0, sizeof(t_pid_wait));
  get_pid_wait_type(type, cur->ascii_type);
  cur->type = type;
  cur->num = num;
  strncpy(cur->name, name, MAX_NAME_LEN-1);
  strncpy(cur->ident, ident, MAX_PATH_LEN-1);
  cur->argv = (char **)clownix_malloc(20 * sizeof(char *), 13);
  memset(cur->argv, 0, 20 * sizeof(char *));
  for (i=0; i<19; i++)
    {
    if (argv[i] == NULL)
      break;
    cur->argv[i] = alloc_argv(argv[i]);
    }
  if (i >= 18)
    KOUT(" ");
  if (type == type_pid_xephyr_frame)
    {
    clownix_free(cur->argv[1], __FUNCTION__);
    cur->argv[1] = alloc_argv(ident);
    }
  if (g_head_pid_waiting)
    g_head_pid_waiting->prev = cur;
  cur->next = g_head_pid_waiting;
  g_head_pid_waiting = cur;
  return cur;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void from_waiting_to_active(t_pid_wait *cur)
{
  if (cur->next)
    cur->next->prev = cur->prev;
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur == g_head_pid_waiting)
    g_head_pid_waiting = cur->next;
  cur->prev = NULL;
  if (g_head_pid_active)
    g_head_pid_active->prev = cur;
  cur->next = g_head_pid_active;
  g_head_pid_active = cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void wireshark_cb(int tid, char *name, int num, int status)
{       
  t_pid_wait *cur = pidwait_find(type_pid_wireshark, name, num);
  if (!cur)
    KERR("ERROR SYNC WIRESHARK %s %d", name, num);
  else if (tid != 22)
    KERR("ERROR SYNC WIRESHARK %d %s %d %d", tid, name, num, status);
  else if (!status)
    cur->sync_wireshark = 1;
}   
/*--------------------------------------------------------------------------*/

      
/****************************************************************************/
static void action_set_pid(t_pid_wait *cur, int new_pid, int old_pid)
{
  int pid;
  if (cur->pid != old_pid) 
    {
    KERR("ERROR PID %s pid:%d old_pid:%d new_pid:%d",
          cur->name, cur->pid, old_pid, new_pid);
    }   
  cur->pid = new_pid;
  switch (cur->type)
    {
    case type_pid_spicy_gtk:
      pid = bank_get_spicy_gtk_pid(cur->name);
      if (pid != old_pid)
        KERR("ERROR PID %s pid:%d %d", cur->name, pid, old_pid);
      bank_set_spicy_gtk_pid(cur->name, new_pid);
      break;
      
    case type_pid_xephyr_frame:
      pid = bank_get_xephyr_frame_pid(cur->name);
      if (pid != old_pid)
        KERR("ERROR PID %s pid:%d %d", cur->name, pid, old_pid);
      bank_set_xephyr_frame_pid(cur->name, new_pid);
      break;
    
    case type_pid_xephyr_session:
      pid = bank_get_xephyr_session_pid(cur->name);
      if (pid != old_pid)
        KERR("ERROR PID %s pid:%d %d", cur->name, pid, old_pid);
      bank_set_xephyr_session_pid(cur->name, new_pid);
      break;
    
    case type_pid_wireshark:
      pid = bank_get_wireshark_pid(cur->name, cur->num);
      if (pid != old_pid)
        KERR("ERROR PID %s pid:%d %d", cur->name, pid, old_pid);
      bank_set_wireshark_pid(cur->name, cur->num, new_pid);
      break;

    case type_pid_dtach:
      pid = bank_get_dtach_pid(cur->name);
      if (pid != old_pid)
        KERR("ERROR PID %s pid:%d %d", cur->name, pid, old_pid);
      bank_set_dtach_pid(cur->name, new_pid);
      break;

    case type_pid_crun_screen:
      pid = bank_get_crun_screen_pid(cur->name);
      if (pid != old_pid)
        KERR("ERROR PID %s pid:%d %d", cur->name, pid, old_pid);
      bank_set_crun_screen_pid(cur->name, new_pid);
      break;

    default:
      KOUT("ERROR %d", cur->type);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int start_launch_xephyr_session(void *ptr)
{                        
  t_pid_wait *cur = (t_pid_wait *) ptr;
  char **argv = cur->argv;
  setenv("DISPLAY", cur->ascii_display, 1);
  execvp(argv[0], argv);
  KOUT("ERROR execvp");
  return 0;
}       
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int start_launch(void *ptr)
{
  t_pid_wait *cur = (t_pid_wait *) ptr;
  char **argv = cur->argv;
  execvp(argv[0], argv);
  KOUT("ERROR execvp");
  return 0;
}       
/*---------------------------------------------------------------------------*/
    
/****************************************************************************/
static void timer_post_death(void *data)
{
  t_hysteresis *hyst = (t_hysteresis *) data;
  hysteresis_free(hyst);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void death_launched(void *data, int status, char *name, int pid)
{
  t_hysteresis *hyst;
  t_pid_wait *cur = (t_pid_wait *) data;
  if (strcmp(name, cur->name))
    KERR("ERROR %d %s %s", cur->type, name, cur->name);
  if (pid != cur->pid)
    KERR("ERROR %d %s %d %d", cur->type, name, pid, cur->pid);
  if (cur->type == type_pid_xephyr_frame)
    {
    kill_previous_xephyr_session_process(cur->name);
    hyst = hysteresis_alloc(type_pid_xephyr_frame, cur->name);
    clownix_timeout_add(150, timer_post_death, (void *) hyst, NULL, NULL);
    }
  if (cur->type == type_pid_xephyr_session)
    {
    hyst = hysteresis_alloc(type_pid_xephyr_session, cur->name);
    clownix_timeout_add(150, timer_post_death, (void *) hyst, NULL, NULL);
    }
  action_set_pid(cur, 0, pid);
  cur->release_forbiden1 = 0;

} 
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_after_launch(void *data)
{   
  t_pid_wait *cur = (t_pid_wait *) data;
  gtk_widget_grab_focus(get_main_window());
  gtk_widget_grab_focus(get_gtkwidget_canvas());
  switch (cur->type)
    {
    case type_pid_spicy_gtk:
    case type_pid_wireshark:
    case type_pid_dtach:
    case type_pid_crun_screen:
      cur->release_forbiden2 = 0;
      break;
    case type_pid_xephyr_frame:
      crun_item_xephyr_session(cur->name);
      cur->release_forbiden2 = 0;
      break;
    case type_pid_xephyr_session:
      cur->release_forbiden2 = 0;
      break;
    default:
      KOUT("ERROR %d", cur->type);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int launch_new_pid_xephyr_session(t_pid_wait *cur)
{
  int pid;
  pid = pid_clone_launch(start_launch_xephyr_session, death_launched, NULL,
                        (void *) cur, (void *) cur, NULL, cur->name, -1, 0);
  clownix_timeout_add(40, timer_after_launch, (void *) cur, NULL, NULL);
  return pid;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int launch_new_pid(t_pid_wait *cur)
{
  int pid;
  pid = pid_clone_launch(start_launch, death_launched, NULL,
                        (void *) cur, (void *) cur, NULL, cur->name, -1, 0);
  clownix_timeout_add(40, timer_after_launch, (void *) cur, NULL, NULL);
  return pid;
} 
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int timer_monitor_waiting_kill_previous(void)
{
  t_pid_wait *cur = g_head_pid_waiting;
  int result = 0;
  while(cur)
    {
    if (cur->release_previous_done == 0)
      {
      cur->release_previous_done = 1;
      switch (cur->type)
        {
        case type_pid_spicy_gtk:
          if (kill_previous_spicy_gtk_process(cur->name))
            result = 1;
          break;
        case type_pid_xephyr_frame:
          if (kill_previous_xephyr_frame_process(cur->ident))
            result = 1;
          if (kill_previous_xephyr_session_process(cur->name))
            result = 1;
          break;
        case type_pid_xephyr_session:
          if (kill_previous_xephyr_session_process(cur->name))
            {
            KERR("ERROR %s %s %s %s", cur->ascii_type, cur->name,
                                      cur->ident, cur->ascii_display);
            result = 1;
            }
          break;
        case type_pid_wireshark:
          if (kill_previous_wireshark_process(cur->ident))
            result = 1;
          break;
        case type_pid_dtach:
          if (kill_previous_dtach_process(cur->name))
            result = 1;
          break;
        case type_pid_crun_screen:
          if (kill_previous_crun_screen_process(cur->name))
            result = 1;
          break;
        default:
          KOUT("ERROR %d", cur->type);
        }
      }
    cur = cur->next;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_monitor_waiting_transfer_to_active(void)
{
  t_pid_wait *next, *cur = g_head_pid_waiting;
  while(cur)
    {
    next = cur->next;
    if (cur->release_previous_done == 1)
      {
      switch (cur->type)
        {
        case type_pid_spicy_gtk:
          if ((!bank_get_spicy_gtk_pid(cur->name)) &&
              (!active_find(cur->type, cur->name, cur->num)))
            from_waiting_to_active(cur);
          break;
        case type_pid_xephyr_frame:
          if ((!bank_get_xephyr_frame_pid(cur->name)) &&
              (!bank_get_xephyr_session_pid(cur->name)) &&
              (!active_find(type_pid_xephyr_frame, cur->name, cur->num))&& 
              (!active_find(type_pid_xephyr_session, cur->name, cur->num))&&
              (!hysteresis_find(type_pid_xephyr_frame, cur->name)) &&
              (!hysteresis_find(type_pid_xephyr_session, cur->name)) &&
              (!unlink_x11_socket_if_exists(cur->display)))
            from_waiting_to_active(cur);
          break;
        case type_pid_xephyr_session:
          if ((!bank_get_xephyr_session_pid(cur->name)) &&
              (!active_find(cur->type, cur->name, cur->num))&&
              (!hysteresis_find(type_pid_xephyr_session, cur->name)))
            from_waiting_to_active(cur);
          break;
        case type_pid_wireshark:
          if ((get_process_pid(WIRESHARK_BIN, cur->ident)) ||
              (get_process_pid(DUMPCAP_BIN, cur->ident)))
            {
            cur->count_sync += 1;
            if (cur->count_sync > 3)
              KERR("ERROR %s %d", cur->name, cur->num);
            else
              client_sync_wireshark(22,cur->name,cur->num,0,wireshark_cb);
            }
          else if (!active_find(cur->type, cur->name, cur->num))
            from_waiting_to_active(cur);
          break;
        case type_pid_dtach:
          if ((!bank_get_dtach_pid(cur->name)) &&
              (!active_find(cur->type, cur->name, cur->num)))
            from_waiting_to_active(cur);
          break;
        case type_pid_crun_screen:
          if ((!bank_get_crun_screen_pid(cur->name)) &&
              (!active_find(cur->type, cur->name, cur->num)))
            from_waiting_to_active(cur);
          break;
        default:
          KOUT("ERROR %d", cur->type);
        }
      }
    cur = next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_monitor_active_launcher(void)
{
  int pid, result = 0;
  t_pid_wait *cur = g_head_pid_active;
  while(cur)
    {
    if ((cur->pid == 0) &&
        (cur->release_forbiden1 == 1) &&
        (cur->release_forbiden2 == 1))
      {
      switch (cur->type)
        {
        case type_pid_spicy_gtk:
          result = bank_get_spicy_gtk_pid(cur->name);
          break;
        case type_pid_xephyr_frame:
          result = bank_get_xephyr_frame_pid(cur->name);
          break;
        case type_pid_xephyr_session:
          result = bank_get_xephyr_session_pid(cur->name);
          break;
        case type_pid_wireshark:
          result = bank_get_wireshark_pid(cur->name, cur->num);
          break;
        case type_pid_dtach:
          result = bank_get_dtach_pid(cur->name);
          break;
        case type_pid_crun_screen:
          result = bank_get_crun_screen_pid(cur->name);
          break;
        default:
          KOUT("ERROR %d", cur->type);
        }
      if (result)
        {
        cur->missed_run += 1;
        if (cur->missed_run > 100)
          {
          KERR("ERROR %s %s %d %d", cur->ascii_type, cur->name,
                                    cur->num, result);
          cur->release_forbiden1 = 0;
          cur->release_forbiden2 = 0;
          }
        }
      else
        {
        if (cur->type == type_pid_xephyr_session)
          pid = launch_new_pid_xephyr_session(cur);
        else
          pid = launch_new_pid(cur);
        if (!pid)
          KOUT("ERROR %s %s %d", cur->ascii_type, cur->name, cur->num);
        action_set_pid(cur, pid, 0);
        }
      }
    cur = cur->next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_monitor_active_destroy(void)
{
  t_pid_wait *next, *cur = g_head_pid_active;
  while(cur)
    {
    next = cur->next;
    if ((cur->release_forbiden1 == 0) &&
        (cur->release_forbiden2 == 0))
      free_pid(cur);
    cur = next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_monitor(void *data)
{
  static int count = 0;
  if (timer_monitor_waiting_kill_previous())
    {
    count += 1;
    if (count > 10)
      KERR("ERROR");
    }
  else  
    {
    count = 0;
    timer_monitor_waiting_transfer_to_active();
    timer_monitor_active_launcher();
    timer_monitor_active_destroy();
    }
  clownix_timeout_add(50, timer_monitor, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void pidwait_kill_all_active(void)
{
  t_pid_wait *cur = g_head_pid_active;
  while(cur)
    {
    if (cur->pid)
      kill(cur->pid, SIGKILL);
    cur = cur->next;
    }
}
/*---------------------------------------------------------------------------*/



/*****************************************************************************/
t_pid_wait *pidwait_find(int type, char *name, int num)
{
  t_pid_wait *cur = g_head_pid_waiting;
  while(cur)
    {
    if ((cur->type == type) &&
        (!strcmp(name, cur->name)) &&
        (num == cur->num))
      break;
    cur = cur->next;
    }
  return cur;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void pidwait_alloc(int type, char *name, int num, char *ident, char **argv)
{
  char ascii_display[MAX_NAME_LEN];
  t_pid_wait *cur = pidwait_find(type, name, num);
  int display;
  if (cur)
    KERR("ERROR %d %s %d", type, name, num);
  else
    {
    if (type == type_pid_xephyr_frame)
      {
      display = alloc_display_pool();
      if (display == 0)
        KERR("ERROR %d %s %d", type, name, num);
      else
        {
        memset(ascii_display, 0, MAX_NAME_LEN);
        snprintf(ascii_display, MAX_NAME_LEN - 1, ":%d", display);
        cur = alloc_pid(type, name, num, ident, argv);
        cur->display = display;
        clownix_free(cur->argv[1], __FUNCTION__);
        cur->argv[1] = alloc_argv(ascii_display);
        strncpy(cur->ascii_display, ascii_display, MAX_NAME_LEN-1);
        cur->release_forbiden1 = 1;
        cur->release_forbiden2 = 1;
        }
      }
    else if (type == type_pid_xephyr_session)
      {
      cur = active_find(type_pid_xephyr_frame, name, num);
      if (cur == NULL)
        KERR("ERROR %d %s %d", type, name, num);
      else
        {
        memset(ascii_display, 0, MAX_NAME_LEN);
        strncpy(ascii_display, cur->ascii_display, MAX_NAME_LEN-1);
        cur = alloc_pid(type, name, num, ident, argv);
        strncpy(cur->ascii_display, ascii_display, MAX_NAME_LEN-1);
        cur->release_forbiden1 = 1;
        cur->release_forbiden2 = 1;
        } 
      }
    else
      {
      cur = alloc_pid(type, name, num, ident, argv);
      cur->release_forbiden1 = 1;
      cur->release_forbiden2 = 1;
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void pidwait_init(void)
{
  init_display_pool();
  g_head_pid_waiting = NULL;
  g_head_pid_active = NULL;
  clownix_timeout_add(20, timer_monitor, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/
