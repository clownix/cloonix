/* Websocket /play: two header lines (#!cloonix-play-dir, #!cloonix-play-name)
   name a demo that proxymous runs from disk; browser bytes are never run.
   Output streams back as text frames, last one "[DONE ok=<n> ko=<n>]". */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/types.h>

#define DEMO_TRACE 0
#if DEMO_TRACE
#define DLOG(fmt, ...) KERR("DEMOEXEC GUI " fmt, ##__VA_ARGS__)
#else
#define DLOG(fmt, ...) do { if (0) KERR(fmt, ##__VA_ARGS__); } while(0)
#endif
#include <sys/socket.h>
#include <sys/un.h>

#include <poll.h>
#include "web_common.h"
#include "pthexec.h"

/* Must match DEMO_EXEC_SOCK of proxymous_passt/src/demo_exec.h: the socket
   in the proxyshare directory on which proxymous accepts a demo to run. */
#define DEMO_EXEC_SOCK  "proxymous_demo.sock"

#define PLAY_MAX_SCRIPT (1024 * 1024)   /* 1 MB script cap */

#define HOST_RDBUF      8192

static __thread char g_play_dir[512];
static __thread char g_play_name[256];

/* Runner connections of the demos going on, for play_stop_all. */
#define PLAY_MAX_RUN 64
static int g_run_fd[PLAY_MAX_RUN];
static int g_run_stopped[PLAY_MAX_RUN];
static int g_nb_run;
static pthread_mutex_t g_run_mutex = PTHREAD_MUTEX_INITIALIZER;

static void run_add(int fd)
{
  pthread_mutex_lock(&g_run_mutex);
  if (g_nb_run < PLAY_MAX_RUN)
    {
    g_run_fd[g_nb_run] = fd;
    g_run_stopped[g_nb_run++] = 0;
    }
  else
    KERR("WARNING play: more than %d demos, one cannot be stopped",
         PLAY_MAX_RUN);
  pthread_mutex_unlock(&g_run_mutex);
}

/* Returns 1 if play_stop_all stopped it. */
static int run_del(int fd)
{
  int i, stopped = 0;
  pthread_mutex_lock(&g_run_mutex);
  for (i = 0; i < g_nb_run; i++)
    {
    if (g_run_fd[i] == fd)
      {
      stopped = g_run_stopped[i];
      g_nb_run--;
      g_run_fd[i] = g_run_fd[g_nb_run];
      g_run_stopped[i] = g_run_stopped[g_nb_run];
      break;
      }
    }
  pthread_mutex_unlock(&g_run_mutex);
  return stopped;
}

/* Half-close: the runner kills the demo and answers its E line. */
void play_stop_all(void)
{
  int i, nb;
  pthread_mutex_lock(&g_run_mutex);
  for (i = 0; i < g_nb_run; i++)
    {
    shutdown(g_run_fd[i], SHUT_WR);
    g_run_stopped[i] = 1;
    }
  pthread_mutex_unlock(&g_run_mutex);
  for (i = 0; i < 100; i++)
    {
    pthread_mutex_lock(&g_run_mutex);
    nb = g_nb_run;
    pthread_mutex_unlock(&g_run_mutex);
    if (nb == 0)
      return;
    usleep(100000);
    }
  KERR("WARNING play: %d demo(s) still running 10s after the stop", nb);
}

static void play_send(int fd, const char *s)
{
  ws_send_text(fd, s, (int)strlen(s));
}

static void play_send_output_line(int fd, const char *s)
{
  char *tmp;
  int len;
  len = (int) strlen(s);
  tmp = (char *) malloc(len + 2);
  if (!tmp)
    {
    KERR("ERROR play: no memory for a %d byte output line", len);
    ws_send_text(fd, s, len);
    return;
    }
  memcpy(tmp, s, len);
  tmp[len] = '\n';
  tmp[len+1] = 0;
  ws_send_text(fd, tmp, len + 1);
  free(tmp);
}

static int play_first_is_shebang(char *script)
{
  char *nl;
  int len;
  if ((script[0] != '#') || (script[1] != '!'))
    return 0;
  nl = strchr(script, '\n');
  len = nl ? (int) (nl - script) : (int) strlen(script);
  while ((len > 0) && ((script[len-1] == ' ') || (script[len-1] == '\r')))
    len--;
  if (memchr(script, '/', len))
    return 1;
  return 0;
}

static char *play_take_header(char *script, const char *key, char *out, int sz)
{
  int klen = (int) strlen(key);
  char *nl;
  if (strncmp(script, key, klen))
    return script;
  nl = strchr(script, '\n');
  if (!nl)
    return script;
  *nl = 0;
  snprintf(out, sz, "%s", script + klen);
  *nl = '\n';
  return nl + 1;
}

static int play_name_is_sane(const char *s)
{
  int i;
  if ((!s) || (!s[0]) || (s[0] == '.') || (s[0] == '/'))
    return 0;
  if (strstr(s, ".."))
    return 0;
  for (i = 0; s[i]; i++)
    {
    if (s[i] == '/')
      return 0;
    }
  return 1;
}

