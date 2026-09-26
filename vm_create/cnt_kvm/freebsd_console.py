#!/usr/bin/env python3
"""freebsd_console - run the scripted freebsd install and watch it.

    ./freebsd_console.py <install.iso> <disk.qcow2>

Called by kvm_freebsd.sh. Standard library only, like the cloonix client.

WHY AN INSTALL AND NOT A CONFIGURED RELEASE IMAGE

  This drove the release vm image over its serial console once. It cannot:
  FreeBSD-*-RELEASE-amd64.qcow2 has NO serial console, boots to the video
  console only, and with no display prints nothing at all -- not the loader,
  not the kernel, not a login. Giving it one means writing /boot.config and
  /boot/loader.conf from inside, which needs the way in that was being
  sought. There is no way round that from the host: the root is UFS and
  linux mounts UFS2 read only.

  So the image is INSTALLED instead. bsdinstall reads /etc/installerconfig
  off the install media and runs with no console at all, and kvm_freebsd.sh
  rebuilds the iso to carry one. That also settles the console problem at
  its root, twice over:

    on the media       the rebuilt iso turns the serial console on, so the
                       install is watchable rather than blind
    in the new system  the post-install script writes /boot.config and
                       /boot/loader.conf, so every later boot has one

  Nothing here has to READ the console to succeed. This file streams it and
  waits for qemu to exit, which is what the last line of the installerconfig
  makes happen. The console is evidence, not a control channel -- which is
  the whole difference from what was here before.

HOW IT REACHES THE INTERNET

  Through cloonix-passt, and NOT "-nic user": the cloonix qemu is built
  --without-default-features, so slirp is not compiled into it. passt is
  what cloonix itself uses to give a VM the internet -- see
  proxymous_passt/src/passt_spawn.c -- and it presents the L2 socket that
  qemu's "-netdev stream" speaks. The install needs it for one thing only,
  the qemu-guest-agent package, and it is the documented exception to the
  local-mirror rule every other builder here follows.
"""
import os
import pty
import re
import select
import signal
import subprocess
import sys
import time

CLOONFS = "/usr/libexec/cloonix/cloonfs"
QEMU_BIN = CLOONFS + "/bin/cloonix-qemu-system"
QEMU_DIR = CLOONFS + "/bin/qemu"
PASST_BIN = CLOONFS + "/bin/cloonix-passt"

RAM = 2048
CPU = 2

# The subnet passt presents to this one guest.
PASST_SOCK = "/tmp/kvm_freebsd_passt.sock"
PASST_PID = "/tmp/kvm_freebsd_passt.pid"
PASST_ADDR = "10.117.0.10"
PASST_MASK = "255.255.255.0"
PASST_GW = "10.117.0.1"
PASST_DNS_FALLBACK = "9.9.9.9"

# An install is not quick: the base packages off the media, then a package
# fetch.
INSTALL_TIMEOUT = 3600
# Nothing on the console for this long means it is not working any more.
QUIET_TIMEOUT = 900

# The prompts that are answered, and the ONLY ones.
ANSWERS = [
    (re.compile(r"Console type \[[^\]]*\]:\s*$"), "\n",
     "the terminal type prompt, taking the default"),
]
MAX_ANSWERS = 4

BAD = [
    (re.compile(r"bsdinstall:.*[Ee]rror"), "the installer reported an error"),
    (re.compile(r"Abort trap"), "something in the installer crashed"),
    (re.compile(r"panic:"), "the installer kernel panicked"),
    (re.compile(r"Updating repositories failed|Fetching packages failed"),
     "pkg could not read the base packages off the media, and the "
     "installer\nis asking whether to try again"),
    (re.compile(r"mountroot>"), "the install kernel could not mount its "
                                "root: the iso did not attach"),
    (re.compile(r"Continue with FreeBSD installation|"
                r"Welcome to FreeBSD\..*Would you like to"),
     "the installer went INTERACTIVE, so /etc/installerconfig was not "
     "read.\nIt is on the media -- the build checks that -- so this "
     "release looks\nfor it somewhere else"),
]
DONE = re.compile(r"CLOONIX_INSTALL_DONE")

class ConsoleError(Exception):
    pass

# passt, the internet for this one guest

G_DNS = None

def host_dns():
    """The host's first real nameserver.

    Used TWICE and it has to be the same address both times: passt is told
    to intercept queries sent to it, and the guest is told to send there.
    The same pairing proxymous_passt/src/passt_spawn.c sets up for a cloonix
    nat. The same two files and the same loopback skip as find_host_dns()
    there: a systemd-resolved stub at 127.0.0.53 listens in the host's
    namespace and means nothing inside a guest.
    """
    global G_DNS
    if G_DNS:
        return G_DNS
    for path in ("/run/systemd/resolve/resolv.conf", "/etc/resolv.conf"):
        try:
            with open(path) as f:
                for line in f:
                    parts = line.split()
                    if len(parts) == 2 and parts[0] == "nameserver":
                        ip = parts[1]
                        if not ip.startswith("127.") and ip != "::1":
                            G_DNS = ip
                            return G_DNS
        except OSError:
            continue
    print("[freebsd] no usable host nameserver found, using %s"
          % PASST_DNS_FALLBACK, flush=True)
    G_DNS = PASST_DNS_FALLBACK
    return G_DNS

