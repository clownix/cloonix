/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
/* cloonix_agent_win.c - the insider agent for a windows guest. */
/* What it does: opens the virtio-serial port and answers the ping doorways */
/* sends. That is the whole of it, and */
/* it is enough for the node to go green and for the machine to be reachable */
/* the way a linux one is at the same stage. */
/* What it does NOT do, and what the linux agent does: byte pipes */
/* (header_type_traffic and the dido_llid contexts), which is what */
/* cloonix_cxp_srv is joined to and what "cloonix cli ssh" rides on. Those */
/* need a second server process inside the guest. Requests for a pipe are */
/* refused here, in one line, rather than ignored: an unanswered leg request */
/* is a client hanging with nothing said. */
/* Why one thread and blocking reads: there is one handle. select() does not */
/* take a windows HANDLE, so the linux shape (select over the port and the */
/* pipe sockets) does not carry over -- but with no pipes there is nothing */
/* to select between. When the pipes arrive this becomes overlapped I/O or a */
/* reader thread, and that is the moment to change it, not before. */
/* The port name is what qemu was told in QEMU_OPTS_CLOON: */
/* -device virtserialport,chardev=cloon0,name=net.cloon.0 */
/* which the vioserial driver publishes as \\.\Global\net.cloon.0 */
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kerr_gate.h"

#include "glob_wire.h"
#include "agent_wire.h"

#define VIRTIOPORT_WIN "\\\\.\\Global\\net.cloon.0"

#define CXP_PORT 4441

#define MAX_LEGS 30

#define OPEN_RETRY_MS 2000

/* The wait comes back this often so the heartbeat can be written, and the
   heartbeat is written at most this often. */
#define WAIT_BEAT_MS  5000
#define BEAT_LOG_MS  60000

/* Long enough for a cxp server to bind, short enough that a client asking for
   a console is not left wondering. */
#define CXP_START_TRIES 10
#define CXP_START_WAIT_MS 500

static HANDLE g_port = INVALID_HANDLE_VALUE;
static FILE  *g_log;
static int    g_dbg = 1;

static int    g_frame_in;
static int    g_frame_out;
static int    g_ping_in;
static int    g_ping_out;
static long long g_byte_in;
static long long g_byte_out;
static DWORD  g_tick_start;
static DWORD  g_tick_last_rx;
static DWORD  g_tick_last_beat;

/* One leg per dido_llid: the byte pipe of a console or a file transfer. */
typedef struct t_leg
{
  int    dido_llid;
  SOCKET sock;
  WSAEVENT evt;
  int    used;
  int    said_out;
  int    said_in;
  /* the host said xoff: the leg is not read until xon */
  int    host_xoff;
} t_leg;

static t_leg g_leg[MAX_LEGS];

static OVERLAPPED g_port_ovl;
static char  g_port_buf[4096];
static int   g_port_pending;

