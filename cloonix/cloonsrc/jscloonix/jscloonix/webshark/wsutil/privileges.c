/* privileges.c
 * Routines for handling privileges, e.g. set-UID and set-GID on UNIX.
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 2006 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"
#define WS_LOG_DOMAIN LOG_DOMAIN_WSUTIL

#include "privileges.h"

#include <wsutil/ws_assert.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef HAVE_PWD_H
#include <pwd.h>
#endif

#ifdef HAVE_GRP_H
#include <grp.h>
#endif

#include <string.h>
#include <errno.h>

static uid_t ruid, euid;
static gid_t rgid, egid;
static bool init_process_policies_called;

void init_process_policies(void)
{
	ruid = getuid();
	euid = geteuid();
	rgid = getgid();
	egid = getegid();

	init_process_policies_called = true;
}

bool started_with_special_privs(void)
{
	ws_assert(init_process_policies_called);
#ifdef HAVE_ISSETUGID
	return issetugid();
#else
	return (ruid != euid || rgid != egid || ruid == 0 || rgid == 0);
#endif
}

/*
 * Return true if the real, effective, or saved (if we can check it) user
 * ID or group are 0.
 */
bool
running_with_special_privs(void)
{
	if (getuid() == 0 || geteuid() == 0)
		return true;
	if (getgid() == 0 || getegid() == 0)
		return true;
	return false;
}

/*
 * Permanently relinquish  set-UID and set-GID privileges.
 * If error, abort since we probably shouldn't continue
 * with elevated privileges.
 * Note that if this error occurs when dumpcap is called from
 * wireshark or tshark, the message seen will be
 * "Child dumpcap process died:". This is obscure but we'll
 *   consider it acceptable since it should be highly unlikely
 *   that this error will occur.
 */

static void
setxid_fail(const char *str)
{
	ws_error("Attempt to relinquish privileges failed [%s()] - aborting: %s\n",
		str, g_strerror(errno));
}

void
relinquish_special_privs_perm(void)
{
	/*
	 * If we were started with special privileges, set the
	 * real and effective group and user IDs to the original
	 * values of the real and effective group and user IDs.
	 * If we're not, don't bother - doing so seems to mung
	 * our group set, at least in Mac OS X 10.5.
	 *
	 * (Set the effective UID last - that takes away our
	 * rights to set anything else.)
	 */
	if (started_with_special_privs()) {
		if (setgid(rgid)                == -1) {setxid_fail("setgid"); }
		if (setegid(rgid)               == -1) {setxid_fail("setegid");}

		if (setuid(ruid)                == -1) {setxid_fail("setuid"); }
		if (seteuid(ruid)               == -1) {setxid_fail("seteuid");}
	}
}

/*
 * Get the current username.  String must be g_free()d after use.
 */
char *
get_cur_username(void) {
	char *username;
	struct passwd *pw = getpwuid(getuid());

	if (pw) {
		username = g_strdup(pw->pw_name);
	} else {
		username = g_strdup("UNKNOWN");
	}
	endpwent();
	return username;
}

/*
 * Get the current group.  String must be g_free()d after use.
 */
char *
get_cur_groupname(void) {
	char *groupname;
	struct group *gr = getgrgid(getgid());

	if (gr) {
		groupname = g_strdup(gr->gr_name);
	} else {
		groupname = g_strdup("UNKNOWN");
	}
	endgrent();
	return groupname;
}

