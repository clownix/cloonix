#!/usr/bin/env python3
"""demo_svg - the figure of a demo, drawn from its own LAYOUT, ETHS, WIRING.

    ./demo_svg.py ../demos/routing/babel.py      writes httpdoc/xdoc_user/svg/demo_babel.svg
    ./demo_svg.py --missing                      every figure a demo chapter names and svg/ lacks

The canvas places the items from the same constants, so the figure of the
book and the canvas of the run are one drawing. Standard library only.
"""
import ast
import math
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.join(HERE, "..")
SVG_DIR = os.path.join(TOP, "httpdoc", "xdoc_user", "svg")


def eth_numbers(eths):
    """ETHS keyed by (name, "eth<i>"), an a2b by (name, "A"|"B"): by num."""
    num = lambda k: ("AB".index(k) if k in ("A", "B") else int(k[3:])) \
        if isinstance(k, str) else k
    return dict(((n, num(k)), v) for (n, k), v in eths.items())

def constants(path):
    """The module-level literals of a demo, names resolved, nothing called."""
    ns = {}
    for node in ast.parse(open(path).read()).body:
        if not isinstance(node, ast.Assign):
            continue
        if any(isinstance(n, (ast.Call, ast.Attribute, ast.Subscript))
               for n in ast.walk(node.value)):
            continue
        try:
            exec(compile(ast.Module([node], []), path, "exec"), {}, ns)
        except Exception:                                # noqa: BLE001
            pass
    return ns

def draw(path):
    ns = constants(path)
    layout, eths, wiring = ns["LAYOUT"], ns.get("ETHS", {}), ns["WIRING"]
    eths = eth_numbers(eths)
    xs = [p[0] for p in layout.values()]
    ys = [p[1] for p in layout.values()]
    k = min(1.0, 760.0 / max(1, max(xs) - min(xs)))
    height = int((max(ys) - min(ys)) * k) + 130

    def at(name):
        x, y = layout[name]
        return 70 + (x - min(xs)) * k, 60 + (y - min(ys)) * k

    def eth_at(name, num):
        x, y = at(name)
        a = math.radians(eths.get((name, num), 90 * num))
        return x + 18 * math.sin(a), y - 18 * math.cos(a)

    cab, top = [], []
    for name, (x, y) in ((n, at(n)) for n in layout):
        if name in wiring:
            top.append('<circle cx="%.0f" cy="%.0f" r="18" fill="#6666cc" '
                       'stroke="#9999ff" stroke-width="2"/>' % (x, y))
            top.append('<text x="%.0f" y="%.0f" text-anchor="middle" '
                       'class="w">%s</text>' % (x, y + 4, name))
            for num, lan, addr in wiring[name]:
                ex, ey = eth_at(name, num)
                if lan in layout:
                    cab.append('<path class="cab" d="M%.0f,%.0f L%.0f,%.0f"/>'
                               % ((ex, ey) + at(lan)))
                top.append('<circle cx="%.0f" cy="%.0f" r="7" fill="#27ae60" '
                           'stroke="#54ff54" stroke-width="1.5"/>' % (ex, ey))
                top.append('<text x="%.0f" y="%.1f" text-anchor="middle" '
                           'class="w" font-size="9">%d</text>'
                           % (ex, ey + 3.5, num))
            addrs = " ".join(a.split("/")[0] for _, _, a in wiring[name] if a)
            top.append('<text x="%.0f" y="%.0f" text-anchor="middle" '
                       'class="m">%s</text>' % (x, y + 42, addrs))
        else:
            top.append('<circle cx="%.0f" cy="%.0f" r="14" fill="#555555" '
                       'stroke="#888888" stroke-width="2"/>' % (x, y))
            top.append('<text x="%.0f" y="%.0f" text-anchor="middle" '
                       'class="m">%s</text>' % (x, y - 19, name))
    out = ['<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 900 %d" '
           'width="900" height="%d" font-family="DejaVu Sans, Helvetica, '
           'Arial, sans-serif">' % (height, height),
           '<style>', '.m{font-family:monospace;font-size:11px;fill:#0f3460}',
           '.w{fill:#ffffff;font-size:11px;font-weight:bold}',
           '.cab{stroke:#3498db;stroke-width:2.5;fill:none}', '</style>',
           '<rect width="900" height="%d" fill="#ffffff"/>' % height]
    name = "demo_%s.svg" % os.path.basename(path)[:-3]
    with open(os.path.join(SVG_DIR, name), "w") as f:
        f.write("\n".join(out + cab + top + ["</svg>"]) + "\n")
    return name

def missing():
    """The demos whose figure a demo chapter names and svg/ does not hold."""
    doc = os.path.join(TOP, "httpdoc", "xdoc_user", "doc")
    text = "".join(open(os.path.join(doc, f)).read() for f in os.listdir(doc)
                   if re.match(r"^demos(_\w+)?\.rst$", f))
    for fig in sorted(set(re.findall(r"/svg/demo_(\w+)\.svg", text))):
        if os.path.exists(os.path.join(SVG_DIR, "demo_%s.svg" % fig)):
            continue
        for area in sorted(os.listdir(os.path.join(TOP, "demos"))):
            cand = os.path.join(TOP, "demos", area, fig + ".py")
            if os.path.isfile(cand):
                yield cand

if __name__ == "__main__":
    args = sys.argv[1:]
    if not args:
        sys.exit(__doc__)
    for demo in (missing() if args == ["--missing"] else args):
        print(draw(demo))
