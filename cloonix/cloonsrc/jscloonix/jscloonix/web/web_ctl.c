/* web_ctl.c — ctl satellite (start/list/stop), signal handling and static
 * file serving. Part of the GUI server TU; see web_common.h. */

#include "web_common.h"

#define MAX_PROCS 32
static struct {
    char sock[128];
    char snf[512];
    pid_t pid;
} g_procs[MAX_PROCS];
static int g_nprocs = 0;
static pthread_mutex_t g_procs_mutex = PTHREAD_MUTEX_INITIALIZER;

#define MAX_VNC 32
static struct {
    char vm[128];       /* VM name */
    char sock[512];     /* VNC Unix socket path */
} g_vnc[MAX_VNC];
static int g_nvnc = 0;
static pthread_mutex_t g_vnc_mutex = PTHREAD_MUTEX_INITIALIZER;

static void kill_all_children(void)
{
    rec_stop_all_unlocked();

    pthread_mutex_lock(&g_procs_mutex);
    for (int i = 0; i < g_nprocs; i++)
        kill(g_procs[i].pid, SIGTERM);
    g_nprocs = 0;
    pthread_mutex_unlock(&g_procs_mutex);

    pthread_mutex_lock(&g_vnc_mutex);
    g_nvnc = 0;
    pthread_mutex_unlock(&g_vnc_mutex);
}

static void sighandler(int sig)
{
    (void)sig;
    kill_all_children();
    if (g_srv_fd >= 0) close(g_srv_fd);
    _exit(0);
}

static void url_decode(char *s)
{
    char *r = s, *w = s;
    while (*r) {
        if (*r == '%' && isxdigit((unsigned char)r[1]) && isxdigit((unsigned char)r[2])) {
            char hex[3] = { r[1], r[2], 0 };
            *w++ = (char)strtol(hex, NULL, 16);
            r += 3;
        } else if (*r == '+') {
            *w++ = ' '; r++;
        } else {
            *w++ = *r++;
        }
    }
    *w = '\0';
}

static void spawn_child_watchdog(pid_t child_pid)
{
    pid_t guiserver_pid = getpid();   /* worker threads share the process pid */
    pid_t wpid = fork();
    if (wpid != 0)
        return;                       /* parent returns; child is the watchdog */
    setsid();
    { int i; for (i = 3; i < 1024; i++) close(i); }
    while (1) {
        if (kill(guiserver_pid, 0) < 0 && errno == ESRCH) {
            kill(-child_pid, SIGTERM);
            usleep(200 * 1000);
            kill(-child_pid, SIGKILL);
            kill(child_pid, SIGKILL);   /* in case child is not a group leader */
            _exit(0);
        }
        if (kill(child_pid, 0) < 0 && errno == ESRCH)
            _exit(0);                 /* child already gone */
        usleep(1000 * 1000);
    }
}

/* A webshark stopped is gone and reaped before the answer, and before a new
   one binds the socket path the dying one unlinks. */
static void webshark_stop(pid_t pid)
{
    int i;
    kill(pid, SIGTERM);
    for (i = 0; i < 100; i++) {
        if (waitpid(pid, NULL, WNOHANG) == pid ||
            (kill(pid, 0) < 0 && errno == ESRCH))
            return;
        usleep(20 * 1000);
    }
    KERR("WARNING webshark %d still there 2 s after SIGTERM, killed", pid);
    kill(pid, SIGKILL);
    waitpid(pid, NULL, 0);
}

static void prune_dead_procs(void)
{
    for (int i = 0; i < g_nprocs; ) {
        (void) waitpid(g_procs[i].pid, NULL, WNOHANG);
        if (kill(g_procs[i].pid, 0) < 0 && errno == ESRCH)
            g_procs[i] = g_procs[--g_nprocs];
        else
            i++;
    }
}