/* 1 when the line passed its gate and was written */
static int agent_vlog(const char *fmt, va_list ap)
{
  static t_kerr_gate tab[KERR_GATE_FMT_MAX];
  va_list ap2;
  SYSTEMTIME st;
  char stamp[40], note[64];
  if (!kerr_gate(kerr_gate_of_fmt(tab, fmt), note, sizeof(note)))
    return 0;
  GetLocalTime(&st);
  snprintf(stamp, sizeof(stamp), "%04d-%02d-%02d %02d:%02d:%02d.%03d ",
           st.wYear, st.wMonth, st.wDay,
           st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
  /* The console too, not the file alone. */
  fputs(stamp, stdout);
  va_copy(ap2, ap);
  vfprintf(stdout, fmt, ap2);
  va_end(ap2);
  fputs(note, stdout);
  fputs("\n", stdout);
  fflush(stdout);
  if (!g_log)
    return 1;
  fputs(stamp, g_log);
  va_copy(ap2, ap);
  vfprintf(g_log, fmt, ap2);
  va_end(ap2);
  fputs(note, g_log);
  fputs("\n", g_log);
  fflush(g_log);
  return 1;
}

static void agent_log(const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  agent_vlog(fmt, ap);
  va_end(ap);
}

static int send_to_virtio(int dido_llid, int type, int val, int len,
                          char *buf);

/* A fault: in the log of the guest, and to the host for the syslog of the
   net while the port is open. */
static void agent_kerr(const char *fmt, ...)
{
  static int in_forward;
  char buf[1200];
  int len, headsize = sock_header_get_size();
  va_list ap, ap2;
  va_start(ap, fmt);
  va_copy(ap2, ap);
  if (agent_vlog(fmt, ap) && (g_port != INVALID_HANDLE_VALUE) && (!in_forward))
    {
    in_forward = 1;
    len = snprintf(buf + headsize, sizeof(buf) - headsize, "WARNING ");
    len += vsnprintf(buf + headsize + len, sizeof(buf) - headsize - len,
                     fmt, ap2);
    if (len > (int) sizeof(buf) - headsize - 1)
      len = sizeof(buf) - headsize - 1;
    send_to_virtio(0, header_type_kerr, header_val_none, len + 1, buf);
    in_forward = 0;
    }
  va_end(ap2);
  va_end(ap);
}

static void agent_dbg(const char *fmt, ...)
{
  va_list ap;
  if (!g_dbg)
    return;
  va_start(ap, fmt);
  agent_vlog(fmt, ap);
  va_end(ap);
}

static const char *type_txt(int type)
{
  switch (type)
    {
    case header_type_traffic:    return "traffic";
    case header_type_vwifi_cli:  return "vwifi_cli";
    case header_type_vwifi_spy:  return "vwifi_spy";
    case header_type_vwifi_ctr:  return "vwifi_ctr";
    case header_type_ctrl:       return "ctrl";
    case header_type_ctrl_agent: return "ctrl_agent";
    case header_type_kerr:       return "kerr";
    }
  return "type?";
}

static const char *val_txt(int val)
{
  switch (val)
    {
    case header_val_none:          return "none";
    case header_val_add_dido_llid: return "add_leg";
    case header_val_del_dido_llid: return "del_leg";
    case header_val_ping:          return "ping";
    case header_val_ack:           return "ack";
    case header_val_reboot:        return "reboot";
    case header_val_halt:          return "halt";
    case header_val_clip_set:      return "clip_set";
    case header_val_clip_get:      return "clip_get";
    case header_val_clip_txt:      return "clip_txt";
    case header_val_xoff:          return "xoff";
    case header_val_xon:           return "xon";
    }
  return "val?";
}

/* C:\cloonix-agent.log first, the temp directory only if that cannot be
   written. */
static void log_open(void)
{
  char path[MAX_PATH];
  char prev[MAX_PATH];
  DWORD len;
  MoveFileExA("C:\\cloonix-agent.log", "C:\\cloonix-agent.log.prev",
              MOVEFILE_REPLACE_EXISTING);
  g_log = fopen("C:\\cloonix-agent.log", "w");
  if (g_log)
    return;
  len = GetTempPathA(MAX_PATH, path);
  if ((len == 0) || (len > MAX_PATH - 32))
    return;
  snprintf(prev, sizeof(prev), "%scloonix-agent.log.prev", path);
  strcat(path, "cloonix-agent.log");
  MoveFileExA(path, prev, MOVEFILE_REPLACE_EXISTING);
  g_log = fopen(path, "w");
}

static int port_write_all(const char *buf, int len)
{
  DWORD done = 0, n;
  OVERLAPPED ovl;
  /* The handle is overlapped, so the write is too and is waited out here:
     the caller wants the bytes gone before it returns. */
  while ((int) done < len)
    {
    memset(&ovl, 0, sizeof(ovl));
    ovl.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!ovl.hEvent)
      {
      agent_kerr("write event failed err=%lu", (unsigned long) GetLastError());
      return -1;
      }
    n = 0;
    if (!WriteFile(g_port, buf + done, (DWORD)(len - done), &n, &ovl))
      {
      if (GetLastError() != ERROR_IO_PENDING)
        {
        agent_kerr("write failed err=%lu", (unsigned long) GetLastError());
        CloseHandle(ovl.hEvent);
        return -1;
        }
      if (!GetOverlappedResult(g_port, &ovl, &n, TRUE))
        {
        agent_kerr("write result failed err=%lu",
                  (unsigned long) GetLastError());
        CloseHandle(ovl.hEvent);
        return -1;
        }
      }
    CloseHandle(ovl.hEvent);
    if (n == 0)
      {
      agent_kerr("write returned zero");
      return -1;
      }
    done += n;
    }
  return 0;
}

