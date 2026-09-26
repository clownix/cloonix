#!/usr/bin/env python3
"""cisco_console - drive an IOS-XE guest through its cloonix serial console.

There is no agent inside a cisco and there cannot be one, so the console is
the only way in. cloonix launches every guest with its qemu chardev wrapped
in a dtach session, and that session is a socket anyone on the machine can
attach to a second time:

    /var/lib/cloonix/<net>/dtach/<vm>

which is exactly what the browser Console panel does. This attaches the same
way and reads up to the prompt instead of drawing to a screen.

    from cisco_console import Console
    with Console("nemo", "cisco1") as c:
        c.wait_ready()                  # through boot, dialog, RETURN
        c.load_config(open("cisco1.cfg").read())
        print(c.show("show ip ospf neighbor"))

Standard library only, like the cloonix client itself.
"""
import os
import pty
import re
import select
import sys
import time
import tty

DTACH_BIN = "/usr/libexec/cloonix/cloonfs/bin/cloonix-dtach"
DTACH_SOCK = "/var/lib/cloonix/%s/dtach/%s"

# A prompt is a name, an optional (config...) part, then > or #, at the very
# end of what has arrived.
PROMPT = re.compile(r"[\r\n]?([\w.\-]+)(\([\w\-]+\))?\s?([>#])\s*$")

# Everything else worth answering on the way to a prompt.
MORE = re.compile(r"--\s?More\s?--\s*$")
RETURN_ME = re.compile(r"Press RETURN to get started", re.I)
DIALOG = re.compile(r"initial configuration dialog.*\]\s*:\s*$", re.I | re.S)
TERMINATE = re.compile(r"terminate autoinstall.*\]\s*:\s*$", re.I | re.S)

class ConsoleError(Exception):
    pass

