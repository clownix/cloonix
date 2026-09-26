#!/usr/bin/env python3
# cloonix_atop.py -- the topology of "cloonix cli <net> dump drawing", drawn.

# The client module, handed over by render(): term_size() lives there.
CL = None

ATOP_MAX_OBJ = 24
ATOP_MAX_LAN = 24
ATOP_MAX_CROSSING = 10
ATOP_MIN_WIDTH = 80
ATOP_GAP = 3

class _AtopCanvas:
    """A character grid, plus the one thing worth counting while drawing.

    Every place a cable crosses another is a place the drawing gets harder
    to follow, so the crossings are counted as they are made and the caller
    decides, once the whole thing is down, whether what came out is a
    picture or a thicket.
    """

    def __init__(self, w, h):
        self.w = w
        self.h = h
        self.rows = [[" "] * w for _ in range(h)]
        self.crossings = 0

    def put(self, y, x, ch, cross=True):
        if not (0 <= y < self.h and 0 <= x < self.w):
            return
        old = self.rows[y][x]
        if old == ch or old == " ":
            self.rows[y][x] = ch
        elif old == "+" or ch == "+":
            # A corner already drawn is kept over a crossing cable.
            if old != "+":
                self.rows[y][x] = "+"
            elif cross:
                self.crossings += 1
        elif (old, ch) in (("-", "|"), ("|", "-")):
            self.rows[y][x] = "+"
            self.crossings += 1
        else:
            self.rows[y][x] = ch

    def text(self, y, x, s):
        for i, ch in enumerate(s):
            self.put(y, x + i, ch, cross=False)

    def vline(self, x, y0, y1):
        for y in range(y0, y1 + 1):
            self.put(y, x, "|")

    def hline(self, y, x0, x1):
        for x in range(min(x0, x1), max(x0, x1) + 1):
            self.put(y, x, "-")

    def lines(self):
        return [("".join(r)).rstrip() for r in self.rows]

class _AtopBox:
    """One box: an object or a lan."""

    def __init__(self, kind, name, sub):
        self.kind = kind        # "obj" or "lan"
        self.name = name
        self.sub = sub          # the type line of an object, "" for a lan
        self.edges = []         # indexes into the cable list
        self.level = 0
        self.x = 0
        self.w = 0
        self.ports = {}         # cable index -> the column it leaves by
        self.port_line = None

def _atop_collect(topo):
    """The topology as boxes and cables, and nothing else."""
    vx = {}
    notes = []
    kinds = (("cnt", "cnt"), ("kvm", "kvm"), ("c2c", "c2c"), ("tap", "tap"),
             ("nat", "nat"), ("a2b", "a2b"), ("phy", "phy"))
    for key, label in kinds:
        for obj in topo.get(key, []) or []:
            name = obj.get("name", "?")
            vx[("obj", name)] = _AtopBox("obj", name, label)
            if obj.get("ping_ok") == 0:
                notes.append("%s is not answering" % name)

    edges = []
    for e in topo.get("endp", []) or []:
        name = e.get("name", "?")
        num = e.get("num", 0)
        if ("obj", name) not in vx:
            vx[("obj", name)] = _AtopBox("obj", name, "?")
        for lan in e.get("lans", []) or []:
            if ("lan", lan) not in vx:
                vx[("lan", lan)] = _AtopBox("lan", lan, "")
            idx = len(edges)
            edges.append({"obj": ("obj", name), "lan": ("lan", lan),
                          "num": str(num)})
            vx[("obj", name)].edges.append(idx)
            vx[("lan", lan)].edges.append(idx)

    for v in vx.values():
        if v.kind == "lan" and len(v.edges) == 1:
            notes.append("lan %s has only one end" % v.name)
    return vx, edges, notes

def _atop_near(vx, edges, key):
    v = vx[key]
    return [edges[i]["lan"] if v.kind == "obj" else edges[i]["obj"]
            for i in v.edges]

def _atop_bfs(vx, edges, root):
    level = {root: 0}
    queue = [root]
    while queue:
        nxt = []
        for k in queue:
            for other in _atop_near(vx, edges, k):
                if other not in level:
                    level[other] = level[k] + 1
                    nxt.append(other)
        queue = nxt
    return level