static int send_to_virtio(int dido_llid, int type, int val, int len, char *buf)
{
  char *payload;
  int headsize = sock_header_get_size();
  if (len > MAX_A2D_LEN - headsize)
    {
    agent_kerr("payload too big %d", len);
    return -1;
    }
  /* Never zero: rx_pktbuf_fill of doorways ends on KOUT for a header that
     promises no payload, so an empty message here kills the host side. */
  if (len <= 0)
    {
    agent_kerr("refusing to send an empty message, type=%d val=%d", type, val);
    return -1;
    }
  sock_header_set_info(buf, dido_llid, 0, 0, type, val, len, &payload);
  g_frame_out += 1;
  g_byte_out += len + headsize;
  if (val == header_val_ping)
    g_ping_out += 1;
  agent_dbg("TX leg=%d %s/%s len=%d [%.60s]", dido_llid, type_txt(type),
            val_txt(val), len,
            (type == header_type_traffic) ? "traffic" : payload);
  return port_write_all(buf, len + headsize);
}

/* The ping is echoed whole: doorways matches the id it sent against the id it
   gets back, so the payload must return unchanged. */
static void answer_ping(int len, char *rx)
{
  char buf[MAX_A2D_LEN];
  int headsize = sock_header_get_size();
  int val_id;
  if (sscanf(rx, LAPING, &val_id) != 1)
    {
    agent_kerr("ping payload not understood: %s", rx);
    return;
    }
  g_ping_in += 1;
  /* The first one, out loud: it is the moment the node goes green, and the
     one line that separates "never reached" from "reached and refused". */
  if (g_ping_in == 1)
    agent_log("FIRST PING from the host, id=%d, answering it", val_id);
  memcpy(buf + headsize, rx, len);
  send_to_virtio(0, header_type_ctrl, header_val_ping, len, buf);
}

/* InitiateSystemShutdownEx wants SE_SHUTDOWN_NAME, which a service running as
   SYSTEM has and a plain user does not. */
static int take_shutdown_privilege(void)
{
  HANDLE tok;
  TOKEN_PRIVILEGES tp;
  int result = -1;
  if (!OpenProcessToken(GetCurrentProcess(),
                        TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &tok))
    return -1;
  if (LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME, &(tp.Privileges[0].Luid)))
    {
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    AdjustTokenPrivileges(tok, FALSE, &tp, 0, NULL, NULL);
    if (GetLastError() == ERROR_SUCCESS)
      result = 0;
    }
  CloseHandle(tok);
  return result;
}

static void do_shutdown(int reboot_wanted)
{
  if (take_shutdown_privilege())
    agent_kerr("no shutdown privilege, running as a service is what gives it");
  if (!InitiateSystemShutdownExA(NULL, NULL, 0, TRUE,
                                 reboot_wanted ? TRUE : FALSE,
                                 SHTDN_REASON_MAJOR_OTHER |
                                 SHTDN_REASON_MINOR_OTHER |
                                 SHTDN_REASON_FLAG_PLANNED))
    agent_kerr("shutdown refused err=%lu", (unsigned long) GetLastError());
}

#define CLIP_OPEN_TRIES 10

static int clip_open(void)
{
  int i;
  for (i = 0; i < CLIP_OPEN_TRIES; i++)
    {
    if (OpenClipboard(NULL))
      return 0;
    Sleep(20);
    }
  agent_kerr("clipboard busy, err=%lu", (unsigned long) GetLastError());
  return -1;
}

static void clip_set(int len, char *utf8)
{
  int wlen;
  HGLOBAL mem;
  WCHAR *dst;
  agent_log("clip: host -> guest, %d bytes", len);
  if (len <= 0)
    return;
  utf8[len-1] = 0;
  wlen = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
  if (wlen <= 0)
    {
    agent_kerr("clipboard text is not utf-8, err=%lu",
              (unsigned long) GetLastError());
    return;
    }
  if (clip_open())
    return;
  /* The clipboard owns the handle after SetClipboardData, so it is neither
     freed nor locked here afterwards. */
  mem = GlobalAlloc(GMEM_MOVEABLE, wlen * sizeof(WCHAR));
  if (!mem)
    {
    CloseClipboard();
    return;
    }
  dst = (WCHAR *) GlobalLock(mem);
  if (!dst)
    {
    GlobalFree(mem);
    CloseClipboard();
    return;
    }
  MultiByteToWideChar(CP_UTF8, 0, utf8, -1, dst, wlen);
  GlobalUnlock(mem);
  EmptyClipboard();
  if (!SetClipboardData(CF_UNICODETEXT, mem))
    {
    agent_kerr("SetClipboardData failed err=%lu",
              (unsigned long) GetLastError());
    GlobalFree(mem);
    }
  else
    agent_log("clip: host -> guest done, %d wide chars on the clipboard",
              wlen);
  CloseClipboard();
}

