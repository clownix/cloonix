#!/usr/bin/env python3
"""sub_systemd_image - the systemd container image (TESTITALL_SYSTEMD, default
trixie_systemd) started with startup_env=CLOONIX_WITH_SYSTEMD=1, the
opt-in cloonixsbininit reads: pid 1 is systemd, the system reaches running
or degraded, ssh and scp work in it, a service can be started, and del
leaves nothing; without the opt-in pid 1 stays cloonixsbininit on the same
image. Skipped when the image is not in bulk.
"""
import os
import subprocess
import tempfile
from sublib import *

C = "tisubSd"
IMG = os.environ.get("TESTITALL_SYSTEMD", "trixie_systemd")


def sub_systemd_image():
    clean(C)
    if not in_bulk(IMG, images()):
        print("      (%s not in dump images: not checked)" % IMG)
        return
    tmp = tempfile.mkdtemp(prefix="testitall_")
    try:
        add_cnt(C, "s", rootfs=IMG)
        wait_answer(C)
        comm = ssh(C, "cat /proc/1/comm").strip()
        if comm != "cloonixsbininit":
            raise Fail("pid 1 of %s without the opt-in is %s" % (IMG, comm))
        delete(C)
        add_cnt(C, "s", ("startup_env=CLOONIX_WITH_SYSTEMD=1",),
                rootfs=IMG)
        wait_answer(C)
        comm = ssh(C, "cat /proc/1/comm").strip()
        why = ssh(C, "cat /run/cloonix_no_systemd 2>/dev/null; true").strip()
        if comm == "cloonixsbininit" and why:
            # a toy with no cgroup of its own: the starter fell back, saying so
            print("      (%s)" % why)
            delete(C)
            return
        if comm != "systemd":
            raise Fail("pid 1 of %s with the opt-in is %s" % (IMG, comm))
        state = ssh(C, "systemctl is-system-running --wait; true",
                    timeout=180).strip()
        if state not in ("running", "degraded"):
            raise Fail("systemd is %r in %s:\n%s"
                       % (state, IMG, ssh(C, "systemctl --failed --no-pager")))
        print("      systemd is %s" % state)
        ssh(C, "systemctl is-active rsyslog || systemctl start rsyslog")
        if ssh(C, "systemctl is-active rsyslog; true").strip() != "active":
            raise Fail("rsyslog is not active in %s" % IMG)
        src = os.path.join(tmp, "put.txt")
        open(src, "w").write("systemd image\n")
        must(cli("scp", src, "%s:/tmp/" % C), "scp")
        if ssh(C, "cat /tmp/put.txt").strip() != "systemd image":
            raise Fail("scp into the systemd image lost the file")
        delete(C)
        r = subprocess.run(["pgrep", "-af", "crun.*%s" % C],
                           capture_output=True, text=True)
        if r.stdout.strip():
            raise Fail("crun left for %s:\n%s" % (C, r.stdout))
    finally:
        clean(C)
        subprocess.run(["rm", "-rf", tmp])


if __name__ == "__main__":
    run("systemd image", sub_systemd_image)
