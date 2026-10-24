/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
/* A small tcpdump-syntax capture filter: a recursive descent parser to a */
/* tree, walked per packet. No libpcap. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdint.h>
#include <arpa/inet.h>

#include "pcap_filter.h"

#define MAX_FILTER_TEXT 512
#define MAX_TOKENS      128
#define MAX_TOKEN_LEN   64
#define MAX_NODES       128
#define MAX_MPLS_LABELS 8

enum {
  OP_OR = 1, OP_AND, OP_NOT,
  OP_PROTO_IP, OP_PROTO_IP6, OP_PROTO_ARP,
  OP_PROTO_TCP, OP_PROTO_UDP, OP_PROTO_ICMP, OP_PROTO_ICMP6,
  OP_PROTO_IGMP, OP_PROTO_MPLS, OP_IP_PROTO, OP_IP6_PROTO,
  OP_HOST, OP_NET, OP_HOST6, OP_NET6, OP_PORT, OP_PORTRANGE, OP_LESS,
  OP_GREATER
};

enum { DIR_EITHER = 0, DIR_SRC, DIR_DST };

typedef struct t_node
{
  int op;
  int dir;
  uint32_t addr;          /* host and net, in host order */
  uint32_t mask;
  uint8_t a6[16];         /* host and net in ipv6, plen6 bits kept */
  int plen6;
  int lo, hi;             /* port, portrange, less, greater */
  struct t_node *left;
  struct t_node *right;
} t_node;

struct t_pcap_filter
{
  char text[MAX_FILTER_TEXT];
  t_node *root;
  t_node pool[MAX_NODES];
  int used;
};

typedef struct t_parse
{
  char tok[MAX_TOKENS][MAX_TOKEN_LEN];
  int ntok;
  int pos;
  char *err;
  int errlen;
  int mpls_depth;         /* the next mpls word reads the label under */
  t_pcap_filter *flt;
} t_parse;

static void fail(t_parse *p, const char *fmt, const char *a1)
{
  if (p->err && p->errlen > 0 && p->err[0] == 0)
    {
    if (a1)
      snprintf(p->err, p->errlen, fmt, a1);
    else
      snprintf(p->err, p->errlen, "%s", fmt);
    }
}

static int tokenize(t_parse *p, const char *text)
{
  const char *s = text;
  int n = 0, len;
  while (*s)
    {
    while (*s && isspace((unsigned char) *s))
      s++;
    if (!*s)
      break;
    if (n >= MAX_TOKENS)
      {
      fail(p, "the filter has too many words in it", NULL);
      return -1;
      }
    if (*s == '(' || *s == ')' || *s == '!')
      {
      p->tok[n][0] = *s;
      p->tok[n][1] = 0;
      n++;
      s++;
      continue;
      }
    if ((s[0] == '&' && s[1] == '&') || (s[0] == '|' && s[1] == '|'))
      {
      p->tok[n][0] = s[0];
      p->tok[n][1] = s[1];
      p->tok[n][2] = 0;
      n++;
      s += 2;
      continue;
      }
    len = 0;
    while (*s && !isspace((unsigned char) *s) && *s != '(' && *s != ')' &&
           *s != '!' && *s != '&' && *s != '|')
      {
      if (len < MAX_TOKEN_LEN - 1)
        p->tok[n][len++] = (char) tolower((unsigned char) *s);
      s++;
      }
    p->tok[n][len] = 0;
    n++;
    }
  p->ntok = n;
  p->pos = 0;
  return n;
}

static const char *peek(t_parse *p)
{
  return (p->pos < p->ntok) ? p->tok[p->pos] : NULL;
}

static int eat(t_parse *p, const char *what)
{
  const char *t = peek(p);
  if (t && !strcmp(t, what))
    {
    p->pos++;
    return 1;
    }
  return 0;
}

