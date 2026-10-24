/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */

/* webshark.c - Wireshark engine with a WebSocket/HTTP frontend (GET / page,
 * /ws). Text frames of JSON. Client: filter{expr}, select{frame},
 * bytes{frame}, stop, save, save_where. Server: packet_list, proto_tree,
 * bytes, status, error, stopped, saved. SPDX-License-Identifier: GPL-2.0-or-later */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include <time.h>
#include <limits.h>
#include <pwd.h>
#include <pthread.h>
#include <sys/un.h>

#include <glib.h>

#include <wsutil/privileges.h>
#include <wsutil/report_message.h>
#include <wsutil/version_info.h>
#include <wsutil/str_util.h>

#include <wiretap/wtap.h>
#include <wiretap/libpcap.h>
#include <wiretap/pcap-encap.h>
#include <wsutil/buffer.h>
#include <epan/epan.h>
#include <epan/epan_dissect.h>
#include <epan/prefs.h>
#include <epan/column.h>
#include <epan/column-info.h>
#include <epan/column-utils.h>
#include <epan/proto.h>
#include <epan/frame_data.h>
#include <epan/frame_data_sequence.h>
#include <epan/tap.h>
#include <epan/timestamp.h>
#include <epan/disabled_protos.h>
#include <epan/tvbuff.h>

#include <epan/dfilter/dfilter.h>
#include "ui/alert_box.h"
#include "cfile.h"
#include "file.h"
#include "frame_tvbuff.h"

/* Configuration */

#define MAX_CLIENTS    8
#define RX_BUFSIZE     65536
#define MAX_PACKETS    500000   /* the capture stops itself here */
/* Ring of raw bytes kept for re-dissection (select, filter, save). */
#define KEEP_RAW       200000

/* Rows batched in one websocket message, and the longest a row waits. */
#define BATCH_MAX_ROWS  256
#define BATCH_MAX_MS    100

/* Per client tx queue: above HIGH xoff and packet rows dropped, below LOW
 * xon, HARD not even a control message passes. */
#define TX_HIGH_WATER   (4 * 1024 * 1024)
#define TX_LOW_WATER    (1 * 1024 * 1024)
#define TX_HARD_CAP     (8 * 1024 * 1024)
#define TX_CHUNK        65536
#define TX_COMPACT      (1024 * 1024)   /* sent bytes given back past this */
#define TX_STALL_SEC    30      /* a client accepting nothing this long is cut */

/* Rows replayed to a client that (re)connects. */
#define REPLAY_MAX_ROWS 5000

/* At most one warning per source per period. */
#define WARN_PERIOD_SEC 5

/* Room for the packet pipe, so a burst waits rather than being dropped. */
#define PKT_PIPE_BYTES  (4 * 1024 * 1024)

/* -Wl,--wrap guards: the capture dissector framework crashes on cpinfo->counts == NULL. */
#include <epan/capture_dissectors.h>

extern void __real_capture_dissector_increment_count(capture_packet_info_t *cpinfo, int proto);
void __wrap_capture_dissector_increment_count(capture_packet_info_t *cpinfo, int proto)
{
    if (cpinfo == NULL || cpinfo->counts == NULL) return;
    __real_capture_dissector_increment_count(cpinfo, proto);
}

extern bool __real_try_capture_dissector(const char *name, uint32_t pattern,
        const uint8_t *pd, int offset, int len,
        capture_packet_info_t *cpinfo, const union wtap_pseudo_header *pseudo_header);
bool __wrap_try_capture_dissector(const char *name, uint32_t pattern,
        const uint8_t *pd, int offset, int len,
        capture_packet_info_t *cpinfo, const union wtap_pseudo_header *pseudo_header)
{
    if (cpinfo == NULL || cpinfo->counts == NULL) return false;
    return __real_try_capture_dissector(name, pattern, pd, offset, len, cpinfo, pseudo_header);
}

extern bool __real_call_capture_dissector(capture_dissector_handle_t handle,
        const uint8_t *pd, int offset, int len,
        capture_packet_info_t *cpinfo, const union wtap_pseudo_header *pseudo_header);
bool __wrap_call_capture_dissector(capture_dissector_handle_t handle,
        const uint8_t *pd, int offset, int len,
        capture_packet_info_t *cpinfo, const union wtap_pseudo_header *pseudo_header)
{
    if (cpinfo == NULL || cpinfo->counts == NULL) return false;
    return __real_call_capture_dissector(handle, pd, offset, len, cpinfo, pseudo_header);
}

/* Global state */

static capture_file  g_cf;
static bool          g_epan_ready = false;
static int           g_server_fd  = -1;
static const char   *g_sock_path  = NULL;
static const char   *g_start_file  = NULL;  /* snf socket path */
static const char   *g_dumpcap_bin = NULL; /* resolved at startup */

/* Packet queue: capture_thread writes raw packets here,
   main thread reads and dissects them (epan is not thread-safe for TRY/CATCH). */
static int g_pkt_pipe[2] = {-1, -1};  /* [0]=read end (main), [1]=write end (capture) */

typedef struct {
    uint32_t caplen;
    uint32_t origlen;
    uint32_t ts_sec;
    uint32_t ts_usec;
    /* followed by caplen bytes of packet data */
} pkt_header_t;

/* Resolve dumpcap path: prefer binary alongside webshark, then cloonix install */
static void resolve_dumpcap_path(void)
{
    if (g_dumpcap_bin) return; /* already set by -d flag */

    /* Try to find dumpcap next to this binary */
    char self[4096] = {0};
    ssize_t n = readlink("/proc/self/exe", self, sizeof(self) - 1);
    if (n > 0) {
        char *slash = strrchr(self, '/');
        if (slash) {
            *(slash + 1) = '\0';
            char *candidate = g_strdup_printf("%scloonix-dumpcap", self);
            if (access(candidate, X_OK) == 0) {
                g_dumpcap_bin = candidate;
                return;
            }
            g_free(candidate);
        }
    }
    /* Fall back to cloonix install location */
    g_dumpcap_bin = "/usr/libexec/cloonix/cloonfs/bin/cloonix-dumpcap";
}

/* Mutex protecting g_cf — dissection happens on the main thread,
   but WebSocket clients can request data from their threads.         */
static pthread_mutex_t g_cf_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Live capture packet store: frame_num (uint32) -> GByteArray* of raw bytes.
   Populated during capture, used by cmd_select/cmd_bytes for re-dissection. */
static GHashTable *g_live_pkts = NULL;  /* guint -> GByteArray* */

/* Live capture state */
static pthread_t       g_capture_thread;
static bool            g_capture_running = false;
static uint32_t        g_frame_count     = 0;  /* total frames so far */
static pid_t           g_dumpcap_pid     = -1; /* dumpcap child, for clean stop */
static uint32_t        g_pcap_network    = 1;  /* pcap linktype of the capture (EN10MB default) */
static bool            g_link_known      = false; /* g_pcap_network read from the capture */
static char            g_save_path[512]  = ""; /* temp .pcap written by cmd_save, served via /save.pcap */
static pthread_mutex_t g_save_mutex      = PTHREAD_MUTEX_INITIALIZER;

/* Live filter, under g_cf_mutex: a non matching packet is stored and counted, never sent. */
static dfilter_t      *g_live_dfcode     = NULL;
static char           *g_live_filter_expr = NULL;

/* Packets given up at the input because dissection is behind. Distinct from
   the per client drops, which are a browser that is behind. */
static uint64_t        g_src_dropped     = 0;
static uint64_t        g_src_seen        = 0;

/* Minimal WebSocket implementation (RFC 6455) */

#define WS_MAGIC "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

/* One websocket client. Only its own tx thread writes the socket; the
 * dissection thread appends to tx, so a slow browser fills its own queue,
 * gets xoff and loses its own rows, never stalling the capture. */
typedef struct {
    int              fd;
    bool             handshake_done;
    GByteArray      *tx;          /* complete websocket frames, byte stream */
    size_t           tx_off;      /* already sent, compacted when it grows */
    pthread_mutex_t  tx_lock;
    pthread_cond_t   tx_cond;
    pthread_t        tx_tid;
    bool             tx_stop;
    bool             xoff;        /* queue went over the high water mark */
    uint32_t         dropped;     /* rows dropped since the last xon */
    uint64_t         dropped_total;
    time_t           warn_time;   /* last warning printed for this client */
} ws_client_t;

static ws_client_t *g_clients[MAX_CLIENTS];
static int          g_client_count = 0;
static pthread_mutex_t g_clients_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Lock order: g_clients_mutex -> tx_lock; g_cf_mutex released before either. */

/* SHA1 for WebSocket handshake — uses GLib */
static void ws_accept_key(const char *client_key, char *out60)
{
    char concat[128];
    g_snprintf(concat, sizeof(concat), "%s%s", client_key, WS_MAGIC);

    GChecksum *cs = g_checksum_new(G_CHECKSUM_SHA1);
    g_checksum_update(cs, (const guchar *)concat, strlen(concat));

    guchar digest[20];
    gsize  dlen = 20;
    g_checksum_get_digest(cs, digest, &dlen);
    g_checksum_free(cs);

    gchar *b64 = g_base64_encode(digest, 20);
    g_strlcpy(out60, b64, 60);
    g_free(b64);
}

/* Write all of it through the send timeout: a short send over a network cut
 * the download below its Content-Length. */
static bool send_all(int fd, const void *buf, size_t len)
{
    size_t done = 0;
    while (done < len) {
        ssize_t w = send(fd, (const char *)buf + done, len - done, MSG_NOSIGNAL);
        if (w > 0) {
            done += w;
            continue;
        }
        if ((w < 0) && ((errno == EINTR) || (errno == EAGAIN) ||
                        (errno == EWOULDBLOCK)))
            continue;
        return false;
    }
    return true;
}

