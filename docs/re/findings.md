# War Wind HD – findings log

## 2026-09-26 session 1

### Environment
- The `ddraw.dll` shipped by Steam is NOT cnc-ddraw: it is a 2016 Direct3D 9 wrapper configured via the registry,
  and it switches the monitor's display mode. Original kept in `<game>/_original_backup/`.
- cnc-ddraw (cloned repo, 7.1.0.1) built with VS 2026 Build Tools (`MSBuild cnc-ddraw.vcxproj /p:Configuration=Release /p:Platform=Win32`)
  and installed as `ddraw.dll` with a windowed profile. cnc-ddraw only calls `ChangeDisplaySettings` in non-windowed
  mode or on the fullscreen-toggle hotkeys, which the profile disables.
- `tools/sandbox.py run` starts WW.EXE on a hidden Win32 desktop with a display-mode watchdog. Not yet exercised.
- Toolkit quirks: use `py -3.13` / repo `.venv` (bash `python` is SVP's); pyghidra must be <3 for Ghidra 11.4;
  `retools.bootstrap` crashes on Watcom PEs (VirtualSize 0 sections).

### Mod loader (`mod/`, builds `dsound.dll`)
- First attempt was a winmm.dll proxy: it built and forwarded fine, but WW.EXE never loaded it. Windows applies
  AppCompat shims to this exe (apphelp + AcGenral + AcSpecfc are in the process), and AcGenral imports winmm,
  AcSpecfc imports ddraw/comctl32/comdlg32/winmm, all from System32 before the exe imports resolve; the loader
  then reuses the already-loaded module by name. dsound.dll is imported by the exe but by no shim, so the proxy
  is dsound.dll (12 exports forwarded to SysWOW64\dsound.dll via generated stubs, `gen_exports.py`).
  Verified with `mod/test/proxytest.exe` and in the game (WarWindHD.log shows hooks installed).
- The game shows "Operation under Windows NT is not supported. Continue?" unless cnc-ddraw `win_version=95`.
- Main menu hit points (640x480): Tha' Roon (115,100), Obblinox (510,90), Eaggra (110,385), Shama' Li (520,395),
  custom scenario/multiplayer (320,180), opening cinematic (325,75), load campaign (115,255), credits (520,245), quit (325,405).
- Applies `WarWindHD\*.wwp` byte patches (verify/patch lines), installs hooks, optional dev command channel
  (`WarWindHD.cmd`: capture/key/click/move/peek) that dumps the primary surface as an 8-bit BMP.
- `tools/deploy.py install [--dev]` / `uninstall`.

### Controls
- Panel hotkeys come from `g_hotkeyTable` (0x4B9B48, 30-byte rows per panel page), not from label text.
  `PanelHotkey` (0x47AF28) loops buttons with eax = index, edx = visible slot; compare at 0x47AF65.
  Mod patches that compare to `cmp ch,[edx+gridKeys]` so keys follow on-screen position.
- Command labels: text id = raceBase[race] (0x4B6158) + pageBase[page] (0x4B6190) + index, drawn via 0x44013C
  from 0x44CD87. Rewriting labels to show the grid key should hook there. The command bar is a scrolling text
  list (up/down arrows), not an icon grid — confirm in game before choosing the final key order.
- Control groups already exist: Shift+digit assigns (0x47ADAC), digit recalls (0x47AE00, 10 groups x 18 units
  at 0x4B99E0). Ctrl+digit did nothing; mod makes it assign (hook at 0x47B4EE).

### Cutscenes
- All videos go through `PlayVideo` 0x411D38(eax=path, edx=flags); callers ignore its result; it sets
  0x4C2D57 while playing and clears the screen at the end. Mod hooks its first 5 bytes (single-byte pushes).
- Captions: `Data/VIDS.CAP` triples (start frame, end frame, index) at 15 fps; text = RES.000[746 + index].
- Pipeline `tools/video_pipeline.py`: Real-ESRGAN x4plus -> RIFE v4.6 x2 -> 2772x2160 picture (6 source px
  cropped per side to remove a bright edge stripe) over 4K stone backdrop (RES.004 #5, palette RES.001 #1),
  NVENC H.264 CQ19 + `_sub` variant with ASS captions. 30-frame test verified; full batch not yet run.

### Resources
- `tools/res.py` (archive reader), `tools/d3gr.py` (D3GR decoder, raw 8bpp frames + 6-bit palettes).
- RES.004 #0-3 are the in-game 640x480 frame art per race; the map viewport and minimap are holes in it.

### In-game verification (sandbox, 2026-09-26 evening)
- Sandbox works: game runs on the hidden desktop, display mode untouched; captures via the mod's dev channel.
- Command menu is a right-click popup (row of icons) on the selected unit; hover label in the status bar.
  Grid keys verified (Q = Move, W = Attack). Label hook shows "Move  [Q]".
- Control groups: the game polls GetKeyState for modifiers each frame; posted key messages cannot hold Ctrl,
  so the harness uses a dev-only GetKeyState overlay (`mod 11 1`). cnc-ddraw patches the same IAT slot at its
  init, so the overlay re-hooks lazily.
- HD video hook: first version resolved the path against the CWD; the game passes paths relative to Data
  (`VIDS\EA\EA1MS.AVI`). Fixed to resolve against `<game>\Data`.
- Modifier bytes were mislabeled at first: 0x4B276C = Ctrl, 0x4B276D = Alt, 0x4B276E = Shift (poll at 0x41FD60).
  After the fix Ctrl+1 / Ctrl+2 store the selection (group table at 0x4B99E0 + n*36) and digits recall.
- HD player: skipping the original PlayVideo also skipped its display-suspend bookkeeping (0x4C2C24 is
  decremented on entry and the caller's 0x413FF8 re-increments it), which made the caller redraw with
  uninitialised state and crash at 0x413792. The hook now mirrors that bookkeeping and the two clear/flip
  calls; EA1MS then played through Media Foundation (12.3 s) and the mission continued.
- Cutscene pipeline bug: ffmpeg dropped the AVIs' repeated-frame (empty) chunks with passthrough timing,
  so encodes were up to 22% short and audio drifted. Extraction now uses `-vf fps=15`; the batch was
  restarted from scratch at 21:19.
