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

#ifndef _DBUTIL_H_

#define _DBUTIL_H_

#include "includes.h"
#include "buffer.h"

#ifdef __GNUC__
#define ATTRIB_PRINTF(fmt,args) __attribute__((format(printf, fmt, args))) 
#define ATTRIB_NORETURN __attribute__((noreturn))
#define ATTRIB_SENTINEL __attribute__((sentinel))
#else
#define ATTRIB_PRINTF(fmt,args)
#define ATTRIB_NORETURN
#define ATTRIB_SENTINEL
#endif


char * stripcontrol(const char * text);
int buf_readfile(buffer* buf, const char* filename);
int buf_getline(buffer * line, FILE * authfile);

void m_close(int fd);
void * m_malloc(size_t size);
char * m_strdup(char * str);
void * m_realloc(void* ptr, size_t size);
#define m_free(X) do {free(X); (X) = NULL;} while (0); 
void setnonblocking(int fd);
void unsetnonblocking(int fd); 

int m_str_to_uint(const char* str, unsigned int *val);


/* Returns 0 if a and b have the same contents */
int constant_time_memcmp(const void* a, const void *b, size_t n);

/* Returns a time in seconds that doesn't go backwards - does not correspond to
a real-world clock */
time_t monotonic_now();


#endif /* _DBUTIL_H_ */
