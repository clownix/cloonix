/** @file
 * Declaration of CRC-32 tvbuff routines
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __CRC32_TVB_H__
#define __CRC32_TVB_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

uint32_t crc32_ccitt_tvb(tvbuff_t *tvb, unsigned len);
uint32_t crc32_ccitt_tvb_offset(tvbuff_t *tvb, unsigned offset, unsigned len);
uint32_t crc32_ccitt_tvb_seed(tvbuff_t *tvb, unsigned len, uint32_t seed);
uint32_t crc32c_tvb_offset_calculate(tvbuff_t *tvb, unsigned offset,
                                           unsigned len, uint32_t seed);
uint32_t crc32_ccitt_tvb_offset_seed(tvbuff_t *tvb, unsigned offset,
                                           unsigned len, uint32_t seed);
uint32_t crc32_802_tvb(tvbuff_t *tvb, unsigned len);
uint32_t crc32_mpeg2_tvb(tvbuff_t *tvb, unsigned len);
uint32_t crc32_mpeg2_tvb_offset(tvbuff_t *tvb, unsigned offset, unsigned len);
uint32_t crc32_mpeg2_tvb_seed(tvbuff_t *tvb, unsigned len, uint32_t seed);
uint32_t crc32_mpeg2_tvb_offset_seed(tvbuff_t *tvb, unsigned offset,
                                           unsigned len, uint32_t seed);
uint32_t crc32_0x0AA725CF_tvb_offset_seed(tvbuff_t *tvb,
                                            unsigned offset, unsigned len, uint32_t seed);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* crc32-tvb.h */
