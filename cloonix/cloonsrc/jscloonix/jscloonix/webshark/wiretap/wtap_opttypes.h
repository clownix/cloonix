/** @file
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 2001 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef WTAP_OPT_TYPES_H
#define WTAP_OPT_TYPES_H

#include <wsutil/inet_addr.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define OPT_EOFOPT             0     /**< Appears in pcapng files, but not in blocks. */
#define OPT_COMMENT            1     /**< A UTF-8 string containing a human-readable comment. */
#define OPT_CUSTOM_STR_COPY    2988  /**< A custom option containing a UTF-8 string, copying allowed. */
#define OPT_CUSTOM_BIN_COPY    2989  /**< A custom option containing binary data, copying allowed. */
#define OPT_CUSTOM_STR_NO_COPY 19372 /**< A custom option containing a UTF-8 string, copying not allowed. */
#define OPT_CUSTOM_BIN_NO_COPY 19373 /**< A custom option containing binary data, copying not allowed. */

#define OPT_SHB_HARDWARE       2     /**< A UTF-8 string containing the description of the
                                       *     hardware used to create this section.
                                       */
#define OPT_SHB_OS             3     /**< A UTF-8 string containing the
                                       *     name of the operating system used to create this section.
                                       */
#define OPT_SHB_USERAPPL       4     /**< A UTF-8 string containing the
                                       *     name of the application used to create this section.
                                       */

#define OPT_IDB_NAME           2     /**< A UTF-8 string containing the name
                                       *     of the device used to capture data.
                                       *     "eth0" / "\Device\NPF_{AD1CE675-96D0-47C5-ADD0-2504B9126B68}"
                                       */
#define OPT_IDB_DESCRIPTION    3     /**< A UTF-8 string containing the description
                                       *     of the device used to capture data.
                                       *     "Wi-Fi" / "Local Area Connection" /
                                       *     "Wireless Network Connection" /
                                       *     "First Ethernet Interface"
                                       */
#define OPT_IDB_IP4ADDR        4     /**< XXX: if_IPv4addr Interface network address and netmask.
                                       *     This option can be repeated multiple times within the same Interface Description Block
                                       *     when multiple IPv4 addresses are assigned to the interface.
                                       *     192 168 1 1 255 255 255 0
                                       */
#define OPT_IDB_IP6ADDR        5     /**< XXX: if_IPv6addr Interface network address and prefix length (stored in the last byte).
                                       *     This option can be repeated multiple times within the same Interface
                                       *     Description Block when multiple IPv6 addresses are assigned to the interface.
                                       *     2001:0db8:85a3:08d3:1319:8a2e:0370:7344/64 is written (in hex) as
                                       *     "20 01 0d b8 85 a3 08 d3 13 19 8a 2e 03 70 73 44 40"
                                       */
#define OPT_IDB_MACADDR        6     /**< XXX: if_MACaddr  Interface Hardware MAC address (48 bits).                             */
#define OPT_IDB_EUIADDR        7     /**< XXX: if_EUIaddr  Interface Hardware EUI address (64 bits)                              */
#define OPT_IDB_SPEED          8     /**< Interface speed (in bps). 100000000 for 100Mbps
                                       */
#define OPT_IDB_TSRESOL        9     /**< Resolution of timestamps. If the Most Significant Bit is equal to zero,
                                       *     the remaining bits indicates the resolution of the timestamp as a
                                       *     negative power of 10 (e.g. 6 means microsecond resolution, timestamps
                                       *     are the number of microseconds since 1/1/1970). If the Most Significant Bit
                                       *     is equal to one, the remaining bits indicates the resolution has a
                                       *     negative power of 2 (e.g. 10 means 1/1024 of second).
                                       *     If this option is not present, a resolution of 10^-6 is assumed
                                       *     (i.e. timestamps have the same resolution of the standard 'libpcap' timestamps).
                                       */
#define OPT_IDB_TZONE          10    /**< Time zone for GMT support.  This option has neer been specified in
                                       *     greater detail and, unless it were to identify something such as
                                       *     an IANA time zone database timezone, would be insufficient for
                                       *     converting between UTC and local time.  Therefore, it SHOULD NOT
                                       *     be used; instead, the if_iana_tzname option SHOULD be used if
                                       *     time zone information is to be specified. */
