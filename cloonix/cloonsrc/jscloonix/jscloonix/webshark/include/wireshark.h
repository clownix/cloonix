/* wireshark.h
 * Global public header with minimally available wireshark API
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __WIRESHARK_H__
#define __WIRESHARK_H__


#define WIRESHARK_VERSION_MAJOR 4
#define WIRESHARK_VERSION_MINOR 5
#define WIRESHARK_VERSION_MICRO 0

/*
 * This header can be included in any file, header or source, public or private.
 * It is strongly recommended to be always included to provide macros that are
 * required for the project and a consistent minimum set of interfaces that are
 * always guaranteed to be available. There is no need to include <glib.h>
 * directly, this header should replace it.
 *
 * Other public headers provided here should be minimal, with stable interfaces
 * and have only global declarations.
 *
 * Every time this header changes everything must be rebuilt so consider carefully
 * if the other project headers included here should really have global scope.
 *
 * See README.developer for a more in-depth guide.
 */

/* System headers.*/
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>

#include <glib.h>

/*
 * Project headers and definitions.
 *
 * Only public headers and symbols can be included here. Nothing related
 * with configuration.
 */

#include <ws_diag_control.h>
#include <ws_posix_compat.h>

#include <wsutil/ws_assert.h>
#include <wsutil/glib-compat.h>
#include <wsutil/wmem/wmem.h>

#endif /* __WIRESHARK_H__ */
