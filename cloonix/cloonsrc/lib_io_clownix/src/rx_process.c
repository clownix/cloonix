/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <asm/types.h>
#include <errno.h>
#include "io_clownix.h"
#include "channel.h"
#include "msg_layer.h"
#include "chunk.h"

static void alloc_boundary(t_data_channel *dchan, char *boundary,
                           t_data_chunk *chunk_bound, int i_bound)

{
  dchan->boundary = (char *)clownix_malloc(strlen(boundary) + 1, IOCMLC);
  strcpy(dchan->boundary, boundary);
  dchan->rx_bound_start = chunk_bound;
  dchan->i_bound_start  = i_bound;
  dchan->decoding_state = rx_type_open_bound_found;
}

static void free_boundary(t_data_channel *dchan)
{
  clownix_free (dchan->boundary, __FUNCTION__);
  dchan->boundary = NULL;
  dchan->rx_bound_start = NULL;
  dchan->i_bound_start = 0;
  dchan->decoding_state = rx_type_ascii_start;
}

/* 8-byte magic prefix for JSON messages: 0xCAFEDECA 0xCAFEDECA */
#define JSON_MAGIC_LEN 8
static const unsigned char json_magic[JSON_MAGIC_LEN] = {
  0xCA, 0xFE, 0xDE, 0xCA, 0xCA, 0xFE, 0xDE, 0xCA };

/* One flat chunk, so a magic spanning two chunks is found. */
static void flatten_rx_chain(t_data_channel *dchan)
{
  int total = 0, i = 0;
  char *flat;
  t_data_chunk *cur;
  if ((!dchan->rx) || (!dchan->rx->next))
    return;
  cur = dchan->rx;
  while (cur) { total += cur->len; cur = cur->next; }
  if (total <= 0)
    return;
  flat = (char *) clownix_malloc(total + 1, IOCMLC);
  flat[total] = 0;
  cur = dchan->rx;
  while (cur)
    {
    memcpy(flat + i, cur->chunk, cur->len);
    i += cur->len;
    cur = cur->next;
    }
  chain_del(&(dchan->rx), NULL);
  chain_append(&(dchan->rx), total, flat);
}

/* How many bytes at the END of the buffer could be a magic still arriving. */
static int magic_prefix_suffix_len(t_data_chunk *rx)
{
  int len, n, j, ok;
  if (!rx)
    return 0;
  len = rx->len;
  n = (len < JSON_MAGIC_LEN - 1) ? len : JSON_MAGIC_LEN - 1;
  for (; n > 0; n--)
    {
    ok = 1;
    for (j = 0; j < n; j++)
      {
      if ((unsigned char)rx->chunk[len - n + j] != json_magic[j])
        { ok = 0; break; }
      }
    if (ok)
      return n;
    }
  return 0;
}

/* Drop everything except the last keep bytes. */
static void drop_all_but_tail(t_data_channel *dchan, int keep)
{
  char *tail;
  int len;
  if (!dchan->rx)
    return;
  len = dchan->rx->len;
  if ((keep <= 0) || (keep > len))
    {
    chain_del(&(dchan->rx), NULL);
    return;
    }
  tail = (char *) clownix_malloc(keep + 1, IOCMLC);
  tail[keep] = 0;
  memcpy(tail, dchan->rx->chunk + (len - keep), keep);
  chain_del(&(dchan->rx), NULL);
  chain_append(&(dchan->rx), keep, tail);
}