#define OPT_IDB_FILTER         11    /**< The filter (e.g. "capture only TCP traffic") used to capture traffic.
                                       *     The first byte of the Option Data keeps a code of the filter used
                                       *     (e.g. if this is a libpcap string, or BPF bytecode, and more).
                                       *     More details about this format will be presented in Appendix XXX (TODO).
                                       *     (TODO: better use different options for different fields?
                                       *     e.g. if_filter_pcap, if_filter_bpf, ...) 00 "tcp port 23 and host 10.0.0.5"
                                       */
#define OPT_IDB_OS             12    /**< A UTF-8 string containing the name of the operating system of the
                                       *     machine in which this interface is installed.
                                       *     This can be different from the same information that can be
                                       *     contained by the Section Header Block
                                       *     (Section 3.1 (Section Header Block (mandatory))) because
                                       *     the capture can have been done on a remote machine.
                                       *     "Windows XP SP2" / "openSUSE 10.2"
                                       */
#define OPT_IDB_FCSLEN         13    /**< An integer value that specified the length of the
                                       *     Frame Check Sequence (in bits) for this interface.
                                       *     For link layers whose FCS length can change during time,
                                       *     the Packet Block Flags Word can be used (see Appendix A (Packet Block Flags Word))
                                       */
#define OPT_IDB_TSOFFSET       14    /**< A 64-bit signed integer value that specifies an offset (in seconds)
                                       *     that must be added to the timestamp of each packet to obtain
                                       *     the absolute timestamp of a packet. If the option is not present,
                                       *     an offst of 0 is assumed (i.e., timestamps in blocks are absolute
                                       *     timestamps).
                                       *
                                       *     This offset is not intended to be used as an offset between local
                                       *     time and UTC; for this purpose, the if_iana_tzname option SHOULD be
                                       *     used to specify a timezone.
                                       */
#define OPT_IDB_HARDWARE       15    /**< A UTF-8 string containing the description
                                       *     of the hardware of the device used
                                       *     to capture data.
                                       *     "Broadcom NetXtreme" /
                                       *     "Intel(R) PRO/1000 MT Network Connection" /
                                       *     "NETGEAR WNA1000Mv2 N150 Wireless USB Micro Adapter"
                                       */

#define OPT_PKT_FLAGS        2
#define OPT_PKT_HASH         3
#define OPT_PKT_DROPCOUNT    4
#define OPT_PKT_PACKETID     5
#define OPT_PKT_QUEUE        6
#define OPT_PKT_VERDICT      7

#define OPT_NS_DNSNAME       2
#define OPT_NS_DNSIP4ADDR    3
#define OPT_NS_DNSIP6ADDR    4

#define OPT_ISB_STARTTIME    2
#define OPT_ISB_ENDTIME      3
#define OPT_ISB_IFRECV       4
#define OPT_ISB_IFDROP       5
#define OPT_ISB_FILTERACCEPT 6
#define OPT_ISB_OSDROP       7
#define OPT_ISB_USRDELIV     8

struct wtap_block;
typedef struct wtap_block *wtap_block_t;

typedef enum {
    WTAP_BLOCK_SECTION = 0,
    WTAP_BLOCK_IF_ID_AND_INFO,
    WTAP_BLOCK_NAME_RESOLUTION,
    WTAP_BLOCK_IF_STATISTICS,
    WTAP_BLOCK_DECRYPTION_SECRETS,
    WTAP_BLOCK_PACKET,
    WTAP_BLOCK_FT_SPECIFIC_REPORT,
    WTAP_BLOCK_FT_SPECIFIC_EVENT,
    WTAP_BLOCK_SYSDIG_EVENT,
    WTAP_BLOCK_META_EVENT,
    WTAP_BLOCK_SYSTEMD_JOURNAL_EXPORT,
    WTAP_BLOCK_CUSTOM,
    MAX_WTAP_BLOCK_TYPE_VALUE
} wtap_block_type_t;

typedef struct wtapng_section_mandatory_s {
    uint64_t            section_length;
} wtapng_section_mandatory_t;