/* Append one complete text frame (header then payload) to a byte queue. */
static void ws_frame_append(GByteArray *out, const char *payload, size_t plen)
{
    uint8_t header[10];
    size_t  hlen;

    header[0] = 0x81; /* FIN + opcode=text */
    if (plen <= 125) {
        header[1] = (uint8_t)plen;
        hlen = 2;
    } else if (plen <= 65535) {
        header[1] = 126;
        header[2] = (plen >> 8) & 0xFF;
        header[3] =  plen       & 0xFF;
        hlen = 4;
    } else {
        header[1] = 127;
        for (int i = 0; i < 8; i++)
            header[2+i] = (plen >> (56 - 8*i)) & 0xFF;
        hlen = 10;
    }
    g_byte_array_append(out, header, hlen);
    g_byte_array_append(out, (const uint8_t *)payload, plen);
}

/* Bytes owed to a client, drained from an offset (no memmove per chunk). */
static size_t tx_queued(const ws_client_t *c)
{
    return c->tx->len - c->tx_off;
}

/* Called with tx_lock held: give the sent bytes back. */
static void tx_compact_locked(ws_client_t *c)
{
    if (c->tx_off == c->tx->len) {
        g_byte_array_set_size(c->tx, 0);
        c->tx_off = 0;
    }
    else if (c->tx_off >= TX_COMPACT) {
        g_byte_array_remove_range(c->tx, 0, c->tx_off);
        c->tx_off = 0;
    }
}

/* One warning per source per period, so a client that flaps cannot fill the
   log with the fact that it is flapping. */
static bool warn_due(time_t *last)
{
    time_t now = time(NULL);
    if (now - *last < WARN_PERIOD_SEC)
        return false;
    *last = now;
    return true;
}

/* Called with tx_lock held: announce that this client is served again. */
static void ws_xon_locked(ws_client_t *c)
{
    char note[192];
    int len = g_snprintf(note, sizeof(note),
        "{\"type\":\"flow\",\"state\":\"xon\",\"dropped\":%u,\"total\":%llu}",
        c->dropped, (unsigned long long)c->dropped_total);
    ws_frame_append(c->tx, note, len);
    fprintf(stderr, "flow: client %d served again, %u rows lost while behind "
                    "(%llu total)\n",
            c->fd, c->dropped, (unsigned long long)c->dropped_total);
    c->xoff   = false;
    c->dropped = 0;
}

/* The only place a row is dropped, g_clients_mutex held. rows > 0: a packet
   batch, droppable; 0: control, goes until the hard cap. */
static void ws_enqueue(ws_client_t *c, const char *json, size_t len,
                       uint32_t rows, bool droppable)
{
    char note[192];
    int  nlen;

    pthread_mutex_lock(&c->tx_lock);

    if (droppable && (c->xoff || (tx_queued(c) >= TX_HIGH_WATER))) {
        if (!c->xoff) {
            c->xoff = true;
            nlen = g_snprintf(note, sizeof(note),
                "{\"type\":\"flow\",\"state\":\"xoff\",\"queued\":%u}",
                (unsigned)tx_queued(c));
            ws_frame_append(c->tx, note, nlen);
            pthread_cond_signal(&c->tx_cond);
            if (warn_due(&c->warn_time))
                fprintf(stderr, "flow: client %d is behind, %u bytes queued, "
                                "dropping rows\n",
                        c->fd, (unsigned)tx_queued(c));
        }
        c->dropped       += rows;
        c->dropped_total += rows;
        pthread_mutex_unlock(&c->tx_lock);
        return;
    }

    if (tx_queued(c) + len + 10 > TX_HARD_CAP) {
        c->dropped       += rows;
        c->dropped_total += rows;
        if (warn_due(&c->warn_time))
            fprintf(stderr, "ERROR flow: client %d queue full at %u bytes, "
                            "a message was dropped\n",
                    c->fd, (unsigned)tx_queued(c));
        pthread_mutex_unlock(&c->tx_lock);
        return;
    }

    ws_frame_append(c->tx, json, len);
    pthread_cond_signal(&c->tx_cond);
    pthread_mutex_unlock(&c->tx_lock);
}

static ws_client_t *client_by_fd_locked(int fd)
{
    for (int i = 0; i < g_client_count; i++)
        if (g_clients[i]->fd == fd)
            return g_clients[i];
    return NULL;
}

/* Queue a control message for one client. Every existing call site keeps its
   fd, and none of them holds g_cf_mutex when it gets here. */
static void ws_send_text(int fd, const char *payload, size_t plen)
{
    ws_client_t *c;
    pthread_mutex_lock(&g_clients_mutex);
    c = client_by_fd_locked(fd);
    if (c)
        ws_enqueue(c, payload, plen, 0, false);
    pthread_mutex_unlock(&g_clients_mutex);
}

/* Queue a message for every client. rows > 0 marks it droppable. */
static void ws_broadcast_rows(const char *json, uint32_t rows)
{
    size_t len = strlen(json);
    pthread_mutex_lock(&g_clients_mutex);
    for (int i = 0; i < g_client_count; i++)
        if (g_clients[i]->handshake_done)
            ws_enqueue(g_clients[i], json, len, rows, rows != 0);
    pthread_mutex_unlock(&g_clients_mutex);
}

static void ws_broadcast(const char *json)
{
    ws_broadcast_rows(json, 0);
}

/* Drains one client's queue onto its socket. The send may block, and that is
   the point: only this thread waits for the browser. */
static void *tx_thread(void *arg)
{
    ws_client_t *c = (ws_client_t *)arg;
    uint8_t chunk[TX_CHUNK];
    time_t  stall = 0;
    size_t  n, done;
    ssize_t w;

    while (1) {
        pthread_mutex_lock(&c->tx_lock);
        while (!c->tx_stop && (tx_queued(c) == 0))
            pthread_cond_wait(&c->tx_cond, &c->tx_lock);
        if (c->tx_stop) {
            pthread_mutex_unlock(&c->tx_lock);
            break;
        }
        n = (tx_queued(c) < TX_CHUNK) ? tx_queued(c) : TX_CHUNK;
        memcpy(chunk, c->tx->data + c->tx_off, n);
        c->tx_off += n;
        tx_compact_locked(c);
        if (c->xoff && (tx_queued(c) < TX_LOW_WATER))
            ws_xon_locked(c);
        pthread_mutex_unlock(&c->tx_lock);

        done = 0;
        while (done < n) {
            w = send(c->fd, chunk + done, n - done, MSG_NOSIGNAL);
            if (w > 0) {
                done += w;
                stall = 0;
                continue;
            }
            if ((w < 0) && (errno == EINTR))
                continue;
            /* SO_SNDTIMEO: keep and retry (a half frame is unreadable); cut only after TX_STALL_SEC of nothing. */
            if ((w < 0) && ((errno == EAGAIN) || (errno == EWOULDBLOCK))) {
                if (!stall)
                    stall = time(NULL);
                else if (time(NULL) - stall >= TX_STALL_SEC) {
                    fprintf(stderr, "ERROR flow: client %d took nothing for "
                                    "%d seconds, closing it\n",
                            c->fd, TX_STALL_SEC);
                    goto dead;
                }
                continue;
            }
            goto dead;
        }
    }
    return NULL;

dead:
    /* Wake the reader so the client is torn down from one place only. */
    shutdown(c->fd, SHUT_RDWR);
    return NULL;
}

/* Decode a received WebSocket frame into text (unmask, single frame assumed) */
static int ws_decode_frame(const uint8_t *buf, int buflen, char *out, int outmax)
{
    if (buflen < 2) return -1;
    bool masked   = (buf[1] & 0x80) != 0;
    uint64_t plen = buf[1] & 0x7F;
    int  off = 2;

    if (plen == 126) {
        if (buflen < 4) return -1;
        plen = ((uint64_t)buf[2] << 8) | buf[3];
        off = 4;
    } else if (plen == 127) {
        if (buflen < 10) return -1;
        plen = 0;
        for (int i = 0; i < 8; i++) plen = (plen << 8) | buf[2+i];
        off = 10;
    }

    uint8_t mask[4] = {0,0,0,0};
    if (masked) {
        if (buflen < off + 4) return -1;
        memcpy(mask, buf + off, 4);
        off += 4;
    }

    if ((int)(off + plen) > buflen) return -1;
    if ((int)plen >= outmax) plen = outmax - 1;

    for (uint64_t i = 0; i < plen; i++)
        out[i] = (char)(buf[off + i] ^ (masked ? mask[i % 4] : 0));
    out[plen] = '\0';
    return (int)(off + plen);
}

/* Embedded HTML frontend (served at GET /) */

/* The actual HTML content is in webshark.html, compiled in as a string.
   For the build we embed it here as a long literal.                   */
extern const char g_webshark_html[];  /* defined in webshark_html.c */
extern size_t     g_webshark_html_len;

/* JSON helpers */

/* Escape a C string for JSON — returns newly allocated string */
static char *json_escape(const char *s)
{
    if (!s) return g_strdup("null");
    GString *gs = g_string_new("\"");
    for (const char *p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if      (c == '"')  g_string_append(gs, "\\\"");
        else if (c == '\\') g_string_append(gs, "\\\\");
        else if (c == '\n') g_string_append(gs, "\\n");
        else if (c == '\r') g_string_append(gs, "\\r");
        else if (c == '\t') g_string_append(gs, "\\t");
        else if (c < 0x20)  g_string_append_printf(gs, "\\u%04x", c);
        else                g_string_append_c(gs, (char)c);
    }
    g_string_append_c(gs, '"');
    return g_string_free(gs, FALSE);
}

/* Proto-tree → JSON */

static void proto_node_to_json(proto_node *node, GString *gs, int depth)
{
    if (!node) return;

    field_info *fi = node->finfo;
    if (!fi) return;

    /* a field with no text of its own gets it from its value, as wireshark
       shows it; called from inside the dissect, where it is valid */
    char filled[ITEM_LABEL_LENGTH];
    const char *label_str = "";
    if (fi->rep)
        label_str = fi->rep->representation;
    else if (fi->hfinfo) {
        filled[0] = '\0';
        proto_item_fill_label(fi, filled);
        label_str = filled;
    }
    const char *abbrev_str = (fi->hfinfo) ? fi->hfinfo->abbrev : NULL;

    char *jlabel  = json_escape(label_str);
    char *jabbrev = json_escape(abbrev_str);

    g_string_append_printf(gs,
        "{\"label\":%s,\"abbrev\":%s,"
        "\"start\":%d,\"length\":%d",
        jlabel, jabbrev,
        fi->start, fi->length);

    g_free(jlabel);
    g_free(jabbrev);

    /* Children */
    if (node->first_child) {
        g_string_append(gs, ",\"children\":[");
        bool first = true;
        for (proto_node *child = node->first_child; child; child = child->next) {
            if (!first) g_string_append_c(gs, ',');
            first = false;
            proto_node_to_json(child, gs, depth + 1);
        }
        g_string_append_c(gs, ']');
    }

    g_string_append_c(gs, '}');
}

