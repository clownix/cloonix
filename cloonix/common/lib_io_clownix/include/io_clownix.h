/*****************************************************************************/
/*    Copyright (C) 2006-2019 cloonix@cloonix.net License AGPL-3             */
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
#ifndef _IO_CLOWNIX_H
#define _IO_CLOWNIX_H

#include <stdio.h>
#include <asm/types.h>
#include <syslog.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "ioc_top.h"


int i_am_a_clone(void);
int i_am_a_clone_no_kerr(void);
char *get_cloonix_name(void);
void set_cloonix_name(char *name);
int get_fd_not_to_close(void);
void set_fd_not_to_close(int fd);
/*---------------------------------------------------------------------------*/



#define IO_MAX_BUF_LEN 50000
#define MAX_STR_LEN 250

#define MAX_SIZE_BIGGEST_MSG 10000000
#define MAX_MALLOC_TYPES 30




int is_nonblock(int llid);


void real_time_close_fd(void);
int clownix_system (char * commande);


int get_waked_in_with_cidx(int cidx);
int get_waked_out_with_cidx(int cidx);
int get_waked_err_with_cidx(int cidx);
int get_fd_with_cidx(int cidx);
int get_fd_with_llid(int llid);
int get_in_bytes_with_cidx(int cidx);
int get_out_bytes_with_cidx(int cidx);

void channel_rx_local_flow_ctrl(void *ptr, int llid, int stop);
void channel_tx_local_flow_ctrl(void *ptr, int llid, int stop);
/*---------------------------------------------------------------------------*/

char *get_bigbuf(void);

/****************************************************************************/
typedef void (*t_fct_job)(int ref_id, void *data);
void job_for_select_init(void);
void *job_for_select_alloc(int ref_id, int *llid1, int *llid2);
void job_for_select_request(void *jfs_hand, t_fct_job cb, void *data);
void job_for_select_free(void *jfs_hand, int *llid1, int *llid2);
void job_for_select_close_fd(void);

/****************************************************************************/
typedef void (*t_fct_async_timeout)(int unique_count);
typedef void (*t_fct_timeout)(void *data);
/*--------------------------------------------------------------------------*/
void clownix_timeout_add(int nb_beats, t_fct_timeout cb, void *data,
                         long long *abs_beat, int *ref);
int clownix_timeout_exists(long long abs_beat, int ref);
void *clownix_timeout_del(long long abs_beat, int ref,
                          const char *file, int line);
void clownix_timeout_del_all(void);
void clownix_timer_beat(void);
int  clownix_get_nb_timeouts(void);
void clownix_timer_init(void);
/*--------------------------------------------------------------------------*/
void clownix_get_current_cpu(int *long_cpu, int *medium_cpu, int *short_cpu);
int  clownix_get_sum_bogomips(void);
/*--------------------------------------------------------------------------*/
int ip_string_to_int (int *inet_addr, char *ip_string);
void int_to_ip_string (int addr, char *ip_string);
int get_nb_mask_ip( char *ip_string);
/*---------------------------------------------------------------------------*/
typedef void (*t_msg_usec_rx_cb)(int llid, long long usec, int len, char *str_rx); 
typedef void (*t_msg_rx_cb)(int llid, int len, char *str_rx); 
typedef void (*t_heartbeat_cb)(int delta);
typedef void  (*t_llid_tx)(int llid, int len, char *buf);

typedef int  (*t_fd_event_glib)(int llid, int fd, int *data_fd);

/*---------------------------------------------------------------------------*/
int msg_mngt_get_epfd(void);

void msg_mngt_init (char *name, int max_len_per_read);
int  msg_exist_channel(int llid);
void msg_delete_channel(int llid);

void check_is_clownix_malloc(void *ptr, int len, const char *caller_ident);

void string_tx(int llid, int len, char *tx);
void string_tx_now(int llid, int len, char *tx);
void watch_tx(int llid, int len, char *tx);

int  string_client_unix(char *pname, t_fd_error err_cb, 
                        t_msg_rx_cb rx_cb, char *little_name);

int  string_client_inet(__u32 ip, __u16 port, 
                        t_fd_error err_cb, 
                         t_msg_rx_cb rx_cb, char *little_name);
int string_server_unix(char *pname,t_fd_connect connect_cb,char *little_name); 
int  string_server_inet(__u16 port,t_fd_connect connect_cb,char *little_name);
int string_server_unix(char *pname,t_fd_connect connect_cb,char *little_name);
int  string_server_inet (__u16 port,t_fd_connect connect_cb,char *little_name);
void msg_mngt_set_callbacks (int llid, t_fd_error err_cb, 
                             t_msg_rx_cb rx_cb);
int msg_watch_fd(int fd, t_fd_event rx_data,
                 t_fd_error err, char *little_name);
struct timeval *channel_get_current_time(void);
void msg_mngt_heartbeat_init(t_heartbeat_cb heartbeat);
void msg_mngt_heartbeat_ms_set (int heartbeat_ms);
unsigned long msg_get_tx_peak_queue_len(int llid);
void msg_mngt_loop(void);
void msg_mngt_loop_once(void);
unsigned long long get_channel_total_recv(void);
unsigned long long get_channel_total_send(void);
int get_channel_cur_recv(void);
int get_channel_cur_send(void);
int  get_clownix_channels_nb(void);
int  get_clownix_channels_max(void);
void *clownix_malloc(int size, int id);
void differed_clownix_free(void *data);
char *clownix_strdup(char *str, int id);
unsigned long *get_clownix_malloc_nb(void);
void clownix_free(void *ptr, const char *caller_ident);
int  get_counter_select_speed(void);
int  channel_get_llid_with_cidx(int cidx);
void nonblock_fd(int fd);
void nodelay_fd(int fd);
void nonnonblock_fd(int fd);

unsigned long channel_get_tx_queue_len(int llid);

void ptr_doorways_client_tx(void *ptr, int llid, int len, char *buf);


int msg_mngt_get_tx_queue_len(int llid);

#endif /* _IO_CLOWNIX_H */

