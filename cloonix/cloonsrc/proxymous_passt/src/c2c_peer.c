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
#include <linux/tcp.h>
#include <time.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "doorways_sock.h"
#include "proxy_crun.h"
#include "util_sock.h"

/* C2C_TRACE: the per-attempt c2c chatter, off. */
#define C2C_TRACE 0
#if C2C_TRACE
#define C2C_DBG(format, a...) KERR(format, ## a)
#else
#define C2C_DBG(format, a...) do { } while (0)
#endif

typedef struct t_c2c_peer
{
  int pair_listen_llid;
  int pair_llid;
  int tcp_listen_llid;
  uint16_t tcp_listen_port;
  char name[MAX_NAME_LEN];
  uint32_t ip;
  uint16_t port;
  char dist_passwd[MSG_DIGEST_LEN];
  struct t_c2c_peer *prev;
  struct t_c2c_peer *next;
} t_c2c_peer;

static t_c2c_peer *g_c2c_peer_head;

static t_c2c_peer *find_c2c_peer_with_name(char *name)
{
  t_c2c_peer *cur = g_c2c_peer_head;
  while (cur)
    {
    if (!strcmp(cur->name, name))
      break;
    cur = cur->next;
    }
  return cur;
}

static t_c2c_peer *find_c2c_peer_with_listen_llid(int pair_listen_llid)
{
  t_c2c_peer *cur = g_c2c_peer_head;
  while (cur)
    {
    if ((pair_listen_llid) && (cur->pair_listen_llid == pair_listen_llid))
      break;
    cur = cur->next;
    }
  return cur;
}

static t_c2c_peer *find_c2c_peer_with_llid(int llid)
{
  t_c2c_peer *cur = g_c2c_peer_head;
  while (cur)
    {
    if (llid)
      {
      if ((cur->pair_llid == llid) ||
          (cur->pair_listen_llid == llid) ||
          (cur->tcp_listen_llid == llid))
        break;
      }
    cur = cur->next;
    }
  return cur;
}

static void alloc_c2c_peer(int pair_listen_llid, char *name, uint32_t ip,
                           uint16_t port, char *dist_passwd)
{
  t_c2c_peer *cur = (t_c2c_peer *) malloc(sizeof(t_c2c_peer));
  memset(cur, 0, sizeof(t_c2c_peer)); 
  cur->pair_listen_llid = pair_listen_llid;
  cur->ip = ip;
  cur->port = port;
  strncpy(cur->name, name, MAX_NAME_LEN-1);
  strncpy(cur->dist_passwd, dist_passwd, MSG_DIGEST_LEN-1);
  if (g_c2c_peer_head)
    g_c2c_peer_head->prev = cur;
  cur->next = g_c2c_peer_head;
  g_c2c_peer_head = cur;
}

#define NB_CLOSED_LLID 32
#define CLOSED_LLID_SECONDS 30

static int g_closed_llid[NB_CLOSED_LLID];
static time_t g_closed_when[NB_CLOSED_LLID];
static int g_closed_idx;

static void note_closed_llid(int llid)
{
  if (!llid)
    return;
  g_closed_llid[g_closed_idx] = llid;
  g_closed_when[g_closed_idx] = time(NULL);
  g_closed_idx = (g_closed_idx + 1) % NB_CLOSED_LLID;
}

static int was_closed_by_us(int llid)
{
  int i;
  time_t now = time(NULL);
  for (i=0; i<NB_CLOSED_LLID; i++)
    {
    if ((g_closed_llid[i] == llid) &&
        (now - g_closed_when[i] <= CLOSED_LLID_SECONDS))
      return 1;
    }
  return 0;
}

