/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
/* cloonix_cxp_serv_win.c - the exec pipe of a windows guest.                */
/* The same place in the chain as cloonix_cxp_srv on linux, the same frames  */
/* on the wire, and now the same opcodes: cxp_wire.h is shared by the two.   */
/* The agent relays those frames between here and the host and reads none of */
/* them, exactly as the linux agent relays to its unix socket.               */
/* Listens on the loopback rather than a unix socket. AF_UNIX exists on      */
/* current windows but is not dependable across the builds a guest might be, */
/* and the agent needs sockets in any case.                                  */
/* The three shapes, and what windows gives for each:                        */
/*   PTY      a pseudoconsole running cmd.exe. CreatePseudoConsole is what   */
/*            gives a resizable terminal with the escape sequences a browser */
/*            terminal expects; a plain pipe to cmd.exe gives neither.       */
/*   EXEC     cmd.exe /c <command>, three pipes, stdout and stderr kept      */
/*            apart so the client can tell them apart the way ssh does.      */
/*   PUT/GET  files and whole trees, with the paths taken as windows paths:  */
/*            "/" is accepted and turned into "\", and a destination that is */
/*            an existing directory receives the file under its own name.    */
/* Modes travel on the wire because linux has them. Windows has no chmod of  */
/* that kind, so an incoming mode is dropped and an outgoing one is the      */
/* usual 0644 or 0755: the client chmods what it writes on its own disk.     */
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <wtsapi32.h>
#include <userenv.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "cxp_wire.h"
#include "kerr_gate.h"

#define CXP_PORT 4441
#define CHUNK 32768
#define MAX_FRAME (1024*1024)
#define MAX_PATH_CXP 4096
#define MAX_NAME_CXP 1024

#define FILE_MODE 0644
#define DIR_MODE  0755

#define EXEC_POLL_MS 20

#define BIND_TRIES 40
#define BIND_RETRY_MS 500

static FILE *g_log;

static int g_no_pty;
static int g_force_pty;
static int g_in_session;
static volatile LONG g_busy;

typedef struct t_conn
{
  SOCKET sock;
  CRITICAL_SECTION tx;
} t_conn;

static void run_on_pipes(t_conn *co, const char *line_in, int interactive);

static void srv_log(const char *fmt, ...)
{
  static t_kerr_gate tab[KERR_GATE_FMT_MAX];
  va_list ap;
  SYSTEMTIME st;
  char stamp[40], note[64];
  if (!kerr_gate(kerr_gate_of_fmt(tab, fmt), note, sizeof(note)))
    return;
  GetLocalTime(&st);
  /* With the DATE: the time alone cannot say which run a line is from. */
  snprintf(stamp, sizeof(stamp), "%04d-%02d-%02d %02d:%02d:%02d.%03d ",
           st.wYear, st.wMonth, st.wDay,
           st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
  fputs(stamp, stdout);
  va_start(ap, fmt);
  vfprintf(stdout, fmt, ap);
  va_end(ap);
  fputs(note, stdout);
  fputs("\n", stdout);
  fflush(stdout);
  if (!g_log)
    return;
  fputs(stamp, g_log);
  va_start(ap, fmt);
  vfprintf(g_log, fmt, ap);
  va_end(ap);
  fputs(note, g_log);
  fputs("\n", g_log);
  fflush(g_log);
}

static void log_open(void)
{
  MoveFileExA("C:\\cloonix-cxp-srv.log", "C:\\cloonix-cxp-srv.log.prev",
              MOVEFILE_REPLACE_EXISTING);
  g_log = fopen("C:\\cloonix-cxp-srv.log", "w");
}

typedef void *HPCON;
#define PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE 0x00020016

typedef HRESULT (WINAPI *t_create_pcon)(COORD, HANDLE, HANDLE, DWORD, HPCON*);
typedef HRESULT (WINAPI *t_resize_pcon)(HPCON, COORD);
typedef void    (WINAPI *t_close_pcon)(HPCON);

static t_create_pcon g_create_pcon;
static t_resize_pcon g_resize_pcon;
static t_close_pcon  g_close_pcon;

static int pcon_api_load(void)
{
  HMODULE k32 = GetModuleHandleA("kernel32.dll");
  if (!k32)
    return -1;
  g_create_pcon = (t_create_pcon) (void *)
                  GetProcAddress(k32, "CreatePseudoConsole");
  g_resize_pcon = (t_resize_pcon) (void *)
                  GetProcAddress(k32, "ResizePseudoConsole");
  g_close_pcon  = (t_close_pcon) (void *)
                  GetProcAddress(k32, "ClosePseudoConsole");
  if ((!g_create_pcon) || (!g_resize_pcon) || (!g_close_pcon))
    {
    srv_log("WARNING no pseudo console in kernel32: no pty on this windows");
    return -1;
    }
  return 0;
}

static int send_all(SOCKET s, const void *buf, int len)
{
  const char *p = (const char *) buf;
  int done = 0, n;
  while (done < len)
    {
    n = send(s, p + done, len - done, 0);
    if (n <= 0)
      return -1;
    done += n;
    }
  return 0;
}

static int send_frame(t_conn *co, int op, const void *payload, int len)
{
  unsigned char head[5];
  int tot = len + 1, result = 0;
  head[0] = (tot >> 24) & 0xFF;
  head[1] = (tot >> 16) & 0xFF;
  head[2] = (tot >> 8)  & 0xFF;
  head[3] = tot & 0xFF;
  head[4] = op & 0xFF;
  EnterCriticalSection(&(co->tx));
  if (send_all(co->sock, head, 5))
    result = -1;
  else if ((len) && (send_all(co->sock, payload, len)))
    result = -1;
  LeaveCriticalSection(&(co->tx));
  return result;
}

static int send_txt(t_conn *co, int op, const char *fmt, ...)
{
  char buf[MAX_PATH_CXP + 256];
  va_list ap;
  int len;
  va_start(ap, fmt);
  len = vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  if (len < 0)
    return -1;
  if (len > (int) sizeof(buf) - 1)
    len = (int) sizeof(buf) - 1;
  return send_frame(co, op, buf, len);
}

static int send_err(t_conn *co, const char *fmt, ...)
{
  char raw[MAX_PATH_CXP + 256], esc[2 * (MAX_PATH_CXP + 256)];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(raw, sizeof(raw), fmt, ap);
  va_end(ap);
  cxp_json_escape(raw, esc, sizeof(esc));
  return send_txt(co, OP_ERROR, "{\"txt\":\"%s\"}", esc);
}

static int recv_all(SOCKET s, void *buf, int len)
{
  char *p = (char *) buf;
  int done = 0, n;
  while (done < len)
    {
    n = recv(s, p + done, len - done, 0);
    if (n <= 0)
      return -1;
    done += n;
    }
  return 0;
}

static int recv_frame(SOCKET s, char **payload, int *plen)
{
  unsigned char head[5];
  int tot, len;
  char *buf;
  *payload = NULL;
  *plen = 0;
  if (recv_all(s, head, 5))
    return -1;
  tot = (head[0] << 24) | (head[1] << 16) | (head[2] << 8) | head[3];
  if ((tot < 1) || (tot > MAX_FRAME))
    return -1;
  len = tot - 1;
  buf = (char *) malloc(len + 1);
  if (!buf)
    return -1;
  if ((len) && (recv_all(s, buf, len)))
    {
    free(buf);
    return -1;
    }
  buf[len] = 0;
  *payload = buf;
  *plen = len;
  return head[4];
}

static void path_norm(char *path)
{
  char *p;
  for (p = path; *p; p++)
    {
    if (*p == '/')
      *p = '\\';
    }
}

static void expand_home(char *path, int sz)
{
  char out[MAX_PATH_CXP];
  const char *home = getenv("USERPROFILE");
  if (path[0] != '~')
    return;
  if ((path[1]) && (path[1] != '/') && (path[1] != '\\'))
    return;
  if ((!home) || (!home[0]))
    return;
  snprintf(out, sizeof(out), "%.2000s%.2000s", home, path + 1);
  snprintf(path, sz, "%s", out);
}

static void path_join(char *dst, int sz, const char *dir, const char *leaf)
{
  int len = (int) strlen(dir);
  if ((len > 0) && ((dir[len-1] == '\\') || (dir[len-1] == '/')))
    snprintf(dst, sz, "%.3000s%.1000s", dir, leaf);
  else
    snprintf(dst, sz, "%.3000s\\%.1000s", dir, leaf);
  path_norm(dst);
}

static int path_is_dir(const char *path)
{
  DWORD attr = GetFileAttributesA(path);
  if (attr == INVALID_FILE_ATTRIBUTES)
    return 0;
  return ((attr & FILE_ATTRIBUTE_DIRECTORY) != 0);
}

static char *last_error_txt(void)
{
  static char txt[256];
  DWORD err = GetLastError();
  int len;
  txt[0] = 0;
  FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                 NULL, err, 0, txt, sizeof(txt) - 1, NULL);
  len = (int) strlen(txt);
  while ((len > 0) && ((txt[len-1] == '\n') || (txt[len-1] == '\r') ||
                       (txt[len-1] == ' ') || (txt[len-1] == '.')))
    txt[--len] = 0;
  if (!txt[0])
    snprintf(txt, sizeof(txt), "error %lu", (unsigned long) err);
  return txt;
}

