"""Draw installer/warwind-remaster.ico: a green-and-gold wind swirl on a dark stone tile.

Original artwork, drawn procedurally; no game art is used.
    python make_icon.py            # writes warwind-remaster.ico (and a 256 px .png preview)
"""
import math
import random
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFilter

HERE = Path(__file__).resolve().parent
S = 1024                      # drawing canvas; downsampled for every icon size
SIZES = [16, 24, 32, 48, 64, 128, 256]

GOLD = (222, 178, 74)
GOLD_DARK = (140, 98, 30)
GREEN = (86, 196, 108)
GREEN_DARK = (24, 92, 48)


def stone_tile():
    rnd = random.Random(1997)
    base = Image.new("RGB", (S, S), (38, 40, 44))
    noise = Image.effect_noise((S // 8, S // 8), 40).resize((S, S), Image.BICUBIC).convert("RGB")
    tile = ImageChops.blend(base, ImageChops.multiply(noise, Image.new("RGB", (S, S), (70, 72, 78))), 0.55)
    d = ImageDraw.Draw(tile)
    # a few chisel cracks
    for _ in range(9):
        x, y = rnd.randrange(S), rnd.randrange(S)
        pts = [(x, y)]
        for _ in range(5):
            x += rnd.randint(-70, 70)
            y += rnd.randint(-70, 70)
            pts.append((x, y))
        d.line(pts, fill=(24, 25, 28), width=5)
    tile = tile.filter(ImageFilter.GaussianBlur(1.5))

    # vignette so the rim reads darker than the centre
    vig = Image.new("L", (S, S), 0)
    ImageDraw.Draw(vig).ellipse((-S * 0.25, -S * 0.25, S * 1.25, S * 1.25), fill=255)
    vig = vig.filter(ImageFilter.GaussianBlur(S // 6))
    tile = Image.composite(tile, Image.new("RGB", (S, S), (12, 12, 14)), vig)

    mask = Image.new("L", (S, S), 0)
    ImageDraw.Draw(mask).rounded_rectangle((24, 24, S - 24, S - 24), radius=190, fill=255)
    out = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    out.paste(tile, (0, 0), mask)
    # bevelled gold rim
    ImageDraw.Draw(out).rounded_rectangle((24, 24, S - 24, S - 24), radius=190, outline=GOLD_DARK, width=28)
    ImageDraw.Draw(out).rounded_rectangle((44, 44, S - 44, S - 44), radius=172, outline=(0, 0, 0, 120), width=8)
    return out


def spiral_arm(draw, cx, cy, phase, r0, r1, turns, w0, w1, colour):
    """Thick tapering Archimedean arm from r0 (inside) to r1 (outside)."""
    steps = 420
    for i in range(steps):
        t = i / (steps - 1)
        a = phase + t * turns * 2 * math.pi
        r = r0 + (r1 - r0) * t
        w = w0 + (w1 - w0) * math.sin(math.pi * t) ** 0.8
        x, y = cx + r * math.cos(a), cy + r * math.sin(a)
        draw.ellipse((x - w, y - w, x + w, y + w), fill=colour)


def swirl_layer(colour, width_scale, offset=(0, 0)):
    layer = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    cx, cy = S / 2 + offset[0], S / 2 + offset[1]
    # three wind arms around a calm eye
    for k, (col, turns) in enumerate(zip(colour, (1.05, 0.95, 1.1))):
        spiral_arm(d, cx, cy, k * 2 * math.pi / 3, 70, 380, turns, 10 * width_scale, 46 * width_scale, col)
    return layer


def emblem():
    img = stone_tile()
    shadow = swirl_layer([(0, 0, 0, 170)] * 3, 1.1, offset=(14, 18)).filter(ImageFilter.GaussianBlur(14))
    img = Image.alpha_composite(img, shadow)
    img = Image.alpha_composite(img, swirl_layer([GOLD_DARK, GREEN_DARK, GOLD_DARK], 1.18))
    img = Image.alpha_composite(img, swirl_layer([GOLD, GREEN, GOLD], 1.0))
    # the eye: a small gold rune dot with a green glint
    d = ImageDraw.Draw(img)
    c = S / 2
    d.ellipse((c - 58, c - 58, c + 58, c + 58), fill=GOLD_DARK)
    d.ellipse((c - 44, c - 44, c + 44, c + 44), fill=GOLD)
    d.ellipse((c - 16, c - 30, c + 12, c - 2), fill=(250, 240, 200))
    glow = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    ImageDraw.Draw(glow).ellipse((c - 150, c - 150, c + 150, c + 150), fill=GREEN + (60,))
    img = Image.alpha_composite(img, glow.filter(ImageFilter.GaussianBlur(40)))
    return img


def main():
    img = emblem()
    frames = [img.resize((n, n), Image.LANCZOS) for n in SIZES]
    frames[-1].save(HERE / "warwind-remaster.ico", sizes=[(n, n) for n in SIZES], append_images=frames[:-1])
    frames[-1].save(HERE / "warwind-remaster.png")
    print("wrote", HERE / "warwind-remaster.ico")


if __name__ == "__main__":
    main()
