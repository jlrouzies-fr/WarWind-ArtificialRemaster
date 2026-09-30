# Widescreen menus: implementation plan

Mockups and notes are in `index.html` in this folder; `mockups.py` regenerates them. All addresses are
in the original WW.EXE. Static facts in this plan were read from the code; everything marked
**(verify)** still needs a live check in the sandbox.

> In this repository only the plan itself is kept. `index.html`, the mockup PNGs, `mockups.py` and
> `wwart.py` stay in the development tree, because every mockup is built from the game's own art.

## Recommended direction

1. **Full-screen menus switch to a 960x540 8-bit mode.** It is 16:9, and it scales 2x on 1080p and 4x on 4K,
   which is the same on-screen size as today's 640x480 menus at 4K.
   - Each screen keeps its 640x480 art and code. The screen's 640x480 coordinate space is placed at a
     per-screen **origin**.
   - A prepared 960x540 **underlay** fills the rest. It is made from that screen's own art (mirrored and
     darkened stone, border tiles, panels) and is in the screen's palette.
2. **Main menu: direction B.** Tablet on the left, a labelled menu column on the right. A is a subset of B
   (the same underlay mechanism and click aliasing, with 4 plaques instead of 9 rows).
3. **Race options menu:** the button column moves onto its own panel; only its item rectangles change.
4. **Briefing:** map and text side by side, and the scenario goal in the empty lower well.
5. **ESC menu:** stays in the 1280x720 mission mode, with a wider 3-column panel. It does not need step 1
   and can be built first.
6. **Victory / defeat:** the picture on the left, the tally in a column on the right.

Total: about 10 to 13 sessions. The ESC menu (about 2) and the race options menu (about 1 after
Phase 1) give the most visible result for the least work.

## Facts the implementation relies on

| What | Where |
|---|---|
| Main menu handler; draw; hit test; hover caption | `0x468BF0`; `0x4686B8` (`DrawImage(0x80040005)` plus `DrawTextCentred(0, 0x1D1, 640)`, the y is at `0x4686F2`/`0x468705`); `0x4685F0`; `0x468638(x, y)` |
| Main menu hotspots | `0x4B6EA6`, 9 x 12 bytes `{u16 l,t,r,b,id,text}`, loop bound `0x6C` (`0x46862A`, `0x4686A7`); caption = RES.000 `0x25E+text` |
| Menus, ESC menu, dialogs | `MenuScreenHandler 0x4713E8`; `g_menuScreen 0x4B6E84` (3 race options, 6 ESC), `g_menuDialog 0x4B7708` (0 base, 3 ideology, 4 sound, 6 game options) |
| Item list | `AddMenuItem 0x46CBA0(id, l, t, r; stack b, sound)`, `MenuItem {u16 l,t,r,b,id; u32 sound; next}`. **The coordinates are unsigned 16-bit, so they must stay >= 0.** |
| Builders | race options `0x470DF0`, ESC `0x471020`, game options `0x46FF30`, sound `0x4705B4`, ideology `0x470CF8` |
| Drawing | `DrawMenu 0x46D014`. For screen 3 it draws the background `0x80040006+race`, then every item from its own rectangle (`0x46DE18..`): the icon `DrawImage(0x8004000E, frame[id])` at (l,t) (frame table: words at `0x4B76C0`), the label at (l+0x2D, t+0xF), the name box `DrawPanel9(#201, l-0x14, t+0x10, 1, 7)`, and the big-orb labels centred at (l-0x17, t+0x37, 0x60). For screen 6 it draws `DrawPanel9(#197+race, [0x4B7728], 0x64, 12, 18)` at `0x46DD6F`, then each label via `DrawTextCentred(l, t+5, r-l)` (`0x46DD95..`). |
| Panel routine | `DrawPanel9 0x46C6CC(eax res, edx x, ebx y, ecx rows, stack cols)`: (cols+2) x (rows+2) tiles of 16 px, random tiles after `srand(0x8000)`. Tile roles are in `wwart.panel9`. |
| Briefing | handler `0x46C06C` (set at `0x46ECBC`, `0x46F09E`, `0x46F1E1`, `0x46F7E6`); frame tick `0x46B4FC`: `[0x4B6F1C] >= 2` -> `0x46ACD4` (map / Hall), else `0x46B23C` (text; calls `0x46A5DC`, `0x46A674`); start mission `0x46B690` |
| Briefing art | RES.004 #0+race: frame 0 background, frames 1..7 maps (600x288 at 20,20), frame 17 cover (559x149 at 0,331), frames 12/13 scroll arrows, frames 15/16 OK/back, frames 18/19 goal toggle; pressed states RES.004 #4 |
| End screen | `DrawGameFrame 0x41A4C0` end branch: `DrawImage(0x80040013+race / 0x8004001B+race)`, palette `0x80040017+race`; `hires.cpp FrameEnter` already switches to 640x480 for it |
| Paths that assume 640x480 | `ScreenCaptureCopy 0x4490A0`; `WM_PAINT` blit of the 640x480 dialog bitmap (`0x468EA4`, `0x5E1D8C`); `EnterDialogScreen 0x46EB58` / `CaptureScreenBitmap 0x414AA4`; `VideoFrameToBack 0x411AEC`; full-screen clears `FillRect(0,0,640,480)` (e.g. `0x468D24`) |