class Passt:
    """The vendored passt, for the life of the install."""

    def __init__(self, log=None):
        self.proc = None
        self.log = log

    def __enter__(self):
        if not os.path.exists(PASST_BIN):
            raise ConsoleError(
                "no %s.\n"
                "  The install fetches one package, and passt is how"
                " cloonix gives\n  a VM the internet. Install cloonix first."
                % PASST_BIN)
        for path in (PASST_SOCK, PASST_PID):
            try:
                os.unlink(path)
            except OSError:
                pass
        argv = [PASST_BIN,
                "--socket", PASST_SOCK,
                "--foreground",
                "--quiet",
                "--dns-forward", host_dns(),
                "--mtu", "1500",
                "-a", PASST_ADDR,
                "-n", PASST_MASK,
                "-g", PASST_GW,
                # Egress only: nothing outside has any business reaching a
                # guest that exists for the length of one install.
                "-t", "none",
                "-u", "none",
                "--pid", PASST_PID]
        self.proc = subprocess.Popen(
            argv, stdout=self.log or subprocess.DEVNULL,
            stderr=subprocess.STDOUT)
        # The socket has to exist before qemu connects to it.
        for _ in range(100):
            if os.path.exists(PASST_SOCK):
                return self
            if self.proc.poll() is not None:
                raise ConsoleError(
                    "passt exited at once (status %d). Its output is in the"
                    " console log." % self.proc.returncode)
            time.sleep(0.1)
        raise ConsoleError("passt never created %s" % PASST_SOCK)

    def __exit__(self, *exc):
        if self.proc and self.proc.poll() is None:
            self.proc.terminate()
            try:
                self.proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.proc.kill()
                self.proc.wait()
        for path in (PASST_SOCK, PASST_PID):
            try:
                os.unlink(path)
            except OSError:
                pass
        return False

# the install

def qemu_stream_check():
    """Three answers, not two: "yes", "no", or "the probe told me nothing".

    -machine q35 with it, because qemu handles -netdev after the machine is
    made and this one is built --without-default-devices, so there is no
    default machine to fall back on. Without it the probe answers "No
    machine specified" and says nothing about netdevs at all.

    That distinction is the point. An earlier version returned a bool, read
    the machine error as "no stream", and stopped a build over a qemu that
    was perfectly capable. A list that was printed and has no stream in it
    is a real no; anything else is not a no, it is nothing.
    """
    try:
        r = subprocess.run([QEMU_BIN, "-L", QEMU_DIR, "-machine", "q35",
                            "-netdev", "help"],
                           capture_output=True, text=True, timeout=30)
        out = r.stdout + r.stderr
    except (OSError, subprocess.SubprocessError) as err:
        return "unknown", str(err)
    if re.search(r"\bstream\b", out):
        return "yes", out
    if "netdev backend" in out.lower():
        return "no", out
    return "unknown", out

