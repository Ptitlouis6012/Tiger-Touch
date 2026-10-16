#!/usr/bin/env python3
"""Fail on a tracked text file that is CRLF, carries a byte-order mark, is not
valid UTF-8, or contains an invisible or bidirectional control character.

The vendored board component is the one place that keeps CRLF on purpose
(.gitattributes marks it `-text`), so it is skipped here.
"""
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TEXT_EXT = {".c", ".h", ".py", ".sh", ".md", ".txt", ".json", ".yml", ".yaml", ".csv", ".html",
            ".js", ".css", ".cmake", ".ini", ".defaults", ".lock"}
SKIP = ("firmware/components/PandaTouch_IDF/",)
BAD_CHARS = {0x200B, 0x200C, 0x200D, 0x2060, 0xFEFF, 0x202A, 0x202B, 0x202C, 0x202D, 0x202E,
             0x2066, 0x2067, 0x2068, 0x2069}


def main():
    out = subprocess.run(["git", "ls-files", "-co", "--exclude-standard"], cwd=ROOT,
                         capture_output=True, text=True).stdout
    bad = []
    for rel in out.splitlines():
        if not rel or rel.startswith(SKIP) or os.path.splitext(rel)[1].lower() not in TEXT_EXT:
            continue
        raw = open(os.path.join(ROOT, rel), "rb").read()
        if raw.startswith(b"\xef\xbb\xbf"):
            bad.append((rel, "byte-order mark"))
            continue
        if b"\r\n" in raw:
            bad.append((rel, "CRLF line endings"))
        if b"\x00" in raw:
            bad.append((rel, "NUL byte"))
        try:
            text = raw.decode("utf-8")
        except UnicodeDecodeError:
            bad.append((rel, "not valid UTF-8"))
            continue
        for ch in set(text):
            if ord(ch) in BAD_CHARS:
                bad.append((rel, "invisible or bidirectional control U+%04X" % ord(ch)))
    if bad:
        print("file format problems (%d):" % len(bad))
        for rel, what in bad:
            print("  %s  %s" % (rel, what))
        return 1
    print("check-file-format: ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