static t_node *node_new(t_parse *p, int op)
{
  t_node *n;
  if (p->flt->used >= MAX_NODES)
    {
    fail(p, "the filter is too complicated", NULL);
    return NULL;
    }
  n = &(p->flt->pool[p->flt->used]);
  p->flt->used += 1;
  memset(n, 0, sizeof(t_node));
  n->op = op;
  return n;
}

/* Dotted quad to a host-order integer. */
static int parse_ipv4(const char *s, uint32_t *out)
{
  unsigned int a[4];
  char extra;
  int i;
  if (sscanf(s, "%u.%u.%u.%u%c", &a[0], &a[1], &a[2], &a[3], &extra) != 4)
    return -1;
  for (i = 0; i < 4; i++)
    if (a[i] > 255)
      return -1;
  *out = (a[0] << 24) | (a[1] << 16) | (a[2] << 8) | a[3];
  return 0;
}

static int parse_uint(const char *s, long max, int *out)
{
  char *end;
  long v;
  if (!s || !*s)
    return -1;
  v = strtol(s, &end, 10);
  if (*end || v < 0 || v > max)
    return -1;
  *out = (int) v;
  return 0;
}

static int parse_num(const char *s, int *out)
{
  return parse_uint(s, 65535, out);
}

static t_node *parse_expr(t_parse *p);

/* host or net of an ipv6 address, buf the token, slash its '/' if any */
static t_node *parse_addr6(t_parse *p, char *buf, char *slash, int is_net,
                           int dir)
{
  t_node *n;
  int v = 128, i;
  uint8_t a[16];
  if (slash)
    {
    *slash = 0;
    if (parse_num(slash + 1, &v) || v > 128)
      {
      fail(p, "'%s' is not a prefix length between 0 and 128", slash + 1);
      return NULL;
      }
    }
  if (inet_pton(AF_INET6, buf, a) != 1)
    {
    fail(p, "'%s' is not an ipv6 address", buf);
    return NULL;
    }
  n = node_new(p, is_net ? OP_NET6 : OP_HOST6);
  if (!n)
    return NULL;
  n->dir = dir;
  n->plen6 = v;
  for (i = 0; i < 16; i++)
    {
    int bits = v - 8 * i;
    n->a6[i] = (bits >= 8) ? a[i] : (bits <= 0) ? 0 :
               (uint8_t) (a[i] & (0xff << (8 - bits)));
    }
  return n;
}

/* the first plen bits of a are those of the prefix p */
static int in_prefix6(const uint8_t *a, const uint8_t *p, int plen)
{
  int full = plen / 8, rest = plen % 8;
  if (memcmp(a, p, full))
    return 0;
  return !rest || !((a[full] ^ p[full]) & (0xff << (8 - rest)) & 0xff);
}