class Install:
    """qemu running the scripted install, and its serial line."""

    def __init__(self, iso, qcow2, log=None):
        self.iso = iso
        self.qcow2 = qcow2
        self.log = log
        self.fd = -1
        self.pid = -1
        self.buf = ""
        self.saw_done = False
        self.errpath = "/tmp/freebsd_qemu_stderr.log"
        self.status = None
        self.answered = 0

    def argv(self):
        return [QEMU_BIN,
                "-L", QEMU_DIR,
                "-enable-kvm", "-nodefaults", "-no-reboot",
                "-machine", "q35,accel=kvm,usb=off,dump-guest-core=off,"
                            "vmport=off,hpet=off",
                "-cpu", "host",
                "-m", str(RAM), "-smp", str(CPU),
                "-boot", "d",
                "-display", "none",
                "-chardev", "stdio,id=char0,signal=off",
                "-serial", "chardev:char0",
                "-device", "virtio-scsi-pci,id=virtio_scsi_pci0",
                # aio=threads and not io_uring, which is what BLOCKNAME uses.
                "-blockdev", "driver=file,cache.direct=on,aio=threads,"
                             "filename=%s,node-name=lolfile" % self.qcow2,
                "-blockdev", "driver=qcow2,cache.direct=on,"
                             "node-name=lolqcow2,file=lolfile",
                "-device", "scsi-hd,drive=lolqcow2,"
                           "discard_granularity=4096,bootindex=1",
                "-drive", "file=%s,if=none,media=cdrom,id=cd" % self.iso,
                "-device", "virtio-scsi-pci",
                "-device", "scsi-cd,drive=cd,bootindex=0",
                "-netdev", "stream,id=net0,server=off,"
                           "addr.type=unix,addr.path=%s" % PASST_SOCK,
                # romfile=: efi-virtio.rom is not in cloonix's pc-bios, qemu would exit.
                "-device", "virtio-net-pci-non-transitional,romfile=,"
                           "netdev=net0,addr=0x5"]

    def open(self):
        if not os.path.exists(QEMU_BIN):
            raise ConsoleError("no %s: install cloonix first" % QEMU_BIN)
        if not os.access("/dev/kvm", os.W_OK):
            raise ConsoleError("/dev/kvm not writable by %s"
                               % (os.environ.get("USER") or os.getuid()))
        argv = self.argv()
        # qemu's OWN messages go to a file, not to the pty.
        errfd = os.open(self.errpath, os.O_RDWR | os.O_CREAT | os.O_TRUNC,
                        0o600)
        pid, fd = pty.fork()
        if pid == 0:
            try:
                os.dup2(errfd, 2)
                os.close(errfd)
                os.execve(argv[0], argv,
                          {"TERM": "vt100", "LANG": "C", "LC_ALL": "C",
                           "PATH": "/usr/bin:/bin:/usr/sbin:/sbin"})
            finally:
                os._exit(1)
        os.close(errfd)
        self.pid = pid
        self.fd = fd
        return self

    def cmdline(self):
        """The argv as one line that can be pasted into a shell.

        Printed on a refusal because the next thing anyone does is run it
        by hand with a bit taken out, and reassembling it from a list one
        item per line is work nobody should have to do.
        """
        out, argv = [], self.argv()
        i = 0
        while i < len(argv):
            a = argv[i]
            if a.startswith("-") and i + 1 < len(argv) and \
               not argv[i + 1].startswith("-"):
                out.append("%s %s" % (a, argv[i + 1]))
                i += 2
            else:
                out.append(a)
                i += 1
        return "  " + " \\\n    ".join(out)

    def write(self, text):
        """Type into the guest. Used only for the ANSWERS above."""
        try:
            os.write(self.fd, text.encode())
        except OSError:
            pass

    def qemu_said(self):
        """Whatever qemu wrote on its own error stream."""
        try:
            with open(self.errpath) as f:
                return f.read().strip()
        except OSError:
            return ""

    def __enter__(self):
        return self.open()

    def __exit__(self, *exc):
        self.close()
        return False

    def close(self):
        if self.fd >= 0:
            try:
                os.close(self.fd)
            except OSError:
                pass
            self.fd = -1
        if self.pid > 0:
            try:
                os.kill(self.pid, signal.SIGTERM)
                for _ in range(50):
                    done, _st = os.waitpid(self.pid, os.WNOHANG)
                    if done:
                        break
                    time.sleep(0.1)
                else:
                    os.kill(self.pid, signal.SIGKILL)
                    os.waitpid(self.pid, 0)
            except OSError:
                pass
            self.pid = -1

    def alive(self):
        if self.pid <= 0:
            return False
        try:
            done, st = os.waitpid(self.pid, os.WNOHANG)
        except OSError:
            self.pid = -1
            return False
        if done:
            self.pid = -1
            self.status = os.waitstatus_to_exitcode(st) if st >= 0 else -1
            return False
        return True

    def read(self, timeout):
        try:
            r, _w, _e = select.select([self.fd], [], [], timeout)
        except (OSError, ValueError):
            return ""
        if not r:
            return ""
        try:
            data = os.read(self.fd, 8192)
        except OSError:
            return ""
        if not data:
            return ""
        text = data.decode("utf-8", "replace")
        if self.log:
            self.log.write(text)
            self.log.flush()
        self.buf += text
        return text

    def watch(self):
        """Stream the install until qemu exits.

        Nothing is typed. The installerconfig runs by itself and ends with a
        poweroff, so a finished install is a qemu that is gone -- and the
        marker it echoes just before that separates "it finished" from "it
        died at the end".
        """
        started = time.time()
        last = started
        shown = 0
        while True:
            got = self.read(2.0)
            now = time.time()
            if got:
                last = now
                if DONE.search(self.buf[-4096:]):
                    self.saw_done = True
                # Answered on the tail only, so the same words quoted later in
                # the output do not set it off again.
                tail = self.buf[-256:]
                for pat, reply, what in ANSWERS:
                    if pat.search(tail):
                        if self.answered >= MAX_ANSWERS:
                            raise ConsoleError(
                                "the media asked the same question %d times."
                                "\nIt is interactive, not scripted, and this"
                                " will not answer it forever.\n"
                                "the console last said:\n%s"
                                % (MAX_ANSWERS, self.tail()))
                        self.answered += 1
                        print("[freebsd] answering %s" % what, flush=True)
                        self.write(reply)
                        # Drop the tail so one prompt is answered once.
                        self.buf += "\n<answered>\n"
                        break
                for pat, why in BAD:
                    if pat.search(self.buf[-4096:]):
                        raise ConsoleError(
                            "%s.\nthe console last said:\n%s"
                            % (why, self.tail()))
            if not self.alive():
                while self.read(0.5):
                    pass
                if self.saw_done or DONE.search(self.buf):
                    return
                said = self.qemu_said()
                if not self.buf.strip():
                    if self.status == 0 and not said:
                        raise ConsoleError(
                            "the guest reset before printing anything and"
                            " qemu exited on it\n(-no-reboot), so nothing"
                            " was started: the bios found no bootable\n"
                            "disc, or loaded one it could not run.\n\n"
                            "That is the install media, not the command"
                            " line. What to look at:\n"
                            "  the boot entry     xorriso -indev"
                            " <iso> -report_el_torito plain\n"
                            "  its load size      must match the iso it"
                            " was built from; short\n"
                            "                     loads part of the loader"
                            " and resets exactly so\n\n"
                            "the command was:\n%s" % self.cmdline())
                    raise ConsoleError(
                        "qemu exited (status %s) before the guest printed"
                        " anything, so it is\nthe command line being"
                        " refused and not the install.\n\nqemu said:\n%s"
                        "\n\nthe command was:\n%s"
                        % (self.status,
                           said or "  (nothing on its error stream either)",
                           self.cmdline()))
                raise ConsoleError(
                    "qemu exited and the install never said it was done.\n"
                    "%sthe console last said:\n%s"
                    % (("qemu said:\n%s\n\n" % said) if said else "",
                       self.tail()))
            # A line every couple of minutes, so a long install looks alive.
            if now - started > shown + 120:
                shown = int(now - started)
                print("[freebsd] installing, %d min elapsed" % (shown // 60),
                      flush=True)
            if now - last > QUIET_TIMEOUT:
                if not self.buf.strip():
                    # Never a single byte, yet the machine is running: it is
                    # talking somewhere this build cannot see.
                    raise ConsoleError(
                        "%d min and NOT ONE BYTE on the serial line, with"
                        " qemu still\nrunning -- so it is booting and"
                        " speaking to the video console.\n\n"
                        "The rebuilt media is given"
                        " /boot/loader.conf.local to put the\nconsole on"
                        " the serial line. Check it is really on there:\n"
                        "  xorriso -indev <install.iso> -find"
                        " /boot/loader.conf.local\n"
                        % (QUIET_TIMEOUT // 60))
                raise ConsoleError(
                    "nothing on the console for %d min and qemu is still "
                    "running.\nthe console last said:\n%s"
                    % (QUIET_TIMEOUT // 60, self.tail()))
            if now - started > INSTALL_TIMEOUT:
                raise ConsoleError(
                    "the install did not finish in %d min.\n"
                    "the console last said:\n%s"
                    % (INSTALL_TIMEOUT // 60, self.tail()))

    def tail(self, n=1200):
        return self.buf[-n:].replace("\r", "")

def main():
    if len(sys.argv) != 3:
        sys.stderr.write("usage: %s <install.iso> <disk.qcow2>\n" % sys.argv[0])
        return 1
    iso, qcow2 = sys.argv[1], sys.argv[2]
    for f in (iso, qcow2):
        if not os.path.exists(f):
            sys.stderr.write("no %s\n" % f)
            return 1
    state, said = qemu_stream_check()
    if state == "no":
        sys.stderr.write(
            "\nFAILED: %s has no \"stream\" netdev.\n"
            "  That is the backend passt presents, and the install needs it\n"
            "  to fetch qemu-guest-agent. What this qemu does have:\n%s\n"
            % (QEMU_BIN, "\n".join("    " + l for l in said.splitlines())))
        return 1
    if state == "unknown":
        print("[freebsd] the netdev list could not be read from this qemu,"
              " so whether", flush=True)
        print("[freebsd] it has the stream backend is not known here."
              " Going on.", flush=True)
    logpath = "/tmp/freebsd_console.log"
    print("[freebsd] console log: %s" % logpath, flush=True)
    print("[freebsd] the install runs unattended and prints as it goes",
          flush=True)
    with open(logpath, "w") as log:
        try:
            with Passt(log=log):
                with Install(iso, qcow2, log=log) as ins:
                    ins.watch()
        except ConsoleError as err:
            sys.stderr.write("\nFAILED: %s\n" % err)
            sys.stderr.write("the whole session is in %s\n" % logpath)
            return 1
    print("[freebsd] installed and powered off", flush=True)
    return 0

if __name__ == "__main__":
    sys.exit(main())
