#!/usr/bin/env python3
"""The version is written once, PROJECT_VER in firmware/CMakeLists.txt.

  - it is MAJOR.MINOR.PATCH;
  - the newest released heading of CHANGELOG.md (the first `## [X.Y.Z]`, below
    `## [Unreleased]`) is that version, or older by exactly the release being
    prepared: PROJECT_VER is never behind the changelog;
  - on a tag build (GITHUB_REF_NAME=vX.Y.Z) the tag equals it, so a mistyped tag
    fails before anything is published rather than after.
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def read(p):
    return open(os.path.join(ROOT, p), encoding="utf-8").read()


def key(v):
    return tuple(int(x) for x in v.split("."))


def main():
    cm = re.search(r'^set\(PROJECT_VER "([^"]*)"\)', read("firmware/CMakeLists.txt"), re.M)
    if not cm:
        print("check-version: PROJECT_VER not found in firmware/CMakeLists.txt")
        return 1
    v = cm.group(1)
    if not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", v):
        print("check-version: PROJECT_VER '%s' is not MAJOR.MINOR.PATCH" % v)
        return 1
    log = read("CHANGELOG.md")
    if "## [Unreleased]" not in log:
        print("check-version: CHANGELOG.md has no '## [Unreleased]' section to write into")
        return 1
    top = re.search(r"^## \[([0-9]+\.[0-9]+\.[0-9]+)\]", log, re.M)
    if top and key(top.group(1)) > key(v):
        print("check-version: CHANGELOG.md has [%s], newer than PROJECT_VER %s" % (top.group(1), v))
        print("  run: bash scripts/bump-version.sh %s" % top.group(1))
        return 1
    if top and top.group(1) != v:
        print("check-version: PROJECT_VER is %s but CHANGELOG.md's newest release is [%s]" % (v, top.group(1)))
        print("  bump-version.sh moves both together; run it rather than editing one of them")
        return 1
    ref = os.environ.get("GITHUB_REF_NAME", "")
    if os.environ.get("GITHUB_REF_TYPE") == "tag" or (ref.startswith("v") and re.fullmatch(r"v[0-9.]+", ref)):
        if ref[1:] != v:
            print("::error::tag %s disagrees with PROJECT_VER %s" % (ref, v))
            print("  not pushed yet: delete the tag and tag again. Already public: release the next version;")
            print("  never move a published tag.")
            return 1
        print("check-version: tag %s matches" % ref)
    print("check-version: ok (%s)" % v)
    return 0


if __name__ == "__main__":
    sys.exit(main())
