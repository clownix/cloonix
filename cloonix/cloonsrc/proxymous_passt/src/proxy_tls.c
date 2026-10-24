/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
/* TLS termination for the https web port and the doors port. OpenSSL */
/* never touches a descriptor: a pair of memory BIOs. */
/* peer -> [rbio] -> SSL_read  -> plaintext -> unix socket -> crun */
/* peer <- [wbio] <- SSL_write <- plaintext <- unix socket <- crun */
/* After every operation wbio is emptied towards the peer. Plaintext from */
/* the crun before the handshake ends is held. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/x509v3.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "proxy_crun.h"
#include "proxy_tls.h"

char *get_net_name(void);

#define TLS_CHUNK       16384
#define TLS_MAX_PENDING (4 * 1024 * 1024)

typedef struct t_tls_conn
{
  SSL  *ssl;
  BIO  *rbio;
  BIO  *wbio;
  char *pending;        /* plaintext waiting for the handshake to finish */
  int   pending_len;
  int   broken;
} t_tls_conn;

static SSL_CTX *g_ctx;
static char g_cert_path[MAX_PATH_LEN];

/* Looked for in CLOONIX_TLS_DIR, <exe dir>/../config, /usr/share/cloonix/ */
/* cert. Both files must be there for a directory to count. */
static int try_dir(const char *dir, char *cert, char *key)
{
  if ((!dir) || (!dir[0]))
    return -1;
  if (snprintf(cert, MAX_PATH_LEN, "%s/cloonix_cert.pem", dir) >= MAX_PATH_LEN)
    return -1;
  if (snprintf(key, MAX_PATH_LEN, "%s/cloonix_key.pem", dir) >= MAX_PATH_LEN)
    return -1;
  if (access(cert, R_OK) || access(key, R_OK))
    return -1;
  return 0;
}

static int find_cert(char *cert, char *key)
{
  char self[MAX_PATH_LEN], dir[MAX_PATH_LEN], *slash;
  int len;

  if (!try_dir(getenv("CLOONIX_TLS_DIR"), cert, key))
    return 0;
  memset(self, 0, MAX_PATH_LEN);
  len = (int) readlink("/proc/self/exe", self, MAX_PATH_LEN-1);
  if (len > 0)
    {
    self[len] = 0;
    slash = strrchr(self, '/');
    if (slash)
      {
      *slash = 0;
      if (snprintf(dir, MAX_PATH_LEN, "%s/../config", self) < MAX_PATH_LEN)
        {
        if (!try_dir(dir, cert, key))
          return 0;
        }
      }
    }
  if (!try_dir("/usr/share/cloonix/cert", cert, key))
    return 0;
  return -1;
}

static int write_pem(const char *path, int mode,
                     int (*wr)(FILE *, void *), void *obj)
{
  int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, mode);
  FILE *f;
  int ok;
  if (fd < 0)
    return -1;
  f = fdopen(fd, "w");
  if (!f)
    {
    KERR("ERROR fdopen: %s", strerror(errno));
    close(fd);
    return -1;
    }
  ok = wr(f, obj);
  fclose(f);
  return ok ? 0 : -1;
}

static int wr_cert(FILE *f, void *obj)
{
  return PEM_write_X509(f, (X509 *) obj);
}

static int wr_key(FILE *f, void *obj)
{
  return PEM_write_PrivateKey(f, (EVP_PKEY *) obj, NULL, NULL, 0, NULL, NULL);
}