static int mkdir_p(const char *path)
{
  char tmp[MAX_PATH_CXP];
  char *p;
  snprintf(tmp, sizeof(tmp), "%s", path);
  path_norm(tmp);
  /* An existing directory is success; CreateDirectory("C:\\") refuses
     rather than answering "already exists". */
  if (path_is_dir(tmp))
    return 0;
  p = tmp;
  if ((tmp[0]) && (tmp[1] == ':'))
    p += 2;
  if (*p == '\\')
    p++;
  for (; *p; p++)
    {
    if (*p != '\\')
      continue;
    *p = 0;
    if ((!CreateDirectoryA(tmp, NULL)) &&
        (GetLastError() != ERROR_ALREADY_EXISTS))
      return -1;
    *p = '\\';
    }
  if ((!CreateDirectoryA(tmp, NULL)) &&
      (GetLastError() != ERROR_ALREADY_EXISTS))
    return -1;
  return 0;
}

static void mkdir_parent(const char *full)
{
  char dir[MAX_PATH_CXP];
  char *slash;
  snprintf(dir, sizeof(dir), "%s", full);
  slash = strrchr(dir, '\\');
  if (!slash)
    return;
  *slash = 0;
  if (dir[0])
    mkdir_p(dir);
}

#define END_BY_NOBODY 0
#define END_BY_SHELL  1     /* the shell exited */
#define END_BY_PCON   2     /* the pseudoconsole stopped producing */
#define END_BY_CLIENT 3     /* the far end of the leg went away */

static const char *end_txt(LONG who)
{
  if (who == END_BY_SHELL)
    return "the shell exited";
  if (who == END_BY_PCON)
    return "the pseudoconsole stopped";
  if (who == END_BY_CLIENT)
    return "the client went away";
  return "nothing said";
}

typedef struct t_session
{
  t_conn *conn;
  HPCON  hpc;
  HANDLE to_pty;      /* we write, the pseudoconsole reads */
  HANDLE from_pty;    /* the pseudoconsole writes, we read */
  HANDLE proc;
  DWORD  pid;
  int    said_out;
  long   out_bytes;
  volatile LONG ended_by;
  int    eof_seen;
} t_session;

static DWORD WINAPI pty_reader(LPVOID arg)
{
  t_session *ses = (t_session *) arg;
  char buf[CHUNK];
  DWORD n;
  for (;;)
    {
    if (!ReadFile(ses->from_pty, buf, sizeof(buf), &n, NULL))
      break;
    if (n == 0)
      break;
    /* Said once, not per read. */
    if (!ses->said_out)
      {
      ses->said_out = 1;
      srv_log("pty: pid %lu, first %d bytes out of the pseudoconsole",
              (unsigned long) ses->pid, (int) n);
      }
    ses->out_bytes += (long) n;
    if (send_frame(ses->conn, OP_STDOUT, buf, (int) n))
      break;
    }
  InterlockedCompareExchange(&(ses->ended_by), END_BY_PCON, END_BY_NOBODY);
  shutdown(ses->conn->sock, SD_RECEIVE);
  return 0;
}

static DWORD WINAPI pty_waiter(LPVOID arg)
{
  t_session *ses = (t_session *) arg;
  WaitForSingleObject(ses->proc, INFINITE);
  InterlockedCompareExchange(&(ses->ended_by), END_BY_SHELL, END_BY_NOBODY);
  shutdown(ses->conn->sock, SD_RECEIVE);
  return 0;
}

