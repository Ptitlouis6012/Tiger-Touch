#!/usr/bin/env python3
"""Fail when the code or its comments are not in English.

Everything committed is English; what a user reads is the only exception, and it
lives in two places: the translation table (i18n_table.h) and the language names
(i18n.c). Anywhere else, a character outside ASCII is either a translation that
escaped the table or a comment in another language.

A handful of typographic symbols are legitimate in a comment or a log line and
are allowed. This is a heuristic: it cannot see a French comment written without
an accent. It is the cheap half of the check; reading the diff is the other.
"""
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

ALLOWED_FILES = {
    "firmware/main/i18n_table.h",   # the translations
    "firmware/main/i18n.c",         # the language names, in their own language
}
ALLOWED_CHARS = set("·°…—–×→µ≤≥≈")
CHECKED = ("firmware/main/", "firmware/tools/", "scripts/")
SKIP = ("firmware/main/ui/fonts/", "firmware/main/ui/boot_splash.c", "firmware/main/ui/brand_logos.c",
        "firmware/main/ui/pictos.c")
EXT = (".c", ".h", ".py", ".sh")


def main():
    out = subprocess.run(["git", "ls-files", "-co", "--exclude-standard"], cwd=ROOT,
                         capture_output=True, text=True).stdout
    bad = []
    for rel in out.splitlines():
        if not rel.startswith(CHECKED) or rel.startswith(SKIP) or rel in ALLOWED_FILES:
            continue
        if not rel.endswith(EXT):
            continue
        for n, line in enumerate(open(os.path.join(ROOT, rel), encoding="utf-8"), 1):
            chars = sorted({c for c in line if ord(c) > 127 and c not in ALLOWED_CHARS})
            if chars:
                bad.append((rel, n, "".join(chars)))
    if bad:
        print("text outside ASCII where only English is expected (%d):" % len(bad))
        for rel, n, chars in bad[:50]:
            print("  %s:%d  U+%s" % (rel, n, " U+".join("%04X" % ord(c) for c in chars)))
        return 1
    print("check-english: ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
