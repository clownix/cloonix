/* topo_web.c — included into cloonix_gui_server.c after the web_*.c fragments
   and all cloonix headers. */

/* Defined further down. Included into cloonix_gui_server.c: no includes. */
static void topo_seed_vmstate(const char *name, int ping_ok);
static void prune_vm_state(t_topo_info *topo);
static int g_topo_was_lost;
static int g_said_not_ready;

void ws_broadcast_topo_json(const char *json, int len);
void ws_send_text(int fd, const char *msg, int len);
void ws_broadcast_images_json(const char *json, int len);
void topo_web_send_images_snapshot(int cfd);
void topo_web_layout_forget_all(void);
void ws_broadcast_activity_json(const char *json, int len);

static t_topo_info *g_topo_current      = NULL;
static int          g_topo_llid         = 0;
static int          g_topo_connect_llid = 0;
static int          g_topo_cb_registered = 0;
static char         g_topo_passwd[MSG_DIGEST_LEN];
static char         g_topo_doors_path[MAX_PATH_LEN];
static void timer_topo_connect(void *data);
static const char *etype_word(int endp_type)
{
  switch (endp_type)
    {
    case endp_type_eths:  case endp_type_taps:  case endp_type_nats:
    case endp_type_c2cs:  case endp_type_phyas: case endp_type_phyms:
    case endp_type_a2b:
      return "eths";
    case endp_type_ethv:  case endp_type_tapv:  case endp_type_natv:
    case endp_type_c2cv:  case endp_type_phyav: case endp_type_phymv:
      return "ethv";
    default:
      return "none";
    }
}