static int spawn_in_pty(t_session *ses, const char *cmd, int cols, int rows)
{
  STARTUPINFOEXA si;
  PROCESS_INFORMATION pi;
  COORD size;
  SIZE_T attr_len = 0;
  HANDLE in_rd = NULL, in_wr = NULL, out_rd = NULL, out_wr = NULL;
  char line[8192], shell[MAX_PATH_CXP], sysdir[MAX_PATH_CXP];
  char windir[MAX_PATH_CXP];
  char *app;

  memset(&si, 0, sizeof(si));
  memset(&pi, 0, sizeof(pi));
  size.X = (SHORT) ((cols > 0) ? cols : 80);
  size.Y = (SHORT) ((rows > 0) ? rows : 24);

  if ((!CreatePipe(&in_rd, &in_wr, NULL, 0)) ||
      (!CreatePipe(&out_rd, &out_wr, NULL, 0)))
    {
    srv_log("CreatePipe failed: %s", last_error_txt());
    return -1;
    }
  if (g_create_pcon(size, in_rd, out_wr, 0, &(ses->hpc)) != S_OK)
    {
    srv_log("CreatePseudoConsole failed: %s", last_error_txt());
    return -1;
    }
  /* The pseudoconsole owns these ends now. */
  CloseHandle(in_rd);
  CloseHandle(out_wr);
  ses->to_pty = in_wr;
  ses->from_pty = out_rd;

  si.StartupInfo.cb = sizeof(STARTUPINFOEXA);
  InitializeProcThreadAttributeList(NULL, 1, 0, &attr_len);
  si.lpAttributeList =
    (LPPROC_THREAD_ATTRIBUTE_LIST) HeapAlloc(GetProcessHeap(), 0, attr_len);
  if (!si.lpAttributeList)
    {
    srv_log("no room for the attribute list");
    return -1;
    }
  if (!InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &attr_len))
    {
    srv_log("InitializeProcThreadAttributeList failed: %s", last_error_txt());
    return -1;
    }
  if (!UpdateProcThreadAttribute(si.lpAttributeList, 0,
                                 PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
                                 ses->hpc, sizeof(ses->hpc), NULL, NULL))
    {
    srv_log("UpdateProcThreadAttribute failed: %s", last_error_txt());
    return -1;
    }

  if ((cmd) && (cmd[0]))
    {
    snprintf(line, sizeof(line), "%s", cmd);
    app = NULL;
    }
  else
    {
    if (!GetSystemDirectoryA(sysdir, sizeof(sysdir)))
      snprintf(sysdir, sizeof(sysdir), "C:\\Windows\\System32");
    snprintf(shell, sizeof(shell), "%.3000s\\cmd.exe", sysdir);
    snprintf(line, sizeof(line), "%s", shell);
    app = shell;
    }
  /* Not the directory this exe came from: that is the insider disc, read
     only, and a shell that opens there cannot write its first file. */
  if (!GetSystemWindowsDirectoryA(windir, sizeof(windir)))
    snprintf(windir, sizeof(windir), "C:\\");
  else if ((windir[0]) && (windir[1] == ':'))
    snprintf(windir, sizeof(windir), "%c:\\", windir[0]);

  if (!CreateProcessA(app, line, NULL, NULL, FALSE,
                      EXTENDED_STARTUPINFO_PRESENT, NULL, windir,
                      &(si.StartupInfo), &pi))
    {
    srv_log("CreateProcess %s failed: %s", line, last_error_txt());
    return -1;
    }
  ses->proc = pi.hProcess;
  ses->pid = pi.dwProcessId;
  CloseHandle(pi.hThread);
  DeleteProcThreadAttributeList(si.lpAttributeList);
  HeapFree(GetProcessHeap(), 0, si.lpAttributeList);
  srv_log("pty: %s started as pid %lu in a %dx%d pseudoconsole",
          line, (unsigned long) ses->pid, (int) size.X, (int) size.Y);
  return 0;
}

static void session_close(t_session *ses)
{
  if (ses->hpc)
    g_close_pcon(ses->hpc);
  if (ses->to_pty)
    CloseHandle(ses->to_pty);
  if (ses->from_pty)
    CloseHandle(ses->from_pty);
  if (ses->proc)
    CloseHandle(ses->proc);
}

