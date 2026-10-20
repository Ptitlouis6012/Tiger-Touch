#!/usr/bin/env python3
"""The screen set in the 45-degree drawing of the Panda Touch.

A capture of assets/screens/ is set on the glass of the drawing with the affine map
that draws the screen in that pose, and the animation of the README
(assets/devices/tiger-touch-views.webp) is a cross-fade between four of them. Run it
again when a captured screen changes (docs/MOCKUPS.md):

  python3 scripts/make-drawings.py

A tool for the person making pictures, not part of any build; it needs Pillow and
numpy. It writes assets/devices/brands/<brand>.png (a printer's full view, one per brand page
of the site), assets/devices/tiger-touch-cards.png, -list.png, -printer.png,
assets/tiger-touch-device.png (the cards, as the README's first picture), the
installer's installer/device.png (the cards at half size) and
assets/devices/tiger-touch-views.webp. The start-up drawing (-splash.png) is not a
capture and is left as it is.

How the map was found: for a committed drawing and the capture it was made from, the
six coefficients below are the ones that make the capture, warped, match the drawing
(mean difference 1.2 out of 255 over the screen). The same map fits the three
drawings, and setting the old captures back in the old drawings gives them back to
within 0.4 out of 255.
"""
import pathlib

import numpy as np
from PIL import Image

ROOT = pathlib.Path(__file__).resolve().parent.parent
DEVICES = ROOT / "assets" / "devices"

# Capture (x, y), 800 x 480 -> drawing (X, Y), 1214 x 1476: X = a x + b y + c, Y = d x + e y + f
AFFINE = (1.0749, 0.0045, 111.95, 0.6175, 1.24173, 208.0)

# drawing -> the capture set in it
DRAWINGS = (("cards", "home-cards"), ("list", "home-list"), ("printer", "printer-snapmaker-u1"))

# brand page (installer/brand-pages.json, "shot") -> the capture of its printer, in assets/devices/brands/
BRANDS = (("anycubic", "printer-anycubic-kobra-s1"), ("bambu-lab", "printer-bambu-x1c"),
          ("creality", "printer-creality-k2"), ("elegoo", "printer-elegoo-cc2"),
          ("flashforge", "printer-flashforge-ad5x"), ("klipper", "printer-klipper-voron"),
          ("octoprint", "printer-octoprint-ender3"), ("prusa", "printer-prusa-core-one"),
          ("snapmaker", "printer-snapmaker-u1"))

# The animation: start-up, cards, list, a printer, then round again; each change is three
# frames of 100 ms (a quarter, a half, three quarters), each screen stays 2.2 s (2.6 s for the first).
VIEWS_SIZE = (560, 681)
VIEWS_ORDER = ("splash", "cards", "list", "printer")


def warp(img, resample):
    a, b, c, d, e, f = AFFINE
    inv = np.linalg.inv(np.array([[a, b, c], [d, e, f], [0, 0, 1]], dtype=np.float64))
    return img.transform((1214, 1476), Image.AFFINE, tuple(inv[:2].reshape(-1)), resample=resample)


def set_in(drawing, capture):
    """The capture on the glass of the drawing, over what was there (the old screen)."""
    out = drawing.convert("RGBA")
    screen = warp(capture.convert("RGB"), Image.BICUBIC).convert("RGBA")
    screen.putalpha(warp(Image.new("L", capture.size, 255), Image.BILINEAR))
    out.alpha_composite(screen)
    return out


def palette(img):
    return img.quantize(colors=256, method=Image.Quantize.FASTOCTREE, dither=Image.Dither.NONE)


def views():
    d = [Image.open(DEVICES / ("tiger-touch-%s.png" % n)).convert("RGBA").resize(VIEWS_SIZE, Image.LANCZOS)
         for n in VIEWS_ORDER]
    frames, durations = [], []
    for i, a in enumerate(d):
        b = d[(i + 1) % len(d)]
        frames.append(a)
        durations.append(2600 if i == 0 else 2200)
        for t in (0.25, 0.5, 0.75):
            frames.append(Image.blend(a, b, t))
            durations.append(100)
    frames[0].save(DEVICES / "tiger-touch-views.webp", save_all=True, append_images=frames[1:],
                   duration=durations, loop=0, quality=75, method=6)


def main():
    jobs = [(DEVICES / ("tiger-touch-%s.png" % n), c) for n, c in DRAWINGS]
    jobs += [(DEVICES / "brands" / ("%s.png" % n), c) for n, c in BRANDS]
    for path, shot in jobs:
        out = set_in(Image.open(path), Image.open(ROOT / "assets" / "screens" / (shot + ".png")))
        palette(out).save(path, optimize=True)
        print("%s: %d KB" % (path.relative_to(ROOT), path.stat().st_size // 1024))
    cards = Image.open(DEVICES / "tiger-touch-cards.png")
    cards.save(ROOT / "assets" / "tiger-touch-device.png", optimize=True)
    palette(cards.convert("RGBA").resize((607, 738), Image.LANCZOS)).save(ROOT / "installer" / "device.png",
                                                                       optimize=True)
    views()
    print("assets/devices/tiger-touch-views.webp: %d KB" % ((DEVICES / "tiger-touch-views.webp").stat().st_size // 1024))


if __name__ == "__main__":
    main()
