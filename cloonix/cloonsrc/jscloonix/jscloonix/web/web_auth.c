/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
/* Who may drive the canvas: user = net name, password = cloonix_passwd,    */
/* the pair of cloonix.cfg. Http basic, because the browser attaches it by  */
/* itself to the websocket upgrades. The web port is https, so the base64   */
/* credentials never travel in clear.                                       */

static char g_auth_user[MAX_NAME_LEN];      /* the net name */
static char g_auth_pass[MSG_DIGEST_LEN];    /* its cloonix_passwd */

static int hdr_value(const char *hdr, const char *key, char *out, int outsz)
{
    const char *p = strcasestr(hdr, key);
    int i = 0;
    if (!p)
        return 0;
    p += strlen(key);
    while ((*p == ' ') || (*p == '\t'))
        p++;
    while (*p && (*p != '\r') && (*p != '\n') && (i < outsz - 1))
        out[i++] = *p++;
    out[i] = 0;
    return 1;
}

static void auth_init(const char *net)
{
    t_cloonix_conf_info *cnf = get_own_cloonix_conf_info();

    memset(g_auth_user, 0, sizeof(g_auth_user));
    memset(g_auth_pass, 0, sizeof(g_auth_pass));
    snprintf(g_auth_user, sizeof(g_auth_user), "%s", net);

    if (cnf)
        snprintf(g_auth_pass, sizeof(g_auth_pass), "%s", cnf->passwd);
    if (!g_auth_pass[0])
        KERR("WARNING no cloonix_passwd found for net %s, the web ports "
             "will refuse every browser", net);
}

static int b64_dec(const char *in, char *out, int outsz)
{
    static const char tbl[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int val = 0, bits = 0, len = 0;
    const char *p;
    for (p = in; *p && (*p != '=') && (*p != '\r') && (*p != '\n'); p++) {
        const char *q = strchr(tbl, *p);
        if ((*p == ' ') || (*p == '\t'))
            continue;
        if (!q || !*p)
            return -1;
        val = (val << 6) | (int)(q - tbl);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            if (len >= outsz - 1)
                return -1;
            out[len++] = (char)((val >> bits) & 0xFF);
        }
    }
    out[len] = 0;
    return len;
}

static int const_time_eq(const char *a, const char *b)
{
    int la = (int)strlen(a), lb = (int)strlen(b), i, diff = la ^ lb;
    for (i = 0; (i < la) && (i < lb); i++)
        diff |= (unsigned char)a[i] ^ (unsigned char)b[i];
    return diff == 0;
}

static int auth_ok(const char *hdr)
{
    char value[512], plain[512], *colon;

    if (!g_auth_pass[0]) {
        KERR("ERROR auth: no cloonix_passwd for net %s, refusing everything",
             g_auth_user);
        return 0;
    }
    if (!hdr_value(hdr, "Authorization:", value, sizeof(value)))
        return 0;
    if ((value[0] != 'B') && (value[0] != 'b')) {
        KERR("ERROR auth: Authorization is not Basic");
        return 0;
    }
    if (strncmp(value + 1, "asic ", 5)) {
        KERR("ERROR auth: Authorization is not Basic");
        return 0;
    }
    if (b64_dec(value + 6, plain, sizeof(plain)) <= 0) {
        KERR("ERROR auth: the basic credential is not base64");
        return 0;
    }
    colon = strchr(plain, ':');
    if (!colon) {
        KERR("ERROR auth: no colon in the basic credential");
        return 0;
    }
    *colon = 0;
    if (!const_time_eq(plain, g_auth_user)) {
        KERR("ERROR auth: user is not the net name, %s was offered", plain);
        return 0;
    }
    if (!const_time_eq(colon + 1, g_auth_pass)) {
        KERR("ERROR auth: wrong password for net %s", g_auth_user);
        sleep(1);
        return 0;
    }
    return 1;
}

static void auth_refuse(int fd)
{
    char rsp[512];
    int len = snprintf(rsp, sizeof(rsp),
        "HTTP/1.1 401 Unauthorized\r\n"
        "WWW-Authenticate: Basic realm=\"cloonix %s\", charset=\"UTF-8\"\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n\r\n", g_auth_user);
    (void)write(fd, rsp, len);
}
