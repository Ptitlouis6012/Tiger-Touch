#!/usr/bin/env python3
"""One page per printer brand, for people searching "<brand> Panda Touch".

Each page puts its brand first - the models, the connection, what the screen
shows, the setup, its own questions - then shows the rest of the project: the
other brands (one line each, a link to their page), what Tiger Touch is, how to
install it. A Bambu Lab owner finds what they came for without wading through
Creality's settings, and still learns the screen does both.

Everything is built from one template and one data file:

  installer/brand-pages.json        the words, one entry per brand
  assets/brands/brands.svg          the logos (the strip the README shows)
  assets/devices/brands/<slug>.png  the screen, captured on the device in Demo
                                    mode and set in the 45-degree drawing
  assets/printers/models/*.png      the printers of each lineup (Tiger Studio
                                    Manager's printer library)

and written into the site (`out`), beside the installer page:

  <slug>/index.html, <slug>/favicon.svg, img/<slug>.png, sitemap.xml, robots.txt,
  and a copy of every page in each other language: <lang>/index.html, <lang>/<slug>/

Called by make-manifest.py when the site is built; never committed (a committed
copy and a generated one drift). To look at it locally:

  python3 scripts/make-brand-pages.py --out /tmp/site   (then serve /tmp/site)
"""
import argparse
import hashlib
import html
import json
import posixpath
import pathlib
import re
import shutil
import struct

ROOT = pathlib.Path(__file__).resolve().parent.parent
STAMPS = {}
BASE = "https://tigertag-project.github.io/Tiger-Touch/"
REPO = "https://github.com/TigerTag-Project/Tiger-Touch"


def esc(s):
    return html.escape(s, quote=True)


# ---- The logos ------------------------------------------------------------------

def logos(svg):
    """{label: (viewBox, inner svg)} from the brand strip, and the strip's pieces
    in order: (label, the logo's <svg>, its <text>)."""
    pieces = []
    for m in re.finditer(r'<svg x="[^"]+" y="[^"]+" width="[^"]+" height="[^"]+" viewBox="([^"]+)"[^>]*>', svg):
        start, depth, end = m.start(), 0, None
        for t in re.finditer(r"<svg\b|</svg>", svg[start:]):
            depth += -1 if t.group() == "</svg>" else 1
            if depth == 0:
                end = start + t.end()
                break
        text = re.search(r"<text[^>]*>([^<]+)</text>", svg[end:end + 400])
        pieces.append((text.group(1), m.group(1), svg[start:end], svg[end + text.start():end + text.end()], start,
                       end + text.end()))
    return pieces


def strip(svg, pieces, slugs, prefix, current):
    """The brand strip as HTML: each logo and its name one link to the brand's
    page, the current one marked. HTML text, not SVG: Chrome keeps its own dark
    colour on a visited link's SVG fill, whatever the stylesheet says."""
    items = []
    for label, vb, logo, _text, _s, _e in pieces:
        slug = slugs.get(label)
        inner = re.sub(r"^<svg[^>]*>", "", logo)[: -len("</svg>")]
        # Sized here too: a page served before its stylesheet still draws small logos.
        icon = '<svg viewBox="%s" width="48" height="48" fill="currentColor" aria-hidden="true">%s</svg>' % (vb, inner)
        cur = ' class="cur" aria-current="page"' if slug == current else ""
        items.append('<a href="%s%s/"%s>%s<span>%s</span></a>' % (prefix, slug, cur, icon, esc(label)))
    return ('<nav class="brands strip" data-i18n-aria="brands_aria" aria-label="Printer brands">%s</nav>'
            % "".join(items))


def english(i18n_js):
    """The English dictionary of installer/i18n.js: {key: text}."""
    start = i18n_js.index("\nen: {")
    end = i18n_js.index("\n},", start)
    return {m.group(1): json.loads(m.group(2))
            for m in re.finditer(r'^  (\w+): (".*"),?$', i18n_js[start:end], re.M)}


def static_english(page_html, en):
    """The installer page's own text, set from the English dictionary.

    The page translates itself in the browser; what a search engine, a reader
    without JavaScript or an AI assistant reads is the HTML as written. That
    copy drifted once (step 3 still said to leave Erase device unticked), so it
    is rewritten from the dictionary every time the site is built."""
    def put(m):
        opening, tag, as_html, key, closing = m.group(1), m.group(2), m.group(3), m.group(4), m.group(6)
        if key not in en:
            return m.group(0)
        text = en[key] if as_html else html.escape(en[key], quote=False)
        return opening + text + closing
    return re.sub(r'(<(\w+)\b[^>]*?\sdata-i18n(-html)?="(\w+)"[^>]*>)(.*?)(</\2>)', put, page_html, flags=re.S)


