#!/usr/bin/env python3
"""Every relative link in the tracked documents points at a file that exists.

The documentation is cross-linked on purpose - each page sends the reader to the
ones next to it - and a link that leads nowhere is the first thing to rot when a
file moves. Anchors are not checked, only files. The vendored board component is
upstream's text and is left as it came.
"""
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SKIP = ("firmware/components/",)
LINK = re.compile(r'\]\(([^)\s]+)\)|<img[^>]*\ssrc="([^"]+)"')


def main():
    out = subprocess.run(["git", "ls-files", "-co", "--exclude-standard", "*.md", "llms.txt"],
                         cwd=ROOT, capture_output=True, text=True, encoding="utf-8").stdout.split()
    files = [f for f in out if not f.startswith(SKIP)]
    if not files:
        print("check-docs: no document found - checking nothing is a failure")
        return 2
    bad = []
    for f in files:
        path = os.path.join(ROOT, f)
        if not os.path.exists(path):
            continue
        text = open(path, encoding="utf-8").read()
        for n, line in enumerate(text.splitlines(), 1):
            for m in LINK.finditer(line):
                target = (m.group(1) or m.group(2)).split("#")[0]
                if not target or target.startswith(("http://", "https://", "mailto:")):
                    continue
                if not os.path.exists(os.path.normpath(os.path.join(os.path.dirname(path), target))):
                    bad.append("%s:%d  broken link -> %s" % (f, n, target))
    if bad:
        print("check-docs: %d broken link(s):" % len(bad))
        for b in bad:
            print("  " + b)
        return 1
    print("check-docs: ok (%d documents)" % len(files))
    return 0


if __name__ == "__main__":
    sys.exit(main())