typedef struct wtapng_iface_descriptions_s {
    GArray *interface_data;
} wtapng_iface_descriptions_t;

typedef struct wtapng_if_descr_mandatory_s {
    int                    wtap_encap;
    uint64_t               time_units_per_second;
    int                    tsprecision;
    uint32_t               snap_len;
    uint8_t                num_stat_entries;
    GArray                *interface_statistics;
} wtapng_if_descr_mandatory_t;

typedef struct wtapng_nrb_mandatory_s {
    GList       *ipv4_addr_list;
    GList       *ipv6_addr_list;
}  wtapng_nrb_mandatory_t;

typedef struct wtapng_if_stats_mandatory_s {
    uint32_t interface_id;
    uint32_t ts_high;
    uint32_t ts_low;
} wtapng_if_stats_mandatory_t;

typedef struct wtapng_dsb_mandatory_s {
    uint32_t               secrets_type;
    uint32_t               secrets_len;
    uint8_t               *secrets_data;
} wtapng_dsb_mandatory_t;

typedef struct wtapng_meta_event_mandatory_s {
    uint32_t               mev_block_type;
    unsigned               mev_data_len;
    uint8_t               *mev_data;
} wtapng_meta_event_mandatory_t;

typedef struct wtapng_ft_specific_mandatory_s {
    unsigned  record_type;      /* the type of record this is - file type-specific value */
} wtapng_ft_specific_mandatory_t;

typedef enum {
    WTAP_OPTTYPE_UINT8,
    WTAP_OPTTYPE_UINT32,
    WTAP_OPTTYPE_UINT64,
    WTAP_OPTTYPE_STRING,
    WTAP_OPTTYPE_BYTES,
    WTAP_OPTTYPE_IPv4,
    WTAP_OPTTYPE_IPv6,
    WTAP_OPTTYPE_CUSTOM,
    WTAP_OPTTYPE_IF_FILTER,
    WTAP_OPTTYPE_PACKET_VERDICT,
    WTAP_OPTTYPE_PACKET_HASH,
    WTAP_OPTTYPE_INT8,
    WTAP_OPTTYPE_INT32,
    WTAP_OPTTYPE_INT64,
} wtap_opttype_e;

typedef enum {
    WTAP_OPTTYPE_SUCCESS = 0,
    WTAP_OPTTYPE_NO_SUCH_OPTION = -1,
    WTAP_OPTTYPE_NOT_FOUND = -2,
    WTAP_OPTTYPE_TYPE_MISMATCH = -3,
    WTAP_OPTTYPE_NUMBER_MISMATCH = -4,
    WTAP_OPTTYPE_ALREADY_EXISTS = -5,
    WTAP_OPTTYPE_BAD_BLOCK = -6,
} wtap_opttype_return_val;

#define PEN_NFLX 10949
#define PEN_VCTR 46254

typedef struct custom_opt_s {
    uint32_t pen;
    union {
        struct generic_custom_opt_data {
            size_t custom_data_len;
            char *custom_data;
        } generic_data;
        struct nflx_custom_opt_data {
            uint32_t type;
            size_t custom_data_len;
            char *custom_data;
            bool use_little_endian;
        } nflx_data;
    } data;
} custom_opt_t;

typedef struct nflx_custom_opt_s {
    bool nflx_use_little_endian;
    uint32_t nflx_type;
    size_t nflx_custom_data_len;
    char *nflx_custom_data;
} nflx_custom_opt_t;

typedef struct wtap_bpf_insn_s {
    uint16_t               code;
    uint8_t                jt;
    uint8_t                jf;
    uint32_t               k;
} wtap_bpf_insn_t;

typedef enum {
    if_filter_pcap = 0,
    if_filter_bpf  = 1
} if_filter_type_e;

typedef struct if_filter_opt_s {
    if_filter_type_e type;
    union {
        char              *filter_str;
        struct wtap_bpf_insns {
            unsigned       bpf_prog_len;
            wtap_bpf_insn_t *bpf_prog;
        }                  bpf_prog;
    }                      data;
} if_filter_opt_t;

