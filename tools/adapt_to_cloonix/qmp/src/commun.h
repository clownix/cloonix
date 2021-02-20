/*****************************************************************************/
/*    Copyright (C) 2006-2021 clownix@clownix.net License AGPL-3             */
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
#include <syslog.h>
#undef KERR
#define KERR(format, a...)                              \
 do {                                                   \
    printf("\n%s %s line:%d   " format,        \
    __FILE__,__FUNCTION__,__LINE__, ## a);              \
    syslog(LOG_ERR, "%s %s line:%d   " format, \
    __FILE__,__FUNCTION__,__LINE__, ## a);              \
    } while (0)

#undef KOUT
#define KOUT(format, a...)                               \
 do {                                                    \
    printf("\n%s %s line:%d   " format,         \
    __FILE__,__FUNCTION__,__LINE__, ## a);               \
    syslog(LOG_ERR, "%s %s line:%d   " format, \
    __FILE__,__FUNCTION__,__LINE__, ## a);               \
    exit(-1);                                            \
    } while (0)
/*--------------------------------------------------------------------------*/
enum {
  msg_rx_type_none=0,
  msg_rx_type_capa,
  msg_rx_type_running,
  msg_rx_type_not_running,
  msg_rx_type_return,
  msg_rx_type_stop,
  msg_rx_type_shutdown,
  msg_rx_type_rtc_change,
  msg_rx_type_tray_cd,
  msg_rx_type_tray_nocd,
};

enum {
  type_none = 0, 
  type_buster, 
  type_stretch, 
  type_ubuntu,
  type_centos8,
  type_redhat8,
  type_fedora30,
  type_opensuse,
};

#define MAX_ASCII_LEN 300
#define MAX_MSG_LEN 3000
#define QMP_SOCK "/tmp/qemu_qmp_sock"



#define QMP_COMMANDS "{\"execute\":\"query-commands\"}"
#define QMP_CAPA "{\"execute\":\"qmp_capabilities\"}"
#define QMP_QUIT "{\"execute\":\"quit\"}"
#define QMP_QERY "{\"execute\":\"query-status\"}"

#define QMP_SENDKEY_DOWN "{\"execute\":\"send-key\",\"arguments\":{\"keys\":"\
                         "[{\"type\":\"qcode\",\"data\":\"down\"}]}}" 

#define QMP_SENDKEY_RET "{\"execute\":\"send-key\",\"arguments\":{\"keys\":"\
                        "[{\"type\":\"qcode\",\"data\":\"ret\"}]}}" 

#define QMP_SENDKEY "{\"execute\":\"send-key\",\"arguments\":{\"keys\":%s}}"

#define QMP_CTRL_ALT_F3 "{\"execute\":\"send-key\",\"arguments\":{\"keys\":"\
                        "["\
                          "{\"type\":\"qcode\",\"data\":\"ctrl\"},"\
                          "{\"type\":\"qcode\",\"data\":\"alt\"},"\
                          "{\"type\":\"qcode\",\"data\":\"f3\"}"\
                        "]}}"


#define MAX_KEYSTROKE 100

#define SINGLE_NUM "[{\"type\":\"number\", \"data\":%d}]"

#define DOUBLE_NUM "[{\"type\":\"number\",\"data\":%d},"\
                    "{\"type\":\"number\",\"data\":%d}]"


char *qmp_keystroke(int fr, char c);

int util_client_socket_unix(char *pname, int *fd);

void qmp_send(char *msg);

void oper_automate_rx_msg(int type_rx_msg, char *qmp_string);
void oper_automate_sheduling(void);
void oper_automate_init(void);
int get_lang(void);





