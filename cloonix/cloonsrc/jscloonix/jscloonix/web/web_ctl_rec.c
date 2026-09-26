/* Websocket /rec: {"cmd":"start","dir":..,"ifaces":[{"name","num"},..]} / {"cmd":"stop"}.
   Per iface: reserve, activate snf capture, thread copies <snf_dir>/<n>_<num>_pcap_sock
   to <dir>/<n>_eth<num>_<k>.pcap. Sends started/count/error/stopped. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "web_common.h"

#define REC_MAX_IFACE   256

typedef struct { uint32_t magic; uint16_t vmaj; uint16_t vmin;
                 int32_t  thiszone; uint32_t sigfigs;
                 uint32_t snaplen; uint32_t network; } rec_pcap_ghdr;
typedef struct { uint32_t ts_sec; uint32_t ts_usec;
                 uint32_t incl_len; uint32_t orig_len; } rec_pcap_rhdr;

#define REC_PCAP_MAGIC         0xa1b2c3d4u
#define REC_PCAP_SWAPPED_MAGIC 0xd4c3b2a1u
#define REC_PCAP_NSEC_MAGIC    0xa1b23c4du
#define REC_PCAP_SWAPPED_NSEC  0x4d3cb2a1u

typedef struct {
    char     name[MAX_NAME_LEN];
    int      num;
    char     key[MAX_NAME_LEN + 16];   /* "<name>_<num>" reservation key */
    char     snf_path[MAX_PATH_LEN];
    char     pcap_path[MAX_PATH_LEN];
    int      read_fd;                  /* the _pcap_sock subscription */
    int      out_fd;                   /* open .pcap file */
    pthread_t thr;                     /* per-iface copy/count thread */
    long     pkts;                     /* packets written so far */
    int      running;
} t_rec_iface;

static pthread_mutex_t g_rec_mtx = PTHREAD_MUTEX_INITIALIZER;
static int          g_rec_active = 0;
static int          g_rec_browser_fd = -1;
static t_rec_iface  g_rec[REC_MAX_IFACE];
static int          g_rec_n = 0;

int rec_iface_is_reserved(const char *key)
{
    int i, reserved = 0;
    pthread_mutex_lock(&g_rec_mtx);
    if (g_rec_active) {
        for (i = 0; i < g_rec_n; i++) {
            if (!strcmp(g_rec[i].key, key)) { reserved = 1; break; }
        }
    }
    pthread_mutex_unlock(&g_rec_mtx);
    return reserved;
}

static void rec_send(int fd, const char *json)
{
    if (fd >= 0)
        ws_send_text(fd, json, (int)strlen(json));
}

static int rec_read_exact(int fd, void *buf, int len)
{
    int off = 0;
    while (off < len) {
        ssize_t r = read(fd, (char *)buf + off, len - off);
        if (r <= 0)
            return 0;
        off += (int)r;
    }
    return 1;
}

#define REC_CONNECT_TRIES 150
#define REC_CONNECT_STEP_US 100000

static int rec_connect_fanout(t_rec_iface *ri)
{
    struct sockaddr_un addr;
    int fd, k;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s_pcap_sock",
             ri->snf_path);
    for (k = 0; k < REC_CONNECT_TRIES && ri->running; k++) {
        fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0)
          {
          KERR("ERROR rec: socket failed errno=%d", errno);
          return -1;
          }
        if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0)
            return fd;
        close(fd);
        usleep(REC_CONNECT_STEP_US);
    }
    if (ri->running)
        KERR("ERROR rec: no dumpcap answered on %s after %d s, %s eth%d is "
             "not recorded", addr.sun_path,
             (REC_CONNECT_TRIES * REC_CONNECT_STEP_US) / 1000000,
             ri->name, ri->num);
    return -1;
}

/* Per-interface thread: copies the pcap stream from dumpcap stdout to the
   .pcap file, counting packets from the record headers. */
