/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <pwd.h>
#include <dirent.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "doors_rpc.h"
#include "cfg_store.h"
#include "automates.h"
#include "ovs_c2c.h"
#include "ovs_nat_main.h"
#include "proxycrun.h"
#include "llid_trace.h"
#include "util_sock.h"
#include "commun_daemon.h"
#include "ovs_c2c.h"
#include "c2c_peer.h"
#include "stall_detect.h"

static int g_llid_proxy_sig;
static char g_proxy_sig_unix[MAX_PATH_LEN];
static char g_passwd[MSG_DIGEST_LEN];
static int g_connect_proxy_ok;
static int g_reconnect_count;

static void timer_proxycrun(void *data);
int get_glob_req_self_destruction(void);
/* Wall clock, not beats (a loaded loop beats late); time this process did
   not run is forgiven, as proxymous does. */
static long long g_proxy_seen_ms;
static t_stall g_stall;
#define WATCH_BEAT_MS   100
#define WATCH_LIMIT_MS  30000

static void proxy_beat_seen(void)
{
  g_proxy_seen_ms = stall_now_ms();
  stall_answered(&g_stall);
}

/* How long since proxymous last beat: a verdict on what goes through it
   (the c2c peers) waits while it is not running. */
long long proxycrun_silence_ms(void)
{
  return stall_now_ms() - g_proxy_seen_ms;
}

static void timer_watch_proxymous(void *data)
{
  long long silence;
  int credit, gap;
  gap = stall_beat(&g_stall);
  if (gap > 0)
    KERR("WARNING this loop did not beat for %d ms (%s), forgiven to "
         "proxymous", gap, stall_why(&g_stall));
  credit = stall_credit_ms(&g_stall);
  silence = stall_now_ms() - g_proxy_seen_ms;
  if (get_glob_req_self_destruction())
    return;
  if (silence > WATCH_LIMIT_MS + credit)
    {
    KERR("ERROR NO HEARTBEAT FROM PROXYMOUS for %lld ms (%d ms forgiven "
         "as this process was not running). It is gone, "
         "and it holds the c2c peers, the passt processes and the udp "
         "proxies -- so this net cannot work and is being destroyed rather "
         "than left looking alive", silence, credit);
    g_connect_proxy_ok = 0;
    recv_kill_uml_clownix(0, 0);
    return;
    }
  clownix_timeout_add(10, timer_watch_proxymous, NULL, NULL, NULL);
}


static void proxy_peer_data_to_crun(char *name, int len, char *buf)
{
  char *ptrs = strstr(buf, PROXYMARKUP_START);
  char *ptre = strstr(buf, PROXYMARKUP_END);
  t_ovs_c2c *cur;
  int pair_llid = 0;
  if ((!ptrs) || (!ptre))
    KERR("ERROR c2c %s: data of its peer from proxymous without its markers, "
         "dropped", name);
  else
    {
    ptrs = ptrs + strlen(PROXYMARKUP_START);
    ptre[0] = 0;
    if ((int) (strlen(ptrs) + 1) != len)
      KERR("ERROR c2c %s: data of its peer from proxymous of %d bytes, %d "
           "announced, dropped", name, (int) strlen(ptrs) + 1, len);
    else
      {
      cur = find_c2c(name);
      if (cur)
        pair_llid = cur->pair_llid;
      if (doors_io_basic_decoder(pair_llid, len-1, ptrs))
        KERR("ERROR c2c %s: a message of its peer that is no c2c message, "
             "dropped: %.80s", name, ptrs);
      }
    }
}

static void proxy_sig_err_cb (int llid, int err, int from)
{
  if (msg_exist_channel(llid))
    msg_delete_channel(llid);
  if (!get_glob_req_self_destruction())
    {
    /* Losing this socket destroys the net, deliberately */
    KERR("ERROR SIGCRUN DISCONNECT TO PROXYMOUS llid=%d err=%d from=%d. "
         "THIS DESTROYS THE WHOLE NET, on purpose: proxymous holds the c2c "
         "peers, the passt processes and the udp proxies, and a restarted "
         "one would have none of them -- the net would look alive with c2c "
         "and nat quietly broken", llid, err, from);
    recv_kill_uml_clownix(0, 0);
    }
}