/* Packet-list summary → JSON */

/* Build the standard 6-column display: No, Time, Source, Dest, Proto, Info */
/* Column setup uses prefs (loaded before calling this). */
static void build_column_format(column_info *cinfo)
{
    build_column_format_array(cinfo, prefs.num_cols, true);
}

/* JSON row of one packet, NULL once its bytes left KEEP_RAW. Caller g_frees. */
static char *packet_to_json_row(capture_file *cf, frame_data *fdata,
                                GByteArray *live_ba)
{
    wtap_rec rec;
    Buffer   buf;

    wtap_rec_init(&rec);
    ws_buffer_init(&buf, 1514);

    if (live_ba) {
        rec.rec_type                           = REC_TYPE_PACKET;
        rec.presence_flags                     = WTAP_HAS_TS | WTAP_HAS_CAP_LEN;
        rec.rec_header.packet_header.caplen    = live_ba->len;
        rec.rec_header.packet_header.len       = live_ba->len;
        rec.rec_header.packet_header.pkt_encap = cf->cd_t;
        ws_buffer_append(&buf, live_ba->data, live_ba->len);
    } else {
        wtap_rec_cleanup(&rec);
        ws_buffer_free(&buf);
        return NULL;
    }

    epan_dissect_t edt;
    epan_dissect_init(&edt, cf->epan, false, false);
    epan_dissect_run(&edt, cf->cd_t, &rec,
                     frame_tvbuff_new_buffer(&g_cf.provider, fdata, &buf),
                     fdata, &cf->cinfo);
    epan_dissect_fill_in_columns(&edt, false, true);

    GString *gs = g_string_new(NULL);
    g_string_append_printf(gs, "{\"num\":%u,\"cols\":[", fdata->num);
    for (int i = 0; i < cf->cinfo.num_cols; i++) {
        const char *text = cf->cinfo.columns[i].col_data;
        char *jt = json_escape(text ? text : "");
        if (i) g_string_append_c(gs, ',');
        g_string_append(gs, jt);
        g_free(jt);
    }
    g_string_append(gs, "]}");

    epan_dissect_cleanup(&edt);
    wtap_rec_cleanup(&rec);
    ws_buffer_free(&buf);

    return g_string_free(gs, FALSE);
}

/* Command handlers */

static void send_status(int fd, const char *msg)
{
    char *jmsg = json_escape(msg);
    char *json = g_strdup_printf("{\"type\":\"status\",\"msg\":%s}", jmsg);
    g_free(jmsg);
    ws_send_text(fd, json, strlen(json));
    g_free(json);
}

static void send_error(int fd, const char *msg)
{
    char *jmsg = json_escape(msg);
    char *json = g_strdup_printf("{\"type\":\"error\",\"msg\":%s}", jmsg);
    g_free(jmsg);
    ws_send_text(fd, json, strlen(json));
    g_free(json);
}

/* Live capture from Unix-socket pcap stream */

/* Read exactly n bytes from fd, retrying on short reads. Returns false on EOF/error. */
static bool read_exact(int fd, void *buf, size_t n)
{
    size_t done = 0;
    while (done < n) {
        ssize_t r = read(fd, (char *)buf + done, n - done);
        if (r <= 0) return false;
        done += r;
    }
    return true;
}

/* Write all of n bytes to a pipe left non-blocking, waiting for room. */
static int write_blocking(int fd, const void *buf, size_t n)
{
    size_t done = 0;
    while (done < n) {
        ssize_t w = write(fd, (const char *)buf + done, n - done);
        if (w > 0) { done += w; continue; }
        if ((w < 0) && ((errno == EAGAIN) || (errno == EWOULDBLOCK) ||
                        (errno == EINTR))) {
            struct pollfd p = { .fd = fd, .events = POLLOUT };
            poll(&p, 1, 100);
            continue;
        }
        return -1;
    }
    return 0;
}

/* Row batch of the dissection thread: filled with g_cf_mutex held, flushed
   without it, so g_cf_mutex and g_clients_mutex are never held together. */
static GString *g_batch      = NULL;
static uint32_t g_batch_rows = 0;
static gint64   g_batch_us   = 0;    /* when the batch took its first row */
static bool     g_cols_sent  = false;
/* A filter change makes the rows gathered before it stale: the replay of the
   filter holds them. g_order keeps a flush and a replay from crossing. */
static uint32_t g_filter_gen = 0;
static uint32_t g_batch_gen  = 0;
static pthread_mutex_t g_order = PTHREAD_MUTEX_INITIALIZER;

/* Called with g_cf_mutex held, so g_cf.cinfo can be read. */
static void batch_open(void)
{
    if (g_batch->len && (g_batch_gen != g_filter_gen)) {
        g_string_truncate(g_batch, 0);
        g_batch_rows = 0;
    }
    if (g_batch->len)
        return;
    g_batch_gen = g_filter_gen;
    g_batch_us = g_get_monotonic_time();
    g_string_append(g_batch, "{\"type\":\"packet_list\",\"cols\":");
    if (!g_cols_sent) {
        g_string_append_c(g_batch, '[');
        for (int i = 0; i < g_cf.cinfo.num_cols; i++) {
            char *jt = json_escape(g_cf.cinfo.columns[i].col_title);
            if (i) g_string_append_c(g_batch, ',');
            g_string_append(g_batch, jt);
            g_free(jt);
        }
        g_string_append_c(g_batch, ']');
        g_cols_sent = true;
    }
    else
        g_string_append(g_batch, "null");
    g_string_append(g_batch, ",\"packets\":[");
}

/* Send whatever has been gathered. Must run without g_cf_mutex held. */
static void batch_flush(void)
{
    char *json;
    if (!g_batch_rows) {
        g_string_truncate(g_batch, 0);
        return;
    }
    g_string_append_printf(g_batch, "],\"total\":%u}", g_frame_count);
    json = g_strdup(g_batch->str);
    g_string_truncate(g_batch, 0);
    pthread_mutex_lock(&g_order);
    if (g_batch_gen == g_filter_gen)
        ws_broadcast_rows(json, g_batch_rows);
    pthread_mutex_unlock(&g_order);
    g_batch_rows = 0;
    g_free(json);
}

/* Dissect one raw packet and add its row to the batch.
   Called with g_cf_mutex held. */
static void live_dissect_and_queue(const uint8_t *data, uint32_t caplen,
                                   uint32_t origlen, uint32_t ts_sec,
                                   uint32_t ts_usec)
{
    g_frame_count++;
    uint32_t fn = g_frame_count;

    /* Build wtap_rec */
    wtap_rec rec;
    memset(&rec, 0, sizeof(rec));
    rec.rec_type           = REC_TYPE_PACKET;
    rec.presence_flags     = WTAP_HAS_TS | WTAP_HAS_CAP_LEN;
    rec.ts.secs            = ts_sec;
    rec.ts.nsecs           = ts_usec * 1000;
    rec.rec_header.packet_header.caplen    = caplen;
    rec.rec_header.packet_header.len       = origlen;
    rec.rec_header.packet_header.pkt_encap = g_cf.cd_t;

    /* Build a local frame_data, then add to sequence (which copies it) */
    frame_data fdlocal;
    frame_data_init(&fdlocal, fn, &rec, 0, g_cf.cum_bytes);
    fdlocal.passed_dfilter = 1;

    frame_data *fdata = frame_data_sequence_add(g_cf.provider.frames, &fdlocal);
    g_cf.count = fn;
    fdata->passed_dfilter = 0;

    /* Set timing fields on the sequence-owned copy so provider.ref stays valid */
    frame_data_set_before_dissect(fdata, &g_cf.elapsed_time,
                                  &g_cf.provider.ref, g_cf.provider.prev_dis);

    /* Keep a copy of raw bytes for later re-dissection (select / bytes cmds) */
    if (g_live_pkts) {
        GByteArray *ba = g_byte_array_new();
        g_byte_array_append(ba, data, caplen);
        g_hash_table_insert(g_live_pkts, GUINT_TO_POINTER(fn), ba);
        if (fn > KEEP_RAW)
            g_hash_table_remove(g_live_pkts, GUINT_TO_POINTER(fn - KEEP_RAW));
    }

    /* Dissect using the sequence-owned frame_data */
    Buffer buf_holder;
    ws_buffer_init(&buf_holder, caplen);
    /* ws_buffer_append takes a non-const source although it only copies from */
    /* it; data is our read-only packet buffer, so the cast is safe. */
    ws_buffer_append(&buf_holder, (uint8_t *)data, caplen);

    /* A tree is built only when there is a filter to apply to it, and then
       only for the fields that filter names. */
    bool want_tree = (g_live_dfcode != NULL);
    epan_dissect_t *edt = epan_dissect_new(g_cf.epan, want_tree, false);
    if (g_live_dfcode)
        epan_dissect_prime_with_dfilter(edt, g_live_dfcode);
    epan_dissect_run(edt, g_cf.cd_t,
                     &rec,
                     frame_tvbuff_new_buffer(&g_cf.provider, fdata, &buf_holder),
                     fdata, &g_cf.cinfo);

    if (g_live_dfcode && !dfilter_apply_edt(g_live_dfcode, edt)) {
        /* Kept and counted, never sent: this is where the volume goes. */
        frame_data_set_after_dissect(fdata, &g_cf.cum_bytes);
        epan_dissect_free(edt);
        ws_buffer_free(&buf_holder);
        return;
    }

    fdata->passed_dfilter = 1;
    epan_dissect_fill_in_columns(edt, false, true);
    frame_data_set_after_dissect(fdata, &g_cf.cum_bytes);
    g_cf.provider.prev_dis = fdata;

    batch_open();
    if (g_batch_rows)
        g_string_append_c(g_batch, ',');
    g_string_append_printf(g_batch, "{\"num\":%u,\"cols\":[", fn);
    for (int i = 0; i < g_cf.cinfo.num_cols; i++) {
        char *jv = json_escape(get_column_text(&g_cf.cinfo, i));
        if (i) g_string_append_c(g_batch, ',');
        g_string_append(g_batch, jv);
        g_free(jv);
    }
    g_string_append(g_batch, "]}");
    g_batch_rows++;

    epan_dissect_free(edt);
    ws_buffer_free(&buf_holder);
    /* fdata is owned by the sequence; fdlocal is on the stack — no cleanup needed */
}

