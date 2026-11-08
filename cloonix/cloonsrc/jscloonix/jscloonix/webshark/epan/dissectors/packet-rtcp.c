/* packet-rtcp.c
 * RTP Control Protocol, RFC 3550: compound packets of sender and receiver
 * reports (loss, jitter, round trip), source descriptions, BYE and APP;
 * feedback (RFC 4585) and XR (RFC 3611) packets shown by their header.
 * Reduced from Wireshark's packet-rtcp.c (Andreas Sikkema and others):
 * same field names, a subset of the features.
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include <epan/packet.h>
#include <epan/conversation.h>

#include "packet-rtp.h"

void proto_register_rtcp(void);
void proto_reg_handoff_rtcp(void);

static int proto_rtcp;

static int hf_rtcp_version;
static int hf_rtcp_padding;
static int hf_rtcp_rc;
static int hf_rtcp_sc;
static int hf_rtcp_pt;
static int hf_rtcp_length;
static int hf_rtcp_ssrc_sender;
static int hf_rtcp_ntp_msw;
static int hf_rtcp_ntp_lsw;
static int hf_rtcp_rtp_timestamp;
static int hf_rtcp_sender_pkt_cnt;
static int hf_rtcp_sender_oct_cnt;
static int hf_rtcp_ssrc_source;
static int hf_rtcp_ssrc_fraction;
static int hf_rtcp_ssrc_cum_nr;
static int hf_rtcp_ssrc_ext_high_seq;
static int hf_rtcp_ssrc_high_seq;
static int hf_rtcp_ssrc_high_cycles;
static int hf_rtcp_ssrc_jitter;
static int hf_rtcp_ssrc_lsr;
static int hf_rtcp_ssrc_dlsr;
static int hf_rtcp_sdes_ssrc_csrc;
static int hf_rtcp_sdes_type;
static int hf_rtcp_sdes_length;
static int hf_rtcp_sdes_text;
static int hf_rtcp_bye_reason;
static int hf_rtcp_app_subtype;
static int hf_rtcp_app_name;
static int hf_rtcp_app_data;
static int hf_rtcp_ssrc_media_source;
static int hf_rtcp_fci;
static int hf_rtcp_setup_frame;

static int ett_rtcp;
static int ett_rtcp_packet;
static int ett_rtcp_report;
static int ett_rtcp_sdes;

static dissector_handle_t rtcp_handle;

#define RTCP_SR    200
#define RTCP_RR    201
#define RTCP_SDES  202
#define RTCP_BYE   203
#define RTCP_APP   204
#define RTCP_RTPFB 205
#define RTCP_PSFB  206
#define RTCP_XR    207

static const value_string rtcp_packet_type_vals[] = {
    { RTCP_SR,    "Sender Report" },
    { RTCP_RR,    "Receiver Report" },
    { RTCP_SDES,  "Source description" },
    { RTCP_BYE,   "Goodbye" },
    { RTCP_APP,   "Application specific" },
    { RTCP_RTPFB, "Generic RTP Feedback" },
    { RTCP_PSFB,  "Payload-specific Feedback" },
    { RTCP_XR,    "Extended report (RFC 3611)" },
    { 0, NULL }
};

static const value_string rtcp_sdes_type_vals[] = {
    { 0, "END" },
    { 1, "CNAME (user and domain)" },
    { 2, "NAME (common name)" },
    { 3, "EMAIL (e-mail address)" },
    { 4, "PHONE (phone number)" },
    { 5, "LOC (geographic location)" },
    { 6, "TOOL (name/version of source app)" },
    { 7, "NOTE (note about source)" },
    { 8, "PRIV (private extensions)" },
    { 0, NULL }
};

void
rtcp_add_address(packet_info *pinfo, const address *addr, unsigned port,
                 uint32_t setup_frame)
{
    address null_addr;
    conversation_t *conv;
    uint32_t *saved;

    if (PINFO_FD_VISITED(pinfo) || port == 0 || addr->type == AT_NONE)
        return;
    clear_address(&null_addr);
    conv = find_conversation(setup_frame, addr, &null_addr, CONVERSATION_UDP,
                             port, 0, NO_ADDR_B | NO_PORT_B);
    if (!conv || conv->setup_frame != setup_frame)
        conv = conversation_new(setup_frame, addr, &null_addr, CONVERSATION_UDP,
                                port, 0, NO_ADDR2 | NO_PORT2);
    saved = wmem_new(wmem_file_scope(), uint32_t);
    *saved = setup_frame;
    conversation_add_proto_data(conv, proto_rtcp, saved);
    conversation_set_dissector(conv, rtcp_handle);
}

static uint32_t
rtcp_find_setup(packet_info *pinfo)
{
    conversation_t *conv;
    uint32_t *frame = NULL;

    conv = find_conversation(pinfo->num, &pinfo->dst, &pinfo->src,
                             CONVERSATION_UDP, pinfo->destport, pinfo->srcport,
                             NO_ADDR_B | NO_PORT_B);
    if (conv)
        frame = (uint32_t *)conversation_get_proto_data(conv, proto_rtcp);
    return frame ? *frame : 0;
}

/* The compound must hold whole packets of version 2, the first an SR or RR. */
static bool
rtcp_compound_ok(tvbuff_t *tvb)
{
    int len = tvb_captured_length(tvb), offset = 0;
    unsigned pt;

    if (len < 8 || len != (int)tvb_reported_length(tvb))
        return false;
    pt = tvb_get_uint8(tvb, 1);
    if (pt != RTCP_SR && pt != RTCP_RR)
        return false;
    while (offset + 4 <= len) {
        if ((tvb_get_uint8(tvb, offset) & 0xC0) != 0x80)
            return false;
        pt = tvb_get_uint8(tvb, offset + 1);
        if (pt < 192 || pt > 223)
            return false;
        offset += 4 * (tvb_get_ntohs(tvb, offset + 2) + 1);
    }
    return offset == len;
}

