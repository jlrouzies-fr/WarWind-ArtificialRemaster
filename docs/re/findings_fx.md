# Modern Graphics findings (2026-09-27/28)

Confirmed statically, and live where noted. Addresses are also in kb.h (map sprite chain, fog/options sections).

## Sprites and art
- Every graphic is in RES.001 (RES.002/003 are sounds). Transparent index 0.
- Groups:
  - #3-12 opaque 48x48 ground tiles; #13-24 colour-keyed borders
  - #25-27 fog circles
  - #28-105 units; #106-163 buildings (#110, #111 building explosions)
  - #164-167 rubble; #168-172 vehicles
  - #175-183 fences, walls, trees and doodads (#178-180 = climate doodad sheets 0x800100B2-B4)
  - #181 and #260 explosions; #255-270 shots, sparks and rings (#264, #265 are gravel and rubble)
  - #271-276 selection and status markers
- Baked shadows are a 1:1 checkerboard of near-black indices (112/114 on trees and buildings, 96 on vehicles), lower-right of the object (sun from the upper-left).
- Palette ramps: fire 8-15, cyan 240-247, blue 88-95, magenta 80-87. No palette cycling, fades or day/night (the only SetEntries call is 0x41186D).

## Draw chain (per mission frame, DrawGameFrame 0x41A4C0)
- Terrain and doodads are drawn into the map surface `*0x542528` by RedrawMapSurface 0x41913C.
  - Hires mode redraws it fully whenever the camera tile origin changes.
  - PresentMapView 0x4193D4 copies it 1:1 into the canvas map view.
- Things are queued into draw lists and rendered by DrawListRender at 0x41A9E6 through the callback 0x4227FC, which calls DrawImage and then DrawSprite 0x4138B0.
- DrawSprite args: eax=res, edx=frame, ebx=x, ecx=y; stack: flags, dst, saveUnder, colourKey (-1 = opaque); `ret 0x10`.
- The final blit is SurfaceBlt 0x412FE0, called at 0x413A52: eax=src, edx=srcRect, ebx=dst, ecx=dstRect, 2 stack args, `ret 8`.
- Fog is DrawShroud 0x419B84, drawn after the sprites: #25-27 index-112 checker circles over the tiles that are not in sight. Setting flag 0x4B33D0 to 0 disables it.
  - Fog bitmaps: g_fogNotVisible 0x545190 and g_explored 0x556410, both 96x96 bits.
  - 0x605DD0 is the pathfinder region map, not fog.
- Projectiles are not things: QueueShotPuffs 0x422148 draws #266-270 puffs. Spell effects are category-7 things.
- Water: small-tile class 30 (g_tileThings >> 11). Class 31 is open ground; 0-11 are trees and deposits.

## Game loop
- InGameFrame 0x41BB84 draws a frame when clock 0x4C29EC >= g_nextFrame 0x4B2744. Flag 0x5440DD = frame drawn, waiting.
- The game only redraws the scene per game frame (about 11 per second at 70%). The 60 Hz presents re-compose the canvas.
- **Bug fixed (timing.cpp):** the precise clock used to rewrite the clock absolutely, so a loaded save (which restores its own clock) froze the mission until real time caught up. It now advances the clock with atomic additions.

## Options screen
- "Change Game Options" is dialog id 6 (0x4B7708), origin X/Y at 0x4B772C/0x4B7730 in the 640x480 layout.
  - In a mission the menu is shown +320,+56 on screen.
- Buttons are added with AddMenuItem 0x46CBA0 (eax=id, edx=left, ebx=top, ecx=right; stack: bottom, sound; `ret 8`).
- DrawMenu is 0x46D014. The release handler dispatches ids <= 0x3E through the jump table at 0x46EFF7; the common exit is 0x46FA81.
- The mod adds ids 0x3F (ON) and 0x40 (OFF) on a row at Y+0x100 (options.cpp). Verified live: the click saves [Graphics] Modern.

## Renderer notes
- The sandbox (hidden desktop) cannot create a D3D9 device: cnc-ddraw falls back to GDI and WWFX_Available is false.
- The `fxdump` dev command forces annotation briefly, so frames can still be dumped and rendered with tools/fxpreview.
- Frame dumps for testing are in captures/fx/*.wwfx.

## Water
- Water is drawn with the teal ramp 176-183 (tile sets #11/#12 and the teal parts of the borders #13/#17/#18). Land tiles use these indices for only a handful of pixels, so "terrain pixel with index 177..183" is a pixel-exact water mask.
- Unexplored ground is drawn greyscale, so water there has no teal indices and is not animated until explored.
- For tests, the whole map can be revealed live by poking 0xFF into g_explored 0x556410 and 0x00 into g_fogNotVisible 0x545190 (both 96 rows x 12 bytes), then forcing a map redraw by jumping the camera away and back.
