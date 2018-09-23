/****************************************************************************/
/* Copy-pasted-modified for cloonix                License GPL-3.0+         */
/*--------------------------------------------------------------------------*/
/* Original code from:                                                      */
/*                            Dropbear SSH                                  */
/*                            Matt Johnston                                 */
/****************************************************************************/
#include "includes.h"
#include "packet.h"
#include "session.h"
#include "dbutil.h"
#include "ssh.h"
#include "algo.h"
#include "buffer.h"
#include "service.h"
#include "channel.h"
#include "queue.h"
#include "io_clownix.h"


static int read_packet_init();
size_t cloonix_read(int fd, void *buf, size_t count);
size_t cloonix_write(int fd, const void *buf, size_t count);
void cloonix_enqueue(struct Queue* queue, void* item);
void cloonix_dequeue(struct Queue* queue);


/***************************************************************************/
int write_packet(void)
{
  int result = 0;
  int len, written;
  buffer * writebuf = NULL;
  if (isempty(&ses.writequeue))
    result = -1;
  else
    {
    writebuf = (buffer*)examine(&ses.writequeue);
    len = writebuf->len - 1 - writebuf->pos;
    if (len <= 0)
      KOUT(" ");
    written = cloonix_write(ses.sock_out, buf_getptr(writebuf, len), len);
    if (written < 0) 
      {
      if ((ses.sock_out != -1) && (errno != EINTR) && (errno != EAGAIN)) 
        KOUT("Error writing: %s", strerror(errno));
      result = -1;
      } 
    else if (written == 0) 
      {
      KERR("ERR_0_WRITE");
      ses.remoteclosed();
      result = -1;
      }
    else if (written == len) 
      {
      cloonix_dequeue(&ses.writequeue);
      buf_free(writebuf);
      writebuf = NULL;
      } 
    else 
      {
      buf_incrpos(writebuf, written);
      KERR("%d %d", written, len);
      }
    }
  return result;
}
/*-------------------------------------------------------------------------*/

/***************************************************************************/
void read_packet()
{
  int ret, len, maxlen;
  unsigned char blocksize = MIN_PACKET_LEN;
  if (ses.readbuf == NULL || ses.readbuf->len < blocksize) 
    {
    ret = read_packet_init();
    if (ret == DROPBEAR_FAILURE) 
      return;
    }
  maxlen = ses.readbuf->len - ses.readbuf->pos;
  if (maxlen < 0)
    KOUT("%d %d", ses.readbuf->len, ses.readbuf->pos);
  else if (maxlen == 0) 
    {
    len = 0;
    } 
  else 
    {
    len = cloonix_read(ses.sock_in, buf_getptr(ses.readbuf, maxlen), maxlen);
    if (len < 0)
      {
      if ((errno != EINTR) && (errno != EAGAIN)) 
        KOUT("Error reading %s", strerror(errno));
      } 
    else if (len == 0)
      {
      KERR("ERR_0_WRITE");
      ses.remoteclosed();
      }
    else
      {
      buf_incrpos(ses.readbuf, len);
      if (len == maxlen) 
        decrypt_packet();
      }
    }
}
/*-------------------------------------------------------------------------*/

/***************************************************************************/
static int read_packet_init(void )
{
  int slen, result = DROPBEAR_FAILURE;;
  unsigned int maxlen;
  unsigned int len;
  unsigned char blocksize = MIN_PACKET_LEN;
  unsigned int macsize = 0;
  unsigned char *in, *out;
  if (ses.readbuf == NULL)
    ses.readbuf = buf_new(INIT_READBUF);
  maxlen = blocksize - ses.readbuf->pos;
  slen = cloonix_read(ses.sock_in, buf_getwriteptr(ses.readbuf, maxlen), maxlen);
  if (slen == 0)
    {
    ses.remoteclosed();
    KOUT(" ");
    }
  if (slen > 0)
    {
    buf_incrwritepos(ses.readbuf, slen);
    if ((unsigned int)slen == maxlen)
      {
      buf_setpos(ses.readbuf, 0);

      in = buf_getptr(ses.readbuf, blocksize);
      out = buf_getwriteptr(ses.readbuf, blocksize);
      memmove(out, in, blocksize);


      len = buf_getint(ses.readbuf) + 4 + macsize;
      if ((len > RECV_MAX_PACKET_LEN) ||
          (len < MIN_PACKET_LEN + macsize) ||
          ((len - macsize) % blocksize != 0))
        KOUT("Integrity error (bad packet size %u)", len);
      if (len > ses.readbuf->size)
        buf_resize(ses.readbuf, len);		
      buf_setlen(ses.readbuf, len);
      buf_setpos(ses.readbuf, blocksize);
      result = DROPBEAR_SUCCESS;
      }
    }
return result;
}
/*-------------------------------------------------------------------------*/

