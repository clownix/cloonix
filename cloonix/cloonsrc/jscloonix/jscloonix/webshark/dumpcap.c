/* dumpcap.c
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 * Cloonix modified
 */

#include <config.h>
#define WS_LOG_DOMAIN LOG_DOMAIN_CAPCHILD

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <glib.h>
#include <string.h>
#include <ws_exit_codes.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <wsutil/ws_getopt.h>
#include <signal.h>
#include <errno.h>
#include <wsutil/array.h>
#include <wsutil/cmdarg_err.h>
#include <wsutil/strtoi.h>
#include <wsutil/version_info.h>
#include <wsutil/socket.h>
#include <wsutil/file_util.h>
#include "ringbuffer.h"
#include "writecap/pcapio.h"
#include <sys/un.h>
#include <wsutil/clopts_common.h>
#include <wsutil/privileges.h>
#include <pcap/pcap.h>
#include "sync_pipe.h"
#include "capture_opts.h"
#include "cloonix_fanout.h"
#include <capture/capture_session.h>
#include <capture/capture_sync.h>
#include "wsutil/tempfile.h"
#include "wsutil/file_util.h"
#include "wsutil/cpu_info.h"
#include "wsutil/os_version_info.h"
#include "wsutil/str_util.h"
#include "wsutil/inet_addr.h"
#include "wsutil/time_util.h"
#include "wsutil/glib-compat.h"
#include <wsutil/ws_assert.h>


#include "wiretap/libpcap.h"
#include "wiretap/pcapng_module.h"
#include "wiretap/pcapng.h"

#define DEF_TCP_PORT 19000
#define MSG_MAX_LENGTH 4096
#define CAP_READ_TIMEOUT        250
#define PIPE_READ_TIMEOUT   250000
#define WRITER_THREAD_TIMEOUT 100000
#define LINKTYPE_ATM_RFC1483    100
#define LINKTYPE_RAW            101
#define LINKTYPE_SLIP_BSDOS     102
#define LINKTYPE_PPP_BSDOS      103
#define LINKTYPE_C_HDLC         104
#define LINKTYPE_IEEE802_11     105
#define LINKTYPE_ATM_CLIP       106
#define LINKTYPE_FRELAY         107
#define LINKTYPE_LOOP           108
#define LINKTYPE_ENC            109
#define LINKTYPE_NETBSD_HDLC    112
#define LINKTYPE_PFSYNC         246
#define LINKTYPE_PKTAP          258



char *get_cloonix_pipe_name(void);
char *get_cloonix_pipe_title(void);

/* Cloonix fan-out. The socket name is fixed and derived from the snf given
 * with -i, so nothing has to agree on a path over a command line: the switch,
 * the gui server and webshark each build it from the one they already have.
 * See cloonix_fanout_names(). */
static const char *cloonix_snf = NULL;

static GAsyncQueue *pcap_queue;
static int64_t pcap_queue_bytes;
static int64_t pcap_queue_packets;
static int64_t pcap_queue_byte_limit;
static int64_t pcap_queue_packet_limit;

static const char *report_capture_filename;
static int sync_pipe_fd = 2;

static void capture_loop_stop(void);
static void cap_pipe_close(int pipe_fd, bool from_socket);


typedef enum {
    CAP_DEVICE_OPEN_NO_ERR,
    CAP_DEVICE_OPEN_ERROR_NO_SUCH_DEVICE,
    CAP_DEVICE_OPEN_ERROR_RFMON_NOTSUP,
    CAP_DEVICE_OPEN_ERROR_PERM_DENIED,
    CAP_DEVICE_OPEN_ERROR_IFACE_NOT_UP,
    CAP_DEVICE_OPEN_ERROR_PROMISC_PERM_DENIED,
    CAP_DEVICE_OPEN_ERROR_OTHER,
    CAP_DEVICE_OPEN_ERROR_GENERIC,
    CAP_DEVICE_OPEN_WARNING_PROMISC_NOTSUP,
    CAP_DEVICE_OPEN_WARNING_TSTAMP_TYPE_NOTSUP,
    CAP_DEVICE_OPEN_WARNING_OTHER
} cap_device_open_status;



typedef enum {
    INITFILTER_NO_ERROR,
    INITFILTER_BAD_FILTER,
    INITFILTER_OTHER_ERROR
} initfilter_status_t;

typedef enum {
    STATE_EXPECT_REC_HDR,
    STATE_READ_REC_HDR,
    STATE_EXPECT_DATA,
    STATE_READ_DATA
} cap_pipe_state_t;

typedef enum {
    PIPOK,
    PIPEOF,
    PIPERR,
    PIPNEXIST
} cap_pipe_err_t;

typedef struct _pcap_pipe_info {
    bool                         byte_swapped;
    struct pcap_hdr              hdr;
    struct pcaprec_modified_hdr  rechdr;
} pcap_pipe_info_t;

typedef struct _pcapng_pipe_info {
    pcapng_block_header_t bh;
    GArray *src_iface_to_global;
} pcapng_pipe_info_t;

struct _loop_data;

typedef struct _capture_src {
    uint32_t                     received;
    uint32_t                     dropped;
    uint32_t                     flushed;
    int                          pcap_fd;
    bool                         pcap_err;
    unsigned                     interface_id;
    unsigned                     idb_id;
    GThread                     *tid;
    int                          snaplen;
    int                          linktype;
    bool                         ts_nsec;
    bool                         from_cap_pipe;
    bool                         from_cap_socket;
    bool                         from_pcapng;
    union {
        pcap_pipe_info_t         pcap;
        pcapng_pipe_info_t       pcapng;
    } cap_pipe_info;
    int                          cap_pipe_fd;
    bool                         cap_pipe_modified;
    char *                       cap_pipe_databuf;
    size_t                       cap_pipe_databuf_size;
    unsigned                     cap_pipe_max_pkt_size;
    size_t                       cap_pipe_bytes_to_read;
    size_t                       cap_pipe_bytes_read;
    int (*cap_pipe_dispatch)(struct _loop_data *, struct _capture_src *, char *, size_t);
    cap_pipe_state_t cap_pipe_state;
    cap_pipe_err_t cap_pipe_err;
} capture_src;

typedef struct _saved_idb {
    bool deleted;
    unsigned interface_id;
    uint8_t *idb;
    unsigned idb_len;
} saved_idb_t;

typedef struct _loop_data {
    bool      go;
    int       err;
    int       packets_captured;
    unsigned  inpkts_to_sync_pipe;
    GArray   *pcaps;
    bool      pcapng_passthrough;
    uint8_t  *saved_shb;
    GArray   *saved_idbs;
    GRWLock   saved_shb_idb_lock;
    FILE     *pdh;
    int       save_file_fd;
    char     *io_buffer;
    uint64_t  bytes_written;      
    int       packets_written;    
    int       file_count;
    GTimer  *file_duration_timer;
    time_t   next_interval_time;
    int      interval_s;
} loop_data;

typedef struct _pcap_queue_element {
    capture_src        *pcap_src;
    union {
        struct pcap_pkthdr  phdr;
        pcapng_block_header_t  bh;
    } u;
    uint8_t             *pd;
} pcap_queue_element;

static loop_data   global_ld;


static capture_options global_capture_opts;
static bool quiet;
static bool use_threads;
static uint64_t start_time;
static void capture_loop_write_packet_cb(uint8_t *pcap_src_p, const struct pcap_pkthdr *phdr, const uint8_t *pd);
static void capture_loop_queue_packet_cb(uint8_t *pcap_src_p, const struct pcap_pkthdr *phdr, const uint8_t *pd);
static void capture_loop_write_pcapng_cb(capture_src *pcap_src, const pcapng_block_header_t *bh, uint8_t *pd);
static void capture_loop_queue_pcapng_cb(capture_src *pcap_src, const pcapng_block_header_t *bh, uint8_t *pd);
static void capture_loop_get_errmsg(char *errmsg, size_t errmsglen,
                                    char *secondary_errmsg,
                                    size_t secondary_errmsglen,
                                    const char *fname, int err,
                                    bool is_close);
static void exit_main(int err);

static void report_new_capture_file(const char *filename);
static void report_packet_count(unsigned int packet_count);



static void dumpcap_cmdarg_err(const char *fmt, va_list ap)
{
        char *msg;
        msg = ws_strdup_vprintf(fmt, ap);
        sync_pipe_write_errmsgs_to_parent(sync_pipe_fd, msg, "");
        g_free(msg);
}