static void do_pty(t_conn *co, const char *body)
{
  t_session ses;
  HANDLE th, wth;
  char cmd[8192];
  char *pl = NULL;
  int pl_len = 0, op, cols, rows, alive = 0;
  DWORD code = 0, written;

  memset(&ses, 0, sizeof(ses));
  ses.conn = co;
  cols = cxp_json_int(body, "cols", 80);
  rows = cxp_json_int(body, "rows", 24);
  cxp_json_str(body, "cmd", cmd, sizeof(cmd));
  if (g_no_pty)
    {
    char line[8192];
    if (cmd[0])
      snprintf(line, sizeof(line), "%s", cmd);
    else
      snprintf(line, sizeof(line), "cmd.exe /q");
    srv_log("pty: serving %s on pipes", line);
    run_on_pipes(co, line, 1);
    return;
    }
  if (spawn_in_pty(&ses, cmd, cols, rows))
    {
    send_err(co, "no pseudoconsole");
    session_close(&ses);
    return;
    }
  th = CreateThread(NULL, 0, pty_reader, &ses, 0, NULL);
  if (!th)
    {
    send_err(co, "no reader thread");
    session_close(&ses);
    return;
    }
  /* Started even if it cannot be: without it a shell that exits leaves the
     session parked, which is worse than a session that outlives its shell. */
  wth = CreateThread(NULL, 0, pty_waiter, &ses, 0, NULL);
  if (!wth)
    srv_log("no waiter thread: a shell that exits will not end the session");
  for (;;)
    {
    op = recv_frame(co->sock, &pl, &pl_len);
    if (op < 0)
      break;
    if ((op == OP_STDIN) && (pl_len))
      {
      if (!WriteFile(ses.to_pty, pl, (DWORD) pl_len, &written, NULL))
        {
        free(pl);
        break;
        }
      }
    else if (op == OP_WINCH)
      {
      COORD sz;
      sz.X = (SHORT) cxp_json_int(pl, "cols", cols);
      sz.Y = (SHORT) cxp_json_int(pl, "rows", rows);
      g_resize_pcon(ses.hpc, sz);
      }
    else if (op == OP_EOF)
      {
      char eot = 26;
      ses.eof_seen = 1;
      WriteFile(ses.to_pty, &eot, 1, &written, NULL);
      }
    free(pl);
    pl = NULL;
    }
  if (pl)
    free(pl);
  InterlockedCompareExchange(&(ses.ended_by), END_BY_CLIENT, END_BY_NOBODY);
  alive = (WaitForSingleObject(ses.proc, 0) != WAIT_OBJECT_0);
  if (ses.to_pty)
    {
    CloseHandle(ses.to_pty);
    ses.to_pty = NULL;
    }
  if (ses.hpc)
    {
    g_close_pcon(ses.hpc);
    ses.hpc = NULL;
    }
  if (ses.proc)
    {
    if (WaitForSingleObject(ses.proc, 3000) != WAIT_OBJECT_0)
      TerminateProcess(ses.proc, 1);
    WaitForSingleObject(ses.proc, 2000);
    GetExitCodeProcess(ses.proc, &code);
    }
  /* Both joins are unbounded on purpose: the threads read this stack frame,
     so giving up on one and returning would pull the frame out from under it. */
  WaitForSingleObject(th, INFINITE);
  CloseHandle(th);
  if (wth)
    {
    WaitForSingleObject(wth, INFINITE);
    CloseHandle(wth);
    }
  srv_log("pty: pid %lu ended, %s%s, %s at teardown, code 0x%lX, "
          "%ld bytes out of the pseudoconsole",
          (unsigned long) ses.pid, end_txt(ses.ended_by),
          ses.eof_seen ? " after the client sent end-of-input" : "",
          alive ? "still running" : "already gone",
          (unsigned long) code, ses.out_bytes);
  send_txt(co, OP_EXIT, "{\"code\":%d}", (int) code);
  session_close(&ses);
}

/* One thread feeds stdin, because recv blocks and the pipes have to be */
typedef struct t_exec
{
  t_conn *conn;
  HANDLE  to_child;
  int     crlf;
  char    line[8192];
  int     len;
  int     esc;
} t_exec;

static void echo_back(t_exec *ex, const char *s, int n)
{
  send_frame(ex->conn, OP_STDOUT, s, n);
}

static void line_flush(t_exec *ex)
{
  DWORD written;
  if (!ex->to_child)
    return;
  ex->line[ex->len++] = '\r';
  ex->line[ex->len++] = '\n';
  WriteFile(ex->to_child, ex->line, (DWORD) ex->len, &written, NULL);
  ex->len = 0;
}

static void stdin_to_child(t_exec *ex, const char *buf, int len)
{
  DWORD written;
  unsigned char c;
  int i;

  if (!ex->crlf)
    {
    WriteFile(ex->to_child, buf, (DWORD) len, &written, NULL);
    return;
    }
  for (i = 0; i < len; i++)
    {
    c = (unsigned char) buf[i];
    /* An escape sequence is a key nothing here can use, swallowed whole: ESC
       alone, then "[" or "O", then bytes up to the letter that ends it. */
    if (ex->esc == 1)
      {
      ex->esc = ((c == '[') || (c == 'O')) ? 2 : 0;
      continue;
      }
    if (ex->esc == 2)
      {
      if ((c >= 0x40) && (c <= 0x7E))
        ex->esc = 0;
      continue;
      }
    if (c == 0x1B)
      {
      ex->esc = 1;
      continue;
      }
    if ((c == '\r') || (c == '\n'))
      {
      echo_back(ex, "\r\n", 2);
      line_flush(ex);
      continue;
      }
    if ((c == 0x08) || (c == 0x7F))
      {
      if (ex->len)
        {
        ex->len -= 1;
        echo_back(ex, "\b \b", 3);
        }
      continue;
      }
    if (c == 0x03)
      {
      echo_back(ex, "^C\r\n", 4);
      ex->len = 0;
      line_flush(ex);
      continue;
      }
    if (c < 0x20)
      continue;
    if (ex->len < (int) sizeof(ex->line) - 2)
      {
      ex->line[ex->len++] = (char) c;
      echo_back(ex, (const char *) &c, 1);
      }
    }
}

static DWORD WINAPI exec_stdin_thread(LPVOID arg)
{
  t_exec *ex = (t_exec *) arg;
  char *pl = NULL;
  int pl_len = 0, op;
  for (;;)
    {
    op = recv_frame(ex->conn->sock, &pl, &pl_len);
    if (op < 0)
      break;
    if ((op == OP_STDIN) && (pl_len) && (ex->to_child))
      stdin_to_child(ex, pl, pl_len);
    else if (op == OP_EOF)
      {
      if (ex->to_child)
        {
        CloseHandle(ex->to_child);
        ex->to_child = NULL;
        }
      }
    free(pl);
    pl = NULL;
    }
  if (pl)
    free(pl);
  if (ex->to_child)
    {
    CloseHandle(ex->to_child);
    ex->to_child = NULL;
    }
  return 0;
}

/* Whatever is in the pipe now, and nothing more: bytes sent, 0 when there are
   none, -1 when the client has gone. */
static int pipe_drain(t_conn *co, HANDLE *ph, int op)
{
  char buf[CHUNK];
  DWORD avail = 0, n = 0;
  if (!*ph)
    return 0;
  if (!PeekNamedPipe(*ph, NULL, 0, NULL, &avail, NULL))
    {
    CloseHandle(*ph);
    *ph = NULL;
    return 0;
    }
  if (!avail)
    return 0;
  if (avail > CHUNK)
    avail = CHUNK;
  if ((!ReadFile(*ph, buf, avail, &n, NULL)) || (n == 0))
    {
    CloseHandle(*ph);
    *ph = NULL;
    return 0;
    }
  if (send_frame(co, op, buf, (int) n))
    return -1;
  return (int) n;
}

