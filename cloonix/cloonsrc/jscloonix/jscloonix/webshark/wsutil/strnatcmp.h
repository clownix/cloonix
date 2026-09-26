/** @file
 *
 * Original code downloaded from: http://sourcefrog.net/projects/natsort/

  strnatcmp.c -- Perform 'natural order' comparisons of strings in C.
  Copyright (C) 2000, 2004 by Martin Pool <mbp sourcefrog net>

  SPDX-License-Identifier: Zlib
 */

#ifndef STRNATCMP_H
#define STRNATCMP_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef char nat_char;

int ws_ascii_strnatcmp(nat_char const *a, nat_char const *b);
int ws_ascii_strnatcasecmp(nat_char const *a, nat_char const *b);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* STRNATCMP_H */
