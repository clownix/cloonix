/* util.c
 * Utility routines
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include <glib.h>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "epan/address.h"
#include "epan/prefs.h"
#include "epan/strutil.h"

#include "ui/util.h"

char * get_args_as_string(int argc, char **argv, int optindex)
{
    int len;
    int i;
    char *argstring;
    len = 0;
    for (i = optindex; i < argc; i++) {
        len += (int) strlen(argv[i]);
        len++;
    }
    if (len == 0)
        return g_strdup("");
    argstring = (char *)g_malloc(len);
    argstring[0] = '\0';
    i = optindex;
    for (;;) {
        (void) g_strlcat(argstring, argv[i], len);
        i++;
        if (i == argc)
            break;
        (void) g_strlcat(argstring, " ", len);
    }
    return argstring;
}

void compute_timestamp_diff(int *diffsec, int *diffusec,
    uint32_t sec1, uint32_t usec1, uint32_t sec2, uint32_t usec2)
{
  if (sec1 == sec2) {
    *diffsec = sec1 - sec2;
    *diffusec = usec1 - usec2;
  } else if (sec1 <= sec2) {
    *diffsec = sec1 - sec2;
    if (usec2 >= usec1) {
      *diffusec = usec1 - usec2;
    } else {
      *diffusec = (usec1 - 1000000) - usec2;
      (*diffsec)++;
    }
  } else {
    *diffsec = sec1 - sec2;
    if (usec2 <= usec1) {
      *diffusec = usec1 - usec2;
    } else {
      *diffusec = (usec1 + 1000000) - usec2;
      (*diffsec)--;
    }
  }
}

static char *sanitize_filter_ip(char *hostname) {
    char *end;
    char *ret;

    ret = g_strdup(hostname);
    if (!ret)
        return NULL;

    end = strchr(ret, '%');
    if (end)
        *end = '\0';
    return ret;
}

const char *get_conn_cfilter(void) {
    static GString *filter_str = NULL;
    char *env, **tokens;
    char *lastp, *lastc, *p;
    char *pprotocol = NULL;
    char *phostname = NULL;
    size_t hostlen;
    char *remip, *locip;

    if (filter_str == NULL) {
        filter_str = g_string_new("");
    }
    if ((env = getenv("SSH_CONNECTION")) != NULL) {
        tokens = g_strsplit(env, " ", 4);
        if (g_strv_length(tokens) == 4) {
            remip = sanitize_filter_ip(tokens[0]);
            locip = sanitize_filter_ip(tokens[2]);
            g_string_printf(filter_str, "not (tcp port %s and host %s "
                             "and tcp port %s and host %s)", tokens[1], remip,
                tokens[3], locip);
            g_free(remip);
            g_free(locip);
        }
        g_strfreev(tokens);
    } else if ((env = getenv("SSH_CLIENT")) != NULL) {
        tokens = g_strsplit(env, " ", 3);
        if (g_strv_length(tokens) == 3) {
            remip = sanitize_filter_ip(tokens[2]);
            g_string_printf(filter_str, "not (tcp port %s and host %s "
                "and tcp port %s)", tokens[1], tokens[0], remip);
            g_free(remip);
        }
        g_strfreev(tokens);
    } else if ((env = getenv("REMOTEHOST")) != NULL) {
        /* FreeBSD 7.0 sets REMOTEHOST to an empty string */
        if (g_ascii_strcasecmp(env, "localhost") == 0 ||
            strcmp(env, "127.0.0.1") == 0 ||
            strcmp(env, "") == 0) {
            return "";
        }
        remip = sanitize_filter_ip(env);
        g_string_printf(filter_str, "not host %s", remip);
        g_free(remip);
    } else if ((env = getenv("DISPLAY")) != NULL) {
        /*
         * This mirrors what _X11TransConnectDisplay() does.
         * Note that, on some systems, the hostname can
         * begin with "/", which means that it's a pathname
         * of a UNIX domain socket to connect to.
         *
         * The comments mirror those in _X11TransConnectDisplay(),
         * too. :-)
         *
         * Display names may be of the following format:
         *
         *    [protocol./] [hostname] : [:] displaynumber [.screennumber]
         *
         * A string with exactly two colons separating hostname
         * from the display indicates a DECnet style name.  Colons
         * in the hostname may occur if an IPv6 numeric address
         * is used as the hostname.  An IPv6 numeric address may
         * also end in a double colon, so three colons in a row
         * indicates an IPv6 address ending in :: followed by
         * :display.  To make it easier for people to read, an
         * IPv6 numeric address hostname may be surrounded by []
         * in a similar fashion to the IPv6 numeric address URL
         * syntax defined by IETF RFC 2732.
         *
         * If no hostname and no protocol is specified, the string
         * is interpreted as the most efficient local connection
         * to a server on the same machine.  This is usually:
         *
         *    o shared memory
         *    o local stream
         *    o UNIX domain socket
         *    o TCP to local host.
         */

        p = env;

        /*
         * Step 0, find the protocol.  This is delimited by
         * the optional slash ('/').
         */
        for (lastp = p; *p != '\0' && *p != ':' && *p != '/'; p++)
            ;
        if (*p == '\0')
            return "";    /* must have a colon */

        if (p != lastp && *p != ':') {    /* protocol given? */
            /* Yes */
            pprotocol = p;

            /* Is it TCP? */
            if (p - lastp != 3 || g_ascii_strncasecmp(lastp, "tcp", 3) != 0)
                return "";    /* not TCP */
            p++;            /* skip the '/' */
        } else
            p = env;        /* reset the pointer in
                           case no protocol was given */

        /*
         * Step 1, find the hostname.  This is delimited either by
         * one colon, or two colons in the case of DECnet (DECnet
         * Phase V allows a single colon in the hostname).  (See
         * note above regarding IPv6 numeric addresses with
         * triple colons or [] brackets.)
         */
        lastp = p;
        lastc = NULL;
        for (; *p != '\0'; p++)
            if (*p == ':')
                lastc = p;

        if (lastc == NULL)
            return "";        /* must have a colon */

        if ((lastp != lastc) && (*(lastc - 1) == ':')
            && (((lastc - 1) == lastp) || (*(lastc - 2) != ':'))) {
                /* DECnet display specified */
                return "";
        } else
            hostlen = lastc - lastp;

        if (hostlen == 0)
            return "";    /* no hostname supplied */

        phostname = (char *)g_malloc(hostlen + 1);
        memcpy(phostname, lastp, hostlen);
        phostname[hostlen] = '\0';

        if (pprotocol == NULL) {
            /*
             * No protocol was explicitly specified, so it
             * could be a local connection over a transport
             * that we won't see.
             *
             * Does the host name refer to the local host?
             * If so, the connection would probably be a
             * local connection.
             *
             * XXX - compare against our host name?
             * _X11TransConnectDisplay() does.
             */
            if (g_ascii_strcasecmp(phostname, "localhost") == 0 ||
                strcmp(phostname, "127.0.0.1") == 0) {
                    g_free(phostname);
                return "";
            }

            /*
             * A host name of "unix" (case-sensitive) also
             * causes a local connection.
             */
            if (strcmp(phostname, "unix") == 0) {
                    g_free(phostname);
                return "";
            }

            /*
             * Does the host name begin with "/"?  If so,
             * it's presumed to be the pathname of a
             * UNIX domain socket.
             */
            if (phostname[0] == '/') {
                g_free(phostname);
                return "";
            }
        }

        g_string_printf(filter_str, "not host %s", phostname);
        g_free(phostname);
    } else {
        return "";
    }
    return filter_str->str;
}