static void run_on_pipes(t_conn *co, const char *line_in, int interactive)
{
  SECURITY_ATTRIBUTES sa;
  STARTUPINFOA si;
  PROCESS_INFORMATION pi;
  t_exec ex;
  HANDLE th;
  HANDLE in_rd = NULL, in_wr = NULL;
  HANDLE out_rd = NULL, out_wr = NULL, err_rd = NULL, err_wr = NULL;
  char line[8192 + 16];
  DWORD code = 0;

  snprintf(line, sizeof(line), "%s", line_in);
  memset(&sa, 0, sizeof(sa));
  sa.nLength = sizeof(sa);
  sa.bInheritHandle = TRUE;
  if ((!CreatePipe(&in_rd, &in_wr, &sa, 0)) ||
      (!CreatePipe(&out_rd, &out_wr, &sa, 0)) ||
      (!CreatePipe(&err_rd, &err_wr, &sa, 0)))
    {
    send_err(co, "CreatePipe: %s", last_error_txt());
    return;
    }
  /* Our ends stay ours: inherited, the child would hold them open and the
     drain below would never see the pipe end. */
  SetHandleInformation(in_wr, HANDLE_FLAG_INHERIT, 0);
  SetHandleInformation(out_rd, HANDLE_FLAG_INHERIT, 0);
  SetHandleInformation(err_rd, HANDLE_FLAG_INHERIT, 0);

  memset(&si, 0, sizeof(si));
  memset(&pi, 0, sizeof(pi));
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESTDHANDLES;
  si.hStdInput = in_rd;
  si.hStdOutput = out_wr;
  si.hStdError = err_wr;
  if (!CreateProcessA(NULL, line, NULL, NULL, TRUE, CREATE_NO_WINDOW,
                      NULL, NULL, &si, &pi))
    {
    send_err(co, "cannot run: %s", last_error_txt());
    CloseHandle(in_rd);  CloseHandle(in_wr);
    CloseHandle(out_rd); CloseHandle(out_wr);
    CloseHandle(err_rd); CloseHandle(err_wr);
    return;
    }
  CloseHandle(in_rd);
  CloseHandle(out_wr);
  CloseHandle(err_wr);

  memset(&ex, 0, sizeof(ex));
  ex.conn = co;
  ex.to_child = in_wr;
  ex.crlf = interactive;
  th = CreateThread(NULL, 0, exec_stdin_thread, &ex, 0, NULL);
  if (!th)
    {
    CloseHandle(in_wr);
    ex.to_child = NULL;
    }
  for (;;)
    {
    int nout = pipe_drain(co, &out_rd, OP_STDOUT);
    int nerr = pipe_drain(co, &err_rd, OP_STDERR);
    if ((nout < 0) || (nerr < 0))
      break;
    if ((!out_rd) && (!err_rd))
      break;
    if ((nout) || (nerr))
      continue;
    if (WaitForSingleObject(pi.hProcess, 0) == WAIT_OBJECT_0)
      {
      while ((pipe_drain(co, &out_rd, OP_STDOUT) > 0) ||
             (pipe_drain(co, &err_rd, OP_STDERR) > 0))
        ;
      break;
      }
    Sleep(EXEC_POLL_MS);
    }
  WaitForSingleObject(pi.hProcess, 3000);
  GetExitCodeProcess(pi.hProcess, &code);
  if (out_rd)
    CloseHandle(out_rd);
  if (err_rd)
    CloseHandle(err_rd);
  send_txt(co, OP_EXIT, "{\"code\":%d}", (int) code);
  if (th)
    {
    /* Releases the stdin thread, which is parked in recv. */
    shutdown(co->sock, SD_RECEIVE);
    WaitForSingleObject(th, INFINITE);
    CloseHandle(th);
    }
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);
  srv_log("pipes: %s ended, code %d", line, (int) code);
}

static void do_exec(t_conn *co, const char *body)
{
  char cmd[8192], line[8192 + 16];
  cxp_json_str(body, "cmd", cmd, sizeof(cmd));
  if (!cmd[0])
    {
    send_err(co, "exec with no command");
    return;
    }
  snprintf(line, sizeof(line), "cmd.exe /c %s", cmd);
  run_on_pipes(co, line, 0);
}

static void swallow_rest(t_conn *co)
{
  char *pl;
  int pl_len, op;
  for (;;)
    {
    op = recv_frame(co->sock, &pl, &pl_len);
    if (pl)
      free(pl);
    if ((op < 0) || (op == OP_END))
      return;
    }
}

static void do_put(t_conn *co, const char *body)
{
  char path[MAX_PATH_CXP], name[MAX_NAME_CXP], esc[2 * MAX_PATH_CXP];
  char joined[MAX_PATH_CXP];
  HANDLE out;
  DWORD written;

  cxp_json_str(body, "path", path, sizeof(path));
  cxp_json_str(body, "name", name, sizeof(name));
  expand_home(path, sizeof(path));
  path_norm(path);
  if (!path[0])
    {
    swallow_rest(co);
    send_err(co, "no path");
    return;
    }
  if (path_is_dir(path))
    {
    char *base;
    path_norm(name);
    base = strrchr(name, '\\');
    base = base ? base + 1 : name;
    if ((!base[0]) || (!strcmp(base, ".")) || (!strcmp(base, "..")))
      {
      swallow_rest(co);
      send_err(co, "%s is a directory and no usable file name was given",
               path);
      return;
      }
    path_join(joined, sizeof(joined), path, base);
    snprintf(path, sizeof(path), "%s", joined);
    }
  out = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                    FILE_ATTRIBUTE_NORMAL, NULL);
  if (out == INVALID_HANDLE_VALUE)
    {
    char *why = last_error_txt();
    swallow_rest(co);
    send_err(co, "%s: %s", path, why);
    return;
    }
  for (;;)
    {
    char *pl = NULL;
    int pl_len = 0;
    int op = recv_frame(co->sock, &pl, &pl_len);
    if (op < 0)
      {
      if (pl)
        free(pl);
      CloseHandle(out);
      return;
      }
    if (op == OP_DATA)
      {
      if ((pl_len) && (!WriteFile(out, pl, (DWORD) pl_len, &written, NULL)))
        {
        char *why = last_error_txt();
        free(pl);
        CloseHandle(out);
        swallow_rest(co);
        send_err(co, "write failed: %s", why);
        return;
        }
      }
    else if (op == OP_END)
      {
      if (pl)
        free(pl);
      break;
      }
    if (pl)
      free(pl);
    }
  CloseHandle(out);
  cxp_json_escape(path, esc, sizeof(esc));
  send_txt(co, OP_EXIT, "{\"code\":0,\"path\":\"%s\"}", esc);
}

