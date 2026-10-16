#!/usr/bin/env python3
"""Fail when a built image or its debug file carries something of the machine
that built it.

The tree is checked by check-private-data.py; this checks what the compiler made
of it. A build stores the absolute path of every source file in its debug info and
its assert messages, and with it the name of whoever built it. The images are what
gets flashed and the ELF is what gets shared to decode a crash.

Looked for, in every image and ELF of the build directory:
  - a path of a user's home directory (C:/Users/..., /Users/..., /home/...);
  - any term of the local, ignored `.private-terms` file, as UTF-8 and UTF-16.

Skipped, quietly, when there is no build: this runs after one, not instead of it.

    python scripts/check-build-privacy.py [--build firmware/build]
"""
import argparse
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
IMAGES = [
    "tiger_touch.bin", "tiger_touch.elf",
    os.path.join("bootloader", "bootloader.bin"), os.path.join("bootloader", "bootloader.elf"),
    os.path.join("partition_table", "partition-table.bin"),
]
HOME_PATH = re.compile(rb"(?i)(?:[A-Za-z]:[\\/]+Users[\\/]+|/Users/|/home/|/c/Users/)[A-Za-z0-9._ -]+")


def local_terms():
    p = os.path.join(ROOT, ".private-terms")
    if not os.path.exists(p):
        return []
    out = []
    for line in open(p, encoding="utf-8"):
        t = line.strip()
        if t and not t.startswith("#"):
            out.append(t)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--build", default=os.path.join(ROOT, "firmware", "build"))
    args = ap.parse_args()

    present = [os.path.join(args.build, r) for r in IMAGES if os.path.exists(os.path.join(args.build, r))]
    if not present:
        print("check-build-privacy: no build in %s, nothing to check" % args.build)
        return 0

    terms = local_terms()
    bad = []
    for path in present:
        data = open(path, "rb").read()
        low = data.lower()
        name = os.path.relpath(path, args.build)
        for m in sorted({m.group(0) for m in HOME_PATH.finditer(data)})[:5]:
            bad.append("%s: a home-directory path: %s" % (name, m.decode("latin-1")[:60]))
        for i, t in enumerate(terms):
            for enc in ("utf-8", "utf-16le"):
                if t.lower().encode(enc) in low:
                    # Never print the term itself: this output can be pasted anywhere.
                    bad.append("%s: private term #%d (%s)" % (name, i + 1, enc))
    if bad:
        print("what the build carries about its machine (%d):" % len(bad))
        for b in bad:
            print("  " + b)
        print("fix: CONFIG_APP_REPRODUCIBLE_BUILD=y in firmware/sdkconfig.defaults, then delete")
        print("firmware/sdkconfig and rebuild.")
        return 1
    print("check-build-privacy: ok (%d files, %d local terms)" % (len(present), len(terms)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