## Phase 1: the 960x540 menu mode (3 to 4 sessions)

The ini switch is `[UI] WideMenus=1`. With 0, today's behaviour stays.

1. **Mode.** In `hires.cpp OnSetScreenHandler`, non-mission handlers get `SetVideoMode(960, 540, 8)`
   instead of 640x480. `0x4713E8` keeps the current mode, as it does today. The end screen's
   `FrameEnter` switch becomes 960x540 too.
2. **Origin per screen.** Generalise `Centered()` / `CentredDX/DY` into `ScreenOrigin(&dx, &dy)`, keyed by
   the handler and `g_menuScreen`:

   | Screen | Origin |
   |---|---|
   | main menu (B) | (16, 30) |
   | main menu (A) | (160, 16) |
   | race options | (16, 30) |
   | briefing | (0, 30) |
   | end screen | (-56, 30) |
   | anything else | (160, 30), centred |

   `AdjustClip`, `ShiftPoint`, `AdjustXY` and the `DrawImage` / `FillRect` / `PutPixel` / `SetTextPos`
   wrappers already apply an offset. Glyphs are drawn with `DrawImage`, so text follows. The clip
   bounds (`g_clip`, `0x4C2130`) are the full 960x540 from `SetVideoMode`.
3. **Underlays.** Hook `DrawImage` for a screen's background call (`0x80040005`,
   `0x80040006+race`, `0x80040000+race` frame 0, `0x80040013/1B+race`, drawn at 0,0).
   - First blit the screen's 960x540 underlay unshifted: an indexed image loaded once into an
     offscreen surface (`CreateOffscreenSurface 0x411AA4` or `IDirectDraw::CreateSurface`, as
     `hudart.cpp` does).
   - Then let the game draw its 640x480 art at the origin.
   - Underlays are derived from game art. `tools/deploy.py` generates them at install time from the
     player's RES files with the `wwart.py` code, and writes them to `WarWindHD\menus\*.png` (8-bit,
     screen palette). Never commit them.
   - About 19 files: main (1), race options (4), briefing (4 races), end screens (8), and a generic
     centred surround for the other screens (menu palette; 1 or 2).
4. **Mouse.** The in-game menu already shifts `lParam` by the menu offset (`input.cpp` line 295). Extend
   that to the menu mode: shift the button message `lParam` values, and hook `GetMousePos 0x413EA0`,
   by -origin. The cursor is drawn by `Flip -> DrawCursor 0x413E30` from `g_mouseX/Y`
   (`0x4C2C25`/`0x4C2C27`), which the game's `WndProc` writes from the `WM_MOUSEMOVE` `lParam`. So
   either keep `WM_MOUSEMOVE` untranslated and translate only in `GetMousePos`, or add +origin in
   `DrawCursor` (reads at `0x413E62`/`0x413E73`). **(verify)** which of `MenuTick`, the hover code and
   the hit tests read `GetMousePos` and which read the message `lParam`.
5. **640x480 assumptions** (see the table above):
   - The captures and `WM_PAINT` bitmaps must use 960x540, or add the origin.
   - `VideoFrameToBack` must write at the origin. Alternatively, switch to 640x480 around
     `PlayVideo 0x411D38` (the opening and earned cinematics play from the menus). Check what
     `video.cpp` already does for the HD videos.
