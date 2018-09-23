/*****************************************************************************/
/*    Copyright (C) 2006-2018 cloonix@cloonix.net License AGPL-3             */
/*                                                                           */
/*  This program is free software: you can redistribute it and/or modify     */
/*  it under the terms of the GNU Affero General Public License as           */
/*  published by the Free Software Foundation, either version 3 of the       */
/*  License, or (at your option) any later version.                          */
/*                                                                           */
/*  This program is distributed in the hope that it will be useful,          */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of           */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            */
/*  GNU Affero General Public License for more details.a                     */
/*                                                                           */
/*  You should have received a copy of the GNU Affero General Public License */
/*  along with this program.  If not, see <http://www.gnu.org/licenses/>.    */
/*                                                                           */
/*****************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#ifndef NO_HMAC_CIPHER

#include <openssl/hmac.h>
#include <openssl/evp.h>
#include "io_clownix.h"
#include "hmac_cipher.h"

static unsigned char glob_key[MSG_DIGEST_LEN];
static const EVP_MD *md_hmac_sha;
static EVP_CIPHER_CTX *cipher_ctx;

/*****************************************************************************/
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
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int cipher( int do_encrypt, unsigned char *iv, 
            int inlen, unsigned char *inbuf,
            unsigned char *outbuf)
{
  int len, tot_len = 0;
  cipher_ctx = EVP_CIPHER_CTX_new();
  if (!cipher_ctx)
    KOUT(" ");
  EVP_CipherInit_ex(cipher_ctx, EVP_aes_256_cbc(), NULL, 
                    glob_key, iv, do_encrypt);
  EVP_CIPHER_CTX_set_key_length(cipher_ctx, MSG_DIGEST_LEN);
  if (EVP_CipherUpdate(cipher_ctx, outbuf, &len, inbuf, inlen))
    {
    tot_len += len;
    if (EVP_CipherFinal_ex(cipher_ctx, outbuf+tot_len, &len))
      tot_len += len;
    }
  EVP_CIPHER_CTX_free(cipher_ctx);
  return tot_len;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int hmac_extract_and_decipher(int len, char *buf, char **outbuf)
{
  int outlen = 0;
  char *md = buf;
  char *md_check;
  *outbuf = NULL;
  if (len > MSG_DIGEST_LEN)
    {
    *outbuf = (char *) clownix_malloc(len, 7);
    outlen = cipher( 0, (unsigned char *) md, len-MSG_DIGEST_LEN, 
    (unsigned char *) (buf+MSG_DIGEST_LEN), (unsigned char *) (*outbuf));   
    if (!outlen)
      {
      clownix_free(*outbuf, __FUNCTION__);
      *outbuf = NULL;
      }
    else
      {
      md_check = compute_msg_digest(outlen, *outbuf);
      if (memcmp(md_check, md, MSG_DIGEST_LEN))
        {
        clownix_free(*outbuf, __FUNCTION__);
        *outbuf = NULL;
        outlen = 0;
        }
      }
    }
  return outlen;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int hmac_insert_and_cipher(int len, char *buf, char **outbuf)
{
  int outlen, max_len;
  char *md = compute_msg_digest(len, buf);
  max_len = len + 2*MSG_DIGEST_LEN;
  *outbuf = (char *) clownix_malloc(max_len, 7);
  memcpy(*outbuf, md, MSG_DIGEST_LEN);
  outlen = cipher( 1, (unsigned char *) md, len, 
           (unsigned char *) buf, (unsigned char *) (*outbuf+MSG_DIGEST_LEN)); 
  if (!outlen)
    {
    clownix_free(*outbuf, __FUNCTION__);
    *outbuf = NULL;
    }
  else
    outlen += MSG_DIGEST_LEN;
  return outlen;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void cipher_change_key(char *key)
{
  if ((strlen(key) < 2) || (strlen(key) >= MSG_DIGEST_LEN))
    KOUT("%d %s", (int) (strlen(key)), key);
  memset(glob_key, 0, MSG_DIGEST_LEN);
  memcpy(glob_key, key, strlen(key));
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void cipher_myinit(void)
{
  cipher_ctx = EVP_CIPHER_CTX_new();
  EVP_add_digest(EVP_sha256());
  md_hmac_sha = EVP_get_digestbyname("SHA256");
  if(!md_hmac_sha)
    KOUT("Unknown message digest \"SHA256\"");
}
/*---------------------------------------------------------------------------*/
#else //NO_HMAC_CIPHER
void cipher_myinit(void)
{
}
#endif //NO_HMAC_CIPHER


