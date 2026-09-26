/** @file
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __WS_MEMPBRK_INT_H__
#define __WS_MEMPBRK_INT_H__

const uint8_t *ws_mempbrk_portable_exec(const uint8_t* haystack, size_t haystacklen, const ws_mempbrk_pattern* pattern, unsigned char *found_needle);

#endif /* __WS_MEMPBRK_INT_H__ */
