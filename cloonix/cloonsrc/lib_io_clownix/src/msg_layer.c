/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#define _GNU_SOURCE
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <asm/types.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/syscall.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "io_clownix.h"
#include "channel.h"
#include "util_sock.h"
#include "msg_layer.h"
#include "chunk.h"
#include "doorways_sock.h"

#define MAX_TOT_LEN_QSIG 500000000
#define MAX_TOT_LEN_QDAT 1000000000
static t_data_channel dchan[CLOWNIX_MAX_CHANNELS];
static char *first_rx_buf;
static int first_rx_buf_max;
static long long  peak_queue_len[MAX_SELECT_CHANNELS];
static char g_bigsndbuf[MAX_SIZE_BIGGEST_MSG];
static unsigned long clownix_malloc_nb[MAX_MALLOC_TYPES];
static char g_cloonix_name[MAX_NAME_LEN];
static int g_cloonix_name_set = 0;

void fct_seqtap_tx(int kind, uint8_t *tx, uint16_t seqtap,
                   int len, uint8_t *buf)
{
  if (len > HEADER_TAP_MSG + TRAF_TAP_BUF_LEN + END_FRAME_ADDED_CHECK_LEN)
    KOUT("ERROR SEND  %d", len);
  tx[0] = 0xCA;
  tx[1] = 0xFE;
  if (kind == kind_seqtap_data)
    {
    tx[2] = 0xDE;
    tx[3] = 0xCA;
    }
  else if (kind == kind_seqtap_sig_hello)
    {
    tx[2] = 0xBE;
    tx[3] = 0xEF;
    }
  else if (kind == kind_seqtap_sig_ready)
    {
    tx[2] = 0xAA;
    tx[3] = 0xBB;
    }
  else
    KOUT("ERROR %d", kind);
  tx[4] = (len & 0xFF00) >> 8;
  tx[5] =  len & 0xFF;
  tx[6] = (seqtap & 0xFF00) >> 8;
  tx[7] =  seqtap & 0xFF;
  if (buf)
    memcpy(tx + HEADER_TAP_MSG, buf, len);
}

/* Header of a seqtap message: its kind and payload length, else the error.
   Silent: the caller reports, a port open to anybody needs a rate limit. */
static int seqtap_hdr(uint8_t *rx, int *len)
{
  int result = kind_seqtap_error;
  *len = ((rx[4] & 0xFF) << 8) + (rx[5] & 0xFF);
  if ((rx[0] == 0xCA) && (rx[1] == 0xFE) &&
      (*len <= TRAF_TAP_BUF_LEN + END_FRAME_ADDED_CHECK_LEN))
    {
    if ((rx[2] == 0xDE) && (rx[3] == 0xCA))
      result = kind_seqtap_data;
    else if ((rx[2] == 0xBE) && (rx[3] == 0xEF))
      result = kind_seqtap_sig_hello;
    else if ((rx[2] == 0xAA) && (rx[3] == 0xBB))
      result = kind_seqtap_sig_ready;
    }
  return result;
}

/* One datagram, whole in rx: any mismatch is kind_seqtap_error, buf NULL,
   and no log here: udp.c warns once per period, with a count. */
int fct_seqtap_rx_udp(int tot_len, int fd, uint8_t *rx,
                  uint16_t *seq, int *buf_len, uint8_t **buf)
{
  int len, result = kind_seqtap_error;
  *buf = NULL;
  *buf_len = 0;
  *seq = 0;
  if (tot_len >= HEADER_TAP_MSG)
    result = seqtap_hdr(rx, &len);
  if (result != kind_seqtap_error)
    {
    if (tot_len - HEADER_TAP_MSG != len)
      result = kind_seqtap_error;
    else
      {
      *seq = ((rx[6] & 0xFF) << 8) + (rx[7] & 0xFF);
      *buf = rx + HEADER_TAP_MSG;
      *buf_len = len;
      }
    }
  return result;
}

/* Stream: header already in rx, payload read here. A fault is the error
   kind with buf NULL: the stream is out of step, the caller stops.        */
int fct_seqtap_rx(int fd, uint8_t *rx, uint16_t *seq,
                  int *buf_len, uint8_t **buf)
{
  int rxlen, len, got = 0, count = 0;
  int result = seqtap_hdr(rx, &len);
  *buf = NULL;
  *buf_len = 0;
  *seq = 0;
  if ((result != kind_seqtap_error) && (len <= 0))
    result = kind_seqtap_error;
  if (result == kind_seqtap_error)
    KERR("ERROR stream out of step: len %d, %hhX %hhX %hhX %hhX %hhX %hhX "
         "%hhX %hhX", len, rx[0], rx[1], rx[2], rx[3], rx[4], rx[5], rx[6],
         rx[7]);
  while ((result != kind_seqtap_error) && (got != len))
    {
    rxlen = read(fd, rx + HEADER_TAP_MSG + got, len - got);
    if (rxlen > 0)
      got += rxlen;
    else if ((rxlen == 0) ||
             ((errno != EINTR) && (errno != EWOULDBLOCK) &&
              (errno != EINPROGRESS) && (errno != EAGAIN)) ||
             (++count >= 50))
      {
      KERR("ERROR read %d errno %d got %d of %d", rxlen, errno, got, len);
      result = kind_seqtap_error;
      }
    }
  if (result != kind_seqtap_error)
    {
    *seq = ((rx[6] & 0xFF) << 8) + (rx[7] & 0xFF);
    *buf = rx + HEADER_TAP_MSG;
    *buf_len = len;
    }
  return result;
}

/* A tap child writes whole frames (< PIPE_BUF): 1 when the header of the
   next one is already in the pipe. One frame per turn of the loop let a tcp
   burst overflow the pipe, a turn costing more than a frame. */