static void proxy_sig_rx_cb(int llid, int len, char *buf)
{
  char name[MAX_NAME_LEN];
  uint16_t udp_port;
  uint32_t ip;
  int inlen, pair_llid;
  char *beat_resp = "transcrun_proxy_heartbeat_resp";
  if ((buf) && (!strcmp(buf, "transcrun_proxy_heartbeat_req")) &&
      (msg_exist_channel(g_llid_proxy_sig)) && (llid == g_llid_proxy_sig))
    {
    proxy_beat_seen();
    proxy_sig_tx(g_llid_proxy_sig, strlen(beat_resp)+1, beat_resp);
    if (get_glob_req_self_destruction())
      return;
    }
  if (get_glob_req_self_destruction())
    return;
  if ((!buf) || (strlen(buf) == 0))
    KERR("ERROR an empty message from proxymous (len %d), dropped", len);
  else if (!msg_exist_channel(g_llid_proxy_sig))
    KERR("ERROR a message from proxymous while its channel (llid %d) is "
         "closed, dropped", g_llid_proxy_sig);
  else if (llid != g_llid_proxy_sig)
    KERR("ERROR a message on llid %d, not the channel of proxymous (llid %d), "
         "dropped", llid, g_llid_proxy_sig);
  else
    {
    if (!strcmp(buf, "transcrun_proxy_heartbeat_req"))
      {
      g_connect_proxy_ok = 1;
      proxy_beat_seen();
      proxy_sig_tx(g_llid_proxy_sig, strlen(beat_resp)+1, beat_resp);
      }
    else if (!strcmp(buf, "transcrun_req_self_destruct"))
      {
      KERR("ERROR PROXYMOUS SAYS THE INSTALLATION DIRECTORY IS GONE, "
           "STOPPING THIS NET (this is proxycrun asking, not a client)");
      auto_self_destruction(0, 0, "proxymous: the installation directory is gone");
      }
    else if (sscanf(buf,
    "transcrun_req_nat_on_KO %63s", name) == 1)
      {
      KERR("ERROR proxymous could not create nat %s, giving up on it here "
           "instead of waiting for a socket that will never appear", name);
      ovs_nat_main_del(0, 0, name);
      }
    else if (sscanf(buf,
    "transcrun_req_nat_pub_KO %63s", name) == 1)
      ovs_nat_main_publish_ko(name);
    else if (sscanf(buf,
    "transcrun_req_udp_KO %63s", name) == 1)
      KERR("ERROR c2c %s: proxymous could not open its udp proxy", name);
    else if (sscanf(buf,
    "transcrun_req_udp_OK %63s %hu", name, &udp_port) == 2)
      ovs_c2c_transmit_get_free_udp_port(name, udp_port);
    else if (sscanf(buf,
    "transcrun_dist_udp_ip_port_OK %63s %x %hu", name, &ip, &udp_port) == 3)
      ovs_c2c_proxy_dist_udp_ip_port_OK(name, ip, udp_port);
    else if (sscanf(buf,
    "transcrun_proxy_callback_connect %63s %d", name, &pair_llid) == 2)
      {
      wrap_proxy_callback_connect(name, pair_llid);
      }
    else if (sscanf(buf,
    "transcrun_proxy_peer_data_to_crun %63s %d", name, &inlen) == 2)
      proxy_peer_data_to_crun(name, inlen, buf);
    else if (sscanf(buf,
    "transcrun_proxy_data_from_crun_ko %63s", name) == 1)
      peer_doorways_client_tx_status(name, -1);
    else if (sscanf(buf,
    "transcrun_proxy_data_from_crun_ok %63s", name) == 1)
      peer_doorways_client_tx_status(name, 0);
    else
      KERR("ERROR an unknown message from proxymous, dropped: %s", buf);
    }
}

/* Without the channel, the verb and the name are said, never what follows
   them: a password can. */
