/****************************************************************************/
/* Copy-pasted-modified for cloonix                License GPL-3.0+         */
/*--------------------------------------------------------------------------*/
/* Original code from:                                                      */
/*                            Dropbear SSH                                  */
/*                            Matt Johnston                                 */
/****************************************************************************/
#include "includes.h"
#include "dbutil.h"
#include "circbuffer.h"
#include "io_clownix.h"

#define MAX_CBUF_SIZE 100000000

/****************************************************************************/
circbuffer * cbuf_new(unsigned int size)
{
  circbuffer *cbuf = NULL;
  if (size > MAX_CBUF_SIZE)
    KOUT("Bad cbuf size");
  cbuf = (circbuffer*)m_malloc(sizeof(circbuffer));
  if (size > 0)
    cbuf->data = (unsigned char*)m_malloc(size);
  cbuf->used = 0;
  cbuf->readpos = 0;
  cbuf->writepos = 0;
  cbuf->size = size;
  return cbuf;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void cbuf_free(circbuffer * cbuf)
{
  if (cbuf)
    {
    m_free(cbuf->data);
    m_free(cbuf);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
unsigned int cbuf_getused(circbuffer * cbuf) 
{
  int result = 0;
  if (cbuf)
    result = cbuf->used;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
unsigned int cbuf_getavail(circbuffer * cbuf)
{
  int result = 0;
  if (cbuf)
    result = cbuf->size - cbuf->used;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
unsigned int cbuf_readlen(circbuffer *cbuf)
{
  int result = 0;
  if (cbuf)
    {
    if (((2*cbuf->size)+cbuf->writepos-cbuf->readpos)%cbuf->size !=
           cbuf->used%cbuf->size)
      KOUT(" ");
    if (((2*cbuf->size)+cbuf->readpos-cbuf->writepos)%cbuf->size != 
           (cbuf->size-cbuf->used)%cbuf->size)
      KOUT(" ");
    if (cbuf->used)
      {
      if (cbuf->readpos < cbuf->writepos)
        result = cbuf->writepos - cbuf->readpos;
      else
        result = cbuf->size - cbuf->readpos;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
unsigned int cbuf_writelen(circbuffer *cbuf)
{
  int result = 0;
  if (cbuf)
    {
    if (cbuf->used > cbuf->size)
      KOUT(" ");
    if (((2*cbuf->size)+cbuf->writepos-cbuf->readpos)%cbuf->size != 
          cbuf->used%cbuf->size)
      KOUT(" ");
    if (((2*cbuf->size)+cbuf->readpos-cbuf->writepos)%cbuf->size != 
         (cbuf->size-cbuf->used)%cbuf->size)
      KOUT(" ");
    if (cbuf->used != cbuf->size)
      {
      if (cbuf->writepos < cbuf->readpos)
        result = cbuf->readpos - cbuf->writepos;
      else
	result = cbuf->size - cbuf->writepos;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
unsigned char* cbuf_readptr(circbuffer *cbuf, unsigned int len)
{
  if (!cbuf)
    KOUT(" ");
  if (len > cbuf_readlen(cbuf))
    KOUT(" ");
  return &cbuf->data[cbuf->readpos];
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
unsigned char* cbuf_writeptr(circbuffer *cbuf, unsigned int len)
{
  if (!cbuf)
    KOUT(" ");
  if (len > cbuf_writelen(cbuf))
    KOUT(" ");
  return &cbuf->data[cbuf->writepos];
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void cbuf_incrwrite(circbuffer *cbuf, unsigned int len)
{
  if (!cbuf)
    KOUT(" ");
  if (len > cbuf_writelen(cbuf))
    KOUT(" ");
  cbuf->used += len;
  if (cbuf->used > cbuf->size)
    KOUT(" ");
  cbuf->writepos = (cbuf->writepos + len) % cbuf->size;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void cbuf_incrread(circbuffer *cbuf, unsigned int len)
{
  if (!cbuf)
    KOUT(" ");
  if (len > cbuf_readlen(cbuf))
    KOUT(" ");
  if (cbuf->used < len)
    KOUT(" ");
  cbuf->used -= len;
  cbuf->readpos = (cbuf->readpos + len) % cbuf->size;
}
/*--------------------------------------------------------------------------*/
