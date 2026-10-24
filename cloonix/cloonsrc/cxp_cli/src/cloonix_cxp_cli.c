/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
/* The host end of the cloonix exec pipe: ssh, scp and get. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "glob_common.h"
#include "cxp_wire.h"

#define DOORS_HDR       (16 + MSG_DIGEST_LEN)
#define MAX_DOORS_BUF   50000

#define DOORS_TYPE_DBSSH     doors_type_dbssh
#define DOORS_VAL_NONE       doors_val_none
#define DOORS_VAL_INIT_LINK  doors_val_init_link
#define DOORS_VAL_LINK_OK    doors_val_link_ok
#define DOORS_VAL_LINK_KO    doors_val_link_ko

#define CXP_COOKIE  "0000cxp"

#define OP_EXEC   0x01
#define OP_PTY    0x02
#define OP_STDIN  0x03
#define OP_EOF    0x04
#define OP_WINCH  0x05
#define OP_STDOUT 0x81
#define OP_STDERR 0x82
#define OP_EXIT   0x83
#define OP_ERROR  0x84

static unsigned char g_key[MSG_DIGEST_LEN];
static const EVP_MD *g_md;
static int g_sock = -1;
static SSL *g_ssl;
static int g_nb_pkt;
static volatile sig_atomic_t g_winch;
static struct termios g_saved_tio;
static int g_tio_saved;

static int write_all(int fd, const void *buf, int len)
{
  const char *p = (const char *) buf;
  int done = 0, n;
  while (done < len)
    {
    n = write(fd, p + done, len - done);
    if (n <= 0)
      {
      if ((n < 0) && (errno == EINTR))
        continue;
      return -1;
      }
    done += n;
    }
  return 0;
}

static int sock_write(const void *buf, int len)
{
  const char *p = (const char *) buf;
  int done = 0, n;
  if (!g_ssl)
    return write_all(g_sock, buf, len);
  while (done < len)
    {
    n = SSL_write(g_ssl, p + done, len - done);
    if (n <= 0)
      return -1;
    done += n;
    }
  return 0;
}

static int sock_read(void *buf, int len)
{
  if (!g_ssl)
    return read(g_sock, buf, len);
  return SSL_read(g_ssl, buf, len);
}

static int tls_wrap(int fd)
{
  int ret, err;
  SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
  if (!ctx)
    return -1;
  SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
  SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);
  g_ssl = SSL_new(ctx);
  SSL_CTX_free(ctx);
  if (!g_ssl)
    return -1;
  SSL_set_fd(g_ssl, fd);
  ret = SSL_connect(g_ssl);
  if (ret == 1)
    return 0;
  err = SSL_get_error(g_ssl, ret);
  SSL_free(g_ssl);
  g_ssl = NULL;
  if ((err == SSL_ERROR_ZERO_RETURN) ||
      ((err == SSL_ERROR_SYSCALL) && ((errno == 0) || (errno == ECONNRESET)
                                      || (errno == EPIPE))))
    return -1;
  fprintf(stderr, "tls handshake failed, ssl error %d errno %d\n", err, errno);
  return -2;
}

/* One doorways frame: header, hmac of the payload, payload. */
static int doors_tx(int type, int val, const char *payload, int len)
{
  unsigned char head[DOORS_HDR];
  unsigned int md_len;
  unsigned char *md;
  int i = 0;

  g_nb_pkt = (g_nb_pkt >= 0xFFFF) ? 1 : g_nb_pkt + 1;
  head[i++] = 0xCA; head[i++] = 0xFE;
  head[i++] = 0;    head[i++] = 0;                       /* tid 0 */
  head[i++] = (len >> 24) & 0xFF; head[i++] = (len >> 16) & 0xFF;
  head[i++] = (len >> 8)  & 0xFF; head[i++] = len & 0xFF;
  head[i++] = (type >> 8) & 0xFF; head[i++] = type & 0xFF;
  head[i++] = (val >> 8)  & 0xFF; head[i++] = val & 0xFF;
  head[i++] = (g_nb_pkt >> 8) & 0xFF; head[i++] = g_nb_pkt & 0xFF;
  md = HMAC(g_md, g_key, MSG_DIGEST_LEN,
            (const unsigned char *) payload, (unsigned int) len, NULL, &md_len);
  memcpy(head + i, md, MSG_DIGEST_LEN);
  i += MSG_DIGEST_LEN;
  head[i++] = 0xDE; head[i++] = 0xCA;
  if (sock_write(head, DOORS_HDR))
    return -1;
  if ((len) && (sock_write(payload, len)))
    return -1;
  return 0;
}