/* A subscriber of <snf_path>_pcap_sock, the socket of the dumpcap the switch
 * runs for this snf (ovs_snf.c): a stream of its own, its own pcap file
 * header first. Returns a read fd, or -1 with errno. */
#define SNF_SOCK_WAIT_S 15
static int open_fanout_sock(const char *snf_path)
{
    struct sockaddr_un sa = {0};
    int err, fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
        return -1;
    sa.sun_family = AF_UNIX;
    snprintf(sa.sun_path, sizeof(sa.sun_path), "%s_pcap_sock", snf_path);
    if (connect(fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        err = errno;
        close(fd);
        errno = err;
        return -1;
    }
    return fd;
}

/* Fallback when no dumpcap runs for this snf: spawn one of our own. */
static int spawn_dumpcap(const char *snf_path, pid_t *out_pid)
{
    int pfd[2];
    if (pipe(pfd) < 0) {
        fprintf(stderr, "spawn_dumpcap: pipe failed: %s\n", strerror(errno));
        return -1;
    }
    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "spawn_dumpcap: fork failed: %s\n", strerror(errno));
        close(pfd[0]); close(pfd[1]);
        return -1;
    }
    if (pid == 0) {
        /* child: stdout -> pipe, stderr -> /dev/null, exec dumpcap */
        dup2(pfd[1], 1);
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) { dup2(devnull, 2); close(devnull); }
        close(pfd[0]); close(pfd[1]);
        execl(g_dumpcap_bin, g_dumpcap_bin, "-i", snf_path, "-Z", "none",
              (char *)NULL);
        _exit(127);
    }
    /* parent */
    close(pfd[1]);
    *out_pid = pid;
    return pfd[0];
}

/* Close the source; reap the child only if we spawned one (g_dumpcap_pid != -1). */
static void reap_dumpcap(int fd)
{
    if (fd >= 0) close(fd);
    if (g_dumpcap_pid > 0) {
        kill(g_dumpcap_pid, SIGTERM);
        int st;
        /* brief wait, then SIGKILL fallback so we never leak a dumpcap */
        for (int i = 0; i < 20; i++) {
            if (waitpid(g_dumpcap_pid, &st, WNOHANG) == g_dumpcap_pid) { g_dumpcap_pid = -1; break; }
            usleep(10000);
        }
        if (g_dumpcap_pid > 0) {
            kill(g_dumpcap_pid, SIGKILL);
            waitpid(g_dumpcap_pid, &st, 0);
            g_dumpcap_pid = -1;
        }
    }
}

/* Background thread: the pcap of the switch's dumpcap, as a subscriber of its
 * socket, or of a dumpcap of our own */
static void *capture_thread(void *arg)
{
    const char *snf_path = (const char *)arg;
    pid_t pid = -1;
    int fd = -1;

    /* Wait up to SNF_SOCK_WAIT_S for the socket of the switch's dumpcap (it
     * is still coming up right after the interface is made; a second dumpcap
     * on the snf fifo split the stream in two). Spawn our own only after. */
    for (int i = 0; (i < SNF_SOCK_WAIT_S * 10) && (fd < 0); i++) {
        fd = open_fanout_sock(snf_path);
        if (fd < 0)
            usleep(100000);
    }
    if (fd >= 0)
        fprintf(stderr, "capture_thread: subscriber of %s_pcap_sock\n",
                snf_path);
    else {
        fprintf(stderr, "capture_thread: no %s_pcap_sock (%s), spawning: "
                "%s -i %s\n", snf_path, strerror(errno), g_dumpcap_bin,
                snf_path);
        fd = spawn_dumpcap(snf_path, &pid);
    }
    if (fd < 0) {
        fprintf(stderr, "capture_thread: no capture source\n");
        g_capture_running = false;
        return NULL;
    }
    g_dumpcap_pid = pid;

    /* Read pcap magic */
    uint32_t magic = 0;
    if (!read_exact(fd, &magic, 4)) {
        fprintf(stderr, "capture_thread: cannot read magic from dumpcap stdout\n");
        reap_dumpcap(fd); g_capture_running = false; return NULL;
    }

    bool byte_swapped = false;
    bool nsec         = false;
    if      (magic == PCAP_MAGIC)         { /* native, us timestamps */ }
    else if (magic == PCAP_SWAPPED_MAGIC) { byte_swapped = true; }
    else if (magic == PCAP_NSEC_MAGIC)    { nsec = true; }
    else if (magic == PCAP_SWAPPED_NSEC_MAGIC) { byte_swapped = true; nsec = true; }
    else {
        fprintf(stderr, "capture_thread: unexpected magic 0x%08x from dumpcap\n", magic);
        reap_dumpcap(fd); g_capture_running = false; return NULL;
    }

    /* Read global header */
    struct pcap_hdr ghdr;
    if (!read_exact(fd, &ghdr, sizeof(ghdr))) {
        fprintf(stderr, "capture_thread: cannot read global header\n");
        reap_dumpcap(fd); g_capture_running = false; return NULL;
    }
    if (byte_swapped) {
        ghdr.snaplen = GUINT32_SWAP_LE_BE(ghdr.snaplen);
        ghdr.network = GUINT32_SWAP_LE_BE(ghdr.network);
    }

    pthread_mutex_lock(&g_cf_mutex);
    g_cf.cd_t = wtap_pcap_encap_to_wtap_encap(ghdr.network);
    g_pcap_network = ghdr.network;   /* remember for cmd_save's pcap header */
    g_frame_count = 0;
    nstime_set_zero(&g_cf.elapsed_time);
    g_cf.cum_bytes       = 0;
    g_cf.provider.ref    = NULL;
    g_cf.provider.prev_dis = NULL;
    if (g_live_pkts)
        g_hash_table_destroy(g_live_pkts);
    g_live_pkts = g_hash_table_new_full(g_direct_hash, g_direct_equal,
                                        NULL,
                                        (GDestroyNotify)g_byte_array_unref);
    pthread_mutex_unlock(&g_cf_mutex);

    fprintf(stderr, "capture_thread: live capture started linktype=%u encap=%d\n",
            ghdr.network, g_cf.cd_t);
    /* the page offers what fits the link: the ip toggle on 802.11 */
    {
        char link[64];
        g_snprintf(link, sizeof(link), "{\"type\":\"link\",\"linktype\":%u}",
                   ghdr.network);
        g_link_known = true;
        ws_broadcast(link);
    }

    uint32_t snaplen = ghdr.snaplen ? ghdr.snaplen : 262144;
    uint8_t *pkt_buf = g_malloc(snaplen + 16);
    uint8_t  wbuf[PIPE_BUF];
    time_t   src_warn = 0;

    while (g_capture_running) {
        struct pcaprec_hdr rhdr;
        if (!read_exact(fd, &rhdr, sizeof(rhdr))) break;

        if (byte_swapped) {
            rhdr.ts_sec   = GUINT32_SWAP_LE_BE(rhdr.ts_sec);
            rhdr.ts_usec  = GUINT32_SWAP_LE_BE(rhdr.ts_usec);
            rhdr.incl_len = GUINT32_SWAP_LE_BE(rhdr.incl_len);
            rhdr.orig_len = GUINT32_SWAP_LE_BE(rhdr.orig_len);
        }
        if (nsec) rhdr.ts_usec /= 1000;

        if (rhdr.incl_len > snaplen + 16) {
            fprintf(stderr, "capture_thread: insane incl_len %u\n", rhdr.incl_len);
            break;
        }

        if (!read_exact(fd, pkt_buf, rhdr.incl_len)) break;

        /* To the dissection thread (epan TRY/CATCH is thread-local). Header
           and payload in ONE write: below PIPE_BUF it is atomic, a full pipe
           gives EAGAIN and the packet is dropped and counted here. */
        pkt_header_t ph = {
            .caplen  = rhdr.incl_len,
            .origlen = rhdr.orig_len,
            .ts_sec  = rhdr.ts_sec,
            .ts_usec = rhdr.ts_usec,
        };
        size_t need = sizeof(ph) + rhdr.incl_len;
        g_src_seen++;
        if (need <= sizeof(wbuf)) {
            memcpy(wbuf, &ph, sizeof(ph));
            memcpy(wbuf + sizeof(ph), pkt_buf, rhdr.incl_len);
            ssize_t w = write(g_pkt_pipe[1], wbuf, need);
            if ((w < 0) && ((errno == EAGAIN) || (errno == EWOULDBLOCK))) {
                g_src_dropped++;
                if (warn_due(&src_warn))
                    fprintf(stderr, "flow: dissection is behind, %llu of %llu "
                                    "packets dropped at the input\n",
                            (unsigned long long)g_src_dropped,
                            (unsigned long long)g_src_seen);
                continue;
            }
            if (w != (ssize_t)need) break;
        }
        else {
            /* Bigger than the atomic size: this one waits rather than risk
               a torn record. Only jumbo snaplens get here. */
            if (write_blocking(g_pkt_pipe[1], &ph, sizeof(ph)) < 0) break;
            if (write_blocking(g_pkt_pipe[1], pkt_buf, rhdr.incl_len) < 0) break;
        }
    }

    g_free(pkt_buf);
    reap_dumpcap(fd);
    g_capture_running = false;
    fprintf(stderr, "capture_thread: stopped after %u packets\n", g_frame_count);
    return NULL;
}

