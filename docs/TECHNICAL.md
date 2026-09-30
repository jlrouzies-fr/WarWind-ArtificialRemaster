# War Wind - Artificial Remaster: technical reference

The [README](../README.md) is the short tour. This page has the details: every feature, how the
mod is put together, every setting, the installer switches, and how to build it.

## Features in detail

- **HD cutscenes.** All 63 cutscenes, upscaled with AI from 320x240 at 15 fps to 1080p
  (1920x1080) at 30 fps, and played in place of the original AVIs. A second copy of each
  cutscene has the in-game captions burned in, and it is used when captions are on.
- **1280x720 missions with a new layout.** Missions run at 1280x720. The map fills the full
  width (1280x592) over a 128-pixel bottom HUD built from the game's own panels: leader
  portrait, minimap, orbs, selected unit and status line. Units are drawn over the whole view
  (the original culling windows are widened to match), and the minimap's view rectangle matches
  the 16:9 view.
- **New bottom HUD.** The HUD sits on a carved background in the colours of your race. It shows
  the selected units as faces with health bars (click a face to select only that unit,
  Shift-click to remove it from the selection), the control groups 1..0 as plates with their
  member counts (click a plate to recall the group), and an info well with the hover and status
  text, word-wrapped. With no message to show, the info well summarises the selection by unit
  type (for example "3 Executioner, 2 Rogue").
- **Command grid (`[UI] CommandGrid`).** The HUD shows the selection's commands as a 5x3 grid
  with the game's own command icons, each labelled with its grid hotkey. Click a slot or press
  its key; the grid replaces the command popup over the unit.
- **Smooth camera at 60 fps.** The camera scrolls pixel by pixel instead of in 24-pixel tiles,
  with arrow keys, screen edges, the mouse wheel (Shift+wheel sideways) and middle-button
  drag, and it is presented at 60 fps (`PresentHz`) between the game's 16 frames per second.
  A minimap click moves the camera there at once; "centre on unit" glides to its target.
  Restarting a mission puts the camera back at the mission's start position. `ScrollSpeed`,
  `EdgeScroll`, `WheelPixels` and `PresentHz` tune it. The box-selection rectangle and the
  building placement footprint are redrawn from the live mouse position at the same rate, so
  they follow the mouse at 60 fps instead of the game's frame rate.
- **Smooth unit motion (`[Video] SmoothMotion`).** Units glide across the ground between the
  game's frames instead of stepping from one position to the next. Every frame shown between two
  game ticks redraws the tick's sprites with each unit placed between where it was drawn on the
  previous tick and where it is drawn now, so selection boxes, health bars and click targets move
  with it. The simulation, unit speeds and animation timing are unchanged.
- **Modern Graphics (`[Graphics] Modern`, F9).** An optional effect pass for the Direct3D 9
  renderer: soft shadows in place of the art's checkerboard ones, contact shading around
  buildings, trees and units, light and glow from fire, explosions and magic, animated water
  (waves anchored to the map, sun glints and caustics, foam along the shores), a soft fog of war,
  and a colour grade (contrast, saturation, vignette, sharpening). F9 (`ToggleKey`) switches it
  off and on during a mission, and "Change Game Options" has a new "Modern Graphics OFF / ON"
  row. Every effect has a strength in the `[Graphics]` section. It is drawn by this project's
  cnc-ddraw build, from `WarWindHD\shaders\wwfx.hlsl`.
- **In-game save and load screen (`[UI] InGameSaveLoad`).** "Save Current Game" and "Load a
  Saved Game" open a screen in the game's own style that lists the saves in `Saves\`, instead
  of the Windows file dialogs: type a name to save (with an overwrite prompt), pick or
  double-click a save to load, Del deletes it (after a confirmation), Esc cancels.
