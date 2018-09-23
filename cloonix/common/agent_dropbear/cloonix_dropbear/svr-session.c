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

#include "includes.h"
#include "session.h"
#include "dbutil.h"
#include "packet.h"
#include "algo.h"
#include "buffer.h"
#include "ssh.h"
#include "channel.h"
#include "chansession.h"
#include "atomicio.h"
#include "service.h"
#include "runopts.h"
#include "io_clownix.h"

extern int sessinitdone;

static void svr_remoteclosed();

struct serversession svr_ses; /* GLOBAL */
void cloonix_srv_session_loop(void);


static const packettype svr_packettypes[] = {
	{SSH_MSG_CHANNEL_DATA, recv_msg_channel_data},
	{SSH_MSG_CHANNEL_WINDOW_ADJUST, recv_msg_channel_window_adjust},
	{SSH_MSG_SERVICE_REQUEST, recv_msg_service_request}, /* server */
	{SSH_MSG_CHANNEL_REQUEST, recv_msg_channel_request},
	{SSH_MSG_CHANNEL_OPEN, recv_msg_channel_open},
	{SSH_MSG_CHANNEL_SUCCESS, ignore_recv_response},
	{SSH_MSG_CHANNEL_FAILURE, ignore_recv_response},
	{SSH_MSG_REQUEST_FAILURE, ignore_recv_response}, /* for keepalive */
	{SSH_MSG_REQUEST_SUCCESS, ignore_recv_response}, /* client */
	{0, 0} /* End */
};


void svr_session(int sock, int childpipe)
{
	common_session_init(sock, sock);
	svr_ses.childpipe = childpipe;
        ses.chantype = &svrchansess;
        ses.channel.ctype = &svrchansess;
	svr_chansessinitialise();
	ses.remoteclosed = svr_remoteclosed;
	ses.packettypes = svr_packettypes;
	ses.isserver = 1;
	sessinitdone = 1;
	send_session_identification();
        ses.authstate.authdone = 1;
        m_close(svr_ses.childpipe);
        cloonix_srv_session_loop();
}

static void svr_remoteclosed() 
{
  if (!isempty(&ses.writequeue))
    KERR("Not empty ");
  else
    wrapper_exit(0, (char *)__FILE__, __LINE__);
}

