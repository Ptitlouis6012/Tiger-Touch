#!/usr/bin/env python3
"""The current version has release notes, and they are written.

    python scripts/check-release-notes.py             # the bench check
    python scripts/check-release-notes.py --released  # the tag-time check

Entries accumulate under `## [Unreleased]` as the work is done (WORKLOG.md is
where it is noted first). scripts/bump-version.sh moves them under
`## [X.Y.Z] - date`, or leaves a scaffold line when there was nothing to move.

  - The bench check accepts notes under the current version or still under
    Unreleased: both mean somebody is writing them.
  - The tag-time check accepts only the first: by then the bump has happened.

Scaffold text counts as no notes at all. A heading with "describe this release"
under it is worse than an empty section, because it looks finished.
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCAFFOLD = ["describe this release", "write the release notes", "todo", "tbd", "fill this in"]


def read(p):
    return open(os.path.join(ROOT, p), encoding="utf-8").read()


def section(text, heading):
    m = re.search(r"^## \[%s\][^\n]*$(.*?)(?=^## |\Z)" % re.escape(heading), text, re.S | re.M)
    return m.group(1) if m else None


def substantive(body):
    out = []
    for line in body.splitlines():
        s = line.strip()
        if not s or s.startswith("#") or any(w in s.lower() for w in SCAFFOLD):
            continue
        out.append(s)
    return out


def main():
    released = "--released" in sys.argv
    v = re.search(r'^set\(PROJECT_VER "([^"]*)"\)', read("firmware/CMakeLists.txt"), re.M).group(1)
    text = read("CHANGELOG.md")
    versioned = section(text, v)
    unreleased = section(text, "Unreleased")
    if versioned is None and released:
        print("check-release-notes: CHANGELOG.md has no '## [%s]' section; run scripts/bump-version.sh %s" % (v, v))
        return 1
    if versioned is not None and substantive(versioned):
        print("check-release-notes: ok (%d line(s) under [%s])" % (len(substantive(versioned)), v))
        return 0
    if not released and unreleased is not None and substantive(unreleased):
        print("check-release-notes: ok (%d line(s) under [Unreleased], version %s)" % (len(substantive(unreleased)), v))
        return 0
    if not released and versioned is None and unreleased is not None:
        # Nothing done since the last release: nothing to describe yet.
        print("check-release-notes: ok (nothing since %s)" % v)
        return 0
    print("check-release-notes: [%s] has no release notes - only headings, blank lines or scaffold." % v)
    print("  Write them from WORKLOG.md; it exists so they are not written from memory.")
    return 1


if __name__ == "__main__":
    sys.exit(main())