static void do_get(t_conn *co, const char *body)
{
  char path[MAX_PATH_CXP], buf[CHUNK];
  HANDLE in;
  LARGE_INTEGER sz;
  DWORD n;

  cxp_json_str(body, "path", path, sizeof(path));
  expand_home(path, sizeof(path));
  path_norm(path);
  if (!path[0])
    {
    send_err(co, "no path");
    return;
    }
  if (path_is_dir(path))
    {
    send_err(co, "%s is a directory", path);
    return;
    }
  in = CreateFileA(path, GENERIC_READ,
                   FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                   NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (in == INVALID_HANDLE_VALUE)
    {
    send_err(co, "%s: %s", path, last_error_txt());
    return;
    }
  sz.QuadPart = 0;
  GetFileSizeEx(in, &sz);
  send_txt(co, OP_META, "{\"size\":%lld,\"mode\":%d}",
           (long long) sz.QuadPart, FILE_MODE);
  for (;;)
    {
    if (!ReadFile(in, buf, sizeof(buf), &n, NULL))
      break;
    if (n == 0)
      break;
    if (send_frame(co, OP_GDATA, buf, (int) n))
      {
      CloseHandle(in);
      return;
      }
    }
  CloseHandle(in);
  send_frame(co, OP_GEND, NULL, 0);
}

/* PUTTREE: a whole directory arrives, one ENTRY per member. */
static void do_put_tree(t_conn *co, const char *body)
{
  char root[MAX_PATH_CXP], name[MAX_NAME_CXP], type[8], link[MAX_NAME_CXP];
  char full[MAX_PATH_CXP];
  HANDLE cur = INVALID_HANDLE_VALUE;
  DWORD written;
  char *pl;
  int pl_len, op;

  cxp_json_str(body, "path", root, sizeof(root));
  expand_home(root, sizeof(root));
  path_norm(root);
  if (!root[0])
    {
    swallow_rest(co);
    send_err(co, "no path");
    return;
    }
  if (mkdir_p(root))
    {
    char *why = last_error_txt();
    swallow_rest(co);
    send_err(co, "%s: %s", root, why);
    return;
    }
  for (;;)
    {
    pl = NULL;
    pl_len = 0;
    op = recv_frame(co->sock, &pl, &pl_len);
    if (op < 0)
      break;
    if (op == OP_ENTRY)
      {
      if (cur != INVALID_HANDLE_VALUE)
        {
        CloseHandle(cur);
        cur = INVALID_HANDLE_VALUE;
        }
      cxp_json_str(pl, "name", name, sizeof(name));
      cxp_json_str(pl, "type", type, sizeof(type));
      if (cxp_safe_rel_path(name))
        {
        free(pl);
        swallow_rest(co);
        send_err(co, "refused member name");
        return;
        }
      path_join(full, sizeof(full), root, name);
      if (!strcmp(type, "d"))
        {
        if (mkdir_p(full))
          {
          char *why = last_error_txt();
          free(pl);
          swallow_rest(co);
          send_err(co, "%s: %s", full, why);
          return;
          }
        }
      else if (!strcmp(type, "l"))
        {
        /* Best effort: making one needs a privilege a guest may not have, and
           a tree that arrives without its symlinks is still the tree. */
        cxp_json_str(pl, "link", link, sizeof(link));
        mkdir_parent(full);
        DeleteFileA(full);
        CreateSymbolicLinkA(full, link, 0x2);
        }
      else
        {
        mkdir_parent(full);
        cur = CreateFileA(full, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                          FILE_ATTRIBUTE_NORMAL, NULL);
        if (cur == INVALID_HANDLE_VALUE)
          {
          char *why = last_error_txt();
          free(pl);
          swallow_rest(co);
          send_err(co, "%s: %s", full, why);
          return;
          }
        }
      }
    else if (op == OP_DATA)
      {
      if ((cur != INVALID_HANDLE_VALUE) && (pl_len) &&
          (!WriteFile(cur, pl, (DWORD) pl_len, &written, NULL)))
        {
        char *why = last_error_txt();
        free(pl);
        CloseHandle(cur);
        swallow_rest(co);
        send_err(co, "write failed: %s", why);
        return;
        }
      }
    else if (op == OP_END)
      {
      if (pl)
        free(pl);
      break;
      }
    if (pl)
      free(pl);
    }
  if (cur != INVALID_HANDLE_VALUE)
    CloseHandle(cur);
  send_txt(co, OP_EXIT, "{\"code\":0}");
}

static int send_one_entry(t_conn *co, const char *parent, const char *rel)
{
  char full[MAX_PATH_CXP], esc[2 * MAX_NAME_CXP], buf[CHUNK];
  WIN32_FIND_DATAA fd;
  HANDLE h;
  DWORD attr, n;
  LARGE_INTEGER sz;

  path_join(full, sizeof(full), parent, rel);
  attr = GetFileAttributesA(full);
  if (attr == INVALID_FILE_ATTRIBUTES)
    return 0;
  /* A junction or a symlink is not followed: a tree that points at itself
     would be walked for ever. */
  if (attr & FILE_ATTRIBUTE_REPARSE_POINT)
    return 0;
  cxp_json_escape(rel, esc, sizeof(esc));

  if (attr & FILE_ATTRIBUTE_DIRECTORY)
    {
    char pattern[MAX_PATH_CXP], child[MAX_NAME_CXP];
    if (send_txt(co, OP_GENTRY, "{\"name\":\"%s\",\"type\":\"d\","
                                "\"mode\":%d}", esc, DIR_MODE))
      return -1;
    path_join(pattern, sizeof(pattern), full, "*");
    h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE)
      return 0;
    do
      {
      if ((!strcmp(fd.cFileName, ".")) || (!strcmp(fd.cFileName, "..")))
        continue;
      snprintf(child, sizeof(child), "%.500s/%.500s", rel, fd.cFileName);
      if (send_one_entry(co, parent, child))
        {
        FindClose(h);
        return -1;
        }
      } while (FindNextFileA(h, &fd));
    FindClose(h);
    return 0;
    }

  h = CreateFileA(full, GENERIC_READ,
                  FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                  NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (h == INVALID_HANDLE_VALUE)
    return 0;
  sz.QuadPart = 0;
  GetFileSizeEx(h, &sz);
  if (send_txt(co, OP_GENTRY, "{\"name\":\"%s\",\"type\":\"f\","
                              "\"mode\":%d,\"size\":%lld}",
               esc, FILE_MODE, (long long) sz.QuadPart))
    {
    CloseHandle(h);
    return -1;
    }
  for (;;)
    {
    if (!ReadFile(h, buf, sizeof(buf), &n, NULL))
      break;
    if (n == 0)
      break;
    if (send_frame(co, OP_GDATA, buf, (int) n))
      {
      CloseHandle(h);
      return -1;
      }
    }
  CloseHandle(h);
  return 0;
}

static void do_get_tree(t_conn *co, const char *body)
{
  char path[MAX_PATH_CXP], parent[MAX_PATH_CXP], base[MAX_NAME_CXP];
  char *slash;
  int len;

  cxp_json_str(body, "path", path, sizeof(path));
  expand_home(path, sizeof(path));
  path_norm(path);
  if (!path[0])
    {
    send_err(co, "no path");
    return;
    }
  len = (int) strlen(path);
  while ((len > 1) && (path[len-1] == '\\'))
    path[--len] = 0;
  if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES)
    {
    send_err(co, "%s: %s", path, last_error_txt());
    return;
    }
  snprintf(parent, sizeof(parent), "%s", path);
  slash = strrchr(parent, '\\');
  if ((!slash) || (!slash[1]))
    {
    send_err(co, "%s has no parent to copy from", path);
    return;
    }
  *slash = 0;
  snprintf(base, sizeof(base), "%s", slash + 1);
  /* "C:\tmp" leaves "C:", which is the current directory of that drive and
     not its root. */
  if ((parent[0]) && (parent[1] == ':') && (!parent[2]))
    snprintf(parent, sizeof(parent), "%c:\\", parent[0]);
  if (send_one_entry(co, parent, base))
    return;
  send_frame(co, OP_GEND, NULL, 0);
}

