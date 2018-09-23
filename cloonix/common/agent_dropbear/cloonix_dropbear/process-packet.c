/*
 * Dropbear - a SSH2 server
 * 
 * Copyright (c) 2002-2004 Matt Johnston
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
#include "packet.h"
#include "session.h"
#include "dbutil.h"
#include "ssh.h"
#include "algo.h"
#include "buffer.h"
#include "service.h"
#include "channel.h"
#include "io_clownix.h"

#define MAX_UNAUTH_PACKET_TYPE SSH_MSG_USERAUTH_PK_OK

static void recv_unimplemented();

/* process a decrypted packet, call the appropriate handler */
void process_packet() {

	unsigned char type;
	unsigned int i;
	time_t now;


	type = buf_getbyte(ses.payload);
	now = monotonic_now();
	ses.last_packet_time_keepalive_recv = now;

	/* These packets we can receive at any time */
	switch(type) {

		case SSH_MSG_IGNORE:
			goto out;
		case SSH_MSG_DEBUG:
			goto out;

		case SSH_MSG_UNIMPLEMENTED:
			goto out;
			
		case SSH_MSG_DISCONNECT:
			KOUT("Disconnect received");
	}

	/* Ignore these packet types so that keepalives don't interfere with
	idle detection. This is slightly incorrect since a tcp forwarded
	global request with failure won't trigger the idle timeout,
	but that's probably acceptable */
	if (!(type == SSH_MSG_GLOBAL_REQUEST || type == SSH_MSG_REQUEST_FAILURE)) {
		ses.last_packet_time_idle = now;
	}



	/* Kindly the protocol authors gave all the preauth packets type values
	 * less-than-or-equal-to 60 ( == MAX_UNAUTH_PACKET_TYPE ).
	 * NOTE: if the protocol changes and new types are added, revisit this 
	 * assumption */
	if ( !ses.authstate.authdone && type > MAX_UNAUTH_PACKET_TYPE ) {
		KOUT("Received message %d before userauth", type);
	}

	for (i = 0; ; i++) {
		if (ses.packettypes[i].type == 0) {
			/* end of list */
			break;
		}

		if (ses.packettypes[i].type == type) {
			ses.packettypes[i].handler();
			goto out;
		}
	}

	
	/* TODO do something more here? */
	recv_unimplemented();

out:
	buf_free(ses.payload);
	ses.payload = NULL;

}



/* This must be called directly after receiving the unimplemented packet.
 * Isn't the most clean implementation, it relies on packet processing
 * occurring directly after decryption (direct use of ses.recvseq).
 * This is reasonably valid, since there is only a single decryption buffer */
static void recv_unimplemented() {
	buf_putbyte(ses.writepayload, SSH_MSG_UNIMPLEMENTED);
	buf_putint(ses.writepayload, ses.recvseq - 1);
	encrypt_packet();
}
