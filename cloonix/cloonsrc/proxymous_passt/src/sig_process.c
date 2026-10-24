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
#include "stall_detect.h"
#include "rpc_clownix.h"
#include "doorways_sock.h"
#include "proxy_crun.h"
#include "util_sock.h"
#include "c2c_peer.h"
#include "nat_main.h"
#include "passt_spawn.h"

static int g_heartbeat_count;
#define BEAT_PERIOD_MS 1000
#define BEAT_LIMIT_MS  20000
static t_stall g_stall;
static int g_stall_init_done;
void write_start_status_file(int is_ready);

static void sig_process_tx_to_switch(char *msg)
{
  int llid_sig = get_llid_sig();
  if ((llid_sig) && (msg_exist_channel(llid_sig)))
    proxy_sig_tx(llid_sig, strlen(msg)+1, msg);
  else
    KERR("ERROR cannot tell the switch: %s", msg);
}

void sig_process_heartbeat(void)
{
  char *beat_req = "transcrun_proxy_heartbeat_req";
  int llid_sig = get_llid_sig();
  int gap, credit_beats;
  if (!g_stall_init_done)
    {
    stall_init(&g_stall, BEAT_PERIOD_MS, BEAT_LIMIT_MS);
    g_stall_init_done = 1;
    }
  gap = stall_beat(&g_stall);
  if (llid_sig)
    {
    if (!msg_exist_channel(llid_sig))
      rpct_recv_kil_req(0, 0);
    proxy_sig_tx(llid_sig, strlen(beat_req)+1, beat_req);
    /* Say what these mean: the switch is not answering. */
    if (gap > 0)
      KERR("WARNING this loop did not beat for %d ms (%s): %d beat(s) were "
           "never sent, so they are not counted against the switch", gap,
           stall_why(&g_stall), gap / BEAT_PERIOD_MS);
    credit_beats = stall_credit_ms(&g_stall) / BEAT_PERIOD_MS;
    if (g_heartbeat_count == 5 + credit_beats)
      KERR("WARNING the switch has not answered 5 heartbeats (%d forgiven)",
           credit_beats);
    if (g_heartbeat_count == 10 + credit_beats)
      KERR("WARNING the switch has not answered 10 heartbeats (%d "
           "forgiven), leaving at 20", credit_beats);
    if (g_heartbeat_count >= 20 + credit_beats)
      KOUT("ERROR the switch has not answered 20 heartbeats of being given "
           "the chance (%d more forgiven as this process was not running): "
           "it is gone or wedged, so this proxymous exits", credit_beats);
    g_heartbeat_count += 1;
    }
}

void sig_process_proxy_peer_data_to_crun(char *name, int len, char *buf)
{
  int hdrlen = (int)(strlen("transcrun_proxy_peer_data_to_crun ")
                     + MAX_NAME_LEN + 12
                     + strlen(PROXYMARKUP_START)
                     + strlen(PROXYMARKUP_END) + 4);
  int sbuflen = hdrlen + len + 1;
  char *sbuf = (char *) clownix_malloc(sbuflen, 1);
  int written;
  int llid_sig = get_llid_sig();
  memset(sbuf, 0, sbuflen);
  written = snprintf(sbuf, sbuflen,
                     "transcrun_proxy_peer_data_to_crun %s %d %s",
                     name, len + 1, PROXYMARKUP_START);
  memcpy(sbuf + written, buf, len);
  written += len;
  memcpy(sbuf + written, PROXYMARKUP_END, strlen(PROXYMARKUP_END));
  written += (int)strlen(PROXYMARKUP_END);
  sbuf[written] = 0;
  if (!msg_exist_channel(llid_sig))
    KERR("ERROR c2c %s: the data of the peer net not passed to the switch, "
         "no channel to it", name);
  else
    proxy_sig_tx(llid_sig, written + 1, sbuf);
  clownix_free(sbuf, __FUNCTION__);
}

void sig_process_tx_proxy_callback_connect(char *name, int pair_llid)
{
  char buf[MAX_PATH_LEN];
  memset(buf, 0, MAX_PATH_LEN);
  snprintf(buf, MAX_PATH_LEN-1,
  "transcrun_proxy_callback_connect %s %d", name, pair_llid);
  sig_process_tx_to_switch(buf);
}

static void dist_udp_ip_port_ok(char *name, uint32_t ip, uint16_t udp_port)
{   
  char buf[MAX_PATH_LEN];
  memset(buf, 0, MAX_PATH_LEN);
  snprintf(buf, MAX_PATH_LEN-1,
  "transcrun_dist_udp_ip_port_OK %s %x %hu", name, ip, udp_port);
  sig_process_tx_to_switch(buf);
}

static void transcrun_proxy_data_from_crun_status(char *name, int status)
{
  char buf[MAX_PATH_LEN];
  memset(buf, 0, MAX_PATH_LEN);
  snprintf(buf, MAX_PATH_LEN-1, "transcrun_proxy_data_from_crun_%s %s",
           status ? "ko" : "ok", name);
  sig_process_tx_to_switch(buf);
}

