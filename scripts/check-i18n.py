#!/usr/bin/env python3
"""The translation table: every row has eleven columns, none is empty, the printf
conversions agree across a row, each column is in its own script, and every row
is used.

`X(S_NAME, en, fr, de, es, it, zh, pt, pt-pt, pl, ru, nl)`. A missing or empty
column falls back to English at run time and nobody notices, which is exactly
why it is checked here.

The table is positional: a value written in the wrong place shows another
language. A shifted column is caught by its script - Chinese only in the zh
column, Cyrillic only in the ru one, and each of those two holding its own
script unless the value is the English one (Wi-Fi, IP). Two Latin columns
swapped (pt and pt-pt) are not caught: read a new row against its neighbours.

A row no screen uses is reported as a warning, not a failure: a key may be
added before its screen.
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TABLE = os.path.join(ROOT, "firmware", "main", "i18n_table.h")
COLUMNS = 11
LANGS = ["en", "fr", "de", "es", "it", "zh", "pt", "pt-pt", "pl", "ru", "nl"]
CJK = re.compile(r"[\u3000-\u9fff\uff00-\uffef]")
CYRILLIC = re.compile(r"[\u0400-\u04ff]")
SOURCES = os.path.join(ROOT, "firmware", "main")
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


def orphans(ids):
    """Rows no source of firmware/main names (the table and its header aside)."""
    used = set()
    for dirpath, _dirs, files in os.walk(SOURCES):
        for f in files:
            if f.endswith((".c", ".h")) and f not in ("i18n_table.h",):
                used |= set(re.findall(r"\bS_[A-Z0-9_]+\b", open(os.path.join(dirpath, f), encoding="utf-8",
                                                                   errors="replace").read()))
    return sorted(i for i in ids if i not in used)


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
        for lang, s in zip(LANGS, items):
            if lang != "zh" and CJK.search(s):
                problems.append("%s: Chinese text in the %s column (a shifted row?)" % (name, lang))
            if lang != "ru" and CYRILLIC.search(s):
                problems.append("%s: Cyrillic text in the %s column (a shifted row?)" % (name, lang))
        en, zh, ru = items[0], items[LANGS.index("zh")], items[LANGS.index("ru")]
        # A loanword taken from the English ("Email" in "Email address") is its own script.
        loan = lambda s: s.lower() in en.lower()
        if zh != en and not loan(zh) and re.search(r"[A-Za-z]{3}", en) and not CJK.search(zh):
            problems.append("%s: the zh column holds no Chinese (a shifted row?)" % name)
        if ru != en and not loan(ru) and re.search(r"[A-Za-z]{3}", en) and not CYRILLIC.search(ru):
            problems.append("%s: the ru column holds no Cyrillic (a shifted row?)" % name)
        ref = CONV.findall(items[0])
        for lang, s in zip(LANGS, items):
            if CONV.findall(s) != ref:
                problems.append("%s: %s has conversions %s, English has %s" % (name, lang, CONV.findall(s), ref))
    unused = orphans(seen)
    for name in unused:
        print("  warning: %s is used by no source in firmware/main" % name)
    if problems:
        print("i18n table (%d rows):" % n)
        for p in problems[:60]:
            print("  " + p)
        return 1
    print("check-i18n: ok (%d rows x %d languages)" % (n, COLUMNS))
    return 0


if __name__ == "__main__":
    sys.exit(main())
