# Draws assets/icon.png (256x256) for the Horizon plugin list: a die (random) with a chequered flag corner.
# Run: python assets/make_icon.py   (needs Pillow)
import os
from PIL import Image, ImageDraw

S = 4  # supersampling
N = 256 * S
img = Image.new("RGBA", (N, N), (0, 0, 0, 0))
d = ImageDraw.Draw(img)

bg = (24, 27, 34, 255)
accent = (232, 152, 58, 255)
pip = (24, 27, 34, 255)
white = (240, 240, 240, 255)

d.rounded_rectangle([0, 0, N - 1, N - 1], radius=56 * S, fill=bg)

# die, slightly rotated look via plain square
x0, y0, x1, y1 = 52 * S, 60 * S, 196 * S, 204 * S
d.rounded_rectangle([x0, y0, x1, y1], radius=28 * S, fill=accent)
r = 13 * S
cx = [(x0 + x1) // 2 - 40 * S, (x0 + x1) // 2, (x0 + x1) // 2 + 40 * S]
cy = [(y0 + y1) // 2 - 40 * S, (y0 + y1) // 2, (y0 + y1) // 2 + 40 * S]
for (i, j) in [(0, 0), (2, 0), (1, 1), (0, 2), (2, 2)]:
    d.ellipse([cx[i] - r, cy[j] - r, cx[i] + r, cy[j] + r], fill=pip)

# chequered flag corner, top right
fx, fy, cell = 168 * S, 28 * S, 14 * S
d.rectangle([fx - 3 * S, fy - 3 * S, fx + 4 * cell + 3 * S, fy + 3 * cell + 3 * S], fill=bg)
for row in range(3):
    for col in range(4):
        c = white if (row + col) % 2 == 0 else (60, 64, 74, 255)
        d.rectangle([fx + col * cell, fy + row * cell, fx + (col + 1) * cell - 1, fy + (row + 1) * cell - 1], fill=c)

out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "icon.png")
img.resize((256, 256), Image.LANCZOS).save(out)
print("wrote", out)
