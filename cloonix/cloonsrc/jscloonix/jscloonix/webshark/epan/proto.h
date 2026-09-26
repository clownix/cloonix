/* proto.h
 * Definitions for protocol display
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __PROTO_H__
#define __PROTO_H__

#include "wsutil/nstime.h"
#include "tvbuff.h"
#include "value_string.h"
#include "packet_info.h"
#include "ftypes/ftypes.h"
#include "register.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

extern int hf_text_only;

#define ITEM_LABEL_LENGTH       240
#define ITEM_LABEL_UNKNOWN_STR  "Unknown"

struct expert_field;

#define cast_same(type, x) (0 ? (type)0 : (x))
#define VALS(x)     (cast_same(const struct _value_string*, (x)))
#define VALS64(x)   (cast_same(const struct _val64_string*, (x)))
#define VALS_EXT_PTR(x) (cast_same(value_string_ext*, (x)))
#define TFS(x)      (cast_same(const struct true_false_string*, (x)))
#define UNS(x)      (cast_same(const struct unit_name_string*, (x)))

typedef void (*custom_fmt_func_t)(char *, uint32_t);

typedef void (*custom_fmt_func_64_t)(char *, uint64_t);

typedef void (*custom_fmt_func_double_t)(char *, double);

#define CF_FUNC(x) ((const void *) (size_t) (x))

#define RVALS(x) (cast_same(const struct _range_string*, (x)))
#define FRAMENUM_TYPE(x) GINT_TO_POINTER(x)

struct _protocol;

typedef struct _protocol protocol_t;

void proto_report_dissector_bug(const char *format, ...)
    G_GNUC_PRINTF(1, 2);

#define REPORT_DISSECTOR_BUG(...)  \
    proto_report_dissector_bug(__VA_ARGS__)

#define __DISSECTOR_ASSERT_STATIC_ANALYSIS_HINT(expression)

#define __DISSECTOR_ASSERT_STRINGIFY(s) # s

#define __DISSECTOR_ASSERT(expression, file, lineno)  \
  (REPORT_DISSECTOR_BUG("%s:%u: failed assertion \"%s\"", \
        file, lineno, __DISSECTOR_ASSERT_STRINGIFY(expression)))

#define __DISSECTOR_ASSERT_HINT(expression, file, lineno, hint)  \
  (REPORT_DISSECTOR_BUG("%s:%u: failed assertion \"%s\" (%s)", \
        file, lineno, __DISSECTOR_ASSERT_STRINGIFY(expression), hint))

#define DISSECTOR_ASSERT(expression)  \
  ((void) ((expression) ? (void)0 : \
   __DISSECTOR_ASSERT (expression, __FILE__, __LINE__))) \
   __DISSECTOR_ASSERT_STATIC_ANALYSIS_HINT(expression)

#define DISSECTOR_ASSERT_HINT(expression, hint)  \
  ((void) ((expression) ? (void)0 : \
   __DISSECTOR_ASSERT_HINT (expression, __FILE__, __LINE__, hint))) \
   __DISSECTOR_ASSERT_STATIC_ANALYSIS_HINT(expression)

#define DISSECTOR_ASSERT_NOT_REACHED()  \
  (REPORT_DISSECTOR_BUG("%s:%u: failed assertion \"DISSECTOR_ASSERT_NOT_REACHED\"", \
        __FILE__, __LINE__))

#define __DISSECTOR_ASSERT_CMPINT(a, op, b, type, fmt) \
  (REPORT_DISSECTOR_BUG("%s:%u: failed assertion " #a " " #op " " #b " (" fmt " " #op " " fmt ")", \
        __FILE__, __LINE__, (type)a, (type)b))

#define DISSECTOR_ASSERT_CMPINT(a, op, b)  \
  ((void) ((a op b) ? (void)0 : \
   __DISSECTOR_ASSERT_CMPINT (a, op, b, int64_t, "%" PRId64))) \
   __DISSECTOR_ASSERT_STATIC_ANALYSIS_HINT(a op b)

#define DISSECTOR_ASSERT_CMPUINT(a, op, b)  \
  ((void) ((a op b) ? (void)0 : \
   __DISSECTOR_ASSERT_CMPINT (a, op, b, uint64_t, "%" PRIu64))) \
   __DISSECTOR_ASSERT_STATIC_ANALYSIS_HINT(a op b)

#define DISSECTOR_ASSERT_CMPUINTHEX(a, op, b)  \
  ((void) ((a op b) ? (void)0 : \
   __DISSECTOR_ASSERT_CMPINT (a, op, b, uint64_t, "0x%" PRIX64))) \
  __DISSECTOR_ASSERT_STATIC_ANALYSIS_HINT(a op b)

#define __DISSECTOR_ASSERT_FIELD_TYPE(hfinfo, t) \
  (REPORT_DISSECTOR_BUG("%s:%u: field %s is not of type "#t, \
        __FILE__, __LINE__, (hfinfo)->abbrev))

#define DISSECTOR_ASSERT_FIELD_TYPE(hfinfo, t)  \
  ((void) (((hfinfo)->type == t) ? (void)0 : \
   __DISSECTOR_ASSERT_FIELD_TYPE ((hfinfo), t))) \
   __DISSECTOR_ASSERT_STATIC_ANALYSIS_HINT((hfinfo)->type == t)

#define DISSECTOR_ASSERT_FIELD_TYPE_IS_INTEGRAL(hfinfo)  \
  ((void) ((FT_IS_INTEGER((hfinfo)->type)) ? (void)0 : \
   REPORT_DISSECTOR_BUG("%s:%u: field %s is not of type FT_CHAR or an FT_{U}INTn type", \
         __FILE__, __LINE__, (hfinfo)->abbrev))) \
   __DISSECTOR_ASSERT_STATIC_ANALYSIS_HINT(FT_IS_INTEGER((hfinfo)->type))

#define __DISSECTOR_ASSERT_FIELD_TYPE_IS_STRING(hfinfo) \
  (REPORT_DISSECTOR_BUG("%s:%u: field %s is not of type FT_STRING, FT_STRINGZ, FT_STRINGZPAD, FT_STRINGZTRUNC, or FT_UINT_STRING", \
        __FILE__, __LINE__, (hfinfo)->abbrev))

#define DISSECTOR_ASSERT_FIELD_TYPE_IS_STRING(hfinfo)  \
  ((void) (FT_IS_STRING((hfinfo)->type) ? (void)0 : \
   __DISSECTOR_ASSERT_FIELD_TYPE_IS_STRING ((hfinfo)))) \
   __DISSECTOR_ASSERT_STATIC_ANALYSIS_HINT(FT_IS_STRING((hfinfo)->type))

#define __DISSECTOR_ASSERT_FIELD_TYPE_IS_TIME(hfinfo) \
  (REPORT_DISSECTOR_BUG("%s:%u: field %s is not of type FT_ABSOLUTE_TIME or FT_RELATIVE_TIME", \
        __FILE__, __LINE__, (hfinfo)->abbrev))

#define DISSECTOR_ASSERT_FIELD_TYPE_IS_TIME(hfinfo)  \
  ((void) (((hfinfo)->type == FT_ABSOLUTE_TIME || \
            (hfinfo)->type == FT_RELATIVE_TIME) ? (void)0 : \
   __DISSECTOR_ASSERT_FIELD_TYPE_IS_TIME ((hfinfo)))) \
   __DISSECTOR_ASSERT_STATIC_ANALYSIS_HINT((hfinfo)->type == FT_ABSOLUTE_TIME || \
                                           (hfinfo)->type == FT_RELATIVE_TIME)

#define ENC_BIG_ENDIAN      0x00000000
#define ENC_LITTLE_ENDIAN   0x80000000

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
    #define ENC_HOST_ENDIAN      ENC_LITTLE_ENDIAN
    #define ENC_ANTI_HOST_ENDIAN ENC_BIG_ENDIAN
#else
    #define ENC_HOST_ENDIAN      ENC_BIG_ENDIAN
    #define ENC_ANTI_HOST_ENDIAN ENC_LITTLE_ENDIAN
#endif

#define ENC_NA          0x00000000

#define ENC_CHARENCODING_MASK             0x0000FFFE  /* mask out byte-order bits and other bits used with string encodings */
#define ENC_ASCII                         0x00000000
#define ENC_ISO_646_IRV                   ENC_ASCII   /* ISO 646 International Reference Version = ASCII */
#define ENC_UTF_8                         0x00000002
#define ENC_UTF_16                        0x00000004
#define ENC_UCS_2                         0x00000006
#define ENC_UCS_4                         0x00000008
#define ENC_ISO_8859_1                    0x0000000A
#define ENC_ISO_8859_2                    0x0000000C
#define ENC_ISO_8859_3                    0x0000000E
#define ENC_ISO_8859_4                    0x00000010
#define ENC_ISO_8859_5                    0x00000012
#define ENC_ISO_8859_6                    0x00000014
#define ENC_ISO_8859_7                    0x00000016
#define ENC_ISO_8859_8                    0x00000018
#define ENC_ISO_8859_9                    0x0000001A
#define ENC_ISO_8859_10                   0x0000001C
#define ENC_ISO_8859_11                   0x0000001E
/* #define ENC_ISO_8859_12                  0x00000020 ISO 8859-12 was abandoned */
#define ENC_ISO_8859_13                   0x00000022
#define ENC_ISO_8859_14                   0x00000024
#define ENC_ISO_8859_15                   0x00000026
#define ENC_ISO_8859_16                   0x00000028
#define ENC_WINDOWS_1250                  0x0000002A
#define ENC_3GPP_TS_23_038_7BITS_PACKED   0x0000002C
#define ENC_3GPP_TS_23_038_7BITS          ENC_3GPP_TS_23_038_7BITS_PACKED
#define ENC_EBCDIC                        0x0000002E
#define ENC_MAC_ROMAN                     0x00000030
#define ENC_CP437                         0x00000032
#define ENC_ASCII_7BITS                   0x00000034
#define ENC_T61                           0x00000036
#define ENC_EBCDIC_CP037                  0x00000038
#define ENC_WINDOWS_1252                  0x0000003A
#define ENC_WINDOWS_1251                  0x0000003C
#define ENC_CP855                         0x0000003E
#define ENC_CP866                         0x00000040
#define ENC_ISO_646_BASIC                 0x00000042
#define ENC_BCD_DIGITS_0_9                0x00000044 /* Packed BCD, digits 0-9 */
#define ENC_KEYPAD_ABC_TBCD               0x00000046 /* Keypad-with-a/b/c "telephony BCD" = 0-9, *, #, a, b, c */
#define ENC_KEYPAD_BC_TBCD                0x00000048 /* Keypad-with-B/C "telephony BCD" = 0-9, B, C, *, # */
#define ENC_3GPP_TS_23_038_7BITS_UNPACKED 0x0000004C
#define ENC_ETSI_TS_102_221_ANNEX_A       0x0000004E /* ETSI TS 102 221 Annex A */
#define ENC_GB18030                       0x00000050
#define ENC_EUC_KR                        0x00000052
#define ENC_APN_STR                       0x00000054 /* The encoding the APN/DNN field follows 3GPP TS 23.003 [2] clause 9.1.*/
#define ENC_DECT_STANDARD_8BITS           0x00000056 /* DECT standard character set as defined in ETSI EN 300 175-5 Annex D */
#define ENC_DECT_STANDARD_4BITS_TBCD      0x00000058 /* DECT standard 4bits character set as defined in ETSI EN 300 175-5 Annex D (BCD with 0xb = SPACE)*/
#define ENC_EBCDIC_CP500                  0x00000060
#define ENC_ZIGBEE               0x40000000

#define ENC_BOM                  0x20000000

#define ENC_STR_NUM     0x01000000
#define ENC_STR_HEX     0x02000000
#define ENC_STRING      0x03000000
#define ENC_STR_MASK    0x0000FFFE

#define ENC_NUM_PREF    0x00200000

#define ENC_SEP_NONE    0x00010000
#define ENC_SEP_COLON   0x00020000
#define ENC_SEP_DASH    0x00040000
#define ENC_SEP_DOT     0x00080000
#define ENC_SEP_SPACE   0x00100000
#define ENC_SEP_MASK    0x001F0000

#define ENC_BCD_ODD_NUM_DIG     0x00010000
#define ENC_BCD_SKIP_FIRST      0x00020000

#define ENC_TIME_SECS_NSECS          0x00000000
#define ENC_TIME_TIMESPEC            0x00000000 /* for backwards source compatibility */
#define ENC_TIME_NTP                 0x00000002
#define ENC_TIME_TOD                 0x00000004
#define ENC_TIME_RTPS                0x00000008
#define ENC_TIME_NTP_BASE_ZERO       0x00000008 /* for backwards source compatibility */
#define ENC_TIME_SECS_USECS          0x00000010
#define ENC_TIME_TIMEVAL             0x00000010 /* for backwards source compatibility */
#define ENC_TIME_SECS                0x00000012
#define ENC_TIME_MSECS               0x00000014
#define ENC_TIME_SECS_NTP            0x00000018
#define ENC_TIME_RFC_3971            0x00000020
#define ENC_TIME_MSEC_NTP            0x00000022
#define ENC_TIME_MIP6                0x00000024
#define ENC_TIME_MP4_FILE_SECS       0x00000026
#define ENC_TIME_CLASSIC_MAC_OS_SECS 0x00000026 /* for backwards source compatibility */
#define ENC_TIME_NSECS               0x00000028
#define ENC_TIME_USECS               0x00000030
#define ENC_TIME_ZBEE_ZCL            0x00000032

#define ENC_ISO_8601_DATE             0x00010000
#define ENC_ISO_8601_TIME             0x00020000
#define ENC_ISO_8601_DATE_TIME        0x00030000
#define ENC_IMF_DATE_TIME             0x00040000 /* Internet Message Format - RFCs 822, 1123, 2822, 5322 */
#define ENC_RFC_822                   0x00040000 /* backwards compatibility */
#define ENC_RFC_1123                  0x00040000 /* backwards source compatibility - not binary */
#define ENC_ISO_8601_DATE_TIME_BASIC  0x00100000
#define ENC_STR_TIME_MASK             0x001F0000

#define ENC_VARINT_PROTOBUF      0x00000002
#define ENC_VARINT_QUIC          0x00000004
#define ENC_VARINT_ZIGZAG        0x00000008
#define ENC_VARINT_SDNV          0x00000010

#define ENC_VARINT_MASK          (ENC_VARINT_PROTOBUF|ENC_VARINT_QUIC|ENC_VARINT_ZIGZAG|ENC_VARINT_SDNV)

#define FIELD_DISPLAY_E_MASK 0xFF