- **Widescreen menus (`[UI] WideMenus`).** The main menu, the race options, the mission briefing
  and the victory/defeat screen are laid out for 16:9. They run in a 960x540 mode, which scales by
  exactly 2x on a 1080p display and 4x on 4K:
  - **Main menu:** the tablet stays on the left, and a column beside it lists the menu as
    labelled rows (the four races' campaigns, load campaign, custom and multiplayer, the opening
    cinematic, credits, quit). A row does exactly what its glyph on the tablet does, hover
    caption included.
  - **Race options:** the race's stone is shown whole, with the menu's buttons on a carved panel
    beside it.
  - **Briefing:** the mission map and the full briefing text side by side on every page (the
    text scrolls in its own panel), with the scenario goal in the lower well, which the original
    leaves empty. Built-in scenarios, which have no map, keep the stone fill there.
  - **Victory/defeat:** the picture on the left, continued into the width, with the tally that
    the original prints over the picture moved into a framed box beside it.
  - **In-game ESC menu:** in a 1280x720 mission it is a wide panel over the dimmed map: the
    leader and clan as its title, the game's buttons in two columns (game and options) with the
    scenario goal between them, and the race's orb buttons for Return to Game and Return to Main
    Menu.

  Any other menu page shown in the wide mode is centred in a carved stone frame. The surrounds
  are built at runtime from the game's own art (each screen's picture mirrored and shaded
  outwards, the game's carved frames and panels), so nothing derived from the game's art is
  stored, on disk or in this repository. The game's own menu code still draws and runs every
  screen: the mod places what it draws and passes the mouse on in the game's coordinates.
  `WideMenus=0` brings back the original screens.
- **Centred menus in missions.** The mission message boxes (and the original ESC menu, with
  `WideMenus=0`) are centred over the 1280x720 view, and their clicks and hover still land where
  they are drawn.
- **Other 640x480 screens on a stone backdrop.** Screens without a wide layout (and every menu
  with `WideMenus=0`) are shown boxed at their original size with integer scaling. The bars
  around them show the game's carved-stone art, AI-upscaled to 4K, instead of black.
- **Accurate game clock (`PreciseClock`).** The game was designed to advance a frame and a game
  tick every 62 ms (16 per second), but its clock moves in 20 ms sleep steps, which stretches
  each frame to 80 ms or more (about 12 per second). The mod drives the clock from the system
  timer, so the game runs at a steady pace. **`GameSpeedPercent` sets that pace**: 100 is the
  speed the game was designed for, and the default, 70, is close to how the unmodified game
  plays on current Windows. The in-game Game Speed slider still applies on top.
- **Modern RTS clicks (`ModernClicks`).** Right click moves and attacks with the selection.
  Right click on one of your own units moves the selection to it, as a click on the ground
  there would; with nothing to order, it keeps the original behaviour (the unit is selected
  with its command menu). Left click only selects, and left click on empty ground deselects. On the minimap, left click or drag moves
  the camera and right click sends the selection there.
- **Grid hotkeys.** Command buttons answer to the key at their on-screen slot
  (`Q W E R T` / `A S D F G` / `Z X C V B`, configurable) instead of each command's letter.
- **Ctrl+digit control groups.** Ctrl+1..0 assigns a group. Shift+digit still works, and the
  digit alone recalls the group.
- **Double-click selection.** Double-clicking a unit selects every visible unit of that type.
- **Fixes.** The colours no longer break after a Windows dialog (the multiplayer wizard, save
  and load). A crash in the game's sprite draw lists (at `0x4443D3`) is fixed. A crash when
  music starts or stops while the timer thread streams it (a race on the music buffer) is
  fixed. A loaded save no longer freezes with `PreciseClock` on: the clock now advances by
  additions, so the time a save restores stays the base of the game's frame deadline. Holding
  the left button down on the HUD no longer draws a selection box on the map. The game's list of
  clickable sprites (256 entries, filled without a bound) can no longer overflow when the wider
  view shows more sprites than that.

## How it works

