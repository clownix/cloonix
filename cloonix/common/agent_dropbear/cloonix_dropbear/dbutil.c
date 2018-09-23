/*
 * Dropbear - a SSH2 server
 * 
 * Copyright (c) 2002,2003 Matt Johnston
 * All rights reserved.
 * 
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

#include "config.h"

#ifdef __linux__
#define _GNU_SOURCE
/* To call clock_gettime() directly */
#include <sys/syscall.h>
#endif /* __linux */


#include "includes.h"
#include "dbutil.h"
#include "buffer.h"
#include "session.h"
#include "atomicio.h"
#include "io_clownix.h"

#define MAX_FMT 100



char * stripcontrol(const char * text) {

	char * ret;
	int len, pos;
	int i;
	
	len = strlen(text);
	ret = m_malloc(len+1);

	pos = 0;
	for (i = 0; i < len; i++) {
		if ((text[i] <= '~' && text[i] >= ' ') /* normal printable range */
				|| text[i] == '\n' || text[i] == '\r' || text[i] == '\t') {
			ret[pos] = text[i];
			pos++;
		}
	}
	ret[pos] = 0x0;
	return ret;
}
			

/* reads the contents of filename into the buffer buf, from the current
 * position, either to the end of the file, or the buffer being full.
 * Returns DROPBEAR_SUCCESS or DROPBEAR_FAILURE */
int buf_readfile(buffer* buf, const char* filename) {

	int fd = -1;
	int len;
	int maxlen;
	int ret = DROPBEAR_FAILURE;

	fd = open(filename, O_RDONLY);

	if (fd < 0) {
		goto out;
	}
	
	do {
		maxlen = buf->size - buf->pos;
		len = read(fd, buf_getwriteptr(buf, maxlen), maxlen);
		if (len < 0) {
			if ((errno == EINTR) || (errno == EAGAIN)) {
				continue;
			}
			goto out;
		}
		buf_incrwritepos(buf, len);
	} while (len < maxlen && len > 0);

	ret = DROPBEAR_SUCCESS;

out:
	if (fd >= 0) {
		m_close(fd);
	}
	return ret;
}

/* get a line from the file into buffer in the style expected for an
 * authkeys file.
 * Will return DROPBEAR_SUCCESS if data is read, or DROPBEAR_FAILURE on EOF.*/
/* Only used for ~/.ssh/known_hosts and ~/.ssh/authorized_keys */
int buf_getline(buffer * line, FILE * authfile) {

	int c = EOF;

	buf_setpos(line, 0);
	buf_setlen(line, 0);

	while (line->pos < line->size) {

		c = fgetc(authfile); /*getc() is weird with some uClibc systems*/
		if (c == EOF || c == '\n' || c == '\r') {
			goto out;
		}

		buf_putbyte(line, (unsigned char)c);
	}

	/* We return success, but the line length will be zeroed - ie we just
	 * ignore that line */
	buf_setlen(line, 0);

out:


	/* if we didn't read anything before EOF or error, exit */
	if (c == EOF && line->pos == 0) {
		return DROPBEAR_FAILURE;
	} else {
		buf_setpos(line, 0);
		return DROPBEAR_SUCCESS;
	}

}	

/* make sure that the socket closes */
void m_close(int fd) {

	if (fd == -1) {
		return;
	}

	int val;
	do {
		val = close(fd);
	} while (val < 0 && ((errno == EINTR) ||(errno ==  EAGAIN)));

	if (val < 0 && errno != EBADF) {
		/* Linux says EIO can happen */
		KOUT("Error closing fd %d, %s", fd, strerror(errno));
	}
}
	
void * m_malloc(size_t size) {

	void* ret;

	if (size == 0) {
		KOUT("m_malloc failed");
	}
	ret = calloc(1, size);
	if (ret == NULL) {
		KOUT("m_malloc failed");
	}
	return ret;

}

char * m_strdup(char * str) {
	char* ret;

	ret = strdup(str);
	if (ret == NULL) {
		KOUT("m_strdup failed");
	}
	return ret;
}

void * m_realloc(void* ptr, size_t size) {

	void *ret;

	if (size == 0) {
		KOUT("m_realloc failed");
	}
	ret = realloc(ptr, size);
	if (ret == NULL) {
		KOUT("m_realloc failed");
	}
	return ret;
}


void setnonblocking(int fd) {


	if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0) {
		if (errno == ENODEV) {
			/* Some devices (like /dev/null redirected in)
			 * can't be set to non-blocking */
		} else {
			KOUT("Couldn't set nonblocking");
		}
	}
}

void unsetnonblocking(int fd) 
{
  int flags = fcntl(fd, F_GETFL, 0);
  flags &= ~O_NONBLOCK;
  if (fcntl(fd, F_SETFL, flags) < 0) 
    KERR("%d", fd);
}


/* Returns DROPBEAR_SUCCESS or DROPBEAR_FAILURE, with the result in *val */
int m_str_to_uint(const char* str, unsigned int *val) {
	unsigned long l;
	errno = 0;
	l = strtoul(str, NULL, 10);
	/* The c99 spec doesn't actually seem to define EINVAL, but most platforms
	 * I've looked at mention it in their manpage */
	if ((l == 0 && errno == EINVAL)
		|| (l == ULONG_MAX && errno == ERANGE)
		|| (l > UINT_MAX)) {
		return DROPBEAR_FAILURE;
	} else {
		*val = l;
		return DROPBEAR_SUCCESS;
	}
}


int constant_time_memcmp(const void* a, const void *b, size_t n)
{
	const char *xa = a, *xb = b;
	uint8_t c = 0;
	size_t i;
	for (i = 0; i < n; i++)
	{
		c |= (xa[i] ^ xb[i]);
	}
	return c;
}

#if defined(__linux__) && defined(SYS_clock_gettime)
/* CLOCK_MONOTONIC_COARSE was added in Linux 2.6.32 but took a while to
reach userspace include headers */
#ifndef CLOCK_MONOTONIC_COARSE
#define CLOCK_MONOTONIC_COARSE 6
#endif
static clockid_t get_linux_clock_source() {
	struct timespec ts;
	if (syscall(SYS_clock_gettime, CLOCK_MONOTONIC_COARSE, &ts) == 0) {
		return CLOCK_MONOTONIC_COARSE;
	}

	if (syscall(SYS_clock_gettime, CLOCK_MONOTONIC, &ts) == 0) {
		return CLOCK_MONOTONIC;
	}
	return -1;
}
#endif 

time_t monotonic_now() {
#if defined(__linux__) && defined(SYS_clock_gettime)
	static clockid_t clock_source = -2;

	if (clock_source == -2) {
		/* First run, find out which one works. 
		-1 will fall back to time() */
		clock_source = get_linux_clock_source();
	}

	if (clock_source >= 0) {
		struct timespec ts;
		if (syscall(SYS_clock_gettime, clock_source, &ts) != 0) {
			/* Intermittent clock failures should not happen */
			KOUT("Clock broke");
		}
		return ts.tv_sec;
	}
#endif /* linux clock_gettime */

	/* Fallback for everything else - this will sometimes go backwards */
	return time(NULL);
}





