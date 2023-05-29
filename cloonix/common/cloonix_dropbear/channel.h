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

#ifndef _CHANNEL_H_
#define _CHANNEL_H_

#include "includes.h"
#include "buffer.h"
#include "circbuffer.h"

#define SSH_OPEN_ADMINISTRATIVELY_PROHIBITED    1
#define SSH_OPEN_CONNECT_FAILED                 2
#define SSH_OPEN_UNKNOWN_CHANNEL_TYPE           3
#define SSH_OPEN_RESOURCE_SHORTAGE              4

/* Not a real type */
#define SSH_OPEN_IN_PROGRESS					99

#define CHAN_EXTEND_SIZE 3 /* how many extra slots to add when we need more */

struct ChanType;


struct Channel {
int init_done;
	unsigned int remotechan;
	unsigned int recvwindow, transwindow;
	unsigned int recvdonelen;
	unsigned int recvmaxpacket, transmaxpacket;
	void* typedata; /* a pointer to type specific data */
	int writefd; /* read from wire, written to insecure side */
	int readfd; /* read from insecure side, written to wire */
	int errfd; /* used like writefd or readfd, depending if it's client or server.
				  Doesn't exactly belong here, but is cleaner here */
	circbuffer *writebuf; /* data from the wire, for local consumption. Can be
							 initially NULL */
	circbuffer *extrabuf; /* extended-data for the program - used like writebuf
					     but for stderr */

	/* Set after running the ChanType-specific close hander
	 * to ensure we don't run it twice (nor type->checkclose()). */
	int close_handler_done;


	int await_open; /* flag indicating whether we've sent an open request
					   for this channel (and are awaiting a confirmation
					   or failure). */

	int flushing;

	const struct ChanType* ctype;

int i_run_in_kvm;
char cloonix_tree_dir[MAX_DROPBEAR_PATH_LEN];


};

struct ChanType {

	char *name;
	int (*inithandler)(struct Channel*);
	void (*reqhandler)(struct Channel*);
	void (*closehandler)(struct Channel*);
};

void chancleanup();
struct Channel* getchannel();
void recv_msg_channel_open();
void recv_msg_channel_request();
void send_msg_channel_failure(struct Channel *channel);
void send_msg_channel_success(struct Channel *channel);
void recv_msg_channel_data();
void recv_msg_channel_extended_data();
void recv_msg_channel_window_adjust();

void common_recv_msg_channel_data(struct Channel *channel, int fd, 
		circbuffer * buf);

extern const struct ChanType clichansess;
int send_msg_channel_open_init(int fd);
void recv_msg_channel_open_confirmation();
void recv_msg_channel_open_failure();

void send_msg_request_success();
void send_msg_request_failure();
void wrapper_exit(int val, char *file, int line);

#endif /* _CHANNEL_H_ */
