#!/usr/bin/env python3
"""The site's dictionaries: eleven languages, the same keys in each, nothing
empty, and the pages and the dictionaries agree.

A missing key falls back to English in the browser and nobody notices, which is
why it is checked here. The checks:

  - the languages are the device's, in the device's order;
  - every language has exactly the keys English has, none empty;
  - `_html` values hold <b>...</b> and no other markup, the rest hold none;
  - step 2 names the button with the word the button carries in that language;
  - the labels of ESP Web Tools' own window (English only, it cannot be
    translated) are quoted untranslated where the page tells the reader to use
    them;
  - {name} and {date} survive translation;
  - every key a page asks for exists, and every key is used by a page (the
    installer, the brand pages' template, or a script);
  - each brand's words (installer/brand-i18n/<lang>.json) have every field
    of installer/brand-pages.json, lists as long, plain text only.
"""
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DICT = os.path.join(ROOT, "installer", "i18n.js")
PAGE = os.path.join(ROOT, "installer", "index.html")
TEMPLATE = os.path.join(ROOT, "scripts", "make-brand-pages.py")
BRANDS = os.path.join(ROOT, "installer", "brand-pages.json")
BRAND_I18N = os.path.join(ROOT, "installer", "brand-i18n")
LANGS = ["en", "fr", "de", "es", "it", "zh", "pt", "pt-pt", "pl", "ru", "nl"]
# Labels of the ESP Web Tools window, as that window prints them.
DIALOG_LABELS = {
    "n3_html": ["Install Tiger Touch", "Erase device", "Next"],
    "keep_html": ["Erase device"],
}
# Keys read by the script of the page rather than by a data-i18n attribute.
USED_BY_SCRIPT = {"lang_name", "title", "bp_title", "rel_latest", "rel_released"}
PLACEHOLDERS = ("{name}", "{date}")
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


def check_brands(order):
    """Each language's brand words against brand-pages.json, the English."""
    problems = []
    fields = ["short", "lead", "models", "connection", "status", "shows", "controls", "setup", "tips",
              "shot_alt", "faq", "note", "via"]
    english = {}
    for b in json.load(open(BRANDS, encoding="utf-8"))["brands"]:
        e = {k: b[k] for k in fields if k in b}
        vias = [m[2] for m in b["lineup"] if len(m) > 2]
        if vias:
            e["via"] = vias[0]
        english[b["slug"]] = e
    present = sorted(f[:-5] for f in os.listdir(BRAND_I18N) if f.endswith(".json"))
    if present != sorted(c for c in order if c != "en"):
        problems.append("brand-i18n holds %s, expected every language but English" % present)

    def plain(where, v):
        if not isinstance(v, str) or not v.strip():
            problems.append("%s is empty or not text" % where)
        elif re.search(r"</?[a-z][^>]*>", v) or "&" in v:
            problems.append("%s holds markup" % where)

    for code in present:
        tr = json.load(open(os.path.join(BRAND_I18N, code + ".json"), encoding="utf-8"))
        for slug in sorted(set(tr) - set(english)):
            problems.append("brand-i18n/%s.json: %s is not a brand" % (code, slug))
        for slug, e in english.items():
            got = tr.get(slug)
            if got is None:
                problems.append("brand-i18n/%s.json: %s missing" % (code, slug))
                continue
            for k in sorted(set(got) - set(e)):
                problems.append("brand-i18n/%s.json: %s.%s is not in English" % (code, slug, k))
            for k, v in e.items():
                where = "brand-i18n/%s.json: %s.%s" % (code, slug, k)
                if k not in got:
                    problems.append(where + " missing")
                elif isinstance(v, list):
                    if not isinstance(got[k], list) or len(got[k]) != len(v):
                        problems.append(where + " has not %d entries" % len(v))
                        continue
                    for i, x in enumerate(got[k]):
                        if isinstance(v[i], list):
                            if not isinstance(x, list) or len(x) != len(v[i]):
                                problems.append("%s[%d] is not a pair" % (where, i))
                                continue
                            for y in x:
                                plain("%s[%d]" % (where, i), y)
                        else:
                            plain("%s[%d]" % (where, i), x)
                else:
                    plain(where, got[k])
    return problems


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
            elif tags or re.search(r"&#?\w+;", v):
                problems.append("%s: %s is plain text and holds markup" % (code, k))
        for k, v in d.items():
            for ph in PLACEHOLDERS:
                if ph in en.get(k, "") and ph not in v:
                    problems.append("%s: %s lost %s" % (code, k, ph))
        if d.get("n2_html") and d.get("cta_btn") and "<b>%s</b>" % d["cta_btn"] not in d["n2_html"]:
            problems.append("%s: step 2 does not name the button as <b>%s</b>" % (code, d["cta_btn"]))
        for k, labels in DIALOG_LABELS.items():
            for label in labels:
                if k in d and "<b>%s</b>" % label not in d[k]:
                    problems.append("%s: %s must quote <b>%s</b> untranslated" % (code, k, label))

    asked = set()
    for path in (PAGE, TEMPLATE):
        src = open(path, encoding="utf-8").read()
        keys = set(re.findall(r'data-i18n(?:-html|-aria|-alt)?="([a-z_0-9]+)"', src))
        # The brand template's features, listed by name: a title and a line each.
        keys |= {f + s for f in re.findall(r'"(bp_f\d)"', src) for s in ("_t", "_d")}
        # The TigerSystem cards, listed by key in the brand template.
        keys |= set(re.findall(r'"(eco_[a-z]+)"', src))
        # The media kit's keys, named in the generator (titles, galleries, the copy button).
        keys |= set(re.findall(r'"(mk_[a-z_]+)"', src))
        # Keys the brand pages fill from their own dictionary, not from i18n.js.
        keys = {k for k in keys if not k.startswith(("b_", "short_"))}
        for k in sorted(keys - set(en)):
            problems.append("%s asks for %s, which the dictionary lacks" % (os.path.basename(path), k))
        asked |= keys
    for k in sorted(set(en) - asked - USED_BY_SCRIPT):
        problems.append("%s is in the dictionary and used by no page" % k)

    problems += check_brands(order)

    if problems:
        print("installer i18n (%d):" % len(problems))
        for p in problems:
            print("  " + p)
        return 1
    print("check-installer-i18n: ok (%d languages, %d keys)" % (len(order), len(en)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