static int play_demo_path(char *path, int sz)
{
  const char *root = g_share_dir[0] ? g_share_dir : CLOONIX_SHARE_DIR;
  const char *p;
  if (!play_name_is_sane(g_play_name))
    return 0;
  if (g_play_dir[0])
    {
    if (g_play_dir[0] == '/')
      return 0;
    if (strstr(g_play_dir, ".."))
      return 0;
    for (p = g_play_dir; *p; p++)
      {
      if ((*p == '.') && ((p == g_play_dir) || (p[-1] == '/')))
        return 0;
      }
    snprintf(path, sz, "%s/%s/%s", root, g_play_dir, g_play_name);
    }
  else
    snprintf(path, sz, "%s/%s", root, g_play_name);
  return 1;
}

static int host_runner_connect(void)
{
  struct sockaddr_un addr;
  char path[MAX_PATH_LEN];
  int fd;

  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1, "%s_%s/%s",
           PROXYSHARE_IN, g_net_name, DEMO_EXEC_SOCK);
  DLOG("trying host runner %s", path);
  if (strlen(path) >= sizeof(addr.sun_path))
    {
    KERR("DEMOEXEC GUI path too long");
    return -1;
    }
  if (access(path, W_OK))
    {
    DLOG("no socket there (%d), staying in the crun", errno);
    return -1;
    }
  fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0)
    {
    KERR("DEMOEXEC GUI socket %d", errno);
    return -1;
    }
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);
  if (connect(fd, (struct sockaddr *) &addr, sizeof(addr)))
    {
    KERR("DEMOEXEC GUI connect %d", errno);
    close(fd);
    return -1;
    }
  DLOG("connected fd=%d", fd);
  return fd;
}

static int host_play(int browser_fd, const char *path)
{
  char buf[HOST_RDBUF], line[HOST_RDBUF], msg[1200], hdr[256];
  char kline[HOST_RDBUF];
  char *script;
  uint8_t *key = NULL;
  uint64_t klen = 0;
  struct pollfd pfd[2];
  int fd, len, used = 0, code = -1, done = 0, n, i, start, skipping = 0;
  int op, browser_gone = 0, stopped;
  long size;
  FILE *f;

  f = fopen(path, "r");
  if (!f)
    {
    KERR("ERROR play: cannot open the demo %s errno=%d", path, errno);
    return -1;
    }
  if (fseek(f, 0, SEEK_END))
    {
    fclose(f);
    KERR("ERROR play: cannot seek in %s errno=%d", path, errno);
    return -1;
    }
  size = ftell(f);
  rewind(f);
  if ((size <= 0) || (size > PLAY_MAX_SCRIPT))
    {
    fclose(f);
    KERR("ERROR play: %s is %ld bytes, the limit is %d", path, size, PLAY_MAX_SCRIPT);
    return -1;
    }
  script = (char *) malloc(size + 1);
  if (!script)
    {
    fclose(f);
    KERR("ERROR play: no memory for %ld bytes of %s", size, path);
    return -1;
    }
  len = (int) fread(script, 1, size, f);
  fclose(f);
  if (len <= 0)
    {
    free(script);
    KERR("ERROR play: short read on %s", path);
    return -1;
    }

  fd = host_runner_connect();
  if (fd < 0)
    {
    free(script);
    return -1;
    }

  DLOG("sending %s len=%d", path, len);
  snprintf(hdr, sizeof(hdr), "RUN %s %d\n", g_net_name, len);
  if (util_write_all(fd, hdr, (int) strlen(hdr)) ||
      util_write_all(fd, script, len))
    {
    free(script);
    close(fd);
    return -1;
    }
  free(script);

  snprintf(msg, sizeof(msg), ">>> %s %s (on the host, outside the crun)",
           g_play_name, g_net_name);
  play_send(browser_fd, msg);
  run_add(fd);

  while (!done)
    {
    pfd[0].fd = fd;
    pfd[0].events = POLLIN;
    pfd[1].fd = browser_fd;
    pfd[1].events = browser_gone ? 0 : POLLIN;
    if (poll(pfd, 2, -1) < 0)
      {
      if (errno == EINTR)
        continue;
      break;
      }
    if ((!browser_gone) && (pfd[1].revents & (POLLIN | POLLHUP | POLLERR)))
      {
      op = ws_read_frame(browser_fd, &key, &klen);
      if ((op < 0) || (op == 0x8))
        {
        browser_gone = 1;
        shutdown(fd, SHUT_WR);
        }
      else if ((op == 0x1) && (key) && (klen < sizeof(kline) - 4))
        {
        snprintf(kline, sizeof(kline), "K %s\n", (char *) key);
        (void) util_write_all(fd, kline, (int) strlen(kline));
        }
      else if (op == 0x9)
        {
        uint8_t pong[2] = { 0x8A, 0x00 };
        (void) write(browser_fd, pong, 2);
        }
      if (key)
        {
        free(key);
        key = NULL;
        }
      }
    if (!(pfd[0].revents & (POLLIN | POLLHUP | POLLERR)))
      continue;
    if (used >= (int) sizeof(buf))
      {
      KERR("DEMOEXEC GUI record longer than %d, resynchronising",
           (int) sizeof(buf));
      used = 0;
      skipping = 1;
      }
    n = (int) read(fd, buf + used, sizeof(buf) - used);
    if (n <= 0)
      {
      if ((n < 0) && (errno == EINTR))
        continue;
      break;
      }
    used += n;
    start = 0;
    for (i = 0; i < used; i++)
      {
      if (buf[i] != '\n')
        continue;
      buf[i] = 0;
      if (skipping)
        skipping = 0;                    /* the tail of the broken record */
      else if (!strncmp(buf + start, "O ", 2))
        {
        snprintf(line, sizeof(line), "%s", buf + start + 2);
        play_send_output_line(browser_fd, line);
        }
      else if (!strncmp(buf + start, "E ", 2))
        {
        code = atoi(buf + start + 2);
        done = 1;
        }
      start = i + 1;
      }
    if (start > 0)
      {
      if (start < used)
        memmove(buf, buf + start, used - start);
      used -= start;
      }
    }
  stopped = run_del(fd) || browser_gone;
  close(fd);
  if (stopped)
    {
    play_send(browser_fd, "[KO stopped, everything deleted]");
    play_send(browser_fd, "[DONE ok=0 ko=1]");
    return 0;
    }
  if (code)
    KERR("WARNING DEMOEXEC GUI demo finished with code %d", code);
  else
    DLOG("host run finished code=0");

  if (code == 0)
    play_send(browser_fd, "[DONE ok=1 ko=0]");
  else
    {
    snprintf(msg, sizeof(msg), "[KO the demo exited with %d]", code);
    play_send(browser_fd, msg);
    if (code == 127)
      {
      play_send(browser_fd, "[KO it ran on the host, outside the crun, and "
                            "the interpreter of its first line is not "
                            "installed there either]");
      }
    else if (code < 0)
      play_send(browser_fd, "[KO the host runner stopped without a result]");
    play_send(browser_fd, "[DONE ok=0 ko=1]");
    }
  return 0;
}

