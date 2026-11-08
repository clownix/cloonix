/* packet-sdp.c
 * Session Description Protocol, RFC 4566: the lines of an offer or an
 * answer, and the RTP and RTCP streams of its media set up for the RTP and
 * RTCP dissectors, with the names of the dynamic payload types and SRTP.
 * Reduced from Wireshark's packet-sdp.c (Jason Lango and others): same
 * field names, a subset of the features.
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <epan/packet.h>
#include <wsutil/inet_addr.h>

#include "packet-rtp.h"

void proto_register_sdp(void);
void proto_reg_handoff_sdp(void);

static int proto_sdp;

static int hf_sdp_version;
static int hf_sdp_owner;
static int hf_sdp_session_name;
static int hf_sdp_session_info;
static int hf_sdp_uri;
static int hf_sdp_email;
static int hf_sdp_phone;
static int hf_sdp_connection_info;
static int hf_sdp_conn_nettype;
static int hf_sdp_conn_addrtype;
static int hf_sdp_conn_addr;
static int hf_sdp_bandwidth;
static int hf_sdp_time;
static int hf_sdp_repeat_time;
static int hf_sdp_timezone;
static int hf_sdp_key;
static int hf_sdp_session_attr;
static int hf_sdp_media;
static int hf_sdp_media_media;
static int hf_sdp_media_port;
static int hf_sdp_media_proto;
static int hf_sdp_media_format;
static int hf_sdp_media_title;
static int hf_sdp_media_attr;
static int hf_sdp_attr_field;
static int hf_sdp_attr_value;
static int hf_sdp_unknown;

static int ett_sdp;
static int ett_sdp_line;
static int ett_sdp_media;

#define SDP_MAX_MEDIA  8

typedef struct {
    char conn[64];
    unsigned port;
    char proto[32];
    unsigned rtcp_port;
    bool rtcp_mux;
    rtp_setup_info_t info;
} sdp_media_t;

typedef struct {
    char conn[64];
    unsigned nmedia;
    sdp_media_t media[SDP_MAX_MEDIA];
} sdp_session_t;

static const value_string sdp_static_fmt_vals[] = {
    {  0, "ITU-T G.711 PCMU" },
    {  3, "GSM 06.10" },
    {  4, "ITU-T G.723" },
    {  8, "ITU-T G.711 PCMA" },
    {  9, "ITU-T G.722" },
    { 13, "Comfort noise (CN)" },
    { 18, "ITU-T G.729" },
    { 34, "ITU-T H.263" },
    { 0, NULL }
};

/* "IN IP4 10.0.0.1" -> "10.0.0.1" into out */
static void
sdp_conn_address(const char *value, char *out, size_t size)
{
    char **parts = g_strsplit_set(value, " ", 4);

    out[0] = 0;
    if (parts[0] && parts[1] && parts[2]) {
        char *slash = strchr(parts[2], '/');
        if (slash)
            *slash = 0;
        g_strlcpy(out, parts[2], size);
    }
    g_strfreev(parts);
}

static bool
sdp_set_address(const char *text, address *addr, ws_in4_addr *v4, ws_in6_addr *v6)
{
    if (ws_inet_pton4(text, v4)) {
        set_address(addr, AT_IPv4, 4, v4);
        return true;
    }
    if (ws_inet_pton6(text, v6)) {
        set_address(addr, AT_IPv6, 16, v6);
        return true;
    }
    return false;
}

