/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
/* cloonix_gui_server.c (produces binary: cloonix_gui_server) Three subsystems
   in one process */

/* cloonix RPC + client layers — included first so topo_web.c sees all types */
#include "io_clownix.h"
#include "doorways_sock.h"
#include "rpc_clownix.h"
#include "json_min.h"
#include "lib_topo.h"
#include "cloonix_conf_info.h"
#include "pthexec.h"

/* Defined further down, used by the included topo_web.c. */
t_cloonix_conf_info *get_own_cloonix_conf_info(void);

static char g_net_name[MAX_NAME_LEN];

/* The web server fragments, one translation unit. */
#include "web/web_common.h"
#include "web/web_ws.c"
#include "web/web_ctl.c"
#include "web/web_vnc.c"
#include "web/web_pty.c"
#include "web/web_ctl_post.c"
#include "web/web_ctl_play.c"
#include "web/web_ctl_rec.c"
#include "web/web_auth.c"
#include "web/web_http.c"
#include "topo_web.c"

static int  g_ctrl_llid;
static int  g_watchdog_ok;
static void timer_watchdog(void *data);
static int  g_watchdog_started;

t_cloonix_conf_info *get_own_cloonix_conf_info(void)
{
  static t_cloonix_conf_info *g_own = NULL;
  int rank;
  if (!g_own)
    g_own = cloonix_cnf_info_get(g_net_name, &rank);
  return g_own;
}

void rpct_recv_pid_req(int llid, int tid, char *name, int num)
{
  if (llid != g_ctrl_llid)
    KERR("ERROR pid request on llid %d, the control llid is %d", llid, g_ctrl_llid);
  rpct_send_pid_resp(llid, tid, name, num, cloonix_get_pid(), getpid());
  g_watchdog_ok = 1;
}

void rpct_recv_kil_req(int llid, int tid)
{
  KTRC("kill request received, exiting");
  if (g_srv_fd >= 0) close(g_srv_fd);
  exit(0);
}

static void err_ctrl_cb(int llid, int err, int from)
{
  KERR("WARNING the link to the switch closed (err %d from %d), exiting", err, from);
  if (g_srv_fd >= 0) close(g_srv_fd);
  exit(0);
}

static void rx_ctrl_cb(int llid, int len, char *buf)
{
  if (rpct_decoder(llid, len, buf))
    KOUT("rpct_decoder error");
}

static void connect_from_ctrl_client(int llid, int llid_new)
{
  msg_mngt_set_callbacks(llid_new, err_ctrl_cb, rx_ctrl_cb);
  if (g_ctrl_llid && msg_exist_channel(g_ctrl_llid))
    msg_delete_channel(g_ctrl_llid);
  g_ctrl_llid = llid_new;
  g_watchdog_ok = 1;
  if (!g_watchdog_started)
    {
    g_watchdog_started = 1;
    clownix_timeout_add(200, timer_watchdog, NULL, NULL, NULL);
    }
}

static void timer_watchdog(void *data)
{
  static int count = 0;
  count += 1;
  if (g_watchdog_ok)
    count = 0;
  if (count >= 60)
    {
    KERR("WARNING no heartbeat from the switch for 12 s, exiting");
    if (g_srv_fd >= 0) close(g_srv_fd);
    exit(0);
    }
  g_watchdog_ok = 0;
  clownix_timeout_add(200, timer_watchdog, NULL, NULL, NULL);
}

static void heartbeat(int delta)
{
}