static void dumpcap_cmdarg_err_cont(const char *fmt, va_list ap)
{
        char *msg;
        msg = ws_strdup_vprintf(fmt, ap);
        sync_pipe_write_errmsgs_to_parent(sync_pipe_fd, msg, "");
        g_free(msg);
}



/* SIGTERM or SIGINT: the stop asked, a select it interrupts is no error */
static volatile sig_atomic_t g_stop_asked = 0;

static void capture_cleanup_handler(int signum )
{
    g_stop_asked = 1;
    capture_loop_stop();
}


static void report_capture_count(void)
{
}


static void exit_main(int status)
{
    if (ringbuf_is_initialized()) {
        ringbuf_free();
        global_capture_opts.save_file = NULL;
    }
    exit(status);
}

static int dlt_to_linktype(int dlt)
{
	if (dlt >= DLT_NULL && dlt <= DLT_FDDI)
		return (dlt);
	if (dlt >= 104)
		return (dlt);
	return (dlt);
}

static void cap_pipe_adjust_pcap_header(bool byte_swapped, struct pcap_hdr *hdr, struct pcaprec_hdr *rechdr)
{
    if (byte_swapped) {
        rechdr->ts_sec = GUINT32_SWAP_LE_BE(rechdr->ts_sec);
        rechdr->ts_usec = GUINT32_SWAP_LE_BE(rechdr->ts_usec);
        rechdr->incl_len = GUINT32_SWAP_LE_BE(rechdr->incl_len);
        rechdr->orig_len = GUINT32_SWAP_LE_BE(rechdr->orig_len);
    }
    if (hdr->version_major == 2 &&
        (hdr->version_minor < 3 ||
         (hdr->version_minor == 3 && rechdr->incl_len > rechdr->orig_len))) {
        uint32_t temp;
        temp = rechdr->orig_len;
        rechdr->orig_len = rechdr->incl_len;
        rechdr->incl_len = temp;
    }
}

static ssize_t cap_pipe_read(int pipe_fd, char *buf, size_t sz, bool from_socket )
{
    return ws_read(pipe_fd, buf, sz);
}

/* select said readable and there is nothing yet: the snf closed its end
   and opened it again for this reader, its header comes next. */
static bool read_again(ssize_t b)
{
    return (b < 0) &&
           ((errno == EAGAIN) || (errno == EWOULDBLOCK) || (errno == EINTR));
}

static int cap_pipe_select(int pipe_fd)
{
    fd_set      rfds;
    struct timeval timeout;
    FD_ZERO(&rfds);
    FD_SET(pipe_fd, &rfds);
    timeout.tv_sec = PIPE_READ_TIMEOUT / 1000000;
    timeout.tv_usec = PIPE_READ_TIMEOUT % 1000000;
    return select(pipe_fd+1, &rfds, NULL, NULL, &timeout);
}


static void cap_pipe_close(int pipe_fd, bool from_socket)
{
    (void) from_socket;
    ws_close(pipe_fd);
}

static ssize_t cap_pipe_read_data_bytes(capture_src *pcap_src, char *errmsg, size_t errmsgl)
{
    int sel_ret;
    int fd = pcap_src->cap_pipe_fd;
    ssize_t sz, bytes_read = 0;
    ssize_t b;

    sz = pcap_src->cap_pipe_bytes_to_read - pcap_src->cap_pipe_bytes_read;
    while (bytes_read < sz) {
        if (fd == -1) {
            snprintf(errmsg, errmsgl, "Invalid file descriptor.");
            pcap_src->cap_pipe_err = PIPNEXIST;
            return -1;
        }

        sel_ret = cap_pipe_select(fd);
        if (sel_ret < 0) {
            snprintf(errmsg, errmsgl,
                       "Unexpected error from select: %s.", g_strerror(errno));
            pcap_src->cap_pipe_err = PIPERR;
            return -1;
        } else if (sel_ret > 0) {
            b = cap_pipe_read(fd, pcap_src->cap_pipe_databuf+pcap_src->cap_pipe_bytes_read+bytes_read,
                              sz-bytes_read, pcap_src->from_cap_socket);
            if (read_again(b))
                continue;
            if (b <= 0) {
                if (b == 0) {
                    snprintf(errmsg, errmsgl,
                               "End of file reading from pipe or socket.");
                    pcap_src->cap_pipe_err = PIPEOF;
                } else {
                    snprintf(errmsg, errmsgl,
                               "Error reading from pipe or socket: %s.",
                               g_strerror(errno));
                    pcap_src->cap_pipe_err = PIPERR;
                }
                return -1;
            }
            bytes_read += b;
        }
    }
    pcap_src->cap_pipe_bytes_read += bytes_read;
    return bytes_read;
}

static void pcap_pipe_open_live(int fd, capture_src *pcap_src,
                                struct pcap_hdr *hdr,
                                char *errmsg, size_t errmsgl,
                                char *secondary_errmsg, size_t secondary_errmsgl);

static char not_our_bug[] =
    "Please report this to the developers of the program writing to the pipe.";

static void cap_pipe_open_live(char *cloonix_pipename,
                               capture_src *pcap_src,
                               void *hdr,
                               char *errmsg, size_t errmsgl,
                               char *secondary_errmsg, size_t secondary_errmsgl)
{
    ws_statb64         pipe_stat;
    struct sockaddr_un sa;
    ssize_t  b;
    int      fd = -1, sel_ret;
    size_t   bytes_read;
    uint32_t magic = 0;
    pcap_src->cap_pipe_fd = -1;

    /* Wait up to 15s for the cloonsnf socket to appear */
    {
      int retries = 0;
      while (ws_stat64(cloonix_pipename, &pipe_stat) < 0 && errno == ENOENT && retries < 75) {
        usleep(200000);  /* 200ms */
        retries++;
      }
    }

    if (ws_stat64(cloonix_pipename, &pipe_stat) < 0) {
        if (errno == ENOENT || errno == ENOTDIR)
            pcap_src->cap_pipe_err = PIPNEXIST;
        else {
            snprintf(errmsg, errmsgl,
                       "The capture session could not be initiated "
                       "due to error getting information on pipe or socket: %s.", g_strerror(errno));
            pcap_src->cap_pipe_err = PIPERR;
        ws_debug("ERROR 1 The capture session could not be "
                 "initiated: %s", errmsg);
        }
        return;
    }
    fd = ws_open(cloonix_pipename, O_RDONLY | O_NONBLOCK, 0000);
    if (fd == -1) {
        snprintf(errmsg, errmsgl,
                   "The capture session could not be initiated "
                   "due to error on pipe open: %s.", g_strerror(errno));
        pcap_src->cap_pipe_err = PIPERR;
        ws_debug("ERROR 2 The capture session could not be "
                 "initiated: %s", errmsg);
        return;
    }
    pcap_src->from_cap_pipe = true;
    pcap_src->from_cap_socket = false;
    pcap_src->cap_pipe_databuf = (char*)g_malloc(2048);
    pcap_src->cap_pipe_databuf_size = 2048;
    bytes_read = 0;
    while (bytes_read < sizeof magic) {
        sel_ret = cap_pipe_select(fd);
        if (sel_ret < 0) {
            snprintf(errmsg, errmsgl,
                       "Unexpected error from select: %s.",
                       g_strerror(errno));
            if (!g_stop_asked)
                ws_debug("ERROR 5 The capture session could not be "
                         "initiated: %s", errmsg);
            goto error;
        } else if (sel_ret > 0) {
            b = cap_pipe_read(fd, ((char *)&magic)+bytes_read,
                              sizeof magic-bytes_read,
                              pcap_src->from_cap_socket);
            if (read_again(b))
                continue;
            if (b <= 0) {
                if (b == 0)
                    snprintf(errmsg, errmsgl,
                               "End of file on pipe magic during open.");
                else
                    snprintf(errmsg, errmsgl,
                               "Error on pipe magic during open: %s.",
                               g_strerror(errno));
                ws_debug("ERROR 6 The capture session could not be "
                         "initiated: %s", errmsg);
                goto error;
            }
            bytes_read += b;
        }
    }
    switch (magic) {
    case PCAP_MAGIC:
    case PCAP_NSEC_MAGIC:
        pcap_src->cap_pipe_info.pcap.byte_swapped = false;
        pcap_src->cap_pipe_modified = false;
        pcap_src->ts_nsec = magic == PCAP_NSEC_MAGIC;
        break;
    default:
        snprintf(errmsg, errmsgl, " (magic = 0x%08x)", magic);
        snprintf(secondary_errmsg, secondary_errmsgl, "%s", not_our_bug);
        ws_debug("ERROR 7 The capture session could not be "
                 "initiated: %s", errmsg);
        goto error;
    }
    pcap_pipe_open_live(fd, pcap_src, (struct pcap_hdr *) hdr, errmsg,
                        errmsgl, secondary_errmsg, secondary_errmsgl);

    return;

error:
    pcap_src->cap_pipe_err = PIPERR;
    cap_pipe_close(fd, pcap_src->from_cap_socket);
    pcap_src->cap_pipe_fd = -1;
}