static void clip_get(void)
{
  char buf[MAX_A2D_LEN];
  int headsize = sock_header_get_size();
  char *payload = buf + headsize;
  HANDLE h;
  WCHAR *src;
  int len = 0;

  payload[0] = ' ';
  payload[1] = 0;
  if (!clip_open())
    {
    h = GetClipboardData(CF_UNICODETEXT);
    if (h)
      {
      src = (WCHAR *) GlobalLock(h);
      if (src)
        {
        len = WideCharToMultiByte(CP_UTF8, 0, src, -1, payload,
                                  MAX_A2D_LEN - headsize - 1, NULL, NULL);
        if (len <= 0)
          {
          agent_kerr("clipboard too big or not convertible, err=%lu",
                    (unsigned long) GetLastError());
          payload[0] = ' ';
          payload[1] = 0;
          }
        GlobalUnlock(h);
        }
      }
    CloseClipboard();
    }
  agent_log("clip: guest -> host, %d bytes%s", (int) strlen(payload),
            (len <= 0) ? " (nothing text shaped on the clipboard)" : "");
  send_to_virtio(0, header_type_ctrl, header_val_clip_txt,
                 (int) strlen(payload) + 1, buf);
}

static t_leg *leg_get(int dido_llid)
{
  int i;
  for (i = 0; i < MAX_LEGS; i++)
    {
    if ((g_leg[i].used) && (g_leg[i].dido_llid == dido_llid))
      return &(g_leg[i]);
    }
  return NULL;
}

static void leg_free(t_leg *leg, int tell_host)
{
  char buf[MAX_A2D_LEN];
  int headsize = sock_header_get_size();
  if (!leg->used)
    return;
  if (tell_host)
    {
    memcpy(buf + headsize, LABREAK, strlen(LABREAK) + 1);
    send_to_virtio(leg->dido_llid, header_type_ctrl_agent,
                   header_val_del_dido_llid, (int) strlen(LABREAK) + 1, buf);
    }
  agent_log("leg %d closed", leg->dido_llid);
  if (leg->evt != WSA_INVALID_EVENT)
    WSACloseEvent(leg->evt);
  closesocket(leg->sock);
  memset(leg, 0, sizeof(t_leg));
  leg->evt = WSA_INVALID_EVENT;
}

static void cxp_server_start(void)
{
  char path[MAX_PATH], *slash;
  STARTUPINFOA si;
  PROCESS_INFORMATION pi;
  if (!GetModuleFileNameA(NULL, path, sizeof(path)))
    return;
  slash = strrchr(path, '\\');
  if (!slash)
    return;
  snprintf(slash + 1, sizeof(path) - (slash + 1 - path),
           "cloonix-cxp-srv.exe");
  memset(&si, 0, sizeof(si));
  memset(&pi, 0, sizeof(pi));
  si.cb = sizeof(si);
  if (!CreateProcessA(path, NULL, NULL, NULL, FALSE,
                      CREATE_NO_WINDOW | DETACHED_PROCESS,
                      NULL, NULL, &si, &pi))
    {
    agent_kerr("cannot start %s, err=%lu", path, (unsigned long)GetLastError());
    return;
    }
  agent_log("started %s as pid %lu", path, (unsigned long) pi.dwProcessId);
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);
}