typedef enum {
    packet_verdict_hardware =       0,
    packet_verdict_linux_ebpf_tc  = 1,
    packet_verdict_linux_ebpf_xdp = 2
} packet_verdict_type_e;

typedef struct packet_verdict_opt_s {
    packet_verdict_type_e type;
    union {
        GByteArray *verdict_bytes;
        uint64_t    verdict_linux_ebpf_tc;
        uint64_t    verdict_linux_ebpf_xdp;
    }               data;
} packet_verdict_opt_t;

typedef struct packet_hash_opt_s {
    uint8_t type;
    GByteArray *hash_bytes;
} packet_hash_opt_t;

typedef union {
    uint8_t uint8val;
    uint32_t uint32val;
    uint64_t uint64val;
    int8_t int8val;
    int32_t int32val;
    int64_t int64val;
    ws_in4_addr ipv4val;    /* network byte order */
    ws_in6_addr ipv6val;
    char *stringval;
    GBytes *byteval;
    custom_opt_t custom_opt;
    if_filter_opt_t if_filterval;
    packet_verdict_opt_t packet_verdictval;
    packet_hash_opt_t packet_hash;
} wtap_optval_t;

typedef struct {
    unsigned option_id;     /**< option code for the option */
    wtap_optval_t value; /**< value */
} wtap_option_t;

#define NFLX_OPT_TYPE_VERSION    1
#define NFLX_OPT_TYPE_TCPINFO    2
#define NFLX_OPT_TYPE_DUMPINFO   4
#define NFLX_OPT_TYPE_DUMPTIME   5
#define NFLX_OPT_TYPE_STACKNAME  6

struct nflx_dumpinfo {
    uint32_t tlh_version;
    uint32_t tlh_type;
    uint64_t tlh_length;
    uint16_t tlh_ie_fport;
    uint16_t tlh_ie_lport;
    uint32_t tlh_ie_faddr_addr32[4];
    uint32_t tlh_ie_laddr_addr32[4];
    uint32_t tlh_ie_zoneid;
    uint64_t tlh_offset_tv_sec;
    uint64_t tlh_offset_tv_usec;
    char    tlh_id[64];
    char    tlh_reason[32];
    char    tlh_tag[32];
    uint8_t tlh_af;
    uint8_t _pad[7];
};

/* Flags used in tlb_eventflags */
#define NFLX_TLB_FLAG_RXBUF     0x0001 /* Includes receive buffer info */
#define NFLX_TLB_FLAG_TXBUF     0x0002 /* Includes send buffer info */
#define NFLX_TLB_FLAG_HDR       0x0004 /* Includes a TCP header */
#define NFLX_TLB_FLAG_VERBOSE   0x0008 /* Includes function/line numbers */
#define NFLX_TLB_FLAG_STACKINFO 0x0010 /* Includes stack-specific info */

/* Flags used in tlb_flags */
#define NFLX_TLB_TF_REQ_SCALE   0x00000020 /* Sent WS option */
#define NFLX_TLB_TF_RCVD_SCALE  0x00000040 /* Received WS option */

/* Values of tlb_state */
#define NFLX_TLB_TCPS_ESTABLISHED 4
#define NFLX_TLB_IS_SYNCHRONIZED(state) (state >= NFLX_TLB_TCPS_ESTABLISHED)

