/*
 * Dropbear - a SSH2 server
 * 
 * Copyright (c) 2002,2003 Matt Johnston
 * All rights reserved.
*/

#include "includes.h"

/* Implemented by matt as specified in freebsd 4.7 manpage.
 * We don't require great speed, is simply for use with sshpty code */
size_t strlcpy(char *dst, const char *src, size_t size) {

	size_t i;

	/* this is undefined, though size==0 -> return 0 */
	if (size < 1) {
		return 0;
	}

	for (i = 0; i < size-1; i++) {
		if (src[i] == '\0') {
			break;
		} else {
			dst[i] = src[i];
		}
	}

	dst[i] = '\0';
	return strlen(src);

}

/* taken from openbsd-compat for OpenSSH 3.6.1p1 */
/* "$OpenBSD: strlcat.c,v 1.8 2001/05/13 15:40:15 deraadt Exp $"
 *
 * Appends src to string dst of size siz (unlike strncat, siz is the
 * full size of dst, not space left).  At most siz-1 characters
 * will be copied.  Always NUL terminates (unless siz <= strlen(dst)).
 * Returns strlen(src) + MIN(siz, strlen(initial dst)).
 * If retval >= siz, truncation occurred.
 */
	size_t
strlcat(dst, src, siz)
	char *dst;
	const char *src;
	size_t siz;
{
	register char *d = dst;
	register const char *s = src;
	register size_t n = siz;
	size_t dlen;

	/* Find the end of dst and adjust bytes left but don't go past end */
	while (n-- != 0 && *d != '\0')
		d++;
	dlen = d - dst;
	n = siz - dlen;

	if (n == 0)
		return(dlen + strlen(s));
	while (*s != '\0') {
		if (n != 1) {
			*d++ = *s;
			n--;
		}
		s++;
	}
	*d = '\0';

	return(dlen + (s - src));	/* count does not include NUL */
}