typedef enum {
    BASE_NONE    = 0,   /**< none */

    BASE_DEC,           /**< decimal [integer, float] */
    BASE_HEX,           /**< hexadecimal [integer, float] */
    BASE_OCT,           /**< octal [integer] */
    BASE_DEC_HEX,       /**< decimal (hexadecimal) [integer] */
    BASE_HEX_DEC,       /**< hexadecimal (decimal) [integer] */
    BASE_CUSTOM,        /**< call custom routine to format [integer, float] */
    BASE_EXP,           /**< exponential [float] */

    SEP_DOT,            /**< hexadecimal bytes with a period (.) between each byte */
    SEP_DASH,           /**< hexadecimal bytes with a dash (-) between each byte */
    SEP_COLON,          /**< hexadecimal bytes with a colon (:) between each byte */
    SEP_SPACE,          /**< hexadecimal bytes with a space between each byte */

    BASE_NETMASK,       /**< Used for IPv4 address that shouldn't be resolved (like for netmasks) */

    BASE_PT_UDP,        /**< UDP port */
    BASE_PT_TCP,        /**< TCP port */
    BASE_PT_DCCP,       /**< DCCP port */
    BASE_PT_SCTP,       /**< SCTP port */

    BASE_OUI,           /**< OUI resolution */

    ABSOLUTE_TIME_LOCAL,        /**< local time in our time zone, with month and day */
    ABSOLUTE_TIME_UTC,          /**< UTC, with month and day */
    ABSOLUTE_TIME_DOY_UTC,      /**< UTC, with 1-origin day-of-year */
    ABSOLUTE_TIME_NTP_UTC,      /**< UTC, with "NULL" when timestamp is all zeros */
    ABSOLUTE_TIME_UNIX,         /**< Unix time */

    BASE_STR_WSP,       /**< Replace all whitespace characters (newline, formfeed, etc) with "space". */
} field_display_e;

#define FIELD_DISPLAY(d) ((d) & FIELD_DISPLAY_E_MASK)

#define FIELD_DISPLAY_IS_ABSOLUTE_TIME(d) \
        (FIELD_DISPLAY(d) >= ABSOLUTE_TIME_LOCAL && FIELD_DISPLAY(d) <= ABSOLUTE_TIME_UNIX)

#define BASE_RANGE_STRING         0x00000100  /**< Use the supplied range string to convert the field to text */
#define BASE_EXT_STRING           0x00000200
#define BASE_VAL64_STRING         0x00000400

#define BASE_ALLOW_ZERO           0x00000800  /**< Display `<none>` instead of `<MISSING>` for zero sized byte array */

#define BASE_UNIT_STRING          0x00001000  /**< Add unit text to the field value */

#define BASE_NO_DISPLAY_VALUE     0x00002000  /**< Just display the field name with no value.  Intended for
                                                   byte arrays or header fields above a subtree */

#define BASE_PROTOCOL_INFO        0x00004000  /**< protocol_t in [FIELDCONVERT].  Internal use only. */

#define BASE_SPECIAL_VALS         0x00008000  /**< field will not display "Unknown" if value_string match is not found */

#define BASE_SHOW_ASCII_PRINTABLE 0x00010000 /**< show byte array as ASCII if it's all printable characters */

#define BASE_SHOW_UTF_8_PRINTABLE 0x00020000 /**< show byte array as UTF-8 if it's all valid and printable UTF-8 characters */

#define IS_BASE_DUAL(b) ((b)==BASE_DEC_HEX||(b)==BASE_HEX_DEC)

#define IS_BASE_PORT(b) (((b)==BASE_PT_UDP||(b)==BASE_PT_TCP||(b)==BASE_PT_DCCP||(b)==BASE_PT_SCTP))

typedef enum {
    HF_REF_TYPE_NONE,       /**< Field is not referenced */
    HF_REF_TYPE_INDIRECT,   /**< Field is indirectly referenced (only applicable for FT_PROTOCOL) via. its child */
    HF_REF_TYPE_DIRECT,     /**< Field is directly referenced */
    HF_REF_TYPE_PRINT       /**< Field is directly referenced for printing (so don't fake its representation either) */
} hf_ref_type;

typedef struct _header_field_info header_field_info;

struct _header_field_info {
    /* ---------- set by dissector --------- */
    const char        *name;              /**< [FIELDNAME] full name of this field */
    const char        *abbrev;            /**< [FIELDFILTERNAME] filter name of this field */
    enum ftenum        type;              /**< [FIELDTYPE] field type, one of FT_ (from ftypes.h) */
    int                display;           /**< [FIELDDISPLAY] one of BASE_, or field bit-width if FT_BOOLEAN and non-zero bitmask */
    const void        *strings;           /**< [FIELDCONVERT] value_string, val64_string, range_string or true_false_string,
                                               typically converted by VALS(), RVALS() or TFS().
                                               If this is an FT_PROTOCOL or BASE_PROTOCOL_INFO then it points to the
                                               associated protocol_t structure */
    uint64_t           bitmask;           /**< [BITMASK] bitmask of interesting bits */
    const char        *blurb;             /**< [FIELDDESCR] Brief description of field */

    /* ------- set by proto routines (prefilled by HFILL macro, see below) ------ */
    int                id;                /**< Field ID */
    int                parent;            /**< parent protocol tree */
    hf_ref_type        ref_type;          /**< is this field referenced by a filter */
    int                same_name_prev_id; /**< ID of previous hfinfo with same abbrev */
    header_field_info *same_name_next;    /**< Link to next hfinfo with same abbrev */
};

#define HFILL -1, 0, HF_REF_TYPE_NONE, -1, NULL

#define HFILL_INIT(hf)   \
    (hf).hfinfo.id                = -1;   \
    (hf).hfinfo.parent            = 0;   \
    (hf).hfinfo.ref_type          = HF_REF_TYPE_NONE;   \
    (hf).hfinfo.same_name_prev_id = -1;   \
    (hf).hfinfo.same_name_next    = NULL;

typedef struct hf_register_info {
    int               *p_id;   /**< written to by register() function */
    header_field_info  hfinfo; /**< the field info to be registered */
} hf_register_info;

typedef struct _item_label_t {
    char representation[ITEM_LABEL_LENGTH];
} item_label_t;

typedef struct field_info {
    const header_field_info *hfinfo;      /**< pointer to registered field information */
    int                  start;           /**< current start of data in field_info.ds_tvb */
    int                  length;          /**< current data length of item in field_info.ds_tvb */
    int                  appendix_start;  /**< start of appendix data */
    int                  appendix_length; /**< length of appendix data */
    int                  tree_type;       /**< one of ETT_ or -1 */
    uint32_t             flags;           /**< bitfield like FI_GENERATED, ... */
    item_label_t        *rep;             /**< string for GUI tree */
    tvbuff_t            *ds_tvb;          /**< data source tvbuff */
    fvalue_t            *value;
    int                 total_layer_num;        /**< Hierarchical layer number, for all protocols in the tree. */
    int                 proto_layer_num;        /**< Protocol layer number, so 1st, 2nd, 3rd, ... for protocol X. */
} field_info;


typedef struct
{
    unsigned  crumb_bit_offset;
    uint8_t crumb_bit_length;
} crumb_spec_t;

#define FI_HIDDEN               0x00000001
#define FI_GENERATED            0x00000002
#define FI_URL                  0x00000004

#define FI_LITTLE_ENDIAN        0x00000008
#define FI_BIG_ENDIAN           0x00000010
#define FI_BITS_OFFSET(n)       (((n) & 7) << 5)
#define FI_BITS_SIZE(n)         (((n) & 63) << 8)
#define FI_VARINT               0x00004000

#define FI_GET_FLAG(fi, flag)   ((fi) ? ((fi)->flags & (flag)) : 0)
#define FI_SET_FLAG(fi, flag) \
    do { \
      if (fi) \
        (fi)->flags = (fi)->flags | (flag); \
    } while(0)
#define FI_RESET_FLAG(fi, flag) \
    do { \
      if (fi) \
        (fi)->flags = (fi)->flags & ~(flag); \
    } while(0)

#define FI_GET_BITS_OFFSET(fi) (FI_GET_FLAG(fi, FI_BITS_OFFSET(7)) >> 5)
#define FI_GET_BITS_SIZE(fi)   (FI_GET_FLAG(fi, FI_BITS_SIZE(63)) >> 8)

typedef struct {
    GHashTable          *interesting_hfids;
    bool                 visible;
    bool                 fake_protocols;
    unsigned             count;
    struct _packet_info *pinfo;
} tree_data_t;

typedef struct _proto_node {
    struct _proto_node *first_child;
    struct _proto_node *last_child;
    struct _proto_node *next;
    struct _proto_node *parent;
    field_info         *finfo;
    tree_data_t        *tree_data;
} proto_node;

typedef proto_node proto_tree;
typedef proto_node proto_item;

#define PI_SEVERITY_MASK        0x00F00000  /**< mask usually for internal use only! */
#define PI_COMMENT              0x00100000
#define PI_CHAT                 0x00200000
#define PI_NOTE                 0x00400000
#define PI_WARN                 0x00600000
#define PI_ERROR                0x00800000

#define PI_GROUP_MASK           0xFF000000  /**< mask usually for internal use only! */
#define PI_CHECKSUM             0x01000000
#define PI_SEQUENCE             0x02000000
#define PI_RESPONSE_CODE        0x03000000
#define PI_REQUEST_CODE         0x04000000
#define PI_UNDECODED            0x05000000
#define PI_REASSEMBLE           0x06000000
#define PI_MALFORMED            0x07000000
#define PI_DEBUG                0x08000000
#define PI_PROTOCOL             0x09000000
#define PI_SECURITY             0x0a000000
#define PI_COMMENTS_GROUP       0x0b000000
#define PI_DECRYPTION           0x0c000000
#define PI_ASSUMPTION           0x0d000000
#define PI_DEPRECATED           0x0e000000
#define PI_RECEIVE              0x0f000000
#define PI_INTERFACE            0x10000000
#define PI_DISSECTOR_BUG        0x11000000

#define PNODE_FINFO(proto_node)  ((proto_node)->finfo)

#define PITEM_FINFO(proto_item)  PNODE_FINFO(proto_item)

#define PTREE_FINFO(proto_tree)  PNODE_FINFO(proto_tree)

#define PTREE_DATA(proto_tree)   ((proto_tree)->tree_data)

#define PNODE_POOL(proto_node)   ((proto_node)->tree_data->pinfo->pool)

static inline bool proto_item_is_hidden(proto_item *ti) {
    if (ti) {
        return FI_GET_FLAG(PITEM_FINFO(ti), FI_HIDDEN);
    }
    return false;
}
#define PROTO_ITEM_IS_HIDDEN(ti) proto_item_is_hidden((ti))

static inline void proto_item_set_hidden(proto_item *ti) {
    if (ti) {
        FI_SET_FLAG(PITEM_FINFO(ti), FI_HIDDEN);
    }
}
#define PROTO_ITEM_SET_HIDDEN(ti) proto_item_set_hidden((ti))

static inline void proto_item_set_visible(proto_item *ti) {
    if (ti) {
        FI_RESET_FLAG(PITEM_FINFO(ti), FI_HIDDEN);
    }
}
#define PROTO_ITEM_SET_VISIBLE(ti) proto_item_set_visible((ti))

static inline bool proto_item_is_generated(proto_item *ti) {
    if (ti) {
        return FI_GET_FLAG(PITEM_FINFO(ti), FI_GENERATED);
    }
    return false;
}
#define PROTO_ITEM_IS_GENERATED(ti) proto_item_is_generated((ti))

static inline void proto_item_set_generated(proto_item *ti) {
    if (ti) {
        FI_SET_FLAG(PITEM_FINFO(ti), FI_GENERATED);
    }
}
#define PROTO_ITEM_SET_GENERATED(ti) proto_item_set_generated((ti))

static inline bool proto_item_is_url(proto_item *ti) {
    if (ti) {
        return FI_GET_FLAG(PITEM_FINFO(ti), FI_URL);
    }
    return false;
}
#define PROTO_ITEM_IS_URL(ti) proto_item_is_url((ti))

static inline void proto_item_set_url(proto_item *ti) {
    if (ti) {
        FI_SET_FLAG(PITEM_FINFO(ti), FI_URL);
    }
}
#define PROTO_ITEM_SET_URL(ti) proto_item_set_url((ti))

typedef void (*proto_tree_foreach_func)(proto_node *, void *);
typedef bool (*proto_tree_traverse_func)(proto_node *, void *);

 void proto_tree_children_foreach(proto_tree *tree,
    proto_tree_foreach_func func, void *data);

typedef struct {
    void (*register_protoinfo)(void);   /* routine to call to register protocol information */
    void (*register_handoff)(void);     /* routine to call to register dissector handoff */
} proto_plugin;

 void proto_register_plugin(const proto_plugin *plugin);

void proto_init(GSList *register_all_plugin_protocols_list,
    GSList *register_all_plugin_handoffs_list, register_cb cb, void *client_data);

extern void proto_cleanup(void);

 bool proto_field_is_referenced(proto_tree *tree, int proto_id);

 proto_tree* proto_item_add_subtree(proto_item *pi, const int idx) G_GNUC_WARN_UNUSED_RESULT;

 proto_tree* proto_item_get_subtree(proto_item *pi);

 proto_item* proto_item_get_parent(const proto_item *pi);

 proto_item* proto_item_get_parent_nth(proto_item *pi, int gen);

 void proto_item_set_text(proto_item *pi, const char *format, ...)
    G_GNUC_PRINTF(2,3);

 void proto_item_append_text(proto_item *pi, const char *format, ...)
    G_GNUC_PRINTF(2,3);

 void proto_item_prepend_text(proto_item *pi, const char *format, ...)
    G_GNUC_PRINTF(2,3);

 void proto_item_set_len(proto_item *pi, const int length);

 void proto_item_set_end(proto_item *pi, tvbuff_t *tvb, int end);

 int proto_item_get_len(const proto_item *pi);

 void proto_item_set_bits_offset_len(proto_item *ti, int bits_offset, int bits_len);

 char *proto_item_get_display_repr(wmem_allocator_t *scope, proto_item *pi);

extern proto_tree* proto_tree_create_root(struct _packet_info *pinfo);

void proto_tree_reset(proto_tree *tree);

 void proto_tree_free(proto_tree *tree);

 bool proto_tree_set_visible(proto_tree *tree, bool visible);

extern void proto_tree_set_fake_protocols(proto_tree *tree, bool fake_protocols);

extern void proto_tree_prime_with_hfid(proto_tree *tree, const int hfid);

extern void proto_tree_prime_with_hfid_print(proto_tree *tree, const int hfid);

/** Get a parent item of a subtree.
 @param tree the tree to get the parent from
 @return parent item */
 proto_item* proto_tree_get_parent(proto_tree *tree);

/** Get the parent tree of a subtree.
 @param tree the tree to get the parent from
 @return parent tree */
 proto_tree *proto_tree_get_parent_tree(proto_tree *tree);

/** Get the root tree from any subtree.
 @param tree the tree to get the root from
 @return root tree */
 proto_tree* proto_tree_get_root(proto_tree *tree);

/** Move an existing item behind another existing item.
 @param tree the tree to which both items belong
 @param fixed_item the item which keeps its position
 @param item_to_move the item which will be moved */
 void proto_tree_move_item(proto_tree *tree, proto_item *fixed_item, proto_item *item_to_move);


/** Set start and length of an appendix for a proto_tree.
 @param tree the tree to set the appendix start and length
 @param tvb the tv buffer of the current data
 @param start the start offset of the appendix
 @param length the length of the appendix */
 void proto_tree_set_appendix(proto_tree *tree, tvbuff_t *tvb, int start, const int length);


/** Add an item to a proto_tree, using the text label registered to that item.
   The item is extracted from the tvbuff handed to it.
 @param tree the tree to append this item to
 @param hfinfo field
 @param tvb the tv buffer of the current data
 @param start start of data in tvb
 @param length length of data in tvb
 @param encoding data encoding
 @return the newly created item */
 proto_item *