static t_node *parse_primitive(t_parse *p)
{
  const char *t = peek(p);
  int dir = DIR_EITHER;
  t_node *n;
  int v, v2;
  uint32_t addr;
  char *slash;
  char buf[MAX_TOKEN_LEN];

  if (!t)
    {
    fail(p, "the filter ends where something was expected", NULL);
    return NULL;
    }
  if (!strcmp(t, "src"))
    {
    dir = DIR_SRC;
    p->pos++;
    }
  else if (!strcmp(t, "dst"))
    {
    dir = DIR_DST;
    p->pos++;
    }
  t = peek(p);
  if (!t)
    {
    fail(p, "src or dst has nothing after it", NULL);
    return NULL;
    }

  if (!strcmp(t, "host") || !strcmp(t, "net"))
    {
    int is_net = !strcmp(t, "net");
    p->pos++;
    t = peek(p);
    if (!t)
      {
      fail(p, "%s needs an address after it", is_net ? "net" : "host");
      return NULL;
      }
    p->pos++;
    snprintf(buf, sizeof(buf), "%s", t);
    slash = strchr(buf, '/');
    if (strchr(buf, ':'))
      return parse_addr6(p, buf, slash, is_net, dir);
    if (slash)
      {
      *slash = 0;
      if (parse_num(slash + 1, &v) || v > 32)
        {
        fail(p, "'%s' is not a prefix length between 0 and 32", slash + 1);
        return NULL;
        }
      }
    else
      v = 32;
    if (parse_ipv4(buf, &addr))
      {
      fail(p, "'%s' is not an ipv4 address", buf);
      return NULL;
      }
    n = node_new(p, is_net ? OP_NET : OP_HOST);
    if (!n)
      return NULL;
    n->dir = dir;
    n->mask = (v == 0) ? 0 : (0xffffffffu << (32 - v));
    n->addr = addr & n->mask;
    return n;
    }

  if (!strcmp(t, "port"))
    {
    p->pos++;
    t = peek(p);
    if (!t || parse_num(t, &v))
      {
      fail(p, "'%s' is not a port number", t ? t : "(nothing)");
      return NULL;
      }
    p->pos++;
    n = node_new(p, OP_PORT);
    if (!n)
      return NULL;
    n->dir = dir;
    n->lo = n->hi = v;
    return n;
    }

  if (!strcmp(t, "portrange"))
    {
    p->pos++;
    t = peek(p);
    if (!t)
      {
      fail(p, "portrange needs low-high after it", NULL);
      return NULL;
      }
    snprintf(buf, sizeof(buf), "%s", t);
    p->pos++;
    slash = strchr(buf, '-');
    if (!slash)
      {
      fail(p, "'%s' is not a range like 5060-5070", buf);
      return NULL;
      }
    *slash = 0;
    if (parse_num(buf, &v) || parse_num(slash + 1, &v2) || v > v2)
      {
      fail(p, "'%s' is not a range like 5060-5070", t);
      return NULL;
      }
    n = node_new(p, OP_PORTRANGE);
    if (!n)
      return NULL;
    n->dir = dir;
    n->lo = v;
    n->hi = v2;
    return n;
    }

  if (!strcmp(t, "less") || !strcmp(t, "greater"))
    {
    int is_less = !strcmp(t, "less");
    p->pos++;
    t = peek(p);
    if (!t || parse_num(t, &v))
      {
      fail(p, "'%s' is not a length", t ? t : "(nothing)");
      return NULL;
      }
    p->pos++;
    n = node_new(p, is_less ? OP_LESS : OP_GREATER);
    if (!n)
      return NULL;
    n->lo = v;
    return n;
    }

  if (dir != DIR_EITHER)
    {
    fail(p, "src or dst only goes before host, net or port, not '%s'", t);
    return NULL;
    }

  {
  int proto_op = 0;
  if (!strcmp(t, "ip"))          proto_op = OP_PROTO_IP;
  else if (!strcmp(t, "ip6"))    proto_op = OP_PROTO_IP6;
  else if (!strcmp(t, "arp"))    proto_op = OP_PROTO_ARP;
  else if (!strcmp(t, "tcp"))    proto_op = OP_PROTO_TCP;
  else if (!strcmp(t, "udp"))    proto_op = OP_PROTO_UDP;
  else if (!strcmp(t, "icmp"))   proto_op = OP_PROTO_ICMP;
  else if (!strcmp(t, "icmp6"))  proto_op = OP_PROTO_ICMP6;
  else if (!strcmp(t, "igmp"))   proto_op = OP_PROTO_IGMP;
  else if (!strcmp(t, "mpls"))   proto_op = OP_PROTO_MPLS;
  if (proto_op)
    {
    t_node *proto, *rest, *both;
    p->pos++;
    /* mpls [label]: as in tcpdump, each mpls word of a filter reads one
       label deeper, the first the outer one */
    if (proto_op == OP_PROTO_MPLS)
      {
      proto = node_new(p, OP_PROTO_MPLS);
      if (!proto)
        return NULL;
      proto->hi = p->mpls_depth++;
      proto->lo = -1;
      t = peek(p);
      if (t && isdigit((unsigned char) t[0]))
        {
        if (parse_uint(t, 1048575, &v))
          {
          fail(p, "'%s' is not an mpls label, 0 to 1048575", t);
          return NULL;
          }
        proto->lo = v;
        p->pos++;
        }
      return proto;
      }
    /* ip proto <n>, ip6 proto <n>: the protocol number, next header for ip6 */
    if (((proto_op == OP_PROTO_IP) || (proto_op == OP_PROTO_IP6)) &&
        (eat(p, "proto")))
      {
      t = peek(p);
      if (!t || parse_num(t, &v) || (v > 255))
        {
        fail(p, "'%s' is not a protocol number, 0 to 255",
             t ? t : "(nothing)");
        return NULL;
        }
      p->pos++;
      proto = node_new(p, (proto_op == OP_PROTO_IP) ? OP_IP_PROTO :
                                                      OP_IP6_PROTO);
      if (proto)
        proto->lo = v;
      return proto;
      }
    proto = node_new(p, proto_op);
    if (!proto)
      return NULL;
    /* A protocol word qualifies the primitive after it and the two are one
       thing, which matters for "not" */
    t = peek(p);
    if (t && (!strcmp(t, "src") || !strcmp(t, "dst") ||
              !strcmp(t, "host") || !strcmp(t, "net") ||
              !strcmp(t, "port") || !strcmp(t, "portrange") ||
              !strcmp(t, "less") || !strcmp(t, "greater")))
      {
      rest = parse_primitive(p);
      if (!rest)
        return NULL;
      both = node_new(p, OP_AND);
      if (!both)
        return NULL;
      both->left = proto;
      both->right = rest;
      return both;
      }
    return proto;
    }
  }

  fail(p, "'%s' is not something this filter understands", t);
  return NULL;
}

