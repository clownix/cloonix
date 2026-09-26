/* inet_addr.c
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"
#include "inet_addr.h"
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>

#define _NTOP_SRC_CAST_
#include "str_util.h"

static inline bool
inet_pton_internal(int af, const char *src, void *dst, size_t dst_size,
                    const char *af_str)
{
    int ret = inet_pton(af, src, dst);
    if (ret < 0) {
        int err = errno;
        memset(dst, 0, dst_size);
        errno = err;
        return false;
    }
    /* ret == 0 invalid src representation, ret == 1 success. */
    return ret == 1;
}

static inline const char *
inet_ntop_internal(int af, const void *src, char *dst, size_t dst_size,
                    const char *af_str)
{
    /* Add a cast to ignore 64-to-32 bit narrowing warnings with some
     * compilers (POSIX uses socklen_t instead of size_t). */
    const char *ret = inet_ntop(af, _NTOP_SRC_CAST_ src, dst, (unsigned int)dst_size);
    if (ret == NULL) {
        int err = errno;
        char errbuf[16];
        /* set result to something that can't be confused with a valid conversion */
        (void)g_strlcpy(dst, ws_strerrorname_r(err, errbuf, sizeof(errbuf)), dst_size);
        errno = err;
        return dst;
    }
    return dst;
}

const char *
ws_inet_ntop4(const void *src, char *dst, size_t dst_size)
{
    return inet_ntop_internal(AF_INET, src, dst, dst_size, "AF_INET");
}

bool
ws_inet_pton4(const char *src, ws_in4_addr *dst)
{
    return inet_pton_internal(AF_INET, src, dst, sizeof(*dst), "AF_INET");
}

const char *
ws_inet_ntop6(const void *src, char *dst, size_t dst_size)
{
    return inet_ntop_internal(AF_INET6, src, dst, dst_size, "AF_INET6");
}

bool
ws_inet_pton6(const char *src, ws_in6_addr *dst)
{
    return inet_pton_internal(AF_INET6, src, dst, sizeof(*dst), "AF_INET6");
}
