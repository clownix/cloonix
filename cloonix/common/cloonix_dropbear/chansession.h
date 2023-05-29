/*
 * Dropbear - a SSH2 server
 * 
 * Copyright (c) 2002,2003 Matt Johnston
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

#ifndef _CHANSESSION_H_
#define _CHANSESSION_H_

#include "channel.h"
#include "listener.h"

struct exxitinfo {

	int exxitpid; /* -1 if not exited */
	int exxitstatus;
	int exxitsignal;
	int exxitcore;
};

struct ChanSess {

	char * cmd; /* command to exec */
	pid_t pid; /* child process pid */

	/* pty details */
	int master; /* the master terminal fd*/
	int slave;
	char * tty;

	struct exxitinfo exxit;

        char *cloonix_name;
        char *cloonix_display;
        char *cloonix_xauth_cookie_key;


int i_run_in_kvm;
char cloonix_tree_dir[MAX_DROPBEAR_PATH_LEN];

};

struct ChildPid {
	pid_t pid;
	struct ChanSess * chansess;
};


void addnewvar(char* param, char* var);

void cli_send_chansess_request();
void cli_tty_cleanup();
void cli_chansess_winchange();

void svr_chansessinitialise();
extern const struct ChanType svrchansess;

struct SigMap {
	int signal;
	char* name;
};

extern const struct SigMap signames[];

#endif /* _CHANSESSION_H_ */