static void free_c2c_peer(t_c2c_peer *cur)
{
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur->next)
    cur->next->prev = cur->prev;
  if (cur == g_c2c_peer_head)
    g_c2c_peer_head = cur->next;
  cur->prev = NULL;
  cur->next = NULL;

  udp_proxy_end_of(cur->name);
  if (cur->pair_listen_llid)
    {
    note_closed_llid(cur->pair_listen_llid);
    doorways_sock_client_inet_delete(cur->pair_listen_llid);
    cur->pair_listen_llid = 0;
    }
  if (cur->pair_llid)
    {
    note_closed_llid(cur->pair_llid);
    doorways_sock_client_inet_delete(cur->pair_llid);
    cur->pair_llid = 0;
    }
  if (cur->tcp_listen_llid)
    {
    note_closed_llid(cur->tcp_listen_llid);
    doorways_sock_client_inet_delete(cur->tcp_listen_llid);
    cur->tcp_listen_llid = 0;
    }
  /* One line per c2c freed, with the count: a leak shows as a climb. */
  {
  int nb = 0;
  t_c2c_peer *walk = g_c2c_peer_head;
  while (walk) { nb += 1; walk = walk->next; }
  C2C_DBG("INFO c2c peer %s released its descriptors, %d peer(s) left",
          cur->name, nb);
  }
  free(cur);
}

static void peer_err_cb (int llid)
{
  t_c2c_peer *cur = find_c2c_peer_with_llid(llid);
  if (!cur)
    {
    if (was_closed_by_us(llid))
      KTRC("error callback for llid %d, a channel this c2c delete closed",
           llid);
    else
      {
      /* A callback for a connection nothing owns and nothing here closed. */
      static int nb_orphan_err;
      int nb_peers = 0;
      t_c2c_peer *walk;
      for (walk = g_c2c_peer_head; walk; walk = walk->next)
        nb_peers += 1;
      nb_orphan_err += 1;
      KERR("ERROR: an error callback fired for llid %d with no c2c peer "
           "record and nothing here closed it. %d peer(s) known, %d such "
           "orphans so far this run", llid, nb_peers, nb_orphan_err);
      }
    }
  else 
    {
    if ((cur->pair_llid != llid) && (cur->pair_listen_llid != llid) &&
        (cur->tcp_listen_llid != llid))
      KERR("ERROR %s: error callback on llid %d, which is none of its "
           "own %d %d %d", cur->name, llid, cur->pair_llid,
           cur->pair_listen_llid, cur->tcp_listen_llid);
    if (msg_exist_channel(llid))
      msg_delete_channel(llid);
    /* NOT freed here: doing so re-enters this callback through the other
       channels' error callbacks and crashes. */
    }
}

static void peer_rx_cb(int llid, int tid,
                       int len_bufraw, char *doors_bufraw,
                       int type, int val, int len, char *buf)
{
  t_c2c_peer *cur = find_c2c_peer_with_llid(llid);
  if (!cur)
    KERR("ERROR %d bytes from a peer net on llid %d, no c2c peer owns it, "
         "dropped", len, llid);
  else if (llid != cur->pair_llid)
    KERR("ERROR c2c %s: %d bytes on llid %d, not its link to the peer net "
         "(llid %d), dropped", cur->name, len, llid, cur->pair_llid);
  else if (!msg_exist_channel(llid))
    KERR("ERROR c2c %s: %d bytes on its closed link to the peer net, "
         "dropped", cur->name, len);
  else if (type != doors_type_switch)
    KERR("ERROR c2c %s: %d bytes of doors type %d from the peer net, not a "
         "switch frame, dropped", cur->name, len, type);
  else if (val == doors_val_link_ko)
    KERR("ERROR c2c %s: the peer net refused the link", cur->name);
  else if (val != doors_val_link_ok)
    {
    if (len > 8 &&
        (unsigned char)buf[0] == 0xCA && (unsigned char)buf[1] == 0xFE &&
        (unsigned char)buf[2] == 0xDE && (unsigned char)buf[3] == 0xCA)
      { buf += 8; len -= 8; }
    sig_process_proxy_peer_data_to_crun(cur->name, len, buf);
    }
}