static t_data_chunk *detect_json_start_in_chunk(t_data_chunk *first)
{
  /* Scan for the full 8-byte magic sequence across the chunk chain. */
  int i, j, keep_on = 1;
  t_data_chunk *cur = first;
  t_data_chunk *result = NULL;
  if (!cur)
    KOUT(" ");
  while (keep_on && cur)
    {
    for (i = 0; i < cur->len; i++)
      {
      if ((unsigned char)cur->chunk[i] == json_magic[0])
        {
        /* Check remaining magic bytes within this chunk */
        int match = 1;
        for (j = 1; j < JSON_MAGIC_LEN; j++)
          {
          if (i + j >= cur->len)
            { match = 0; break; } /* magic spans chunks — caller handles */
          if ((unsigned char)cur->chunk[i+j] != json_magic[j])
            { match = 0; break; }
          }
        if (match)
          {
          result = cur;
          keep_on = 0;
          break;
          }
        }
      }
    cur = cur->next;
    }
  return result;
}

           
static int chunks_rx_proxy_sig_header(t_data_channel *dchan)
{
  uint32_t total_len = 0;
  int result = 0;
  if ((dchan->rx) && (dchan->rx->len >= PROXY_HEADER_LEN))
    {
    total_len += (uint32_t) (dchan->rx->chunk[0] << 24) & 0xFF000000;
    total_len += (uint32_t) (dchan->rx->chunk[1] << 16) & 0x00FF0000;
    total_len += (uint32_t) (dchan->rx->chunk[2] << 8)  & 0x0000FF00;
    total_len += (uint32_t) (dchan->rx->chunk[3] & 0xFF);
    if (total_len > MAX_TOT_LEN_PROXY)
      KOUT("ERROR TOO LONG %u %hhu %hhu %hhu %hhu", total_len,
            dchan->rx->chunk[0], dchan->rx->chunk[1],
            dchan->rx->chunk[2], dchan->rx->chunk[3]);
    dchan->total_length = total_len;
    result = 1;
    }
  return result;
}

static int chunks_rx_proxy_sig_all_received(t_data_channel *dchan,
                                        char **found, int *found_len)
{
  int tot_len, len_what_is_left, result = 0;
  char *end_msg,  *tot_rx, *what_is_left;
  tot_len = make_a_buf_copy(dchan->rx, PROXY_HEADER_LEN, &tot_rx);
  if (tot_len >= dchan->total_length)
    {
    chain_del(&(dchan->rx), NULL);
    *found = tot_rx;
    *found_len = dchan->total_length;
    len_what_is_left = tot_len - *found_len;
    end_msg = tot_rx + dchan->total_length;
    if (len_what_is_left)
      {
      what_is_left = (char *) clownix_malloc(len_what_is_left+1, IOCMLC);
      what_is_left[len_what_is_left] = 0;
      memcpy(what_is_left, end_msg, len_what_is_left);
      chain_append(&(dchan->rx), len_what_is_left, what_is_left);
      }
    result = 1;
    }
  else
    clownix_free(tot_rx, __FUNCTION__);
  return result;
}

static void proxy_sig_rx_to_process(t_data_channel *dchan)
{
  int found_len;
  char *found;
  if (dchan->decoding_state == rx_type_proxy_sig_start)
    {
    if (chunks_rx_proxy_sig_header(dchan))
      dchan->decoding_state = rx_type_proxy_sig_header_ok;
    }
  while(dchan->decoding_state == rx_type_proxy_sig_header_ok)
    {
    if (chunks_rx_proxy_sig_all_received(dchan, &found, &found_len))
      {
      dchan->rx_callback(dchan->llid, found_len, found);
      clownix_free(found, __FUNCTION__);
      dchan->decoding_state = rx_type_proxy_sig_start;
      if ((dchan->llid) && (dchan->rx))
        {
        if (chunks_rx_proxy_sig_header(dchan))
          dchan->decoding_state = rx_type_proxy_sig_header_ok;
        }
      else
        break;
      }
    else if (dchan->decoding_state == rx_type_proxy_sig_start)
      {
      if (chunks_rx_proxy_sig_header(dchan))
        {
        KERR("WARNING2: THIS IS A RARE OCCURENCE");
        dchan->decoding_state = rx_type_proxy_sig_header_ok;
        }
      }
    else
      break;
    }
}

