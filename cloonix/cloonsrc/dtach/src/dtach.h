/* dtach - Copyright (C) 2001, 2004-2016 Ned T. Crigler  SPDX-License-Identifier: GPL-2.0-or-later */
/* Reduced for cloonix: two modes, Linux only. */
#ifndef dtach_h
#define dtach_h

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <pty.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>

extern char *progname, *sockname;
extern int detach_char, no_suspend;
extern struct termios orig_term;
extern int dont_have_tty;

enum
{
	MSG_PUSH	= 0,
	MSG_ATTACH	= 1,
	MSG_DETACH	= 2,
	MSG_WINCH	= 3,
	MSG_REDRAW	= 4,
};

/* Redraw on attach: only the ctrl-L method is left. */
#define REDRAW_CTRL_L 2

/* The client to master protocol. */
struct packet
{
	unsigned char type;
	unsigned char len;
	union
	{
		unsigned char buf[sizeof(struct winsize)];
		struct winsize ws;
	} u;
};

/* Master to client is a plain text stream; BUFSIZE is its buffer. */
#define BUFSIZE 4096

/* This hopefully moves to the bottom of the screen */
#define EOS "\033[999H"

int attach_main(void);
int master_main(char **argv);

#endif
