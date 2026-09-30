# War Wind HD: bottom HUD with a visible command grid

## README: files in this folder

> In this repository only the plan itself is kept. The mockup PNGs and `mockups.py` stay in the
> development tree, because every mockup is built from the game's own art.

| File | What it shows |
|---|---|
| `current_layout_y1.png` | Today's hi-res layout for comparison (capture `y1.png`: map 1158x693, right column, duplicated bottom bar). |
| `mockup_a_console.png` | **Recommended.** HUD height 176 (149 body + the existing 27 px status bar). Map view 1280x544. From left: live minimap block with the Active Units / Inn orbs, leader block (portrait, emblem, resources), selection panel (group portrait plus up to 16 unit faces with health bars), control-group plates 1..0 over an info well, and a 5x3 command grid built from the game's own popup icons in carved slots, with the grid letter in the corner. "Attack" is shown highlighted: hover frame, label in the grid header and the status bar. |
| `mockup_a_console_3x.png` | Mockup A with a 3x nearest-neighbour upscale (3840x2160), which is how it looks on a 4K screen. |
| `mockup_b_orbs.png` | Same layout with a single hero selected (capture `x5.png`): portrait, health and mana bars, bio-upgrade diagram, stats. The command grid uses the column's blue orb buttons with light glyphs: a hover ring on Attack, and dimmed orbs for empty slots. |
| `mockup_c_slim.png` | Slim HUD of height 128 (101 body + 27 bar), map view 1280x592. Minimap, a collapsed leader plate, the selection in a single row, and a compact 5x3 grid of 28 px slots. |
| `icons_sheet_3x.png` | All 23 unit-page commands in both button styles (tile and orb) with an example grid letter, 3x. The letters show only where the badge sits, not a real mapping. |
| `mockups.py` | Generator for all of the above (`.venv/Scripts/python.exe patches/WarWind/ui_bottom_bar/mockups.py`). |