static void pcap_pipe_open_live(int fd,
                                capture_src *pcap_src,
                                struct pcap_hdr *hdr,
                                char *errmsg, size_t errmsgl,
                                char *secondary_errmsg, size_t secondary_errmsgl)
{
    size_t   bytes_read;
    ssize_t  b;
    int      sel_ret;
    bytes_read = 0;
    while (bytes_read < sizeof(struct pcap_hdr)) {
        sel_ret = cap_pipe_select(fd);
        if (sel_ret < 0) {
            snprintf(errmsg, errmsgl,
                       "Unexpected error from select: %s.",
                       g_strerror(errno));
            ws_debug("ERROR 8 The capture session could not be "
                     "initiated: %s", errmsg);
            goto error;
        } else if (sel_ret > 0) {
            b = cap_pipe_read(fd, ((char *)hdr)+bytes_read,
                              sizeof(struct pcap_hdr) - bytes_read,
                              pcap_src->from_cap_socket);
            if (read_again(b))
                continue;
            if (b <= 0) {
                if (b == 0)
                    snprintf(errmsg, errmsgl,
                               "End of file on pipe header during open.");
                else
                    snprintf(errmsg, errmsgl,
                               "Error on pipe header during open: %s.",
                               g_strerror(errno));
                snprintf(secondary_errmsg, secondary_errmsgl,
                           "%s", not_our_bug);
                ws_debug("ERROR 9 The capture session could not be "
                         "initiated: %s", errmsg);
                goto error;
            }
            bytes_read += b;
        }
    }
    if (pcap_src->cap_pipe_info.pcap.byte_swapped) {
        hdr->version_major = GUINT16_SWAP_LE_BE(hdr->version_major);
        hdr->version_minor = GUINT16_SWAP_LE_BE(hdr->version_minor);
        hdr->snaplen = GUINT32_SWAP_LE_BE(hdr->snaplen);
        hdr->network = GUINT32_SWAP_LE_BE(hdr->network);
    }
    pcap_src->linktype = dlt_to_linktype(hdr->network);
    pcap_src->cap_pipe_max_pkt_size = WTAP_MAX_PACKET_SIZE_STANDARD;
    if (hdr->version_major < 2) {
        snprintf(errmsg, errmsgl,
                   "The old pcap format version %d.%d is not supported.",
                   hdr->version_major, hdr->version_minor);
        snprintf(secondary_errmsg, secondary_errmsgl, "%s",
                   not_our_bug);
        ws_debug("ERROR 10 The capture session could not be "
                 "initiated: %s", errmsg);
        goto error;
    }

    pcap_src->cap_pipe_fd = fd;
    return;

error:
    pcap_src->cap_pipe_err = PIPERR;
    cap_pipe_close(fd, pcap_src->from_cap_socket);
    pcap_src->cap_pipe_fd = -1;
}

static int pcapng_read_shb(capture_src *pcap_src,
                           char *errmsg,
                           size_t errmsgl)
{
    pcapng_section_header_block_t shb;

    {
        pcap_src->cap_pipe_bytes_to_read = sizeof(pcapng_block_header_t) + sizeof(pcapng_section_header_block_t);
        if (cap_pipe_read_data_bytes(pcap_src, errmsg, errmsgl) < 0) {
            return -1;
        }
    }
    memcpy(&shb, pcap_src->cap_pipe_databuf + sizeof(pcapng_block_header_t), sizeof(pcapng_section_header_block_t));
    switch (shb.magic)
    {
    case PCAPNG_MAGIC:
        break;
    case PCAPNG_SWAPPED_MAGIC:
        return -1;
    default:
        snprintf(errmsg, errmsgl,
                   "Unrecognized pcapng format or not pcapng data.");
        return -1;
    }

    pcap_src->cap_pipe_max_pkt_size = WTAP_MAX_PACKET_SIZE_STANDARD;
    pcap_src->cap_pipe_state = STATE_EXPECT_DATA;

    return 0;
}

static bool
pcapng_adjust_block(capture_src *pcap_src, const pcapng_block_header_t *bh, uint8_t *pd)
{
    switch(bh->block_type) {
    case BLOCK_TYPE_SHB:
    {
        if (global_ld.pcapng_passthrough) {
            g_free(global_ld.saved_shb);
            global_ld.saved_shb = (uint8_t *) g_memdup2(pd, bh->block_total_length);
            for (unsigned i = 0; i < global_ld.saved_idbs->len; i++) {
                saved_idb_t *idb_source = &g_array_index(global_ld.saved_idbs, saved_idb_t, i);
                g_free(idb_source->idb);
            }
            g_array_set_size(global_ld.saved_idbs, 0);
        } else {
            for (unsigned i = 0; i < pcap_src->cap_pipe_info.pcapng.src_iface_to_global->len; i++) {
                uint32_t iface_id = g_array_index(pcap_src->cap_pipe_info.pcapng.src_iface_to_global, uint32_t, i);
                saved_idb_t *idb_source = &g_array_index(global_ld.saved_idbs, saved_idb_t, iface_id);
                ws_assert(idb_source->interface_id == pcap_src->interface_id);
                g_free(idb_source->idb);
                memset(idb_source, 0, sizeof(saved_idb_t));
                idb_source->deleted = true;
            }
        }
        g_array_set_size(pcap_src->cap_pipe_info.pcapng.src_iface_to_global, 0);
    }
        break;
    case BLOCK_TYPE_IDB:
    {
        saved_idb_t idb_source = { 0 };
        idb_source.interface_id = pcap_src->interface_id;
        idb_source.idb_len = bh->block_total_length;
        idb_source.idb = (uint8_t *) g_memdup2(pd, idb_source.idb_len);
        g_array_append_val(global_ld.saved_idbs, idb_source);
        uint32_t iface_id = global_ld.saved_idbs->len - 1;
        g_array_append_val(pcap_src->cap_pipe_info.pcapng.src_iface_to_global, iface_id);
    }
        break;
    case BLOCK_TYPE_EPB:
    case BLOCK_TYPE_ISB:
    {
        if (global_ld.pcapng_passthrough) {
            break;
        }
        uint32_t iface_id;
        memcpy(&iface_id, pd + sizeof(pcapng_block_header_t), 4);
        if (iface_id < pcap_src->cap_pipe_info.pcapng.src_iface_to_global->len) {
            memcpy(pd + sizeof(pcapng_block_header_t),
                   &g_array_index(pcap_src->cap_pipe_info.pcapng.src_iface_to_global, uint32_t, iface_id), 4);
        } else {
            return false;
        }
    }
        break;
    default:
        break;
    }

    return true;
}

static bool is_data_block(uint32_t block_type)
{
    switch (block_type) {
        case BLOCK_TYPE_PB:
        case BLOCK_TYPE_EPB:
        case BLOCK_TYPE_SPB:
        case BLOCK_TYPE_SYSTEMD_JOURNAL_EXPORT:
        case BLOCK_TYPE_SYSDIG_EVENT:
        case BLOCK_TYPE_SYSDIG_EVENT_V2:
        case BLOCK_TYPE_SYSDIG_EVENT_V2_LARGE:
            return true;
        default:
            break;
    }
    return false;
}