static int
dissect_rtcp_report_block(tvbuff_t *tvb, int offset, proto_tree *tree, unsigned num)
{
    proto_tree *src_tree, *seq_tree;
    proto_item *ti;
    uint32_t fraction, dlsr;
    int32_t cum;

    src_tree = proto_tree_add_subtree_format(tree, tvb, offset, 24, ett_rtcp_report,
                                             NULL, "Source %u", num);
    proto_tree_add_item(src_tree, hf_rtcp_ssrc_source, tvb, offset, 4, ENC_BIG_ENDIAN);
    fraction = tvb_get_uint8(tvb, offset + 4);
    proto_tree_add_uint_format_value(src_tree, hf_rtcp_ssrc_fraction, tvb, offset + 4, 1,
                                     fraction, "%u / 256", fraction);
    cum = (int32_t)(tvb_get_ntoh24(tvb, offset + 5) << 8) >> 8;
    proto_tree_add_int(src_tree, hf_rtcp_ssrc_cum_nr, tvb, offset + 5, 3, cum);
    ti = proto_tree_add_item(src_tree, hf_rtcp_ssrc_ext_high_seq, tvb, offset + 8, 4,
                             ENC_BIG_ENDIAN);
    seq_tree = proto_item_add_subtree(ti, ett_rtcp_report);
    proto_tree_add_item(seq_tree, hf_rtcp_ssrc_high_cycles, tvb, offset + 8, 2, ENC_BIG_ENDIAN);
    proto_tree_add_item(seq_tree, hf_rtcp_ssrc_high_seq, tvb, offset + 10, 2, ENC_BIG_ENDIAN);
    proto_tree_add_item(src_tree, hf_rtcp_ssrc_jitter, tvb, offset + 12, 4, ENC_BIG_ENDIAN);
    proto_tree_add_item(src_tree, hf_rtcp_ssrc_lsr, tvb, offset + 16, 4, ENC_BIG_ENDIAN);
    dlsr = tvb_get_ntohl(tvb, offset + 20);
    proto_tree_add_uint_format_value(src_tree, hf_rtcp_ssrc_dlsr, tvb, offset + 20, 4,
                                     dlsr, "%u (%.3f seconds)", dlsr, dlsr / 65536.0);
    return offset + 24;
}

