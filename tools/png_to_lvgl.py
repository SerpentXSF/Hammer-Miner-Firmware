#!/usr/bin/env python3
"""Convert a PNG into the LVGL RGB565 C array this firmware's display uses.

The screens under main/displays/images/themes/<theme>/export/ are generated
files. The vendor shipped them without the generator, so this reproduces the
same layout byte for byte: little-endian RGB565, two bytes per pixel, wrapped
in the include guards LVGL expects.

    python tools/png_to_lvgl.py splashscreen2.png splashscreen2.c

The C symbol is taken from the output file's stem, so the name must match what
themes.c references.
"""

import argparse
import os
import sys

try:
    from PIL import Image
except ImportError:  # pragma: no cover
    sys.exit("Pillow is required: pip install pillow")

HEADER = """#ifdef __has_include
    #if __has_include("lvgl.h")
        #ifndef LV_LVGL_H_INCLUDE_SIMPLE
            #define LV_LVGL_H_INCLUDE_SIMPLE
        #endif
    #endif
#endif

#if defined(LV_LVGL_H_INCLUDE_SIMPLE)
    #include "lvgl.h"
#else
    #include "lvgl/lvgl.h"
#endif


#ifndef LV_ATTRIBUTE_MEM_ALIGN
#define LV_ATTRIBUTE_MEM_ALIGN
#endif

#ifndef LV_ATTRIBUTE_IMAGE_{UPPER}
#define LV_ATTRIBUTE_IMAGE_{UPPER}
#endif

const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST LV_ATTRIBUTE_IMAGE_{UPPER} uint8_t {name}_map[] = {{
"""

FOOTER = """}};

const lv_image_dsc_t {name} = {{
  .header.cf = LV_COLOR_FORMAT_RGB565,
  .header.magic = LV_IMAGE_HEADER_MAGIC,
  .header.w = {w},
  .header.h = {h},
  .data_size = {px} * 2,
  .data = {name}_map,
}};
"""


def rgb565(r: int, g: int, b: int) -> int:
    """Pack to RGB565, rounding rather than truncating.

    The shipped files round -- 39 becomes 5 in the 5-bit red field, not the 4
    that masking off the low bits gives -- so this matches them.
    """
    r5 = (r * 31 + 127) // 255
    g6 = (g * 63 + 127) // 255
    b5 = (b * 31 + 127) // 255
    return (r5 << 11) | (g6 << 5) | b5


def decode(c_path: str, png_path: str) -> None:
    """Recover a PNG from a generated array.

    Editing a decoded image and re-encoding keeps untouched pixels bit-exact,
    which regenerating from the raw_images source does not: those PNGs are not
    what the shipped arrays were built from.
    """
    import re

    text = open(c_path, encoding="utf-8", errors="replace").read()
    body = text.split("_map[] = {")[1].split("};")[0]
    data = [int(v, 16) for v in re.findall(r"0x([0-9a-fA-F]{2})", body)]

    w = int(re.search(r"\.header\.w = (\d+)", text).group(1))
    h = int(re.search(r"\.header\.h = (\d+)", text).group(1))

    img = Image.new("RGB", (w, h))
    px = img.load()
    i = 0
    for y in range(h):
        for x in range(w):
            v = data[i] | (data[i + 1] << 8)
            i += 2
            r5, g6, b5 = (v >> 11) & 0x1F, (v >> 5) & 0x3F, v & 0x1F
            # replicate the high bits into the low ones, so re-encoding
            # returns the same 5/6-bit values instead of drifting down one
            px[x, y] = (
                (r5 << 3) | (r5 >> 2),
                (g6 << 2) | (g6 >> 4),
                (b5 << 3) | (b5 >> 2),
            )
    img.save(png_path)
    print("%s -> %s (%dx%d)" % (c_path, png_path, w, h))


def convert(png_path: str, c_path: str, per_line: int = 16) -> None:
    name = os.path.splitext(os.path.basename(c_path))[0]
    img = Image.open(png_path).convert("RGB")
    w, h = img.size
    px = img.load()

    out = [HEADER.format(UPPER=name.upper(), name=name)]
    row = []
    for y in range(h):
        for x in range(w):
            v = rgb565(*px[x, y])
            # little endian: low byte first, matching the shipped files
            row.append("0x%02x, 0x%02x, " % (v & 0xFF, (v >> 8) & 0xFF))
            if len(row) == per_line:
                out.append("  " + "".join(row).rstrip() + "\n")
                row = []
    if row:
        out.append("  " + "".join(row).rstrip() + "\n")
    out.append(FOOTER.format(name=name, w=w, h=h, px=w * h))

    with open(c_path, "w", encoding="utf-8", newline="\n") as fh:
        fh.write("".join(out))
    print("%s -> %s (%dx%d, %d bytes of pixel data)" % (png_path, c_path, w, h, w * h * 2))


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("src")
    ap.add_argument("dst")
    ap.add_argument(
        "--decode",
        action="store_true",
        help="recover a PNG from a generated .c instead of encoding one",
    )
    args = ap.parse_args()
    if args.decode:
        decode(args.src, args.dst)
    else:
        convert(args.src, args.dst)


if __name__ == "__main__":
    main()
