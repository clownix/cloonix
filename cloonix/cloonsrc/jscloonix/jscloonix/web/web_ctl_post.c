/* web_ctl_post.c — POST /ctl/... topology mutation handlers.
 * Part of the GUI server TU; see web_common.h. */

#include "web_common.h"

static void extract_json_str(const char *body, const char *key, char *out, int outsz)
{
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    out[0] = 0;
    char *p = strstr(body, pat);
    if (p) {
        p = strchr(p + strlen(pat), ':');
        if (p) {
            p++;
            while (*p == ' ') p++;
            if (*p == '"') p++;
            int i = 0;
            while (*p && *p != '"' && i < outsz-1) out[i++] = *p++;
            out[i] = 0;
        }
    }
}

/* The "type" of each entry of the "eth" array: eths, ethv or none. */
static void parse_eth_types(const char *body, t_eth_table *tab, int nb)
{
    char *p = strstr(body, "\"eth\"");
    if (p) p = strchr(p, '[');
    for (int i = 0; i < nb && p; i++) {
        char *t = strstr(p, "\"type\"");
        if (!t) break;
        t = strchr(t+6, ':'); if (!t) break;
        while (*t == ':' || *t == ' ' || *t == '"') t++;
        if (strncmp(t, "eths", 4) == 0) tab[i].endp_type = endp_type_eths;
        else if (strncmp(t, "ethv", 4) == 0) tab[i].endp_type = endp_type_ethv;
        else tab[i].endp_type = endp_type_none;
        p = t;
    }
}

static void handle_add_vm(int fd, const char *body, int blen)
{
    t_topo_kvm kvm;
    memset(&kvm, 0, sizeof(kvm));

    char *p;
    extract_json_str(body, "name", kvm.name, MAX_NAME_LEN);
    extract_json_str(body, "image", kvm.rootfs_input, MAX_PATH_LEN);
    if ((p = strstr(body, "\"cpu\":")))      sscanf(p+6,  "%d", &kvm.cpu);
    if ((p = strstr(body, "\"mem\":")))      sscanf(p+6,  "%d", &kvm.mem);
    if ((p = strstr(body, "\"nb_eth\":")))   sscanf(p+9,  "%d", &kvm.nb_tot_eth);
    if ((p = strstr(body, "\"persistent\":")))  { int v=0; sscanf(p+13,"%d",&v);
        if(v) kvm.vm_config_flags |= VM_CONFIG_FLAG_PERSISTENT; }
    if ((p = strstr(body, "\"no_qemu_ga\":"))) { int v=0; sscanf(p+13,"%d",&v);
        if(v) kvm.vm_config_flags |= VM_CONFIG_FLAG_NO_QEMU_GA; }
    if ((p = strstr(body, "\"windows\":")))    { int v=0; sscanf(p+10,"%d",&v);
        if(v) {
            kvm.vm_config_flags |= VM_CONFIG_FLAG_WINDOWS |
                                   VM_CONFIG_FLAG_FULL_VIRT |
                                   VM_CONFIG_FLAG_UEFI |
                                   VM_CONFIG_FLAG_ADDED_CDROM;
            strncpy(kvm.added_cdrom, "virtio-win.iso", MAX_PATH_LEN-1);
        } }
    if ((p = strstr(body, "\"cisco\":")))      { int v=0; sscanf(p+8,"%d",&v);
        if(v) kvm.vm_config_flags |= VM_CONFIG_FLAG_NO_QEMU_GA; }
    if ((p = strstr(body, "\"freebsd\":")))    { int v=0; sscanf(p+10,"%d",&v);
        if(v) kvm.vm_config_flags |= VM_CONFIG_FLAG_FREEBSD; }
    if ((p = strstr(body, "\"nb_vwif\":")))   sscanf(p+10, "%d", &kvm.nb_tot_nb_vwif);

    if (kvm.nb_tot_eth > MAX_ETH_VM) kvm.nb_tot_eth = MAX_ETH_VM;
    parse_eth_types(body, kvm.eth_table, kvm.nb_tot_eth);

    if (!kvm.name[0] || !kvm.rootfs_input[0]) {
        KERR("WARNING handle_add_vm: missing name=%s rootfs=%s body=%.200s",
             kvm.name, kvm.rootfs_input, body);
        http_fail(fd, 400, NULL, "Missing name or image");
        return;
    }
    topo_web_add_vm(&kvm);
    const char *ok = "HTTP/1.1 200 OK\r\nContent-Length: 3\r\nConnection: close\r\n\r\nok\n";
    (void)write(fd, ok, strlen(ok));
}