proto_tree_add_item_new(proto_tree *tree, header_field_info *hfinfo, tvbuff_t *tvb,
    const int start, int length, const unsigned encoding);

 proto_item *
proto_tree_add_item(proto_tree *tree, int hfindex, tvbuff_t *tvb,
    const int start, int length, const unsigned encoding);

/** Add an item to a proto_tree, using the text label registered to that item.
   The item is extracted from the tvbuff handed to it.

   Return the length of the item through the pointer.
 @param tree the tree to append this item to
 @param hfinfo field
 @param tvb the tv buffer of the current data
 @param start start of data in tvb
 @param length length of data in tvb
 @param encoding data encoding
 @param[out] lenretval points to a int that will be set to the item length
 @return the newly created item, and *lenretval is set to the item length */
 proto_item *
proto_tree_add_item_new_ret_length(proto_tree *tree, header_field_info *hfinfo, tvbuff_t *tvb,
    const int start, int length, const unsigned encoding, int *lenretval);

 proto_item *
proto_tree_add_item_ret_length(proto_tree *tree, int hfindex, tvbuff_t *tvb,
    const int start, int length, const unsigned encoding, int *lenretval);

/** Add an integer data item to a proto_tree, using the text label registered to that item.
The item is extracted from the tvbuff handed to it, and the retrieved
value is also set to *retval so the caller gets it back for other uses.

This function retrieves the value even if the passed-in tree param is NULL,
so that it can be used by dissectors at all times to both get the value
and set the tree item to it.

Like other proto_tree_add functions, if there is a tree and the value cannot
be decoded from the tvbuff, then an expert info error is reported.

This function accepts ENC_LITTLE_ENDIAN and ENC_BIG_ENDIAN for native number
encoding in the tvbuff

The length argument must
be set to the appropriate size of the native type as in other proto_add routines.

Integers of 8, 16, 24 and 32 bits can be retrieved with the _ret_int and
ret_uint functions; integers of 40, 48, 56, and 64 bits can be retrieved
with the _ret_uint64 function; Boolean values of 8, 16, 24, 32, 40, 48,
56, and 64 bits can be retrieved with the _ret_boolean function.

@param tree the tree to append this item to
@param hfindex field
@param tvb the tv buffer of the current data
@param start start of data in tvb (cannot be negative)
@param length length of data in tvb (for strings can be -1 for remaining)
@param encoding data encoding (e.g, ENC_LITTLE_ENDIAN, ENC_BIG_ENDIAN, ENC_ASCII|ENC_STRING, etc.)
@param[out] retval points to a int32_t or uint32_t which will be set to the value
@return the newly created item, and *retval is set to the decoded value masked/shifted according to bitmask
*/
 proto_item *
proto_tree_add_item_ret_int(proto_tree *tree, int hfindex, tvbuff_t *tvb,
    const int start, int length, const unsigned encoding, int32_t *retval);

 proto_item *
proto_tree_add_item_ret_int64(proto_tree *tree, int hfindex, tvbuff_t *tvb,
    const int start, int length, const unsigned encoding, int64_t *retval);

 proto_item *
proto_tree_add_item_ret_uint(proto_tree *tree, int hfindex, tvbuff_t *tvb,
    const int start, int length, const unsigned encoding, uint32_t *retval);

 proto_item *
proto_tree_add_item_ret_uint64(proto_tree *tree, int hfindex, tvbuff_t *tvb,
    const int start, int length, const unsigned encoding, uint64_t *retval);

 proto_item *
proto_tree_add_item_ret_varint(proto_tree *tree, int hfindex, tvbuff_t *tvb,
    const int start, int length, const unsigned encoding, uint64_t *retval, int *lenretval);

 proto_item *
proto_tree_add_item_ret_boolean(proto_tree *tree, int hfindex, tvbuff_t *tvb,
    const int start, int length, const unsigned encoding, bool *retval);

 proto_item *
proto_tree_add_item_ret_ipv4(proto_tree *tree, int hfindex, tvbuff_t *tvb,
    const int start, int length, const unsigned encoding, ws_in4_addr *retval);

/**
 * @brief Parse an ipv6 address from the buffer and add it to the tree,
 * writing the value to the pointer specified by the caller. The pointer
 * must not be null.
 *
 * @param tree the tree
 * @param hfindex the field
 * @param tvb the tv buffer
 * @param start the start index of data in tvb
 * @param length the length of data. calls REPORT_DISSECTOR_BUG if not equal to FT_IPv6_LEN
 * @param encoding encodings not yet supported. calls REPORT_DISSECTOR_BUG if not equal to 0
 * @param retval where the address should be written, must not be null
 * @return the newly created item
 */
 proto_item *
proto_tree_add_item_ret_ipv6(proto_tree *tree, int hfindex, tvbuff_t *tvb,
    const int start, int length, const unsigned encoding, ws_in6_addr *retval);

/**
 * @brief Parse an ethernet address from the buffer and add it to the tree,
 * writing the value to the pointer specified by the caller. The pointer
 * must not be null.
 *
 * @param tree the tree
 * @param hfindex the field
 * @param tvb the tv buffer
 * @param start the start index of data in tvb
 * @param length the length of data. calls REPORT_DISSECTOR_BUG if not equal to FT_ETHER_LEN
 * @param encoding encodings not yet supported. calls REPORT_DISSECTOR_BUG if not equal to 0
 * @param retval a buffer of at least FT_ETHER_LEN bytes for the address, must not be null
 * @return the newly created item
 */
 proto_item *
proto_tree_add_item_ret_ether(proto_tree *tree, int hfindex, tvbuff_t *tvb,
    const int start, int length, const unsigned encoding, uint8_t *retval);

/**
 * @brief Parse a float from the buffer and add it to the tree,
 * returning the item added and the parsed value via retval.
 *
 * @param tree the tree
 * @param hfindex the field
 * @param tvb the tv buffer
 * @param start start index of data in tvb
 * @param length the length of data. calls REPORT_DISSECTOR_BUG if not equal to 4
 * @param encoding ENC_LITTLE_ENDIAN or ENC_BIG_ENDIAN
 * @param[out] retval for the decoded value
 * @return the newly created item
 */
 proto_item *
proto_tree_add_item_ret_float(proto_tree *tree, int hfindex, tvbuff_t *tvb,
                                const int start, int length,
                                const unsigned encoding, float *retval);

/**
 * @brief Parse a double from the buffer and add it to the tree,
 * returning the item added and the parsed value via retval
 *
 * @param tree the tree
 * @param hfindex the field
 * @param tvb the tv buffer
 * @param start start index of data in tvb
 * @param length length of data. calls REPORT_DISSECTOR_BUG if not equal to 8
 * @param encoding ENC_LITTLE_ENDIAN or ENC_BIG_ENDIAN
 * @param[out] retval for the decoded value
 * @return the newly created item and retval is set to the decoded value
 */
 proto_item *
proto_tree_add_item_ret_double(proto_tree *tree, int hfindex, tvbuff_t *tvb,
                                const int start, int length,
                                const unsigned encoding, double *retval);

/** Add an string item to a proto_tree, using the text label registered to
that item.

The item is extracted from the tvbuff handed to it, and the retrieved
value and its length are returned through pointers so the caller can use
them.  The value is allocated using the wmem scope passed in.

This function retrieves the value and length even if the passed-in tree
param is NULL, so that then can be used by dissectors at all times to
both get the value and set the tree item to it.

Like other proto_tree_add functions, if there is a tree and the value cannot
be decoded from the tvbuff, then an expert info error is reported.

This function accepts string encodings.

@param scope the wmem scope to use to allocate the string
@param tree the tree to append this item to
@param hfindex field
@param tvb the tv buffer of the current data
@param start start of data in tvb (cannot be negative)
@param length length of data in tvb (for strings can be -1 for remaining)
@param encoding data encoding (e.g, ENC_ASCII, ENC_UTF_8, etc.)
@param[out] retval points to a uint8_t * that will be set to point to the
string value
@param[out] lenretval points to a int that will be set to the item length
@return the newly created item, *retval is set to the decoded value,
and *lenretval is set to the item length
*/
 proto_item *
proto_tree_add_item_ret_string_and_length(proto_tree *tree, int hfindex,
    tvbuff_t *tvb, const int start, int length, const unsigned encoding,
    wmem_allocator_t *scope, const uint8_t **retval, int *lenretval);

/** Add an string item to a proto_tree, using the text label registered to
that item.

The item is extracted from the tvbuff handed to it, and the retrieved
value is returned through a pointer so the caller can use it.  The value
is allocated using the wmem scope passed in.

This function retrieves the value even if the passed-in tree param is NULL,
so that it can be used by dissectors at all times to both get the value
and set the tree item to it.

Like other proto_tree_add functions, if there is a tree and the value cannot
be decoded from the tvbuff, then an expert info error is reported.

This function accepts string encodings.

@param scope the wmem scope to use to allocate the string
@param tree the tree to append this item to
@param hfindex field
@param tvb the tv buffer of the current data
@param start start of data in tvb (cannot be negative)
@param length length of data in tvb (for strings can be -1 for remaining)
@param encoding data encoding (e.g, ENC_ASCII, ENC_UTF_8, etc.)
@param[out] retval points to a uint8_t * that will be set to point to the
string value
@return the newly created item, and *retval is set to the decoded value
*/
 proto_item *
proto_tree_add_item_ret_string(proto_tree *tree, int hfindex, tvbuff_t *tvb,
    const int start, int length, const unsigned encoding,
    wmem_allocator_t *scope, const uint8_t **retval);

/** Add an string or byte array item to a proto_tree, using the
text label registered to that item.

This provides a string that is a display representation of the value,
and the length of the item, similar to what
proto_tree_add_item_ret_string_and_length() does.

@param scope the wmem scope to use to allocate the string
@param tree the tree to append this item to
@param hfindex field
@param tvb the tv buffer of the current data
@param start start of data in tvb (cannot be negative)
@param length length of data in tvb (for strings can be -1 for remaining)
@param encoding data encoding (e.g, ENC_ASCII, ENC_UTF_8, etc.)
@param[out] retval points to a uint8_t * that will be set to point to the
string value
@param[out] lenretval points to a int that will be set to the item length
@return the newly created item, *retval is set to the display string,
and *lenretval is set to the item length
*/
 proto_item *
proto_tree_add_item_ret_display_string_and_length(proto_tree *tree, int hfindex,
    tvbuff_t *tvb,
    const int start, int length, const unsigned encoding,
    wmem_allocator_t *scope, char **retval, int *lenretval);

/** Add an string or byte array item to a proto_tree, using the
text label registered to that item.

This provides a string that is a display representation of the value,
similar to what proto_tree_add_item_ret_string() does.

@param tree the tree to append this item to
@param hfindex field
@param tvb the tv buffer of the current data
@param start start of data in tvb (cannot be negative)
@param length length of data in tvb (for strings can be -1 for remaining)
@param encoding data encoding (e.g, ENC_ASCII, ENC_UTF_8, etc.)
@param scope the wmem scope to use to allocate the string
@param[out] retval points to a uint8_t * that will be set to point to the
string value
@return the newly created item, *retval is set to the display string
*/
 proto_item *
proto_tree_add_item_ret_display_string(proto_tree *tree, int hfindex,
    tvbuff_t *tvb,
    const int start, int length, const unsigned encoding,
    wmem_allocator_t *scope, char **retval);

/** Add a time item to a proto_tree, using thetext label registered to that item.

This provides a string that is a display representation of the time value

@param tree the tree to append this item to
@param hfindex field
@param tvb the tv buffer of the current data
@param start start of data in tvb (cannot be negative)
@param length length of data in tvb (for strings can be -1 for remaining)
@param encoding data encoding (e.g, ENC_ASCII, ENC_UTF_8, etc.)
@param scope the wmem scope to use to allocate the string
@param[out] retval points to a uint8_t * that will be set to point to the
string value
@return the newly created item, *retval is set to the display string
*/
 proto_item *
proto_tree_add_item_ret_time_string(proto_tree *tree, int hfindex,
	tvbuff_t *tvb,
	const int start, int length, const unsigned encoding,
	wmem_allocator_t *scope, char **retval);

/** (INTERNAL USE ONLY) Add a text-only node to a proto_tree.
 @param tree the tree to append this item to
 @param tvb the tv buffer of the current data
 @param start start of data in tvb
 @param length length of data in tvb
 @param format printf like format string
 @param ... printf like parameters
 @return the newly created item */
proto_item *
proto_tree_add_text_internal(proto_tree *tree, tvbuff_t *tvb, int start, int length, const char *format,
    ...) G_GNUC_PRINTF(5,6);

/** (INTERNAL USE ONLY) Add a text-only node to a proto_tree using a variable argument list.
 @param tree the tree to append this item to
 @param tvb the tv buffer of the current data
 @param start start of data in tvb
 @param length length of data in tvb
 @param format printf like format string
 @param ap variable argument list
 @return the newly created item */
proto_item *
proto_tree_add_text_valist_internal(proto_tree *tree, tvbuff_t *tvb, int start,
    int length, const char *format, va_list ap) G_GNUC_PRINTF(5, 0);

/** Add a text-only node that creates a subtree underneath.
 @param tree the tree to append this item to
 @param tvb the tv buffer of the current data
 @param start start of data in tvb
 @param length length of data in tvb
 @param idx one of the ett_ array elements registered with proto_register_subtree_array()
 @param tree_item item returned with tree creation. Can be NULL if going to be unused
 @param text label for the tree
 @return the newly created tree */
 proto_tree *
proto_tree_add_subtree(proto_tree *tree, tvbuff_t *tvb, int start, int length, int idx,
    proto_item **tree_item, const char *text);

/** Add a text-only node that creates a subtree underneath.
 @param tree the tree to append this item to
 @param tvb the tv buffer of the current data
 @param start start of data in tvb
 @param length length of data in tvb
 @param idx one of the ett_ array elements registered with proto_register_subtree_array()
 @param tree_item item returned with tree creation. Can be NULL if going to be unused
 @param format printf like format string
 @param ... printf like parameters
 @return the newly created tree */
 proto_tree *
proto_tree_add_subtree_format(proto_tree *tree, tvbuff_t *tvb, int start, int length, int idx,
    proto_item **tree_item, const char *format, ...) G_GNUC_PRINTF(7,8);

/** Add a text-only node to a proto_tree with tvb_format_text() string. */
proto_item *
proto_tree_add_format_text(proto_tree *tree, tvbuff_t *tvb, int start, int length);

/** Add a text-only node to a proto_tree with tvb_format_text_wsp() string. */
proto_item *
proto_tree_add_format_wsp_text(proto_tree *tree, tvbuff_t *tvb, int start, int length);

/** Add a FT_NONE field to a proto_tree.
 @param tree the tree to append this item to
 @param hfindex field index
 @param tvb the tv buffer of the current data
 @param start start of data in tvb
 @param length length of data in tvb
 @param format printf like format string
 @param ... printf like parameters
 @return the newly created item */
 proto_item *
proto_tree_add_none_format(proto_tree *tree, const int hfindex, tvbuff_t *tvb, const int start,
    int length, const char *format, ...) G_GNUC_PRINTF(6,7);

