/* tvbuff_zlib.c
 *
 * Copyright (c) 2000 by Gilbert Ramirez <gram@alumni.rice.edu>
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <config.h>
#define WS_LOG_DOMAIN LOG_DOMAIN_EPAN

#include <glib.h>

#include <string.h>
#include <wsutil/glib-compat.h>


#include "tvbuff.h"

tvbuff_t *
tvb_uncompress_zlib(tvbuff_t *tvb , const int offset , int comprlen )
{
	return NULL;
}

tvbuff_t *
tvb_child_uncompress_zlib(tvbuff_t *parent, tvbuff_t *tvb, const int offset, int comprlen)
{
	tvbuff_t *new_tvb = tvb_uncompress_zlib(tvb, offset, comprlen);
	if (new_tvb)
		tvb_set_child_real_data_tvbuff (parent, new_tvb);
	return new_tvb;
}

tvbuff_t *
tvb_uncompress(tvbuff_t *tvb, const int offset, int comprlen)
{
	return tvb_uncompress_zlib(tvb, offset, comprlen);
}

tvbuff_t *
tvb_child_uncompress(tvbuff_t *parent, tvbuff_t *tvb, const int offset, int comprlen)
{
	return tvb_child_uncompress_zlib(parent, tvb, offset, comprlen);
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
