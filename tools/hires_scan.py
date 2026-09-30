"""Generate hi-res dependency tables for findings_hires.md (static; reads lin.pkl + index.db + WW.EXE).

usage: hires_scan.py screen   -> every 640/480/639/479/638/478/320/240/307200/1280 immediate, classified
       hires_scan.py ingame   -> in-game layout constants (x 0x1B0..0x280, minimap/scroll-clamp values)
"""
import bisect
import pickle
import re
import sqlite3
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from ww import PE  # noqa: E402

H = Path(__file__).resolve().parents[1]
lin = pickle.load(open(H / "lin.pkl", "rb"))
db = sqlite3.connect(H / "index.db")
funcs = sorted(db.execute("select address,end_ea from funcs").fetchall())
st = [f[0] for f in funcs]
pe = PE()


def fn(a):
    return funcs[bisect.bisect_right(st, a) - 1][0]


ROLE = {
    0x4102C0: ("FatalMessage: re-enters 640x480 if width<640", "other"),
    0x411D38: ("PlayVideo: switches to 320x240, restores saved g_screenWidth/Height", "video"),
    0x412EA8: ("debug fill of backbuffer rows (320 bytes) - dead code at 0x412F50", "other"),
    0x412FE0: ("stack var [esp+0x140]", "unrelated"),
    0x415820: ("CheckDDCaps if width>=640", "other"),
    0x415A68: ("GameFlowDispatch: FillRect 640x480 + Flip between screens", "menus"),
    0x416EE4: ("panel: DrawImage at x=0x213", "in-game UI"),
    0x416FB0: ("DrawPanelHeader: clip x518..640 y0..131, art at 0x213", "in-game UI"),
    0x417280: ("DrawCommandPanel: clip x512..640 y278..453, buttons x=0x213..", "in-game UI"),
    0x4171D4: ("DrawBottomBar: art frame 1 + status text at (30,460)", "in-game UI"),
    0x4180FC: ("DrawMinimapFrame: clip x518..640 y131..278", "in-game UI"),
    0x4184C8: ("DrawMinimap: blit 96x96 minimap to (530,136) + view rect", "in-game UI"),
    0x4185F0: ("RedrawMapTile(sx,sy) if visible (+22,+20), clamp 517/452", "in-game map"),
    0x418764: ("ScrollMapSurface up: self-blit 518x453 + redraw strip", "in-game map"),
    0x4189D4: ("ScrollMapSurface down", "in-game map"),
    0x418C58: ("ScrollMapSurface left", "in-game map"),
    0x418EBC: ("ScrollMapSurface right", "in-game map"),
    0x41913C: ("RedrawMapSurface (12x12 big tiles) or incremental scroll", "in-game map"),
    0x4193D4: ("PresentMapView: blit map surf 518x453 -> back(0,0); or fog 22x19 FillRects", "in-game map"),
    0x4194C8: ("draw thing (visible test +0x18/+0x1a)", "in-game map"),
    0x419880: ("draw overlay tiles (visible +0x18/+0x1a)", "in-game map"),
    0x419B84: ("RedrawMapObjects layer (clip 518x453)", "in-game map"),
    0x419E3C: ("overlay (visible +0x18/+0x1a)", "in-game map"),
    0x419F3C: ("hover: minimap rect 530..626 x 136..232, x<=517 = map", "in-game UI"),
    0x419FE0: ("DrawSprites pass: clip 518x454", "in-game map"),
    0x41A4C0: ("InGameFrame: map/fog/sprites/panel/minimap; FillRect 640x480 at end-of-game", "in-game map"),
    0x41AF08: ("InGameMouseTick: minimap drag, edge scroll x>638 / y>478", "in-game map"),
    0x41B868: ("UpdateCursor: x>=517 -> panel cursor", "in-game map"),
    0x41BE20: ("DragRectToTiles: clamp x to 517", "in-game map"),
    0x41CE50: ("PanelClick: buttons x 0x213..0x249", "in-game UI"),
    0x41DB1C: ("InGameLButtonDown: minimap click, x>517 -> panel", "in-game map"),
    0x41EB00: ("RButton on panel/minimap", "in-game UI"),
    0x41EC88: ("MinimapRButton / center view", "in-game UI"),
    0x41F00C: ("KeyScroll arrows: clamp 0xAA/0xAD", "in-game map"),
    0x41F380: ("InitGameSurfaces", "in-game map"),
    0x41F454: ("Create/RestoreMapSurfaces: map 518x453, minimap 96x96", "in-game map"),
    0x41F4E8: ("loop bound 480 (table, not screen)", "unrelated"),
    0x41F64C: ("FillRect 640x480", "menus"),
    0x41FCB4: ("InGame panel hit x 0x213..0x249 (+ msg switch)", "in-game UI"),
    0x4214C0: ("minimap unit dots at +0x212,+0x88", "in-game UI"),
    0x42196C: ("CenterViewOnThing: clamp 0xAA/0xAC", "in-game map"),
    0x4226B4: ("overlay clamps to 518x453", "in-game map"),
    0x422F10: ("minimap point test", "in-game UI"),
    0x447058: ("LoadGame: center view clamp 0xAA/0xAC", "in-game map"),
    0x448C30: ("FillRect 640x480", "menus"),
    0x448DF0: ("FillRect 640x480", "menus"),
    0x4490A0: ("ScreenCapture copy 640x480 from primary", "other"),
    0x449170: ("ScreenCapture LBM: 307200 buffer", "other"),
    0x4493F0: ("unrolled row copy (video 2x)", "video"),
    0x44CA14: ("panel page 2 (smallx compare)", "in-game UI"),
    0x457248: ("alt panel (0x4571F8) bar: 479 scale, text at 0x215", "in-game UI"),
    0x4574E8: ("alt panel bar, text at 0x215", "in-game UI"),
    0x457F40: ("PanelHover/tooltips x 0x211..0x273", "in-game UI"),
    0x45AA4C: ("AI/logic visible-area test (+0x16/+0x14)", "in-game map"),
    0x4682E0: ("InitDisplay: SetVideoMode(640,480,8), error text, clear", "menus"),
    0x4686B8: ("menu image", "menus"),
    0x46891C: ("menu screen", "menus"),
    0x46ACD4: ("briefing/menu", "menus"),
    0x46B23C: ("menu", "menus"),
    0x46B4FC: ("menu", "menus"),
    0x46B690: ("menu", "menus"),
    0x46BD3C: ("menu (msg id)", "menus"),
    0x46C018: ("menu", "menus"),
    0x46C508: ("menu", "menus"),
    0x46CC4C: ("menu", "menus"),
    0x46CE90: ("menu text centering (320-w)", "menus"),
    0x46D014: ("menu", "menus"),
    0x46E7B8: ("menu", "menus"),
    0x46EC34: ("menu", "menus"),
    0x46EF18: ("menu", "menus"),
    0x46FF30: ("menu", "menus"),
    0x470DF0: ("menu text clip right=640", "menus"),
    0x471020: ("menu x+320", "menus"),
    0x47130C: ("menu", "menus"),
    0x47174C: ("menu", "menus"),
    0x4718FC: ("menu", "menus"),
    0x471A14: ("menu", "menus"),
    0x471B6C: ("menu", "menus"),
    0x471C68: ("menu", "menus"),
    0x4787C0: ("g_videoFlags=0x140 (flags, not a width)", "unrelated"),
    0x47AEBC: ("SelectAllOnScreen rect 518x479", "in-game map"),
    0x47BB34: ("visible test +0x1a/+0x18", "in-game map"),
    0x47C1B0: ("visible test +0x16/+0x14", "in-game map"),
    0x47C264: ("visible test +0x16/+0x14", "in-game map"),
    0x47EC1C: ("FillRect 640x480", "menus"),
    0x489C20: ("multiplayer menu", "menus"),
}


