/** @file
 * File utility definitions
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __FILE_UTIL_H__
#define __FILE_UTIL_H__

#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define IO_BUF_SIZE (64 * 1024)

#ifndef S_ISREG
#define S_ISREG(mode)   (((mode) & S_IFMT) == S_IFREG)
#endif
#ifndef S_IFIFO
#define S_IFIFO _S_IFIFO
#endif
#ifndef S_ISFIFO
#define S_ISFIFO(mode)  (((mode) & S_IFMT) == S_IFIFO)
#endif
#ifndef S_ISDIR
#define S_ISDIR(mode)   (((mode) & S_IFMT) == S_IFDIR)
#endif

#define ws_statb64	struct stat

#define ws_open			open
#define ws_rename		rename
#define ws_mkdir(dir,mode)	mkdir(dir,mode)
#define ws_stat64		stat
#define ws_unlink		unlink
#define ws_remove		remove
#define ws_fopen		fopen
#define ws_freopen		freopen

typedef size_t ws_file_size_t;
typedef ssize_t ws_file_ssize_t;

#define ws_read    read
#define ws_write   write
#ifdef __cplusplus
#define ws_close   ::close
#else
#define ws_close   close
#endif

#define ws_close_if_possible ws_close

#define ws_dup     dup
#ifdef HAVE_FSEEKO
#define ws_fseek64 fseeko	/* AC_SYS_LARGEFILE should make off_t 64-bit */
#define ws_ftell64 ftello	/* AC_SYS_LARGEFILE should make off_t 64-bit */
#else
#define ws_fseek64(fh,offset,whence)	fseek(fh,(long)(offset),whence)
#define ws_ftell64 ftell
#endif
#define ws_fstat64 fstat	/* AC_SYS_LARGEFILE should make off_t 64-bit */
#define ws_lseek64 lseek	/* AC_SYS_LARGEFILE should make off_t 64-bit */
#define ws_fdopen  fdopen
#define ws_fileno  fileno
#define ws_isatty  isatty
#define ws_getc_unlocked getc_unlocked
#define O_BINARY   0		/* Win32 needs the O_BINARY flag for open() */

/* Other CRT functions */
#define ws_getpid  getpid
#define ws_umask   umask

#define WS_DIR				GDir
#define WS_DIRENT			const char
#define ws_dir_open			g_dir_open
#define ws_dir_read_name		g_dir_read_name
#define ws_dir_get_name(dirent)		dirent
#define ws_dir_rewind			g_dir_rewind
#define ws_dir_close			g_dir_close


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __FILE_UTIL_H__ */