/* What the streams need, from one line; called for each line in order. */
static void
sdp_learn(wmem_allocator_t *scope, sdp_session_t *s, char type, const char *value)
{
    sdp_media_t *m;

    if (s->nmedia > SDP_MAX_MEDIA)
        return;
    if (type == 'm') {
        char **parts;
        if (s->nmedia == SDP_MAX_MEDIA) {
            /* the media past the last kept one, and their lines, are ignored */
            s->nmedia++;
            return;
        }
        m = &s->media[s->nmedia++];
        parts = g_strsplit_set(value, " ", -1);
        if (parts[0] && parts[1]) {
            m->port = (unsigned)strtoul(parts[1], NULL, 10);
            if (parts[2])
                g_strlcpy(m->proto, parts[2], sizeof(m->proto));
        }
        g_strfreev(parts);
        m->info.srtp_tag_len = 10;
        return;
    }
    m = s->nmedia ? &s->media[s->nmedia - 1] : NULL;
    if (type == 'c') {
        sdp_conn_address(value, m ? m->conn : s->conn, m ? sizeof(m->conn) : sizeof(s->conn));
        return;
    }
    if (type != 'a' || !m)
        return;
    if (g_ascii_strncasecmp(value, "rtpmap:", 7) == 0) {
        char *end;
        unsigned pt = (unsigned)strtoul(value + 7, &end, 10);
        if (pt >= RTP_DYN_PT_MIN && pt <= RTP_DYN_PT_MAX && *end == ' ') {
            char *name = wmem_strdup(scope, end + 1), *slash = strchr(name, '/');
            if (slash)
                *slash = 0;
            m->info.dyn_name[pt - RTP_DYN_PT_MIN] = name;
        }
    } else if (g_ascii_strncasecmp(value, "rtcp:", 5) == 0) {
        m->rtcp_port = (unsigned)strtoul(value + 5, NULL, 10);
    } else if (g_ascii_strcasecmp(value, "rtcp-mux") == 0) {
        m->rtcp_mux = true;
    } else if (g_ascii_strncasecmp(value, "crypto:", 7) == 0) {
        if (strstr(value, "_32"))
            m->info.srtp_tag_len = 4;
    }
}

static void
sdp_setup_streams(packet_info *pinfo, sdp_session_t *s, proto_tree **media_trees,
                  tvbuff_t *tvb)
{
    unsigned i, n = s->nmedia < SDP_MAX_MEDIA ? s->nmedia : SDP_MAX_MEDIA;
    address addr;
    ws_in4_addr v4;
    ws_in6_addr v6;

    for (i = 0; i < n; i++) {
        sdp_media_t *m = &s->media[i];
        const char *conn = m->conn[0] ? m->conn : s->conn;
        unsigned rtcp;
        proto_item *ti;
        if (!m->port || !strstr(m->proto, "RTP/") || !sdp_set_address(conn, &addr, &v4, &v6))
            continue;
        m->info.setup_frame = pinfo->num;
        m->info.srtp = strstr(m->proto, "SAVP") != NULL;
        rtp_add_address(pinfo, &addr, m->port, &m->info);
        rtcp = m->rtcp_mux ? 0 : (m->rtcp_port ? m->rtcp_port : m->port + 1);
        if (rtcp)
            rtcp_add_address(pinfo, &addr, rtcp, pinfo->num);
        if (media_trees[i]) {
            ti = proto_tree_add_format_text(media_trees[i], tvb, 0, 0);
            proto_item_set_text(ti, "%s stream to %s port %u set up, RTCP %s%s",
                                m->info.srtp ? "SRTP" : "RTP", conn, m->port,
                                rtcp ? "port " : "multiplexed",
                                rtcp ? wmem_strdup_printf(pinfo->pool, "%u", rtcp) : "");
            proto_item_set_generated(ti);
        }
    }
}

