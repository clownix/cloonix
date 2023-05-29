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

#ifndef _RUNOPTS_H_
#define _RUNOPTS_H_

#include "includes.h"
#include "buffer.h"
#include "list.h"

typedef struct runopts {

	unsigned int recv_window;

} runopts;

extern runopts opts;

typedef struct svr_runopts 
{
  char unix_dropbear_sock[MAX_DROPBEAR_PATH_LEN];
  char pidfile[MAX_DROPBEAR_PATH_LEN];
  int i_run_in_kvm;
  char cloonix_tree_dir[MAX_DROPBEAR_PATH_LEN];
} svr_runopts;

extern svr_runopts svr_opts;

void svr_getopts(int argc, char ** argv);

typedef struct cli_runopts {
        char *cloonix_doors;
        char *cloonix_password;
	char *progname;
	char *vmname;
	char *cmd;
	int wantpty;
} cli_runopts;

extern cli_runopts cli_opts;
void cli_getopts(int argc, char ** argv);

#endif /* _RUNOPTS_H_ */
