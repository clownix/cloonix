/* packet-sip.c
 * Session Initiation Protocol, RFC 3261: the request or status line, the
 * headers (compact forms included), and the body handed on by its
 * Content-Type, the SDP of a call above all. UDP and TCP, on 5060 and
 * heuristically elsewhere. Reduced from Wireshark's packet-sip.c (Martin
 * Mathieson, Anders Broman and others): same field names, a subset of the
 * features.
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
#include <wsutil/str_util.h>

#include "packet-media-type.h"

void proto_register_sip(void);
void proto_reg_handoff_sip(void);

#define SIP_PORT 5060

static int proto_sip;

static int hf_sip_request_line;
static int hf_sip_method;
static int hf_sip_ruri;
static int hf_sip_status_line;
static int hf_sip_status_code;
static int hf_sip_msg_hdr;
static int hf_sip_cseq_seq;
static int hf_sip_cseq_method;
static int hf_sip_from_user;
static int hf_sip_from_tag;
static int hf_sip_to_user;
static int hf_sip_to_tag;
static int hf_sip_msg_body;

static int ett_sip;
static int ett_sip_line;
static int ett_sip_hdr;
static int ett_sip_hdr_item;
static int ett_sip_body;

static dissector_handle_t sip_handle;
static dissector_handle_t sip_tcp_handle;
static dissector_handle_t text_lines_handle;
static dissector_table_t media_type_table;

typedef struct {
    const char *name;
    const char *compact;
    bool number;
    int hf;
} sip_header_t;

/* RFC 3261 and the extensions a PBX and its phones use */
static sip_header_t sip_headers[] = {
    { "Accept",              NULL, false, -1 },
    { "Accept-Encoding",     NULL, false, -1 },
    { "Accept-Language",     NULL, false, -1 },
    { "Alert-Info",          NULL, false, -1 },
    { "Allow",               NULL, false, -1 },
    { "Allow-Events",        "u",  false, -1 },
    { "Authorization",       NULL, false, -1 },
    { "Call-ID",             "i",  false, -1 },
    { "Call-Info",           NULL, false, -1 },
    { "Contact",             "m",  false, -1 },
    { "Content-Disposition", NULL, false, -1 },
    { "Content-Encoding",    "e",  false, -1 },
    { "Content-Language",    NULL, false, -1 },
    { "Content-Length",      "l",  true,  -1 },
    { "Content-Type",        "c",  false, -1 },
    { "CSeq",                NULL, false, -1 },
    { "Date",                NULL, false, -1 },
    { "Diversion",           NULL, false, -1 },
    { "Error-Info",          NULL, false, -1 },
    { "Event",               "o",  false, -1 },
    { "Expires",             NULL, true,  -1 },
    { "From",                "f",  false, -1 },
    { "Max-Forwards",        NULL, true,  -1 },
    { "MIME-Version",        NULL, false, -1 },
    { "Min-Expires",         NULL, true,  -1 },
    { "Min-SE",              NULL, false, -1 },
    { "Organization",        NULL, false, -1 },
    { "P-Asserted-Identity", NULL, false, -1 },
    { "P-Preferred-Identity", NULL, false, -1 },
    { "Priority",            NULL, false, -1 },
    { "Privacy",             NULL, false, -1 },
    { "Proxy-Authenticate",  NULL, false, -1 },
    { "Proxy-Authorization", NULL, false, -1 },
    { "Proxy-Require",       NULL, false, -1 },
    { "RAck",                NULL, false, -1 },
    { "Reason",              NULL, false, -1 },
    { "Record-Route",        NULL, false, -1 },
    { "Refer-To",            "r",  false, -1 },
    { "Referred-By",         "b",  false, -1 },
    { "Replaces",            NULL, false, -1 },
    { "Reply-To",            NULL, false, -1 },
    { "Require",             NULL, false, -1 },
    { "Retry-After",         NULL, false, -1 },
    { "Route",               NULL, false, -1 },
    { "RSeq",                NULL, false, -1 },
    { "Server",              NULL, false, -1 },
    { "Session-Expires",     "x",  false, -1 },
    { "Subject",             "s",  false, -1 },
    { "Subscription-State",  NULL, false, -1 },
    { "Supported",           "k",  false, -1 },
    { "Timestamp",           NULL, false, -1 },
    { "To",                  "t",  false, -1 },
    { "Unsupported",         NULL, false, -1 },
    { "User-Agent",          NULL, false, -1 },
    { "Via",                 "v",  false, -1 },
    { "Warning",             NULL, false, -1 },
    { "WWW-Authenticate",    NULL, false, -1 },
};