def replace_strip(page_html, new):
    """The installer page's strip (an inline SVG or an earlier HTML strip), swapped for the generated one."""
    i = page_html.index('class="brands strip"')
    start = page_html.rindex("<", 0, i)
    tag = re.match(r"<(\w+)", page_html[start:]).group(1)
    depth = 0
    for t in re.finditer(r"<%s\b|</%s>" % (tag, tag), page_html[start:]):
        depth += -1 if t.group().startswith("</") else 1
        if depth == 0:
            return page_html[:start] + new + page_html[start + t.end():]
    raise SystemExit("the installer page's brand strip is not closed")


def favicon(viewbox, logo):
    inner = re.sub(r'^<svg[^>]*>', "", logo)[: -len("</svg>")]
    inner = inner.replace("currentColor", "#ffffff")
    return ('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">'
            '<rect width="64" height="64" rx="14" fill="#14161a"/>'
            '<svg x="10" y="10" width="44" height="44" viewBox="%s" fill="#ffffff">%s</svg></svg>\n' % (viewbox, inner))


# ---- The page ------------------------------------------------------------------

# The six features of "What Tiger Touch is": a title (<key>_t) and a line (<key>_d) each, in i18n.js.
FEATURES = ("bp_f1", "bp_f2", "bp_f3", "bp_f4", "bp_f5", "bp_f6")


