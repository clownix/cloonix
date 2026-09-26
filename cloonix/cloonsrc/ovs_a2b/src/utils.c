/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/un.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "utils.h"

static int g_nb_tot_malloc;

void utils_init(void)
{
  g_nb_tot_malloc = 0;
}

