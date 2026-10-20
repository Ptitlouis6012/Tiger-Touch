#!/usr/bin/env python3
"""Animated views of the screen, made from the committed captures.

The captures in assets/screens/ are real screens of the device (Demo mode). An
animation is made by changing values on a capture - a percentage, a line, a bar,
a time - the way the firmware draws them: the same font (LVGL's Montserrat
Medium), the same sizes, colours and geometry as firmware/main/ui/. How it works
and how to add a view: docs/MOCKUPS.md.

A tool for the person making pictures, not part of any build (it needs Pillow,
and the font, which ESP-IDF's component manager puts in firmware/managed_components/
on the first build):

  python3 scripts/make-mockups.py            every view, screen alone and in the frame
  python3 scripts/make-mockups.py cards      one view: cards, list, update, tour or stills

Writes assets/mockups/animated/screen/tiger-touch-screen-<view>.gif and .webp (the
screen, 800x480), animated/front/panda-touch-front-<view> (in the front picture of
the Panda Touch) and animated/angle/panda-touch-angle-<view> (in the three-quarter
photograph); `stills` writes every
screen, still, in the three-quarter photograph to assets/mockups/angle/ and every capture
in the front picture, at 3x, to assets/mockups/front/.
"""
import math
import pathlib
import sys

from PIL import Image, ImageDraw, ImageFont

ROOT = pathlib.Path(__file__).resolve().parent.parent
SHOTS = ROOT / "assets" / "screens"
OUT = ROOT / "assets" / "mockups" / "animated"
FONT_FILE = ROOT / "firmware/managed_components/lvgl__lvgl/scripts/generators/built_in_font/Montserrat-Medium.ttf"

# firmware/main/ui/ui_theme.h and ui_settings.c
C_BG = (20, 22, 26)
C_TEXT = (0xf2, 0xf2, 0xf2)
C_MUTED = (0x8b, 0x90, 0x9a)
C_LINE = (0x2b, 0x2f, 0x38)
C_GREEN = (0x3e, 0xcf, 0x8e)
C_BLUE = (0x3d, 0xa4, 0xf0)
C_AMBER = (0xf5, 0xb8, 0x4a)
C_RING_BG = (0x26, 0x45, 0x6f)

PCT_EVERY_S = 20  # firmware/main/demo_mode.c: one per cent every 20 s
SS = 4            # supersampling of rings and bars


def font(size):
    return ImageFont.truetype(str(FONT_FILE), size)


# ---- Text: found on the capture, then replaced --------------------------------------

class Text:
    """A label of the capture: where the firmware drew `text`, found by trying the
    positions around `near` until the drawing matches the capture."""

    def __init__(self, shot, text, size, color, near, align="left", search=6):
        self.size, self.color, self.align = size, color, align
        f = font(size)
        x0, y0 = near
        best = None
        # LVGL puts a glyph on whole pixels but Pillow can place it on quarters: try both
        for dy in range(-search, search + 1):
            for dx in [d + q / 4 for d in range(-search, search + 1) for q in range(4)]:
                x, y = x0 + dx, y0 + dy
                box = self._box(f, text, x, y)
                ref = shot.crop(box)
                bg = _border_colour(ref)
                tile = Image.new("RGB", ref.size, bg)
                ImageDraw.Draw(tile).text((x - box[0], y - box[1]), text, font=f, fill=color, anchor="ls")
                err = _diff(tile, ref)
                if best is None or err < best[0]:
                    best = (err, x, y, bg)
        self.err, self.x, self.y, self.bg = best
        self.adv = f.getlength(text)
        self.old = text

    @staticmethod
    def _box(f, text, x, y):
        l, t, r, b = f.getbbox(text, anchor="ls")
        return (int(x + l) - 2, int(y + t) - 2, int(x + r) + 3, int(y + b) + 3)

    def anchor_for(self, text):
        """Where to draw `text` so that it sits as the firmware would put it."""
        f = font(self.size)
        if self.align == "right":
            return self.x + self.adv - f.getlength(text)
        if self.align == "center":
            return self.x + (self.adv - f.getlength(text)) / 2
        return self.x

    def draw(self, img, text, color=None, pad_left=0, pad_right=0):
        """Erase the capture's text and write `text` in its place; the capture's own
        pixels are kept while the text is the same."""
        if text == self.old and color is None:
            return
        f = font(self.size)
        d = ImageDraw.Draw(img)
        old = self._box(f, self.old, self.x, self.y)
        d.rectangle((old[0] - pad_left, old[1], old[2] + pad_right, old[3]), fill=self.bg)
        if text:
            d.text((self.anchor_for(text), self.y), text, font=f, fill=color or self.color, anchor="ls")


