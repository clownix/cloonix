/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
/* Launch and supervise the vendored passt on the host, outside the crun:  */
/*   cloonix-passt --socket PATH --foreground --no-map-gw --mtu 1500        */
/*                 -t <ports> -u <ports> --pid PIDFILE                      */
/* -t/-u are the published ports of the nat, "none" for egress only.        */
/* passt assigns the guest ip by its own dhcp/ndp.                          */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/prctl.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "pthexec.h"
#include "passt_spawn.h"

/* PASST_TRACE: the per-nat passt chatter.                                  */
#define PASST_TRACE 0
#if PASST_TRACE
#define PASST_DBG(format, a...) KERR(format, ## a)
#else
#define PASST_DBG(format, a...) do { } while (0)
#endif

#define PASST_MTU  "1500"
#define PASST_ADDR "10.117.0.10"
#define PASST_MASK "255.255.255.0"
/* Public resolver of last resort, used ONLY if no usable (non-loopback) */
#define PASST_DNS_FALLBACK "9.9.9.9"

/* One passt per nat, so a list keyed by socket path and not a singleton. */
typedef struct t_passt
{
  int  pid;
  char sock_path[MAX_PATH_LEN];
  char pid_path[MAX_PATH_LEN];
  struct t_passt *prev;
  struct t_passt *next;
} t_passt;

static t_passt *g_head_passt;
static char g_log_path[MAX_PATH_LEN];
static char g_dns_fwd[MAX_NAME_LEN];

static t_passt *passt_find(char *socket_path)
{
  t_passt *cur = g_head_passt;
  while (cur)
    {
    if (!strcmp(cur->sock_path, socket_path))
      return cur;
    cur = cur->next;
    }
  return NULL;
}

static void passt_free(t_passt *cur)
{
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur->next)
    cur->next->prev = cur->prev;
  if (g_head_passt == cur)
    g_head_passt = cur->next;
  free(cur);
}

static void passt_kill_one(t_passt *cur)
{
  int i, rc;
  if (cur->pid > 0)
    {
    kill(cur->pid, SIGTERM);
    for (i = 0; i < 20; i++)
      {
      rc = waitpid(cur->pid, NULL, WNOHANG);
      if (rc == cur->pid || rc < 0)
        break;
      usleep(50000);
      }
    if (i == 20)
      {
      KERR("WARNING passt pid=%d did not go on SIGTERM, killing it",
           cur->pid);
      kill(cur->pid, SIGKILL);
      waitpid(cur->pid, NULL, 0);
      }
    }
  if (cur->sock_path[0])
    unlink(cur->sock_path);
  if (cur->pid_path[0])
    unlink(cur->pid_path);
}

static int said_no_dns;

/* Result goes to out[] (caller-sized MAX_NAME_LEN). Returns the passt option
   it goes with: --dns-forward maps it to the resolver of the host; with no
   such resolver passt forwards to nothing and advertises nothing, so the
   fallback is given to the guests by dhcp instead, --dns. */
static const char *find_host_dns(char *out)
{
  const char *files[2];
  FILE *f;
  char line[512], ip[MAX_NAME_LEN];
  int i, found = 0;

  files[0] = "/run/systemd/resolve/resolv.conf";
  files[1] = "/etc/resolv.conf";
  out[0] = 0;
  for (i = 0; (i < 2) && (!found); i++)
    {
    f = fopen(files[i], "r");
    if (!f)
      continue;
    while (fgets(line, sizeof(line), f))
      {
      if (strncmp(line, "nameserver", 10))
        continue;
      if (sscanf(line, "nameserver %63s", ip) != 1)
        continue;
      if (!strncmp(ip, "127.", 4))
        continue;
      if (!strcmp(ip, "::1"))
        continue;
      strncpy(out, ip, MAX_NAME_LEN-1);
      found = 1;
      break;
      }
    fclose(f);
    }
  if (!found)
    {
    /* the usual state of a desktop (127.0.0.53): said once per process */
    if (!said_no_dns)
      KERR("WARNING no usable host nameserver found, the guests are given %s",
           PASST_DNS_FALLBACK);
    said_no_dns = 1;
    strncpy(out, PASST_DNS_FALLBACK, MAX_NAME_LEN-1);
    return "--dns";
    }
  return "--dns-forward";
}

