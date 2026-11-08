/* packet-rtp.c
 * Real-Time Transport Protocol, RFC 3550: header, payload type, SRTP when
 * the SDP of the call says so, telephone-event payloads to rtpevent.
 * Reduced from Wireshark's packet-rtp.c (Andreas Sikkema, Jason Lango,
 * Michael Lum and others): same field names, a subset of the features.
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include <string.h>
#include <epan/packet.h>
#include <epan/conversation.h>

#include "packet-rtp.h"

void proto_register_rtp(void);
void proto_reg_handoff_rtp(void);

static int proto_rtp;
static int proto_srtp;

static int hf_rtp_version;
static int hf_rtp_padding;
static int hf_rtp_extension;
static int hf_rtp_csrc_count;
static int hf_rtp_marker;
static int hf_rtp_payload_type;
static int hf_rtp_seq_nr;
static int hf_rtp_timestamp;
static int hf_rtp_ssrc;
static int hf_rtp_csrc_item;
static int hf_rtp_ext_profile;
static int hf_rtp_ext_length;
static int hf_rtp_hdr_ext;
static int hf_rtp_payload;
static int hf_rtp_padding_data;
static int hf_rtp_padding_count;
static int hf_rtp_setup;
static int hf_rtp_setup_frame;
static int hf_rtp_setup_method;
static int hf_srtp_enc_payload;
static int hf_srtp_auth_tag;

static int ett_rtp;
static int ett_rtp_setup;
static int ett_rtp_csrc;
static int ett_rtp_ext;

static dissector_handle_t rtp_handle;
static dissector_handle_t rtcp_handle;
static dissector_handle_t rtpevent_handle;

#define RTP_VERSION(b)  ((b) >> 6)
#define RTP_PADDING(b)  ((b) & 0x20)
#define RTP_EXT(b)      ((b) & 0x10)
#define RTP_CC(b)       ((b) & 0x0F)
#define RTP_MARKER(b)   ((b) & 0x80)
#define RTP_PT(b)       ((b) & 0x7F)

/* RFC 3551 */
static const value_string rtp_payload_type_vals[] = {
    {  0, "ITU-T G.711 PCMU" },
    {  3, "GSM 06.10" },
    {  4, "ITU-T G.723" },
    {  5, "DVI4 8000 samples/s" },
    {  6, "DVI4 16000 samples/s" },
    {  7, "Experimental linear predictive encoding" },
    {  8, "ITU-T G.711 PCMA" },
    {  9, "ITU-T G.722" },
    { 10, "16-bit uncomp. linear PCM stereo" },
    { 11, "16-bit uncomp. linear PCM mono" },
    { 12, "QCELP" },
    { 13, "Comfort noise (CN)" },
    { 14, "MPEG-I/II Audio" },
    { 15, "ITU-T G.728" },
    { 16, "DVI4 11025 samples/s" },
    { 17, "DVI4 22050 samples/s" },
    { 18, "ITU-T G.729" },
    { 25, "Sun CellB video encoding" },
    { 26, "JPEG-compressed video" },
    { 28, "'nv' program" },
    { 31, "ITU-T H.261" },
    { 32, "MPEG-I/II Video" },
    { 33, "MPEG-II transport streams" },
    { 34, "ITU-T H.263" },
    { 0, NULL }
};

