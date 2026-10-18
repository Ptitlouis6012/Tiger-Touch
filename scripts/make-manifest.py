#!/usr/bin/env python3
"""Build the published site of a release: the installer page, its images, and
the OTA manifest.

    python scripts/make-manifest.py --version 2.0.0 --repo owner/name \\
        --dist dist --out site

`dist` holds the release's own assets (downloaded from the release, never
rebuilt). Written into `out`, beside a copy of installer/:

  version.json    read by every Tiger Touch in the field (net/ota.c)
  manifest.json   read by ESP Web Tools on the installer page
  the four images the installer writes, at their offsets
  tiger-touch-tour.webp   the screen in the three-quarter photograph, start-up, list
                          and cards (assets/mockups/animated/angle/panda-touch-angle-tour.webp)
  <brand>/index.html      one page per printer brand, sitemap.xml, robots.txt
                          (make-brand-pages.py)

THREE RULES, each of which has cost a sibling project a release.

**Never committed.** A committed copy and a generated one drift, and the drift is
invisible until a fleet acts on it.

**Built around the release, never around what is live.** The version and the
images come from the tag being published, so this gives the same output whether
it runs before or after the release appears.

**The hash is of the exact bytes the device downloads.** Taken from the release
asset itself: a rebuilt image differs, and the device refuses an image whose
SHA-256 is not the manifest's - correctly, and confusingly.

version.json may grow keys forever; none of the three the device reads
(`version`, `firmware_url`, `firmware_sha256`) is ever removed, renamed or
retyped: a unit unplugged for a year comes back and reads exactly those.
"""
import argparse
import datetime
import hashlib
import json
import pathlib
import shutil
import subprocess
import sys

# Kept in step with firmware/partitions.csv. The NVS (0x9000) is in no part, so
# an install over an existing board keeps its Wi-Fi and account. The initial
# OTA data resets the boot choice to ota_0, where the application is written: a
# board that had updated itself into ota_1 would otherwise keep booting that.
PARTS = [
    ("bootloader.bin", 0x0),
    ("partition-table.bin", 0x8000),
    ("ota_data_initial.bin", 0xF000),
    ("tiger-touch.bin", 0x20000),
]


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--version", required=True)
    ap.add_argument("--repo", required=True, help="owner/name")
    ap.add_argument("--dist", required=True, help="the release's assets")
    ap.add_argument("--out", required=True)
    ap.add_argument("--installer", default="installer", help="the page's sources")
    a = ap.parse_args()

    dist, out = pathlib.Path(a.dist), pathlib.Path(a.out)
    app = dist / ("tiger-touch-v%s.bin" % a.version)
    if not app.exists():
        print("error: %s is missing - nothing to publish" % app, file=sys.stderr)
        return 1
    out.mkdir(parents=True, exist_ok=True)
    shutil.copytree(a.installer, out, dirs_exist_ok=True)
    (out / "README.md").unlink(missing_ok=True)
    # The installer's picture: the tour in the three-quarter photograph, the same file
    # as the README's and the media kit's (scripts/make-mockups.py), so they never differ.
    root = pathlib.Path(__file__).resolve().parent.parent
    shutil.copyfile(root / "assets" / "mockups" / "animated" / "angle" / "panda-touch-angle-tour.webp", out / "tiger-touch-tour.webp")
    # One page per printer brand, the sitemap and robots.txt (make-brand-pages.py).
    subprocess.run([sys.executable, str(root / "scripts" / "make-brand-pages.py"), "--out", str(out)], check=True)

    base = "https://github.com/%s/releases/download/v%s" % (a.repo, a.version)
    parts = []
    for name, offset in PARTS:
        src = app if name == "tiger-touch.bin" else dist / name
        if not src.exists():
            print("error: %s is missing - the installer would write an incomplete board" % src, file=sys.stderr)
            return 1
        # Served beside the page: a cross-origin fetch of a release asset is at
        # the mercy of whatever CORS headers that host sends.
        shutil.copyfile(src, out / name)
        parts.append({"path": name, "offset": offset})

    (out / "manifest.json").write_text(json.dumps({
        "name": "Tiger Touch",
        "version": a.version,
        "new_install_prompt_erase": True,
        "builds": [{"chipFamily": "ESP32-S3", "parts": parts}],
    }, indent=2) + "\n", encoding="utf-8")

    (out / "version.json").write_text(json.dumps({
        # The three keys the device reads. Frozen.
        "version": a.version,
        "firmware_url": "%s/%s" % (base, app.name),
        "firmware_sha256": sha256(app),
        # For people and the installer page; a device skips what it does not know.
        "released": datetime.datetime.now(datetime.timezone.utc).replace(microsecond=0).isoformat(),
        "notes_url": "https://github.com/%s/releases/tag/v%s" % (a.repo, a.version),
    }, indent=2) + "\n", encoding="utf-8")
    print("site in %s: version %s, %d parts, OTA manifest written" % (out, a.version, len(parts)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
