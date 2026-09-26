/* tvbuff_zstd.c
 * Copyright 2022, Kevin Albertson <kevin.eric.albertson [AT] gmail.com>
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * Decompress ZSTD: http://facebook.github.io/zstd/
 */

#include "config.h"
#include "proto.h" // DISSECTOR_ASSERT_HINT
#include "tvbuff.h"
#include "tvbuff-int.h" // tvb_add_to_chain

#define MAX_LOOP_ITERATIONS 100

tvbuff_t *tvb_uncompress_zstd(tvbuff_t *tvb, const int offset, int comprlen)
{
    (void)tvb;
    (void)offset;
    (void)comprlen;
    return NULL;
}

tvbuff_t *tvb_child_uncompress_zstd(tvbuff_t *parent, tvbuff_t *tvb, const int offset, int comprlen)
{
    tvbuff_t *uncompressed = tvb_uncompress_zstd(tvb, offset, comprlen);
    if (!uncompressed)
    {
        return uncompressed;
    }
    tvb_add_to_chain(parent, uncompressed);
    return uncompressed;
}
