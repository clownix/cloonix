#!/usr/bin/env python3
"""The web site and the figures of the two books.

- every local link and image of httpdoc/*.html exists (downloads/ is filled
  on the server, build_user and build_dev by httpdoc/cmd: skipped when not
  built)
- every svg of xdoc_user/svg and xdoc_dev/svg is well-formed xml and is
  shown by a page of its book
- every image an rst names exists
    ./check_site.py
"""
import glob
import os
import re
import sys
import xml.etree.ElementTree as ET

HERE = os.path.dirname(os.path.abspath(__file__))
DOC = os.path.normpath(os.path.join(HERE, "..", "httpdoc"))
LINK_RE = re.compile(r'(?:href|src)="([^"#?]+)')
IMG_RE = re.compile(r"^\s*\.\. (?:image|figure)::\s*(\S+)", re.M)


def site_links():
    bad = []
    for page in sorted(glob.glob(os.path.join(DOC, "*.html"))):
        for url in LINK_RE.findall(open(page, encoding="utf-8").read()):
            if re.match(r"^[a-z]+:", url) or url.startswith("downloads/"):
                continue
            path = os.path.normpath(os.path.join(DOC, url))
            top = os.path.relpath(path, DOC).split(os.sep)[0]
            if top in ("build_user", "build_dev") and \
               not os.path.isdir(os.path.join(DOC, top)):
                continue
            if not os.path.exists(path):
                bad.append("%s links %s, which does not exist"
                           % (os.path.basename(page), url))
    return bad


def book_figures(book):
    bad = []
    root = os.path.join(DOC, book)
    shown = set()
    for rst in glob.glob(os.path.join(root, "**", "*.rst"), recursive=True):
        for img in IMG_RE.findall(open(rst, encoding="utf-8").read()):
            path = os.path.normpath(os.path.join(root, img.lstrip("/"))
                                    if img.startswith("/") else
                                    os.path.join(os.path.dirname(rst), img))
            shown.add(path)
            if not os.path.exists(path):
                bad.append("%s names %s, which does not exist"
                           % (os.path.relpath(rst, DOC), img))
    for svg in sorted(glob.glob(os.path.join(root, "svg", "*.svg"))):
        try:
            ET.parse(svg)
        except ET.ParseError as err:
            bad.append("%s is not well-formed: %s"
                       % (os.path.relpath(svg, DOC), err))
        if os.path.normpath(svg) not in shown:
            bad.append("%s is shown by no page of %s"
                       % (os.path.relpath(svg, DOC), book))
    return bad


def main():
    bad = site_links()
    for book in ("xdoc_user", "xdoc_dev"):
        bad += book_figures(book)
    for line in bad:
        print("SITE: " + line)
    if bad:
        return 1
    print("the site links and the figures of the two books are all there")
    return 0


if __name__ == "__main__":
    sys.exit(main())
