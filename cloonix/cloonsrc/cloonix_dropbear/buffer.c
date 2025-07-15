/*
 * Dropbear SSH
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

/* Buffer handling routines, designed to avoid overflows/using invalid data */

#include "includes.h"
#include "dbutil.h"
#include "buffer.h"
#include "io_clownix.h"


#define STORE32H(x, y)                                                                     \
     { (y)[0] = (unsigned char)(((x)>>24)&255); (y)[1] = (unsigned char)(((x)>>16)&255);   \
       (y)[2] = (unsigned char)(((x)>>8)&255); (y)[3] = (unsigned char)((x)&255); }

#define LOAD32H(x, y)                            \
     { x = ((unsigned long)((y)[0] & 255)<<24) | \
           ((unsigned long)((y)[1] & 255)<<16) | \
           ((unsigned long)((y)[2] & 255)<<8)  | \
           ((unsigned long)((y)[3] & 255)); }


/* Prevent integer overflows when incrementing buffer position/length.
 * Calling functions should check arguments first, but this provides a
 * backstop */
#define BUF_MAX_INCR 1000000000
#define BUF_MAX_SIZE 1000000000

/* avoid excessively large numbers, > ~8192 bits */
#define BUF_MAX_MPINT (8240 / 8)

/* Create (malloc) a new buffer of size */
buffer* buf_new(unsigned int size) {

	buffer* buf;
	
	if (size > BUF_MAX_SIZE) {
		KOUT("buf->size too big");
	}

	buf = (buffer*)m_malloc(sizeof(buffer));

	if (size > 0) {
		buf->data = (char*)m_malloc(size);
	} else {
		buf->data = NULL;
	}

	buf->size = size;
	buf->pos = 0;
	buf->len = 0;

	return buf;

}

/* free the buffer's data and the buffer itself */
void buf_free(buffer* buf) {

	m_free(buf->data)
	m_free(buf);
}


/* resize a buffer, pos and len will be repositioned if required when
 * downsizing */
void buf_resize(buffer *buf, unsigned int newsize) {

	if (newsize > BUF_MAX_SIZE) {
		KOUT("buf->size too big");
	}

	buf->data = m_realloc(buf->data, newsize);
	buf->size = newsize;
	buf->len = MIN(newsize, buf->len);
	buf->pos = MIN(newsize, buf->pos);

}

/* Create a copy of buf, allocating required memory etc. */
/* The new buffer is sized the same as the length of the source buffer. */
buffer* buf_newcopy(buffer* buf) {
	
	buffer* ret;

	ret = buf_new(buf->len);
	ret->len = buf->len;
	memcpy(ret->data, buf->data, buf->len);
	return ret;
}

/* Set the length of the buffer */
void buf_setlen(buffer* buf, unsigned int len) {
	if (len > buf->size) {
		KOUT("Bad buf_setlen");
	}
	buf->len = len;
}

/* Increment the length of the buffer */
void buf_incrlen(buffer* buf, unsigned int incr) {
	if (incr > BUF_MAX_INCR || buf->len + incr > buf->size) {
		KOUT("Bad buf_incrlen");
	}
	buf->len += incr;
}
/* Set the position of the buffer */
void buf_setpos(buffer* buf, unsigned int pos) {

	if (pos > buf->len) {
		KOUT("Bad buf_setpos");
	}
	buf->pos = pos;
}

/* increment the postion by incr, increasing the buffer length if required */
void buf_incrwritepos(buffer* buf, unsigned int incr) {
	if (incr > BUF_MAX_INCR || buf->pos + incr > buf->size) {
		KOUT("Bad buf_incrwritepos");
	}
	buf->pos += incr;
	if (buf->pos > buf->len) {
		buf->len = buf->pos;
	}
}

/* increment the position by incr, negative values are allowed, to
 * decrement the pos*/