static char *topo_to_json(t_topo_info *topo, int *out_len)
{
  int i, j, used = 0, cap = 65536;
  char *buf = (char *) clownix_malloc(cap, 1);
  if (!buf)
    {
    KERR("ERROR topo: no memory for the json buffer");
    *out_len = 0; return NULL;
    }

#define TW_APPEND(fmt, ...) \
  do { \
    int _n = snprintf(buf + used, cap - used, fmt, ##__VA_ARGS__); \
    if (_n < 0) { clownix_free(buf, __FUNCTION__); *out_len = 0; return NULL; } \
    if (used + _n >= cap) { \
      int _newcap = (used + _n + 1) * 2; \
      char *_nb = (char *) clownix_malloc(_newcap, 1); \
      if (!_nb) { clownix_free(buf, __FUNCTION__); *out_len = 0; return NULL; } \
      memcpy(_nb, buf, used); \
      clownix_free(buf, __FUNCTION__); \
      buf = _nb; cap = _newcap; \
      snprintf(buf + used, cap - used, fmt, ##__VA_ARGS__); \
    } \
    used += _n; \
  } while (0)

  TW_APPEND("{\"type\":\"topo\",\"net\":\"%s\"", topo->clc.network);

  prune_vm_state(topo);

  TW_APPEND(",\"kvm\":[");
  for (i = 0; i < topo->nb_kvm; i++)
    {
    topo_seed_vmstate(topo->kvm[i].name, topo->kvm[i].ping_ok);
    int e;
    t_topo_kvm *k = &topo->kvm[i];
    /* the wlans follow the eths, etype "wlan", each on its air */
    TW_APPEND("%s{\"name\":\"%s\",\"nb_eth\":%d,\"vm_id\":%d,"
              "\"cpu\":%d,\"mem\":%d,\"rootfs\":\"%s\","
              "\"rootfs_used\":\"%s\",\"rootfs_backing\":\"%s\","
              "\"vm_config_flags\":%d,\"vhost\":[",
              i ? "," : "", k->name, k->nb_tot_eth + k->nb_tot_nb_vwif,
              k->vm_id, k->cpu, k->mem, k->rootfs_input,
              k->rootfs_used, k->rootfs_backing, k->vm_config_flags);
    for (e = 0; e < k->nb_tot_eth + k->nb_tot_nb_vwif; e++)
      TW_APPEND("%s\"%s\"", e ? "," : "",
                e < k->nb_tot_eth ? k->eth_table[e].vhost_ifname : "");
    TW_APPEND("],\"etype\":[");
    for (e = 0; e < k->nb_tot_eth + k->nb_tot_nb_vwif; e++)
      TW_APPEND("%s\"%s\"", e ? "," : "",
        e >= k->nb_tot_eth ? "wlan" :
        k->eth_table[e].endp_type == endp_type_eths ? "eths" :
        k->eth_table[e].endp_type == endp_type_ethv ? "ethv" : "none");
    TW_APPEND("]}");
    }
  TW_APPEND("]");

  TW_APPEND(",\"cnt\":[");
  for (i = 0; i < topo->nb_cnt; i++)
    {
    int e;
    t_topo_cnt *c = &topo->cnt[i];
    /* Seed the colour from the topology, not only from events. */
    topo_seed_vmstate(c->name, c->ping_ok);
    /* the wlans follow the eths, etype "wlan", each on its air */
    TW_APPEND("%s{\"name\":\"%s\",\"nb_eth\":%d,\"vm_id\":%d,"
              "\"brandtype\":\"%s\",\"image\":\"%s\",\"persistent\":%d,\"vhost\":[",
              i ? "," : "", c->name, c->nb_tot_eth + c->nb_tot_nb_vwif,
              c->vm_id, c->brandtype, c->image, c->is_persistent);
    for (e = 0; e < c->nb_tot_eth + c->nb_tot_nb_vwif; e++)
      TW_APPEND("%s\"%s\"", e ? "," : "",
                e < c->nb_tot_eth ? c->eth_table[e].vhost_ifname : "");
    TW_APPEND("],\"etype\":[");
    for (e = 0; e < c->nb_tot_eth + c->nb_tot_nb_vwif; e++)
      TW_APPEND("%s\"%s\"", e ? "," : "",
        e >= c->nb_tot_eth ? "wlan" :
        c->eth_table[e].endp_type == endp_type_eths ? "eths" :
        c->eth_table[e].endp_type == endp_type_ethv ? "ethv" : "none");
    TW_APPEND("]}");
    }
  TW_APPEND("]");

  TW_APPEND(",\"info_phy\":[");
  for (i = 0; i < topo->nb_info_phy; i++)
    TW_APPEND("%s\"%s\"", i ? "," : "", topo->info_phy[i].name);
  TW_APPEND("]");

  TW_APPEND(",\"lan\":[");
  for (i = 0; i < topo->nb_bridges; i++)
    {
    TW_APPEND("%s\"%s\"", i ? "," : "", topo->bridges[i].br);
    }
  TW_APPEND("]");

  /* tap, nat, a2b, c2c, phy: one array each in t_topo_info; topo->endp
     carries only the lan attachments. */
  TW_APPEND(",\"endp\":[");
  {
  int first_endp = 1;
  for (i = 0; i < topo->nb_tap; i++)
    {
    TW_APPEND("%s{\"name\":\"%s\",\"kind\":\"tap\",\"num\":0,"
              "\"etype\":[\"%s\"]}",
              first_endp ? "" : ",", topo->tap[i].name,
              etype_word(topo->tap[i].endp_type));
    first_endp = 0;
    }
  for (i = 0; i < topo->nb_nat; i++)
    {
    TW_APPEND("%s{\"name\":\"%s\",\"kind\":\"nat\",\"num\":0,"
              "\"etype\":[\"%s\"]}",
              first_endp ? "" : ",", topo->nat[i].name,
              etype_word(topo->nat[i].endp_type));
    first_endp = 0;
    }
  for (i = 0; i < topo->nb_a2b; i++)
    {
    TW_APPEND("%s{\"name\":\"%s\",\"kind\":\"a2b\",\"num\":0,"
              "\"etype\":[\"%s\",\"%s\"]}",
              first_endp ? "" : ",", topo->a2b[i].name,
              etype_word(topo->a2b[i].endp_type0),
              etype_word(topo->a2b[i].endp_type1));
    first_endp = 0;
    }
  for (i = 0; i < topo->nb_c2c; i++)
    {
    TW_APPEND("%s{\"name\":\"%s\",\"kind\":\"c2c\",\"num\":0,"
              "\"tcp_peered\":%d,\"udp_peered\":%d,\"etype\":[\"%s\"]}",
              first_endp ? "" : ",", topo->c2c[i].name,
              topo->c2c[i].tcp_connection_peered,
              topo->c2c[i].udp_connection_peered,
              etype_word(topo->c2c[i].endp_type));
    first_endp = 0;
    }
  for (i = 0; i < topo->nb_phy; i++)
    {
    TW_APPEND("%s{\"name\":\"%s\",\"kind\":\"phy\",\"num\":0,"
              "\"etype\":[\"%s\"],\"phykind\":\"%s\"}",
              first_endp ? "" : ",", topo->phy[i].name,
              etype_word(topo->phy[i].endp_type),
              ((topo->phy[i].endp_type == endp_type_phyms) ||
               (topo->phy[i].endp_type == endp_type_phymv)) ? "macvlan"
                                                              : "absorb");
    first_endp = 0;
    }
  }
  TW_APPEND("]");

  /* Edges: endp = { name, num, lan[] } — one entry per attached interface */
  TW_APPEND(",\"edge\":[");
  int first_edge = 1;
  for (i = 0; i < topo->nb_endp; i++)
    {
    t_topo_endp *ep = &topo->endp[i];
    for (j = 0; j < ep->lan.nb_lan; j++)
      {
      TW_APPEND("%s{\"name\":\"%s\",\"num\":%d,\"lan\":\"%s\"}",
                first_edge ? "" : ",",
                ep->name, ep->num, ep->lan.lan[j].lan);
      first_edge = 0;
      }
    }
  for (i = 0; i < topo->nb_kvm; i++)
    {
    t_topo_kvm *k = &topo->kvm[i];
    for (j = 0; (j < k->nb_tot_nb_vwif) && (j < MAX_VWIF); j++)
      {
      if (!k->vwif_air[j][0])
        continue;
      TW_APPEND("%s{\"name\":\"%s\",\"num\":%d,\"lan\":\"%s\"}",
                first_edge ? "" : ",", k->name, k->nb_tot_eth + j,
                k->vwif_air[j]);
      first_edge = 0;
      }
    }
  for (i = 0; i < topo->nb_cnt; i++)
    {
    t_topo_cnt *c = &topo->cnt[i];
    for (j = 0; (j < c->nb_tot_nb_vwif) && (j < MAX_VWIF); j++)
      {
      if (!c->vwif_air[j][0])
        continue;
      TW_APPEND("%s{\"name\":\"%s\",\"num\":%d,\"lan\":\"%s\"}",
                first_edge ? "" : ",", c->name, c->nb_tot_eth + j,
                c->vwif_air[j]);
      first_edge = 0;
      }
    }
  TW_APPEND("]");

  /* each air once, its capture as an eth's: the radios on it carry it */
  TW_APPEND(",\"air\":[");
  {
  char seen[64][MAX_NAME_LEN];
  int nb_seen = 0, k;
  for (i = 0; i < topo->nb_kvm + topo->nb_cnt; i++)
    {
    int nb   = (i < topo->nb_kvm) ? topo->kvm[i].nb_tot_nb_vwif
                                  : topo->cnt[i - topo->nb_kvm].nb_tot_nb_vwif;
    char (*air)[MAX_NAME_LEN] = (i < topo->nb_kvm) ? topo->kvm[i].vwif_air
                              : topo->cnt[i - topo->nb_kvm].vwif_air;
    int *endp = (i < topo->nb_kvm) ? topo->kvm[i].vwif_endp_type
                                   : topo->cnt[i - topo->nb_kvm].vwif_endp_type;
    for (j = 0; (j < nb) && (j < MAX_VWIF); j++)
      {
      if (!air[j][0])
        continue;
      for (k = 0; (k < nb_seen) && strcmp(seen[k], air[j]); k++);
      if (k < nb_seen)
        continue;
      if (nb_seen >= (int) (sizeof(seen)/sizeof(seen[0])))
        {
        KERR("WARNING topo: more than %d airs, %s not given to the canvas",
             nb_seen, air[j]);
        continue;
        }
      strncpy(seen[nb_seen], air[j], MAX_NAME_LEN-1);
      seen[nb_seen][MAX_NAME_LEN-1] = 0;
      TW_APPEND("%s{\"name\":\"%s\",\"etype\":\"%s\"}", nb_seen ? "," : "",
                air[j], endp[j] == endp_type_eths ? "eths" : "ethv");
      nb_seen += 1;
      }
    }
  }
  TW_APPEND("]");

  TW_APPEND("}");
  *out_len = used;
  return buf;
#undef TW_APPEND
}

#define MAX_PENDING_LANS 64
static char g_pending_lans[MAX_PENDING_LANS][MAX_NAME_LEN];
static int  g_nb_pending_lans = 0;

static void broadcast_pending_lans(void)
{
  int i, used = 0, cap = 4096;
  char *buf = (char *) clownix_malloc(cap, 1);
  if (!buf) return;
  used = snprintf(buf, cap, "{\"type\":\"pending_lans\",\"lans\":[");
  for (i = 0; i < g_nb_pending_lans; i++)
    {
    int n = snprintf(buf+used, cap-used, "%s\"%s\"",
                     i ? "," : "", g_pending_lans[i]);
    if (n > 0) used += n;
    }
  used += snprintf(buf+used, cap-used, "]}");
  ws_broadcast_topo_json(buf, used);
  clownix_free(buf, __FUNCTION__);
}

static void pending_lan_promote(const char *name)
{
  int i;
  for (i = 0; i < g_nb_pending_lans; i++)
    {
    if (!strcmp(g_pending_lans[i], name))
      {
      memmove(&g_pending_lans[i], &g_pending_lans[i+1],
              (g_nb_pending_lans - i - 1) * MAX_NAME_LEN);
      g_nb_pending_lans--;
      broadcast_pending_lans();
      return;
      }
    }
}

static void timer_apply_topo(void *data)
{
  t_topo_info *topo = (t_topo_info *) data;
  int len = 0;
  char *json;
  if ((!g_topo_current) || topo_compare(topo, g_topo_current))
    {
    int i;
    topo_free_topo(g_topo_current);
    g_topo_current = topo_duplicate(topo);
    if ((topo->nb_kvm + topo->nb_cnt + topo->nb_c2c + topo->nb_nat +
         topo->nb_tap + topo->nb_a2b + topo->nb_phy + topo->nb_bridges) == 0)
      topo_web_layout_forget_all();
    for (i = 0; i < topo->nb_bridges; i++)
      pending_lan_promote(topo->bridges[i].br);
    json = topo_to_json(topo, &len);
    if (json)
      {
      ws_broadcast_topo_json(json, len);
      clownix_free(json, __FUNCTION__);
      }
    else
      KERR("ERROR topo_to_json returned NULL");
    }
  topo_free_topo(topo);
}

static void topo_cb(int tid, t_topo_info *topo)
{
  t_topo_info *ntopo = topo_duplicate(topo);
  clownix_timeout_add(1, timer_apply_topo, (void *) ntopo, NULL, NULL);
}

static void topo_web_recv_topo(int tid, t_topo_info *topo)
{
  if (g_topo_cb_registered)
    topo_cb(tid, topo);
  else
    KERR("WARNING a topo came before its callback was registered, dropped");
}

void topo_doorways_tx(int llid, int len, char *buf)
{
  if (doorways_tx_bufraw(llid, 0, doors_type_switch, doors_val_none, len, buf))
    KERR("ERROR topo send failed on llid %d", llid);
}

static void topo_doors_rx(int llid, int tid,
                           int len_bufraw, char *doors_bufraw,
                           int type, int val, int len, char *buf)
{
  if (type == doors_type_switch)
    {
    if (val == doors_val_link_ok || val == doors_val_init_link)
      {
      }
    else if (val == doors_val_link_ko)
      {
      KERR("WARNING topo client: link lost on llid %d", llid);
      }
    else
      {
      int r1 = doors_io_basic_decoder(llid, len, buf);
      if (r1)
        {
        int r3 = rpct_decoder(llid, len, buf);
        if (r3)
          KERR("ERROR topo client: unknown message on llid %d, %d bytes", llid, len);
        }
      }
    }
  else
    KERR("ERROR topo client: unexpected doors type %d", type);
}

/* A beat on the topology socket, twice a second. */
#define TOPO_BEAT_MS 500

static void timer_topo_beat(void *data)
{
  char beat[128];
  int len;
  (void) data;
  len = snprintf(beat, sizeof(beat),
                 "{\"type\":\"beat\",\"srv\":%d,\"bld\":\"%s %s\"}",
                 g_topo_llid ? 1 : 0, __DATE__, __TIME__);
  {
  static int said;
  int nb = topo_ws_broadcast(beat, len);
  if ((nb > 0) && (!said))
    {
    said = 1;
    KTRC("the beat is running, %d browser(s) hearing it, every %d ms",
         nb, TOPO_BEAT_MS);
    }
  }
  clownix_timeout_add(TOPO_BEAT_MS / 10, timer_topo_beat, NULL, NULL, NULL);
}

static void topo_err_cb(int llid)
{
  KERR("WARNING topo client: connection closed llid=%d — will reconnect", llid);
  g_topo_was_lost = 1;
  g_topo_llid = 0;
  g_topo_connect_llid = 0;
  clownix_timeout_add(5, timer_topo_connect, NULL, NULL, NULL);
}

int topo_callback_connect(int llid, int fd)
{
  if (llid != g_topo_connect_llid)
    KERR("ERROR topo connect: llid %d where %d was expected", llid, g_topo_connect_llid);
  g_topo_connect_llid = 0;

  g_topo_llid = doorways_sock_client_inet_end(doors_type_switch, llid, fd,
                                               g_topo_passwd,
                                               topo_err_cb,
                                               topo_doors_rx);
  if (!g_topo_llid)
    {
    KERR("ERROR topo client: doorways_sock_client_inet_end failed");
    return 0;
    }
  if (doorways_sig_bufraw(g_topo_llid, llid, doors_type_switch,
                           doors_val_init_link, "OK"))
    KERR("ERROR topo client: doorways_sig_bufraw failed");

  g_topo_cb_registered = 1;
  send_event_topo_sub(g_topo_llid, 0);
  send_slowperiodic_sub(g_topo_llid, 0);
  send_eventfull_sub(g_topo_llid, 0);
  send_topo_small_event_sub(g_topo_llid, 0);
  if (g_topo_was_lost)
    {
    g_topo_was_lost = 0;
    KERR("WARNING topo client: reconnected llid=%d", g_topo_llid);
    }
  return 0;
}

static char g_images_json[65536];
static int  g_images_json_len = 0;

static void build_images_json(int nb_kvm, t_slowperiodic *kvm,
                               int nb_cnt, t_slowperiodic *cnt)
{
  int i, used = 0;
  int cap = (int)sizeof(g_images_json);
#define IJ_APPEND(fmt, ...) \
  do { int _n = snprintf(g_images_json+used, cap-used, fmt, ##__VA_ARGS__); \
       if (_n > 0 && used+_n < cap) used += _n; } while(0)

  IJ_APPEND("{\"type\":\"images\"");
  if (!pthexec_qemu_bin())
    IJ_APPEND(",\"kvm_absent\":1");
  IJ_APPEND(",\"kvm\":[");
  for (i = 0; i < nb_kvm; i++)
    IJ_APPEND("%s\"%s\"", i ? "," : "", kvm[i].name);
  IJ_APPEND("]");
  IJ_APPEND(",\"cnt\":[");
  for (i = 0; i < nb_cnt; i++)
    IJ_APPEND("%s\"%s\"", i ? "," : "", cnt[i].name);
  IJ_APPEND("]}");
  g_images_json_len = used;
#undef IJ_APPEND
}

#define MAX_SLOWP 128
static t_slowperiodic g_sp_kvm[MAX_SLOWP];
static t_slowperiodic g_sp_cnt[MAX_SLOWP];
static int g_nb_kvm = 0, g_nb_cnt = 0;

static void slowperiodic_qcow2_cb(int nb, t_slowperiodic *spic)
{
  g_nb_kvm = (nb < MAX_SLOWP) ? nb : MAX_SLOWP;
  memcpy(g_sp_kvm, spic, g_nb_kvm * sizeof(t_slowperiodic));
  build_images_json(g_nb_kvm, g_sp_kvm, g_nb_cnt, g_sp_cnt);
  if (g_images_json_len > 0)
    ws_broadcast_images_json(g_images_json, g_images_json_len);
}

static void slowperiodic_cnt_cb(int nb, t_slowperiodic *spic)
{
  g_nb_cnt = (nb < MAX_SLOWP) ? nb : MAX_SLOWP;
  memcpy(g_sp_cnt, spic, g_nb_cnt * sizeof(t_slowperiodic));
  build_images_json(g_nb_kvm, g_sp_kvm, g_nb_cnt, g_sp_cnt);
  if (g_images_json_len > 0)
    ws_broadcast_images_json(g_images_json, g_images_json_len);
}

void topo_web_send_images_snapshot(int cfd)
{
  if (g_images_json_len > 0)
    ws_send_text(cfd, g_images_json, g_images_json_len);
}

void topo_web_add_lan(char *name)
{
  int i;
  for (i = 0; i < g_nb_pending_lans; i++)
    if (!strcmp(g_pending_lans[i], name)) return;
  if (g_nb_pending_lans < MAX_PENDING_LANS)
    {
    strncpy(g_pending_lans[g_nb_pending_lans], name, MAX_NAME_LEN-1);
    g_nb_pending_lans++;
    }
  broadcast_pending_lans();
}

/* GET /ctl/commands: the dump commands of the switch, one a line,
   waited for here by the http thread, answered by the main loop. */
#define COMMANDS_WAIT_S 5
static pthread_mutex_t g_cmds_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  g_cmds_cond  = PTHREAD_COND_INITIALIZER;
static int   g_cmds_tid;
static int   g_cmds_done;
static char *g_cmds_text;

/* http threads never touch doorways (owned by the main loop): queued here,
   sent from the loop. */
typedef struct t_job
{
  void (*fn)(struct t_job *);
  char what[MAX_NAME_LEN];
  char s1[MAX_PATH_LEN];
  char s2[MAX_PATH_LEN];
  int i1;
  int i2;
  t_topo_kvm kvm;
  t_topo_cnt cnt;
  struct t_job *next;
} t_job;

static pthread_mutex_t g_job_mutex = PTHREAD_MUTEX_INITIALIZER;
static t_job *g_job_head;
static t_job *g_job_tail;
static int g_job_pipe[2] = { -1, -1 };

static t_job *job_new(void (*fn)(t_job *), const char *what)
{
  t_job *j = (t_job *) malloc(sizeof(t_job));
  if (!j)
    KOUT("ERROR malloc");
  memset(j, 0, sizeof(t_job));
  j->fn = fn;
  strncpy(j->what, what, MAX_NAME_LEN-1);
  return j;
}

static void job_post(t_job *j)
{
  char c = 0;
  pthread_mutex_lock(&g_job_mutex);
  if (g_job_tail)
    g_job_tail->next = j;
  else
    g_job_head = j;
  g_job_tail = j;
  pthread_mutex_unlock(&g_job_mutex);
  /* EAGAIN: the pipe is full, the main loop is woken already */
  if ((write(g_job_pipe[1], &c, 1) != 1) && (errno != EAGAIN))
    KERR("ERROR web request: the main loop was not woken: %s",
         strerror(errno));
}

/* main loop: every queued request, in order */
static int job_rx(int llid, int fd)
{
  char buf[256];
  int len, nb = 0;
  t_job *j, *next;
  (void) llid;
  while ((len = read(fd, buf, sizeof(buf))) > 0)
    nb += len;
  pthread_mutex_lock(&g_job_mutex);
  j = g_job_head;
  g_job_head = NULL;
  g_job_tail = NULL;
  pthread_mutex_unlock(&g_job_mutex);
  for (; j; j = next)
    {
    next = j->next;
    if (!g_topo_llid)
      KERR("WARNING %s: no connection to the server of the net", j->what);
    else
      j->fn(j);
    free(j);
    }
  return nb;
}

static void job_err(int llid, int err, int from)
{
  KOUT("ERROR the web request pipe broke: %d %d %d", llid, err, from);
}

static void job_init(void)
{
  if ((pipe(g_job_pipe)) ||
      (fcntl(g_job_pipe[0], F_SETFL, O_NONBLOCK)) ||
      (fcntl(g_job_pipe[1], F_SETFL, O_NONBLOCK)))
    KOUT("ERROR web request pipe: %s", strerror(errno));
  if (msg_watch_fd(g_job_pipe[0], job_rx, job_err, "webjob") <= 0)
    KOUT("ERROR web request pipe not watched");
}

static void do_add_vm(t_job *j)
{
  send_add_vm(g_topo_llid, 0, &(j->kvm));
}

static void do_add_cnt(t_job *j)
{
  send_cnt_add(g_topo_llid, 0, &(j->cnt));
}

static void do_commands(t_job *j)
{
  int tid;
  (void) j;
  pthread_mutex_lock(&g_cmds_mutex);
  tid = g_cmds_tid;
  pthread_mutex_unlock(&g_cmds_mutex);
  send_list_commands_req(g_topo_llid, tid, 0);
}

static void do_add_tap(t_job *j)
{
  send_tap_add(g_topo_llid, 0, j->s1);
}

static void do_add_nat(t_job *j)
{
  send_nat_add(g_topo_llid, 0, j->s1, "none");
}

static void do_add_phy(t_job *j)
{
  send_phy_add(g_topo_llid, 0, j->s1,
               j->i1 ? endp_type_phymv : endp_type_phyav);
}

static void do_add_a2b(t_job *j)
{
  send_a2b_add(g_topo_llid, 0, j->s1);
}

static void do_position(t_job *j)
{
  send_item_pos(g_topo_llid, 0, j->s1, j->i1, j->i2);
}

static void do_poseth(t_job *j)
{
  send_item_poseth(g_topo_llid, 0, j->s1, j->i1, j->i2);
}

static void do_note(t_job *j)
{
  send_item_note(g_topo_llid, 0, j->s1, j->s2);
}

static void do_cnf_a2b(t_job *j)
{
  send_a2b_cnf(g_topo_llid, 0, j->s1, j->s2);
}

static void do_add_c2c(t_job *j)
{
  int rank;
  char c2c_name[MAX_NAME_LEN];
  char dest[MAX_NAME_LEN];
  t_cloonix_conf_info *cnf, *local_cnf;
  t_rpct_tx saved_tx;
  memset(c2c_name, 0, MAX_NAME_LEN);
  memset(dest, 0, MAX_NAME_LEN);
  if (sscanf(j->s1, "%63s %63s", c2c_name, dest) != 2)
    {
    KERR("WARNING topo_web_add_c2c: bad params '%s' (need: name dest)", j->s1);
    return;
    }
  cnf = cloonix_cnf_info_get(dest, &rank);
  if (!cnf)
    {
    KERR("WARNING topo_web_add_c2c: unknown dest cluster '%s' (valid: %s)",
         dest, cloonix_conf_info_get_names());
    return;
    }
  local_cnf = get_own_cloonix_conf_info();
  if (!local_cnf)
    {
    KERR("ERROR topo_web_add_c2c: no local cloonix conf info");
    return;
    }
  saved_tx = rpct_get_string_tx();
  rpct_redirect_string_tx(doors_tx_switch_val_c2c);
  send_c2c_add(g_topo_llid, 0, c2c_name, local_cnf->c2c_udp_ip,
               dest, cnf->ip, (cnf->port & 0xFFFF),
               cnf->passwd, cnf->c2c_udp_ip,
               local_cnf->c2c_udp_port_low);
  rpct_redirect_string_tx(saved_tx);
}

static void do_del_node(t_job *j)
{
  send_del_name(g_topo_llid, 0, j->s1);
}

static void do_rma(t_job *j)
{
  (void) j;
  send_del_all(g_topo_llid, 0);
}

static void do_reboot_vm(t_job *j)
{
  send_vmcmd(g_topo_llid, 0, j->s1, vmcmd_reboot_with_qemu, 0);
}

static void do_save_vm(t_job *j)
{
  send_sav_vm(g_topo_llid, 0, j->s1, j->s2);
}

static void do_add_edge(t_job *j)
{
  send_add_lan_endp(g_topo_llid, 0, j->s1, j->i1, j->s2);
}

static void do_del_edge(t_job *j)
{
  send_del_lan_endp(g_topo_llid, 0, j->s1, j->i1, j->s2);
}

static void do_snf_set(t_job *j)
{
  send_snf_add(g_topo_llid, 0, j->s1, j->i1, j->i2);
}

static void do_wireshark_start(t_job *j)
{
  send_sync_wireshark_req(g_topo_llid, 0, j->s1, j->i1, 1);
}

/* the name s1, the numbers i1 i2, the text s2 */
static void job_named(void (*fn)(t_job *), const char *what, char *s1,
                      int i1, int i2, char *s2)
{
  t_job *j = job_new(fn, what);
  strncpy(j->s1, s1, MAX_PATH_LEN-1);
  if (s2)
    strncpy(j->s2, s2, MAX_PATH_LEN-1);
  j->i1 = i1;
  j->i2 = i2;
  job_post(j);
}

void topo_web_add_vm(t_topo_kvm *kvm)
{
  t_job *j = job_new(do_add_vm, "add vm");
  memcpy(&(j->kvm), kvm, sizeof(t_topo_kvm));
  job_post(j);
}

void topo_web_add_cnt(t_topo_cnt *cnt)
{
  t_job *j = job_new(do_add_cnt, "add cnt");
  memcpy(&(j->cnt), cnt, sizeof(t_topo_cnt));
  job_post(j);
}

void recv_list_commands_resp(int llid, int tid, int qty, t_list_commands *list)
{
  int i, len = 0;
  char *txt = (char *) malloc(qty * (MAX_LIST_COMMANDS_LEN + 1) + 1);
  (void) llid;
  if (!txt)
    {
    KERR("ERROR commands: no memory for %d lines", qty);
    return;
    }
  txt[0] = 0;
  for (i = 0; i < qty; i++)
    len += sprintf(txt + len, "%s\n", list[i].cmd);
  pthread_mutex_lock(&g_cmds_mutex);
  if ((tid == g_cmds_tid) && (!g_cmds_done))
    {
    free(g_cmds_text);
    g_cmds_text = txt;
    txt = NULL;
    g_cmds_done = 1;
    pthread_cond_signal(&g_cmds_cond);
    }
  pthread_mutex_unlock(&g_cmds_mutex);
  if (txt)
    {
    KERR("WARNING commands: an answer %d nobody waits for", tid);
    free(txt);
    }
}

void topo_web_commands(int fd)
{
  static int tid = 1000;
  struct timespec end;
  char hdr[160];
  char *txt = NULL;
  int err = 0;
  if (!g_topo_llid)
    {
    http_fail(fd, 503, "commands", "no connection to the server of the net");
    return;
    }
  clock_gettime(CLOCK_REALTIME, &end);
  end.tv_sec += COMMANDS_WAIT_S;
  pthread_mutex_lock(&g_cmds_mutex);
  g_cmds_tid = ++tid;
  g_cmds_done = 0;
  job_post(job_new(do_commands, "commands"));
  while ((!g_cmds_done) && (!err))
    err = pthread_cond_timedwait(&g_cmds_cond, &g_cmds_mutex, &end);
  if (g_cmds_done)
    {
    txt = g_cmds_text;
    g_cmds_text = NULL;
    }
  g_cmds_done = 1;
  pthread_mutex_unlock(&g_cmds_mutex);
  if (!txt)
    {
    KERR("WARNING commands: no dump commands from the server in %d s",
         COMMANDS_WAIT_S);
    http_fail(fd, 504, NULL, "the server did not answer");
    return;
    }
  snprintf(hdr, sizeof(hdr), "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n"
           "Content-Length: %d\r\nConnection: close\r\n\r\n",
           (int) strlen(txt));
  if ((write(fd, hdr, strlen(hdr)) < 0) ||
      (write(fd, txt, strlen(txt)) < 0))
    KERR("WARNING commands: the answer did not go: %s", strerror(errno));
  free(txt);
}

void topo_web_add_tap(char *name)
{
  job_named(do_add_tap, "add tap", name, 0, 0, NULL);
}

void topo_web_add_nat(char *name)
{
  job_named(do_add_nat, "add nat", name, 0, 0, NULL);
}

void topo_web_add_phy(char *name, int macvlan)
{
  job_named(do_add_phy, "add phy", name, macvlan, 0, NULL);
}

void topo_web_add_a2b(char *name)
{
  job_named(do_add_a2b, "add a2b", name, 0, 0, NULL);
}

void topo_web_position(char *name, int x, int y)
{
  job_named(do_position, "position", name, x, y, NULL);
}

void topo_web_poseth(char *name, int num, int deg)
{
  job_named(do_poseth, "poseth", name, num, deg, NULL);
}

void topo_web_note(char *name, char *txt)
{
  job_named(do_note, "note", name, 0, 0, txt);
}

void topo_web_cnf_a2b(char *name, int dir, char *cmd, int val)
{
  char dir_cmd_val[MAX_PATH_LEN];
  if ((dir != 0) && (dir != 1))
    { KERR("WARNING topo_web_cnf_a2b: bad dir=%d name=%s", dir, name); return; }
  memset(dir_cmd_val, 0, MAX_PATH_LEN);
  snprintf(dir_cmd_val, MAX_PATH_LEN-1,
           "dir=%d cmd=%s val=%d", dir, cmd, val);
  job_named(do_cnf_a2b, "cnf", name, 0, 0, dir_cmd_val);
}

void topo_web_add_c2c(char *name_and_dest)
{
  job_named(do_add_c2c, "add c2c", name_and_dest, 0, 0, NULL);
}

void topo_web_del_node(char *name)
{
  job_named(do_del_node, "del", name, 0, 0, NULL);
}

void topo_web_rma(void)
{
  job_post(job_new(do_rma, "del all"));
}

void topo_web_reboot_vm(char *name)
{
  job_named(do_reboot_vm, "reboot vm", name, 0, 0, NULL);
}

void topo_web_save_vm(char *name, char *path)
{
  job_named(do_save_vm, "save vm", name, 0, 0, path);
}

void topo_web_add_edge(char *name, int num, char *lan)
{
  job_named(do_add_edge, "plug", name, num, 0, lan);
}

void topo_web_del_edge(char *name, int num, char *lan)
{
  job_named(do_del_edge, "unplug", name, num, 0, lan);
}

void topo_web_snf_set(char *name, int num, int on)
{
  job_named(do_snf_set, "snf", name, num, on, NULL);
}

void topo_web_wireshark_start(char *name, int num)
{
  job_named(do_wireshark_start, "wireshark start", name, num, 0, NULL);
}

static void send_usage_levels(int nb, t_eventfull_endp *endp);

void recv_eventfull(int llid, int tid, int nb, t_eventfull_endp *endp)
{
  int i, used = 0, cap = 4096, first = 1;
  char *buf;
  (void)llid; (void)tid;

  int nb_active = 0;
  for (i = 0; i < nb; i++)
    if (endp[i].ptx || endp[i].prx) nb_active++;
  if (nb_active == 0)
    return;

  buf = (char *) clownix_malloc(cap, 1);
  if (!buf) return;

  used = snprintf(buf, cap, "{\"type\":\"activity\",\"evts\":[");
  for (i = 0; i < nb; i++)
    {
    if (!endp[i].ptx && !endp[i].prx) continue;
    int n = snprintf(buf+used, cap-used,
                     "%s{\"name\":\"%s\",\"num\":%d,\"ptx\":%d,\"prx\":%d}",
                     first ? "" : ",",
                     endp[i].name, endp[i].num,
                     endp[i].ptx ? 1 : 0,
                     endp[i].prx ? 1 : 0);
    if (n > 0 && used+n < cap) { used += n; first = 0; }
    }
  used += snprintf(buf+used, cap-used, "]}");
  ws_broadcast_topo_json(buf, used);
  clownix_free(buf, __FUNCTION__);

  /* Per-interface throughput sample, for the PlotTraffic plot of cr-traffic.js. */
  {
    int j, tused, tcap = 8192, tfirst = 1;
    char *tbuf = (char *) clownix_malloc(tcap, 1);
    if (tbuf)
      {
      tused = snprintf(tbuf, tcap, "{\"type\":\"traffic\",\"evts\":[");
      for (j = 0; j < nb; j++)
        {
        int n = snprintf(tbuf+tused, tcap-tused,
                         "%s{\"name\":\"%s\",\"num\":%d,\"btx\":%d,\"brx\":%d,\"ms\":%d}",
                         tfirst ? "" : ",",
                         endp[j].name, endp[j].num,
                         endp[j].btx, endp[j].brx, endp[j].ms);
        if (n > 0 && tused+n < tcap) { tused += n; tfirst = 0; }
        }
      tused += snprintf(tbuf+tused, tcap-tused, "]}");
      ws_broadcast_topo_json(tbuf, tused);
      clownix_free(tbuf, __FUNCTION__);
      }
  }

  /* Per-node RAM/CPU gauge levels. */
  send_usage_levels(nb, endp);
}

static void send_usage_levels(int nb, t_eventfull_endp *endp)
{
  int i, used, cap = 4096, first = 1, nb_node = 0;
  char *buf;
  for (i = 0; i < nb; i++)
    if (endp[i].num == 0)
      nb_node++;
  if (nb_node == 0)
    return;
  buf = (char *) clownix_malloc(cap, 1);
  if (!buf)
  {
    KERR("ERROR topo: no memory for the %d byte node list", cap);
    return;
  }
  used = snprintf(buf, cap, "{\"type\":\"usage\",\"evts\":[");
  for (i = 0; i < nb; i++)
    {
    if (endp[i].num != 0)
      continue;
    int n = snprintf(buf+used, cap-used,
                     "%s{\"name\":\"%s\",\"ram\":%d,\"cpu\":%d}",
                     first ? "" : ",",
                     endp[i].name, endp[i].ram, endp[i].cpu);
    if (n > 0 && used+n < cap) { used += n; first = 0; }
    }
  used += snprintf(buf+used, cap-used, "]}");
  ws_broadcast_topo_json(buf, used);
  clownix_free(buf, __FUNCTION__);
}

#define MAX_VM_STATE MAX_VM
typedef struct t_vm_state
{
  char name[MAX_NAME_LEN];
  int  qmp_conn_ok;
  int  ping_ok;
  int  cga_ping_ok;
} t_vm_state;
static t_vm_state g_vm_state[MAX_VM_STATE];
static int        g_nb_vm_state = 0;

static t_vm_state *get_vm_state(const char *name)
{
  int i;
  for (i = 0; i < g_nb_vm_state; i++)
    if (!strcmp(g_vm_state[i].name, name)) return &g_vm_state[i];
  if (g_nb_vm_state < MAX_VM_STATE)
    {
    memset(&g_vm_state[g_nb_vm_state], 0, sizeof(t_vm_state));
    strncpy(g_vm_state[g_nb_vm_state].name, name, MAX_NAME_LEN-1);
    return &g_vm_state[g_nb_vm_state++];
    }
  KERR("ERROR vmstate table full at %d, no colour for %s",
       MAX_VM_STATE, name);
  return NULL;
}

/* Keep only the names the topology still lists. */
static void prune_vm_state(t_topo_info *topo)
{
  int i, j, keep = 0, found;
  if (!topo)
    return;
  for (i = 0; i < g_nb_vm_state; i++)
    {
    found = 0;
    for (j = 0; (!found) && (j < topo->nb_kvm); j++)
      if (!strcmp(g_vm_state[i].name, topo->kvm[j].name))
        found = 1;
    for (j = 0; (!found) && (j < topo->nb_cnt); j++)
      if (!strcmp(g_vm_state[i].name, topo->cnt[j].name))
        found = 1;
    if (found)
      {
      if (keep != i)
        memcpy(&(g_vm_state[keep]), &(g_vm_state[i]), sizeof(t_vm_state));
      keep += 1;
      }
    }
  g_nb_vm_state = keep;
}

/* Guest state, from vm_evt_* messages and from the topology. */
static void broadcast_vmstate(const char *name, const char *state);

static void topo_seed_vmstate(const char *name, int ping_ok)
{
  t_vm_state *vs = get_vm_state(name);
  if ((vs) && (vs->cga_ping_ok != ping_ok))
    {
    vs->cga_ping_ok = ping_ok;
    broadcast_vmstate(name,
                      (vs->ping_ok || vs->cga_ping_ok) ? "up" :
                      (vs->qmp_conn_ok ? "booting" : "down"));
    }
}

static void broadcast_vmstate(const char *name, const char *state)
{
  char buf[512];
  int len = snprintf(buf, sizeof(buf),
    "{\"type\":\"vmstate\",\"name\":\"%s\",\"state\":\"%s\"}", name, state);
  if (len > 0) ws_broadcast_topo_json(buf, len);
}

#define MAX_LAYOUT_KEEP 1024
typedef struct t_layout_keep
{
  char name[MAX_NAME_LEN];
  int x;
  int y;
  int has_xy;
  int deg[MAX_ETH_VM];
  int has_deg[MAX_ETH_VM];
  char note[MAX_NOTE_LEN];
} t_layout_keep;
static t_layout_keep g_layout_keep[MAX_LAYOUT_KEEP];
static int g_nb_layout_keep;

static t_layout_keep *layout_keep_find(char *name)
{
  int i;
  for (i = 0; i < g_nb_layout_keep; i++)
    if (!strcmp(g_layout_keep[i].name, name))
      return &g_layout_keep[i];
  if (g_nb_layout_keep >= MAX_LAYOUT_KEEP)
    return NULL;
  memset(&g_layout_keep[g_nb_layout_keep], 0, sizeof(t_layout_keep));
  strncpy(g_layout_keep[g_nb_layout_keep].name, name, MAX_NAME_LEN-1);
  return &g_layout_keep[g_nb_layout_keep++];
}

void topo_web_layout_forget_all(void)
{
  g_nb_layout_keep = 0;
}



static int fmt_item_pos(char *buf, int size, char *name, int x, int y)
{
  return snprintf(buf, size,
    "{\"type\":\"itempos\",\"name\":\"%s\",\"x\":%d,\"y\":%d}", name, x, y);
}

static int fmt_item_poseth(char *buf, int size, char *name, int num, int deg)
{
  return snprintf(buf, size,
    "{\"type\":\"itemposeth\",\"name\":\"%s\",\"num\":%d,\"deg\":%d}",
    name, num, deg);
}

static int fmt_item_note(char *buf, int size, char *name, char *txt)
{
  int len = snprintf(buf, size, "{\"type\":\"itemnote\",\"name\":\"%s\","
                     "\"txt\":\"", name);
  if ((len < 0) || (len >= size - 8))
    return -1;
  len += json_escape(buf + len, txt, size - len - 3);
  len += snprintf(buf + len, size - len, "\"}");
  return len;
}

/* "" removes the note, the switch sends it for an item gone too */
void recv_item_note(int llid, int tid, char *name, char *txt)
{
  char buf[1024];
  int len;
  t_layout_keep *k = layout_keep_find(name);
  (void)llid; (void)tid;
  if (k)
    {
    memset(k->note, 0, MAX_NOTE_LEN);
    strncpy(k->note, txt, MAX_NOTE_LEN-1);
    }
  else
    KERR("WARNING note on %s not kept: the layout table is full", name);
  len = fmt_item_note(buf, sizeof(buf), name, txt);
  if (len > 0) ws_broadcast_topo_json(buf, len);
}

void recv_item_pos(int llid, int tid, char *name, int x, int y)
{
  char buf[256];
  int len;
  t_layout_keep *k = layout_keep_find(name);
  (void)llid; (void)tid;
  if (k)
    {
    k->x = x;
    k->y = y;
    k->has_xy = 1;
    }
  len = fmt_item_pos(buf, sizeof(buf), name, x, y);
  if (len > 0) ws_broadcast_topo_json(buf, len);
}

void recv_item_poseth(int llid, int tid, char *name, int num, int deg)
{
  char buf[256];
  int len;
  t_layout_keep *k = layout_keep_find(name);
  (void)llid; (void)tid;
  if ((k) && (num >= 0) && (num < MAX_ETH_VM))
    {
    k->deg[num] = deg;
    k->has_deg[num] = 1;
    }
  len = fmt_item_poseth(buf, sizeof(buf), name, num, deg);
  if (len > 0) ws_broadcast_topo_json(buf, len);
}

static void send_layout_kept(int cfd)
{
  char buf[256];
  int i, j, len;
  for (i = 0; i < g_nb_layout_keep; i++)
    {
    t_layout_keep *k = &g_layout_keep[i];
    if (k->has_xy)
      {
      len = fmt_item_pos(buf, sizeof(buf), k->name, k->x, k->y);
      if (len > 0) ws_send_text(cfd, buf, len);
      }
    for (j = 0; j < MAX_ETH_VM; j++)
      if (k->has_deg[j])
        {
        len = fmt_item_poseth(buf, sizeof(buf), k->name, j, k->deg[j]);
        if (len > 0) ws_send_text(cfd, buf, len);
        }
    if (k->note[0])
      {
      char nbuf[1024];
      len = fmt_item_note(nbuf, sizeof(nbuf), k->name, k->note);
      if (len > 0) ws_send_text(cfd, nbuf, len);
      }
    }
}

void recv_topo_small_event(int llid, int tid, char *name,
                           char *p1, char *p2, int evt)
{
  const char *state;
  t_vm_state *vs;
  (void)llid; (void)tid; (void)p1; (void)p2;

  vs = get_vm_state(name);
  if (!vs) return;

  switch (evt)
    {
    case vm_evt_qmp_conn_ok:        vs->qmp_conn_ok  = 1; break;
    case vm_evt_qmp_conn_ko:        vs->qmp_conn_ok  = 0; break;
    case vm_evt_ping_ok:            vs->ping_ok      = 1; break;
    case vm_evt_ping_ko:            vs->ping_ok      = 0; break;
    case vm_evt_cloonix_ga_ping_ok: vs->cga_ping_ok  = 1; break;
    case vm_evt_cloonix_ga_ping_ko: vs->cga_ping_ok  = 0; break;
    default: return;
    }

  if (vs->ping_ok || vs->cga_ping_ok)
    state = "up";
  else if (vs->qmp_conn_ok)
    state = "booting";
  else
    state = "down";

  broadcast_vmstate(name, state);
}

static int topo_doors_try_connect(void)
{
  uint32_t ip;
  int port, llid;
  doorways_sock_address_detect(g_topo_doors_path, &ip, &port);
  if (ip == 0)
    llid = doorways_sock_client_unix_start(g_topo_doors_path,
                                           topo_callback_connect);
  else
    llid = doorways_sock_client_inet_start(ip, port, topo_callback_connect);
  return llid;
}

static void timer_topo_connect(void *data)
{
  int llid;
  (void)data;
  if (g_topo_llid)              /* already connected via the callback */
    return;
  llid = topo_doors_try_connect();
  if (llid)
    {
    g_topo_connect_llid = llid;
    }
  else
    clownix_timeout_add(5, timer_topo_connect, NULL, NULL, NULL);
}

void topo_web_init(char *doors_path, char *passwd)
{
  int llid;

  g_topo_current = NULL;
  g_topo_llid    = 0;
  memset(g_topo_passwd, 0, MSG_DIGEST_LEN);
  strncpy(g_topo_passwd, passwd, MSG_DIGEST_LEN-1);
  memset(g_topo_doors_path, 0, MAX_PATH_LEN);
  strncpy(g_topo_doors_path, doors_path, MAX_PATH_LEN-1);

  doors_io_basic_json_init(topo_doorways_tx);
  job_init();

  llid = topo_doors_try_connect();
  if (llid)
    {
    g_topo_connect_llid = llid;
    }
  else
    {
    if (!g_said_not_ready)
      {
      g_said_not_ready = 1;
      KERR("WARNING topo_web_init: doors %s not ready yet — retrying",
           doors_path);
      }
    clownix_timeout_add(5, timer_topo_connect, NULL, NULL, NULL);
    }
  clownix_timeout_add(TOPO_BEAT_MS / 10, timer_topo_beat, NULL, NULL, NULL);
}

char *topo_web_get_snapshot_json(int *out_len)
{
  *out_len = 0;
  if (!g_topo_current)
    return NULL;
  return topo_to_json(g_topo_current, out_len);
}

/* Send the whole topology to one browser. */
void topo_web_send_snapshot(int cfd)
{
  int i, len = 0;
  char *json;
  if (!g_topo_current)
    return;
  json = topo_to_json(g_topo_current, &len);
  if (json)
    {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdangling-pointer"
    ws_send_text(cfd, json, len);
#pragma GCC diagnostic pop
    clownix_free(json, __FUNCTION__);
    }
  send_layout_kept(cfd);
  for (i = 0; i < g_nb_vm_state; i++)
    {
    const char *state;
    char buf[512];
    int blen;
    if (g_vm_state[i].ping_ok || g_vm_state[i].cga_ping_ok)
      state = "up";
    else if (g_vm_state[i].qmp_conn_ok)
      state = "booting";
    else
      state = "down";
    blen = snprintf(buf, sizeof(buf),
      "{\"type\":\"vmstate\",\"name\":\"%s\",\"state\":\"%s\"}",
      g_vm_state[i].name, state);
    if (blen > 0) ws_send_text(cfd, buf, blen);
    }
  {
    int j, used = 0, cap = 4096;
    char *pbuf = (char *) clownix_malloc(cap, 1);
    if (pbuf)
      {
      used = snprintf(pbuf, cap, "{\"type\":\"pending_lans\",\"lans\":[");
      for (j = 0; j < g_nb_pending_lans; j++)
        {
        int n = snprintf(pbuf+used, cap-used, "%s\"%s\"",
                         j ? "," : "", g_pending_lans[j]);
        if (n > 0) used += n;
        }
      used += snprintf(pbuf+used, cap-used, "]}");
      ws_send_text(cfd, pbuf, used);
      clownix_free(pbuf, __FUNCTION__);
      }
  }
}
