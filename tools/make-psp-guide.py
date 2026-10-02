#!/usr/bin/env python3
"""Compose the controls-guide texture from a photograph and a game frame.

docs/artwork/references/psp-1000-evan-amos.png (public domain) is the console;
docs/artwork/psp-guide-gameplay.png, an unretouched 480x272 capture from a
physical PSP, is set into its screen in perspective. The background is a plain
gradient. Nothing here comes from an image model.

Writes ports/extremetuxracer/data/textures/psp-guide.png, 512x256, which the
game shows at 854x480: the photograph is squeezed vertically here by the same
factor the game stretches it. src/controls_guide.cpp holds the label targets
for this exact placement; move one and the other has to follow.
"""
from pathlib import Path
from PIL import Image, ImageDraw

root = Path(__file__).resolve().parent.parent
photo = Image.open(root / 'docs/artwork/references/psp-1000-evan-amos.png').convert('RGBA')
frame = Image.open(root / 'docs/artwork/psp-guide-gameplay.png').convert('RGBA')

# The display's corners in the photograph: top left, top right, bottom
# right, bottom left. Measured from the edges of the lit panel.
screen = [(1051, 229), (3132, 558), (2838, 1784), (737, 1416)]
w, h = frame.size
rows, rhs = [], []
for (x, y), (u, v) in zip(screen, [(0, 0), (w, 0), (w, h), (0, h)]):
    rows.append([x, y, 1, 0, 0, 0, -u * x, -u * y]); rhs.append(u)
    rows.append([0, 0, 0, x, y, 1, -v * x, -v * y]); rhs.append(v)
m = [row + [value] for row, value in zip(rows, rhs)]
for i in range(8):                      # Gauss-Jordan for the eight coefficients
    p = max(range(i, 8), key=lambda r: abs(m[r][i]))
    m[i], m[p] = m[p], m[i]
    m[i] = [value / m[i][i] for value in m[i]]
    for r in range(8):
        if r != i:
            m[r] = [a - m[r][i] * b for a, b in zip(m[r], m[i])]
warped = frame.transform(photo.size, Image.PERSPECTIVE, [row[8] for row in m], Image.BICUBIC)
mask = Image.new('L', photo.size, 0)
ImageDraw.Draw(mask).polygon(screen, fill=255)
console = photo.copy()
console.paste(warped, (0, 0), mask)

texture = Image.new('RGB', (512, 256))
draw = ImageDraw.Draw(texture)
for y in range(256):
    t = y / 255
    draw.line([(0, y), (511, y)], fill=(int(206 + 34 * t), int(226 + 22 * t), int(243 + 9 * t)))
cut = console.crop((85, 47, 3976, 2164)).resize((300, 145), Image.LANCZOS)
texture.paste(cut, (106, 52), cut)
out = root / 'ports/extremetuxracer/data/textures/psp-guide.png'
texture.save(out)
print(out)