static void do_list(t_conn *co, const char *body)
{
  char path[MAX_PATH_CXP], pattern[MAX_PATH_CXP], esc[2 * MAX_NAME_CXP];
  char pesc[2 * MAX_PATH_CXP];
  WIN32_FIND_DATAA fd;
  LARGE_INTEGER sz;
  HANDLE h;
  int len, rc;

  cxp_json_str(body, "path", path, sizeof(path));
  expand_home(path, sizeof(path));
  path_norm(path);
  if (!path[0])
    snprintf(path, sizeof(path), "C:\\");
  len = (int) strlen(path);
  while ((len > 3) && (path[len-1] == '\\'))
    path[--len] = 0;
  /* "C:" is the current directory of that drive and not its root. */
  if ((path[0]) && (path[1] == ':') && (!path[2]))
    snprintf(path, sizeof(path), "%c:\\", path[0]);
  path_join(pattern, sizeof(pattern), path, "*");
  h = FindFirstFileA(pattern, &fd);
  if (h == INVALID_HANDLE_VALUE)
    {
    send_err(co, "%s: %s", path, last_error_txt());
    return;
    }
  do
    {
    if ((!strcmp(fd.cFileName, ".")) || (!strcmp(fd.cFileName, "..")))
      continue;
    cxp_json_escape(fd.cFileName, esc, sizeof(esc));
    if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
      rc = send_txt(co, OP_GENTRY, "{\"name\":\"%s\",\"type\":\"d\","
                                   "\"mode\":%d}", esc, DIR_MODE);
    else
      {
      sz.LowPart = fd.nFileSizeLow;
      sz.HighPart = (LONG) fd.nFileSizeHigh;
      rc = send_txt(co, OP_GENTRY, "{\"name\":\"%s\",\"type\":\"f\","
                                   "\"mode\":%d,\"size\":%lld}",
                    esc, FILE_MODE, (long long) sz.QuadPart);
      }
    if (rc)
      {
      FindClose(h);
      return;
      }
    } while (FindNextFileA(h, &fd));
  FindClose(h);
  if (send_frame(co, OP_GEND, NULL, 0))
    return;
  cxp_json_escape(path, pesc, sizeof(pesc));
  send_txt(co, OP_EXIT, "{\"code\":0,\"path\":\"%s\"}", pesc);
}

static void serve(SOCKET s)
{
  t_conn co;
  char *pl = NULL;
  int pl_len = 0, op;

  co.sock = s;
  InitializeCriticalSection(&(co.tx));
  InterlockedIncrement(&g_busy);
  op = recv_frame(s, &pl, &pl_len);
  if (op < 0)
    srv_log("the leg closed before any frame arrived");
  else
    {
    switch (op)
      {
      case OP_PTY:     do_pty(&co, pl);      break;
      case OP_EXEC:    do_exec(&co, pl);     break;
      case OP_PUT:     do_put(&co, pl);      break;
      case OP_GET:     do_get(&co, pl);      break;
      case OP_PUTTREE: do_put_tree(&co, pl); break;
      case OP_GETTREE: do_get_tree(&co, pl); break;
      case OP_LIST:    do_list(&co, pl);     break;
      default:
        srv_log("unexpected first opcode 0x%02x", op);
        send_err(&co, "unexpected first opcode %d", op);
        break;
      }
    }
  if (pl)
    free(pl);
  closesocket(s);
  DeleteCriticalSection(&(co.tx));
  InterlockedDecrement(&g_busy);
}

/* Session 0 is where the launcher puts us, qemu-ga being a service; a
   pseudoconsole created there never attaches a shell. */
static void take_tcb_privilege(void)
{
  HANDLE tok;
  TOKEN_PRIVILEGES tp;
  if (!OpenProcessToken(GetCurrentProcess(),
                        TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &tok))
    return;
  if (LookupPrivilegeValue(NULL, SE_TCB_NAME, &(tp.Privileges[0].Luid)))
    {
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    AdjustTokenPrivileges(tok, FALSE, &tp, 0, NULL, NULL);
    }
  CloseHandle(tok);
}

