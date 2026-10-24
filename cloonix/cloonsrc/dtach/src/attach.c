/* dtach - Copyright (C) 2004-2016 Ned T. Crigler  SPDX-License-Identifier: GPL-2.0-or-later */
#include "dtach.h"

#ifndef VDISABLE
#ifdef _POSIX_VDISABLE
#define VDISABLE _POSIX_VDISABLE
#else
#define VDISABLE 0377
#endif
#endif

/* Current terminal settings, restored after a suspend. */
static struct termios cur_term;
/* 1 if the window size changed */
static int win_changed;

static void
restore_term(void)
{
	tcsetattr(0, TCSADRAIN, &orig_term);

	/* cursor visible, VT100 */
	printf("\033[?25h");
	fflush(stdout);
}

static int
connect_socket(char *name)
{
	int s;
	struct sockaddr_un sockun;

	if (strlen(name) > sizeof(sockun.sun_path) - 1)
	{
		errno = ENAMETOOLONG;
		return -1;
	}

	s = socket(PF_UNIX, SOCK_STREAM, 0);
	if (s < 0)
		return -1;
	sockun.sun_family = AF_UNIX;
	strcpy(sockun.sun_path, name);
	if (connect(s, (struct sockaddr*)&sockun, sizeof(sockun)) < 0)
	{
		close(s);

		/* ECONNREFUSED for a regular file too: check it is a socket */
		if (errno == ECONNREFUSED)
		{
			struct stat st;

			if (stat(name, &st) < 0)
				return -1;
			else if (!S_ISSOCK(st.st_mode) || S_ISREG(st.st_mode))
				errno = ENOTSOCK;
		}
		return -1;
	}
	return s;
}

static void
die(int sig)
{
	if (sig == SIGHUP || sig == SIGINT)
		printf(EOS "\r\n[detached]\r\n");
	else
		printf(EOS "\r\n[got signal %d - dying]\r\n", sig);
	exit(1);
}

static void
win_change()
{
	signal(SIGWINCH, win_change);
	win_changed = 1;
}

static void
process_kbd(int s, struct packet *pkt)
{
	if (!no_suspend && (pkt->u.buf[0] == cur_term.c_cc[VSUSP]))
	{
		pkt->type = MSG_DETACH;
		write(s, pkt, sizeof(struct packet));

		tcsetattr(0, TCSADRAIN, &orig_term);
		printf(EOS "\r\n");
		kill(getpid(), SIGTSTP);
		tcsetattr(0, TCSADRAIN, &cur_term);

		pkt->type = MSG_ATTACH;
		write(s, pkt, sizeof(struct packet));

		pkt->type = MSG_REDRAW;
		pkt->len = REDRAW_CTRL_L;
		ioctl(0, TIOCGWINSZ, &pkt->u.ws);
		write(s, pkt, sizeof(struct packet));
		return;
	}
	else if (pkt->u.buf[0] == detach_char)
	{
		printf(EOS "\r\n[detached]\r\n");
		exit(0);
	}
	else if (pkt->u.buf[0] == '\f')
		win_changed = 1;

	write(s, pkt, sizeof(struct packet));
}

int
attach_main(void)
{
	struct packet pkt;
	unsigned char buf[BUFSIZE];
	fd_set readfds;
	int s;

	/* no error shown when noerror */
	s = connect_socket(sockname);
	if (s < 0 && errno == ENAMETOOLONG)
	{
		char *slash = strrchr(sockname, '/');

		/* a long socket path shortened by chdir */
		if (slash)
		{
			int dirfd = open(".", O_RDONLY);

			if (dirfd >= 0)
			{
				*slash = '\0';
				if (chdir(sockname) >= 0)
				{
					s = connect_socket(slash + 1);
					fchdir(dirfd);
				}
				*slash = '/';
				close(dirfd);
			}
		}
	}
	if (s < 0)
	{
		printf("%s: %s: %s\n", progname, sockname, strerror(errno));
		return 1;
	}

	/* current settings = original here */
	cur_term = orig_term;

	atexit(restore_term);

	signal(SIGPIPE, SIG_IGN);
	signal(SIGXFSZ, SIG_IGN);
	signal(SIGHUP, die);
	signal(SIGTERM, die);
	signal(SIGINT, die);
	signal(SIGQUIT, die);
	signal(SIGWINCH, win_change);

	cur_term.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL);
	cur_term.c_iflag &= ~(IXON|IXOFF);
	cur_term.c_oflag &= ~(OPOST);
	cur_term.c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);
	cur_term.c_cflag &= ~(CSIZE|PARENB);
	cur_term.c_cflag |= CS8;
	cur_term.c_cc[VLNEXT] = VDISABLE;
	cur_term.c_cc[VMIN] = 1;
	cur_term.c_cc[VTIME] = 0;
	tcsetattr(0, TCSADRAIN, &cur_term);

	/* clear screen, VT100 */
	write(1, "\33[H\33[J", 6);

	memset(&pkt, 0, sizeof(struct packet));
	pkt.type = MSG_ATTACH;
	write(s, &pkt, sizeof(struct packet));

	pkt.type = MSG_REDRAW;
	pkt.len = REDRAW_CTRL_L;
	ioctl(0, TIOCGWINSZ, &pkt.u.ws);
	write(s, &pkt, sizeof(struct packet));

	while (1)
	{
		int n;

		FD_ZERO(&readfds);
		FD_SET(0, &readfds);
		FD_SET(s, &readfds);
		n = select(s + 1, &readfds, NULL, NULL, NULL);
		if (n < 0 && errno != EINTR && errno != EAGAIN)
		{
			printf(EOS "\r\n[select failed]\r\n");
			exit(1);
		}

		if (n > 0 && FD_ISSET(s, &readfds))
		{
			ssize_t len = read(s, buf, sizeof(buf));

			if (len == 0)
			{
				printf(EOS "\r\n[EOF - dtach terminating]"
					"\r\n");
				exit(0);
			}
			else if (len < 0)
			{
				printf(EOS "\r\n[read returned an error]\r\n");
				exit(1);
			}
			write(1, buf, len);
			n--;
		}
		if (n > 0 && FD_ISSET(0, &readfds))
		{
			ssize_t len;

			pkt.type = MSG_PUSH;
			memset(pkt.u.buf, 0, sizeof(pkt.u.buf));
			len = read(0, pkt.u.buf, sizeof(pkt.u.buf));

			if (len <= 0)
				exit(1);

			pkt.len = len;
			process_kbd(s, &pkt);
			n--;
		}

		if (win_changed)
		{
			win_changed = 0;

			pkt.type = MSG_WINCH;
			ioctl(0, TIOCGWINSZ, &pkt.u.ws);
			write(s, &pkt, sizeof(pkt));
		}
	}
	return 0;
}