def page(b, brands, pieces, svg, en, tr):
    slug, name = b["slug"], b["name"]
    url = BASE + slug + "/"
    title = en["bp_title"].replace("{name}", name)  # 60 characters at most
    desc = b["meta"]  # written for search results: 150-160 characters, a whole sentence
    img = BASE + "img/%s.png" % slug
    # With its fingerprint: a network that cached an older picture fetches the new one.
    og = BASE + "og/%s.jpg?v=%s" % (slug, hashlib.sha256((ROOT / "assets" / "og" / (slug + ".jpg")).read_bytes()).hexdigest()[:10])
    slugs = {x["logo"]: x["slug"] for x in brands}
    others = [x for x in brands if x["slug"] != slug]
    logo = next(p for p in pieces if p[0] == b["logo"])

    ld = [
        {"@context": "https://schema.org", "@type": "SoftwareApplication", "name": "Tiger Touch",
         "applicationCategory": "UtilitiesApplication",
         "operatingSystem": "Firmware for the BIGTREETECH Panda Touch and K-Touch (ESP32-S3)",
         "description": b["lead"], "url": url, "image": [og, img], "license": "https://opensource.org/licenses/MIT",
         "offers": {"@type": "Offer", "price": "0", "priceCurrency": "EUR"},
         "isAccessibleForFree": True, "downloadUrl": BASE, "codeRepository": REPO,
         "featureList": [s for s in b["shows"]]},
        {"@context": "https://schema.org", "@type": "BreadcrumbList", "itemListElement": [
            {"@type": "ListItem", "position": 1, "name": "Tiger Touch", "item": BASE},
            {"@type": "ListItem", "position": 2, "name": name, "item": url}]},
        {"@context": "https://schema.org", "@type": "FAQPage", "mainEntity": [
            {"@type": "Question", "name": q, "acceptedAnswer": {"@type": "Answer", "text": a}} for q, a in b["faq"]]},
    ]

    # Every piece of text carries its key: lang.js swaps it for the reader's
    # language, from i18n.js (the words every page shares) or the page's own
    # dictionary (this brand's words, written below as I18N_PAGE).
    li = lambda key, xs: "".join('<li data-i18n="%s_%d">%s</li>' % (key, i, esc(x)) for i, x in enumerate(xs))
    other_rows = "".join(
        '<li><a href="../%s/"><b>%s</b></a> <span data-i18n="short_%s">%s</span></li>'
        % (o["slug"], esc(o["name"]), key_of(o["slug"]), esc(o["short"])) for o in others)
    faq = "".join('<details><summary data-i18n="b_faq_%d_q">%s</summary><p data-i18n="b_faq_%d_a">%s</p></details>'
                  % (i, esc(q), i, esc(a)) for i, (q, a) in enumerate(b["faq"]))
    feats = "".join('<div><h3 data-i18n="bp_f%d_t">%s</h3><p data-i18n="bp_f%d_d">%s</p></div>'
                    % (i + 1, esc(en[k + "_t"]), i + 1, esc(en[k + "_d"]))
                    for i, k in enumerate(FEATURES))
    logo_svg = logo[2].replace("<svg ", '<svg class="bp-logo" width="92" height="92" aria-hidden="true" ', 1)
    logo_svg = re.sub(r' x="[^"]+" y="[^"]+" width="[^"]+" height="[^"]+"', "", logo_svg, count=1)
    vias = {m[2] for m in b["lineup"] if len(m) > 2}
    if len(vias) > 1:
        raise SystemExit("%s: one 'via' note per brand, found %s" % (slug, sorted(vias)))
    # English as written, with {name} filled: what the HTML holds before any script runs.
    E = lambda k: en[k].replace("{name}", name)
    T = lambda k: esc(E(k))

    return """<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{title}</title>
<meta name="description" content="{desc}">
<link rel="canonical" href="{url}">
<meta property="og:type" content="website">
<meta property="og:site_name" content="Tiger Touch">
<meta property="og:title" content="Tiger Touch for {name} 3D printers">
<meta property="og:description" content="{ogdesc}">
<meta property="og:url" content="{url}">
<meta property="og:locale" content="en_US">
<meta property="og:image" content="{og}">
<meta property="og:image:width" content="1200">
<meta property="og:image:height" content="630">
<meta property="og:image:alt" content="TigerTouch for {name} 3D printers: the screen showing a {name} printer, and its lineup">
<meta name="twitter:card" content="summary_large_image">
<meta name="twitter:title" content="Tiger Touch for {name} 3D printers">
<meta name="twitter:description" content="{ogdesc}">
<meta name="twitter:image" content="{og}">
<meta name="twitter:image:alt" content="TigerTouch for {name} 3D printers: the screen showing a {name} printer, and its lineup">
<link rel="icon" href="favicon.svg" type="image/svg+xml">
<link rel="stylesheet" href="../site.css?v={v_css}">
<script>try {{ var t = localStorage.getItem("tigertouch-theme"); if (t === "light" || t === "dark") document.documentElement.dataset.theme = t; }} catch (e) {{}}</script>
<script src="../i18n.js?v={v_i18n}"></script>
<script>window.I18N_PAGE = {page_dict};</script>
<script src="../theme.js?v={v_theme}" defer></script>
<script src="../lang.js?v={v_lang}" defer></script>
<script src="../release.js?v={v_release}" defer></script>
<script type="application/ld+json">{ld}</script>
</head>
<body data-brand="{name}" data-title="bp_title">
<header class="bar">
  <div class="wrap">
    <a class="logo duo" href="../"><b>{name}</b> <span class="x">x</span> Tiger<span>System</span></a>
    <div class="tools">
      <a class="pill solid" href="../#install" data-i18n="cta_btn">{t_install}</a>
      <button class="themebtn" id="themebtn" type="button" data-i18n-aria="theme_label" aria-label="{t_theme}">
        <svg class="moon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M21 12.8A9 9 0 1 1 11.2 3a7 7 0 0 0 9.8 9.8z"/></svg>
        <svg class="sun" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><circle cx="12" cy="12" r="4"/><path d="M12 2v2M12 20v2M4.9 4.9l1.4 1.4M17.7 17.7l1.4 1.4M2 12h2M20 12h2M4.9 19.1l1.4-1.4M17.7 6.3l1.4-1.4"/></svg>
      </button>
      <div class="langbox" id="langbox" data-open="0">
        <button class="langbtn" id="langbtn" type="button" aria-haspopup="listbox"
                aria-expanded="false" data-i18n-aria="lang_label" aria-label="{t_lang}">
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"
               stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">
            <circle cx="12" cy="12" r="9"/><path d="M3 12h18M12 3c3 3.2 3 14.8 0 18M12 3c-3 3.2-3 14.8 0 18"/>
          </svg>
          <span id="langnow">English</span>
          <svg class="caret" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5"
               stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M6 9l6 6 6-6"/></svg>
        </button>
        <ul class="langmenu" id="langmenu" role="listbox"></ul>
      </div>
    </div>
  </div>
</header>
<main class="wrap">
  <nav class="crumbs" data-i18n-aria="crumbs_aria" aria-label="{t_crumbs}"><a href="../">Tiger Touch</a> <span aria-hidden="true">/</span> <span>{name}</span></nav>

  <section class="hero bp-hero">
    <div class="intro">
      <div class="bp-head">
        {logo_svg}
        <h1 class="bp-title" data-i18n-html="bp_h1_html">{h1}</h1>
      </div>
      <p class="lead" data-i18n="b_lead">{lead}</p>
      <dl class="facts">
        <dt data-i18n="bp_models">{t_models}</dt><dd data-i18n="b_models">{models}</dd>
        <dt data-i18n="bp_connection">{t_connection}</dt><dd data-i18n="b_connection">{connection}</dd>
        <dt data-i18n="bp_tested">{t_tested}</dt><dd data-i18n="b_status">{status}</dd>
      </dl>
      {note}
      <p class="cta"><a class="pill solid big" href="../#install" data-i18n="bp_cta">{t_cta}</a>
        <a class="pill" href="{repo}" data-i18n="bp_source">{t_source}</a></p>
      <p class="latest" data-latest hidden></p>
      <p class="small-note"><span data-i18n="restore_txt">{t_restore}</span> <a href="{repo}/blob/main/docs/INSTALL.md" data-i18n="restore_link">{t_restore_link}</a></p>
    </div>
    <figure class="show bp-shot">
      <img src="../img/{slug}.png" width="607" height="738" data-i18n-alt="b_alt" alt="{shot_alt}">
      <figcaption data-i18n="bp_caption">{t_caption}</figcaption>
    </figure>
  </section>

  <!-- For people not installing today: the community, as on the installer page. -->
  <section class="join">
    <div>
      <h2 data-i18n="join_title">{t_join}</h2>
      <p data-i18n="join_text">{t_join_text}</p>
    </div>
    <a class="joinbtn" href="https://tigersystem.io" target="_blank" rel="noopener" data-i18n="join_btn">{t_join_btn}</a>
  </section>

  <section class="bp-sec lineup-sec" data-i18n-aria="bp_lineup_aria" aria-label="{t_lineup}">
    <ul class="lineup" style="--per: {per}">{lineup}</ul>
  </section>

  <section class="card bp-sec need">
    <h2 data-i18n="bp_need_title">{t_need}</h2>
    <ul class="ticks">
      <li data-i18n-html="bp_need1_html">{t_need1}</li>
      <li data-i18n-html="bp_need2_html">{t_need2}</li>
      <li data-i18n-html="bp_need3_html">{t_need3}</li>
      <li data-i18n-html="bp_need4_html">{t_need4}</li>
    </ul>
  </section>

  <section class="card bp-sec">
    <h2 data-i18n="bp_shows_title">{t_shows}</h2>
    <ul class="ticks">{shows}</ul>
    <h3 data-i18n="bp_controls_title">{t_controls}</h3>
    <p data-i18n="b_controls">{controls}</p>
  </section>

  <section class="card bp-sec">
    <h2 data-i18n="bp_setup_title">{t_setup}</h2>
    <ol>{setup}</ol>
    <p class="tip"><b data-i18n="bp_tip">{t_tip}</b> <span data-i18n="b_tips">{tips}</span></p>
  </section>

  <section class="card bp-sec">
    <h2 data-i18n="bp_faq_title">{t_faq}</h2>
    <div class="faq">{faq}</div>
  </section>

  <section class="bp-sec">
    <h2 data-i18n="bp_every_title">{t_every}</h2>
    <p data-i18n="bp_every_text">{t_every_text}</p>
    {strip}
    <ul class="others">{others}</ul>
  </section>

  <section class="card bp-sec">
    <h2 data-i18n="bp_what_title">{t_what}</h2>
    <p data-i18n="bp_what_text">{t_what_text}</p>
    <h3 data-i18n="phil_title">{t_phil}</h3>
    <p><span data-i18n-html="phil_html">{t_phil_html}</span> <span data-i18n="bp_phil_more">{t_phil_more}</span></p>
    <div class="features">{feats}</div>
    <p class="cta"><a class="pill solid big" href="../#install" data-i18n="bp_cta2">{t_cta2}</a></p>
  </section>

  {eco}
  <footer>
  <nav class="foot">
    <a class="pill" href="../" data-i18n="bp_foot_installer">{t_foot}</a>
    <a class="pill" href="{repo}" target="_blank" rel="noopener" data-i18n="f_source">{t_f_source}</a>
    <a class="pill" href="https://wiki.tigersystem.io" target="_blank" rel="noopener" data-i18n="f_wiki">{t_f_wiki}</a>
    <a class="pill" href="{repo}/releases" target="_blank" rel="noopener" data-i18n="f_releases">{t_f_releases}</a>
    <a class="pill" href="https://tigersystem.io" target="_blank" rel="noopener">tigersystem.io</a>
  </nav>
  <p class="legal"><span data-i18n="legal">{t_legal}</span> <a href="{repo}/blob/main/TRADEMARK.md">TRADEMARK.md</a></p>
  </footer>
</main>
</body>
</html>
""".format(title=esc(title), desc=esc(desc), ogdesc=esc(b["og"]), url=url, name=esc(name), img=img, og=og, slug=slug, repo=REPO,
           ld=json.dumps(ld, ensure_ascii=False).replace("</", "<\\/"), logo_svg=logo_svg, lead=esc(b["lead"]),
           models=esc(b["models"]), connection=esc(b["connection"]), status=esc(b["status"]),
           shot_alt=esc(b["shot_alt"]), shows=li("b_shows", b["shows"]), controls=esc(b["controls"]),
           setup=li("b_setup", b["setup"]), tips=esc(b["tips"]), faq=faq, strip=strip(svg, pieces, slugs, "../", slug),
           others=other_rows, feats=feats, page_dict=page_dict(b, brands, tr),
           h1=E("bp_h1_html"),
           t_install=T("cta_btn"), t_theme=T("theme_label"), t_lang=T("lang_label"), t_crumbs=T("crumbs_aria"),
           t_models=T("bp_models"), t_connection=T("bp_connection"), t_tested=T("bp_tested"), t_cta=T("bp_cta"),
           t_source=T("bp_source"), t_restore=T("restore_txt"), t_restore_link=T("restore_link"),
           t_caption=T("bp_caption"), t_lineup=T("bp_lineup_aria"), t_need=T("bp_need_title"),
           t_need1=E("bp_need1_html"), t_need2=E("bp_need2_html"), t_need3=E("bp_need3_html"), t_need4=E("bp_need4_html"),
           t_shows=T("bp_shows_title"), t_controls=T("bp_controls_title"), t_setup=T("bp_setup_title"), t_tip=T("bp_tip"),
           t_faq=T("bp_faq_title"), t_every=T("bp_every_title"), t_every_text=T("bp_every_text"), t_what=T("bp_what_title"),
           t_what_text=T("bp_what_text"), t_phil=T("phil_title"), t_phil_html=E("phil_html"), t_phil_more=T("bp_phil_more"),
           t_cta2=T("bp_cta2"), t_join=T("join_title"), t_join_text=T("join_text"), t_join_btn=T("join_btn"),
           t_foot=T("bp_foot_installer"), t_f_source=T("f_source"), t_f_wiki=T("f_wiki"), t_f_releases=T("f_releases"),
           t_legal=T("legal"), eco=ecosystem("../", en),
           # Printers on a row: all of them up to seven, else half (rounded up) -
           # one row, or two, never three (site.css shrinks them to fit).
           per=len(b["lineup"]) if len(b["lineup"]) <= 7 else (len(b["lineup"]) + 1) // 2,
           note=('<p class="bp-note" data-i18n="b_note">%s</p>' % esc(b["note"])) if b.get("note") else "", **STAMPS,
           lineup="".join('<li><img src="../img/models/%s" width="240" height="240" loading="lazy" alt="%s %s">'
                          '<span>%s</span>%s</li>' % (esc(m[0]), esc(name), esc(m[1]), esc(m[1]),
                                                      '<small data-i18n="b_via">%s</small>' % esc(m[2]) if len(m) > 2 else "")
                          for m in b["lineup"]))