def _atop_components(vx, edges):
    seen = set()
    out = []
    for key in sorted(vx):
        if key in seen:
            continue
        comp = sorted(_atop_bfs(vx, edges, key))
        seen.update(comp)
        out.append(comp)
    return out

def _atop_levels(vx, edges, comp):
    """Rows, by breadth first from the middle of the component.

    The root decides the height. From one end of a chain the drawing is as
    tall as the chain is long; from its middle it is half that and reads
    outwards both ways. So the root is the box whose furthest box is
    nearest -- the centre -- and among equals the one with the most cables,
    which puts a lan with ten machines on it above the ten rather than
    somewhere in the middle of them."""
    best = None
    for key in comp:
        level = _atop_bfs(vx, edges, key)
        rank = (max(level.values()), -len(vx[key].edges), key)
        if best is None or rank < best[0]:
            best = (rank, level)
    level = best[1]
    rows = {}
    for k in comp:
        rows.setdefault(level[k], []).append(k)
    for lv in rows:
        rows[lv].sort()
    return level, rows

def _atop_order(vx, edges, level, rows):
    """Left to right inside each row, by the average position of the
    neighbours in the row above, then in the row below, a few times over.

    The barycentre heuristic. It is not exact and does not pretend to be,
    but every crossing it removes is one that does not have to be drawn as
    a plus sign, and on a dozen boxes it usually removes them all."""
    order = {}
    for lv in rows:
        for i, k in enumerate(rows[lv]):
            order[k] = float(i)

    def sweep(levels, look):
        for lv in levels:
            bary = []
            for k in rows[lv]:
                pos = [order[o] for o in _atop_near(vx, edges, k)
                       if level[o] == lv + look]
                bary.append((sum(pos) / len(pos) if pos else order[k],
                             order[k], k))
            bary.sort()
            rows[lv] = [k for _, _, k in bary]
            for i, k in enumerate(rows[lv]):
                order[k] = float(i)

    top = max(rows)
    for _ in range(4):
        sweep(range(1, top + 1), -1)
        sweep(range(top - 1, -1, -1), +1)
    return rows

def _atop_measure(vx, edges, comp):
    """How wide a box has to be. It depends on what is written in it and on
    how many cables leave it, and on nothing that is decided later."""
    for k in comp:
        v = vx[k]
        n = len(v.edges)
        if v.kind == "obj":
            labels = [edges[i]["num"] for i in v.edges]
            need = sum(len(s) for s in labels) + 2 * max(0, n - 1)
            inner = max(len(v.name), len(v.sub), need)
        else:
            inner = max(len(v.name), 2 * n - 1 if n else 1)
        v.w = inner + 4

def _atop_place(vx, rows):
    """Boxes packed left to right in a row, each row centred on the widest.

    Then the one case worth a rule of its own: a row that is a single lan
    with three or more cables is a bus, and a bus is drawn as one. It is
    widened to the whole drawing so its cables drop straight down onto the
    machines instead of fanning out of a small box in the middle."""
    widths = {}
    for lv in sorted(rows):
        widths[lv] = (sum(vx[k].w for k in rows[lv])
                      + ATOP_GAP * (len(rows[lv]) - 1))
    total = max(widths.values())
    for lv in sorted(rows):
        x = (total - widths[lv]) // 2
        for k in rows[lv]:
            vx[k].x = x
            x += vx[k].w + ATOP_GAP
    for lv in sorted(rows):
        if len(rows[lv]) == 1:
            v = vx[rows[lv][0]]
            if v.kind == "lan" and len(v.edges) >= 3:
                v.x = 0
                v.w = total
    return total

def _atop_spread(targets, sizes, left, right, gap):
    """Port columns as near as they can be to where their cable is going.

    Each port wants to sit directly above (or below) the far end of its
    cable, so that the cable comes out a straight line. They cannot all have
    it: they must stay inside their box and keep clear of each other. This
    pushes them apart from the left, then back in from the right, then apart
    again, which lands every one of them on its wish or as close to it as
    the two constraints allow. The box was measured wide enough for the
    tightest packing, so this always terminates with them in order."""
    n = len(targets)
    step = [sizes[i] + gap for i in range(n)]
    p = list(targets)
    for i in range(1, n):
        p[i] = max(p[i], p[i - 1] + step[i - 1])
    p[n - 1] = min(p[n - 1], right - sizes[n - 1] + 1)
    for i in range(n - 2, -1, -1):
        p[i] = min(p[i], p[i + 1] - step[i])
    p[0] = max(p[0], left)
    for i in range(1, n):
        p[i] = max(p[i], p[i - 1] + step[i - 1])
    return p

