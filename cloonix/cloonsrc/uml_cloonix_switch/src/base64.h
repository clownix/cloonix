/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */

/* This is a public domain base64 implementation written by WEI Zhicheng. */

#define BASE64_ENCODE_OUT_SIZE(s) ((unsigned int)((((s) + 2) / 3) * 4 + 1))

int
base64_encode(char *in, int inlen, char *out);