/* The leg payloads make one byte stream; the cxp frames are cut from it. */
static char *g_doors_buf;
static int   g_doors_len, g_doors_max;
static char *g_strm_buf;
static int   g_strm_len, g_strm_max;

static int grow(char **buf, int *max, int need)
{
  char *nb;
  if (need <= *max)
    return 0;
  while (*max < need)
    *max = (*max) ? (*max) * 2 : 65536;
  nb = (char *) realloc(*buf, *max);
  if (!nb)
    return -1;
  *buf = nb;
  return 0;
}

static int doors_feed(const char *data, int len)
{
  if (grow(&g_doors_buf, &g_doors_max, g_doors_len + len))
    return -1;
  memcpy(g_doors_buf + g_doors_len, data, len);
  g_doors_len += len;
  for (;;)
    {
    unsigned char *b = (unsigned char *) g_doors_buf;
    int plen, type, val;
    if (g_doors_len < DOORS_HDR)
      return 0;
    if ((b[0] != 0xCA) || (b[1] != 0xFE))
      return -1;
    plen = (b[4] << 24) | (b[5] << 16) | (b[6] << 8) | b[7];
    type = (b[8] << 8) | b[9];
    val  = (b[10] << 8) | b[11];
    (void) type;
    if ((plen < 0) || (plen > MAX_DOORS_BUF))
      return -1;
    if (g_doors_len < DOORS_HDR + plen)
      return 0;
    if (val == DOORS_VAL_NONE)
      {
      if (grow(&g_strm_buf, &g_strm_max, g_strm_len + plen))
        return -1;
      memcpy(g_strm_buf + g_strm_len, g_doors_buf + DOORS_HDR, plen);
      g_strm_len += plen;
      }
    memmove(g_doors_buf, g_doors_buf + DOORS_HDR + plen,
            g_doors_len - DOORS_HDR - plen);
    g_doors_len -= DOORS_HDR + plen;
    }
}

static int cxp_tx(int op, const char *payload, int len)
{
  char head[5];
  int tot = len + 1;
  char *msg;
  int rc;
  head[0] = (tot >> 24) & 0xFF; head[1] = (tot >> 16) & 0xFF;
  head[2] = (tot >> 8)  & 0xFF; head[3] = tot & 0xFF;
  head[4] = op & 0xFF;
  msg = (char *) malloc(5 + len);
  if (!msg)
    return -1;
  memcpy(msg, head, 5);
  if (len)
    memcpy(msg + 5, payload, len);
  rc = doors_tx(DOORS_TYPE_DBSSH, DOORS_VAL_NONE, msg, 5 + len);
  free(msg);
  return rc;
}

/* Next protocol frame out of the stream, 0 when more bytes are needed. */
static int cxp_next(int *op, char **payload, int *plen)
{
  int tot;
  unsigned char *b = (unsigned char *) g_strm_buf;
  if (g_strm_len < 5)
    return 0;
  tot = (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3];
  if ((tot < 1) || (tot > 4 * 1024 * 1024))
    return -1;
  if (g_strm_len < 4 + tot)
    return 0;
  *op = b[4];
  *plen = tot - 1;
  *payload = g_strm_buf + 5;
  return 1;
}

static void cxp_pop(int plen)
{
  int used = 5 + plen;
  memmove(g_strm_buf, g_strm_buf + used, g_strm_len - used);
  g_strm_len -= used;
}

static int connect_target(const char *target)
{
  int fd, port;
  char ip[256];
  const char *colon;

  if (target[0] == '/')
    {
    struct sockaddr_un addr;
    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
      return -1;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path) - 1, "%s", target);
    if (connect(fd, (struct sockaddr *) &addr, sizeof(addr)))
      {
      close(fd);
      return -1;
      }
    return fd;
    }
  colon = strrchr(target, ':');
  if (!colon)
    return -1;
  if ((int) (colon - target) >= (int) sizeof(ip))
    return -1;
  memset(ip, 0, sizeof(ip));
  memcpy(ip, target, colon - target);
  port = atoi(colon + 1);
  {
  struct sockaddr_in addr;
  int on = 1, ret;
  fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0)
    return -1;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons((unsigned short) port);
  if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1)
    {
    close(fd);
    return -1;
    }
  if (connect(fd, (struct sockaddr *) &addr, sizeof(addr)))
    {
    close(fd);
    return -1;
    }
  setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));
  ret = tls_wrap(fd);
  if (ret == -2)
    {
    close(fd);
    return -1;
    }
  if (ret)
    {
    close(fd);
    if ((getenv("CLOONIX_REQUIRE_TLS")) &&
        (!strcmp(getenv("CLOONIX_REQUIRE_TLS"), "1")))
      {
      fprintf(stderr, "%s does not speak tls and CLOONIX_REQUIRE_TLS=1 "
                      "refuses an unencrypted console\n", target);
      return -1;
      }
    fprintf(stderr, "%s closed the socket on the tls hello, it has no tls: "
                    "the console is not encrypted\n", target);
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
      return -1;
    if (connect(fd, (struct sockaddr *) &addr, sizeof(addr)))
      {
      close(fd);
      return -1;
      }
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));
    }
  }
  return fd;
}