static int callback_connect(int llid, int fd)
{
  t_c2c_peer *cur = find_c2c_peer_with_listen_llid(llid);
  int pair_llid;
  if (!cur)
    {
    KERR("WARNING %d: connected but no peer owns this listen llid, "
         "closing the socket", llid);
    close(fd);
    }
  else
    {
    if (cur->pair_llid)
      {
      KERR("WARNING %s: already connected to the peer, closing this "
           "redundant connection", cur->name);
      close(fd);
      }
    else
      {
      pair_llid = doorways_sock_client_inet_end(doors_type_switch,
                                                llid, fd,
                                                cur->dist_passwd,
                                                peer_err_cb, peer_rx_cb);
      if (!pair_llid)
        {
        KERR("WARNING %s: the doorways handshake failed, closing the "
             "socket", cur->name);
        close(fd);
        }
      else
        {
        C2C_DBG("INFO c2c %s connected to its peer, llid=%d", cur->name,
                pair_llid);
        cur->pair_llid = pair_llid;
        if (doorways_sig_bufraw(pair_llid, llid, doors_type_switch,
                                doors_val_init_link, "OK"))
          KERR("WARNING c2c %s: the start of the link could not be sent to "
               "the peer net", cur->name);
        sig_process_tx_proxy_callback_connect(cur->name, pair_llid);
        if (msg_exist_channel(cur->pair_listen_llid))
          msg_delete_channel(cur->pair_listen_llid);
        cur->pair_listen_llid = 0;
        }
      }
    }
  return 0;
}

int c2c_peer_dist_tcp_ip_port(char *name, uint32_t ip, 
                              uint16_t tcp_port, char *dist_passwd)
{
  int llid, result = -1;
  char ip_string[MAX_NAME_LEN];
  t_c2c_peer *cur = find_c2c_peer_with_name(name);
  if (cur && ((msg_exist_channel(cur->pair_listen_llid)) ||
              (msg_exist_channel(cur->pair_llid))))
    {
    result = 0;
    }
  else
    {
    if (cur)
      free_c2c_peer(cur);
    llid = doorways_sock_client_inet_start(ip, tcp_port, callback_connect);
    C2C_DBG("INFO c2c %s starting a connection to the peer, llid=%d "
            "(had_peer=%d)", name, llid, cur ? 1 : 0);
    if (llid == 0)
      {
      int_to_ip_string(ip, ip_string);
      KERR("ERROR c2c %s: no tcp connection could be started to the peer "
           "net at %s port %hu", name, ip_string, tcp_port);
      }
    else
      {
      result = 0;
      alloc_c2c_peer(llid, name, ip, tcp_port, dist_passwd);
      }
    }
  return result;
}

/* Sent by the switch of either end; only the master has a udp proxy. */
int c2c_peer_delete(char *name)
{
  int result = -1;
  t_c2c_peer *cur = find_c2c_peer_with_name(name);
  if (!cur)
    {
    udp_proxy_end_of(name);
    KTRC("c2c peer %s already gone, released its udp proxy if it "
         "still held one", name);
    }
  else
    {
    KTRC("c2c peer %s deleted on request from the switch", name);
    free_c2c_peer(cur);
    result = 0;
    }
  return result;
}

int c2c_peer_proxy_data_from_crun(char *name, int len, char *buf)
{
  int result = -1;
  t_c2c_peer *cur = find_c2c_peer_with_name(name);
  if (!cur)
    KERR("ERROR c2c %s: no such c2c peer here, its data for the peer net "
         "dropped (the switch is told)", name);
  else if (!msg_exist_channel(cur->pair_llid))
    KERR("ERROR c2c %s: no link to the peer net, its data dropped (the "
         "switch is told)", name);
  else if (doorways_tx_bufraw(cur->pair_llid, 0, doors_type_switch,
                              doors_val_c2c, len-1, buf))
    KERR("ERROR c2c %s: the link to the peer net refused its data (the "
         "switch is told)", name);
  else
    result = 0;
  return result;
}

void c2c_peer_init(void)
{
  g_c2c_peer_head = NULL;
}
