/* packet-rtp-events.c
 * RTP payload for DTMF digits and telephony events, RFC 4733 (was 2833),
 * reached from RTP through the telephone-event name an SDP gives a dynamic
 * payload type. Reduced from Wireshark's packet-rtp-events.c (Kevin A. Noll).
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include <epan/packet.h>

void proto_register_rtp_events(void);

static int proto_rtp_events;

static int hf_rtp_events_event;
static int hf_rtp_events_end;
static int hf_rtp_events_reserved;
static int hf_rtp_events_volume;
static int hf_rtp_events_duration;

static int ett_rtp_events;

static const value_string rtp_event_type_values[] = {
    {  0, "DTMF Zero 0" },
    {  1, "DTMF One 1" },
    {  2, "DTMF Two 2" },
    {  3, "DTMF Three 3" },
    {  4, "DTMF Four 4" },
    {  5, "DTMF Five 5" },
    {  6, "DTMF Six 6" },
    {  7, "DTMF Seven 7" },
    {  8, "DTMF Eight 8" },
    {  9, "DTMF Nine 9" },
    { 10, "DTMF Star *" },
    { 11, "DTMF Pound #" },
    { 12, "DTMF A" },
    { 13, "DTMF B" },
    { 14, "DTMF C" },
    { 15, "DTMF D" },
    { 16, "Flash" },
    { 0, NULL }
};

static int
dissect_rtp_events(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void *data)
{
    proto_item *ti;
    proto_tree *events_tree;
    int offset = 0;
    uint8_t event;
    bool end;

    col_set_str(pinfo->cinfo, COL_PROTOCOL, "RTP EVENT");
    if (tvb_captured_length(tvb) < 4) {
        col_set_str(pinfo->cinfo, COL_INFO, "Payload type=RTP Event, truncated");
        return tvb_captured_length(tvb);
    }
    while (tvb_captured_length_remaining(tvb, offset) >= 4) {
        event = tvb_get_uint8(tvb, offset);
        end = (tvb_get_uint8(tvb, offset + 1) & 0x80) != 0;
        if (offset == 0)
            col_add_fstr(pinfo->cinfo, COL_INFO, "Payload type=RTP Event, %s%s",
                         val_to_str_const(event, rtp_event_type_values, "Unknown"),
                         end ? " (end)" : "");
        ti = proto_tree_add_item(tree, proto_rtp_events, tvb, offset, 4, ENC_NA);
        events_tree = proto_item_add_subtree(ti, ett_rtp_events);
        proto_item_append_text(ti, ": %s",
                               val_to_str_const(event, rtp_event_type_values, "Unknown"));
        proto_tree_add_item(events_tree, hf_rtp_events_event, tvb, offset, 1, ENC_BIG_ENDIAN);
        proto_tree_add_item(events_tree, hf_rtp_events_end, tvb, offset + 1, 1, ENC_BIG_ENDIAN);
        proto_tree_add_item(events_tree, hf_rtp_events_reserved, tvb, offset + 1, 1, ENC_BIG_ENDIAN);
        proto_tree_add_item(events_tree, hf_rtp_events_volume, tvb, offset + 1, 1, ENC_BIG_ENDIAN);
        proto_tree_add_item(events_tree, hf_rtp_events_duration, tvb, offset + 2, 2, ENC_BIG_ENDIAN);
        offset += 4;
    }
    return offset;
}

void
proto_register_rtp_events(void)
{
    static hf_register_info hf[] = {
        { &hf_rtp_events_event,
          { "Event ID", "rtpevent.event_id", FT_UINT8, BASE_DEC,
            VALS(rtp_event_type_values), 0x0, NULL, HFILL }},
        { &hf_rtp_events_end,
          { "End of Event", "rtpevent.end_of_event", FT_BOOLEAN, 8, NULL, 0x80,
            NULL, HFILL }},
        { &hf_rtp_events_reserved,
          { "Reserved", "rtpevent.reserved", FT_BOOLEAN, 8, NULL, 0x40,
            NULL, HFILL }},
        { &hf_rtp_events_volume,
          { "Volume", "rtpevent.volume", FT_UINT8, BASE_DEC, NULL, 0x3F,
            NULL, HFILL }},
        { &hf_rtp_events_duration,
          { "Event Duration", "rtpevent.duration", FT_UINT16, BASE_DEC, NULL, 0x0,
            NULL, HFILL }},
    };
    static int *ett[] = {
        &ett_rtp_events,
    };

    proto_rtp_events = proto_register_protocol("RFC 4733 RTP Event", "RTP Event", "rtpevent");
    proto_register_field_array(proto_rtp_events, hf, array_length(hf));
    proto_register_subtree_array(ett, array_length(ett));
    register_dissector("rtpevent", dissect_rtp_events, proto_rtp_events);
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
