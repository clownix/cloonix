#!/usr/bin/env python3
# cloonix_files.py - the two-pane file manager of "cloonix cli <net> files".
import os
import json
import queue
import shutil
import tempfile
import threading

ALT_ON = "\x1b[?1049h"
ALT_OFF = "\x1b[?1049l"
CLEAR = "\x1b[2J"
CUR_OFF = "\x1b[?25l"
CUR_ON = "\x1b[?25h"
NORMAL = "\x1b[0m"

BLUE = "\x1b[0;37;44m"      # a panel, and the screen behind it
DIRC = "\x1b[1;37;44m"      # a directory, white and bold
LINK = "\x1b[1;36;44m"      # a symlink, cyan
HEAD = "\x1b[1;33;44m"      # the column titles, yellow
CYAN = "\x1b[0;30;46m"      # the cursor, the top bar, the key labels
KEYNUM = "\x1b[0;37;40m"    # the digits of the key bar
DIALOG = "\x1b[0;30;47m"    # a dialog, black on light grey
DTITLE = "\x1b[1;34;47m"    # its title, blue
SHADOW = "\x1b[0;30;40m"    # what a dialog casts down and to the right

# The size column, wide enough for UP--DIR and for what human() returns.
SIZEW = 9

FKEYS = ("Help", "Reload", "", "", "Copy", "", "Mkdir", "Delete", "GoTo",
         "Quit")

HELP = (
    ("Tab", "the other pane becomes the active one"),
    ("Up Down", "move the cursor, also k and j"),
    ("PgUp PgDn", "a screenful, Home and End the ends of the list"),
    ("Enter", "open the directory under the cursor, also Right"),
    ("Backspace", "up one directory, also Left"),
    ("F5 or c", "copy what is selected into the other pane"),
    ("F7 or m", "make a directory in the active pane"),
    ("F8 or d", "delete what is selected, after a yes"),
    ("F2 or r", "read the directory again"),
    ("g or F9", "type a path to go to, in the active pane"),
    ("F1 or ?", "this"),
    ("F10 or q", "leave -- if F10 does nothing the terminal took it, use q"),
)

OLD_GUEST = ("this guest answers no LIST: its insider iso predates the "
             "files verb.\nRebuild it (cmd_mk_iso) and restart the guest.")

def at(row, col):
    return "\x1b[%d;%dH" % (row, col)

# The frame, in the two alphabets mc has.
UNI = {"h": "\u2500", "v": "\u2502", "tl": "\u250c", "tr": "\u2510",
       "bl": "\u2514", "br": "\u2518", "lt": "\u251c", "rt": "\u2524",
       "up": "\u2534"}
ASC = {"h": "-", "v": "|", "tl": "+", "tr": "+", "bl": "+", "br": "+",
       "lt": "+", "rt": "+", "up": "+"}

def frame_set():
    """Which of the two alphabets the terminal can show."""
    if os.name == "nt":
        try:
            import ctypes
            if ctypes.WinDLL("kernel32").GetConsoleOutputCP() == 65001:
                return UNI
        except Exception:
            pass
        return ASC
    charset = (os.environ.get("LC_ALL") or os.environ.get("LC_CTYPE") or
               os.environ.get("LANG") or "")
    if ("." in charset) and ("utf" not in charset.lower()):
        return ASC
    return UNI

# Keys, as names, on both platforms.
SEQ = {
    "[A": "up", "[B": "down", "[C": "right", "[D": "left",
    "[H": "home", "[F": "end",
    "OA": "up", "OB": "down", "OC": "right", "OD": "left",
    "OH": "home", "OF": "end",
    "[1~": "home", "[4~": "end", "[7~": "home", "[8~": "end",
    "[5~": "pgup", "[6~": "pgdn", "[3~": "f8", "[2~": "insert",
    "OP": "f1", "OQ": "f2", "OR": "f3", "OS": "f4",
    "[11~": "f1", "[12~": "f2", "[13~": "f3", "[14~": "f4",
    "[15~": "f5", "[17~": "f6", "[18~": "f7", "[19~": "f8",
    "[20~": "f9", "[21~": "f10",
}