static void *rec_iface_thread(void *arg)
{
    t_rec_iface *ri = (t_rec_iface *)arg;
    rec_pcap_ghdr ghdr;
    int byte_swapped = 0;
    uint8_t *pkt = NULL;
    uint32_t snaplen;
    long last_sent = -1;
    char msg[256];

    ri->read_fd = rec_connect_fanout(ri);
    if (ri->read_fd < 0)
        goto done;

    /* Global pcap header (24 bytes): copy verbatim to the file, decode magic. */
    if (!rec_read_exact(ri->read_fd, &ghdr, sizeof(ghdr)))
      {
        KERR("ERROR rec: cannot read the pcap global header from dumpcap");
        goto done;
      }
    if (write(ri->out_fd, &ghdr, sizeof(ghdr)) != (ssize_t)sizeof(ghdr))
      {
        KERR("ERROR rec: cannot write the pcap header to %s errno=%d",
             ri->pcap_path, errno);
        goto done;
      }
    if (ghdr.magic == REC_PCAP_SWAPPED_MAGIC ||
        ghdr.magic == REC_PCAP_SWAPPED_NSEC)
        byte_swapped = 1;
    else if (ghdr.magic != REC_PCAP_MAGIC && ghdr.magic != REC_PCAP_NSEC_MAGIC)
      {
        KERR("ERROR rec: dumpcap sent magic 0x%08x, that is not a pcap stream",
             ghdr.magic);
        goto done;
      }

    snaplen = byte_swapped ? __builtin_bswap32(ghdr.snaplen) : ghdr.snaplen;
    if (snaplen == 0 || snaplen > (1u << 20))
        snaplen = 262144;
    pkt = malloc(snaplen + 16);
    if (!pkt)
      {
        KERR("ERROR rec: no memory for a %u byte packet buffer", snaplen + 16);
        goto done;
      }

    while (ri->running) {
        rec_pcap_rhdr rhdr;
        uint32_t incl;
        if (!rec_read_exact(ri->read_fd, &rhdr, sizeof(rhdr)))
            break;
        incl = byte_swapped ? __builtin_bswap32(rhdr.incl_len) : rhdr.incl_len;
        if (incl > snaplen + 16)
            break;
        if (!rec_read_exact(ri->read_fd, pkt, (int)incl))
            break;
        if (write(ri->out_fd, &rhdr, sizeof(rhdr)) != (ssize_t)sizeof(rhdr))
            break;
        if (incl && write(ri->out_fd, pkt, incl) != (ssize_t)incl)
            break;
        ri->pkts++;
        if (ri->pkts != last_sent) {
            last_sent = ri->pkts;
            snprintf(msg, sizeof(msg),
                "{\"type\":\"count\",\"name\":\"%s\",\"num\":%d,\"pkts\":%ld}",
                ri->name, ri->num, ri->pkts);
            pthread_mutex_lock(&g_rec_mtx);
            if (g_rec_active)
                rec_send(g_rec_browser_fd, msg);
            pthread_mutex_unlock(&g_rec_mtx);
        }
    }

done:
    if (pkt)
        free(pkt);
    if (ri->read_fd >= 0) { close(ri->read_fd); ri->read_fd = -1; }
    ri->running = 0;
    return NULL;
}

static void rec_stop_all(void)
{
    int i;
    pthread_mutex_lock(&g_rec_mtx);
    if (!g_rec_active) { pthread_mutex_unlock(&g_rec_mtx); return; }
    g_rec_active = 0;
    for (i = 0; i < g_rec_n; i++)
        g_rec[i].running = 0;
    pthread_mutex_unlock(&g_rec_mtx);

    for (i = 0; i < g_rec_n; i++) {
        if (g_rec[i].read_fd >= 0)
            shutdown(g_rec[i].read_fd, SHUT_RDWR);
    }
    for (i = 0; i < g_rec_n; i++) {
        if (g_rec[i].thr)
            pthread_join(g_rec[i].thr, NULL);
        g_rec[i].thr = 0;
        if (g_rec[i].out_fd >= 0) { fsync(g_rec[i].out_fd); close(g_rec[i].out_fd); g_rec[i].out_fd = -1; }
    }
    g_rec_n = 0;
}

/* Called from the SIGTERM handler / kill_all_children so a dying gui-server
   frees the record too. */
void rec_stop_all_unlocked(void)
{
    int i;
    g_rec_active = 0;
    for (i = 0; i < g_rec_n; i++) {
        if (g_rec[i].read_fd >= 0) close(g_rec[i].read_fd);
        if (g_rec[i].out_fd >= 0) close(g_rec[i].out_fd);
    }
    g_rec_n = 0;
}