static void chunks_state_rx_type_ascii_start(t_data_channel *dchan)
{
  /* JSON: scan for magic prefix 0xCAFEDECA0xCAFEDECA then '{'. */
  t_data_chunk *chunk_of_start;
  int magic_pos, total_bytes, i;
  t_data_chunk *cur;
  char *flat; char *nrx;
  if (dchan->boundary)
    KOUT(" ");
  if (!dchan->rx || !dchan->rx->len)
    return;
  chunk_of_start = detect_json_start_in_chunk(dchan->rx);
  if (!chunk_of_start)
    {
    flatten_rx_chain(dchan);
    chunk_of_start = detect_json_start_in_chunk(dchan->rx);
    }
  if (!chunk_of_start)
    {
    int keep = magic_prefix_suffix_len(dchan->rx);
    if ((dchan->rx) && (keep == dchan->rx->len))
      return;
    {
    static unsigned long nb_no_magic;
    nb_no_magic += 1;
    if ((nb_no_magic % 100) == 1)
      KERR("ERROR dropping %d bytes with no JSON magic (%lu so far), "
           "keeping %d that may be a magic still arriving: %.60s",
           dchan->rx ? dchan->rx->len - keep : 0, nb_no_magic, keep,
           dchan->rx ? dchan->rx->chunk : "");
    }
    drop_all_but_tail(dchan, keep);
    return;
    }
  chain_del(&(dchan->rx), chunk_of_start);
  magic_pos = 0;
  if (dchan->rx)
    {
    for (magic_pos = 0; magic_pos < dchan->rx->len; magic_pos++)
      {
      int match = 1, j;
      if ((unsigned char)dchan->rx->chunk[magic_pos] != json_magic[0])
        continue;
      for (j = 1; j < JSON_MAGIC_LEN; j++)
        {
        if (magic_pos + j >= dchan->rx->len ||
            (unsigned char)dchan->rx->chunk[magic_pos+j] != json_magic[j])
          { match = 0; break; }
        }
      if (match)
        break;
      }
    }
  total_bytes = 0;
  cur = dchan->rx;
  while (cur) { total_bytes += cur->len; cur = cur->next; }
  total_bytes -= magic_pos;
  if (total_bytes < JSON_MAGIC_LEN + 1)
    return; /* magic not fully arrived — wait for more data */
  flat = (char *) clownix_malloc(total_bytes + 1, IOCMLC);
  flat[total_bytes] = 0;
  i = 0;
  cur = dchan->rx;
  {
    int bytes = cur->len - magic_pos;
    memcpy(flat + i, cur->chunk + magic_pos, bytes);
    i += bytes;
    cur = cur->next;
  }
  while (cur)
    {
    memcpy(flat + i, cur->chunk, cur->len);
    i += cur->len;
    cur = cur->next;
    }
  chain_del(&(dchan->rx), NULL);
  nrx = flat; /* flat[0]=0xCA, flat[8]='{' */
  chain_append(&(dchan->rx), total_bytes, nrx);
  alloc_boundary(dchan, "}", dchan->rx, JSON_MAGIC_LEN);
}

static int chunks_state_rx_type_close_bound_found(t_data_channel *dchan,
                                                  char **found, int *found_len)
{
  int tot_len, len_what_is_left, depth, in_str, escape, result = 0;
  int i, close_pos;
  char *tot_rx, *what_is_left = NULL;
  if (!dchan->boundary)
    KOUT(" ");
  if (!dchan->rx_bound_start)
    KOUT(" ");
  tot_len = make_a_buf_copy(dchan->rx, dchan->i_bound_start, &tot_rx);
  if (tot_len <= 0)
    {
    clownix_free(tot_rx, __FUNCTION__);
    return 0;
    }
  depth = 0; in_str = 0; escape = 0; close_pos = -1;
  for (i = 0; i < tot_len; i++)
    {
    char c = tot_rx[i];
    if (escape)        { escape = 0; continue; }
    if (c == '\\' && in_str) { escape = 1; continue; }
    if (c == '"')     { in_str = !in_str; continue; }
    if (in_str)        { continue; }
    if (c == '{')     { depth++; }
    else if (c == '}')
      {
      depth--;
      if (depth == 0) { close_pos = i; break; }
      }
    }
  if (close_pos < 0)
    {
    clownix_free(tot_rx, __FUNCTION__);
    return 0;
    }
  result = 1;
  *found_len       = close_pos + 1;
  len_what_is_left = tot_len - *found_len;
  if (len_what_is_left > 0)
    {
    int skip = 0, j;
    while (skip < len_what_is_left)
      {
      if ((unsigned char)(tot_rx[*found_len + skip]) == json_magic[0])
        {
        int match = 1;
        for (j = 1; j < JSON_MAGIC_LEN; j++)
          {
          if (skip + j >= len_what_is_left ||
              (unsigned char)(tot_rx[*found_len + skip + j]) != json_magic[j])
            { match = 0; break; }
          }
        if (match) break;
        }
      skip++;
      }
    if (skip == len_what_is_left)
      {
      int keep = 0, k, ok;
      int max = (len_what_is_left < JSON_MAGIC_LEN - 1) ?
                len_what_is_left : JSON_MAGIC_LEN - 1;
      for (; max > 0; max--)
        {
        ok = 1;
        for (k = 0; k < max; k++)
          {
          if ((unsigned char)(tot_rx[tot_len - max + k]) != json_magic[k])
            { ok = 0; break; }
          }
        if (ok)
          { keep = max; break; }
        }
      skip = len_what_is_left - keep;
      len_what_is_left = keep;
      }
    else
      len_what_is_left -= skip;
    if (len_what_is_left > 0)
      {
      what_is_left = (char *) clownix_malloc(len_what_is_left + 1, IOCMLC);
      what_is_left[len_what_is_left] = 0;
      memcpy(what_is_left, tot_rx + *found_len + skip, len_what_is_left);
      }
    }
  *found = tot_rx;
  (*found)[*found_len] = 0;
  chain_del(&(dchan->rx), NULL);
  free_boundary(dchan);
  if (len_what_is_left > 0)
    chain_append(&(dchan->rx), len_what_is_left, what_is_left);
  return result;
}