int fct_seqtap_pipe_more(int fd)
{
  int avail = 0;
  if (ioctl(fd, FIONREAD, &avail) < 0)
    {
    KERR("WARNING the bytes waiting in tap pipe fd %d cannot be counted: %s",
         fd, strerror(errno));
    return 0;
    }
  return (avail >= HEADER_TAP_MSG);
}

/* Which channels are holding queued bytes. */
/* Queue sizes per channel; two allocations per chunk, so halve the count. */
void msg_layer_report_queues(char *txt, int txt_len)
{
  int i, len = 0, nb_tx = 0, nb_rx = 0;
  int worst_cidx = -1;
  unsigned long worst_size = 0;
  int rx_worst_cidx = -1, rx_worst_chunks = 0;
  long long total_tx = 0;
  long long chunks = 0;
  t_data_chunk *cur;
  unsigned long *nb = get_clownix_malloc_nb();

  if ((!txt) || (txt_len < 32))
    return;
  txt[0] = 0;
  for (i = 1; i < MAX_SELECT_CHANNELS; i++)
    {
    int this_rx = 0;
    if (!dchan[i].llid)
      continue;
    if (dchan[i].tot_txq_size > 0)
      {
      nb_tx += 1;
      total_tx += dchan[i].tot_txq_size;
      if (dchan[i].tot_txq_size > worst_size)
        {
        worst_size = dchan[i].tot_txq_size;
        worst_cidx = i;
        }
      }
    for (cur = dchan[i].rx; cur; cur = cur->next)
      this_rx += 1;
    for (cur = dchan[i].tx; cur; cur = cur->next)
      chunks += 1;
    chunks += this_rx;
    if (this_rx)
      {
      nb_rx += 1;
      if (this_rx > rx_worst_chunks)
        {
        rx_worst_chunks = this_rx;
        rx_worst_cidx = i;
        }
      }
    }
  len += snprintf(txt+len, txt_len-len,
                  "%d chan with tx (%lld bytes, worst llid %d: %lu), "
                  "%d chan with rx (worst llid %d: %d chunks). "
                  "Chunks on channels %lld = %lld allocations, of %lu live "
                  "ident1: %s",
                  nb_tx, total_tx,
                  (worst_cidx > 0) ? dchan[worst_cidx].llid : 0, worst_size,
                  nb_rx,
                  (rx_worst_cidx > 0) ? dchan[rx_worst_cidx].llid : 0,
                  rx_worst_chunks,
                  chunks, 2*chunks, nb[1],
                  (2*chunks + 200 < (long long)nb[1]) ?
                  "ORPHANED, not on any channel" : "accounted for");
}

int get_tot_txq_size(int cidx)
{
  return (dchan[cidx].tot_txq_size);
}

void set_cloonix_name(char *name)
{
  if (name && (strlen(name) > 1))
    {
    memset(g_cloonix_name, 0, MAX_NAME_LEN);
    strncpy(g_cloonix_name, name, MAX_NAME_LEN-1);
    g_cloonix_name_set = 1;
    }
}

unsigned long *get_clownix_malloc_nb(void)
{
  return clownix_malloc_nb;
}

int g_i_am_a_clone = 0;
int g_i_am_a_clone_no_kerr = 0;
int i_am_a_clone(void)
{
  return g_i_am_a_clone;
}

long long  *get_peak_queue_len(int cidx)
{
  return(&(peak_queue_len[cidx]));
}

t_data_channel *get_dchan(int cidx)
{
  return (&(dchan[cidx]));
}

char *get_bigbuf(void)
{
  return g_bigsndbuf;
}

void *clownix_malloc(int size, int ident)
{
  unsigned long *tmp;
  tmp = (unsigned long *) malloc(size + (8 * sizeof(unsigned long)));
  if (!tmp)
    KOUT("%d %d", size, ident);
  if (ident <= 0)
    KOUT(" ");
  if (ident >= MAX_MALLOC_TYPES)
    KOUT(" ");
  tmp[0] = 0xABCD; 
  tmp[1] = ident; 
  tmp[2] = size/sizeof(unsigned long) + 1;
  tmp[tmp[2]+4] = 0xCAFEDECA;
  tmp[tmp[2]+5] = 0xAACCCCBB;
  clownix_malloc_nb[0] += 1;
  clownix_malloc_nb[ident] += 1;
  return ((void *)(&(tmp[4])));
}

void clownix_free(void *ptr, const char *caller_ident)
{
  unsigned long *tmp;
  int ident;
  if (ptr)
    {
    tmp = (unsigned long *) ((char *)ptr - 4 * sizeof(unsigned long));
    if (tmp[0] != 0xABCD)
      KOUT("%s %08lX", caller_ident, tmp[0]);
    ident = tmp[1];
    if (ident <= 0)
      KOUT("%s", caller_ident);
    if (ident >= MAX_MALLOC_TYPES)
      KOUT(" ");
    if (tmp[tmp[2]+4] != 0xCAFEDECA)
      KOUT("%s, %d", caller_ident, ident);
    if (tmp[tmp[2]+5] != 0xAACCCCBB)
      KOUT("%s, %d", caller_ident, ident);
    memset(tmp, 0, (tmp[2]+5)*sizeof(unsigned long));
    free(tmp);
    if (clownix_malloc_nb[ident] == 0)
      KOUT("%s, %d", caller_ident, ident);
    if (clownix_malloc_nb[0] == 0)
      KOUT(" %s, %d",  caller_ident, ident);
    clownix_malloc_nb[0] -= 1;
    clownix_malloc_nb[ident] -= 1;
    }
}