/** Add a FT_PROTOCOL to a proto_tree.
 @param tree the tree to append this item to
 @param hfindex field index
 @param tvb the tv buffer of the current data
 @param start start of data in tvb
 @param length length of data in tvb
 @param format printf like format string
 @param ... printf like parameters
 @return the newly created item */
 proto_item *
proto_tree_add_protocol_format(proto_tree *tree, int hfindex, tvbuff_t *tvb, int start,
    int length, const char *format, ...) G_GNUC_PRINTF(6,7);

/** Add a FT_BYTES to a proto_tree.
 @param tree the tree to append this item to
 @param hfindex field index
 @param tvb the tv buffer of the current data
 @param start start of data in tvb
 @param length length of data in tvb
 @param start_ptr pointer to the data to display
 @return the newly created item */
 proto_item *
proto_tree_add_bytes(proto_tree *tree, int hfindex, tvbuff_t *tvb, int start,
    int length, const uint8_t* start_ptr);

/** Add a FT_BYTES to a proto_tree like proto_tree_add_bytes,
 but used when the tvb data length does not match the bytes length.
 @param tree the tree to append this item to
 @param hfindex field index
 @param tvb the tv buffer of the current data
 @param start start of data in tvb
 @param length length of data in tvb
 @param start_ptr pointer to the data to display
 @param ptr_length length of data in start_ptr
 @return the newly created item */
 proto_item *
proto_tree_add_bytes_with_length(proto_tree *tree, int hfindex, tvbuff_t *tvb, int start,
    int length, const uint8_t *start_ptr, int ptr_length);

/** Get and add a byte-array-based FT_* to a proto_tree.

 Supported: FT_BYTES, FT_UINT_BYTES, FT_OID, FT_REL_OID, and FT_SYSTEM_ID.

 The item is extracted from the tvbuff handed to it, based on the ENC_* passed
 in for the encoding, and the retrieved byte array is also set to *retval so the
 caller gets it back for other uses.

 This function retrieves the value even if the passed-in tree param is NULL,
 so that it can be used by dissectors at all times to both get the value
 and set the tree item to it.

 Like other proto_tree_add functions, if there is a tree and the value cannot
 be decoded from the tvbuff, then an expert info error is reported. For string
 encoding, this means that a failure to decode the hex value from the string
 results in an expert info error being added to the tree.

 If encoding is string-based, it will convert using tvb_get_string_bytes(); see
 that function's comments for details.

 @note The GByteArray retval must be pre-constructed using g_byte_array_new().

 @param tree the tree to append this item to
 @param hfindex field index
 @param tvb the tv buffer of the current data
 @param start start of data in tvb
 @param length length of data in tvb
 @param encoding data encoding (e.g, ENC_LITTLE_ENDIAN, or ENC_UTF_8|ENC_STR_HEX)
 @param[in,out] retval points to a GByteArray which will be set to the bytes from the Tvb.
 @param[in,out] endoff if not NULL, gets set to the character after those consumed.
 @param[in,out] err if not NULL, gets set to 0 if no failure, else the errno code (e.g., EINVAL).
 @return the newly created item, and retval is set to the decoded value
 */
 proto_item *
proto_tree_add_bytes_item(proto_tree *tree, int hfindex, tvbuff_t *tvb,
    const int start, int length, const unsigned encoding,
    GByteArray *retval, int *endoff, int *err);

/** Add a formatted FT_BYTES to a proto_tree, with the format generating
    the string for the value and with the field name being included
    automatically.
 @param tree the tree to append this item to
 @param hfindex field index
 @param tvb the tv buffer of the current data
 @param start start of data in tvb
 @param length length of data in tvb
 @param start_ptr pointer to the data to display
 @param format printf like format string
 @param ... printf like parameters
 @return the newly created item */
 proto_item *
proto_tree_add_bytes_format_value(proto_tree *tree, int hfindex, tvbuff_t *tvb,
    int start, int length, const uint8_t* start_ptr, const char *format,
    ...) G_GNUC_PRINTF(7,8);

/** Add a formatted FT_BYTES to a proto_tree, with the format generating
    the entire string for the entry, including any field name.
 @param tree the tree to append this item to
 @param hfindex field index
 @param tvb the tv buffer of the current data
 @param start start of data in tvb
 @param length length of data in tvb
 @param start_ptr pointer to the data to display
 @param format printf like format string
 @param ... printf like parameters
 @return the newly created item */
 proto_item *
proto_tree_add_bytes_format(proto_tree *tree, int hfindex, tvbuff_t *tvb, int start,
    int length, const uint8_t* start_ptr, const char *format, ...) G_GNUC_PRINTF(7,8);

/** Add a FT_ABSOLUTE_TIME or FT_RELATIVE_TIME to a proto_tree.
 @param tree the tree to append this item to
 @param hfindex field index
 @param tvb the tv buffer of the current data
 @param start start of data in tvb
 @param length length of data in tvb
 @param value_ptr pointer to the data to display
 @return the newly created item */
 proto_item *
proto_tree_add_time(proto_tree *tree, int hfindex, tvbuff_t *tvb, int start,
    int length, const nstime_t* value_ptr);

/** Get and add a FT_ABSOLUTE_TIME or FT_RELATIVE_TIME to a proto_tree.
 The item is extracted from the tvbuff handed to it, based on the ENC_* passed
 in for the encoding, and the retrieved value is also set to *retval so the
 caller gets it back for other uses.

 This function retrieves the value even if the passed-in tree param is NULL,
 so that it can be used by dissectors at all times to both get the value
 and set the tree item to it.

 Like other proto_tree_add functions, if there is a tree and the value cannot
 be decoded from the tvbuff, then an expert info error is reported. For string
 encoding, this means that a failure to decode the time value from the string
 results in an expert info error being added to the tree.

 If encoding is string-based, it will convert using tvb_get_string_time(); see
 that function's comments for details.

 @note The nstime_t *retval must be pre-allocated as a nstime_t.

 @param tree the tree to append this item to
 @param hfindex field index
 @param tvb the tv buffer of the current data
 @param start start of data in tvb
 @param length length of data in tvb
 @param encoding data encoding (e.g, ENC_LITTLE_ENDIAN, ENC_UTF_8|ENC_ISO_8601_DATE_TIME, etc.)
 @param[in,out] retval points to a nstime_t which will be set to the value
 @param[in,out] endoff if not NULL, gets set to the character after those consumed.
 @param[in,out] err if not NULL, gets set to 0 if no failure, else EINVAL.
 @return the newly created item, and retval is set to the decoded value
 */
 proto_item *
proto_tree_add_time_item(proto_tree *tree, int hfindex, tvbuff_t *tvb,
    const int start, int length, const unsigned encoding,
    nstime_t *retval, int *endoff, int *err);


/** Add a formatted FT_ABSOLUTE_TIME or FT_RELATIVE_TIME to a proto_tree, with
    the format generating the string for the value and with the field name
    being included automatically.
 @param tree the tree to append this item to
 @param hfindex field index
 @param tvb the tv buffer of the current data
 @param start start of data in tvb
 @param length length of data in tvb
 @param value_ptr pointer to the data to display
 @param format printf like format string
 @param ... printf like parameters
 @return the newly created item */
 proto_item *
proto_tree_add_time_format_value(proto_tree *tree, int hfindex, tvbuff_t *tvb,
    int start, int length, nstime_t* value_ptr, const char *format, ...)
    G_GNUC_PRINTF(7,8);

/** Add a formatted FT_ABSOLUTE_TIME or FT_RELATIVE_TIME to a proto_tree, with
    the format generating the entire string for the entry, including any field
    name.
 @param tree the tree to append this item to
 @param hfindex field index
 @param tvb the tv buffer of the current data
 @param start start of data in tvb
 @param length length of data in tvb
 @param value_ptr pointer to the data to display
 @param format printf like format string
 @param ... printf like parameters
 @return the newly created item */
 proto_item *
proto_tree_add_time_format(proto_tree *tree, int hfindex, tvbuff_t *tvb, int start,
    int length, nstime_t* value_ptr, const char *format, ...) G_GNUC_PRINTF(7,8);

/** Add a FT_IPXNET to a proto_tree.
 @param tree the tree to append this item to
 @param hfindex field index
 @param tvb the tv buffer of the current data
 @param start start of data in tvb
 @param length length of data in tvb
 @param value data to display
 @return the newly created item */
 proto_item *
proto_tree_add_ipxnet(proto_tree *tree, int hfindex, tvbuff_t *tvb, int start,
    int length, uint32_t value);

/** Add a formatted FT_IPXNET to a proto_tree, with the format generating
    the string for the value and with the field name being included
    automatically.
 @param tree the tree to append this item to
 @param hfindex field index
 @param tvb the tv buffer of the current data
 @param start start of data in tvb
 @param length length of data in tvb
 @param value data to display
 @param format printf like format string
 @param ... printf like parameters
 @return the newly created item */
 proto_item *
proto_tree_add_ipxnet_format_value(proto_tree *tree, int hfindex, tvbuff_t *tvb,
    int start, int length, uint32_t value, const char *format, ...)
    G_GNUC_PRINTF(7,8);

/** Add a formatted FT_IPXNET to a proto_tree, with the format generating
    the entire string for the entry, including any field name.
 @param tree the tree to append this item to
 @param hfindex field index
 @param tvb the tv buffer of the current data
 @param start start of data in tvb
 @param length length of data in tvb
 @param value data to display
 @param format printf like format string
 @param ... printf like parameters
 @return the newly created item */
 proto_item *
proto_tree_add_ipxnet_format(proto_tree *tree, int hfindex, tvbuff_t *tvb, int start,
    int length, uint32_t value, const char *format, ...) G_GNUC_PRINTF(7,8);

/** Add a FT_IPv4 to a proto_tree.
 @param tree the tree to append this item to
 @param hfindex field index
 @param tvb the tv buffer of the current data
 @param start start of data in tvb
 @param length length of data in tvb
 @param value data to display
 @return the newly created item */
 proto_item *
proto_tree_add_ipv4(proto_tree *tree, int hfindex, tvbuff_t *tvb, int start,
    int length, ws_in4_addr value);

/** Add a formatted FT_IPv4 to a proto_tree, with the format generating
    the string for the value and with the field name being included
    automatically.
 @param tree the tree to append this item to
 @param hfindex field index
 @param tvb the tv buffer of the current data
 @param start start of data in tvb
 @param length length of data in tvb
 @param value data to display
 @param format printf like format string
 @param ... printf like parameters
 @return the newly created item */
 proto_item *
proto_tree_add_ipv4_format_value(proto_tree *tree, int hfindex, tvbuff_t *tvb,
    int start, int length, ws_in4_addr value, const char *format, ...)
    G_GNUC_PRINTF(7,8);

/** Add a formatted FT_IPv4 to a proto_tree, with the format generating
    the entire string for the entry, including any field name.
 @param tree the tree to append this item to
 @param hfindex field index
 @param tvb the tv buffer of the current data
 @param start start of data in tvb
 @param length length of data in tvb
 @param value data to display
 @param format printf like format string
 @param ... printf like parameters
 @return the newly created item */
 proto_item *
proto_tree_add_ipv4_format(proto_tree *tree, int hfindex, tvbuff_t *tvb, int start,
    int length, ws_in4_addr value, const char *format, ...) G_GNUC_PRINTF(7,8);

/** Add a FT_IPv6 to a proto_tree.
 @param tree the tree to append this item to
 @param hfindex field index
 @param tvb the tv buffer of the current data
 @param start start of data in tvb
 @param length length of data in tvb
 @param value_ptr data to display
 @return the newly created item */
 proto_item *
proto_tree_add_ipv6(proto_tree *tree, int hfindex, tvbuff_t *tvb, int start,
    int length, const ws_in6_addr *value_ptr);

/** Add a formatted FT_IPv6 to a proto_tree, with the format generating
    the string for the value and with the field name being included
    automatically.
 @param tree the tree to append this item to
 @param hfindex field index
 @param tvb the tv buffer of the current data
 @param start start of data in tvb
 @param length length of data in tvb
 @param value_ptr data to display
 @param format printf like format string
 @param ... printf like parameters
 @return the newly created item */
 proto_item *
proto_tree_add_ipv6_format_value(proto_tree *tree, int hfindex, tvbuff_t *tvb,
    int start, int length, const ws_in6_addr *value_ptr, const char *format,
    ...) G_GNUC_PRINTF(7,8);

/** Add a formatted FT_IPv6 to a proto_tree, with the format generating
    the entire string for the entry, including any field name.
 @param tree the tree to append this item to
 @param hfindex field index
 @param tvb the tv buffer of the current data
 @param start start of data in tvb
 @param length length of data in tvb
 @param value_ptr data to display
 @param format printf like format string
 @param ... printf like parameters
 @return the newly created item */
 proto_item *
proto_tree_add_ipv6_format(proto_tree *tree, int hfindex, tvbuff_t *tvb, int start,
    int length, const ws_in6_addr *value_ptr, const char *format, ...) G_GNUC_PRINTF(7,8);

/** Add a FT_ETHER to a proto_tree.
 @param tree the tree to append this item to
 @param hfindex field index
 @param tvb the tv buffer of the current data
 @param start start of data in tvb
 @param length length of data in tvb
 @param value data to display
 @return the newly created item */
 proto_item *
proto_tree_add_ether(proto_tree *tree, int hfindex, tvbuff_t *tvb, int start,
    int length, const uint8_t* value);

/** Add a formatted FT_ETHER to a proto_tree, with the format generating
    the string for the value and with the field name being included
    automatically.
 @param tree the tree to append this item to
 @param hfindex field index
 @param tvb the tv buffer of the current data
 @param start start of data in tvb
 @param length length of data in tvb
 @param value data to display
 @param format printf like format string
 @param ... printf like parameters
 @return the newly created item */
 proto_item *
proto_tree_add_ether_format_value(proto_tree *tree, int hfindex, tvbuff_t *tvb,
    int start, int length, const uint8_t* value, const char *format, ...)
    G_GNUC_PRINTF(7,8);

/** Add a formatted FT_ETHER to a proto_tree, with the format generating
    the entire string for the entry, including any field name.
 @param tree the tree to append this item to
 @param hfindex field index
 @param tvb the tv buffer of the current data
 @param start start of data in tvb
 @param length length of data in tvb
 @param value data to display
 @param format printf like format string
 @param ... printf like parameters
 @return the newly created item */
 proto_item *
proto_tree_add_ether_format(proto_tree *tree, int hfindex, tvbuff_t *tvb, int start,
    int length, const uint8_t* value, const char *format, ...) G_GNUC_PRINTF(7,8);

/** Add a FT_GUID to a proto_tree.
 @param tree the tree to append this item to
 @param hfindex field index
 @param tvb the tv buffer of the current data
 @param start start of data in tvb
 @param length length of data in tvb
 @param value_ptr data to display
 @return the newly created item */
 proto_item *
proto_tree_add_guid(proto_tree *tree, int hfindex, tvbuff_t *tvb, int start,
    int length, const e_guid_t *value_ptr);

/** Add a formatted FT_GUID to a proto_tree, with the format generating
    the string for the value and with the field name being included
    automatically.
 @param tree the tree to append this item to
 @param hfindex field index
 @param tvb the tv buffer of the current data
 @param start start of data in tvb
 @param length length of data in tvb
 @param value_ptr data to display
 @param format printf like format string
 @param ... printf like parameters
 @return the newly created item */
 proto_item *