static void string_rx_to_process(t_data_channel *dchan)
{
  int found_len;
  char *found;
  if (dchan->decoding_state == rx_type_ascii_start)
    chunks_state_rx_type_ascii_start(dchan);
  while(dchan->decoding_state == rx_type_open_bound_found)
    {
    if (chunks_state_rx_type_close_bound_found(dchan, &found, &found_len))
      {
      dchan->rx_callback(dchan->llid, found_len, found);
      clownix_free(found, __FUNCTION__);
      if (dchan->llid)
        chunks_state_rx_type_ascii_start(dchan);
      else
        break;
      }
    else if (dchan->decoding_state == rx_type_ascii_start)
      chunks_state_rx_type_ascii_start(dchan);
    else
      break;
    }
}

void chunk_chain_delete(t_data_channel *dchan)
{
  chain_del(&(dchan->rx), NULL);
  if (dchan->decoding_state == rx_type_ascii_start)
    {
    if (dchan->boundary)
      KOUT(" ");
    }
  else if (dchan->decoding_state == rx_type_open_bound_found)
    {
    if (!dchan->boundary)
      KOUT(" ");
    free_boundary(dchan);
    }
  else if ((dchan->decoding_state == rx_type_watch)                 ||
           (dchan->decoding_state == rx_type_proxy_traf_tcp_start)  ||
           (dchan->decoding_state == rx_type_proxy_traf_unix_start) ||
           (dchan->decoding_state == rx_type_proxy_sig_start)       ||
           (dchan->decoding_state == rx_type_proxy_sig_header_ok)   ||
           (dchan->decoding_state == rx_type_listen)                ||
           (dchan->decoding_state == rx_type_doorways)) 
    {
    }
  else
    KOUT("ERROR %d", dchan->decoding_state);
}

void new_rx_to_process(t_data_channel *dchan, int len, char *new_rx)
{
  chain_append(&dchan->rx, len, new_rx);
  if ((dchan->decoding_state == rx_type_ascii_start)||
      (dchan->decoding_state == rx_type_open_bound_found))
    {
    string_rx_to_process(dchan);
    }
  else if ((dchan->decoding_state == rx_type_proxy_sig_start) ||
           (dchan->decoding_state == rx_type_proxy_sig_header_ok))
    {
    proxy_sig_rx_to_process(dchan);
    }
  else if ((dchan->decoding_state == rx_type_proxy_traf_unix_start) ||
           (dchan->decoding_state == rx_type_proxy_traf_tcp_start))
    {
    KOUT("DROP new_rx: proxy_traf state %d llid=%d len=%d data appended but never dispatched",
         dchan->decoding_state, dchan->llid, len);
    }
  else
    KOUT("ERROR %d", dchan->decoding_state);
}