static void play_programme(int browser_fd, char *script)
{
  char demo[MAX_PATH_LEN];
  (void) script;
  if (!play_demo_path(demo, sizeof(demo)))
    {
    play_send(browser_fd, "[KO a demo is played by its name in the share "
                          "directory, and none was given]");
    play_send(browser_fd, "[DONE ok=0 ko=1]");
    return;
    }
  if (host_play(browser_fd, demo))
    {
    play_send(browser_fd, "[KO the host runner of proxymous is not "
                          "reachable, the demo cannot be played from here]");
    play_send(browser_fd, "[DONE ok=0 ko=1]");
    }
}

static void play_script(int browser_fd, char *script)
{
  g_play_dir[0] = 0;
  g_play_name[0] = 0;
  script = play_take_header(script, "#!cloonix-play-dir ",
                            g_play_dir, sizeof(g_play_dir));
  script = play_take_header(script, "#!cloonix-play-name ",
                            g_play_name, sizeof(g_play_name));

  if (!play_first_is_shebang(script))
    {
    play_send(browser_fd, "[KO this file is not a demo: a demo is a"
                          " programme and starts with a shebang]");
    play_send(browser_fd, "[KO a python one starts with"
                          " #!/usr/bin/env python3]");
    play_send(browser_fd, "[DONE ok=0 ko=1]");
    return;
    }
  play_programme(browser_fd, script);
}

void handle_play_ws(int browser_fd, const char *full_hdr)
{
  uint8_t *payload = NULL;
  uint64_t plen = 0;
  int op;

  if (ws_handshake(browser_fd, full_hdr))
    return;

  for (;;)
    {
    op = ws_read_frame(browser_fd, &payload, &plen);
    if (op < 0)
      return;
    if (op == 0x8)                       /* client closed before sending */
      {
      if (payload) free(payload);
      return;
      }
    if (op == 0x9)                       /* ping -> pong, keep waiting */
      {
      uint8_t pong[2] = { 0x8A, 0x00 };
      (void) write(browser_fd, pong, 2);
      if (payload) free(payload);
      continue;
      }
    if (op == 0x1)                       /* the script (text frame) */
      break;
    if (payload) { free(payload); payload = NULL; }   /* ignore other ops */
    }

  if (plen == 0 || !payload)
    {
    if (payload) free(payload);
    play_send(browser_fd, "[DONE ok=0 ko=0]");
    return;
    }
  if (plen > PLAY_MAX_SCRIPT)
    {
    free(payload);
    play_send(browser_fd, "[KO script too large]");
    return;
    }

  play_script(browser_fd, (char *) payload);
  free(payload);
}