# ---- The brands' words in every language ---------------------------------------

# The fields of brand-pages.json a reader sees, translated in installer/brand-i18n/<lang>.json.
FIELDS = ["short", "lead", "models", "connection", "status", "shows", "controls", "setup", "tips", "shot_alt", "faq",
          "note", "via"]


def key_of(slug):
    return slug.replace("-", "_")


def english_fields(b):
    e = {k: b[k] for k in FIELDS if k in b}
    vias = [m[2] for m in b["lineup"] if len(m) > 2]
    if vias:
        e["via"] = vias[0]
    return e


def flat(f):
    """One brand's fields as the page's keys: b_lead, b_shows_0, b_faq_1_q..."""
    d = {}
    for k, v in f.items():
        if k == "short":
            continue
        k = {"shot_alt": "alt"}.get(k, k)
        if k == "faq":
            for i, (q, a) in enumerate(v):
                d["b_faq_%d_q" % i], d["b_faq_%d_a" % i] = q, a
        elif isinstance(v, list):
            for i, x in enumerate(v):
                d["b_%s_%d" % (k, i)] = x
        else:
            d["b_" + k] = v
    return d


def page_dict(b, brands, tr):
    """{lang: {key: text}} for one page: its brand's words, and the one-line
    summary of every other brand. English included, to switch back to it."""
    out = {}
    for code, by_slug in tr.items():
        d = flat(by_slug[b["slug"]])
        for o in brands:
            if o["slug"] != b["slug"]:
                d["short_" + key_of(o["slug"])] = by_slug[o["slug"]]["short"]
        out[code] = d
    return json.dumps(out, ensure_ascii=False, separators=(",", ":")).replace("</", "<\\/")