static const char *sip_methods[] = {
    "ACK", "BYE", "CANCEL", "INFO", "INVITE", "MESSAGE", "NOTIFY", "OPTIONS",
    "PRACK", "PUBLISH", "REFER", "REGISTER", "SUBSCRIBE", "UPDATE", NULL
};

typedef enum { SIP_OTHER, SIP_REQUEST, SIP_STATUS } sip_line_t;

/* "METHOD sip:uri SIP/2.0", or "SIP/2.0 200 OK" */
static sip_line_t
sip_first_line(tvbuff_t *tvb, int offset, int linelen, int *token_len)
{
    int i;

    if (linelen >= 12 && tvb_strneql(tvb, offset, "SIP/2.0 ", 8) == 0) {
        for (i = 8; i < 11; i++)
            if (!g_ascii_isdigit(tvb_get_uint8(tvb, offset + i)))
                return SIP_OTHER;
        *token_len = 7;
        return SIP_STATUS;
    }
    if (linelen < 12 || tvb_strneql(tvb, offset + linelen - 8, " SIP/2.0", 8) != 0)
        return SIP_OTHER;
    for (i = 0; i < linelen && tvb_get_uint8(tvb, offset + i) != ' '; i++)
        if (!g_ascii_isupper(tvb_get_uint8(tvb, offset + i)))
            return SIP_OTHER;
    if (i == 0 || i >= linelen - 9)
        return SIP_OTHER;
    *token_len = i;
    return SIP_REQUEST;
}

static int
sip_header_index(const char *name)
{
    unsigned i;

    for (i = 0; i < array_length(sip_headers); i++)
        if (g_ascii_strcasecmp(name, sip_headers[i].name) == 0 ||
            (sip_headers[i].compact && g_ascii_strcasecmp(name, sip_headers[i].compact) == 0))
            return (int)i;
    return -1;
}

/* user part and tag of a From or To value */
static void
sip_add_addr_parts(proto_tree *tree, tvbuff_t *tvb, int offset, int len, packet_info *pinfo,
                   const char *value, int hf_user, int hf_tag)
{
    const char *p = strstr(value, "sip:");
    const char *tag = strstr(value, ";tag=");

    if (!p)
        p = strstr(value, "sips:");
    if (p) {
        const char *user = strchr(p, ':') + 1;
        const char *at = strchr(user, '@');
        const char *stop = strpbrk(user, ">;");
        if (at && (!stop || at < stop))
            proto_tree_add_string(tree, hf_user, tvb, offset, len,
                                  wmem_strndup(pinfo->pool, user, at - user));
    }
    if (tag) {
        const char *t = tag + 5;
        size_t n = strcspn(t, ";> \t");
        proto_tree_add_string(tree, hf_tag, tvb, offset, len,
                              wmem_strndup(pinfo->pool, t, n));
    }
}

/* Bytes of the SIP message at offset; 0 when it is not SIP; -1 when TCP
 * must bring more of it (desegment_offset and desegment_len are set). */
