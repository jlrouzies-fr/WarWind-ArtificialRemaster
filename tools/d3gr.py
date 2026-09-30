"""Decoder for War Wind D3GR graphics resources.

Layout: "D3GR", u32 flags, u32 dataStart, 3*u32 0, u32 frameCount, u32 frameOffset[frameCount]
(relative to dataStart). Frame: u32 size, u32 type, i16 x, i16 y, u16 h, u16 w, pixels (x/y order per DrawSprite 0x4138B0).
Frame type 2 is raw 8-bit indexed pixels, row-major. Palette resources (flags 0x2001)
hold 256 6-bit RGB triplets from offset 0x20.
"""
import struct
import sys
from pathlib import Path

from PIL import Image

from res import res

MAGIC = b"D3GR"


def frames(entry):
    if entry[:4] != MAGIC:
        return []
    start = struct.unpack_from("<I", entry, 8)[0]
    count = struct.unpack_from("<I", entry, 0x18)[0]
    out = []
    for off in struct.unpack_from(f"<{count}I", entry, 0x1C):
        f = start + off
        size, ftype, x, y, h, w = struct.unpack_from("<IIhhHH", entry, f)
        out.append({"type": ftype, "x": x, "y": y, "w": w, "h": h, "pixels": entry[f + 16:f + size]})
    return out


def palette(entry):
    """Palette resource: u16 count (256) at 0x1C, then 6-bit RGB triplets from 0x20."""
    raw = entry[0x20:0x20 + 768]
    return [min(255, v * 255 // 63) for v in raw]


def to_image(frame, pal):
    img = Image.frombytes("P", (frame["w"], frame["h"]), bytes(frame["pixels"][:frame["w"] * frame["h"]]))
    img.putpalette(pal)
    return img


if __name__ == "__main__":
    archive, out = int(sys.argv[1]), Path(sys.argv[2])
    pal_index = int(sys.argv[3]) if len(sys.argv) > 3 else 0
    out.mkdir(parents=True, exist_ok=True)
    pal = palette(res(1)[pal_index])
    a = res(archive)
    for i in range(len(a)):
        for j, fr in enumerate(frames(a[i])):
            if fr["type"] == 2 and fr["w"] * fr["h"] >= 64 * 64 and len(fr["pixels"]) >= fr["w"] * fr["h"]:
                to_image(fr, pal).save(out / f"r{archive}_{i:03d}_{j:02d}_{fr['w']}x{fr['h']}.png")
                if j > 2:
                    break