def _border_colour(img):
    """The background around a label: the commonest colour of its edge."""
    w, h = img.size
    px = img.load()
    edge = [px[x, 0] for x in range(w)] + [px[x, h - 1] for x in range(w)]
    return max(set(edge), key=edge.count)


def _diff(a, b):
    pa, pb = a.tobytes(), b.tobytes()
    return sum(abs(x - y) for x, y in zip(pa, pb)) / max(1, len(pa))


# ---- Shapes: drawn the way LVGL draws them ---------------------------------------------

def arc_ring(size, r_out, width, pct, track, ind, bg, rounded=True):
    """An lv_arc: a full track, the indicator from 12 o'clock, clockwise, round ends."""
    big = Image.new("RGB", (size * SS, size * SS), bg)
    d = ImageDraw.Draw(big)
    c = size * SS / 2
    box = [c - r_out * SS, c - r_out * SS, c + r_out * SS, c + r_out * SS]  # Pillow draws the width inwards
    w = width * SS
    d.arc(box, 0, 360, fill=track, width=w)
    if pct > 0:
        a0, a1 = -90, -90 + 360 * min(pct, 100) / 100
        d.arc(box, a0, a1, fill=ind, width=w)
        if rounded and pct < 100:
            rm = (r_out - width / 2) * SS
            for a in (a0, a1):
                x, y = c + rm * math.cos(math.radians(a)), c + rm * math.sin(math.radians(a))
                d.ellipse([x - w / 2, y - w / 2, x + w / 2, y + w / 2], fill=ind)
    return big.resize((size, size), Image.LANCZOS)


def bar(img, x, y, w, h, pct, track, ind):
    """An lv_bar with round ends: the track, then the indicator from the left."""
    big = Image.new("RGB", (w * SS, h * SS))
    big.paste(img.crop((x, y, x + w, y + h)).resize((w * SS, h * SS)))
    d = ImageDraw.Draw(big)
    r = h * SS // 2
    d.rounded_rectangle([0, 0, w * SS - 1, h * SS - 1], radius=r, fill=track)
    iw = round(w * SS * min(pct, 100) / 100)
    if iw > 0:
        d.rounded_rectangle([0, 0, max(iw, 2 * r) - 1, h * SS - 1], radius=r, fill=ind)
    img.paste(big.resize((w, h), Image.LANCZOS), (x, y))


def dim(img, box, opa, bg):
    """What LVGL draws for an object at `opa` over `bg`: the capture's pixels faded toward it."""
    region = img.crop(box)
    img.paste(Image.blend(Image.new("RGB", region.size, bg), region, opa), box[:2])