def translations(brands):
    """{lang: {slug: fields}}, English from brand-pages.json, the rest from brand-i18n/."""
    tr = {"en": {b["slug"]: english_fields(b) for b in brands}}
    for f in sorted((ROOT / "installer" / "brand-i18n").glob("*.json")):
        tr[f.stem] = json.loads(f.read_text())
    return tr

# ---- The rest of TigerSystem, at the foot of every page ---------------------------

# (picture in assets/ecosystem/, name, address, i18n.js key of its line)
ECOSYSTEM = [
    ("studio-and-connect.png", "Tiger Studio Manager", "https://github.com/TigerTag-Project/TigerTag-Studio-Manager", "eco_studio"),
    ("tigerpod-mini.png", "TigerPOD Mini", "https://github.com/TigerTag-Project/TigerPOD", "eco_pod"),
    ("tigerscale-v3.png", "TigerScale V3", "https://github.com/TigerTag-Project/Tiger-Scale-V3", "eco_scale"),
    ("tigerspool.png", "TigerSpool", "https://github.com/TigerTag-Project/TigerSpool-RFID", "eco_spool"),
]


def png_size(path):
    w, h = struct.unpack(">II", path.read_bytes()[16:24])
    return w, h


def ecosystem(prefix, en):
    """The other TigerSystem tools, one card each: secondary to the page, there to be discovered."""
    cards = []
    for pic, name, url, key in ECOSYSTEM:
        w, h = png_size(ROOT / "assets" / "ecosystem" / pic)
        cards.append('<a class="eco-item" href="%s" target="_blank" rel="noopener">'
                     '<img src="%seco/%s" width="%d" height="%d" loading="lazy" alt="">'
                     '<h3>%s</h3><p data-i18n="%s">%s</p></a>' % (url, prefix, pic, w, h, esc(name), key, esc(en[key])))
    return ('<section class="eco-sec" aria-labelledby="eco-title">\n'
            '    <h2 id="eco-title" data-i18n="eco_title">%s</h2>\n'
            '    <p data-i18n="eco_sub">%s</p>\n'
            '    <div class="eco-grid">%s</div>\n'
            '  </section>\n' % (esc(en["eco_title"]), esc(en["eco_sub"]), "".join(cards)))


