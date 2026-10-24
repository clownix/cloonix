/* web_http.c — WebSocket upgrade handlers, connection dispatcher and the
 * HTTP accept loop. Part of the GUI server TU; see web_common.h. */

#include "web_common.h"

/* A push socket: handshake, snapshot, then pings answered until close. */
static void serve_push_ws(int fd, const char *full_hdr, void (*add)(int),
                          void (*snapshot)(int), void (*del)(int))
{
    if (ws_accept(fd, full_hdr, ""))
      {
      KERR("ERROR http: websocket upgrade refused errno=%d", errno);
      close(fd); return;
      }
    add(fd);
    snapshot(fd);
    while (1) {
        uint8_t *payload = NULL; uint64_t plen = 0;
        int op = ws_read_frame(fd, &payload, &plen);
        free(payload);
        if (op < 0 || op == 8) break;
        if (op == 9) { uint8_t pong[2] = {0x8A, 0}; ws_write_frame(fd, pong, 2, NULL, 0); }
    }
    del(fd);
    close(fd);
}

static int origin_is_ours(const char *hdr)
{
    char origin[512], host[512], *p;

    if (!hdr_value(hdr, "Origin:", origin, sizeof(origin)))
        return 1;                       /* not a browser */
    if (!origin[0] || !strcmp(origin, "null"))
        return 0;                       /* a sandboxed frame, never ours */
    if (!hdr_value(hdr, "Host:", host, sizeof(host)))
        return 0;
    p = strstr(origin, "://");
    if (!p)
        return 0;
    p += 3;
    return !strcmp(p, host);
}