/* Lowest instance number >= 1 whose file does not exist yet. */
static void rec_build_pcap_path(const char *dir, const char *name, int num,
                                char *out, int outsz)
{
    int inst;
    for (inst = 1; inst < 1000000; inst++) {
        snprintf(out, outsz, "%s/%s_eth%d_%d.pcap", dir, name, num, inst);
        if (access(out, F_OK) != 0)
            return;   /* free slot — keep this name */
    }
    snprintf(out, outsz, "%s/%s_eth%d_1.pcap", dir, name, num);
}

static int rec_parse_ifaces(const char *body, const char *dir)
{
    const char *p = strstr(body, "\"ifaces\"");
    int n = 0;
    if (p) p = strchr(p, '[');
    while (p && n < REC_MAX_IFACE) {
        const char *obj = strchr(p, '{');
        const char *end = strchr(p, ']');
        char nm[MAX_NAME_LEN] = {0};
        int num = -1;
        const char *q;
        if (!obj || (end && obj > end))
            break;
        q = strstr(obj, "\"name\"");
        if (q) {
            q = strchr(q + 6, ':');
            if (q) {
                while (*q == ':' || *q == ' ' || *q == '"') q++;
                int i = 0;
                while (*q && *q != '"' && i < MAX_NAME_LEN - 1) nm[i++] = *q++;
            }
        }
        q = strstr(obj, "\"num\"");
        if (q) { q = strchr(q + 5, ':'); if (q) sscanf(q + 1, "%d", &num); }
        if (nm[0] && (!web_name_ok(nm))) {
            KERR("ERROR rec: refused the interface name [%s]: a name, not a path", nm);
            nm[0] = 0;
        }
        if (nm[0] && num >= 0) {
            t_rec_iface *ri = &g_rec[n];
            memset(ri, 0, sizeof(*ri));
            snprintf(ri->name, sizeof(ri->name), "%s", nm);
            ri->num = num;
            snprintf(ri->key, sizeof(ri->key), "%s_%d", nm, num);
            snprintf(ri->snf_path, sizeof(ri->snf_path),
                     "%s/%s_%d", g_snf_dir, nm, num);
            rec_build_pcap_path(dir, nm, num,
                                ri->pcap_path, sizeof(ri->pcap_path));
            ri->read_fd = -1; ri->out_fd = -1;
            n++;
        }
        p = strchr(obj, '}');
        if (!p) break;
    }
    return n;
}

static int rec_mkdir_p(const char *path)
{
    char tmp[MAX_PATH_LEN];
    char *q;
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (q = tmp + 1; *q; q++) {
        if (*q == '/') {
            *q = '\0';
            if (mkdir(tmp, 0755) < 0 && errno != EEXIST)
              {
              KERR("ERROR rec: mkdir %s errno=%d", tmp, errno);
              return -1;
              }
            *q = '/';
        }
    }
    if (mkdir(tmp, 0755) < 0 && errno != EEXIST) {
        KERR("WARNING rec: mkdir %s: %s", tmp, strerror(errno));
        return -1;
    }
    return 0;
}