static int pcap_pipe_dispatch(loop_data *ld, capture_src *pcap_src, char *errmsg, size_t errmsgl)
{
    struct pcap_pkthdr  phdr;
    enum { PD_REC_HDR_READ, PD_DATA_READ, PD_PIPE_EOF, PD_PIPE_ERR,
           PD_ERR } result;
    ssize_t   b;
    unsigned new_bufsize;
    pcap_pipe_info_t *pcap_info = &pcap_src->cap_pipe_info.pcap;


    switch (pcap_src->cap_pipe_state) {

    case STATE_EXPECT_REC_HDR:

            pcap_src->cap_pipe_state = STATE_READ_REC_HDR;
            pcap_src->cap_pipe_bytes_to_read = pcap_src->cap_pipe_modified ?
                sizeof(struct pcaprec_modified_hdr) : sizeof(struct pcaprec_hdr);
            pcap_src->cap_pipe_bytes_read = 0;


    case STATE_READ_REC_HDR:
        {
            b = cap_pipe_read(pcap_src->cap_pipe_fd, ((char *)&pcap_info->rechdr)+pcap_src->cap_pipe_bytes_read,
                 pcap_src->cap_pipe_bytes_to_read - pcap_src->cap_pipe_bytes_read, pcap_src->from_cap_socket);
            if (read_again(b))
                return 0;
            if (b <= 0) {
                if (b == 0)
                    result = PD_PIPE_EOF;
                else
                    result = PD_PIPE_ERR;
                break;
            }
            pcap_src->cap_pipe_bytes_read += b;
        }
        if (pcap_src->cap_pipe_bytes_read < pcap_src->cap_pipe_bytes_to_read) {
            return 0;
        }
        result = PD_REC_HDR_READ;
        break;

    case STATE_EXPECT_DATA:
            pcap_src->cap_pipe_state = STATE_READ_DATA;
            pcap_src->cap_pipe_bytes_to_read = pcap_info->rechdr.hdr.incl_len;
            pcap_src->cap_pipe_bytes_read = 0;

    case STATE_READ_DATA:
        {
            b = cap_pipe_read(pcap_src->cap_pipe_fd,
                              pcap_src->cap_pipe_databuf+pcap_src->cap_pipe_bytes_read,
                              pcap_src->cap_pipe_bytes_to_read - pcap_src->cap_pipe_bytes_read,
                              pcap_src->from_cap_socket);
            if (read_again(b))
                return 0;
            if (b <= 0) {
                if (b == 0)
                    result = PD_PIPE_EOF;
                else
                    result = PD_PIPE_ERR;
                break;
            }
            pcap_src->cap_pipe_bytes_read += b;
        }
        if (pcap_src->cap_pipe_bytes_read < pcap_src->cap_pipe_bytes_to_read) {
            return 0;
        }
        result = PD_DATA_READ;
        break;

    default:
        snprintf(errmsg, errmsgl,
                   "pcap_pipe_dispatch: invalid state");
        result = PD_ERR;

    }
    switch (result) {
    case PD_REC_HDR_READ:
        cap_pipe_adjust_pcap_header(pcap_src->cap_pipe_info.pcap.byte_swapped, &pcap_info->hdr,
                               &pcap_info->rechdr.hdr);
        if (pcap_info->rechdr.hdr.incl_len > pcap_src->cap_pipe_max_pkt_size) {
            snprintf(errmsg, errmsgl, "Frame %u too long (%d bytes)",
                       ld->packets_captured+1, pcap_info->rechdr.hdr.incl_len);
            break;
        }
        if (pcap_info->rechdr.hdr.incl_len > pcap_src->cap_pipe_databuf_size) {
            new_bufsize = pcap_info->rechdr.hdr.incl_len;
            new_bufsize--;
            new_bufsize |= new_bufsize >> 1;
            new_bufsize |= new_bufsize >> 2;
            new_bufsize |= new_bufsize >> 4;
            new_bufsize |= new_bufsize >> 8;
            new_bufsize |= new_bufsize >> 16;
            new_bufsize++;
            pcap_src->cap_pipe_databuf = (char*)g_realloc(pcap_src->cap_pipe_databuf, new_bufsize);
            pcap_src->cap_pipe_databuf_size = new_bufsize;
        }

        if (pcap_info->rechdr.hdr.incl_len) {
            pcap_src->cap_pipe_state = STATE_EXPECT_DATA;
            return 0;
        }
    case PD_DATA_READ:
        phdr.ts.tv_sec = pcap_info->rechdr.hdr.ts_sec;
        phdr.ts.tv_usec = pcap_info->rechdr.hdr.ts_usec;
        phdr.caplen = pcap_info->rechdr.hdr.incl_len;
        phdr.len = pcap_info->rechdr.hdr.orig_len;

        if (use_threads) {
            capture_loop_queue_packet_cb((uint8_t *)pcap_src, &phdr, pcap_src->cap_pipe_databuf);
        } else {
            capture_loop_write_packet_cb((uint8_t *)pcap_src, &phdr, pcap_src->cap_pipe_databuf);
        }
        pcap_src->cap_pipe_state = STATE_EXPECT_REC_HDR;
        return 1;

    case PD_PIPE_EOF:
        pcap_src->cap_pipe_err = PIPEOF;
        return -1;

    case PD_PIPE_ERR:
        snprintf(errmsg, errmsgl, "Error reading from pipe: %s",
                   g_strerror(errno));
    case PD_ERR:
        break;
    }
    pcap_src->cap_pipe_err = PIPERR;
    return -1;
}


static bool capture_loop_open_input(capture_options *capture_opts, loop_data *ld,
                                    char *errmsg, size_t errmsg_len,
                                    char *secondary_errmsg, size_t secondary_errmsg_len)
{
    cap_device_open_status open_status;
    char                open_status_str[PCAP_ERRBUF_SIZE];
    char               *sync_msg_str;
    interface_options  *interface_opts;
    capture_src        *pcap_src;
    unsigned            i;
            bool pipe_err = false;

    if ((use_threads == false) &&
        (capture_opts->ifaces->len > 1)) {
        snprintf(errmsg, errmsg_len,
                   "Using threads is required for capturing on multiple interfaces.");
        return false;
    }

    int pcapng_src_count = 0;
    for (i = 0; i < capture_opts->ifaces->len; i++) {
        interface_opts = &g_array_index(capture_opts->ifaces, interface_options, i);
        pcap_src = g_new0(capture_src, 1);
        if (pcap_src == NULL) {
            snprintf(errmsg, errmsg_len,
                   "Could not allocate memory.");
            return false;
        }

        pcap_src->pcap_fd = -1;
        pcap_src->interface_id = i;
        pcap_src->linktype = -1;
        pcap_src->cap_pipe_fd = -1;
        pcap_src->cap_pipe_dispatch = pcap_pipe_dispatch;
        pcap_src->cap_pipe_state = STATE_EXPECT_REC_HDR;
        pcap_src->cap_pipe_err = PIPOK;
        g_array_append_val(ld->pcaps, pcap_src);

        cap_pipe_open_live(get_cloonix_pipe_name(), pcap_src,
                           &pcap_src->cap_pipe_info.pcap.hdr,
                           errmsg, errmsg_len,
                           secondary_errmsg, secondary_errmsg_len);

        if (pcap_src->cap_pipe_fd == -1) {
            pipe_err = true;
        }
        if (pipe_err) {
            /* errmsg is what cap_pipe_open_live filled in, empty when the
               fifo is not there at all. */
            if (!g_stop_asked)
                ws_info("KO the capture pipe %s could not be opened: %s\n",
                        get_cloonix_pipe_name(),
                        errmsg[0] ? errmsg :
                        (access(get_cloonix_pipe_name(), F_OK) ?
                         "it does not exist (no snf on this interface)" :
                         "no reason given"));
        } else {
            open_status = CAP_DEVICE_OPEN_NO_ERR;
        }

        if (open_status != CAP_DEVICE_OPEN_NO_ERR) {
            sync_msg_str = ws_strdup_printf("%s.", open_status_str);
            g_free(sync_msg_str);
        }
        if (pcap_src->from_pcapng) {
            pcapng_src_count++;
        } else {
            saved_idb_t idb_source = { 0 };
            idb_source.interface_id = i;
            g_rw_lock_writer_lock (&ld->saved_shb_idb_lock);
            pcap_src->idb_id = global_ld.saved_idbs->len;
            g_array_append_val(global_ld.saved_idbs, idb_source);
            g_rw_lock_writer_unlock (&ld->saved_shb_idb_lock);
        }
    }

    if (capture_opts->ifaces->len == 1 && pcapng_src_count == 1) {
        ld->pcapng_passthrough = true;
        g_rw_lock_writer_lock (&ld->saved_shb_idb_lock);
        ws_assert(global_ld.saved_idbs->len == 0);
        g_rw_lock_writer_unlock (&ld->saved_shb_idb_lock);
    }
    relinquish_special_privs_perm();
    return true;
}