static void to_proxymous(int len, char *msg)
{
  char *sp = strchr(msg, ' ');
  int shown = (int) strlen(msg);
  if (sp)
    shown = (int) (sp - msg) + 1 + (int) strcspn(sp + 1, " ");
  if (!msg_exist_channel(g_llid_proxy_sig))
    KERR("ERROR not sent to proxymous, no channel to it: %.*s", shown, msg);
  else
    proxy_sig_tx(g_llid_proxy_sig, len, msg);
}

void proxycrun_transmit_write_start_status_file_ready(void)
{
  char *sbuf="transcrun_write_start_status_file_ready";
  to_proxymous(strlen(sbuf)+1, sbuf);
}

void proxycrun_transmit_proxy_data(char *name, int len, char *buf)
{
  /* "transcrun_proxy_data_from_crun <name> <len> " + markups + payload. */
  int hdrlen = (int)(strlen("transcrun_proxy_data_from_crun ")
                     + MAX_NAME_LEN + 12           /* " %d " */
                     + strlen(PROXYMARKUP_START)
                     + strlen(PROXYMARKUP_END) + 4);
  int sbuflen = hdrlen + len + 1;
  char *sbuf = (char *) clownix_malloc(sbuflen, 1);
  int written;
  memset(sbuf, 0, sbuflen);
  written = snprintf(sbuf, sbuflen,
                     "transcrun_proxy_data_from_crun %s %d %s",
                     name, len + 1, PROXYMARKUP_START);
  memcpy(sbuf + written, buf, len);
  written += len;
  memcpy(sbuf + written, PROXYMARKUP_END, strlen(PROXYMARKUP_END));
  written += (int)strlen(PROXYMARKUP_END);
  sbuf[written] = 0;
  to_proxymous(written + 1, sbuf);
  clownix_free(sbuf, __FUNCTION__);
}

void proxycrun_transmit_c2c_del(char *name)
{
  char buf[MAX_PATH_LEN];
  memset(buf, 0, MAX_PATH_LEN);
  snprintf(buf, MAX_PATH_LEN-1, "transcrun_req_c2c_del %s", name);
  to_proxymous(strlen(buf)+1, buf);
}

void proxycrun_transmit_dist_tcp_ip_port(char *name, uint32_t ip,
                                         uint16_t port, char *passwd)
{
  char buf[MAX_PATH_LEN];
  memset(buf, 0, MAX_PATH_LEN);
  snprintf(buf, MAX_PATH_LEN-1,
  "transcrun_dist_tcp_ip_port %s %x %hu %s", name, ip, port, passwd);
  to_proxymous(strlen(buf)+1, buf);
} 
  
void proxycrun_transmit_dist_udp_ip_port(char *name,
                                         uint32_t ip, uint16_t port)
{
  char buf[MAX_PATH_LEN];
  memset(buf, 0, MAX_PATH_LEN); 
  snprintf(buf, MAX_PATH_LEN-1,
  "transcrun_dist_udp_ip_port %s %x %hu", name, ip, port);
  to_proxymous(strlen(buf)+1, buf);
}

void proxycrun_transmit_req_udp(char *name, int c2c_udp_port_low)
{
  char buf[MAX_PATH_LEN];
  uint16_t port = (uint16_t) c2c_udp_port_low;
  memset(buf, 0, MAX_PATH_LEN); 
  snprintf(buf, MAX_PATH_LEN-1, "transcrun_req_udp %s %hu", name, port);
  to_proxymous(strlen(buf)+1, buf);
}

void proxycrun_transmit_req_nat(char *name, int on, char *ports)
{
  char buf[2*MAX_PATH_LEN];
  memset(buf, 0, 2*MAX_PATH_LEN);
  if (on)
    snprintf(buf, 2*MAX_PATH_LEN-1, "transcrun_req_nat_on %s %s", name,
             ((ports) && (strlen(ports))) ? ports : "none");
  else
    snprintf(buf, 2*MAX_PATH_LEN-1, "transcrun_req_nat_off %s", name);
  to_proxymous(strlen(buf)+1, buf);
}