def _atop_ports(vx, edges, rows):
    """Which column each cable leaves its box by, row by row from the top.

    A box sorts its cables by where the far end sits, so they leave it in
    the same left to right order as the boxes they go to. That alone removes
    most of the crossings any other order would make. Where the far end is
    in the row above, its port column is already decided and is taken as the
    target, so the cable comes out straight; where it is in the row below,
    the middle of that box is the best guess there is.

    On an object the port column is where the interface number is printed,
    so every number sits at the end of its own cable and nothing has to be
    matched up by eye."""
    for lv in sorted(rows):
        for k in rows[lv]:
            v = vx[k]
            if not v.edges:
                continue

            def target(idx, v=v, lv=lv):
                other = (edges[idx]["lan"] if v.kind == "obj"
                         else edges[idx]["obj"])
                ov = vx[other]
                if ov.level == lv - 1 and idx in ov.ports:
                    return float(ov.ports[idx])
                return ov.x + (ov.w - 1) / 2.0

            v.edges.sort(key=lambda i: (target(i), edges[i]["num"]))
            targets = [int(round(target(i))) for i in v.edges]
            if v.kind == "obj":
                labels = [edges[i]["num"] for i in v.edges]
                cols = _atop_spread(targets, [len(s) for s in labels],
                                    v.x + 2, v.x + v.w - 3, 2)
                v.port_line = list(zip(cols, labels))
            else:
                cols = _atop_spread(targets, [1] * len(v.edges),
                                    v.x + 2, v.x + v.w - 3, 1)
            for i, idx in enumerate(v.edges):
                v.ports[idx] = cols[i]

def _atop_channels(band_edges):
    """One row of the band per horizontal run, two runs sharing a row
    whenever they do not overlap. A cable that is already straight takes no
    row at all, and after the port placement above most of them are.

    The order the rows are handed out in is what keeps a fan tidy: the cable
    reaching furthest out gets the row nearest the box it leaves, so the
    ones inside it pass underneath rather than through. Done the other way
    round, every cable of a fan crosses every other."""
    chan = {}
    used = []
    left = sorted([e for e in band_edges if e[2] < e[1]],
                  key=lambda t: (t[2], t[1]))
    right = sorted([e for e in band_edges if e[2] > e[1]],
                   key=lambda t: (-t[2], -t[1]))
    for idx, x0, x1 in left + right:
        lo, hi = min(x0, x1) - 1, max(x0, x1) + 1
        for c, taken in enumerate(used):
            if all(hi < a or lo > b for a, b in taken):
                taken.append((lo, hi))
                chan[idx] = c
                break
        else:
            used.append([(lo, hi)])
            chan[idx] = len(used) - 1
    return chan, len(used)