static void capture_loop_close_input(loop_data *ld)
{
    unsigned     i;
    capture_src *pcap_src;
    for (i = 0; i < ld->pcaps->len; i++) {
        pcap_src = g_array_index(ld->pcaps, capture_src *, i);
        if (pcap_src->from_cap_pipe) {
            if (pcap_src->cap_pipe_fd >= 0) {
                cap_pipe_close(pcap_src->cap_pipe_fd, pcap_src->from_cap_socket);
                pcap_src->cap_pipe_fd = -1;
            }
            if (pcap_src->cap_pipe_databuf != NULL) {
                g_free(pcap_src->cap_pipe_databuf);
                pcap_src->cap_pipe_databuf = NULL;
            }
            if (pcap_src->from_pcapng) {
                g_array_free(pcap_src->cap_pipe_info.pcapng.src_iface_to_global, TRUE);
                pcap_src->cap_pipe_info.pcapng.src_iface_to_global = NULL;
            }
        }
    }

    ld->go = false;
}

static bool
capture_loop_init_pcapng_output(capture_options *capture_opts, loop_data *ld,
                                int *err)
{
    g_rw_lock_reader_lock (&ld->saved_shb_idb_lock);

    if (ld->pcapng_passthrough && !ld->saved_shb) {
        g_rw_lock_reader_unlock (&ld->saved_shb_idb_lock);
        return true;
    }

    bool successful = true;
    GString *os_info_str = g_string_new("");

    *err = 0;
    get_os_version_info(os_info_str);

    if (ld->saved_shb) {
        pcapng_block_header_t bh;
        memcpy(&bh, ld->saved_shb, sizeof(pcapng_block_header_t));
        successful = pcapng_write_block(ld->pdh, ld->saved_shb, bh.block_total_length, &ld->bytes_written, err);
    } else {
        GString *cpu_info_str = g_string_new("");
        get_cpu_info(cpu_info_str);
        successful = pcapng_write_section_header_block(ld->pdh,
                                                       cpu_info_str->str,
                                                       os_info_str->str,
                                                       get_appname_and_version(),
                                                       -1,
                                                       &ld->bytes_written,
                                                       err);
        g_string_free(cpu_info_str, TRUE);
    }
    for (unsigned i = 0; successful && (i < ld->saved_idbs->len); i++) {
        saved_idb_t idb_source = g_array_index(ld->saved_idbs, saved_idb_t, i);
        if (idb_source.deleted) {
            successful = pcapng_write_interface_description_block(global_ld.pdh,
                                                                  "Interface went out of scope",
                                                                  "dummy",
                                                                  "Dumpcap dummy interface",
                                                                  NULL,
                                                                  os_info_str->str,
                                                                  NULL,
                                                                  -1,
                                                                  0,
                                                                  &(global_ld.bytes_written),
                                                                  0,
                                                                  6,
                                                                  &global_ld.err);
        } else if (idb_source.idb && idb_source.idb_len) {
            successful = pcapng_write_block(global_ld.pdh, idb_source.idb, idb_source.idb_len, &ld->bytes_written, err);
        } else if (idb_source.interface_id < capture_opts->ifaces->len) {
            unsigned if_id = idb_source.interface_id;
            interface_options *interface_opts = &g_array_index(capture_opts->ifaces, interface_options, if_id);
            capture_src *pcap_src = g_array_index(ld->pcaps, capture_src *, if_id);
            if (pcap_src->from_cap_pipe) {
                pcap_src->snaplen = pcap_src->cap_pipe_info.pcap.hdr.snaplen;
            }
            successful = pcapng_write_interface_description_block(global_ld.pdh,
                                                                   NULL,
                                                                   get_cloonix_pipe_name(),
                                                                  interface_opts->descr,
                                                                  interface_opts->cfilter,
                                                                  os_info_str->str,
                                                                  interface_opts->hardware,
                                                                  pcap_src->linktype,
                                                                  pcap_src->snaplen,
                                                                  &(global_ld.bytes_written),
                                                                  0,
                                                                  pcap_src->ts_nsec ? 9 : 6,
                                                                  &global_ld.err);
        }
    }
    g_rw_lock_reader_unlock (&ld->saved_shb_idb_lock);
    g_string_free(os_info_str, TRUE);
    return successful;
}

static bool capture_loop_init_output(capture_options *capture_opts, loop_data *ld, char *errmsg, int errmsg_len)
{
    int err = 0;
    if ((capture_opts->use_pcapng == false) &&
        (capture_opts->ifaces->len > 1)) {
        snprintf(errmsg, errmsg_len,
                   "Using PCAPNG is required for capturing on multiple interfaces. Use the -n option.");
        return false;
    }
        ld->pdh = ws_fdopen(ld->save_file_fd, "wb");
        if (ld->pdh == NULL) {
            err = errno;
        } else {
            size_t buffsize = IO_BUF_SIZE;
            ws_statb64 statb;

            if (ws_fstat64(ld->save_file_fd, &statb) == 0) {
                if (statb.st_blksize > IO_BUF_SIZE) {
                    buffsize = statb.st_blksize;
                }
            }
            ld->io_buffer = (char *)g_malloc(buffsize);
            if (capture_opts->output_to_pipe)
                setvbuf(ld->pdh, NULL, _IONBF, 0);
            else
                setvbuf(ld->pdh, ld->io_buffer, _IOFBF, buffsize);
        }
    if (ld->pdh) {
        bool successful;
        if (capture_opts->use_pcapng) {
            successful = capture_loop_init_pcapng_output(capture_opts, ld, &err);
        } else {
            capture_src *pcap_src;
            pcap_src = g_array_index(ld->pcaps, capture_src *, 0);
            if (pcap_src->from_cap_pipe) {
                pcap_src->snaplen = pcap_src->cap_pipe_info.pcap.hdr.snaplen;
            }
            successful = libpcap_write_file_header(ld->pdh, pcap_src->linktype, pcap_src->snaplen,
                                                pcap_src->ts_nsec, &ld->bytes_written, &err);
        }
        if (!successful) {
            fclose(ld->pdh);
            ld->pdh = NULL;
            g_free(ld->io_buffer);
            ld->io_buffer = NULL;
        }
    }

    if (ld->pdh == NULL) {
        if (err < 0) {
            snprintf(errmsg, errmsg_len,
                       "The file to which the capture would be"
                       " saved (\"%s\") could not be opened: Error %d.",
                       capture_opts->save_file, err);
        } else {
            snprintf(errmsg, errmsg_len,
                       "The file to which the capture would be"
                       " saved (\"%s\") could not be opened: %s.",
                       capture_opts->save_file, g_strerror(err));
        }
        return false;
    }

    return true;
}

static bool capture_loop_close_output(capture_options *capture_opts, loop_data *ld, int *err_close)
{
    unsigned int i;
    capture_src *pcap_src;
    uint64_t     end_time = create_timestamp();
    bool success;
        if (capture_opts->use_pcapng) {
            for (i = 0; i < global_ld.pcaps->len; i++) {
                pcap_src = g_array_index(global_ld.pcaps, capture_src *, i);
            }
        }
        if (fclose(ld->pdh) == EOF) {
            if (err_close != NULL) {
                *err_close = errno;
            }
            success = false;
        } else {
            success = true;
        }
        g_free(ld->io_buffer);
        ld->io_buffer = NULL;
        return success;
}

static int capture_loop_dispatch(loop_data *ld,
                      char *errmsg, int errmsg_len, capture_src *pcap_src)
{
    int    inpkts = 0;
    int    packet_count_before;
    int    sel_ret;

    packet_count_before = ld->packets_captured;
    if (pcap_src->from_cap_pipe) {
            sel_ret = cap_pipe_select(pcap_src->cap_pipe_fd);
            if (sel_ret <= 0) {
                if (sel_ret < 0 && errno != EINTR) {
                    snprintf(errmsg, errmsg_len,
                            "Unexpected error from select: %s", g_strerror(errno));
                    ld->go = false;
                }
            }
        if (sel_ret > 0) {
            inpkts = pcap_src->cap_pipe_dispatch(ld, pcap_src, errmsg, errmsg_len);
            if (inpkts < 0) {
                ws_assert(pcap_src->cap_pipe_err != PIPOK);
            }
        }
    }


    return ld->packets_captured - packet_count_before;
}