int ip_string_to_int (uint32_t *inet_addr, char *ip_string)
{
  int result = -1;
  unsigned int part[4];
  if (strlen(ip_string) != 0)
    {
    if ((sscanf(ip_string,"%u.%u.%u.%u", 
                          &part[0], &part[1], &part[2], &part[3]) == 4) &&
        (part[0]<=0xFF) && (part[1]<=0xFF) &&
        (part[2]<=0xFF) && (part[3]<=0xFF))
      {
      result = 0;
      *inet_addr = 0;
      *inet_addr += part[0]<<24;
      *inet_addr += part[1]<<16;
      *inet_addr += part[2]<<8;
      *inet_addr += part[3];
      }
    }
  return result;
}

void int_to_ip_string (int addr, char *ip_string)
{
  sprintf(ip_string, "%d.%d.%d.%d", (int) ((addr >> 24) & 0xff),
          (int)((addr >> 16) & 0xff), (int)((addr >> 8) & 0xff),
          (int) (addr & 0xff));
}

static void default_err_kill(int llid, int err, int from)
{
  KOUT("ERROR %d %d %d", llid, err, from);
}

static void default_rx_callback(int llid, int len, char *str_rx) 
{
  KOUT("ERROR %d %d", llid, len);
}

void err_dchan_cb(int llid, int err, int from)
{
  int cidx;
  if (msg_exist_channel(llid))
    {
    cidx = channel_check_llid(llid, __FUNCTION__);
    if (cidx == 0)
      KERR("ERROR");
    else
      {
      t_fd_error err_cb_local = dchan[cidx].error_callback;
      if (!dchan[cidx].rx_callback)
        KERR("ERROR no rx callback on llid %d (err=%d from=%d), skipped",
             llid, err, from);
      if (!err_cb_local)
        {
        KERR("ERROR no error callback on llid %d (err=%d from=%d), "
             "deleting the channel: decoding_state=%d rx_cb=%s",
             llid, err, from, dchan[cidx].decoding_state,
             dchan[cidx].rx_callback ? "set" : "null");
        channel_delete(llid);
        }
      else
        err_cb_local(llid, err, from);
      }
    }
}

static int rx_dchan_traf_cb(int llid, int fd)
{
  int len, cidx, correct_recv = 0;
  cidx = channel_check_llid(llid, __FUNCTION__);
  if (cidx == 0)
    KERR("ERROR");
  else
    {
    if (!llid || (dchan[cidx].llid != llid))
      KOUT("ERROR");
    if (dchan[cidx].fd != fd)
      KOUT("ERROR");
    first_rx_buf = (char *)clownix_malloc(TRAF_TAP_MINUS_TCP, IOCMLC);
    len = util_read(first_rx_buf,TRAF_TAP_MINUS_TCP,get_fd_with_cidx(cidx));
    if (len < 0)
      {
      clownix_free(first_rx_buf, __FUNCTION__);
      if (errno != EOPNOTSUPP)
        err_dchan_cb(llid, errno, 1132);
      correct_recv = -1;
      }
    else
      {
      correct_recv = len;
      first_rx_buf[len] = 0;
      if ((dchan[cidx].decoding_state == rx_type_proxy_traf_unix_start) ||
          (dchan[cidx].decoding_state == rx_type_proxy_traf_tcp_start))
        {
        if (dchan[cidx].rx_callback)
          dchan[cidx].rx_callback(llid, len, first_rx_buf);
        else
          KERR("ERROR no rx callback on llid %d, %d bytes dropped, "
               "decoding_state=%d", llid, len, dchan[cidx].decoding_state);
        clownix_free(first_rx_buf, __FUNCTION__);
        }
      else
        KOUT("ERROR");
      }
    }
  return correct_recv;
}

static int rx_dchan_cb(int llid, int fd)
{                         
  int len, cidx, correct_recv = 0;
  cidx = channel_check_llid(llid, __FUNCTION__);
  if (cidx == 0)
    KERR("ERROR");
  else
    {
    if (!llid || (dchan[cidx].llid != llid))
      KOUT("ERROR");
    if (dchan[cidx].fd != fd)
      KOUT("ERROR");
    first_rx_buf = (char *)clownix_malloc(first_rx_buf_max+1, IOCMLC);
    len = util_read(first_rx_buf, first_rx_buf_max, get_fd_with_cidx(cidx));
    if (len < 0)
      {
      clownix_free(first_rx_buf, __FUNCTION__);
      if (errno != EOPNOTSUPP)
        err_dchan_cb(llid, errno, 1132);
      correct_recv = -1;
      }
    else
      { 
      correct_recv = len;
      first_rx_buf[len] = 0;
      if ((dchan[cidx].decoding_state == rx_type_proxy_traf_unix_start) ||
          (dchan[cidx].decoding_state == rx_type_proxy_traf_tcp_start))
        KOUT("ERROR");
      new_rx_to_process(&(dchan[cidx]), len, first_rx_buf);
      }
    }
  return correct_recv;
}

int tx_dchan_cb(int llid, int fd)
{
  int cidx, result, correct_send, total_correct_send = 0;
  cidx = channel_check_llid(llid, __FUNCTION__);
  if (cidx == 0)
    KERR("ERROR");
  else
    {
    if (dchan[cidx].llid !=  llid)
      KOUT("ERROR");
    if (dchan[cidx].fd !=  fd)
      KOUT("ERROR");
    if (channel_get_tx_queue_len(llid))
      {
      if (!dchan[cidx].tx)
        KOUT("ERROR");
      do
        {
        result = tx_try_send_chunk(&(dchan[cidx]), cidx,
                                   &correct_send, err_dchan_cb);
        total_correct_send += correct_send;
        } while (result == 1);
      }
    }
  return total_correct_send;
}