class Keyboard:
    """A daemon thread reads stdin and a queue hands the bytes over.

    select() does not work on a windows console handle, and a plain
    blocking read cannot tell the escape key from the first byte of an
    arrow: both are 0x1b and only the wait afterwards separates them. The
    queue can be asked with a timeout, which is the whole reason for it.
    """

    def __init__(self):
        self.q = queue.Queue()
        threading.Thread(target=self._reader, daemon=True).start()

    def _reader(self):
        while True:
            try:
                # os.read, not sys.stdin: a daemon thread parked in a BufferedReader deadlocks at exit.
                data = os.read(0, 1024)
            except (OSError, ValueError):
                break
            if not data:
                break
            for b in data:
                self.q.put(b)

    def _get(self, timeout=None):
        try:
            return self.q.get(timeout=timeout)
        except queue.Empty:
            return None

    def drain(self):
        while self._get(0) is not None:
            pass

    def key(self):
        b = self._get()
        if b is None:
            return "quit"
        if b != 0x1b:
            if b in (13, 10):
                return "enter"
            if b == 9:
                return "tab"
            if b in (8, 127):
                return "back"
            if b == 3:
                return "quit"        # raw mode: ctrl-c arrives here
            return chr(b)
        nxt = self._get(0.05)
        if nxt is None:
            return "esc"
        seq = chr(nxt)
        if seq not in ("[", "O"):
            return "esc"
        while len(seq) < 8:
            if (len(seq) >= 2) and (seq[-1].isalpha() or seq[-1] == "~"):
                # Modifiers come as extra parameters ("[21;3~" for F10+alt): drop them.
                if ";" in seq:
                    if seq[-1] == "~":
                        seq = seq.split(";")[0] + "~"
                    else:
                        seq = seq[0] + seq[-1]
                return SEQ.get(seq, "unknown")
            nxt = self._get(0.05)
            if nxt is None:
                return "unknown"
            seq += chr(nxt)
        return "unknown"

def join_sep(base, leaf, sep):
    if base.endswith(sep):
        return base + leaf
    return base + sep + leaf

def parent_sep(path, sep):
    if sep == "\\":
        if (len(path) <= 3) and (path[1:2] == ":"):
            return path
        cut = path.rstrip("\\").rfind("\\")
        if cut < 0:
            return path
        if (cut == 2) and (path[1:2] == ":"):
            return path[:3]
        return path[:cut] or "\\"
    if path == "/":
        return "/"
    cut = path.rstrip("/").rfind("/")
    return path[:cut] or "/"

# One directory, read from wherever it is.
def host_list(path):
    rows = []
    for name in os.listdir(path):
        full = os.path.join(path, name)
        try:
            st = os.lstat(full)
        except OSError:
            continue
        if os.path.islink(full):
            kind = "l"
        elif os.path.isdir(full):
            kind = "d"
        else:
            kind = "f"
        rows.append({"name": name, "type": kind, "size": st.st_size})
    return rows, os.path.abspath(path)

def guest_list(cli, mkleg, path):
    """One LIST on a leg of its own.

    The guest server takes one opcode per connection and closes, so every
    listing opens a leg exactly as every scp does.
    """
    leg = mkleg()
    try:
        leg.no_timeout()
        cxp = cli.Cxp(leg)
        cxp.send(cli.OP_LIST, {"path": path})
        rows, real = [], path
        while True:
            got = cxp.recv()
            if got is None:
                raise IOError("the guest closed the listing")
            op, payload = got
            if op == cli.OP_GENTRY:
                rows.append(json.loads(payload.decode("utf-8", "replace")))
            elif op == cli.OP_EXIT:
                try:
                    real = json.loads(payload.decode()).get("path") or path
                except ValueError:
                    pass
                return rows, real
            elif op == cli.OP_ERROR:
                try:
                    txt = json.loads(payload.decode()).get("txt", "")
                except ValueError:
                    txt = payload.decode("utf-8", "replace")
                if "unexpected first opcode" in txt:
                    raise IOError(OLD_GUEST)
                raise IOError(txt)
    finally:
        leg.close()

