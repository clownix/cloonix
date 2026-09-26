/* cpu_info.c
 * Routines to report CPU information
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"
#include <wsutil/cpu_info.h>

#include <string.h>

#include <wsutil/ws_cpuid.h>
#include <wsutil/file_util.h>

static int compare_model_names(const void *a, const void *b, void * user_data )
{
    return strcmp((const char *)a, (const char *)b);
}

struct string_info {
    GString *str;
    const char *sep;
};

static gboolean add_model_name_to_string(void * key, void * value ,
                         void * data)
{
    struct string_info *info = (struct string_info *)data;
    if (info->sep != NULL)
        g_string_append(info->str, info->sep);
    g_string_append(info->str, g_strstrip((char *)key));
    info->sep = ", ";
    return false;
}

void get_cpu_info(GString *str)
{
    GTree *model_names = g_tree_new_full(compare_model_names, NULL, g_free, NULL);
    FILE *proc_cpuinfo;

    proc_cpuinfo = ws_fopen("/proc/cpuinfo", "r");
    if (proc_cpuinfo == NULL) {
        /* Just give up. */
        g_tree_destroy(model_names);
        return;
    }

    char *line = NULL;
    size_t linecap = 0;
    static const char prefix[] = "model name\t: ";
    #define PREFIX_STRLEN (sizeof prefix - 1)
    ssize_t linelen;

    for (;;) {
        linelen = getline(&line, &linecap, proc_cpuinfo);
        if (linelen == -1) {
           break;
        }
        if (linelen != 0)
            line[linelen - 1] = '\0';
        if (strncmp(line, prefix, PREFIX_STRLEN) == 0) {
            char *model_name;
            model_name = g_strdup(line + PREFIX_STRLEN);
            g_tree_insert(model_names, model_name, NULL);
        }
    }

    fclose(proc_cpuinfo);
#define xx_free free  /* hack so checkAPIs doesn't complain */
    xx_free(line);    /* yes, free(), as getline() mallocates it */

    int num_model_names = g_tree_nnodes(model_names);
    if (num_model_names > 0) {
        if (str->len > 0)
            g_string_append(str, ", with ");
        if (num_model_names > 1) {
            g_string_append(str, "{ ");
        }
        struct string_info info;
        info.str = str;
        info.sep = NULL;
        g_tree_foreach(model_names, add_model_name_to_string, &info);
        if (num_model_names > 1) {
            g_string_append(str, " }");
        }
    }
    g_tree_destroy(model_names);
    if (ws_cpuid_sse42())
        g_string_append(str, " (with SSE4.2)");
}