static void handle_add_cnt(int fd, const char *body, int blen)
{
    t_topo_cnt cnt;
    memset(&cnt, 0, sizeof(cnt));

    char *p;
    extract_json_str(body, "name", cnt.name, MAX_NAME_LEN);
    extract_json_str(body, "brandtype", cnt.brandtype, MAX_NAME_LEN);
    extract_json_str(body, "image", cnt.image, MAX_PATH_LEN);
    extract_json_str(body, "startup_env", cnt.startup_env, MAX_PATH_LEN);
    extract_json_str(body, "vmount", cnt.vmount, MAX_SIZE_VMOUNT);
    if ((p = strstr(body, "\"nb_eth\":")))    sscanf(p+9,  "%d", &cnt.nb_tot_eth);
    if ((p = strstr(body, "\"nb_vwif\":")))   sscanf(p+10, "%d", &cnt.nb_tot_nb_vwif);
    if ((p = strstr(body, "\"persistent\":"))) { int v=0; sscanf(p+13,"%d",&v); cnt.is_persistent=v; }

    if (cnt.nb_tot_eth > MAX_ETH_VM) cnt.nb_tot_eth = MAX_ETH_VM;
    parse_eth_types(body, cnt.eth_table, cnt.nb_tot_eth);

    if (!cnt.name[0] || !cnt.brandtype[0]) {
        KERR("WARNING handle_add_cnt: missing name=%s brand=%s body=%.200s",
             cnt.name, cnt.brandtype, body);
        http_fail(fd, 400, NULL, "Missing name or brandtype");
        return;
    }
    topo_web_add_cnt(&cnt);
    const char *ok = "HTTP/1.1 200 OK\r\nContent-Length: 3\r\nConnection: close\r\n\r\nok\n";
    (void)write(fd, ok, strlen(ok));
}

/* Simple name-only POST handlers (body: {"name":"lan01"}) */
static void handle_c2c(int fd, const char *body, int blen, const char *label)
{
    char name[MAX_NAME_LEN] = {0};
    char dest[MAX_NAME_LEN] = {0};
    char name_dest[2*MAX_NAME_LEN] = {0};
    (void)blen;
    extract_json_str(body, "name", name, MAX_NAME_LEN);
    extract_json_str(body, "dest", dest, MAX_NAME_LEN);
    if (!name[0] || !dest[0]) {
        KERR("WARNING %s: missing name/dest — body=%.200s", label, body);
        http_fail(fd, 400, NULL, "Missing name/dest");
        return;
    }
    snprintf(name_dest, sizeof(name_dest), "%s %s", name, dest);
    topo_web_add_c2c(name_dest);
    const char *ok = "HTTP/1.1 200 OK\r\nContent-Length: 3\r\nConnection: close\r\n\r\nok\n";
    (void)write(fd, ok, strlen(ok));
}

static void handle_add_phy(int fd, const char *body, int blen)
{
    char name[MAX_NAME_LEN] = {0}, type[32] = {0};
    (void)blen;
    extract_json_str(body, "name", name, MAX_NAME_LEN);
    extract_json_str(body, "type", type, (int)sizeof(type));
    if (!name[0] || !web_name_ok(name) ||
        (strcmp(type, "absorb") && strcmp(type, "macvlan"))) {
        KERR("WARNING handle_add_phy: bad params body=%.200s", body);
        http_fail(fd, 400, NULL, "Bad params");
        return;
    }
    topo_web_add_phy(name, !strcmp(type, "macvlan"));
    http_ok(fd, "ok\n");
}

static void handle_simple_name(int fd, const char *body, int blen,
                                const char *label,
                                void (*fn)(char *))
{
    char name[MAX_NAME_LEN] = {0};
    extract_json_str(body, "name", name, MAX_NAME_LEN);
    if (!name[0]) {
        KERR("WARNING %s: missing name — body=%.200s", label, body);
        http_fail(fd, 400, NULL, "Missing name");
        return;
    }
    fn(name);
    const char *ok = "HTTP/1.1 200 OK\r\nContent-Length: 3\r\nConnection: close\r\n\r\nok\n";
    (void)write(fd, ok, strlen(ok));
}

static void handle_add_edge(int fd, const char *body, int blen)
{
    char name[MAX_NAME_LEN] = {0};
    char lan[MAX_NAME_LEN]  = {0};
    int  num = -1;
    char *p;
    extract_json_str(body, "name", name, MAX_NAME_LEN);
    extract_json_str(body, "lan", lan, MAX_NAME_LEN);
    if ((p = strstr(body, "\"num\":"))) sscanf(p+6, "%d", &num);
    if (!name[0] || !lan[0] || num < 0) {
        KERR("WARNING handle_add_edge: bad params name=%s lan=%s num=%d body=%.200s",
             name, lan, num, body);
        http_fail(fd, 400, NULL, "Bad params");
        return;
    }
    topo_web_add_edge(name, num, lan);
    const char *ok = "HTTP/1.1 200 OK\r\nContent-Length: 3\r\nConnection: close\r\n\r\nok\n";
    (void)write(fd, ok, strlen(ok));
}