static int server_has_new_connect_from_client(int id, int fd) 
{
  int fd_new, llid, cidx, serv_cidx;
  char *little_name;
  serv_cidx = channel_check_llid(id, __FUNCTION__);
  if (serv_cidx == 0)
    KERR("ERROR");
  else
    {
    util_fd_accept(fd, &fd_new, __FUNCTION__);
    if (fd_new >= 0)
      {
      little_name = channel_get_little_name(id);
      llid = channel_create(fd_new, kind_server, little_name, rx_dchan_cb, 
                            tx_dchan_cb, err_dchan_cb);
      if (!llid)
        KOUT("ERROR");
      cidx = channel_check_llid(llid, __FUNCTION__);
      if (cidx == 0)
        KERR("ERROR");
      else
        {
        if (!dchan[serv_cidx].server_connect_callback)
          KOUT("ERROR");
        memset(&dchan[cidx], 0, sizeof(t_data_channel));
        dchan[cidx].decoding_state = rx_type_ascii_start;
        dchan[cidx].llid = llid;
        dchan[cidx].fd = fd_new;
        dchan[serv_cidx].server_connect_callback(id, llid);
        }
      }
    }
  return 0;
}

void msg_delete_channel(int llid)
{
  int cidx;
  cidx = channel_check_llid(llid, __FUNCTION__);
  if (cidx == 0)
    KERR("ERROR");
  else
    {
    channel_delete(llid);
    chain_delete(&(dchan[cidx].tx), &(dchan[cidx].last_tx));
    chunk_chain_delete(&(dchan[cidx]));
    memset(&(dchan[cidx]), 0, sizeof(t_data_channel));
    }
}

void msg_mngt_heartbeat_ms_set (int heartbeat_ms)
{
 channel_heartbeat_ms_set (heartbeat_ms);
}

void msg_mngt_heartbeat_init (t_heartbeat_cb heartbeat)
{
  channel_beat_set (heartbeat);
}

int msg_mngt_get_tx_queue_len(int llid)
{
  return (channel_get_tx_queue_len(llid));
}

/* Past TX_QUEUE_HIGH queued bytes a flow to that llid holds back: a relay
   stops reading until under TX_QUEUE_LOW, a stream of packets drops. */
#define TX_QUEUE_HIGH (8 * 1024 * 1024)
#define TX_QUEUE_LOW  (1 * 1024 * 1024)

int msg_tx_queue_high(int llid)
{
  return (channel_get_tx_queue_len(llid) > TX_QUEUE_HIGH);
}

int msg_tx_queue_low(int llid)
{
  return (channel_get_tx_queue_len(llid) < TX_QUEUE_LOW);
}

/* A relay reads on one llid and writes on another. On a beat; *paused is
   the relay's. */

void msg_relay_flow_ctrl(int rx_llid, int tx_llid, int *paused)
{
  unsigned long q;
  if ((!msg_exist_channel(rx_llid)) || (!msg_exist_channel(tx_llid)))
    {
    if ((*paused) && (msg_exist_channel(rx_llid)))
      channel_rx_local_flow_ctrl(rx_llid, 0);
    *paused = 0;
    return;
    }
  q = channel_get_tx_queue_len(tx_llid);
  if ((!(*paused)) && (q > TX_QUEUE_HIGH))
    {
    channel_rx_local_flow_ctrl(rx_llid, 1);
    *paused = 1;
    }
  else if ((*paused) && (q < TX_QUEUE_LOW))
    {
    channel_rx_local_flow_ctrl(rx_llid, 0);
    *paused = 0;
    }
}

int msg_watch_fd(int fd, t_fd_event rx_data,
                 t_fd_error err,  char *little_name)
{
  int llid, cidx;
  if (fd < 0)
    KOUT("ERROR");
  if (!err)
    KOUT("ERROR");
  llid = channel_create(fd, kind_simple_watch, little_name, rx_data, 
                        tx_dchan_cb, err_dchan_cb);
  if (llid)
    {
    cidx = channel_check_llid(llid, __FUNCTION__);
    if (cidx == 0)
      {
      KERR("ERROR");
      llid = 0;
      }
    else
      {
      memset(&dchan[cidx], 0, sizeof(t_data_channel));
      dchan[cidx].decoding_state = rx_type_watch;
      dchan[cidx].rx_callback = default_rx_callback;
      dchan[cidx].error_callback = err;
      dchan[cidx].llid = llid;
      dchan[cidx].fd = fd;
      }
    }
  return (llid);
}

void proxy_traf_tx(int llid, int len, char *str_tx, int is_unix)
{     
  char *ntx;
  int cidx;
  if ((len<=0) || (len > MAX_TOT_LEN_QSIG))
    KOUT("ERROR %d", len);
  cidx = channel_check_llid(llid, __FUNCTION__);
  if (cidx == 0)
    KERR("ERROR");
  else
    {
    if (dchan[cidx].llid != llid)
      KOUT("ERROR %d %d %d", cidx, dchan[cidx].llid, llid);
    if (is_unix)
      {
      if ((dchan[cidx].decoding_state != rx_type_proxy_traf_unix_start) &&
          (dchan[cidx].decoding_state != rx_type_doorways))
        KOUT("ERROR %d", dchan[cidx].decoding_state);
      }
    else
      {
      if (dchan[cidx].decoding_state != rx_type_proxy_traf_tcp_start)
        KOUT("ERROR %d", dchan[cidx].decoding_state);
      }
    if (dchan[cidx].tot_txq_size > (2 * MAX_TOT_LEN_QSIG))
      KOUT("ERROR %d %lu", len, dchan[cidx].tot_txq_size);
    else
      {
      dchan[cidx].tot_txq_size += len;
      if (peak_queue_len[cidx] < dchan[cidx].tot_txq_size)
        peak_queue_len[cidx] = dchan[cidx].tot_txq_size; 
      ntx = (char *)clownix_malloc(len, IOCMLC);
      memcpy (ntx, str_tx, len);
      chain_append_tx(&(dchan[cidx].tx), &(dchan[cidx].last_tx), len, ntx);
      }
    }
}

  
void proxy_traf_unix_tx(int llid, int len, char *str_tx)
{
  proxy_traf_tx(llid, len, str_tx, 1);
}