static void ports_to_passt(char *ports, char *tcp_spec, char *udp_spec)
{
  char tmp[MAX_PATH_LEN];
  char *tok, *sav, *spec;
  int nb_tcp = 0, nb_udp = 0, *nb;
  memset(tcp_spec, 0, MAX_PATH_LEN);
  memset(udp_spec, 0, MAX_PATH_LEN);
  if ((ports) && (strlen(ports)) && (strcmp(ports, "none")))
    {
    memset(tmp, 0, MAX_PATH_LEN);
    strncpy(tmp, ports, MAX_PATH_LEN-1);
    tok = strtok_r(tmp, ",", &sav);
    while (tok)
      {
      spec = NULL;
      if (!strncmp(tok, "tcp:", 4))
        {
        spec = tcp_spec;
        nb = &nb_tcp;
        }
      else if (!strncmp(tok, "udp:", 4))
        {
        spec = udp_spec;
        nb = &nb_udp;
        }
      else
        KERR("ERROR published port %s is neither tcp nor udp", tok);
      if (spec)
        {
        if (*nb)
          strncat(spec, ",", MAX_PATH_LEN - strlen(spec) - 2);
        strncat(spec, tok + 4, MAX_PATH_LEN - strlen(spec) - 2);
        *nb += 1;
        }
      tok = strtok_r(NULL, ",", &sav);
      }
    }
  if (!nb_tcp)
    strncpy(tcp_spec, "none", MAX_PATH_LEN-1);
  if (!nb_udp)
    strncpy(udp_spec, "none", MAX_PATH_LEN-1);
}

int passt_spawn_start(char *socket_path, char *ports, char *why)
{
  int pid, fd;
  const char *dns_opt;
  char tcp_spec[MAX_PATH_LEN];
  char udp_spec[MAX_PATH_LEN];
  char bin[MAX_PATH_LEN];
  char sock_path[MAX_PATH_LEN];
  char pid_path[MAX_PATH_LEN];

  memset(sock_path, 0, MAX_PATH_LEN);
  memset(pid_path,  0, MAX_PATH_LEN);
  memset(g_log_path,  0, MAX_PATH_LEN);
  /* Beside proxymous: cloonfs/bin in the bundle, bin/ of the toy. */
  snprintf(bin, MAX_PATH_LEN-1, "%s/cloonix-passt", pthexec_self_bin_dir());
  if (access(bin, X_OK))
    {
    KERR("ERROR passt %s: %s", bin, strerror(errno));
    return -1;
    }
  strncpy(sock_path, socket_path, MAX_PATH_LEN-1);
  snprintf(pid_path, MAX_PATH_LEN-1, "%s.pid", socket_path);
  snprintf(g_log_path, MAX_PATH_LEN-1, "%s.log", socket_path);

  unlink(sock_path);

  dns_opt = find_host_dns(g_dns_fwd);
  PASST_DBG("INFO passt %s %s", dns_opt, g_dns_fwd);
  /* creation and a publish asked by the user are nominal */
  if (strcmp(why, "nat created") && strcmp(why, "republish"))
    KERR("WARNING passt started on %s, ports %s: %s", socket_path,
         (ports && strlen(ports)) ? ports : "none", why);

  ports_to_passt(ports, tcp_spec, udp_spec);

  pid = fork();
  if (pid < 0)
    {
    KERR("ERROR passt fork %d", errno);
    return -1;
    }
  if (pid == 0)
    {
    prctl(PR_SET_PDEATHSIG, SIGKILL);
    /* proxymous may have died between fork() and prctl(). */
    if (getppid() == 1)
      _exit(0);
    fd = open(g_log_path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd >= 0)
      {
      dup2(fd, 1);
      dup2(fd, 2);
      close(fd);
      }
    {
    int k;
    for (k = 3; k < 1024; k++)
      close(k);
    }
    /* CLOONIX_PASST_DEBUG set in the environment of proxymous: passt logs
       every connection event (a reset on a fresh publish is being chased),
       else quiet: twenty lines per start, six hundred a run with thirty nats. */
    execl(bin, "cloonix-passt",
          "--socket",     sock_path,
          "--foreground",
          getenv("CLOONIX_PASST_DEBUG") ? "--debug" : "--quiet",
          "--map-host-loopback", PASST_GW,
          dns_opt,               g_dns_fwd,
          "--mtu",        PASST_MTU,
          "-a",           PASST_ADDR,
          "-n",           PASST_MASK,
          "-g",           PASST_GW,
          "-t", tcp_spec,
          "-u", udp_spec,
          "--pid",        pid_path,
          (char *)NULL);
    KERR("ERROR passt execl %s %d", bin, errno);
    _exit(1);
    }
  {
  t_passt *elem = (t_passt *) malloc(sizeof(t_passt));
  if (elem == NULL)
    {
    KERR("ERROR no memory to track passt pid=%d, killing it rather than "
         "losing it", pid);
    kill(pid, SIGKILL);
    waitpid(pid, NULL, 0);
    return -1;
    }
  memset(elem, 0, sizeof(t_passt));
  elem->pid = pid;
  snprintf(elem->sock_path, MAX_PATH_LEN-1, "%s", socket_path);
  snprintf(elem->pid_path, MAX_PATH_LEN-1, "%s.pid", socket_path);
  if (g_head_passt)
    g_head_passt->prev = elem;
  elem->next = g_head_passt;
  g_head_passt = elem;
  }
  PASST_DBG("INFO passt spawned pid=%d sock=%s", pid, socket_path);
  return pid;
}