void
rtp_add_address(packet_info *pinfo, const address *addr, unsigned port,
                const rtp_setup_info_t *info)
{
    address null_addr;
    conversation_t *conv;
    rtp_setup_info_t *saved;
    unsigned i;

    if (PINFO_FD_VISITED(pinfo) || port == 0 || addr->type == AT_NONE)
        return;
    clear_address(&null_addr);
    conv = find_conversation(info->setup_frame, addr, &null_addr,
                             CONVERSATION_UDP, port, 0, NO_ADDR_B | NO_PORT_B);
    if (!conv || conv->setup_frame != info->setup_frame)
        conv = conversation_new(info->setup_frame, addr, &null_addr,
                                CONVERSATION_UDP, port, 0, NO_ADDR2 | NO_PORT2);
    saved = wmem_new0(wmem_file_scope(), rtp_setup_info_t);
    saved->setup_frame = info->setup_frame;
    saved->srtp = info->srtp;
    saved->srtp_tag_len = info->srtp_tag_len;
    for (i = 0; i < array_length(info->dyn_name); i++)
        if (info->dyn_name[i])
            saved->dyn_name[i] = wmem_strdup(wmem_file_scope(), info->dyn_name[i]);
    conversation_add_proto_data(conv, proto_rtp, saved);
    conversation_set_dissector(conv, rtp_handle);
}

static const rtp_setup_info_t *
rtp_find_setup(packet_info *pinfo)
{
    conversation_t *conv;
    void *info = NULL;

    conv = find_conversation(pinfo->num, &pinfo->dst, &pinfo->src,
                             CONVERSATION_UDP, pinfo->destport, pinfo->srcport,
                             NO_ADDR_B | NO_PORT_B);
    if (conv)
        info = conversation_get_proto_data(conv, proto_rtp);
    if (!info) {
        conv = find_conversation(pinfo->num, &pinfo->src, &pinfo->dst,
                                 CONVERSATION_UDP, pinfo->srcport,
                                 pinfo->destport, NO_ADDR_B | NO_PORT_B);
        if (conv)
            info = conversation_get_proto_data(conv, proto_rtp);
    }
    return (const rtp_setup_info_t *)info;
}

static const char *
rtp_pt_name(packet_info *pinfo, unsigned pt, const rtp_setup_info_t *info)
{
    const char *name;

    if (pt >= RTP_DYN_PT_MIN && pt <= RTP_DYN_PT_MAX) {
        if (info && info->dyn_name[pt - RTP_DYN_PT_MIN])
            return info->dyn_name[pt - RTP_DYN_PT_MIN];
        return wmem_strdup_printf(pinfo->pool, "DynamicRTP-Type-%u", pt);
    }
    name = try_val_to_str(pt, rtp_payload_type_vals);
    if (name)
        return name;
    return wmem_strdup_printf(pinfo->pool, "Unknown (%u)", pt);
}

