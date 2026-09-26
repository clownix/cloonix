/* os_version_info.c
 * Routines to report operating system version information
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "config.h"
#include <wsutil/os_version_info.h>
#include <string.h>
#include <errno.h>
#include <sys/utsname.h>
#include <wsutil/unicode-utils.h>

void get_os_version_info(GString *str)
{
  struct utsname name;
  if (uname(&name) < 0) {
    g_string_append_printf(str, "unknown OS version (uname failed - %s)",
    g_strerror(errno));
    return;
  }
  g_string_append_printf(str, "%s %s", name.sysname, name.release);
}

