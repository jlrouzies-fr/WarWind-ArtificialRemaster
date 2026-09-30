# In-game Save / Load menu (replace the Windows file dialogs)

**Status (2026-09-27): implemented** in `mod/src/saveload.cpp` (`[UI] InGameSaveLoad`). Sandbox-verified:
mission save (new file + overwrite prompt), ESC-menu load (double-click), main-menu load, Esc / Cancel,
Delete with confirmation, `InGameSaveLoad=0` restores the Windows dialogs, no faults logged. Not exercised:
the custom / multiplayer `.NSV` path (same code, other extension and title).

Task brief for an agent implementing an in-game Save / Load screen in the **War Wind HD** mod.
Today, "Save Current Game" / "Load a Saved Game" (ESC menu in a mission) and the main-menu
load open the Windows common file dialog (`GetSaveFileNameA` / `GetOpenFileNameA`). Goal: an
in-game screen in the game's own art style that lists the save slots, and lets the player type a
name to save or pick one to load, without any Windows window.

## Project context (read first)

- Game: War Wind (SSI 1997), `G:\SteamLibrary\steamapps\common\War Wind\WW.EXE`. It is a Watcom C
  build using the register calling convention: arguments in eax, edx, ebx, ecx, then the stack.
  The callee preserves every register except eax. WW.EXE stays **unmodified on disk**.