int main(int argc, char *argv[])
{
  char ctrl_path[MAX_PATH_LEN];
  char doors_path[MAX_PATH_LEN];
  char passwd[MSG_DIGEST_LEN];
  char *snf_dir, *web_port_str;
  int   web_port;
  pthread_t http_tid;

  if (argc != 6)
    {
    fprintf(stderr,
      "Usage: cloonix_gui_server <snf_dir> <web_port>"
      " <ctrl_sock_path> <doors_path> <passwd>\n");
    return 1;
    }

  snf_dir      = argv[1];
  web_port_str = argv[2];
  snprintf(ctrl_path,  MAX_PATH_LEN, "%s", argv[3]);
  snprintf(doors_path, MAX_PATH_LEN, "%s", argv[4]);
  snprintf(passwd,     MSG_DIGEST_LEN, "%s", argv[5]);

  if (sscanf(web_port_str, "%d", &web_port) != 1 || web_port <= 0)
    {
    fprintf(stderr, "Bad web_port: %s\n", web_port_str);
    return 1;
    }

  {
    char tmp[MAX_PATH_LEN];
    snprintf(tmp, MAX_PATH_LEN, "%s", ctrl_path);
    char *slash = strrchr(tmp, '/');
    if (slash) *slash = '\0';
    slash = strrchr(tmp, '/');
    if (slash) snprintf(g_net_name, MAX_NAME_LEN, "%s", slash+1);
    else       snprintf(g_net_name, MAX_NAME_LEN, "%s", ctrl_path);
  }

  pthexec_init();
  if (cloonix_conf_info_init(pthexec_cloonix_cfg()))
    KERR("WARNING could not load cluster config %s (c2c unavailable, and "
         "the web port will have no password)", pthexec_cloonix_cfg());

  {
    char jsweb_unix[MAX_PATH_LEN];
    snprintf(jsweb_unix, MAX_PATH_LEN-1, "%s_%s/proxy_pjsweb.sock",
             PROXYSHARE_IN, g_net_name);
    if (http_server_init(snf_dir, jsweb_unix) < 0)
      {
      fprintf(stderr, "%s: http_server_init failed sock=%s\n",
              g_net_name, jsweb_unix);
      return 1;
      }
  }

  umask(0000);
  cloonix_set_pid(getpid());
  {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sighandler;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT,  &sa, NULL);
  }
  msg_mngt_init("gui_server", IO_MAX_BUF_LEN);
  set_cloonix_name(g_net_name);
  msg_mngt_heartbeat_init(heartbeat);
  doorways_sock_init(0);

  topo_web_init(doors_path, passwd);

  unlink(ctrl_path);
  string_server_unix(ctrl_path, connect_from_ctrl_client, "ctrl");

  if (pthread_create(&http_tid, NULL, http_server_thread, NULL) != 0)
    KOUT("pthread_create http_server_thread failed");
  pthread_detach(http_tid);

  msg_mngt_loop();

  KERR("ERROR the event loop returned");
  if (g_srv_fd >= 0) close(g_srv_fd);
  return 0;
}

void recv_event_topo(int llid, int tid, t_topo_info *topo)
{
  (void)llid;
  topo_web_recv_topo(tid, topo);
}

void recv_status_ok(int llid, int tid, char *t)  {(void)llid;(void)tid;(void)t;}
void recv_status_ko(int llid, int tid, char *r)  {(void)llid;(void)tid;(void)r;}
void recv_slowperiodic_qcow2(int llid, int tid, int nb, t_slowperiodic *s)
  { (void)llid;(void)tid; slowperiodic_qcow2_cb(nb, s); }
void recv_slowperiodic_cnt(int llid, int tid, int nb, t_slowperiodic *s)
  { (void)llid;(void)tid; slowperiodic_cnt_cb(nb, s); }
void recv_sync_wireshark_resp(int llid, int tid, char *name, int num, int status)
{
  /* The snf grants with status 2, the switch refuses with status 1: 0 never
     comes. */
  char buf[512];
  int len;
  (void)llid; (void)tid;
  if (status != 1)
    {
    len = snprintf(buf, sizeof(buf),
      "{\"type\":\"webshark_ready\",\"name\":\"%s\",\"num\":%d}", name, num);
    }
  else
    {
    len = snprintf(buf, sizeof(buf),
      "{\"type\":\"webshark_error\",\"name\":\"%s\",\"num\":%d}", name, num);
    }
  if (len > 0) ws_broadcast_topo_json(buf, len);
}

