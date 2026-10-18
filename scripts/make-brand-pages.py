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

and written into the site (`out`), beside the installer page:

  <slug>/index.html, <slug>/favicon.svg, img/<slug>.png, sitemap.xml, robots.txt

Called by make-manifest.py when the site is built; never committed (a committed
copy and a generated one drift). To look at it locally:

  python3 scripts/make-brand-pages.py --out /tmp/site   (then serve /tmp/site)
"""
import argparse
import html
import json
import pathlib
import re
import shutil

ROOT = pathlib.Path(__file__).resolve().parent.parent
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
    """The brand strip with each logo a link to its page; the current one marked."""
    out, last = [], 0
    for label, _vb, logo, text, start, end in pieces:
        out.append(svg[last:start])
        slug = slugs.get(label)
        cls = ' class="cur" aria-current="page"' if slug == current else ""
        if slug:
            out.append('<a href="%s%s/"%s>%s%s</a>' % (prefix, slug, cls, logo, svg[start + len(logo):end]))
        else:
            out.append(svg[start:end])
        last = end
    out.append(svg[last:])
    s = "".join(out)
    # Inline in the page: sized by the stylesheet, drawn in the text's colour.
    root = ' width="1080" height="110" color="#8b93a1" fill="#8b93a1"'
    if root not in s:
        raise SystemExit("brands.svg's root changed: update strip()")
    names = ", ".join(p[0] for p in pieces)
    return s.replace(root, ' fill="currentColor" class="brands strip" role="img" aria-label="%s"' % names, 1)


def replace_strip(page_html, new):
    """The installer page's strip, swapped for the generated one (with links)."""
    start = page_html.index('class="brands strip"')
    start = page_html.rindex("<svg", 0, start)
    depth = 0
    for t in re.finditer(r"<svg\b|</svg>", page_html[start:]):
        depth += -1 if t.group() == "</svg>" else 1
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
    ("Install from your browser", "Plug the screen in, press Install in Chrome or Edge: two minutes, no tools, no soldering."),
    ("Updates over Wi-Fi", "The screen finds new versions by itself and asks; each one is checked before it is used."),
    ("Live view in Tiger Studio", "Tiger Studio Manager shows the screen live, touches it and takes screenshots for a bug report or a post."),
    ("Free and open source", "MIT licence, no subscription, eleven languages, made by the TigerSquad community."),
]