static int
dissect_sip_message(tvbuff_t *tvb, int offset, packet_info *pinfo, proto_tree *tree,
                    bool is_tcp)
{
    int remaining = tvb_reported_length_remaining(tvb, offset);
    int linelen, next, token_len = 0, line, hdr_start, body_start = -1;
    int content_length = -1, msg_len, body_len;
    sip_line_t kind;
    char *first, *ctype = NULL, *cseq_method = NULL;
    proto_item *ti;
    proto_tree *sip_tree, *sub, *hdr_tree;

    linelen = tvb_find_line_end(tvb, offset, remaining, &next, false);
    if (linelen <= 0)
        return 0;
    kind = sip_first_line(tvb, offset, linelen, &token_len);
    if (kind == SIP_OTHER)
        return 0;

    /* the headers end at the first empty line */
    hdr_start = next;
    line = next;
    while (tvb_offset_exists(tvb, line)) {
        int len = tvb_find_line_end(tvb, line, -1, &next, is_tcp && pinfo->can_desegment);
        if (len == -1)
            break;
        if (len == 0) {
            body_start = next;
            break;
        }
        if (g_ascii_isalpha(tvb_get_uint8(tvb, line))) {
            int colon = tvb_find_uint8(tvb, line, len, ':');
            if (colon > line) {
                char *name = (char *)tvb_get_string_enc(pinfo->pool, tvb, line, colon - line,
                                                        ENC_ASCII);
                g_strstrip(name);
                if (sip_header_index(name) >= 0 &&
                    (g_ascii_strcasecmp(name, "Content-Length") == 0 ||
                     g_ascii_strcasecmp(name, "l") == 0))
                    content_length = (int)strtol((char *)tvb_get_string_enc(pinfo->pool,
                                                 tvb, colon + 1, line + len - colon - 1,
                                                 ENC_ASCII), NULL, 10);
            }
        }
        line = next;
    }
    if (body_start < 0) {
        if (is_tcp && pinfo->can_desegment) {
            pinfo->desegment_offset = offset;
            pinfo->desegment_len = DESEGMENT_ONE_MORE_SEGMENT;
            return -1;
        }
        body_start = offset + remaining;
    }
    body_len = offset + remaining - body_start;
    if (content_length >= 0 && (is_tcp || content_length < body_len))
        body_len = content_length;
    if (is_tcp && body_start + body_len > offset + remaining) {
        if (pinfo->can_desegment) {
            pinfo->desegment_offset = offset;
            pinfo->desegment_len = body_start + body_len - offset - remaining;
            return -1;
        }
        body_len = offset + remaining - body_start;
    }
    msg_len = body_start + body_len - offset;

    col_set_str(pinfo->cinfo, COL_PROTOCOL, "SIP");
    ti = proto_tree_add_item(tree, proto_sip, tvb, offset, msg_len, ENC_NA);
    sip_tree = proto_item_add_subtree(ti, ett_sip);

    first = (char *)tvb_get_string_enc(pinfo->pool, tvb, offset, linelen, ENC_UTF_8);
    if (kind == SIP_REQUEST) {
        char *method = wmem_strndup(pinfo->pool, first, token_len);
        char *ruri = wmem_strndup(pinfo->pool, first + token_len + 1, linelen - token_len - 9);
        ti = proto_tree_add_string(sip_tree, hf_sip_request_line, tvb, offset, linelen, first);
        sub = proto_item_add_subtree(ti, ett_sip_line);
        proto_tree_add_string(sub, hf_sip_method, tvb, offset, token_len, method);
        proto_tree_add_string(sub, hf_sip_ruri, tvb, offset + token_len + 1,
                              linelen - token_len - 9, ruri);
        col_append_sep_fstr(pinfo->cinfo, COL_INFO, " | ", "Request: %s %s", method, ruri);
    } else {
        unsigned code = (unsigned)strtoul(first + 8, NULL, 10);
        ti = proto_tree_add_string(sip_tree, hf_sip_status_line, tvb, offset, linelen, first);
        sub = proto_item_add_subtree(ti, ett_sip_line);
        proto_tree_add_uint(sub, hf_sip_status_code, tvb, offset + 8, 3, code);
        col_append_sep_fstr(pinfo->cinfo, COL_INFO, " | ", "Status: %s", first + 8);
    }

    ti = proto_tree_add_item(sip_tree, hf_sip_msg_hdr, tvb, hdr_start,
                             body_start - hdr_start, ENC_NA);
    hdr_tree = proto_item_add_subtree(ti, ett_sip_hdr);
    line = hdr_start;
    while (line < body_start) {
        int len = tvb_find_line_end(tvb, line, body_start - line, &next, false);
        int colon, idx;
        char *name, *value;
        if (len <= 0)
            break;
        colon = tvb_find_uint8(tvb, line, len, ':');
        if (colon <= line) {
            proto_tree_add_format_text(hdr_tree, tvb, line, len);
            line = next;
            continue;
        }
        name = (char *)tvb_get_string_enc(pinfo->pool, tvb, line, colon - line, ENC_ASCII);
        g_strstrip(name);
        value = (char *)tvb_get_string_enc(pinfo->pool, tvb, colon + 1,
                                           line + len - colon - 1, ENC_UTF_8);
        g_strstrip(value);
        idx = sip_header_index(name);
        if (idx < 0) {
            proto_tree_add_format_text(hdr_tree, tvb, line, len);
            line = next;
            continue;
        }
        if (sip_headers[idx].number)
            ti = proto_tree_add_uint(hdr_tree, sip_headers[idx].hf, tvb, line, len,
                                     (unsigned)strtoul(value, NULL, 10));
        else
            ti = proto_tree_add_string(hdr_tree, sip_headers[idx].hf, tvb, line, len, value);
        if (strcmp(sip_headers[idx].name, "CSeq") == 0) {
            char *sp = strchr(value, ' ');
            sub = proto_item_add_subtree(ti, ett_sip_hdr_item);
            proto_tree_add_uint(sub, hf_sip_cseq_seq, tvb, line, len,
                                (unsigned)strtoul(value, NULL, 10));
            if (sp) {
                cseq_method = g_strstrip(sp + 1);
                proto_tree_add_string(sub, hf_sip_cseq_method, tvb, line, len, cseq_method);
            }
        } else if (strcmp(sip_headers[idx].name, "From") == 0) {
            sub = proto_item_add_subtree(ti, ett_sip_hdr_item);
            sip_add_addr_parts(sub, tvb, line, len, pinfo, value, hf_sip_from_user,
                               hf_sip_from_tag);
        } else if (strcmp(sip_headers[idx].name, "To") == 0) {
            sub = proto_item_add_subtree(ti, ett_sip_hdr_item);
            sip_add_addr_parts(sub, tvb, line, len, pinfo, value, hf_sip_to_user,
                               hf_sip_to_tag);
        } else if (strcmp(sip_headers[idx].name, "Content-Type") == 0) {
            char *semi = strchr(value, ';');
            ctype = wmem_ascii_strdown(pinfo->pool, value, semi ? semi - value : -1);
            g_strstrip(ctype);
        }
        line = next;
    }
    if (kind == SIP_STATUS && cseq_method)
        col_append_fstr(pinfo->cinfo, COL_INFO, " (%s)", cseq_method);

    if (body_len > 0) {
        tvbuff_t *body = tvb_new_subset_length(tvb, body_start, body_len);
        media_content_info_t content_info = { MEDIA_CONTAINER_SIP_DATA, NULL, NULL, NULL };
        ti = proto_tree_add_item(sip_tree, hf_sip_msg_body, tvb, body_start, body_len, ENC_NA);
        sub = proto_item_add_subtree(ti, ett_sip_body);
        if (ctype && strcmp(ctype, "application/sdp") == 0)
            col_append_str(pinfo->cinfo, COL_INFO, ", with session description");
        col_set_fence(pinfo->cinfo, COL_INFO);
        if (!ctype || !dissector_try_string_new(media_type_table, ctype, body, pinfo, sub,
                                                true, &content_info))
            call_dissector(text_lines_handle, body, pinfo, sub);
    }
    return msg_len;
}

