/* packet-rtp.h
 * RTP and RTCP conversations set up by the SDP of a call.
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __PACKET_RTP_H__
#define __PACKET_RTP_H__

#include <epan/packet.h>

#define RTP_DYN_PT_MIN 96
#define RTP_DYN_PT_MAX 127

/* What an SDP says of one RTP stream: names of its dynamic payload types,
 * SRTP or not, and the length of the SRTP authentication tag. */
typedef struct _rtp_setup_info {
    uint32_t setup_frame;
    bool srtp;
    unsigned srtp_tag_len;
    char *dyn_name[RTP_DYN_PT_MAX - RTP_DYN_PT_MIN + 1];
} rtp_setup_info_t;

/* RTP to addr:port from the frame of info on, any source. */
void rtp_add_address(packet_info *pinfo, const address *addr, unsigned port,
                     const rtp_setup_info_t *info);

/* RTCP to addr:port from setup_frame on, any source. */
void rtcp_add_address(packet_info *pinfo, const address *addr, unsigned port,
                      uint32_t setup_frame);

#endif /* __PACKET_RTP_H__ */