# ---- One address per language --------------------------------------------------

# The site's languages: (code, folder, hreflang, og:locale). English is the root;
# every other language is a copy of every page under its folder (/fr/, /fr/elegoo/),
# its text written in, so a search engine indexes each language at its own address.
LANGS = [("en", "", "en", "en_US"), ("fr", "fr/", "fr", "fr_FR"), ("de", "de/", "de", "de_DE"),
         ("es", "es/", "es", "es_ES"), ("it", "it/", "it", "it_IT"), ("zh", "zh/", "zh-Hans", "zh_CN"),
         ("pt", "pt/", "pt-BR", "pt_BR"), ("pt-pt", "pt-pt/", "pt-PT", "pt_PT"), ("pl", "pl/", "pl", "pl_PL"),
         ("ru", "ru/", "ru", "ru_RU"), ("nl", "nl/", "nl", "nl_NL")]


def dictionaries(i18n_js):
    """Every language of installer/i18n.js: {code: {key: text}}."""
    out = {}
    for m in re.finditer(r'^(?:(\w+)|"([\w-]+)"): \{$(.*?)^\},', i18n_js, re.M | re.S):
        out[m.group(1) or m.group(2)] = {k.group(1): json.loads(k.group(2))
                                         for k in re.finditer(r'^  (\w+): (".*"),?$', m.group(3), re.M)}
    return out


def alternates(path):
    """The <link> tags naming every language's address of the page at `path` ("" or "elegoo/")."""
    return "".join('<link rel="alternate" hreflang="%s" href="%s%s%s">\n' % (h, BASE, folder, path)
                   for _c, folder, h, _l in LANGS) + '<link rel="alternate" hreflang="x-default" href="%s%s">\n' % (BASE, path)


def sentence(text, limit=158):
    """A description for search results: the text, cut at a word near `limit`."""
    text = re.sub(r"<[^>]+>", "", text)
    if len(text) <= limit:
        return text
    return text[:limit].rsplit(" ", 1)[0].rstrip(",;:") + "..."


def localize(page_html, words, brand):
    """The page with its text in one language: every data-i18n element filled
    from `words` ({name} the page's brand), as lang.js would in the browser."""
    w = lambda k: words[k].replace("{name}", brand) if k in words else None

    def body(m):
        opening, tag, as_html, key, closing = m.group(1), m.group(2), m.group(3), m.group(4), m.group(6)
        v = w(key)
        if v is None:
            return m.group(0)
        return opening + (v if as_html else html.escape(v, quote=False)) + closing
    page_html = re.sub(r'(<(\w+)\b[^>]*?\sdata-i18n(-html)?="(\w+)"[^>]*>)(.*?)(</\2>)', body, page_html, flags=re.S)

    def attr(m):
        tag = m.group(0)
        for kind, name in (("aria", "aria-label"), ("alt", "alt")):
            k = re.search(r'data-i18n-%s="(\w+)"' % kind, tag)
            if k and w(k.group(1)) is not None:
                tag = re.sub(r'(\s%s=)"[^"]*"' % name, lambda a: '%s"%s"' % (a.group(1), esc(w(k.group(1)))), tag)
        return tag
    return re.sub(r'<[^>]*\sdata-i18n-(?:aria|alt)="\w+"[^>]*>', attr, page_html)