static t_node *parse_factor(t_parse *p)
{
  t_node *n, *sub;
  if (eat(p, "not") || eat(p, "!"))
    {
    sub = parse_factor(p);
    if (!sub)
      return NULL;
    n = node_new(p, OP_NOT);
    if (!n)
      return NULL;
    n->left = sub;
    return n;
    }
  if (eat(p, "("))
    {
    sub = parse_expr(p);
    if (!sub)
      return NULL;
    if (!eat(p, ")"))
      {
      fail(p, "a bracket was opened and not closed", NULL);
      return NULL;
      }
    return sub;
    }
  return parse_primitive(p);
}

static t_node *parse_term(t_parse *p)
{
  t_node *n, *right, *and_node;
  const char *t;
  n = parse_factor(p);
  if (!n)
    return NULL;
  for (;;)
    {
    if (!eat(p, "and") && !eat(p, "&&"))
      {
      t = peek(p);
      if (!t)
        break;
      if (!strcmp(t, "or") || !strcmp(t, "||") || !strcmp(t, ")"))
        break;
      }
    right = parse_factor(p);
    if (!right)
      return NULL;
    and_node = node_new(p, OP_AND);
    if (!and_node)
      return NULL;
    and_node->left = n;
    and_node->right = right;
    n = and_node;
    }
  return n;
}

static t_node *parse_expr(t_parse *p)
{
  t_node *n, *right, *or_node;
  n = parse_term(p);
  if (!n)
    return NULL;
  for (;;)
    {
    if (!eat(p, "or") && !eat(p, "||"))
      break;
    right = parse_term(p);
    if (!right)
      return NULL;
    or_node = node_new(p, OP_OR);
    if (!or_node)
      return NULL;
    or_node->left = n;
    or_node->right = right;
    n = or_node;
    }
  return n;
}