static void *handle_conn(void *arg)
{
    conn_t *c = arg;
    int fd = c->fd;
    free(c);
    pthread_detach(pthread_self());

    char hdr[MAX_HDR + 1];
    int  hlen = 0;
    while (hlen < MAX_HDR) {
        ssize_t n = read(fd, hdr + hlen, MAX_HDR - hlen);
        if (n <= 0) goto done;
        hlen += (int)n;
        hdr[hlen] = '\0';
        if (strstr(hdr, "\r\n\r\n") || strstr(hdr, "\n\n"))
            break;
    }

    int hdr_end = 0;
    char *hdr_end_ptr = strstr(hdr, "\r\n\r\n");
    if (hdr_end_ptr) hdr_end = (int)(hdr_end_ptr - hdr) + 4;
    else { hdr_end_ptr = strstr(hdr, "\n\n"); if (hdr_end_ptr) hdr_end = (int)(hdr_end_ptr - hdr) + 2; }
    int hdr_leftover = hlen - hdr_end;
    char *hdr_leftover_p = hdr + hdr_end;

    char method[16], path[512], proto[16];
    if (sscanf(hdr, "%15s %511s %15s", method, path, proto) != 3) {
        http_fail(fd, 400, "request line", "cannot parse the request line");
        goto done;
    }
    if (!auth_ok(hdr)) {
        /* auth_ok said why; no credentials is how every login begins. */
        auth_refuse(fd);
        goto done;
    }

    if (!origin_is_ours(hdr)) {
        char why[600];
        snprintf(why, sizeof(why), "another origin, %s %s", method, path);
        http_fail(fd, 403, "origin check", why);
        goto done;
    }

    if (strncmp(path, "/ctl/", 5) == 0) {
        const char *q = strchr(path, '?');
        const char *query = q ? q+1 : "";
        if (strncmp(path+5, "topo", 4) == 0) { ctl_topo(fd); }
        else if (strncmp(path+5, "commands", 8) == 0) topo_web_commands(fd);
        else if (strncmp(path+5, "start", 5) == 0) ctl_start(fd, query);
        else if (strncmp(path+5, "stop",  4) == 0) ctl_stop(fd, query);
        else if (strncmp(path+5, "vnc-desktop", 11) == 0) {
            char body[512] = {0}; int blen = 0; int content_len = 0;
            const char *cl = strcasestr(hdr, "Content-Length:");
            if (cl) sscanf(cl+15, "%d", &content_len);
            if (content_len > 0 && content_len < (int)sizeof(body)-1) {
                int take = hdr_leftover < content_len ? hdr_leftover : content_len;
                if (take > 0) { memcpy(body, hdr_leftover_p, take); blen = take; }
                while (blen < content_len) { ssize_t n = read(fd, body+blen, content_len-blen); if (n<=0) break; blen+=(int)n; }
            }
            handle_vnc_start(fd, body, blen);
        }
        else if (strncmp(path+5, "add-vm", 6) == 0) {
            char body[8192] = {0};
            int blen = 0;
            int content_len = 0;
            const char *cl = strcasestr(hdr, "Content-Length:");
            if (cl) sscanf(cl+15, "%d", &content_len);
            if (content_len > 0 && content_len < (int)sizeof(body)-1) {
                if (hdr_leftover > 0) {
                    int take = hdr_leftover < content_len ? hdr_leftover : content_len;
                    memcpy(body, hdr_leftover_p, take);
                    blen = take;
                }
                while (blen < content_len) {
                    ssize_t n = read(fd, body + blen, content_len - blen);
                    if (n <= 0) break;
                    blen += (int)n;
                }
            }
            handle_add_vm(fd, body, blen);
        }
        else if (strncmp(path+5, "add-cnt", 7) == 0) {
            char body[8192] = {0};
            int blen = 0;
            int content_len = 0;
            const char *cl = strcasestr(hdr, "Content-Length:");
            if (cl) sscanf(cl+15, "%d", &content_len);
            if (content_len > 0 && content_len < (int)sizeof(body)-1) {
                if (hdr_leftover > 0) {
                    int take = hdr_leftover < content_len ? hdr_leftover : content_len;
                    memcpy(body, hdr_leftover_p, take);
                    blen = take;
                }
                while (blen < content_len) {
                    ssize_t n = read(fd, body + blen, content_len - blen);
                    if (n <= 0) break;
                    blen += (int)n;
                }
            }
            handle_add_cnt(fd, body, blen);
        }
        else if (strncmp(path+5, "add-lan", 7) == 0) {
            char body[512] = {0}; int blen = 0; int content_len = 0;
            const char *cl = strcasestr(hdr, "Content-Length:");
            if (cl) sscanf(cl+15, "%d", &content_len);
            if (content_len > 0 && content_len < (int)sizeof(body)-1) {
                int take = hdr_leftover < content_len ? hdr_leftover : content_len;
                if (take > 0) { memcpy(body, hdr_leftover_p, take); blen = take; }
                while (blen < content_len) { ssize_t n = read(fd, body+blen, content_len-blen); if (n<=0) break; blen+=(int)n; }
            }
            handle_simple_name(fd, body, blen, "handle_add_lan", topo_web_add_lan);
        }
        else if (strncmp(path+5, "add-tap", 7) == 0) {
            char body[512] = {0}; int blen = 0; int content_len = 0;
            const char *cl = strcasestr(hdr, "Content-Length:");
            if (cl) sscanf(cl+15, "%d", &content_len);
            if (content_len > 0 && content_len < (int)sizeof(body)-1) {
                int take = hdr_leftover < content_len ? hdr_leftover : content_len;
                if (take > 0) { memcpy(body, hdr_leftover_p, take); blen = take; }
                while (blen < content_len) { ssize_t n = read(fd, body+blen, content_len-blen); if (n<=0) break; blen+=(int)n; }
            }
            handle_simple_name(fd, body, blen, "handle_add_tap", topo_web_add_tap);
        }
        else if (strncmp(path+5, "add-nat", 7) == 0) {
            char body[512] = {0}; int blen = 0; int content_len = 0;
            const char *cl = strcasestr(hdr, "Content-Length:");
            if (cl) sscanf(cl+15, "%d", &content_len);
            if (content_len > 0 && content_len < (int)sizeof(body)-1) {
                int take = hdr_leftover < content_len ? hdr_leftover : content_len;
                if (take > 0) { memcpy(body, hdr_leftover_p, take); blen = take; }
                while (blen < content_len) { ssize_t n = read(fd, body+blen, content_len-blen); if (n<=0) break; blen+=(int)n; }
            }
            handle_simple_name(fd, body, blen, "handle_add_nat", topo_web_add_nat);
        }
        else if (strncmp(path+5, "add-phy", 7) == 0) {
            char body[512] = {0}; int blen = 0; int content_len = 0;
            const char *cl = strcasestr(hdr, "Content-Length:");
            if (cl) sscanf(cl+15, "%d", &content_len);
            if (content_len > 0 && content_len < (int)sizeof(body)-1) {
                int take = hdr_leftover < content_len ? hdr_leftover : content_len;
                if (take > 0) { memcpy(body, hdr_leftover_p, take); blen = take; }
                while (blen < content_len) { ssize_t n = read(fd, body+blen, content_len-blen); if (n<=0) break; blen+=(int)n; }
            }
            handle_add_phy(fd, body, blen);
        }
        else if (strncmp(path+5, "add-a2b", 7) == 0) {
            char body[512] = {0}; int blen = 0; int content_len = 0;
            const char *cl = strcasestr(hdr, "Content-Length:");
            if (cl) sscanf(cl+15, "%d", &content_len);
            if (content_len > 0 && content_len < (int)sizeof(body)-1) {
                int take = hdr_leftover < content_len ? hdr_leftover : content_len;
                if (take > 0) { memcpy(body, hdr_leftover_p, take); blen = take; }
                while (blen < content_len) { ssize_t n = read(fd, body+blen, content_len-blen); if (n<=0) break; blen+=(int)n; }
            }
            handle_simple_name(fd, body, blen, "handle_add_a2b", topo_web_add_a2b);
        }
        else if (strncmp(path+5, "position", 8) == 0) {
            char body[512] = {0}; int blen = 0; int content_len = 0;
            const char *cl = strcasestr(hdr, "Content-Length:");
            if (cl) sscanf(cl+15, "%d", &content_len);
            if (content_len > 0 && content_len < (int)sizeof(body)-1) {
                int take = hdr_leftover < content_len ? hdr_leftover : content_len;
                if (take > 0) { memcpy(body, hdr_leftover_p, take); blen = take; }
                while (blen < content_len) { ssize_t n = read(fd, body+blen, content_len-blen); if (n<=0) break; blen+=(int)n; }
            }
            handle_position(fd, body, blen);
        }
        else if (strncmp(path+5, "note", 4) == 0) {
            char body[2048] = {0}; int blen = 0; int content_len = 0;
            const char *cl = strcasestr(hdr, "Content-Length:");
            if (cl) sscanf(cl+15, "%d", &content_len);
            if (content_len > 0 && content_len < (int)sizeof(body)-1) {
                int take = hdr_leftover < content_len ? hdr_leftover : content_len;
                if (take > 0) { memcpy(body, hdr_leftover_p, take); blen = take; }
                while (blen < content_len) { ssize_t n = read(fd, body+blen, content_len-blen); if (n<=0) break; blen+=(int)n; }
            }
            handle_note(fd, body, blen);
        }
        else if (strncmp(path+5, "poseth", 6) == 0) {
            char body[512] = {0}; int blen = 0; int content_len = 0;
            const char *cl = strcasestr(hdr, "Content-Length:");
            if (cl) sscanf(cl+15, "%d", &content_len);
            if (content_len > 0 && content_len < (int)sizeof(body)-1) {
                int take = hdr_leftover < content_len ? hdr_leftover : content_len;
                if (take > 0) { memcpy(body, hdr_leftover_p, take); blen = take; }
                while (blen < content_len) { ssize_t n = read(fd, body+blen, content_len-blen); if (n<=0) break; blen+=(int)n; }
            }
            handle_poseth(fd, body, blen);
        }
        else if (strncmp(path+5, "cnf-a2b", 7) == 0) {
            char body[512] = {0}; int blen = 0; int content_len = 0;
            const char *cl = strcasestr(hdr, "Content-Length:");
            if (cl) sscanf(cl+15, "%d", &content_len);
            if (content_len > 0 && content_len < (int)sizeof(body)-1) {
                int take = hdr_leftover < content_len ? hdr_leftover : content_len;
                if (take > 0) { memcpy(body, hdr_leftover_p, take); blen = take; }
                while (blen < content_len) { ssize_t n = read(fd, body+blen, content_len-blen); if (n<=0) break; blen+=(int)n; }
            }
            handle_cnf_a2b(fd, body, blen);
        }
        else if (strncmp(path+5, "add-c2c", 7) == 0) {
            char body[512] = {0}; int blen = 0; int content_len = 0;
            const char *cl = strcasestr(hdr, "Content-Length:");
            if (cl) sscanf(cl+15, "%d", &content_len);
            if (content_len > 0 && content_len < (int)sizeof(body)-1) {
                int take = hdr_leftover < content_len ? hdr_leftover : content_len;
                if (take > 0) { memcpy(body, hdr_leftover_p, take); blen = take; }
                while (blen < content_len) { ssize_t n = read(fd, body+blen, content_len-blen); if (n<=0) break; blen+=(int)n; }
            }
            handle_c2c(fd, body, blen, "handle_add_c2c");
        }
        else if (strncmp(path+5, "rma", 3) == 0) {
            play_stop_all();
            topo_web_rma();
            const char *ok = "HTTP/1.1 200 OK\r\nContent-Length: 3\r\n"
                             "Connection: close\r\n\r\nok\n";
            (void)write(fd, ok, strlen(ok));
        }
        else if (strncmp(path+5, "del-node", 8) == 0) {
            char body[512] = {0}; int blen = 0; int content_len = 0;
            const char *cl = strcasestr(hdr, "Content-Length:");
            if (cl) sscanf(cl+15, "%d", &content_len);
            if (content_len > 0 && content_len < (int)sizeof(body)-1) {
                int take = hdr_leftover < content_len ? hdr_leftover : content_len;
                if (take > 0) { memcpy(body, hdr_leftover_p, take); blen = take; }
                while (blen < content_len) { ssize_t n = read(fd, body+blen, content_len-blen); if (n<=0) break; blen+=(int)n; }
            }
            handle_simple_name(fd, body, blen, "handle_del_node", topo_web_del_node);
        }
        else if (strncmp(path+5, "reboot-vm", 9) == 0) {
            char body[512] = {0}; int blen = 0; int content_len = 0;
            const char *cl = strcasestr(hdr, "Content-Length:");
            if (cl) sscanf(cl+15, "%d", &content_len);
            if (content_len > 0 && content_len < (int)sizeof(body)-1) {
                int take = hdr_leftover < content_len ? hdr_leftover : content_len;
                if (take > 0) { memcpy(body, hdr_leftover_p, take); blen = take; }
                while (blen < content_len) { ssize_t n = read(fd, body+blen, content_len-blen); if (n<=0) break; blen+=(int)n; }
            }
            handle_simple_name(fd, body, blen, "handle_reboot_vm", topo_web_reboot_vm);
        }
        else if (strncmp(path+5, "save-vm", 7) == 0) {
            char body[1024] = {0}; int blen = 0; int content_len = 0;
            const char *cl = strcasestr(hdr, "Content-Length:");
            if (cl) sscanf(cl+15, "%d", &content_len);
            if (content_len > 0 && content_len < (int)sizeof(body)-1) {
                int take = hdr_leftover < content_len ? hdr_leftover : content_len;
                if (take > 0) { memcpy(body, hdr_leftover_p, take); blen = take; }
                while (blen < content_len) { ssize_t n = read(fd, body+blen, content_len-blen); if (n<=0) break; blen+=(int)n; }
            }
            char name[MAX_NAME_LEN] = {0};
            char vm_path[MAX_PATH_LEN] = {0};
            char *p;
            if ((p = strstr(body, "\"name\""))) {
                p = strchr(p+6, ':'); if (p) {
                    while (*p==':'||*p==' '||*p=='"') p++;
                    int i=0; while(*p&&*p!='"'&&i<MAX_NAME_LEN-1) name[i++]=*p++;
                }
            }
            if ((p = strstr(body, "\"path\""))) {
                p = strchr(p+6, ':'); if (p) {
                    while (*p==':'||*p==' '||*p=='"') p++;
                    int i=0; while(*p&&*p!='"'&&i<MAX_PATH_LEN-1) vm_path[i++]=*p++;
                }
            }
            if (!name[0] || !vm_path[0]) {
                const char *r="HTTP/1.1 400 Bad Request\r\nContent-Length:0\r\nConnection: close\r\n\r\n";
                (void)write(fd,r,strlen(r));
            } else {
                topo_web_save_vm(name, vm_path);
                const char *ok="HTTP/1.1 200 OK\r\nContent-Length:3\r\nConnection: close\r\n\r\nok\n";
                (void)write(fd, ok, strlen(ok));
            }
        }
        else if (strncmp(path+5, "add-edge", 8) == 0) {
            char body[512] = {0}; int blen = 0; int content_len = 0;
            const char *cl = strcasestr(hdr, "Content-Length:");
            if (cl) sscanf(cl+15, "%d", &content_len);
            if (content_len > 0 && content_len < (int)sizeof(body)-1) {
                int take = hdr_leftover < content_len ? hdr_leftover : content_len;
                if (take > 0) { memcpy(body, hdr_leftover_p, take); blen = take; }
                while (blen < content_len) { ssize_t n = read(fd, body+blen, content_len-blen); if (n<=0) break; blen+=(int)n; }
            }
            handle_add_edge(fd, body, blen);
        }
        else if (strncmp(path+5, "del-edge", 8) == 0) {
            char body[512] = {0}; int blen = 0; int content_len = 0;
            const char *cl = strcasestr(hdr, "Content-Length:");
            if (cl) sscanf(cl+15, "%d", &content_len);
            if (content_len > 0 && content_len < (int)sizeof(body)-1) {
                int take = hdr_leftover < content_len ? hdr_leftover : content_len;
                if (take > 0) { memcpy(body, hdr_leftover_p, take); blen = take; }
                while (blen < content_len) { ssize_t n = read(fd, body+blen, content_len-blen); if (n<=0) break; blen+=(int)n; }
            }
            handle_del_edge(fd, body, blen);
        }
        else if (strncmp(path+5, "snf", 3) == 0) {
            char body[256] = {0}; int blen = 0; int content_len = 0;
            const char *cl = strcasestr(hdr, "Content-Length:");
            if (cl) sscanf(cl+15, "%d", &content_len);
            if (content_len > 0 && content_len < (int)sizeof(body)-1) {
                int take = hdr_leftover < content_len ? hdr_leftover : content_len;
                if (take > 0) { memcpy(body, hdr_leftover_p, take); blen = take; }
                while (blen < content_len) { ssize_t n = read(fd, body+blen, content_len-blen); if (n<=0) break; blen+=(int)n; }
            }
            handle_snf(fd, body, blen);
        }
        else if (strncmp(path+5, "wireshark-start", 15) == 0) {
            char body[256] = {0}; int blen = 0; int content_len = 0;
            const char *cl = strcasestr(hdr, "Content-Length:");
            if (cl) sscanf(cl+15, "%d", &content_len);
            if (content_len > 0 && content_len < (int)sizeof(body)-1) {
                int take = hdr_leftover < content_len ? hdr_leftover : content_len;
                if (take > 0) { memcpy(body, hdr_leftover_p, take); blen = take; }
                while (blen < content_len) { ssize_t n = read(fd, body+blen, content_len-blen); if (n<=0) break; blen+=(int)n; }
            }
            char name[MAX_NAME_LEN] = {0}; int num = 0;
            char *p;
            if ((p = strstr(body, "\"name\""))) {
                p = strchr(p+6, ':'); if (p) {
                    while (*p==':'||*p==' '||*p=='"') p++;
                    int i=0; while(*p&&*p!='"'&&i<MAX_NAME_LEN-1) name[i++]=*p++;
                }
            }
            if ((p = strstr(body, "\"num\":"))) sscanf(p+6, "%d", &num);
            if (!name[0]) {
                const char *r="HTTP/1.1 400 Bad Request\r\nContent-Length:0\r\nConnection: close\r\n\r\n";
                (void)write(fd,r,strlen(r));
            } else {
                topo_web_wireshark_start(name, num);
                const char *ok="HTTP/1.1 200 OK\r\nContent-Length:3\r\nConnection: close\r\n\r\nok\n";
                (void)write(fd, ok, strlen(ok));
            }
        }
        else { const char *r="HTTP/1.1 404 Not Found\r\nContent-Length:0\r\nConnection: close\r\n\r\n"; (void)write(fd,r,strlen(r)); }
    } else if (strncmp(path, VNC_PREFIX, strlen(VNC_PREFIX)) == 0) {
        const char *subpath = strchr(path + strlen(VNC_PREFIX), '/');
        if (subpath && strncmp(subpath, "/novnc/", 7) == 0) {
            serve_file(fd, subpath);
        } else {
            proxy_to_vnc_unix(fd, method, path, proto, hdr, hdr_end, hdr_leftover_p, hdr_leftover);
        }
    } else if (strncmp(path, XTERM_PREFIX, strlen(XTERM_PREFIX)) == 0) {
        serve_file(fd, path);
    } else if (strncmp(path, SH_PREFIX, strlen(SH_PREFIX)) == 0) {
        proxy_to_sh(fd, method, path, proto, hdr, hlen);
    } else if (strncmp(path, WS_PREFIX, strlen(WS_PREFIX)) == 0) {
        proxy_to_unix(fd, method, path, proto, hdr, hlen);
    } else if (strcmp(path, "/images") == 0) {
        int is_ws = (strstr(hdr, "Upgrade: websocket") != NULL ||
                     strstr(hdr, "Upgrade: WebSocket") != NULL);
        if (is_ws) {
            serve_push_ws(fd, hdr, images_ws_add,
                          topo_web_send_images_snapshot, images_ws_remove);
            return NULL;
        } else {
            http_fail(fd, 400, path, "WebSocket upgrade required");
        }
    } else if (strcmp(path, "/topo") == 0) {
        int is_ws = (strstr(hdr, "Upgrade: websocket") != NULL ||
                     strstr(hdr, "Upgrade: WebSocket") != NULL);
        if (is_ws) {
            serve_push_ws(fd, hdr, topo_ws_add,
                          topo_web_send_snapshot, topo_ws_remove);
            return NULL;
        } else {
            http_fail(fd, 400, path, "WebSocket upgrade required");
        }
    } else if (strcmp(path, "/play") == 0) {
        int is_ws = (strstr(hdr, "Upgrade: websocket") != NULL ||
                     strstr(hdr, "Upgrade: WebSocket") != NULL);
        if (is_ws) {
            handle_play_ws(fd, hdr);
        } else {
            http_fail(fd, 400, path, "WebSocket upgrade required");
        }
    } else if (strcmp(path, "/rec") == 0) {
        int is_ws = (strstr(hdr, "Upgrade: websocket") != NULL ||
                     strstr(hdr, "Upgrade: WebSocket") != NULL);
        if (is_ws) {
            handle_rec_ws(fd, hdr);
        } else {
            http_fail(fd, 400, path, "WebSocket upgrade required");
        }
    } else if (strncmp(path, "/files/", 7) == 0) {
        const char *q = strchr(path, '?');
        const char *query = q ? q + 1 : "";
        char route[64] = {0};
        sscanf(path + 7, "%63[^/?]", route);
        if (strcmp(route, "list") == 0) {
            files_list(fd, query);
        } else if (strcmp(route, "get") == 0) {
            files_get(fd, query);
        } else {
            const char *r = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
            (void)write(fd, r, strlen(r));
        }
    } else {
        serve_file(fd, path);
    }

done:
    close(fd);
    return NULL;
}