6. **cnc-ddraw.** With `boxing=true`, 960x540 fills 1080p and 4K exactly. On 1440p it is boxed at 2x
   over the backdrop. That is acceptable, or those players can turn boxing off for smooth scaling.

*Exit test (sandbox):*
- Every menu screen shows its art at the origin over the underlay (a generic surround for screens
  without their own).
- Clicks and hover work.
- The cursor is where the mouse is.
- The opening cinematic plays full screen.
- Load and save through `saveload.cpp` work.
- `WideMenus=0` restores today's behaviour.

## Phase 2: race options menu (about 1 session)

- Hook `AddMenuItem` while `g_menuScreen == 3` and map each id to its new rectangle. The table is in
  `campaign_rects.txt` (menu space, screen = +16,+30). Every value is >= 0.
- Name boxes are one tile narrower so both fit side by side:
  - `push 7` -> `push 6` at `0x46DE7C` and `0x46DF15`;
  - `sub edx,0x14` -> `sub edx,0x10` at `0x46DE96` and `0x46DF2F`.
- The underlay holds the art surround and the column panel (`DrawPanel9 #201`, 16 cols x 30 rows at
  screen 672,14); it is baked into the underlay, so no runtime call is needed.
- The sub-dialogs (options, sound, ideology) keep their 640x480 layout inside the origin. They are
  drawn over the menu background; **(verify)** that their panel positions still look centred, or
  give them a centred origin of their own.

## Phase 3: main menu B (2 to 3 sessions; A takes 1 to 2)

- **Underlay:** the stone surround, the #201 frame around the tablet at screen (0,14), and the column
  panel at (672,14).
- **Rows:** after `0x4686B8` the mod draws the headings and rows:
  - RES.004 #14 icons, frames 14, 16, 26, 31, 33 (+1 while pressed);
  - labels in `0x800100F7`, or `0x800100F8` when hot;
  - 4 race icons, 36x36, shrunk from the tablet's glyph rectangles and mapped to the menu palette,
    generated with the underlays.
- **Click aliasing:** a button message whose point lies in row k is forwarded with the centre of
  hotspot row k (table `0x4B6EA6`) as its position. The game's own `0x4685F0` then returns the right
  id. While the cursor is over a row, call `0x468638(centre)` so the caption shows. No change to the
  hotspot table or its loop bounds is needed.
- **Strings:** the new ones (9 short labels, 3 headings) live in the mod. They are English only, like
  the rest of the mod's text.
- **Direction A** needs the same aliasing for its 4 plaques, plus moving the caption y (`0x1D1` ->
  `0x1F7` at `0x4686F2`/`0x468705`) into the strip below the frame.

## Phase 4: briefing (2 to 3 sessions)

- The underlay is the right text panel and the 30 px bands, one file per race.
- **Text:**
  - Find the briefing text call in `0x46B23C` / `0x46A674`. It is probably `0x4403A4` (eax mode,
    edx x, ebx y, ecx lines, stack width, stack text), as used by ideology with width `0x230`.
  - Move it to x 654, y 28, width 272, about 20 lines, in menu space.
  - Keep the line spacing if it is a font property; 18 px in the mockup, 20 today. **(verify)**
- **Map:** draw the map in both modes. Either always take the `0x46ACD4` map path before the text,
  or skip the text page's fill of the map area. **(verify)** what `0x46B23C` fills.