/* The capture stopped itself: tell the panel, once. */
static void capture_full(void)
{
    char msg[160];
    g_capture_running = false;
    if (g_dumpcap_pid > 0)
        kill(g_dumpcap_pid, SIGTERM);
    fprintf(stderr, "ERROR capacity: %d packets held, the capture is stopped\n",
            MAX_PACKETS);
    g_snprintf(msg, sizeof(msg),
        "{\"type\":\"stopped\",\"frames\":%u,\"reason\":\"capacity\"}",
        g_frame_count);
    ws_broadcast(msg);
}

/* What was lost at the input, reported to the panel when it changes. */
static void report_source_drops(void)
{
    static uint64_t last = 0;
    static time_t   when = 0;
    char msg[192];

    if ((g_src_dropped == last) || !warn_due(&when))
        return;
    last = g_src_dropped;
    g_snprintf(msg, sizeof(msg),
        "{\"type\":\"flow\",\"state\":\"source\",\"dropped\":%llu,\"seen\":%llu}",
        (unsigned long long)g_src_dropped, (unsigned long long)g_src_seen);
    ws_broadcast(msg);
}

/* Dissection thread, big stack for deep dissector chains. The poll timeout
   flushes a half full batch on a quiet link. */
static void *dissect_thread(void *arg)
{
    (void)arg;
    uint8_t buf[65536];
    struct pollfd pfd = { .fd = g_pkt_pipe[0], .events = POLLIN };
    bool full = false;

    while (g_capture_running) {
        pkt_header_t ph;
        ssize_t r;

        int pr = poll(&pfd, 1, BATCH_MAX_MS);
        if (pr < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (pr == 0) {
            batch_flush();
            report_source_drops();
            continue;
        }

        r = read(g_pkt_pipe[0], &ph, sizeof(ph));
        if (r != (ssize_t)sizeof(ph)) break;
        if (ph.caplen > sizeof(buf)) break;  /* sanity */

        ssize_t got = 0;
        while (got < (ssize_t)ph.caplen) {
            r = read(g_pkt_pipe[0], buf + got, ph.caplen - got);
            if (r <= 0) goto done;
            got += r;
        }

        pthread_mutex_lock(&g_cf_mutex);
        if (g_frame_count >= MAX_PACKETS)
            full = true;
        else
            live_dissect_and_queue(buf, ph.caplen, ph.origlen,
                                   ph.ts_sec, ph.ts_usec);
        pthread_mutex_unlock(&g_cf_mutex);

        /* Both of these take g_clients_mutex, so neither may run above. */
        if (full) {
            batch_flush();
            capture_full();
            break;
        }
        /* a steady stream never lets the poll time out: a row waits
           BATCH_MAX_MS at most all the same */
        if ((g_batch_rows >= BATCH_MAX_ROWS) ||
            (g_batch_rows && (g_get_monotonic_time() - g_batch_us >=
                              BATCH_MAX_MS * 1000))) {
            batch_flush();
            report_source_drops();
        }
    }
done:
    batch_flush();
    return NULL;
}

static void start_live_capture(const char *path)
{
    if (g_capture_running) return;
    if (pipe(g_pkt_pipe) < 0) {
        perror("pipe"); return;
    }
    /* Room for a burst, and a write side that reports a full pipe instead of
       waiting on it. */
    /* F_SETPIPE_SZ above pipe-max-size is EPERM and leaves 64k (600 packets): ask what the kernel allows. */
    {
        long want = PKT_PIPE_BYTES, maxsz = 0;
        FILE *f = fopen("/proc/sys/fs/pipe-max-size", "r");
        if (f) {
            if (fscanf(f, "%ld", &maxsz) != 1)
                maxsz = 0;
            fclose(f);
        }
        if (maxsz > 0 && maxsz < want)
            want = maxsz;
        if (fcntl(g_pkt_pipe[1], F_SETPIPE_SZ, want) < 0)
            fprintf(stderr, "ERROR packet pipe stays at %ld bytes, %ld refused: %s\n",
                    (long)fcntl(g_pkt_pipe[1], F_GETPIPE_SZ), want, strerror(errno));
    }
    fcntl(g_pkt_pipe[1], F_SETFL,
          fcntl(g_pkt_pipe[1], F_GETFL, 0) | O_NONBLOCK);

    if (!g_batch)
        g_batch = g_string_new(NULL);
    g_capture_running = true;

    /* Capture thread: reads from dumpcap, writes raw packets to pipe */
    pthread_create(&g_capture_thread, NULL, capture_thread, (void *)path);
    pthread_detach(g_capture_thread);

    /* Dissection thread: reads from pipe, dissects, broadcasts.
       64 MB stack — Wireshark dissectors can be deeply recursive. */
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 64 * 1024 * 1024);
    pthread_t dtid;
    pthread_create(&dtid, &attr, dissect_thread, NULL);
    pthread_attr_destroy(&attr);
    pthread_detach(dtid);
}

/* cmd: open — kept for filter replay; not used for live socket */
/* cmd: select — send proto tree for a frame */
static void cmd_select(int fd, uint32_t frame_num)
{
    pthread_mutex_lock(&g_cf_mutex);

    frame_data *fdata = frame_data_sequence_find(g_cf.provider.frames, frame_num);
    if (!fdata) {
        pthread_mutex_unlock(&g_cf_mutex);
        send_error(fd, "Frame not found");
        return;
    }

    wtap_rec rec;
    Buffer   buf;
    wtap_rec_init(&rec);
    ws_buffer_init(&buf, 1514);

    /* For live captures use the in-memory store; for file captures seek via wtap. */
    GByteArray *live_ba = g_live_pkts
        ? g_hash_table_lookup(g_live_pkts, GUINT_TO_POINTER(frame_num))
        : NULL;

    if (live_ba) {
        rec.rec_type                           = REC_TYPE_PACKET;
        rec.presence_flags                     = WTAP_HAS_TS | WTAP_HAS_CAP_LEN;
        rec.rec_header.packet_header.caplen    = live_ba->len;
        rec.rec_header.packet_header.len       = live_ba->len;
        rec.rec_header.packet_header.pkt_encap = g_cf.cd_t;
        ws_buffer_append(&buf, live_ba->data, live_ba->len);
    } else {
        /* No wtap file behind a live capture: bytes past KEEP_RAW are gone. */
        pthread_mutex_unlock(&g_cf_mutex);
        wtap_rec_cleanup(&rec);
        ws_buffer_free(&buf);
        send_error(fd, "Frame bytes no longer kept");
        return;
    }

    epan_dissect_t edt;
    epan_dissect_init(&edt, g_cf.epan, true, true);
    epan_dissect_run(&edt, g_cf.cd_t, &rec,
                     frame_tvbuff_new_buffer(&g_cf.provider, fdata, &buf),
                     fdata, &g_cf.cinfo);

    /* Build JSON tree */
    GString *gs = g_string_new(NULL);
    g_string_append_printf(gs, "{\"type\":\"proto_tree\",\"frame\":%u,\"tree\":[",
                           frame_num);

    bool first = true;
    for (proto_node *top = edt.tree->first_child; top; top = top->next) {
        if (!first) g_string_append_c(gs, ',');
        first = false;
        proto_node_to_json(top, gs, 0);
    }
    g_string_append(gs, "]}");

    epan_dissect_cleanup(&edt);
    wtap_rec_cleanup(&rec);
    ws_buffer_free(&buf);
    pthread_mutex_unlock(&g_cf_mutex);

    char *json = g_string_free(gs, FALSE);
    ws_send_text(fd, json, strlen(json));
    g_free(json);
}

/* cmd: bytes — send hex + ascii dump */
static void cmd_bytes(int fd, uint32_t frame_num)
{
    pthread_mutex_lock(&g_cf_mutex);

    frame_data *fdata = frame_data_sequence_find(g_cf.provider.frames, frame_num);
    if (!fdata) {
        pthread_mutex_unlock(&g_cf_mutex);
        send_error(fd, "Frame not found");
        return;
    }

    wtap_rec rec;
    Buffer   buf;
    wtap_rec_init(&rec);
    ws_buffer_init(&buf, 65536);

    GByteArray *live_ba = g_live_pkts
        ? g_hash_table_lookup(g_live_pkts, GUINT_TO_POINTER(frame_num))
        : NULL;

    if (live_ba) {
        ws_buffer_append(&buf, live_ba->data, live_ba->len);
    } else {
        pthread_mutex_unlock(&g_cf_mutex);
        wtap_rec_cleanup(&rec);
        ws_buffer_free(&buf);
        send_error(fd, "Frame bytes no longer kept");
        return;
    }

    const uint8_t *data   = ws_buffer_start_ptr(&buf);
    uint32_t       datalen = (uint32_t)ws_buffer_length(&buf);

    GString *hex   = g_string_new(NULL);
    GString *ascii = g_string_new(NULL);

    for (uint32_t i = 0; i < datalen; i++) {
        g_string_append_printf(hex, "%02x", data[i]);
        char c = (data[i] >= 0x20 && data[i] < 0x7f) ? (char)data[i] : '.';
        if (c == '"' || c == '\\') g_string_append_c(ascii, '\\');
        g_string_append_c(ascii, c);
    }

    pthread_mutex_unlock(&g_cf_mutex);
    wtap_rec_cleanup(&rec);
    ws_buffer_free(&buf);

    char *json = g_strdup_printf(
        "{\"type\":\"bytes\",\"frame\":%u,\"len\":%u,\"hex\":\"%s\",\"ascii\":\"%s\"}",
        frame_num, datalen, hex->str, ascii->str);

    ws_send_text(fd, json, strlen(json));
    g_free(json);
    g_string_free(hex,   TRUE);
    g_string_free(ascii, TRUE);
}

/* Bounded packet list, built with g_cf_mutex already held. Defined with the
   rest of the replay path further down. */
static char *build_list_json_locked(void);
static void cmd_filter_ordered(int fd, const char *expr);

/* cmd: filter. The expression is kept for the live stream; the scan below
 * gives the packets already held the same verdict. The list is bounded and
 * built with the lock held (an unbounded "ip" answer was megabytes). */
static void cmd_filter(int fd, const char *expr)
{
    pthread_mutex_lock(&g_order);
    cmd_filter_ordered(fd, expr);
    pthread_mutex_unlock(&g_order);
}

