#!/usr/bin/env python3
"""sub_cnt_limits_enforced - cnf cpu= and ram= are enforced, not only
written: under cpu=25 a spin loop gets about a quarter of a core (read from
cpu.stat over 4 s); under ram=100 a shell holding 200 MB in a variable is killed and the
container still answers; 0 lifts both and the loop gets a whole core.
"""
from sublib import *

C = "tisubL"
SPIN = "timeout 4 sh -c 'while :; do :; done'"
# 200 MB held in a shell variable: the shell is what the kernel kills.
HOLD = "x=$(head -c 200M /dev/zero | tr '\\0' a) && echo rc=0"


def cpu_share():
    a = int(ssh(C, "awk '/usage_usec/ {print $2}' /sys/fs/cgroup/cpu.stat"))
    ssh(C, SPIN, check=False)
    b = int(ssh(C, "awk '/usage_usec/ {print $2}' /sys/fs/cgroup/cpu.stat"))
    return (b - a) / 4000000.0


def sub_cnt_limits_enforced():
    if os.environ.get("TESTITALL_TOY"):
        print("      (toy: a container of the toy has no cgroup, the cap "
              "is refused there; checked by testitall.py)")
        return
    clean(C)
    try:
        add_cnt(C, "s")
        wait_answer(C)
        must(cli("cnf", C, "cpu=25", "ram=100"), "cnf cpu= ram=")
        share = cpu_share()
        print("      cpu=25: the loop got %.0f%% of a core" % (share * 100))
        if not 0.10 < share < 0.40:
            raise Fail("cpu=25 and the loop got %.0f%% of a core"
                       % (share * 100))
        out = ssh(C, HOLD, check=False)
        if "rc=0" in out:
            raise Fail("ram=100 and a 200 MB tail was not killed: %s" % out)
        wait_answer(C, timeout=20)
        must(cli("cnf", C, "cpu=0", "ram=0"), "cnf 0")
        share = cpu_share()
        print("      cpu=0: the loop got %.0f%% of a core" % (share * 100))
        if share < 0.70:
            raise Fail("cap lifted and the loop got %.0f%%" % (share * 100))
        out = ssh(C, HOLD, check=False)
        if "rc=0" not in out:
            raise Fail("ram cap lifted and 200 MB still fails: %s" % out)
        delete(C)
    finally:
        clean(C)


if __name__ == "__main__":
    run("cnt limits enforced", sub_cnt_limits_enforced)
