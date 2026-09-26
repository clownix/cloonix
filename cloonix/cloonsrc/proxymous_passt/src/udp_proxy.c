/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#include <stdio.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <signal.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/select.h>
#include <fcntl.h>
#include "io_clownix.h"
#include "proxy_crun.h"
#include "util_sock.h"

/* One udp proxy per c2c, up to MAX_UDP_PROXY of them. */
#define MAX_UDP_PROXY 5

typedef struct t_udp_proxy
{
  int used;
  char name[MAX_NAME_LEN];
  char unix_dgram_rx[MAX_PATH_LEN];
  char unix_dgram_tx[MAX_PATH_LEN];
  struct sockaddr_in loc_addr;
  uint32_t distant_ip;
  uint16_t distant_udp_port;
  char distant_ip_string[MAX_NAME_LEN];
  int udp_sock;
  int unix_sock;
  int udp_llid;
  int unix_llid;
  int udp_forward_ok;
  int nb_dropped_no_peer;
  unsigned long nb_full_to_crun;
  unsigned long nb_full_to_peer;
} t_udp_proxy;

/* Non blocking with big buffers: a blocking sendto here and in the c2c
   deadlocked the two (each stuck while the other's queue was full). A
   full queue drops the frame, as udp does. */
static int say_now(uint64_t *last)
{
  uint64_t now = cloonix_get_msec();
  if (now - *last < 10000)
    return 0;
  *last = now;
  return 1;
}

static void dgram_buffers(int fd)
{
  int sz = 4 * 1024 * 1024;
  nonblock_fd(fd);
  setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &sz, sizeof(sz));
  setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sz, sizeof(sz));
}

static t_udp_proxy g_proxy[MAX_UDP_PROXY];

static int udp_proxy_init(t_udp_proxy *cur, char *proxyshare,
                          uint16_t *udp_port, uint16_t c2c_udp_port_low);
static int from_crun_rx_one(int llid, int fd);
static int from_udp_rx_one(int llid, int fd);

/* Up to 64 datagrams per wakeup on each side: the unix queue in front of a
   socket holds ten (net.unix.max_dgram_qlen), one read per wakeup left it
   full and the c2c dropping. */
static int drain(int llid, int fd, int (*one)(int, int))
{
  int i, result = 0, r;
  for (i=0; i<64; i++)
    {
    r = one(llid, fd);
    if (r <= 0)
      return (r < 0) ? r : result;
    result = r;
    }
  return result;
}

static int from_crun_rx_cb(int llid, int fd)
{
  return drain(llid, fd, from_crun_rx_one);
}

static int from_udp_rx_cb(int llid, int fd)
{
  return drain(llid, fd, from_udp_rx_one);
}

static void proxy_free(t_udp_proxy *cur);

static t_udp_proxy *proxy_find(char *name)
{
  int i;
  t_udp_proxy *result = NULL;
  if ((name) && (name[0]))
    {
    for (i=0; i<MAX_UDP_PROXY; i++)
      {
      if ((g_proxy[i].used) && (!strcmp(g_proxy[i].name, name)))
        {
        result = &(g_proxy[i]);
        break;
        }
      }
    }
  return result;
}

static t_udp_proxy *proxy_find_with_llid(int llid)
{
  int i;
  t_udp_proxy *result = NULL;
  for (i=0; i<MAX_UDP_PROXY; i++)
    {
    if ((g_proxy[i].used) &&
        ((g_proxy[i].udp_llid == llid) || (g_proxy[i].unix_llid == llid)))
      {
      result = &(g_proxy[i]);
      break;
      }
    }
  return result;
}

static t_udp_proxy *proxy_alloc(char *name)
{
  int i;
  t_udp_proxy *cur = NULL;
  char busy[MAX_UDP_PROXY * MAX_NAME_LEN];
  int len = 0;
  for (i=0; i<MAX_UDP_PROXY; i++)
    {
    if (!g_proxy[i].used)
      {
      cur = &(g_proxy[i]);
      break;
      }
    }
  if (!cur)
    {
    memset(busy, 0, MAX_UDP_PROXY * MAX_NAME_LEN);
    for (i=0; i<MAX_UDP_PROXY; i++)
      len += snprintf(busy+len, (MAX_UDP_PROXY * MAX_NAME_LEN) - len,
                      "%s ", g_proxy[i].name);
    KERR("ERROR %s wants a udp proxy and all %d are taken by: %s",
         name, MAX_UDP_PROXY, busy);
    }
  else
    {
    memset(cur, 0, sizeof(t_udp_proxy));
    cur->used = 1;
    cur->udp_sock = -1;
    cur->unix_sock = -1;
    strncpy(cur->name, name, MAX_NAME_LEN-1);
    }
  return cur;
}

