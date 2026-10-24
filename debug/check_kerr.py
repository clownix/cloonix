#!/usr/bin/env python3
r"""check_kerr - does every path that leaves the normal one say so in a KERR?

    ./check_kerr.py           exit 1 on a finding
    ./check_kerr.py -v        the waived sites too

The C cloonix writes itself, not the vendored qemu, ovs, passt, crun, dtach
or webshark, nor cxp_cli, a command line tool telling its user on stderr.
Four shapes:

  refusal  a send_status_ko with no KERR or KOUT in its block
  syscall  the failure branch of a system call with no KERR or KOUT, unless
           it names the errno it expects there (EAGAIN, EINTR, ENOENT...)
  default  the default: of a switch with no KERR or KOUT before its end
  ktrc     a KTRC whose text says a fault, a report a trace build only shows

Found the first time: 28 silent refusals by hand, then this scan. A site
that must stay as it is goes in WAIVED with its reason.
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "..", "cloonix", "cloonsrc")
DIRS = ("uml_cloonix_switch", "doorways", "suid_power", "ovs_drv", "ovs_snf",
        "ovs_nat_passt", "ovs_a2b", "ovs_c2c", "proxymous_passt",
        "lib_io_clownix", "lib_rpc_clownix", "insider_agents",
        "jscloonix/jscloonix", "glob_include")
SKIP = ("/webshark/",)

LOUD = re.compile(r"\b(KERR|KERR_HEAD|KOUT|refuse|refuse_air|cxp_log|srv_log|"
                  r"agent_kerr)\s*\(")
CALLS = ("socket bind connect listen accept accept4 socketpair setsockopt "
         "pipe pipe2 fork vfork clone execv execve execvp execl execlp "
         "mkdir mount umount umount2 setns unshare chdir chroot pivot_root "
         "chmod fchmod chown fchown lchown rename symlink link truncate "
         "ftruncate mmap epoll_create epoll_create1 epoll_ctl timerfd_create "
         "timerfd_settime eventfd signalfd inotify_init inotify_init1 "
         "inotify_add_watch prctl setuid setgid setresuid setresgid setgroups "
         "pthread_create malloc calloc realloc strdup fopen fdopen opendir "
         "open openat dup dup2 dup3 ioctl fcntl sendmsg recvmsg sendto "
         "recvfrom write send writev read recv readv getaddrinfo sigaction "
         "setrlimit posix_spawn").split()
CALL = r"\b(%s)\s*\(" % "|".join(CALLS)
PTR_CALL = r"\b(malloc|calloc|realloc|strdup|fopen|fdopen|opendir)\s*\("
FAILS = re.compile(r"(<\s*0\b|==\s*-1\b|==\s*NULL\b|==\s*MAP_FAILED\b)")
EXPECTED = re.compile(r"\b(EAGAIN|EWOULDBLOCK|EINTR|EINPROGRESS|ENOENT|EEXIST|"
                      r"ESRCH|ECHILD|ENOTCONN|EBADF|EPIPE|ECONNRESET|"
                      r"ECONNREFUSED|ENOTDIR|EISCONN|EALREADY)\b")
FAULT = re.compile(r"(refus|fail|error|cannot|can't|could not|bad |lost|"
                   r"wrong|unknown|missing|dropped|too long|overflow|"
                   r"impossible|unexpected|corrupt|invalid|broken)", re.I)

# (file, function, kind): why the site stays as it is
WAIVED = {
    ("ovs.c", "ovs_destroy_test", "ktrc"): "teardown progress, one beat in 20",
    ("ovs_snf.c", "ovs_snf_resp_msg_add_lan", "ktrc"):
        "the snf is being stopped, the refusal changes nothing",
    ("qga_dialog.c", "qga_rx_cb", "ktrc"):
        "ordinary while qemu-ga settles, running out of retries is the KERR",
    ("crun.c", "step_target", "ktrc"): "follows the KERR of its create_net",
    ("udp.c", "rx_dgram_one", "ktrc"):
        "the opening of a tunnel, before the far end knew this end",
    ("c2c_peer.c", "peer_err_cb", "ktrc"): "a channel its own delete closed",
    ("main.c", "check_and_set_uid", "ktrc"):
        "proxymous is not setuid, the euid of a user's net stays the user's",
    ("udp_proxy.c", "from_crun_err_cb", "ktrc"): "its proxy was freed already",
    ("udp_proxy.c", "from_udp_err_cb", "ktrc"): "its proxy was freed already",
    ("udp_proxy.c", "from_crun_rx_one", "ktrc"):
        "the first frame before the peer is known, every 100th is a KERR",
    ("tx_process.c", "tx_try_send_chunk", "ktrc"): "the peer gone: nominal",
    ("pcap_relay.c", "pcap_relay_snf_gone", "refusal"):
        "the snf stops on request, its subscription ends with it",
    ("recv_add_lan.c", "local_add_lan", "refusal"): "cnt_add_lan said why",
    ("recv_add_lan.c", "recv_del_lan_endp", "refusal"): "cnt_del_lan said why",
    ("recv_from_client.c", "recv_c2c_cnf", "refusal"):
        "ovs_c2c_mac_mangle said why",
    ("utils_cmd_line_maker.c", "utils_send_status_ko", "refusal"):
        "the helper, each caller is scanned",
    ("event_subscriber.c", "event_trace_sub_evt", "default"):
        "recv_trace_sub refuses with a KERR",
    ("topo_web.c", "etype_word", "default"): "a mapping, none is a value",
    ("topo_web.c", "recv_topo_small_event", "default"):
        "the other events change nothing here",
    ("json_interface.c", "doors_io_basic_decoder_one", "default"):
        "not of this layer: the caller tries its next decoder and says a "
        "message none of them knows",
    ("web_ws.c", "http_reason", "default"): "a mapping",
    ("cxp_wire.c", "cxp_json_str", "default"): "a character copied as is",
    ("kvm.c", "tap_is_in_the_netns", "syscall"): "a probe, -1 is its answer",
    ("ovs_snf.c", "snf_pid_is_this_tap", "syscall"): "a /proc scan",
    ("ovs_snf.c", "snf_find_pid_of_dumpcap", "syscall"): "a /proc scan",
    ("recv_from_client.c", "read_sys_file", "syscall"): "a probe of /sys",
    ("recv_from_client.c", "pid_measure", "syscall"):
        "the process may be gone",
    ("utils_cmd_line_maker.c", "qemu_ended", "syscall"):
        "no log is an empty one",
    ("crun_utils.c", "precopy_file", "syscall"):
        "counted, the first unreadable is told after the copy",
    ("crun_utils.c", "lift_scope_cpu", "syscall"): "why goes to the caller",
    ("crun_utils.c", "crun_utils_mount_exists", "syscall"):
        "/proc/1 hidden in a toy, /proc/self is tried next",
    ("crun_utils.c", "crun_utils_umount_tree", "syscall"):
        "/proc/1 hidden in a toy, /proc/self is tried next",
    ("guard.c", "guard_pid_is_callers", "syscall"): "a /proc probe",
    ("ovs_drv.c", "proc_line", "syscall"): "a /proc reader",
    ("ovs_popen.c", "ovs_popen_read_lines", "syscall"): "no file, nothing said",
    ("ovs_popen.c", "ovs_popen_last_said", "syscall"): "no file, nothing said",
    ("ovs_popen.c", "log_what_the_command_said", "syscall"):
        "no file, nothing said",
    ("passt_shuttle.c", "passt_shuttle_connect", "syscall"):
        "timer_passt_connect tries again until passt listens",
    ("dir_watch.c", "read_ppid", "syscall"): "a /proc reader",
    ("passt_spawn.c", "find_host_dns", "syscall"): "the next file is tried",
    ("file_read_write.c", "dev_file_rw_ko", "syscall"): "why goes to the caller",
    ("cloonix_cxp_serv.c", "cxp_log", "syscall"): "the logger itself",
    ("wifi.c", "request", "syscall"): "-errno goes to the caller",
    ("wifi.c", "radios_create", "syscall"): "why goes to the caller",
    ("web_vnc.c", "proxy_to_vnc_unix", "syscall"):
        "a browser asks for optional files, a 404 answers it",
    ("intf_flags.c", "tap_open", "syscall"): "err goes to the caller",
    ("pthexec.c", "pthexec_running_in_crun", "syscall"): "a probe",
    ("qcow2_hdr.c", "qcow2_create_derived", "syscall"):
        "a module of its own, qemu_automaton says why",
    ("pcap_filter.c", "pcap_filter_compile", "syscall"):
        "a module of its own, err goes to pcap_relay",
}


def strip(text):
    """Comments blanked, string contents x'd, every offset kept."""
    out, i, n = list(text), 0, len(text)
    while i < n:
        c = text[i]
        if text.startswith("/*", i):
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            for k in range(i, j):
                if out[k] != "\n":
                    out[k] = " "
            i = j
        elif text.startswith("//", i):
            j = text.find("\n", i)
            j = n if j < 0 else j
            for k in range(i, j):
                out[k] = " "
            i = j
        elif c in "\"'":
            j = i + 1
            while j < n and text[j] != c:
                j += 2 if text[j] == "\\" else 1
            for k in range(i + 1, min(j, n)):
                if out[k] != "\n":
                    out[k] = "x"
            i = j + 1
        else:
            i += 1
    return "".join(out)