static bool
capture_loop_open_output(capture_options *capture_opts, int *save_file_fd,
                         char *errmsg, int errmsg_len)
{
    char     *capfile_name = NULL;
    char     *prefix, *suffix;
    bool      is_tempfile;
    GError   *err_tempfile = NULL;


    if (capture_opts->save_file != NULL) {
        capfile_name = g_strdup(capture_opts->save_file);

        if (capture_opts->output_to_pipe == true) { /* either "-" or named pipe */
            if (strcmp(capfile_name, "-") == 0) {
                *save_file_fd = 1;
            } else {
                *save_file_fd = ws_open(capfile_name, O_WRONLY|O_BINARY, 0);
            }
        }
        else {
                *save_file_fd = ws_open(capfile_name, O_WRONLY|O_BINARY|O_TRUNC|O_CREAT,
                                        (capture_opts->group_read_access) ? 0640 : 0600);
        }
        is_tempfile = false;
    } else {
        if (global_capture_opts.ifaces->len > 1) {
            prefix = ws_strdup_printf("wireshark_%d_interfaces", global_capture_opts.ifaces->len);
        } else {
            char *basename;
            const interface_options *interface_opts;

            interface_opts = &g_array_index(global_capture_opts.ifaces, interface_options, 0);
            if (interface_opts->descr) {
                basename = g_path_get_basename(interface_opts->descr);
            } else {
                basename = g_path_get_basename(get_cloonix_pipe_name());
            }
            prefix = g_strconcat("wireshark_", basename, NULL);
            g_free(basename);
        }
        if (capture_opts->use_pcapng) {
            suffix = ".pcapng";
        } else {
            suffix = ".pcap";
        }
        *save_file_fd = create_tempfile(capture_opts->temp_dir, &capfile_name, prefix, suffix, &err_tempfile);
        g_free(prefix);
        is_tempfile = true;
    }
    if (*save_file_fd == -1) {
        if (is_tempfile) {
            snprintf(errmsg, errmsg_len,
                       "The temporary file to which the capture would be saved "
                       "could not be opened: %s.", err_tempfile->message);
            g_error_free(err_tempfile);
        } else {
            snprintf(errmsg, errmsg_len,
                       "The file to which the capture would be saved (\"%s\") "
                       "could not be opened: %s.", capfile_name,
                       g_strerror(errno));
        }
        g_free(capfile_name);
        return false;
    }
    g_free(capture_opts->save_file);
        capture_opts->save_file = capfile_name;

    return true;
}

static time_t get_next_time_interval(int interval_s) {
    time_t next_time = time(NULL);
    next_time -= next_time % interval_s;
    next_time += interval_s;
    return next_time;
}

static bool do_file_switch_or_stop(capture_options *capture_opts)
{
        global_ld.go = false;
        return false;
}

static void * pcap_read_handler(void* arg)
{
    capture_src *pcap_src = (capture_src *)arg;
    char         errmsg[MSG_MAX_LENGTH+1];
    while (global_ld.go && pcap_src->cap_pipe_err == PIPOK) {
        capture_loop_dispatch(&global_ld, errmsg, sizeof(errmsg), pcap_src);
    }
    g_thread_exit(NULL);
    return (NULL);
}

static bool capture_loop_dequeue_packet(void) {
    pcap_queue_element *queue_element;

    g_async_queue_lock(pcap_queue);
    queue_element = (pcap_queue_element *)g_async_queue_timeout_pop_unlocked(pcap_queue, WRITER_THREAD_TIMEOUT);
    if (queue_element) {
        if (queue_element->pcap_src->from_pcapng) {
            pcap_queue_bytes -= queue_element->u.bh.block_total_length;
        } else {
            pcap_queue_bytes -= queue_element->u.phdr.caplen;
        }
        pcap_queue_packets -= 1;
    }
    g_async_queue_unlock(pcap_queue);
    if (queue_element) {
        if (queue_element->pcap_src->from_pcapng) {
            capture_loop_write_pcapng_cb(queue_element->pcap_src,
                                        &queue_element->u.bh,
                                        queue_element->pd);
        } else {
            capture_loop_write_packet_cb((uint8_t *) queue_element->pcap_src,
                                        &queue_element->u.phdr,
                                        queue_element->pd);
        }
        g_free(queue_element->pd);
        g_free(queue_element);
        return true;
    }
    return false;
}

static char * handle_npcap_bug(char *adapter_name , char *cap_err_str )
{
    bool have_npcap = false;

    if (!have_npcap) {
        return g_strdup("");
    }

    return ws_strdup_printf("If you have not removed that adapter, this "
                          "is probably a known issue in Npcap resulting from "
                          "the behavior of the Windows networking stack. "
                          "Work is being done in Npcap to improve the "
                          "handling of this issue; it does not need to "
                          "be reported as a Wireshark or Npcap bug.");
}

static bool capture_loop_start(capture_options *capture_opts)
{
    struct timeval    upd_time, cur_time;
    int               err_close;
    int               inpkts;
    GTimer           *autostop_duration_timer = NULL;
    bool              write_ok;
    bool              close_ok;
    bool              cfilter_error         = false;
    char              errmsg[MSG_MAX_LENGTH+1];
    char              secondary_errmsg[MSG_MAX_LENGTH+1];
    capture_src      *pcap_src;
    interface_options *interface_opts;
    unsigned          i, error_index        = 0;
    *errmsg           = '\0';
    *secondary_errmsg = '\0';
    global_ld.go                  = true;
    global_ld.packets_captured    = 0;
    global_ld.inpkts_to_sync_pipe = 0;
    global_ld.err                 = 0;
    global_ld.pdh                 = NULL;
    global_ld.save_file_fd        = -1;
    global_ld.io_buffer           = NULL;
    global_ld.file_count          = 0;
    global_ld.file_duration_timer = NULL;
    global_ld.next_interval_time  = 0;
    global_ld.interval_s          = 0;
    if (!capture_loop_open_input(capture_opts, &global_ld, errmsg, sizeof(errmsg),
                                 secondary_errmsg, sizeof(secondary_errmsg))) {
        goto error;
    }
    if (capture_opts->saving_to_file) {
        if (!capture_loop_open_output(capture_opts, &global_ld.save_file_fd,
                                      errmsg, sizeof(errmsg))) {
            goto error;
        }
        if (!capture_loop_init_output(capture_opts, &global_ld, errmsg,
                                      sizeof(errmsg))) {
            goto error;
        }
        fflush(global_ld.pdh);
        report_new_capture_file(capture_opts->save_file);
    }
    if (capture_opts->has_file_interval) {
        global_ld.interval_s = capture_opts->file_interval;
        global_ld.next_interval_time = get_next_time_interval(global_ld.interval_s);
    }
    if (capture_opts->has_autostop_filesize) {
        if (capture_opts->autostop_filesize > UINT32_C(2000000000)) {
            capture_opts->autostop_filesize = UINT32_C(2000000000);
        }
    }
    if (capture_opts->has_autostop_duration) {
        autostop_duration_timer = g_timer_new();
    }
    gettimeofday(&upd_time, NULL);
    start_time = create_timestamp();
    if (use_threads) {
        pcap_queue = g_async_queue_new();
        pcap_queue_bytes = 0;
        pcap_queue_packets = 0;
        for (i = 0; i < global_ld.pcaps->len; i++) {
            pcap_src = g_array_index(global_ld.pcaps, capture_src *, i);
            pcap_src->tid = g_thread_new("Capture read", pcap_read_handler, pcap_src);
        }
    }
    while (global_ld.go) {
        if (use_threads) {
            bool dequeued = capture_loop_dequeue_packet();

            if (dequeued) {
                inpkts = 1;
            } else {
                inpkts = 0;
            }
        } else {
            pcap_src = g_array_index(global_ld.pcaps, capture_src *, 0);
            inpkts = capture_loop_dispatch(&global_ld, errmsg,
                                           sizeof(errmsg), pcap_src);
        }
        if (inpkts == 0) {
            bool open_interfaces = false;
            for (i = 0; i < global_ld.pcaps->len; i++) {
                pcap_src = g_array_index(global_ld.pcaps, capture_src *, i);
                if (pcap_src->cap_pipe_err == PIPOK) {
                    open_interfaces = true;
                }
            }
            if (!open_interfaces) {
                global_ld.go = false;
            }
        }
        if (inpkts > 0) {
            if (capture_opts->output_to_pipe) {
                fflush(global_ld.pdh);
            }
        }
        gettimeofday(&cur_time, NULL);
        if (((uint64_t)cur_time.tv_sec * 1000000 + cur_time.tv_usec) >
            ((uint64_t)upd_time.tv_sec * 1000000 + upd_time.tv_usec + capture_opts->update_interval*1000))
        {
            upd_time = cur_time;
            if (global_ld.inpkts_to_sync_pipe) {
                fflush(global_ld.pdh);
                if (!quiet)
                    report_packet_count(global_ld.inpkts_to_sync_pipe);
                global_ld.inpkts_to_sync_pipe = 0;
            }
            if (autostop_duration_timer != NULL && g_timer_elapsed(autostop_duration_timer, NULL) >= capture_opts->autostop_duration) {
                global_ld.go = false;
                continue;
            }
            if (global_ld.file_duration_timer != NULL && g_timer_elapsed(global_ld.file_duration_timer, NULL) >= capture_opts->file_duration) {
                if (!do_file_switch_or_stop(capture_opts))
                    continue;
            }
            if (global_ld.interval_s && time(NULL) >= global_ld.next_interval_time) {
                if (!do_file_switch_or_stop(capture_opts))
                    continue;
            }
        }
    }
    if (use_threads) {
        for (i = 0; i < global_ld.pcaps->len; i++) {
            pcap_src = g_array_index(global_ld.pcaps, capture_src *, i);
            g_thread_join(pcap_src->tid);
        }
        while (1) {
            bool dequeued = capture_loop_dequeue_packet();
            if (!dequeued) {
                break;
            }
            if (capture_opts->output_to_pipe) {
                fflush(global_ld.pdh);
            }
        }
    }
    if (global_ld.file_duration_timer != NULL)
        g_timer_destroy(global_ld.file_duration_timer);
    if (autostop_duration_timer != NULL)
        g_timer_destroy(autostop_duration_timer);
    for (i = 0; i < capture_opts->ifaces->len; i++) {
        pcap_src = g_array_index(global_ld.pcaps, capture_src *, i);
        if (pcap_src->from_cap_pipe && pcap_src->cap_pipe_err == PIPERR) {
            break;
        }
    }
    if (global_ld.err == 0) {
        write_ok = true;
    } else {
        capture_loop_get_errmsg(errmsg, sizeof(errmsg), secondary_errmsg,
                                sizeof(secondary_errmsg),
                                capture_opts->save_file, global_ld.err, false);
        write_ok = false;
    }
    if (capture_opts->saving_to_file) {
        close_ok = capture_loop_close_output(capture_opts, &global_ld, &err_close);
    } else
        close_ok = true;
    if (global_ld.inpkts_to_sync_pipe) {
        if (!quiet)
            report_packet_count(global_ld.inpkts_to_sync_pipe);
        global_ld.inpkts_to_sync_pipe = 0;
    }
    if (!close_ok && write_ok) {
        capture_loop_get_errmsg(errmsg, sizeof(errmsg), secondary_errmsg,
                                sizeof(secondary_errmsg),
                                capture_opts->save_file, err_close, true);
    }
    report_capture_count();
    capture_loop_close_input(&global_ld);
    return write_ok && close_ok;

error:
        if (global_ld.save_file_fd != -1) {
            ws_close(global_ld.save_file_fd);
        }
        if (capture_opts->save_file != NULL) {
            ws_unlink(capture_opts->save_file);
        }
    capture_loop_close_input(&global_ld);
    return false;
}


