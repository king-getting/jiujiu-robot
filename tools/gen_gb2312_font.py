# -*- coding: utf-8 -*-
from PIL import Image, ImageDraw, ImageFont
from pathlib import Path

FONT_PATH = r"C:\Windows\Fonts\simsun.ttc"
SIZE = 16
OUT = Path(r"C:\Users\89640\Documents\智能聊天机器人——啾啾\03_ESP32代码\jiujiu_main\gb2312_16.h")

chars = []
seen = set()
for hi in range(0xA1, 0xF8):
    for lo in range(0xA1, 0xFF):
        b = bytes([hi, lo])
        try:
            ch = b.decode("gb2312")
        except Exception:
            continue
        if ch and ch not in seen:
            seen.add(ch)
            chars.append(ch)
chars.sort(key=lambda c: ord(c))

font = ImageFont.truetype(FONT_PATH, SIZE, index=0)

def render_glyph(ch):
    im = Image.new("L", (SIZE, SIZE), 0)
    d = ImageDraw.Draw(im)
    bbox = d.textbbox((0, 0), ch, font=font)
    w = bbox[2] - bbox[0]
    h = bbox[3] - bbox[1]
    x = (SIZE - w) // 2 - bbox[0]
    y = (SIZE - h) // 2 - bbox[1]
    d.text((x, y), ch, font=font, fill=255)
    out = bytearray()
    for yy in range(SIZE):
        row = 0
        for xx in range(SIZE):
            if im.getpixel((xx, yy)) > 127:
                row |= 1 << (7 - (xx & 7))
            if (xx & 7) == 7:
                out.append(row)
                row = 0
        # SIZE=16, append happened exactly twice
    return bytes(out)

font_data = bytearray()
codes = []
for idx, ch in enumerate(chars):
    glyph = render_glyph(ch)
    if len(glyph) != 32:
        raise RuntimeError(f"bad glyph {ch} {len(glyph)}")
    font_data += glyph
    codes.append(ord(ch))

lines = [
    "#ifndef GB2312_16_H",
    "#define GB2312_16_H",
    "",
    "#include <Arduino.h>",
    "",
    f"#define GB2312_GLYPH_COUNT {len(chars)}",
    "",
    "static const uint8_t gb2312_font[] PROGMEM = {",
]
for off in range(0, len(font_data), 16):
    chunk = font_data[off:off+16]
    lines.append("  " + ",".join(str(b) for b in chunk) + ",")
lines.append("};")
lines.append("")
lines.append("static const uint16_t gb2312_unicode[] PROGMEM = {")
for off in range(0, len(codes), 12):
    chunk = codes[off:off+12]
    lines.append("  " + ",".join(str(c) for c in chunk) + ",")
lines.append("};")
lines.append("")
lines.append("#endif")
OUT.write_text("\n".join(lines) + "\n", encoding="utf-8")
print("chars", len(chars), "font_bytes", len(font_data), "header", OUT.stat().st_size)