def relocate(page_html, path, folder):
    """A page at `path` moved under `folder`: links to files (a dot in their last
    segment) still reach the one copy at the root; links to pages stay within
    the language, as they are relative."""
    up = "../" * (folder.count("/") + path.count("/"))

    def fix(m):
        url = m.group(2)
        if re.match(r"[a-z]+:|/|#", url) or "." not in url.split("?")[0].rsplit("/", 1)[-1]:
            return m.group(0)
        target = posixpath.normpath(posixpath.join(path, url.split("?")[0]))
        return '%s="%s%s%s"' % (m.group(1), up, target, ("?" + url.split("?", 1)[1]) if "?" in url else "")
    return re.sub(r'\b(src|href|manifest)="([^"]*)"', fix, page_html)


def translate(page_html, path, code, folder, hreflang, locale, words):
    """The English page at `path`, as a page of its own in one language."""
    brand = re.search(r'<body[^>]*\sdata-brand="([^"]*)"', page_html)
    brand = html.unescape(brand.group(1)) if brand else ""
    page_dict = re.search(r"window\.I18N_PAGE = (\{.*?\});</script>", page_html)
    if page_dict:
        words = dict(words, **json.loads(page_dict.group(1).replace("<\\/", "</")).get(code, {}))
    out = localize(page_html, words, brand)
    title_key = re.search(r'<body[^>]*\sdata-title="(\w+)"', page_html)
    title = words[title_key.group(1) if title_key else "title"].replace("{name}", brand)
    desc = sentence(words["b_lead"] if "b_lead" in words else words["tagline"])
    url = BASE + folder + path
    out = out.replace('<html lang="en">', '<html lang="%s" data-fixed-lang="%s">' % (hreflang, code), 1)
    out = re.sub(r"<title>.*?</title>", "<title>%s</title>" % esc(title), out, count=1)
    for prop, value in (("og:title", title), ("twitter:title", title)):
        out = re.sub(r'(<meta (?:property|name)="%s" content=)"[^"]*"' % prop, lambda m: '%s"%s"' % (m.group(1), esc(value)), out)
    for prop in ('name="description"', 'property="og:description"', 'name="twitter:description"'):
        out = re.sub(r'(<meta %s content=)"[^"]*"' % prop, lambda m: '%s"%s"' % (m.group(1), esc(desc)), out)
    out = re.sub(r'(<link rel="canonical" href=)"[^"]*"', '\\1"%s"' % url, out)
    out = re.sub(r'(<meta property="og:url" content=)"[^"]*"', '\\1"%s"' % url, out)
    out = re.sub(r'(<meta property="og:locale" content=)"[^"]*"', '\\1"%s"' % locale, out)
    # The structured data in the page's language: its address, its description, its questions.
    def data(m):
        ld = json.loads(m.group(1).replace("<\\/", "</"))
        for item in ld if isinstance(ld, list) else [ld]:
            kind = item.get("@type")
            if kind == "SoftwareApplication":
                item["url"], item["description"], item["inLanguage"] = url, desc, hreflang
            elif kind == "BreadcrumbList":
                for i, li in enumerate(item["itemListElement"]):
                    li["item"] = BASE + folder + ("" if i == 0 else path)
            elif kind == "FAQPage":
                for i, q in enumerate(item["mainEntity"]):
                    q["name"] = words.get("b_faq_%d_q" % i, q["name"])
                    q["acceptedAnswer"]["text"] = words.get("b_faq_%d_a" % i, q["acceptedAnswer"]["text"])
        return '<script type="application/ld+json">%s</script>' % json.dumps(ld, ensure_ascii=False).replace("</", "<\\/")
    out = re.sub(r'<script type="application/ld\+json">(.*?)</script>', data, out, flags=re.S)
    out = out.replace('<span id="langnow">English</span>', '<span id="langnow">%s</span>' % esc(words["lang_name"]))
    return relocate(out, path, folder)


