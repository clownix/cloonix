/* dtach - Copyright (C) 2004-2016 Ned T. Crigler  SPDX-License-Identifier: GPL-2.0-or-later */
#include "dtach.h"

struct pty
{
	int fd;
	pid_t pid;
	/* old and new, compared */
	struct termios term;
	struct winsize ws;
};

struct client
{
	struct client *next;
	struct client **pprev;
	int fd;
	int attached;
	/* Whether this client already received the buffer being sent. */
	int got_this;
};

static struct client *clients;
static struct pty the_pty;

static void
unlink_socket(void)
{
	unlink(sockname);
}

static void
die(int sig)
{
	if (sig == SIGCHLD)
		return;
	exit(1);
}

static int
setnonblocking(int fd)
{
	int flags;

#if defined(O_NONBLOCK)
	flags = fcntl(fd, F_GETFL);
	if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
		return -1;
	return 0;
#elif defined(FIONBIO)
	flags = 1;
	if (ioctl(fd, FIONBIO, &flags) < 0)
		return -1;
	return 0;
#else
#warning Do not know how to set non-blocking mode.
	return 0;
#endif
}

static int
init_pty(char **argv, int statusfd)
{
	/* the original terminal settings; the attacher sends the window size */
	the_pty.term = orig_term;
	memset(&the_pty.ws, 0, sizeof(struct winsize));

	if (!dont_have_tty)
		the_pty.pid = forkpty(&the_pty.fd, NULL, &the_pty.term, NULL);
	else
		the_pty.pid = forkpty(&the_pty.fd, NULL, NULL, NULL);
	if (the_pty.pid < 0)
		return -1;
	else if (the_pty.pid == 0)
	{
		execvp(*argv, argv);

		/* the error to statusfd, else stdout */
		if (statusfd != -1)
			dup2(statusfd, 1);
		else
			printf(EOS "\r\n");

		printf("%s: could not execute %s: %s\r\n", progname,
		       *argv, strerror(errno));
		fflush(stdout);
		_exit(127);
	}
	return 0;
}

static int
create_socket(char *name)
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
	if (bind(s, (struct sockaddr*)&sockun, sizeof(sockun)) < 0)
	{
		close(s);
		return -1;
	}
	if (listen(s, 128) < 0)
	{
		close(s);
		return -1;
	}
	if (setnonblocking(s) < 0)
	{
		close(s);
		return -1;
	}
	if (chmod(name, 0600) < 0)
	{
		close(s);
		return -1;
	}
	return s;
}

static void
update_socket_modes(int exec)
{
	struct stat st;
	mode_t newmode;

	if (stat(sockname, &st) < 0)
		return;

	if (exec)
		newmode = st.st_mode | S_IXUSR;
	else
		newmode = st.st_mode & ~S_IXUSR;

	if (st.st_mode != newmode)
		chmod(sockname, newmode);
}

/* pty output to the attached clients; the pty gone, exit. */
static void
pty_activity(int s)
{
	unsigned char buf[BUFSIZE];
	ssize_t len;
	struct client *p;
	fd_set readfds, writefds;
	int highest_fd, nclients;

	len = read(the_pty.fd, buf, sizeof(buf));

	if (len <= 0)
		exit(1);

	if (tcgetattr(the_pty.fd, &the_pty.term) < 0)
		exit(1);

top:
	/* Wait for a writable client, or a new one on the control socket. */
	FD_ZERO(&readfds);
	FD_ZERO(&writefds);
	FD_SET(s, &readfds);
	highest_fd = s;
	for (p = clients, nclients = 0; p; p = p->next)
	{
		if (!p->attached)
			continue;
		FD_SET(p->fd, &writefds);
		if (p->fd > highest_fd)
			highest_fd = p->fd;
		nclients++;
	}
	if (nclients == 0)
		return;
	if (select(highest_fd + 1, &readfds, &writefds, NULL, NULL) < 0)
		return;

	for (p = clients, nclients = 0; p; p = p->next)
	{
		ssize_t written;

		if (!FD_ISSET(p->fd, &writefds))
			continue;
		/* Already served this buffer in an earlier round: skip, or output doubles. */
		if (p->got_this)
			continue;

		written = 0;
		while (written < len)
		{
			ssize_t n = write(p->fd, buf + written, len - written);

			if (n > 0)
			{
				written += n;
				continue;
			}
			else if (n < 0 && errno == EINTR)
				continue;
			else if (n < 0 && errno != EAGAIN)
				nclients = -1;
			break;
		}
		if (nclients != -1 && written == len)
		{
			p->got_this = 1;
			nclients++;
		}
	}

	if (!FD_ISSET(s, &readfds) && nclients == 0)
		goto top;

	for (p = clients; p; p = p->next)
		p->got_this = 0;
}

static void
control_activity(int s)
{
	int fd;
	struct client *p;
 
	fd = accept(s, NULL, NULL);
	if (fd < 0)
		return;
	else if (setnonblocking(fd) < 0)
	{
		close(fd);
		return;
	}

	p = malloc(sizeof(struct client));
	p->fd = fd;
	p->attached = 0;
	p->got_this = 0;
	p->pprev = &clients;
	p->next = *(p->pprev);
	if (p->next)
		p->next->pprev = &p->next;
	*(p->pprev) = p;
}