static int udp_init_pick_a_free_udp_port(t_udp_proxy *cur, uint16_t *udp_port,
                                         int *udp_sock,
                                         uint16_t c2c_udp_port_low)
{
  int i, fd, result = -1, max_try=100;
  uint16_t port;
  uint32_t len = sizeof(struct sockaddr_in);
  *udp_port = -1;
  if ((fd = socket(AF_INET, SOCK_DGRAM, 0))< 0)
    KOUT("%d", errno);
  memset(&(cur->loc_addr),  0, len);
  cur->loc_addr.sin_family = AF_INET;
  cur->loc_addr.sin_addr.s_addr = INADDR_ANY;
  for(i=0; i<max_try; i++)
    {
    port = c2c_udp_port_low + i;
    cur->loc_addr.sin_port = htons(port);
    if (bind(fd, (const struct sockaddr *)&(cur->loc_addr), len) == 0)
      {
      result = 0;
      *udp_port = port;
      *udp_sock = fd;
      break;
      }
    }
  if (result)
    {
    /* The socket goes back when no port could be bound: this leaked one
       descriptor per failed attempt. */
    close(fd);
    KERR("ERROR %s found no free udp port from %hu", cur->name,
         c2c_udp_port_low);
    }
  return result;
}

static void from_crun_err_cb(int llid, int err, int from)
{   
  t_udp_proxy *cur = proxy_find_with_llid(llid);
  if (msg_exist_channel(llid))
    msg_delete_channel(llid);
  if (!cur)
    KTRC("crun side error on llid %d, no proxy of that llid", llid);
  else
    {
    KERR("ERROR %s: the crun side of its udp proxy failed (%d %d)",
         cur->name, err, from);
    proxy_free(cur);
    }
}

static void from_udp_err_cb(int llid, int err, int from)
{   
  t_udp_proxy *cur = proxy_find_with_llid(llid);
  if (msg_exist_channel(llid))
    msg_delete_channel(llid);
  if (!cur)
    KTRC("udp side error on llid %d, no proxy of that llid", llid);
  else
    {
    KERR("ERROR %s: the udp side of its udp proxy failed (%d %d)",
         cur->name, err, from);
    proxy_free(cur);
    }
}

static int from_crun_rx_one(int llid, int fd)
{
  char buf[MAX_DGRAM_LEN];
  struct sockaddr_in dist_addr;
  struct sockaddr *pdist_addr = (struct sockaddr *) (&dist_addr);
  uint32_t ln = sizeof(struct sockaddr_in);
  int len_tx, len, result = 0;
  t_udp_proxy *cur = proxy_find_with_llid(llid);
  if (!cur)
    {
    KERR("ERROR data on llid %d with no udp proxy behind it", llid);
    if (msg_exist_channel(llid))
      msg_delete_channel(llid);
    return -1;
    }
  len = read(fd, buf, MAX_DGRAM_LEN-1);
  if (len == 0)
    {
    KERR("ERROR %s: the crun side of its udp proxy closed", cur->name);
    proxy_free(cur);
    }
  else if (len < 0)
    {
    if ((errno != EAGAIN) && (errno != EINTR) && (errno != EINPROGRESS))
      {
      KERR("ERROR %s: read on the crun side failed, errno %d", cur->name,
           errno);
      proxy_free(cur);
      }
    }
  else
    {
    if ((cur->distant_ip) && (cur->distant_udp_port))
      {
      memset(pdist_addr, 0, sizeof(struct sockaddr_in));
      dist_addr.sin_family = AF_INET;
      dist_addr.sin_addr.s_addr = htonl(cur->distant_ip);
      dist_addr.sin_port = htons(cur->distant_udp_port);
      len_tx = sendto(cur->udp_sock, buf, len, 0, pdist_addr, ln);
      if ((len_tx == -1) && ((errno == EAGAIN) || (errno == ENOBUFS)))
        {
        static uint64_t last;
        cur->nb_full_to_peer += 1;
        if (say_now(&last))
          KERR("WARNING %s: the udp socket to the peer is full, %lu frames "
               "dropped so far", cur->name, cur->nb_full_to_peer);
        result = len;
        }
      else if (len_tx != len)
        {
        KERR("ERROR %s: sent %d of %d to the peer, errno %d", cur->name,
             len_tx, len, errno);
        proxy_free(cur);
        result = -1;
        }
      else
        result = len;
      }
    else
      {
      result = len;
      cur->nb_dropped_no_peer += 1;
      if (cur->nb_dropped_no_peer == 1)
        KTRC("%s: a frame was dropped, the peer address is not known yet. "
             "Its sequence number will be missing at the far end",
             cur->name);
      else if ((cur->nb_dropped_no_peer % 100) == 0)
        KERR("WARNING %s: %d frames dropped so far with no peer address",
             cur->name, cur->nb_dropped_no_peer);
      }
    }
  return result;
}