static int relaunch_in_console_session(void)
{
  DWORD sid = WTSGetActiveConsoleSessionId();
  HANDLE tok = NULL, dup = NULL;
  void *env = NULL;
  STARTUPINFOA si;
  PROCESS_INFORMATION pi;
  char path[MAX_PATH_CXP], line[MAX_PATH_CXP + 64];
  int result = -1;

  if (sid == 0xFFFFFFFF)
    {
    srv_log("no console session to move to");
    return -1;
    }
  take_tcb_privilege();
  if (!WTSQueryUserToken(sid, &tok))
    return -1;
  if (!DuplicateTokenEx(tok, MAXIMUM_ALLOWED, NULL, SecurityImpersonation,
                        TokenPrimary, &dup))
    {
    srv_log("DuplicateTokenEx failed: %s", last_error_txt());
    CloseHandle(tok);
    return -1;
    }
  if (!GetModuleFileNameA(NULL, path, sizeof(path)))
    {
    srv_log("GetModuleFileName failed: %s", last_error_txt());
    CloseHandle(dup);
    CloseHandle(tok);
    return -1;
    }
  snprintf(line, sizeof(line), "\"%s\" -insession", path);
  CreateEnvironmentBlock(&env, dup, FALSE);
  memset(&si, 0, sizeof(si));
  memset(&pi, 0, sizeof(pi));
  si.cb = sizeof(si);
  si.lpDesktop = (char *) "winsta0\\default";
  if (CreateProcessAsUserA(dup, NULL, line, NULL, NULL, FALSE,
                           CREATE_UNICODE_ENVIRONMENT | CREATE_NO_WINDOW,
                           env, NULL, &si, &pi))
    {
    srv_log("a session is available now: relaunched in session %lu as pid "
            "%lu, this copy ends so the new one can take the port",
            (unsigned long) sid, (unsigned long) pi.dwProcessId);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    result = 0;
    }
  else
    srv_log("CreateProcessAsUser failed: %s", last_error_txt());
  if (env)
    DestroyEnvironmentBlock(env);
  CloseHandle(dup);
  CloseHandle(tok);
  return result;
}

#define HOP_POLL_MS 3000

static DWORD WINAPI hop_watcher(LPVOID arg)
{
  (void) arg;
  for (;;)
    {
    Sleep(HOP_POLL_MS);
    if (g_busy)
      continue;
    if (relaunch_in_console_session())
      continue;
    /* Not return: this ends the process, listener and all. */
    ExitProcess(0);
    }
  return 0;
}

static DWORD WINAPI serve_thread(LPVOID arg)
{
  serve((SOCKET) (ULONG_PTR) arg);
  return 0;
}

int main(int argc, char *argv[])
{
  WSADATA wsa;
  SOCKET srv, cli;
  struct sockaddr_in addr;
  int on = 1, i;
  DWORD sid = 0;

  for (i = 1; i < argc; i++)
    {
    if (!strcmp(argv[i], "-nopty"))
      g_no_pty = 1;
    else if (!strcmp(argv[i], "-pty"))
      g_force_pty = 1;
    else if (!strcmp(argv[i], "-insession"))
      g_in_session = 1;
    }
  log_open();
  if (!ProcessIdToSessionId(GetCurrentProcessId(), &sid))
    sid = (DWORD) -1;
  if ((sid == 0) && (!g_force_pty))
    g_no_pty = 1;
  {
  char exe[MAX_PATH];
  srv_log("======== cloonix windows cxp server ========");
  srv_log("built %s %s", __DATE__, __TIME__);
  if (GetModuleFileNameA(NULL, exe, sizeof(exe)))
    srv_log("running from %s", exe);
  srv_log("pid %lu", (unsigned long) GetCurrentProcessId());
  }
  srv_log("cloonix windows cxp server in session %lu, terminals %s",
          (unsigned long) sid,
          g_no_pty ? "on pipes" : "on a pseudoconsole");
  if (g_no_pty)
    srv_log("pipes: no escape sequences and nothing full screen, a DOS "
            "prompt and file transfer as usual. -pty forces the "
            "pseudoconsole back.");
  if ((pcon_api_load()) && (!g_no_pty))
    {
    srv_log("no pseudoconsole in this windows: 1809 or later is needed, "
            "or run this with -nopty for terminals on pipes");
    return 1;
    }
  if (WSAStartup(MAKEWORD(2,2), &wsa))
    {
    srv_log("WSAStartup failed");
    return 1;
    }
  srv = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (srv == INVALID_SOCKET)
    {
    srv_log("socket failed err=%d", WSAGetLastError());
    return 1;
    }
  /* NOT SO_REUSEADDR: on windows it lets a second process bind this same port
     and take connections from the first. */
  setsockopt(srv, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
             (const char *) &on, sizeof(on));
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(CXP_PORT);
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  for (i = 0; ; i++)
    {
    if (bind(srv, (struct sockaddr *) &addr, sizeof(addr)) != SOCKET_ERROR)
      break;
    if ((WSAGetLastError() == WSAEADDRINUSE) && (g_in_session) &&
        (i < BIND_TRIES))
      {
      Sleep(BIND_RETRY_MS);
      continue;
      }
    if (WSAGetLastError() == WSAEADDRINUSE)
      srv_log("another cxp server already has 127.0.0.1:%d, "
              "this one stops here", CXP_PORT);
    else
      srv_log("bind 127.0.0.1:%d failed err=%d", CXP_PORT, WSAGetLastError());
    return 1;
    }
  if (listen(srv, 8) == SOCKET_ERROR)
    {
    srv_log("listen failed err=%d", WSAGetLastError());
    return 1;
    }
  srv_log("listening on 127.0.0.1:%d", CXP_PORT);
  /* Only from session 0, and only when a pseudoconsole is what we would
     rather be serving. */
  if ((sid == 0) && (!g_in_session) && (!g_force_pty))
    {
    HANDLE wh;
    srv_log("watching for a logged-in session, to move there and serve a "
            "real terminal");
    wh = CreateThread(NULL, 0, hop_watcher, NULL, 0, NULL);
    if (wh)
      CloseHandle(wh);
    else
      srv_log("no watcher thread: this copy stays on pipes for good");
    }
  for (;;)
    {
    cli = accept(srv, NULL, NULL);
    if (cli == INVALID_SOCKET)
      {
      srv_log("accept failed err=%d", WSAGetLastError());
      Sleep(1000);
      continue;
      }
    /* A thread per connection. */
    {
    HANDLE th = CreateThread(NULL, 0, serve_thread,
                             (LPVOID) (ULONG_PTR) cli, 0, NULL);
    if (!th)
      {
      srv_log("no thread for the leg: %s", last_error_txt());
      closesocket(cli);
      }
    else
      CloseHandle(th);
    }
    }
}
