/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
/* The key lookup every json decoder of cloonix shares: one flat object.    */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "glob_common.h"
#include "json_min.h"

/* Quoted strings are skipped whole: a key name inside a value never matches.*/
char *json_find_key(char *buf, const char *key)
{
  int  keylen = (int)strlen(key);
  char *p = buf;
  while (*p)
    {
    if (*p == '"')
      {
      if (strncmp(p + 1, key, keylen) == 0 && *(p + 1 + keylen) == '"')
        {
        char *q = p + 1 + keylen + 1;
        while (*q == ' ' || *q == '\t') q++;
        if (*q == ':')
          {
          q++;
          while (*q == ' ' || *q == '\t') q++;
          return q;
          }
        }
      p++;
      while (*p && *p != '"')
        {
        if (*p == '\\' && *(p+1)) p++;
        p++;
        }
      if (*p == '"') p++;
      continue;
      }
    p++;
    }
  return NULL;
}

int json_get_int(char *buf, const char *key, int *out)
{
  char *p = json_find_key(buf, key);
  if (!p) return -1;
  *out = (int)strtol(p, NULL, 10);
  return 0;
}

int json_get_uint(char *buf, const char *key, unsigned int *out)
{
  char *p = json_find_key(buf, key);
  if (!p) return -1;
  *out = (unsigned int)strtoul(p, NULL, 10);
  return 0;
}

/* A value that does not fit is dropped whole: a truncated name would name  */
/* something else than what was asked.                                      */
char *json_get_str(char *buf, const char *key, char *out, int maxlen)
{
  char *p = json_find_key(buf, key);
  char *q; int j;
  if (!p || *p != '"') { out[0] = 0; return NULL; }
  p++;
  j = 0;
  q = p;
  while (*q && *q != '"')
    {
    if (*q == '\\' && *(q+1))
      {
      q++;
      if (j < maxlen - 1)
        {
        if      (*q == '"')  out[j++] = '"';
        else if (*q == '\\') out[j++] = '\\';
        else if (*q == 'n')  out[j++] = '\n';
        else if (*q == 'r')  out[j++] = '\r';
        else if (*q == 't')  out[j++] = '\t';
        else { out[j++] = '\\'; if (j < maxlen-1) out[j++] = *q; }
        }
      else
        j++;
      q++;
      }
    else
      {
      if (j < maxlen - 1)
        out[j] = *q;
      j++;
      q++;
      }
    }
  if (j > maxlen - 1)
    {
    KERR("WARNING json %s: %d chars, %d at most, dropped", key, j, maxlen-1);
    j = 0;
    }
  out[j] = 0;
  if (*q == '"') q++;
  return q;
}

/* Quotes, backslashes and control characters: a raw newline in a reason
   made the whole answer undecodable for the client. */
int json_escape(char *dst, const char *src, int dstlen)
{
  int i = 0, j = 0;
  unsigned char c;
  while (src[i] && j < dstlen - 7)
    {
    c = (unsigned char) src[i++];
    if (c == '"' || c == '\\')
      {
      dst[j++] = '\\';
      dst[j++] = c;
      }
    else if (c == '\n')
      {
      dst[j++] = '\\';
      dst[j++] = 'n';
      }
    else if (c == '\t')
      {
      dst[j++] = '\\';
      dst[j++] = 't';
      }
    else if (c == '\r')
      {
      dst[j++] = '\\';
      dst[j++] = 'r';
      }
    else if (c < 0x20)
      j += sprintf(dst + j, "\\u%04x", c);
    else
      dst[j++] = c;
    }
  dst[j] = 0;
  return j;
}

/* Copy the "fctident" of a message into output; none is fatal.            */
void json_fctident(char *input, char *output)
{
  char *p, *q; int len;
  p = strstr(input, "\"fctident\"");
  if (!p) KOUT("%s\n", input);
  p = strchr(p, ':'); if (!p) KOUT("%s\n", input); p++;
  while (*p == ' ') p++;
  if (*p != '"') KOUT("%s\n", input);
  p++;
  q = strchr(p, '"'); if (!q) KOUT("%s\n", input);
  len = (int)(q - p);
  if (len <= 0 || len >= MAX_CLOWNIX_BOUND_LEN) KOUT("%s\n", input);
  memcpy(output, p, len); output[len] = 0;
}