bool display_is_remote(void)
{
    static bool remote_display_checked;
    static bool is_remote;

    if (!remote_display_checked) {
        is_remote = (strlen(get_conn_cfilter()) > 0);
    }
    return is_remote;
}

// MUST be UTF-8
static char *last_open_dir;

const char *
get_last_open_dir(void)
{
    return last_open_dir;
}

void
set_last_open_dir(const char *dirname)
{
    size_t len;
    char *new_last_open_dir;

    if (dirname && dirname[0]) {
        len = strlen(dirname);
        if (dirname[len-1] == G_DIR_SEPARATOR) {
            new_last_open_dir = g_strconcat(dirname, (char *)NULL);
        }
        else {
            new_last_open_dir = g_strconcat(dirname,
                                            G_DIR_SEPARATOR_S, (char *)NULL);
        }
    } else {
        new_last_open_dir = NULL;
    }

    g_free(last_open_dir);
    last_open_dir = new_last_open_dir;
}

const char *
get_open_dialog_initial_dir(void)
{
    const char *initial_dir = NULL;

    switch (prefs.gui_fileopen_style) {

    case FO_STYLE_LAST_OPENED:
        break;

    case FO_STYLE_SPECIFIED:
        break;

    case FO_STYLE_CWD:
        break;

    default:
        ws_assert_not_reached();
        break;
    }
    return initial_dir;
}