def _atop_draw(vx, edges, rows, total_w):
    """The boxes, then the cables between them, onto one grid."""
    lv_max = max(rows)
    box_h = {}
    band_h = {}
    bands = {}
    for lv in sorted(rows):
        box_h[lv] = 5 if vx[rows[lv][0]].kind == "obj" else 3
    for lv in range(lv_max):
        band_edges = []
        for k in rows[lv]:
            v = vx[k]
            for idx in v.edges:
                other = (edges[idx]["lan"] if v.kind == "obj"
                         else edges[idx]["obj"])
                if vx[other].level == lv + 1:
                    band_edges.append((idx, v.ports[idx],
                                       vx[other].ports[idx]))
        chan, nchan = _atop_channels(band_edges)
        bands[lv] = (band_edges, chan)
        band_h[lv] = nchan + 2

    y = 0
    top_y = {}
    for lv in sorted(rows):
        top_y[lv] = y
        y += box_h[lv] + (band_h[lv] if lv < lv_max else 0)
    cv = _AtopCanvas(total_w, y)

    for lv in sorted(rows):
        y0 = top_y[lv]
        for k in rows[lv]:
            v = vx[k]
            inner = v.w - 4
            cv.text(y0, v.x, "+" + "-" * (v.w - 2) + "+")
            cv.text(y0 + box_h[lv] - 1, v.x, "+" + "-" * (v.w - 2) + "+")
            body = [v.name.center(inner)]
            if v.kind == "obj":
                body.append(v.sub.center(inner))
                line = [" "] * inner
                for col, lbl in v.port_line or []:
                    for i, ch in enumerate(lbl):
                        if 0 <= col - v.x - 2 + i < inner:
                            line[col - v.x - 2 + i] = ch
                body.append("".join(line))
            for i, txt in enumerate(body):
                cv.text(y0 + 1 + i, v.x, "| " + txt + " |")

    for lv in range(lv_max):
        band_edges, chan = bands[lv]
        y0 = top_y[lv] + box_h[lv]
        y1 = top_y[lv + 1] - 1
        for idx, xu, xd in band_edges:
            cv.put(y0 - 1, xu, "+", cross=False)
            cv.put(y1 + 1, xd, "+", cross=False)
            if idx not in chan:
                cv.vline(xu, y0, y1)
                continue
            row = y0 + 1 + chan[idx]
            cv.vline(xu, y0, row - 1)
            cv.hline(row, xu, xd)
            cv.put(row, xu, "+", cross=False)
            cv.put(row, xd, "+", cross=False)
            cv.vline(xd, row + 1, y1)
    return cv

def render_topo_ascii(topo, width=None):
    """The topology as a drawing, or a line saying why not.

    Each piece of the net that hangs together is drawn on its own, so a net
    cut in two by a missing cable comes out as two drawings, which is the
    quickest way there is to see that it IS cut in two. Objects on no lan at
    all are named on one line at the end rather than drawn as boxes with
    nothing attached, and a lan with a single end is noted under the drawing
    the same way "topo" flags it."""
    if width is None:
        width = CL.term_size()[0]
    width = max(width, ATOP_MIN_WIDTH)

    vx, edges, notes = _atop_collect(topo)
    net = topo.get("network", "?")
    nb_obj = len([1 for k in vx if k[0] == "obj"])
    nb_lan = len([1 for k in vx if k[0] == "lan"])
    out = ["net %s -- %d object%s, %d lan%s" %
           (net, nb_obj, "" if nb_obj == 1 else "s",
            nb_lan, "" if nb_lan == 1 else "s")]
    if not vx:
        return "\n".join(out + ["", "  (nothing in it yet)"])
    if nb_obj > ATOP_MAX_OBJ or nb_lan > ATOP_MAX_LAN:
        return "\n".join(out + [
            "",
            "  too much to draw: the limit here is %d objects and %d lans."
            % (ATOP_MAX_OBJ, ATOP_MAX_LAN),
            "  \"cloonix cli %s dump topo\" lists all of it." % net])

    alone = []
    for comp in _atop_components(vx, edges):
        if len(comp) == 1:
            v = vx[comp[0]]
            alone.append("%s (%s)" % (v.name, v.sub or "lan"))
            continue
        level, rows = _atop_levels(vx, edges, comp)
        for k in comp:
            vx[k].level = level[k]
        rows = _atop_order(vx, edges, level, rows)
        _atop_measure(vx, edges, comp)
        total = _atop_place(vx, rows)
        if total > width:
            out += ["",
                    "  one piece of this net wants %d columns and the "
                    "terminal has %d." % (total, width),
                    "  widen it, or use \"cloonix cli %s dump topo\"." % net]
            continue
        _atop_ports(vx, edges, rows)
        cv = _atop_draw(vx, edges, rows, total)
        if cv.crossings > ATOP_MAX_CROSSING:
            out += ["",
                    "  one piece of this net draws with %d cable crossings, "
                    "which is past" % cv.crossings,
                    "  what can be followed. \"cloonix cli %s dump topo\" lists it "
                    "instead." % net]
            continue
        out.append("")
        out.extend(cv.lines())

    if alone:
        out += ["", "  on no lan: " + ", ".join(sorted(alone))]
    if notes:
        out.append("")
        out += ["  note: " + n for n in sorted(set(notes))]
    return "\n".join(out)

def render(client, topo, width=None):
    """The one entry point: the drawing, or a line saying why not."""
    global CL
    CL = client
    return render_topo_ascii(topo, width)
