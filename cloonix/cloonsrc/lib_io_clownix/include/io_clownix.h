/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef _IO_CLOWNIX_H
#define _IO_CLOWNIX_H

#include <stdio.h>
#include <asm/types.h>
#include <syslog.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "glob_common.h"

int i_am_a_clone(void);
void set_cloonix_name(char *name);

#define IO_MAX_BUF_LEN 50000

#define MAX_SIZE_BIGGEST_MSG 10000000
#define MAX_MALLOC_TYPES 64

enum {
     kind_seqtap_data = 5,
     kind_seqtap_sig_hello,
     kind_seqtap_sig_ready,
     kind_seqtap_error
     };
void fct_seqtap_tx(int kind, uint8_t *tx, uint16_t seqtap,
                   int len, uint8_t *buf);
int fct_seqtap_rx_udp(int len, int fd, uint8_t *rx, uint16_t *seq, int *buf_len, uint8_t **buf);
int fct_seqtap_rx(int fd, uint8_t *rx, uint16_t *seq, int *buf_len, uint8_t **buf);


int get_fd_with_cidx(int cidx);
int get_fd_with_llid(int llid);

void channel_rx_local_flow_ctrl(int llid, int stop);
void channel_tx_local_flow_ctrl(int llid, int stop);

char *get_bigbuf(void);

typedef void (*t_fct_job)(int ref_id, void *data);
void job_for_select_init(void);
void *job_for_select_alloc(int ref_id, int *llid1, int *llid2);
int  job_for_select_request(void *jfs_hand, t_fct_job cb, void *data);
void job_for_select_free(void *jfs_hand, int *llid1, int *llid2);
void job_for_select_close_fd(void);

typedef void (*t_fct_async_timeout)(int unique_count);
typedef void (*t_fct_timeout)(void *data);
void clownix_timer_report_top(char *txt, int txt_len);
void msg_layer_report_queues(char *txt, int txt_len);
void clownix_timeout_add(int nb_beats, t_fct_timeout cb, void *data,
                         long long *abs_beat, int *ref);
void *clownix_timeout_del(long long abs_beat, int ref,
                          const char *file, int line);
void clownix_timeout_del_all(void);
void clownix_timer_beat(void);
void clownix_timer_init(void);
int ip_string_to_int (uint32_t *inet_addr, char *ip_string);
void int_to_ip_string (int addr, char *ip_string);
typedef void (*t_msg_usec_rx_cb)(int llid, long long usec, int len, char *str_rx); 
typedef void (*t_msg_rx_cb)(int llid, int len, char *str_rx); 
typedef void (*t_heartbeat_cb)(int delta);
typedef void  (*t_llid_tx)(int llid, int len, char *buf);

typedef int  (*t_fd_event_glib)(int llid, int fd, int *data_fd);

void msg_mngt_init (char *name, int max_len_per_read);
int  msg_exist_channel(int llid);
void msg_delete_channel(int llid);

void string_tx(int llid, int len, char *tx);
void string_tx_now(int llid, int len, char *tx);
void watch_tx(int llid, int len, char *tx);

int  string_client_unix(char *pname, t_fd_error err_cb, 
                        t_msg_rx_cb rx_cb, char *little_name);

void proxy_traf_tcp_tx(int llid, int len, char *str_tx);
int  proxy_traf_tcp_server(uint16_t port, t_fd_connect connect_cb); 

void proxy_traf_unix_tx(int llid, int len, char *str_tx);
int  proxy_traf_unix_client(char *pname, t_fd_error err_cb, t_msg_rx_cb rx_cb);

void proxy_sig_tx(int llid, int len, char *str_tx);
int  proxy_sig_client(char *pname, t_fd_error err_cb, t_msg_rx_cb rx_cb);
int  proxy_sig_server(char *pname, t_fd_connect connect_cb); 

int string_server_unix(char *pname,t_fd_connect connect_cb,char *little_name); 
int string_server_inet(uint16_t port,t_fd_connect connect_cb,char *little_name);
void msg_mngt_set_callbacks (int llid, t_fd_error err_cb, 
                             t_msg_rx_cb rx_cb);
int msg_watch_fd(int fd, t_fd_event rx_data,
                 t_fd_error err, char *little_name);
void msg_mngt_heartbeat_init(t_heartbeat_cb heartbeat);
void msg_mngt_heartbeat_ms_set (int heartbeat_ms);
void msg_mngt_loop(void);
int  get_clownix_channels_nb(void);
void *clownix_malloc(int size, int id);
unsigned long *get_clownix_malloc_nb(void);
void clownix_free(void *ptr, const char *caller_ident);
int  channel_get_llid_with_cidx(int cidx);
void lio_clean_all_llid(void);
char *lio_linear(char *argv[]);
void nonblock_fd(int fd);
void nodelay_fd(int fd);
void nonnonblock_fd(int fd);

unsigned long channel_get_tx_queue_len(int llid);

void doors_tx_switch_val_c2c(int llid, int len, char *buf);

int msg_mngt_get_tx_queue_len(int llid);
void msg_relay_flow_ctrl(int rx_llid, int tx_llid, int *paused);
int msg_tx_queue_high(int llid);
int msg_tx_queue_low(int llid);

char *get_cloonix_name(void);
/* ctrl_link.c: the control link of an ovs satellite to the main server.   */
#define CTRL_LINK_GRACE_MS 5000
void ctrl_link_init(char *net, char *kind, char *name, int type_hop,
                    char *ctrl_path);
void ctrl_link_add_path(char *path);
void ctrl_link_keep_path(char *path);
void ctrl_link_add_child(int pid);
void ctrl_link_pid_req(int llid, int tid, char *name, int num);
void ctrl_link_stop(void);
int  ctrl_link_llid(void);
#endif

