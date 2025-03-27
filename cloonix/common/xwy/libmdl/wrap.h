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
int wrap_util_proxy_client_socket_unix(char *pname, int *fd);
int wrap_socket_connect_unix_connect_ok(char *path);
int wrap_socket_connect_inet(unsigned long ip, int port,
                             int fd_type, const char *fct);

void *wrap_malloc(size_t size);
void wrap_free(void *ptr, int line);
ssize_t wrap_write_one(int s, const void *buf, long unsigned int len);
ssize_t wrap_write_cli(int s, const void *buf, long unsigned int len);
ssize_t wrap_write_srv(int s, const void *buf, long unsigned int len);
ssize_t wrap_write_x11_wrwait_x11(int s, const void *buf, long unsigned int len);
ssize_t wrap_write_x11_wrwait_soc(int s, const void *buf, long unsigned int len);
ssize_t wrap_write_x11_x11(int s, const void *buf, long unsigned int len);
ssize_t wrap_write_x11_soc(int s, const void *buf, long unsigned int len);
ssize_t wrap_write_pty(int s, const void *buf, long unsigned int len);
ssize_t wrap_write_scp(int s, const void *buf, long unsigned int len);
ssize_t wrap_write_dialog_thread(int s, const void *buf, long unsigned int len);
ssize_t wrap_write_cloon(int s, const void *buf, long unsigned int len);
ssize_t wrap_write_kout(int s, const void *buf, long unsigned int len);
ssize_t wrap_read_cli(int s, void *buf, size_t len);
ssize_t wrap_read_srv(int s, void *buf, size_t len);
ssize_t wrap_read_x11_rd_x11(int s, void *buf, size_t len);
ssize_t wrap_read_x11_rd_soc(int s, void *buf, size_t len);
ssize_t wrap_read_pty(int s, void *buf, size_t len);
ssize_t wrap_read_scp(int s, void *buf, size_t len);
ssize_t wrap_read_zero(int s, void *buf, size_t len);
ssize_t wrap_read_sig(int s, void *buf, size_t len);
ssize_t wrap_read_dialog_thread(int s, void *buf, size_t len);
ssize_t wrap_read_cloon(int s, void *buf, size_t len);
ssize_t wrap_read_kout(int s, void *buf, size_t len);
/*--------------------------------------------------------------------------*/
int wrap_accept(int fd_listen, int fd_type, const char *fct);
int wrap_close(int fd, const char *fct);
int wrap_socket_listen_unix(char *path, int fd_type, const char *fct);
int wrap_socket_connect_unix(char *path, int fd_type, const char *fct);
int wrap_epoll_create(int fd_type, const char *fct);
struct epoll_event *wrap_epoll_event_alloc(int epfd, int fd, int id);
void wrap_epoll_event_free(int epfd, struct epoll_event *epev);
void wrap_nonblock(int fd);
void wrap_nonnonblock(int fd);
int wrap_pipe(int pipefd[2], int fd_type, const char *fct);
int wrap_socketpair(int sockfd[2], int fd_type, const char *fct);
int wrap_openpty(int *amaster, int *aslave, char *name, int fd_type);
void wrap_pty_make_controlling_tty(int ttyfd, char *ttyname);
void wrap_mutex_lock(void);
void wrap_mutex_unlock(void);
void wrap_init(void);
void wrap_mkdir(char *dst_dir);
int wrap_touch(char *path);
int wrap_chmod_666(char *path);
int wrap_file_exists(char *path);
/*--------------------------------------------------------------------------*/