static int
dissect_rtp_common(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                   const rtp_setup_info_t *info)
{
    int len = tvb_reported_length(tvb);
    int offset = 12, payload_len, i;
    unsigned cc, pt, padding_count = 0, tag_len;
    uint8_t b0, b1;
    uint16_t seq;
    uint32_t ts, ssrc;
    bool srtp;
    const char *pt_name;
    proto_item *ti;
    proto_tree *rtp_tree, *sub;

    if (tvb_captured_length(tvb) < 12)
        return 0;
    b0 = tvb_get_uint8(tvb, 0);
    if (RTP_VERSION(b0) != 2)
        return 0;
    b1 = tvb_get_uint8(tvb, 1);
    /* RFC 5761: RTCP multiplexed on the RTP port */
    if (b1 >= 192 && b1 <= 223 && rtcp_handle)
        return call_dissector(rtcp_handle, tvb, pinfo, tree);

    cc = RTP_CC(b0);
    pt = RTP_PT(b1);
    seq = tvb_get_ntohs(tvb, 2);
    ts = tvb_get_ntohl(tvb, 4);
    ssrc = tvb_get_ntohl(tvb, 8);
    srtp = info && info->srtp;
    pt_name = rtp_pt_name(pinfo, pt, info);

    col_set_str(pinfo->cinfo, COL_PROTOCOL, srtp ? "SRTP" : "RTP");
    col_add_fstr(pinfo->cinfo, COL_INFO, "PT=%s, SSRC=0x%X, Seq=%u, Time=%u%s",
                 pt_name, ssrc, seq, ts, RTP_MARKER(b1) ? ", Mark" : "");

    ti = proto_tree_add_item(tree, srtp ? proto_srtp : proto_rtp, tvb, 0, -1, ENC_NA);
    rtp_tree = proto_item_add_subtree(ti, ett_rtp);

    if (info) {
        ti = proto_tree_add_string_format(rtp_tree, hf_rtp_setup, tvb, 0, 0, "",
                                          "Stream setup by SDP (frame %u)",
                                          info->setup_frame);
        proto_item_set_generated(ti);
        sub = proto_item_add_subtree(ti, ett_rtp_setup);
        ti = proto_tree_add_uint(sub, hf_rtp_setup_frame, tvb, 0, 0, info->setup_frame);
        proto_item_set_generated(ti);
        ti = proto_tree_add_string(sub, hf_rtp_setup_method, tvb, 0, 0, "SDP");
        proto_item_set_generated(ti);
    }

    proto_tree_add_item(rtp_tree, hf_rtp_version, tvb, 0, 1, ENC_BIG_ENDIAN);
    proto_tree_add_item(rtp_tree, hf_rtp_padding, tvb, 0, 1, ENC_BIG_ENDIAN);
    proto_tree_add_item(rtp_tree, hf_rtp_extension, tvb, 0, 1, ENC_BIG_ENDIAN);
    proto_tree_add_item(rtp_tree, hf_rtp_csrc_count, tvb, 0, 1, ENC_BIG_ENDIAN);
    proto_tree_add_item(rtp_tree, hf_rtp_marker, tvb, 1, 1, ENC_BIG_ENDIAN);
    proto_tree_add_uint_format_value(rtp_tree, hf_rtp_payload_type, tvb, 1, 1,
                                     pt, "%s (%u)", pt_name, pt);
    proto_tree_add_item(rtp_tree, hf_rtp_seq_nr, tvb, 2, 2, ENC_BIG_ENDIAN);
    proto_tree_add_item(rtp_tree, hf_rtp_timestamp, tvb, 4, 4, ENC_BIG_ENDIAN);
    proto_tree_add_item(rtp_tree, hf_rtp_ssrc, tvb, 8, 4, ENC_BIG_ENDIAN);

    if (cc) {
        sub = proto_tree_add_subtree_format(rtp_tree, tvb, offset, 4 * cc,
                                            ett_rtp_csrc, NULL,
                                            "Contributing Source identifiers (%u)", cc);
        for (i = 0; i < (int)cc; i++, offset += 4)
            proto_tree_add_item(sub, hf_rtp_csrc_item, tvb, offset, 4, ENC_BIG_ENDIAN);
    }
    if (RTP_EXT(b0)) {
        unsigned words = tvb_get_ntohs(tvb, offset + 2);
        sub = proto_tree_add_subtree(rtp_tree, tvb, offset, 4 + 4 * words,
                                     ett_rtp_ext, NULL, "Header extension");
        proto_tree_add_item(sub, hf_rtp_ext_profile, tvb, offset, 2, ENC_BIG_ENDIAN);
        proto_tree_add_item(sub, hf_rtp_ext_length, tvb, offset + 2, 2, ENC_BIG_ENDIAN);
        if (words)
            proto_tree_add_item(sub, hf_rtp_hdr_ext, tvb, offset + 4, 4 * words, ENC_NA);
        offset += 4 + 4 * words;
    }

    if (srtp) {
        /* the padding is encrypted, the tag follows the encrypted part */
        tag_len = info->srtp_tag_len;
        payload_len = len - offset - (int)tag_len;
        if (payload_len < 0) {
            payload_len = len - offset;
            tag_len = 0;
        }
        if (payload_len > 0)
            proto_tree_add_item(rtp_tree, hf_srtp_enc_payload, tvb, offset,
                                payload_len, ENC_NA);
        if (tag_len)
            proto_tree_add_item(rtp_tree, hf_srtp_auth_tag, tvb,
                                offset + payload_len, tag_len, ENC_NA);
        return len;
    }

    if (RTP_PADDING(b0) && tvb_captured_length(tvb) == (unsigned)len && len > offset)
        padding_count = tvb_get_uint8(tvb, len - 1);
    payload_len = len - offset - (int)padding_count;
    if (payload_len < 0) {
        payload_len = len - offset;
        padding_count = 0;
    }
    if (payload_len > 0) {
        const char *dyn = NULL;
        if (info && pt >= RTP_DYN_PT_MIN && pt <= RTP_DYN_PT_MAX)
            dyn = info->dyn_name[pt - RTP_DYN_PT_MIN];
        if (dyn && rtpevent_handle && g_ascii_strcasecmp(dyn, "telephone-event") == 0)
            call_dissector(rtpevent_handle,
                           tvb_new_subset_length(tvb, offset, payload_len),
                           pinfo, tree);
        else
            proto_tree_add_item(rtp_tree, hf_rtp_payload, tvb, offset,
                                payload_len, ENC_NA);
    }
    if (padding_count) {
        if (padding_count > 1)
            proto_tree_add_item(rtp_tree, hf_rtp_padding_data, tvb, len - padding_count,
                                padding_count - 1, ENC_NA);
        proto_tree_add_item(rtp_tree, hf_rtp_padding_count, tvb, len - 1, 1,
                            ENC_BIG_ENDIAN);
    }
    return len;
}