def cls(a, m, o, v):
    f = fn(a)
    if v == 0x500 and m in ("sub", "lea"):
        return "window-message id (0x500 range)", "unrelated"
    if v == 0xF0 and m == "and":
        return "bit mask", "unrelated"
    if "esp" in o and v in (0xF0, 0x140):
        return "stack offset", "unrelated"
    if f in (0x451970, 0x4547DC):
        return "thing field value 0x280 (game logic)", "unrelated"
    if f in (0x41DB1C, 0x45AA4C) and v == 0x1E0:
        return "arg to sound/logic call (not screen)", "unrelated"
    if f in (0x4863D8, 0x486BA8, 0x48534C):
        return "DirectSound / stack", "unrelated"
    if f in (0x4500B0, 0x490B44):
        return "bit mask", "unrelated"
    return ROLE.get(f, ("?", "?"))


def main():
    mode = sys.argv[1]
    if mode == "screen":
        vals = {640, 480, 639, 479, 638, 478, 320, 240, 307200, 1280}
        for a, s, m, o in lin:
            if a < 0x410000:
                continue
            v = next((int(t, 0) for t in re.findall(r"0x[0-9a-f]+|\b\d+\b", o) if int(t, 0) in vals), None)
            if v is None:
                continue
            role, path = cls(a, m, o, v)
            print(f"| {a:06X} | {fn(a):06X} | `{pe.read(a, s).hex(' ')}` | `{m} {o}` | {v} | {role} | {path} |")
    elif mode == "ingame":
        F = {f for f, (r, p) in ROLE.items() if p in ("in-game UI", "in-game map")}
        extra = {0x83, 0x88, 0x8D, 0xE7, 0xE8, 0x116, 0x12E, 0xAA, 0xAB, 0xAC, 0xAD, 0x55, 0x56}
        for a, s, m, o in lin:
            if a < 0x410000 or fn(a) not in F:
                continue
            if "esp" in o and m in ("add", "sub"):
                continue
            mm = re.search(r"(?:, |^)(0x[0-9a-f]+)$", o)
            if not mm:
                continue
            v = int(mm.group(1), 16)
            if 0x1B0 <= v <= 0x280 or v in extra:
                print(f"| {a:06X} | {fn(a):06X} | `{pe.read(a, s).hex(' ')}` | `{m} {o}` | {ROLE[fn(a)][0]} |")


if __name__ == "__main__":
    main()