static void
sdp_add_line(proto_tree *tree, proto_tree **media_tree, tvbuff_t *tvb, packet_info *pinfo,
             int offset, int linelen, char type, const char *value, const sdp_media_t *m)
{
    proto_item *ti;
    proto_tree *sub;
    char **parts;
    unsigned i;
    int vstart = offset + 2, vlen = linelen - 2;
    proto_tree *where = *media_tree ? *media_tree : tree;

    switch (type) {
    case 'v':
        proto_tree_add_string(where, hf_sdp_version, tvb, vstart, vlen, value);
        break;
    case 'o':
        proto_tree_add_string(where, hf_sdp_owner, tvb, vstart, vlen, value);
        break;
    case 's':
        proto_tree_add_string(where, hf_sdp_session_name, tvb, vstart, vlen, value);
        break;
    case 'i':
        proto_tree_add_string(where, *media_tree ? hf_sdp_media_title : hf_sdp_session_info,
                              tvb, vstart, vlen, value);
        break;
    case 'u':
        proto_tree_add_string(where, hf_sdp_uri, tvb, vstart, vlen, value);
        break;
    case 'e':
        proto_tree_add_string(where, hf_sdp_email, tvb, vstart, vlen, value);
        break;
    case 'p':
        proto_tree_add_string(where, hf_sdp_phone, tvb, vstart, vlen, value);
        break;
    case 'b':
        proto_tree_add_string(where, hf_sdp_bandwidth, tvb, vstart, vlen, value);
        break;
    case 't':
        proto_tree_add_string(where, hf_sdp_time, tvb, vstart, vlen, value);
        break;
    case 'r':
        proto_tree_add_string(where, hf_sdp_repeat_time, tvb, vstart, vlen, value);
        break;
    case 'z':
        proto_tree_add_string(where, hf_sdp_timezone, tvb, vstart, vlen, value);
        break;
    case 'k':
        proto_tree_add_string(where, hf_sdp_key, tvb, vstart, vlen, value);
        break;
    case 'c':
        ti = proto_tree_add_string(where, hf_sdp_connection_info, tvb, vstart, vlen, value);
        sub = proto_item_add_subtree(ti, ett_sdp_line);
        parts = g_strsplit_set(value, " ", 4);
        if (parts[0]) {
            proto_tree_add_string(sub, hf_sdp_conn_nettype, tvb, vstart, vlen, parts[0]);
            if (parts[1]) {
                proto_tree_add_string(sub, hf_sdp_conn_addrtype, tvb, vstart, vlen, parts[1]);
                if (parts[2])
                    proto_tree_add_string(sub, hf_sdp_conn_addr, tvb, vstart, vlen, parts[2]);
            }
        }
        g_strfreev(parts);
        break;
    case 'm': {
        ti = proto_tree_add_string(tree, hf_sdp_media, tvb, vstart, vlen, value);
        sub = proto_item_add_subtree(ti, ett_sdp_media);
        *media_tree = sub;
        parts = g_strsplit_set(value, " ", -1);
        if (parts[0]) {
            proto_tree_add_string(sub, hf_sdp_media_media, tvb, vstart, vlen, parts[0]);
            if (parts[1]) {
                proto_tree_add_uint(sub, hf_sdp_media_port, tvb, vstart, vlen,
                                    (unsigned)strtoul(parts[1], NULL, 10));
                if (parts[2])
                    proto_tree_add_string(sub, hf_sdp_media_proto, tvb, vstart, vlen, parts[2]);
            }
        }
        for (i = 3; parts[0] && parts[1] && parts[2] && parts[i]; i++) {
            unsigned pt = (unsigned)strtoul(parts[i], NULL, 10);
            const char *name = try_val_to_str(pt, sdp_static_fmt_vals);
            if (!name && m && pt >= RTP_DYN_PT_MIN && pt <= RTP_DYN_PT_MAX)
                name = m->info.dyn_name[pt - RTP_DYN_PT_MIN];
            if (name)
                proto_tree_add_string_format_value(sub, hf_sdp_media_format, tvb, vstart,
                                                   vlen, parts[i], "%s (%s)", name, parts[i]);
            else
                proto_tree_add_string(sub, hf_sdp_media_format, tvb, vstart, vlen, parts[i]);
        }
        g_strfreev(parts);
        break;
    }
    case 'a': {
        const char *colon = strchr(value, ':');
        ti = proto_tree_add_string(where, *media_tree ? hf_sdp_media_attr : hf_sdp_session_attr,
                                   tvb, vstart, vlen, value);
        sub = proto_item_add_subtree(ti, ett_sdp_line);
        if (colon) {
            char *field = wmem_strndup(pinfo->pool, value, colon - value);
            proto_tree_add_string(sub, hf_sdp_attr_field, tvb, vstart, vlen, field);
            proto_tree_add_string(sub, hf_sdp_attr_value, tvb, vstart, vlen, colon + 1);
        } else {
            proto_tree_add_string(sub, hf_sdp_attr_field, tvb, vstart, vlen, value);
        }
        break;
    }
    default:
        proto_tree_add_string(where, hf_sdp_unknown, tvb, offset, linelen,
                              tvb_format_text(pinfo->pool, tvb, offset, linelen));
        break;
    }
}