- Mod: proxy `dsound.dll`, C++ in `Vibe-Reverse-Engineering\patches\WarWind\mod\src\`.
  - Build: `powershell -NoProfile -Command "& cmd.exe /c '<abs>\mod\build.cmd'"` produces `mod\build\dsound.dll`.
  - Deploy: `tools\deploy.py install [--dev]`. `--dev` enables the dev command channel.
  - Hook helpers are in `hook.h`: `HookVerify`, `HookJump`, `HookDetour`, `WatcomCall`/`WatcomCall3`,
    and the `WRAP` macro.
  - Game addresses live in `game.h`. The knowledge base is `kb.h`; also read `findings_*.txt`.
- **Testing rules (mandatory):**
  - Never launch the game on the user's desktop, and never in real fullscreen. Use
    `tools\sandbox.py run` (hidden desktop, display-mode watchdog, audio muted).
  - Drive the game with `tools\caps.py "<dev command>" ...` (commands are listed in `devcmd.h`).
  - Ask the user before any launch they will see.
  - Back up files into `backups\<date>_<slug>\` before editing them.
- Rendering:
  - Menus are 640x480, 8-bit palettized, under cnc-ddraw.
  - During missions the screen is 1280x720, and the game draws into an offscreen canvas that
    `canvas.cpp` composes onto the screen. The ESC menu (screen handler `0x4713E8`) is drawn
    centred over the map area through `hires.cpp` (`MenuShift`, `CentredDX/DY`).
  - A save/load screen opened from the ESC menu has to work in that mode. From the main menu it
    runs at 640x480.
  - Reusable pieces:
    - Drawing primitives: `DrawImage 0x413A7C`, which applies each frame's stored offset.
      `FillRect 0x41468C` and `SetTextPos 0x43FD2C`. Text printing: `PrintString 0x43FE50`,
      `PrintResourceText 0x43FDDC`, `SelectFont 0x43FCFC`. Race panel fonts: `0x800100E9 + 4*race`.
    - `hudart.cpp`: carved background and sunken wells, painted in the race's colour ramp.
    - Dialog border tiles per race: RES.001 #197..#200 (see `ui_bottom_bar/plan.md`).

## How saving/loading works today (static findings, verify live)

| Address | What |
|---|---|
| `0x448990` | Save game. If `*(int*)0x4B66EC == 1` (network game) it calls `0x444680` (Save *Custom or Multiplayer* As, `*.NSV`), else `0x4445B0` (Save *Campaign* As, `*.SAV`). A zero return means cancelled. The path ends up at **`0x4B48E2`**, the file is opened with mode `"wb"` (`0x4B1084`) via `0x49AA70` (fopen), then serialised. |
| `0x448C30` | Load game. Network: `0x444820` (Restore Custom/Multiplayer, `*.NSV`); else `0x444750` (Restore Campaign, `*.SAV`). The path is at **`0x4B49E6`**, opened `"rb"` (`0x4B07A8`). |
| `0x444680`, `0x444750`, `0x444820`, `0x4445B0` (check the exact start: a wrapper starts at `0x44459F`/`0x4445B0`) | Fill an `OPENFILENAMEA` on the stack and call `GetSaveFileNameA` (thunk `0x4A0B0C`) / `GetOpenFileNameA` (thunk `0x4A0B06`), returning its BOOL. Fields: owner `*(HWND*)0x4C2C20`; title RES.000 `0x294..0x297`; filter RES.000 `0x298` (SAV) / `0x29D` (NSV); `nMaxFile` 0x104; `lpstrInitialDir` `*(char**)0x4B4AF4` = `"SAVES\"`; `lpstrDefExt` `*(char**)0x4B4AEC` = `"SAV"` / `*(char**)0x4B4AF0` = `"NSV"`; flags `0x28080E`/`0x28080C`. `lpstrFile` comes from a register (`ebp`); **confirm it is `0x4B48E2` / `0x4B49E6`**. Before the call, `0x410274` calls `IDirectDraw::FlipToGDISurface` so the dialog is visible. |
| `0x448C84..` | After a successful load path: fopen, then clear screen (FillRect + Flip twice), then the loader runs. |
| RES.000 `0x294..0x29D` | The dialog titles and filters listed above. |
| Save folder | `<game>\Saves\` (the files are `*.SAV`). Steam Cloud syncs it (`steam_autocloud.vdf` is present). |

The save file itself is a named-field serializer, with strings such as "selectedIDs" and
"bigstartx" (see `kb.h`). There is **no need to touch it**: the plan below only replaces the step
that picks the file name.

## Proposed design

1. **Replace the four dialog wrappers** (detour their entry points). Each wrapper runs a modal
   in-game screen and returns 1 after writing a full path into the same buffer the game reads
   afterwards (`0x4B48E2` for save, `0x4B49E6` for load), or 0 when cancelled. Everything after
   that (fopen, serialisation, loading) stays the game's own code. Keep the `.SAV` / `.NSV`
   choice and the `SAVES\` folder.
2. **Modal loop** inside the wrapper, following `video.cpp` `PlayFile`:
   - Pump messages and route keyboard and mouse to the screen.
   - Redraw every frame into the back buffer (`*(void**)0x4C2B30`), then call `Flip 0x414588`.
   - Leave the game's display state as it found it.
   - The ESC-menu case is a mission at 1280x720 using the canvas: draw into the canvas (the
     canvas composes the screen at Flip), centred over the map area like the ESC menu.
   - The main-menu case is 640x480.
3. **Screen content** (War Wind style, race-coloured, the game's font):
   - Title (RES.000 `0x294..0x297`, without the "War Wind - " prefix).
   - A scrollable list of save files in `Saves\` with name, date/time and size, newest first.
     Mouse and arrow keys select, the wheel scrolls, and a double-click confirms.
   - Save only: a text field for the save name (a typed name gives `<name>.SAV`) and an
     "overwrite?" confirmation.
   - Load only: delete a save with Del and a confirmation (optional).
   - Buttons: OK (Enter), Cancel (Esc). Reuse the look of the game's Yes/No dialog (the ESC menu's
     "Restart Scenario?" box) or the column's orb buttons.
4. **Keyboard focus:**
   - While the screen is open, stop the game's own key handling from acting on the keys (they
     go to the text field).
   - Grid hotkeys and the camera must not react. `input.cpp` / `camera.cpp` receive the window
     messages first, so add a "modal UI active" check there.
5. **Ini switch:** `[UI] InGameSaveLoad=1`. When it is 0, keep the Windows dialogs.

## Pitfalls already known

- Palette: GDI and Windows dialogs used to corrupt the colours. Fixed by
  `patchsets/fix-gdi-palette.wwp`. Once the dialogs are gone this matters less, but keep the patch.
- The game's cursor is drawn by `Flip` (`DrawCursor 0x413E30`). In missions the mod feeds it screen
  coordinates (`CanvasSetScreenMouse`), so keep calling Flip to keep the cursor alive.
- `SetScreenHandler 0x414048` is hooked by `hires.cpp` and switches video modes. Do **not** change
  screen handler for the modal screen; stay inside the wrapper call.
- Mouse messages in a mission are translated screen→canvas in `input.cpp` `HookedWndProc`. The
  modal screen should work in screen coordinates: take them before the translation, or use
  `CanvasSetScreenMouse`'s input.
- Network games (`*(int*)0x4B66EC == 1`) use `.NSV` and an extra network step (`0x488278`) after
  the dialog. Keep that path untouched.
- Wrapper boundaries: confirm each detour site's prologue bytes with `HookVerify` against the
  original exe (`_original_backup\WW.EXE`) before patching.

## Acceptance tests (in the sandbox)

1. Mission, ESC, "Save Current Game": the in-game screen appears. Type a name and press Enter.
   A new `Saves\<name>.SAV` exists, and the mission continues without broken colours or a
   stuck cursor.
2. ESC, "Load a Saved Game": pick that save and confirm. The mission reloads, with units and
   camera where they were saved.
3. Main menu load path, same as test 2.
4. Cancel paths (Esc, Cancel button) return to the menu with nothing saved or loaded.
5. `InGameSaveLoad=0` brings back the Windows dialogs.
6. No new faults in `WarWindHD.log` (the mod logs access violations with registers).
