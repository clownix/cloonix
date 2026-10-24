/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
/* The key lookup every json decoder of cloonix shares: one flat object. */
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

static int json_hex4(char *q, unsigned int *v)
{
  int i;
  *v = 0;
  for (i = 0; i < 4; i++)
    {
    char c = q[i];
    *v <<= 4;
    if      (c >= '0' && c <= '9') *v |= (unsigned int) (c - '0');
    else if (c >= 'a' && c <= 'f') *v |= (unsigned int) (c - 'a' + 10);
    else if (c >= 'A' && c <= 'F') *v |= (unsigned int) (c - 'A' + 10);
    else return -1;
    }
  return 0;
}

/* \uXXXX and surrogate pairs to utf-8; returns q past the escape. */
static char *json_utf8(char *q, char *out, int *j, int maxlen)
{
  unsigned int cp, lo;
  unsigned char b[4];
  int i, n;
  if (json_hex4(q + 1, &cp))
    {
    if (*j < maxlen - 1) out[*j] = '?';
    *j += 1;
    return q + 1;
    }
  q += 5;
  if ((cp >= 0xD800) && (cp <= 0xDBFF) && (q[0] == '\\') && (q[1] == 'u') &&
      !json_hex4(q + 2, &lo) && (lo >= 0xDC00) && (lo <= 0xDFFF))
    {
    cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
    q += 6;
    }
  else if ((cp >= 0xD800) && (cp <= 0xDFFF))
    cp = '?';
  if (cp < 0x80)
    { b[0] = (unsigned char) cp; n = 1; }
  else if (cp < 0x800)
    { b[0] = 0xC0 | (cp >> 6); b[1] = 0x80 | (cp & 0x3F); n = 2; }
  else if (cp < 0x10000)
    { b[0] = 0xE0 | (cp >> 12); b[1] = 0x80 | ((cp >> 6) & 0x3F);
      b[2] = 0x80 | (cp & 0x3F); n = 3; }
  else
    { b[0] = 0xF0 | (cp >> 18); b[1] = 0x80 | ((cp >> 12) & 0x3F);
      b[2] = 0x80 | ((cp >> 6) & 0x3F); b[3] = 0x80 | (cp & 0x3F); n = 4; }
  for (i = 0; i < n; i++)
    {
    if (*j < maxlen - 1) out[*j] = (char) b[i];
    *j += 1;
    }
  return q;
}

/* A value that does not fit is dropped whole: a truncated name would name */
/* something else than what was asked. */
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
        else if (*q == 'u')
          {
          q = json_utf8(q, out, &j, maxlen);
          continue;
          }
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

/* Copy the "fctident" of a message into output; none is fatal. */
void json_fctident(char *input, char *output)
{
  char *p, *q; int len;
  p = strstr(input, "\"fctident\"");
  if (!p) KOUT("%s", input);
  p = strchr(p, ':'); if (!p) KOUT("%s", input); p++;
  while (*p == ' ') p++;
  if (*p != '"') KOUT("%s", input);
  p++;
  q = strchr(p, '"'); if (!q) KOUT("%s", input);
  len = (int)(q - p);
  if (len <= 0 || len >= MAX_CLOWNIX_BOUND_LEN) KOUT("%s", input);
  memcpy(output, p, len); output[len] = 0;
}