static void cmd_filter_ordered(int fd, const char *expr)
{
    pthread_mutex_lock(&g_cf_mutex);
    g_filter_gen += 1;
    if (g_cf.state == FILE_CLOSED) {
        pthread_mutex_unlock(&g_cf_mutex);
        send_error(fd, "No file open");
        return;
    }

    dfilter_t  *dfcode = NULL;
    df_error_t *df_err = NULL;

    if (expr && *expr) {
        if (!dfilter_compile(expr, &dfcode, &df_err)) {
            pthread_mutex_unlock(&g_cf_mutex);
            char *jmsg = json_escape(df_err ? df_err->msg : "Filter error");
            char *json = g_strdup_printf("{\"type\":\"error\",\"msg\":%s}", jmsg);
            g_free(jmsg);
            ws_send_text(fd, json, strlen(json));
            g_free(json);
            if (df_err) df_error_free(&df_err);
            return;
        }
    }

    if (g_live_dfcode)
        dfilter_free(g_live_dfcode);
    g_live_dfcode = dfcode;
    g_free(g_live_filter_expr);
    g_live_filter_expr = g_strdup(expr ? expr : "");

    for (uint32_t fn = 1; fn <= g_cf.count; fn++) {
        frame_data *fdata = frame_data_sequence_find(g_cf.provider.frames, fn);
        if (!fdata)
            continue;
        if (!dfcode) {
            fdata->passed_dfilter = 1;
            continue;
        }
        /* Bytes gone from the ring: the frame cannot be judged, it is out. */
        if ((g_cf.count > KEEP_RAW) && (fn <= g_cf.count - KEEP_RAW)) {
            fdata->passed_dfilter = 0;
            continue;
        }

        wtap_rec rec2;
        Buffer   buf2;
        wtap_rec_init(&rec2);
        ws_buffer_init(&buf2, 1514);

        GByteArray *live_ba = g_live_pkts
            ? g_hash_table_lookup(g_live_pkts, GUINT_TO_POINTER(fn))
            : NULL;

        if (live_ba) {
            rec2.rec_type                           = REC_TYPE_PACKET;
            rec2.presence_flags                     = WTAP_HAS_TS | WTAP_HAS_CAP_LEN;
            rec2.rec_header.packet_header.caplen    = live_ba->len;
            rec2.rec_header.packet_header.len       = live_ba->len;
            rec2.rec_header.packet_header.pkt_encap = g_cf.cd_t;
            ws_buffer_append(&buf2, live_ba->data, live_ba->len);
        } else {
            wtap_rec_cleanup(&rec2);
            ws_buffer_free(&buf2);
            fdata->passed_dfilter = 0;
            continue;
        }

        epan_dissect_t fedt;
        epan_dissect_init(&fedt, g_cf.epan, true, false);
        epan_dissect_prime_with_dfilter(&fedt, dfcode);
        epan_dissect_run(&fedt, g_cf.cd_t, &rec2,
                         frame_tvbuff_new_buffer(&g_cf.provider, fdata, &buf2),
                         fdata, NULL);
        fdata->passed_dfilter = dfilter_apply_edt(dfcode, &fedt) ? 1 : 0;
        epan_dissect_cleanup(&fedt);
        wtap_rec_cleanup(&rec2);
        ws_buffer_free(&buf2);
    }

    /* Under the scan's lock: a live batch must not slip in front of the table replacement. */
    char *json = build_list_json_locked();
    pthread_mutex_unlock(&g_cf_mutex);

    /* Every panel of this interface: the filter decides what the live stream carries. */
    (void)fd;
    if (json) {
        ws_broadcast(json);
        g_free(json);
    }
}

/* Message dispatcher */

/* Minimal JSON key extraction — good enough for our protocol */
static char *json_get_str(const char *json, const char *key)
{
    char search[64];
    g_snprintf(search, sizeof(search), "\"%s\":", key);
    const char *p = strstr(json, search);
    if (!p) return NULL;
    p += strlen(search);
    while (*p == ' ') p++;
    if (*p != '"') return NULL;
    p++;
    GString *gs = g_string_new(NULL);
    while (*p && *p != '"') {
        if (*p == '\\' && *(p+1)) p++;
        g_string_append_c(gs, *p++);
    }
    return g_string_free(gs, FALSE);
}

static bool json_get_uint(const char *json, const char *key, uint32_t *out)
{
    char search[64];
    g_snprintf(search, sizeof(search), "\"%s\":", key);
    const char *p = strstr(json, search);
    if (!p) return false;
    p += strlen(search);
    while (*p == ' ') p++;
    char *end;
    long val = strtol(p, &end, 10);
    if (end == p) return false;
    *out = (uint32_t)val;
    return true;
}

/* cmd: stop  — halt the live capture, leaving captured packets intact */
/* so they can be browsed and saved. Replies {"type":"stopped",...}. */
static void cmd_stop(int fd)
{
    if (g_capture_running) {
        /* Stop the loop, then kill dumpcap so the blocking read returns at once. */
        g_capture_running = false;
        if (g_dumpcap_pid > 0) {
            kill(g_dumpcap_pid, SIGTERM);
        }
    }
    char msg[128];
    g_snprintf(msg, sizeof(msg),
               "{\"type\":\"stopped\",\"frames\":%u}", g_frame_count);
    ws_send_text(fd, msg, strlen(msg));
}

/* cmd: save - pcap rebuilt from g_live_pkts and the frame_data sequence,
   original linktype. Replies saved{url} or error. */
static bool write_u32(int fd, uint32_t v)
{
    return write(fd, &v, 4) == 4;
}

/* Home of the user running the net, from getpwuid: $HOME of this process
   chain is not trusted. */
static const char *save_home(void)
{
    static char home[512];
    if (!home[0]) {
        struct passwd *pw = getpwuid(getuid());
        if (pw && pw->pw_dir && pw->pw_dir[0])
            g_snprintf(home, sizeof(home), "%s", pw->pw_dir);
        else
            g_snprintf(home, sizeof(home), "/tmp");
    }
    return home;
}

static void save_default_path(char *out, int len)
{
    g_snprintf(out, len, "%s/capture.pcap", save_home());
}

/* A save path must stay inside that home; ".." is refused, not resolved.
 * Returns NULL when fine, else the reason. */
static const char *save_path_refused(const char *path)
{
    const char *home = save_home();
    size_t hlen = strlen(home);
    if (!path || path[0] != '/')
        return "The path has to be absolute";
    if (strstr(path, ".."))
        return "The path may not contain ..";
    if (strncmp(path, home, hlen) || (path[hlen] != '/'))
        return "The path has to be inside your home directory";
    if (path[hlen + 1] == '\0')
        return "That is the directory itself, not a file";
    return NULL;
}
static void cmd_save(int fd, const char *want_path)
{
    char dest[512];
    const char *refused;

    /* No path: temp file served over http (works for a remote browser). A path: written there. */
    if (want_path && want_path[0]) {
        refused = save_path_refused(want_path);
        if (refused) {
            char e[512];
            g_snprintf(e, sizeof(e),
                       "{\"type\":\"error\",\"msg\":\"%s\"}", refused);
            ws_send_text(fd, e, strlen(e));
            return;
        }
        g_snprintf(dest, sizeof(dest), "%s", want_path);
    }
    else
        dest[0] = 0;

    pthread_mutex_lock(&g_cf_mutex);
    uint32_t nframes = g_frame_count;
    uint32_t network = g_pcap_network;
    if (nframes == 0 || !g_live_pkts) {
        pthread_mutex_unlock(&g_cf_mutex);
        const char *e = "{\"type\":\"error\",\"msg\":\"No packets to save\"}";
        ws_send_text(fd, e, strlen(e));
        return;
    }

    char tmp[512];
    if (dest[0])
        g_snprintf(tmp, sizeof(tmp), "%s", dest);
    else
        g_snprintf(tmp, sizeof(tmp), "/tmp/webshark_save_%d.pcap", (int)getpid());
    int out = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out < 0) {
        pthread_mutex_unlock(&g_cf_mutex);
        char e[640];
        g_snprintf(e, sizeof(e),
                   "{\"type\":\"error\",\"msg\":\"Cannot write %s: %s\"}",
                   tmp, g_strerror(errno));
        ws_send_text(fd, e, strlen(e));
        return;
    }

    /* pcap global header: magic + version 2.4 + zone/sigfigs 0 + snaplen + net */
    bool ok = true;
    ok = ok && write_u32(out, PCAP_MAGIC);
    uint16_t vmaj = 2, vmin = 4;
    ok = ok && (write(out, &vmaj, 2) == 2);
    ok = ok && (write(out, &vmin, 2) == 2);
    ok = ok && write_u32(out, 0);          /* thiszone */
    ok = ok && write_u32(out, 0);          /* sigfigs */
    ok = ok && write_u32(out, 262144);     /* snaplen */
    ok = ok && write_u32(out, network);    /* linktype */

    uint32_t written = 0;
    for (uint32_t fn = 1; ok && fn <= nframes; fn++) {
        GByteArray *ba = g_hash_table_lookup(g_live_pkts, GUINT_TO_POINTER(fn));
        if (!ba) continue;   /* should not happen, but skip gaps safely */
        frame_data *fdata = frame_data_sequence_find(g_cf.provider.frames, fn);

        uint32_t ts_sec  = 0, ts_usec = 0, orig_len = ba->len;
        if (fdata) {
            ts_sec   = (uint32_t)fdata->abs_ts.secs;
            ts_usec  = (uint32_t)(fdata->abs_ts.nsecs / 1000);
            orig_len = fdata->pkt_len ? fdata->pkt_len : ba->len;
        }
        ok = ok && write_u32(out, ts_sec);
        ok = ok && write_u32(out, ts_usec);
        ok = ok && write_u32(out, ba->len);   /* incl_len (caplen) */
        ok = ok && write_u32(out, orig_len);  /* orig_len */
        ok = ok && (write(out, ba->data, ba->len) == (ssize_t)ba->len);
        if (ok) written++;
    }
    close(out);
    pthread_mutex_unlock(&g_cf_mutex);

    if (!ok) {
        unlink(tmp);
        const char *e = "{\"type\":\"error\",\"msg\":\"Write error while saving\"}";
        ws_send_text(fd, e, strlen(e));
        return;
    }

    if (dest[0]) {
        /* Saved where it was asked for; there is nothing to download. */
        char msg2[700];
        g_snprintf(msg2, sizeof(msg2),
            "{\"type\":\"saved\",\"path\":\"%s\",\"frames\":%u}",
            dest, written);
        ws_send_text(fd, msg2, strlen(msg2));
        return;
    }

    /* Publish the path so the HTTP handler can serve /save.pcap. */
    pthread_mutex_lock(&g_save_mutex);
    g_snprintf(g_save_path, sizeof(g_save_path), "%s", tmp);
    pthread_mutex_unlock(&g_save_mutex);

    /* RELATIVE url: the page is served under /ws-<sock>/ by the gui server,
     * an absolute one would hit the gui server root and 404. */
    char msg[256];
    g_snprintf(msg, sizeof(msg),
        "{\"type\":\"saved\",\"url\":\"save.pcap\",\"name\":\"capture.pcap\",\"frames\":%u}",
        written);
    ws_send_text(fd, msg, strlen(msg));
}