static int
dissect_sip(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void *data)
{
    int len = dissect_sip_message(tvb, 0, pinfo, tree, false);

    return len > 0 ? (int)tvb_captured_length(tvb) : 0;
}

static int
dissect_sip_tcp(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void *data)
{
    int offset = 0, len = tvb_reported_length(tvb), r;

    while (offset < len) {
        /* CRLF keep-alives between messages */
        while (offset < len && (tvb_get_uint8(tvb, offset) == '\r' ||
                                tvb_get_uint8(tvb, offset) == '\n'))
            offset++;
        if (offset >= len)
            break;
        r = dissect_sip_message(tvb, offset, pinfo, tree, true);
        if (r == 0)
            return offset;
        if (r < 0)
            return len;
        offset += r;
    }
    return len;
}

static bool
dissect_sip_heur(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void *data)
{
    int next, token_len, linelen;

    linelen = tvb_find_line_end(tvb, 0, -1, &next, false);
    if (linelen <= 0 || sip_first_line(tvb, 0, linelen, &token_len) == SIP_OTHER)
        return false;
    return dissect_sip_message(tvb, 0, pinfo, tree, false) > 0;
}

static bool
dissect_sip_tcp_heur(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void *data)
{
    int next, token_len, linelen;

    linelen = tvb_find_line_end(tvb, 0, -1, &next, false);
    if (linelen <= 0 || sip_first_line(tvb, 0, linelen, &token_len) == SIP_OTHER)
        return false;
    dissect_sip_tcp(tvb, pinfo, tree, data);
    return true;
}

