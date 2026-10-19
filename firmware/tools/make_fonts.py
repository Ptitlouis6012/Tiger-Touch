"""Regenerate the UI faces main/ui/fonts/font_ui_{14,16,20,24}.c.

    python tools/make_fonts.py --noto PATH/TO/NotoSansSC-Medium.otf --sources DIR

What goes in (one face per size, the way the TigerSpool builds them):
  - Montserrat Medium: ASCII, Latin-1 + Latin Extended-A (French through
    Polish), the bullet and the ellipsis, and the Cyrillic letters Russian
    needs;
  - LVGL's symbol list from FontAwesome (the LV_SYMBOL_* glyphs, plus the sun);
  - a Noto Sans SC Medium subset holding exactly the Han characters the
    translation table writes, a few hundred glyphs instead of thousands.

A fifth face, font_icon_32.c, holds only FontAwesome's sun at 32 px: the
settings menu's Screen row, as on TigerScale, drawn the size of the row's other
icons (LVGL's Montserrat 32, which has no sun).

The character set is read from main/i18n_table.h (and the language names in
main/i18n.c), so adding a Chinese sentence and re-running this is all it takes.
The run ends by checking that every character the table can draw has a glyph:
LVGL draws a missing one as a blank box and logs nothing.

Nothing font-shaped is committed except the generated .c files. The sources:
  - Montserrat-Medium.ttf and FontAwesome5-Solid+Brands+Regular.woff are LVGL's.
    The component carries them after a first build: in
    scripts/generators/built_in_font/ (9.6), scripts/built_in_font/ (up to
    9.2); both are looked in. --sources (or LVGL_FONT_SOURCES) gives another
    folder, such as github.com/lvgl/lvgl at tag v9.2.0, the files every earlier
    face was generated from;
  - Noto Sans SC Medium (SIL OFL) is given with --noto, or NOTO_SANS_SC;
  - lv_font_conv is run through npx, or LV_FONT_CONV points at its .js.
"""
import glob
import os
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
MAIN = ROOT / "main" / "ui"
I18N_DIR = ROOT / "main"

SIZES = [14, 16, 20, 24]
TEXT_RANGE = "0x20-0x7F,0xA1-0x17F,0x2022,0x2026,0x401,0x410-0x44F,0x451"
# LVGL's own symbol list, plus the sun (61829) and three the settings menu
# draws beside its rows: user (61447), print (61487) and globe (61612).
SYMBOLS = ("61441,61448,61451,61452,61452,61453,61457,61459,61461,61465,61468,61473,61478,61479,61480,"
           "61502,61507,61512,61515,61516,61517,61521,61522,61523,61524,61543,61544,61550,61552,61553,"
           "61556,61559,61560,61561,61563,61587,61589,61636,61637,61639,61641,61664,61671,61674,61683,"
           "61724,61732,61787,61829,61931,62016,62017,62018,62019,62020,62087,62099,62212,62189,62810,"
           "63426,63650,61447,61487,61612")


def first_existing(patterns):
    for pat in patterns:
        hits = sorted(glob.glob(str(pat)))
        if hits:
            return pathlib.Path(hits[0])
    return None


def find_sources():
    argv = sys.argv[1:]
    otf = None
    if "--noto" in argv:
        otf = pathlib.Path(argv[argv.index("--noto") + 1])
    elif os.environ.get("NOTO_SANS_SC"):
        otf = pathlib.Path(os.environ["NOTO_SANS_SC"])
    cands = []
    if "--sources" in argv:
        cands.append(pathlib.Path(argv[argv.index("--sources") + 1]))
    if os.environ.get("LVGL_FONT_SOURCES"):
        cands.append(pathlib.Path(os.environ["LVGL_FONT_SOURCES"]))
    # LVGL up to 9.2 kept them in scripts/built_in_font/, 9.6 in scripts/generators/built_in_font/.
    cands.append(ROOT / "managed_components" / "lvgl__lvgl" / "scripts" / "generators" / "built_in_font")
    cands.append(ROOT / "managed_components" / "lvgl__lvgl" / "scripts" / "built_in_font")
    builtin = next((c for c in cands if (c / "Montserrat-Medium.ttf").exists()), None)
    conv = os.environ.get("LV_FONT_CONV")
    problems = []
    if builtin is None:
        problems.append("Montserrat-Medium.ttf and the FontAwesome woff (pass --sources DIR, see above)")
    if otf is None or not otf.exists():
        problems.append("NotoSansSC-Medium.otf (pass --noto PATH or set NOTO_SANS_SC)")
    if problems:
        sys.exit("cannot find: " + "; ".join(problems))
    return builtin / "Montserrat-Medium.ttf", builtin / "FontAwesome5-Solid+Brands+Regular.woff", otf, conv