All pixels come from the game: panel art `0x800100C1` (RES.001 #193), command icons `0x800100CA` (#202), portraits (#211), the bio diagram (#227), the Tha'Roon small font (#233), the in-game palette (RES.001 #0 x4, which matches the captures exactly), and real 1280x720 captures for the map and the live panel blocks. The only synthetic pieces are the 2 px well bevels (drawn in palette colours sampled from the column's minimap well), the blank orb (the tankard orb with its glyph painted out) and the glyph recolouring on the orbs.

---

## 1. Verdict

**Realistic, and cheaper than it looks. Two facts make it so.**

1. **The command icons already exist.** The game's right-click command popup draws 24x24 icons from resource `0x800100CA` (RES.001 #202). The resource has 150 frames: 75 commands, each as a normal/highlighted pair. The popup routine at `0x44CA14` picks frame `2*(pageBase[page] + button) + hovered` (code at `0x44CC7A..0x44CCBA`; the page base table is at `0x4B6190`). That covers every unit command, building, spell, upgrade and train option. No command icons need drawing; only a Back button and the hotkey badges are new, and both are made from existing pieces.
2. **The panel code is already relocatable.** `hires.cpp` wraps every right-column routine and shifts its draw primitives (DrawImage, FillRect, SetClipRect, SetTextPos, PutPixel) by an offset. The patch generator `gen_hires_patch.py` is a table of view-size immediates. Moving the column's pieces to the bottom generalises the single `+640` offset into a small table of rectangles. It is not a rewrite.

**Effort: about 2 to 3 weeks of focused work (roughly 8 to 12 sessions), in 4 phases.** Phase 1 is the smallest useful step: the grid appears in today's layout. It takes about 2 to 4 sessions and delivers most of the user value on its own (visible commands and letters, one-click orders).

**Hard parts, honestly:**
- The command UI is a **popup state machine**, not a panel. See 2.3: pages, sub-levels, per-unit "popup" flags that travel as network orders, and a pending-target mode. Showing its level 0 permanently and handling sub-pages (Build, Cast spell) as grid pages is the riskiest logic work. Multiplayer lockstep must not see extra orders.
- **No horizontal HUD art exists.** The in-game frame is a vertical 122 px column per race. A bottom HUD has to be composed from its pieces, from the carved textures, and from the briefing-screen console art (RES.004 #0 bottom half, which has a carved slot row and an orb cluster). It needs four race variants (purple Tha'Roon, grey Obblinox, green Eaggra, brown Shama'Li).
- **The selection panel shows one portrait.** A multi-unit "faces" grid (mockup A) is a new feature: walk the selection list, pick a portrait per unit type, add health bars. It is optional (Phase 4).
- **Input.** Every panel hit test is a code immediate. The plan in 3.2 avoids re-patching them one by one, but the mouse remap has to cover both message lParams and `GetMousePos`.

---

## 2. What the game does today (addresses from kb.h, findings_clicks.txt, lin.pkl)

### 2.1 Layout at 640x480 and at 1280x720 today
- Map view 518x453 at (0,0), its own surface `0x542528` created at `0x41F454`, presented by `0x4193D4`. Hi-res: 1158x693 via `feature-hires_720.wwp`.
- Right column x 518..640, drawn from art frame 0 of `0x800100C1+race` (122x453) by four routines, each clipped to a y band:
  - `DrawPanelHeader 0x416FB0`: clip y 0..0x83. Leader name plate, portrait `0x800100D3+race` at (0x213,0x1B), clan emblem, blue bar, resource counter.
  - `DrawMinimapFrame 0x4180FC`: clip y 0x83..0x116. Minimap frame, "Show Active Units" (549,255) and "Show Units in Inn" (612,255) orbs.
  - `DrawMinimap 0x4184C8` / `BuildMinimap 0x4216FC` / `DrawMinimapDots 0x4214C0`: 96x96 at (0x212,0x88). View rectangle via `DrawRectOutline 0x414888`, colour 0x4D. `DrawAltPanel 0x4571F8` replaces the minimap while `0x4B2644` is set.
  - `DrawCommandPanel 0x417280`: clip y 0x116..0x1C5, x 0x200..0x280. Selected-unit name plate, portrait 55x71, health/mana bars, bio-upgrade diagram (`0x800100E3+race`, 96x55) or the group portrait.
  - `DrawBottomBar 0x4171D4`: frame 1 (640x27) at y 453, status text at SetTextPos(0x1E,0x1CC). Text comes from `g_statusText 0x544016` (set by `0x440100`/`0x44013C`).
  - Dispatch: `DrawPanels 0x4181B4` using `g_panelDirtyBits 0x4B264C` (1 header, 2 minimap, 4 command, 8 bottom).
- Frame art pieces in `0x800100C1` (RES.001 #193, same layout for #194..196 = other races): 0 column, 1 bottom bar, 6 small orb (27x27), 7/8 wrench and tankard orbs (53x49 / 49x49), 17..20 their hover states, 10..16 carved texture strips and blocks, 21 dark well 102x176.

### 2.2 Input paths (all immediates; hi-res values are generated by `gen_hires_patch.py`)
- `InGameLButtonPress 0x41EB00` (the orb hit tests 0x213..0x239 / 0x24C..0x272, y 0xED..0x113), `InGameLButtonDown 0x41DB1C` (release: minimap 0x212..0x272 / 0x88..0xE8, `x>0x205 -> PanelClick 0x41CE50`), `InGameRButton 0x41EC88`, `InGameRButtonPress 0x41FCB4` (portrait hold), `InGameMouseTick 0x41AF08` (minimap drag, edge scroll 0x27E / 0x1DE), `UpdateCursorShape 0x41B868`, `DragRectToTiles 0x41BE20`, `PanelHover 0x457F40` (tooltips, many compares, collected automatically by the generator), `0x422F10`, `0x419F3C`.
- The mouse enters through `WndProc 0x414EEC`: button lParams, plus `g_mouseX/Y 0x4C2C25/27` read by `GetMousePos 0x413EA0`. The mod already hooks WndProc (ModernClicks).

### 2.3 Command popup state machine (the part a grid must drive)
- Right click on your own unit sends `SendOrder(dir, id, 0x56, clan, shift)`. When it executes, it selects the unit with the "popup" flag (Thing+0x10 bit 2), calls `OpenCommandPopup 0x44E7CC`, and sets `g_panelMode 0x4B60EE = 2`.
- `OpenCommandPopup` walks the selection, ANDs each unit's command mask from `0x44CE00(state, id)` into `g_panelState[g_panelStateIndex>>16]` (`0x4B60F4`, stride 14: +0 page, +2 mask, +6 highlighted, +8 count), and resets the depth `0x4B6168` to 0. **It is a pure local computation** except for the flag test in `0x44C9B4`, which returns the first selected thing with bit 2 set.
- Drawing: `DrawCommandPopup 0x44CA14`, called from InGameFrame at `0x41AC24` only when `g_panelMode==2`. Icons go in a horizontal row at the unit's screen position (24 px apart); sub-levels are stacked. Hover label via `0x44CD82 -> 0x44013C` (the mod's "Move  [Q]" hook).
- Actions: `PopupClick 0x41D0B8` (left release while the popup is open), `PanelPressButton 0x44FF1C(index, slot)`, `PopupCommandAction 0x44ED10(button, arg)`. They set `g_pendingCmd 0x4B60EC` / `g_pendingCursor 0x4B60F0` / panelMode 5 (target) or 6/>=7 (placement), or send the order immediately. Cancel is at `0x41DD64`, plus `ClosePopupLevel 0x44FED4` and `ResetPanel 0x44C994`.
- Hotkeys: `PanelHotkey 0x47AF28` loops the enabled buttons (eax = index, edx = visible slot). The mod patched the compare at `0x47AF65` to use `GridKeys` by visible slot. Keys currently work only while the popup is open.
- **Side finding (worth checking live in today's build):** `DrawCommandPopup` returns early when the unit's view-relative tile column is `> 0x15` (`0x44CA47 cmp al,15h / ja`). It picks the popup direction with `cmp al,0Ch` (`0x44CA4F`, same test in PanelPressButton at `0x44FF4A`). Neither is in `feature-hires_720.wwp`, so in the 1158 px view the popup may not draw for units right of about x=528. A persistent grid makes this moot, but it should be patched (0x15 -> 0x30, 0x0C -> 0x18) anyway.

---

## 3. Proposed design

### 3.1 Layout (1280x720, x3 = 4K)

Recommended **A: H = 176**. The status bar stays exactly where it is today (y 693..720), so the only new area is the 149 px body at y 544..693.

```
y 0 ┌──────────────────────────── map view 1280 x 544 (full width) ────────────────────────────┐
    │                                                                                          │
544 ├──────┬──────┬────────────────────────────┬───────────────────────┬──────────────────────┤
    │mini- │leader│ selection: name / portrait │ groups 1..0            │ label well           │
    │map   │block │ faces x16 + health bars    │ info well (hover help, │ 5x3 grid: Q W E R T  │
    │orbs  │ res. │ (single unit: bars + bio)  │ costs, queue)          │           A S D F G  │
    │      │      │                            │                        │           Z X C V B  │
693 ├──────┴──────┴────────────────────────────┴───────────────────────┴──────────────────────┤
    │ status bar (frame 1 twice), status text at x=30                                           │
720 └──────────────────────────────────────────────────────────────────────────────────────────┘
 x: 8    136   270                          756                     1062                1268
```

- Minimap block: column y 131..232 (122x101) at (8,552). Orbs block: column y 232..280 at (8,645).
- Leader block: column y 0..131 at (136,554).
- Selection block: column y 278..453 (175 tall) does not fit in 149, so it is split into two bands. The name plate, portrait and bars (y 278..395, 117 tall) go to (270,556); the bio grid (y 395..453) goes beside them, which gives mockup B's arrangement. The band mapper in 3.2 supports this.
- Command grid: 5x3 slots of 34 px (24 px icon + bevel), gap 5, at x=1062, plus a 14 px label well above it.
- **C (H=128)** for players who want more map: map 1280x592 (+48 px of map). The leader block collapses into two text plates, which needs new drawing. The bio grid and the faces row compete for space.
- At 4K the HUD is 528 of 2160 px (24%), which is on par with StarCraft II's console. Today's layout spends 366x2079 on the column plus a bar (about 21%), so the map area stays about the same while its shape becomes 16:6.8 instead of about 5:3.

### 3.2 Engine changes (mod side)

1. **Band mapper (generalises `g_columnMode`/`g_barMode`).** A table of rects `{src rect in column space -> dst origin}`. Column space is today's hi-res column (x 1158..1280, y 0..693). ShiftPoint, AdjustClip, AdjustFill and the SetTextPos thunk map through the table. When DrawImage or FillRect cover more than one band (the column art frame 0 is drawn whole under a band clip), the wrapper issues one call per intersecting band, clipped to that band. The existing panel wrappers keep setting the mode, so only the mapping function changes.
2. **Virtual column off-screen.** Regenerate the hi-res patch with the column origin at x=1280 instead of 1158 (`DX_COLUMN = 762`), so that no real mouse position lands in it. The map boundary tests (`0x205+DX` family, 8 sites) then treat all of x<1280 as map.
3. **Input remap.** In the WndProc hook, a button message whose point falls in a HUD band is translated to column space before the game sees it, and `GetMousePos 0x413EA0` is hooked the same way (for hover and tooltips in PanelHover, and minimap drag in InGameMouseTick). The map-area test gains `y < 544`. Clicks in the HUD outside any band (the carved background) are swallowed. With this, none of the ~40 panel hit-test immediates need new values beyond the `+762` regeneration.
4. **Map view 1280x544** (`gen_hires_patch.py` with `VW=1280, VH=544`). Derived values, following the same rules the 1158x693 table used:
   - visible small tiles: cols ceil(1280/24)=54 (0x36, was 0x31), rows ceil(544/24)+1=24 (0x18, was 0x1E); margins +4/+5 -> 0x3B/0x1D
   - fog grid 54x23 (`0x419449`/`0x419462`)
   - big-tile loops x 0x1B/0x1C, y 0x0C/0x0D
   - scroll clamps x 192-54 = 0x8A (was 0x8F), y 192-23 = 0xA9 (was 0xA3); half-view centring 26x11 (`0x4219C7..`, `0x4474B6..`)
   - minimap view rect 27x12 px and centring offsets (`0x41AF70..`, `0x41AFAB..`); minimap origin constants (`0x4184D1..`, `0x421647`, `0x4216D2`) point to the virtual column, and the band mapper moves the result
   - edge scroll: right edge 0x4FE -> 0x4FE (unchanged, x = 1278); bottom edge `0x41B104` must stay at the **screen** bottom (718) even though the view ends at 544. Otherwise hovering the HUD scrolls the map.
   - the draw-order base 1008 still fits (24 rows x 54 cols), and the draw pools of 6000/4500 nodes are enough (the view is smaller than 1158x693).
   - message-box centring (`kMenuDX/DY` in hires.cpp) re-centres over 1280x544.
5. **Command grid renderer** (new `hudgrid.cpp`), drawn after DrawPanels in InGameFrame (hook near `0x41AC1A`):
   - On a selection change (`g_firstSelected 0x4B2640` / `g_curSelected 0x4B274C` differ from the last frame) compute level 0 **locally**: call `OpenCommandPopup` with a local-only temporary of the bit 2 flag, or replicate its loop over `0x44CE00`. Never send `0x56`. This keeps lockstep network traffic identical to today.
   - Draw the enabled buttons in visible-slot order into 5x3 cells: `DrawImage(0x800100CA, 2*(pageBase[page]+i) + hovered, x, y)`, then the slot bevel (FillRect in palette 66/67/69/70/98) and the letter badge (a FillRect plate plus the font glyph `0x800100E9+4*race` through DrawImage). This is the same glyph path DrawText uses, so it is palette-correct.
   - Click or hotkey on slot n calls the same code PanelHotkey reaches: `PanelPressButton(index, slot)` / `PopupCommandAction`, with `g_panelMode` set to 2 around the call, so target mode (5), placement (6/7) and the orders work unchanged. Sub-pages (Build, Cast spell, Train) replace the grid contents, with **B = Back** (or Esc) in the last slot, like StarCraft.
   - Hotkeys become active whenever the grid shows buttons, not only while the popup is open (InGameKeyDispatch `0x47B400 -> PanelHotkey`).
   - Disable the floating popup (skip the call at `0x41AC24`) when the grid is on. Right-click on your own unit then becomes a plain order or no-op (ModernClicks already routes it).
   - Hover over a slot writes the label into the status bar through the existing label hook (`0x44CD82`), so the text also carries "[W]".
6. **HUD background** is built once per mission per race into an offscreen DirectDraw surface (the same `CreateOffscreenSurface 0x411AA4` path the game uses) and blitted under the bands each frame the panels are dirty.

### 3.3 Art

- **Command icons: reuse `0x800100CA`** (all 75 command icons, 24x24, normal plus highlighted). Mockups A and C show them in the game's own gold-on-purple tile style. For the orb style (B), the glyph is extracted from the highlighted frame and painted light on a blank orb. That works for all 23 unit commands (`icons_sheet_3x.png`), but the orb style needs a hand-cleaned blank orb: the mockup's is an automatic paint-out and shows faint streaks.
- **Hotkey letters:** the race font glyphs (Tha'Roon small green `#233` = `0x800100E9`, Obblinox white `#237`, Eaggra blue script `#241`, Shama'Li orange `#245`) on a 1 px lavender-edged dark plate in the lower-right corner of the slot. This is the same colour language as the name plates. `[Controls] GridKeys` (controls.cpp) stays the single source of truth for the letters.
- **Frame pieces per race:** column frame 0 (bands), bottom bar frame 1, the orbs 6/7/8 plus hover states 17..20, the dialog border tiles `#197..#200` (16x16 corners/edges per race: purple, grey steel, green metal, brown wood), and the carved texture blocks 10..16. The briefing console `RES.004 #0` (bottom 175 rows: carved slot row, text well, 2x2 orb cluster) is the best reference for a horizontal console and can be cut into repeatable slot tiles.
- **New 8-bit art** (only if we want better than composed pieces): a horizontal rim strip per race and a blank orb per race. Either author them as indexed PNGs in the in-game palette and load them from `WarWindHD\hud_<race>.png` (the mod converts them to a surface at mission start), or generate them at runtime from the pieces above, as `mockups.py` does. Keep to the palette; no alpha beyond index 0.
- **Unit faces (Phase 4):** no small portraits exist. Crop 34x40 out of the 55x71 portraits (`0x800100D3+race`, frames per unit type), as mockup A does, which needs a type-to-frame table. Alternatively use frame 0 of each unit's sprite.

---

## 4. Phased implementation

**Phase 1: grid in today's layout (smallest useful step, 2 to 4 sessions).**
Draw the 5x3 grid (24 px icons, 0 px gap, 120x72 plus letter rows) in the right column's extension (column y 453..693), which today is repeated filler art. Implement the local level-0 computation, clicks, hotkeys without the popup, hover labels, the sub-page and Back logic, and popup suppression behind an ini switch (`[UI] CommandGrid=1`). Also fix `0x44CA47`/`0x44CA4F`/`0x44FF4A`. No layout or input-remap risk: the panel clicks are handled by the mod inside a rect it owns.
*Exit test:* sandbox run. Select a worker, and with the grid showing Move/Attack/Stop/Build/..., press W and click an enemy to attack. Press R, then a building key, then place it. Then a spell page. Multiplayer: confirm with the order log that no 0x56/0x57 traffic is added.

**Phase 2: band mapper and virtual column (3 to 4 sessions).**
Generalise ShiftPoint/AdjustClip into the band table and split draws across bands. Move the column origin to 1280 in `gen_hires_patch.py` and remap the mouse in WndProc and GetMousePos. Keep the map at 1158x693 for now: the column blocks are drawn at their new bottom positions while the old column area shows the map's right edge blank. This isolates the drawing and input work from view resizing.
*Exit test:* minimap click/drag, the Active Units / Inn orbs, portrait right-hold, tooltips and the alt panel all work at their new positions.

**Phase 3: map view 1280x544 and HUD background (2 to 3 sessions).**
Regenerate the view table (3.2.4), keep the bottom edge-scroll at the screen edge, build the per-race HUD surface, and move the Phase 1 grid into the HUD. Re-centre message boxes.
*Exit test:* scroll every map edge (clamps 0x8A/0xA9), check that units are culled at the new edges, check the fog, box select near the HUD edge (the drag end clamps at `0x41B824..`), and check all four races.

**Phase 4: polish (optional, 2 to 4 sessions).**
Unit-face selection grid with health bars (click a face to select just that unit, Shift-click to remove it), control-group plates (groups at `0x4B99E0`), the info well (costs and help text from RES.000), the orb-style buttons as an option, and the slim HUD (C) as an ini choice.

---

## 5. Risks and mitigations

| Risk | Mitigation |
|---|---|
| Local popup computation diverges from the real one (flag bit 2 semantics, buildings vs units, mixed selections) | Phase 1 compares the grid's page and mask against the real popup (open the popup and diff `g_panelState`) in a dev command before disabling the popup. |
| Network games: extra orders from the grid | The grid never issues 0x56/0x57; actions go through the same `PopupCommandAction`/SendOrder path as the popup. Check the order stream at `0x489144`. |
| Draws spanning bands (full column art under a band clip) | Split per band in the DrawImage/FillRect wrappers. Unit test with a dev capture per band. |
| A missed input immediate still assumes the column at 1158 | The generator already fails loudly on mismatched immediates. The remap makes the real mouse never reach column space except through the table. |
| Edge-scroll or box-select behaviour over the HUD | Explicit tests in Phase 3. The bottom edge-scroll threshold is kept at the screen edge. |
| Art looks pasted together | Mockups show the composed result is convincing. Budget a pass of hand-authored rim and orb pieces per race if needed. |

Nothing here needs launching the game outside the sandbox. Every phase ends with a sandbox capture compared against these mockups.