static t_leg *leg_alloc(int dido_llid)
{
  int i;
  SOCKET s;
  struct sockaddr_in addr;
  t_leg *leg = NULL;
  for (i = 0; i < MAX_LEGS; i++)
    {
    if (!g_leg[i].used)
      {
      leg = &(g_leg[i]);
      break;
      }
    }
  if (!leg)
    {
    agent_kerr("no room for another leg, %d already open", MAX_LEGS);
    return NULL;
    }
  s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (s == INVALID_SOCKET)
    {
    agent_kerr("socket failed err=%d", WSAGetLastError());
    return NULL;
    }
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(CXP_PORT);
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (connect(s, (struct sockaddr *) &addr, sizeof(addr)) == SOCKET_ERROR)
    {
    int try;
    agent_log("no cxp server on 127.0.0.1:%d, starting one", CXP_PORT);
    closesocket(s);
    cxp_server_start();
    for (try = 0; try < CXP_START_TRIES; try++)
      {
      Sleep(CXP_START_WAIT_MS);
      s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
      if (s == INVALID_SOCKET)
        return NULL;
      if (connect(s, (struct sockaddr *) &addr, sizeof(addr)) != SOCKET_ERROR)
        break;
      closesocket(s);
      s = INVALID_SOCKET;
      }
    if (s == INVALID_SOCKET)
      {
      agent_kerr("still no cxp server after starting one");
      return NULL;
      }
    }
  leg->evt = WSACreateEvent();
  if (leg->evt == WSA_INVALID_EVENT)
    {
    agent_kerr("WSACreateEvent failed err=%d", WSAGetLastError());
    closesocket(s);
    return NULL;
    }
  /* Readable and closed both wake the wait; the socket goes non-blocking as a
     side effect of this call, which is what we want for the relay. */
  if (WSAEventSelect(s, leg->evt, FD_READ | FD_CLOSE) == SOCKET_ERROR)
    {
    agent_kerr("WSAEventSelect failed err=%d", WSAGetLastError());
    WSACloseEvent(leg->evt);
    closesocket(s);
    return NULL;
    }
  leg->dido_llid = dido_llid;
  leg->sock = s;
  leg->used = 1;
  agent_log("leg %d open to the pty server", dido_llid);
  return leg;
}

/* Bytes from the pty server towards the host. */
static void leg_to_virtio(t_leg *leg)
{
  char buf[MAX_A2D_LEN];
  int headsize = sock_header_get_size();
  int len;
  for (;;)
    {
    len = recv(leg->sock, buf + headsize, MAX_A2D_LEN - headsize - 1, 0);
    if (len > 0)
      {
      if (!leg->said_out)
        {
        leg->said_out = 1;
        agent_log("leg %d: first %d bytes from the pty server to the host",
                  leg->dido_llid, len);
        }
      send_to_virtio(leg->dido_llid, header_type_traffic,
                     header_val_none, len, buf);
      }
    else if (len == 0)
      {
      leg_free(leg, 1);
      return;
      }
    else
      {
      if (WSAGetLastError() != WSAEWOULDBLOCK)
        {
        agent_kerr("leg %d recv err=%d", leg->dido_llid, WSAGetLastError());
        leg_free(leg, 1);
        }
      return;
      }
    }
}

/* Bytes from the host towards the pty server. */
static void virtio_to_leg(t_leg *leg, int len, char *payload)
{
  int done = 0, n;
  if (!leg->said_in)
    {
    leg->said_in = 1;
    agent_log("leg %d: first %d bytes from the host to the pty server",
              leg->dido_llid, len);
    }
  while (done < len)
    {
    n = send(leg->sock, payload + done, len - done, 0);
    if (n > 0)
      done += n;
    else
      {
      if (WSAGetLastError() == WSAEWOULDBLOCK)
        {
        Sleep(1);
        continue;
        }
      agent_kerr("leg %d send err=%d", leg->dido_llid, WSAGetLastError());
      leg_free(leg, 1);
      return;
      }
    }
}

static void accept_pipe(int dido_llid)
{
  char buf[MAX_A2D_LEN];
  int headsize = sock_header_get_size();
  char *payload = buf + headsize;
  snprintf(payload, MAX_A2D_LEN - headsize - 1, DBSSH_SERV_DOORS_RESP);
  send_to_virtio(dido_llid, header_type_ctrl_agent,
                 header_val_add_dido_llid, (int) strlen(payload) + 1, buf);
}

static void refuse_pipe(int dido_llid)
{
  char buf[MAX_A2D_LEN];
  int headsize = sock_header_get_size();
  char *payload = buf + headsize;
  agent_kerr("byte pipe %d refused: nothing answered on 127.0.0.1:%d",
            dido_llid, CXP_PORT);
  snprintf(payload, MAX_A2D_LEN - headsize - 1,
           "NO_CXP_SERVER cloonix-cxp-srv.exe is not listening in this guest");
  send_to_virtio(dido_llid, header_type_ctrl_agent,
                 header_val_add_dido_llid, (int) strlen(payload) + 1, buf);
}

