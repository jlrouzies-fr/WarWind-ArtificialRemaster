# Hi-res (1280x720) static map — summary of subagent analysis, 2026-09-26
Full classified lists: `python tools/hires_scan.py screen|ingame`; details in kb.h hi-res section.

- No hardcoded pitch: all drawing is DirectDraw Blt/BltFast or software copies using each Lock's lPitch.
  DrawSprite 0x4138B0, DrawImage 0x413A7C, SurfaceBlt 0x412FE0, FillRect 0x41468C, DrawLine 0x414720 (GDI).
- SetVideoMode 0x414214 derives clip rect (0x4C2120..) and screen rect (0x4C2130) from w/h.
- Map view: own offscreen surface 0x542528, 518x453, created at 0x41F454, presented at (0,0) by 0x4193D4.
  Scroll bytes: bigx 0x5440E3, bigy 0x5440E2, smallx 0x5440E5, smally 0x5440E4. 24px small / 48px big tiles, 192x192 map.
- Layout 640x480: map (0,0)-(518,453); right column x 518..640 (header y0..131, 96x96 minimap at (530,136),
  command panel to 453); bottom bar y 453..480. In-game screen handler 0x41FD58.
- Button rects are code immediates, not DGROUP tables. No screen-sized .bss buffers.

## Proposed patches
- M0: 0x4684D1 `BA E0 01 00 00`->720, 0x4684DB `B8 80 02 00 00`->1280. Menus: either switch mode only while
  0x41FD58 is active (causes a mode change per mission, but cnc-ddraw windowed makes it a surface resize only),
  or add a (320,120) origin offset in DrawSprite/FillRect/SetClipRect 0x414A54/DrawLine/SetTextPos 0x43FD2C/
  PutPixel 0x4149EC, mouse offset in WndProc 0x4151A0/0x415247, main-menu BitBlt 0x468ED1.
- M1: nothing needed.
- M2 (view 1158x693): 0x205/0x206/0x207 +0x280 and 0x1C4/0x1C5/0x1C6 +0xF0 in ~17 map functions (start 0x41F474);
  small-tile ranges +0x16/+0x14 -> 0x31/0x1E; sprite margins 0x1A/0x18 -> 0x35/0x22; big-tile loops 0xC -> 0x1A/0x10;
  fog counts at 0x419449/0x419462 0x16/0x13 -> 0x31/0x1D; scroll limits 0xAA/0xAD/0xAC -> 0x8F/0xA3/0xA2;
  minimap click limits 0x55/0x56 -> 0x47/0x51, re-centre 0x217/0x8D; edge scroll 0x41B044 0x27E->0x4FE, 0x41B104 0x1DE->0x2CE.
- M3: g_uiDX/g_uiDY honoured by the draw primitives; dx=640 around 0x416FB0, 0x4180FC, 0x417280, 0x4184C8,
  0x4571F8, 0x457F40, 0x4214C0; dy=240 around 0x4171D4; minimap rect 0x4184D1..0x4184E0; input x 0x200..0x280 +0x280,
  y 0x1C0..0x1E0 +0xF0 at 0x41AF08, 0x41DB1C, 0x41CE50, 0x41FCB4, 0x41EB00, 0x41EC88, 0x419F3C, 0x457F40, 0x422F10.
  New gaps to fill: bottom bar x 640..1158, right column y 453..693.

## Open conflicts (verify live)
- D3GR frame header: DrawSprite adds frame +8 to x, so +8 is likely x (decoder currently labels it y; offsets are 0 for full-screen art so exports are unaffected).
- Frame art RES.004 #0-3 looked like minimap left / viewport right, but code places the map view at (0,0) and minimap at x=530.