static void on_winch(int sig)
{
  (void) sig;
  g_winch = 1;
}

static void restore_tio(void)
{
  if (g_tio_saved)
    tcsetattr(0, TCSADRAIN, &g_saved_tio);
}

static void send_winch(void)
{
  struct winsize ws;
  char msg[128];
  if (ioctl(0, TIOCGWINSZ, &ws))
    return;
  snprintf(msg, sizeof(msg), "{\"cols\":%d,\"rows\":%d}",
           ws.ws_col ? ws.ws_col : 80, ws.ws_row ? ws.ws_row : 24);
  cxp_tx(OP_WINCH, msg, (int) strlen(msg));
}

int main(int argc, char **argv)
{
  char req[512], msg[8192], cmd[4096], esc[8192];
  char buf[16384];
  int i, n, op, plen, exit_code = 0, linked = 0, done = 0;
  int stdin_open = 1;
  char *payload, *passwd;
  struct winsize ws;

  if (argc < 4)
    {
    fprintf(stderr,
            "usage: %s <ip:port | unix_socket> <passwd> <vm> [command...]\n",
            argv[0]);
    return 1;
    }
  EVP_add_digest(EVP_sha256());
  g_md = EVP_get_digestbyname("SHA256");
  if (!g_md)
    {
    fprintf(stderr, "no sha256\n");
    return 1;
    }
  memset(g_key, 0, MSG_DIGEST_LEN);
  passwd = argv[2];
  if (!strcmp(passwd, "-"))
    {
    passwd = getenv("CLOONIX_PASSWD");
    if ((!passwd) || (!passwd[0]))
      {
      fprintf(stderr, "passwd is - and CLOONIX_PASSWD is not set\n");
      return 1;
      }
    }
  n = (int) strlen(passwd);
  if (n > MSG_DIGEST_LEN)
    n = MSG_DIGEST_LEN;
  memcpy(g_key, passwd, n);

  g_sock = connect_target(argv[1]);
  if (g_sock < 0)
    {
    fprintf(stderr, "cannot reach cloonix on %s\n", argv[1]);
    return 1;
    }

  n = snprintf(req, sizeof(req),
               "DBSSH_CLI_DOORS_REQ name=%s cookie=%s" CLOONIX_WIRE_VERSION_FMT,
               argv[3], CXP_COOKIE, CLOONIX_WIRE_VERSION);
  if (doors_tx(DOORS_TYPE_DBSSH, DOORS_VAL_INIT_LINK, req, n + 1))
    {
    fprintf(stderr, "cannot send the leg request\n");
    return 1;
    }

  while (!linked)
    {
    unsigned char *b;
    n = sock_read(buf, sizeof(buf));
    if (n <= 0)
      {
      fprintf(stderr, "cloonix closed the leg to %s\n", argv[3]);
      return 1;
      }
    if (grow(&g_doors_buf, &g_doors_max, g_doors_len + n))
      return 1;
    memcpy(g_doors_buf + g_doors_len, buf, n);
    g_doors_len += n;
    b = (unsigned char *) g_doors_buf;
    if (g_doors_len >= DOORS_HDR)
      {
      int lp = (b[4] << 24) | (b[5] << 16) | (b[6] << 8) | b[7];
      int val = (b[10] << 8) | b[11];
      if (g_doors_len >= DOORS_HDR + lp)
        {
        if (val == DOORS_VAL_LINK_KO)
          {
          fprintf(stderr, "cloonix refused the leg to %s\n", argv[3]);
          return 1;
          }
        if (val == DOORS_VAL_LINK_OK)
          linked = 1;
        else if (val != DOORS_VAL_NONE)
          {
          /* Anything else here means the two ends do not agree on what the
             values mean. */
          fprintf(stderr, "cloonix answered the leg request for %s with an "
                  "unknown value %d, expected %d: this cloonix and this "
                  "client are not the same build\n",
                  argv[3], val, DOORS_VAL_LINK_OK);
          return 1;
          }
        memmove(g_doors_buf, g_doors_buf + DOORS_HDR + lp,
                g_doors_len - DOORS_HDR - lp);
        g_doors_len -= DOORS_HDR + lp;
        }
      }
    }

  cmd[0] = 0;
  for (i = 4; i < argc; i++)
    {
    if (cmd[0])
      strncat(cmd, " ", sizeof(cmd) - strlen(cmd) - 1);
    strncat(cmd, argv[i], sizeof(cmd) - strlen(cmd) - 1);
    }
  cxp_json_escape(cmd, esc, sizeof(esc));

  if (cmd[0])
    {
    n = snprintf(msg, sizeof(msg), "{\"cmd\":\"%s\"}", esc);
    cxp_tx(OP_EXEC, msg, n);
    }
  else
    {
    int cols = 80, rows = 24;
    if (!ioctl(0, TIOCGWINSZ, &ws))
      {
      if (ws.ws_col)
        cols = ws.ws_col;
      if (ws.ws_row)
        rows = ws.ws_row;
      }
    n = snprintf(msg, sizeof(msg),
                 "{\"cmd\":\"\",\"cols\":%d,\"rows\":%d}", cols, rows);
    cxp_tx(OP_PTY, msg, n);
    if (isatty(0))
      {
      struct termios raw;
      if (!tcgetattr(0, &g_saved_tio))
        {
        g_tio_saved = 1;
        raw = g_saved_tio;
        cfmakeraw(&raw);
        tcsetattr(0, TCSANOW, &raw);
        atexit(restore_tio);
        }
      }
    signal(SIGWINCH, on_winch);
    }

  signal(SIGPIPE, SIG_IGN);

  while (!done)
    {
    fd_set rfds;
    int maxfd = g_sock;
    FD_ZERO(&rfds);
    FD_SET(g_sock, &rfds);
    if (stdin_open)
      FD_SET(0, &rfds);
    /* A tls record already decrypted is not seen by select. */
    if ((g_ssl) && (SSL_pending(g_ssl) > 0))
      {
      struct timeval tv = {0, 0};
      if (select(maxfd + 1, &rfds, NULL, NULL, &tv) < 0)
        {
        if (errno == EINTR)
          continue;
        break;
        }
      FD_SET(g_sock, &rfds);
      }
    else if (select(maxfd + 1, &rfds, NULL, NULL, NULL) < 0)
      {
      if (errno == EINTR)
        {
        if (g_winch)
          {
          g_winch = 0;
          send_winch();
          }
        continue;
        }
      break;
      }
    if ((stdin_open) && (FD_ISSET(0, &rfds)))
      {
      n = read(0, buf, sizeof(buf));
      if (n > 0)
        cxp_tx(OP_STDIN, buf, n);
      else if ((n < 0) && (errno == EINTR))
        ;
      else
        {
        cxp_tx(OP_EOF, NULL, 0);
        stdin_open = 0;
        }
      }
    if (FD_ISSET(g_sock, &rfds))
      {
      n = sock_read(buf, sizeof(buf));
      if (n <= 0)
        break;
      if (doors_feed(buf, n))
        {
        fprintf(stderr, "bad frame from cloonix\n");
        break;
        }
      for (;;)
        {
        int rc = cxp_next(&op, &payload, &plen);
        if (rc < 0)
          {
          done = 1;
          break;
          }
        if (rc == 0)
          break;
        if (op == OP_STDOUT)
          write_all(1, payload, plen);
        else if (op == OP_STDERR)
          write_all(2, payload, plen);
        else if (op == OP_EXIT)
          {
          char jbuf[256];
          char *p;
          int cp = (plen < (int) sizeof(jbuf) - 1) ? plen
                                                   : (int) sizeof(jbuf) - 1;
          memcpy(jbuf, payload, cp);
          jbuf[cp] = 0;
          if (getenv("CXP_DEBUG")) fprintf(stderr, "[EXIT frame %d bytes: %s]\n", plen, jbuf);
          p = strstr(jbuf, "\"code\"");
          if (p)
            {
            p = strchr(p, ':');
            if (p)
              exit_code = atoi(p + 1);
            }
          done = 1;
          }
        else if (op == OP_ERROR)
          {
          write_all(2, payload, plen);
          write_all(2, "\n", 1);
          exit_code = 255;
          done = 1;
          }
        cxp_pop(plen);
        }
      }
    }
  restore_tio();
  if (g_ssl)
    SSL_free(g_ssl);
  close(g_sock);
  return exit_code;
}