static void
dissect_rtcp_sdes(tvbuff_t *tvb, int offset, int end, proto_tree *tree, unsigned count)
{
    proto_tree *chunk;
    proto_item *chunk_item;
    unsigned i, type, len;
    int start;

    for (i = 0; i < count && offset + 4 <= end; i++) {
        start = offset;
        chunk = proto_tree_add_subtree_format(tree, tvb, offset, -1, ett_rtcp_sdes,
                                              &chunk_item, "Chunk %u", i + 1);
        proto_tree_add_item(chunk, hf_rtcp_sdes_ssrc_csrc, tvb, offset, 4, ENC_BIG_ENDIAN);
        offset += 4;
        while (offset < end) {
            type = tvb_get_uint8(tvb, offset);
            proto_tree_add_item(chunk, hf_rtcp_sdes_type, tvb, offset, 1, ENC_BIG_ENDIAN);
            offset++;
            if (type == 0) {
                /* END, then padding to the next 32-bit boundary */
                offset = (offset + 3) & ~3;
                break;
            }
            if (offset >= end)
                break;
            len = tvb_get_uint8(tvb, offset);
            proto_tree_add_item(chunk, hf_rtcp_sdes_length, tvb, offset, 1, ENC_BIG_ENDIAN);
            offset++;
            if (len && offset + (int)len <= end)
                proto_tree_add_item(chunk, hf_rtcp_sdes_text, tvb, offset, len, ENC_ASCII);
            offset += len;
        }
        proto_item_set_len(chunk_item, offset - start);
    }
}