def languages(out, paths, i18n_js):
    """Every page in every language, each naming the others (hreflang), and the sitemap."""
    dicts = dictionaries(i18n_js)
    for path in paths:
        src = out / path / "index.html"
        english_html = src.read_text().replace("</head>", alternates(path) + "</head>", 1)
        src.write_text(english_html)
        for code, folder, hreflang, locale in LANGS[1:]:
            words = dict(dicts["en"], **dicts[code])
            d = out / folder / path
            d.mkdir(parents=True, exist_ok=True)
            (d / "index.html").write_text(translate(english_html, path, code, folder, hreflang, locale, words))
    rows = []
    for path in paths:
        links = "".join('    <xhtml:link rel="alternate" hreflang="%s" href="%s%s%s"/>\n' % (h, BASE, f, path)
                        for _c, f, h, _l in LANGS)
        rows += ["  <url><loc>%s%s%s</loc>\n%s  </url>\n" % (BASE, f, path, links) for _c, f, _h, _l in LANGS]
    (out / "sitemap.xml").write_text(
        '<?xml version="1.0" encoding="UTF-8"?>\n<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9" '
        'xmlns:xhtml="http://www.w3.org/1999/xhtml">\n' + "".join(rows) + "</urlset>\n")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True, help="the site being built")
    a = ap.parse_args()
    out = pathlib.Path(a.out)
    # Each shared file is linked with its own fingerprint: a page never meets a
    # stylesheet older than itself in a browser's cache (Pages keeps them 10 min).
    for name in ("site.css", "theme.js", "release.js", "i18n.js", "lang.js"):
        STAMPS["v_" + name.split(".")[0].replace("site", "css")] = hashlib.sha256((out / name).read_bytes()).hexdigest()[:10]
    en = english((out / "i18n.js").read_text())
    data = json.loads((ROOT / "installer" / "brand-pages.json").read_text())
    brands = data["brands"]
    tr = translations(brands)
    svg = (ROOT / "assets" / "brands" / "brands.svg").read_text()
    pieces = logos(svg)
    labels = {p[0] for p in pieces}
    (out / "img").mkdir(parents=True, exist_ok=True)
    # The share pictures (scripts/make-og-images.py, committed).
    shutil.copytree(ROOT / "assets" / "og", out / "og", dirs_exist_ok=True)
    # The brands' logos one file each, for the installer page's comparison.
    shutil.copytree(ROOT / "assets" / "brands" / "logos", out / "logos", dirs_exist_ok=True)
    # The other TigerSystem tools' pictures, for the foot of every page.
    shutil.copytree(ROOT / "assets" / "ecosystem", out / "eco", dirs_exist_ok=True)
    for b in brands:
        if b["logo"] not in labels:
            raise SystemExit("no logo named %r in brands.svg" % b["logo"])
        d = out / b["slug"]
        d.mkdir(parents=True, exist_ok=True)
        (d / "index.html").write_text(page(b, brands, pieces, svg, en, tr))
        p = next(p for p in pieces if p[0] == b["logo"])
        (d / "favicon.svg").write_text(favicon(p[1], p[2]))
        shutil.copyfile(ROOT / "assets" / "devices" / "brands" / (b["shot"] + ".png"), out / "img" / (b["slug"] + ".png"))
        (out / "img" / "models").mkdir(exist_ok=True)
        for m in b["lineup"]:
            shutil.copyfile(ROOT / "assets" / "printers" / "models" / m[0], out / "img" / "models" / m[0])
    # The installer page shows the same strip, each logo a link to its page.
    index = out / "index.html"
    slugs = {b["logo"]: b["slug"] for b in brands}
    home = replace_strip(index.read_text(), strip(svg, pieces, slugs, "", None))
    home = home.replace("\n  <footer>", "\n  " + ecosystem("", en) + "  <footer>", 1)
    home = static_english(home, en)
    stamp = hashlib.sha256((ROOT / "assets" / "og" / "home.jpg").read_bytes()).hexdigest()[:10]
    home = re.sub(r'(og/home\.jpg)(\?v=\w+)?"', r'\1?v=%s"' % stamp, home)
    home = re.sub(r'href="site\.css(\?v=\w+)?"', 'href="site.css?v=%s"' % STAMPS["v_css"], home)
    for js in ("theme", "lang", "i18n"):
        home = re.sub(r'src="%s\.js(\?v=\w+)?"' % js, 'src="%s.js?v=%s"' % (js, STAMPS["v_" + js]), home)
    index.write_text(home)
    languages(out, [""] + [b["slug"] + "/" for b in brands], (out / "i18n.js").read_text())
    (out / "robots.txt").write_text("User-agent: *\nAllow: /\nSitemap: %ssitemap.xml\n" % BASE)
    (out / "brand-pages.json").unlink(missing_ok=True)
    shutil.rmtree(out / "brand-i18n", ignore_errors=True)
    print("brand pages: %s" % ", ".join(b["slug"] for b in brands))


if __name__ == "__main__":
    main()
