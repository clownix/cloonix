/*****************************************************************************/
/*    Copyright (C) 2006-2020 clownix@clownix.net License AGPL-3             */
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
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <asm/types.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <linux/sockios.h>
#include <sys/time.h>


#include "commun.h"

static int g_state;

enum {
  state_dialog_idle = 0,
  state_capa_sending,
  state_capa_sent,
  state_capa_done,
  state_ctrl_alt_f3_sent,
  state_ctrl_alt_f3_done,
  state_waiting_return,
  state_ready,
  state_end_oper_setup,
  state_quit_sending,
  state_quit_sent,
};

static int g_count_keys;
static int g_count_lines;
static int g_max_lines;
static int g_i;

#define MAX_LINES 50
#define MAX_LINES_LEN 1000
static char g_lines[MAX_LINES][MAX_LINES_LEN];



/****************************************************************************/
static void init_lines_start(void)
{
  strcpy(g_lines[g_i++], "root");
  strcpy(g_lines[g_i++], "root");

  strcpy(g_lines[g_i++], "echo -e \"[Service]\\n"
                         "ExecStart=-/sbin/agetty -a root hvc0\\n"
                         "Type=idle\\n"
                         "Restart=always\\n"
                         "RestartSec=0\" > "
                         "/etc/systemd/system/serial-getty@hvc0.service");

  strcpy(g_lines[g_i++], "mkdir -p /etc/systemd/system/getty.target.wants");

  strcpy(g_lines[g_i++], "ln -s /etc/systemd/system/serial-getty@hvc0.service "
                         "/etc/systemd/system/getty.target.wants/"
                         "serial-getty@hvc0.service");

  strcpy(g_lines[g_i++], "cat /etc/systemd/system/getty.target.wants/"
                         "serial-getty@hvc0.service");

  strcpy(g_lines[g_i++], "echo \"GRUB_CMDLINE_LINUX_DEFAULT=\\\"noquiet "
                         "console=ttyS0 console=tty1 earlyprintk=serial "
                         "net.ifnames=0\\\"\" >> /etc/default/grub");

  strcpy(g_lines[g_i++], "grub-mkconfig -o /boot/grub/grub.cfg");

  strcpy(g_lines[g_i++], "sync");

  strcpy(g_lines[g_i++], "shutdown now");

  g_max_lines = g_i;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static char *get_state_ascii(int state, char *ascii_state)
{
  memset(ascii_state, 0, MAX_ASCII_LEN);
  switch (state)
    {

    case state_dialog_idle:
      snprintf(ascii_state, MAX_ASCII_LEN-1, "IDLE");
      break;

    case state_capa_sending:
      snprintf(ascii_state, MAX_ASCII_LEN-1, "CAPA_SENDING");
      break;

    case state_capa_sent:
      snprintf(ascii_state, MAX_ASCII_LEN-1, "CAPA_SENT");
      break;

    case state_capa_done:
      snprintf(ascii_state, MAX_ASCII_LEN-1, "CAPA_DONE");
      break;

    case state_ctrl_alt_f3_sent:
      snprintf(ascii_state, MAX_ASCII_LEN-1, "CTRL_ALT_F3_SENT");
      break;

    case state_ctrl_alt_f3_done:
      snprintf(ascii_state, MAX_ASCII_LEN-1, "CTRL_ALT_F3_DONE");
      break;

    case state_ready:
      snprintf(ascii_state, MAX_ASCII_LEN-1, "READY");
      break;

    case state_waiting_return:
      snprintf(ascii_state, MAX_ASCII_LEN-1, "WAITING_RETURN");
      break;

    case state_end_oper_setup:
      snprintf(ascii_state, MAX_ASCII_LEN-1, "END OPER SETUP");
      break;

    case state_quit_sending:
      snprintf(ascii_state, MAX_ASCII_LEN-1, "QUIT_SENDING");
      break;

    case state_quit_sent:
      snprintf(ascii_state, MAX_ASCII_LEN-1, "QUIT_SENT");
      break;

    default:
      KOUT("%d", state);

    }
  return ascii_state;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void set_g_state(int new_state)
{
  char old_ascii_state[MAX_ASCII_LEN];
  char new_ascii_state[MAX_ASCII_LEN];
  get_state_ascii(g_state, old_ascii_state);
  get_state_ascii(new_state, new_ascii_state);
  if (strcmp(old_ascii_state, new_ascii_state))
    printf("%s to %s\n", old_ascii_state, new_ascii_state);
  g_state = new_state;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void send_char_qmp(char key)
{
  int lang_distro = get_lang();
  char qmp[MAX_MSG_LEN];
  memset(qmp, 0, MAX_MSG_LEN);
  snprintf(qmp, MAX_MSG_LEN-1, QMP_SENDKEY, qmp_keystroke(lang_distro, key));
  qmp_send(qmp);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void oper_automate_rx_msg(int type_rx_msg, char *qmp_string)
{
  char key;
  if (type_rx_msg == msg_rx_type_shutdown)
    set_g_state(state_quit_sending);
  else switch (g_state)
    {
    case state_dialog_idle:
      if (type_rx_msg == msg_rx_type_capa)
        set_g_state(state_capa_sending);
      else
        KERR("%s", qmp_string);
      break;

    case state_capa_sending:
      break;

    case state_capa_sent:
      if (type_rx_msg == msg_rx_type_return)
        set_g_state(state_capa_done);
      else
        KERR("%s", qmp_string);
      break;

    case state_ctrl_alt_f3_sent:
      if (type_rx_msg == msg_rx_type_return)
        set_g_state(state_ctrl_alt_f3_done);
      else
        KERR("%s", qmp_string);
      break;

    case state_ctrl_alt_f3_done:
    case state_capa_done:
    case state_ready:
    case state_end_oper_setup:
      break;

    case state_waiting_return:
      if (type_rx_msg == msg_rx_type_return)
        {
        key = g_lines[g_count_lines][g_count_keys];
        if (!key)
          {
          printf("\n");
          printf("Waiting 6 secs...\n");
          qmp_send(QMP_SENDKEY_RET);
          g_count_lines += 1;
          g_count_keys = 0;
          sleep(6);
          }
        else
          {
          usleep(100000);
          send_char_qmp(key);
          g_count_keys += 1;
          printf("%c", key);
          fflush(stdout);
          }
        if (g_count_lines == g_max_lines)
          set_g_state(state_end_oper_setup);
        else
          set_g_state(state_waiting_return);
        }
      else
        KERR("%s", qmp_string);
      break;

    case state_quit_sending:
      break;

    case state_quit_sent:
      if (type_rx_msg == msg_rx_type_return)
        KOUT(" ");
      else
        KERR("%s", qmp_string);
      break;

    default:
      KOUT(" ");
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void oper_automate_sheduling(void)
{
  static int count = 0;
  static int count_f3 = 0;
  switch (g_state)
    {
    case state_dialog_idle:
      break;

    case state_capa_sending:
      qmp_send(QMP_CAPA);
      set_g_state(state_capa_sent);
      break;


    case state_capa_done:
      if (count == 0)
        printf("waiting 40 sec\n");
      if ((count == 5)||(count == 10)  ||
          (count == 15)||(count == 20) ||
          (count == 25)||(count == 30) ||
          (count == 35)||(count == 40) )
        printf("%d\n", count);
      count++;
      if (count == 40)
        {
        printf("Sending first ctrl-alt-f3\n");
        qmp_send(QMP_CTRL_ALT_F3);
        set_g_state(state_ctrl_alt_f3_sent);
        }
      break;

    case state_ctrl_alt_f3_done:
      if (count_f3 == 0)
        printf("Ctrl-alt-f3 done waiting 15\n");
      count_f3++;
      printf("%d\n", count_f3);
      if (count_f3 == 15)
        {
        qmp_send(QMP_SENDKEY_RET);
        set_g_state(state_waiting_return);
        }
      break;

    case state_ctrl_alt_f3_sent:
    case state_capa_sent:
    case state_ready:
    case state_waiting_return:
      break;

    case state_quit_sending:
      qmp_send(QMP_QUIT);
      set_g_state(state_quit_sent);
      break;

    case state_quit_sent:
    case state_end_oper_setup:
      break;

    default:
      KERR("%d", g_state);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void oper_automate_init(void)
{
  g_state = state_dialog_idle;
  g_count_keys = 0;
  g_count_lines = 0;
  g_i = 0;
  memset(g_lines, 0, MAX_LINES * MAX_LINES_LEN);
  init_lines_start();
}
/*--------------------------------------------------------------------------*/