static int make_cert_in(const char *dir, char *cert, char *key)
{
  EVP_PKEY *pkey = NULL;
  X509 *x = NULL;
  X509_NAME *name;
  X509_EXTENSION *ext;
  X509V3_CTX v3;
  int result = -1;
  if (mkdir(dir, 0755) && (errno != EEXIST))
    return -1;
  if (access(dir, W_OK))
    return -1;
  if ((snprintf(cert, MAX_PATH_LEN, "%s/cloonix_cert.pem", dir) >= MAX_PATH_LEN) ||
      (snprintf(key, MAX_PATH_LEN, "%s/cloonix_key.pem", dir) >= MAX_PATH_LEN))
    return -1;
  pkey = EVP_RSA_gen(2048);
  x = X509_new();
  if ((!pkey) || (!x))
    goto out;
  X509_set_version(x, 2);
  ASN1_INTEGER_set(X509_get_serialNumber(x), (long) time(NULL));
  X509_gmtime_adj(X509_get_notBefore(x), 0);
  X509_gmtime_adj(X509_get_notAfter(x), 10L * 365 * 24 * 3600);
  X509_set_pubkey(x, pkey);
  name = X509_get_subject_name(x);
  X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                             (const unsigned char *) "cloonix", -1, -1, 0);
  X509_set_issuer_name(x, name);
  X509V3_set_ctx(&v3, x, x, NULL, NULL, 0);
  ext = X509V3_EXT_conf_nid(NULL, &v3, NID_subject_alt_name,
                            "IP:127.0.0.1,DNS:localhost");
  if (ext)
    {
    X509_add_ext(x, ext, -1);
    X509_EXTENSION_free(ext);
    }
  if (X509_sign(x, pkey, EVP_sha256()) <= 0)
    goto out;
  if (write_pem(key, 0600, wr_key, pkey) || write_pem(cert, 0644, wr_cert, x))
    {
    unlink(key);
    unlink(cert);
    goto out;
    }
  result = 0;
out:
  if (x)
    X509_free(x);
  if (pkey)
    EVP_PKEY_free(pkey);
  return result;
}

static int make_cert(char *cert, char *key)
{
  char self[MAX_PATH_LEN], dir[MAX_PATH_LEN], *slash;
  const char *env = getenv("CLOONIX_TLS_DIR");
  int len;
  if ((env) && (env[0]) && (!make_cert_in(env, cert, key)))
    return 0;
  memset(self, 0, MAX_PATH_LEN);
  len = (int) readlink("/proc/self/exe", self, MAX_PATH_LEN-1);
  if (len > 0)
    {
    self[len] = 0;
    slash = strrchr(self, '/');
    if (slash)
      {
      *slash = 0;
      if (snprintf(dir, MAX_PATH_LEN, "%s/../config", self) < MAX_PATH_LEN)
        {
        if ((!access(dir, W_OK)) && (!make_cert_in(dir, cert, key)))
          return 0;
        }
      }
    }
  if (!make_cert_in("/usr/share/cloonix/cert", cert, key))
    return 0;
  if (snprintf(dir, MAX_PATH_LEN, "/var/lib/cloonix/%s/cert",
               get_net_name()) < MAX_PATH_LEN)
    {
    if (!make_cert_in(dir, cert, key))
      return 0;
    }
  return -1;
}

static void log_ssl_err(const char *who)
{
  unsigned long e;
  char buf[256];
  int said_cert = 0;
  static time_t last_cert;
  static unsigned long nb_cert;
  while ((e = ERR_get_error()) != 0)
    {
    ERR_error_string_n(e, buf, sizeof(buf));
    if (strstr(buf, "bad certificate"))
      {
      /* An open tab retries each minute: the advice once per ten minutes. */
      if (said_cert)
        continue;
      said_cert = 1;
      nb_cert += 1;
      if (time(NULL) - last_cert < 600)
        continue;
      last_cert = time(NULL);
      KERR("ERROR TLS %s: the browser refused the certificate %s because it "
           "trusts another one for this address, from an earlier toy or "
           "bundle on the same port. In firefox: Settings, Privacy, "
           "Certificates, View Certificates, Servers, remove the entry for "
           "this address, then load the page and accept the warning once "
           "(%lu refusal(s) so far)",
           who, g_cert_path[0] ? g_cert_path : "(unknown)", nb_cert);
      }
    else if (strstr(buf, "http request"))
      KERR("WARNING TLS %s: a plain http request reached this https port, "
           "the address in the browser must start with https://", who);
    else
      KERR("ERROR TLS %s: %s", who, buf);
    }
}

