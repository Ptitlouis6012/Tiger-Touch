#!/usr/bin/env python3
"""Fail when bench tooling has found its way into the tree.

While a device is being brought up, it is handy to have a serial console that
dumps the screen, a fake printer to look at a layout, a memory trace at every
step. All of that is a way into a shipped device, and none of it belongs in the
product. Such an aid is built in a local, ignored folder; this guard is what
makes "temporarily" impossible to commit.

It fails on file names and on symbols the project has used for exactly that.
It cannot know the next one: extend the lists the day one is found.
"""
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

FORBIDDEN_FILES = re.compile(
    r"(debug_console|bench\.|snapseq|watch_prints|restart_cycles|splashcap|"
    r"screen_?capture|log_?capture|serial_?console|fake_?printer|demo_?printer)", re.I)

FORBIDDEN_SYMBOLS = [
    (re.compile(r"\bdebug_console_start\b"), "serial capture console"),
    (re.compile(r"\bui_dashboard_demo\b"), "demo card"),
    (re.compile(r"\bui_dashboard_dump_geometry\b"), "geometry dump"),
    (re.compile(r"\bcloud_keepalive_kick\b"), "bench kick of the keep-alive"),
    (re.compile(r"\"S:(BEGIN|END|CRC|OK|FAIL|GEOM|ACCT)"), "capture protocol"),
    (re.compile(r"\bsnapf?\b\s*[\"']"), "screenshot command"),
    (re.compile(r"\bfake_printer\b|\bdemo_view\b|\bdemo_nozzles\b"), "demo data"),
    (re.compile(r"costs of '%s'"), "memory-cost measurement log"),
    (re.compile(r"\bPT_BENCH\b|\bTIGERTOUCH_BENCH\b"), "bench build switch"),
]

CODE_EXT = (".c", ".h", ".py", ".sh", ".ps1", ".cmake", ".txt", ".yml")
SELF = {"scripts/check-bench-residue.py"}


def files():
    out = subprocess.run(["git", "ls-files", "-co", "--exclude-standard"], cwd=ROOT,
                         capture_output=True, text=True).stdout
    return [p for p in out.splitlines() if p]


def main():
    bad = []
    for rel in files():
        base = os.path.basename(rel)
        if rel not in SELF and FORBIDDEN_FILES.search(base):
            bad.append((rel, 0, "file name looks like bench tooling"))
        if rel in SELF or not rel.endswith(CODE_EXT):
            continue
        if rel.startswith(("firmware/main/ui/fonts/", "firmware/components/PandaTouch_IDF/")):
            continue
        try:
            text = open(os.path.join(ROOT, rel), encoding="utf-8").read()
        except Exception:
            continue
        for n, line in enumerate(text.splitlines(), 1):
            for pat, what in FORBIDDEN_SYMBOLS:
                if pat.search(line):
                    bad.append((rel, n, what))
    if bad:
        print("bench residue in the tree (%d):" % len(bad))
        for rel, n, what in bad:
            print("  %s%s  %s" % (rel, (":%d" % n) if n else "", what))
        return 1
    print("check-bench-residue: ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
