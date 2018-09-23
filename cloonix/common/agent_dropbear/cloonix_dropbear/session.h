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

#ifndef _SESSION_H_
#define _SESSION_H_

#include "includes.h"
#include "options.h"
#include "buffer.h"
#include "channel.h"
#include "queue.h"
#include "listener.h"
#include "packet.h"
#include "chansession.h"
#include "dbutil.h"
#include "algo.h"


int get_sessinitdone(void);
void common_session_init(int sock_in, int sock_out);
void session_loop(void(*loophandler)());
void session_cleanup();
void send_session_identification();
void send_msg_ignore();
void ignore_recv_response();


/* Server */
void svr_session(int sock, int childpipe);

/* Client */
void cli_session(int sock_in, int sock_out);
void cleantext(unsigned char* dirtytext);

/* crypto parameters that are stored individually for transmit and receive */
struct key_context_directional {
	const struct dropbear_cipher *algo_crypt;
	const struct dropbear_cipher_mode *crypt_mode;
	const struct dropbear_hash *algo_mac;
	int hash_index; /* lookup for libtomcrypt */
	int valid;
};

struct key_context {

	struct key_context_directional recv;
	struct key_context_directional trans;
};

struct packetlist;
struct packetlist {
	struct packetlist *next;
	buffer * payload;
};

struct AuthState {
        unsigned char authtypes; /* Flags indicating which auth types are still 
                                                                valid */
        unsigned int failcount; /* Number of (failed) authentication attempts.*/
        unsigned authdone : 1; /* 0 if we haven't authed, 1 if we have. Applies for
                                                          client and server (though has differing 
                                                          meanings). */
        unsigned perm_warn : 1; /* Server only, set if bad permissions on 
                                                           ~/.ssh/authorized_keys have already been
                                                           logged. */
};

struct sshsession {

	/* Is it a client or server? */
	unsigned char isserver;

	int sock_in;
	int sock_out;

	/* vmname will be initially NULL as we delay
	 * reading the remote version string. it will be set
	 * by the time any recv_() packet methods are called */
	char *remoteident; 

	int maxfd; /* the maximum file descriptor to check with select() */


	/* Packet buffers/values etc */
	buffer *writepayload; /* Unencrypted payload to write - this is used
							 throughout the code, as handlers fill out this
							 buffer with the packet to send. */
	struct Queue writequeue; /* A queue of encrypted packets to send */
	buffer *readbuf; /* From the wire, decrypted in-place */
	buffer *payload; /* Post-decompression, the actual SSH packet */
	unsigned int transseq, recvseq; /* Sequence IDs */

	/* Packet-handling flags */
	const packettype * packettypes; /* Packet handler mappings for this
										session, see process-packet.c */

	
						
	/* time of the last packet send/receive, for keepalive. Not real-world clock */
	time_t last_packet_time_keepalive_sent;
	time_t last_packet_time_keepalive_recv;
	time_t last_packet_time_any_sent;

	time_t last_packet_time_idle; /* time of the last packet transmission or receive, for
								idle timeout purposes so ignores SSH_MSG_IGNORE
								or responses to keepalives. Not real-world clock */


	struct key_context *keys;
	buffer *session_id; /* this is the hash from the first kex */
	buffer *hash; /* the session hash */

	void(*remoteclosed)(); /* A callback to handle closure of the
									  remote connection */

	struct AuthState authstate; /* Common amongst client and server, since most
								   struct elements are common */

	struct Channel channel; 
	const struct ChanType *chantype; 

	/* TCP forwarding - where manage listeners */
	struct Listener ** listeners;
	unsigned int listensize;


};

struct serversession {

	/* Server specific options */
	int childpipe; /* kept open until we successfully authenticate */
	/* userauth */

	struct ChildPid * childpids; /* array of mappings childpid<->channel */
	unsigned int childpidsize;

	/* The resolved remote address, used for lastlog etc */
	char *vmname;


};


typedef enum {
	STATE_NOTHING,
	USERAUTH_WAIT,
	USERAUTH_REQ_SENT,
	USERAUTH_FAIL_RCVD,
	USERAUTH_SUCCESS_RCVD,
	SESSION_RUNNING
} cli_state;

struct clientsession {

	const struct dropbear_kex *param_kex_algo;
	cli_state state; /* Used to progress auth/channelsession etc */
	int tty_raw_mode; /* Whether we're in raw mode (and have to clean up) */
	struct termios saved_tio;
	int stdincopy;
	int stdinflags;
	int stdoutcopy;
	int stdoutflags;
	int stderrcopy;
	int stderrflags;

	/* for escape char handling */
	int last_char;

	int winchange; /* Set to 1 when a windowchange signal happens */

	int ignore_next_auth_response;
	int auth_interact_failed; /* flag whether interactive auth can still
								 be used */
	int interact_request_received; /* flag whether we've received an 
									  info request from the server for
									  interactive auth.*/


	int retval; /* What the command exit status was - we emulate it */

};

extern struct sshsession ses;
extern struct serversession svr_ses;
extern struct clientsession cli_ses;



#define XAUTH_BIN2 "/usr/bin/X11/xauth"
#define XAUTH_BIN3 "/usr/bin/xauth"
#define XAUTH_BIN4 "/bin/xauth"
#define MAX_BIN_PATH_LEN 100
#define XAUTH_CMD_LEN 500
#define XAUTH_FILE "/tmp/xauth_"



#endif /* _SESSION_H_ */
