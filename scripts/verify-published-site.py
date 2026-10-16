#!/usr/bin/env python3
"""Read the published site back and compare it with the release.

    python scripts/verify-published-site.py --url https://.../ --version 2.0.0 --dist dist

A deployment can report success and go on serving the previous build; in a
sibling project that told every device in the field it was up to date against a
release that had already shipped, with nothing red anywhere. The only proof is
fetching what is served. Retries for a few minutes: the CDN takes a moment.
"""
import argparse
import hashlib
import json
import pathlib
import sys
import time
import urllib.request


def fetch(url):
    req = urllib.request.Request(url, headers={"Cache-Control": "no-cache"})
    with urllib.request.urlopen(req, timeout=30) as r:
        return r.read()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--url", required=True)
    ap.add_argument("--version", required=True)
    ap.add_argument("--dist", required=True)
    a = ap.parse_args()
    base = a.url.rstrip("/") + "/"
    want = hashlib.sha256((pathlib.Path(a.dist) / ("tiger-touch-v%s.bin" % a.version)).read_bytes()).hexdigest()
    last = ""
    for attempt in range(20):
        try:
            v = json.loads(fetch(base + "version.json?t=%d" % time.time()))
            m = json.loads(fetch(base + "manifest.json?t=%d" % time.time()))
            ok = (v.get("version") == a.version and v.get("firmware_sha256") == want
                  and m.get("version") == a.version)
            if ok:
                img = fetch(v["firmware_url"])
                if hashlib.sha256(img).hexdigest() != want:
                    print("error: the release asset does not hash to the manifest's SHA-256")
                    return 1
                print("published site serves %s, and its image matches" % a.version)
                return 0
            last = "serves %s / %s" % (v.get("version"), m.get("version"))
        except Exception as e:  # network, JSON, 404 while the CDN catches up
            last = str(e)
        time.sleep(15)
    print("error: the published site does not serve %s (%s)" % (a.version, last))
    return 1


if __name__ == "__main__":
    sys.exit(main())