void sig_process_rx(char *proxyshare, char *buf)
{
  char name[MAX_NAME_LEN];
  char ports[MAX_PATH_LEN];
  char passwd[MSG_DIGEST_LEN];
  uint16_t udp_port, tcp_port;
  uint32_t ip;
  int16_t pmain;
  int16_t pjsweb;
  char *ptrs, *ptre;
  char sbuf[MAX_PATH_LEN];
  int len, max = MAX_PATH_LEN-1;
  uint16_t c2c_udp_port_low;

  if (!strcmp(buf, "transcrun_write_start_status_file_ready"))
    {
    write_start_status_file(1);
    }
  else if (!strcmp(buf, "transcrun_proxy_heartbeat_resp"))
    {
    g_heartbeat_count = 0;
    stall_answered(&g_stall);
    }
  else if (sscanf(buf,
  "transcrun_set_config %63s %hd %hd %31s",
  name, &pmain, &pjsweb, passwd) == 4)
    {
    if (set_config(name, pmain, pjsweb, passwd))
      KOUT("ERROR %s", buf);
    }
  else if (sscanf(buf,
  "transcrun_req_nat_on %63s %299s", name, ports) == 2)
    {
    if (nat_main_request(name, 1, ports, 0))
      {
      /* Tell the switch, not just the local log */
      char resp[MAX_PATH_LEN];
      KERR("ERROR %s", buf);
      memset(resp, 0, MAX_PATH_LEN);
      snprintf(resp, MAX_PATH_LEN-1, "transcrun_req_nat_on_KO %s", name);
      sig_process_tx_to_switch(resp);
      }
    }
  else if (sscanf(buf,
  "transcrun_req_nat_pub %63s %299s", name, ports) == 2)
    {
    if (nat_main_request(name, 1, ports, 1))
      {
      /* The KO first, the respawn on the previous ports after: the
         switch must not take the passt that comes back for the new one. */
      char resp[MAX_PATH_LEN];
      t_ctx_nat *cur = get_ctx_nat(name);
      memset(resp, 0, MAX_PATH_LEN);
      snprintf(resp, MAX_PATH_LEN-1, "transcrun_req_nat_pub_KO %s", name);
      sig_process_tx_to_switch(resp);
      if ((cur) && (passt_spawn_start(cur->passt_sock_path, cur->ports,
                                      "back on the previous ports") < 0))
        KERR("ERROR %s: passt could not be put back on %s", name, cur->ports);
      }
    }
  else if (sscanf(buf,
  "transcrun_req_nat_off %63s", name) == 1)
    {
    if (nat_main_request(name, 0, "none", 0))
      KERR("ERROR %s", buf);
    }
  else if (sscanf(buf,
  "transcrun_req_c2c_del %63s", name) == 1)
    {
    c2c_peer_delete(name);
    }
  else if (sscanf(buf,
  "transcrun_dist_tcp_ip_port %63s %x %hu %31s",name,&ip,&tcp_port,passwd)==4)
    {
    c2c_peer_dist_tcp_ip_port(name, ip, tcp_port, passwd);
    }
  else if (sscanf(buf,
  "transcrun_proxy_data_from_crun %63s %d", name, &len) == 2)
    {
    ptrs = strstr(buf, PROXYMARKUP_START);
    ptre = strstr(buf, PROXYMARKUP_END);
    if ((!ptrs) || (!ptre))
      {
      KERR("ERROR c2c %s: data from the switch without its markers, "
           "dropped", name);
      transcrun_proxy_data_from_crun_status(name, -1);
      }
    else
      {
      ptrs = ptrs + strlen(PROXYMARKUP_START);
      ptre[0] = 0;
      if ((int) (strlen(ptrs) + 1) != len)
        {
        KERR("ERROR c2c %s: data from the switch of %d bytes, %d announced, "
             "dropped", name, (int) strlen(ptrs) + 1, len);
        transcrun_proxy_data_from_crun_status(name, -1);
        }
      else
        transcrun_proxy_data_from_crun_status(name,
                              c2c_peer_proxy_data_from_crun(name, len, ptrs));
      }
    }
  else if (sscanf(buf,
  "transcrun_req_udp %63s %hu", name, &c2c_udp_port_low) == 2)
    {
    memset(sbuf, 0, MAX_PATH_LEN);
    if (udp_proxy_init_for(name, proxyshare, &udp_port, c2c_udp_port_low))
      snprintf(sbuf,max,"transcrun_req_udp_KO %s", name);
    else
      snprintf(sbuf,max,"transcrun_req_udp_OK %s %hu", name, udp_port);
    sig_process_tx_to_switch(sbuf);
    }
  else if (sscanf(buf,
  "transcrun_dist_udp_ip_port %63s %x %hu", name, &ip, &udp_port) == 3)
    {
    udp_proxy_dist_udp_ip_port(name, ip, udp_port);
    dist_udp_ip_port_ok(name, ip, udp_port);
    }
  else
    KOUT("ERROR %s", buf);
}
