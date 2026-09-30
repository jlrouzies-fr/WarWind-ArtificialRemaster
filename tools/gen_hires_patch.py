"""Generate patchsets/feature-hires_720.wwp: in-place immediates that grow the in-game map view and move
input hit-tests to the right column and bottom bar of the in-mission canvas (see mod/src/layout.h).

Every entry names an instruction start address, the immediate it must currently hold and the new
value; the immediate width is taken from the instruction bytes, so the encoding never changes.
Drawing offsets for the panels are applied by hooks in mod/src/hires.cpp, not here.
"""
import pickle
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from ww import PE  # noqa: E402

PROJECT = Path(__file__).resolve().parents[1]
OUT = PROJECT / "patchsets" / "feature-hires_720.wwp"

# Canvas geometry (mod/src/layout.h): the screen's map area is 1280x592 above a 128 px HUD, and
# the canvas map view adds a 72 px margin on every side.
SCREEN_VIEW_W, SCREEN_VIEW_H, MARGIN = 1280, 592, 72
VW, VH = SCREEN_VIEW_W + 2 * MARGIN, SCREEN_VIEW_H + 2 * MARGIN   # map view in the canvas (original 518x453)
DX, DY = VW - 518, VH - 453             # offsets of the right column / status bar
DRAW_NODES, OVERLAY_NODES = 8000, 6000  # must match kDrawNodes / kOverlayNodes in mod/src/hires.cpp
MAP = 192                               # map size in small (24 px) tiles