struct nflx_tcpinfo {
    uint64_t tlb_tv_sec;
    uint64_t tlb_tv_usec;
    uint32_t tlb_ticks;
    uint32_t tlb_sn;
    uint8_t tlb_stackid;
    uint8_t tlb_eventid;
    uint16_t tlb_eventflags;
    int32_t tlb_errno;
    uint32_t tlb_rxbuf_tls_sb_acc;
    uint32_t tlb_rxbuf_tls_sb_ccc;
    uint32_t tlb_rxbuf_tls_sb_spare;
    uint32_t tlb_txbuf_tls_sb_acc;
    uint32_t tlb_txbuf_tls_sb_ccc;
    uint32_t tlb_txbuf_tls_sb_spare;
    int32_t tlb_state;
    uint32_t tlb_starttime;
    uint32_t tlb_iss;
    uint32_t tlb_flags;
    uint32_t tlb_snd_una;
    uint32_t tlb_snd_max;
    uint32_t tlb_snd_cwnd;
    uint32_t tlb_snd_nxt;
    uint32_t tlb_snd_recover;
    uint32_t tlb_snd_wnd;
    uint32_t tlb_snd_ssthresh;
    uint32_t tlb_srtt;
    uint32_t tlb_rttvar;
    uint32_t tlb_rcv_up;
    uint32_t tlb_rcv_adv;
    uint32_t tlb_flags2;
    uint32_t tlb_rcv_nxt;
    uint32_t tlb_rcv_wnd;
    uint32_t tlb_dupacks;
    int32_t tlb_segqlen;
    int32_t tlb_snd_numholes;
    uint32_t tlb_flex1;
    uint32_t tlb_flex2;
    uint32_t tlb_fbyte_in;
    uint32_t tlb_fbyte_out;
    uint8_t tlb_snd_scale:4,
            tlb_rcv_scale:4;
    uint8_t _pad[3];

    uint64_t tlb_stackinfo_bbr_cur_del_rate;
    uint64_t tlb_stackinfo_bbr_delRate;
    uint64_t tlb_stackinfo_bbr_rttProp;
    uint64_t tlb_stackinfo_bbr_bw_inuse;
    uint32_t tlb_stackinfo_bbr_inflight;
    uint32_t tlb_stackinfo_bbr_applimited;
    uint32_t tlb_stackinfo_bbr_delivered;
    uint32_t tlb_stackinfo_bbr_timeStamp;
    uint32_t tlb_stackinfo_bbr_epoch;
    uint32_t tlb_stackinfo_bbr_lt_epoch;
    uint32_t tlb_stackinfo_bbr_pkts_out;
    uint32_t tlb_stackinfo_bbr_flex1;
    uint32_t tlb_stackinfo_bbr_flex2;
    uint32_t tlb_stackinfo_bbr_flex3;
    uint32_t tlb_stackinfo_bbr_flex4;
    uint32_t tlb_stackinfo_bbr_flex5;
    uint32_t tlb_stackinfo_bbr_flex6;
    uint32_t tlb_stackinfo_bbr_lost;
    uint16_t tlb_stackinfo_bbr_pacing_gain;
    uint16_t tlb_stackinfo_bbr_cwnd_gain;
    uint16_t tlb_stackinfo_bbr_flex7;
    uint8_t tlb_stackinfo_bbr_bbr_state;
    uint8_t tlb_stackinfo_bbr_bbr_substate;
    uint8_t tlb_stackinfo_bbr_inhpts;
    uint8_t tlb_stackinfo_bbr_ininput;
    uint8_t tlb_stackinfo_bbr_use_lt_bw;
    uint8_t tlb_stackinfo_bbr_flex8;
    uint32_t tlb_stackinfo_bbr_pkt_epoch;

    uint32_t tlb_len;
};

typedef void (*wtap_block_create_func)(wtap_block_t block);
typedef void (*wtap_mand_free_func)(wtap_block_t block);
typedef void (*wtap_mand_copy_func)(wtap_block_t dest_block, wtap_block_t src_block);

