#!/usr/bin/env python3
"""sub_targz_image - a cnt image left as <image>.tar.gz in bulk.

The server takes the tar.gz when the directory is absent: the first add
extracts it into <image>_targz, the container is created on it, and every
later add on this or any other net finds the directory. Checked
here, on an image packed from a running container of CNT_ROOTFS:

  1. the tar.gz alone: the add is ok, the guest answers, the topology names
     <image>_targz, the top of the tree is writable by its owner, so a plain
     rm -rf removes it, the modes inside are those of the image, no .tmp and
     no .log are left; the mount the container is built on is read only, a
     write through it is answered EROFS
  2. a second add is served without a new extraction
  3. --persistent is refused on the tar.gz and on the _targz name
  4. dump images shows kind tar.gz and kind targz
  5. a directory of the plain name comes first: the topology names it, and
     --persistent on it, once chmoded read only, is refused
  6. a tree without the extraction mark (an earlier extraction) is erased
     and extracted again, its modes right; a stale .tmp of a killed
     extraction is erased and the add succeeds
  7. two adds at once on a fresh tar.gz: one extraction, both ok
  8. a corrupt tar.gz: refused, the reason names the log, the log exists,
     nothing is left as a directory
  9. neither the directory nor the tar.gz: the refusal names both forms

A tree extracted by another user is used through the same idmap as any
directory of bulk; it needs a second account and is not run here.
"""
import errno
import os
import shutil
import stat
import threading
import time
from sublib import *

IMG = "tisub_tgz"
BAD = "tisub_bad"
A, B, C = "tisubTgzA", "tisubTgzB", "tisubTgzC"
NAMES = (A, B, C)
TGZ = os.path.join(BULK, IMG + ".tar.gz")
DIR = os.path.join(BULK, IMG)
EXT = os.path.join(BULK, IMG + "_targz")
TMP = os.path.join(BULK, ".%s_targz.%d.tmp" % (IMG, os.getuid()))
LOG = "%s.%d.log" % (EXT, os.getuid())


def rm_tree(path):
    if not os.path.lexists(path):
        return
    for root, dirs, files in os.walk(path):
        os.chmod(root, 0o700)
    shutil.rmtree(path)


def rm_all_forms():
    rm_tree(DIR)
    rm_tree(EXT)
    rm_tree(TMP)
    for p in (TGZ, LOG, os.path.join(BULK, "." + IMG + "_targz.lock"),
              os.path.join(BULK, BAD + ".tar.gz"),
              os.path.join(BULK, "%s_targz.%d.log" % (BAD, os.getuid())),
              os.path.join(BULK, "." + BAD + "_targz.lock")):
        if os.path.lexists(p):
            os.unlink(p)
    rm_tree(os.path.join(BULK, BAD + "_targz"))


def pack_image():
    """The tar.gz, made from inside a container running CNT_ROOTFS, as the
    realnet demo does: the tree in bulk belongs to a subuid window and is
    not readable from here, the running guest reads it as root."""
    add_cnt(C, "s")
    wait_answer(C)
    # tar answers 1 for a file that changed while read, the syslog here.
    ssh(C, "tar czf /tmp/img.tar.gz --one-file-system "
           "--exclude=./tmp/img.tar.gz --exclude=./cloonixmnt "
           "--exclude=./proc --exclude=./sys --exclude=./dev "
           "--exclude=./run -C / . ; test $? -le 1", timeout=900)
    must(cli("scp", "%s:/tmp/img.tar.gz" % C, TGZ, timeout=900),
         "scp of the packed image")
    delete(C)
    if not os.path.isfile(TGZ) or os.path.getsize(TGZ) < 1000000:
        raise Fail("%s was not written" % TGZ)


def image_of(name):
    for obj in ftopo().get("cnt", []) or []:
        if obj.get("name") == name:
            return obj.get("image", "")
    raise Fail("%s not in the topology" % name)


def refused(what, *cmd):
    r = cli(*cmd, ack=120)
    said = (r.stdout + r.stderr).strip()
    if r.returncode == 0:
        raise Fail("%s: accepted, client said %r" % (what, said))
    if not said:
        raise Fail("%s: refused with no reason" % what)
    return said


def top_mode(path, when):
    """The mode of the top of the tree, printed at each step: if something
    takes the write bit of the owner away this says after which act."""
    st = os.stat(path)
    print("      %-40s %s mode %o owner uid %d"
          % (when, os.path.basename(path), stat.S_IMODE(st.st_mode),
             st.st_uid))
    return st


