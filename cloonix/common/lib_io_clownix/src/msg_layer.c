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
#include <bits/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <execinfo.h>




#include "io_clownix.h"
#include "channel.h"
#include "util_sock.h"
#include "msg_layer.h"
#include "chunk.h"
#include "doorways_sock.h"


#define MAX_TOT_LEN_QSIG 500000000
#define MAX_TOT_LEN_QDAT 1000000000
/*---------------------------------------------------------------------------*/
static t_data_channel dchan[CLOWNIX_MAX_CHANNELS];
static char *first_rx_buf;
static int first_rx_buf_max;
static long long  peak_queue_len[MAX_SELECT_CHANNELS];
/*---------------------------------------------------------------------------*/
static char g_bigsndbuf[MAX_SIZE_BIGGEST_MSG];
static unsigned long clownix_malloc_nb[MAX_MALLOC_TYPES];
static char g_cloonix_name[MAX_NAME_LEN];
static int g_cloonix_name_set = 0;
static int g_fd_not_to_close;
static int g_fd_not_to_close_set = 0;


void doorways_linker_helper(void);
void cloonix_conf_linker_helper(void);

/****************************************************************************/
char *malloc_to_ascii_encode(int len, uint8_t *msg)
{
  uint8_t *byte = (uint8_t *) msg;
  int size = len;
  int len_str = 3 * len + 1;
  char *ascii_msg, *ptr;
  if (len > MAX_ICMP_RXTX_SIG_LEN)
    KOUT("ERROR %d", len);
  ascii_msg = (char *) malloc(len_str);
  if (ascii_msg == NULL)
    KOUT("ERROR %d", len);
  memset(ascii_msg, 0, len_str);
  ptr = ascii_msg;
  while (size > 0)
    {
    size--;
    sprintf(ptr, "%02hhx ", *byte);
    byte++;
    ptr += 3;
    }
  return ascii_msg;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
uint8_t *malloc_to_byte_encode(int len, char *msg)
{
  int len_str = strlen(msg);
  int vlen = len_str / 3;
  int size = vlen;
  uint8_t *byte, *byte_msg, *result = NULL;
  char *ptr;
  if ((vlen * 3) != len_str) 
    KERR("ERROR %d %d %s", len_str, vlen, msg);
  else
    {
    if (vlen > MAX_ICMP_RXTX_SIG_LEN)
      KOUT("ERROR %d", vlen);
    byte_msg = (uint8_t *) malloc(vlen);
    if (byte_msg == NULL)
      KOUT("ERROR %d", len_str);
    byte = byte_msg;
    ptr = msg;
    while (size > 0)
      {
      size--;
      sscanf(ptr, "%02hhx ", byte);
      byte++;
      ptr += 3;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void fct_seqtap_tx(int kind, uint8_t *tx, uint16_t seqtap,
                   int len, uint8_t *buf)
{
  if (len > MAX_TAP_BUF_LEN + END_FRAME_ADDED_CHECK_LEN)
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
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int fct_seqtap_rx(int is_dgram, int tot_len, int fd, uint8_t *rx,
                  uint16_t *seq, int *buf_len, uint8_t **buf)
{
  int result = kind_seqtap_error;
  int rxlen, len, count = 0;
  *buf = NULL;
  *buf_len = 0;
  *seq = 0;
  if ((rx[0] != 0xCA) || (rx[1] != 0xFE))
    KERR("ERROR %hhX %hhX %hhX %hhX %hhX %hhX %hhX %hhX",
         rx[0], rx[1], rx[2], rx[3], rx[4], rx[5], rx[6], rx[7]);
  else
    {
    if ((rx[2] == 0xDE) && (rx[3] == 0xCA))
      result = kind_seqtap_data;
    else if ((rx[2] == 0xBE) && (rx[3] == 0xEF))
      result = kind_seqtap_sig_hello;
    else if ((rx[2] == 0xAA) && (rx[3] == 0xBB))
      result = kind_seqtap_sig_ready;
    else
      KERR("ERROR %hhX %hhX %hhX %hhX %hhX %hhX %hhX %hhX",
           rx[0], rx[1], rx[2], rx[3], rx[4], rx[5], rx[6], rx[7]);
    }
  if (result != kind_seqtap_error)
    {
    len = ((rx[4] & 0xFF) << 8) + (rx[5] & 0xFF);
    if (len > MAX_TAP_BUF_LEN + END_FRAME_ADDED_CHECK_LEN)
      KERR("ERROR %d %hhX %hhX %hhX %hhX %hhX %hhX %hhX %hhX",
           len, rx[0], rx[1], rx[2], rx[3], rx[4], rx[5], rx[6], rx[7]);
    else
      {
      *seq = ((rx[6] & 0xFF) << 8) + (rx[7] & 0xFF);
      if (is_dgram == 0)
        {
        rxlen = read(fd, rx + HEADER_TAP_MSG, len);
        while (rxlen == -1)
          {
          if ((errno!=EINTR) && (errno!=EWOULDBLOCK) && (errno!=EAGAIN))
            {
            KERR("ERROR %d %d %d %hhX %hhX %hhX %hhX %hhX %hhX %hhX %hhX",
                 rxlen, errno, len, rx[0], rx[1], rx[2], rx[3],
                 rx[4], rx[5], rx[6], rx[7]);
            break;
            } 
          count += 1;
          if (count >= 50)
            {
            KERR("ERROR %d %d %d %hhX %hhX %hhX %hhX %hhX %hhX %hhX %hhX",
                 rxlen, errno, len, rx[0], rx[1], rx[2], rx[3],
                 rx[4], rx[5], rx[6], rx[7]);
            break;
            }
          usleep(100);
          rxlen = read(fd, rx + HEADER_TAP_MSG, len);
          }
        }
      else
        {
        rxlen = tot_len - HEADER_TAP_MSG;
        }
      if (rxlen != len)
        {
        KERR("ERROR %d %d %d %hhX %hhX %hhX %hhX %hhX %hhX %hhX %hhX",
             rxlen, errno, len, rx[0], rx[1], rx[2], rx[3],
             rx[4], rx[5], rx[6], rx[7]);
        }
      else
        {
        *buf = rx + HEADER_TAP_MSG;
        *buf_len = len;
        }
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int get_pid_num_and_name(char *name)
{
  FILE *fp;
  char ps_cmd[MAX_PATH_LEN];
  char line[MAX_PATH_LEN];
  char tmp_name[MAX_PATH_LEN];
  int pid, result = 0;
  snprintf(ps_cmd, MAX_PATH_LEN-1, "%s axo pid,args", PS_BIN);
  fp = popen(ps_cmd, "r");
  if (fp == NULL)
    KERR("ERROR %s %d", ps_cmd, errno);
  else
    {
    memset(line, 0, MAX_PATH_LEN);
    while (fgets(line, MAX_PATH_LEN-1, fp))
      {
      if (sscanf(line,
         "%d /usr/libexec/cloonix/server/cloonix-main-server "
         "/usr/libexec/cloonix/common/etc/cloonix.cfg %s",
         &pid, tmp_name) == 2) 
       {
       result = pid;
       strncpy(name, tmp_name, MAX_NAME_LEN-1);
       break;
       }
      }
    pclose(fp);
    }
  return result;
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
int lib_io_running_in_crun(char *name)
{
  int pid, result = 0;
  char *file_name = "/proc/1/cmdline";
  char buf[MAX_PATH_LEN];
  FILE *fd = fopen(file_name, "r");
  if (fd == NULL)
    KERR("WARNING: Cannot open %s", file_name);
  else
    {
    memset(buf, 0, MAX_PATH_LEN);
    if (!fgets(buf, MAX_PATH_LEN-1, fd))
      KERR("WARNING: Cannot read %s", file_name);
    else if (strstr(buf, CRUN_STARTER))
      {
      result = 1;
      if (name)
        {
        memset(name, 0, MAX_NAME_LEN);
        pid = get_pid_num_and_name(name);
        if (pid == 0)
          KERR("WARNING NO CLOONIX SERVER RUNNING");
        }
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/



/*****************************************************************************/
int get_tot_txq_size(int cidx)
{
  return (dchan[cidx].tot_txq_size);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int get_fd_not_to_close(void)
{
  return (g_fd_not_to_close);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void set_fd_not_to_close(int fd)
{
  g_fd_not_to_close_set = 1;
  g_fd_not_to_close = fd;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void set_cloonix_name(char *name)
{
  if (name && (strlen(name) > 1))
    {
    memset(g_cloonix_name, 0, MAX_NAME_LEN);
    strncpy(g_cloonix_name, name, MAX_NAME_LEN-1);
    g_cloonix_name_set = 1;
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *get_cloonix_name(void)
{
  return (g_cloonix_name);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
unsigned long *get_clownix_malloc_nb(void)
{
  return clownix_malloc_nb;
}
/*---------------------------------------------------------------------------*/

int g_i_am_a_clone = 0;
int g_i_am_a_clone_no_kerr = 0;
/*****************************************************************************/
int i_am_a_clone(void)
{
  return g_i_am_a_clone;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int i_am_a_clone_no_kerr(void)
{
  return g_i_am_a_clone_no_kerr;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
long long  *get_peak_queue_len(int cidx)
{
  return(&(peak_queue_len[cidx]));
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
t_data_channel *get_dchan(int cidx)
{
  return (&(dchan[cidx]));
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *get_bigbuf(void)
{
  return g_bigsndbuf;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
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
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *clownix_strdup(char *str, int id)
{
    char *ptr;
    size_t len = strlen(str);
    ptr = clownix_malloc(len + 1, id);
    memcpy(ptr, str, len + 1);
    return ptr;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
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
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_differed_clownix_free(void *data)
{
  clownix_free(data, "differed_clownix_free");
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void differed_clownix_free(void *data)
{
  clownix_timeout_add(1, timer_differed_clownix_free, data, NULL, NULL);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
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
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void int_to_ip_string (int addr, char *ip_string)
{
  sprintf(ip_string, "%d.%d.%d.%d", (int) ((addr >> 24) & 0xff),
          (int)((addr >> 16) & 0xff), (int)((addr >> 8) & 0xff),
          (int) (addr & 0xff));
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int get_nb_mask_ip( char *ip_string)
{
  int result = 0;
  unsigned long  mask = 0;
  unsigned int part[4];
  if (strlen(ip_string) == 0)
    KOUT(" ");
  if (( sscanf(ip_string,"%u.%u.%u.%u",&part[0],&part[1],&part[2],&part[3])==4)
       &&(part[0]<=0xFF)&&(part[1]<=0xFF)&&(part[2]<=0xFF)&&(part[3]<=0xFF))
    {
    mask += part[0]<<24;
    mask += part[1]<<16;
    mask += part[2]<<8;
    mask += part[3];
    }
  while( mask != 0 )
    {
    result += ( mask & 1L );
    mask >>= 1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
unsigned long msg_get_tx_peak_queue_len(int llid)
{
  int cidx;
  long long result = 0;
  if (msg_exist_channel(llid))
    {
    cidx = channel_check_llid(llid, __FUNCTION__);
    result = peak_queue_len[cidx];
    peak_queue_len[cidx] = 0;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void default_err_kill(int llid, int err, int from)
{
  KOUT("%d %d %d\n", llid, err, from);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void default_rx_callback(int llid, int len, char *str_rx) 
{
  KOUT("%d %d", llid, len);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void err_dchan_cb(int llid, int err, int from)
{
  int cidx;
  if (msg_exist_channel(llid))
    {
    cidx = channel_check_llid(llid, __FUNCTION__);
    if (!dchan[cidx].rx_callback)
      KOUT("%d %d\n", err, from);
    if (!dchan[cidx].error_callback)
      KOUT("%d %d\n", err, from);
    dchan[cidx].error_callback(llid, err, from);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int rx_dchan_cb(int llid, int fd)
{
  int len, cidx, correct_recv;
  cidx = channel_check_llid(llid, __FUNCTION__);
  if (!llid || (dchan[cidx].llid != llid))
    KOUT(" ");
  if (dchan[cidx].fd != fd)
    KOUT(" ");
  first_rx_buf = (char *)clownix_malloc(first_rx_buf_max+1, IOCMLC);
/*
  if (kind == kind_server_proxy_traf_inet) 
    len = util_read_brakes_on(first_rx_buf, first_rx_buf_max, 
                              get_fd_with_cidx(cidx), llid);
  else
*/
  len = util_read(first_rx_buf, first_rx_buf_max, get_fd_with_cidx(cidx));
  if (len < 0)
    {
    clownix_free(first_rx_buf, __FUNCTION__);
    if (errno != EOPNOTSUPP)
      err_dchan_cb(llid, errno, 1132);
    correct_recv = 0;
    }
  else
    {
    correct_recv = len;
    first_rx_buf[len] = 0;
    if ((dchan[cidx].decoding_state == rx_type_proxy_traf_unix_start) ||
        (dchan[cidx].decoding_state == rx_type_proxy_traf_tcp_start))
      {
      dchan[cidx].rx_callback(llid, len, first_rx_buf);
      clownix_free(first_rx_buf, __FUNCTION__);
      }
    else
      {
      new_rx_to_process(&(dchan[cidx]), len, first_rx_buf);
      }
    }
  return correct_recv;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int tx_dchan_cb(int llid, int fd)
{
  int cidx, result, correct_send, total_correct_send = 0;
  cidx = channel_check_llid(llid, __FUNCTION__);
  if (dchan[cidx].llid !=  llid)
    KOUT(" ");
  if (dchan[cidx].fd !=  fd)
    KOUT(" ");
  if (channel_get_tx_queue_len(llid))
    {
    if (!dchan[cidx].tx)
      KOUT(" ");
    do
      {
      result = tx_try_send_chunk(&(dchan[cidx]), cidx, &correct_send, 
               err_dchan_cb);
      total_correct_send += correct_send;
      } while (result == 1);
    }
  return total_correct_send;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int server_has_new_connect_from_client(int id, int fd) 
{
  int fd_new, llid, cidx, serv_cidx;
  char *little_name;
  serv_cidx = channel_check_llid(id, __FUNCTION__);
  util_fd_accept(fd, &fd_new, __FUNCTION__);
  if (fd_new >= 0)
    {
    little_name = channel_get_little_name(id);
    llid = channel_create(fd_new, kind_server, little_name, rx_dchan_cb, 
                          tx_dchan_cb, err_dchan_cb);
    if (!llid)
      KOUT(" ");
    cidx = channel_check_llid(llid, __FUNCTION__);
    if (!dchan[serv_cidx].server_connect_callback)
      KOUT(" ");
    memset(&dchan[cidx], 0, sizeof(t_data_channel));
    dchan[cidx].decoding_state = rx_type_ascii_start;
    dchan[cidx].llid = llid;
    dchan[cidx].fd = fd_new;
    dchan[serv_cidx].server_connect_callback(id, llid);
    }
  return 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void msg_delete_channel(int llid)
{
  int cidx;
  cidx = channel_check_llid(llid, __FUNCTION__);
  channel_delete(llid);
  chain_delete(&(dchan[cidx].tx), &(dchan[cidx].last_tx));
  chunk_chain_delete(&(dchan[cidx]));
  memset(&(dchan[cidx]), 0, sizeof(t_data_channel));
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void msg_mngt_heartbeat_ms_set (int heartbeat_ms)
{
 channel_heartbeat_ms_set (heartbeat_ms);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void msg_mngt_heartbeat_init (t_heartbeat_cb heartbeat)
{
  channel_beat_set (heartbeat);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int msg_mngt_get_tx_queue_len(int llid)
{
  return (channel_get_tx_queue_len(llid));
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
int msg_watch_fd(int fd, t_fd_event rx_data,
                 t_fd_error err,  char *little_name)
{
  int llid, cidx;
  if (fd < 0)
    KOUT(" ");
  if (!err)
    KOUT(" ");
  llid = channel_create(fd, kind_simple_watch, little_name, rx_data, 
                        tx_dchan_cb, err_dchan_cb);
  if (llid)
    {
    cidx = channel_check_llid(llid, __FUNCTION__);
    memset(&dchan[cidx], 0, sizeof(t_data_channel));
    dchan[cidx].decoding_state = rx_type_watch;
    dchan[cidx].rx_callback = default_rx_callback;
    dchan[cidx].error_callback = err;
    dchan[cidx].llid = llid;
    dchan[cidx].fd = fd;
    }
  return (llid);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void proxy_traf_tx(int llid, int len, char *str_tx, int is_unix)
{     
  char *ntx;
  int cidx;
  if ((len<=0) || (len > MAX_TOT_LEN_QSIG))
    KOUT("ERROR %d", len);
  cidx = channel_check_llid(llid, __FUNCTION__);
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
/*---------------------------------------------------------------------------*/

  
/*****************************************************************************/
void proxy_traf_unix_tx(int llid, int len, char *str_tx)
{
  proxy_traf_tx(llid, len, str_tx, 1);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void proxy_traf_tcp_tx(int llid, int len, char *str_tx)
{
  proxy_traf_tx(llid, len, str_tx, 0);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void proxy_sig_tx(int llid, int len, char *str_tx)
{
  char *ntx;
  int cidx;
  if ((len<=0) || (len > MAX_TOT_LEN_PROXY))
    KOUT("ERROR %d", len);
  cidx = channel_check_llid(llid, __FUNCTION__);
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
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void proxy_traf_server_connect_from_client(int id, int fd, int is_unix)
{
  int  kind, fd_new, llid, cidx, serv_cidx;
  if (is_unix)
    kind = kind_server_proxy_traf_unix;
  else 
    kind = kind_server_proxy_traf_inet;
  serv_cidx = channel_check_llid(id, __FUNCTION__);
  util_fd_accept(fd, &fd_new, __FUNCTION__);
  if (fd_new >= 0)
    {
    llid = channel_create(fd_new, kind, "proxy", rx_dchan_cb,
                          tx_dchan_cb, err_dchan_cb);
    if (!llid)
      KOUT(" ");
    channel_rx_local_flow_ctrl(llid, 1);
    cidx = channel_check_llid(llid, __FUNCTION__);
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
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int proxy_traf_unix_server_connect_from_client(int id, int fd)
{
  proxy_traf_server_connect_from_client(id, fd, 1);
  return 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int proxy_traf_tcp_server_connect_from_client(int id, int fd)
{ 
  proxy_traf_server_connect_from_client(id, fd, 0);
  return 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int proxy_sig_server_connect_from_client(int id, int fd)
{   
  int fd_new, llid, cidx, serv_cidx;
  serv_cidx = channel_check_llid(id, __FUNCTION__);
  util_fd_accept(fd, &fd_new, __FUNCTION__);
  if (fd_new >= 0)
    {
    llid = channel_create(fd_new, kind_server_proxy_sig, "proxy", rx_dchan_cb,
                          tx_dchan_cb, err_dchan_cb);
    if (!llid)
      KOUT(" ");
    cidx = channel_check_llid(llid, __FUNCTION__);
    if (!dchan[serv_cidx].server_connect_callback)
      KOUT(" ");
    memset(&dchan[cidx], 0, sizeof(t_data_channel));
    dchan[cidx].decoding_state = rx_type_proxy_sig_start;
    dchan[cidx].llid = llid;
    dchan[cidx].fd = fd_new;
    dchan[serv_cidx].server_connect_callback(id, llid);
    }
  return 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
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
      KOUT(" ");
    if (!rx_cb)
      KOUT(" ");
    llid = channel_create(fd, kind_client, "proxy", rx_dchan_cb,
                          tx_dchan_cb, err_dchan_cb);
    if (!llid)
      KOUT(" ");
    cidx = channel_check_llid(llid, __FUNCTION__);
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
  return (llid);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int proxy_traf_unix_client(char *pname, t_fd_error err_cb, t_msg_rx_cb rx_cb)
{
  return proxy_traf_client(pname, 0, 0, err_cb, rx_cb, 1);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int proxy_traf_tcp_client(uint32_t ip, uint16_t port,
                          t_fd_error err_cb, t_msg_rx_cb rx_cb)
{
  return proxy_traf_client(NULL, ip, port, err_cb, rx_cb, 0);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int proxy_sig_client(char *pname, t_fd_error err_cb, t_msg_rx_cb rx_cb)
{ 
  int fd, llid=0, cidx;
  if (!util_client_socket_unix(pname, &fd))
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
    memset(&dchan[cidx], 0, sizeof(t_data_channel));
    dchan[cidx].decoding_state = rx_type_proxy_sig_start;
    dchan[cidx].llid = llid;
    dchan[cidx].fd = fd;
    dchan[cidx].error_callback = err_cb;
    dchan[cidx].rx_callback = rx_cb;
    }
  return (llid);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
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
      KOUT(" ");
    cidx = channel_check_llid(llid, __FUNCTION__);
    memset(&dchan[cidx], 0, sizeof(t_data_channel));
    dchan[cidx].decoding_state = rx_type_listen;
    dchan[cidx].server_connect_callback = connect_cb;
    }
  return llid;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int proxy_traf_unix_server(char *pname, t_fd_connect connect_cb)
{
  return proxy_traf_server(pname, 0, connect_cb, 1);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int proxy_traf_tcp_server(uint16_t port, t_fd_connect connect_cb)
{
  return proxy_traf_server(NULL, port, connect_cb, 0);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
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
    memset(&dchan[cidx], 0, sizeof(t_data_channel));
    dchan[cidx].decoding_state = rx_type_listen;
    dchan[cidx].server_connect_callback = connect_cb;
    }
  return llid;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int string_server_unix(char *pname, t_fd_connect connect_cb,
                        char *little_name)
{
  int llid=0, cidx, listen_fd;
  listen_fd = util_socket_listen_unix(pname);
  if (listen_fd >= 0)
    {
    llid = channel_create(listen_fd, kind_simple_watch, little_name,
                          server_has_new_connect_from_client,
                          NULL, default_err_kill);
    if (!llid)
      KOUT(" ");
    cidx = channel_check_llid(llid, __FUNCTION__);
    memset(&dchan[cidx], 0, sizeof(t_data_channel));
    dchan[cidx].decoding_state = rx_type_ascii_start;
    dchan[cidx].server_connect_callback = connect_cb;
    }
  return llid;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int string_server_inet(uint16_t port, t_fd_connect connect_cb, char *little_name)
{
  int llid = 0, cidx, listen_fd;
  listen_fd = util_socket_listen_inet(port);
  if (listen_fd >= 0)
    {
    llid = channel_create(listen_fd, kind_simple_watch, little_name,
                          server_has_new_connect_from_client,
                          NULL, default_err_kill);
    if (!llid)
      KOUT(" ");
    cidx = channel_check_llid(llid, __FUNCTION__);
    memset(&dchan[cidx], 0, sizeof(t_data_channel));
    dchan[cidx].decoding_state = rx_type_listen;
    dchan[cidx].is_tcp = 1;
    dchan[cidx].server_connect_callback = connect_cb;
    }
  return llid;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
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
    memset(&dchan[cidx], 0, sizeof(t_data_channel));
    dchan[cidx].decoding_state = rx_type_ascii_start;
    dchan[cidx].llid = llid;
    dchan[cidx].fd = fd;
    dchan[cidx].error_callback = err_cb;
    dchan[cidx].rx_callback = rx_cb;
    }
  return (llid);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int string_client_inet(uint32_t ip, uint16_t port, 
                       t_fd_error err_cb, t_msg_rx_cb rx_cb, 
                       char *little_name)
{
  int fd, llid=0, cidx;
  if (!util_client_socket_inet(ip, port, &fd))
    llid = channel_create(fd, kind_client, little_name, rx_dchan_cb, 
                          tx_dchan_cb, err_dchan_cb);
  if (llid)
    {
    if (!err_cb)
      KOUT(" ");
    if (!rx_cb)
      KOUT(" ");
    cidx = channel_check_llid(llid, __FUNCTION__);
    memset(&dchan[cidx], 0, sizeof(t_data_channel));
    dchan[cidx].decoding_state = rx_type_ascii_start;
    dchan[cidx].llid = llid;
    dchan[cidx].fd = fd;
    dchan[cidx].error_callback = err_cb;
    dchan[cidx].rx_callback = rx_cb;
    }
  return (llid);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void  msg_mngt_set_callbacks (int llid, t_fd_error err_cb, 
                              t_msg_rx_cb rx_cb)
{
  int cidx;
  cidx = channel_check_llid(llid, __FUNCTION__);
  if (dchan[cidx].llid !=  llid)
    KOUT(" ");
  if (!err_cb)
    KOUT(" ");
  if (!rx_cb)
    KOUT(" ");
  dchan[cidx].error_callback = err_cb;
  dchan[cidx].rx_callback = rx_cb;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void string_tx(int llid, int len, char *str_tx)
{
  char *ntx;
  int cidx;
  if ((len<=0) || (len > MAX_TOT_LEN_QSIG))
    KOUT("%d", len);
  if (msg_exist_channel(llid))
    {
    cidx = channel_check_llid(llid, __FUNCTION__);
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
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void ptr_string_tx(int llid, int len, char *str_tx)
{
  if (msg_exist_channel(llid))
    string_tx(llid, len, str_tx);
}

/*****************************************************************************/
void watch_tx(int llid, int len, char *str_tx)
{
  char *ntx;
  int cidx;
  if ((msg_exist_channel(llid)) && len)
    {
    if ((len<0) || (len > MAX_TOT_LEN_QDAT))
      KOUT("%d", len);
    cidx = channel_check_llid(llid, __FUNCTION__);
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
      KERR("%d %d", (int)dchan[cidx].tot_txq_size, MAX_TOT_LEN_QDAT);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int is_nonblock(int llid)
{
  int flags, fd;
  int cidx = channel_check_llid(llid, __FUNCTION__);
  fd = get_fd_with_cidx(cidx);
  flags = fcntl(fd, F_GETFL, 0);
  if (flags < 0)
    KOUT(" ");
  return (flags & O_NONBLOCK);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void string_tx_now(int llid, int len, char *str_tx)
{
  int fd, cidx, txlen, len_sent = 0, count = 0;
  cidx = channel_check_llid(llid, __FUNCTION__);
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
      err_dchan_cb(llid, errno, 131);
      msg_delete_channel(llid);
      KERR("%d %d %d", errno, txlen, len - len_sent);
      break;
      }
    else if (txlen == 0)
      {
      count++;
      if (count == 1000)
        {
        err_dchan_cb(llid, errno, 131);
        msg_delete_channel(llid);
        KERR("%d %d %d", errno, txlen, len - len_sent);
        break;
        }
      }
    fsync(fd);
    len_sent += txlen;
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void msg_mngt_loop(void)
{
  channel_loop(0);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void msg_mngt_loop_once(void)
{
  channel_loop(1);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void signal_pipe(int no_use)
{
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int msg_mngt_get_epfd(void)
{
  return channel_get_epfd();
}
/*---------------------------------------------------------------------------*/


/****************************************************************************/
static void full_write(int fd, const char *buf, size_t len)
{
  ssize_t ret;
  while (len > 0)
    {
    ret = write(fd, buf, len);
    if ((ret == -1) && (errno != EINTR))
      break;
    buf += (size_t) ret;
    len -= (size_t) ret;
    }
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static void sigsegv_handler(int sig)
{ 
  char start[] = "BACKTRACE ------------\n";
  char end[] = "addr2line -f -C -i -e <execfile> +0x<XXX>\n";
  void *bt[1024];
  int i, bt_size;
  char **bt_syms;
  int fd = open("/tmp/cloonix_sigsegv_backtrace",O_WRONLY|O_CREAT|O_TRUNC,0644);
  size_t len;
  bt_size = backtrace(bt, 1024);
  bt_syms = backtrace_symbols(bt, bt_size);
  full_write(fd, start, strlen(start));
  for (i = 1; i < bt_size; i++)
    {
    len = strlen(bt_syms[i]);
    full_write(fd, bt_syms[i], len);
    full_write(fd, "\n", 1);
    }
  full_write(fd, end, strlen(end));
  free(bt_syms);
  close(fd);
  exit(1);
}
/*--------------------------------------------------------------------------*/


/*****************************************************************************/
void msg_mngt_init (char *name, int max_len_per_read)
{
  struct sigaction act;

  signal(SIGSEGV, sigsegv_handler);

  cloonix_conf_linker_helper();
  doorways_linker_helper();
  memset (peak_queue_len, 0, MAX_SELECT_CHANNELS*sizeof(long long));
  memset (clownix_malloc_nb, 0, MAX_MALLOC_TYPES*sizeof(unsigned long));
  if (g_cloonix_name_set == 0)
    strcpy(g_cloonix_name, " ");
  if (g_fd_not_to_close_set == 0)
    g_fd_not_to_close = -1;
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
/*---------------------------------------------------------------------------*/