def ceil_div(a, b):
    return -(-a // b)


COLS = ceil_div(VW, 24)                 # visible small-tile columns (original 22)
ROWS = ceil_div(VH, 24) + 1             # visible rows, one extra for the odd-row offset (original 20)
CULL_COLS, CULL_ROWS = COLS + 4, ROWS + 4   # sprite culling window with a 4-tile margin
BIG_COLS = ceil_div(VW, 48) + 1         # big (48 px) tile loops (original 12)
BIG_ROWS = VH // 48 + 2                 # (original 11)
SCROLL_MAX_X, SCROLL_MAX_Y = MAP - COLS, MAP - (ROWS - 1)
# draw-order key = base - 22*row - col indexes 1024 buckets; rows and columns run from -4 (the
# culling margin above / left of the drawn area) to CULL_ROWS / CULL_COLS
DRAW_KEY_BASE = 1023 - 22 * 4 - 4
assert DRAW_KEY_BASE - 22 * CULL_ROWS - CULL_COLS >= 0


def neg8(v):
    """Displacement byte of a lea [reg - v]."""
    return 0x100 - v

# (address, old immediate, new immediate)
PATCHES = [
    # --- map surface, scroll blits, clip rects, present, sprite passes
    *[(a, 0x206, VW) for a in (0x41F474, 0x41876D, 0x4189DD, 0x418C61, 0x418ECA, 0x418F0A, 0x418FCB,
                                0x419165, 0x419478, 0x419BCE, 0x419FF9, 0x41A290, 0x41A3B8, 0x418717,
                                0x41872A, 0x41A547, 0x41A9BF)],
    *[(a, 0x1C5, VH) for a in (0x41F479, 0x418772, 0x4189E2, 0x418A22, 0x418AE9, 0x418C66, 0x418EC5,
                                0x41916A, 0x41947D, 0x419BDA, 0x4186CD, 0x4186E1, 0x41A52D, 0x41A9C4)],
    *[(a, 0x1C6, VH + 1) for a in (0x419FFE, 0x41A295, 0x41A3BD)],
    *[(a, 0x1C4, VH - 1) for a in (0x4186A8, 0x4186B8)],
    *[(a, 0x205, VW - 1) for a in (0x4186F2, 0x418702)],
    # end-of-game full clears in InGameFrame
    (0x41A81E, 0x1E0, 480 + DY), (0x41A823, 0x280, 640 + DX),
    (0x41A838, 0x1E0, 480 + DY), (0x41A83D, 0x280, 640 + DX),
    # --- visible tile counts (24 px small tiles, original 22x20; fog rows one fewer)
    (0x41860B, 0x16, COLS), (0x418626, 0x14, ROWS),
    (0x419449, 0x16, COLS), (0x419462, 0x13, ROWS - 1),
    (0x41A9F7, 0x14, ROWS), (0x41AA52, 0x1A, CULL_COLS), (0x41AA7C, 0x18, CULL_ROWS),
    # sprite collection
    (0x4196B9, 0x18, CULL_ROWS), (0x419714, 0x1A, CULL_COLS),
    # per-frame culling windows (origin + rows/cols): unit producer caller, tile-thing pass,
    # overlay markers, building/object grid walk, terrain redraw
    (0x42920B, 0x18, CULL_ROWS), (0x429226, 0x1A, CULL_COLS),
    (0x419E4C, 0x18, CULL_ROWS), (0x419E66, 0x1A, CULL_COLS),
    (0x421444, 0x18, CULL_ROWS), (0x421462, 0x1A, CULL_COLS),
    (0x47BBC6, 0x18, CULL_ROWS), (0x47BBB7, 0x1A, CULL_COLS),
    (0x41989D, 0x18, CULL_ROWS), (0x4198BB, 0x1A, CULL_COLS),
    # incremental scroll down / right: first big-tile row / column of the newly exposed strip
    (0x418A62, 0x9, VH // 48), (0x418F4A, 0xA, VW // 48),
    # centre view on a thing (portrait click, messages): half view and scroll clamps
    (0x4219C7, neg8(11), neg8(COLS // 2)), (0x4219D8, neg8(11), neg8(COLS // 2)),
    (0x4219D0, 0xAA, SCROLL_MAX_X), (0x4219E3, 0xAA, SCROLL_MAX_X),
    (0x4219EE, neg8(10), neg8(ROWS // 2)), (0x4219FF, 0xA, ROWS // 2),
    (0x4219F7, 0xAC, MAP - ROWS), (0x421A0A, 0xAC, MAP - ROWS),
    (0x4474B6, 0xB, COLS // 2), (0x4474CC, 0xB, COLS // 2),
    (0x4474BF, 0xAA, SCROLL_MAX_X), (0x4474D7, 0xAA, SCROLL_MAX_X),
    (0x4474E7, 0xA, ROWS // 2), (0x4474FD, 0xA, ROWS // 2),
    (0x4474F0, 0xAC, MAP - ROWS), (0x447508, 0xAC, MAP - ROWS),
    # right-click command popup: skipped when the unit is further than the view into the view,
    # opens to the left from the middle
    (0x44CA47, 0x15, COLS - 1), (0x44CA4F, 0xC, COLS // 2), (0x44FF4A, 0xC, COLS // 2),
    # "is tile on screen" tests (exact view)
    *[(a, 0x16, COLS) for a in (0x45B36C, 0x45B714, 0x45B884, 0x47C23C, 0x47C285)],
    *[(a, 0x14, ROWS) for a in (0x45B390, 0x45B738, 0x45B8A9, 0x47C253, 0x47C2AB)],
    # draw-order key = base - 22*row - col indexes a 1024-bucket list; the old base 600 goes
    # negative (entry dropped) with a larger view
    *[(a, 0x258, DRAW_KEY_BASE) for a in (0x4197C3, 0x420517, 0x42058D, 0x420B1F, 0x47BE45, 0x47BEC4)],
    # draw-list node pools (relocated to larger buffers by hires.cpp): clear counts
    (0x41F3B8, 0x7D0, DRAW_NODES), (0x41A8B2, 0x7D0, DRAW_NODES),
    (0x41F3D2, 0x5DC, OVERLAY_NODES), (0x41A8C6, 0x5DC, OVERLAY_NODES),
    # --- big tile (48 px) loop bounds
    (0x418824, 0xC, BIG_COLS), (0x418AC0, 0xC, BIG_COLS), (0x418AD4, 0xB, BIG_ROWS), (0x418D42, 0xB, BIG_ROWS),
    (0x418FA8, 0xC, BIG_COLS), (0x418FBE, 0xB, BIG_ROWS), (0x4191D0, 0xC, BIG_COLS), (0x4191E8, 0xC, BIG_ROWS + 1),
    (0x419BED, 0xD, BIG_COLS + 1), (0x419C05, 0xC, BIG_ROWS + 1), (0x419C90, 0xD, BIG_COLS + 1), (0x419CB8, 0xD, BIG_COLS + 1),
    # --- scroll clamps: map size - visible tiles
    (0x41F0A9, 0xAD, SCROLL_MAX_Y), (0x41F0AE, 0xAD, SCROLL_MAX_Y), (0x41F16A, 0xAA, SCROLL_MAX_X), (0x41F16F, 0xAA, SCROLL_MAX_X),
    (0x41B052, 0xAA, SCROLL_MAX_X), (0x41B068, 0xAA, SCROLL_MAX_X), (0x41B06D, 0xAA, SCROLL_MAX_X),
    (0x41B113, 0xAD, SCROLL_MAX_Y), (0x41B129, 0xAD, SCROLL_MAX_Y), (0x41B12E, 0xAD, SCROLL_MAX_Y),
    # --- minimap drag / click (InGameMouseTick, RButton): origin +640, centring half-view, clamps
    *[(a, 0x212, 0x212 + DX) for a in (0x41AF3B, 0x41ECE6, 0x41DC63, 0x41DC83, 0x419F5A, 0x419F7C, 0x457FD8, 0x422F5B)],
    *[(a, 0x272, 0x272 + DX) for a in (0x41AF45, 0x41ECF1, 0x41DC6A, 0x419F61, 0x41EBF5)],
    *[(a, 0x217, 0x212 + COLS // 4 + DX) for a in (0x41AF70, 0x41AF87, 0x41ED1D, 0x41ED34)],
    *[(a, 0x8D, 0x88 + ROWS // 4) for a in (0x41AFAB, 0x41AFC1, 0x41ED58, 0x41ED6E)],
    *[(a, 0x55, SCROLL_MAX_X // 2) for a in (0x41AF7B, 0x41AF94, 0x41ED28, 0x41ED41)],
    *[(a, 0x56, SCROLL_MAX_Y // 2) for a in (0x41AFB6, 0x41AFCE, 0x41ED63, 0x41ED7B)],
    (0x41B044, 0x27E, 0x27E + DX), (0x41B104, 0x1DE, 0x1DE + DY),
    # --- panel / map boundary tests (x >= 517 is the panel)
    *[(a, 0x205, 0x205 + DX) for a in (0x41EDB3, 0x41DCA3, 0x419F95, 0x41B881, 0x41BE2E, 0x41BE3A, 0x41BE73, 0x41BE7F)],
    *[(a, 0x1E0, 0x1E0 + DY) for a in (0x41E527, 0x41E52E, 0x41E5C9, 0x41E5D0)],
    (0x47AEED, 0x206, VW), (0x47AEF2, 0x1DF, 0x1DF + DY),
    # --- panel button hit-tests
    (0x41EB6A, 0x213, 0x213 + DX), (0x41EB75, 0x239, 0x239 + DX), (0x41EBEA, 0x24C, 0x24C + DX),
    (0x41CE71, 0x213, 0x213 + DX), (0x41CE7C, 0x249, 0x249 + DX),
    (0x457F59, 0x211, 0x211 + DX), (0x457F64, 0x213, 0x213 + DX), (0x457F6B, 0x273, 0x273 + DX),
    (0x457F8C, 0x213, 0x213 + DX), (0x457F92, 0x249, 0x249 + DX), (0x457FB2, 0x24D, 0x24D + DX),
    (0x457FB9, 0x278, 0x278 + DX), (0x457FDF, 0x273, 0x273 + DX), (0x457FFE, 0x223, 0x223 + DX),
    (0x422F25, 0x204, 0x204 + DX), (0x422F67, 0x200, 0x200 + DX),
    # --- sprite screen rectangles are clamped to the view before hit-testing/drawing (0x4227E9..)
    *[(a, 0x206, VW) for a in (0x422890, 0x422897, 0x4228A8, 0x4228AF, 0x42290A, 0x422911, 0x422922,
                                0x422929, 0x42297A, 0x4229CF, 0x4229D6, 0x4229E7, 0x4229EE, 0x422A49,
                                0x422A50, 0x422A61, 0x422A68, 0x422AB9, 0x422B01, 0x422B08, 0x422B19,
                                0x422B20, 0x422B49, 0x422B50, 0x422B61, 0x422B68, 0x422D88, 0x422DBB)],
    *[(a, 0x1C5, VH) for a in (0x4228C2, 0x4228C9, 0x4228DA, 0x4228E1, 0x42293C, 0x422943, 0x422954,
                                0x42295B, 0x42297F, 0x422986, 0x422997, 0x42299E, 0x422A01, 0x422A08,
                                0x422A19, 0x422A20, 0x422A7B, 0x422A82, 0x422A93, 0x422A9A, 0x422ABE,
                                0x422AC5, 0x422AD6, 0x422ADD, 0x422B2B, 0x422B71, 0x422DAC, 0x422DC0)],
    # --- drag-select box: start allowed while anchor x <= 528 / y <= 480, end clamped to 504 x 480
    (0x41B7F3, 0x210, 0x210 + DX), (0x41B7FA, 0x1E0, 0x1E0 + DY),
    (0x41B824, 0x1F8, 0x1F8 + DX), (0x41B835, 0x1F8, 0x1F8 + DX),
    (0x41B843, 0x1E0, 0x1E0 + DY), (0x41B852, 0x1E0, 0x1E0 + DY),
    # --- panel click ranges in the in-game handler and PanelClick
    (0x41FCD2, 0x213, 0x213 + DX), (0x41FCDD, 0x249, 0x249 + DX),
    (0x41CF2F, 0x213, 0x213 + DX), (0x41CF36, 0x249, 0x249 + DX),
    # --- minimap drawing origin (absolute coordinates fed to blits and pixel plots)
    (0x4184D1, 0x212, 0x212 + DX), (0x418540, 0x211, 0x211 + DX), (0x418545, 0x212, 0x212 + DX),
    (0x41854C, 0x212, 0x212 + DX), (0x418577, 0x21D, 0x21D + DX), (0x41857C, 0x271, 0x271 + DX),
    (0x418583, 0x271, 0x271 + DX), (0x421647, 0x212, 0x212 + DX), (0x4216D2, 0x212, 0x212 + DX),
]


PANEL_HOVER = (0x457F40, 0x4586E5)


def panel_hover_compares(lin, listed):
    """PanelHover hit-tests: x immediates are 0x200..0x280, y immediates are below 0x1C6."""
    extra = []
    for addr, _, mnemonic, ops in lin:
        if PANEL_HOVER[0] <= addr < PANEL_HOVER[1] and mnemonic == "cmp" and addr not in listed:
            value = int(ops.rsplit(",", 1)[1], 16) if ops.rsplit(",", 1)[1].strip().startswith("0x") else None
            if value is not None and 0x200 <= value <= 0x280:
                extra.append((addr, value, value + DX))
    return extra


def encode(pe, sizes, addr, old, new):
    size = sizes[addr]
    raw = bytearray(pe.read(addr, size))
    for width in (4, 2, 1):
        if size >= width and int.from_bytes(raw[size - width:], "little") == old:
            if new >= 1 << (8 * width):
                raise ValueError(f"{addr:#x}: {new:#x} does not fit in {width} byte(s)")
            patched = raw[:size - width] + new.to_bytes(width, "little")
            return bytes(raw), bytes(patched)
    raise ValueError(f"{addr:#x}: immediate {old:#x} not found in {raw.hex(' ')}")


def main():
    pe = PE()
    lin = pickle.load(open(PROJECT / "lin.pkl", "rb"))
    sizes = {a: s for a, s, _, _ in lin}
    patches = PATCHES + panel_hover_compares(lin, {a for a, _, _ in PATCHES})
    seen = set()
    lines = ["# 1280x720 in-game view: generated by tools/gen_hires_patch.py; do not edit by hand"]
    for addr, old, new in patches:
        if addr in seen:
            raise ValueError(f"duplicate entry {addr:#x}")
        seen.add(addr)
        orig, patched = encode(pe, sizes, addr, old, new)
        lines.append(f"verify {addr:X} {orig.hex(' ')}")
        lines.append(f"patch  {addr:X} {patched.hex(' ')}")
    OUT.parent.mkdir(exist_ok=True)
    OUT.write_text("\n".join(lines) + "\n")
    print(f"{len(patches)} patches -> {OUT}")


if __name__ == "__main__":
    main()