void
proto_register_sip(void)
{
    static hf_register_info hf[] = {
        { &hf_sip_request_line,
          { "Request-Line", "sip.Request-Line", FT_STRING, BASE_NONE, NULL, 0x0,
            NULL, HFILL }},
        { &hf_sip_method,
          { "Method", "sip.Method", FT_STRING, BASE_NONE, NULL, 0x0,
            "SIP Method", HFILL }},
        { &hf_sip_ruri,
          { "Request-URI", "sip.r-uri", FT_STRING, BASE_NONE, NULL, 0x0,
            NULL, HFILL }},
        { &hf_sip_status_line,
          { "Status-Line", "sip.Status-Line", FT_STRING, BASE_NONE, NULL, 0x0,
            NULL, HFILL }},
        { &hf_sip_status_code,
          { "Status-Code", "sip.Status-Code", FT_UINT32, BASE_DEC, NULL, 0x0,
            "SIP Status Code", HFILL }},
        { &hf_sip_msg_hdr,
          { "Message Header", "sip.msg_hdr", FT_NONE, BASE_NONE, NULL, 0x0,
            NULL, HFILL }},
        { &hf_sip_cseq_seq,
          { "Sequence Number", "sip.CSeq.seq", FT_UINT32, BASE_DEC, NULL, 0x0,
            NULL, HFILL }},
        { &hf_sip_cseq_method,
          { "Method", "sip.CSeq.method", FT_STRING, BASE_NONE, NULL, 0x0,
            NULL, HFILL }},
        { &hf_sip_from_user,
          { "SIP from address User Part", "sip.from.user", FT_STRING, BASE_NONE,
            NULL, 0x0, NULL, HFILL }},
        { &hf_sip_from_tag,
          { "SIP from tag", "sip.from.tag", FT_STRING, BASE_NONE, NULL, 0x0,
            NULL, HFILL }},
        { &hf_sip_to_user,
          { "SIP to address User Part", "sip.to.user", FT_STRING, BASE_NONE,
            NULL, 0x0, NULL, HFILL }},
        { &hf_sip_to_tag,
          { "SIP to tag", "sip.to.tag", FT_STRING, BASE_NONE, NULL, 0x0,
            NULL, HFILL }},
        { &hf_sip_msg_body,
          { "Message Body", "sip.msg_body", FT_NONE, BASE_NONE, NULL, 0x0,
            NULL, HFILL }},
    };
    static int *ett[] = {
        &ett_sip,
        &ett_sip_line,
        &ett_sip_hdr,
        &ett_sip_hdr_item,
        &ett_sip_body,
    };
    hf_register_info *hf_headers;
    unsigned i;

    proto_sip = proto_register_protocol("Session Initiation Protocol", "SIP", "sip");
    proto_register_field_array(proto_sip, hf, array_length(hf));
    /* one field per header, named and filtered like the header: sip.Call-ID */
    hf_headers = g_new0(hf_register_info, array_length(sip_headers));
    for (i = 0; i < array_length(sip_headers); i++) {
        header_field_info h = {
            sip_headers[i].name,
            g_strdup_printf("sip.%s", sip_headers[i].name),
            sip_headers[i].number ? FT_UINT32 : FT_STRING,
            sip_headers[i].number ? BASE_DEC : BASE_NONE,
            NULL, 0x0, NULL, HFILL
        };
        hf_headers[i].p_id = &sip_headers[i].hf;
        hf_headers[i].hfinfo = h;
    }
    proto_register_field_array(proto_sip, hf_headers, array_length(sip_headers));
    proto_register_subtree_array(ett, array_length(ett));
    sip_handle = register_dissector("sip", dissect_sip, proto_sip);
    sip_tcp_handle = register_dissector("sip.tcp", dissect_sip_tcp, proto_sip);
}

void
proto_reg_handoff_sip(void)
{
    text_lines_handle = find_dissector("data-text-lines");
    media_type_table = find_dissector_table("media_type");
    dissector_add_uint_with_preference("udp.port", SIP_PORT, sip_handle);
    dissector_add_uint_with_preference("tcp.port", SIP_PORT, sip_tcp_handle);
    heur_dissector_add("udp", dissect_sip_heur, "SIP over UDP", "sip_udp", proto_sip,
                       HEURISTIC_ENABLE);
    heur_dissector_add("tcp", dissect_sip_tcp_heur, "SIP over TCP", "sip_tcp", proto_sip,
                       HEURISTIC_ENABLE);
    /* bodies of NOTIFY (message waiting, transfer progress) shown as text */
    dissector_add_string("media_type", "application/simple-message-summary", text_lines_handle);
    dissector_add_string("media_type", "message/sipfrag", text_lines_handle);
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