def page(b, brands, pieces, svg):
    slug, name = b["slug"], b["name"]
    url = BASE + slug + "/"
    title = "%s printers on a Panda Touch or K-Touch - Tiger Touch" % name
    desc = b["lead"]
    if len(desc) > 158:
        desc = desc[:155].rsplit(" ", 1)[0] + "..."
    img = BASE + "img/%s.png" % slug
    slugs = {x["logo"]: x["slug"] for x in brands}
    others = [x for x in brands if x["slug"] != slug]
    logo = next(p for p in pieces if p[0] == b["logo"])

    ld = [
        {"@context": "https://schema.org", "@type": "SoftwareApplication", "name": "Tiger Touch",
         "applicationCategory": "UtilitiesApplication",
         "operatingSystem": "Firmware for the BIGTREETECH Panda Touch and K-Touch (ESP32-S3)",
         "description": b["lead"], "url": url, "image": img, "license": "https://opensource.org/licenses/MIT",
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
    logo_svg = logo[2].replace("<svg ", '<svg class="bp-logo" aria-hidden="true" ', 1)
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
<meta property="og:title" content="Tiger Touch for {name}">
<meta property="og:description" content="{desc}">
<meta property="og:url" content="{url}">
<meta property="og:image" content="{img}">
<meta name="twitter:card" content="summary_large_image">
<link rel="icon" href="favicon.svg" type="image/svg+xml">
<link rel="stylesheet" href="../site.css">
<script>try {{ var t = localStorage.getItem("tigertouch-theme"); if (t === "light" || t === "dark") document.documentElement.dataset.theme = t; }} catch (e) {{}}</script>
<script src="../theme.js" defer></script>
<script type="application/ld+json">{ld}</script>
</head>
<body>
<header class="bar">
  <div class="wrap">
    <a class="logo" href="../">Tiger<span>Touch</span></a>
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
      {logo_svg}
      <h1 class="bp-title">Tiger Touch for <span>{name}</span> printers</h1>
      <p class="lead">{lead}</p>
      <dl class="facts">
        <dt>Models</dt><dd>{models}</dd>
        <dt>Connection</dt><dd>{connection}</dd>
        <dt>Tested</dt><dd>{status}</dd>
      </dl>
      <p class="cta"><a class="pill solid big" href="../#install">Install Tiger Touch from your browser</a>
        <a class="pill" href="{repo}">Source on GitHub</a></p>
    </div>
    <figure class="show bp-shot">
      <img src="../img/{slug}.png" width="607" height="738" alt="{shot_alt}">
      <figcaption>Captured on a Panda Touch running Tiger Touch.</figcaption>
    </figure>
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
    <h2>What Tiger Touch is</h2>
    <p>A free, open-source firmware that turns the BIGTREETECH Panda Touch or K-Touch, a 5-inch touchscreen made for Bambu Lab, into one live screen for all your 3D printers. No computer and no bridge: it signs in to your TigerSystem account, reads your printers from it and talks to each one directly.</p>
    <div class="features">{feats}</div>
    <p class="cta"><a class="pill solid big" href="../#install">Install it now, in two minutes</a></p>
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
  </footer>
</main>
</body>
</html>
""".format(title=esc(title), desc=esc(desc), url=url, name=esc(name), img=img, slug=slug, repo=REPO,
           ld=json.dumps(ld, ensure_ascii=False).replace("</", "<\\/"), logo_svg=logo_svg, lead=esc(b["lead"]),
           models=esc(b["models"]), connection=esc(b["connection"]), status=esc(b["status"]),
           shot_alt=esc(b["shot_alt"]), shows=li(b["shows"]), controls=esc(b["controls"]),
           setup=li(b["setup"]), tips=esc(b["tips"]), faq=faq, strip=strip(svg, pieces, slugs, "../", slug),
           others=other_rows, feats=feats)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True, help="the site being built")
    a = ap.parse_args()
    out = pathlib.Path(a.out)
    data = json.loads((ROOT / "installer" / "brand-pages.json").read_text())
    brands = data["brands"]
    svg = (ROOT / "assets" / "brands" / "brands.svg").read_text()
    pieces = logos(svg)
    labels = {p[0] for p in pieces}
    (out / "img").mkdir(parents=True, exist_ok=True)
    for b in brands:
        if b["logo"] not in labels:
            raise SystemExit("no logo named %r in brands.svg" % b["logo"])
        d = out / b["slug"]
        d.mkdir(parents=True, exist_ok=True)
        (d / "index.html").write_text(page(b, brands, pieces, svg))
        p = next(p for p in pieces if p[0] == b["logo"])
        (d / "favicon.svg").write_text(favicon(p[1], p[2]))
        shutil.copyfile(ROOT / "assets" / "devices" / "brands" / (b["shot"] + ".png"), out / "img" / (b["slug"] + ".png"))
    # The installer page shows the same strip, each logo a link to its page.
    index = out / "index.html"
    slugs = {b["logo"]: b["slug"] for b in brands}
    index.write_text(replace_strip(index.read_text(), strip(svg, pieces, slugs, "", None)))
    urls = [BASE] + [BASE + b["slug"] + "/" for b in brands]
    (out / "sitemap.xml").write_text(
        '<?xml version="1.0" encoding="UTF-8"?>\n<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">\n'
        + "".join("  <url><loc>%s</loc></url>\n" % u for u in urls) + "</urlset>\n")
    (out / "robots.txt").write_text("User-agent: *\nAllow: /\nSitemap: %ssitemap.xml\n" % BASE)
    (out / "brand-pages.json").unlink(missing_ok=True)
    print("brand pages: %s" % ", ".join(b["slug"] for b in brands))


if __name__ == "__main__":
    main()