static int
dissect_rtcp(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void *data)
{
    int len = tvb_captured_length(tvb), offset = 0, end, i;
    unsigned b0, pt, count;
    uint32_t setup;
    proto_item *ti;
    proto_tree *rtcp_tree, *pkt_tree;

    col_set_str(pinfo->cinfo, COL_PROTOCOL, "RTCP");
    col_clear(pinfo->cinfo, COL_INFO);

    ti = proto_tree_add_item(tree, proto_rtcp, tvb, 0, -1, ENC_NA);
    rtcp_tree = proto_item_add_subtree(ti, ett_rtcp);
    setup = rtcp_find_setup(pinfo);
    if (setup) {
        ti = proto_tree_add_uint(rtcp_tree, hf_rtcp_setup_frame, tvb, 0, 0, setup);
        proto_item_set_generated(ti);
    }

    while (offset + 4 <= len) {
        b0 = tvb_get_uint8(tvb, offset);
        pt = tvb_get_uint8(tvb, offset + 1);
        count = b0 & 0x1F;
        end = offset + 4 * (tvb_get_ntohs(tvb, offset + 2) + 1);
        if (end > len)
            end = len;
        col_append_sep_str(pinfo->cinfo, COL_INFO, "   ",
                           val_to_str_const(pt, rtcp_packet_type_vals, "Unknown"));

        pkt_tree = proto_tree_add_subtree(rtcp_tree, tvb, offset, end - offset,
                                          ett_rtcp_packet, NULL,
                                          val_to_str_const(pt, rtcp_packet_type_vals,
                                                           "Unknown"));
        proto_tree_add_item(pkt_tree, hf_rtcp_version, tvb, offset, 1, ENC_BIG_ENDIAN);
        proto_tree_add_item(pkt_tree, hf_rtcp_padding, tvb, offset, 1, ENC_BIG_ENDIAN);
        if (pt == RTCP_SDES || pt == RTCP_BYE)
            proto_tree_add_item(pkt_tree, hf_rtcp_sc, tvb, offset, 1, ENC_BIG_ENDIAN);
        else if (pt == RTCP_SR || pt == RTCP_RR)
            proto_tree_add_item(pkt_tree, hf_rtcp_rc, tvb, offset, 1, ENC_BIG_ENDIAN);
        proto_tree_add_item(pkt_tree, hf_rtcp_pt, tvb, offset + 1, 1, ENC_BIG_ENDIAN);
        proto_tree_add_item(pkt_tree, hf_rtcp_length, tvb, offset + 2, 2, ENC_BIG_ENDIAN);
        i = offset + 4;

        switch (pt) {
        case RTCP_SR:
        case RTCP_RR:
            if (i + 4 > end)
                break;
            proto_tree_add_item(pkt_tree, hf_rtcp_ssrc_sender, tvb, i, 4, ENC_BIG_ENDIAN);
            i += 4;
            if (pt == RTCP_SR && i + 20 <= end) {
                proto_tree_add_item(pkt_tree, hf_rtcp_ntp_msw, tvb, i, 4, ENC_BIG_ENDIAN);
                proto_tree_add_item(pkt_tree, hf_rtcp_ntp_lsw, tvb, i + 4, 4, ENC_BIG_ENDIAN);
                proto_tree_add_item(pkt_tree, hf_rtcp_rtp_timestamp, tvb, i + 8, 4, ENC_BIG_ENDIAN);
                proto_tree_add_item(pkt_tree, hf_rtcp_sender_pkt_cnt, tvb, i + 12, 4, ENC_BIG_ENDIAN);
                proto_tree_add_item(pkt_tree, hf_rtcp_sender_oct_cnt, tvb, i + 16, 4, ENC_BIG_ENDIAN);
                i += 20;
            }
            for (unsigned n = 0; n < count && i + 24 <= end; n++)
                i = dissect_rtcp_report_block(tvb, i, pkt_tree, n + 1);
            break;
        case RTCP_SDES:
            dissect_rtcp_sdes(tvb, i, end, pkt_tree, count);
            break;
        case RTCP_BYE:
            for (unsigned n = 0; n < count && i + 4 <= end; n++, i += 4)
                proto_tree_add_item(pkt_tree, hf_rtcp_ssrc_source, tvb, i, 4, ENC_BIG_ENDIAN);
            if (i < end) {
                unsigned rlen = tvb_get_uint8(tvb, i);
                if (rlen && i + 1 + (int)rlen <= end)
                    proto_tree_add_item(pkt_tree, hf_rtcp_bye_reason, tvb, i + 1,
                                        rlen, ENC_ASCII);
            }
            break;
        case RTCP_APP:
            proto_tree_add_item(pkt_tree, hf_rtcp_app_subtype, tvb, offset, 1, ENC_BIG_ENDIAN);
            if (i + 8 > end)
                break;
            proto_tree_add_item(pkt_tree, hf_rtcp_ssrc_source, tvb, i, 4, ENC_BIG_ENDIAN);
            proto_tree_add_item(pkt_tree, hf_rtcp_app_name, tvb, i + 4, 4, ENC_ASCII);
            if (i + 8 < end)
                proto_tree_add_item(pkt_tree, hf_rtcp_app_data, tvb, i + 8,
                                    end - i - 8, ENC_NA);
            break;
        case RTCP_RTPFB:
        case RTCP_PSFB:
        case RTCP_XR:
            if (i + 4 > end)
                break;
            proto_tree_add_item(pkt_tree, hf_rtcp_ssrc_sender, tvb, i, 4, ENC_BIG_ENDIAN);
            i += 4;
            if (pt != RTCP_XR && i + 4 <= end) {
                proto_tree_add_item(pkt_tree, hf_rtcp_ssrc_media_source, tvb, i, 4,
                                    ENC_BIG_ENDIAN);
                i += 4;
            }
            if (i < end)
                proto_tree_add_item(pkt_tree, hf_rtcp_fci, tvb, i, end - i, ENC_NA);
            break;
        default:
            break;
        }
        if (end <= offset)
            break;
        offset = end;
    }
    return len;
}