/* A passt that cannot bind a published port exits within ms: -1 if it
   exited (and is forgotten) half a second after the start, else 0. */
int passt_spawn_probe(char *socket_path)
{
  int i, rc, status;
  t_passt *cur = passt_find(socket_path);
  if (cur == NULL)
    return -1;
  for (i = 0; i < 10; i++)
    {
    usleep(50000);
    rc = waitpid(cur->pid, &status, WNOHANG);
    if ((rc == cur->pid) || ((rc < 0) && (errno == ECHILD)))
      {
      KERR("ERROR passt pid=%d for %s exited status=%d right after start",
           cur->pid, socket_path, status);
      cur->pid = -1;
      passt_kill_one(cur);
      passt_free(cur);
      return -1;
      }
    }
  return 0;
}

int passt_spawn_beat(void)
{
  /* Every one of them, not just the last. */
  int status, rc, alive = 0, died = 0;
  t_passt *next, *cur = g_head_passt;
  while (cur)
    {
    next = cur->next;
    if (cur->pid > 0)
      {
      rc = waitpid(cur->pid, &status, WNOHANG);
      if (rc == 0)
        alive += 1;
      else if ((rc == cur->pid) || ((rc < 0) && (errno == ECHILD)))
        {
        KERR("ERROR passt pid=%d for %s exited status=%d",
             cur->pid, cur->sock_path, status);
        cur->pid = -1;
        died += 1;
        }
      else
        alive += 1;
      }
    cur = next;
    }
  if (died)
    return 0;
  if (alive)
    return 1;
  return -1;                          /* none currently supervised */
}

void passt_spawn_stop_one(char *socket_path)
{
  t_passt *cur = passt_find(socket_path);
  if (cur == NULL)
    {
    KERR("ERROR no passt is known for %s, nothing to stop", socket_path);
    return;
    }
  PASST_DBG("INFO passt stopped pid=%d sock=%s", cur->pid, cur->sock_path);
  passt_kill_one(cur);
  passt_free(cur);
}

void passt_spawn_stop(void)
{
  t_passt *next, *cur = g_head_passt;
  int nb = 0;
  while (cur)
    {
    next = cur->next;
    passt_kill_one(cur);
    passt_free(cur);
    nb += 1;
    cur = next;
    }
  if (nb)
    PASST_DBG("INFO %d passt stopped", nb);
}