static int from_udp_rx_one(int llid, int fd)
{
  char buf[MAX_DGRAM_LEN];
  struct sockaddr_un name;
  struct sockaddr *pname = (struct sockaddr *)&name;
  int len_tx, len, len_un = sizeof(struct sockaddr_un);
  int result = 0;
  t_udp_proxy *cur = proxy_find_with_llid(llid);
  if (!cur)
    {
    KERR("ERROR data on llid %d with no udp proxy behind it", llid);
    if (msg_exist_channel(llid))
      msg_delete_channel(llid);
    return -1;
    }
  len = read(fd, buf, MAX_DGRAM_LEN-1);
  if (len == 0)
    {
    KERR("ERROR %s: the udp side of its udp proxy closed", cur->name);
    proxy_free(cur);
    }
  else if (len < 0)
    {
    if ((errno != EAGAIN) && (errno != EINTR) && (errno != EINPROGRESS))
      {
      KERR("ERROR %s: read on the udp side failed, errno %d", cur->name,
           errno);
      proxy_free(cur);
      }
    }
  else
    {
    memset(&name, 0, sizeof(struct sockaddr_un));
    name.sun_family = AF_UNIX;
    if (strlen(cur->unix_dgram_rx) >= sizeof(name.sun_path))
      KOUT("ERROR SOCK_DGRAM PATH TOO LONG %s", cur->unix_dgram_rx);
    strncpy(name.sun_path, cur->unix_dgram_rx, sizeof(name.sun_path)-1);
    len_tx = sendto(cur->unix_sock, buf, len, 0, pname, len_un);
    if ((len_tx == -1) && ((errno == EAGAIN) || (errno == ENOBUFS)))
      {
      static uint64_t last;
      cur->nb_full_to_crun += 1;
      if (say_now(&last))
        KERR("WARNING %s: the c2c is not taking frames, %lu dropped so far "
             "at its socket", cur->name, cur->nb_full_to_crun);
      result = len;
      }
    else if (len_tx != len)
      {
      /* ECONNREFUSED on a unix datagram socket means the far end is not there
         any more */
      if (errno == ECONNREFUSED)
        KTRC("%s: the crun socket has gone, this proxy ends with it",
             cur->name);
      else
        KERR("ERROR %s: sent %d of %d to the crun, errno %d", cur->name,
             len_tx, len, errno);
      proxy_free(cur);
      result = -1;
      }
    else
      result = len;
    }
  return result;
}

static void proxy_free(t_udp_proxy *cur)
{
  if (!cur->used)
    return;
  if (msg_exist_channel(cur->udp_llid))
    msg_delete_channel(cur->udp_llid);
  if (msg_exist_channel(cur->unix_llid))
    msg_delete_channel(cur->unix_llid);
  if (cur->unix_sock != -1)
    close(cur->unix_sock);
  if (cur->udp_sock != -1)
    close(cur->udp_sock);
  if (cur->unix_dgram_rx[0])
    unlink(cur->unix_dgram_rx);
  if (cur->unix_dgram_tx[0])
    unlink(cur->unix_dgram_tx);
  memset(cur, 0, sizeof(t_udp_proxy));
  cur->udp_sock = -1;
  cur->unix_sock = -1;
}