proto_tree_add_guid_format_value(proto_tree *tree, int hfindex, tvbuff_t *tvb,
    int start, int length, const e_guid_t *value_ptr, const char *format,
    ...) G_GNUC_PRINTF(7,8);

/** Add a formatted FT_GUID to a proto_tree, with the format generating
    the entire string for the entry, including any field name.
 @param tree the tree to append this item to
 @param hfindex field index
 @param tvb the tv buffer of the current data
 @param start start of data in tvb
 @param length length of data in tvb
 @param value_ptr data to display
 @param format printf like format string
 @param ... printf like parameters
 @return the newly created item */
 proto_item *
proto_tree_add_guid_format(proto_tree *tree, int hfindex, tvbuff_t *tvb, int start,
    int length, const e_guid_t *value_ptr, const char *format, ...) G_GNUC_PRINTF(7,8);

/** Add a FT_OID to a proto_tree.
 @param tree the tree to append this item to
 @param hfindex field index
 @param tvb the tv buffer of the current data
 @param start start of data in tvb
 @param length length of data in tvb
 @param value_ptr data to display
 @return the newly created item */
 proto_item *
proto_tree_add_oid(proto_tree *tree, int hfindex, tvbuff_t *tvb, int start,
    int length, const uint8_t* value_ptr);

/** Add a formatted FT_OID to a proto_tree, with the format generating
    the string for the value and with the field name being included
    automatically.
 @param tree the tree to append this item to
 @param hfindex field index
 @param tvb the tv buffer of the current data
 @param start start of data in tvb
 @param length length of data in tvb
 @param value_ptr data to display
 @param format printf like format string
 @param ... printf like parameters
 @return the newly created item */
 proto_item *
proto_tree_add_oid_format_value(proto_tree *tree, int hfindex, tvbuff_t *tvb,
    int start, int length, const uint8_t* value_ptr, const char *format,
    ...) G_GNUC_PRINTF(7,8);

/** Add a formatted FT_OID to a proto_tree, with the format generating
    the entire string for the entry, including any field name.
 @param tree the tree to append this item to
 @param hfindex field index
 @param tvb the tv buffer of the current data
 @param start start of data in tvb
 @param length length of data in tvb
 @param value_ptr data to display
 @param format printf like format string
 @param ... printf like parameters
 @return the newly created item */
 proto_item *
proto_tree_add_oid_format(proto_tree *tree, int hfindex, tvbuff_t *tvb, int start,
    int length, const uint8_t* value_ptr, const char *format, ...) G_GNUC_PRINTF(7,8);

/** Add an FT_STRING, FT_STRINGZ, FT_STRINGZPAD, or FT_STRINGZTRUNC to a
    proto_tree. The value passed in should be a UTF-8 encoded null terminated
    string, such as produced by tvb_get_string_enc(), regardless of the original
    packet data.

    This function is used to add a custom string *value* to the protocol tree.
    Do not format the string value for display, for example by using format_text().
    The input string represents packet data, not a display label. Formatting
    labels is a concern of the UI. Doing that here would change the meaning of the packet
    data, restrict the options for formatting later and make display filtering unintuitive
    for whitespace and other special characters.

 @param tree the tree to append this item to
 @param hfindex field index
 @param tvb the tv buffer of the current data
 @param start start of data in tvb
 @param length length of data in tvb
 @param value data to display
 @return the newly created item */
 proto_item *
proto_tree_add_string(proto_tree *tree, int hfindex, tvbuff_t *tvb, int start,
    int length, const char* value);

/** Add a formatted FT_STRING, FT_STRINGZ, FT_STRINGZPAD, or FT_STRINGZTRUNC
    to a proto_tree, with the format generating the string for the value
    and with the field name being included automatically.
 @param tree the tree to append this item to
 @param hfindex field index
 @param tvb the tv buffer of the current data
 @param start start of data in tvb
 @param length length of data in tvb
 @param value data to display
 @param format printf like format string
 @param ... printf like parameters
 @return the newly created item */
 proto_item *
proto_tree_add_string_format_value(proto_tree *tree, int hfindex, tvbuff_t *tvb,
    int start, int length, const char* value, const char *format, ...)
    G_GNUC_PRINTF(7,8);

/** Add a formatted FT_STRING, FT_STRINGZ, FT_STRINGZPAD, or FT_STRINGZTRUNC
    to a proto_tree, with the format generating the entire string for the
    entry, including any field name.
 @param tree the tree to append this item to
 @param hfindex field index
 @param tvb the tv buffer of the current data
 @param start start of data in tvb
 @param length length of data in tvb
 @param value data to display
 @param format printf like format string
 @param ... printf like parameters
 @return the newly created item */
 proto_item *
proto_tree_add_string_format(proto_tree *tree, int hfindex, tvbuff_t *tvb, int start,
    int length, const char* value, const char *format, ...) G_GNUC_PRINTF(7,8);

/** Add a FT_BOOLEAN to a proto_tree.
 @param tree the tree to append this item to
 @param hfindex field index
 @param tvb the tv buffer of the current data
 @param start start of data in tvb
 @param length length of data in tvb
 @param value data to display
 @return the newly created item */
 proto_item *
proto_tree_add_boolean(proto_tree *tree, int hfindex, tvbuff_t *tvb, int start,
    int length, uint64_t value);

/** Add a formatted FT_BOOLEAN to a proto_tree, with the format generating
    the string for the value and with the field name being included
    automatically.
 @param tree the tree to append this item to
 @param hfindex field index
 @param tvb the tv buffer of the current data
 @param start start of data in tvb
 @param length length of data in tvb
 @param value data to display
 @param format printf like format string
 @param ... printf like parameters
 @return the newly created item */
 proto_item *
proto_tree_add_boolean_format_value(proto_tree *tree, int hfindex,
    tvbuff_t *tvb, int start, int length, uint64_t value,
    const char *format, ...) G_GNUC_PRINTF(7,8);

/** Add a formatted FT_BOOLEAN to a proto_tree, with the format generating
    the entire string for the entry, including any field name.
 @param tree the tree to append this item to
 @param hfindex field index
 @param tvb the tv buffer of the current data
 @param start start of data in tvb
 @param length length of data in tvb
 @param value data to display
 @param format printf like format string
 @param ... printf like parameters
 @return the newly created item */
 proto_item *
proto_tree_add_boolean_format(proto_tree *tree, int hfindex, tvbuff_t *tvb, int start,
    int length, uint64_t value, const char *format, ...) G_GNUC_PRINTF(7,8);

/** Add a FT_FLOAT to a proto_tree.
 @param tree the tree to append this item to
 @param hfindex field index
 @param tvb the tv buffer of the current data
 @param start start of data in tvb
 @param length length of data in tvb
 @param value data to display
 @return the newly created item */
 proto_item *
proto_tree_add_float(proto_tree *tree, int hfindex, tvbuff_t *tvb, int start,
    int length, float value);

/** Add a formatted FT_FLOAT to a proto_tree, with the format generating
    the string for the value and with the field name being included
    automatically.
 @param tree the tree to append this item to
 @param hfindex field index
 @param tvb the tv buffer of the current data
 @param start start of data in tvb
 @param length length of data in tvb
 @param value data to display
 @param format printf like format string
 @param ... printf like parameters
 @return the newly created item */
 proto_item *
proto_tree_add_float_format_value(proto_tree *tree, int hfindex, tvbuff_t *tvb,
    int start, int length, float value, const char *format, ...)
    G_GNUC_PRINTF(7,8);

/** Add a formatted FT_FLOAT to a proto_tree, with the format generating
    the entire string for the entry, including any field name.
 @param tree the tree to append this item to
 @param hfindex field index
 @param tvb the tv buffer of the current data
 @param start start of data in tvb
 @param length length of data in tvb
 @param value data to display
 @param format printf like format string
 @param ... printf like parameters
 @return the newly created item */
 proto_item *
proto_tree_add_float_format(proto_tree *tree, int hfindex, tvbuff_t *tvb, int start,
    int length, float value, const char *format, ...) G_GNUC_PRINTF(7,8);

/** Add a FT_DOUBLE to a proto_tree.
 @param tree the tree to append this item to
 @param hfindex field index
 @param tvb the tv buffer of the current data
 @param start start of data in tvb
 @param length length of data in tvb
 @param value data to display
 @return the newly created item */
 proto_item *
proto_tree_add_double(proto_tree *tree, int hfindex, tvbuff_t *tvb, int start,
    int length, double value);

/** Add a formatted FT_DOUBLE to a proto_tree, with the format generating
    the string for the value and with the field name being included
    automatically.
 @param tree the tree to append this item to
 @param hfindex field index
 @param tvb the tv buffer of the current data
 @param start start of data in tvb
 @param length length of data in tvb
 @param value data to display
 @param format printf like format string
 @param ... printf like parameters
 @return the newly created item */
 proto_item *
proto_tree_add_double_format_value(proto_tree *tree, int hfindex, tvbuff_t *tvb,
    int start, int length, double value, const char *format, ...)
    G_GNUC_PRINTF(7,8);

/** Add a formatted FT_DOUBLE to a proto_tree, with the format generating
    the entire string for the entry, including any field name.
 @param tree the tree to append this item to
 @param hfindex field index
 @param tvb the tv buffer of the current data
 @param start start of data in tvb
 @param length length of data in tvb
 @param value data to display
 @param format printf like format string
 @param ... printf like parameters
 @return the newly created item */
 proto_item *
proto_tree_add_double_format(proto_tree *tree, int hfindex, tvbuff_t *tvb, int start,
    int length, double value, const char *format, ...) G_GNUC_PRINTF(7,8);

/** Add one of FT_UINT8, FT_UINT16, FT_UINT24 or FT_UINT32 to a proto_tree.
 @param tree the tree to append this item to
 @param hfindex field index
 @param tvb the tv buffer of the current data
 @param start start of data in tvb
 @param length length of data in tvb
 @param value data to display
 @return the newly created item */
 proto_item *
proto_tree_add_uint(proto_tree *tree, int hfindex, tvbuff_t *tvb, int start,
    int length, uint32_t value);

/** Add a formatted FT_UINT8, FT_UINT16, FT_UINT24 or FT_UINT32 to a proto_tree,
    with the format generating the string for the value and with the field
    name being included automatically.
 @param tree the tree to append this item to
 @param hfindex field index
 @param tvb the tv buffer of the current data
 @param start start of data in tvb
 @param length length of data in tvb
 @param value data to display
 @param format printf like format string
 @param ... printf like parameters
 @return the newly created item */
 proto_item *
proto_tree_add_uint_format_value(proto_tree *tree, int hfindex, tvbuff_t *tvb,
    int start, int length, uint32_t value, const char *format, ...)
    G_GNUC_PRINTF(7,8);

/** Add a formatted FT_UINT8, FT_UINT16, FT_UINT24 or FT_UINT32 to a proto_tree,
    with the format generating the entire string for the entry, including any
    field name.
 @param tree the tree to append this item to
 @param hfindex field index
 @param tvb the tv buffer of the current data
 @param start start of data in tvb
 @param length length of data in tvb
 @param value data to display
 @param format printf like format string
 @param ... printf like parameters
 @return the newly created item */
 proto_item *
proto_tree_add_uint_format(proto_tree *tree, int hfindex, tvbuff_t *tvb, int start,
    int length, uint32_t value, const char *format, ...) G_GNUC_PRINTF(7,8);

/** Add an FT_UINT64 to a proto_tree.
 @param tree the tree to append this item to
 @param hfindex field index
 @param tvb the tv buffer of the current data
 @param start start of data in tvb
 @param length length of data in tvb
 @param value data to display
 @return the newly created item */
 proto_item *
proto_tree_add_uint64(proto_tree *tree, int hfindex, tvbuff_t *tvb, int start,
    int length, uint64_t value);

/** Add a formatted FT_UINT64 to a proto_tree, with the format generating
    the string for the value and with the field name being included
    automatically.
 @param tree the tree to append this item to
 @param hfindex field index
 @param tvb the tv buffer of the current data
 @param start start of data in tvb
 @param length length of data in tvb
 @param value data to display
 @param format printf like format string
 @param ... printf like parameters
 @return the newly created item */
 proto_item *
proto_tree_add_uint64_format_value(proto_tree *tree, int hfindex, tvbuff_t *tvb,
    int start, int length, uint64_t value, const char *format, ...)
    G_GNUC_PRINTF(7,8);

/** Add a formatted FT_UINT64 to a proto_tree, with the format generating
    the entire string for the entry, including any field name.
 @param tree the tree to append this item to
 @param hfindex field index
 @param tvb the tv buffer of the current data
 @param start start of data in tvb
 @param length length of data in tvb
 @param value data to display
 @param format printf like format string
 @param ... printf like parameters
 @return the newly created item */
 proto_item *
proto_tree_add_uint64_format(proto_tree *tree, int hfindex, tvbuff_t *tvb, int start,
    int length, uint64_t value, const char *format, ...) G_GNUC_PRINTF(7,8);

/** Add one of FT_INT8, FT_INT16, FT_INT24 or FT_INT32 to a proto_tree.
 @param tree the tree to append this item to
 @param hfindex field index
 @param tvb the tv buffer of the current data
 @param start start of data in tvb
 @param length length of data in tvb
 @param value data to display
 @return the newly created item */
 proto_item *
proto_tree_add_int(proto_tree *tree, int hfindex, tvbuff_t *tvb, int start,
    int length, int32_t value);

/** Add a formatted FT_INT8, FT_INT16, FT_INT24 or FT_INT32 to a proto_tree,
    with the format generating the string for the value and with the field
    name being included automatically.
 @param tree the tree to append this item to
 @param hfindex field index
 @param tvb the tv buffer of the current data
 @param start start of data in tvb
 @param length length of data in tvb
 @param value data to display
 @param format printf like format string
 @param ... printf like parameters
 @return the newly created item */
 proto_item *
proto_tree_add_int_format_value(proto_tree *tree, int hfindex, tvbuff_t *tvb,
    int start, int length, int32_t value, const char *format, ...)
    G_GNUC_PRINTF(7,8);

/** Add a formatted FT_INT8, FT_INT16, FT_INT24 or FT_INT32 to a proto_tree,
    with the format generating the entire string for the entry, including
    any field name.
 @param tree the tree to append this item to
 @param hfindex field index
 @param tvb the tv buffer of the current data
 @param start start of data in tvb
 @param length length of data in tvb
 @param value data to display
 @param format printf like format string
 @param ... printf like parameters
 @return the newly created item */
 proto_item *
proto_tree_add_int_format(proto_tree *tree, int hfindex, tvbuff_t *tvb, int start,
    int length, int32_t value, const char *format, ...) G_GNUC_PRINTF(7,8);

/** Add an FT_INT64 to a proto_tree.
 @param tree the tree to append this item to
 @param hfindex field index
 @param tvb the tv buffer of the current data
 @param start start of data in tvb
 @param length length of data in tvb
 @param value data to display
 @return the newly created item */
 proto_item *
proto_tree_add_int64(proto_tree *tree, int hfindex, tvbuff_t *tvb, int start,
    int length, int64_t value);

