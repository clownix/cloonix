/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
/* qcow2 header read and write, in place of qemu-img create and info. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "qcow2_hdr.h"

#define QCOW2_MAGIC       0x514649fbUL
#define QCOW2_VERSION     3
#define QCOW2_CLUSTER_BITS 16
#define QCOW2_CLUSTER_SIZE (1 << QCOW2_CLUSTER_BITS)
#define QCOW2_HEADER_LEN  104
#define QCOW2_EXT_BACKING_FMT 0xE2792ACAUL
#define QCOW2_L2_COVER    ((unsigned long long)QCOW2_CLUSTER_SIZE * \
                           (QCOW2_CLUSTER_SIZE / 8))
#define QCOW2_BACKING_OFF 128

static void put_be32(unsigned char *p, unsigned int v)
{
  p[0] = (v >> 24) & 0xFF;
  p[1] = (v >> 16) & 0xFF;
  p[2] = (v >>  8) & 0xFF;
  p[3] =  v        & 0xFF;
}

static void put_be64(unsigned char *p, unsigned long long v)
{
  put_be32(p,   (unsigned int)(v >> 32));
  put_be32(p+4, (unsigned int)(v & 0xFFFFFFFFULL));
}

static unsigned int get_be32(unsigned char *p)
{
  return (((unsigned int)p[0] << 24) | ((unsigned int)p[1] << 16) |
          ((unsigned int)p[2] <<  8) |  (unsigned int)p[3]);
}

static unsigned long long get_be64(unsigned char *p)
{
  return (((unsigned long long)get_be32(p) << 32) | get_be32(p+4));
}

/* Reads the first 104 bytes. */
static int read_header(char *path, unsigned char *hdr)
{
  int fd, result = -1;
  fd = open(path, O_RDONLY);
  if (fd >= 0)
    {
    if (read(fd, hdr, QCOW2_HEADER_LEN) == QCOW2_HEADER_LEN)
      {
      if (get_be32(hdr) == QCOW2_MAGIC)
        result = 0;
      }
    close(fd);
    }
  return result;
}

static int backing_virtual_size(char *backing, unsigned long long *size)
{
  int result = -1;
  unsigned char hdr[QCOW2_HEADER_LEN];
  struct stat st;
  if (!read_header(backing, hdr))
    {
    *size = get_be64(hdr + 24);
    result = 0;
    }
  else if (!stat(backing, &st))
    {
    *size = (unsigned long long) st.st_size;
    result = 0;
    }
  return result;
}

int qcow2_get_backing(char *path, char *backing, int len)
{
  int fd, result = -1;
  unsigned int bsize;
  unsigned long long boff;
  unsigned char hdr[QCOW2_HEADER_LEN];
  memset(backing, 0, len);
  if (!read_header(path, hdr))
    {
    boff  = get_be64(hdr + 8);
    bsize = get_be32(hdr + 16);
    if ((boff == 0) || (bsize == 0))
      result = 0;
    else if (bsize >= (unsigned int) len)
      result = -1;
    else
      {
      fd = open(path, O_RDONLY);
      if (fd >= 0)
        {
        if (pread(fd, backing, bsize, (off_t) boff) == (int) bsize)
          result = 0;
        close(fd);
        }
      }
    }
  return result;
}

int qcow2_create_derived(char *backing, char *used)
{
  int fd, i, blen, l1_clusters, nb_clusters, result = -1;
  unsigned int l1_size;
  unsigned long long vsize, l1_off;
  unsigned char *cluster;

  blen = strlen(backing);
  if ((blen == 0) || (blen > QCOW2_CLUSTER_SIZE - QCOW2_BACKING_OFF - 1))
    return -1;
  if (backing_virtual_size(backing, &vsize))
    return -1;

  l1_size = (unsigned int)((vsize + QCOW2_L2_COVER - 1) / QCOW2_L2_COVER);
  if (l1_size == 0)
    l1_size = 1;
  l1_clusters = (int)((l1_size * 8 + QCOW2_CLUSTER_SIZE - 1) /
                      QCOW2_CLUSTER_SIZE);
  l1_off      = 3ULL * QCOW2_CLUSTER_SIZE;
  nb_clusters = 3 + l1_clusters;

  cluster = (unsigned char *) malloc(QCOW2_CLUSTER_SIZE);
  if (!cluster)
    return -1;

  fd = open(used, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd < 0)
    {
    free(cluster);
    return -1;
    }

  memset(cluster, 0, QCOW2_CLUSTER_SIZE);
  put_be32(cluster +  0, QCOW2_MAGIC);
  put_be32(cluster +  4, QCOW2_VERSION);
  put_be64(cluster +  8, QCOW2_BACKING_OFF);
  put_be32(cluster + 16, (unsigned int) blen);
  put_be32(cluster + 20, QCOW2_CLUSTER_BITS);
  put_be64(cluster + 24, vsize);
  put_be32(cluster + 32, 0);
  put_be32(cluster + 36, l1_size);
  put_be64(cluster + 40, l1_off);
  put_be64(cluster + 48, (unsigned long long) QCOW2_CLUSTER_SIZE);
  put_be32(cluster + 56, 1);
  put_be32(cluster + 60, 0);
  put_be64(cluster + 64, 0);
  put_be64(cluster + 72, 0);
  put_be64(cluster + 80, 0);
  put_be64(cluster + 88, 0);
  put_be32(cluster + 96, 4);
  put_be32(cluster + 100, QCOW2_HEADER_LEN);
  put_be32(cluster + 104, QCOW2_EXT_BACKING_FMT);
  put_be32(cluster + 108, 5);
  memcpy(cluster + 112, "qcow2", 5);
  put_be32(cluster + 120, 0);
  put_be32(cluster + 124, 0);
  memcpy(cluster + QCOW2_BACKING_OFF, backing, blen);
  if (write(fd, cluster, QCOW2_CLUSTER_SIZE) == QCOW2_CLUSTER_SIZE)
    {
    memset(cluster, 0, QCOW2_CLUSTER_SIZE);
    put_be64(cluster, 2ULL * QCOW2_CLUSTER_SIZE);
    if (write(fd, cluster, QCOW2_CLUSTER_SIZE) == QCOW2_CLUSTER_SIZE)
      {
      memset(cluster, 0, QCOW2_CLUSTER_SIZE);
      for (i = 0; i < nb_clusters; i++)
        {
        cluster[2*i]   = 0;
        cluster[2*i+1] = 1;
        }
      if (write(fd, cluster, QCOW2_CLUSTER_SIZE) == QCOW2_CLUSTER_SIZE)
        {
        memset(cluster, 0, QCOW2_CLUSTER_SIZE);
        result = 0;
        for (i = 0; i < l1_clusters; i++)
          {
          if (write(fd, cluster, QCOW2_CLUSTER_SIZE) != QCOW2_CLUSTER_SIZE)
            result = -1;
          }
        }
      }
    }
  if (close(fd))
    result = -1;
  free(cluster);
  if (result)
    unlink(used);
  return result;
}