- **Scroll:** the scroll arrows (frames 12/13, pressed states in #4) keep their place in the lower
  well and scroll the right panel. **(verify)** the scroll state variable.
- **Scenario goal:**
  - Print it in the lower well (x 34, y 368 on screen, width 440, `0x800100E7+race`).
  - Get its RES.000 id from the "Review scenario goal" path (ESC menu id `0x11`, handler in the jump
    table `0x46EE1C`) and reuse that lookup.
- **Hall of Heroes mode** (`[0x4B6F1C] >= 2`, slots visible): the right panel keeps the text. A later
  step can show the selected hero's portrait and stats there, since the portraits exist in this
  palette (RES.001 #211+).

## Phase 5: ESC menu in a mission (about 2 sessions; independent of Phase 1)

- **Origin:** in `hires.cpp` the menu origin moves from `kMenuDX` +320 to +160 for this menu, so no
  rectangle is negative. Sub-dialogs (options, sound, the save/load screen) must stay centred: add
  160 to `g_menuPanelX` (`0x4B772C`), set in `MenuInit 0x47130C`, while in a mission.
- **Rectangles:** hook `AddMenuItem` while `g_menuScreen == 6 && g_menuDialog == 0`. The new
  rectangles are in `esc_rects.txt`.
- **Network games** (`0x4B66EC == 1`): the menu has no "Review goal" item and starts at y `0x85`.
  Give it a second table, with the goal box and no Review button.
- **Panel:** patch the `DrawPanel9` arguments at `0x46DD6F..0x46DD7B`:

  | Argument | Today | New |
  |---|---|---|
  | push (cols) | `0x12` | 50 |
  | ecx (rows) | `0xC` | 19 |
  | ebx (y) | `0x64` | 72 |
  | edx (x) | `[0x4B7728]` | 64 |

  Do not change the `[0x4B7728]` value itself: the builders use it for the rectangles.
- **Post-panel hook at `0x46DD8B`** (after the panel, before the label loop). The mod draws:
  - the title (leader and clan, `0x5440F8` / `0x54410D`) in the big race font;
  - the column headings;
  - the raised plates (the `saveload.cpp` `Raised` style; the hovered item uses `g_menuHover`
    `0x4B76FC` and the dark fill), 3 px above each rectangle because the labels are drawn at t+5;
  - the goal well and its text;
  - the orb bar `DrawImage(0x8004000A+race, 6)`.
- **Label loop:** skip ids `0x14` and `0x07`. Their labels are drawn by the mod under the orbs, and the
  orb rectangles are their click areas.
- **Dimming:** after the menu's scene redraw on `0x1401` (`0x41913C`, `0x4216FC`, `0x41A4C0`), remap the
  canvas map area with the 0.4x table from `saveload.cpp` (`PickColours`). Share that code instead of
  copying it.

## Phase 6: victory / defeat (about 1 session)

- 960x540 with origin (-56,30), and 8 underlays (race x result) in the end palettes (#23..#26).
- Find the tally print coordinates in the end branch of `DrawGameFrame` and move them into the right
  column (x about 610 on screen, vertically centred). If they are spread over many immediates, keep
  the text where it is: the widened picture alone is still an improvement.

## Palette rules for anything new

- Draw only with indices of the screen's palette:
  - menus: RES.001 #1;
  - briefing and missions: RES.001 #0;
  - end screens: RES.004 #23..#26.
- Kits do not cross palettes: #14 icons are for menus; #10..#13, #197..#200 and the portraits are for
  missions and the briefing.
- Darkening uses precomputed index tables. The mission palette also has the blend table `0x80010002`
  (`T[112][dst]` is about 60 %).
- There are no smooth gradients, glows or race tints on the olive menu palette.

## Risks

| Risk | Mitigation |
|---|---|
| A hidden 640x480 assumption (captures, fades, `WM_PAINT`, video) shows the wrong region | Phase 1 exit test walks every menu path, including a cinematic, a load and a mission start and end. `WideMenus=0` fallback. |
| Cursor offset after translating the mouse | Translate only in `GetMousePos` and the button messages, or add +origin in `DrawCursor` (Phase 1 step 4). |
| `MenuItem` coordinates are unsigned | Origins are chosen so every rectangle is >= 0 (the tables here respect this). |
| Network game: ESC menu layout differs, and the menu pauses the other players | A separate rectangle table. Draw-only changes, so no new orders are sent. |
| Underlays contain game art | Generate at install from the player's RES files; never commit. |
| The race variants of the art look different (Obblinox steel, Eaggra green, Shama'Li wood) | The generator builds all 4 from their own frames; review a sheet per race before shipping. |
| 1440p shows bars around 960x540 | Documented; boxing off gives smooth scaling. |

## Testing

Never launch the game on the user's desktop. Use `tools/sandbox.py run` (hidden desktop) and
`tools/caps.py` captures, and ask before any visible launch. Another session may be using the
sandbox, so coordinate. Compare each captured screen with its mockup here.
