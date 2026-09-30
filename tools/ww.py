"""Quick static helpers for WW.EXE (Watcom, non-ASLR, image base 0x400000)."""
import re
import struct
import sys
from pathlib import Path

import capstone

GAME = Path(__file__).resolve().parents[4]
EXE = GAME / "WW.EXE"


class PE:
    def __init__(self, path=EXE):
        self.data = Path(path).read_bytes()
        d = self.data
        pe = struct.unpack_from("<I", d, 0x3C)[0]
        n = struct.unpack_from("<H", d, pe + 6)[0]
        opt = struct.unpack_from("<H", d, pe + 20)[0]
        self.base = struct.unpack_from("<I", d, pe + 24 + 28)[0]
        self.secs = []
        for i in range(n):
            o = pe + 24 + opt + i * 40
            name = d[o:o + 8].rstrip(b"\0").decode()
            vs, va, rs, rp = struct.unpack_from("<IIII", d, o + 8)
            self.secs.append((name, self.base + va, rp, rs))
        self.md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
        self.md.detail = False

    def off(self, va):
        for _, sva, rp, rs in self.secs:
            if sva <= va < sva + rs and rs:
                return va - sva + rp
        return None

    def read(self, va, n):
        o = self.off(va)
        return self.data[o:o + n]

    def cstr(self, va):
        o = self.off(va)
        if o is None:
            return None
        return self.data[o:self.data.index(b"\0", o)].decode("latin1")

    def code(self):
        name, va, rp, rs = self.secs[0]
        return va, self.data[rp:rp + rs]

    def dis(self, va, n=40):
        out = []
        for ins in self.md.disasm(self.read(va, n * 8), va):
            s = f"{ins.address:08x}: {ins.mnemonic:6s} {ins.op_str}"
            for m in re.findall(r"0x4[bc][0-9a-f]{4}\b", ins.op_str):
                st = self.cstr(int(m, 16))
                if st and len(st) > 2 and all(32 <= ord(c) < 127 for c in st[:20]):
                    s += f'    ; "{st[:50]}"'
            out.append(s)
            n -= 1
            if n <= 0:
                break
        return "\n".join(out)

    def find(self, pat):
        va, c = self.code()
        return [va + m.start() for m in re.finditer(pat, c, re.S)]

    def refs(self, target):
        return self.find(re.escape(struct.pack("<I", target)))

    def calls_to(self, target):
        va, c = self.code()
        res = []
        for m in re.finditer(rb"\xe8", c):
            rel = struct.unpack_from("<i", c, m.start() + 1)[0] if m.start() + 5 <= len(c) else 0
            if va + m.start() + 5 + rel == target:
                res.append(va + m.start())
        return res

    def func_start(self, va):
        """Heuristic: Watcom prologues push regs; scan back for ret/int3 padding."""
        o = self.off(va)
        d = self.data
        for back in range(0, 0x4000):
            p = o - back
            if d[p - 1] in (0xC3,) or (d[p - 3] == 0xC2 and d[p - 1] == 0):
                if d[p] in (0x53, 0x51, 0x52, 0x56, 0x57, 0x55, 0x83, 0x81, 0x68, 0x50, 0xB8, 0x89, 0x8B, 0x31, 0x6A, 0xA1, 0xC7, 0xE8, 0xE9, 0x80, 0x66, 0x0F, 0x85, 0xBA, 0xBB, 0xB9, 0x8A, 0x3B, 0xA0, 0x8D, 0x29, 0x01, 0x9C, 0xF6, 0xFF, 0x39, 0x3D):
                    return va - back
        return None


if __name__ == "__main__":
    p = PE()
    cmd = sys.argv[1]
    if cmd == "dis":
        print(p.dis(int(sys.argv[2], 16), int(sys.argv[3]) if len(sys.argv) > 3 else 40))
    elif cmd == "refs":
        print([hex(x) for x in p.refs(int(sys.argv[2], 16))])
    elif cmd == "calls":
        print([hex(x) for x in p.calls_to(int(sys.argv[2], 16))])
    elif cmd == "str":
        print(p.cstr(int(sys.argv[2], 16)))
