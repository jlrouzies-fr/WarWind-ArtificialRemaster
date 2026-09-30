"""Reader for War Wind RES.00x archives: u32 count, u32 offset[count], entries back to back."""
import struct
from pathlib import Path

GAME = Path(__file__).resolve().parents[4]
DATA = GAME / "Data"


class ResArchive:
    def __init__(self, path):
        self.path = Path(path)
        self.data = self.path.read_bytes()
        n = struct.unpack_from("<I", self.data, 0)[0]
        self.offsets = list(struct.unpack_from(f"<{n}I", self.data, 4))

    def __len__(self):
        return len(self.offsets)

    def __getitem__(self, i):
        end = self.offsets[i + 1] if i + 1 < len(self.offsets) else len(self.data)
        return self.data[self.offsets[i]:end]


def res(n):
    return ResArchive(DATA / f"RES.{n:03d}")


def strings():
    a = res(0)
    return [a[i].split(b"\0")[0].decode("latin1") for i in range(len(a))]