static void dispatch_command(int fd, const char *json)
{
    char *cmd = json_get_str(json, "cmd");
    if (!cmd) return;

    if (strcmp(cmd, "select") == 0) {
        uint32_t frame = 0;
        if (json_get_uint(json, "frame", &frame))
            cmd_select(fd, frame);
        else
            send_error(fd, "Missing frame parameter");
    } else if (strcmp(cmd, "bytes") == 0) {
        uint32_t frame = 0;
        if (json_get_uint(json, "frame", &frame))
            cmd_bytes(fd, frame);
        else
            send_error(fd, "Missing frame parameter");
    } else if (strcmp(cmd, "filter") == 0) {
        char *expr = json_get_str(json, "expr");
        cmd_filter(fd, expr ? expr : "");
        g_free(expr);
    } else if (strcmp(cmd, "stop") == 0) {
        cmd_stop(fd);
    } else if (strcmp(cmd, "save") == 0) {
        char *path = json_get_str(json, "path");
        cmd_save(fd, path);
        g_free(path);
    } else if (strcmp(cmd, "save_where") == 0) {
        /* What to put in the box before the user types anything. */
        char def[512], m[640];
        save_default_path(def, sizeof(def));
        g_snprintf(m, sizeof(m),
                   "{\"type\":\"save_where\",\"path\":\"%s\"}", def);
        ws_send_text(fd, m, strlen(m));
    } else {
        send_error(fd, "Unknown command");
    }

    g_free(cmd);
}

/* HTTP request handler (serves the single-page frontend) */

static void handle_http(int fd, const char *request)
{
    /* Only handle GET / */
    if (strncmp(request, "GET ", 4) != 0) {
        const char *r405 =
            "HTTP/1.1 405 Method Not Allowed\r\n"
            "Content-Length: 0\r\n\r\n";
        send(fd, r405, strlen(r405), MSG_NOSIGNAL);
        return;
    }

    /* Extract path */
    const char *path_start = request + 4;
    const char *path_end   = strchr(path_start, ' ');
    if (!path_end) path_end = path_start + strlen(path_start);
    char path[256] = "/";
    int plen = (int)(path_end - path_start);
    if (plen < (int)sizeof(path)) {
        memcpy(path, path_start, plen);
        path[plen] = '\0';
    }

    /* Drop the query string: routes are exact strcmp and the download comes as save.pcap?t=<epoch>. */
    {
        char *q = strchr(path, '?');
        if (q) *q = '\0';
    }

    if (strcmp(path, "/") == 0 || strcmp(path, "/index.html") == 0) {
        char header[256];
        g_snprintf(header, sizeof(header),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html; charset=utf-8\r\n"
            "Content-Length: %zu\r\n"
            "Connection: close\r\n\r\n",
            g_webshark_html_len);
        if (send_all(fd, header, strlen(header)))
            send_all(fd, g_webshark_html, g_webshark_html_len);
    } else if (strcmp(path, "/save.pcap") == 0) {
        /* Serve the temp .pcap written by cmd_save as a file download. */
        char spath[512];
        pthread_mutex_lock(&g_save_mutex);
        g_snprintf(spath, sizeof(spath), "%s", g_save_path);
        pthread_mutex_unlock(&g_save_mutex);

        int pf = (spath[0]) ? open(spath, O_RDONLY) : -1;
        if (pf < 0) {
            const char *r404 =
                "HTTP/1.1 404 Not Found\r\n"
                "Content-Length: 13\r\n\r\n"
                "404 Not Found";
            send(fd, r404, strlen(r404), MSG_NOSIGNAL);
        } else {
            struct stat st;
            fstat(pf, &st);
            char header[320];
            g_snprintf(header, sizeof(header),
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: application/vnd.tcpdump.pcap\r\n"
                "Content-Disposition: attachment; filename=\"capture.pcap\"\r\n"
                "Content-Length: %lld\r\n"
                "Connection: close\r\n\r\n",
                (long long)st.st_size);
            char buf[65536];
            ssize_t n;
            if (send_all(fd, header, strlen(header))) {
                while ((n = read(pf, buf, sizeof(buf))) > 0) {
                    if (!send_all(fd, buf, n)) {
                        fprintf(stderr, "ERROR http: the download of %s was "
                                        "cut, errno=%d\n", spath, errno);
                        break;
                    }
                }
            }
            close(pf);
        }
    } else {
        const char *r404 =
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Length: 13\r\n\r\n"
            "404 Not Found";
        send(fd, r404, strlen(r404), MSG_NOSIGNAL);
    }
}

/* Per-client thread */

typedef struct { ws_client_t *cli; } client_thread_arg_t;

/* History for a (re)connecting client: the last REPLAY_MAX_ROWS rows passing
 * the filter, sent with replay:true (the client replaces its table).
 * Returns a string to free, or NULL. Takes and releases g_cf_mutex. */
static char *build_list_json_locked(void)
{
    if (g_cf.state == FILE_CLOSED || g_cf.count == 0)
        return NULL;

    /* Walk back to the first of the last REPLAY_MAX_ROWS rows that pass the filter. */
    uint32_t first = g_cf.count, kept = 0;
    uint32_t oldest = (g_cf.count > KEEP_RAW) ? g_cf.count - KEEP_RAW + 1 : 1;
    for (uint32_t fn = g_cf.count; fn >= oldest; fn--) {
        frame_data *fdata = frame_data_sequence_find(g_cf.provider.frames, fn);
        if (fdata && (!g_live_dfcode || fdata->passed_dfilter)) {
            first = fn;
            if (++kept >= REPLAY_MAX_ROWS)
                break;
        }
        if (fn == 1)
            break;
    }

    GString *gs = g_string_new("{\"type\":\"packet_list\",\"cols\":[");
    for (int i = 0; i < g_cf.cinfo.num_cols; i++) {
        char *jt = json_escape(g_cf.cinfo.columns[i].col_title);
        if (i) g_string_append_c(gs, ',');
        g_string_append(gs, jt);
        g_free(jt);
    }
    g_string_append(gs, "],\"packets\":[");

    uint32_t count = 0;
    for (uint32_t fn = first; fn <= g_cf.count; fn++) {
        frame_data *fdata = frame_data_sequence_find(g_cf.provider.frames, fn);
        if (!fdata) continue;
        if (g_live_dfcode && !fdata->passed_dfilter) continue;
        GByteArray *row_ba = g_live_pkts
            ? g_hash_table_lookup(g_live_pkts, GUINT_TO_POINTER(fn))
            : NULL;
        if (!row_ba)
            continue;
        char *row = packet_to_json_row(&g_cf, fdata, row_ba);
        if (!row) continue;
        if (count) g_string_append_c(gs, ',');
        g_string_append(gs, row);
        g_free(row);
        count++;
    }

    /* total is what the server holds, never the rows of this message. */
    char *jf = json_escape(g_live_filter_expr ? g_live_filter_expr : "");
    g_string_append_printf(gs,
        "],\"total\":%u,\"replay\":true,\"shown\":%u,\"from\":%u,\"filter\":%s}",
        g_cf.count, count, first, jf);
    g_free(jf);

    return g_string_free(gs, FALSE);
}

static char *build_replay_json(void)
{
    char *json;
    pthread_mutex_lock(&g_cf_mutex);
    json = build_list_json_locked();
    pthread_mutex_unlock(&g_cf_mutex);
    return json;
}

