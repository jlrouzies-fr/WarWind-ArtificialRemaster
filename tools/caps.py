"""Queue dev commands to the running game, then convert every new capture BMP to PNG.

    python caps.py "key 1B" "sleep 1500" "capture m1.bmp" ...
Captures are moved from the game folder to patches/WarWind/captures/ as PNGs.
With no arguments, only converts the capture BMPs already in the game folder.
"""
import io
import sys
import time
from pathlib import Path

from PIL import Image

import deploy

OUT = Path(__file__).resolve().parents[1] / "captures"


def main(cmds):
    OUT.mkdir(exist_ok=True)
    names = [c.split()[1] for c in cmds if c.split()[0] in ("capture", "capcanvas")]
    deploy.queue_commands(cmds)
    wait = sum(int(c.split()[1]) for c in cmds if c.startswith("sleep ")) / 1000 + 1.5 + 0.2 * len(cmds)
    time.sleep(wait)
    convert(names)


def convert(names, timeout=15.0):
    deadline = time.time() + timeout
    for n in names:
        src = deploy.GAME / n
        while not src.exists() and time.time() < deadline:
            time.sleep(0.2)
        if not src.exists():
            print(f"{n}: not produced")
            continue
        png = OUT / (Path(n).stem + ".png")
        data = read_when_complete(src)
        with Image.open(io.BytesIO(data)) as im:
            im.convert("RGB").save(png)
            size = im.size
        for _ in range(20):  # the file may still be held briefly by the game or an indexer
            try:
                src.unlink()
                break
            except PermissionError:
                time.sleep(0.5)
        else:
            print(f"{src}: could not be deleted")
        print(f"{png} {size}")


def read_when_complete(src):
    """The game writes a capture in several calls; read it once its size stops changing."""
    size = -1
    while src.stat().st_size != size:
        size = src.stat().st_size
        time.sleep(0.2)
    return src.read_bytes()


if __name__ == "__main__":
    if len(sys.argv) > 1:
        main(sys.argv[1:])
    else:
        convert([p.name for p in deploy.GAME.glob("*.bmp")])
