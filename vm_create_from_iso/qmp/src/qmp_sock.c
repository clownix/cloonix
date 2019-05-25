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

static int g_time_count;
static int g_qmp_fd;
static int g_type_distro;
static int g_lang_distro;


/****************************************************************************/
void qmp_send(char *msg)
{
  int len;
  len = write(g_qmp_fd, msg, strlen(msg));
  if (len != strlen(msg))
    KOUT("%s", msg);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static time_t my_second(void)
{
  static time_t offset = 0; 
  struct timeval tv;
  gettimeofday(&tv, NULL);
  if (offset == 0)
    offset = tv.tv_sec;
  return (tv.tv_sec - offset);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void process_qmp_rx(char *qmp_string)
{
  int type_rx_msg = msg_rx_type_none;
  if ((strstr(qmp_string, "QMP")) && (strstr(qmp_string, "capabilities")))
    type_rx_msg = msg_rx_type_capa;
  else if (strstr(qmp_string, "\"running\": true"))
    type_rx_msg = msg_rx_type_running;
  else if (strstr(qmp_string, "{\"return\": {}}"))
    type_rx_msg = msg_rx_type_return;
  else if (strstr(qmp_string, "SHUTDOWN")) 
    type_rx_msg = msg_rx_type_shutdown;
  else if (strstr(qmp_string, "DEVICE_TRAY_MOVED")) 
    {
    if (strstr(qmp_string, "ide1-cd0")) 
      type_rx_msg = msg_rx_type_tray_cd;
    else
      type_rx_msg = msg_rx_type_tray_nocd;
    }
  else if (strstr(qmp_string, "STOP"))
    type_rx_msg = msg_rx_type_stop;
  else if (strstr(qmp_string, "RTC_CHANGE"))
    type_rx_msg = msg_rx_type_rtc_change;
  else if (strstr(qmp_string, "\"running\": false"))
    type_rx_msg = msg_rx_type_not_running;
  else
    printf("NOT KNOWN: %s\n", qmp_string);
  if ((type_rx_msg != msg_rx_type_stop) && 
      (type_rx_msg != msg_rx_type_tray_nocd))
    {
    oper_automate_rx_msg(type_rx_msg, qmp_string);
    }
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static void process_fd_event(int fd)

{
  static char rx[MAX_MSG_LEN];
  int len;
  memset(rx, 0, MAX_MSG_LEN);
  len = read(fd, rx, MAX_MSG_LEN-1);
  if (len == 0)
    {
    exit(0);
    }
  else if (len < 0)
    {
    if ((errno != EAGAIN) && (errno != EINTR))
      KOUT("%d", errno);
    }
  else
    {
    process_qmp_rx(rx);
    }
};
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void select_wait_and_switch(void)
{
  fd_set infd;
  int result, fd_max;
  time_t cur_sec;
  static struct timeval timeout;
  FD_ZERO(&infd);
  FD_SET(g_qmp_fd, &infd);
  fd_max = g_qmp_fd;
  result = select(fd_max + 1, &infd, NULL, NULL, &timeout);
  if ( result < 0 )
    KOUT(" ");
  else if (result == 0) 
    {
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    }
  else 
    {
    if (FD_ISSET(g_qmp_fd, &infd))
      process_fd_event(g_qmp_fd);
    }
  cur_sec = my_second();
  if (cur_sec != g_time_count)
    {
    g_time_count = cur_sec;
    oper_automate_sheduling();
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void usage(char *name)
{
  printf("\n%s <distro> <lang>", name);
  printf("\n<lang> can be \"us\" or \"fr\", it is for the keyboard conf");
  printf("\n <distro> can be:");
  printf("\n                  buster");
  printf("\n                  stretch");
  printf("\n                  opensuse");
  printf("\n                  ubuntu");
  printf("\n                  centos8");
  printf("\n                  redhat8");
  printf("\n                  fedora30");
  printf("\n");
  exit(1);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int get_type(void)
{
  return g_type_distro;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int get_lang(void)
{
  return g_lang_distro;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int main(int argc, char *argv[])
{
  int count = 0;
  g_time_count = 0;
  g_lang_distro = 0;
  if (argc != 3)
    usage(argv[0]);
  if (!strcmp(argv[1], "buster"))
    g_type_distro = type_buster;
  else if (!strcmp(argv[1], "stretch"))
    g_type_distro = type_stretch;
  else if (!strcmp(argv[1], "opensuse"))
    g_type_distro = type_opensuse;
  else if (!strcmp(argv[1], "ubuntu"))
    g_type_distro = type_ubuntu;
  else if (!strcmp(argv[1], "centos8"))
    g_type_distro = type_centos8;
  else if (!strcmp(argv[1], "redhat8"))
    g_type_distro = type_redhat8;
  else if (!strcmp(argv[1], "fedora30"))
    g_type_distro = type_fedora30;
  else
    usage(argv[0]);
  if (!strcmp(argv[2], "fr"))
    g_lang_distro = 1;
  else if (!strcmp(argv[2], "us"))
    g_lang_distro = 0;
  else
    usage(argv[0]);
  oper_automate_init();
  while (util_client_socket_unix(QMP_SOCK, &g_qmp_fd))
    {
    sleep(2);
    printf("\nRE-TRYING\n");
    count++;
    if (count>10)
      KOUT(" ");
    }
  for (;;)
    select_wait_and_switch();
  return 0;
}
/*--------------------------------------------------------------------------*/
