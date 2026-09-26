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

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "proxy_crun.h"
#include "util_sock.h"
#include "nat_main.h"
#include "passt_spawn.h"

static t_ctx_nat *g_head_ctx_nat;

static t_ctx_nat *ctx_nat_find(char *nat)
{
  t_ctx_nat *cur = g_head_ctx_nat;
  while(cur)
    {
    if (!strcmp(cur->nat, nat))
      break;
    cur = cur->next;
    }
  return cur;
}

static t_ctx_nat *ctx_nat_find_with_llid_listen(int llid)
{
  t_ctx_nat *cur = g_head_ctx_nat;
  while(cur)
    {
    if ((llid) && (cur->llid_listen == llid))
      break;
    cur = cur->next;
    }
  return cur;
}

static t_ctx_nat *ctx_nat_find_with_llid_sig(int llid)
{
  t_ctx_nat *cur = g_head_ctx_nat;
  while(cur)
    {
    if ((llid) && (cur->llid_sig == llid))
      break;
    cur = cur->next;
    }
  return cur;
}

static void alloc_ctx_nat(char *nat, char *nat_prxy_path, int llid_listen)
{
  t_ctx_nat *cur = (t_ctx_nat *) malloc(sizeof(t_ctx_nat));
  memset(cur, 0, sizeof(t_ctx_nat));
  cur->llid_listen = llid_listen;
  strncpy(cur->nat, nat, MAX_NAME_LEN-1);
  strncpy(cur->nat_prxy_path, nat_prxy_path, MAX_PATH_LEN-1);
  if (g_head_ctx_nat)
    g_head_ctx_nat->prev = cur;
  cur->next = g_head_ctx_nat;
  g_head_ctx_nat = cur;
}

static void free_ctx_nat(t_ctx_nat *cur)
{
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur->next)
    cur->next->prev = cur->prev;
  if (g_head_ctx_nat == cur)
    g_head_ctx_nat = cur->next;
  free(cur);
}

#define NB_TORN_DOWN 64
static char g_torn_down[NB_TORN_DOWN][MAX_NAME_LEN];
static int  g_torn_down_idx;

static void note_torn_down(char *nat)
{
  memset(g_torn_down[g_torn_down_idx], 0, MAX_NAME_LEN);
  strncpy(g_torn_down[g_torn_down_idx], nat, MAX_NAME_LEN-1);
  g_torn_down_idx = (g_torn_down_idx + 1) % NB_TORN_DOWN;
}

static int was_torn_down(char *nat)
{
  int i;
  for (i = 0; i < NB_TORN_DOWN; i++)
    if (!strcmp(g_torn_down[i], nat))
      return 1;
  return 0;
}

static void delete_nat_ctx(t_ctx_nat *cur)
{
  note_torn_down(cur->nat);
  if ((cur->llid_listen) && (msg_exist_channel(cur->llid_listen)))
    {
    msg_delete_channel(cur->llid_listen);
    cur->llid_listen = 0;
    }
  if ((cur->llid_sig) && (msg_exist_channel(cur->llid_sig)))
    {
    msg_delete_channel(cur->llid_sig);
    cur->llid_sig = 0;
    }
  unlink(cur->nat_prxy_path);
  passt_spawn_stop_one(cur->passt_sock_path);
  free_ctx_nat(cur);
}

static void err_nat_prxy_cb (int llid, int err, int from)
{
  t_ctx_nat *cur = ctx_nat_find_with_llid_sig(llid);
  if (msg_exist_channel(llid))
    msg_delete_channel(llid);
  if (!cur)
    KERR("ERROR TRAFFIC CONNECTION %d", llid);
  else
    {
    cur->llid_sig = 0;
    delete_nat_ctx(cur);
    }
}
  
static void rx_nat_prxy_cb (int llid, int len, char *buf)
{ 
  if (rpct_decoder(llid, len, buf))
    KOUT("ERROR DECODER %s", buf);
} 

static void connect_from_nat_prxy_client(int llid, int llid_new)
{
  t_ctx_nat *cur = ctx_nat_find_with_llid_listen(llid);
  if (!cur)
    KERR("ERROR LISTENING CONNECTION %d %d", llid, llid_new);
  else
    {
    msg_mngt_set_callbacks (llid_new, err_nat_prxy_cb, rx_nat_prxy_cb);
    msg_delete_channel(cur->llid_listen);
    cur->llid_listen = 0;
    cur->llid_sig = llid_new;
    }
}

