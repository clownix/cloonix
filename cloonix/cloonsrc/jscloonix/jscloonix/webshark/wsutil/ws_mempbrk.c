/* ws_mempbrk.c
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include "ws_mempbrk.h"
#include "ws_mempbrk_int.h"

#include <string.h>

void
ws_mempbrk_compile(ws_mempbrk_pattern* pattern, const char *needles)
{
    const char *n = needles;
    memset(pattern->patt, 0, 256);
    while (*n) {
        pattern->patt[(int)*n] = 1;
        n++;
    }
}


const uint8_t *
ws_mempbrk_portable_exec(const uint8_t* haystack, size_t haystacklen, const ws_mempbrk_pattern* pattern, unsigned char *found_needle)
{
    const uint8_t *haystack_end = haystack + haystacklen;

    while (haystack < haystack_end) {
        if (pattern->patt[*haystack]) {
            if (found_needle)
                *found_needle = *haystack;
            return haystack;
        }
        haystack++;
    }

    return NULL;
}


 const uint8_t *
ws_mempbrk_exec(const uint8_t* haystack, size_t haystacklen, const ws_mempbrk_pattern* pattern, unsigned char *found_needle)
{
    return ws_mempbrk_portable_exec(haystack, haystacklen, pattern, found_needle);
}

 const uint8_t *
ws_memrpbrk_exec(const uint8_t* haystack, size_t haystacklen, const ws_mempbrk_pattern* pattern, unsigned char *found_needle)
{
    const uint8_t *haystack_end = haystack + haystacklen;

    while (haystack_end > haystack) {
        if (pattern->patt[*(--haystack_end)]) {
            if (found_needle)
                *found_needle = *haystack_end;
            return haystack_end;
        }
    }

    return NULL;
}

/*
 * Editor modelines  -  https://www.wireshark.org/tools/modelines.html
 *
 * Local variables:
 * c-basic-offset: 8
 * tab-width: 8
 * indent-tabs-mode: t
 * End:
 *
 * vi: set shiftwidth=8 tabstop=8 noexpandtab:
 * :indentSize=8:tabSize=8:noTabs=false:
 */