/** Add a formatted FT_INT64 to a proto_tree, with the format generating
    the string for the value and with the field name being included
    automatically.
 @param tree the tree to append this item to
 @param hfindex field index
 @param tvb the tv buffer of the current data
 @param start start of data in tvb
 @param length length of data in tvb
 @param value data to display
 @param format printf like format string
 @param ... printf like parameters
 @return the newly created item */
 proto_item *
proto_tree_add_int64_format_value(proto_tree *tree, int hfindex, tvbuff_t *tvb,
    int start, int length, int64_t value, const char *format, ...)
    G_GNUC_PRINTF(7,8);

/** Add a formatted FT_INT64 to a proto_tree, with the format generating
    the entire string for the entry, including any field name.
 @param tree the tree to append this item to
 @param hfindex field index
 @param tvb the tv buffer of the current data
 @param start start of data in tvb
 @param length length of data in tvb
 @param value data to display
 @param format printf like format string
 @param ... printf like parameters
 @return the newly created item */
 proto_item *
proto_tree_add_int64_format(proto_tree *tree, int hfindex, tvbuff_t *tvb, int start,
    int length, int64_t value, const char *format, ...) G_GNUC_PRINTF(7,8);

/** Add a FT_EUI64 to a proto_tree.
 @param tree the tree to append this item to
 @param hfindex field index
 @param tvb the tv buffer of the current data
 @param start start of data in tvb
 @param length length of data in tvb
 @param value data to display
 @return the newly created item */
 proto_item *
proto_tree_add_eui64(proto_tree *tree, int hfindex, tvbuff_t *tvb, int start,
    int length, const uint64_t value);

/** Add a formatted FT_EUI64 to a proto_tree, with the format generating
    the string for the value and with the field name being included
    automatically.
 @param tree the tree to append this item to
 @param hfindex field index
 @param tvb the tv buffer of the current data
 @param start start of data in tvb
 @param length length of data in tvb
 @param value data to display
 @param format printf like format string
 @param ... printf like parameters
 @return the newly created item */
 proto_item *
proto_tree_add_eui64_format_value(proto_tree *tree, int hfindex, tvbuff_t *tvb,
    int start, int length, const uint64_t value, const char *format, ...)
    G_GNUC_PRINTF(7,8);

/** Add a formatted FT_EUI64 to a proto_tree, with the format generating
    the entire string for the entry, including any field name.
 @param tree the tree to append this item to
 @param hfindex field index
 @param tvb the tv buffer of the current data
 @param start start of data in tvb
 @param length length of data in tvb
 @param value data to display
 @param format printf like format string
 @param ... printf like parameters
 @return the newly created item */
 proto_item *
proto_tree_add_eui64_format(proto_tree *tree, int hfindex, tvbuff_t *tvb, int start,
    int length, const uint64_t value, const char *format, ...) G_GNUC_PRINTF(7,8);

/** Structure used in proto_tree_add_mac48_detail below */
typedef struct _mac_hf_list_t {
    int *hf_addr;               // FT_ETHER, BASE_NONE
    int *hf_oui;                // FT_UINT24, BASE_OUI
    int *hf_lg;                 // FT_BOOLEAN, 24 bits, mask 0x020000
    int *hf_ig;                 // FT_BOOLEAN, 24 bits, mask 0x010000
} mac_hf_list_t;

/** Add a MAC-48 (Ethernet) address to a proto_tree from the tvb.
    Handles full and OUI resolution, IG and LG bits, and hidden
    generic fields, all as a subtree of the address item.
 @param list_specific the mac_hf_list_t with field indexes for the specific addr type
 @param list_generic the mac_hf_list_t with field indexes for the generic addr type
 @param idx one of the ett_ array elements registered with proto_register_subtree_array()
 @param tvb the tv buffer of the current data
 @param tree the tree to append this item to
 @param offset start of data in tvb representing the MAC-48 address */
 proto_item *
proto_tree_add_mac48_detail(const mac_hf_list_t *list_specific,
    const mac_hf_list_t *list_generic,
    int idx, tvbuff_t *tvb, proto_tree *tree, int offset);

/** Useful for quick debugging. Also sends string to STDOUT, so don't
    leave call to this function in production code.
 @param tree the tree to append the text to
 @param format printf like format string
 @param ... printf like parameters
 @return the newly created item */
 proto_item *
proto_tree_add_debug_text(proto_tree *tree, const char *format,
    ...) G_GNUC_PRINTF(2,3);

/** Fill given label_str with a simple string representation of field.
 @param finfo the item to get the info from
 @param label_str the string to fill
 @todo think about changing the parameter profile */
 void
proto_item_fill_label(const field_info *finfo, char *label_str);

/** Fill the given display_label_str with the string representation of a field
 * formatted according to its type and field display specifier.
 * Used to display custom columns and packet diagram values.
 @param fi The item to get the info from
 @param display_label_str The string to fill
 @return The length of the label excluding the terminating '\0'.
 */
 int
proto_item_fill_display_label(const field_info *fi, char *display_label_str, const int label_str_size);

/** Register a new protocol.
 @param name the full name of the new protocol
 @param short_name abbreviated name of the new protocol
 @param filter_name protocol name used for a display filter string
 @return the new protocol handle */
 int
proto_register_protocol(const char *name, const char *short_name, const char *filter_name);

/** Register a "helper" protocol (pino - protocol in name only).
 This is for dissectors that need distinguishing names and don't need the other
 features (like enable/disable).  One use case is a protocol with multiple dissection
 functions in a single dissector table needing unique "dissector names" to remove
 confusion with Decode As dialog.  Another use case is for a dissector table set
 up to handle TLVs within a single protocol (and allow "external" TLVs being
 registered through the dissector table).
 @param name the full name of the new protocol
 @param short_name abbreviated name of the new protocol
 @param filter_name protocol name used for a display filter string
 @param parent_proto the "real" protocol for the helper.  The parent decides enable/disable
 @param field_type FT_PROTOCOL or FT_BYTES.  Allows removal of "protocol highlighting" (FT_BYTES)
 if pino is part of TLV.
 @return the new protocol handle */
 int
proto_register_protocol_in_name_only(const char *name, const char *short_name, const char *filter_name,
    int parent_proto, enum ftenum field_type);

/** Deregister a protocol.
 This is only used internally for reloading Lua plugins and must not be used
 by dissectors or plugins.
 @param short_name abbreviated name of the protocol
 @return true if protocol is removed */
bool
proto_deregister_protocol(const char *short_name);

/** Register a protocol alias.
 This is for dissectors whose original name has changed, e.g. BOOTP to DHCP.
 @param proto_id protocol id returned by proto_register_protocol (0-indexed)
 @param alias_name alias for the protocol's filter name */
 void
proto_register_alias(const int proto_id, const char *alias_name);

/** This type of function can be registered to get called whenever
    a given field was not found but a its prefix is matched;
    It can be used to procrastinate the hf array registration.
   @param match  what's being matched */
typedef void (*prefix_initializer_t)(const char* match);

/** Register a new prefix for delayed initialization of field arrays
    Note that the initializer function MAY NOT be called before the dissector
    is first called.  That is, dissectors using this function must be prepared
    to call the initializer before beginning dissection; they should do this by
    calling proto_registrar_get_byname() on one of the dissector's field names.
@param prefix the prefix for the new protocol
@param initializer function that will initialize the field array for the given prefix */
 void
proto_register_prefix(const char *prefix,  prefix_initializer_t initializer);

/** Initialize every remaining uninitialized prefix. */
 void proto_initialize_all_prefixes(void);

/** Register a header_field array.
 @param parent the protocol handle from proto_register_protocol()
 @param hf the hf_register_info array
 @param num_records the number of records in hf */
 void
proto_register_field_array(const int parent, hf_register_info *hf, const int num_records);

/** Deregister an already registered field.
 @param parent the protocol handle from proto_register_protocol()
 @param hf_id the field to deregister */
 void
proto_deregister_field (const int parent, int hf_id);

/** Add data to be freed when deregistered fields are freed.
 @param data a pointer to data to free */
 void
proto_add_deregistered_data (void *data);

/** Add a memory slice to be freed when deregistered fields are freed.
 @param block_size the size of the block
 @param mem_block a pointer to the block to free */
void
proto_add_deregistered_slice (size_t block_size, void *mem_block);

/** Free strings in a field.
 @param field_type the field type (one of FT_ values)
 @param field_display field display value (one of BASE_ values)
 @param field_strings field strings */
 void
proto_free_field_strings (ftenum_t field_type, unsigned int field_display, const void *field_strings);

/** Free fields deregistered in proto_deregister_field(). */
 void
proto_free_deregistered_fields (void);

/** Register a protocol subtree (ett) array.
 @param indices array of ett indices
 @param num_indices the number of records in indices */
 void
proto_register_subtree_array(int * const *indices, const int num_indices);

/** Get name of registered header_field number n.
 @param n item # n (0-indexed)
 @return the name of this registered item */
 const char* proto_registrar_get_name(const int n);

/** Get abbreviation of registered header_field number n.
 @param n item # n (0-indexed)
 @return the abbreviation of this registered item */
 const char* proto_registrar_get_abbrev(const int n);

/** Get the header_field information based upon a field or protocol id.
 @param hfindex item # n (0-indexed)
 @return the registered item */
 header_field_info* proto_registrar_get_nth(unsigned hfindex);

/** Get the header_field information based upon a field name.
 @param field_name the field name to search for
 @return the registered item */
 header_field_info* proto_registrar_get_byname(const char *field_name);

/** Get the header_field information based upon a field alias.
 @param alias_name the aliased field name to search for
 @return the registered item */
 header_field_info* proto_registrar_get_byalias(const char *alias_name);

/** Get the header_field id based upon a field name.
 @param field_name the field name to search for
 @return the field id for the registered item */
 int proto_registrar_get_id_byname(const char *field_name);

/** Get enum ftenum FT_ of registered header_field number n.
 @param n item # n (0-indexed)
 @return the registered item */
 enum ftenum proto_registrar_get_ftype(const int n);

/** Get parent protocol of registered header_field number n.
 @param n item # n (0-indexed)
 @return -1 if item _is_ a protocol */
 int proto_registrar_get_parent(const int n);

/** Is item # n a protocol?
 @param n item # n (0-indexed)
 @return true if it's a protocol, false if it's not */
 bool proto_registrar_is_protocol(const int n);

/** Get length of registered field according to field type.
 @param n item # n (0-indexed)
 @return 0 means undeterminable at registration time, -1 means unknown field */
extern int proto_registrar_get_length(const int n);


/** Routines to use to iterate over the protocols and their fields;
 * they return the item number of the protocol in question or the
 * appropriate hfinfo pointer, and keep state in "*cookie". */
 int proto_get_first_protocol(void **cookie);
 int proto_get_data_protocol(void *cookie);
 int proto_get_next_protocol(void **cookie);
 header_field_info *proto_get_first_protocol_field(const int proto_id, void **cookie);
 header_field_info *proto_get_next_protocol_field(const int proto_id, void **cookie);

/** Check if a protocol name is already registered.
 @param name the name to search for
 @return proto_id */
 bool proto_name_already_registered(const char *name);

/** Given a protocol's filter_name.
 @param filter_name the filter name to search for
 @return proto_id */
 int proto_get_id_by_filter_name(const char* filter_name);

/** Given a protocol's short name.
 @param short_name the protocol short name to search for
 @return proto_id */
 int proto_get_id_by_short_name(const char* short_name);

/** Can item # n decoding be disabled?
 @param proto_id protocol id (0-indexed)
 @return true if it's a protocol, false if it's not */
 bool proto_can_toggle_protocol(const int proto_id);

/** Get the "protocol_t" structure for the given protocol's item number.
 @param proto_id protocol id (0-indexed) */
 protocol_t *find_protocol_by_id(const int proto_id);

/** Get the protocol's name for the given protocol's item number.
 @param proto_id protocol id (0-indexed)
 @return its name */
 const char *proto_get_protocol_name(const int proto_id);

/** Get the protocol's item number, for the given protocol's "protocol_t".
 @return its proto_id */
 int proto_get_id(const protocol_t *protocol);

/** Get the protocol's short name, for the given protocol's "protocol_t".
 @return its short name. */
 const char *proto_get_protocol_short_name(const protocol_t *protocol);

/** Get the protocol's long name, for the given protocol's "protocol_t".
 @return its long name. */
 const char *proto_get_protocol_long_name(const protocol_t *protocol);

/** Is protocol's decoding enabled ?
 @return true if decoding is enabled, false if not */
 bool proto_is_protocol_enabled(const protocol_t *protocol);

/** Is protocol's enabled by default (most are)?
 @return true if decoding is enabled by default, false if not */
 bool proto_is_protocol_enabled_by_default(const protocol_t *protocol);

/** Is this a protocol in name only (i.e. not a real one)?
 @return true if helper, false if not */
 bool proto_is_pino(const protocol_t *protocol);

/** Get a protocol's filter name by its item number.
 @param proto_id protocol id (0-indexed)
 @return its filter name. */
 const char *proto_get_protocol_filter_name(const int proto_id);

/** Associate a heuristic dissector with a protocol
 * INTERNAL USE ONLY!!!
 * @param protocol to associate the heuristic with
 * @param short_name heuristic dissector's short name
 */
extern void proto_add_heuristic_dissector(protocol_t *protocol, const char *short_name);

/** Apply func to all heuristic dissectors of a protocol
 * @param protocol to iterate over heuristics
 * @param func function to execute on heuristics
 * @param user_data user-specific data for function
 */
 void proto_heuristic_dissector_foreach(const protocol_t *protocol, GFunc func,
    void *user_data);

/** Find commonly-used protocols in a layer list.
 * @param layers Protocol layer list
 * @param is_ip Set to true if the layer list contains IPv4 or IPv6, otherwise
 * unchanged. May be NULL.
 * @param is_tcp Set to true if the layer list contains TCP, otherwise
 * unchanged. May be NULL.
 * @param is_udp Set to true if the layer list contains UDP, otherwise
 * unchanged. May be NULL.
 * @param is_sctp Set to true if the layer list contains SCTP, otherwise
 * unchanged. May be NULL.
 * @param is_tls Set to true if the layer list contains SSL/TLS, otherwise
 * unchanged. May be NULL.
 * @param is_rtp Set to true if the layer list contains RTP, otherwise
 * unchanged. May be NULL.
 * @param is_lte_rlc Set to true if the layer list contains LTE RLC, otherwise
 * unchanged. May be NULL.
 */
 void proto_get_frame_protocols(const wmem_list_t *layers,
      bool *is_ip, bool *is_tcp, bool *is_udp, bool *is_sctp,
      bool *is_tls, bool *is_rtp, bool *is_lte_rlc);

/** Check whether a protocol, specified by name, is in a layer list.
 * @param layers Protocol layer list
 * @param proto_name Name of protocol to find
 * @return true if the protocol is found, false if it isn't
 */
 bool proto_is_frame_protocol(const wmem_list_t *layers, const char* proto_name);

/** Create a string of all layers in the packet.
 * @param pinfo Pointer to packet info
 * @return string of layer names
 */
 char * proto_list_layers(const packet_info *pinfo);

/** Mark protocol with the given item number as disabled by default.
 @param proto_id protocol id (0-indexed) */
 void proto_disable_by_default(const int proto_id);