def modes_wrong(path):
    """The top is writable by its owner, or rm -rf empties the tree and leaves
    its top level in place, and the modes inside are the image's: a
    tree chmoded read only breaks every daemon that drops CAP_DAC_OVERRIDE
    (named -u), so /var/tmp must still be 1777 and the setuid bit of passwd
    must have survived. /tmp is not looked at: the archive is packed from a
    running container, where /tmp is the runtime tmpfs, not the image's."""
    st = os.stat(path)
    if not st.st_mode & stat.S_IWUSR:
        return ("%s: the top has no write bit, mode %o, rm -rf cannot remove it"
                % (path, stat.S_IMODE(st.st_mode)))
    n = os.path.join(path, "var/tmp")
    if os.path.isdir(n):
        m = stat.S_IMODE(os.stat(n).st_mode)
        if m != 0o1777:
            return "%s: mode %o, the image has 1777" % (n, m)
    n = os.path.join(path, "usr/bin/passwd")
    if os.path.isfile(n) and not (os.stat(n).st_mode & stat.S_ISUID):
        return "%s: the setuid bit is gone" % n
    return None


def lower_refuses_writes(name):
    """The image is mounted read only for the container: the modes cannot
    protect the tree, they are the image's, the mount does."""
    if TOY:
        print("      (toy: the mount is inside the toy, not probed from here)")
        return
    lower = "/var/lib/cloonix/%s/cnt/%s/lower" % (NET, name)
    if not os.path.isfile(os.path.join(lower, ".cloonix_targz_3")):
        raise Fail("%s: the image is not mounted there" % lower)
    probe = os.path.join(lower, "write_probe")
    try:
        os.close(os.open(probe, os.O_WRONLY | os.O_CREAT, 0o644))
    except OSError as e:
        if e.errno != errno.EROFS:
            raise Fail("%s: refused with %s, wanted EROFS"
                       % (probe, errno.errorcode.get(e.errno, e.errno)))
        print("      write through the mount of the image: EROFS")
        return
    os.unlink(probe)
    raise Fail("%s was created: the image is mounted writable" % probe)


def act_first_add():
    t0 = time.time()
    must(cli("add", "cnt", A, "eth=1", IMG, ack=240), "add cnt on the tar.gz")
    wait_answer(A, timeout=240)
    print("      first add on the tar.gz: %.1fs" % (time.time() - t0))
    if image_of(A) != IMG + "_targz":
        raise Fail("topology names %r, wanted %s_targz" % (image_of(A), IMG))
    if not os.path.isdir(EXT):
        raise Fail("%s was not extracted" % EXT)
    top_mode(EXT, "after the first add, A running")
    w = modes_wrong(EXT)
    if w:
        raise Fail(w)
    lower_refuses_writes(A)
    if os.path.exists(TMP):
        raise Fail("%s left behind" % TMP)
    if os.path.exists(LOG):
        raise Fail("%s left behind after a good extraction" % LOG)


def act_second_add():
    mtime = os.stat(EXT).st_mtime
    t0 = time.time()
    add_cnt(B, "s", rootfs=IMG)
    wait_answer(B)
    if os.stat(EXT).st_mtime != mtime:
        raise Fail("the second add extracted again")
    print("      second add: %.1fs, no extraction" % (time.time() - t0))
    ssh(A, "ip addr add 10.77.0.1/24 dev eth0 && ip link set eth0 up")
    ssh(B, "ip addr add 10.77.0.2/24 dev eth0 && ip link set eth0 up")
    must(cli("plug", A, "eth0", "tisubLan"), "plug")
    must(cli("plug", B, "eth0", "tisubLan"), "plug")
    ping(A, "10.77.0.2")
    ssh(A, "touch /root/written_in_the_overlay")
    top_mode(EXT, "A and B running, A wrote /root")
    delete(B)
    delete(A)
    top_mode(EXT, "A and B deleted")


def act_persistent_refused():
    said = refused("--persistent on the tar.gz", "add", "cnt", C, "eth=1",
                   "--persistent", IMG)
    if "persistent" not in said or "tar.gz" not in said:
        raise Fail("the reason says nothing of the tar.gz: %r" % said)
    said = refused("--persistent on the _targz name", "add", "cnt", C,
                   "eth=1", "--persistent", IMG + "_targz")
    if "persistent" not in said:
        raise Fail("the reason says nothing of persistent: %r" % said)
    wait_topo(C, False)


def act_dump_images():
    out = must(cli("dump", "images", quiet=True), "dump images")
    rows = [ln.split() for ln in out.splitlines() if ln.strip()]
    kinds = dict((r[1], r[0]) for r in rows if len(r) >= 2)
    if kinds.get(IMG + ".tar.gz") != "tar.gz":
        raise Fail("dump images does not list %s.tar.gz as tar.gz:\n%s"
                   % (IMG, out))
    if kinds.get(IMG + "_targz") != "targz":
        raise Fail("dump images does not list %s_targz as targz:\n%s"
                   % (IMG, out))