static void ctl_start(int fd, const char *query)
{
    char sock[128] = {0}, snf[512] = {0}, snf_sock[256] = {0};
    const char *p;
    if ((p = strstr(query, "sock=")))     sscanf(p + 5,     "%127[^&]", sock);
    if ((p = strstr(query, "snf=")))      sscanf(p + 4,     "%511[^&]", snf);
    if ((p = strstr(query, "snf_sock="))) sscanf(p + 9,     "%255[^&]", snf_sock);
    url_decode(sock); url_decode(snf); url_decode(snf_sock);

    if (!sock[0]) {
        http_fail(fd, 400, "ctl_start", "Missing params"); return;
    }
    if ((!web_name_ok(sock)) ||
        (snf_sock[0] && (!web_name_ok(snf_sock))) ||
        (snf[0] && (!web_name_ok(snf)))) {
        KERR("ERROR ctl_start: refused sock=[%s] snf=[%s] snf_sock=[%s]: "
             "these are names, not paths", sock, snf, snf_sock);
        http_fail(fd, 400, "ctl_start", "bad name"); return;
    }

    if (!snf[0]) snprintf(snf, sizeof(snf), "%s", g_snf_dir);

    char snf_path[640];
    if (snf_sock[0])
        snprintf(snf_path, sizeof(snf_path), "%s/%s", g_snf_dir, snf_sock);
    else
        snprintf(snf_path, sizeof(snf_path), "%s", snf);

    /* One webshark per interface, and a start is a fresh one */
    pid_t old[MAX_PROCS];
    int nold = 0;
    pthread_mutex_lock(&g_procs_mutex);
    prune_dead_procs();
    for (int i = 0; i < g_nprocs; ) {
        if (strcmp(g_procs[i].sock, sock) == 0) {
            KTRC("ctl_start: %s had webshark pid %d, replaced", sock,
                 g_procs[i].pid);
            old[nold++] = g_procs[i].pid;
            g_procs[i] = g_procs[--g_nprocs];
        }
        else
            i++;
    }
    pthread_mutex_unlock(&g_procs_mutex);
    for (int i = 0; i < nold; i++)
        webshark_stop(old[i]);

    char sock_path[640];
    snprintf(sock_path, sizeof(sock_path), "%s/%s", g_snf_dir, sock);
    KTRC("ctl_start: webshark -f %s %s  (output in %s/webshark_%s.log)",
         sock_path, snf_path, g_snf_dir, sock);

    /* No head start here: webshark waits for the fan-out fifo itself, in its
       capture thread. */

    int efd[2];
    if (pipe2(efd, O_CLOEXEC)) {
        efd[0] = -1;
        efd[1] = -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        if (efd[0] >= 0) { close(efd[0]); close(efd[1]); }
        KERR("ERROR ctl_start: fork failed errno=%d", errno);
        http_fail(fd, 500, "ctl_start", "fork failed"); return;
    }
    if (pid == 0) {
        int err, lfd;
        char logfile[700];
        setsid();
        for (int i = 3; i < 1024; i++) {
            if (i != efd[1])
                close(i);
        }
        snprintf(logfile, sizeof(logfile), "%s/webshark_%s.log", g_snf_dir, sock);
        lfd = open(logfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (lfd >= 0) {
            dup2(lfd, 1);
            dup2(lfd, 2);
            if (lfd > 2)
                close(lfd);
        }
        execl(g_webshark_bin, g_webshark_bin, "-f", sock_path, snf_path, NULL);
        err = errno;
        if (efd[1] >= 0)
            (void)write(efd[1], &err, sizeof(err));
        _exit(127);
    }

    if (efd[0] >= 0) {
        int err = 0, n;
        close(efd[1]);
        n = (int) read(efd[0], &err, sizeof(err));
        close(efd[0]);
        if (n == (int) sizeof(err)) {
            KERR("ERROR ctl_start: cannot exec %s errno=%d (%s)",
                 g_webshark_bin, err, strerror(err));
            waitpid(pid, NULL, 0);
            http_fail(fd, 500, "ctl_start", "exec failed");
            return;
        }
    }

    {
        int status = 0;
        usleep(300 * 1000);
        if (waitpid(pid, &status, WNOHANG) == pid) {
            if (WIFEXITED(status))
                KERR("ERROR ctl_start: webshark exited %d straight away",
                     WEXITSTATUS(status));
            else if (WIFSIGNALED(status))
                KERR("ERROR ctl_start: webshark died on signal %d",
                     WTERMSIG(status));
            http_fail(fd, 500, "ctl_start", "webshark died at once");
            return;
        }
    }

    pthread_mutex_lock(&g_procs_mutex);
    if (g_nprocs < MAX_PROCS) {
        snprintf(g_procs[g_nprocs].sock, sizeof(g_procs[0].sock), "%s", sock);
        snprintf(g_procs[g_nprocs].snf, sizeof(g_procs[0].snf), "%s", snf);
        g_procs[g_nprocs].pid = pid;
        g_nprocs++;
    }
    pthread_mutex_unlock(&g_procs_mutex);

    spawn_child_watchdog(pid);

    http_ok(fd, "started");
}

int snf_webshark_in_use(const char *name, int num)
{
    char want[160];
    int i, found = 0;
    snprintf(want, sizeof(want), "sock_%s_%d", name, num);
    pthread_mutex_lock(&g_procs_mutex);
    for (i = 0; i < g_nprocs; ) {
        if (kill(g_procs[i].pid, 0) < 0 && errno == ESRCH)
            g_procs[i] = g_procs[--g_nprocs];   /* prune dead */
        else
            i++;
    }
    for (i = 0; i < g_nprocs; i++) {
        if (!strcmp(g_procs[i].sock, want)) { found = 1; break; }
    }
    pthread_mutex_unlock(&g_procs_mutex);
    return found;
}

static void ctl_stop(int fd, const char *query)
{
    char sock[128] = {0};
    const char *p = strstr(query, "sock=");
    if (p) sscanf(p + 5, "%127[^&]", sock);
    if (!web_name_ok(sock)) {
        KERR("ERROR ctl_stop: refused sock=[%s]", sock);
        http_fail(fd, 400, "ctl_stop", "bad name"); return;
    }

    pthread_mutex_lock(&g_procs_mutex);
    for (int i = 0; i < g_nprocs; i++) {
        if (strcmp(g_procs[i].sock, sock) == 0) {
            pid_t pid = g_procs[i].pid;
            g_procs[i] = g_procs[--g_nprocs];
            pthread_mutex_unlock(&g_procs_mutex);
            webshark_stop(pid);
            http_ok(fd, "stopped"); return;
        }
    }
    pthread_mutex_unlock(&g_procs_mutex);
    /* Stopping what is not there is not a fault: every closed panel asks. */
    http_ok(fd, "none");
}

static const char *mime(const char *path)
{
    const char *ext = strrchr(path, '.');
    if (!ext) return "application/octet-stream";
    if (!strcmp(ext, ".html")) return "text/html; charset=utf-8";
    if (!strcmp(ext, ".js"))   return "application/javascript; charset=utf-8";
    if (!strcmp(ext, ".css"))  return "text/css; charset=utf-8";
    if (!strcmp(ext, ".ico"))  return "image/x-icon";
    if (!strcmp(ext, ".png"))  return "image/png";
    if (!strcmp(ext, ".svg"))  return "image/svg+xml";
    if (!strcmp(ext, ".json")) return "application/json; charset=utf-8";
    if (!strcmp(ext, ".py"))   return "text/plain; charset=utf-8";
    if (!strcmp(ext, ".ogg"))  return "audio/ogg";
    return "application/octet-stream";
}

static void serve_file(int fd, const char *rel_path)
{
    if (strstr(rel_path, "..")) {
        http_fail(fd, 403, "static file", "path contains ..");
        return;
    }

    const char *file = rel_path;
    if (strcmp(file, "/") == 0)
        file = "/cloonix.html";

    if (strcmp(file, "/favicon.ico") == 0) {
        const char *rsp = "HTTP/1.1 204 No Content\r\n"
                          "Connection: close\r\n\r\n";
        (void) write(fd, rsp, strlen(rsp));
        return;
    }

    char full[1024];
    snprintf(full, sizeof(full), "%s%s", g_www_dir, file);

    int f = open(full, O_RDONLY);
    if (f < 0) {
        KERR("WARNING http 404 static file: cannot open %s errno=%d (%s)",
             full, errno, strerror(errno));
        http_fail(fd, 404, NULL, "no such file");
        return;
    }

    struct stat st;
    fstat(f, &st);
    off_t size = st.st_size;

    char hdr[512];
    int hlen = snprintf(hdr, sizeof(hdr),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %lld\r\n"
        "Cache-Control: no-store\r\n"
        "Connection: close\r\n"
        "\r\n",
        mime(file), (long long)size);
    if (util_write_all(fd, hdr, hlen)) {
        KERR("WARNING http: %s cut while sending the header", full);
        close(f);
        return;
    }

    char buf[BUF_SIZE];
    ssize_t n;
    while ((n = read(f, buf, sizeof(buf))) > 0) {
        if (util_write_all(fd, buf, (int)n)) {
            KERR("WARNING http: %s cut after %lld of %lld bytes",
                 full, (long long)lseek(f, 0, SEEK_CUR), (long long)size);
            break;
        }
    }
    close(f);
}

static void user_demo_dir_init(void)
{
    struct stat st;

    snprintf(g_share_dir, sizeof(g_share_dir), "%s", CLOONIX_SHARE_DIR);
    snprintf(g_user_demo_dir, sizeof(g_user_demo_dir),
             "%s/demos", CLOONIX_SHARE_DIR);

    /* Not created here: the directory is the installer's. */
    if (stat(CLOONIX_SHARE_DIR, &st) != 0 || !S_ISDIR(st.st_mode))
        KERR("WARNING %s is missing, the demo panel will be empty. "
             "It is created by install_cloonfs.", CLOONIX_SHARE_DIR);
    else if (access(g_user_demo_dir, W_OK) != 0)
        KERR("WARNING %s is not writable, demos cannot be saved.",
             g_user_demo_dir);
}

/* Validate a demo filename: a single path component, no traversal, no slash,
   non-empty, not starting with a dot. */
static int web_name_ok(const char *s)
{
    int i;
    if ((!s) || (!s[0]) || (s[0] == '.'))
        return 0;
    if (strstr(s, ".."))
        return 0;
    for (i = 0; s[i]; i++) {
        if ((s[i] == '/') || (s[i] == '\\'))
            return 0;
    }
    return 1;
}

static int demo_name_ok(const char *name)
{
    if (!name || !name[0] || name[0] == '.')
        return 0;
    if (strchr(name, '/') || strstr(name, ".."))
        return 0;
    if (strlen(name) >= 256)
        return 0;
    return 1;
}

static int safe_subdir(const char *sub, char *out, int outsz)
{
    if (!sub) sub = "";
    while (*sub == '/') sub++;                 /* no leading slashes */
    if (strstr(sub, ".."))                     /* no traversal */
        return 0;
    const char *p = sub;
    while (*p) {
        if (*p == '.' && (p == sub || p[-1] == '/'))
            return 0;
        p++;
    }
    if (strlen(sub) >= 512)
        return 0;
    if (sub[0])
        snprintf(out, outsz, "%s/%s", g_share_dir, sub);
    else
        snprintf(out, outsz, "%s", g_share_dir);
    return 1;
}

static void files_list(int fd, const char *query)
{
    char sub[512] = {0};
    const char *p = strstr(query, "dir=");
    if (p) { sscanf(p + 4, "%511[^&]", sub); url_decode(sub); }

    char base[1024];
    if (!safe_subdir(sub, base, sizeof(base))) {
        http_fail(fd, 400, "files/list", "bad directory"); return;
    }

    char dirs[256][256];  int nd = 0;
    char files[256][256]; int nf = 0;
    DIR *d = opendir(base);
    if (d) {
        struct dirent *e;
        while ((e = readdir(d)) && (nd < 256 || nf < 256)) {
            if (e->d_name[0] == '.')
                continue;
            char full[1280];
            snprintf(full, sizeof(full), "%s/%s", base, e->d_name);
            struct stat st;
            if (stat(full, &st) != 0)
                continue;
            if (S_ISDIR(st.st_mode) && nd < 256)
                snprintf(dirs[nd++], sizeof(dirs[0]), "%s", e->d_name);
            else if (S_ISREG(st.st_mode) && nf < 256)
                snprintf(files[nf++], sizeof(files[0]), "%s", e->d_name);
        }
        closedir(d);
    }
    for (int i = 1; i < nd; i++) {
        char t[256]; memcpy(t, dirs[i], sizeof(t)); int j = i - 1;
        while (j >= 0 && strcmp(dirs[j], t) > 0) { memcpy(dirs[j+1], dirs[j], sizeof(dirs[0])); j--; }
        memcpy(dirs[j+1], t, sizeof(dirs[0]));
    }
    for (int i = 1; i < nf; i++) {
        char t[256]; memcpy(t, files[i], sizeof(t)); int j = i - 1;
        while (j >= 0 && strcmp(files[j], t) > 0) { memcpy(files[j+1], files[j], sizeof(files[0])); j--; }
        memcpy(files[j+1], t, sizeof(files[0]));
    }

    char body[512 * 260 + 64];
    int  bl = 0;
    bl += snprintf(body + bl, sizeof(body) - bl, "{\"dir\":\"%s\",\"dirs\":[", sub);
    for (int i = 0; i < nd; i++)
        bl += snprintf(body + bl, sizeof(body) - bl, "%s\"%s\"", i ? "," : "", dirs[i]);
    bl += snprintf(body + bl, sizeof(body) - bl, "],\"files\":[");
    for (int i = 0; i < nf; i++)
        bl += snprintf(body + bl, sizeof(body) - bl, "%s\"%s\"", i ? "," : "", files[i]);
    bl += snprintf(body + bl, sizeof(body) - bl, "]}");

    char hdr[256];
    int hl = snprintf(hdr, sizeof(hdr),
        "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
        "Content-Length: %d\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n", bl);
    (void)write(fd, hdr, hl);
    (void)write(fd, body, bl);
}

static void files_get(int fd, const char *query)
{
    char name[256] = {0}, sub[512] = {0};
    const char *p = strstr(query, "name=");
    if (p) { sscanf(p + 5, "%255[^&]", name); url_decode(name); }
    p = strstr(query, "dir=");
    if (p) { sscanf(p + 4, "%511[^&]", sub); url_decode(sub); }
    char base[1024];
    if (!demo_name_ok(name) || !safe_subdir(sub, base, sizeof(base))) {
        http_fail(fd, 400, "files/get", "bad demo name or directory"); return;
    }
    char full[1300];
    snprintf(full, sizeof(full), "%s/%s", base, name);
    int f = open(full, O_RDONLY);
    if (f < 0) {
        KERR("WARNING files/get: no such demo %s", full);
        http_fail(fd, 404, "files/get", "no such demo"); return;
    }
    struct stat st; fstat(f, &st);
    char hdr[256];
    int hl = snprintf(hdr, sizeof(hdr),
        "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n"
        "Content-Length: %lld\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n",
        (long long)st.st_size);
    if (util_write_all(fd, hdr, hl)) {
        close(f);
        return;
    }
    char buf[BUF_SIZE];
    ssize_t rn;
    while ((rn = read(f, buf, sizeof(buf))) > 0) {
        if (util_write_all(fd, buf, (int)rn)) {
            KERR("WARNING http: %s cut while sending", full);
            break;
        }
    }
    close(f);
}

static void ctl_topo(int fd)
{
    int len = 0;
    char *json = topo_web_get_snapshot_json(&len);
    char head[256];
    int hlen;

    if ((!json) || (len <= 0)) {
        const char *body = "topology not known yet\n";
        char ko[256];
        int kl = snprintf(ko, sizeof(ko),
                          "HTTP/1.1 503 Service Unavailable\r\n"
                          "Content-Type: text/plain\r\n"
                          "Content-Length: %d\r\n"
                          "Connection: close\r\n\r\n%s",
                          (int) strlen(body), body);
        (void) write(fd, ko, kl);
        if (json)
            clownix_free(json, __FUNCTION__);
        return;
    }
    hlen = snprintf(head, sizeof(head),
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: application/json\r\n"
                    "Cache-Control: no-store\r\n"
                    "Content-Length: %d\r\n"
                    "Connection: close\r\n\r\n", len);
    (void) write(fd, head, hlen);
    (void) write(fd, json, len);
    clownix_free(json, __FUNCTION__);
}