static void dispatch(int dido_llid, int type, int val, int len, char *rx)
{
  if (dido_llid)
    {
    t_leg *leg = leg_get(dido_llid);
    if (!leg)
      {
      if ((type == header_type_ctrl_agent) &&
          (val == header_val_add_dido_llid))
        {
        leg = leg_alloc(dido_llid);
        if (!leg)
          refuse_pipe(dido_llid);
        else
          accept_pipe(dido_llid);
        }
      return;
      }
    if (type == header_type_traffic)
      {
      if (val == header_val_none)
        virtio_to_leg(leg, len, rx);
      else
        agent_kerr("leg %d: traffic val %d not handled", dido_llid, val);
      }
    else if (type == header_type_ctrl_agent)
      {
      if (val == header_val_del_dido_llid)
        leg_free(leg, 0);
      else if ((val == header_val_xoff) || (val == header_val_xon))
        leg->host_xoff = (val == header_val_xoff);
      else if (val != header_val_ack)
        agent_kerr("leg %d: ctrl_agent val %d not handled", dido_llid, val);
      }
    else
      agent_kerr("leg %d: type %d val %d not handled", dido_llid, type, val);
    return;
    }
  if (type == header_type_ctrl)
    {
    int val_id;
    switch (val)
      {
      case header_val_ping:
        answer_ping(len, rx);
        break;
      case header_val_reboot:
        if (sscanf(rx, LABOOT, &val_id) == 1)
          do_shutdown(1);
        break;
      case header_val_halt:
        if (sscanf(rx, LAHALT, &val_id) == 1)
          do_shutdown(0);
        break;
      case header_val_clip_set:
        clip_set(len, rx);
        break;
      case header_val_clip_get:
        clip_get();
        break;
      default:
        agent_kerr("ctrl val %d not handled", val);
        break;
      }
    }
  else
    agent_kerr("type %d val %d not handled", type, val);
}

/* Reassembly, the same shape as the linux agent: a read gives whatever the
   port has, which is not a message. */
typedef struct t_rx_state
{
  char rawbuf[MAX_A2D_LEN];
  int  offset;
  int  paylen;
  int  dido_llid;
  int  vwifi_base;
  int  vwifi_cid;
  int  type;
  int  val;
  char *payload;
} t_rx_state;

static void rx_reset(t_rx_state *rx)
{
  memset(rx, 0, sizeof(t_rx_state));
}

static int rx_consume(t_rx_state *rx, char *buf, int len)
{
  int headsize = sock_header_get_size();
  int taken, want;

  while (len > 0)
    {
    if (rx->offset < headsize)
      want = headsize - rx->offset;
    else
      want = headsize + rx->paylen - rx->offset;
    taken = (len < want) ? len : want;
    if (rx->offset + taken > MAX_A2D_LEN)
      {
      agent_kerr("oversized message, resynchronising");
      rx_reset(rx);
      return -1;
      }
    memcpy(rx->rawbuf + rx->offset, buf, taken);
    rx->offset += taken;
    buf += taken;
    len -= taken;

    if (rx->offset == headsize)
      {
      if (sock_header_get_info(rx->rawbuf, &(rx->dido_llid),
                               &(rx->vwifi_base), &(rx->vwifi_cid),
                               &(rx->type), &(rx->val),
                               &(rx->paylen), &(rx->payload)))
        {
        agent_kerr("not in sync, header magic lost");
        rx_reset(rx);
        return -1;
        }
      if ((rx->paylen < 0) || (rx->paylen > MAX_A2D_LEN - headsize))
        {
        agent_kerr("declared payload %d out of range", rx->paylen);
        rx_reset(rx);
        return -1;
        }
      if (rx->paylen == 0)
        {
        g_frame_in += 1;
        g_tick_last_rx = GetTickCount();
        agent_dbg("RX leg=%d %s/%s len=0", rx->dido_llid,
                  type_txt(rx->type), val_txt(rx->val));
        dispatch(rx->dido_llid, rx->type, rx->val, 0, rx->payload);
        rx_reset(rx);
        }
      }
    else if ((rx->offset > headsize) &&
             (rx->offset == headsize + rx->paylen))
      {
      rx->rawbuf[MAX_A2D_LEN-1] = 0;
      g_frame_in += 1;
      g_tick_last_rx = GetTickCount();
      agent_dbg("RX leg=%d %s/%s len=%d [%.60s]", rx->dido_llid,
                type_txt(rx->type), val_txt(rx->val), rx->paylen,
                (rx->type == header_type_traffic) ? "traffic" : rx->payload);
      dispatch(rx->dido_llid, rx->type, rx->val, rx->paylen, rx->payload);
      rx_reset(rx);
      }
    }
  return 0;
}