static void capture_loop_stop(void)
{
    unsigned     i;
    capture_src *pcap_src;
    for (i = 0; i < global_ld.pcaps->len; i++) {
        pcap_src = g_array_index(global_ld.pcaps, capture_src *, i);
    }
    global_ld.go = false;
}


static void capture_loop_get_errmsg(char *errmsg, size_t errmsglen, char *secondary_errmsg,
                                    size_t secondary_errmsglen, const char *fname,
                                    int err, bool is_close)
{
    static const char find_space[] =
        "You will need to free up space on that file system"
        " or put the capture file on a different file system.";
    switch (err) {
    case ENOSPC:
        snprintf(errmsg, errmsglen,
                   "Not all the packets could be written to the file"
                   " to which the capture was being saved\n"
                   "(\"%s\") because there is no space left on the file system\n"
                   "on which that file resides.",
                   fname);
        snprintf(secondary_errmsg, secondary_errmsglen, "%s", find_space);
        break;
    default:
        if (is_close) {
            snprintf(errmsg, errmsglen,
                       "The file to which the capture was being saved\n"
                       "(\"%s\") could not be closed: %s.",
                       fname, g_strerror(err));
        } else {
            snprintf(errmsg, errmsglen,
                       "An error occurred while writing to the file"
                       " to which the capture was being saved\n"
                       "(\"%s\"): %s.",
                       fname, g_strerror(err));
        }
        break;
    }
}

static void capture_loop_wrote_one_packet(capture_src *pcap_src) {
    global_ld.packets_captured++;
    global_ld.packets_written++;
    global_ld.inpkts_to_sync_pipe++;
    if (!use_threads) {
        pcap_src->received++;
    }
    if (global_capture_opts.has_autostop_packets && global_ld.packets_captured >= global_capture_opts.autostop_packets) {
        fflush(global_ld.pdh);
        global_ld.go = false;
        return;
    }
    if (global_capture_opts.has_autostop_written_packets && global_ld.packets_captured >= global_capture_opts.autostop_written_packets) {
        fflush(global_ld.pdh);
        global_ld.go = false;
        return;
    }
    if (global_capture_opts.has_file_packets && global_ld.packets_written >= global_capture_opts.file_packets) {
        do_file_switch_or_stop(&global_capture_opts);
        return;
    }
    if (global_capture_opts.has_autostop_filesize &&
        global_capture_opts.autostop_filesize > 0 &&
        global_ld.bytes_written / 1000 >= global_capture_opts.autostop_filesize) {
        do_file_switch_or_stop(&global_capture_opts);
        return;
    }
}

static void capture_loop_write_pcapng_cb(capture_src *pcap_src, const pcapng_block_header_t *bh, uint8_t *pd)
{
    int          err;
    ws_assert(global_capture_opts.use_pcapng);
    if (!global_ld.go) {
        pcap_src->flushed++;
        return;
    }
    if (!pcapng_adjust_block(pcap_src, bh, pd)) {
        ws_assert_not_reached();
        return;
    }
    if (bh->block_type == BLOCK_TYPE_SHB && !global_ld.pcapng_passthrough) {
        return;
    }
    if (global_ld.pdh) {
        bool successful;
        successful = pcapng_write_block(global_ld.pdh,
                                       pd,
                                       bh->block_total_length,
                                       &global_ld.bytes_written, &err);
        fflush(global_ld.pdh);
        if (!successful) {
            global_ld.go = false;
            global_ld.err = err;
            pcap_src->dropped++;
        } else if (is_data_block(bh->block_type)) {
            capture_loop_wrote_one_packet(pcap_src);
        } else if (bh->block_type == BLOCK_TYPE_SHB && report_capture_filename) {
            sync_pipe_write_string_msg(sync_pipe_fd, SP_FILE, report_capture_filename);
            report_capture_filename = NULL;
        }
    }
}

static void capture_loop_write_packet_cb(uint8_t *pcap_src_p, const struct pcap_pkthdr *phdr,
                             const uint8_t *pd)
{
    capture_src *pcap_src = (capture_src *) (void *) pcap_src_p;
    int          err;
    unsigned     ts_mul    = pcap_src->ts_nsec ? 1000000000 : 1000000;
    if (!global_ld.go) {
        pcap_src->flushed++;
        return;
    }
    if (global_ld.pdh) {
        bool successful;
        if (global_capture_opts.use_pcapng) {
            successful = pcapng_write_enhanced_packet_block(global_ld.pdh,
                                                            NULL,
                                                            phdr->ts.tv_sec, (int32_t)phdr->ts.tv_usec,
                                                            phdr->caplen, phdr->len,
                                                            pcap_src->idb_id,
                                                            ts_mul,
                                                            pd, 0,
                                                            &global_ld.bytes_written, &err);
        } else {
            successful = libpcap_write_packet(global_ld.pdh,
                                              phdr->ts.tv_sec, (int32_t)phdr->ts.tv_usec,
                                              phdr->caplen, phdr->len,
                                              pd,
                                              &global_ld.bytes_written, &err);
        }
        if (!successful) {
            global_ld.go = false;
            global_ld.err = err;
            pcap_src->dropped++;
        } else {
            capture_loop_wrote_one_packet(pcap_src);
        }
    }
}

