/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <openssl/hmac.h>
#include <openssl/evp.h>
#include "io_clownix.h"
#include "hmac_cipher.h"

static unsigned char glob_key[MSG_DIGEST_LEN];
static const EVP_MD *md_hmac_sha;
static EVP_CIPHER_CTX *cipher_ctx;

char *compute_msg_digest(int len, char *data)
{
  unsigned char *md;
  unsigned int md_len;
  md = HMAC(md_hmac_sha, ( unsigned char *)glob_key, MSG_DIGEST_LEN, 
            (const unsigned char *) data, (unsigned int) len, NULL, &md_len);
  if (md_len != MSG_DIGEST_LEN)
    KOUT("%d %d", md_len, MSG_DIGEST_LEN);
  return (char *)md;
}

void cipher_change_key(char *key)
{
  if ((strlen(key) < 2) || (strlen(key) >= MSG_DIGEST_LEN))
    KOUT("%d %s", (int) (strlen(key)), key);
  memset(glob_key, 0, MSG_DIGEST_LEN);
  memcpy(glob_key, key, strlen(key));
}

void cipher_myinit(void)
{
  cipher_ctx = EVP_CIPHER_CTX_new();
  EVP_add_digest(EVP_sha256());
  md_hmac_sha = EVP_get_digestbyname("SHA256");
  if(!md_hmac_sha)
    KOUT("Unknown message digest \"SHA256\"");
}