/* The port is not there while the guest boots and goes when qemu stops, so
   both are waited on rather than treated as failures. */
static int port_open(void)
{
  int tries = 0;
  DWORD err;
  for (;;)
    {
    /* FILE_FLAG_OVERLAPPED, and it is not optional. */
    g_port = CreateFileA(VIRTIOPORT_WIN, GENERIC_READ | GENERIC_WRITE,
                         0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
    if (g_port != INVALID_HANDLE_VALUE)
      return 0;
    err = GetLastError();
    agent_dbg("open %s try %d, err=%lu", VIRTIOPORT_WIN, tries,
              (unsigned long) err);
    if ((tries % 15) == 0)
      {
      char *why = "";
      /* The port is opened with no sharing, so a second copy of this agent
         gets denied rather than a port that is not there. */
      if (err == ERROR_FILE_NOT_FOUND)
        why = " (no such port: vioserial missing, or the machine has no"
              " cloonix channels)";
      agent_log("waiting for %s, err=%lu%s", VIRTIOPORT_WIN,
                (unsigned long) err, why);
      }
    if (err == ERROR_ACCESS_DENIED)
      {
      agent_log("another agent holds the port, this one stops here");
      return -1;
      }
    tries += 1;
    Sleep(OPEN_RETRY_MS);
    }
}

static int port_read_arm(void)
{
  DWORD n;
  if (g_port_pending)
    return 0;
  memset(&g_port_ovl, 0, sizeof(g_port_ovl));
  g_port_ovl.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
  if (!g_port_ovl.hEvent)
    {
    agent_kerr("CreateEvent failed err=%lu", (unsigned long) GetLastError());
    return -1;
    }
  if (ReadFile(g_port, g_port_buf, sizeof(g_port_buf), &n, &g_port_ovl))
    {
    g_port_pending = 1;
    return 0;
    }
  if (GetLastError() == ERROR_IO_PENDING)
    {
    g_port_pending = 1;
    return 0;
    }
  agent_kerr("ReadFile failed err=%lu", (unsigned long) GetLastError());
  CloseHandle(g_port_ovl.hEvent);
  g_port_ovl.hEvent = NULL;
  return -1;
}

static void port_read_done(t_rx_state *rx)
{
  DWORD n = 0;
  if (!GetOverlappedResult(g_port, &g_port_ovl, &n, FALSE))
    {
    agent_kerr("GetOverlappedResult err=%lu", (unsigned long) GetLastError());
    n = 0;
    }
  CloseHandle(g_port_ovl.hEvent);
  g_port_ovl.hEvent = NULL;
  g_port_pending = 0;
  agent_dbg("port read gave %lu bytes", (unsigned long) n);
  if (n > 0)
    {
    g_byte_in += n;
    rx_consume(rx, g_port_buf, (int) n);
    }
}

static void heartbeat_log(void)
{
  DWORD now = GetTickCount();
  int nb_legs = 0, i;
  if ((now - g_tick_last_beat) < BEAT_LOG_MS)
    return;
  g_tick_last_beat = now;
  for (i = 0; i < MAX_LEGS; i++)
    if (g_leg[i].used)
      nb_legs += 1;
  agent_log("alive %lus: in %d frames (%d ping) %lld bytes, "
            "out %d frames (%d ping) %lld bytes, %d legs",
            (unsigned long) ((now - g_tick_start) / 1000),
            g_frame_in, g_ping_in, g_byte_in,
            g_frame_out, g_ping_out, g_byte_out, nb_legs);
  if (!g_frame_in)
    agent_log("NOTHING HAS EVER ARRIVED on %s. The host sends a ping every "
              "beat, so this is not the agent: either the machine was made "
              "without the cloonix channel (qemu wants -device "
              "virtserialport,name=net.cloon.0) or doorways on the host has "
              "not marked this guest up and running.", VIRTIOPORT_WIN);
  else
    agent_log("last frame in %lu s ago",
              (unsigned long) ((now - g_tick_last_rx) / 1000));
}

static int wait_and_switch(t_rx_state *rx)
{
  HANDLE h[MAX_LEGS + 1];
  t_leg *owner[MAX_LEGS + 1];
  DWORD nb = 0, ret;
  int i;

  if (port_read_arm())
    return -1;
  h[nb] = g_port_ovl.hEvent;
  owner[nb] = NULL;
  nb += 1;
  for (i = 0; i < MAX_LEGS; i++)
    {
    if ((g_leg[i].used) && (!g_leg[i].host_xoff))
      {
      h[nb] = (HANDLE) g_leg[i].evt;
      owner[nb] = &(g_leg[i]);
      nb += 1;
      }
    }
  /* Not INFINITE: the wait must come back for the heartbeat to be written. */
  ret = WaitForMultipleObjects(nb, h, FALSE, WAIT_BEAT_MS);
  if (ret == WAIT_TIMEOUT)
    {
    heartbeat_log();
    return 0;
    }
  if (ret == WAIT_FAILED)
    {
    agent_kerr("wait failed err=%lu", (unsigned long) GetLastError());
    return -1;
    }
  i = (int) (ret - WAIT_OBJECT_0);
  if ((i < 0) || (i >= (int) nb))
    {
    agent_kerr("wait returned %lu, out of range", (unsigned long) ret);
    return -1;
    }
  if (owner[i] == NULL)
    port_read_done(rx);
  else
    {
    WSANETWORKEVENTS ev;
    t_leg *leg = owner[i];
    if (WSAEnumNetworkEvents(leg->sock, leg->evt, &ev) == SOCKET_ERROR)
      {
      agent_kerr("leg %d WSAEnumNetworkEvents err=%d",
                leg->dido_llid, WSAGetLastError());
      leg_free(leg, 1);
      }
    else if (ev.lNetworkEvents & (FD_READ | FD_CLOSE))
      leg_to_virtio(leg);
    }
  return 0;
}

static void legs_close_all(void)
{
  int i;
  for (i = 0; i < MAX_LEGS; i++)
    {
    if (g_leg[i].used)
      leg_free(&(g_leg[i]), 0);
    }
}

int main(int argc, char *argv[])
{
  static t_rx_state rx;
  WSADATA wsa;
  DWORD sid = 0;
  char exe[MAX_PATH];
  int i;

  (void) argc;
  (void) argv;
  log_open();
  if (getenv("CLOONIX_AGENT_QUIET"))
    g_dbg = 0;
  g_tick_start = GetTickCount();
  g_tick_last_rx = g_tick_start;
  g_tick_last_beat = g_tick_start;
  if (!ProcessIdToSessionId(GetCurrentProcessId(), &sid))
    sid = (DWORD) -1;
  agent_log("======== cloonix windows agent ========");
  agent_log("built %s %s", __DATE__, __TIME__);
  if (GetModuleFileNameA(NULL, exe, sizeof(exe)))
    agent_log("running from %s", exe);
  else
    agent_kerr("running from an unknown path, err=%lu",
              (unsigned long) GetLastError());
  agent_log("pid %lu, session %lu", (unsigned long) GetCurrentProcessId(),
            (unsigned long) sid);
  agent_log("the port wanted is %s", VIRTIOPORT_WIN);
  agent_log("frame logging is %s", g_dbg ? "ON" : "OFF (CLOONIX_AGENT_QUIET)");
  if (sid == 0)
    agent_log("session 0 is a service session: its clipboard is the one of "
              "the service window station, NOT the one the person at the "
              "desktop is using, so clipboard traffic here goes nowhere "
              "visible");
  if (WSAStartup(MAKEWORD(2,2), &wsa))
    {
    agent_kerr("WSAStartup failed, no byte pipes will be possible");
    }
  for (i = 0; i < MAX_LEGS; i++)
    g_leg[i].evt = WSA_INVALID_EVENT;
  for (;;)
    {
    if (port_open())
      break;
    agent_log("port %s OPEN, waiting for the host to ping",
              VIRTIOPORT_WIN);
    g_tick_last_beat = GetTickCount();
    rx_reset(&rx);
    while (!wait_and_switch(&rx))
      ;
    agent_log("port %s CLOSED after %d frames in and %d out, reopening",
              VIRTIOPORT_WIN, g_frame_in, g_frame_out);
    legs_close_all();
    if (g_port_pending)
      {
      CancelIo(g_port);
      if (g_port_ovl.hEvent)
        CloseHandle(g_port_ovl.hEvent);
      g_port_ovl.hEvent = NULL;
      g_port_pending = 0;
      }
    CloseHandle(g_port);
    g_port = INVALID_HANDLE_VALUE;
    Sleep(OPEN_RETRY_MS);
    }
  WSACleanup();
  return 0;
}