void proxy_traf_tcp_tx(int llid, int len, char *str_tx)
{
  proxy_traf_tx(llid, len, str_tx, 0);
}

void proxy_sig_tx(int llid, int len, char *str_tx)
{
  char *ntx;
  int cidx;
  if ((len<=0) || (len > MAX_TOT_LEN_PROXY))
    KOUT("ERROR %d", len);
  cidx = channel_check_llid(llid, __FUNCTION__);
  if (cidx == 0)
    KERR("ERROR");
  else
    {
    if (dchan[cidx].llid != llid)
      KOUT("ERROR %d %d %d", cidx, dchan[cidx].llid, llid);
    if ((dchan[cidx].decoding_state != rx_type_proxy_sig_start) &&
        (dchan[cidx].decoding_state != rx_type_proxy_sig_header_ok))
      KOUT("ERROR %d", dchan[cidx].decoding_state);
    if (dchan[cidx].tot_txq_size >  MAX_TOT_LEN_QSIG)
      KOUT("ERROR %d %lu", len, dchan[cidx].tot_txq_size);
    else
      {
      dchan[cidx].tot_txq_size += len + PROXY_HEADER_LEN;
      if (peak_queue_len[cidx] < dchan[cidx].tot_txq_size)
        peak_queue_len[cidx] = dchan[cidx].tot_txq_size;
      ntx = (char *)clownix_malloc(len + PROXY_HEADER_LEN, IOCMLC);
      memcpy (ntx + PROXY_HEADER_LEN, str_tx, len);
      ntx[0] = ((len >> 24) & 0xff);
      ntx[1] = ((len >> 16) & 0xff);
      ntx[2] = ((len >> 8) & 0xff);
      ntx[3] = (len & 0xff);
      chain_append_tx(&(dchan[cidx].tx), &(dchan[cidx].last_tx),
                      len + PROXY_HEADER_LEN, ntx);
      }
    }
}

static void proxy_traf_server_connect_from_client(int id, int fd, int is_unix)
{
  int  kind, fd_new, llid, cidx, serv_cidx;
  if (is_unix)
    kind = kind_server_proxy_traf_unix;
  else 
    kind = kind_server_proxy_traf_inet;
  serv_cidx = channel_check_llid(id, __FUNCTION__);
  if (serv_cidx == 0)
    KERR("ERROR");
  else
    {
    util_fd_accept(fd, &fd_new, __FUNCTION__);
    if (fd_new >= 0)
      {
      llid = channel_create(fd_new, kind, "proxy", rx_dchan_traf_cb,
                            tx_dchan_cb, err_dchan_cb);
      if (!llid)
        KOUT("ERROR");
      cidx = channel_check_llid(llid, __FUNCTION__);
      if (cidx == 0)
        {
        KERR("ERROR");
        llid = 0;
        }
      else
        {
        channel_rx_local_flow_ctrl(llid, 1);
        if (!dchan[serv_cidx].server_connect_callback)
          KOUT(" ");
        memset(&dchan[cidx], 0, sizeof(t_data_channel));
        if (is_unix)
          dchan[cidx].decoding_state = rx_type_proxy_traf_unix_start;
        else
          dchan[cidx].decoding_state = rx_type_proxy_traf_tcp_start;
        dchan[cidx].llid = llid;
        dchan[cidx].fd = fd_new;
        dchan[serv_cidx].server_connect_callback(id, llid);
        }
      }
    }
}

static int proxy_traf_unix_server_connect_from_client(int id, int fd)
{
  proxy_traf_server_connect_from_client(id, fd, 1);
  return 0;
}

static int proxy_traf_tcp_server_connect_from_client(int id, int fd)
{ 
  proxy_traf_server_connect_from_client(id, fd, 0);
  return 0;
}

static int proxy_sig_server_connect_from_client(int id, int fd)
{   
  int fd_new, llid, cidx, serv_cidx;
  serv_cidx = channel_check_llid(id, __FUNCTION__);
  if (serv_cidx == 0)
    KERR("ERROR");
  else
    {
    util_fd_accept(fd, &fd_new, __FUNCTION__);
    if (fd_new >= 0)
      {
      llid = channel_create(fd_new, kind_server_proxy_sig, "proxy", rx_dchan_cb,
                            tx_dchan_cb, err_dchan_cb);
      if (!llid)
        KOUT("ERROR");
      cidx = channel_check_llid(llid, __FUNCTION__);
      if (cidx == 0)
        {
        KERR("ERROR");
        llid = 0;
        }
      else
        {
        if (!dchan[serv_cidx].server_connect_callback)
          KOUT(" ");
        memset(&dchan[cidx], 0, sizeof(t_data_channel));
        dchan[cidx].decoding_state = rx_type_proxy_sig_start;
        dchan[cidx].llid = llid;
        dchan[cidx].fd = fd_new;
        dchan[serv_cidx].server_connect_callback(id, llid);
        }
      }
    }
  return 0;
}

