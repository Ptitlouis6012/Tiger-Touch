#!/usr/bin/env python3
"""Fail when a log line prints something that identifies a person or a machine.

A serial console dump is what people paste into a bug report. So a log line may
say what happened and to which kind of thing ("a Bambu Lab link opened"), never
whose it was: no Wi-Fi name, address, serial number, account identifier, e-mail,
credential, token, or the name a user gave a printer.

It reads each ESP_LOGx call's arguments (the format string is not looked at) and
fails on an identifier from that list. When a call needs a value of this kind to
be useful, it is the wrong call: log the outcome instead.
"""
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

SENSITIVE = re.compile(
    r"(?:->|\.|\b)(ssid|pass|password|passwd|token|uid|email|mail|sn|serial|ip|cc|code|"
    r"name|devid|dev_id|user|username|board|board_id|mainboard|mac|bssid|refresh)\b")
CALL = re.compile(r"ESP_LOG[EWIDV]\s*\(")
STRING = re.compile(r'"(?:[^"\\]|\\.)*"')
CHECKED = ("firmware/main/",)
SKIP = ("firmware/main/ui/fonts/",)


def calls(text):
    for m in CALL.finditer(text):
        i = m.end()
        depth = 1
        j = i
        while j < len(text) and depth:
            c = text[j]
            if c == '"':
                k = j + 1
                while k < len(text) and text[k] != '"':
                    k += 2 if text[k] == "\\" else 1
                j = k
            elif c == "(":
                depth += 1
            elif c == ")":
                depth -= 1
            j += 1
        yield text.count("\n", 0, m.start()) + 1, text[i:j - 1]


def main():
    out = subprocess.run(["git", "ls-files", "-co", "--exclude-standard"], cwd=ROOT,
                         capture_output=True, text=True).stdout
    bad = []
    for rel in out.splitlines():
        if not rel.endswith(".c") or not rel.startswith(CHECKED) or rel.startswith(SKIP):
            continue
        text = open(os.path.join(ROOT, rel), encoding="utf-8").read()
        for line, args in calls(text):
            # the first argument is the tag, the string literals are the format
            body = STRING.sub('""', args)
            m = SENSITIVE.search(body)
            if m:
                bad.append((rel, line, m.group(1)))
    if bad:
        print("log lines that print identifying values (%d):" % len(bad))
        for rel, n, what in bad:
            print("  %s:%d  %s" % (rel, n, what))
        return 1
    print("check-log-privacy: ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