static void handle_del_edge(int fd, const char *body, int blen)
{
    char name[MAX_NAME_LEN] = {0};
    char lan[MAX_NAME_LEN]  = {0};
    int  num = -1;
    char *p;
    extract_json_str(body, "name", name, MAX_NAME_LEN);
    extract_json_str(body, "lan", lan, MAX_NAME_LEN);
    if ((p = strstr(body, "\"num\":"))) sscanf(p+6, "%d", &num);
    if (!name[0] || !lan[0] || num < 0) {
        KERR("WARNING handle_del_edge: bad params name=%s lan=%s num=%d", name, lan, num);
        http_fail(fd, 400, NULL, "Bad params");
        return;
    }
    topo_web_del_edge(name, num, lan);
    const char *ok = "HTTP/1.1 200 OK\r\nContent-Length: 3\r\nConnection: close\r\n\r\nok\n";
    (void)write(fd, ok, strlen(ok));
}

static void handle_snf(int fd, const char *body, int blen)
{
    char name[MAX_NAME_LEN] = {0};
    int  num = -1, on = -1;
    char *p;
    (void)blen;
    extract_json_str(body, "name", name, MAX_NAME_LEN);
    if ((p = strstr(body, "\"num\":"))) sscanf(p+6, "%d", &num);
    if ((p = strstr(body, "\"on\":")))  sscanf(p+5, "%d", &on);
    if (!name[0] || num < 0 || (on != 0 && on != 1)) {
        KERR("WARNING handle_snf: bad params name=%s num=%d on=%d body=%.200s",
             name, num, on, body);
        http_fail(fd, 400, NULL, "Bad params");
        return;
    }

    if (on == 0) {
        char key[160];
        snprintf(key, sizeof(key), "%s_%d", name, num);
        const char *who = NULL;
        if (rec_iface_is_reserved(key))        who = "the main record (PcapRec)";
        else if (snf_webshark_in_use(name, num)) who = "a running webshark";
        if (who) {
            char msg[256];
            int mlen = snprintf(msg, sizeof(msg),
                "{\"error\":\"eth%d of %s is in use by %s. Stop it before "
                "removing the snf capability.\"}", num, name, who);
            char hdr[160];
            int hlen = snprintf(hdr, sizeof(hdr),
                "HTTP/1.1 409 Conflict\r\n"
                "Content-Type: application/json\r\n"
                "Content-Length: %d\r\nConnection: close\r\n\r\n", mlen);
            (void)write(fd, hdr, hlen);
            (void)write(fd, msg, mlen);
            KERR("WARNING handle_snf: refused del-snf %s eth%d — in use by %s", name, num, who);
            return;
        }
    }

    topo_web_snf_set(name, num, on);
    const char *ok = "HTTP/1.1 200 OK\r\nContent-Length: 3\r\nConnection: close\r\n\r\nok\n";
    (void)write(fd, ok, strlen(ok));
}

static int ctl_get_name(const char *body, char *name)
{
    extract_json_str(body, "name", name, MAX_NAME_LEN);
    return (name[0] != 0);
}

static void handle_position(int fd, const char *body, int blen)
{
    char name[MAX_NAME_LEN] = {0};
    int x = 0, y = 0, got_x = 0, got_y = 0;
    char *p;
    (void)blen;
    if ((p = strstr(body, "\"x\":"))) { sscanf(p+4, "%d", &x); got_x = 1; }
    if ((p = strstr(body, "\"y\":"))) { sscanf(p+4, "%d", &y); got_y = 1; }
    if (!ctl_get_name(body, name) || !got_x || !got_y) {
        KERR("WARNING handle_position: bad params body=%.200s", body);
        http_fail(fd, 400, NULL, "Bad params");
        return;
    }
    topo_web_position(name, x, y);
    const char *ok = "HTTP/1.1 200 OK\r\nContent-Length: 3\r\nConnection: close\r\n\r\nok\n";
    (void)write(fd, ok, strlen(ok));
}

static void handle_poseth(int fd, const char *body, int blen)
{
    char name[MAX_NAME_LEN] = {0};
    int num = -1, deg = 0, got_deg = 0;
    char *p;
    (void)blen;
    if ((p = strstr(body, "\"num\":"))) sscanf(p+6, "%d", &num);
    if ((p = strstr(body, "\"deg\":"))) { sscanf(p+6, "%d", &deg); got_deg = 1; }
    if (!ctl_get_name(body, name) || (num < 0) || !got_deg) {
        KERR("WARNING handle_poseth: bad params body=%.200s", body);
        http_fail(fd, 400, NULL, "Bad params");
        return;
    }
    topo_web_poseth(name, num, deg);
    const char *ok = "HTTP/1.1 200 OK\r\nContent-Length: 3\r\nConnection: close\r\n\r\nok\n";
    (void)write(fd, ok, strlen(ok));
}