static int proxy_traf_client(char *pname, uint32_t ip, uint16_t port,
                             t_fd_error err_cb, t_msg_rx_cb rx_cb, int is_unix)
{
  int res, fd, llid=0, cidx;
  if (is_unix)
    res = util_proxy_client_socket_unix(pname, &fd);
  else
    res = util_client_socket_inet(ip, port, &fd);
  if (!res)
    {
    if (!err_cb)
      KOUT("ERROR");
    if (!rx_cb)
      KOUT("ERROR");
    llid = channel_create(fd, kind_client, "proxy", rx_dchan_traf_cb,
                          tx_dchan_cb, err_dchan_cb);
    if (!llid)
      KOUT(" ");
    cidx = channel_check_llid(llid, __FUNCTION__);
    if (cidx == 0)
      {
      KERR("ERROR");
      llid = 0;
      }
    else
      {
      memset(&dchan[cidx], 0, sizeof(t_data_channel));
      if (is_unix)
        dchan[cidx].decoding_state = rx_type_proxy_traf_unix_start;
      else
        dchan[cidx].decoding_state = rx_type_proxy_traf_tcp_start;
      dchan[cidx].llid = llid;
      dchan[cidx].fd = fd;
      dchan[cidx].error_callback = err_cb;
      dchan[cidx].rx_callback = rx_cb;
      }
    }
  return (llid);
}

int proxy_traf_unix_client(char *pname, t_fd_error err_cb, t_msg_rx_cb rx_cb)
{
  return proxy_traf_client(pname, 0, 0, err_cb, rx_cb, 1);
}

int proxy_sig_client(char *pname, t_fd_error err_cb, t_msg_rx_cb rx_cb)
{ 
  int fd, llid=0, cidx;
  if (util_client_socket_unix(pname, &fd))
    KERR("WARNING %s", pname);
  else
    {
    if (!err_cb)
      KOUT(" ");
    if (!rx_cb)
      KOUT(" ");
    llid = channel_create(fd, kind_client, "proxy", rx_dchan_cb,
                          tx_dchan_cb, err_dchan_cb);
    if (!llid)
      KOUT(" ");
    cidx = channel_check_llid(llid, __FUNCTION__);
    if (cidx == 0)
      {
      KERR("ERROR");
      llid = 0;
      }
    else
      {
      memset(&dchan[cidx], 0, sizeof(t_data_channel));
      dchan[cidx].decoding_state = rx_type_proxy_sig_start;
      dchan[cidx].llid = llid;
      dchan[cidx].fd = fd;
      dchan[cidx].error_callback = err_cb;
      dchan[cidx].rx_callback = rx_cb;
      }
    }
  return (llid);
}

static int proxy_traf_server(char *pname, uint16_t port,
                             t_fd_connect connect_cb, int is_unix)
{
  int llid=0, cidx, listen_fd;
  if (is_unix)
    listen_fd = util_socket_listen_unix(pname);
  else
    listen_fd = util_socket_listen_inet(port);
  if (listen_fd < 0)
    {
    if (pname)
      KERR("ERROR %d %d %s", listen_fd, errno, pname);
    else
      KERR("ERROR %d %d %hu", listen_fd, errno, port);
    }
  else
    {
    if (is_unix)
      {
      llid = channel_create(listen_fd, kind_simple_watch, "proxy",
                            proxy_traf_unix_server_connect_from_client,
                            NULL, default_err_kill);
      }
    else
      {
      llid = channel_create(listen_fd, kind_simple_watch, "proxy",
                            proxy_traf_tcp_server_connect_from_client,
                            NULL, default_err_kill);
      }
    if (!llid)
      KOUT("ERROR");
    cidx = channel_check_llid(llid, __FUNCTION__);
    if (cidx == 0)
      {
      KERR("ERROR");
      llid = 0;
      }
    else
      {
      memset(&dchan[cidx], 0, sizeof(t_data_channel));
      dchan[cidx].decoding_state = rx_type_listen;
      dchan[cidx].server_connect_callback = connect_cb;
      }
    }
  return llid;
}

int proxy_traf_tcp_server(uint16_t port, t_fd_connect connect_cb)
{
  return proxy_traf_server(NULL, port, connect_cb, 0);
}

int proxy_sig_server(char *pname, t_fd_connect connect_cb)
{
  int llid=0, cidx, listen_fd;
  listen_fd = util_socket_listen_unix(pname);
  if (listen_fd < 0)
    KERR("ERROR %d %d", listen_fd, errno);
  else
    {
    llid = channel_create(listen_fd, kind_simple_watch, "proxy",
                          proxy_sig_server_connect_from_client,
                          NULL, default_err_kill);
    if (!llid)
      KOUT(" ");
    cidx = channel_check_llid(llid, __FUNCTION__);
    if (cidx == 0)
      {
      KERR("ERROR");
      llid = 0;
      }
    else
      {
      memset(&dchan[cidx], 0, sizeof(t_data_channel));
      dchan[cidx].decoding_state = rx_type_listen;
      dchan[cidx].server_connect_callback = connect_cb;
      }
    }
  return llid;
}

int string_server_unix(char *pname, t_fd_connect connect_cb,
                        char *little_name)
{
  int llid=0, cidx, listen_fd;
  listen_fd = util_socket_listen_unix(pname);
  if (listen_fd < 0)
    KERR("ERROR no listen socket on %s: %s", pname, strerror(errno));
  else
    {
    llid = channel_create(listen_fd, kind_simple_watch, little_name,
                          server_has_new_connect_from_client,
                          NULL, default_err_kill);
    if (!llid)
      KOUT(" ");
    cidx = channel_check_llid(llid, __FUNCTION__);
    if (cidx == 0)
      {
      KERR("ERROR");
      llid = 0;
      }
    else
      {
      memset(&dchan[cidx], 0, sizeof(t_data_channel));
      dchan[cidx].decoding_state = rx_type_ascii_start;
      dchan[cidx].server_connect_callback = connect_cb;
      }
    }
  return llid;
}