static int
dissect_sdp(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void *data)
{
    int offset, next, linelen, pass;
    proto_item *ti;
    proto_tree *sdp_tree, *media_tree;
    proto_tree *media_trees[SDP_MAX_MEDIA];
    const sdp_media_t *cur;
    sdp_session_t *s;
    unsigned mi, learned = 0;

    col_append_str(pinfo->cinfo, COL_PROTOCOL, "/SDP");
    ti = proto_tree_add_item(tree, proto_sdp, tvb, 0, -1, ENC_NA);
    sdp_tree = proto_item_add_subtree(ti, ett_sdp);
    s = wmem_new0(pinfo->pool, sdp_session_t);
    memset(media_trees, 0, sizeof(media_trees));

    /* pass 0 learns the streams (an rtpmap follows its m= line), pass 1 draws */
    for (pass = 0; pass < 2; pass++) {
        offset = 0;
        mi = 0;
        cur = NULL;
        media_tree = NULL;
        while (tvb_offset_exists(tvb, offset)) {
            char type, *value;
            linelen = tvb_find_line_end(tvb, offset, -1, &next, false);
            if (next <= offset)
                break;
            if (linelen < 2 || tvb_get_uint8(tvb, offset + 1) != '=') {
                if (pass == 1 && linelen > 0)
                    proto_tree_add_string(sdp_tree, hf_sdp_unknown, tvb, offset, linelen,
                                          tvb_format_text(pinfo->pool, tvb, offset, linelen));
                offset = next;
                continue;
            }
            type = (char)tvb_get_uint8(tvb, offset);
            value = (char *)tvb_get_string_enc(pinfo->pool, tvb, offset + 2, linelen - 2,
                                               ENC_UTF_8);
            if (pass == 0) {
                sdp_learn(pinfo->pool, s, type, value);
            } else {
                if (type == 'm') {
                    mi++;
                    cur = mi <= learned ? &s->media[mi - 1] : NULL;
                }
                sdp_add_line(sdp_tree, &media_tree, tvb, pinfo, offset, linelen, type,
                             value, cur);
                if (type == 'm' && cur)
                    media_trees[mi - 1] = media_tree;
            }
            offset = next;
        }
        if (pass == 0) {
            learned = s->nmedia < SDP_MAX_MEDIA ? s->nmedia : SDP_MAX_MEDIA;
            s->nmedia = learned;
        }
    }
    sdp_setup_streams(pinfo, s, media_trees, tvb);
    return tvb_captured_length(tvb);
}

