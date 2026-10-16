#!/usr/bin/env python3
"""The translation table: every row has eleven columns, none is empty, and the
printf conversions agree across a row.

`X(S_NAME, en, fr, de, es, it, zh, pt, pt-pt, pl, ru, nl)`. A missing or empty
column falls back to English at run time and nobody notices, which is exactly
why it is checked here.
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TABLE = os.path.join(ROOT, "firmware", "main", "i18n_table.h")
COLUMNS = 11
LANGS = ["en", "fr", "de", "es", "it", "zh", "pt", "pt-pt", "pl", "ru", "nl"]
CONV = re.compile(r"%[-+ 0#]*\d*(?:\.\d+)?[diuxXsc]")


def parse(text):
    """Yield (id, [strings]) for every X(...) row, honouring C string syntax."""
    i = 0
    while True:
        i = text.find("X(", i)
        if i < 0:
            return
        j = i + 2
        name = ""
        while text[j] not in ",\n":
            name += text[j]
            j += 1
        j += 1
        items = []
        while True:
            while text[j] in " \n\t":
                j += 1
            if text[j] == ")":
                break
            if text[j] == ",":
                j += 1
                continue
            if text[j] != '"':
                raise ValueError("unexpected %r after %s" % (text[j], name))
            j += 1
            s = ""
            while text[j] != '"':
                if text[j] == "\\":
                    s += text[j:j + 2]
                    j += 2
                else:
                    s += text[j]
                    j += 1
            j += 1
            items.append(s)
        yield name.strip(), items
        i = j


def main():
    text = open(TABLE, encoding="utf-8").read()
    # A comment block must not be mistaken for a row.
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    problems = []
    n = 0
    seen = set()
    for name, items in parse(text):
        n += 1
        if name in seen:
            problems.append("%s: defined twice" % name)
        seen.add(name)
        if len(items) != COLUMNS:
            problems.append("%s: %d columns, expected %d" % (name, len(items), COLUMNS))
            continue
        for lang, s in zip(LANGS, items):
            if not s.strip():
                problems.append("%s: empty %s" % (name, lang))
        ref = CONV.findall(items[0])
        for lang, s in zip(LANGS, items):
            if CONV.findall(s) != ref:
                problems.append("%s: %s has conversions %s, English has %s" % (name, lang, CONV.findall(s), ref))
    if problems:
        print("i18n table (%d rows):" % n)
        for p in problems[:60]:
            print("  " + p)
        return 1
    print("check-i18n: ok (%d rows x %d languages)" % (n, COLUMNS))
    return 0


if __name__ == "__main__":
    sys.exit(main())
