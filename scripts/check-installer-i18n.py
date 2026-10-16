#!/usr/bin/env python3
"""The installer page's dictionary: eleven languages, the same keys in each,
nothing empty, and the page and the dictionary agree.

A missing key falls back to English in the browser and nobody notices, which is
why it is checked here. The checks:

  - the languages are the device's, in the device's order;
  - every language has exactly the keys English has, none empty;
  - `_html` values hold <b>...</b> and no other markup, the rest hold none;
  - step 2 names the button with the word the button carries in that language;
  - the labels of ESP Web Tools' own window (English only, it cannot be
    translated) are quoted untranslated where the page tells the reader to use
    them;
  - every key the page asks for exists, and every key is used by the page.
"""
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DICT = os.path.join(ROOT, "installer", "i18n.js")
PAGE = os.path.join(ROOT, "installer", "index.html")
LANGS = ["en", "fr", "de", "es", "it", "zh", "pt", "pt-pt", "pl", "ru", "nl"]
# Labels of the ESP Web Tools window, as that window prints them.
DIALOG_LABELS = {
    "n3_html": ["Install Tiger Touch", "Erase device", "Next"],
    "keep_html": ["Erase device"],
}
# Keys read by the script of the page rather than by a data-i18n attribute.
USED_BY_SCRIPT = {"lang_name", "title"}
LINE = re.compile(r'^  ([a-z_0-9]+): ("(?:[^"\\]|\\.)*"),$')
HEAD = re.compile(r'^(?:([a-z]{2})|"([a-z]{2}-[a-z]{2})"): \{$')


def load():
    langs, order, cur = {}, [], None
    for n, line in enumerate(open(DICT, encoding="utf-8"), 1):
        line = line.rstrip("\n")
        m = HEAD.match(line)
        if m:
            cur = m.group(1) or m.group(2)
            order.append(cur)
            langs[cur] = {}
            continue
        if line == "},":
            cur = None
            continue
        m = LINE.match(line)
        if m and cur:
            if m.group(1) in langs[cur]:
                print("  %s: %s defined twice" % (cur, m.group(1)))
            langs[cur][m.group(1)] = json.loads(m.group(2))
            continue
        if cur and line.strip() and not line.strip().startswith("//"):
            print("  i18n.js:%d: cannot read this line inside %s" % (n, cur))
    return order, langs


def main():
    problems = []
    order, langs = load()
    if order != LANGS:
        problems.append("languages are %s, expected %s" % (order, LANGS))
    en = langs.get("en", {})
    for code in order:
        d = langs[code]
        for k in sorted(set(en) - set(d)):
            problems.append("%s: missing %s" % (code, k))
        for k in sorted(set(d) - set(en)):
            problems.append("%s: %s is not in English" % (code, k))
        for k, v in d.items():
            if not v.strip():
                problems.append("%s: %s is empty" % (code, k))
            tags = re.findall(r"</?[^>]*>", v)
            if k.endswith("_html"):
                if any(t not in ("<b>", "</b>") for t in tags):
                    problems.append("%s: %s holds markup other than <b>" % (code, k))
                if v.count("<b>") != v.count("</b>"):
                    problems.append("%s: %s has unbalanced <b>" % (code, k))
            elif tags or "&" in v:
                problems.append("%s: %s is plain text and holds markup" % (code, k))
        if d.get("n2_html") and d.get("cta_btn") and "<b>%s</b>" % d["cta_btn"] not in d["n2_html"]:
            problems.append("%s: step 2 does not name the button as <b>%s</b>" % (code, d["cta_btn"]))
        for k, labels in DIALOG_LABELS.items():
            for label in labels:
                if k in d and "<b>%s</b>" % label not in d[k]:
                    problems.append("%s: %s must quote <b>%s</b> untranslated" % (code, k, label))

    page = open(PAGE, encoding="utf-8").read()
    asked = set(re.findall(r'data-i18n(?:-html|-aria)?="([a-z_0-9]+)"', page))
    for k in sorted(asked - set(en)):
        problems.append("index.html asks for %s, which the dictionary lacks" % k)
    for k in sorted(set(en) - asked - USED_BY_SCRIPT):
        problems.append("%s is in the dictionary and not used by index.html" % k)

    if problems:
        print("installer i18n (%d):" % len(problems))
        for p in problems:
            print("  " + p)
        return 1
    print("check-installer-i18n: ok (%d languages, %d keys)" % (len(order), len(en)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