static bool
dissect_rtcp_heur(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void *data)
{
    /* RTCP goes to the odd port above its RTP */
    if (pinfo->srcport < 1025 || pinfo->destport < 1025 ||
        !(pinfo->srcport & 1) || !(pinfo->destport & 1))
        return false;
    if (!rtcp_compound_ok(tvb))
        return false;
    dissect_rtcp(tvb, pinfo, tree, data);
    return true;
}

void
proto_register_rtcp(void)
{
    static hf_register_info hf[] = {
        { &hf_rtcp_version,
          { "Version", "rtcp.version", FT_UINT8, BASE_DEC, NULL, 0xC0,
            NULL, HFILL }},
        { &hf_rtcp_padding,
          { "Padding", "rtcp.padding", FT_BOOLEAN, 8, NULL, 0x20,
            NULL, HFILL }},
        { &hf_rtcp_rc,
          { "Reception report count", "rtcp.rc", FT_UINT8, BASE_DEC, NULL, 0x1F,
            NULL, HFILL }},
        { &hf_rtcp_sc,
          { "Source count", "rtcp.sc", FT_UINT8, BASE_DEC, NULL, 0x1F,
            NULL, HFILL }},
        { &hf_rtcp_pt,
          { "Packet type", "rtcp.pt", FT_UINT8, BASE_DEC,
            VALS(rtcp_packet_type_vals), 0x0, NULL, HFILL }},
        { &hf_rtcp_length,
          { "Length", "rtcp.length", FT_UINT16, BASE_DEC, NULL, 0x0,
            "32-bit words, minus one, of this packet", HFILL }},
        { &hf_rtcp_ssrc_sender,
          { "Sender SSRC", "rtcp.senderssrc", FT_UINT32, BASE_HEX_DEC, NULL, 0x0,
            NULL, HFILL }},
        { &hf_rtcp_ntp_msw,
          { "Timestamp, MSW", "rtcp.timestamp.ntp.msw", FT_UINT32, BASE_DEC,
            NULL, 0x0, NULL, HFILL }},
        { &hf_rtcp_ntp_lsw,
          { "Timestamp, LSW", "rtcp.timestamp.ntp.lsw", FT_UINT32, BASE_DEC,
            NULL, 0x0, NULL, HFILL }},
        { &hf_rtcp_rtp_timestamp,
          { "RTP timestamp", "rtcp.timestamp.rtp", FT_UINT32, BASE_DEC, NULL, 0x0,
            NULL, HFILL }},
        { &hf_rtcp_sender_pkt_cnt,
          { "Sender's packet count", "rtcp.sender.packetcount", FT_UINT32,
            BASE_DEC, NULL, 0x0, NULL, HFILL }},
        { &hf_rtcp_sender_oct_cnt,
          { "Sender's octet count", "rtcp.sender.octetcount", FT_UINT32,
            BASE_DEC, NULL, 0x0, NULL, HFILL }},
        { &hf_rtcp_ssrc_source,
          { "Identifier", "rtcp.ssrc.identifier", FT_UINT32, BASE_HEX_DEC, NULL,
            0x0, NULL, HFILL }},
        { &hf_rtcp_ssrc_fraction,
          { "Fraction lost", "rtcp.ssrc.fraction", FT_UINT8, BASE_DEC, NULL, 0x0,
            NULL, HFILL }},
        { &hf_rtcp_ssrc_cum_nr,
          { "Cumulative number of packets lost", "rtcp.ssrc.cum_nr", FT_INT24,
            BASE_DEC, NULL, 0x0, NULL, HFILL }},
        { &hf_rtcp_ssrc_ext_high_seq,
          { "Extended highest sequence number received", "rtcp.ssrc.ext_high",
            FT_UINT32, BASE_DEC, NULL, 0x0, NULL, HFILL }},
        { &hf_rtcp_ssrc_high_seq,
          { "Highest sequence number received", "rtcp.ssrc.high_seq", FT_UINT16,
            BASE_DEC, NULL, 0x0, NULL, HFILL }},
        { &hf_rtcp_ssrc_high_cycles,
          { "Sequence number cycles count", "rtcp.ssrc.high_cycles", FT_UINT16,
            BASE_DEC, NULL, 0x0, NULL, HFILL }},
        { &hf_rtcp_ssrc_jitter,
          { "Interarrival jitter", "rtcp.ssrc.jitter", FT_UINT32, BASE_DEC, NULL,
            0x0, "In RTP timestamp units", HFILL }},
        { &hf_rtcp_ssrc_lsr,
          { "Last SR timestamp", "rtcp.ssrc.lsr", FT_UINT32, BASE_DEC, NULL, 0x0,
            NULL, HFILL }},
        { &hf_rtcp_ssrc_dlsr,
          { "Delay since last SR timestamp", "rtcp.ssrc.dlsr", FT_UINT32, BASE_DEC,
            NULL, 0x0, NULL, HFILL }},
        { &hf_rtcp_sdes_ssrc_csrc,
          { "SSRC / CSRC identifier", "rtcp.sdes.ssrc_csrc", FT_UINT32,
            BASE_HEX_DEC, NULL, 0x0, NULL, HFILL }},
        { &hf_rtcp_sdes_type,
          { "Type", "rtcp.sdes.type", FT_UINT8, BASE_DEC,
            VALS(rtcp_sdes_type_vals), 0x0, NULL, HFILL }},
        { &hf_rtcp_sdes_length,
          { "Length", "rtcp.sdes.length", FT_UINT8, BASE_DEC, NULL, 0x0,
            NULL, HFILL }},
        { &hf_rtcp_sdes_text,
          { "Text", "rtcp.sdes.text", FT_STRING, BASE_NONE, NULL, 0x0,
            NULL, HFILL }},
        { &hf_rtcp_bye_reason,
          { "Reason for leaving", "rtcp.bye.reason", FT_STRING, BASE_NONE, NULL,
            0x0, NULL, HFILL }},
        { &hf_rtcp_app_subtype,
          { "Subtype", "rtcp.app.subtype", FT_UINT8, BASE_DEC, NULL, 0x1F,
            NULL, HFILL }},
        { &hf_rtcp_app_name,
          { "Name (ASCII)", "rtcp.app.name", FT_STRING, BASE_NONE, NULL, 0x0,
            NULL, HFILL }},
        { &hf_rtcp_app_data,
          { "Application specific data", "rtcp.app.data", FT_BYTES, BASE_NONE,
            NULL, 0x0, NULL, HFILL }},
        { &hf_rtcp_ssrc_media_source,
          { "Media source SSRC", "rtcp.mediassrc", FT_UINT32, BASE_HEX_DEC, NULL,
            0x0, NULL, HFILL }},
        { &hf_rtcp_fci,
          { "Feedback control information", "rtcp.fci", FT_BYTES, BASE_NONE,
            NULL, 0x0, NULL, HFILL }},
        { &hf_rtcp_setup_frame,
          { "Setup frame", "rtcp.setup-frame", FT_FRAMENUM, BASE_NONE, NULL, 0x0,
            "Frame of the SDP that set up this stream", HFILL }},
    };
    static int *ett[] = {
        &ett_rtcp,
        &ett_rtcp_packet,
        &ett_rtcp_report,
        &ett_rtcp_sdes,
    };

    proto_rtcp = proto_register_protocol("Real-time Transport Control Protocol",
                                         "RTCP", "rtcp");
    proto_register_field_array(proto_rtcp, hf, array_length(hf));
    proto_register_subtree_array(ett, array_length(ett));
    rtcp_handle = register_dissector("rtcp", dissect_rtcp, proto_rtcp);
}

void
proto_reg_handoff_rtcp(void)
{
    heur_dissector_add("udp", dissect_rtcp_heur, "RTCP over UDP", "rtcp_udp",
                       proto_rtcp, HEURISTIC_ENABLE);
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