void wtap_opttypes_initialize(void);
wtap_block_t wtap_block_create(wtap_block_type_t block_type);
wtap_block_t wtap_block_ref(wtap_block_t block);
void wtap_block_unref(wtap_block_t block);
void wtap_block_array_free(GArray* block_array);
void wtap_block_array_unref(GArray* block_array);
void wtap_block_array_ref(GArray* block_array);
wtap_block_type_t wtap_block_get_type(wtap_block_t block);
void* wtap_block_get_mandatory_data(wtap_block_t block);
unsigned wtap_block_count_option(wtap_block_t block, unsigned option_id);
wtap_opttype_return_val wtap_block_add_uint8_option(wtap_block_t block, unsigned option_id, uint8_t value);
wtap_opttype_return_val wtap_block_set_uint8_option_value(wtap_block_t block, unsigned option_id, uint8_t value);
wtap_opttype_return_val wtap_block_get_uint8_option_value(wtap_block_t block, unsigned option_id, uint8_t* value) G_GNUC_WARN_UNUSED_RESULT;
wtap_opttype_return_val wtap_block_add_uint32_option(wtap_block_t block, unsigned option_id, uint32_t value);
wtap_opttype_return_val wtap_block_set_uint32_option_value(wtap_block_t block, unsigned option_id, uint32_t value);
wtap_opttype_return_val wtap_block_get_uint32_option_value(wtap_block_t block, unsigned option_id, uint32_t* value) G_GNUC_WARN_UNUSED_RESULT;
wtap_opttype_return_val wtap_block_add_uint64_option(wtap_block_t block, unsigned option_id, uint64_t value);
wtap_opttype_return_val wtap_block_set_uint64_option_value(wtap_block_t block, unsigned option_id, uint64_t value);
wtap_opttype_return_val wtap_block_get_uint64_option_value(wtap_block_t block, unsigned option_id, uint64_t* value) G_GNUC_WARN_UNUSED_RESULT;
wtap_opttype_return_val wtap_block_add_int8_option(wtap_block_t block, unsigned option_id, int8_t value);
wtap_opttype_return_val wtap_block_set_int8_option_value(wtap_block_t block, unsigned option_id, int8_t value);
wtap_opttype_return_val wtap_block_get_int8_option_value(wtap_block_t block, unsigned option_id, int8_t* value) G_GNUC_WARN_UNUSED_RESULT;
wtap_opttype_return_val wtap_block_add_int32_option(wtap_block_t block, unsigned option_id, int32_t value);
wtap_opttype_return_val wtap_block_set_int32_option_value(wtap_block_t block, unsigned option_id, int32_t value);
wtap_opttype_return_val wtap_block_get_int32_option_value(wtap_block_t block, unsigned option_id, int32_t* value) G_GNUC_WARN_UNUSED_RESULT;
wtap_opttype_return_val wtap_block_add_int64_option(wtap_block_t block, unsigned option_id, int64_t value);
wtap_opttype_return_val wtap_block_set_int64_option_value(wtap_block_t block, unsigned option_id, int64_t value);
wtap_opttype_return_val wtap_block_get_int64_option_value(wtap_block_t block, unsigned option_id, int64_t* value) G_GNUC_WARN_UNUSED_RESULT;
wtap_opttype_return_val wtap_block_add_ipv4_option(wtap_block_t block, unsigned option_id, uint32_t value);
wtap_opttype_return_val wtap_block_set_ipv4_option_value(wtap_block_t block, unsigned option_id, uint32_t value);
wtap_opttype_return_val wtap_block_get_ipv4_option_value(wtap_block_t block, unsigned option_id, uint32_t* value) G_GNUC_WARN_UNUSED_RESULT;
wtap_opttype_return_val wtap_block_add_ipv6_option(wtap_block_t block, unsigned option_id, ws_in6_addr *value);
wtap_opttype_return_val wtap_block_set_ipv6_option_value(wtap_block_t block, unsigned option_id, ws_in6_addr *value);
wtap_opttype_return_val wtap_block_get_ipv6_option_value(wtap_block_t block, unsigned option_id, ws_in6_addr* value) G_GNUC_WARN_UNUSED_RESULT;
wtap_opttype_return_val wtap_block_add_string_option(wtap_block_t block, unsigned option_id, const char *value, size_t value_length);
wtap_opttype_return_val wtap_block_add_string_option_owned(wtap_block_t block, unsigned option_id, char *value);
wtap_opttype_return_val wtap_block_add_string_option_format(wtap_block_t block, unsigned option_id, const char *format, ...)
G_GNUC_PRINTF(3,4);
wtap_opttype_return_val wtap_block_set_string_option_value(wtap_block_t block, unsigned option_id, const char* value, size_t value_length);
wtap_opttype_return_val wtap_block_set_nth_string_option_value(wtap_block_t block, unsigned option_id, unsigned idx, const char* value, size_t value_length);
wtap_opttype_return_val wtap_block_set_string_option_value_format(wtap_block_t block, unsigned option_id, const char *format, ...)
G_GNUC_PRINTF(3,4);
wtap_opttype_return_val wtap_block_set_nth_string_option_value_format(wtap_block_t block, unsigned option_id, unsigned idx, const char *format, ...)
G_GNUC_PRINTF(4,5);
wtap_opttype_return_val wtap_block_get_string_option_value(wtap_block_t block, unsigned option_id, char** value) G_GNUC_WARN_UNUSED_RESULT;
wtap_opttype_return_val wtap_block_get_nth_string_option_value(wtap_block_t block, unsigned option_id, unsigned idx, char** value) G_GNUC_WARN_UNUSED_RESULT;
wtap_opttype_return_val wtap_block_add_bytes_option(wtap_block_t block, unsigned option_id, const uint8_t *value, size_t value_length);
wtap_opttype_return_val wtap_block_add_bytes_option_borrow(wtap_block_t block, unsigned option_id, GBytes *value);
wtap_opttype_return_val wtap_block_set_bytes_option_value(wtap_block_t block, unsigned option_id, const uint8_t* value, size_t value_length);
wtap_opttype_return_val wtap_block_set_nth_bytes_option_value(wtap_block_t block, unsigned option_id, unsigned idx, GBytes* value);
wtap_opttype_return_val wtap_block_get_bytes_option_value(wtap_block_t block, unsigned option_id, GBytes** value) G_GNUC_WARN_UNUSED_RESULT;
wtap_opttype_return_val wtap_block_get_nth_bytes_option_value(wtap_block_t block, unsigned option_id, unsigned idx, GBytes** value) G_GNUC_WARN_UNUSED_RESULT;
wtap_opttype_return_val wtap_block_add_nflx_custom_option(wtap_block_t block, uint32_t nflx_type, const char *nflx_custom_data, size_t nflx_custom_data_len);
wtap_opttype_return_val wtap_block_get_nflx_custom_option(wtap_block_t block, uint32_t nflx_type, char *nflx_custom_data, size_t nflx_custom_data_len);
wtap_opttype_return_val wtap_block_add_custom_option(wtap_block_t block, unsigned option_id, uint32_t pen, const char *custom_data, size_t custom_data_len);
wtap_opttype_return_val wtap_block_add_if_filter_option(wtap_block_t block, unsigned option_id, if_filter_opt_t* value);
wtap_opttype_return_val wtap_block_set_if_filter_option_value(wtap_block_t block, unsigned option_id, if_filter_opt_t* value);
wtap_opttype_return_val wtap_block_get_if_filter_option_value(wtap_block_t block, unsigned option_id, if_filter_opt_t* value) G_GNUC_WARN_UNUSED_RESULT;
wtap_opttype_return_val wtap_block_add_packet_verdict_option(wtap_block_t block, unsigned option_id, packet_verdict_opt_t* value);
wtap_opttype_return_val wtap_block_set_nth_packet_verdict_option_value(wtap_block_t block, unsigned option_id, unsigned idx, packet_verdict_opt_t* value);
wtap_opttype_return_val wtap_block_get_nth_packet_verdict_option_value(wtap_block_t block, unsigned option_id, unsigned idx, packet_verdict_opt_t* value) G_GNUC_WARN_UNUSED_RESULT;
void wtap_packet_verdict_free(packet_verdict_opt_t* verdict);
wtap_opttype_return_val wtap_block_add_packet_hash_option(wtap_block_t block, unsigned option_id, packet_hash_opt_t* value);
void wtap_packet_hash_free(packet_hash_opt_t* hash);
wtap_opttype_return_val wtap_block_remove_option(wtap_block_t block, unsigned option_id);
wtap_opttype_return_val wtap_block_remove_nth_option_instance(wtap_block_t block, unsigned option_id, unsigned idx);
void wtap_block_copy(wtap_block_t dest_block, wtap_block_t src_block);
wtap_block_t wtap_block_make_copy(wtap_block_t block);
typedef bool (*wtap_block_foreach_func)(wtap_block_t block, unsigned option_id, wtap_opttype_e option_type, wtap_optval_t *option, void *user_data);
bool wtap_block_foreach_option(wtap_block_t block, wtap_block_foreach_func func, void* user_data);
void wtap_opttypes_cleanup(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* WTAP_OPT_TYPES_H */