/***************************************************************************/
void decrypt_packet()
{
  unsigned char blocksize = MIN_PACKET_LEN;
  unsigned char macsize = 0;
  unsigned int padlen;
  unsigned int len;
  unsigned char *in, *out;
  buf_setpos(ses.readbuf, blocksize);
  len = ses.readbuf->len - macsize - ses.readbuf->pos;

  in = buf_getptr(ses.readbuf, len);
  out = buf_getwriteptr(ses.readbuf, len);
  memmove(out, in, len);

  buf_incrpos(ses.readbuf, len);
  buf_setpos(ses.readbuf, PACKET_PADDING_OFF);
  padlen = buf_getbyte(ses.readbuf);
  len = ses.readbuf->len - padlen - 4 - 1 - macsize;
  if ((len > RECV_MAX_PAYLOAD_LEN) || (len < 1))
    KOUT("Bad packet size %u", len);
  buf_setpos(ses.readbuf, PACKET_PAYLOAD_OFF);
  ses.payload = buf_new(len);
  memcpy(ses.payload->data, buf_getptr(ses.readbuf, len), len);
  buf_incrlen(ses.payload, len);
  buf_free(ses.readbuf);
  ses.readbuf = NULL;
  buf_setpos(ses.payload, 0);
  ses.recvseq++;
}
/*-------------------------------------------------------------------------*/

/***************************************************************************/
void encrypt_packet(void)
{
  unsigned char padlen;
  unsigned char blocksize = MIN_PACKET_LEN;
  unsigned char mac_size = 0;
  buffer * writebuf;
  unsigned char packet_type;
  unsigned int len, encrypt_buf_size;
  time_t now;
  unsigned char *in, *out;
  buf_setpos(ses.writepayload, 0);
  packet_type = buf_getbyte(ses.writepayload);
  buf_setpos(ses.writepayload, 0);
  encrypt_buf_size = (ses.writepayload->len + 4 + 1) + 
                      MAX(MIN_PACKET_LEN, blocksize) + 3 +
                      mac_size + 1;
  writebuf = buf_new(encrypt_buf_size);
  buf_setlen(writebuf, PACKET_PAYLOAD_OFF);
  buf_setpos(writebuf, PACKET_PAYLOAD_OFF);
  memcpy(buf_getwriteptr(writebuf, ses.writepayload->len),
                         buf_getptr(ses.writepayload, 
                                    ses.writepayload->len),
                                    ses.writepayload->len);
  buf_incrwritepos(writebuf, ses.writepayload->len);
  buf_setpos(ses.writepayload, 0);
  buf_setlen(ses.writepayload, 0);
  padlen = blocksize - (writebuf->len) % blocksize;
  if (padlen < 4)
    padlen += blocksize;
  if (writebuf->len + padlen < MIN_PACKET_LEN)
    padlen += blocksize;
  buf_setpos(writebuf, 0);
  buf_putint(writebuf, writebuf->len + padlen - 4);
  buf_putbyte(writebuf, padlen);
  buf_setpos(writebuf, writebuf->len);
  buf_incrlen(writebuf, padlen);
  buf_setpos(writebuf, 0);
  len = writebuf->len;

  in = buf_getptr(writebuf, len);
  out = buf_getwriteptr(writebuf, len);
  memmove(out, in, len);

  buf_incrpos(writebuf, len);
  buf_putbyte(writebuf, packet_type);
  buf_setpos(writebuf, 0);
  cloonix_enqueue(&ses.writequeue, (void*)writebuf);
  ses.transseq++;
  now = monotonic_now();
  ses.last_packet_time_any_sent = now;
  if ((packet_type != SSH_MSG_REQUEST_FAILURE) &&
      (packet_type != SSH_MSG_UNIMPLEMENTED) &&
      (packet_type != SSH_MSG_IGNORE))
    ses.last_packet_time_idle = now;
}
/*-------------------------------------------------------------------------*/
