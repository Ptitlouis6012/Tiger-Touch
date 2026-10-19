#!/usr/bin/env python3
"""Add or remove a translation key in every language at once.

  python3 scripts/i18n-key.py device add S_NAME values.json
  python3 scripts/i18n-key.py device remove S_NAME
  python3 scripts/i18n-key.py site add key_name values.json
  python3 scripts/i18n-key.py site remove key_name

`values.json` holds one object, {"en": "...", "fr": "...", ...}, with the eleven
languages (en, fr, de, es, it, zh, pt, pt-pt, pl, ru, nl); a missing one is an
error. `device` is the screen's table, firmware/main/i18n_table.h (a row, in
column order); `site` is the website's dictionary, installer/i18n.js (the key in
each language block). Editing eleven places by hand is how a value lands in the
wrong column; this writes them together, then the guards check the result.
"""
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
TABLE = ROOT / "firmware" / "main" / "i18n_table.h"
SITE = ROOT / "installer" / "i18n.js"
LANGS = ["en", "fr", "de", "es", "it", "zh", "pt", "pt-pt", "pl", "ru", "nl"]


def load_values(path):
    v = json.loads(pathlib.Path(path).read_text(encoding="utf-8"))
    missing = [l for l in LANGS if not isinstance(v.get(l), str) or not v[l].strip()]
    if missing:
        sys.exit("values.json lacks %s" % ", ".join(missing))
    extra = sorted(set(v) - set(LANGS))
    if extra:
        sys.exit("values.json has unknown languages: %s" % ", ".join(extra))
    return v


def c_string(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n") + '"'


def row_span(text, name):
    """(start, end) of the X(name, ...) row, end after its closing ')' and newline."""
    m = re.search(r"^X\(%s,\s" % re.escape(name), text, re.M)
    if not m:
        return None
    i, in_str = m.end(), False
    while i < len(text):
        ch = text[i]
        if in_str:
            if ch == "\\":
                i += 2
                continue
            if ch == '"':
                in_str = False
        elif ch == '"':
            in_str = True
        elif ch == ")":
            end = i + 1
            if text[end:end + 1] == "\n":
                end += 1
            return m.start(), end
        i += 1
    sys.exit("unterminated row %s" % name)


def device(action, name, values_path=None):
    if not re.fullmatch(r"S_[A-Z0-9_]+", name):
        sys.exit("a device key is S_ and capitals: %s" % name)
    text = TABLE.read_text(encoding="utf-8")
    span = row_span(text, name)
    if action == "remove":
        if not span:
            sys.exit("%s is not in the table" % name)
        TABLE.write_text(text[:span[0]] + text[span[1]:], encoding="utf-8", newline="")
        print("removed %s from %s" % (name, TABLE.relative_to(ROOT)))
        return
    if span:
        sys.exit("%s is already in the table" % name)
    v = load_values(values_path)
    cells = [c_string(v[l]) for l in LANGS]
    row = "X(%s, %s,\n  %s)\n" % (name, ", ".join(cells[:5]), ", ".join(cells[5:]))
    if not text.endswith("\n"):
        text += "\n"
    TABLE.write_text(text + row, encoding="utf-8", newline="")
    print("added %s to %s" % (name, TABLE.relative_to(ROOT)))


def site(action, key, values_path=None):
    if not re.fullmatch(r"[a-z][a-z0-9_]*", key):
        sys.exit("a site key is lower case and underscores: %s" % key)
    text = SITE.read_text(encoding="utf-8")
    line = re.compile(r"^  %s: .*\n" % re.escape(key), re.M)
    if action == "remove":
        n = len(line.findall(text))
        if n == 0:
            sys.exit("%s is not in the dictionary" % key)
        SITE.write_text(line.sub("", text), encoding="utf-8", newline="")
        print("removed %s from %d languages" % (key, n))
        return
    if line.search(text):
        sys.exit("%s is already in the dictionary" % key)
    v = load_values(values_path)
    for lang in LANGS:
        head = "\n%s: {\n" % (lang if "-" not in lang else '"%s"' % lang)
        start = text.index(head)
        end = text.index("\n},", start)
        text = text[:end] + "\n  %s: %s," % (key, json.dumps(v[lang], ensure_ascii=False)) + text[end:]
    SITE.write_text(text, encoding="utf-8", newline="")
    print("added %s in %d languages" % (key, len(LANGS)))


def main():
    a = sys.argv[1:]
    if len(a) < 3 or a[0] not in ("device", "site") or a[1] not in ("add", "remove") or (a[1] == "add") != (len(a) == 4):
        sys.exit(__doc__)
    (device if a[0] == "device" else site)(a[1], a[2], a[3] if len(a) == 4 else None)


if __name__ == "__main__":
    main()
