/****************************************************************************/
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
static void lines_csr1000v(void)
{
  strcpy(g_lines[g_i++], "enable");
  strcpy(g_lines[g_i++], "show platform software vnic-if interface-mapping");
  strcpy(g_lines[g_i++], "clear platform software vnic-if nvtable");
  strcpy(g_lines[g_i++], "conf t");
  strcpy(g_lines[g_i++], "username root privilege 15 secret 0 root");
  strcpy(g_lines[g_i++], "line vty 0 4");
  strcpy(g_lines[g_i++], "login local");
  strcpy(g_lines[g_i++], "exit");
  strcpy(g_lines[g_i++], "hostname cisco");
  strcpy(g_lines[g_i++], "ip domain name demo.net");
  strcpy(g_lines[g_i++], "crypto key generate rsa");
  strcpy(g_lines[g_i++], "1024");
  strcpy(g_lines[g_i++], "ip ssh version 2");
  strcpy(g_lines[g_i++], "ip ssh rsa keypair-name cisco.demo.net");
  strcpy(g_lines[g_i++], "ip scp server enable");
  strcpy(g_lines[g_i++], "interface GigabitEthernet1");
  strcpy(g_lines[g_i++], "ip address dhcp");
  strcpy(g_lines[g_i++], "no shutdown");
  strcpy(g_lines[g_i++], "exit");
  strcpy(g_lines[g_i++], "ip ssh pubkey-chain");
  strcpy(g_lines[g_i++], "username root");
  strcpy(g_lines[g_i++], "key-string");
  strcpy(g_lines[g_i++], "ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQDGR6miK5hkP8eelcvitoRqsmLsSwwWSvk2gL9y/1IXwI/B4gUkzrqp7QJR30nD3s6/26VMs+ehRBgVuNv9ps5");
  strcpy(g_lines[g_i++], "rt62KNHHe+y9qGBBKNik4FcKeCR3OigC9bKYtiwGxlCjRA21d1qA/SWa2Ll4Rf7+4K//4hYYXHu0FPxQXyznwOu4NGmZ3DMaCH9+vYXA6SXGNA7vWCQjN39+lBd6+yP3cKxQfdTNOu9OfwGt+soAE");
  strcpy(g_lines[g_i++], "AB4fZB3XLFMdE9x/2zITMwwDxi3AI887y+qhsy3W0fpiuX9zsOTGoJ+1aM9XHSYwn9IcE5A/g0xWv58TpHRjVa5ZXCiOzzq4sC04UGXmL5Wb cisco@debian");
  strcpy(g_lines[g_i++], "exit");
  strcpy(g_lines[g_i++], "exit");
  strcpy(g_lines[g_i++], "exit");
  strcpy(g_lines[g_i++], "exit");
  strcpy(g_lines[g_i++], "write");
  strcpy(g_lines[g_i++], "reload");
  strcpy(g_lines[g_i++], "");
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
  int type_distro = get_type();
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
        {
        if (type_distro == type_csr1000v)
          printf("waiting 120 sec\n");
        else
          printf("waiting 20 sec\n");
        }
      if ((count % 5) == 0)
        printf("%d\n", count);
      count++;
      if (type_distro != type_csr1000v)
        KOUT(" ");
      if (count == 120)
        {
        send_char_qmp('n');
        send_char_qmp('o');
        qmp_send(QMP_SENDKEY_RET);
        }
      else if (count == 130)
        {
        send_char_qmp('y');
        send_char_qmp('e');
        send_char_qmp('s');
        qmp_send(QMP_SENDKEY_RET);
        printf("waiting 80 more sec\n");
        }
      else if (count == 210)
        {
        qmp_send(QMP_SENDKEY_RET);
        set_g_state(state_waiting_return);
        }
      break;

    case state_ctrl_alt_f3_done:
      count_f3++;
      if (count_f3 == 4)
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
  int type_distro = get_type();
  g_state = state_dialog_idle;
  g_count_keys = 0;
  g_count_lines = 0;
  g_i = 0;
  memset(g_lines, 0, MAX_LINES * MAX_LINES_LEN);
  if (type_distro != type_csr1000v)
    KOUT("%d", type_distro); 
  lines_csr1000v();
  g_max_lines = g_i;
}
/*--------------------------------------------------------------------------*/