/* {"name":..,"txt":..}: the switch checks the note and broadcasts it */
static void handle_note(int fd, const char *body, int blen)
{
    char name[MAX_NAME_LEN] = {0};
    char txt[MAX_PATH_LEN] = {0};   /* a long one goes on, for the switch to refuse */
    char copy[2048];
    (void)blen;
    snprintf(copy, sizeof(copy), "%s", body);
    json_get_str(copy, "name", name, MAX_NAME_LEN);
    if (!name[0] || !json_find_key(copy, "txt")) {
        KERR("WARNING handle_note: bad params body=%.200s", body);
        http_fail(fd, 400, NULL, "Bad params");
        return;
    }
    json_get_str(copy, "txt", txt, MAX_PATH_LEN);
    topo_web_note(name, txt);
    const char *ok = "HTTP/1.1 200 OK\r\nContent-Length: 3\r\nConnection: close\r\n\r\nok\n";
    (void)write(fd, ok, strlen(ok));
}

static void handle_cnf_a2b(int fd, const char *body, int blen)
{
    char name[MAX_NAME_LEN] = {0};
    char cmd[MAX_NAME_LEN]  = {0};
    int  dir = -1, val = 0, got_val = 0;
    char *p;
    (void)blen;
    extract_json_str(body, "name", name, MAX_NAME_LEN);
    extract_json_str(body, "cmd", cmd, MAX_NAME_LEN);
    if ((p = strstr(body, "\"dir\":"))) sscanf(p+6, "%d", &dir);
    if ((p = strstr(body, "\"val\":"))) { sscanf(p+6, "%d", &val); got_val = 1; }

    /* Accept only the commands the a2b scheduler actually implements so a
       stray cmd string can never reach the a2b process. */
    int cmd_ok = (!strcmp(cmd, "loss")  || !strcmp(cmd, "delay") ||
                  !strcmp(cmd, "rate")  || !strcmp(cmd, "silentms"));
    if (!name[0] || (dir != 0 && dir != 1) || !cmd_ok || !got_val) {
        KERR("WARNING handle_cnf_a2b: bad params name=%s dir=%d cmd=%s val=%d body=%.200s",
             name, dir, cmd, val, body);
        http_fail(fd, 400, NULL, "Bad params");
        return;
    }
    topo_web_cnf_a2b(name, dir, cmd, val);
    const char *ok = "HTTP/1.1 200 OK\r\nContent-Length: 3\r\nConnection: close\r\n\r\nok\n";
    (void)write(fd, ok, strlen(ok));
}

static void handle_vnc_start(int fd, const char *body, int blen)
{
    char name[MAX_NAME_LEN] = {0};
    int  vm_id = -1;
    char *p;
    extract_json_str(body, "name", name, MAX_NAME_LEN);
    if ((p = strstr(body, "\"vm_id\":"))) sscanf(p+8, "%d", &vm_id);
    if (!name[0] || vm_id < 0) {
        http_fail(fd, 400, "ctl_post", "Bad name/vm_id"); return;
    }
    /* Build vnc socket path: snf_dir/../vm/vm<id>/vnc_sock (must match
       VNC_SOCK / utils_get_vnc_path on the switch side). */
    char sock_path[MAX_PATH_LEN];
    snprintf(sock_path, sizeof(sock_path),
             "%s/../vm/vm%d/vnc_sock", g_snf_dir, vm_id);
    pthread_mutex_lock(&g_vnc_mutex);
    int found = 0;
    for (int i = 0; i < g_nvnc; i++) {
        if (strcmp(g_vnc[i].vm, name) == 0) {
            snprintf(g_vnc[i].sock, sizeof(g_vnc[0].sock), "%s", sock_path);
            found = 1; break;
        }
    }
    if (!found && g_nvnc < MAX_VNC) {
        snprintf(g_vnc[g_nvnc].vm,   sizeof(g_vnc[0].vm),   "%s", name);
        snprintf(g_vnc[g_nvnc].sock, sizeof(g_vnc[0].sock), "%s", sock_path);
        g_nvnc++;
    }
    pthread_mutex_unlock(&g_vnc_mutex);
    char resp_body[256];
    int blen2 = snprintf(resp_body, sizeof(resp_body),
                         "{\"url\":\"/vnc-%s/\"}", name);
    char resp_hdr[256];
    int hlen = snprintf(resp_hdr, sizeof(resp_hdr),
        "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
        "Content-Length: %d\r\nConnection: close\r\n\r\n", blen2);
    (void)write(fd, resp_hdr, hlen);
    (void)write(fd, resp_body, blen2);
}