def act_directory_first():
    os.rename(EXT, DIR)
    try:
        top_mode(DIR, "renamed to the plain name")
        add_cnt(A, "s", rootfs=IMG)
        wait_answer(A)
        top_mode(DIR, "A running on the plain name")
        if image_of(A) != IMG:
            raise Fail("with the directory present the topology names %r"
                       % image_of(A))
        delete(A)
        top_mode(DIR, "A deleted")
        w = modes_wrong(DIR)
        if w:
            raise Fail("%s: something wrote the modes of the image while "
                       "the tree was in use" % w)
        os.chmod(DIR, 0o555)
        refused("--persistent on the read only directory", "add", "cnt", C,
                "eth=1", "--persistent", IMG)
        wait_topo(C, False)
    finally:
        os.chmod(DIR, 0o755)
        os.rename(DIR, EXT)


def act_stale_tree():
    """A tree of an earlier extraction: the server marks each tree it makes
    and redoes one without the mark. Made here by chmoding var/tmp of the
    tree and the top to 555, as the old extractions left them, and putting
    the old mark in place of the current one."""
    marker = os.path.join(EXT, ".cloonix_targz_3")
    if not os.path.isfile(marker):
        raise Fail("%s: no extraction mark in the tree" % marker)
    os.unlink(marker)
    open(os.path.join(EXT, ".cloonix_targz_2"), "w").close()
    os.chmod(os.path.join(EXT, "var/tmp"), 0o555)
    os.chmod(EXT, 0o555)
    t0 = time.time()
    must(cli("add", "cnt", A, "eth=1", IMG, ack=240), "add cnt over an unmarked tree")
    wait_answer(A, timeout=240)
    print("      add on a tree without the mark: %.1fs" % (time.time() - t0))
    if not os.path.isfile(marker):
        raise Fail("the tree without the mark was not extracted again")
    w = modes_wrong(EXT)
    if w:
        raise Fail("after the re-extraction: %s" % w)
    delete(A)


def act_stale_tmp():
    rm_tree(EXT)
    os.mkdir(TMP)
    open(os.path.join(TMP, "junk"), "w").write("half written\n")
    must(cli("add", "cnt", A, "eth=1", IMG, ack=240), "add cnt over a stale tmp")
    wait_answer(A, timeout=240)
    if os.path.exists(TMP):
        raise Fail("the stale %s was kept" % TMP)
    delete(A)


def act_two_at_once():
    rm_tree(EXT)
    out = {}

    def one(name):
        out[name] = cli("add", "cnt", name, "eth=1", IMG, ack=240)
    th = [threading.Thread(target=one, args=(n,)) for n in (A, B)]
    for t in th:
        t.start()
    for t in th:
        t.join()
    for n in (A, B):
        must(out[n], "add cnt %s during a shared extraction" % n)
    for n in (A, B):
        wait_answer(n, timeout=240)
    if os.path.exists(TMP):
        raise Fail("%s left behind" % TMP)
    delete(A)
    delete(B)


def act_corrupt():
    bad = os.path.join(BULK, BAD + ".tar.gz")
    open(bad, "wb").write(b"this is not a gzip stream\n" * 1000)
    said = refused("corrupt tar.gz", "add", "cnt", C, "eth=1", BAD)
    log = os.path.join(BULK, "%s_targz.%d.log" % (BAD, os.getuid()))
    # the server names the log as it sees bulk, which in a toy is not the
    # host's path: the file name is what both agree on
    if os.path.basename(log) not in said:
        raise Fail("the reason does not name %s: %r" % (log, said))
    if not os.path.isfile(log):
        raise Fail("%s was not written" % log)
    if os.path.exists(os.path.join(BULK, BAD + "_targz")):
        raise Fail("%s_targz exists after a failed extraction" % BAD)
    wait_topo(C, False)


def act_neither():
    said = refused("no image at all", "add", "cnt", C, "eth=1",
                   "tisub_no_such_image")
    if "tisub_no_such_image.tar.gz" not in said:
        raise Fail("the refusal does not name the tar.gz form: %r" % said)
    wait_topo(C, False)


def sub_targz_image():
    clean(*NAMES)
    rm_all_forms()
    try:
        pack_image()
        act_first_add()
        act_second_add()
        act_persistent_refused()
        act_dump_images()
        act_directory_first()
        act_stale_tree()
        act_stale_tmp()
        act_two_at_once()
        act_corrupt()
        act_neither()
    finally:
        clean(*NAMES)
        rm_all_forms()


if __name__ == "__main__":
    run("tar.gz image", sub_targz_image)