class Pane:
    """A directory, a cursor in it, and where the window onto it starts.

    A pane also holds what its side is: the leg factory into that guest, or
    None for this machine, and whether that guest is windows. Two guest
    panes are two different guests, so neither can be a global."""

    def __init__(self, tag, lister, sep, mkleg=None):
        self.tag = tag
        self.label = tag        # what the top bar says over this panel
        self.lister = lister
        self.sep = sep
        self.mkleg = mkleg
        self.host = mkleg is None
        self.is_win = False
        self.path = ""
        self.rows = []
        self.cur = 0
        self.top = 0

    def load(self, path=None, keep=None):
        """Read a directory. keep is a name to leave the cursor on, which is
        what puts it back on the directory just left when going up."""
        want = self.path if path is None else path
        rows, real = self.lister(want)
        rows.sort(key=lambda e: (e.get("type") != "d", e["name"].lower()))
        self.path = real
        self.rows = [{"name": "..", "type": "d", "size": 0}] + rows
        self.cur = 0
        if keep:
            for i, e in enumerate(self.rows):
                if e["name"] == keep:
                    self.cur = i
                    break
        self.top = 0

    def entry(self):
        return self.rows[self.cur] if self.rows else None

    def child(self, name):
        return join_sep(self.path, name, self.sep)

    def move(self, delta, height):
        if not self.rows:
            return
        self.cur = max(0, min(len(self.rows) - 1, self.cur + delta))
        if self.cur < self.top:
            self.top = self.cur
        if self.cur >= self.top + height:
            self.top = self.cur - height + 1

