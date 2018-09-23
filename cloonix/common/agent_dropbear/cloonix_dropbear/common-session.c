/****************************************************************************/
/* Copy-pasted-modified for cloonix                License GPL-3.0+         */
/*--------------------------------------------------------------------------*/
/* Original code from:                                                      */
/*                            Dropbear SSH                                  */
/*                            Matt Johnston                                 */
/****************************************************************************/
#include "includes.h"
#include "session.h"
#include "dbutil.h"
#include "packet.h"
#include "algo.h"
#include "buffer.h"
#include "ssh.h"
#include "channel.h"
#include "runopts.h"

#include "io_clownix.h"




struct sshsession ses; /* GLOBAL */

/* need to know if the session struct has been initialised, this way isn't the
 * cleanest, but works OK */
int sessinitdone = 0; /* GLOBAL */

/* this is set when we get SIGINT or SIGTERM, the handler is in main.c */
int exitflag = 0; /* GLOBAL */



struct sshsession *get_srv_session_loop(void)
{
  return (&ses);
}




static int void_cipher(const unsigned char* in, unsigned char* out,
                unsigned long len, void* UNUSED(cipher_state)) {
        if (in != out) {
                memmove(out, in, len);
        }
        return 0;
}

static int void_start(int UNUSED(cipher), const unsigned char* UNUSED(IV),
                        const unsigned char* UNUSED(key),
                        int UNUSED(keylen), int UNUSED(num_rounds), void* UNUSED(cipher_state)) {
        return 0;
}

const struct dropbear_cipher dropbear_nocipher =
        {0, MIN_PACKET_LEN};
const struct dropbear_cipher_mode dropbear_mode_none =
        {void_start, void_cipher, void_cipher};
const struct dropbear_hash dropbear_nohash =
        {0, 0};


int get_sessinitdone(void)
{
  return sessinitdone;
}

/* called only at the start of a session, set up initial state */
void common_session_init(int sock_in, int sock_out) {
	time_t now;

        memset(&ses, 0, sizeof(struct sshsession));
	ses.sock_in = sock_in;
	ses.sock_out = sock_out;
	ses.maxfd = MAX(sock_in, sock_out);


	now = monotonic_now();
	ses.last_packet_time_idle = now;
	ses.last_packet_time_any_sent = 0;
	
	ses.writepayload = buf_new(TRANS_MAX_PAYLOAD_LEN);
	ses.transseq = 0;

	ses.readbuf = NULL;
	ses.payload = NULL;
	ses.recvseq = 0;
	initqueue(&ses.writequeue);
	ses.keys = (struct key_context*)m_malloc(sizeof(struct key_context));
	ses.keys->recv.algo_crypt = &dropbear_nocipher;
	ses.keys->trans.algo_crypt = &dropbear_nocipher;
	ses.keys->recv.crypt_mode = &dropbear_mode_none;
	ses.keys->trans.crypt_mode = &dropbear_mode_none;
	ses.keys->recv.algo_mac = &dropbear_nohash;
	ses.keys->trans.algo_mac = &dropbear_nohash;
	ses.session_id = NULL;
	ses.remoteident = NULL;
	ses.chantype = NULL;
}


/* clean up a session on exit */
void session_cleanup() {
	
	/* we can't cleanup if we don't know the session state */
	if (!sessinitdone) {
		return;
	}

	chancleanup();
	
	/* Cleaning up keys must happen after other cleanup
	functions which might queue packets */
	if (ses.session_id) {
		buf_free(ses.session_id);
		ses.session_id = NULL;
	}
	if (ses.hash) {
		buf_free(ses.hash);
		ses.hash = NULL;
	}
	m_free(ses.keys);

}

void cloonix_enqueue(struct Queue* queue, void* item);



void send_session_identification() {
	buffer *writebuf = buf_new(strlen(LOCAL_IDENT "\r\n") + 1);
	buf_putbytes(writebuf, LOCAL_IDENT "\r\n", strlen(LOCAL_IDENT "\r\n"));
	buf_putbyte(writebuf, 0x0); /* packet type */
	buf_setpos(writebuf, 0);
cloonix_enqueue(&ses.writequeue, writebuf);
}



void ignore_recv_response() {
	// Do nothing
}




/*****************************************************************************/
int file_exists(char *path)
{
  int err, result = 0;
  err = access(path, F_OK);
  if (!err)
    result = 1;
  return result;
}
/*---------------------------------------------------------------------------*/