int proxy_tls_init(void)
{
  char cert[MAX_PATH_LEN], key[MAX_PATH_LEN];

  if (g_ctx)
    return 0;
  memset(g_cert_path, 0, MAX_PATH_LEN);
  if (find_cert(cert, key))
    {
    if (make_cert(cert, key))
      {
      KERR("ERROR TLS no cloonix_cert.pem and cloonix_key.pem found and "
           "none could be written, no tls on the web and doors ports");
      return -1;
      }
    KERR("WARNING TLS no certificate was found, one was made: %s", cert);
    }
  OPENSSL_init_ssl(0, NULL);
  SSL_load_error_strings();
  g_ctx = SSL_CTX_new(TLS_server_method());
  if (!g_ctx)
    {
    log_ssl_err("SSL_CTX_new");
    return -1;
    }
  SSL_CTX_set_min_proto_version(g_ctx, TLS1_2_VERSION);
  SSL_CTX_set_options(g_ctx, SSL_OP_NO_COMPRESSION);
  SSL_CTX_set_mode(g_ctx, SSL_MODE_ENABLE_PARTIAL_WRITE |
                          SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
  if (SSL_CTX_use_certificate_chain_file(g_ctx, cert) != 1)
    {
    log_ssl_err("use_certificate_chain_file");
    SSL_CTX_free(g_ctx);
    g_ctx = NULL;
    return -1;
    }
  if (SSL_CTX_use_PrivateKey_file(g_ctx, key, SSL_FILETYPE_PEM) != 1)
    {
    log_ssl_err("use_PrivateKey_file");
    SSL_CTX_free(g_ctx);
    g_ctx = NULL;
    return -1;
    }
  if (SSL_CTX_check_private_key(g_ctx) != 1)
    {
    KERR("ERROR TLS %s does not go with %s", key, cert);
    SSL_CTX_free(g_ctx);
    g_ctx = NULL;
    return -1;
    }
  snprintf(g_cert_path, MAX_PATH_LEN, "%s", cert);
  return 0;
}

void *proxy_tls_alloc(void)
{
  t_tls_conn *cur;
  if (!g_ctx)
    return NULL;
  cur = (t_tls_conn *) malloc(sizeof(t_tls_conn));
  if (!cur)
    {
    KERR("ERROR no memory for a tls connection");
    return NULL;
    }
  memset(cur, 0, sizeof(t_tls_conn));
  cur->ssl = SSL_new(g_ctx);
  if (!cur->ssl)
    {
    log_ssl_err("SSL_new");
    free(cur);
    return NULL;
    }
  cur->rbio = BIO_new(BIO_s_mem());
  cur->wbio = BIO_new(BIO_s_mem());
  if ((!cur->rbio) || (!cur->wbio))
    {
    if (cur->rbio)
      BIO_free(cur->rbio);
    if (cur->wbio)
      BIO_free(cur->wbio);
    SSL_free(cur->ssl);
    free(cur);
    return NULL;
    }
  /* SSL_free takes the two BIOs with it, they must not be freed twice */
  SSL_set_bio(cur->ssl, cur->rbio, cur->wbio);
  SSL_set_accept_state(cur->ssl);
  return (void *) cur;
}

void proxy_tls_free(void *handle)
{
  t_tls_conn *cur = (t_tls_conn *) handle;
  if (!cur)
    return;
  if (cur->pending)
    free(cur->pending);
  if (cur->ssl)
    SSL_free(cur->ssl);
  free(cur);
}

static void flush_wbio(t_tls_conn *cur, t_proxy_tls_out cipher_cb, void *user)
{
  char buf[TLS_CHUNK];
  int n;
  for (;;)
    {
    n = BIO_read(cur->wbio, buf, sizeof(buf));
    if (n <= 0)
      break;
    cipher_cb(user, n, buf);
    }
}

/* An error that is not WANT_READ or WANT_WRITE ends the connection. */
static int fatal_err(t_tls_conn *cur, int ret, const char *who)
{
  int err = SSL_get_error(cur->ssl, ret);
  if ((err == SSL_ERROR_WANT_READ) || (err == SSL_ERROR_WANT_WRITE))
    return 0;
  if ((err == SSL_ERROR_ZERO_RETURN) || (err == SSL_ERROR_SYSCALL))
    {
    cur->broken = 1;
    return -1;
    }
  log_ssl_err(who);
  cur->broken = 1;
  return -1;
}

static int drain_pending(t_tls_conn *cur, t_proxy_tls_out cipher_cb, void *user)
{
  int n, done = 0;
  if ((!cur->pending) || (!SSL_is_init_finished(cur->ssl)))
    return 0;
  while (done < cur->pending_len)
    {
    n = SSL_write(cur->ssl, cur->pending + done, cur->pending_len - done);
    if (n <= 0)
      {
      if (fatal_err(cur, n, "SSL_write pending"))
        return -1;
      break;
      }
    done += n;
    }
  flush_wbio(cur, cipher_cb, user);
  if (done >= cur->pending_len)
    {
    free(cur->pending);
    cur->pending = NULL;
    cur->pending_len = 0;
    }
  else if (done > 0)
    {
    memmove(cur->pending, cur->pending + done, cur->pending_len - done);
    cur->pending_len -= done;
    }
  return 0;
}

int proxy_tls_rx_cipher(void *handle, int len, char *buf,
                        t_proxy_tls_out plain_cb,
                        t_proxy_tls_out cipher_cb, void *user)
{
  t_tls_conn *cur = (t_tls_conn *) handle;
  char plain[TLS_CHUNK];
  int n, done = 0;

  if ((!cur) || (cur->broken))
    return -1;
  while (done < len)
    {
    n = BIO_write(cur->rbio, buf + done, len - done);
    if (n <= 0)
      {
      KERR("ERROR TLS BIO_write refused %d bytes", len - done);
      cur->broken = 1;
      return -1;
      }
    done += n;
    }

  if (!SSL_is_init_finished(cur->ssl))
    {
    n = SSL_accept(cur->ssl);
    flush_wbio(cur, cipher_cb, user);
    if (n != 1)
      {
      if (fatal_err(cur, n, "SSL_accept"))
        return -1;
      return 0;
      }
    if (drain_pending(cur, cipher_cb, user))
      return -1;
    }

  /* Drain SSL_read: one BIO_write may hold several records. */
  for (;;)
    {
    n = SSL_read(cur->ssl, plain, sizeof(plain));
    if (n > 0)
      {
      plain_cb(user, n, plain);
      continue;
      }
    if (fatal_err(cur, n, "SSL_read"))
      return -1;
    break;
    }
  flush_wbio(cur, cipher_cb, user);
  return 0;
}

int proxy_tls_tx_plain(void *handle, int len, char *buf,
                       t_proxy_tls_out cipher_cb, void *user)
{
  t_tls_conn *cur = (t_tls_conn *) handle;
  int n, done = 0;
  char *bigger;

  if ((!cur) || (cur->broken))
    return -1;

  if ((!SSL_is_init_finished(cur->ssl)) || (cur->pending))
    {
    if (cur->pending_len + len > TLS_MAX_PENDING)
      {
      KERR("ERROR TLS too much held before handshake, closing");
      cur->broken = 1;
      return -1;
      }
    bigger = (char *) realloc(cur->pending, cur->pending_len + len);
    if (!bigger)
      {
      KERR("ERROR no memory for the pending bytes of a tls connection");
      cur->broken = 1;
      return -1;
      }
    cur->pending = bigger;
    memcpy(cur->pending + cur->pending_len, buf, len);
    cur->pending_len += len;
    if (SSL_is_init_finished(cur->ssl))
      {
      if (drain_pending(cur, cipher_cb, user))
        return -1;
      }
    return 0;
    }

  while (done < len)
    {
    n = SSL_write(cur->ssl, buf + done, len - done);
    if (n <= 0)
      {
      if (fatal_err(cur, n, "SSL_write"))
        return -1;
      break;
      }
    done += n;
    }
  flush_wbio(cur, cipher_cb, user);
  return 0;
}