/** Enable / Disable protocol of the given item number.
 @param proto_id protocol id (0-indexed)
 @param enabled enable / disable the protocol */
 void proto_set_decoding(const int proto_id, const bool enabled);

/** Disable all protocols. */
 void proto_disable_all(void);

/** Re-enable all protocols that are not marked as disabled by default. */
 void proto_reenable_all(void);

/** Disable disabling/enabling of protocol of the given item number.
 @param proto_id protocol id (0-indexed) */
 void proto_set_cant_toggle(const int proto_id);

/** Checks for existence any protocol or field within a tree.
 @param tree "Protocols" are assumed to be a child of the [empty] root node.
 @param id hfindex of protocol or field
 @return true = found, false = not found
 @todo add explanation of id parameter */
extern bool proto_check_for_protocol_or_field(const proto_tree* tree, const int id);

/** Return GPtrArray* of field_info pointers for all hfindex that appear in
    tree. Only works with primed trees, and is fast.
 @param tree tree of interest
 @param hfindex primed hfindex
 @return GPtrArray pointer

   The caller should *not* free the GPtrArray*; proto_tree_free_node()
   handles that. */
 GPtrArray* proto_get_finfo_ptr_array(const proto_tree *tree, const int hfindex);

/** Return whether we're tracking any interesting fields.
    Only works with primed trees, and is fast.
 @param tree tree of interest
 @return true if we're tracking interesting fields */
 bool proto_tracking_interesting_fields(const proto_tree *tree);

/** Return GPtrArray* of field_info pointers for all hfindex that appear in
    tree. Works with any tree, primed or unprimed, and is slower than
    proto_get_finfo_ptr_array because it has to search through the tree.
 @param tree tree of interest
 @param hfindex index of field info of interest
 @return GPtrArry pointer

    The caller does need to free the returned GPtrArray with
    g_ptr_array_free(<array>, true). */
 GPtrArray* proto_find_finfo(proto_tree *tree, const int hfindex);

/** Return GPtrArray* of field_info pointer for first hfindex that appear in
tree. Works with any tree, primed or unprimed, and is slower than
proto_get_finfo_ptr_array because it has to search through the tree.
@param tree tree of interest
@param hfindex index of field info of interest
@return GPtrArry pointer

    The caller does need to free the returned GPtrArray with
    g_ptr_array_free(<array>, true). */
 GPtrArray* proto_find_first_finfo(proto_tree *tree, const int hfindex);

/** Return GPtrArray* of field_info pointers containg all hfindexes that appear
    in tree.
 @param tree tree of interest
 @return GPtrArry pointer

    The caller does need to free the returned GPtrArray with
    g_ptr_array_free(<array>, true). */
 GPtrArray* proto_all_finfos(proto_tree *tree);

/** Dumps a glossary of the protocol registrations to STDOUT */
 void proto_registrar_dump_protocols(void);

/** Dumps a glossary of the field value strings or true/false strings to STDOUT */
 void proto_registrar_dump_values(void);

/** Dumps a mapping file for loading tshark output into ElasticSearch */
 void proto_registrar_dump_elastic(const char* filter);

/** Dumps the number of protocol and field registrations to STDOUT.
 @return false if we pre-allocated enough fields, true otherwise. */
 bool proto_registrar_dump_fieldcount(void);

/** Dumps a glossary of the protocol and field registrations to STDOUT. */
 void proto_registrar_dump_fields(void);

/** Dumps protocol and field abbreviations to STDOUT which start with prefix. */
 bool proto_registrar_dump_field_completions(const char *prefix);

/** Dumps a glossary field types and descriptive names to STDOUT */
 void proto_registrar_dump_ftypes(void);

/** Get string representation of display field value
 @param field_display field display value (one of BASE_ values)
 @return string representation of display field value or "Unknown" if doesn't exist */
 const char* proto_field_display_to_string(int field_display);

/** Number of elements in the tree_is_expanded array. With MSVC and a
 * libwireshark.dll, we need a special declaration. */
extern int num_tree_types;

/** Returns true if subtrees of that type are to be expanded. */
 bool tree_expanded(int tree_type);

/** Sets if subtrees of that type are to be expanded. */
 void tree_expanded_set(int tree_type, bool value);

 int hfinfo_bitshift(const header_field_info *hfinfo);

struct epan_dissect;

/** Can we do a "match selected" on this field.
 @param finfo field_info
 @param edt epan dissecting
 @return true if we can do a "match selected" on the field, false otherwise. */
 bool
proto_can_match_selected(const field_info *finfo, struct epan_dissect *edt);

/** Construct a "match selected" display filter string.
 @param finfo field_info
 @param edt epan dissecting
 @return the wmem NULL alloced display filter string.  Needs to be freed with wmem_free(NULL, ...) */
 char*
proto_construct_match_selected_string(const field_info *finfo, struct epan_dissect *edt);

/** Find field from offset in tvb.
 @param tree tree of interest
 @param offset offset in the tvb
 @param tvb the tv buffer
 @return the corresponding field_info */
 field_info*
proto_find_field_from_offset(proto_tree *tree, unsigned offset, tvbuff_t *tvb);

/** Find undecoded bytes in a tree
 @param tree tree of interest
 @param length the length of the frame
 @return an array to be used as bitmap of decoded bytes */
 char*
proto_find_undecoded_data(proto_tree *tree, unsigned length);

/** This function will dissect a sequence of bytes that describe a bitmask.
 @param tree the tree to append this item to
 @param tvb the tv buffer of the current data
 @param offset start of data in tvb
 @param hf_hdr an 8/16/24/32/40/48/56/64 bit integer that describes the
        bitmask to be dissected.
        This field will form an expansion under which the individual fields
        of the bitmask are dissected and displayed.
        This field must be of the type FT_[U]INT{8|16|24|32|40|48|56|64}.
 @param ett subtree index
 @param fields an array of pointers to int that lists all the fields of the
        bitmask. These fields can be either of the type FT_BOOLEAN for flags
        or another integer of the same type/size as hf_hdr with a mask specified.
        This array is terminated by a NULL entry.
        FT_BOOLEAN bits that are set to 1 will have the name added to the expansion.
        FT_integer fields that have a value_string attached will have the
        matched string displayed on the expansion line.
 @param encoding big or little endian byte representation (ENC_BIG_ENDIAN/ENC_LITTLE_ENDIAN/ENC_HOST_ENDIAN)
 @return the newly created item */
 proto_item *
proto_tree_add_bitmask(proto_tree *tree, tvbuff_t *tvb, const unsigned offset,
        const int hf_hdr, const int ett, int * const *fields, const unsigned encoding);

/** This function will dissect a sequence of bytes that describe a bitmask.
    The value of the integer containing the bitmask is returned through
    a pointer.
 @param tree the tree to append this item to
 @param tvb the tv buffer of the current data
 @param offset start of data in tvb
 @param hf_hdr an 8/16/24/32/40/48/56/64 bit integer that describes the
        bitmask to be dissected.
        This field will form an expansion under which the individual fields
        of the bitmask are dissected and displayed.
        This field must be of the type FT_[U]INT{8|16|24|32|40|48|56|64}.
 @param ett subtree index
 @param fields an array of pointers to int that lists all the fields of the
        bitmask. These fields can be either of the type FT_BOOLEAN for flags
        or another integer of the same type/size as hf_hdr with a mask specified.
        This array is terminated by a NULL entry.
        FT_BOOLEAN bits that are set to 1 will have the name added to the expansion.
        FT_integer fields that have a value_string attached will have the
        matched string displayed on the expansion line.
 @param encoding big or little endian byte representation (ENC_BIG_ENDIAN/ENC_LITTLE_ENDIAN/ENC_HOST_ENDIAN)
 @param[out] retval points to a uint64_t which will be set
 @return the newly created item, and *retval is set to the decoded value masked/shifted according to bitmask */
 proto_item *
proto_tree_add_bitmask_ret_uint64(proto_tree *tree, tvbuff_t *tvb, const unsigned offset,
        const int hf_hdr, const int ett, int * const *fields,
        const unsigned encoding, uint64_t *retval);

/** This function will dissect a sequence of bytes that describe a bitmask.
    This has "filterable" bitmask header functionality of proto_tree_add_bitmask
    with the ability to control what data is appended to the header like
    proto_tree_add_bitmask_text
 @param tree the tree to append this item to
 @param tvb the tv buffer of the current data
 @param offset start of data in tvb
 @param hf_hdr an 8/16/24/32/40/48/56/64 bit integer that describes the
        bitmask to be dissected.
        This field will form an expansion under which the individual fields
        of the bitmask are dissected and displayed.
        This field must be of the type FT_[U]INT{8|16|24|32|40|48|56|64}.
 @param ett subtree index
 @param fields an array of pointers to int that lists all the fields of the
        bitmask. These fields can be either of the type FT_BOOLEAN for flags
        or another integer of the same type/size as hf_hdr with a mask specified.
        This array is terminated by a NULL entry.
        FT_BOOLEAN bits that are set to 1 will have the name added to the expansion.
        FT_integer fields that have a value_string attached will have the
        matched string displayed on the expansion line.
 @param encoding big or little endian byte representation (ENC_BIG_ENDIAN/ENC_LITTLE_ENDIAN/ENC_HOST_ENDIAN)
 @param flags bitmask field using BMT_NO_* flags to determine behavior
 @return the newly created item */
 proto_item *
proto_tree_add_bitmask_with_flags(proto_tree *tree, tvbuff_t *tvb, const unsigned offset,
        const int hf_hdr, const int ett, int * const *fields, const unsigned encoding, const int flags);

/** This function will dissect a sequence of bytes that describe a bitmask.
    This has "filterable" bitmask header functionality of proto_tree_add_bitmask
    with the ability to control what data is appended to the header like
    proto_tree_add_bitmask_text
    The value of the integer containing the bitmask is returned through
    a pointer.
 @param tree the tree to append this item to
 @param tvb the tv buffer of the current data
 @param offset start of data in tvb
 @param hf_hdr an 8/16/24/32/40/48/56/64 bit integer that describes the
        bitmask to be dissected.
        This field will form an expansion under which the individual fields
        of the bitmask are dissected and displayed.
        This field must be of the type FT_[U]INT{8|16|24|32|40|48|56|64}.
 @param ett subtree index
 @param fields an array of pointers to int that lists all the fields of the
        bitmask. These fields can be either of the type FT_BOOLEAN for flags
        or another integer of the same type/size as hf_hdr with a mask specified.
        This array is terminated by a NULL entry.
        FT_BOOLEAN bits that are set to 1 will have the name added to the expansion.
        FT_integer fields that have a value_string attached will have the
        matched string displayed on the expansion line.
 @param encoding big or little endian byte representation (ENC_BIG_ENDIAN/ENC_LITTLE_ENDIAN/ENC_HOST_ENDIAN)
 @param flags bitmask field using BMT_NO_* flags to determine behavior
 @param[out] retval points to a uint64_t which will be set
 @return the newly created item, and *retval is set to the decoded value masked/shifted according to bitmask */
 proto_item *
proto_tree_add_bitmask_with_flags_ret_uint64(proto_tree *tree, tvbuff_t *tvb, const unsigned offset,
        const int hf_hdr, const int ett, int * const *fields,
        const unsigned encoding, const int flags, uint64_t *retval);

/** This function will dissect a value that describe a bitmask. Similar to proto_tree_add_bitmask(),
    but with a passed in value (presumably because it can't be retrieved directly from tvb)
 @param tree the tree to append this item to
 @param tvb the tv buffer of the current data
 @param offset start of data in tvb
 @param hf_hdr an 8/16/24/32/64 bit integer that describes the bitmask to be dissected.
        This field will form an expansion under which the individual fields of the
        bitmask is dissected and displayed.
        This field must be of the type FT_[U]INT{8|16|24|32|64}.
 @param ett subtree index
 @param fields an array of pointers to int that lists all the fields of the
        bitmask. These fields can be either of the type FT_BOOLEAN for flags
        or another integer of the same type/size as hf_hdr with a mask specified.
        This array is terminated by a NULL entry.
        FT_BOOLEAN bits that are set to 1 will have the name added to the expansion.
        FT_integer fields that have a value_string attached will have the
        matched string displayed on the expansion line.
 @param value bitmask value
 @return the newly created item */
 proto_item *
proto_tree_add_bitmask_value(proto_tree *tree, tvbuff_t *tvb, const unsigned offset,
        const int hf_hdr, const int ett, int * const *fields, const uint64_t value);

/** This function will dissect a value that describe a bitmask. Similar to proto_tree_add_bitmask(),
    but with a passed in value (presumably because it can't be retrieved directly from tvb)
    This has "filterable" bitmask header functionality of proto_tree_add_bitmask_value
    with the ability to control what data is appended to the header like
    proto_tree_add_bitmask_text
 @param tree the tree to append this item to
 @param tvb the tv buffer of the current data
 @param offset start of data in tvb
 @param hf_hdr an 8/16/24/32/64 bit integer that describes the bitmask to be dissected.
        This field will form an expansion under which the individual fields of the
        bitmask is dissected and displayed.
        This field must be of the type FT_[U]INT{8|16|24|32|64}.
 @param ett subtree index
 @param fields an array of pointers to int that lists all the fields of the
        bitmask. These fields can be either of the type FT_BOOLEAN for flags
        or another integer of the same type/size as hf_hdr with a mask specified.
        This array is terminated by a NULL entry.
        FT_BOOLEAN bits that are set to 1 will have the name added to the expansion.
        FT_integer fields that have a value_string attached will have the
        matched string displayed on the expansion line.
 @param value bitmask value
 @param flags bitmask field using BMT_NO_* flags to determine behavior
 @return the newly created item */
 proto_item *
proto_tree_add_bitmask_value_with_flags(proto_tree *tree, tvbuff_t *tvb, const unsigned offset,
        const int hf_hdr, const int ett, int * const *fields, const uint64_t value, const int flags);

/** This function will dissect a sequence of bytes that describe a bitmask. Similar
    to proto_tree_add_bitmask(), but with no "header" item to group all of the fields
 @param tree the tree to append this item to
 @param tvb the tv buffer of the current data
 @param offset start of data in tvb
 @param len number of bytes of data
 @param fields an array of pointers to int that lists all the fields of the
        bitmask. These fields can be either of the type FT_BOOLEAN for flags
        or another integer of the same type/size as hf_hdr with a mask specified.
        This array is terminated by a NULL entry.
        FT_BOOLEAN bits that are set to 1 will have the name added to the expansion.
        FT_integer fields that have a value_string attached will have the
        matched string displayed on the expansion line.
 @param encoding big or little endian byte representation (ENC_BIG_ENDIAN/ENC_LITTLE_ENDIAN/ENC_HOST_ENDIAN) */
 void
proto_tree_add_bitmask_list(proto_tree *tree, tvbuff_t *tvb, const unsigned offset,
                                const int len, int * const *fields, const unsigned encoding);