void proxycrun_transmit_req_nat_publish(char *name, char *ports)
{
  char buf[2*MAX_PATH_LEN];
  memset(buf, 0, 2*MAX_PATH_LEN);
  snprintf(buf, 2*MAX_PATH_LEN-1, "transcrun_req_nat_pub %s %s", name,
           ((ports) && (strlen(ports))) ? ports : "none");
  to_proxymous(strlen(buf)+1, buf);
}

static int proxycrun_transmit_config(char *passwd)
{
  char buf[MAX_PATH_LEN];
  int llid, result = -1;
  char *net = cfg_get_cloonix_name();
  int pmain = cfg_get_server_port();
  int pjsweb = cfg_get_web_port();
  llid = proxy_sig_client(g_proxy_sig_unix, proxy_sig_err_cb, proxy_sig_rx_cb);
  if (llid == 0)
    KERR("ERROR %s", g_proxy_sig_unix);
  else
    {
    g_llid_proxy_sig = llid;
    llid_trace_alloc(g_llid_proxy_sig,"CLOON",0,0,type_llid_trace_unix_proxy);
    memset(buf, 0, MAX_PATH_LEN); 
    snprintf(buf,MAX_PATH_LEN-1,"transcrun_set_config %s %d %d %s",
             net,pmain,pjsweb,passwd);
    proxy_sig_tx(g_llid_proxy_sig, strlen(buf)+1, buf);
    result = 0; 
    }
  return result;
}

static void timer_proxycrun(void *data)
{
  if (proxycrun_transmit_config(g_passwd))
    {
    clownix_timeout_add(50, timer_proxycrun, NULL, NULL, NULL);
    g_reconnect_count += 1;
    if (g_reconnect_count > 20)
      {
      KERR("ERROR IMPOSSIBLE SIGCRUN CONNECTION TO PROXYMOUS after %d "
           "tries, giving up", g_reconnect_count);
      if (!get_glob_req_self_destruction())
        recv_kill_uml_clownix(0, 0);
      }
    }
  else if (g_reconnect_count)
    {
    KTRC("connected to proxymous after %d tries", g_reconnect_count);
    g_reconnect_count = 0;
    }
}

int proxycrun_connect_proxy_ok(void)
{
  return g_connect_proxy_ok;
}

static void timer_kill_proxycrun(void *data)
{
  if ((g_llid_proxy_sig) && (msg_exist_channel(g_llid_proxy_sig)))
    {
    msg_delete_channel(g_llid_proxy_sig);
    g_llid_proxy_sig = 0;
    }
}

void proxycrun_kill(void)
{
  if ((g_llid_proxy_sig) && (msg_exist_channel(g_llid_proxy_sig)))
    {
    channel_rx_local_flow_ctrl(g_llid_proxy_sig, 1);
    channel_tx_local_flow_ctrl(g_llid_proxy_sig, 1);
    clownix_timeout_add(50, timer_kill_proxycrun, NULL, NULL, NULL);
    }
}

void proxycrun_init(char *passwd)
{
  char *net = cfg_get_cloonix_name();
  g_connect_proxy_ok = 0;
  g_llid_proxy_sig = 0;
  g_reconnect_count = 0;
  memset(g_proxy_sig_unix, 0, MAX_PATH_LEN-1);
  snprintf(g_proxy_sig_unix, MAX_PATH_LEN-1,
           "%s_%s/proxymous_sig_stream.sock", PROXYSHARE_IN, net);
  memset(g_passwd, 0, MSG_DIGEST_LEN);
  strncpy(g_passwd, passwd, MSG_DIGEST_LEN-1);
  g_proxy_seen_ms = stall_now_ms();
  stall_init(&g_stall, WATCH_BEAT_MS, WATCH_LIMIT_MS);
  clownix_timeout_add(10, timer_proxycrun, NULL, NULL, NULL);
  clownix_timeout_add(10, timer_watch_proxymous, NULL, NULL, NULL);
}