class Screen:

    def __init__(self, cli):
        self.cli = cli
        self.fr = frame_set()
        self.cols, self.lines = cli.term_size()

    def resized(self):
        cols, lines = self.cli.term_size()
        if (cols, lines) != (self.cols, self.lines):
            self.cols, self.lines = cols, lines
            return True
        return False

    def body_height(self):
        return max(1, self.lines - 8)

    def cells(self):
        """The two panel widths. Together they are the screen: each panel
        carries its own frame and the two meet in the middle, as in mc."""
        left = max(20, self.cols // 2)
        return left, max(20, self.cols - left)

    def _room(self, width):
        """What is left of a panel for a name."""
        return max(4, width - SIZEW - 3)

    def _frame(self, width, corners, title, hilite=False):
        """A frame line with a word set into it: the path at the top of a
        panel, the count at the bottom."""
        h = self.fr["h"]
        room = max(4, width - 4)
        if len(title) > room:
            title = "..." + title[-(room - 3):]
        title = " %s " % title if title else ""
        pad = max(0, width - 2 - len(title))
        lead = pad // 2
        return (BLUE + self.fr[corners[0]] + h * lead
                + (CYAN if hilite else BLUE) + title + BLUE
                + h * (pad - lead) + self.fr[corners[1]])

    def _head(self, width):
        v = self.fr["v"]
        return (BLUE + v + HEAD + "%-*s" % (self._room(width), " Name")
                + BLUE + v + HEAD + "Size".center(SIZEW) + BLUE + v)

    def _row(self, pane, idx, width, active):
        """One file: the mark mc puts before a name, the name, and either a
        size or the word that stands where a size would be."""
        room = self._room(width)
        v = self.fr["v"]
        colour, name, size = BLUE, "", ""
        if idx < len(pane.rows):
            e = pane.rows[idx]
            kind = e.get("type")
            name = {"d": "/", "l": "@"}.get(kind, " ") + e["name"]
            if e["name"] == "..":
                size = "UP--DIR"
            elif kind == "d":
                size = "SUB-DIR"
            else:
                size = self.cli.human(e.get("size", 0))
            colour = DIRC if kind == "d" else (LINK if kind == "l" else BLUE)
        if len(name) > room:
            name = name[:room - 1] + "~"
        name = "%-*s" % (room, name)
        size = "%*s" % (SIZEW, size[:SIZEW])
        if active and pane.rows and (idx == pane.cur):
            return BLUE + v + CYAN + name + v + size + BLUE + v
        return BLUE + v + colour + name + BLUE + v + colour + size + BLUE + v

    def _split(self, width):
        h = self.fr["h"]
        return (BLUE + self.fr["lt"] + h * self._room(width) + self.fr["up"]
                + h * SIZEW + self.fr["rt"])

    def _mini(self, pane, width):
        """The line mc keeps under a panel for whatever the cursor is on."""
        e = pane.entry()
        if not e:
            text = ""
        elif e["name"] == "..":
            text = "UP--DIR"
        else:
            what = {"d": "SUB-DIR", "l": "SYMLINK"}.get(
                e.get("type"), self.cli.human(e.get("size", 0)))
            text = "%s  %s" % (e["name"], what)
        room = max(1, width - 3)
        v = self.fr["v"]
        return BLUE + v + " " + "%-*s" % (room, text[:room]) + v

    def _keybar(self):
        cell = max(4, self.cols // 10)
        out = []
        for i, label in enumerate(FKEYS):
            room = max(4, self.cols - 9 * cell) if i == 9 else cell
            if not label:
                out.append(CYAN + " " * room)
                continue
            out.append(KEYNUM + "%2d" % (i + 1)
                       + CYAN + "%-*s" % (room - 2, label[:room - 2]))
        return "".join(out)

    def paint(self, left, right, active, message):
        wl, wr = self.cells()
        height = self.body_height()
        out = [BLUE, CLEAR, at(1, 1),
               CYAN + " %-*s" % (wl - 1, left.label[:wl - 1])
               + " %-*s" % (wr - 1, right.label[:wr - 1])]
        for pane, width, col in ((left, wl, 1), (right, wr, wl + 1)):
            act = pane is active
            count = max(0, len(pane.rows) - 1)
            out.append(at(2, col)
                       + self._frame(width, ("tl", "tr"), pane.path, act))
            out.append(at(3, col) + self._head(width))
            for line in range(height):
                out.append(at(4 + line, col)
                           + self._row(pane, pane.top + line, width, act))
            out.append(at(4 + height, col) + self._split(width))
            out.append(at(5 + height, col) + self._mini(pane, width))
            out.append(at(6 + height, col) + self._frame(
                width, ("bl", "br"),
                "%d item%s" % (count, "" if count == 1 else "s")))
        out.append(at(self.lines - 1, 1) + BLUE
                   + " %-*s" % (max(0, self.cols - 1),
                                message[:self.cols - 1]))
        out.append(at(self.lines, 1) + self._keybar() + NORMAL)
        os.write(1, "".join(out).encode("utf-8", "replace"))

    def _box(self, title, width, height):
        width = max(20, min(width, self.cols - 4))
        row = max(1, (self.lines - height) // 2)
        col = max(1, (self.cols - width) // 2)
        h, v = self.fr["h"], self.fr["v"]
        head = " %s " % title
        pad = max(0, width - 2 - len(head))
        lead = pad // 2
        out = [at(row, col) + DIALOG + self.fr["tl"] + h * lead + DTITLE
               + head + DIALOG + h * (pad - lead) + self.fr["tr"]]
        for i in range(1, height - 1):
            out.append(at(row + i, col) + DIALOG + v + " " * (width - 2) + v)
        out.append(at(row + height - 1, col) + DIALOG + self.fr["bl"]
                   + h * (width - 2) + self.fr["br"])
        if (col + width + 2) <= self.cols:
            for i in range(1, height):
                out.append(at(row + i, col + width) + SHADOW + "  ")
        if (row + height) <= self.lines:
            out.append(at(row + height, col + 2) + SHADOW + " " * width)
        return out, row, col, width

    def _buttons(self, row, col, width, names):
        """The foot of a dialog. The first name is the one Enter presses,
        which is what the angle brackets mc puts round it mean."""
        plain, shown = [], []
        for i, name in enumerate(names):
            one = ("[< %s >]" % name) if not i else ("[ %s ]" % name)
            plain.append(one)
            shown.append((CYAN if not i else DIALOG) + one)
        pad = max(0, (width - 2 - len("   ".join(plain))) // 2)
        return at(row, col + 1) + DIALOG + " " * pad + "   ".join(shown)

    def dialog(self, title, prompt, seed, keys):
        """One typed line: a path to go to, a name to create. Its own little
        loop, the main one reading keys as names and this needing them as
        characters."""
        width = min(max(46, len(prompt) + 10), self.cols - 4)
        text = seed
        while True:
            out, row, col, width = self._box(title, width, 7)
            field = width - 6
            shown = text[-field:]
            out.append(at(row + 2, col + 3) + DIALOG
                       + prompt[:width - 6])
            out.append(at(row + 3, col + 3) + CYAN + "%-*s" % (field, shown))
            out.append(self._buttons(row + 5, col, width, ("Ok", "Cancel")))
            out.append(at(row + 3, col + 3 + min(field - 1, len(shown)))
                       + CUR_ON)
            os.write(1, "".join(out).encode("utf-8", "replace"))
            k = keys.key()
            os.write(1, CUR_OFF.encode())
            if k == "enter":
                return text
            if k in ("esc", "quit"):
                return None
            if k == "back":
                text = text[:-1]
            elif (len(k) == 1) and (k >= " "):
                text += k

    def confirm(self, title, question, keys):
        """Yes or no, on the keys mc takes: y, n, Enter for the button the
        angle brackets are round, Esc for the other one."""
        width = min(max(46, len(question) + 10), self.cols - 4)
        out, row, col, width = self._box(title, width, 6)
        out.append(at(row + 2, col + 3) + DIALOG + question[:width - 6])
        out.append(self._buttons(row + 4, col, width, ("Yes", "No")))
        os.write(1, "".join(out).encode("utf-8", "replace"))
        while True:
            k = keys.key()
            if k in ("y", "Y", "enter"):
                return True
            if k in ("n", "N", "esc", "quit", "f10"):
                return False

    def help(self, keys):
        rows = HELP[:max(1, self.lines - 6)]
        out, row, col, width = self._box("Keys", min(66, self.cols - 4),
                                         len(rows) + 4)
        for i, (key, what) in enumerate(rows):
            line = " %-11s%s" % (key, what)
            out.append(at(row + 1 + i, col + 3) + DIALOG + line[:width - 6])
        out.append(self._buttons(row + len(rows) + 2, col, width, ("Ok",)))
        os.write(1, "".join(out).encode("utf-8", "replace"))
        keys.key()

def guest_exec(cli, pane, cmd):
    """A command in the pane's guest, for the two things no opcode covers:
    making a directory and removing one. Both ends run it through their
    shell, /bin/sh on linux and cmd.exe on windows, so the quoting is
    that shell's."""
    leg = pane.mkleg()
    try:
        leg.no_timeout()
        cxp = cli.Cxp(leg)
        cxp.send(cli.OP_EXEC, {"cmd": cmd})
        err = bytearray()
        rc = cxp.pump(lambda b: None, lambda b: err.extend(b))
        return rc, err.decode("utf-8", "replace").strip()
    finally:
        leg.close()

def do_delete(cli, pane, entry):
    name = entry["name"]
    target = pane.child(name)
    if pane.host:
        try:
            if (entry["type"] == "d") and not os.path.islink(target):
                shutil.rmtree(target)
            else:
                os.remove(target)
        except OSError as err:
            return "%s" % err
        return "deleted %s" % name
    if pane.is_win:
        cmd = ('rd /s /q "%s"' % target if entry["type"] == "d"
               else 'del /f /q "%s"' % target)
    else:
        cmd = 'rm -rf "%s"' % target
    rc, err = guest_exec(cli, pane, cmd)
    if rc:
        return err or ("delete failed, code %d" % rc)
    return "deleted %s" % name

def do_mkdir(cli, pane, name):
    target = pane.child(name)
    if pane.host:
        try:
            os.makedirs(target)
        except OSError as err:
            return "%s" % err
        return "created %s" % name
    cmd = ('md "%s"' % target) if pane.is_win else ('mkdir -p "%s"' % target)
    rc, err = guest_exec(cli, pane, cmd)
    if rc:
        return err or ("mkdir failed, code %d" % rc)
    return "created %s" % name

def run_plain(cli, keys, saved, fn):
    """Out of the full screen for the length of a transfer, so the progress
    line the client already writes is the one the user watches, and back
    afterwards. Returns what fn returned and the new raw-mode token."""
    os.write(1, (NORMAL + ALT_OFF + CUR_ON).encode())
    cli.raw_mode_off(saved)
    try:
        rc = fn()
    except (OSError, IOError) as err:
        os.write(2, ("%s\n" % err).encode())
        rc = 2
    if rc:
        os.write(2, b"\n-- that failed, press a key --")
    saved = cli.raw_mode_on()
    if rc:
        keys.drain()
        keys.key()
    os.write(1, (ALT_ON + CUR_OFF).encode())
    return rc, saved

def copy_action(cli, src, dst, entry):
    """What F5 runs, built from the client's own scp functions.

    Up, down, or both: no opcode carries a file from one guest to another,
    so guest to guest is a get into a directory here and a put out of it.
    The file crosses this machine and needs room on it."""
    name = entry["name"]
    is_dir = entry["type"] == "d"

    def put(mkleg, local, where):
        if is_dir:
            return cli.scp_put_dir(mkleg, local, where)
        return cli.scp_put(mkleg, local, where)

    def get(mkleg, rpath, where):
        if is_dir:
            return cli.scp_get_dir(mkleg, rpath, where)
        return cli.scp_get(mkleg, rpath, where)

    if src.host:
        local = os.path.join(src.path, name)
        return lambda: put(dst.mkleg, local, dst.path)
    if dst.host:
        return lambda: get(src.mkleg, src.child(name), dst.path)

    def down_and_up():
        tmp = tempfile.mkdtemp(prefix="cloonix-files-")
        try:
            rc = get(src.mkleg, src.child(name), tmp)
            if rc:
                return rc
            return put(dst.mkleg, os.path.join(tmp, name), dst.path)
        finally:
            shutil.rmtree(tmp, ignore_errors=True)

    return down_and_up

def guest_pane(cli, mkleg_for, vm):
    """One guest side, opened at its home directory. What kind of guest it
    is comes from the path it answers with: a drive letter is windows."""
    mkleg = mkleg_for(vm)
    pane = Pane(vm, lambda p: guest_list(cli, mkleg, p), "/", mkleg)
    pane.load("~")
    if pane.path[1:2] == ":":
        pane.sep = "\\"
        pane.is_win = True
    pane.label = "%s   %s guest" % (vm, "windows" if pane.is_win else "linux")
    return pane

def run(cli, mkleg_for, vms):
    """Entry point of the verb. cli is the client module, which owns the
    transfers, the raw mode and the leg; mkleg_for(vm) makes a leg factory.

    vms is one guest name, this machine taking the left pane, or two, both
    panes being guests."""
    try:
        if len(vms) > 1:
            left = guest_pane(cli, mkleg_for, vms[0])
            right = guest_pane(cli, mkleg_for, vms[1])
        else:
            left = Pane("host", host_list, os.sep)
            left.load(os.getcwd())
            right = guest_pane(cli, mkleg_for, vms[0])
    except (OSError, IOError) as err:
        os.write(2, ("%s\n" % err).encode())
        return 2

    saved = cli.raw_mode_on()
    if saved is None:
        os.write(2, b"files needs a terminal\n")
        return 2
    keys = Keyboard()
    screen = Screen(cli)
    active, other = left, right
    message = "F1 for the keys, F5 to copy into the other pane"
    os.write(1, (ALT_ON + CUR_OFF).encode())
    try:
        while True:
            screen.resized()
            height = screen.body_height()
            active.move(0, height)
            screen.paint(left, right, active, message)
            k = keys.key()
            message = ""
            if k in ("quit", "q", "f10"):
                break
            if k == "tab":
                active, other = other, active
            elif k in ("f1", "?"):
                screen.help(keys)
            elif k in ("up", "k"):
                active.move(-1, height)
            elif k in ("down", "j"):
                active.move(1, height)
            elif k == "pgup":
                active.move(-height, height)
            elif k == "pgdn":
                active.move(height, height)
            elif k == "home":
                active.move(-len(active.rows), height)
            elif k == "end":
                active.move(len(active.rows), height)
            elif k in ("enter", "right", "l"):
                e = active.entry()
                if e and e["type"] == "d":
                    try:
                        if e["name"] == "..":
                            leaf = os.path.basename(
                                active.path.rstrip(active.sep))
                            active.load(parent_sep(active.path, active.sep),
                                        keep=leaf)
                        else:
                            active.load(active.child(e["name"]))
                    except (OSError, IOError) as err:
                        message = "%s" % err
            elif k in ("back", "left", "h"):
                try:
                    leaf = os.path.basename(active.path.rstrip(active.sep))
                    active.load(parent_sep(active.path, active.sep), keep=leaf)
                except (OSError, IOError) as err:
                    message = "%s" % err
            elif k in ("f2", "r"):
                try:
                    keep = active.entry()["name"] if active.entry() else None
                    active.load(keep=keep)
                except (OSError, IOError) as err:
                    message = "%s" % err
            elif k in ("f9", "g"):
                want = screen.dialog("Change directory", "Path:",
                                     active.path, keys)
                if want:
                    try:
                        active.load(want)
                    except (OSError, IOError) as err:
                        message = "%s" % err
            elif k in ("f5", "c"):
                e = active.entry()
                if (not e) or (e["name"] == ".."):
                    message = "nothing selected"
                else:
                    fn = copy_action(cli, active, other, e)
                    rc, saved = run_plain(cli, keys, saved, fn)
                    message = ("copied %s" % e["name"]) if not rc else \
                              ("%s was not copied" % e["name"])
                    try:
                        other.load(keep=e["name"])
                    except (OSError, IOError) as err:
                        message = "%s" % err
            elif k in ("f7", "m"):
                name = screen.dialog("Create a new directory", "Name:",
                                     "", keys)
                if name:
                    message = do_mkdir(cli, active, name)
                    try:
                        active.load(keep=name)
                    except (OSError, IOError) as err:
                        message = "%s" % err
            elif k in ("f8", "d"):
                e = active.entry()
                if (not e) or (e["name"] == ".."):
                    message = "nothing selected"
                else:
                    what = "directory " if e["type"] == "d" else ""
                    if screen.confirm("Delete", 'Delete %s"%s" ?'
                                      % (what, e["name"]), keys):
                        message = do_delete(cli, active, e)
                        try:
                            active.load()
                        except (OSError, IOError) as err:
                            message = "%s" % err
                    else:
                        message = "left alone"
    finally:
        os.write(1, (ALT_OFF + CUR_ON + NORMAL).encode())
        cli.raw_mode_off(saved)
    return 0
