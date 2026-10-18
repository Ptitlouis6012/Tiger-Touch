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

  <slug>/index.html, <slug>/favicon.svg, img/<slug>.png, sitemap.xml, robots.txt

Called by make-manifest.py when the site is built; never committed (a committed
copy and a generated one drift). To look at it locally:

  python3 scripts/make-brand-pages.py --out /tmp/site   (then serve /tmp/site)
"""
import argparse
import hashlib
import html
import json
import pathlib
import re
import shutil

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
    return '<nav class="brands strip" aria-label="Printer brands">%s</nav>' % "".join(items)


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

FEATURES = [
    ("Every brand on one screen", "Bambu Lab, Creality, Elegoo, Anycubic, Snapmaker, FlashForge, Prusa, Klipper and OctoPrint, up to ten printers live at once."),
    ("Nothing to type", "Your printers come from your TigerSystem account, with their addresses and access codes, as Tiger Studio Manager added them."),
    ("Install from your browser", "Plug the screen in, press Install in Chrome or Edge: a few minutes, no tools, no soldering."),
    ("Updates over Wi-Fi", "The screen finds new versions by itself and asks; each one is checked before it is used."),
    ("Live view in Tiger Studio", "Tiger Studio Manager shows the screen live, touches it and takes screenshots for a bug report or a post."),
    ("Free and open source", "MIT licence, no subscription, eleven languages, made by the TigerSquad community."),
]


def page(b, brands, pieces, svg):
    slug, name = b["slug"], b["name"]
    url = BASE + slug + "/"
    title = "%s printers on Panda Touch / K-Touch | Tiger Touch" % name  # 60 characters at most
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

    li = lambda xs: "".join("<li>%s</li>" % esc(x) for x in xs)
    other_rows = "".join(
        '<li><a href="../%s/"><b>%s</b></a> <span>%s</span></li>' % (o["slug"], esc(o["name"]), esc(o["short"]))
        for o in others)
    faq = "".join("<details><summary>%s</summary><p>%s</p></details>" % (esc(q), esc(a)) for q, a in b["faq"])
    feats = "".join("<div><h3>%s</h3><p>%s</p></div>" % (esc(t), esc(d)) for t, d in FEATURES)
    logo_svg = logo[2].replace("<svg ", '<svg class="bp-logo" width="92" height="92" aria-hidden="true" ', 1)
    logo_svg = re.sub(r' x="[^"]+" y="[^"]+" width="[^"]+" height="[^"]+"', "", logo_svg, count=1)

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
<meta property="og:title" content="Tiger Touch for {name} printers">
<meta property="og:description" content="{ogdesc}">
<meta property="og:url" content="{url}">
<meta property="og:locale" content="en_US">
<meta property="og:image" content="{og}">
<meta property="og:image:width" content="1200">
<meta property="og:image:height" content="630">
<meta property="og:image:alt" content="TigerTouch for {name} printers: the screen showing a {name} printer, and its lineup">
<meta name="twitter:card" content="summary_large_image">
<meta name="twitter:title" content="Tiger Touch for {name} printers">
<meta name="twitter:description" content="{ogdesc}">
<meta name="twitter:image" content="{og}">
<meta name="twitter:image:alt" content="TigerTouch for {name} printers: the screen showing a {name} printer, and its lineup">
<link rel="icon" href="favicon.svg" type="image/svg+xml">
<link rel="stylesheet" href="../site.css?v={v_css}">
<script>try {{ var t = localStorage.getItem("tigertouch-theme"); if (t === "light" || t === "dark") document.documentElement.dataset.theme = t; }} catch (e) {{}}</script>
<script src="../theme.js?v={v_theme}" defer></script>
<script src="../release.js?v={v_release}" defer></script>
<script type="application/ld+json">{ld}</script>
</head>
<body>
<header class="bar">
  <div class="wrap">
    <a class="logo duo" href="../"><b>{name}</b> <span class="x">x</span> Tiger<span>System</span></a>
    <div class="tools">
      <a class="pill solid" href="../#install">Install</a>
      <button class="themebtn" id="themebtn" type="button" aria-label="Light or dark">
        <svg class="moon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M21 12.8A9 9 0 1 1 11.2 3a7 7 0 0 0 9.8 9.8z"/></svg>
        <svg class="sun" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><circle cx="12" cy="12" r="4"/><path d="M12 2v2M12 20v2M4.9 4.9l1.4 1.4M17.7 17.7l1.4 1.4M2 12h2M20 12h2M4.9 19.1l1.4-1.4M17.7 6.3l1.4-1.4"/></svg>
      </button>
    </div>
  </div>
</header>
<main class="wrap">
  <nav class="crumbs" aria-label="Breadcrumb"><a href="../">Tiger Touch</a> <span aria-hidden="true">/</span> <span>{name}</span></nav>

  <section class="hero bp-hero">
    <div class="intro">
      <div class="bp-head">
        {logo_svg}
        <h1 class="bp-title">Tiger<span>Touch</span> for <span>{name}</span> printers</h1>
      </div>
      <p class="lead">{lead}</p>
      <dl class="facts">
        <dt>Models</dt><dd>{models}</dd>
        <dt>Connection</dt><dd>{connection}</dd>
        <dt>Tested</dt><dd>{status}</dd>
      </dl>
      {note}
      <p class="cta"><a class="pill solid big" href="../#install">Install Tiger Touch from your browser</a>
        <a class="pill" href="{repo}">Source on GitHub</a></p>
      <p class="latest" data-latest hidden></p>
      <p class="small-note">This guide is in English; the installer speaks eleven languages. The original firmware can be reinstalled at any time: <a href="{repo}/blob/main/docs/INSTALL.md">how to go back</a>.</p>
    </div>
    <figure class="show bp-shot">
      <img src="../img/{slug}.png" width="607" height="738" alt="{shot_alt}">
      <figcaption>Captured in Demo mode on a Panda Touch running Tiger Touch: the screen is real, the printer data is made up.</figcaption>
    </figure>
  </section>

  <section class="bp-sec lineup-sec" aria-label="{name} printers">
    <ul class="lineup">{lineup}</ul>
  </section>

  <section class="card bp-sec need">
    <h2>What you need</h2>
    <ul class="ticks">
      <li>A BIGTREETECH <b>Panda Touch</b> or <b>K-Touch</b>, the 5-inch touchscreen, sold separately: maybe the one already in your drawer. Tiger Touch has been run on a Panda Touch; the K-Touch is the same board and has not been run on one yet.</li>
      <li>For the first installation only: <b>Chrome or Edge</b> on a computer and a <b>USB-C data cable</b>. Nothing stays plugged in afterwards.</li>
      <li>Your printer on the same <b>Wi-Fi</b> network as the screen (or its maker's cloud, where the page above says so).</li>
      <li>A free <b>TigerSystem account</b>, with the printer added in <b>Tiger Studio Manager</b>, the desktop app (Windows, macOS, Linux).</li>
    </ul>
  </section>

  <section class="card bp-sec">
    <h2>What the screen shows for a {name} printer</h2>
    <ul class="ticks">{shows}</ul>
    <h3>Pause, stop and the light</h3>
    <p>{controls}</p>
  </section>

  <section class="card bp-sec">
    <h2>Set it up in three steps</h2>
    <ol>{setup}</ol>
    <p class="tip"><b>Tip.</b> {tips}</p>
  </section>

  <section class="card bp-sec">
    <h2>{name} questions</h2>
    <div class="faq">{faq}</div>
  </section>

  <section class="bp-sec">
    <h2>The same screen, every brand</h2>
    <p>Tiger Touch follows every printer of your TigerSystem account on one screen, whatever the brand, each through its maker's own connection:</p>
    {strip}
    <ul class="others">{others}</ul>
  </section>

  <section class="card bp-sec">
    <h2>What Tiger<span class="tt-touch">Touch</span> is</h2>
    <p>A free, open-source firmware that turns the BIGTREETECH Panda Touch or K-Touch, a 5-inch touchscreen made for Bambu Lab, into one live screen for all your 3D printers. No computer left on and no bridge: it signs in to your TigerSystem account, reads your printers from it and talks to each one directly.</p>
    <h3>Built around makers, not around a brand</h3>
    <p>Printer makers build their software around their own machines: their apps, their clouds and their screens are designed to keep you with their printers. <b>TigerSystem starts from the other end: you.</b> Whatever printers you own, whatever their brand, whatever computer or phone you use, our tools adapt to the way you work, not the other way round. A {name} printer today, another brand tomorrow: the same screen follows both.</p>
    <div class="features">{feats}</div>
    <p class="cta"><a class="pill solid big" href="../#install">Install it now, in a few minutes</a></p>
  </section>

  <section class="join">
    <div>
      <h2>You dreamed of it. The TigerSquad built it.</h2>
      <p>Tiger Touch, Tiger Studio, TigerScale, TigerSpool, TigerPOD: free tools made by a community of makers, for every brand of printer.</p>
    </div>
    <a class="joinbtn" href="https://tigersystem.io" target="_blank" rel="noopener">Join us now</a>
  </section>

  <footer>
  <nav class="foot">
    <a class="pill" href="../">Tiger Touch installer</a>
    <a class="pill" href="{repo}" target="_blank" rel="noopener">Source code on GitHub</a>
    <a class="pill" href="https://wiki.tigersystem.io" target="_blank" rel="noopener">Documentation (wiki)</a>
    <a class="pill" href="{repo}/releases" target="_blank" rel="noopener">What's new</a>
    <a class="pill" href="https://tigersystem.io" target="_blank" rel="noopener">tigersystem.io</a>
  </nav>
  <p class="legal">Tiger Touch is an independent open-source project, not affiliated with or endorsed by BIGTREETECH or {name}. Product names, logos and pictures belong to their owners and appear only to say which machines Tiger Touch works with (<a href="{repo}/blob/main/TRADEMARK.md">trademarks</a>).</p>
  </footer>
</main>
</body>
</html>
""".format(title=esc(title), desc=esc(desc), ogdesc=esc(b["og"]), url=url, name=esc(name), img=img, og=og, slug=slug, repo=REPO,
           ld=json.dumps(ld, ensure_ascii=False).replace("</", "<\\/"), logo_svg=logo_svg, lead=esc(b["lead"]),
           models=esc(b["models"]), connection=esc(b["connection"]), status=esc(b["status"]),
           shot_alt=esc(b["shot_alt"]), shows=li(b["shows"]), controls=esc(b["controls"]),
           setup=li(b["setup"]), tips=esc(b["tips"]), faq=faq, strip=strip(svg, pieces, slugs, "../", slug),
           others=other_rows, feats=feats,
           note=('<p class="bp-note">%s</p>' % esc(b["note"])) if b.get("note") else "", **STAMPS,
           lineup="".join('<li><img src="../img/models/%s" width="240" height="240" loading="lazy" alt="%s %s">'
                          '<span>%s</span>%s</li>' % (esc(m[0]), esc(name), esc(m[1]), esc(m[1]),
                                                      '<small>%s</small>' % esc(m[2]) if len(m) > 2 else "")
                          for m in b["lineup"]))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True, help="the site being built")
    a = ap.parse_args()
    out = pathlib.Path(a.out)
    # Each shared file is linked with its own fingerprint: a page never meets a
    # stylesheet older than itself in a browser's cache (Pages keeps them 10 min).
    for name in ("site.css", "theme.js", "release.js"):
        STAMPS["v_" + name.split(".")[0].replace("site", "css")] = hashlib.sha256((out / name).read_bytes()).hexdigest()[:10]
    data = json.loads((ROOT / "installer" / "brand-pages.json").read_text())
    brands = data["brands"]
    svg = (ROOT / "assets" / "brands" / "brands.svg").read_text()
    pieces = logos(svg)
    labels = {p[0] for p in pieces}
    (out / "img").mkdir(parents=True, exist_ok=True)
    # The share pictures (scripts/make-og-images.py, committed).
    shutil.copytree(ROOT / "assets" / "og", out / "og", dirs_exist_ok=True)
    # The brands' logos one file each, for the installer page's comparison.
    shutil.copytree(ROOT / "assets" / "brands" / "logos", out / "logos", dirs_exist_ok=True)
    for b in brands:
        if b["logo"] not in labels:
            raise SystemExit("no logo named %r in brands.svg" % b["logo"])
        d = out / b["slug"]
        d.mkdir(parents=True, exist_ok=True)
        (d / "index.html").write_text(page(b, brands, pieces, svg))
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
    home = static_english(home, english((out / "i18n.js").read_text()))
    stamp = hashlib.sha256((ROOT / "assets" / "og" / "home.jpg").read_bytes()).hexdigest()[:10]
    home = re.sub(r'(og/home\.jpg)(\?v=\w+)?"', r'\1?v=%s"' % stamp, home)
    home = re.sub(r'href="site\.css(\?v=\w+)?"', 'href="site.css?v=%s"' % STAMPS["v_css"], home)
    home = re.sub(r'src="theme\.js(\?v=\w+)?"', 'src="theme.js?v=%s"' % STAMPS["v_theme"], home)
    index.write_text(home)
    urls = [BASE] + [BASE + b["slug"] + "/" for b in brands]
    (out / "sitemap.xml").write_text(
        '<?xml version="1.0" encoding="UTF-8"?>\n<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">\n'
        + "".join("  <url><loc>%s</loc></url>\n" % u for u in urls) + "</urlset>\n")
    (out / "robots.txt").write_text("User-agent: *\nAllow: /\nSitemap: %ssitemap.xml\n" % BASE)
    (out / "brand-pages.json").unlink(missing_ok=True)
    print("brand pages: %s" % ", ".join(b["slug"] for b in brands))


if __name__ == "__main__":
    main()