t_pcap_filter *pcap_filter_compile(const char *text, char *err, int errlen)
{
  t_parse p;
  t_pcap_filter *flt;
  const char *s;

  if (err && errlen > 0)
    err[0] = 0;
  if (!text)
    return NULL;
  for (s = text; *s; s++)
    if (!isspace((unsigned char) *s))
      break;
  if (!*s)
    return NULL;                        /* no filter, and not an error */
  if (strlen(text) >= MAX_FILTER_TEXT)
    {
    if (err && errlen > 0)
      snprintf(err, errlen, "the filter is longer than %d characters",
               MAX_FILTER_TEXT - 1);
    return NULL;
    }
  flt = (t_pcap_filter *) malloc(sizeof(t_pcap_filter));
  if (!flt)
    {
    if (err && errlen > 0)
      snprintf(err, errlen, "no memory for the filter");
    return NULL;
    }
  memset(flt, 0, sizeof(t_pcap_filter));
  snprintf(flt->text, sizeof(flt->text), "%s", text);
  memset(&p, 0, sizeof(p));
  p.err = err;
  p.errlen = errlen;
  p.flt = flt;
  if (tokenize(&p, text) <= 0)
    {
    if (err && errlen > 0 && err[0] == 0)
      snprintf(err, errlen, "the filter has nothing in it");
    free(flt);
    return NULL;
    }
  flt->root = parse_expr(&p);
  if (!flt->root)
    {
    if (err && errlen > 0 && err[0] == 0)
      snprintf(err, errlen, "the filter could not be understood");
    free(flt);
    return NULL;
    }
  if (p.pos != p.ntok)
    {
    if (err && errlen > 0)
      snprintf(err, errlen, "'%s' does not belong there, an 'and' or 'or' "
                            "may be missing", p.tok[p.pos]);
    free(flt);
    return NULL;
    }
  return flt;
}

typedef struct t_pkt
{
  int len;
  int ethertype;
  int is_ip4, is_ip6, is_arp;
  int proto;                    /* ip protocol number, 0 if not ip */
  uint32_t src, dst;            /* ipv4 only */
  int sport, dport;             /* tcp and udp only, -1 otherwise */
  uint8_t src6[16], dst6[16];   /* ipv6 only */
  int nlab;                     /* mpls labels read, outer first */
  int lab[MAX_MPLS_LABELS];
} t_pkt;

static void dissect(const uint8_t *d, int len, t_pkt *k)
{
  int ihl, off;
  memset(k, 0, sizeof(t_pkt));
  k->len = len;
  k->sport = k->dport = -1;
  if (len < 14)
    return;
  k->ethertype = (d[12] << 8) | d[13];
  if ((k->ethertype == 0x8847) || (k->ethertype == 0x8848))
    {
    for (off = 14; (k->nlab < MAX_MPLS_LABELS) && (len >= off + 4); off += 4)
      {
      k->lab[k->nlab++] = (d[off] << 12) | (d[off + 1] << 4) | (d[off + 2] >> 4);
      if (d[off + 2] & 0x01)
        break;
      }
    return;
    }
  if (k->ethertype == 0x0806)
    {
    k->is_arp = 1;
    if (len >= 42)
      {
      k->src = (d[28] << 24) | (d[29] << 16) | (d[30] << 8) | d[31];
      k->dst = (d[38] << 24) | (d[39] << 16) | (d[40] << 8) | d[41];
      }
    return;
    }
  if (k->ethertype == 0x86dd)
    {
    k->is_ip6 = 1;
    if (len >= 54)
      {
      k->proto = d[20];         /* next header, with no extension chasing */
      memcpy(k->src6, d + 22, 16);
      memcpy(k->dst6, d + 38, 16);
      }
    return;
    }
  if (k->ethertype != 0x0800)
    return;
  if (len < 34)
    return;
  k->is_ip4 = 1;
  ihl = (d[14] & 0x0f) * 4;
  if (ihl < 20 || len < 14 + ihl)
    return;
  k->proto = d[23];
  k->src = (d[26] << 24) | (d[27] << 16) | (d[28] << 8) | d[29];
  k->dst = (d[30] << 24) | (d[31] << 16) | (d[32] << 8) | d[33];
  /* A later fragment carries no ports. */
  if (((d[20] & 0x1f) | d[21]) != 0)
    return;
  off = 14 + ihl;
  if ((k->proto == 6 || k->proto == 17) && len >= off + 4)
    {
    k->sport = (d[off] << 8) | d[off + 1];
    k->dport = (d[off + 2] << 8) | d[off + 3];
    }
}