def close_of(code, i, op="(", cl=")"):
    """Index just past the match of the opener at i."""
    depth = 0
    for j in range(i, len(code)):
        if code[j] == op:
            depth += 1
        elif code[j] == cl:
            depth -= 1
            if depth == 0:
                return j + 1
    return len(code)


def block_of(code, pos):
    """The innermost {...} holding pos."""
    depth = 0
    for j in range(pos, -1, -1):
        if code[j] == "}":
            depth += 1
        elif code[j] == "{":
            if depth == 0:
                return code[j:close_of(code, j, "{", "}")]
            depth -= 1
    return code


def body_after(code, i):
    """The statement or block starting at i."""
    while i < len(code) and code[i].isspace():
        i += 1
    if i < len(code) and code[i] == "{":
        return code[i:close_of(code, i, "{", "}")]
    depth = 0
    for j in range(i, len(code)):
        if code[j] in "([":
            depth += 1
        elif code[j] in ")]":
            depth -= 1
        elif code[j] == ";" and depth == 0:
            return code[i:j + 1]
    return code[i:]


def function_at(code, pos):
    """Name of the function the offset is in."""
    best = "?"
    for m in re.finditer(r"^[A-Za-z_][\w \*]*?\b(\w+)\s*\([^;{]*?\)\s*\{",
                         code[:pos], re.M | re.S):
        best = m.group(1)
    return best