static void rec_handle_start(int browser_fd, const char *body)
{
    char dir[MAX_PATH_LEN] = {0};
    char rep[REC_MAX_IFACE * (2 * MAX_NAME_LEN + 48) + 64];
    int  i, rl;
    const char *p;

    pthread_mutex_lock(&g_rec_mtx);
    if (g_rec_active) {
        pthread_mutex_unlock(&g_rec_mtx);
        rec_send(browser_fd, "{\"type\":\"error\",\"msg\":\"record already running\"}");
        return;
    }
    pthread_mutex_unlock(&g_rec_mtx);

    char sub[MAX_PATH_LEN] = {0};
    p = strstr(body, "\"dir\"");
    if (p) {
        p = strchr(p + 5, ':');
        if (p) {
            while (*p == ':' || *p == ' ' || *p == '"') p++;
            int j = 0;
            while (*p && *p != '"' && j < (int)sizeof(sub) - 1) sub[j++] = *p++;
        }
    }
    {
        char *s = sub;
        while (*s == '/') s++;                 /* drop leading slashes */
        if (s != sub) memmove(sub, s, strlen(s) + 1);
    }
    if (!sub[0])
        snprintf(sub, sizeof(sub), "pcap");
    if (strstr(sub, "..")) {
        rec_send(browser_fd, "{\"type\":\"error\",\"msg\":\"bad directory\"}");
        KERR("ERROR rec: refused a directory holding .. : %s", sub);
        return;
    }
    {
        const char *root = g_share_dir[0] ? g_share_dir : CLOONIX_SHARE_DIR;
        snprintf(dir, sizeof(dir), "%s/%s", root, sub);
    }
    if (rec_mkdir_p(dir) < 0) {
        rec_send(browser_fd, "{\"type\":\"error\",\"msg\":\"cannot create directory\"}");
        KERR("ERROR rec: cannot create %s errno=%d", dir, errno);
        return;
    }

    g_rec_n = rec_parse_ifaces(body, dir);
    if (g_rec_n == 0) {
        rec_send(browser_fd, "{\"type\":\"error\",\"msg\":\"no sniffable interfaces\"}");
        return;
    }

    pthread_mutex_lock(&g_rec_mtx);
    g_rec_active = 1;
    g_rec_browser_fd = browser_fd;
    pthread_mutex_unlock(&g_rec_mtx);

    for (i = 0; i < g_rec_n; i++) {
        t_rec_iface *ri = &g_rec[i];
        /* Activate the cloonsnf capture process for this iface, the same rpc
           the webshark path sends; the thread below waits for its dumpcap. */
        topo_web_wireshark_start(ri->name, ri->num);
        ri->out_fd = open(ri->pcap_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (ri->out_fd < 0) {
            KERR("ERROR rec: cannot open %s errno=%d", ri->pcap_path, errno);
            continue;
        }
        ri->running = 1;
        if (pthread_create(&ri->thr, NULL, rec_iface_thread, ri) != 0) {
            KERR("ERROR rec: pthread_create failed for %s", ri->key);
            ri->running = 0;
            ri->thr = 0;
            close(ri->out_fd); ri->out_fd = -1;
        }
    }

    rl = snprintf(rep, sizeof(rep), "{\"type\":\"started\",\"ifaces\":[");
    for (i = 0; i < g_rec_n; i++) {
        const char *base = strrchr(g_rec[i].pcap_path, '/');
        base = base ? base + 1 : g_rec[i].pcap_path;
        rl += snprintf(rep + rl, sizeof(rep) - rl,
                       "%s{\"name\":\"%s\",\"num\":%d,\"file\":\"%s\"}",
                       i ? "," : "", g_rec[i].name, g_rec[i].num, base);
    }
    rl += snprintf(rep + rl, sizeof(rep) - rl, "]}");
    rec_send(browser_fd, rep);
}

void handle_rec_ws(int browser_fd, const char *full_hdr)
{
    uint8_t *payload = NULL;
    uint64_t plen = 0;
    int op;

    if (ws_handshake(browser_fd, full_hdr))
      {
        KERR("ERROR rec: websocket handshake failed");
        return;
      }

    for (;;) {
        op = ws_read_frame(browser_fd, &payload, &plen);
        if (op < 0)
            break;
        if (op == 0x8) {                 /* client closed */
            if (payload) { free(payload); payload = NULL; }
            break;
        }
        if (op == 0x9) {                 /* ping -> pong */
            uint8_t pong[2] = { 0x8A, 0x00 };
            (void)write(browser_fd, pong, 2);
            if (payload) { free(payload); payload = NULL; }
            continue;
        }
        if (op != 0x1) {                 /* ignore non-text */
            if (payload) { free(payload); payload = NULL; }
            continue;
        }
        if (strstr((char *)payload, "\"start\"")) {
            rec_handle_start(browser_fd, (char *)payload);
        }
        else if (strstr((char *)payload, "\"stop\"")) {
            rec_stop_all();
            rec_send(browser_fd, "{\"type\":\"stopped\"}");
        }
        if (payload) { free(payload); payload = NULL; }
    }

    pthread_mutex_lock(&g_rec_mtx);
    if (g_rec_browser_fd == browser_fd)
        g_rec_browser_fd = -1;
    pthread_mutex_unlock(&g_rec_mtx);
    rec_stop_all();
}
