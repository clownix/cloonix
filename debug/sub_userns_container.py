#!/usr/bin/env python3
"""sub_userns_container - the privilege of a container follows the owner of
its image, as crun decides it: a root-owned image runs privileged (the
identity uid_map, real root), a user-owned one runs in a user namespace
(container root mapped to the user that started the net, the rest to subuids).
A user-owned image that came up privileged, or a root-owned one that came up
in a user namespace, is the bug this guards.
"""
import os
from sublib import *

C = "tisubUns"


def image_owner_uid():
    """The uid that owns the container image in bulk, dir or tar.gz, or None
    when neither is there to read."""
    for p in (os.path.join(BULK, CNT_ROOTFS), os.path.join(BULK, CNT_ROOTFS + ".tar.gz")):
        if os.path.exists(p):
            try:
                return os.stat(p).st_uid
            except OSError:
                return None
    return None


def sub_userns_container():
    clean(C)
    try:
        add_cnt(C, "s")
        wait_answer(C)
        m = ssh(C, "cat /proc/self/uid_map").strip()
        print("      uid_map inside %s: %s" % (C, " | ".join(m.split("\n"))))
        rows = [r.split() for r in m.splitlines() if len(r.split()) == 3]
        if not rows:
            raise Fail("no uid_map read in the container")
        inside0, outside0, count0 = (int(x) for x in rows[0])
        identity = inside0 == 0 and outside0 == 0 and count0 > 1000000
        owner = image_owner_uid()
        owner_str = "unknown" if owner is None else str(owner)

        if identity:
            print("      privileged container: identity map, real root, no "
                  "user namespace (image %s owned by %s)" % (CNT_ROOTFS, owner_str))
            if owner not in (None, 0):
                raise Fail("the container is privileged but its image %s is "
                           "owned by uid %d, not root: crun should have made a "
                           "user namespace" % (CNT_ROOTFS, owner))
        else:
            print("      user namespace: container root -> host uid %d (image "
                  "%s owned by %s)" % (outside0, CNT_ROOTFS, owner_str))
            if inside0 != 0 or count0 != 1:
                raise Fail("unexpected first uid_map row %s" % rows[0])
            if owner == 0:
                raise Fail("image %s is root-owned yet the container is a user "
                           "namespace: crun should have made it privileged"
                           % CNT_ROOTFS)
            if outside0 != os.getuid():
                print("      note: container root maps to %d, this process is %d"
                      % (outside0, os.getuid()))
            if len(rows) > 1 and int(rows[1][1]) < 65536:
                raise Fail("the subuid base %s of the second row is too low"
                           % rows[1][1])
        delete(C)
    finally:
        clean(C)


if __name__ == "__main__":
    run("userns container", sub_userns_container)