/* End the proxy of this c2c, and only that one. */
void udp_proxy_end_of(char *name)
{
  t_udp_proxy *cur = proxy_find(name);
  if (!cur)
    KTRC("end asked for the udp proxy of %s, which has none", name);
  else
    proxy_free(cur);
}

void udp_proxy_end(void)
{
  int i;
  for (i=0; i<MAX_UDP_PROXY; i++)
    {
    if (g_proxy[i].used)
      proxy_free(&(g_proxy[i]));
    }
}

void udp_proxy_dist_udp_ip_port(char *name, uint32_t ip, uint16_t udp_port)
{
  t_udp_proxy *cur = proxy_find(name);
  if (!cur)
    KERR("ERROR %s: peer address given for a c2c with no udp proxy", name);
  else
    {
    cur->distant_ip = ip;
    cur->distant_udp_port = udp_port;
    int_to_ip_string(cur->distant_ip, cur->distant_ip_string);
    }
}

int udp_proxy_init_for(char *name, char *proxyshare, uint16_t *udp_port,
                       uint16_t c2c_udp_port_low)
{
  int result = -1;
  t_udp_proxy *cur;
  if ((!name) || (!name[0]))
    {
    KERR("ERROR a udp proxy was asked for with no c2c name");
    return -1;
    }
  cur = proxy_find(name);
  if (cur)
    {
    KERR("WARNING %s already had a udp proxy, replacing it", name);
    proxy_free(cur);
    }
  cur = proxy_alloc(name);
  if (cur)
    {
    result = udp_proxy_init(cur, proxyshare, udp_port, c2c_udp_port_low);
    if (result)
      proxy_free(cur);
    }
  return result;
}

static int udp_proxy_init(t_udp_proxy *cur, char *proxyshare,
                          uint16_t *udp_port, uint16_t c2c_udp_port_low)
{
  int sock, result = -1;
  if (!udp_init_pick_a_free_udp_port(cur, udp_port, &sock, c2c_udp_port_low))
    {
    snprintf(cur->unix_dgram_tx, MAX_PATH_LEN-1,
             "%s/dgram_c2c_%hu_tx.sock", proxyshare, *udp_port);
    snprintf(cur->unix_dgram_rx, MAX_PATH_LEN-1,
             "%s/dgram_c2c_%hu_rx.sock", proxyshare, *udp_port);
    if (strlen(cur->unix_dgram_rx) >= 108)
      KOUT("ERROR PATH LEN %lu", strlen(cur->unix_dgram_rx));
    if (strlen(cur->unix_dgram_tx) >= 108)
      KOUT("ERROR PATH LEN %lu", strlen(cur->unix_dgram_tx));
    cur->unix_sock = util_socket_unix_dgram(cur->unix_dgram_tx);
    if (cur->unix_sock == -1)
      {
      close(sock);
      KERR("ERROR %s could not make %s", cur->name, cur->unix_dgram_tx);
      }
    else
      {
      dgram_buffers(sock);
      dgram_buffers(cur->unix_sock);
      cur->udp_sock = sock;
      cur->udp_llid = msg_watch_fd(cur->udp_sock, from_udp_rx_cb,
                                   from_udp_err_cb, "udp_inet");
      cur->unix_llid = msg_watch_fd(cur->unix_sock, from_crun_rx_cb,
                                    from_crun_err_cb, "udp_unix");
      if ((cur->udp_llid == 0) || (cur->unix_llid == 0))
        KERR("ERROR %s could not watch its sockets %d %d", cur->name,
             cur->udp_llid, cur->unix_llid);
      else
        {
        cur->udp_forward_ok = 1;
        result = 0;
        }
      }
    }
  return result; 
}

void udp_proxy_init_all(void)
{
  int i;
  memset(g_proxy, 0, sizeof(g_proxy));
  for (i=0; i<MAX_UDP_PROXY; i++)
    {
    g_proxy[i].udp_sock = -1;
    g_proxy[i].unix_sock = -1;
    }
}
