/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
/* Watch the directory this binary was started from. See dir_watch.c.       */

/* Start watching. Returns 0 when the watch is armed. */
int  dir_watch_init(void);

void dir_watch_note_server(int llid);
