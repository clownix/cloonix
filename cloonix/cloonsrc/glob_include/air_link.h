/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
/* Air link suid_power (medium) <-> doorways (kvm radios): seqpacket
   <root>/air_link; message: type, kvm name, agent payload. */
#ifndef AIR_LINK_H
#define AIR_LINK_H

#define AIR_LINK_SOCK "air_link"
#define AIR_LINK_HEAD (1 + MAX_NAME_LEN)
/* KVMWIFI_FRAME_HEAD then the frame: the radio index is the sender's to
   suid_power, the receiver's to doorways */
#define AIR_LINK_FRAME 1
/* to doorways: the radio index (1 byte), then 6 bytes per peer address */
#define AIR_LINK_PEERS 2

#endif