static void *client_thread(void *arg_v)
{
    client_thread_arg_t *arg = (client_thread_arg_t *)arg_v;
    ws_client_t *c = arg->cli;
    int fd  = c->fd;
    g_free(arg);

    uint8_t rx[RX_BUFSIZE];
    int     rx_len = 0;
    bool    ws_mode = false;

    while (1) {
        int n = recv(fd, rx + rx_len, sizeof(rx) - rx_len - 1, 0);
        if (n <= 0) break;
        rx_len += n;
        rx[rx_len] = '\0';

        if (!ws_mode) {
            /* Check if this is a WebSocket upgrade */
            if (strstr((char *)rx, "Upgrade: websocket") ||
                strstr((char *)rx, "Upgrade: WebSocket")) {
                /* Find Sec-WebSocket-Key */
                const char *key_hdr = strstr((char *)rx, "Sec-WebSocket-Key: ");
                if (!key_hdr) break;
                key_hdr += 19;
                char key[64] = {0};
                const char *eol = strchr(key_hdr, '\r');
                if (!eol) eol = strchr(key_hdr, '\n');
                if (!eol) break;
                int klen = (int)(eol - key_hdr);
                if (klen >= (int)sizeof(key)) break;
                memcpy(key, key_hdr, klen);
                /* Trim */
                while (klen > 0 && (key[klen-1]==' '||key[klen-1]=='\r'||key[klen-1]=='\n'))
                    key[--klen] = '\0';

                char accept[64];
                ws_accept_key(key, accept);

                char resp[512];
                g_snprintf(resp, sizeof(resp),
                    "HTTP/1.1 101 Switching Protocols\r\n"
                    "Upgrade: websocket\r\n"
                    "Connection: Upgrade\r\n"
                    "Sec-WebSocket-Accept: %s\r\n\r\n",
                    accept);
                send(fd, resp, strlen(resp), MSG_NOSIGNAL);

                /* History built, queued and client made visible in one go: no live batch may overtake the replay. */
                char *hist = build_replay_json();
                pthread_mutex_lock(&g_clients_mutex);
                if (hist)
                    ws_enqueue(c, hist, strlen(hist), 0, false);
                if (g_link_known) {
                    char link[64];
                    g_snprintf(link, sizeof(link),
                               "{\"type\":\"link\",\"linktype\":%u}",
                               g_pcap_network);
                    ws_enqueue(c, link, strlen(link), 0, false);
                }
                c->handshake_done = true;
                pthread_mutex_unlock(&g_clients_mutex);
                g_free(hist);

                ws_mode = true;
                rx_len = 0;
                continue;
            } else if (strstr((char *)rx, "\r\n\r\n") ||
                       strstr((char *)rx, "\n\n")) {
                /* Plain HTTP request */
                handle_http(fd, (char *)rx);
                break;
            }
            /* Keep buffering for more of the HTTP request */
            if (rx_len >= (int)sizeof(rx) - 1) break;
            continue;
        }

        /* WebSocket frame processing */
        int consumed = 0;
        while (consumed < rx_len) {
            uint8_t *frame = rx + consumed;
            int remaining  = rx_len - consumed;

            if (remaining < 2) break;

            uint8_t opcode = frame[0] & 0x0F;

            /* Connection close */
            if (opcode == 0x08) goto disconnect;
            /* Ping → Pong */
            if (opcode == 0x09) {
                uint8_t pong[2] = {0x8A, 0x00};
                send(fd, pong, 2, MSG_NOSIGNAL);
                consumed += 2;
                continue;
            }

            char text[RX_BUFSIZE];
            int frame_len = ws_decode_frame(frame, remaining, text, sizeof(text));
            if (frame_len < 0) break; /* incomplete frame, wait for more */

            consumed += frame_len;

            if (opcode == 0x01 || opcode == 0x02) { /* text or binary */
                dispatch_command(fd, text);
            }
        }

        /* Shift remaining data */
        if (consumed > 0 && consumed < rx_len) {
            memmove(rx, rx + consumed, rx_len - consumed);
            rx_len -= consumed;
        } else if (consumed >= rx_len) {
            rx_len = 0;
        }
    }

disconnect:
    /* Out of the broadcast first, so nothing can reach it while it is being
       taken apart, then the tx thread is stopped and joined, then freed. */
    pthread_mutex_lock(&g_clients_mutex);
    for (int i = 0; i < g_client_count; i++) {
        if (g_clients[i] == c) {
            g_clients[i] = g_clients[--g_client_count];
            break;
        }
    }
    pthread_mutex_unlock(&g_clients_mutex);

    pthread_mutex_lock(&c->tx_lock);
    c->tx_stop = true;
    pthread_cond_signal(&c->tx_cond);
    pthread_mutex_unlock(&c->tx_lock);
    pthread_join(c->tx_tid, NULL);

    close(c->fd);
    g_byte_array_free(c->tx, TRUE);
    pthread_mutex_destroy(&c->tx_lock);
    pthread_cond_destroy(&c->tx_cond);
    g_free(c);
    return NULL;
}

/* epan plumbing */

/* Report routines — all provided by ui/alert_box.c */
static const struct report_message_routines g_report_routines = {
    vfailure_alert_box,
    vwarning_alert_box,
    open_failure_alert_box,
    read_failure_alert_box,
    write_failure_alert_box,
    cfile_open_failure_alert_box,
    cfile_dump_open_failure_alert_box,
    cfile_read_failure_alert_box,
    cfile_write_failure_alert_box,
    cfile_close_failure_alert_box,
};

static void ws_register_cb(register_action_e action, const char *message, void *u)
{
    (void)action; (void)message; (void)u;
}

/* main */

static void sighandler(int sig)
{
    (void)sig;
    if (g_server_fd >= 0) close(g_server_fd);
    if (g_sock_path) unlink(g_sock_path);
    exit(0);
}

#include <execinfo.h>

static void sigsegv_handler(int sig)
{
    void *bt[32];
    int n = backtrace(bt, 32);
    fprintf(stderr, "\n=== SIGSEGV (signal %d) ===\n", sig);
    backtrace_symbols_fd(bt, n, STDERR_FILENO);
    fprintf(stderr, "===========================\n");
    fflush(stderr);
    _exit(1);
}

static void print_usage(const char *prog)
{
    fprintf(stderr,
        "Usage: %s -f SOCK_PATH SNF_PATH\n"
        "\n"
        "  -f SOCK_PATH  Unix socket to create (cloonix_gui_server connects here)\n"
        "  -d DUMPCAP    Path to cloonix-dumpcap (default: %s)\n"
        "  SNF_PATH      Cloonix snf socket for live capture\n",
        prog, g_dumpcap_bin);
}

int main(int argc, char *argv[])
{
    const char *sock_path = NULL;

    /* Initialise the capture_file */
    cap_file_init(&g_cf);
    resolve_dumpcap_path();

    /* Parse args */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-f") == 0 && i+1 < argc) {
            sock_path = argv[++i];
        } else if (strcmp(argv[i], "-d") == 0 && i+1 < argc) {
            g_dumpcap_bin = argv[++i];
        } else if (strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (argv[i][0] != '-') {
            g_start_file = argv[i];
        }
    }

    if (!sock_path) {
        fprintf(stderr, "Error: -f SOCK_PATH is required\n");
        print_usage(argv[0]);
        return 1;
    }
    g_sock_path = sock_path;

    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT,  sighandler);
    signal(SIGTERM, sighandler);
    signal(SIGSEGV, sigsegv_handler);

    /* Init wireshark core */
    init_process_policies();
    openlog("webshark", LOG_PID | LOG_NDELAY, LOG_USER);
    relinquish_special_privs_perm();
    init_report_message("webshark", &g_report_routines);

    timestamp_set_type(TS_RELATIVE);
    timestamp_set_precision(TS_PREC_AUTO);
    timestamp_set_seconds_type(TS_SECONDS_DEFAULT);

    wtap_init(true);

    if (!epan_init(ws_register_cb, NULL, true)) {
        fprintf(stderr, "epan_init failed\n");
        return 1;
    }

    /* Load prefs (sets up dissector defaults) */
    epan_load_settings();
    prefs_apply_all();
    build_column_format(&g_cf.cinfo);
    g_epan_ready = true;

    static const struct packet_provider_funcs provider_funcs = {
        cap_file_provider_get_frame_ts,
        cap_file_provider_get_interface_name,
        cap_file_provider_get_interface_description,
        cap_file_provider_get_modified_block
    };

    g_cf.epan = epan_new(&g_cf.provider, &provider_funcs);

    /* Start live capture */
    if (g_start_file) {
        g_cf.provider.frames = new_frame_data_sequence();
        g_cf.state = FILE_READ_IN_PROGRESS;
        fprintf(stdout, "webshark: starting live capture from %s\n", g_start_file);
        fflush(stdout);
        start_live_capture(g_start_file);
    }

    /* Unix domain socket server */
    g_server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (g_server_fd < 0) { perror("socket"); return 1; }

    unlink(sock_path);

    struct sockaddr_un addr_un = {0};
    addr_un.sun_family = AF_UNIX;
    if (strlen(sock_path) >= sizeof(addr_un.sun_path)) {
        fprintf(stderr, "Socket path too long: %s\n", sock_path);
        return 1;
    }
    strncpy(addr_un.sun_path, sock_path, sizeof(addr_un.sun_path) - 1);

    if (bind(g_server_fd, (struct sockaddr *)&addr_un, sizeof(addr_un)) < 0) {
        perror("bind"); return 1;
    }
    if (listen(g_server_fd, 8) < 0) {
        perror("listen"); return 1;
    }

    fprintf(stdout, "webshark listening on unix:%s [build %s %s]\n",
            sock_path, __DATE__, __TIME__);
    fflush(stdout);

    /* Accept loop */
    while (1) {
        int cfd = accept(g_server_fd, NULL, NULL);
        if (cfd < 0) {
            if (errno == EINTR) continue;
            break;
        }

        pthread_mutex_lock(&g_clients_mutex);
        if (g_client_count >= MAX_CLIENTS) {
            pthread_mutex_unlock(&g_clients_mutex);
            const char *busy = "HTTP/1.1 503 Service Unavailable\r\n"
                               "Content-Length: 0\r\n\r\n";
            send(cfd, busy, strlen(busy), MSG_NOSIGNAL);
            close(cfd);
            continue;
        }

        /* A send blocking longer than this returns to the tx thread, which keeps the bytes. */
        struct timeval tv = { .tv_sec = 2, .tv_usec = 0 };
        setsockopt(cfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

        ws_client_t *c = g_new0(ws_client_t, 1);
        c->fd = cfd;
        c->tx = g_byte_array_new();
        pthread_mutex_init(&c->tx_lock, NULL);
        pthread_cond_init(&c->tx_cond, NULL);
        g_clients[g_client_count++] = c;
        pthread_mutex_unlock(&g_clients_mutex);

        if (pthread_create(&c->tx_tid, NULL, tx_thread, c)) {
            /* Without it nothing would ever leave, and the join at teardown
               would be given a thread id that was never a thread. */
            fprintf(stderr, "ERROR no tx thread for client %d, dropping it\n",
                    cfd);
            pthread_mutex_lock(&g_clients_mutex);
            for (int i = 0; i < g_client_count; i++)
                if (g_clients[i] == c) {
                    g_clients[i] = g_clients[--g_client_count];
                    break;
                }
            pthread_mutex_unlock(&g_clients_mutex);
            close(cfd);
            g_byte_array_free(c->tx, TRUE);
            pthread_mutex_destroy(&c->tx_lock);
            pthread_cond_destroy(&c->tx_cond);
            g_free(c);
            continue;
        }

        client_thread_arg_t *arg = g_new(client_thread_arg_t, 1);
        arg->cli = c;

        pthread_t tid;
        pthread_create(&tid, NULL, client_thread, arg);
        pthread_detach(tid);
    }

    epan_free(g_cf.epan);
    epan_cleanup();
    wtap_cleanup();
    return 0;
}
