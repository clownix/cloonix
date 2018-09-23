/*
 * Dropbear - a SSH2 server
 * 
 * Copyright (c) 2002-2006 Matt Johnston
 * All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE. */

#include "includes.h"
#include "dbutil.h"
#include "session.h"
#include "buffer.h"
#include "runopts.h"

#include "io_clownix.h"

extern int exitflag;

svr_runopts svr_opts;
int cloonix_socket_listen_unix(char *unix_sock);
static int listensockets(void)
{
  int result = cloonix_socket_listen_unix(svr_opts.unix_dropbear_sock);
  return result;
}
/*CLOONIX*/

static void sigchld_handler(int dummy);
//static void sigsegv_handler(int);
static void sigintterm_handler(int fish);
static void main_noinetd();
static void commonsetup();

int main_i_run_in_kvm(void)
{
  return svr_opts.i_run_in_kvm;
}

char *main_cloonix_tree_dir(void)
{
  return svr_opts.cloonix_tree_dir;
}


int main(int argc, char ** argv)
{
  int lenmax = MAX_DROPBEAR_PATH_LEN-1;
  memset(&svr_opts, 0, sizeof(svr_runopts));
  if (argc == 1)
    {
    svr_opts.i_run_in_kvm = 1;
    strncpy(svr_opts.unix_dropbear_sock, UNIX_DROPBEAR_SOCK, lenmax);
    strncpy(svr_opts.pidfile, PATH_DROPBEAR_PID, lenmax);
    }
  else if (argc == 4)
    {
    svr_opts.i_run_in_kvm = 0;
    strncpy(svr_opts.unix_dropbear_sock, argv[1], lenmax);
    strncpy(svr_opts.pidfile, argv[2], lenmax);
    strncpy(svr_opts.cloonix_tree_dir, argv[3], lenmax);
    }
  else
    KOUT("%d", argc);

  opts.recv_window = DEFAULT_RECV_WINDOW;

  main_noinetd();
  return -1;
}


void main_noinetd() {
	fd_set fds;
	int i;
	int val;
	int maxsock, listensock;
	FILE *pidfile = NULL;
	int childpipes[MAX_UNAUTH_CLIENTS];
	int childsock;
	int childpipe[2];
	pid_t fork_ret = 0;
	size_t conn_idx = 0;
	struct sockaddr_storage remoteaddr;
	socklen_t remoteaddrlen;
	commonsetup();
	for (i = 0; i < MAX_UNAUTH_CLIENTS; i++) {
		childpipes[i] = -1;
	}
	listensock = listensockets();
        maxsock = listensock;
	FD_SET(listensock, &fds);
	if (daemon(0, 0) < 0) {
		KOUT("Failed to daemonize: %s", strerror(errno));
	}

	/* create a PID file so that we can be killed easily */
	pidfile = fopen(svr_opts.pidfile, "w");
	if (pidfile) {
		fprintf(pidfile, "%d\n", getpid());
		fclose(pidfile);
	}

	/* incoming connection select loop */
	for(;;) {

		FD_ZERO(&fds);
		
		/* listening sockets */
		FD_SET(listensock, &fds);

		/* pre-authentication clients */
		for (i = 0; i < MAX_UNAUTH_CLIENTS; i++) {
			if (childpipes[i] >= 0) {
				FD_SET(childpipes[i], &fds);
				maxsock = MAX(maxsock, childpipes[i]);
			}
		}

		val = select(maxsock+1, &fds, NULL, NULL, NULL);

		if (exitflag) {
			unlink(svr_opts.pidfile);
                        wrapper_exit(0, (char *)__FILE__, __LINE__);
		}
		
		if (val == 0) {
			/* timeout reached - shouldn't happen. eh */
			continue;
		}

		if (val < 0) {
			if ((errno == EINTR) || (errno == EAGAIN)) {
				continue;
			}
			KOUT("Listening socket error");
		}

		/* close fds which have been authed or closed - svr-auth.c handles
		 * closing the auth sockets on success */
		for (i = 0; i < MAX_UNAUTH_CLIENTS; i++) {
			if (childpipes[i] >= 0 && FD_ISSET(childpipes[i], &fds)) {
				m_close(childpipes[i]);
				childpipes[i] = -1;
			}
		}


		if (!FD_ISSET(listensock, &fds)) 
			continue;

		remoteaddrlen = sizeof(remoteaddr);
		childsock = accept(listensock, 
				(struct sockaddr*)&remoteaddr, &remoteaddrlen);

		if (childsock < 0) {
			/* accept failed */
			continue;
		}


		if (pipe(childpipe) < 0) {
			goto out;
		}

		prctl(PR_SET_PDEATHSIG, SIGKILL);

		fork_ret = fork();
		if (fork_ret < 0) {
			KERR("Error forking: %s", strerror(errno));
			goto out;
		}

		
		if (fork_ret > 0) {
			m_close(childpipe[1]);

			/* parent */
		for (conn_idx = 0; conn_idx < MAX_UNAUTH_CLIENTS; conn_idx++) {
			if (childpipes[conn_idx] < 0)
{
childpipes[conn_idx] = childpipe[0];
break;
}
                 }

		} else {
			/* child */

			prctl(PR_SET_PDEATHSIG, SIGKILL);
			if (setsid() < 0) {
				KOUT("setsid: %s", strerror(errno));
			}

			/* make sure we close sockets */
			m_close(listensock);
			m_close(childpipe[0]);

			/* start the session */
			svr_session(childsock, childpipe[1]);
			/* don't return */
			KOUT(" ");
		}

out:
		/* This section is important for the parent too */
		m_close(childsock);
		}
	} /* for(;;) loop */



/* catch + reap zombie children */
static void sigchld_handler(int UNUSED(unused)) {
	struct sigaction sa_chld;

	const int saved_errno = errno;

	while(waitpid(-1, NULL, WNOHANG) > 0); 

	sa_chld.sa_handler = sigchld_handler;
	sa_chld.sa_flags = SA_NOCLDSTOP;
	sigemptyset(&sa_chld.sa_mask);
	if (sigaction(SIGCHLD, &sa_chld, NULL) < 0) {
		KOUT("signal() error");
	}
	errno = saved_errno;
}

/* catch ctrl-c or sigterm */
static void sigintterm_handler(int UNUSED(unused)) {

  exitflag = 1;
}

/* Things used by inetd and non-inetd modes */
static void commonsetup() {

	struct sigaction sa_chld;
	/* set up cleanup handler */
	if (signal(SIGINT, sigintterm_handler) == SIG_ERR || 
		signal(SIGTERM, sigintterm_handler) == SIG_ERR ||
		signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
		KOUT("signal() error");
	}

	/* catch and reap zombie children */
	sa_chld.sa_handler = sigchld_handler;
	sa_chld.sa_flags = SA_NOCLDSTOP;
	sigemptyset(&sa_chld.sa_mask);
	if (sigaction(SIGCHLD, &sa_chld, NULL) < 0) {
		KOUT("signal() error");
	}
/*
	if (signal(SIGSEGV, sigsegv_handler) == SIG_ERR) {
		KOUT("signal() error");
	}
*/


}