def clock(t0_min, t_s):
    m = (t0_min * 60 + t_s) // 60
    return "%02d:%02d" % ((m // 60) % 24, m % 60)


def fmt_left(sec):
    """firmware/main/ui/ui_dashboard.c ui_fmt_left"""
    if sec <= 0:
        return "--"
    if sec >= 3600:
        return "%d h %02d min" % (sec // 3600, (sec % 3600) // 60)
    return "%d min" % ((sec + 59) // 60)


# ---- The views ------------------------------------------------------------------------

DONE_FOR_S = 40   # firmware/main/demo_mode.c: a finished print shows for 40 s, then the next starts
CYCLE_S = 100 * PCT_EVERY_S


def demo_print(left0, t, paused=False):
    """firmware/main/demo_mode.c at `t` s, for a print that had `left0` s left at 0:
    (state, percent, seconds left). A finished print shows Done for 40 s, then the next
    one starts at 0 %."""
    if paused:
        return "Paused", 100 - math.ceil(left0 / PCT_EVERY_S), left0
    left = left0 - t
    if left > 0:
        return "Printing", 100 - math.ceil(left / PCT_EVERY_S), left
    since = -left
    if since <= DONE_FOR_S:
        return "Done", 100, 0
    left = CYCLE_S - (since - DONE_FOR_S)
    return "Printing", 100 - math.ceil(left / PCT_EVERY_S), left


STATE_COLOUR = {"Printing": C_BLUE, "Paused": C_AMBER, "Done": C_GREEN}


def view_cards(t_from=0, t_to=11 * 60, t0=16 * 60 + 5):
    """Home, cards: two prints at 1 % every 20 s; the clock runs, the time left
    counts down, the estimated end stays; at 100 % the state reads Done and it and the
    line turn green, the time left goes and pause and stop dim; 40 s later the next
    print starts."""
    shot = Image.open(SHOTS / "home-cards.png").convert("RGB")
    clk = Text(shot, "16:05", 24, C_TEXT, (371, 41), "center")
    cards = []
    # firmware/main/ui/ui_cards.c: cards of 378 px at x 16 and 406, 1 px of border; the
    # figure at (12, 138), the line at (96, 148) 270 x 10, the light, pause and stop 108 x 56 at
    # x 12, 135 and 258, y 60; the values under the captions at x 12 and 190 (half a card);
    # the state right-aligned at the top right (STATUS_W 150, y 17)
    # the nozzle and bed values in the first two tiles, found at x 64 and 188 of the card
    for cx, k, p0, left, end, noz, bed in ((16, 0, 45, "19 min", "16:23", "220°", "60°"),
                                           (406, 1, 70, "10 min", "16:15", "220°", "61°")):
        cards.append(dict(
            x=cx, k=k, left0=(100 - p0) * PCT_EVERY_S, noz0=noz, bed0=bed,
            noz=Text(shot, noz, 20, C_TEXT, (cx + 64, 411), search=3),
            bed=Text(shot, bed, 20, C_TEXT, (cx + 188, 411), search=3),
            pct=Text(shot, "%d %%" % p0, 24, C_TEXT, (cx + 13, 226)),
            left=Text(shot, left, 20, C_TEXT, (cx + 13, 339)),
            endt=Text(shot, end, 20, C_TEXT, (cx + 191, 339)),
            st=Text(shot, "Printing", 16, C_BLUE, (cx + 367 - font(16).getlength("Printing"), 98), "right"),
        ))
    _report("cards", [clk] + [x for c in cards for x in (c["pct"], c["left"], c["endt"], c["st"], c["noz"], c["bed"])])
    frames = []
    for t in range(t_from, t_to + 1, PCT_EVERY_S):
        img = shot.copy()
        clk.draw(img, clock(t0, t))
        for c in cards:
            state, p, left_s = demo_print(c["left0"], t)
            done = state == "Done"
            c["st"].draw(img, state, STATE_COLOUR[state])
            c["pct"].draw(img, "%d %%" % p, pad_right=8)
            bar(img, c["x"] + 97, 213, 270, 10, p, C_LINE, STATE_COLOUR[state])
            c["left"].draw(img, fmt_left(left_s))
            c["endt"].draw(img, "--:--" if done else clock(t0, t + left_s))
            # demo_mode.c: a printer with no print running cools to the room's temperature
            c["noz"].draw(img, "%d°" % (24 + c["k"] % 3) if done else c["noz0"], pad_right=6)
            c["bed"].draw(img, "%d°" % (23 + c["k"] % 2) if done else c["bed0"], pad_right=6)
            if done:
                # Nothing left to pause or stop: the two buttons dim to 40 %, as ui_job_set() does.
                for bx in (c["x"] + 136, c["x"] + 259):
                    dim(img, (bx, 125, bx + 108, 181), 0.4, (30, 33, 40))
        frames.append(img)
    return frames


# The list, firmware/main/ui/ui_cards.c: rows of 690 x 76 every 84 px, 16 px from the left;
# home-list-top.png shows the first five (from y 76), home-list.png the last five (from y 56),
# taken one after the other. Per printer: its place in demo_mode.c (the order of the account,
# which breaks ties in ui_cards_sort), the capture and row, and what the row shows there:
# percent, time left, state, nozzle and bed.
ROWS = (
    ("Core One", 6, "home-list-top", 0, 96, "2 min", "Printing", "221°", "60°"),
    ("K2 Plus", 1, "home-list-top", 1, 90, "4 min", "Printing", "220°", "60°"),
    ("Ender-3 S1", 9, "home-list-top", 2, 83, "6 min", "Printing", "219°", "60°"),
    ("U1", 4, "home-list-top", 3, 78, "8 min", "Printing", "220°", "60°"),
    ("X1 Carbon", 0, "home-list-top", 4, 65, "12 min", "Printing", "220°", "60°"),
    ("Centauri Carbon 2", 2, "home-list", 0, 41, "20 min", "Printing", "221°", "61°"),
    ("Voron 2.4", 7, "home-list", 1, 32, "23 min", "Printing", "220°", "60°"),
    ("AD5X", 5, "home-list", 2, 31, "23 min", "Paused", "219°", "59°"),
    ("P2S", 8, "home-list", 3, 16, "28 min", "Printing", "220°", "61°"),
    ("Kobra S1", 3, "home-list", 4, 9, "31 min", "Printing", "220°", "60°"),
)
ROW_W, ROW_H, ROW_STEP = 690, 76, 84
LIST_TOP = {"home-list-top": 76, "home-list": 56}


def view_list(t_from=0, t_to=11 * 60, t0=16 * 60 + 11):
    """Home, list, scrolled to its end: the ten printers at 1 % every 20 s, the paused one
    standing still, in order of the end of their print (ui_cards_sort): a finished print
    reads Done in green, loses its time left, dims pause and stop and goes to the bottom;
    40 s later its next print starts there."""
    shots = {k: Image.open(SHOTS / (k + ".png")).convert("RGB") for k in LIST_TOP}
    base = shots["home-list"]
    clk = Text(base, "16:11", 24, C_TEXT, (373, 41), "center")
    rows, texts = [], [clk]
    for name, k, src, i, p0, left, state, noz, bed in ROWS:
        top = LIST_TOP[src] + i * ROW_STEP
        sprite = shots[src].crop((16, top, 16 + ROW_W, top + ROW_H))
        # Where ui_cards.c build_row puts them, less the row's x 16: the figure at 268, the
        # time left right-aligned at 418, the state under the name at 64, the temperatures at 454.
        r = dict(name=name, k=k, sprite=sprite, paused=state == "Paused",
                 left0=(100 - p0) * PCT_EVERY_S, noz0=noz, bed0=bed,
                 pct=Text(sprite, "%d %%" % p0, 20, C_TEXT, (268, 27), search=2),
                 left=Text(sprite, left, 14, C_MUTED, (418 - font(14).getlength(left), 27), "right"),
                 st=Text(sprite, state, 14, STATE_COLOUR[state], (64, 58)),
                 noz=Text(sprite, noz, 16, C_TEXT, (454, 28)),
                 bed=Text(sprite, bed, 16, C_TEXT, (454, 60)))
        rows.append(r)
        texts += [r["pct"], r["left"], r["st"], r["noz"], r["bed"]]
    _report("list", texts)
    frames = []
    for t in range(t_from, t_to + 1, PCT_EVERY_S):
        img = base.copy()
        clk.draw(img, clock(t0, t))
        drawn = []
        for r in rows:
            state, p, left_s = demo_print(r["left0"], t, r["paused"])
            busy = state != "Done"
            s = r["sprite"].copy()
            r["st"].draw(s, state, STATE_COLOUR[state])
            r["pct"].draw(s, "%d %%" % p, pad_right=6)
            r["left"].draw(s, fmt_left(left_s) if busy else "")
            bar(s, 268, 38, 150, 6, p, C_LINE, STATE_COLOUR[state])
            # demo_mode.c: a printer with no print running cools to the room's temperature
            r["noz"].draw(s, r["noz0"] if busy else "%d°" % (24 + r["k"] % 3))
            r["bed"].draw(s, r["bed0"] if busy else "%d°" % (23 + r["k"] % 2))
            if not busy:
                # Pause and stop dim to 40 % (ui_cards.c: ROW_PAUSE_X, ROW_STOP_X, 54 x 52, centred in the row).
                for bx in (568, 628):
                    dim(s, (bx, 12, bx + 54, 64), 0.4, (30, 33, 40))
            # ui_cards_sort: the seconds left, a printer with none last; ties in the account's order
            drawn.append(((left_s if left_s > 0 else 1 << 30), r["k"], s))
        drawn.sort(key=lambda d: (d[0], d[1]))
        for slot, (_, _, s) in enumerate(drawn[-5:]):  # scrolled to the end: the last five rows
            img.paste(s, (16, LIST_TOP["home-list"] + slot * ROW_STEP))
        frames.append(img)
    return frames


def view_update():
    """The update: the confirmation, then the ring from 0 to 100 %, then Rebooting
    (firmware/main/ui/ui_settings.c: ring 220 px centred at (400, 170), 14 px wide)."""
    confirm = Image.open(SHOTS / "update-confirm.png").convert("RGB")
    frames = [confirm] * 8
    f48, f20 = font(48), font(20)
    for p in list(range(0, 101, 2)) + [100] * 6:
        img = Image.new("RGB", (800, 480), C_BG)
        ring = arc_ring(232, 110, 14, p, C_RING_BG, C_GREEN, C_BG)
        img.paste(ring, (400 - 116, 170 - 116))
        d = ImageDraw.Draw(img)
        for pts in (((400, 143), (400, 197)), ((378, 165), (400, 143), (422, 165))):
            d.line(pts, fill=C_GREEN, width=12, joint="curve")
            for x, y in (pts[0], pts[-1]):
                d.ellipse([x - 6, y - 6, x + 6, y + 6], fill=C_GREEN)
        rebooting = p == 100 and len(frames) > 8 + 51
        # Centred on x 400, on the baselines found on assets/screens/update-ring.png.
        d.text((400, 339), "%d%%" % p, font=f48, fill=C_TEXT, anchor="ms")
        d.text((400, 382), "Rebooting" if rebooting else "Installing", font=f20, fill=C_MUTED, anchor="ms")
        if not rebooting:
            d.text((400, 424), "Do not power off", font=f20, fill=C_TEXT, anchor="ms")
        frames.append(img)
    return frames


def splash():
    """The start-up screen, decoded from the firmware's own copy (firmware/main/ui/boot_splash.c, RGB565)."""
    import re
    src = (ROOT / "firmware/main/ui/boot_splash.c").read_text()
    body = src[src.index("SPLASH_PIXELS"):]
    vals = [int(v, 16) for v in re.findall(r"0x([0-9a-fA-F]{4})", body)][:800 * 480]
    rgb = bytearray()
    for v in vals:
        rgb += bytes((((v >> 11) & 31) * 255 // 31, ((v >> 5) & 63) * 255 // 63, (v & 31) * 255 // 31))
    return Image.frombytes("RGB", (800, 480), bytes(rgb))


def view_tour():
    """Power on: the start-up screen, then the list for six minutes, then the cards
    for the next six, on one clock (each step 20 s, shown twice as long as in the
    single views: a frame repeated is merged into one longer frame in the GIF)."""
    black = Image.new("RGB", (800, 480), (0, 0, 0))
    slow = lambda frames: [f for f in frames for _ in range(2)]
    return ([black] * 3 + [splash()] * 14 + slow(view_list(0, 6 * 60))
            + slow(view_cards(6 * 60 + PCT_EVERY_S, 12 * 60, t0=16 * 60 + 11)))


def brand_screens():
    """The three screens of the protocols' logos, taken out of their front mockups
    (assets/mockups/front/panda-touch-front-brands-*.png, made at 3x): the glass, back to 800 x 480."""
    out = {}
    for f in sorted((ROOT / "assets/mockups/front").glob("panda-touch-front-brands-*.png")):
        im = Image.open(f).convert("RGB")
        out[f.stem.replace("panda-touch-front-", "")] = im.crop((114, 81, 114 + 1116, 81 + 675)).resize((800, 480), Image.LANCZOS)
    return out


def view_stills():
    """Every capture, the start-up screen and the protocols' screens in the three-quarter
    picture: assets/mockups/angle/panda-touch-angle-<name>.png, 1200 px wide; every
    capture in the front picture: assets/mockups/front/panda-touch-front-<name>.png."""
    screens = {"splash": splash()}
    screens.update(brand_screens())
    for f in sorted(SHOTS.glob("*.png")):
        screens[f.stem] = Image.open(f).convert("RGB")
    d = ROOT / "assets/mockups/angle"
    d.mkdir(parents=True, exist_ok=True)
    for name, scr in screens.items():
        in_angle(scr, 1200).save(d / ("panda-touch-angle-%s.png" % name), optimize=True)
    print("stills: %d pictures in %s" % (len(screens), d.relative_to(ROOT)))
    # The captures in the front picture, at 3x (the protocols' screens are made from these, not of them)
    front = ROOT / "assets/mockups/front"
    n = 0
    for f in sorted(SHOTS.glob("*.png")):
        in_frame(Image.open(f).convert("RGB"), 3).save(front / ("panda-touch-front-%s.png" % f.stem), optimize=True)
        n += 1
    print("stills: %d pictures in %s" % (n, front.relative_to(ROOT)))
    return []


VIEWS = {"cards": view_cards, "list": view_list, "update": view_update, "tour": view_tour,
         "stills": view_stills}


def _report(view, texts):
    worst = max(t.err for t in texts)
    print("%s: %d labels found on the capture, worst mismatch %.1f/255" % (view, len(texts), worst))
    if worst > 24:  # a figure of narrow digits (1, 2) sits up to a pixel off the firmware's glyphs
        sys.exit("%s: a label was not found where expected: check its text, size and position" % view)


# ---- Output --------------------------------------------------------------------------

def in_frame(screen, s=2):
    """The screen in the front picture (assets/devices/panda-touch-front.png), `s` times its size."""
    gx, gy, gw, gh = 38 * s, 27 * s, 372 * s, 225 * s  # the glass, measured on the picture
    dev = Image.open(ROOT / "assets/devices/panda-touch-front.png").convert("RGBA")
    dev = dev.resize((dev.width * s, dev.height * s), Image.LANCZOS)
    scale = max(gw / screen.width, gh / screen.height)
    sc = screen.resize((round(screen.width * scale), round(screen.height * scale)), Image.LANCZOS)
    x0, y0 = (sc.width - gw) // 2, (sc.height - gh) // 2
    dev.alpha_composite(sc.crop((x0, y0, x0 + gw, y0 + gh)).convert("RGBA"), (gx, gy))
    return dev


# The glass of assets/devices/panda-touch-angle.png, its four corners measured on the
# picture: top left, top right, bottom right, bottom left.
ANGLE_GLASS = ((247.5, 70.0), (1231.5, 89.0), (1119.5, 768.0), (110.8, 655.8))


def _homography(src, dst):
    """The 8 coefficients Pillow's PERSPECTIVE transform wants: output (dst) -> input (src)."""
    import numpy as np
    a, b = [], []
    for (x, y), (u, v) in zip(dst, src):
        a.append([x, y, 1, 0, 0, 0, -u * x, -u * y]); b.append(u)
        a.append([0, 0, 0, x, y, 1, -v * x, -v * y]); b.append(v)
    return [float(c) for c in np.linalg.solve(np.array(a), np.array(b))]


def in_angle(screen, width=1000):
    """The screen in the three-quarter picture: warped in perspective onto its glass,
    with a faint sheen, then the whole scaled to `width`."""
    dev = Image.open(ROOT / "assets/devices/panda-touch-angle.png").convert("RGBA")
    k = 2  # warp a screen twice as large: sharper text once it is shrunk
    big = screen.convert("RGBA").resize((screen.width * k, screen.height * k), Image.LANCZOS)
    src = ((0, 0), (big.width, 0), (big.width, big.height), (0, big.height))
    coeffs = _homography(src, ANGLE_GLASS)
    warped = big.transform(dev.size, Image.PERSPECTIVE, coeffs, Image.BICUBIC)
    # The glass's edge, drawn four times larger then shrunk: a clean, anti-aliased border.
    m = Image.new("L", (dev.width * 4, dev.height * 4), 0)
    ImageDraw.Draw(m).polygon([(x * 4, y * 4) for x, y in ANGLE_GLASS], fill=255)
    mask = m.resize(dev.size, Image.LANCZOS)
    warped.putalpha(mask)
    out = dev.copy()
    out.alpha_composite(warped)
    # A faint diagonal reflection across the glass, as on the product picture.
    sheen = Image.new("L", dev.size, 0)
    (x0, y0), (x1, y1), (x2, y2), (x3, y3) = ANGLE_GLASS
    ImageDraw.Draw(sheen).polygon([(x0 + (x1 - x0) * .55, y0 + (y1 - y0) * .55), (x1, y1),
                                   (x1 + (x2 - x1) * .45, y1 + (y2 - y1) * .45),
                                   (x3 + (x2 - x3) * .25, y3 + (y2 - y3) * .25)], fill=16)
    from PIL import ImageChops, ImageFilter
    sheen = ImageChops.multiply(sheen.filter(ImageFilter.GaussianBlur(60)), mask)
    out.alpha_composite(Image.merge("RGBA", (Image.new("L", dev.size, 255),) * 3 + (sheen,)))
    return out.resize((width, round(out.height * width / out.width)), Image.LANCZOS)


def save(name, frames, step_ms=150, hold_ms=1800, framings=("front",)):
    durations = [step_ms] * len(frames)
    durations[0] = 700
    durations[-1] = hold_ms
    OUT.mkdir(parents=True, exist_ok=True)
    place = {"front": in_frame, "angle": in_angle}
    for suffix, fr in [("", frames)] + [("-" + p, [place[p](f) for f in frames]) for p in framings]:
        flat = []
        for f in fr:
            if f.mode == "RGBA":
                b = Image.new("RGBA", f.size, C_BG + (255,))
                b.alpha_composite(f)
                f = b.convert("RGB")
            flat.append(f)
        # One palette for every frame, from the first, middle and last: no flicker.
        sheet = Image.new("RGB", (flat[0].width, flat[0].height * 3))
        for k, f in enumerate((flat[0], flat[len(flat) // 2], flat[-1])):
            sheet.paste(f, (0, k * f.height))
        pal = sheet.quantize(colors=256, method=Image.Quantize.MAXCOVERAGE)
        gif = [f.quantize(palette=pal, dither=Image.Dither.NONE) for f in flat]
        folder = suffix.lstrip("-") or "screen"
        prefix = {"screen": "tiger-touch-screen-", "front": "panda-touch-front-", "angle": "panda-touch-angle-"}[folder]
        base = OUT / folder / (prefix + name)  # a different name in each folder: downloads do not collide
        base.parent.mkdir(parents=True, exist_ok=True)
        gif[0].save(base.with_suffix(".gif"), save_all=True, append_images=gif[1:], duration=durations, loop=0,
                    optimize=True)
        fr[0].save(base.with_suffix(".webp"), save_all=True, append_images=fr[1:], duration=durations, loop=0,
                   quality=90)
        print("  %s/%s.gif %d KB, .webp %d KB" % (base.parent.name, base.name, base.with_suffix(".gif").stat().st_size // 1024,
                                               base.with_suffix(".webp").stat().st_size // 1024))


def main():
    if not FONT_FILE.exists():
        sys.exit("no %s: build the firmware once (idf.py fetches LVGL)" % FONT_FILE.relative_to(ROOT))
    names = sys.argv[1:] or list(VIEWS)
    for n in names:
        if n not in VIEWS:
            sys.exit("unknown view %r: %s" % (n, ", ".join(VIEWS)))
        frames = VIEWS[n]()
        if n == "stills":
            continue
        save(n, frames, step_ms=160 if n == "tour" else 150, framings=("front", "angle"))


if __name__ == "__main__":
    main()