static int match_node(t_node *n, t_pkt *k)
{
  if (!n)
    return 1;
  switch (n->op)
    {
    case OP_OR:
      return match_node(n->left, k) || match_node(n->right, k);
    case OP_AND:
      return match_node(n->left, k) && match_node(n->right, k);
    case OP_NOT:
      return !match_node(n->left, k);
    case OP_PROTO_IP:
      return k->is_ip4;
    case OP_PROTO_IP6:
      return k->is_ip6;
    case OP_PROTO_ARP:
      return k->is_arp;
    case OP_PROTO_TCP:
      return (k->is_ip4 || k->is_ip6) && k->proto == 6;
    case OP_PROTO_UDP:
      return (k->is_ip4 || k->is_ip6) && k->proto == 17;
    case OP_PROTO_ICMP:
      return k->is_ip4 && k->proto == 1;
    case OP_PROTO_ICMP6:
      return k->is_ip6 && k->proto == 58;
    case OP_PROTO_IGMP:
      return k->is_ip4 && k->proto == 2;
    case OP_PROTO_MPLS:
      return (k->nlab > n->hi) && ((n->lo < 0) || (k->lab[n->hi] == n->lo));
    case OP_IP_PROTO:
      return k->is_ip4 && k->proto == n->lo;
    case OP_IP6_PROTO:
      return k->is_ip6 && k->proto == n->lo;
    case OP_HOST:
    case OP_NET:
      if (!k->is_ip4 && !k->is_arp)
        return 0;
      if (n->dir == DIR_SRC)
        return (k->src & n->mask) == n->addr;
      if (n->dir == DIR_DST)
        return (k->dst & n->mask) == n->addr;
      return ((k->src & n->mask) == n->addr) ||
             ((k->dst & n->mask) == n->addr);
    case OP_HOST6:
    case OP_NET6:
      if (!k->is_ip6)
        return 0;
      if (n->dir == DIR_SRC)
        return in_prefix6(k->src6, n->a6, n->plen6);
      if (n->dir == DIR_DST)
        return in_prefix6(k->dst6, n->a6, n->plen6);
      return in_prefix6(k->src6, n->a6, n->plen6) ||
             in_prefix6(k->dst6, n->a6, n->plen6);
    case OP_PORT:
    case OP_PORTRANGE:
      if (k->sport < 0)
        return 0;
      if (n->dir == DIR_SRC)
        return (k->sport >= n->lo) && (k->sport <= n->hi);
      if (n->dir == DIR_DST)
        return (k->dport >= n->lo) && (k->dport <= n->hi);
      return ((k->sport >= n->lo) && (k->sport <= n->hi)) ||
             ((k->dport >= n->lo) && (k->dport <= n->hi));
    case OP_LESS:
      return k->len <= n->lo;
    case OP_GREATER:
      return k->len >= n->lo;
    }
  return 0;
}

int pcap_filter_match(t_pcap_filter *flt, const uint8_t *pkt, int len)
{
  t_pkt k;
  if (!flt || !flt->root)
    return 1;
  dissect(pkt, len, &k);
  return match_node(flt->root, &k);
}

void pcap_filter_free(t_pcap_filter *flt)
{
  if (flt)
    free(flt);
}

const char *pcap_filter_text(t_pcap_filter *flt)
{
  return flt ? flt->text : "";
}