static void capture_loop_queue_packet_cb(uint8_t *pcap_src_p, const struct pcap_pkthdr *phdr,
                             const uint8_t *pd)
{
    capture_src        *pcap_src = (capture_src *) (void *) pcap_src_p;
    pcap_queue_element *queue_element;
    bool                limit_reached;

    if (!global_ld.go) {
        pcap_src->flushed++;
        return;
    }

    queue_element = g_new(pcap_queue_element, 1);
    if (queue_element == NULL) {
       pcap_src->dropped++;
       return;
    }
    queue_element->pcap_src = pcap_src;
    queue_element->u.phdr = *phdr;
    queue_element->pd = (uint8_t *)g_malloc(phdr->caplen);
    if (queue_element->pd == NULL) {
        pcap_src->dropped++;
        g_free(queue_element);
        return;
    }
    memcpy(queue_element->pd, pd, phdr->caplen);
    g_async_queue_lock(pcap_queue);
    if (((pcap_queue_byte_limit == 0) || (pcap_queue_bytes < pcap_queue_byte_limit)) &&
        ((pcap_queue_packet_limit == 0) || (pcap_queue_packets < pcap_queue_packet_limit))) {
        limit_reached = false;
        g_async_queue_push_unlocked(pcap_queue, queue_element);
        pcap_queue_bytes += phdr->caplen;
        pcap_queue_packets += 1;
    } else {
        limit_reached = true;
    }
    g_async_queue_unlock(pcap_queue);
    if (limit_reached) {
        pcap_src->dropped++;
        g_free(queue_element->pd);
        g_free(queue_element);
    } else {
        pcap_src->received++;
    }
}

static void
capture_loop_queue_pcapng_cb(capture_src *pcap_src, const pcapng_block_header_t *bh, uint8_t *pd)
{
    pcap_queue_element *queue_element;
    bool                limit_reached;

    if (!global_ld.go) {
        pcap_src->flushed++;
        return;
    }

    queue_element = g_new(pcap_queue_element, 1);
    if (queue_element == NULL) {
       pcap_src->dropped++;
       return;
    }
    queue_element->pcap_src = pcap_src;
    queue_element->u.bh = *bh;
    queue_element->pd = (uint8_t *)g_malloc(bh->block_total_length);
    if (queue_element->pd == NULL) {
        pcap_src->dropped++;
        g_free(queue_element);
        return;
    }
    memcpy(queue_element->pd, pd, bh->block_total_length);
    g_async_queue_lock(pcap_queue);
    if (((pcap_queue_byte_limit == 0) || (pcap_queue_bytes < pcap_queue_byte_limit)) &&
        ((pcap_queue_packet_limit == 0) || (pcap_queue_packets < pcap_queue_packet_limit))) {
        limit_reached = false;
        g_async_queue_push_unlocked(pcap_queue, queue_element);
        pcap_queue_bytes += bh->block_total_length;
        pcap_queue_packets += 1;
    } else {
        limit_reached = true;
    }
    g_async_queue_unlock(pcap_queue);
    if (limit_reached) {
        pcap_src->dropped++;
        g_free(queue_element->pd);
        g_free(queue_element);
    } else {
        pcap_src->received++;
    }
}




#define LONGOPT_IFNAME             LONGOPT_BASE_APPLICATION+1
#define LONGOPT_IFDESCR            LONGOPT_BASE_APPLICATION+2
#define LONGOPT_CAPTURE_COMMENT    LONGOPT_BASE_APPLICATION+3

int main(int argc, char *argv[])
{
    char             *err_msg;
    int               opt;
    struct sigaction  action, oldaction;
    int               status;
    int               i;
    unsigned          j;
    GString          *str;

    for (i=1; i<argc; i++) {
        if (strcmp("-Z", argv[i]) == 0) {
            i++;
            if (i >= argc) {
                exit_main(1);
            }

            if (strcmp(argv[i], SIGNAL_PIPE_CTRL_ID_NONE) != 0) {
                if (!ws_strtoi(argv[i], NULL, &sync_pipe_fd) || sync_pipe_fd <= 0) {
                    exit_main(1);
                }
            }
        }
    }

    cmdarg_err_init(dumpcap_cmdarg_err, dumpcap_cmdarg_err_cont);


    global_ld.pcaps = g_array_new(FALSE, FALSE, sizeof(capture_src *));
    global_ld.pcapng_passthrough = false;
    global_ld.saved_shb = NULL;
    global_ld.saved_idbs = g_array_new(FALSE, TRUE, sizeof(saved_idb_t));

    memset(&action, 0, sizeof(action));
    action.sa_handler = capture_cleanup_handler;
    action.sa_flags = 0;
    sigemptyset(&action.sa_mask);
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGPIPE, &action, NULL);
    sigaction(SIGHUP, NULL, &oldaction);
    if (oldaction.sa_handler == SIG_DFL)
        sigaction(SIGHUP, &action, NULL);

    init_process_policies();

    capture_opts_init(&global_capture_opts);
    global_capture_opts.saving_to_file      = true;
    global_capture_opts.has_ring_num_files  = true;


    while ((opt = ws_getopt_long(argc, argv, "i:t:Z:", NULL, NULL)) != -1) {
        switch (opt) {
        case 't':
        case 'i':
            /* Cloonix: remember the snf the fan-out names itself after. */
            if (opt == 'i') {
                cloonix_snf = ws_optarg;
            }
            status = capture_opts_add_opt(&global_capture_opts, opt, ws_optarg);
            if (status != 0) {
                exit_main(status);
            }
            break;
        case 'Z':
            break;
        default:
            cmdarg_err("Invalid Option: %s", argv[ws_optind-1]);
            exit_main(1);
            break;
        }
    }

    if ((pcap_queue_byte_limit > 0) || (pcap_queue_packet_limit > 0)) {
        use_threads = true;
    }
    if ((pcap_queue_byte_limit == 0) && (pcap_queue_packet_limit == 0)) {
        pcap_queue_byte_limit = 1000 * 1000;
        pcap_queue_packet_limit = 1000;
    }

    if (global_capture_opts.ifaces->len > 1) {
        use_threads = true;
        global_capture_opts.use_pcapng = true;
    }
    status = capture_opts_default_iface_if_necessary(&global_capture_opts, NULL);
    if (status != 0) {
        exit_main(status);
    }
    for (j = 0; j < global_capture_opts.ifaces->len; j++) {
        interface_options *interface_opts;
        interface_opts = &g_array_index(global_capture_opts.ifaces, interface_options, j);
    }
    capture_opts_trim_snaplen(&global_capture_opts, 1);
    capture_opts_trim_ring_num_files(&global_capture_opts);

    /* Always write plain pcap to stdout so the parent can read it directly */
    global_capture_opts.save_file      = g_strdup("-");
    global_capture_opts.output_to_pipe = true;
    global_capture_opts.use_pcapng     = false;

    /* Cloonix: put the fan-out in front of stdout.
     *
     * This has to happen after the options above and before the capture loop:
     * it replaces fd 1 with a pipe of its own, and the capture loop takes fd 1
     * as it finds it. The socket it serves is fixed and built from the snf
     * given with -i, so there is nothing to ask for on the command line.
     * stdout is kept as one of the receivers, so a dumpcap started by hand
     * still writes its pcap where it always did. */
    if (cloonix_snf) {
        /* stdout is kept as a receiver only when it is NOT a terminal.
         * This dumpcap is now started by the switch, which may well have a
         * terminal on fd 1, and a capture written to it would be pages of
         * binary over whatever was there. Redirected to a file or a pipe on
         * purpose, it still works as it always did. */
        cloonix_fanout_start(cloonix_snf, !isatty(1));
    }

    fflush(stderr);
    if (capture_loop_start(&global_capture_opts)) {
        exit_main(0);
    } else {
        exit_main(1);
    }
    return 0;
}


/****************************************************************************************************************/
/* indication report routines */


static void
report_packet_count(unsigned int packet_count)
{
    static unsigned int count = 0;

        sync_pipe_write_uint_msg(sync_pipe_fd, SP_PACKET_COUNT, packet_count);
}

static void
report_new_capture_file(const char *filename)
{
        if (global_ld.pcapng_passthrough) {
            report_capture_filename = filename;
        } else {
            sync_pipe_write_string_msg(sync_pipe_fd, SP_FILE, filename);
        }
}



