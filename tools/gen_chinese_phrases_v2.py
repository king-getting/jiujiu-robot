# -*- coding: utf-8 -*-
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

PHRASES = [
    "我在呢",
    "加油鸭",
    "早点休息",
    "今天也要开心",
    "晚安",
    "Keep going",
    "Good night",
    "Take a break",
    "Love yourself",
    "你真的很棒",
    "慢慢来也没关系",
    "我一直陪着你",
    "抱抱你",
    "明天会更好",
    "记得喝水",
    "别太累了",
    "相信自己",
    "你是被爱着的",
    "吃顿好的",
    "你已经做得很好了",
]

FONT_PATH = r"C:\Windows\Fonts\simhei.ttf"
FONT_SIZE = 22
GLYPH_W = 24
GLYPH_H = 24
OUT_PATH = Path(r"C:\Users\89640\Documents\智能聊天机器人——啾啾\03_ESP32代码\jiujiu_main\chinese_phrases.h")


def pack_glyph_row(image, y, width):
    data = bytearray()
    byte = 0
    bit_index = 0
    for x in range(width):
        if image.getpixel((x, y)) > 127:
            byte |= 0x80 >> bit_index
        bit_index += 1
        if bit_index == 8:
            data.append(byte)
            byte = 0
            bit_index = 0
    if bit_index:
        data.append(byte)
    return bytes(data)


def render_phrase(phrase):
    width = len(phrase) * GLYPH_W
    image = Image.new("L", (width, GLYPH_H), 0)
    draw = ImageDraw.Draw(image)
    font = ImageFont.truetype(FONT_PATH, FONT_SIZE)
    for index, char in enumerate(phrase):
        x = index * GLYPH_W
        bbox = draw.textbbox((0, 0), char, font=font)
        char_w = bbox[2] - bbox[0]
        char_h = bbox[3] - bbox[1]
        draw.text(
            (x + (GLYPH_W - char_w) / 2 - bbox[0],
             (GLYPH_H - char_h) / 2 - bbox[1]),
            char, font=font, fill=255,
        )
    out = bytearray()
    for y in range(GLYPH_H):
        out += pack_glyph_row(image, y, width)
    return bytes(out)


def main():
    lines = [
        "#ifndef CHINESE_PHRASES_H",
        "#define CHINESE_PHRASES_H",
        "",
        "#include <Arduino.h>",
        "",
    ]
    names = []
    for index, phrase in enumerate(PHRASES):
        name = f"zh_phrase_{index}"
        names.append(name)
        data = render_phrase(phrase)
        width = len(phrase) * GLYPH_W
        lines.append(f"static const uint8_t {name}[] PROGMEM = {{")
        for offset in range(0, len(data), 16):
            chunk = data[offset:offset + 16]
            lines.append("  " + ",".join(str(b) for b in chunk) + ",")
        lines.append("};")
        lines.append(f"#define {name.upper()}_W {width}")
        lines.append("")
    lines.append("#define ZH_PHRASE_COUNT " + str(len(PHRASES)))
    lines.append("")
    lines.append("struct ZhPhrase { const uint8_t* data; uint16_t w; const char* text; };")
    lines.append("static const ZhPhrase zh_phrases[ZH_PHRASE_COUNT] = {")
    for index, name in enumerate(names):
        lines.append(f'  {{ {name}, {name.upper()}_W, "{PHRASES[index]}" }},')
    lines.append("};")
    lines.append("")
    lines.append("#endif")
    OUT_PATH.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print("generated", OUT_PATH, "phrases", len(PHRASES))


if __name__ == "__main__":
    main()