void buf_incrpos(buffer* buf,  int incr) {
	if (incr > BUF_MAX_INCR ||
			(unsigned int)((int)buf->pos + incr) > buf->len 
			|| ((int)buf->pos + incr) < 0) {
		KOUT("Bad buf_incrpos");
	}
	buf->pos += incr;
}

/* Get a byte from the buffer and increment the pos */
unsigned char buf_getbyte(buffer* buf) {

	/* This check is really just ==, but the >= allows us to check for the
	 * bad case of pos > len, which should _never_ happen. */
	if (buf->pos >= buf->len) {
		KOUT("Bad buf_getbyte %d %d", buf->pos, buf->len);
	}
	return buf->data[buf->pos++];
}

/* Get a bool from the buffer and increment the pos */
unsigned char buf_getbool(buffer* buf) {

	unsigned char b;
	b = buf_getbyte(buf);
	if (b != 0)
		b = 1;
	return b;
}

/* put a byte, incrementing the length if required */
void buf_putbyte(buffer* buf, unsigned char val) {

	if (buf->pos >= buf->len) {
		buf_incrlen(buf, 1);
	}
	buf->data[buf->pos] = val;
	buf->pos++;
}

/* returns an in-place pointer to the buffer, checking that
 * the next len bytes from that position can be used */
unsigned char* buf_getptr(buffer* buf, unsigned int len) {

	if (buf->pos + len > buf->len) {
		KOUT("Bad buf_getptr");
	}
	return ((unsigned char*) &buf->data[buf->pos]);
}

/* like buf_getptr, but checks against total size, not used length.
 * This allows writing past the used length, but not past the size */
unsigned char* buf_getwriteptr(buffer* buf, unsigned int len) {

	if (buf->pos + len > buf->size) {
		KOUT("Bad buf_getwriteptr");
	}
	return ((unsigned char*) &buf->data[buf->pos]);
}

/* Return a null-terminated string, it is malloced, so must be free()ed
 * Note that the string isn't checked for null bytes, hence the retlen
 * may be longer than what is returned by strlen */
char* buf_getstring(buffer* buf, unsigned int *retlen) {

	unsigned int len;
	char* ret;
	len = buf_getint(buf);
	if (len > MAX_STRING_LEN) {
		KOUT("String too long");
	}

	if (retlen != NULL) {
		*retlen = len;
	}
	ret = m_malloc(len+1);
	memcpy(ret, buf_getptr(buf, len), len);
	buf_incrpos(buf, len);
	ret[len] = '\0';

	return ret;
}


/* Just increment the buffer position the same as if we'd used buf_getstring,
 * but don't bother copying/malloc()ing for it */
void buf_eatstring(buffer *buf) {

	buf_incrpos( buf, buf_getint(buf) );
}

/* Get an uint32 from the buffer and increment the pos */
unsigned int buf_getint(buffer* buf) {
	unsigned int ret;

	LOAD32H(ret, buf_getptr(buf, 4));
	buf_incrpos(buf, 4);
	return ret;
}

/* put a 32bit uint into the buffer, incr bufferlen & pos if required */
void buf_putint(buffer* buf, int unsigned val) {

	STORE32H(val, buf_getwriteptr(buf, 4));
	buf_incrwritepos(buf, 4);

}

/* put a SSH style string into the buffer, increasing buffer len if required */
void buf_putstring(buffer* buf, char* str, unsigned int len) {
	
	buf_putint(buf, len);
	buf_putbytes(buf, str, len);

}

/* puts an entire buffer as a SSH string. ignore pos of buf_str. */
void buf_putbufstring(buffer *buf, const buffer* buf_str) {
	buf_putstring(buf, buf_str->data, buf_str->len);
}

/* put the set of len bytes into the buffer, incrementing the pos, increasing
 * len if required */
void buf_putbytes(buffer *buf, char *bytes, unsigned int len) {
	memcpy(buf_getwriteptr(buf, len), bytes, len);
	buf_incrwritepos(buf, len);
}
	

