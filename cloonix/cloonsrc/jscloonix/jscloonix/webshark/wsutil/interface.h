/** @file
 * Utility functions to get infos from interfaces
 *
 * Copyright 2016, Dario Lombardo
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef _INTERFACE_H
#define _INTERFACE_H

#include <glib.h>

GSList* local_interfaces_to_list(void);

#endif