class Console:
    def __init__(self, net, vm, sock=None, dtach=None, log=None):
        self.net = net
        self.vm = vm
        self.sock = sock or (DTACH_SOCK % (net, vm))
        self.dtach = dtach or DTACH_BIN
        self.log = log
        self.fd = -1
        self.pid = -1
        self.buf = ""
        self.hostname = None

    # plumbing
    def open(self):
        if not os.path.exists(self.sock):
            raise ConsoleError("no console socket %s: is %s started?"
                               % (self.sock, self.vm))
        if not os.path.exists(self.dtach):
            raise ConsoleError("no %s" % self.dtach)
        pid, fd = pty.fork()
        if pid == 0:
            # E: do not take a detach character, an empty one would be NUL and
            # ctrl-space would detach.
            env = {"TERM": "vt100", "LANG": "C", "LC_ALL": "C"}
            try:
                os.execve(self.dtach,
                          [self.dtach, "-a", self.sock, "-E", "-z"], env)
            finally:
                os._exit(1)
        self.pid = pid
        self.fd = fd
        # Raw pty: cooked input holds the space that answers --More-- until a newline.
        try:
            tty.setraw(self.fd)
        except tty.error:
            pass
        return self

    def close(self):
        if self.fd >= 0:
            try:
                os.close(self.fd)
            except OSError:
                pass
            self.fd = -1
        if self.pid > 0:
            try:
                os.kill(self.pid, 9)
                os.waitpid(self.pid, 0)
            except OSError:
                pass
            self.pid = -1

    def __enter__(self):
        return self.open()

    def __exit__(self, *exc):
        self.close()

    def _read(self, timeout):
        """Whatever has arrived within timeout, "" when nothing did."""
        try:
            r, _, _ = select.select([self.fd], [], [], timeout)
        except (OSError, ValueError):
            raise ConsoleError("console of %s went away" % self.vm)
        if not r:
            return ""
        try:
            data = os.read(self.fd, 8192)
        except OSError:
            raise ConsoleError("console of %s closed" % self.vm)
        if not data:
            raise ConsoleError("console of %s closed" % self.vm)
        text = data.decode("utf-8", "replace")
        if self.log:
            self.log.write(text)
        return text

    def _write(self, text):
        os.write(self.fd, text.encode())

    # reading up to something
    def expect(self, timeout=60, answer=True):
        """Read until a prompt, returning everything up to it.

        answer=True deals with what stands between a booting router and its
        prompt: the pager, the autoinstall questions, and the RETURN that
        the console asks for. Each is answered once and the wait carries on
        rather than counting as an arrival.
        """
        end = time.time() + timeout
        while True:
            m = PROMPT.search(self.buf)
            if m:
                out = self.buf[:m.start()]
                self.hostname = m.group(1)
                self.mode = m.group(3)
                self.buf = ""
                return out
            if answer:
                if MORE.search(self.buf):
                    # Only when length 0 is not set: the day-0 sets it, an
                    # image configured by hand may not have it.
                    self.buf = MORE.sub("", self.buf)
                    self._write(" ")
                    continue
                if DIALOG.search(self.buf) or TERMINATE.search(self.buf):
                    self.buf = ""
                    self._write("no\r")
                    continue
                if RETURN_ME.search(self.buf):
                    self.buf = ""
                    self._write("\r")
                    continue
            left = end - time.time()
            if left <= 0:
                raise ConsoleError("%s: no prompt after %ds, last saw:\n%s"
                                   % (self.vm, timeout, self.buf[-400:]))
            self.buf += self._read(min(left, 1.0))

    def wait_ready(self, timeout=600):
        """Wait for a usable prompt, through as much boot as is left.

        Listening comes first and the carriage return only after a silent
        stretch. The other way round, the nudge lands in whatever question
        the router is sitting on -- and a bare return is an answer to the
        autoinstall dialog, not a no-op.
        """
        end = time.time() + timeout
        while time.time() < end:
            try:
                self.expect(timeout=min(10, max(1, end - time.time())))
            except ConsoleError:
                self._write("\r")
                continue
            if self.mode == ">":
                self._write("enable\r")
                self.expect(timeout=20)
            return self
        raise ConsoleError("%s: not ready after %ds" % (self.vm, timeout))

    # talking
    def send(self, line, timeout=60):
        """One line, then wait for the prompt it comes back with.

        Waiting for the prompt after every line IS the flow control: a
        serial console has none, and a config poured in faster than IOS
        parses it loses characters.
        """
        self._write(line + "\r")
        return self.expect(timeout=timeout)

    def show(self, cmd, timeout=60):
        out = self.send(cmd, timeout=timeout)
        # The line just typed comes back echoed; the answer is what follows.
        return out.split("\n", 1)[1] if "\n" in out else ""

    def load_config(self, text, timeout=60):
        """Play a configuration in, one line at a time.

        Blank lines and ! are dropped: they are worth nothing to IOS and
        cost a round trip each. end/exit at the outer level is left to the
        caller of this, which closes config mode itself.
        """
        self.send("configure terminal", timeout=timeout)
        for raw in text.split("\n"):
            line = raw.rstrip()
            if not line.strip() or line.strip().startswith("!"):
                continue
            if line.strip() == "end":
                continue
            out = self.send(line, timeout=timeout)
            if "% Invalid input" in out or "% Incomplete" in out:
                raise ConsoleError("%s refused: %s\n%s"
                                   % (self.vm, line.strip(), out.strip()))
        self.send("end", timeout=timeout)
        return self

    def write_mem(self, timeout=180):
        return self.send("write memory", timeout=timeout)

def main():
    if len(sys.argv) < 4:
        sys.exit("usage: %s <net> <vm> <config file>\n"
                 "       %s <net> <vm> --show '<command>'"
                 % (sys.argv[0], sys.argv[0]))
    net, vm, arg = sys.argv[1], sys.argv[2], sys.argv[3]
    with Console(net, vm) as c:
        c.wait_ready()
        if arg == "--show":
            print(c.show(sys.argv[4]))
        else:
            c.load_config(open(arg).read())
            print("%s: %s loaded" % (vm, arg))

if __name__ == "__main__":
    main()