static int unix_listener(const char *path)
{
    struct sockaddr_un addr = {0};
    int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) { KERR("ERROR socket %s: %s", path, strerror(errno)); return -1; }
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path) - 1, "%s", path);

    /* The parent dir is proxymous'; made here too in case the gui server
       starts first. */
    {
        char dir[MAX_PATH_LEN];
        char *slash;
        snprintf(dir, sizeof(dir), "%s", path);
        slash = strrchr(dir, '/');
        if (slash) {
            *slash = '\0';
            if (mkdir(dir, 0777) < 0 && errno != EEXIST)
                KERR("WARNING mkdir %s failed errno=%d", dir, errno);
        }
    }
    unlink(path);
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        KERR("ERROR bind %s: %s", path, strerror(errno)); close(fd); return -1;
    }
    if (listen(fd, 32) < 0) {
        KERR("ERROR listen %s: %s", path, strerror(errno)); close(fd); return -1;
    }
    return fd;
}

static int http_server_init(const char *snf_dir, const char *unix_path)
{
    snprintf(g_snf_dir, sizeof(g_snf_dir), "%s", snf_dir);

    {
        char self[512] = {0};
        ssize_t n = readlink("/proc/self/exe", self, sizeof(self) - 1);
        if (n > 0) {
            self[n] = '\0';
            char *slash = strrchr(self, '/');
            if (slash) slash[1] = '\0';
            else       strcpy(self, "./");
        } else {
            strcpy(self, "./");
        }
        snprintf(g_www_dir, sizeof(g_www_dir), "%s", self);
        size_t len = strlen(g_www_dir);
        if (len > 1 && g_www_dir[len-1] == '/')
            g_www_dir[len-1] = '\0';
        snprintf(g_webshark_bin, sizeof(g_webshark_bin),
                 "%scloonix-webshark", self);
    }

    user_demo_dir_init();

    auth_init(g_net_name);

    signal(SIGPIPE, SIG_IGN);
    /* SIGCHLD must NOT be ignored: pid_clone needs it */

    g_srv_fd = unix_listener(unix_path);
    if (g_srv_fd < 0)
      {
        KERR("ERROR http: no listening socket, the server cannot accept anything");
        return -1;
      }

    return g_srv_fd;
}

