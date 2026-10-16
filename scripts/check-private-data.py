#!/usr/bin/env python3
"""Fail when a tracked file carries something that must never be published.

This repository is private today and may not be tomorrow, and a git history is
permanent. So the question is asked on every commit, not at the moment of
opening it up.

What is looked for, in every text file:
  - an e-mail address (the GitHub no-reply form is the one exception);
  - a private IPv4 address (10.x, 172.16-31.x, 192.168.x, 169.254.x);
  - a MAC address;
  - a personal path (C:\\Users\\..., /Users/..., /home/...);
  - a private key block, or a long secret-looking token;
  - any term listed in a local, ignored `.private-terms` file - one per line -
    which is where a Wi-Fi name, a serial number or an account handle goes,
    precisely because it cannot be written in a tracked file.

What is NOT looked for: a person's name in prose. A pattern cannot see that;
reading the diff does.

The Firebase web key in firmware/main/cloud.c is public by design (it is served
without authentication by the TigerTag project's own hosting) and is the single
allowlisted secret-shaped string.
"""
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Generated data: long runs that look like secrets and are not. Only the first
# lines of these (what the generator wrote about itself) are read, never the data.
SKIP_PREFIXES = (
    "firmware/main/ui/fonts/",
    "firmware/main/ui/boot_splash.c",
    "firmware/main/ui/brand_logos.c",
    "firmware/main/ui/pictos.c",
    "firmware/dependencies.lock",
    "firmware/components/PandaTouch_IDF/docs/images/",
)
TEXT_EXT = {".c", ".h", ".py", ".sh", ".ps1", ".md", ".txt", ".json", ".yml", ".yaml",
            ".csv", ".defaults", ".html", ".js", ".css", ".cmake", ".lock", ".ini"}

PUBLIC_KEYS = {
    # cloud.c: the project's public web client key.
    "AIzaSyCkxPTs_Cv0KVLqsZj-UKWWqIY0OtfVpnw",
}

EMAIL = re.compile(r"[A-Za-z0-9._%+-]+@[A-Za-z0-9-]+(?:\.[A-Za-z0-9-]+)*\.[A-Za-z]{2,}")
IPV4 = re.compile(r"\b(?:10\.\d{1,3}\.\d{1,3}\.\d{1,3}|172\.(?:1[6-9]|2\d|3[01])\.\d{1,3}\.\d{1,3}"
                  r"|192\.168\.\d{1,3}\.\d{1,3}|169\.254\.\d{1,3}\.\d{1,3})\b")
MAC = re.compile(r"\b(?:[0-9A-Fa-f]{2}[:-]){5}[0-9A-Fa-f]{2}\b")
PATH = re.compile(r"(?:[A-Za-z]:\\Users\\|/Users/[A-Za-z]|/home/[a-z])")
PRIVKEY = re.compile(r"-----BEGIN [A-Z ]*PRIVATE KEY-----")
GOOGLE_KEY = re.compile(r"AIza[0-9A-Za-z_\-]{30,}")
LONG_HEX = re.compile(r"\b[0-9A-Fa-f]{32,}\b")


def tracked_files():
    try:
        out = subprocess.run(["git", "ls-files", "-co", "--exclude-standard"], cwd=ROOT,
                             capture_output=True, text=True, check=True).stdout
        return [p for p in out.splitlines() if p]
    except Exception:
        found = []
        for d, _, fs in os.walk(ROOT):
            if any(x in d for x in (os.sep + ".git", os.sep + "build", "managed_components")):
                continue
            for f in fs:
                found.append(os.path.relpath(os.path.join(d, f), ROOT).replace(os.sep, "/"))
        return found


def local_terms():
    p = os.path.join(ROOT, ".private-terms")
    if not os.path.exists(p):
        return []
    terms = []
    for line in open(p, encoding="utf-8"):
        t = line.strip()
        if t and not t.startswith("#"):
            terms.append(t.lower())
    return terms


def main():
    terms = local_terms()
    problems = []
    for rel in tracked_files():
        generated = rel.startswith(SKIP_PREFIXES)
        ext = os.path.splitext(rel)[1].lower()
        if ext not in TEXT_EXT and os.path.basename(rel) not in ("LICENSE", ".gitignore", ".gitattributes"):
            continue
        path = os.path.join(ROOT, rel)
        try:
            text = open(path, encoding="utf-8").read()
        except Exception:
            continue
        lines = text.splitlines()
        if generated:
            # A generated file is data; what a tool wrote about itself is in its
            # header, and that is where a machine's path ends up.
            lines = lines[:12]
        for n, line in enumerate(lines, 1):
            low = line.lower()
            for m in EMAIL.finditer(line):
                if not m.group(0).lower().endswith("@users.noreply.github.com"):
                    problems.append((rel, n, "e-mail address"))
            if IPV4.search(line):
                problems.append((rel, n, "private IPv4 address"))
            if MAC.search(line):
                problems.append((rel, n, "MAC address"))
            if PATH.search(line):
                problems.append((rel, n, "personal path"))
            if PRIVKEY.search(line):
                problems.append((rel, n, "private key block"))
            for m in GOOGLE_KEY.finditer(line):
                if m.group(0) not in PUBLIC_KEYS:
                    problems.append((rel, n, "API key"))
            if LONG_HEX.search(line) and not rel.startswith("docs/") and not generated:
                problems.append((rel, n, "long hexadecimal identifier"))
            for t in terms:
                if t in low:
                    problems.append((rel, n, "a term from .private-terms"))
    if problems:
        print("private data in tracked files (%d):" % len(problems))
        for rel, n, what in problems[:60]:
            print("  %s:%d  %s" % (rel, n, what))
        if len(problems) > 60:
            print("  ... and %d more" % (len(problems) - 60))
        return 1
    print("check-private-data: ok (%d local term%s)" % (len(terms), "" if len(terms) == 1 else "s"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
