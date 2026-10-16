"""Boot picture -> main/ui/boot_splash.c (RGB565, 800x480).

    python tools/make_splash.py PICTURE.png [--preview PREVIEW.png]

The picture is the brand's boot screen; it is not kept in this repository, only
the generated C file is.
"""
import pathlib
import struct
import sys
import zlib

if len(sys.argv) < 2:
    sys.exit(__doc__)
SRC = sys.argv[1]
OUT_C = str(pathlib.Path(__file__).resolve().parent.parent / "main" / "ui" / "boot_splash.c")
PREVIEW = sys.argv[sys.argv.index("--preview") + 1] if "--preview" in sys.argv else None
DW, DH = 800, 480

data = open(SRC, "rb").read()
pos = 8
idat = b""
w = h = 0
while pos < len(data):
    n = struct.unpack(">I", data[pos:pos + 4])[0]
    tag = data[pos + 4:pos + 8]
    body = data[pos + 8:pos + 8 + n]
    if tag == b"IHDR":
        w, h, bd, ct, _, _, il = struct.unpack(">IIBBBBB", body)
        assert (bd, ct, il) == (8, 2, 0), "expected 8-bit RGB, not interlaced"
    elif tag == b"IDAT":
        idat += body
    pos += 12 + n

raw = zlib.decompress(idat)
bpp, stride = 3, w * 3
img = bytearray(h * stride)
prev = bytearray(stride)
for y in range(h):
    ft = raw[y * (stride + 1)]
    line = bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
    if ft == 1:
        for i in range(bpp, stride):
            line[i] = (line[i] + line[i - bpp]) & 255
    elif ft == 2:
        for i in range(stride):
            line[i] = (line[i] + prev[i]) & 255
    elif ft == 3:
        for i in range(stride):
            a = line[i - bpp] if i >= bpp else 0
            line[i] = (line[i] + ((a + prev[i]) >> 1)) & 255
    elif ft == 4:
        for i in range(stride):
            a = line[i - bpp] if i >= bpp else 0
            b = prev[i]
            c = prev[i - bpp] if i >= bpp else 0
            p = a + b - c
            pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
            pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
            line[i] = (line[i] + pr) & 255
    img[y * stride:(y + 1) * stride] = line
    prev = line
print("decoded", w, "x", h)

# area-average resize to DWxDH
out565 = []
preview = bytearray()
xs = [(int(x * w / DW), max(int(x * w / DW) + 1, int((x + 1) * w / DW))) for x in range(DW)]
for dy in range(DH):
    y0 = int(dy * h / DH)
    y1 = max(y0 + 1, int((dy + 1) * h / DH))
    preview.append(0)
    for dx in range(DW):
        x0, x1 = xs[dx]
        r = g = b = n = 0
        for yy in range(y0, y1):
            base = yy * stride
            for xx in range(x0, x1):
                i = base + xx * 3
                r += img[i]
                g += img[i + 1]
                b += img[i + 2]
                n += 1
        r //= n
        g //= n
        b //= n
        v = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
        out565.append(v)
        preview += bytes((((v >> 11) & 31) * 255 // 31, ((v >> 5) & 63) * 255 // 63, (v & 31) * 255 // 31))

lines = []
for i in range(0, len(out565), 16):
    lines.append(",".join("0x%04X" % v for v in out565[i:i + 16]) + ",")

c = """/* Generated from bootscreentigertouch.png by a script: do not edit by hand.
 * 800x480, RGB565 little-endian, opaque. Shown for a few seconds at power-on. */
#include "boot_splash.h"

static const uint16_t SPLASH_PIXELS[800 * 480] = {
%s
};

const lv_image_dsc_t boot_splash = {
    .header = {
        .magic = LV_IMAGE_HEADER_MAGIC,
        .cf = LV_COLOR_FORMAT_RGB565,
        .w = 800,
        .h = 480,
        .stride = 800 * 2,
    },
    .data_size = sizeof SPLASH_PIXELS,
    .data = (const uint8_t *)SPLASH_PIXELS,
};
""" % "\n".join(lines)
open(OUT_C, "w", encoding="utf-8", newline="\n").write(c)
print("wrote", OUT_C, len(c) // 1024, "KB of source")


def chunk(t, d):
    x = struct.pack(">I", len(d)) + t + d
    return x + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)


if PREVIEW:
    with open(PREVIEW, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", struct.pack(">IIBBBBB", DW, DH, 8, 2, 0, 0, 0)))
        f.write(chunk(b"IDAT", zlib.compress(bytes(preview), 6)))
        f.write(chunk(b"IEND", b""))
    print("preview", PREVIEW)
