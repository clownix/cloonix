/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
/* cxp_wire.c - see cxp_wire.h. Compiled by gcc and by the mingw cross       */
/* compiler both, so nothing here may be platform specific.                  */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cxp_wire.h"

void cxp_json_str(const char *body, const char *key, char *out, int outsz)
{
  char pattern[64];
  const char *p;
  int i = 0;
  out[0] = 0;
  if (!body)
    return;
  snprintf(pattern, sizeof(pattern), "\"%s\"", key);
  p = strstr(body, pattern);
  if (!p)
    return;
  p = strchr(p + strlen(pattern), ':');
  if (!p)
    return;
  p++;
  while ((*p == ' ') || (*p == '\t'))
    p++;
  if (*p != '"')
    return;
  p++;
  while (*p && (*p != '"') && (i < outsz - 1))
    {
    if (*p != '\\')
      {
      out[i++] = *p++;
      continue;
      }
    p++;
    if (!*p)
      break;
    switch (*p)
      {
      case 'n':  out[i++] = '\n'; p++; break;
      case 't':  out[i++] = '\t'; p++; break;
      case 'r':  out[i++] = '\r'; p++; break;
      case 'b':  out[i++] = '\b'; p++; break;
      case 'f':  out[i++] = '\f'; p++; break;
      case '/':  out[i++] = '/';  p++; break;
      case '"':  out[i++] = '"';  p++; break;
      case '\\': out[i++] = '\\'; p++; break;
      case 'u':
        {
        unsigned int cp = 0;
        int k, ok = 1;
        for (k = 1; k <= 4; k++)
          {
          char c = p[k];
          if ((c >= '0') && (c <= '9')) cp = (cp << 4) + (c - '0');
          else if ((c >= 'a') && (c <= 'f')) cp = (cp << 4) + (c - 'a' + 10);
          else if ((c >= 'A') && (c <= 'F')) cp = (cp << 4) + (c - 'A' + 10);
          else { ok = 0; break; }
          }
        if (!ok)
          { out[i++] = *p++; break; }
        p += 5;
        if (cp < 0x80)
          out[i++] = (char) cp;
        else if (cp < 0x800)
          {
          if (i + 2 >= outsz) { i = outsz - 1; break; }
          out[i++] = (char)(0xC0 | (cp >> 6));
          out[i++] = (char)(0x80 | (cp & 0x3F));
          }
        else
          {
          if (i + 3 >= outsz) { i = outsz - 1; break; }
          out[i++] = (char)(0xE0 | (cp >> 12));
          out[i++] = (char)(0x80 | ((cp >> 6) & 0x3F));
          out[i++] = (char)(0x80 | (cp & 0x3F));
          }
        break;
        }
      default: out[i++] = *p++; break;
      }
    }
  out[i] = 0;
}

int cxp_json_int(const char *body, const char *key, int dflt)
{
  char pattern[64];
  const char *p;
  int val;
  if (!body)
    return dflt;
  snprintf(pattern, sizeof(pattern), "\"%s\"", key);
  p = strstr(body, pattern);
  if (!p)
    return dflt;
  p = strchr(p + strlen(pattern), ':');
  if (!p)
    return dflt;
  if (sscanf(p + 1, "%d", &val) != 1)
    return dflt;
  return val;
}

void cxp_json_escape(const char *in, char *out, int outsz)
{
  int i = 0;
  out[0] = 0;
  if (!in)
    return;
  while ((*in) && (i < outsz - 7))
    {
    unsigned char c = (unsigned char) *in;
    if ((c == '"') || (c == '\\'))
      { out[i++] = '\\'; out[i++] = *in++; }
    else if (c == '\n')
      { out[i++] = '\\'; out[i++] = 'n'; in++; }
    else if (c == '\r')
      { out[i++] = '\\'; out[i++] = 'r'; in++; }
    else if (c == '\t')
      { out[i++] = '\\'; out[i++] = 't'; in++; }
    else if (c < 0x20)
      { i += snprintf(out + i, outsz - i, "\\u%04x", c); in++; }
    else
      out[i++] = *in++;
    }
  out[i] = 0;
}

int cxp_safe_rel_path(const char *name)
{
  const char *p = name;
  const char *slash;
  int len;
  if ((!name) || (!name[0]) || (name[0] == '/') || (name[0] == '\\'))
    return -1;
  if (strchr(name, '\\'))
    return -1;
  if (strchr(name, ':'))
    return -1;
  while (*p)
    {
    slash = strchr(p, '/');
    len = slash ? (int) (slash - p) : (int) strlen(p);
    if (len == 0)
      return -1;
    if ((len == 2) && (!strncmp(p, "..", 2)))
      return -1;
    if (!slash)
      break;
    p = slash + 1;
    }
  return 0;
}
