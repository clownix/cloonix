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
// #define DEBUG



int debug_get_trunc_usec(void);

char *debug_get_fd_type_txt(int from);
char *debug_get_thread_type_txt(int type);

#ifdef DEBUG

#define DEBUG_LOG_FILE_SRV "/tmp/xwy_log_srv"
#define DEBUG_LOG_FILE_CLI "/tmp/xwy_log_cli"


char *debug_get_evt_type_txt(int type);
char *debug_pty_txt(t_msg *msg);
int debug_get_ioctl_queue_len(int fd, int type, int *used);
void debug_dump_enqueue(int fd_dst, t_msg *msg, int all, int th);
void debug_dump_enqueue_levels(int fd_dst, int slots, int bytes); 
void debug_wrap_read_write(int is_read, int fd, int len, 
                           int from, const void *buf);
void debug_dump_rxmsg(t_msg *msg);
void debug_dump_thread(int x11_fd, int sock_fd_ass, 
                       int srv_idx, int cli_idx);
void debug_evt(const char * format, ...);
void debug_init(int is_srv);


#define DEBUG_INIT(is_srv)                 \
   do {                                    \
    debug_init(is_srv);                    \
    } while (0)                            \

#define DEBUG_EVT(format, a...)            \
   do {                                    \
      debug_evt(format, ## a);             \
    } while (0)

#define DEBUG_IOCTL_TX_QUEUE(fd,type,used)      \
   do {                                         \
      debug_get_ioctl_queue_len(fd,type,&used); \
    } while (0)

#define DEBUG_WRAP_READ_WRITE(is_read, fd, len, from, buf) \
   do {                                                    \
      debug_wrap_read_write(is_read, fd, len, from, buf);  \
    } while (0)

#define DEBUG_DUMP_ENQUEUE(fd_dst, msg, all, th) \
   do {                                          \
      debug_dump_enqueue(fd_dst, msg, all, th);  \
    } while (0)

#define DEBUG_DUMP_ENQUEUE_LEVELS(fd_dst, slots, bytes) \
   do {                                                 \
      debug_dump_enqueue_levels(fd_dst, slots, bytes);  \
    } while (0)

#define DEBUG_DUMP_RXMSG(msg)        \
   do {                              \
      debug_dump_rxmsg(msg);         \
    } while (0)

#define DEBUG_DUMP_THREAD(sock_fd_ass, x11_fd, srv_idx, cli_idx) \
   do {                                                          \
      debug_dump_thread(sock_fd_ass, x11_fd, srv_idx, cli_idx);  \
    } while (0)


#else

#define DEBUG_INIT(is_srv)
#define DEBUG_EVT(format, a...)
#define DEBUG_IOCTL_TX_QUEUE(fd,type,used)
#define DEBUG_WRAP_READ_WRITE(is_read, fd, len, from, buf) 
#define DEBUG_DUMP_ENQUEUE(fd_dst, msg, all, th)
#define DEBUG_DUMP_ENQUEUE_LEVELS(fd_dst, slots, bytes)
#define DEBUG_DUMP_RXMSG(msg)
#define DEBUG_DUMP_THREAD(x11_fd, sock_fd_ass, srv_idx, cli_idx)
#endif

/*--------------------------------------------------------------------------*/