def is_han(c):
    cp = ord(c)
    return 0x3000 <= cp <= 0x303F or 0x4E00 <= cp <= 0x9FFF or 0xFF00 <= cp <= 0xFFEF


def drawn_text():
    """Every character the firmware draws from its own strings."""
    text = ""
    for name in ("i18n_table.h", "i18n.c"):
        p = I18N_DIR / name
        if p.exists():
            text += p.read_text(encoding="utf-8")
    return text

# The icons drawn at 32 px that Montserrat 32 lacks: the sun (U+F185).
ICON_32 = "61829"


def main():
    ttf, fa, otf, conv = find_sources()
    text = drawn_text()
    han = "".join(sorted({c for c in text if is_han(c)}))
    print(f"{len(han)} distinct Han characters")

    work = pathlib.Path(tempfile.mkdtemp(prefix="tiger-touch-fonts-"))
    try:
        for src in (ttf, fa, otf):
            shutil.copy(src, work / src.name)
        for sz in SIZES:
            name = f"font_ui_{sz}.c"
            out = MAIN / "fonts" / name
            runner = ["node", str(conv)] if conv else ["npx", "--yes", "lv_font_conv@1.5.3"]
            cmd = runner + [
                   "--no-compress", "--no-prefilter", "--bpp", "4", "--size", str(sz),
                   "--font", ttf.name, "-r", TEXT_RANGE,
                   "--font", fa.name, "-r", SYMBOLS,
                   "--font", otf.name, "--symbols", han,
                   "--format", "lvgl", "-o", name, "--force-fast-kern-format"]
            subprocess.run(cmd, cwd=work, check=True, shell=(os.name == "nt" and not conv))
            # Written beside its sources under a bare name, so that the header
            # lv_font_conv leaves in the file names no path of this machine.
            shutil.move(str(work / name), str(out))
            print(f"  {out.relative_to(ROOT)}  {out.stat().st_size // 1024} KB")
        name = "font_icon_32.c"
        out = MAIN / "fonts" / name
        subprocess.run(runner + ["--no-compress", "--no-prefilter", "--bpp", "4", "--size", "32",
                                 "--font", fa.name, "-r", ICON_32, "--format", "lvgl", "-o", name,
                                 "--force-fast-kern-format"],
                       cwd=work, check=True, shell=(os.name == "nt" and not conv))
        shutil.move(str(work / name), str(out))
        print(f"  {out.relative_to(ROOT)}  {out.stat().st_size // 1024} KB")
    finally:
        shutil.rmtree(work, ignore_errors=True)

    # Coverage: every printable character of the table must have a glyph.
    ok = True
    for sz in SIZES:
        have = {int(m, 16) for m in re.findall(r"/\* U\+([0-9A-Fa-f]+)", (MAIN / "fonts" / f"font_ui_{sz}.c").read_text(encoding="utf-8"))}
        quoted = re.findall(r'"((?:[^"\\]|\\.)*)"', text)
        need = {ord(c) for s in quoted for c in s if c >= " " and not (0xF000 <= ord(c) <= 0xF8FF)}
        need.discard(0x0A)
        missing = sorted(need - have)
        if missing:
            ok = False
            print(f"  size {sz}: no glyph for " + " ".join(f"U+{cp:04X}({chr(cp)})" for cp in missing))
    if ok:
        print("every character of the table has a glyph in all four faces")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
