/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
/* One warning site, one gate: KERR_BURST lines at most in KERR_WINDOW_MS, */
/* the others counted and announced by the next line that passes. KERR */
/* keeps a gate per call site; the loggers of the guest agents keep one per */
/* format string, kerr_gate_of_fmt. */
#ifndef KERR_GATE_H
#define KERR_GATE_H

#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

#define KERR_WINDOW_MS 10000
#define KERR_BURST 10
#define KERR_GATE_FMT_MAX 256

typedef struct t_kerr_gate
{
  const char *fmt;
  long long start_ms;
  unsigned int nb;
  unsigned int skipped;
} t_kerr_gate;

static inline long long kerr_gate_now_ms(void)
{
#ifdef _WIN32
  return (long long) GetTickCount64();
#else
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (((long long) ts.tv_sec) * 1000) + (ts.tv_nsec / 1000000);
#endif
}

/* 1: log the line, note is its suffix. 0: this line is counted only. */
static inline int kerr_gate(t_kerr_gate *g, char *note, int note_len)
{
  long long now_ms = kerr_gate_now_ms();
  note[0] = 0;
  if ((g->start_ms == 0) || (now_ms - g->start_ms >= KERR_WINDOW_MS))
    {
    g->start_ms = now_ms;
    g->nb = 0;
    }
  if (g->nb >= KERR_BURST)
    {
    g->skipped += 1;
    return 0;
    }
  g->nb += 1;
  if (g->skipped)
    {
    snprintf(note, note_len, " [%u line(s) of this site not logged]",
             g->skipped);
    g->skipped = 0;
    }
  return 1;
}

/* The gate of a format string; a full table shares its last slot. */
static inline t_kerr_gate *kerr_gate_of_fmt(t_kerr_gate *tab, const char *fmt)
{
  int i;
  for (i = 0; i < KERR_GATE_FMT_MAX - 1; i++)
    {
    if (tab[i].fmt == fmt)
      return &(tab[i]);
    if (tab[i].fmt == NULL)
      {
      tab[i].fmt = fmt;
      return &(tab[i]);
      }
    }
  return &(tab[KERR_GATE_FMT_MAX - 1]);
}

#endif