int string_server_inet(uint16_t port, t_fd_connect connect_cb, char *little_name)
{
  int llid = 0, cidx, listen_fd;
  listen_fd = util_socket_listen_inet(port);
  if (listen_fd < 0)
    KERR("ERROR no listen socket on port %hu: %s", port, strerror(errno));
  else
    {
    llid = channel_create(listen_fd, kind_simple_watch, little_name,
                          server_has_new_connect_from_client,
                          NULL, default_err_kill);
    if (!llid)
      KOUT(" ");
    cidx = channel_check_llid(llid, __FUNCTION__);
    if (cidx == 0)
      {
      KERR("ERROR");
      llid = 0;
      }
    else
      {
      memset(&dchan[cidx], 0, sizeof(t_data_channel));
      dchan[cidx].decoding_state = rx_type_listen;
      dchan[cidx].is_tcp = 1;
      dchan[cidx].server_connect_callback = connect_cb;
      }
    }
  return llid;
}

int  string_client_unix(char *pname, t_fd_error err_cb, 
                        t_msg_rx_cb rx_cb, char *little_name)
{
  int fd, llid=0, cidx;
  if (!util_client_socket_unix(pname, &fd))
    {
    if (!err_cb)
      KOUT(" ");
    if (!rx_cb)
      KOUT(" ");
    llid = channel_create(fd, kind_client, little_name, rx_dchan_cb, 
                          tx_dchan_cb, err_dchan_cb);
    if (!llid)
      KOUT(" ");
    cidx = channel_check_llid(llid, __FUNCTION__);
    if (cidx == 0)
      {
      KERR("ERROR");
      llid = 0;
      }
    else
      {
      memset(&dchan[cidx], 0, sizeof(t_data_channel));
      dchan[cidx].decoding_state = rx_type_ascii_start;
      dchan[cidx].llid = llid;
      dchan[cidx].fd = fd;
      dchan[cidx].error_callback = err_cb;
      dchan[cidx].rx_callback = rx_cb;
      }
    }
  return (llid);
}

void  msg_mngt_set_callbacks (int llid, t_fd_error err_cb, 
                              t_msg_rx_cb rx_cb)
{
  int cidx;
  cidx = channel_check_llid(llid, __FUNCTION__);
  if (cidx == 0)
    KERR("ERROR");
  else
    {
    if (dchan[cidx].llid !=  llid)
      KOUT(" ");
    if (!err_cb)
      KOUT(" ");
    if (!rx_cb)
      KOUT(" ");
    dchan[cidx].error_callback = err_cb;
    dchan[cidx].rx_callback = rx_cb;
    }
}

void string_tx(int llid, int len, char *str_tx)
{
  char *ntx;
  int cidx;
  if ((len<=0) || (len > MAX_TOT_LEN_QSIG))
    KOUT("%d", len);
  if (msg_exist_channel(llid))
    {
    cidx = channel_check_llid(llid, __FUNCTION__);
    if (cidx == 0)
      KERR("ERROR");
    else
      {
      if (dchan[cidx].llid != llid)
        KOUT("%d %d %d", cidx, dchan[cidx].llid, llid);
      if ((dchan[cidx].decoding_state != rx_type_ascii_start) && 
          (dchan[cidx].decoding_state != rx_type_open_bound_found)) 
        KOUT(" ");
      if (dchan[cidx].tot_txq_size < MAX_TOT_LEN_QSIG)
        {
        dchan[cidx].tot_txq_size += len;
        if (peak_queue_len[cidx] < dchan[cidx].tot_txq_size)
          peak_queue_len[cidx] = dchan[cidx].tot_txq_size;
        ntx = (char *)clownix_malloc(len, IOCMLC);
        memcpy (ntx, str_tx, len);
        chain_append_tx(&(dchan[cidx].tx), &(dchan[cidx].last_tx), len, ntx);
        }
      }
    }
}

static void ptr_string_tx(int llid, int len, char *str_tx)
{
  if (msg_exist_channel(llid))
    string_tx(llid, len, str_tx);
}

void watch_tx(int llid, int len, char *str_tx)
{
  char *ntx;
  int cidx;
  if ((msg_exist_channel(llid)) && len)
    {
    if ((len<0) || (len > MAX_TOT_LEN_QDAT))
      KOUT("%d", len);
    cidx = channel_check_llid(llid, __FUNCTION__);
    if (cidx == 0)
      KERR("ERROR");
    else
      {
      if (dchan[cidx].llid != llid)
        KOUT(" ");
      if (dchan[cidx].decoding_state != rx_type_watch)
        KOUT(" ");
      if (dchan[cidx].tot_txq_size < MAX_TOT_LEN_QDAT)
        {
        dchan[cidx].tot_txq_size += len;
        if (peak_queue_len[cidx] < dchan[cidx].tot_txq_size)
          peak_queue_len[cidx] = dchan[cidx].tot_txq_size;
        ntx = (char *)clownix_malloc(len, IOCMLC);
        memcpy (ntx, str_tx, len);
        chain_append_tx(&(dchan[cidx].tx), &(dchan[cidx].last_tx), len, ntx);
        }
      else
        KERR("WARNING tx queue of %d bytes is full (%d), message dropped",
             (int)dchan[cidx].tot_txq_size, MAX_TOT_LEN_QDAT);
      }
    }
  else
    KERR("ERROR %d %d %s", llid, len, str_tx);
}

