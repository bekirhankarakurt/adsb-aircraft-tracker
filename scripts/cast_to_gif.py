#!/usr/bin/env python3
"""Render an asciinema v2 cast (plain text, no escape sequences) as an animated GIF.

Usage: cast_to_gif.py INPUT.cast OUTPUT.gif   (requires Pillow and DejaVu Sans Mono)
"""
import json
import sys

from PIL import Image, ImageDraw, ImageFont

FONT = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"
SIZE = 15
BG, FG, PROMPT = (22, 27, 34), (201, 209, 217), (126, 231, 135)
PAD = 12
FRAME_STEP = 0.12  # seconds of cast time merged into one GIF frame


def load(path):
    with open(path, encoding="utf-8") as f:
        header = json.loads(f.readline())
        events = [json.loads(line) for line in f if line.strip()]
    return header, [(t, data) for t, kind, data in events if kind == "o"]


def apply(lines, row_col, text):
    row, col = row_col
    for ch in text:
        if ch == "\n":
            row += 1
            col = 0
            lines.append([]) if row >= len(lines) else None
        elif ch == "\r":
            col = 0
        else:
            line = lines[row]
            while len(line) <= col:
                line.append(" ")
            line[col] = ch
            col += 1
    return row, col


def render(lines, rows, cols, font, cw, ch):
    image = Image.new("RGB", (cols * cw + 2 * PAD, rows * ch + 2 * PAD), BG)
    draw = ImageDraw.Draw(image)
    visible = lines[-rows:]
    for r, line in enumerate(visible):
        text = "".join(line)
        colour = PROMPT if text.startswith("$ ") else FG
        draw.text((PAD, PAD + r * ch), text, font=font, fill=colour)
    return image.convert("P", palette=Image.ADAPTIVE, colors=8)


def main():
    header, events = load(sys.argv[1])
    rows, cols = header["height"], header["width"]
    font = ImageFont.truetype(FONT, SIZE)
    cw = round(font.getlength("M"))
    ch = SIZE + 4
    lines, pos = [[]], (0, 0)
    frames, durations = [], []
    last_emit, clock = 0.0, 0.0
    for t, data in events:
        pos = apply(lines, pos, data)
        if t - last_emit >= FRAME_STEP or data.endswith("\n"):
            frames.append(render(lines, rows, cols, font, cw, ch))
            durations.append(max(int((t - clock) * 1000), 20))
            last_emit, clock = t, t
    frames.append(render(lines, rows, cols, font, cw, ch))
    durations.append(3000)
    frames[0].save(sys.argv[2], save_all=True, append_images=frames[1:], duration=durations,
                   loop=0, optimize=True, disposal=1)


if __name__ == "__main__":
    main()
