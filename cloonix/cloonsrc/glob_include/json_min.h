/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
char *json_find_key(char *buf, const char *key);
int   json_get_int(char *buf, const char *key, int *out);
int   json_get_uint(char *buf, const char *key, unsigned int *out);
char *json_get_str(char *buf, const char *key, char *out, int maxlen);
int   json_escape(char *dst, const char *src, int dstlen);
void  json_fctident(char *input, char *output);
