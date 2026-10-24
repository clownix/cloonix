# httpdoc — the clownix.net pages

Five pages, one stylesheet, six screenshots, the cloonix logos and the two books. Run `./cmd`, copy the
directory to the web root and the site is live; of the pages, `./cmd` only
writes the version.

    index.html     what cloonix is, and whether it is the tool you want
    learn.html     what there is to learn with it, and in what order
    demos.html     five demos as the browser shows them, one screenshot each,
                   the windows one twice (its lock screen)
    img/           the screenshots of demos.html, taken from the web canvas,
                   and the cloonix images: cloonix64.png is the logo of the nav
                   of every page, cloonix32.png the favicon,
                   smile_cloonix_title.png the title of index.html
    start.html     from a tarball to a running network, in five steps
    downloads.html the three tarballs and the four directories of guest images
    style.css      the whole design
    xdoc_user/     the sphinx sources of the user book
    xdoc_dev/      the sphinx sources of the developer book
    xdoc_theme/    the sphinx theme: the header and footer of the pages around a chapter
    cmd, allclean  build and remove build_user/ and build_dev/

## What is deliberate

**No javascript, no framework, no webfont, no build step.** Five pages and
one stylesheet. They render with javascript off and need nothing fetched from a third
party.

**System fonts, the colours of the canvas.** A serif for the text, a sans
for the headings, the nav and the tables, a monospace for the commands, all
taken from the machine of the reader. The colours are those the canvas
gives its objects: the blue of a cable for the links, the grey of a lan for
the footer, the green dot of a sniffable interface for the bullets.

**The figures are inline SVG**, styled by the same stylesheet as the text; the six screenshots of demos.html are png, being the browser itself.
Change a colour in `style.css` and every diagram follows, and no figure
costs a request of its own.

**Five pages, not ten.** One page each for *what is this*, *what would I do
with it*, *what does it look like*, *how do I run it* and *where are the
files*. Everything else is in the two books, linked
rather than summarised.

**Every sentence carries a fact, and no sentence carries a judgement.** No
"powerful and flexible", no "worth knowing", no "the point is". Every claim
is checkable: the toy needs no root, sixty routers start in about a minute,
the client is one python file. `debug/check_doc_words.sh` enforces it over
this directory and both books.

**The failures are on the page.** `start.html` ends with the things that go
wrong on a first install, each with its cause. The inotify limit and a
locked-down user namespace have their own lines: neither produces an error
message anywhere.

## Wiring it up

The sources of the two books are here, `xdoc_user` and `xdoc_dev`, and one
script builds both:

    ./cmd        writes build_user/ and build_dev/
    ./allclean   removes build_user/ and build_dev/

`index.html` is the one index of the site. The master page of each book is
named after the book, `build_user/user.html` and `build_dev/dev.html`, and
`xdoc_theme/layout.html` gives every page of the books the header, the footer
and `style.css` of the site: no sphinx stylesheet, no script, no search page,
no single-page edition. The rules for the sphinx body are the `.book` block of
`style.css`.

The nav of every page, the pages of the books included, links to
`build_user/user.html` as Documentation and to `build_dev/dev.html` as
Developers, so after `./cmd` both books open from `index.html`, in the tree
as on the web root: the directory is copied as it is. The
two build directories are in `.gitignore`.

`./cmd` rather than a bare `sphinx-build`: it reads the version out of
`cloonix/cloonix_config.py` and substitutes it wherever the sources say
`__LAST__`, so no page of the books carries a version number by hand.

The pages are plain files with the version written in two of them, and
`./cmd` writes the one of `cloonix/cloonix_config.py` there, so the pages and
the books name the same tarballs:

    index.html     the <p class="ver"> line, the two source tarballs of the Install band
    downloads.html the two source tarballs, the wget block under them

## The downloads

`downloads.html` and the Downloads band of `index.html` link to a
`downloads/` directory beside the pages, which this tree does not hold:

    downloads/cloonix-source-<num>.tar.gz
    downloads/cloonix-targz_store-<num>.tar.gz   the targz_store directory of the
                                              tree the sources were made from
    downloads/cloonix-bundle.tar.gz     ./doitall at the root of the tree
    downloads/cloonix-toy.tar.xz        toy_targz/create_toy_targz.sh
    downloads/cloonix-frr-toy.tar.xz          toy_targz/create_frr_toy_targz.sh
    downloads/cloonix-bird-toy.tar.xz         toy_targz/create_bird_toy_targz.sh
    downloads/cloonix-suricata-toy.tar.xz     toy_targz/create_suricata_toy_targz.sh
    downloads/cloonix-bind-toy.tar.xz         toy_targz/create_bind_toy_targz.sh
    downloads/cnt_bulk/<image>.tar.gz         the four directories that
    downloads/kvm_bulk/<image>.qcow2.gz       vm_create/allbulk.sh --go
    downloads/cnt_demo_bulk/<image>.tar.gz    leaves in the home: copy them
    downloads/kvm_demo_bulk/<image>.qcow2.gz  as they are

The four directory titles of `downloads.html` link to the directory itself,
so the web server must list a directory that has no index. Every image is
also linked by name: the names are those of `./doitall.sh --list` in
`vm_create/cnt_kvm` and `vm_create/cnt_kvm_demo`, and an image added to or
removed from those two lists is added to or removed from the page by hand.

## The screenshots and the logos

The images of `img/` other than the six screenshots are the cloonix logos
and drawings; the pages use three of them, cloonix64.png (the nav, shown at
28 pixels), cloonix32.png (the favicon) and smile_cloonix_title.png (the
title of index.html), and the books get the same nav and favicon from
`xdoc_theme/layout.html`.

The six screenshot png of `img/` are the web canvas as the browser shows it, one per
demo of `demos.html`. They are taken again when the canvas changes. Every
other figure of the site and of the books is an inline or sphinx SVG that
explains a relationship: the processes, the sockets, the frames.