| Piece | What it is |
| --- | --- |
| `dsound.dll` | The mod. It is a proxy DLL: Windows loads it from the game folder in place of the system `dsound.dll`, and it forwards all 12 DirectSound exports to the real `SysWOW64\dsound.dll`. Once loaded into `WW.EXE`, it checks the build (`BB 08 00 00 00` at VA `0x4684C7`), applies the `WarWindHD\*.wwp` byte patches in memory, and installs its hooks. The mod has to be `dsound.dll`: the compatibility shims Windows applies to `WW.EXE` load `winmm`, `ddraw`, `comctl32` and `comdlg32` from System32 before the game's own imports resolve, so a proxy with any of those names in the game folder is ignored. |
| `WarWindHD.ini` | Feature switches (see below). |
| `WarWindHD\feature-hires_720.wwp` | A generated list of `verify`/`patch` lines (321 patches), applied in memory. It changes immediate operands to lay out the full-width map view, widen the unit culling windows and move input hit-tests for the 1280x720 screen. |
| `WarWindHD\fix-gdi-palette.wwp` | Two patches that keep the game colours after a Windows dialog. Before a dialog the game realizes a GDI palette on its window, which cnc-ddraw copies into the DirectDraw palette shifted by 10 entries; the patch skips those realize calls. |
| `Data\VIDS_HD\<RACE>\<NAME>.mp4` | The remastered cutscenes (`<NAME>_sub.mp4` when captions are on). A cutscene without an HD file plays its original AVI. |
| `ddraw.dll` + `ddraw.ini` | **Required.** [cnc-ddraw](https://github.com/FunkyFr3sh/cnc-ddraw), a DirectDraw replacement by FunkyFr3sh (MIT licence), built with [two patches of ours](../third_party/cnc-ddraw/): one adds a `backdrop=` image drawn around boxed screens instead of black bars, the other adds the Direct3D 9 effect passes behind Modern Graphics (exports `WWFX_*` that the mod calls). The `ddraw.dll` that ships with the Steam version is a Direct3D 9 wrapper that changes the display mode. The profile used here gives a borderless window over the whole screen with integer scaling. It never changes the display mode, and it reports Windows 95 to the game, which skips the "Windows NT is not supported" dialog. |
| `WarWindHD\backdrop.png` | The 3840x2160 stone backdrop that cnc-ddraw draws around boxed screens: the 640x480 screens, and the wide menus on a display they do not fill exactly (`backdrop=WarWindHD\backdrop.png` in `ddraw.ini`). It is the same backdrop the HD cutscenes sit on. |
| `WarWindHD\shaders\wwfx.hlsl` | The Modern Graphics shaders. cnc-ddraw compiles them at start-up with Windows' `d3dcompiler_47.dll`; the mod tells it, per frame, which pixels are units, buildings, trees, shadows and fog, where the lights are, and which world position the view shows (the shader finds water by its palette colours). |

## Installer

The installer finds the Steam install through the registry and `libraryfolders.vdf`. You can
also pass the folder yourself, as in `.\Install-WarWindRemaster.ps1 "D:\Games\War Wind"`. It then:

- checks `WW.EXE`, which it only reads;
- backs up `ddraw.dll`, `ddraw.ini`, `dsound.dll`, `WarWindHD.ini` and the files in `WarWindHD\` to
  `<game>\_ArtificialRemaster_backup\` along with a manifest;
- installs this project's cnc-ddraw build as `ddraw.dll` and writes the War Wind profile into
  `ddraw.ini` (other keys already in a cnc-ddraw `ddraw.ini` are kept). If that build cannot be
  downloaded, it falls back to the official cnc-ddraw release, and the 640x480 screens get
  black bars instead of the backdrop;
- installs the mod files, `WarWindHD\backdrop.png` and `WarWindHD\shaders\wwfx.hlsl` from this
  project's release;
- downloads and unpacks the cutscene archives (about 740 MB) into `Data\VIDS_HD`, showing progress.

Downloads are cached in `%LOCALAPPDATA%\WarWind-ArtificialRemaster\downloads`, and an
interrupted download resumes where it stopped.

Useful switches:

| Switch | Effect |
| --- | --- |
| `-SkipCutscenes` | Mod and cnc-ddraw only; the original cutscenes play. |
| `-Disable PreciseClock,WheelScroll` | Switch features off (`GridHotkeys`, `CtrlGroups`, `EdgeScroll`, `WheelScroll`, `MiddleDrag`, `DoubleClickType`, `ModernClicks`, `HiRes`, `PreciseClock`, `SmoothMotion`, `CommandGrid`, `InGameSaveLoad`, `WideMenus`, `Modern`, `HDVideos`). |
| `-LocalFiles <folder>` | Use files you already downloaded (release assets, `cnc-ddraw.zip`). |
| `-Tag v1.0.0-beta` | Install a specific release. |
| `-Force` | Re-unpack the cutscenes, rewrite `ddraw.ini` from the profile alone, and reset `WarWindHD.ini` to its defaults. |

### Uninstall

```powershell
.\Install-WarWindRemaster.ps1 -Uninstall            # asks before deleting Data\VIDS_HD
.\Install-WarWindRemaster.ps1 -Uninstall -RemoveCutscenes
```

Uninstalling restores every file from `_ArtificialRemaster_backup\` and removes what the
install added. A manual uninstall works too: delete `dsound.dll`, `WarWindHD.ini`, `WarWindHD\` and
`Data\VIDS_HD\`, then put the original `ddraw.dll` (and `ddraw.ini`, if there was one) back from
the backup folder.

## Settings (`WarWindHD.ini`)

```ini
[Mod]
PatchDir=WarWindHD     ; folder (inside the game folder) holding the *.wwp byte-patch files
[Controls]
GridHotkeys=1          ; command buttons use the key at their on-screen slot
GridKeys=QWERTASDFGZXCVB  ; keys for slots 0,1,2,... (left to right, top to bottom)
CtrlGroups=1           ; Ctrl+digit assigns a control group (Shift+digit still works)
ScrollSpeed=0          ; camera pixels per second for keys and edges, up to 1800
                       ; (0 = from the game's Screen Scroll Speed)
EdgeScroll=1           ; screen edges scroll the map
WheelScroll=1          ; wheel scrolls the map, Shift+wheel sideways
WheelPixels=96         ; pixels scrolled per wheel notch (replaces WheelSteps)
MiddleDrag=1           ; middle-button drag pans the map
DoubleClickType=1      ; double-click selects all visible units of that type
ModernClicks=1         ; right click moves/attacks (on an own unit: moves the selection to it),
                       ; left click only selects, left click on empty ground deselects;
                       ; minimap: left click/drag moves the camera, right click sends the selection
[Video]
HiRes=1                ; missions at 1280x720: full-width map over a bottom HUD (menus: WideMenus)
PreciseClock=1         ; game ticks every 62 ms as designed (16/s) instead of 80 ms or more (12/s)
GameSpeedPercent=70    ; pace of that clock: 100 = designed speed, 70 = about the unmodified game
                       ; on current Windows (the in-game Game Speed slider applies on top)
PresentHz=60           ; frames per second shown between the game's own frames
                       ; (camera, units, selection box; 0 = off)
SmoothMotion=1         ; units glide between game frames at PresentHz instead of stepping
                       ; (their speed and animation timing do not change)
[UI]
CommandGrid=1          ; bottom HUD shows the selection's commands as a 5x3 hotkey grid
InGameSaveLoad=1       ; save/load in an in-game screen listing Saves\ instead of the Windows dialogs
WideMenus=1            ; widescreen menus: main menu, race options, briefing and victory/defeat
                       ; in a 960x540 mode (2x on 1080p, 4x on 4K) around their original art,
                       ; and the in-game (ESC) menu as a wide panel with the scenario goal
                       ; (0 = the original screens)
[Graphics]
Modern=1               ; Modern Graphics (needs ddraw.ini renderer=auto or d3d9; off under GDI)
ToggleKey=0x78         ; virtual-key code that switches it in a mission (0x78 = F9)
ShadowOpacity=0.5      ; darkness under full shadow (0..1)
ShadowSoftness=3       ; shadow blur, pixels
SunAngle=35            ; unit shadow direction, degrees below the horizontal, towards the right
ShadowLength=5         ; how far unit shadows fall, pixels
AmbientOcclusion=0.45  ; contact shading around buildings, trees and units (0..1)
AmbientOcclusionRadius=10
FogOpacity=0.45        ; fog of war darkness (0..1)
WaterWaves=0.8         ; water wave distortion, pixels (0 = off)
WaterGlints=1.0        ; sun glints and caustics on water (0 = off, 1 = default)
WaterFoam=1.0          ; foam along shores (0 = off, 1 = default)
Ambient=0.92           ; scene brightness without lights (1 = original)
LightIntensity=1.0     ; light from fire, explosions and magic
GlowRadius=5
Bloom=0.25
LightRadius=0.5        ; light radius scale
Contrast=1.06          ; colour grade: 1 = unchanged
Saturation=1.08
Vignette=0.25          ; 0 = none
Sharpen=0.2            ; 0 = none
[Cutscenes]
HDVideos=1             ; play Data\VIDS_HD remasters when present
[Dev]
Commands=0             ; developer command channel (WarWindHD.cmd), used by the test harness
```

`WheelSteps` is gone: the wheel now scrolls by `WheelPixels`. An old `WheelSteps` line is
ignored (the installer drops it when it merges your settings).

The mod writes `WarWindHD.log` next to `WW.EXE`.

## Building

Requirements: Visual Studio Build Tools (MSVC, x86 target) and Python 3 with `pefile`.

```bat
cd src\mod
python gen_exports.py   :: regenerate src\dsound.def + src\proxy_stubs.inc from SysWOW64\dsound.dll (only when needed)
build.cmd               :: -> src\mod\build\dsound.dll
```

`build.cmd` expects `vcvarsall.bat` at the VS 2026 Build Tools path. Edit the first line if yours
is elsewhere. The mod includes `wwfx.h` from the patched cnc-ddraw tree: set `CNC_DDRAW` to that
clone (by default `build.cmd` looks for `<War Wind>\cnc-ddraw` next to this repository). `src/mod/test/` holds a small proxy-forwarding test (`build_test.cmd`).

`src/patchsets/feature-hires_720.wwp` is generated by `tools/gen_hires_patch.py` and should not
be edited by hand. `src/patchsets/fix-gdi-palette.wwp` is written by hand.

For the cnc-ddraw `ddraw.dll`, see [`third_party/cnc-ddraw/`](../third_party/cnc-ddraw/): the two
patches, the upstream commit they apply to, and the build steps.

To make the release assets (mod files, `ddraw.dll`, `backdrop.png`, `wwfx.hlsl`, the installer, the
third-party licences and `SHA256SUMS.txt`, plus the cutscene zips unless `--no-cutscenes`):

```bat
python tools\package_release.py --dll src\mod\build\dsound.dll --ddraw <cnc-ddraw>\bin\Release\ddraw.dll ^
    --vids-hd "<War Wind>\Data\VIDS_HD" --out dist
```

### Repository layout

```
src/mod/            mod DLL sources (C++17), build.cmd, gen_exports.py, WarWindHD.ini,
                    shaders/wwfx.hlsl (Modern Graphics)
src/patchsets/      generated byte-patch files (*.wwp)
assets/             backdrop.png (AI-upscaled stone art, installed as WarWindHD\backdrop.png)
third_party/        cnc-ddraw: our backdrop and Modern Graphics patches, rebuild notes, and the licences
installer/          Install-WarWindRemaster.ps1, its icon, and make_icon.py (draws the icon)
tools/              reverse-engineering, video, packaging and test tools (Python / PowerShell)
docs/re/            reverse-engineering notes: addresses, structures, calling conventions, input
                    and click handling, the HUD (portraits, health bars, control groups), the
                    plan for the bottom HUD with its command grid, the in-game save/load screen,
                    the Modern Graphics findings, smooth motion, the widescreen menu plan, and
                    the briefing screen
```

### About `tools/`

These are the scripts used to make the mod. Most of them locate the game and the work files
relative to their own position, and they expect the development layout they were written in:

```
<War Wind>\Vibe-Reverse-Engineering\patches\WarWind\tools\    (these scripts)
<War Wind>\Vibe-Reverse-Engineering\patches\WarWind\mod\      (= src/mod)
<War Wind>\Vibe-Reverse-Engineering\patches\WarWind\patchsets\ (= src/patchsets)
<War Wind>\cnc-ddraw\                                          (cnc-ddraw clone with the patches applied)
```

To use them, copy them into that layout. The exception is `package_release.py`, which runs
from this repository. `tools/bin/` (ffmpeg, Real-ESRGAN,
RIFE) is downloaded separately and never committed. The Python dependencies are `pefile`,
`capstone`, `Pillow` and `tqdm` (plus `pycaw` for `sandbox.py`). `ctx.py`, `hires_scan.py` and
`scan_imm.py` also need a local Ghidra export (`lin.pkl`, `index.db`), which is not distributed.

| Tool | Purpose |
| --- | --- |
| `video_pipeline.py`, `render_all.cmd` | Cutscene remaster pipeline (see below). |
| `cap2ass.py` | `Data\VIDS.CAP` caption table to ASS subtitles. |
| `res.py`, `d3gr.py` | Readers for the `RES.00x` archives and D3GR graphics. |
| `ww.py`, `gen_hires_patch.py`, `hires_scan.py`, `scan_imm.py`, `ctx.py` | Static analysis helpers for `WW.EXE` (screen-size immediates, disassembly context), and the generator for the 720p patch set. |
| `deploy.py` | Developer install/uninstall of a local build (mod, patch sets, shaders, backdrop and the cnc-ddraw build). |
| `sandbox.py`, `hidden_windows.py`, `caps.py`, `shot.ps1` | Test harness that runs the game on a hidden desktop with a display-mode watchdog, and captures frames through the mod's developer command channel. |
| `ddraw.user.ini`, `ddraw.dev.ini`, `ddraw.dev.d3d9.ini` | cnc-ddraw profiles for players and for the test harness (GDI, or Direct3D 9 with `sandbox.py run --ini`). |
| `fxpreview/` | Renders a Modern Graphics frame dump (dev command `fxdump`) through the same pass chain offline, to a PNG. Built against the patched cnc-ddraw (`build.cmd`). |
| `package_release.py` | Builds the release assets (see [Building](#building)); cutscene zips are split per race, each under 2 GB. |

## How the cutscenes were made

`tools/video_pipeline.py` processes each of the 63 Cinepak AVIs (320x240, 15 fps) in four steps:

1. **ffmpeg** extracts the frames and the audio.
2. **Real-ESRGAN** (`realesrgan-x4plus`) upscales every frame 4x.
3. **RIFE** v4.6 interpolates the frames from 15 to 30 fps.
4. **ffmpeg** crops a bright stripe (6 source pixels) from the frame edges and scales the
   picture to 1386x1080. It centres the picture on a backdrop built from the game's
   carved-stone menu art, then encodes H.264 (x264 `veryslow`, CRF 24) with 64 kbps mono AAC
   audio (the originals are 22 kHz mono).

All 125 videos, the captioned copies included, come to about 700 MB. The backdrop is built at
3840x2160 and scaled to 1080p for the videos; the 4K version is `assets/backdrop.png`, which
cnc-ddraw shows around the 640x480 screens.

The cutscenes that have captions also get a `_sub` variant, with the captions from
`Data\VIDS.CAP` and `RES.000` converted to ASS and burned in. The pipeline caches each stage,
so an interrupted batch resumes where it stopped.