void
proto_register_sdp(void)
{
    static hf_register_info hf[] = {
        { &hf_sdp_version,
          { "Session Description Protocol Version (v)", "sdp.version", FT_STRING,
            BASE_NONE, NULL, 0x0, NULL, HFILL }},
        { &hf_sdp_owner,
          { "Owner/Creator, Session Id (o)", "sdp.owner", FT_STRING, BASE_NONE,
            NULL, 0x0, NULL, HFILL }},
        { &hf_sdp_session_name,
          { "Session Name (s)", "sdp.session_name", FT_STRING, BASE_NONE, NULL, 0x0,
            NULL, HFILL }},
        { &hf_sdp_session_info,
          { "Session Information (i)", "sdp.session_info", FT_STRING, BASE_NONE,
            NULL, 0x0, NULL, HFILL }},
        { &hf_sdp_uri,
          { "URI of Description (u)", "sdp.uri", FT_STRING, BASE_NONE, NULL, 0x0,
            NULL, HFILL }},
        { &hf_sdp_email,
          { "E-mail Address (e)", "sdp.email", FT_STRING, BASE_NONE, NULL, 0x0,
            NULL, HFILL }},
        { &hf_sdp_phone,
          { "Phone Number (p)", "sdp.phone", FT_STRING, BASE_NONE, NULL, 0x0,
            NULL, HFILL }},
        { &hf_sdp_connection_info,
          { "Connection Information (c)", "sdp.connection_info", FT_STRING,
            BASE_NONE, NULL, 0x0, NULL, HFILL }},
        { &hf_sdp_conn_nettype,
          { "Connection Network Type", "sdp.connection_info.network_type",
            FT_STRING, BASE_NONE, NULL, 0x0, NULL, HFILL }},
        { &hf_sdp_conn_addrtype,
          { "Connection Address Type", "sdp.connection_info.address_type",
            FT_STRING, BASE_NONE, NULL, 0x0, NULL, HFILL }},
        { &hf_sdp_conn_addr,
          { "Connection Address", "sdp.connection_info.address", FT_STRING,
            BASE_NONE, NULL, 0x0, NULL, HFILL }},
        { &hf_sdp_bandwidth,
          { "Bandwidth Information (b)", "sdp.bandwidth", FT_STRING, BASE_NONE,
            NULL, 0x0, NULL, HFILL }},
        { &hf_sdp_time,
          { "Time Description, active time (t)", "sdp.time", FT_STRING, BASE_NONE,
            NULL, 0x0, NULL, HFILL }},
        { &hf_sdp_repeat_time,
          { "Repeat Time (r)", "sdp.repeat_time", FT_STRING, BASE_NONE, NULL, 0x0,
            NULL, HFILL }},
        { &hf_sdp_timezone,
          { "Time Zone Adjustments (z)", "sdp.timezone", FT_STRING, BASE_NONE,
            NULL, 0x0, NULL, HFILL }},
        { &hf_sdp_key,
          { "Encryption Key (k)", "sdp.key", FT_STRING, BASE_NONE, NULL, 0x0,
            NULL, HFILL }},
        { &hf_sdp_session_attr,
          { "Session Attribute (a)", "sdp.session_attr", FT_STRING, BASE_NONE,
            NULL, 0x0, NULL, HFILL }},
        { &hf_sdp_media,
          { "Media Description, name and address (m)", "sdp.media", FT_STRING,
            BASE_NONE, NULL, 0x0, NULL, HFILL }},
        { &hf_sdp_media_media,
          { "Media Type", "sdp.media.media", FT_STRING, BASE_NONE, NULL, 0x0,
            NULL, HFILL }},
        { &hf_sdp_media_port,
          { "Media Port", "sdp.media.port", FT_UINT16, BASE_DEC, NULL, 0x0,
            NULL, HFILL }},
        { &hf_sdp_media_proto,
          { "Media Protocol", "sdp.media.proto", FT_STRING, BASE_NONE, NULL, 0x0,
            NULL, HFILL }},
        { &hf_sdp_media_format,
          { "Media Format", "sdp.media.format", FT_STRING, BASE_NONE, NULL, 0x0,
            NULL, HFILL }},
        { &hf_sdp_media_title,
          { "Media Title (i)", "sdp.media_title", FT_STRING, BASE_NONE, NULL, 0x0,
            NULL, HFILL }},
        { &hf_sdp_media_attr,
          { "Media Attribute (a)", "sdp.media_attr", FT_STRING, BASE_NONE, NULL,
            0x0, NULL, HFILL }},
        { &hf_sdp_attr_field,
          { "Media Attribute Fieldname", "sdp.media_attribute.field", FT_STRING,
            BASE_NONE, NULL, 0x0, NULL, HFILL }},
        { &hf_sdp_attr_value,
          { "Media Attribute Value", "sdp.media_attribute.value", FT_STRING,
            BASE_NONE, NULL, 0x0, NULL, HFILL }},
        { &hf_sdp_unknown,
          { "Unknown line", "sdp.unknown", FT_STRING, BASE_NONE, NULL, 0x0,
            NULL, HFILL }},
    };
    static int *ett[] = {
        &ett_sdp,
        &ett_sdp_line,
        &ett_sdp_media,
    };

    proto_sdp = proto_register_protocol("Session Description Protocol", "SDP", "sdp");
    proto_register_field_array(proto_sdp, hf, array_length(hf));
    proto_register_subtree_array(ett, array_length(ett));
    register_dissector("sdp", dissect_sdp, proto_sdp);
}

void
proto_reg_handoff_sdp(void)
{
    dissector_add_string("media_type", "application/sdp", find_dissector("sdp"));
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