/** This function will dissect a value that describe a bitmask. Similar to proto_tree_add_bitmask_list(),
    but with a return value
 @param tree the tree to append this item to
 @param tvb the tv buffer of the current data
 @param offset start of data in tvb
 @param len number of bytes of data
 @param fields an array of pointers to int that lists all the fields of the
        bitmask. These fields can be either of the type FT_BOOLEAN for flags
        or another integer of the same type/size as hf_hdr with a mask specified.
        This array is terminated by a NULL entry.
        FT_BOOLEAN bits that are set to 1 will have the name added to the expansion.
        FT_integer fields that have a value_string attached will have the
        matched string displayed on the expansion line.
 @param encoding big or little endian byte representation (ENC_BIG_ENDIAN/ENC_LITTLE_ENDIAN/ENC_HOST_ENDIAN)
 @param retval if a pointer is passed here the value is returned. */
  void
proto_tree_add_bitmask_list_ret_uint64(proto_tree *tree, tvbuff_t *tvb, const unsigned offset,
					const int len, int * const *fields, const unsigned encoding, uint64_t *retval);

/** This function will dissect a value that describe a bitmask. Similar to proto_tree_add_bitmask_list(),
    but with a passed in value (presumably because it can't be retrieved directly from tvb)
 @param tree the tree to append this item to
 @param tvb the tv buffer of the current data
 @param offset start of data in tvb
 @param len number of bytes of data
 @param fields an array of pointers to int that lists all the fields of the
        bitmask. These fields can be either of the type FT_BOOLEAN for flags
        or another integer of the same type/size as hf_hdr with a mask specified.
        This array is terminated by a NULL entry.
        FT_BOOLEAN bits that are set to 1 will have the name added to the expansion.
        FT_integer fields that have a value_string attached will have the
        matched string displayed on the expansion line.
 @param value bitmask value */
 void
proto_tree_add_bitmask_list_value(proto_tree *tree, tvbuff_t *tvb, const unsigned offset,
                                const int len, int * const *fields, const uint64_t value);


/** This function will dissect a sequence of bytes that describe a bitmask.
 @param tree the tree to append this item to
 @param tvb the tv buffer of the current data
 @param offset start of data in tvb
 @param len number of bytes of data
 @param hf_hdr an 8/16/24/32 bit integer that describes the bitmask to be dissected.
        This field will form an expansion under which the individual fields of the
        bitmask are dissected and displayed.
        This field must be of the type FT_[U]INT{8|16|24|32}.
 @param ett subtree index
 @param fields an array of pointers to int that lists all the fields of the
        bitmask. These fields can be either of the type FT_BOOLEAN for flags
        or another integer with a mask specified.
        This array is terminated by a NULL entry.
        FT_BOOLEAN bits that are set to 1 will have the name added to the expansion.
        FT_integer fields that have a value_string attached will have the
        matched string displayed on the expansion line.
 @param exp expert info field used when decodable_len < len.  This also means this function
        should be called even when tree == NULL
 @param encoding big or little endian byte representation (ENC_BIG_ENDIAN/ENC_LITTLE_ENDIAN/ENC_HOST_ENDIAN)
 @return the newly created item */
 proto_item *
proto_tree_add_bitmask_len(proto_tree *tree, tvbuff_t *tvb, const unsigned offset, const unsigned len,
        const int hf_hdr, const int ett, int * const *fields, struct expert_field* exp, const unsigned encoding);

/** Add a text with a subtree of bitfields.
 @param tree the tree to append this item to
 @param tvb the tv buffer of the current data
 @param offset start of data in tvb
 @param len length of the field name
 @param name field name (NULL if bitfield contents should be used)
 @param fallback field name if none of bitfields were usable
 @param ett subtree index
 @param fields NULL-terminated array of bitfield indexes
 @param encoding big or little endian byte representation (ENC_BIG_ENDIAN/ENC_LITTLE_ENDIAN/ENC_HOST_ENDIAN)
 @param flags bitmask field
 @return the newly created item */
 proto_item *
proto_tree_add_bitmask_text(proto_tree *tree, tvbuff_t *tvb, const unsigned offset, const unsigned len,
        const char *name, const char *fallback,
        const int ett, int * const *fields, const unsigned encoding, const int flags);

#define BMT_NO_FLAGS    0x00    /**< Don't use any flags */
#define BMT_NO_APPEND   0x01    /**< Don't change the title at all */
#define BMT_NO_INT      0x02    /**< Don't add integral (non-boolean) fields to title */
#define BMT_NO_FALSE    0x04    /**< Don't add booleans unless they're true */
#define BMT_NO_TFS      0x08    /**< Don't use true_false_string while formatting booleans */

/** Add bits to a proto_tree, using the text label registered to that item.
   The item is extracted from the tvbuff handed to it.
 @param tree the tree to append this item to
 @param hf_index field index. Fields for use with this function should have bitmask==0.
 @param tvb the tv buffer of the current data
 @param bit_offset start of data in tvb expressed in bits
 @param no_of_bits length of data in tvb expressed in bits
 @param encoding data encoding
 @return the newly created item */
 proto_item *
proto_tree_add_bits_item(proto_tree *tree, const int hf_index, tvbuff_t *tvb, const unsigned bit_offset,
    const int no_of_bits, const unsigned encoding);

/** Add bits to a proto_tree, using the text label registered to that item.
*  The item is extracted from the tvbuff handed to it as a set
*  of crumbs (segments) of contiguous bits, specified by an
*  array of crumb_spec elements.  The crumbs are assembled to
*  create the value.  There may be any number of crumbs
*  specifying up to a total of 64 bits which may occur anywhere
*  within the tvb. If the span of the crumbs within the tvb is 4
*  octets or less, a bitmap of the crumbs is produced.
 @param tree the tree to append this item to
 @param hf_index field index. Fields for use with this function should have bitmask==0.
 @param tvb the tv buffer of the current data
 @param bit_offset of the first crumb in tvb expressed in bits
 @param crumb_spec pointer to crumb_spec array
 @param return_value if a pointer is passed here the value is returned.
 @return the newly created item */
 proto_item *
proto_tree_add_split_bits_item_ret_val(proto_tree *tree, const int hf_index, tvbuff_t *tvb,
    const unsigned bit_offset, const crumb_spec_t *crumb_spec, uint64_t *return_value);

/** Add bitmap text for a split-bits crumb to a proto_tree,
*  using the text label registered to an item. The bitmap is
*  extracted from the tvbuff handed to it as a crumb (segment)
*  of contiguous bits, specified by one of an array of
*  crumb_spec elements. This function is normally called once
*  per crumb, after the call to
   proto_tree_add_split_bits_item_ret_val
 @param tree the tree to append this item to
 @param hf_index field index. Fields for use with this function should have bitmask==0.
 @param tvb the tv buffer of the current data
 @param bit_offset of the first crumb in tvb expressed in bits
 @param crumb_spec pointer to crumb_spec array
 @param crumb_index into the crumb_spec array for this crumb */
void
proto_tree_add_split_bits_crumb(proto_tree *tree, const int hf_index, tvbuff_t *tvb,
    const unsigned bit_offset, const crumb_spec_t *crumb_spec, uint16_t crumb_index);

/** Add bits to a proto_tree, using the text label registered to that item.
   The item is extracted from the tvbuff handed to it.
 @param tree the tree to append this item to
 @param hf_index field index. Fields for use with this function should have bitmask==0.
 @param tvb the tv buffer of the current data
 @param bit_offset start of data in tvb expressed in bits
 @param no_of_bits length of data in tvb expressed in bits
 @param return_value if a pointer is passed here the value is returned.
 @param encoding data encoding
 @return the newly created item */
 proto_item *
proto_tree_add_bits_ret_val(proto_tree *tree, const int hf_index, tvbuff_t *tvb,
    const unsigned bit_offset, const int no_of_bits, uint64_t *return_value, const unsigned encoding);

/** Add bits for a FT_UINT8, FT_UINT16, FT_UINT24 or FT_UINT32
    header field to a proto_tree, with the format generating the
    string for the value and with the field name being included automatically.
 @param tree the tree to append this item to
 @param hf_index field index
 @param tvb the tv buffer of the current data
 @param bit_offset start of data in tvb expressed in bits
 @param no_of_bits length of data in tvb expressed in bit
 @param value data to display
 @param encoding data encoding
 @param format printf like format string
 @return the newly created item */
 proto_item *
proto_tree_add_uint_bits_format_value(proto_tree *tree, const int hf_index, tvbuff_t *tvb,
    const unsigned bit_offset, const int no_of_bits, uint32_t value, const unsigned encoding,
    const char *format, ...)
    G_GNUC_PRINTF(8,9);

/** Add bits for a FT_UINT8, FT_UINT16, FT_UINT24 or FT_UINT32
    header field to a proto_tree, with the format generating the
    string for the value and with the field name being included automatically.
 @param tree the tree to append this item to
 @param hf_index field index
 @param tvb the tv buffer of the current data
 @param bit_offset start of data in tvb expressed in bits
 @param no_of_bits length of data in tvb expressed in bit
 @param value data to display
 @param encoding data encoding
 @param format printf like format string
 @return the newly created item */
 proto_item *
proto_tree_add_uint64_bits_format_value(proto_tree *tree, const int hf_index, tvbuff_t *tvb,
    const unsigned bit_offset, const int no_of_bits, uint64_t value, const unsigned encoding,
    const char *format, ...)
    G_GNUC_PRINTF(8,9);

/** Add bits for a FT_BOOLEAN header field to a proto_tree, with
    the format generating the string for the value and with the field
    name being included automatically.
 @param tree the tree to append this item to
 @param hf_index field index
 @param tvb the tv buffer of the current data
 @param bit_offset start of data in tvb expressed in bits
 @param no_of_bits length of data in tvb expressed in bit
 @param value data to display
 @param encoding data encoding
 @param format printf like format string
 @param ... printf like parameters
 @return the newly created item */
proto_item *
proto_tree_add_boolean_bits_format_value(proto_tree *tree, const int hf_index, tvbuff_t *tvb,
    const unsigned bit_offset, const int no_of_bits, uint64_t value, const unsigned encoding,
    const char *format, ...)
    G_GNUC_PRINTF(8,9);

/** Add bits for a FT_INT8, FT_INT16, FT_INT24 or FT_INT32
    header field to a proto_tree, with the format generating the
    string for the value and with the field name being included automatically.
 @param tree the tree to append this item to
 @param hf_index field index
 @param tvb the tv buffer of the current data
 @param bit_offset start of data in tvb expressed in bits
 @param no_of_bits length of data in tvb expressed in bit
 @param value data to display
 @param encoding data encoding
 @param format printf like format string
 @param ... printf like parameters
 @return the newly created item */
proto_item *
proto_tree_add_int_bits_format_value(proto_tree *tree, const int hf_index, tvbuff_t *tvb,
    const unsigned bit_offset, const int no_of_bits, int32_t value, const unsigned encoding,
    const char *format, ...)
    G_GNUC_PRINTF(8,9);

/** Add bits for a FT_INT8, FT_INT16, FT_INT24 or FT_INT32
    header field to a proto_tree, with the format generating the
    string for the value and with the field name being included automatically.
 @param tree the tree to append this item to
 @param hf_index field index
 @param tvb the tv buffer of the current data
 @param bit_offset start of data in tvb expressed in bits
 @param no_of_bits length of data in tvb expressed in bit
 @param value data to display
 @param encoding data encoding
 @param format printf like format string
 @param ... printf like parameters
 @return the newly created item */
proto_item *
proto_tree_add_int64_bits_format_value(proto_tree *tree, const int hf_index, tvbuff_t *tvb,
    const unsigned bit_offset, const int no_of_bits, int64_t value, const unsigned encoding,
    const char *format, ...)
    G_GNUC_PRINTF(8,9);

/** Add bits for a FT_FLOAT header field to a proto_tree, with
    the format generating the string for the value and with the field
    name being included automatically.
 @param tree the tree to append this item to
 @param hf_index field index
 @param tvb the tv buffer of the current data
 @param bit_offset start of data in tvb expressed in bits
 @param no_of_bits length of data in tvb expressed in bit
 @param value data to display
 @param encoding data encoding
 @param format printf like format string
 @param ... printf like parameters
 @return the newly created item */
proto_item *
proto_tree_add_float_bits_format_value(proto_tree *tree, const int hf_index, tvbuff_t *tvb,
    const unsigned bit_offset, const int no_of_bits, float value, const unsigned encoding,
    const char *format, ...)
    G_GNUC_PRINTF(8,9);


/** Add a FT_STRING with ENC_3GPP_TS_23_038_7BITS_PACKED encoding to a
    proto_tree.
 @param tree the tree to append this item to
 @param hfindex field index
 @param tvb the tv buffer of the current data
 @param bit_offset start of data in tvb expressed in bits
 @param no_of_chars number of 7bits characters to display
 @return the newly created item */
 proto_item *
proto_tree_add_ts_23_038_7bits_packed_item(proto_tree *tree, const int hfindex, tvbuff_t *tvb,
    const unsigned bit_offset, const int no_of_chars);

/** Add a FT_STRING with ENC_ASCII_7BITS encoding to a proto_tree.
 @param tree the tree to append this item to
 @param hfindex field index
 @param tvb the tv buffer of the current data
 @param bit_offset start of data in tvb expressed in bits
 @param no_of_chars number of 7bits characters to display
 @return the newly created item */
 proto_item *
proto_tree_add_ascii_7bits_item(proto_tree *tree, const int hfindex, tvbuff_t *tvb,
    const unsigned bit_offset, const int no_of_chars);

proto_item * proto_tree_add_checksum(proto_tree *tree, tvbuff_t *tvb, const unsigned offset,
        const int hf_checksum, const int hf_checksum_status, struct expert_field* bad_checksum_expert,
        packet_info *pinfo, uint32_t computed_checksum, const unsigned encoding, const unsigned flags);

proto_item* proto_tree_add_checksum_bytes(proto_tree *tree, tvbuff_t *tvb, const unsigned offset,
		const int hf_checksum, const int hf_checksum_status, struct expert_field* bad_checksum_expert,
		packet_info *pinfo, const uint8_t *computed_checksum, size_t checksum_len, const unsigned flags);

typedef enum
{
    PROTO_CHECKSUM_E_BAD = 0,
    PROTO_CHECKSUM_E_GOOD,
    PROTO_CHECKSUM_E_UNVERIFIED,
    PROTO_CHECKSUM_E_NOT_PRESENT,
    PROTO_CHECKSUM_E_ILLEGAL
} proto_checksum_enum_e;

#define PROTO_CHECKSUM_NO_FLAGS     0x00    /**< Don't use any flags */
#define PROTO_CHECKSUM_VERIFY       0x01    /**< Compare against computed checksum */
#define PROTO_CHECKSUM_GENERATED    0x02    /**< Checksum is generated only */
#define PROTO_CHECKSUM_IN_CKSUM     0x04    /**< Internet checksum routine used for computation */
#define PROTO_CHECKSUM_ZERO         0x08    /**< Computed checksum must be zero (but correct checksum can't be calculated) */
#define PROTO_CHECKSUM_NOT_PRESENT  0x10    /**< Checksum field is not present (Just populates status field) */

extern const value_string proto_checksum_vals[];
unsigned char proto_check_field_name(const char *field_name);
unsigned char proto_check_field_name_lower(const char *field_name);
const char * proto_custom_set(proto_tree* tree, GSList *field_id,
                             int occurrence,
                             char *result,
                             char *expr, const int size );
char * proto_custom_get_filter(struct epan_dissect *edt, GSList *field_id, int occurrence);
const char * hfinfo_char_value_format_display(int display, char buf[7], uint32_t value);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* proto.h */