void string_tx_now(int llid, int len, char *str_tx)
{
  int fd, cidx, txlen, len_sent = 0, count = 0;
  cidx = channel_check_llid(llid, __FUNCTION__);
  if (cidx == 0)
    KERR("ERROR");
  else
    {
    if (dchan[cidx].llid != llid)
      KOUT(" ");
    if ((dchan[cidx].decoding_state != rx_type_ascii_start) && 
        (dchan[cidx].decoding_state != rx_type_open_bound_found))
      KOUT(" ");
    fd = get_fd_with_cidx(cidx);
    nonnonblock_fd(fd);
    while (len_sent != len)
      {   
      txlen = write (fd, str_tx + len_sent, len - len_sent);
      if (txlen < 0)
        {
        KERR("ERROR write: %s, %d bytes not sent, channel closed",
             strerror(errno), len - len_sent);
        err_dchan_cb(llid, errno, 131);
        msg_delete_channel(llid);
        break;
        }
      else if (txlen == 0)
        {
        count++;
        if (count == 1000)
          {
          KERR("ERROR write took 0 bytes 1000 times, %d bytes not sent, "
               "channel closed", len - len_sent);
          err_dchan_cb(llid, errno, 131);
          msg_delete_channel(llid);
          break;
          }
        }
      fsync(fd);
      len_sent += txlen;
      }
    }
}

void msg_mngt_loop(void)
{
  channel_loop();
}

static void signal_pipe(int no_use)
{
}

#include <ucontext.h>
#include <execinfo.h>
static void sigsegv_handler_info(int sig, siginfo_t *info, void *ctx)
{
/* syslog and backtrace_symbols are not signal-safe: best effort, we die. */
  void *bt[256];
  int i, bt_size;
  char **bt_syms;
  unsigned long addr = (unsigned long) (info ? info->si_addr : 0);

  if (sig == SIGQUIT)
    KERR("ERROR CRASH signal=%d pid=%d: not a fault, a watchdog found this "
         "process not answering and asks where it is", sig, (int) getpid());
  KERR("ERROR CRASH signal=%d pid=%d faulting_address=0x%lx",
       sig, (int) getpid(), addr);

  if (addr == 0)
    KERR("ERROR CRASH ADDRESS ZERO: a null function pointer was called, or a null "
         "struct dereferenced. The jump to zero destroys the return chain, "
         "so the frames below are usually few or none -- look for a callback "
         "that can be left unset, and use the core dump for the registers");

  if (ctx)
    {
    ucontext_t *uc = (ucontext_t *) ctx;
    unsigned long rip = (unsigned long) uc->uc_mcontext.gregs[REG_RIP];
    unsigned long rsp = (unsigned long) uc->uc_mcontext.gregs[REG_RSP];
    KERR("ERROR CRASH rip=0x%lx rsp=0x%lx", rip, rsp);
    if (rip == 0)
      {
      int k;
      KERR("ERROR CRASH rip is 0, so the CALLER's return address is on the stack. "
           "Run each of the next lines through addr2line -- the first that "
           "resolves inside a cloonix binary is who made the call:");
      for (k = 0; k < 6; k++)
        {
        unsigned long *slot = ((unsigned long *) rsp) + k;
        KERR("ERROR CRASH   stack[%d] = 0x%lx", k, *slot);
        }
      }
    }

  bt_size = backtrace(bt, 256);
  KERR("ERROR CRASH backtrace has %d frame(s):", bt_size);
  bt_syms = backtrace_symbols(bt, bt_size);
  if (!bt_syms)
    KERR("ERROR CRASH cannot symbolise the frames (out of memory in the handler)");
  else
    {
    for (i = 0; i < bt_size; i++)
      KERR("ERROR CRASH frame %d: %s", i, bt_syms[i]);
    }
  KERR("ERROR CRASH to name an address: addr2line -f -C -i -e <binary> 0x<offset> "
       "-- use the offset from the (+0x...) part WITHOUT the plus. If the "
       "installed binary is stripped, use the one in the build tree; the "
       "offsets only match the exact build that crashed.");

  {
  sigset_t set;
  signal(sig, SIG_DFL);
  sigemptyset(&set);
  sigaddset(&set, sig);
  sigprocmask(SIG_UNBLOCK, &set, NULL);
  }
  raise(sig);
  _exit(1);
}

void msg_mngt_init (char *name, int max_len_per_read)
{
  struct sigaction act;

  {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_sigaction = sigsegv_handler_info;
  sa.sa_flags = SA_SIGINFO;
  sigemptyset(&(sa.sa_mask));
  sigaction(SIGSEGV, &sa, NULL);
  sigaction(SIGBUS,  &sa, NULL);
  sigaction(SIGFPE,  &sa, NULL);
  sigaction(SIGILL,  &sa, NULL);
  sigaction(SIGQUIT, &sa, NULL);
  }

  pthexec_init();
  memset (peak_queue_len, 0, MAX_SELECT_CHANNELS*sizeof(long long));
  memset (clownix_malloc_nb, 0, MAX_MALLOC_TYPES*sizeof(unsigned long));
  if (g_cloonix_name_set == 0)
    strcpy(g_cloonix_name, " ");
  memset(&act, 0, sizeof(act));
  sigfillset(&act.sa_mask);
  act.sa_flags = 0;
  act.sa_handler = signal_pipe;
  sigaction(SIGPIPE, &act, NULL);
  first_rx_buf_max = max_len_per_read;
  memset(dchan, 0, CLOWNIX_MAX_CHANNELS*sizeof(t_data_channel));
  channel_init();
  rpct_init(ptr_string_tx);
} 

char *get_cloonix_name(void)
{
  return (g_cloonix_name);
}
