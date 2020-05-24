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
#include <signal.h>
#include <sys/ioctl.h>
#include <asm/types.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>


#include "io_clownix.h"
#include "channel.h"
#include "util_sock.h"
#include "msg_layer.h"
#include "chunk.h"
#include "doorways_sock.h"

#define MAX_TOT_LEN_QSIG 10000000
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
void check_is_clownix_malloc(void *ptr, int len, const char *caller_ident)
{
  unsigned long *tmp;
  int ident;
  unsigned long chk_len = 0;
  if (len)
    chk_len = ((unsigned long) len)/sizeof(unsigned long) + 1;
  if (ptr)
    {
    tmp = (unsigned long *) ((char *)ptr - 4 * sizeof(unsigned long));
    if (tmp[0] != 0xABCD)
      KOUT("%s %08lX", caller_ident, tmp[0]);
    ident = tmp[1];
    if (ident <= 0)
      KOUT("%s", caller_ident);
    if (ident >= MAX_MALLOC_TYPES)
      KOUT("%s", caller_ident);
    if (tmp[tmp[2]+4] != 0xCAFEDECA) 
      KOUT("%s, %d", caller_ident, ident);
    if (tmp[tmp[2]+5] != 0xAACCCCBB) 
      KOUT("%s, %d", caller_ident, ident);
    if (chk_len)
      {
      if (tmp[2] != chk_len)
        KOUT("%s, %lu %lu", caller_ident, tmp[2], chk_len);
      }
    }
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
int ip_string_to_int (int *inet_addr, char *ip_string)
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
  int cidx, is_blkd;
  long long result = 0;
  if (msg_exist_channel(llid))
    {
    cidx = channel_check_llid(llid, &is_blkd, __FUNCTION__);
    if (!is_blkd)
      {
      result = peak_queue_len[cidx];
      peak_queue_len[cidx] = 0;
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void default_err_kill(void *ptr, int llid, int err, int from)
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
void err_dchan_cb(void *ptr, int llid, int err, int from)
{
  int cidx, is_blkd;
  if (msg_exist_channel(llid))
    {
    cidx = channel_check_llid(llid, &is_blkd, __FUNCTION__);
    if (is_blkd)
      KOUT(" ");
    if (!dchan[cidx].rx_callback)
      KOUT("%d %d\n", err, from);
    if (!dchan[cidx].error_callback)
      KOUT("%d %d\n", err, from);
    dchan[cidx].error_callback(ptr, llid, err, from);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int rx_dchan_cb(void *ptr, int llid, int fd)
{
  int len, cidx, is_blkd, correct_recv;
  cidx = channel_check_llid(llid, &is_blkd, __FUNCTION__);
  if (is_blkd)
    KOUT(" ");
  if (!llid || (dchan[cidx].llid != llid))
    KOUT(" ");
  if (dchan[cidx].fd != fd)
    KOUT(" ");
  first_rx_buf = (char *)clownix_malloc(first_rx_buf_max+1, IOCMLC);
  len = util_read (first_rx_buf, first_rx_buf_max, get_fd_with_cidx(cidx));
  if (len < 0)
    {
    clownix_free(first_rx_buf, __FUNCTION__);
    err_dchan_cb(NULL, llid, errno, 2);
    correct_recv = 0;
    }
  else
    {
    correct_recv = len;
    first_rx_buf[len] = 0;
    new_rx_to_process(&(dchan[cidx]), len, first_rx_buf);
    }
  return correct_recv;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
int tx_dchan_cb(void *ptr, int llid, int fd)
{
  int cidx, is_blkd, result, correct_send, total_correct_send = 0;
  cidx = channel_check_llid(llid, &is_blkd, __FUNCTION__);
  if (is_blkd)
    KOUT(" ");
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
static int server_has_new_connect_from_client(void *ptr, int id, int fd) 
{
  int fd_new, is_blkd, llid, cidx, serv_cidx;
  char *little_name;
  serv_cidx = channel_check_llid(id, &is_blkd, __FUNCTION__);
  if (is_blkd)
    KOUT(" ");
  util_fd_accept(fd, &fd_new, __FUNCTION__);
  if (fd_new >= 0)
    {
    little_name = channel_get_little_name(id);
    llid = channel_create(fd_new, 0, kind_server, little_name, rx_dchan_cb, 
                          tx_dchan_cb, err_dchan_cb);
    if (!llid)
      KOUT(" ");
    cidx = channel_check_llid(llid, &is_blkd, __FUNCTION__);
    if (is_blkd)
      KOUT(" ");
    if (!dchan[serv_cidx].server_connect_callback)
      KOUT(" ");
    memset(&dchan[cidx], 0, sizeof(t_data_channel));
    dchan[cidx].decoding_state = rx_type_ascii_start;
    dchan[cidx].llid = llid;
    dchan[cidx].fd = fd_new;
    dchan[serv_cidx].server_connect_callback(ptr, id, llid);
    }
  return 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void msg_delete_channel(int llid)
{
  int cidx, is_blkd;
  cidx = channel_check_llid(llid, &is_blkd, __FUNCTION__);
  channel_delete(llid);
  if (is_blkd)
    {
    if (blkd_delete(NULL, llid))
      KERR(" ");
    }
  else
    {
    chain_delete(&(dchan[cidx].tx), &(dchan[cidx].last_tx));
    chunk_chain_delete(&(dchan[cidx]));
    memset(&(dchan[cidx]), 0, sizeof(t_data_channel));
    }
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
  int llid, cidx, is_blkd;
  if (fd < 0)
    KOUT(" ");
  if (!err)
    KOUT(" ");
  llid = channel_create(fd, 0, kind_simple_watch, little_name, rx_data, 
                        tx_dchan_cb, err_dchan_cb);
  if (!llid)
    KOUT(" ");
  cidx = channel_check_llid(llid, &is_blkd, __FUNCTION__);
  if (is_blkd)
    KOUT(" ");
  memset(&dchan[cidx], 0, sizeof(t_data_channel));
  dchan[cidx].decoding_state = rx_type_watch;
  dchan[cidx].rx_callback = default_rx_callback;
  dchan[cidx].error_callback = err;
  dchan[cidx].llid = llid;
  dchan[cidx].fd = fd;
  return (llid);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int string_server_unix(char *pname, t_fd_connect connect_cb,
                        char *little_name)
{
  int llid=0, cidx, is_blkd, listen_fd;
  listen_fd = util_socket_listen_unix(pname);
  if (listen_fd >= 0)
    {
    llid = channel_create(listen_fd, 0, kind_simple_watch, little_name,
                          server_has_new_connect_from_client,
                          NULL, default_err_kill);
    if (!llid)
      KOUT(" ");
    cidx = channel_check_llid(llid, &is_blkd, __FUNCTION__);
    if (is_blkd)
      KOUT(" ");
    memset(&dchan[cidx], 0, sizeof(t_data_channel));
    dchan[cidx].decoding_state = rx_type_ascii_start;
    dchan[cidx].server_connect_callback = connect_cb;
    }
  return llid;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int string_server_inet(__u16 port, t_fd_connect connect_cb, char *little_name)
{
  int llid = 0, cidx, is_blkd, listen_fd;
  listen_fd = util_socket_listen_inet(port);
  if (listen_fd >= 0)
    {
    llid = channel_create(listen_fd, 0, kind_simple_watch, little_name,
                          server_has_new_connect_from_client,
                          NULL, default_err_kill);
    if (!llid)
      KOUT(" ");
    cidx = channel_check_llid(llid, &is_blkd, __FUNCTION__);
    if (is_blkd)
      KOUT(" ");
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
  int fd, llid=0, cidx, is_blkd;
  if (!util_client_socket_unix(pname, &fd))
    {
    if (!err_cb)
      KOUT(" ");
    if (!rx_cb)
      KOUT(" ");
    llid = channel_create(fd, 0, kind_client, little_name, rx_dchan_cb, 
                          tx_dchan_cb, err_dchan_cb);
    if (!llid)
      KOUT(" ");
    cidx = channel_check_llid(llid, &is_blkd, __FUNCTION__);
    if (is_blkd)
      KOUT(" ");
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
int string_client_inet(__u32 ip, __u16 port, 
                       t_fd_error err_cb, t_msg_rx_cb rx_cb, 
                       char *little_name)
{
  int fd, llid=0, cidx, is_blkd;
  if (!util_client_socket_inet(ip, port, &fd))
    llid = channel_create(fd, 0, kind_client, little_name, rx_dchan_cb, 
                          tx_dchan_cb, err_dchan_cb);
  if (llid)
    {
    if (!err_cb)
      KOUT(" ");
    if (!rx_cb)
      KOUT(" ");
    cidx = channel_check_llid(llid, &is_blkd, __FUNCTION__);
    if (is_blkd)
      KOUT(" ");
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
  int cidx, is_blkd;
  cidx = channel_check_llid(llid, &is_blkd, __FUNCTION__);
  if (is_blkd)
    KOUT(" ");
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
  int cidx, is_blkd;
  if ((len<=0) || (len > MAX_TOT_LEN_QSIG))
    KOUT("%d", len);
  cidx = channel_check_llid(llid, &is_blkd, __FUNCTION__);
  if (is_blkd)
    KOUT(" ");
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
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void ptr_string_tx(void *ptr, int llid, int len, char *str_tx)
{
  (void) ptr;
  if (msg_exist_channel(llid))
    string_tx(llid, len, str_tx);
}

/*****************************************************************************/
void watch_tx(int llid, int len, char *str_tx)
{
  char *ntx;
  int cidx, is_blkd;
  if (len)
    {
    if ((len<0) || (len > MAX_TOT_LEN_QDAT))
      KOUT("%d", len);
    cidx = channel_check_llid(llid, &is_blkd, __FUNCTION__);
    if (is_blkd)
      KOUT(" ");
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
  int flags, fd, is_blkd;
  int cidx = channel_check_llid(llid, &is_blkd, __FUNCTION__);
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
  int fd, cidx, is_blkd, txlen, len_sent = 0, count = 0;
  cidx = channel_check_llid(llid, &is_blkd, __FUNCTION__);
  if (is_blkd)
    KOUT(" ");
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
      err_dchan_cb(NULL, llid, errno, 131);
      msg_delete_channel(llid);
      KERR("%d %d %d", errno, txlen, len - len_sent);
      break;
      }
    else if (txlen == 0)
      {
      count++;
      if (count == 1000)
        {
        err_dchan_cb(NULL, llid, errno, 131);
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

/*****************************************************************************/
void msg_mngt_init (char *name, int max_len_per_read)
{
  struct sigaction act;
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
  blkd_init(NULL, channel_tx_local_flow_ctrl, 
                  channel_rx_local_flow_ctrl, 
                  rpct_send_peer_flow_control);
  rpct_init(NULL, ptr_string_tx);
} 
/*---------------------------------------------------------------------------*/

