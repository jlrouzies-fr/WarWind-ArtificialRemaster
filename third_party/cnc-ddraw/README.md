# cnc-ddraw with a backdrop image and Modern Graphics

The mod ships a build of [cnc-ddraw](https://github.com/FunkyFr3sh/cnc-ddraw) by FunkyFr3sh
(MIT licence, see [`LICENSE`](LICENSE)) with two changes of ours, applied in this order:

1. [`cnc-ddraw-backdrop.patch`](cnc-ddraw-backdrop.patch): a backdrop image around boxed screens.
2. [`cnc-ddraw-wwfx.patch`](cnc-ddraw-wwfx.patch): the Direct3D 9 effect passes behind the mod's
   Modern Graphics option.

## What the backdrop patch adds

A new `ddraw.ini` key, `backdrop=<png>`. When the game picture is boxed (the 640x480 menus,
briefings and the victory/defeat screen shown on a larger display), cnc-ddraw draws this image
around it instead of black bars. The path is relative to the game folder unless it is absolute.
The War Wind profile uses `backdrop=WarWindHD\backdrop.png`: the carved-stone menu art, AI-upscaled
to 3840x2160 (the same backdrop the HD cutscenes sit on).

- `src/config.c`, `inc/config.h`: reads the `backdrop` key (empty by default, so nothing changes
  for other games).
- `src/utils.c`, `inc/utils.h`: `util_get_backdrop()` loads the PNG once with the bundled LodePNG.
- `src/render_d3d9.c`, `inc/render_d3d9.h`: the image goes into an offscreen surface and is
  stretched to the window with `StretchRect` before the game picture is drawn.
- `src/render_gdi.c`: the same through `StretchDIBits` for the GDI renderer.

The OpenGL renderer is unchanged and still draws black bars. If the file is missing or cannot be
decoded, every renderer falls back to black bars.

## What the wwfx patch adds

A small effect API that the War Wind mod (`dsound.dll`) calls through `GetProcAddress`; nothing
changes for other games, which never call it.

- `src/wwfx.c`, `inc/wwfx.h` (new): exports `WWFX_Setup` (shader path and effect settings),
  `WWFX_Submit` (per frame: a class mask marking terrain, units, buildings, trees and effects,
  shadow and fog-of-war coverage planes, the light sources, and the world position the frame
  shows, which anchors the animated water to the map), `WWFX_Available` and `WWFX_Screenshot`
  (API version 3). The shaders are compiled
  at start-up from the HLSL file the mod names (`WarWindHD\shaders\wwfx.hlsl`) with
  `d3dcompiler_47.dll`; if that fails the passes stay off.
- `src/render_d3d9.c`: when a frame has been submitted and the passes are available, the game
  picture goes through the pass chain (soft shadows, contact shading, animated water, lights and
  bloom, fog of war, colour grade) before it is presented (the shader finds water on the terrain
  by its palette colours); otherwise it is drawn as before.
- `src/ddsurface.c`: on a flip of the primary surface the submitted planes are latched, so they
  become current together with the picture they describe.
- `exports.def`, `cnc-ddraw.vcxproj`, `cnc-ddraw.vcxproj.filters`: the exports and the new files.

The GDI and OpenGL renderers are unchanged; with them the mod's Modern Graphics option has no
effect.

## Rebuilding `ddraw.dll`

The patches are against cnc-ddraw commit
[`279a057`](https://github.com/FunkyFr3sh/cnc-ddraw/commit/279a057ee7e1e4d584b56141c014ab01f3c6ee6d)
(master, 2026-08-30; version 7.1.0.1).

```bat
git clone https://github.com/FunkyFr3sh/cnc-ddraw
cd cnc-ddraw
git checkout 279a057ee7e1e4d584b56141c014ab01f3c6ee6d
git apply <this repo>\third_party\cnc-ddraw\cnc-ddraw-backdrop.patch
git apply <this repo>\third_party\cnc-ddraw\cnc-ddraw-wwfx.patch
msbuild cnc-ddraw.sln /p:Configuration=Release /p:Platform=x86
```

The result is `bin\Release\ddraw.dll` (32-bit). The release build of this project was made with
the Visual Studio 2026 Build Tools. cnc-ddraw's own `build.cmd` (w64devkit/make) should also work.
Install it as `ddraw.dll` next to `WW.EXE`, together with the `ddraw.ini` profile from
`tools/ddraw.user.ini`, the image as `WarWindHD\backdrop.png` and the shaders
(`src/mod/shaders/wwfx.hlsl`) as `WarWindHD\shaders\wwfx.hlsl`.

## Licences

cnc-ddraw includes code under other licences, which apply to the built `ddraw.dll` as well:

| Component | Licence | File |
| --- | --- | --- |
| cnc-ddraw, by FunkyFr3sh | MIT | [`LICENSE`](LICENSE) |
| Microsoft Detours | MIT | [`LICENSE-detours.md`](LICENSE-detours.md) |
| LodePNG, by Lode Vandevenne | zlib | [`LICENSE-lodepng.txt`](LICENSE-lodepng.txt) |

The patches themselves are offered under the same MIT licence as cnc-ddraw.