static int
dissect_rtp(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void *data)
{
    return dissect_rtp_common(tvb, pinfo, tree, rtp_find_setup(pinfo));
}

/* RTP seen without its SDP (a capture started during the call): version 2,
 * no padding, extension nor CSRC, an audio or dynamic payload type, even
 * unprivileged ports on both sides. */
static bool
dissect_rtp_heur(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void *data)
{
    uint8_t b0, b1;
    unsigned pt;

    if (tvb_captured_length(tvb) < 12)
        return false;
    b0 = tvb_get_uint8(tvb, 0);
    b1 = tvb_get_uint8(tvb, 1);
    if (b0 != 0x80)
        return false;
    pt = RTP_PT(b1);
    if (!(pt == 0 || pt == 3 || pt == 8 || pt == 9 || pt == 13 || pt == 18 ||
          (pt >= RTP_DYN_PT_MIN && pt <= RTP_DYN_PT_MAX)))
        return false;
    if (pinfo->srcport < 1024 || pinfo->destport < 1024 ||
        (pinfo->srcport & 1) || (pinfo->destport & 1))
        return false;
    return dissect_rtp_common(tvb, pinfo, tree, NULL) > 0;
}

void
proto_register_rtp(void)
{
    static hf_register_info hf[] = {
        { &hf_rtp_version,
          { "Version", "rtp.version", FT_UINT8, BASE_DEC, NULL, 0xC0,
            NULL, HFILL }},
        { &hf_rtp_padding,
          { "Padding", "rtp.padding", FT_BOOLEAN, 8, NULL, 0x20,
            NULL, HFILL }},
        { &hf_rtp_extension,
          { "Extension", "rtp.ext", FT_BOOLEAN, 8, NULL, 0x10,
            NULL, HFILL }},
        { &hf_rtp_csrc_count,
          { "Contributing source identifiers count", "rtp.cc", FT_UINT8,
            BASE_DEC, NULL, 0x0F, NULL, HFILL }},
        { &hf_rtp_marker,
          { "Marker", "rtp.marker", FT_BOOLEAN, 8, NULL, 0x80,
            NULL, HFILL }},
        { &hf_rtp_payload_type,
          { "Payload type", "rtp.p_type", FT_UINT8, BASE_DEC, NULL, 0x7F,
            NULL, HFILL }},
        { &hf_rtp_seq_nr,
          { "Sequence number", "rtp.seq", FT_UINT16, BASE_DEC, NULL, 0x0,
            NULL, HFILL }},
        { &hf_rtp_timestamp,
          { "Timestamp", "rtp.timestamp", FT_UINT32, BASE_DEC, NULL, 0x0,
            NULL, HFILL }},
        { &hf_rtp_ssrc,
          { "Synchronization Source identifier", "rtp.ssrc", FT_UINT32,
            BASE_HEX_DEC, NULL, 0x0, NULL, HFILL }},
        { &hf_rtp_csrc_item,
          { "CSRC item", "rtp.csrc.item", FT_UINT32, BASE_HEX_DEC, NULL, 0x0,
            NULL, HFILL }},
        { &hf_rtp_ext_profile,
          { "Defined by profile", "rtp.ext.profile", FT_UINT16, BASE_HEX,
            NULL, 0x0, NULL, HFILL }},
        { &hf_rtp_ext_length,
          { "Extension length", "rtp.ext.len", FT_UINT16, BASE_DEC, NULL, 0x0,
            NULL, HFILL }},
        { &hf_rtp_hdr_ext,
          { "Header extension", "rtp.hdr_ext", FT_BYTES, BASE_NONE, NULL, 0x0,
            NULL, HFILL }},
        { &hf_rtp_payload,
          { "Payload", "rtp.payload", FT_BYTES, BASE_NONE, NULL, 0x0,
            NULL, HFILL }},
        { &hf_rtp_padding_data,
          { "Padding data", "rtp.padding.data", FT_BYTES, BASE_NONE, NULL, 0x0,
            NULL, HFILL }},
        { &hf_rtp_padding_count,
          { "Padding count", "rtp.padding.count", FT_UINT8, BASE_DEC, NULL, 0x0,
            NULL, HFILL }},
        { &hf_rtp_setup,
          { "Stream setup", "rtp.setup", FT_STRING, BASE_NONE, NULL, 0x0,
            "Stream setup, method and frame number", HFILL }},
        { &hf_rtp_setup_frame,
          { "Setup frame", "rtp.setup-frame", FT_FRAMENUM, BASE_NONE, NULL, 0x0,
            "Frame that set up this stream", HFILL }},
        { &hf_rtp_setup_method,
          { "Setup Method", "rtp.setup-method", FT_STRING, BASE_NONE, NULL, 0x0,
            "Method used to set up this stream", HFILL }},
    };
    static hf_register_info hf_srtp[] = {
        { &hf_srtp_enc_payload,
          { "SRTP Encrypted Payload", "srtp.enc_payload", FT_BYTES, BASE_NONE,
            NULL, 0x0, NULL, HFILL }},
        { &hf_srtp_auth_tag,
          { "SRTP Auth Tag", "srtp.auth_tag", FT_BYTES, BASE_NONE, NULL, 0x0,
            NULL, HFILL }},
    };
    static int *ett[] = {
        &ett_rtp,
        &ett_rtp_setup,
        &ett_rtp_csrc,
        &ett_rtp_ext,
    };

    proto_rtp = proto_register_protocol("Real-Time Transport Protocol", "RTP", "rtp");
    proto_srtp = proto_register_protocol("Secure Real-time Transport Protocol",
                                         "SRTP", "srtp");
    proto_register_field_array(proto_rtp, hf, array_length(hf));
    proto_register_field_array(proto_srtp, hf_srtp, array_length(hf_srtp));
    proto_register_subtree_array(ett, array_length(ett));
    rtp_handle = register_dissector("rtp", dissect_rtp, proto_rtp);
}

void
proto_reg_handoff_rtp(void)
{
    rtcp_handle = find_dissector("rtcp");
    rtpevent_handle = find_dissector("rtpevent");
    heur_dissector_add("udp", dissect_rtp_heur, "RTP over UDP", "rtp_udp",
                       proto_rtp, HEURISTIC_ENABLE);
}

/*
 * Editor modelines  -  https://www.wireshark.org/tools/modelines.html
 *
 * Local variables:
 * c-basic-offset: 4
 * tab-width: 8
 * indent-tabs-mode: nil
 * End:
 *
 * vi: set shiftwidth=4 tabstop=8 expandtab:
 * :indentSize=4:tabSize=8:noTabs=true:
 */
