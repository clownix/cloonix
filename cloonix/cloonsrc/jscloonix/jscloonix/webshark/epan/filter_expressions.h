/* filter_expressions.h
 * Submitted by Edwin Groothuis <wireshark@mavetju.org>
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __FILTER_EXPRESSIONS_H__
#define __FILTER_EXPRESSIONS_H__

#include <epan/prefs.h>
#include <epan/wmem_scopes.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct filter_expression {
	char	*label;
	char	*expression;
	char	*comment;

	bool enabled;	/* Can be set to false by Preferences Dialog */
} filter_expression_t;

void filter_expression_iterate_expressions(wmem_foreach_func func, void* user_data);

filter_expression_t *filter_expression_new(const char *label,
    const char *expr, const char *comment, const bool enabled);

void filter_expression_register_uat(module_t* pref_module);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __FILTER_EXPRESSIONS_H__ */