def scan_file(path, code, raw):
    found = []

    def add(kind, pos, what):
        line = code.count("\n", 0, pos) + 1
        found.append((kind, path, line, function_at(code, pos), what))

    for m in re.finditer(r"\b(utils_)?send_status_ko\s*\(", code):
        if not LOUD.search(block_of(code, m.start())):
            add("refusal", m.start(), "send_status_ko, nothing logged")
    for m in re.finditer(r"\bif\s*\(", code):
        c0 = m.end() - 1
        c1 = close_of(code, c0)
        cond = code[c0 + 1:c1 - 1]
        hit = re.search(CALL, cond) and (
            FAILS.search(cond) or
            (re.match(r"\s*!", cond) and re.search(PTR_CALL, cond)))
        if not hit:
            v = re.match(r"\s*\(?\s*(\w+)\s*\)?\s*(<\s*0|==\s*-1|==\s*NULL)\s*$",
                         cond) or re.match(r"\s*!\s*(\w+)\s*$", cond)
            if v:
                back = code[max(0, m.start() - 400):m.start()]
                made = PTR_CALL if v.group(0).lstrip().startswith("!") \
                    else CALL
                assign = list(re.finditer(r"\b%s\s*=\s*(\([^()]*\)\s*)?%s"
                                          % (re.escape(v.group(1)), made), back))
                hit = assign and not re.search(r";[^;]*;[^;]*;\s*$",
                                               back[assign[-1].end():])
        if not hit:
            continue
        body = body_after(code, c1)
        if LOUD.search(body) or EXPECTED.search(body) or "goto" in body:
            continue
        add("syscall", m.start(), "if (%s)" % " ".join(cond.split())[:70])
    for m in re.finditer(r"\bdefault\s*:", code):
        rest = code[m.end():]
        stop = re.search(r"\bcase\b[^:]*:|\bdefault\s*:|\}", rest)
        seg = rest[:stop.start()] if stop else rest
        depth = 0
        end = len(rest)
        for j, ch in enumerate(rest):
            if ch == "{":
                depth += 1
            elif ch == "}":
                if depth == 0:
                    end = j
                    break
                depth -= 1
        seg = rest[:end] if "{" in seg else seg
        if not LOUD.search(seg):
            add("default", m.start(), "default: with nothing logged")
    for m in re.finditer(r"\bKTRC\s*\(", code):
        txt = raw[m.end():close_of(code, m.end() - 1)]
        if FAULT.search(txt):
            add("ktrc", m.start(), "KTRC" + " ".join(txt.split())[:70])
    return found


def main():
    verbose = "-v" in sys.argv
    found, waived = [], []
    for d in DIRS:
        for root, _, files in os.walk(os.path.join(SRC, d)):
            for f in sorted(files):
                path = os.path.join(root, f)
                if not f.endswith(".c") or any(s in path for s in SKIP):
                    continue
                raw = open(path, errors="replace").read()
                rel = os.path.relpath(path, SRC)
                for item in scan_file(rel, strip(raw), raw):
                    key = (os.path.basename(rel), item[3], item[0])
                    (waived if key in WAIVED else found).append(item)
    for kind, path, line, fn, what in found:
        print("%-8s %s:%d %s(): %s" % (kind, path, line, fn, what))
    if verbose:
        for kind, path, line, fn, what in waived:
            print("waived   %s:%d %s(): %s -- %s"
                  % (path, line, fn, what,
                     WAIVED[(os.path.basename(path), fn, kind)]))
    if found:
        print("%d path(s) leave the normal one without a KERR" % len(found))
        sys.exit(1)
    print("every path that leaves the normal one says so in a KERR "
          "(%d waived)" % len(waived))


if __name__ == "__main__":
    main()
