#!/usr/bin/env python3
"""Genere common/font.h : glyphes 8x16 en niveaux de gris (DejaVu Sans Mono).
Couvre l'ASCII, le Latin-1 et quelques signes francais (oe, guillemets, tirets)."""
import sys
from PIL import Image, ImageDraw, ImageFont

FONT = sys.argv[1] if len(sys.argv) > 1 else "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"
W, H = 8, 16
cps = list(range(32, 127)) + list(range(160, 256)) + [0x152, 0x153, 0x2019, 0x2018, 0x201C, 0x201D, 0x2013, 0x2014, 0x2026, 0x20AC, 0x2022, 0x2190, 0x2191, 0x2192, 0x2193, 0x25A0, 0x25B3, 0x25CB, 0x25A1, 0x2715]

def glyph(cp):
    img = Image.new("L", (W * 4, H * 4), 0)
    d = ImageDraw.Draw(img)
    # rendu a 4x puis reduction : bords lisses
    big = ImageFont.truetype(FONT, 12 * 4)
    d.text((2, 0), chr(cp), fill=255, font=big)
    small = img.resize((W, H), Image.LANCZOS)
    return list(small.get_flattened_data()) if hasattr(small, 'get_flattened_data') else list(small.getdata())

out = ["/* Genere par common/tools/make_font.py : ne pas modifier a la main. */",
       "#ifndef FONT_H", "#define FONT_H", "#include <stdint.h>",
       f"#define FONT_W {W}", f"#define FONT_H_PX {H}", f"#define FONT_COUNT {len(cps)}",
       "static const uint16_t font_cp[FONT_COUNT] = {" + ",".join(str(c) for c in cps) + "};",
       "static const uint8_t font_px[FONT_COUNT][FONT_W * FONT_H_PX] = {"]
for cp in cps:
    out.append("{" + ",".join(str(v) for v in glyph(cp)) + "},")
out += ["};", "#endif", ""]
open("common/font.h", "w").write("\n".join(out))
print(len(cps), "glyphes")
