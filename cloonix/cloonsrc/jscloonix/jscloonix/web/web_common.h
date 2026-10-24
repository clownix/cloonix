/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */

/* One TU: cloonix_gui_server.c includes web_*.c then topo_web.c. Routes: /ws-,
 * /vnc-, /sh-<net>/<kind>/ pty, /topo /images ws, POST /ctl/, static files. */

#ifndef WEB_COMMON_H
#define WEB_COMMON_H

#include <ctype.h>
#include "util_sock.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <pty.h>
#include <dirent.h>
#include <poll.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_HDR       16384
#define BUF_SIZE      65536
#define WS_PREFIX     "/ws-"
#define VNC_PREFIX    "/vnc-"
#define SH_PREFIX     "/sh-"
#define XTERM_PREFIX  "/xterm/"

#define MAX_TOPO_WS_CLIENTS 64

typedef struct { int fd; } conn_t;

extern int  g_srv_fd;

void http_fail(int fd, int code, const char *ctx, const char *body);
void http_ok(int fd, const char *body);
int  topo_ws_broadcast(const char *msg, int len);
void ws_write_frame(int fd, const uint8_t *hdr, int hlen,
                    const char *msg, int len);
extern char g_www_dir[512];
extern char g_snf_dir[512];
extern char g_webshark_bin[512];
extern char g_user_demo_dir[512];
extern char g_share_dir[512];

void topo_web_send_snapshot(int cfd);
void topo_web_send_images_snapshot(int cfd);
void topo_web_add_vm(t_topo_kvm *kvm);
void topo_web_add_cnt(t_topo_cnt *cnt);
void topo_web_add_lan(char *name);
void topo_web_add_tap(char *name);
void topo_web_commands(int fd);
void topo_web_add_nat(char *name);
void topo_web_add_a2b(char *name);
void topo_web_add_phy(char *name, int macvlan);
void topo_web_cnf_a2b(char *name, int dir, char *cmd, int val);
void topo_web_position(char *name, int x, int y);
void topo_web_poseth(char *name, int num, int deg);
void topo_web_note(char *name, char *txt);
void topo_web_add_c2c(char *name_and_dest);
void topo_web_del_node(char *name);
void topo_web_reboot_vm(char *name);
void topo_web_save_vm(char *name, char *path);
void topo_web_add_edge(char *name, int num, char *lan);
void topo_web_del_edge(char *name, int num, char *lan);
void topo_web_rma(void);
void topo_web_wireshark_start(char *name, int num);
void topo_web_snf_set(char *name, int num, int on);
int  snf_webshark_in_use(const char *name, int num);
char *topo_web_get_snapshot_json(int *out_len);
void ws_send_text(int fd, const char *msg, int len);
void ws_broadcast_topo_json(const char *json, int len);
void ws_broadcast_images_json(const char *json, int len);
static void topo_ws_add(int fd);
static void topo_ws_remove(int fd);
static void images_ws_add(int fd);
static void images_ws_remove(int fd);
static void sha1(const uint8_t *msg, size_t len, uint8_t out[20]);
static void base64_enc(const uint8_t *in, size_t len, char *out);
static int  ws_accept(int fd, const char *full_hdr, const char *proto);
static int  ws_handshake(int fd, const char *full_hdr);
static int  ws_write(int fd, const void *data, size_t len);
static int  ws_read_frame(int fd, uint8_t **buf_out, uint64_t *plen_out);

static void kill_all_children(void);
static void sighandler(int sig);
static void url_decode(char *s);
static void ctl_start(int fd, const char *query);
static void ctl_stop(int fd, const char *query);
static const char *mime(const char *path);
static void serve_file(int fd, const char *rel_path);
static void user_demo_dir_init(void);
static int  web_name_ok(const char *s);
static int  demo_name_ok(const char *name);
static void files_list(int fd, const char *query);
static int  safe_subdir(const char *sub, char *out, int outsz);
static void files_get(int fd, const char *query);

static void ws_vnc_bridge(int browser_fd, int vnc_fd,
                            const char *leftover, int leftover_len);
static void pump(int a, int b);
static void proxy_to_vnc_unix(int browser_fd,
                                const char *method, const char *path,
                                const char *proto, const char *full_hdr,
                                int full_hdr_len,
                                const char *leftover, int leftover_len);
static void proxy_to_unix(int browser_fd,
                          const char *method, const char *path,
                          const char *proto, const char *full_hdr,
                          int full_hdr_len);

static void ws_pty_bridge(int browser_fd, int pty_master);
static void proxy_to_sh(int browser_fd,
                        const char *method, const char *path,
                        const char *proto, const char *full_hdr,
                        int full_hdr_len);

static void handle_add_vm(int fd, const char *body, int blen);
static void handle_add_cnt(int fd, const char *body, int blen);
static void extract_json_str(const char *body, const char *key,
                             char *out, int outsz);
static void handle_c2c(int fd, const char *body, int blen, const char *label);
static void handle_simple_name(int fd, const char *body, int blen,
                               const char *label, void (*fn)(char *));
static void handle_add_edge(int fd, const char *body, int blen);
static void handle_del_edge(int fd, const char *body, int blen);
static void handle_vnc_start(int fd, const char *body, int blen);

void handle_rec_ws(int browser_fd, const char *full_hdr);
int  rec_iface_is_reserved(const char *key);
void rec_stop_all_unlocked(void);

void handle_play_ws(int browser_fd, const char *full_hdr);
void play_stop_all(void);
static void ctl_topo(int fd);
static void *handle_conn(void *arg);
static int  http_server_init(const char *snf_dir, const char *unix_path);
static void *http_server_thread(void *arg);

#endif
