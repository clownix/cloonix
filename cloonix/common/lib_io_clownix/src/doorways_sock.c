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
#ifndef NO_HMAC_CIPHER

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/sockios.h>
#include "io_clownix.h"
#include "channel.h"
#include "msg_layer.h"
#include "doorways_sock.h"
#include "util_sock.h"
#include "chunk.h"
#include "hmac_cipher.h"


#define MAX_TOT_LEN_WARNING_DOORWAYS_Q 100000000
#define MAX_TOT_LEN_DOORWAYS_Q         500000000
#define MAX_TOT_LEN_DOORWAYS_SOCK_Q     50000000

static int g_to_exit;

typedef struct t_rx_pktdoors
{
  char doors_bufraw[MAX_DOORWAYS_BUF_LEN];
  int  offset;
  int  idx_hmac;
  int  tid;
  int  paylen;
  int  head_doors_type;
  int  val;
  int  nb_pkt_rx;
  char *payload;
} t_rx_pktdoors;


/****************************************************************************/
typedef struct t_llid
{
  int  llid;
  int  register_listen_llid;
  int  fd;
  char fct[MAX_NAME_LEN];
  t_rx_pktdoors rx_pktdoors;
  int  doors_type;
  int  nb_pkt_tx;
  char passwd[MSG_DIGEST_LEN];
  t_doorways_llid cb_llid;
  t_doorways_end cb_end;
  t_doorways_rx cb_rx;
} t_llid;
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_llid *g_llid_data[CLOWNIX_MAX_CHANNELS];
static int g_max_tx_sock_queue_len_reached;
static int g_max_tx_doorway_queue_len_reached;
static int g_init_done;
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void fct_10_sec_timeout(void *data)
{
  if (g_max_tx_sock_queue_len_reached)
    {
    KERR("TX MAX SOCK LEN HIT: %d", g_max_tx_sock_queue_len_reached);
    g_max_tx_sock_queue_len_reached = 0;
    }
  if (g_max_tx_doorway_queue_len_reached)
    {
    KERR("DOORWAY MAX QUEUE LEN HIT: %d", g_max_tx_doorway_queue_len_reached);
    g_max_tx_doorway_queue_len_reached = 0;
    }

  clownix_timeout_add(1000, fct_10_sec_timeout, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void fct_stop_writing_timeout(void *data)
{
  unsigned long ul_llid = (unsigned long) data;
  channel_tx_local_flow_ctrl((int) ul_llid, 0);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int max_tx_sock_queue_len_reached(int llid, int cidx, int fd)
{
  t_llid *lid = g_llid_data[llid];
  int used, max_reached = 0;
  unsigned long ul_llid = llid;
  if (!lid)
    KERR(" ");
  else
    {
    if (ioctl(fd, SIOCOUTQ, &used))
      KERR(" ");
    else
      {
      if (used > MAX_TOT_LEN_DOORWAYS_SOCK_Q) 
        {
        KERR("%d %d", used, MAX_TOT_LEN_DOORWAYS_SOCK_Q);
        channel_tx_local_flow_ctrl(llid, 1);
        clownix_timeout_add(2, fct_stop_writing_timeout,
                            (void *) ul_llid,NULL,NULL);
        g_max_tx_sock_queue_len_reached += 1;
        max_reached = 1;
        }
      }
    }
  return max_reached;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int tst_port(char *str_port, int *port)
{
  int result = 0;
  unsigned long val;
  char *endptr;
  val = strtoul(str_port, &endptr, 10);
  if ((endptr == NULL)||(endptr[0] != 0))
    result = -1;
  else
    {
    if ((val < 1) || (val > 65535))
      result = -1;
    *port = (int) val;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int get_ip_and_port(char *doors, uint32_t *ip, int *port)
{
  int result;
  char *ptr_ip, *ptr_port;
  ptr_ip = doors;
  ptr_port = strchr(doors, ':');
  *ptr_port = 0;
  ptr_port++;
  if (ip_string_to_int (ip, ptr_ip))
    result = -1;
  else
    result = tst_port(ptr_port, port);
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void doorways_sock_address_detect(char *doors_client_addr,
                                  uint32_t *ip, int *port)
{
  char doors[MAX_PATH_LEN];
  struct stat stat_file;
  if (strlen(doors_client_addr) >= MAX_PATH_LEN)
    KOUT("LENGTH Problem");
  memset(doors, 0, MAX_PATH_LEN);
  *ip = 0;
  *port = 0;
  strncpy(doors, doors_client_addr, MAX_PATH_LEN-1);
  if (strchr(doors, ':'))
    {
    if (get_ip_and_port(doors, ip, port))
      KOUT("ERROR %s bad ip:port address", doors);
    }
  else if (!stat(doors_client_addr, &stat_file))
    {
    if (!S_ISSOCK(stat_file.st_mode))
      KOUT("ERROR %s not unix socket address", doors);
    }
  else
    KOUT("ERROR %s bad ip:port or not found unix socket address", doors);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void clean_llid(int llid)
{
  t_llid *lid = g_llid_data[llid];
  if (lid)
    {
    g_llid_data[llid] = NULL;
    if (!lid->cb_end)
      KOUT(" ");
    lid->cb_end(llid);
    clownix_free(lid, __FUNCTION__);
    }
  if (msg_exist_channel(llid))
    msg_delete_channel(llid);
  if (g_to_exit)
    exit(0);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void called_from_channel_free_llid(int llid)
{
  t_llid *lid = g_llid_data[llid];
  if (lid)
    {
    clean_llid(llid);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static t_llid *alloc_llid(int doors_type, int llid, int fd, char *fct)
{
  int cidx;
  t_data_channel *dchan;
  if (!doors_type)
    KOUT("%d %s", llid, fct);
  if (g_llid_data[llid])
    KOUT("%d %s %s", llid, g_llid_data[llid]->fct, fct);
  g_llid_data[llid] = (t_llid *) clownix_malloc(sizeof(t_llid), 9);
  memset(g_llid_data[llid], 0, sizeof(t_llid));
  g_llid_data[llid]->doors_type = doors_type;
  g_llid_data[llid]->llid = llid;
  g_llid_data[llid]->fd = fd;
  strncpy(g_llid_data[llid]->fct, fct, MAX_NAME_LEN - 1);
  cidx = channel_check_llid(llid, __FUNCTION__);
  dchan = get_dchan(cidx);
  memset(dchan, 0, sizeof(t_data_channel));
  dchan->decoding_state = rx_type_doorways;
  dchan->llid = llid;
  dchan->fd = fd;
  return (g_llid_data[llid]);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void set_hmac_password(int i, char *tx, int len, char *payload)
{
  int j, idx = i;
  char *md = compute_msg_digest(len, payload);
  for (j=0; j<MSG_DIGEST_LEN; j++)
    tx[idx++] = md[j];  
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int check_hmac_password(int i, char *rx, int len, char *payload)
{
  int j, k, idx, result = 0;
  char *md = compute_msg_digest(len, payload);
  idx = i;
  for (j=0; j<MSG_DIGEST_LEN; j++)
    {
    k = idx++;
    if (rx[k] != md[j]) 
      {
      result = -1;
      break;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void sock_header_set_info(char *tx,
                                 int llid, int len, int type, int val,
                                 int nb_pkt, char **ntx)
{
  int idx_hmac, i = 0;

  tx[i++] = 0xCA & 0xFF;
  tx[i++] = 0xFE & 0xFF;
  
  tx[i++] = ((llid & 0xFF00) >> 8) & 0xFF;
  tx[i++] = llid & 0xFF;
  tx[i++] = ((len & 0xFF000000) >> 24) & 0xFF;
  tx[i++] = ((len & 0xFF0000) >> 16) & 0xFF;
  tx[i++] = ((len & 0xFF00) >> 8) & 0xFF;
  tx[i++] = len & 0xFF;
  tx[i++] = ((type & 0xFF00) >> 8) & 0xFF;
  tx[i++] = type & 0xFF;
  tx[i++] = ((val & 0xFF00) >> 8) & 0xFF;
  tx[i++] = val & 0xFF;
  tx[i++] = ((nb_pkt & 0xFF00) >> 8) & 0xFF;
  tx[i++] = nb_pkt & 0xFF;
  idx_hmac = i;
  i += MSG_DIGEST_LEN;
  tx[i++] = 0xDE & 0xFF;
  tx[i++] = 0xCA & 0xFF;

  *ntx = &(tx[i++]);
  set_hmac_password(idx_hmac, tx, len, *ntx);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int sock_header_get_info(char *rx,
                                 int *llid, int *len, int *type, int *val,
                                 int *nb_pkt, char **nrx)
{
  int idx_hmac, i = 0, result=0;
  if ((rx[i++] & 0xFF) != 0xCA)
    {
    for (i=0; i<16; i++)
      printf(" %02X", (rx[i] & 0xFF));
    KERR("%02X \n", rx[0]& 0xFF);
    result = -1; 
    }
  else if ((rx[i++] & 0xFF) != 0xFE)
    {
    KERR("%02X \n", rx[1]);
    result = -1; 
    }
  else
    {
    *llid = ((rx[i] & 0xFF) << 8) + (rx[i+1] & 0xFF);
    i += 2;
    *len  = ((rx[i] & 0xFF) << 24) + ((rx[i+1] & 0xFF) << 16);
    i += 2;
    *len  += ((rx[i] & 0xFF) << 8) + (rx[i+1] & 0xFF);
    i += 2;
    *type = ((rx[i] & 0xFF) << 8) + (rx[i+1] & 0xFF);
    i += 2;
    *val  = ((rx[i] & 0xFF) << 8) + (rx[i+1] & 0xFF);
    i += 2;
    *nb_pkt  = ((rx[i] & 0xFF) << 8) + (rx[i+1] & 0xFF);
    i += 2;
    idx_hmac = i;
    i += MSG_DIGEST_LEN;
    if ((rx[i++] & 0xFF) != 0xDE)
      {
      KERR("%02X \n", rx[i-1]);
      result = -1; 
      }
    else if ((rx[i++] & 0xFF) != 0xCA)
      {
      KERR("%02X \n", rx[i-1]);
      result = -1; 
      }
    else
      {
      result = idx_hmac;
      *nrx  = &(rx[i++]);
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void err_listen_cb(int llid, int err, int from)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void err_cb(int llid, int err, int from)
{
  clean_llid(llid);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void cli_err_cb(int llid, int err, int from)
{
  clean_llid(llid);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void tx_err_cb(int llid, int err, int from)
{
  clean_llid(llid);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int tx_write(char *msg, int len, int fd)
{
  int tx_len = 0;
  tx_len = write(fd, (unsigned char *) msg, len);
  if (tx_len < 0)
    {
    if ((errno == EAGAIN) || (errno == EINTR) || (errno == EINPROGRESS))
      tx_len = 0;
    }
  return tx_len;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int doors_tx_send_chunk(t_data_channel *dchan, int cidx, 
                               int *correct_send, t_fd_error err_cb)
{
  int len, fstr_len, result;
  char *fstr;
  fstr_len = dchan->tx->len - dchan->tx->len_done;
  fstr = dchan->tx->chunk + dchan->tx->len_done;
  len = tx_write(fstr, fstr_len, get_fd_with_cidx(cidx));
  *correct_send = 0;
  if (len > 0)
    {
    *correct_send = len;
    if (dchan->tot_txq_size < (unsigned int) len)
      KOUT("%lu %d", dchan->tot_txq_size, len);
    dchan->tot_txq_size -= len;
    if (len == fstr_len)
      {
      first_elem_delete(&(dchan->tx), &(dchan->last_tx));
      if (dchan->tx)
        result = 1;
      else
        result = 0;
      }
    else
      {
      dchan->tx->len_done += len;
      result = 2;
      }
    }
  else if (len == 0)
    {
    result = 0;
    }
  else if (len < 0)
    {
    KERR("%d", errno);
    if (errno != EPIPE)
      KERR("%d", errno);
    dchan->tot_txq_size = 0;
    chain_delete(&(dchan->tx), &(dchan->last_tx));
    if (!err_cb)
      KOUT(" ");
    err_cb(dchan->llid, errno, 3);
    result = -1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/



/*****************************************************************************/
static int tx_cb(int llid, int fd)
{
  int cidx, result, correct_send, total_correct_send = 0;
  t_data_channel *dchan;
  t_llid *lid = g_llid_data[llid];
  if (!lid)
    KERR("ERROR");
  else
    {
    if (lid->fd != fd)
      KOUT("%d %d", lid->fd, fd);
    cidx = channel_check_llid(llid, __FUNCTION__);
    if (cidx == 0)
      KERR("ERROR");
    else
      {
      dchan = get_dchan(cidx);
      if (dchan->llid !=  llid)
        KOUT("%d %d %d %d %d", cidx, llid,  fd, dchan->llid, dchan->fd);
      if (dchan->fd !=  fd)
        KOUT(" ");
      if (channel_get_tx_queue_len(llid))
        {
        if (!dchan->tx)
          KOUT(" ");
        do
          {
          if (max_tx_sock_queue_len_reached(llid, cidx, fd))
            result = 0;
          else
            {
            result = doors_tx_send_chunk(dchan, cidx, &correct_send, tx_err_cb);
            total_correct_send += correct_send;
            }
          } while (result == 1);
        }
      }
    }
  return total_correct_send;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static int rx_pktdoors_fill(int *len, char  *buf, t_rx_pktdoors *rx_pktdoors)
{
  int headsize = doorways_header_size();
  int result, len_chosen, len_desired, len_avail = *len;
  if (rx_pktdoors->offset < headsize)
    {
    len_desired = headsize - rx_pktdoors->offset;
    if (len_avail >= len_desired)
      {
      len_chosen = len_desired;
      result = 1;
      }
    else
      {
      len_chosen = len_avail;
      result = 2;
      }
    }
  else
    {
    if (rx_pktdoors->paylen <= 0)
      KOUT(" ");
    len_desired = headsize + rx_pktdoors->paylen - rx_pktdoors->offset;
    if (len_avail >= len_desired)
      {
      len_chosen = len_desired;
      result = 3;
      }
    else
      {
      len_chosen = len_avail;
      result = 2;
      }
    }
  if (len_chosen + rx_pktdoors->offset > MAX_DOORWAYS_BUF_LEN)
    KOUT("%d %d", len_chosen, rx_pktdoors->offset);
  memcpy(rx_pktdoors->doors_bufraw+rx_pktdoors->offset, buf, len_chosen);
  rx_pktdoors->offset += len_chosen;
  *len -= len_chosen;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int rx_pktdoors_get_paylen(t_rx_pktdoors *rx_pktdoors)
{
  int nb_pkt, result = 0;
  rx_pktdoors->idx_hmac = sock_header_get_info(rx_pktdoors->doors_bufraw, 
                                             &(rx_pktdoors->tid),
                                             &(rx_pktdoors->paylen),
                                             &(rx_pktdoors->head_doors_type),
                                             &(rx_pktdoors->val), 
                                             &(nb_pkt),
                                             &(rx_pktdoors->payload));
  if (rx_pktdoors->idx_hmac == -1)
    {
    result = -1;
    rx_pktdoors->offset = 0;
    rx_pktdoors->paylen = 0;
    rx_pktdoors->payload = NULL;
    rx_pktdoors->tid = 0;
    rx_pktdoors->head_doors_type = 0;
    rx_pktdoors->val = 0;
    }
  else
    {
    if ((nb_pkt-1) != rx_pktdoors->nb_pkt_rx)
      {
      KERR("%d %d", nb_pkt, rx_pktdoors->nb_pkt_rx);
      result = -1;
      }
    if (nb_pkt == 0xFFFF)
      rx_pktdoors->nb_pkt_rx = 0;
    else
      rx_pktdoors->nb_pkt_rx = nb_pkt;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int rx_pktdoors_process(t_llid *lid, t_rx_pktdoors *rx_pktdoors)
{
  int headsize = doorways_header_size();
  int result = 0;
  if (!lid->cb_rx)
    KOUT(" ");
  if (rx_pktdoors->idx_hmac != 14)
    KOUT("%d", rx_pktdoors->idx_hmac);
  if (!rx_pktdoors->payload)
    KOUT(" ");
  cipher_change_key(lid->passwd);
  if (check_hmac_password(rx_pktdoors->idx_hmac, rx_pktdoors->doors_bufraw,
                          rx_pktdoors->paylen, rx_pktdoors->payload))
    {
    KERR("BAD PASSWORD");
    result = -1;
    }
  else
    {
    lid->cb_rx(lid->llid, rx_pktdoors->tid,
               headsize + rx_pktdoors->paylen, rx_pktdoors->doors_bufraw, 
               rx_pktdoors->head_doors_type, rx_pktdoors->val,
               rx_pktdoors->paylen, rx_pktdoors->payload);
    }
  rx_pktdoors->offset = 0;
  rx_pktdoors->paylen = 0;
  rx_pktdoors->payload = NULL;
  rx_pktdoors->tid = 0;
  rx_pktdoors->head_doors_type = 0;
  rx_pktdoors->val = 0;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int rx_doorways(t_llid *lid, int len, char *buf)
{
  int result = 0;
  int res, len_done, len_left_to_do = len;
  while (len_left_to_do)
    {
    len_done = len - len_left_to_do;
    res = rx_pktdoors_fill(&len_left_to_do, buf + len_done, &(lid->rx_pktdoors));
    if (res == 1)
      {
      if (rx_pktdoors_get_paylen(&(lid->rx_pktdoors)))
        {
        result = -1;
        break;
        }
      }
    else if (res == 2)
      {
      }
    else if (res == 3)
      {
      if(rx_pktdoors_process(lid, &(lid->rx_pktdoors)))
        {
        result = -1;
        break;
        }
      }
    else
      KOUT("%d", res);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int rx_cb(int llid, int fd)
{
  int result = -1;
  char buf[MAX_DOORWAYS_BUF_LEN];
  t_llid *lid = g_llid_data[llid];
  if (!lid)
    KERR("ERROR llid %d %d", llid, fd);
  else
    {
    if (lid->fd != fd)
      KOUT("%d %d", lid->fd, fd);
    result = util_read(buf, MAX_DOORWAYS_BUF_LEN, fd);
    if (result < 0)
      {
      result = 0;
      clean_llid(llid);
      }
    else 
      {
      if (rx_doorways(lid, result, buf))
        {
        KERR("WARNING %d", result);
        clean_llid(llid);
        }
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int server_new_connect_from_client(int id, int fd)
{
  int fd_new, llid;
  char *little_name;
  t_llid *listen_lid = g_llid_data[id];
  t_llid *lid;
  if (!listen_lid)
    KERR(" ");
  else
    {
    if (listen_lid->fd != fd)
      KOUT("%d %d", listen_lid->fd, fd);
    util_fd_accept(fd, &fd_new, __FUNCTION__);
    if (fd_new > 0)
      {
      little_name = channel_get_little_name(id);
      llid = channel_create(fd_new, kind_server_doors, little_name, rx_cb,
                            tx_cb, cli_err_cb);
      if (!llid)
        KOUT(" ");
      lid = alloc_llid(doors_type_server, llid, fd_new, (char *)__FUNCTION__);
      lid->cb_end = listen_lid->cb_end;
      lid->cb_rx = listen_lid->cb_rx;
      strncpy(lid->passwd, listen_lid->passwd, MSG_DIGEST_LEN-1);
      if (!listen_lid->cb_llid)
        KOUT(" ");
      listen_lid->cb_llid(listen_lid->register_listen_llid, llid);
      }
    }
  return 0;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
int doorways_header_size(void)
{
  return 16 + MSG_DIGEST_LEN ;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int register_listen_fd(int fd, char *passwd, t_doorways_llid cb_llid,
                              t_doorways_end cb_end, t_doorways_rx cb_rx)
{
  int llid;
  const char *fct = __FUNCTION__;
  t_llid *listen_lid;
  llid = channel_create(fd, kind_simple_watch, "doorway_serv",
                        server_new_connect_from_client, NULL, err_listen_cb);
  if (!llid)
    KOUT("ERROR");
  listen_lid = alloc_llid(doors_type_listen_server, llid, fd, (char *) fct);
  listen_lid->register_listen_llid = llid;
  listen_lid->cb_llid = cb_llid;
  listen_lid->cb_end = cb_end;
  listen_lid->cb_rx = cb_rx;
  strncpy(listen_lid->passwd, passwd, MSG_DIGEST_LEN-1);
  return llid;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int doorways_sock_server_proxy(char *net_name, char *unix_path, char *passwd,
                               t_doorways_llid cb_llid, t_doorways_end cb_end,
                               t_doorways_rx cb_rx)
{
  int listen_fd, listen_llid = 0;
  if (g_init_done != 777)
    KOUT(" ");
  listen_fd = util_socket_listen_unix(unix_path);
  if (listen_fd < 0)
    KERR("ERROR");
  else
    listen_llid = register_listen_fd(listen_fd,passwd,cb_llid,cb_end,cb_rx);
  return listen_llid;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void doorways_sock_server(char *net_name, int port, char *passwd, 
                         t_doorways_llid cb_llid, t_doorways_end cb_end,
                         t_doorways_rx cb_rx)
{
  int listen_fd;
  char unix_path[MAX_PATH_LEN];
  if (g_init_done != 777)
    KOUT(" ");
  memset(unix_path, 0, MAX_PATH_LEN);
  snprintf(unix_path, MAX_PATH_LEN-1, "%s_%s/proxy_pmain.sock",
                                      PROXYSHARE_IN, net_name);
  listen_fd = util_socket_listen_unix(unix_path);
  if (listen_fd < 0)
    KERR("ERROR");
  else
    register_listen_fd(listen_fd, passwd, cb_llid, cb_end, cb_rx);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void doorways_connect_error(int llid, int err, int from)
{
  int fd;
  if (msg_exist_channel(llid))
    {
    fd = get_fd_with_llid(llid);
    channel_delete(llid);
    if (fd != -1)
      close(fd);
    }
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
int doorways_sock_client_unix_start(char *sock_path, t_fd_event conn_rx)
{
  int fd, llid = 0;
  if (g_init_done != 777)
    KOUT("ERROR");
  if (!util_nonblock_client_socket_unix(sock_path, &fd))
    {
    llid = channel_create(fd, kind_simple_watch_connect, "connect_wait",
                          conn_rx, conn_rx, doorways_connect_error);
    if (!llid)
      KOUT("ERROR");
    }
  return llid;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
int doorways_sock_client_inet_start(uint32_t ip, int port, t_fd_event conn_rx)
{
  int fd, llid = 0;
  if (g_init_done != 777)
    KOUT("ERROR");
  fd = util_nonblock_client_socket_inet(ip, port);
  if (fd != -1)
    {
    llid = channel_create(fd, kind_simple_watch_connect, "connect_wait",
                          conn_rx, conn_rx, doorways_connect_error);
    if (!llid)
      KOUT("ERROR %X %d", ip, port);
    }
  return llid;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void doorways_sock_client_inet_delete(int llid)
{
  if (msg_exist_channel(llid))
    channel_delete(llid);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
int doorways_sock_client_inet_end(int type, int llid, int fd, 
                                  char *passwd,
                                  t_doorways_end cb_end,
                                  t_doorways_rx cb_rx)
{
  t_llid *lid;
  int err = 0, result = 0;
  unsigned int len = sizeof(err);

  if (!msg_exist_channel(llid))
    KERR("ERROR %d %d", llid, fd);
  else
    {
    channel_delete(llid); 
    if (!getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len))
      {
      if (err == 0)
        {
        result = channel_create(fd, kind_client, "doorways_cli", rx_cb,
                                tx_cb, err_cb);
        if (!result)
          KOUT("ERROR");
        lid = alloc_llid(type, result, fd, (char *) __FUNCTION__);
        lid->cb_end = cb_end;
        lid->cb_rx = cb_rx;
        strncpy(lid->passwd, passwd, MSG_DIGEST_LEN-1);
        }
      else
        {
        KERR("ERROR %d %d", llid, fd);
        util_free_fd(fd);
        }
      }
    else
      {
      KERR("ERROR %d %d", llid, fd);
      util_free_fd(fd);
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
int doorways_sock_client_inet_end_glib(int type, int fd, char *passwd, 
                                        t_doorways_end cb_end,
                                        t_doorways_rx cb_rx,
                                        t_fd_event *rx_glib,
                                        t_fd_event *tx_glib) 
{
  t_llid *lid; 
  int err = 0, result = 0;
  unsigned int len = sizeof(err);
  if (!getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len))
    {
    if (err == 0)
      {
      result = channel_create(fd, kind_glib_managed, "glib",
                              NULL, NULL, err_cb);
      if (!result)
        KOUT(" ");
      lid = alloc_llid(type, result, fd, (char *) __FUNCTION__);
      lid->cb_end = cb_end;
      lid->cb_rx = cb_rx;
      *rx_glib = rx_cb;
      *tx_glib = tx_cb;
      strncpy(lid->passwd, passwd, MSG_DIGEST_LEN-1);
      }
    else
      {
      KERR("%d %d", fd, err);
      util_free_fd(fd);
      }
    }
  else
    {
    KERR("%d", fd);
    util_free_fd(fd);
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void doorways_tx_append(t_llid *lid, int cidx, t_data_channel *dchan,
                               int tid, int type, int val, int len, char *buf)
{
  char *payload;
  int headsize = doorways_header_size();
  int tot_len = len + headsize;
  long long *peak_queue_len;
  if (lid->nb_pkt_tx == 0xFFFF) 
    lid->nb_pkt_tx = 1; 
  else
    lid->nb_pkt_tx += 1; 
  cipher_change_key(lid->passwd);
  sock_header_set_info(buf, tid, len, type, val, lid->nb_pkt_tx, &payload);
  if (payload != buf + headsize)
    KOUT("%p %p", payload, buf);
  dchan->tot_txq_size += tot_len;
  peak_queue_len = get_peak_queue_len(cidx);
  if (*peak_queue_len < dchan->tot_txq_size)
    *peak_queue_len = dchan->tot_txq_size;
  chain_append_tx(&(dchan->tx), &(dchan->last_tx), tot_len, buf);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static char *convert_and_copy(char *buf, int len)
{
  int headsize = doorways_header_size();
  char *msg_buf = (char *) clownix_malloc(len + headsize, 9);
  memset(msg_buf, 0, headsize);
  memcpy(msg_buf+headsize, buf, len);
  return msg_buf;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void doorways_tx_split(t_llid *lid, int cidx, t_data_channel *dchan,
                              int tid, int type, int val, int len, char *buf)
{
  int headsize = doorways_header_size();
  int len_max = MAX_DOORWAYS_BUF_LEN - headsize;
  int len_left = len;
  int len_done = 0;
  char *msg_buf;
  while(len_left)
    {
    if (len_left <= len_max)
      {
      msg_buf = convert_and_copy(buf+len_done, len_left);
      doorways_tx_append(lid,cidx,dchan,tid,type,val,len_left,msg_buf);
      len_left = 0;
      }
    else
      {
      msg_buf = convert_and_copy(buf+len_done, len_max);
      doorways_tx_append(lid,cidx,dchan,tid,type,val,len_max,msg_buf);
      len_done += len_max;
      len_left -= len_max;
      }
    }
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
int doorways_tx_or_rx_still_in_queue(int llid)
{
  int result = 0;
  t_data_channel *dchan;
  int cidx = channel_check_llid(llid, __FUNCTION__);
  t_llid *lid = g_llid_data[llid];
  if (cidx == 0)
    KERR("ERROR");
  else
    {
    dchan = get_dchan(cidx);
    if (dchan->tot_txq_size)
      result = 1;
    else if (lid)
      {
      if ((lid->rx_pktdoors.offset) || (lid->rx_pktdoors.paylen))
        result = 1;
      }
    }
  return (result);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
int doorways_tx_bufraw(int llid,int tid,int type,int val,int len,char *buf)
{
  int cidx, fd, result = -1;
  t_llid *lid;
  t_data_channel *dchan;

  if (g_init_done != 777)
    KOUT("ERROR");
  if ((len<0) || (len > 10*MAX_DOORWAYS_BUF_LEN))
    KOUT("ERROR %d", len);
  if (len == 0)
    return 0;
  if (!llid) 
    KOUT("ERROR");
  if (!msg_exist_channel(llid))
    return result;
  fd = get_fd_with_llid(llid);
  if (fd == -1)
    KOUT("ERROR");
  else
    {
    lid = g_llid_data[llid];
    if (lid)
      {
      if (llid != lid->llid)
        KOUT("ERROR %d %d", llid, lid->llid);
      if (lid->doors_type != doors_type_server) 
        {
        if (lid->doors_type != type)
          {
          if (lid->doors_type == doors_type_dbssh)
            {
            if ((type != doors_type_dbssh_x11_ctrl) &&
                (type != doors_type_dbssh_x11_traf))
              KERR("ERROR %d %d", lid->doors_type, type);
            }
          else
             KERR("ERROR %d %d", lid->doors_type, type);
          }
        }
      cidx = channel_check_llid(llid, __FUNCTION__);
      if (cidx == 0)
        KERR("ERROR %d %d", lid->doors_type, type);
      else
        {
        dchan = get_dchan(cidx);
        if (dchan->llid != lid->llid)
          KOUT("ERROR");
        if (dchan->tot_txq_size >  MAX_TOT_LEN_WARNING_DOORWAYS_Q)
          {
          g_max_tx_doorway_queue_len_reached += 1;
          }
        if (dchan->tot_txq_size <  MAX_TOT_LEN_DOORWAYS_Q)
          {
          doorways_tx_split(lid, cidx, dchan, tid, type, val, len, buf);
          result = 0;
          }
        else
          {
          KERR("ERROR %d %d", (int) dchan->tot_txq_size,
                              MAX_TOT_LEN_DOORWAYS_Q);
          }
        }
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int doorways_sig_bufraw(int llid, int tid, int type, int val, char *buf)
{
return doorways_tx_bufraw(llid, tid, type, val, strlen(buf)+1, buf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void doors_tx_switch_val_none(int llid, int len, char *buf)
{
  if (doorways_tx_bufraw(llid,0,doors_type_switch,doors_val_none,len,buf))
    KERR("WARNING %s", buf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void doors_tx_switch_val_c2c(int llid, int len, char *buf)
{
  if (doorways_tx_bufraw(llid,0,doors_type_switch,doors_val_c2c,len,buf))
    KERR("WARNING %s", buf);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void doorways_clean_llid(int llid)
{
  clean_llid(llid);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void doorways_sock_init(int to_exit)
{
  g_to_exit = to_exit;
  cipher_myinit();
  g_init_done = 777;
  memset(g_llid_data, 0, sizeof(t_llid *) * CLOWNIX_MAX_CHANNELS); 
  g_max_tx_sock_queue_len_reached = 0;
  g_max_tx_doorway_queue_len_reached = 0;
  clownix_timeout_add(1000, fct_10_sec_timeout, NULL, NULL, NULL);
}
/*---------------------------------------------------------------------------*/
#endif

void doorways_linker_helper(void)
{
}