static void *http_server_thread(void *arg)
{
    struct pollfd pfd;
    (void)arg;

    pfd.fd = g_srv_fd;
    pfd.events = POLLIN;
    while (1) {
        int cfd;
        conn_t *c;
        pthread_t tid;
        int n = poll(&pfd, 1, -1);
        if (n < 0) {
            if (errno == EINTR) continue;
            KERR("ERROR http listener stops, poll: %s", strerror(errno));
            break;
        }
        if (!(pfd.revents & POLLIN))
            continue;
        cfd = accept4(pfd.fd, NULL, NULL, SOCK_CLOEXEC);
        if (cfd < 0) {
            if ((errno == EINTR) || (errno == ECONNABORTED))
                continue;
            /* out of descriptors or memory: wait, the listener must live */
            if ((errno == EMFILE) || (errno == ENFILE) ||
                (errno == ENOBUFS) || (errno == ENOMEM)) {
                KERR("WARNING http accept: %s, retrying", strerror(errno));
                usleep(100000);
                continue;
            }
            KERR("ERROR http listener stops, accept: %s", strerror(errno));
            break;
        }
        c = malloc(sizeof(conn_t));
        if (!c) {
            KERR("WARNING http: no memory for a connection, closed");
            close(cfd);
            continue;
        }
        c->fd = cfd;
        if (pthread_create(&tid, NULL, handle_conn, c)) {
            KERR("WARNING http: no thread for a connection, closed");
            close(cfd);
            free(c);
            continue;
        }
    }
    close(g_srv_fd);
    return NULL;
}

