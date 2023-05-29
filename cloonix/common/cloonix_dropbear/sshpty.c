/*
 * Dropbear - a SSH2 server
 *
 * Copied from OpenSSH-3.5p1 source, modified by Matt Johnston 2003
 * 
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * Allocating a pseudo-terminal, and making it the controlling tty.
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 */

#define _GNU_SOURCE 
#include "includes.h"
#include "dbutil.h"
#include "errno.h"
#include "sshpty.h"

#include "io_clownix.h"


int
myopenpty (int *amaster, int *aslave, char *name, struct termios *termp,
         struct winsize *winp)
{
  char buf[512];
  int master, slave;
  master = posix_openpt (O_RDWR);
  if (master == -1)
    return -1;
  if (grantpt (master))
    goto fail;
  if (unlockpt (master))
    goto fail;
if (ptsname_r (master, buf, sizeof buf))
    goto fail;
  slave = open (buf, O_RDWR | O_NOCTTY);
  if (slave == -1)
      goto fail;
  if(termp)
    tcsetattr (slave, TCSAFLUSH, termp);
  if (winp)
    ioctl (slave, TIOCSWINSZ, winp);
  *amaster = master;
  *aslave = slave;
  if (name != NULL)
    strcpy (name, buf);
return 0;
 fail:
  close (master);
  return -1;
}

int
pty_allocate(int *ptyfd, int *ttyfd, char *namebuf, int namebuflen)
{
	char *name;
	int i;

	i = myopenpty(ptyfd, ttyfd, NULL, NULL, NULL);
	if (i < 0) {
		KERR("pty_allocate: openpty: %.100s", strerror(errno));
		return 0;
	}
	name = ttyname(*ttyfd);
	if (!name) {
		KOUT("ttyname fails for openpty device");
	}

	strlcpy(namebuf, name, namebuflen);	
	return 1;
}


void
pty_release(const char *tty_name)
{
	if (chown(tty_name, (uid_t) 0, (gid_t) 0) < 0
			&& (errno != ENOENT)) {
		KERR("chown %.100s 0 0 failed: %.100s", tty_name, strerror(errno));
	}
	if (chmod(tty_name, (mode_t) 0666) < 0
			&& (errno != ENOENT)) {
		KERR("chmod %.100s 0666 failed: %.100s", tty_name, strerror(errno));
	}
}


void
pty_make_controlling_tty(int *ttyfd, char *tty_name)
{
	int fd;
	signal(SIGTTOU, SIG_IGN);
	fd = open(_PATH_TTY, O_RDWR | O_NOCTTY);
	if (fd >= 0) {
		(void) ioctl(fd, TIOCNOTTY, NULL);
		close(fd);
	}
	if (setsid() < 0) {
		KERR("setsid: %.100s", strerror(errno));
	}

	fd = open(_PATH_TTY, O_RDWR | O_NOCTTY);
	if (fd >= 0) {
		KERR("Failed to disconnect from controlling tty.\n");
		close(fd);
	}
	if (ioctl(*ttyfd, TIOCSCTTY, NULL) < 0) {
		KERR("ioctl(TIOCSCTTY): %.100s", strerror(errno));
	}
	fd = open(tty_name, O_RDWR);
	if (fd < 0) {
		KERR("%.100s: %.100s", tty_name, strerror(errno));
	} else {
		close(fd);
	}
	fd = open(_PATH_TTY, O_WRONLY);
	if (fd < 0) {
		KERR("open /dev/tty failed - could not set controlling tty: %.100s",
		    strerror(errno));
	} else {
		close(fd);
	}
}

void
pty_change_window_size(int ptyfd, int row, int col,
	int xpixel, int ypixel)
{
	struct winsize w;

	w.ws_row = row;
	w.ws_col = col;
	w.ws_xpixel = xpixel;
	w.ws_ypixel = ypixel;
	(void) ioctl(ptyfd, TIOCSWINSZ, &w);
}