int nat_main_request(char *nat, int on, char *ports, int expected)
{
  t_ctx_nat *cur;
  int llid_listen, result = -1;
  char nat_prxy_path[MAX_PATH_LEN];
  if ((!nat) || (strlen(nat) == 0))
    KOUT("ERROR");
  cur = ctx_nat_find(nat);
  if ((on) && (cur) && (expected))
    {
    /* A publish change: the previous ports are kept, so that a passt
       that cannot hold the new ones is put back as it was. */
    char prev[MAX_PATH_LEN];
    memset(prev, 0, MAX_PATH_LEN);
    snprintf(prev, MAX_PATH_LEN-1, "%s", cur->ports);
    memset(cur->ports, 0, MAX_PATH_LEN);
    if (ports)
      snprintf(cur->ports, MAX_PATH_LEN-1, "%s", ports);
    passt_spawn_stop_one(cur->passt_sock_path);
    if ((passt_spawn_start(cur->passt_sock_path, cur->ports, "republish") < 0) ||
        (passt_spawn_probe(cur->passt_sock_path) < 0))
      {
      KERR("ERROR %s: passt does not hold the ports %s, back to %s",
           nat, cur->ports, prev);
      memset(cur->ports, 0, MAX_PATH_LEN);
      snprintf(cur->ports, MAX_PATH_LEN-1, "%s", prev);
      }
    else
      result = 0;
    }
  else if (on)
    {
    if (cur)
      {
      KERR("WARNING %s EXISTS, tearing down stale ctx before re-arming", nat);
      delete_nat_ctx(cur);
      cur = NULL;
      }
      {
      memset(nat_prxy_path, 0, MAX_PATH_LEN);
      snprintf(nat_prxy_path, MAX_PATH_LEN-1,"%s/%s/%s",
               get_proxyshare(), NAT_PROXY_DIR, nat);
      if (!access(nat_prxy_path, R_OK))
        {
        KERR("WARNING %s exists ERASING", nat_prxy_path);
        unlink(nat_prxy_path);
        }
      llid_listen = string_server_unix(nat_prxy_path,
                                       connect_from_nat_prxy_client,
                                       "prxy");
      if (llid_listen == 0)
        KERR("ERROR %s LISTEN", nat);
      else
        {
        alloc_ctx_nat(nat, nat_prxy_path, llid_listen);
        cur = ctx_nat_find(nat);
        snprintf(cur->passt_sock_path, MAX_PATH_LEN-1, "%s/%s/%s_passt",
                 get_proxyshare(), NAT_PROXY_DIR, nat);
        if (ports)
          snprintf(cur->ports, MAX_PATH_LEN-1, "%s", ports);
        if (passt_spawn_start(cur->passt_sock_path, cur->ports, "nat created") < 0)
          {
          /* A nat without its passt is not a nat */
          KERR("ERROR %s passt spawn failed, the nat cannot work: tearing "
               "down the context and reporting the failure", nat);
          delete_nat_ctx(cur);
          result = -1;
          }
        else
          result = 0;
        }
      }
    }
  else
    {
    if (!cur)
      {
      /* Normal ordering, not an orphan */
      if (!was_torn_down(nat))
        KERR("ERROR %s DOES NOT EXIST here and was never torn down either, "
             "so if a passt was spawned for it, it is now orphaned", nat);
      else
        /* Nothing to do: success, so sig_process.c logs no ERROR of its own. */
        result = 0;
      }
    else
      {
      delete_nat_ctx(cur);
      result = 0;
      }
    }
  return result;
}

void rpct_recv_poldiag_msg(int llid, int tid, char *line)
{
  char nat[MAX_NAME_LEN];
  char msg[MAX_PATH_LEN];
  t_ctx_nat *cur = ctx_nat_find_with_llid_sig(llid);
  if (!cur)
    KERR("ERROR TRAFFIC CONNECTION %d %s", llid, line);
  else
    {
    if (sscanf(line,
    "nat_proxy_beat_req %63s", nat) == 1)
      {
      if (strcmp(nat, cur->nat))
        KERR("ERROR %s %s", nat, cur->nat);
      else
        {
        memset(msg, 0, MAX_PATH_LEN);
        snprintf(msg, MAX_PATH_LEN-1, "nat_proxy_beat_resp %s", nat);
        rpct_send_poldiag_msg(llid, tid, msg);
        }
      }
    else
      KERR("ERROR %s", line);
    }
}

void rpct_recv_sigdiag_msg(int llid, int tid, char *line)
{
  char nat[MAX_NAME_LEN];
  char buf[2*MAX_PATH_LEN];
  t_ctx_nat *cur = ctx_nat_find_with_llid_sig(llid);
  if (!cur)
    KERR("ERROR TRAFFIC CONNECTION %d %s", llid, line);
  else
    {
    memset(buf, 0, 2*MAX_PATH_LEN);
    if (sscanf(line,
    "nat_proxy_hello_req %63s", nat) == 1)
      {
      if (strcmp(nat, cur->nat))
        KERR("ERROR %s %s", nat, cur->nat);
      else
        {
        snprintf(buf, 2*MAX_PATH_LEN-1, "nat_proxy_hello_resp %s passt:%s",
                 nat, cur->passt_sock_path);
        rpct_send_sigdiag_msg(llid, tid, buf);
        }
      }
    else
      KERR("ERROR %s", line);
    }
}

void nat_main_kil_req(void)
{
  t_ctx_nat *next, *cur = g_head_ctx_nat;
  while(cur)
    {
    next = cur->next;
    delete_nat_ctx(cur);
    cur = next;
    }
}

void nat_main_passt_died(void)
{
  if (g_head_ctx_nat)
    KERR("ERROR passt died, tearing down %s and any other nat ctx",
         g_head_ctx_nat->nat);
  nat_main_kil_req();
}

t_ctx_nat *get_ctx_nat(char *nat)
{
  return ctx_nat_find(nat);
}

void nat_main_init(void)
{
  g_head_ctx_nat = NULL;
}