static void
client_activity(struct client *p)
{
	ssize_t len;
	struct packet pkt;

	memset(&pkt, 0, sizeof(struct packet));
	len = read(p->fd, &pkt, sizeof(struct packet));
	if (len < 0 && (errno == EAGAIN || errno == EINTR))
		return;

	if (len <= 0)
	{
		close(p->fd);
		if (p->next)
			p->next->pprev = p->pprev;
		*(p->pprev) = p->next;
		free(p);
		return;
	}

	/* A packet is fixed size: act only on a whole one, a partial read left len and the union uninitialised. */
	if (len != (ssize_t) sizeof(struct packet))
		return;

	if (pkt.type == MSG_PUSH)
	{
		if (pkt.len <= sizeof(pkt.u.buf))
			write(the_pty.fd, pkt.u.buf, pkt.len);
	}

	else if (pkt.type == MSG_ATTACH)
		p->attached = 1;
	else if (pkt.type == MSG_DETACH)
		p->attached = 0;

	/* Window size change request, without a forced redraw. */
	else if (pkt.type == MSG_WINCH)
	{
		the_pty.ws = pkt.u.ws;
		ioctl(the_pty.fd, TIOCSWINSZ, &the_pty.ws);
	}

	/* Redraw on attach. Only the ctrl-L method is left, so pkt.len, which
	** used to choose between three, is not read. */
	else if (pkt.type == MSG_REDRAW)
	{
		char c = '\f';

		the_pty.ws = pkt.u.ws;
		ioctl(the_pty.fd, TIOCSWINSZ, &the_pty.ws);

		/* Send a ^L only if the program is in no-echo and
		** character-at-a-time mode. */
		if (((the_pty.term.c_lflag & (ECHO|ICANON)) == 0) &&
		    (the_pty.term.c_cc[VMIN] == 1))
			write(the_pty.fd, &c, 1);
	}
}

/* The master: watches the pty process and the attached clients. */
static void
master_process(int s, char **argv, int statusfd)
{
	struct client *p, *next;
	fd_set readfds;
	int highest_fd;
	int nullfd;

	int has_attached_client = 0;

	setsid();

	atexit(unlink_socket);

	signal(SIGCHLD, die);
	if (init_pty(argv, statusfd) < 0)
	{
		if (statusfd != -1)
			dup2(statusfd, 1);
		if (errno == ENOENT)
			printf("%s: Could not find a pty.\n", progname);
		else
			printf("%s: init_pty: %s\n", progname, strerror(errno));
		exit(1);
	}

	signal(SIGPIPE, SIG_IGN);
	signal(SIGXFSZ, SIG_IGN);
	signal(SIGHUP, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);
	signal(SIGTTOU, SIG_IGN);
	signal(SIGINT, die);
	signal(SIGTERM, die);

	if (statusfd != -1)
		close(statusfd);

	/* daemon: stdin/stdout/stderr on /dev/null */
	nullfd = open("/dev/null", O_RDWR);
	dup2(nullfd, 0);
	dup2(nullfd, 1);
	dup2(nullfd, 2);
	if (nullfd > 2)
		close(nullfd);

	while (1)
	{
		int new_has_attached_client = 0;

		FD_ZERO(&readfds);
		FD_SET(s, &readfds);
		highest_fd = s;

		FD_SET(the_pty.fd, &readfds);
		if (the_pty.fd > highest_fd)
			highest_fd = the_pty.fd;

		for (p = clients; p; p = p->next)
		{
			FD_SET(p->fd, &readfds);
			if (p->fd > highest_fd)
				highest_fd = p->fd;

			if (p->attached)
				new_has_attached_client = 1;
		}

		if (has_attached_client != new_has_attached_client)
		{
			update_socket_modes(new_has_attached_client);
			has_attached_client = new_has_attached_client;
		}

		if (select(highest_fd + 1, &readfds, NULL, NULL, NULL) < 0)
		{
			if (errno == EINTR || errno == EAGAIN)
				continue;
			exit(1);
		}

		if (FD_ISSET(s, &readfds))
			control_activity(s);
		for (p = clients; p; p = next)
		{
			next = p->next;
			if (FD_ISSET(p->fd, &readfds))
				client_activity(p);
		}
		if (FD_ISSET(the_pty.fd, &readfds))
			pty_activity(s);
	}
}

int
master_main(char **argv)
{
	int fd[2] = {-1, -1};
	int s;
	pid_t pid;

	s = create_socket(sockname);
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
					s = create_socket(slash + 1);
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

	fcntl(s, F_SETFD, FD_CLOEXEC);

	/* Closed by exec: the fork below reports an exec failure, cloonix reads the exit status. */
	if (pipe(fd) >= 0)
	{
		if (fcntl(fd[0], F_SETFD, FD_CLOEXEC) < 0 ||
		    fcntl(fd[1], F_SETFD, FD_CLOEXEC) < 0)
		{
			close(fd[0]);
			close(fd[1]);
			fd[0] = fd[1] = -1;
		}
	}

	pid = fork();
	if (pid < 0)
	{
		printf("%s: fork: %s\n", progname, strerror(errno));
		unlink_socket();
		return 1;
	}
	else if (pid == 0)
	{
		if (fd[0] != -1)
			close(fd[0]);
		master_process(s, argv, fd[1]);
		return 0;
	}

	if (fd[0] != -1)
	{
		char buf[1024];
		ssize_t len;

		close(fd[1]);
		len = read(fd[0], buf, sizeof(buf));
		if (len > 0)
		{
			write(2, buf, len);
			kill(pid, SIGTERM);
			return 1;
		}
		close(fd[0]);
	}
	close(s);
	return 0;
}
